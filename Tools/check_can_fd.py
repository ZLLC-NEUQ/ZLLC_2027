#!/usr/bin/env python3
"""Run the real CAN driver against deterministic HAL/FreeRTOS doubles.

python Tools/check_can_fd.py [--cxx g++|path/to/cl.exe]
No hardware is accessed. Both robot roles and performance configurations run.
For ARM compilation, use the project's Keil/EIDE target as well.
"""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile


HAL_HEADER = r'''
#pragma once
#include <stdint.h>
typedef enum { HAL_OK, HAL_ERROR, HAL_BUSY, HAL_TIMEOUT } HAL_StatusTypeDef;
typedef enum { HAL_FDCAN_STATE_RESET, HAL_FDCAN_STATE_READY,
               HAL_FDCAN_STATE_BUSY, HAL_FDCAN_STATE_ERROR } HAL_FDCAN_StateTypeDef;
typedef enum { FDCAN1_IT0_IRQn, FDCAN2_IT0_IRQn, FDCAN3_IT0_IRQn } IRQn_Type;
typedef struct { uint32_t PSR, CCCR, ECR, IR; } FDCAN_GlobalTypeDef;
extern FDCAN_GlobalTypeDef mock_can[3];
#define FDCAN1 (&mock_can[0])
#define FDCAN2 (&mock_can[1])
#define FDCAN3 (&mock_can[2])
typedef struct {
    uint32_t FrameFormat, TxElmtSize, RxFifo0ElmtsNbr, RxFifo1ElmtsNbr;
} FDCAN_InitTypeDef;
typedef struct {
    FDCAN_GlobalTypeDef *Instance;
    FDCAN_InitTypeDef Init;
    HAL_FDCAN_StateTypeDef State;
    uint32_t ErrorCode;
} FDCAN_HandleTypeDef;
typedef struct {
    uint32_t Identifier, IdType, RxFrameType, DataLength, ErrorStateIndicator,
             BitRateSwitch, FDFormat, RxTimestamp, FilterIndex, IsFilterMatchingFrame;
} FDCAN_RxHeaderTypeDef;
typedef struct {
    uint32_t Identifier, IdType, TxFrameType, DataLength, ErrorStateIndicator,
             BitRateSwitch, FDFormat, TxEventFifoControl, MessageMarker;
} FDCAN_TxHeaderTypeDef;
typedef struct {
    uint32_t IdType, FilterIndex, FilterType, FilterConfig, FilterID1, FilterID2;
} FDCAN_FilterTypeDef;
typedef struct { uint32_t CYCCNT; } Mock_DWT;
extern Mock_DWT mock_dwt;
#define DWT (&mock_dwt)
#define FDCAN_FRAME_CLASSIC 0U
#define FDCAN_FRAME_FD_NO_BRS 0x100U
#define FDCAN_STANDARD_ID 0U
#define FDCAN_DATA_FRAME 0U
#define FDCAN_REMOTE_FRAME 0x20000000U
#define FDCAN_DATA_BYTES_8 4U
#define FDCAN_DATA_BYTES_64 18U
#define FDCAN_DLC_BYTES_8 8U
#define FDCAN_DLC_BYTES_64 15U
#define FDCAN_CLASSIC_CAN 0U
#define FDCAN_FD_CAN 0x200000U
#define FDCAN_BRS_OFF 0U
#define FDCAN_ESI_ACTIVE 0U
#define FDCAN_NO_TX_EVENTS 0U
#define FDCAN_PSR_BO 0x80U
#define FDCAN_CCCR_INIT 1U
#define FDCAN_RX_FIFO0 0U
#define FDCAN_RX_FIFO1 1U
#define FDCAN_FILTER_MASK 0U
#define FDCAN_FILTER_TO_RXFIFO0 1U
#define FDCAN_FILTER_TO_RXFIFO1 2U
#define FDCAN_REJECT 2U
#define FDCAN_REJECT_REMOTE 1U
#define FDCAN_CFG_RX_FIFO0 0U
#define FDCAN_IT_RX_FIFO0_NEW_MESSAGE (1U << 0)
#define FDCAN_IT_RX_FIFO0_FULL (1U << 2)
#define FDCAN_IT_RX_FIFO0_MESSAGE_LOST (1U << 3)
#define FDCAN_IT_RX_FIFO1_NEW_MESSAGE (1U << 4)
#define FDCAN_IT_RX_FIFO1_FULL (1U << 6)
#define FDCAN_IT_RX_FIFO1_MESSAGE_LOST (1U << 7)
#define FDCAN_FLAG_RX_FIFO0_NEW_MESSAGE FDCAN_IT_RX_FIFO0_NEW_MESSAGE
#define FDCAN_FLAG_RX_FIFO1_NEW_MESSAGE FDCAN_IT_RX_FIFO1_NEW_MESSAGE
#define FDCAN_IT_RAM_ACCESS_FAILURE (1U << 17)
#define FDCAN_IT_BUS_OFF (1U << 25)
#define FDCAN_IT_RAM_WATCHDOG (1U << 26)
#define FDCAN_IT_RESERVED_ADDRESS_ACCESS (1U << 29)
#define HAL_FDCAN_ERROR_RAM_ACCESS 1U
#define HAL_FDCAN_ERROR_RAM_WDG 2U
#define HAL_FDCAN_ERROR_RESERVED_AREA 4U
#define __HAL_FDCAN_CLEAR_FLAG(h, flags) ((h)->Instance->IR &= ~(flags))
#define UNUSED(x) ((void)(x))
#ifdef __cplusplus
extern "C" {
#endif
uint32_t __get_PRIMASK(void);
void __disable_irq(void);
void __set_PRIMASK(uint32_t);
void __DMB(void);
uint32_t __get_IPSR(void);
uint32_t HAL_GetTick(void);
uint32_t NVIC_GetEnableIRQ(IRQn_Type);
void HAL_NVIC_DisableIRQ(IRQn_Type);
void HAL_NVIC_EnableIRQ(IRQn_Type);
HAL_StatusTypeDef HAL_FDCAN_ConfigFilter(FDCAN_HandleTypeDef *, const FDCAN_FilterTypeDef *);
HAL_StatusTypeDef HAL_FDCAN_ConfigGlobalFilter(FDCAN_HandleTypeDef *, uint32_t, uint32_t, uint32_t, uint32_t);
HAL_StatusTypeDef HAL_FDCAN_ConfigFifoWatermark(FDCAN_HandleTypeDef *, uint32_t, uint32_t);
HAL_StatusTypeDef HAL_FDCAN_ActivateNotification(FDCAN_HandleTypeDef *, uint32_t, uint32_t);
HAL_StatusTypeDef HAL_FDCAN_Start(FDCAN_HandleTypeDef *);
HAL_StatusTypeDef HAL_FDCAN_Stop(FDCAN_HandleTypeDef *);
HAL_StatusTypeDef HAL_FDCAN_Init(FDCAN_HandleTypeDef *);
uint32_t HAL_FDCAN_GetTxFifoFreeLevel(const FDCAN_HandleTypeDef *);
HAL_StatusTypeDef HAL_FDCAN_AddMessageToTxFifoQ(FDCAN_HandleTypeDef *, const FDCAN_TxHeaderTypeDef *, const uint8_t *);
uint32_t HAL_FDCAN_GetRxFifoFillLevel(const FDCAN_HandleTypeDef *, uint32_t);
HAL_StatusTypeDef HAL_FDCAN_GetRxMessage(FDCAN_HandleTypeDef *, uint32_t, FDCAN_RxHeaderTypeDef *, uint8_t *);
void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef *, uint32_t);
void HAL_FDCAN_RxFifo1Callback(FDCAN_HandleTypeDef *, uint32_t);
void HAL_FDCAN_ErrorStatusCallback(FDCAN_HandleTypeDef *, uint32_t);
void HAL_FDCAN_ErrorCallback(FDCAN_HandleTypeDef *);
#ifdef __cplusplus
}
#endif
'''

FREERTOS_HEADER = r'''
#pragma once
#include <stdint.h>
typedef int BaseType_t;
typedef unsigned UBaseType_t;
typedef uint32_t TickType_t;
typedef void *QueueHandle_t;
#define pdFALSE 0
#define pdTRUE 1
#define pdPASS 1
#define taskSCHEDULER_NOT_STARTED 0
#define taskSCHEDULER_RUNNING 1
#ifdef __cplusplus
extern "C" {
#endif
BaseType_t xTaskGetSchedulerState(void);
BaseType_t xQueueSendFromISR(QueueHandle_t, const void *, BaseType_t *);
void mock_yield(BaseType_t);
#ifdef __cplusplus
}
#endif
#define portYIELD_FROM_ISR(x) mock_yield(x)
'''


def compiler_environment(compiler):
    env = os.environ.copy()
    if Path(compiler).name.lower() != "cl.exe":
        return env
    toolset = Path(compiler).parents[3]
    kits = Path(os.environ.get("ProgramFiles(x86)", "C:/Program Files (x86)")) / "Windows Kits/10"
    versions = sorted((kits / "Include").glob("10.*"))
    if not versions:
        raise RuntimeError("MSVC host test needs an installed Windows SDK")
    version = versions[-1].name
    env["INCLUDE"] = os.pathsep.join(map(str, [toolset / "include"] +
        [kits / "Include" / version / name for name in ("ucrt", "shared", "um")]))
    env["LIB"] = os.pathsep.join(map(str, [toolset / "lib/x64"] +
        [kits / "Lib" / version / name / "x64" for name in ("ucrt", "um")]))
    env["PATH"] = str(Path(compiler).parent) + os.pathsep + env.get("PATH", "")
    return env


def native_compiler(requested):
    if requested:
        return shutil.which(requested) or requested
    candidates = [shutil.which("g++"), shutil.which("cl")]
    if os.name == "nt":
        for variable in ("ProgramFiles", "ProgramFiles(x86)"):
            base = Path(os.environ.get(variable, "C:/Program Files")) / "Microsoft Visual Studio"
            candidates += list(reversed(sorted(base.glob("*/*/VC/Tools/MSVC/*/bin/Hostx64/x64/cl.exe"))))
    for candidate in candidates:
        if not candidate:
            continue
        try:
            flag = "/?" if Path(candidate).name.lower() == "cl.exe" else "--version"
            result = subprocess.run([str(candidate), flag], capture_output=True, timeout=10)
            if result.returncode == 0:
                return str(candidate)
        except (OSError, subprocess.TimeoutExpired):
            continue  # An installed compiler may be unable to start (e.g. a missing runtime DLL).
    raise RuntimeError("No working native C++ compiler found; specify --cxx g++ or a full path to cl.exe")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cxx", help="native g++ or cl.exe; default: detect a compiler that can start")
    args = parser.parse_args()
    compiler = native_compiler(args.cxx)
    print("Host C++ compiler: " + compiler, flush=True)
    root = Path(__file__).resolve().parents[1]
    env = compiler_environment(compiler)
    # Test artifacts stay in the workspace and TemporaryDirectory removes only its own directory.
    with tempfile.TemporaryDirectory(prefix="can_fd_", dir=root / "Tools") as directory:
        work = Path(directory)
        assert work.resolve().parent == (root / "Tools").resolve()
        (work / "stm32h7xx_hal.h").write_text(HAL_HEADER, encoding="utf-8")
        (work / "main.h").write_text('#include "stm32h7xx_hal.h"\n', encoding="utf-8")
        (work / "ita_chariot.h").write_text('extern bool Reset_Pitch_2;\n', encoding="utf-8")
        for name in ("FreeRTOS.h", "task.h", "queue.h"):
            (work / name).write_text(FREERTOS_HEADER if name == "FreeRTOS.h" else '#include "FreeRTOS.h"\n', encoding="utf-8")
        for role in ("CHASSIS", "GIMBAL"):
            (work / "config.h").write_text('#define %s\n' % role, encoding="utf-8")
            for performance in (0, 1):
                exe = work / (role.lower() + str(performance) + ".exe")
                includes = [work, root / "User/Driver/Inc", root / "User/Task/Inc"]
                sources = [root / "User/Driver/Src/drv_can.cpp", root / "Tools/can_fd_tests.cpp"]
                defines = ["STM32H723xx", "PERFORMANCE_TEST_ENABLE=%d" % performance]
                if Path(compiler).name.lower() == "cl.exe":
                    command = [compiler, "/nologo", "/std:c++14", "/EHsc", "/W4", "/WX", "/utf-8"]
                    command += ["/I" + str(path) for path in includes] + ["/D" + value for value in defines]
                    command += list(map(str, sources)) + ["/Fe:" + str(exe)]
                else:
                    command = [compiler, "-std=c++11", "-Wall", "-Wextra", "-Werror", "-Wno-missing-field-initializers"]
                    command += ["-I" + str(path) for path in includes] + ["-D" + value for value in defines]
                    command += list(map(str, sources)) + ["-o", str(exe)]
                subprocess.run(command, cwd=work, env=env, check=True)
                subprocess.run([str(exe)], cwd=work, env=env, check=True)
                print("PASS: %s PERFORMANCE_TEST_ENABLE=%d" % (role, performance), flush=True)


if __name__ == "__main__":
    main()
