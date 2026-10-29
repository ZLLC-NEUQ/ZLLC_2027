#!/usr/bin/env python3
"""Build and exercise the actual 2027 referee implementation on the host.

    python Tools/check_referee_protocol.py [--cxx C:/mingw64/bin/g++.exe]

Only hardware, RTOS timing and performance instrumentation are stubbed. Wire
layout, parsing, CRC and transmit code are the repository's actual sources.
The independent bitwise CRC/frame generator in referee_protocol_tests.cpp
supplies the input oracle. This cannot verify STM32 UART/DMA timing or RF links.
All generated headers and executables live in a temporary directory.
"""

import argparse
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


STUBS = {
    "config.h": "#pragma once\n#if !defined(GIMBAL) && !defined(CHASSIS)\n#define CHASSIS\n#endif\n#ifndef PERFORMANCE_TEST_ENABLE\n#define PERFORMANCE_TEST_ENABLE 0\n#endif\n",
    "dvc_performance.h": "#pragma once\n#define PERF_EVENT_AT(...) ((void)0)\n#define PERF_EVENT(...) ((void)0)\n",
    "freertos_tasks.h": "#pragma once\n#include <stdint.h>\n#include \"task.h\"\ninline uint32_t FreeRTOS_GetRxCycle() { return 0; }\n",
    "FreeRTOS.h": "#pragma once\n#include <stdint.h>\n",
    "task.h": """#pragma once
#include <assert.h>
extern unsigned test_critical_depth;
extern unsigned test_critical_entries;
inline void test_enter_critical() { ++test_critical_depth; ++test_critical_entries; }
inline void test_exit_critical() { assert(test_critical_depth != 0); --test_critical_depth; }
#define taskENTER_CRITICAL() test_enter_critical()
#define taskEXIT_CRITICAL() test_exit_critical()
""",
    "drv_math.h": "#pragma once\n",
    "buzzer.h": "#pragma once\n",
    "dvc_GraphicsSendTask.h": """#pragma once
#include <stdint.h>
struct JudgeReceive_t { uint8_t robot_id; uint8_t Radar_Double_Damage_Flag; };
extern JudgeReceive_t JudgeReceiveData;
void Referee_DMA_EnqueuePacket(const uint8_t *, uint16_t);
uint8_t Referee_DMA_TryEnqueuePacket(const uint8_t *, uint16_t);
""",
    "drv_uart.h": """#pragma once
#include <stdint.h>
#include <stddef.h>
#define UART_BUFFER_SIZE 128
#define HAL_UART_STATE_READY 0x20
#define HAL_UART_STATE_BUSY_TX 0x21
struct UART_HandleTypeDef { void *Instance; uint32_t gState; };
enum HAL_StatusTypeDef { HAL_OK = 0, HAL_ERROR = 1, HAL_BUSY = 2, HAL_TIMEOUT = 3 };
#define USART1 ((void *)1)
#define USART2 ((void *)2)
#define USART3 ((void *)3)
#define UART4 ((void *)4)
#define UART5 ((void *)5)
#define USART6 ((void *)6)
#define UART7 ((void *)7)
#define UART8 ((void *)8)
#define UART9 ((void *)9)
#define USART10 ((void *)10)
typedef void (*UART_Call_Back)(uint8_t *, uint16_t);
struct Struct_UART_Manage_Object {
    UART_HandleTypeDef *UART_Handler;
    uint8_t Tx_Buffer[UART_BUFFER_SIZE];
    uint8_t Rx_Buffer[UART_BUFFER_SIZE];
    uint16_t Rx_Buffer_Length, Tx_Buffer_Length, Rx_Length, Tx_Length;
    UART_Call_Back Callback_Function;
};
extern Struct_UART_Manage_Object UART1_Manage_Object, UART2_Manage_Object;
extern Struct_UART_Manage_Object UART3_Manage_Object, UART4_Manage_Object;
extern Struct_UART_Manage_Object UART5_Manage_Object, UART6_Manage_Object;
extern Struct_UART_Manage_Object UART7_Manage_Object, UART8_Manage_Object;
extern Struct_UART_Manage_Object UART9_Manage_Object, UART10_Manage_Object;
extern UART_HandleTypeDef huart1, huart10;
uint32_t HAL_GetTick();
uint8_t UART_Send_Data(UART_HandleTypeDef *, uint8_t *, uint16_t);
HAL_StatusTypeDef HAL_UART_Transmit(UART_HandleTypeDef *, uint8_t *, uint16_t, uint32_t);
HAL_StatusTypeDef HAL_UART_Transmit_DMA(UART_HandleTypeDef *, uint8_t *, uint16_t);
uint8_t UART_RecoveryPending(UART_HandleTypeDef *);
inline uint32_t __get_PRIMASK() { return 0; }
inline void __disable_irq() {}
inline void __enable_irq() {}
""",
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--cxx", default=shutil.which("g++") or "C:/mingw64/bin/g++.exe")
    args = parser.parse_args()
    root = args.root.resolve()
    with tempfile.TemporaryDirectory(prefix="referee_protocol_check_") as directory:
        temporary = Path(directory)
        for name, contents in STUBS.items():
            (temporary / name).write_text(contents, encoding="utf-8")
        for role in ("CHASSIS", "GIMBAL"):
            executable = temporary / ("referee_protocol_%s.exe" % role.lower())
            command = [args.cxx, "-std=c++11", "-O1", "-g", "-Wall", "-Wextra",
                       "-Wno-write-strings", "-Wno-unused-parameter", "-fno-strict-aliasing",
                       "-D" + role, "-I" + str(temporary), "-I" + str(root / "User/Device/Inc"),
                       str(root / "User/Device/Src/dvc_referee.cpp"),
                       str(root / "Tools/referee_protocol_tests.cpp"), "-o", str(executable)]
            subprocess.check_call(command)
            print("Running %s protocol checks:" % role, flush=True)
            subprocess.check_call([str(executable)])
    subprocess.check_call([sys.executable, str(root / "Tools/check_referee_transport.py"),
                           "--root", str(root), "--cxx", args.cxx])


if __name__ == "__main__":
    main()
