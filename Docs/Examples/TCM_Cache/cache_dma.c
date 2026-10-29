#include "cache_dma.h"
#include <limits.h>

_Static_assert(sizeof(uintptr_t) == 4U, "This example targets a 32-bit Cortex-M7");

bool Tutorial_DmaSramInit(void)
{
    if ((SCB->CCR & SCB_CCR_DC_Msk) != 0U) {
        return false;
    }
    __HAL_RCC_D2SRAM1_CLK_ENABLE();
    __HAL_RCC_D2SRAM2_CLK_ENABLE();
    __DSB();
    /* Volatile word accesses prevent byte writes or compiler-generated memset.
     * Do not call again after any object in this reserved region is in use. */
    for (uintptr_t address = TUTORIAL_DMA_BASE;
         address < TUTORIAL_DMA_BASE + TUTORIAL_DMA_SIZE;
         address += sizeof(uint32_t)) {
        *(volatile uint32_t *)address = 0U;
    }
    __DSB();
    return true;
}

static bool checked_size(void *buffer, size_t capacity, size_t length,
                         int32_t *rounded)
{
    const uintptr_t base = (uintptr_t)buffer;
    const uintptr_t end = TUTORIAL_DMA_BASE + TUTORIAL_DMA_SIZE;

    if ((buffer == NULL) || ((base & (TUTORIAL_CACHE_LINE - 1U)) != 0U) ||
        (capacity == 0U) || ((capacity & (TUTORIAL_CACHE_LINE - 1U)) != 0U) ||
        (length == 0U) || (length > capacity)) {
        return false;
    }
    /* Subtraction follows the range check, avoiding pointer addition overflow. */
    if ((base < TUTORIAL_DMA_BASE) || (base >= end) || (capacity > end - base)) {
        return false;
    }
    if (length > SIZE_MAX - (TUTORIAL_CACHE_LINE - 1U)) {
        return false;
    }
    const size_t bytes = (length + TUTORIAL_CACHE_LINE - 1U) &
                         ~(size_t)(TUTORIAL_CACHE_LINE - 1U);
    if ((bytes > capacity) || (bytes > INT32_MAX)) {
        return false;
    }
    *rounded = (int32_t)bytes;
    return true;
}

bool Tutorial_DmaTxPrepare(void *buffer, size_t capacity, size_t length)
{
    int32_t bytes;
    if (!checked_size(buffer, capacity, length, &bytes)) {
        return false;
    }
    __DSB();
    if ((SCB->CCR & SCB_CCR_DC_Msk) != 0U) {
        SCB_CleanDCache_by_Addr((uint32_t *)buffer, bytes);
    }
    /* Also orders CPU SRAM writes before DMA start when D-cache is disabled. */
    __DSB();
    return true;
}

static bool invalidate_rx(void *buffer, size_t capacity, size_t length)
{
    int32_t bytes;
    if (!checked_size(buffer, capacity, length, &bytes)) {
        return false;
    }
    __DSB();
    if ((SCB->CCR & SCB_CCR_DC_Msk) != 0U) {
        SCB_InvalidateDCache_by_Addr(buffer, bytes);
    }
    __DSB();
    return true;
}

bool Tutorial_DmaRxPrepare(void *buffer, size_t capacity, size_t length)
{
    /* Discard all previous contents in the affected exclusive lines. This
     * prevents dirty CPU lines from later overwriting incoming DMA data. */
    return invalidate_rx(buffer, capacity, length);
}

bool Tutorial_DmaRxComplete(void *buffer, size_t capacity, size_t length)
{
    /* Only after DMA has STOPPED writing: discard speculative/stale fills.
     * Never clean an RX destination after DMA; that could overwrite new data. */
    return invalidate_rx(buffer, capacity, length);
}
