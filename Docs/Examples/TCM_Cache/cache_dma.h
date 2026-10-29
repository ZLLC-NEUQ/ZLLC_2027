#ifndef TUTORIAL_CACHE_DMA_H
#define TUTORIAL_CACHE_DMA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "stm32h7xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TUTORIAL_CACHE_LINE 32U
#define TUTORIAL_DMA_BASE UINT32_C(0x30000000)
#define TUTORIAL_DMA_SIZE UINT32_C(0x00008000)
#define TUTORIAL_DMA_BUFFER __attribute__((section(".dma_cacheable"), aligned(32)))
#define TUTORIAL_ITCM __attribute__((section(".tutorial_itcm"), noinline))
#define TUTORIAL_DTCM_DATA __attribute__((section(".tutorial_dtcm_data"), aligned(8)))
#define TUTORIAL_DTCM_BSS __attribute__((section(".tutorial_dtcm_bss"), aligned(8)))

/* Reset-time only: reserve all 32 KiB of D2 SRAM; no active DMA or other users.
 * Call before enabling D-cache and before reading NOLOAD buffers. This destroys
 * the entire region. Aligned word stores initialize both SRAM banks and ECC. */
bool Tutorial_DmaSramInit(void);

/* Manual-maintenance strategy only, on cacheable Normal memory.
 * buffer: 32-byte aligned base of an EXCLUSIVE allocation in H723 D2 SRAM.
 * capacity: actual reserved bytes, positive and a multiple of 32.
 * length: positive payload bytes <= capacity; never a HAL element count > 8 bit.
 * The caller owns ALL cache lines of the allocation. No CPU/ISR/task may access
 * it while DMA owns it. These functions provide barriers, not locking.
 * length is rounded upward only within the validated allocation. */
bool Tutorial_DmaTxPrepare(void *buffer, size_t capacity, size_t length);
bool Tutorial_DmaRxPrepare(void *buffer, size_t capacity, size_t length);
bool Tutorial_DmaRxComplete(void *buffer, size_t capacity, size_t length);

/* Alternate startup-only strategy. MPU region 7 and the ENTIRE D2 SRAM region
 * must be reserved. Never combine this with the manual UART example on D2 SRAM.
 * Return false if cache/MPU is already enabled; do not reconfigure live memory. */
bool Tutorial_MpuDmaNonCacheable(void);

/* Optional GNU ld tutorial sections, separate from existing .itcm_ekf sections.
 * Call in Reset_Handler after SystemInit and before __libc_init_array.
 * Keep this initializer and its stack in ordinary initialized memory. */
void Tutorial_TcmInit(void);

/* Single-instance, normal-mode, 8-bit UART DMA example. Only the public task
 * APIs may be called by a task/main; serialize them with ONE application mutex.
 * Call hooks from the existing HAL callbacks; they run in interrupt context.
 * Other code must never use the selected UART or its DMA streams concurrently. */
bool Tutorial_UartBind(UART_HandleTypeDef *uart);
HAL_StatusTypeDef Tutorial_UartSend(const uint8_t *source, size_t length);
HAL_StatusTypeDef Tutorial_UartReceive(size_t length);
bool Tutorial_UartTakeRx(uint8_t *destination, size_t capacity, size_t *length);
HAL_StatusTypeDef Tutorial_UartAbortAndReset(void);
bool Tutorial_UartHasFault(void);
void Tutorial_UartTxCompleteHook(UART_HandleTypeDef *uart);
void Tutorial_UartRxCompleteHook(UART_HandleTypeDef *uart);
void Tutorial_UartErrorHook(UART_HandleTypeDef *uart);

#ifdef __cplusplus
}
#endif
#endif
