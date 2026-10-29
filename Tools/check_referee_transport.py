#!/usr/bin/env python3
"""Exercise the actual shared referee DMA queue with a deterministic host HAL.

    python Tools/check_referee_transport.py [--cxx C:/mingw64/bin/g++.exe]

Only the transport section is compiled from dvc_GraphicsSendTask.cpp so these
tests do not need the UI drawing, chassis, CMSIS or STM32 peripheral headers.
The queue, locking, start/completion decisions and physical rate gates are the
repository code, not a Python reimplementation. This does not verify physical
DMA timing, NVIC masking, or UART error interrupts on a board.
"""

import argparse
from pathlib import Path
import re
import shutil
import subprocess
import tempfile


HAL = r"""
#include <cassert>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <vector>

struct RefereeDMAPacket_t {
    uint8_t data[REFEREE_DMA_MAX_PACKET_LEN];
    uint16_t len;
};
struct Test_DMA { int State; };
struct Test_UART { Test_DMA *hdmatx; int gState; };
static Test_DMA dma;
static Test_UART huart10 = { &dma, 0 };
static uint32_t tick;
static unsigned critical_depth, starts;
static bool recovery;
static int next_status;
static uint8_t *active;
static std::vector<unsigned> payloads;

#define HAL_UART_STATE_READY 0
#define HAL_DMA_STATE_READY 0
#define HAL_OK 0
#define taskENTER_CRITICAL() (++critical_depth)
#define taskEXIT_CRITICAL() (--critical_depth)

static uint32_t HAL_GetTick() { return tick; }
static int UART_RecoveryPending(Test_UART *) { return recovery; }
static int UART_Send_Data(Test_UART *, uint8_t *data, uint16_t length)
{
    assert(critical_depth > 0);
    assert(length >= 9 && length <= REFEREE_DMA_MAX_PACKET_LEN);
    if (next_status != HAL_OK) { return next_status; }
    active = data;
    ++starts;
    payloads.push_back(data[7]);
    dma.State = 1;
    huart10.gState = 1;
    return HAL_OK;
}
"""


TESTS = r"""
static void reset()
{
    referee_dma_head = referee_dma_tail = referee_dma_count = referee_dma_busy = 0;
    for (auto &limit : referee_dma_rate_limits) {
        limit.started = 0;
        limit.last_start_ms = 0;
    }
    dma.State = huart10.gState = 0;
    huart10.hdmatx = &dma;
    tick = starts = critical_depth = 0;
    recovery = false;
    next_status = HAL_OK;
    payloads.clear();
    active = nullptr;
}

static void complete() { dma.State = huart10.gState = 0; }

// Transport accepts opaque complete frames. CRC/layout is covered separately
// by check_referee_protocol.py; byte 7 is a unique FIFO marker for this test.
static void packet(uint8_t *p, uint16_t command, uint8_t marker = 0)
{
    memset(p, 0, 9);
    p[0] = 0xa5;
    p[5] = static_cast<uint8_t>(command);
    p[6] = static_cast<uint8_t>(command >> 8);
    p[7] = marker;
}

int main()
{
    uint8_t p[REFEREE_DMA_MAX_PACKET_LEN];
    reset();
    assert(!Referee_DMA_TryEnqueuePacket(nullptr, 9));
    assert(!Referee_DMA_TryEnqueuePacket(p, 8));
    assert(!Referee_DMA_TryEnqueuePacket(p, REFEREE_DMA_MAX_PACKET_LEN + 1));

    // Fill the queue while the first slot is still owned by DMA. A rejected
    // producer must not overwrite either active bytes or accepted FIFO order.
    packet(p, 0x0301, 0);
    assert(Referee_DMA_TryEnqueuePacket(p, 9));
    assert(starts == 1 && referee_dma_count == 1 && referee_dma_busy == 1);
    for (unsigned i = 1; i < REFEREE_DMA_TX_QUEUE_DEPTH; ++i) {
        packet(p, 0x0301, static_cast<uint8_t>(i));
        assert(Referee_DMA_TryEnqueuePacket(p, 9));
    }
    packet(p, 0x0301, 255);
    assert(!Referee_DMA_TryEnqueuePacket(p, 9));
    assert(referee_dma_count == REFEREE_DMA_TX_QUEUE_DEPTH);
    assert(active[7] == 0 && critical_depth == 0);

    // A DMA-only or UART-only completion is insufficient to release a slot.
    dma.State = HAL_DMA_STATE_READY;
    Referee_DMA_Service();
    assert(referee_dma_count == REFEREE_DMA_TX_QUEUE_DEPTH && referee_dma_busy);
    dma.State = 1;
    huart10.gState = HAL_UART_STATE_READY;
    Referee_DMA_Service();
    assert(referee_dma_count == REFEREE_DMA_TX_QUEUE_DEPTH && referee_dma_busy);

    tick = 33;
    complete();
    Referee_DMA_Service();
    assert(starts == 1 && referee_dma_count == REFEREE_DMA_TX_QUEUE_DEPTH - 1);
    assert(!referee_dma_busy);
    Referee_DMA_Service(); // An idle poll must not discard an unstarted head.
    assert(referee_dma_count == REFEREE_DMA_TX_QUEUE_DEPTH - 1);
    tick = 34;
    Referee_DMA_Service();
    assert(starts == 2 && active[7] == 1);
    for (unsigned i = 2; i < REFEREE_DMA_TX_QUEUE_DEPTH; ++i) {
        complete();
        tick = i * 34;
        Referee_DMA_Service();
        assert(active[7] == i);
    }
    complete();
    Referee_DMA_Service();
    assert(referee_dma_count == 0 && !referee_dma_busy);
    assert(starts == REFEREE_DMA_TX_QUEUE_DEPTH);
    Referee_DMA_Service();
    assert(referee_dma_count == 0 && critical_depth == 0);

    // Independent protocol expectations, deliberately not read from the
    // implementation's rate-limit values. Legacy UI and new robot producers
    // must both enter the same physical rate gate.
    const uint16_t commands[] = {0x0301, 0x0305, 0x0307, 0x0308};
    const uint32_t intervals[] = {34, 200, 1000, 334};
    for (unsigned k = 0; k < 4; ++k) {
        reset();
        packet(p, commands[k], 1);
        Referee_DMA_EnqueuePacket(p, 9);
        packet(p, commands[k], 2);
        assert(Referee_DMA_TryEnqueuePacket(p, 9));
        complete();
        tick = intervals[k] - 1;
        Referee_DMA_Service();
        assert(starts == 1);
        tick = intervals[k];
        Referee_DMA_Service();
        assert(starts == 2 && active[7] == 2);
    }

    reset();
    tick = UINT32_MAX - 10;
    packet(p, 0x0301);
    assert(Referee_DMA_TryEnqueuePacket(p, 9));
    packet(p, 0x0301, 1);
    assert(Referee_DMA_TryEnqueuePacket(p, 9));
    complete();
    tick = 22; // 33 ms elapsed across wrap.
    Referee_DMA_Service();
    assert(starts == 1);
    tick = 23;
    Referee_DMA_Service();
    assert(starts == 2);

    // Recovery pauses new starts and holds the current queue slot until the
    // driver's abort/recovery service has restored both ready states.
    reset();
    recovery = true;
    packet(p, 0x0305);
    assert(Referee_DMA_TryEnqueuePacket(p, 9));
    Referee_DMA_Service();
    assert(starts == 0 && referee_dma_count == 1);
    recovery = false;
    Referee_DMA_Service();
    assert(starts == 1);
    recovery = true;
    complete();
    Referee_DMA_Service();
    assert(referee_dma_count == 1 && referee_dma_busy);
    recovery = false;
    Referee_DMA_Service();
    assert(referee_dma_count == 0 && !referee_dma_busy);

    // HAL_BUSY must retain a copied packet for a later successful start.
    reset();
    next_status = 2;
    packet(p, 0x0308, 42);
    assert(Referee_DMA_TryEnqueuePacket(p, 9));
    assert(starts == 0 && referee_dma_count == 1 && !referee_dma_busy);
    Referee_DMA_Service();
    assert(referee_dma_count == 1);
    next_status = HAL_OK;
    Referee_DMA_Service();
    assert(starts == 1 && referee_dma_busy && active[7] == 42);

    // The startup service is safe before the physical DMA handle is bound.
    reset();
    huart10.hdmatx = nullptr;
    packet(p, 0x0301);
    assert(Referee_DMA_TryEnqueuePacket(p, 9));
    Referee_DMA_Service();
    assert(starts == 0 && referee_dma_count == 1);
    huart10.hdmatx = &dma;
    Referee_DMA_Service();
    assert(starts == 1 && critical_depth == 0);

    puts("Shared referee DMA transport: queue ownership, backpressure, completion, "
         "four rate gates, tick wrap and recovery PASS");
}
"""


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--cxx", default=shutil.which("g++") or "C:/mingw64/bin/g++.exe")
    args = parser.parse_args()
    root = args.root.resolve()
    source = (root / "User/Device/Src/dvc_GraphicsSendTask.cpp").read_text(encoding="utf-8")
    header = (root / "User/Device/Inc/dvc_GraphicsSendTask.h").read_text(encoding="utf-8")
    capacity = re.search(r"^#define\s+REFEREE_DMA_TX_QUEUE_DEPTH\s+(\d+)", header, re.MULTILINE)
    packet_size = re.search(r"^#define\s+SEND_MAX_SIZE\s+(\d+)", header, re.MULTILINE)
    if not capacity or not packet_size:
        raise RuntimeError("Cannot locate referee DMA queue constants in its actual header")
    start = source.index("static RefereeDMAPacket_t referee_dma_queue")
    end = source.index("/**********************************************************************************************************", start)
    transport = source[start:end]
    if "void Referee_DMA_EnqueuePacket(" not in transport:
        raise RuntimeError("The actual referee DMA transport section was not extracted completely")
    constants = ("#define REFEREE_DMA_TX_QUEUE_DEPTH " + capacity.group(1) + "\n"
                 "#define REFEREE_DMA_MAX_PACKET_LEN " + packet_size.group(1) + "\n")
    with tempfile.TemporaryDirectory(prefix="referee_transport_check_") as directory:
        temporary = Path(directory)
        test_source = temporary / "referee_transport_tests.cpp"
        test_source.write_text(constants + HAL + transport + TESTS, encoding="utf-8")
        executable = temporary / "referee_transport_tests.exe"
        subprocess.check_call([args.cxx, "-std=c++11", "-Wall", "-Wextra", "-Werror",
                               str(test_source), "-o", str(executable)])
        subprocess.check_call([str(executable)])


if __name__ == "__main__":
    main()
