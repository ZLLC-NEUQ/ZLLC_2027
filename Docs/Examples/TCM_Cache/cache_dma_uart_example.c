#include "cache_dma.h"
#include <string.h>

enum { BUFFER_BYTES = 256 };
static uint8_t tx_buffer[BUFFER_BYTES] TUTORIAL_DMA_BUFFER;
static uint8_t rx_buffer[BUFFER_BYTES] TUTORIAL_DMA_BUFFER;

/* Metadata stays OUTSIDE DMA cache lines. Volatile alone is not a lock: the
 * application mutex serializes all task APIs, and brief PRIMASK sections guard
 * transitions against IRQ callbacks. Neither DMA nor a second task may access
 * these private buffers by another path. */
static UART_HandleTypeDef *bound_uart;
static volatile bool tx_busy;
static volatile bool rx_busy;
static volatile bool rx_ready;
static volatile bool fault;
static volatile bool recovering;
static size_t rx_length;

static uint32_t irq_lock(void)
{
    uint32_t saved = __get_PRIMASK();
    __disable_irq();
    __DMB();
    return saved;
}

static void irq_unlock(uint32_t saved)
{
    __DMB();
    __set_PRIMASK(saved);
}

static bool valid_dma(const DMA_HandleTypeDef *dma, uint32_t direction)
{
    return (dma != NULL) && IS_DMA_STREAM_INSTANCE(dma->Instance) &&
           (dma->Init.Direction == direction) &&
           (dma->Init.Mode == DMA_NORMAL) &&
           (dma->Init.PeriphInc == DMA_PINC_DISABLE) &&
           (dma->Init.MemInc == DMA_MINC_ENABLE) &&
           (dma->Init.PeriphDataAlignment == DMA_PDATAALIGN_BYTE) &&
           (dma->Init.MemDataAlignment == DMA_MDATAALIGN_BYTE) &&
           (dma->Init.MemBurst == DMA_MBURST_SINGLE) &&
           (dma->Init.PeriphBurst == DMA_PBURST_SINGLE);
}

bool Tutorial_UartBind(UART_HandleTypeDef *uart)
{
    /* Call once, before this UART is used. DMA/DMAMUX clocks, routing and IRQs
     * must already be configured. This is not a circular/IDLE receive example. */
    if ((bound_uart != NULL) || (uart == NULL) ||
        (uart->Init.WordLength != UART_WORDLENGTH_8B) ||
        (uart->Init.Parity != UART_PARITY_NONE) ||
        (uart->Init.Mode != UART_MODE_TX_RX) ||
        (uart->hdmatx == uart->hdmarx) ||
        (uart->gState != HAL_UART_STATE_READY) ||
        (uart->RxState != HAL_UART_STATE_READY) ||
        !valid_dma(uart->hdmatx, DMA_MEMORY_TO_PERIPH) ||
        !valid_dma(uart->hdmarx, DMA_PERIPH_TO_MEMORY)) {
        return false;
    }
    bound_uart = uart;
    return true;
}

HAL_StatusTypeDef Tutorial_UartSend(const uint8_t *source, size_t length)
{
    if ((bound_uart == NULL) || (source == NULL) || (length == 0U) ||
        (length > sizeof(tx_buffer))) {
        return HAL_ERROR;
    }
    uint32_t saved = irq_lock();
    if (tx_busy || fault || recovering) {
        irq_unlock(saved);
        return HAL_BUSY;
    }
    tx_busy = true;
    irq_unlock(saved);

    /* Copy into persistent private storage. The caller may reuse source after
     * this function returns; tx_buffer remains owned until the TX callback. */
    memcpy(tx_buffer, source, length);
    if (!Tutorial_DmaTxPrepare(tx_buffer, sizeof(tx_buffer), length)) {
        fault = true;
        return HAL_ERROR;
    }
    saved = irq_lock();
    if (fault) {
        irq_unlock(saved);
        return HAL_ERROR;
    }
    HAL_StatusTypeDef result = HAL_UART_Transmit_DMA(bound_uart, tx_buffer,
                                                    (uint16_t)length);
    /* On ANY start error, require abort/recovery before releasing ownership.
     * This also handles HAL_BUSY caused by unexpected external UART use. */
    if (result != HAL_OK) {
        fault = true;
    }
    irq_unlock(saved);
    return result;
}

HAL_StatusTypeDef Tutorial_UartReceive(size_t length)
{
    if ((bound_uart == NULL) || (length == 0U) || (length > sizeof(rx_buffer))) {
        return HAL_ERROR;
    }
    uint32_t saved = irq_lock();
    if (rx_busy || rx_ready || fault || recovering) {
        irq_unlock(saved);
        return HAL_BUSY;
    }
    rx_busy = true;
    rx_length = length;
    irq_unlock(saved);

    if (!Tutorial_DmaRxPrepare(rx_buffer, sizeof(rx_buffer), length)) {
        fault = true;
        return HAL_ERROR;
    }
    saved = irq_lock();
    if (fault) {
        irq_unlock(saved);
        return HAL_ERROR;
    }
    HAL_StatusTypeDef result = HAL_UART_Receive_DMA(bound_uart, rx_buffer,
                                                   (uint16_t)length);
    if (result != HAL_OK) {
        fault = true;
    }
    irq_unlock(saved);
    return result;
}

void Tutorial_UartTxCompleteHook(UART_HandleTypeDef *uart)
{
    if ((uart == bound_uart) && tx_busy && !fault && !recovering) {
        /* Normal-mode HAL UART TX completion follows the final UART TC IRQ. */
        __DSB();
        tx_busy = false;
    }
}

void Tutorial_UartRxCompleteHook(UART_HandleTypeDef *uart)
{
    if ((uart == bound_uart) && rx_busy && !fault && !recovering) {
        /* Normal-mode DMA has finished. No access to rx_buffer before this. */
        if (!Tutorial_DmaRxComplete(rx_buffer, sizeof(rx_buffer), rx_length)) {
            fault = true;
            return;
        }
        rx_busy = false;
        __DMB();
        rx_ready = true;
    }
}

void Tutorial_UartErrorHook(UART_HandleTypeDef *uart)
{
    if (uart == bound_uart) {
        /* Do not assume this callback proves both DMA streams are stopped.
         * Keep all ownership until task-context AbortAndReset succeeds. */
        fault = true;
    }
}

bool Tutorial_UartTakeRx(uint8_t *destination, size_t capacity, size_t *length)
{
    if ((destination == NULL) || (length == NULL)) {
        return false;
    }
    uint32_t saved = irq_lock();
    if (!rx_ready || fault || recovering || (capacity < rx_length)) {
        irq_unlock(saved);
        return false;
    }
    const size_t received = rx_length;
    irq_unlock(saved);

    /* rx_ready prevents a new RX operation until the copy has completed. */
    memcpy(destination, rx_buffer, received);
    saved = irq_lock();
    if (fault) {
        irq_unlock(saved);
        return false; /* Discard destination on false; a UART error intervened. */
    }
    *length = received;
    rx_ready = false;
    irq_unlock(saved);
    return true;
}

bool Tutorial_UartHasFault(void)
{
    return fault;
}

HAL_StatusTypeDef Tutorial_UartAbortAndReset(void)
{
    if (bound_uart == NULL) {
        return HAL_ERROR;
    }
    /* Task/main only, with interrupts and the HAL tick running. A failed abort
     * keeps buffers unavailable. Never spin in an IRQ waiting for this. */
    uint32_t saved = irq_lock();
    recovering = true;
    fault = true;
    irq_unlock(saved);

    HAL_StatusTypeDef result = HAL_UART_Abort(bound_uart);
    if (result != HAL_OK) {
        recovering = false;
        return result;
    }
    /* An earlier HAL error may already have cleared UART DMAT/DMAR. Check the
     * streams themselves; UART_Abort only follows still-enabled requests. */
    DMA_HandleTypeDef *streams[2] = {bound_uart->hdmatx, bound_uart->hdmarx};
    for (size_t i = 0U; i < 2U; ++i) {
        DMA_Stream_TypeDef *stream = (DMA_Stream_TypeDef *)streams[i]->Instance;
        if ((stream->CR & DMA_SxCR_EN) != 0U) {
            result = HAL_DMA_Abort(streams[i]);
            if ((result != HAL_OK) ||
                ((stream->CR & DMA_SxCR_EN) != 0U)) {
                recovering = false;
                return HAL_ERROR;
            }
        }
        /* A previous abort timeout can leave State == ERROR even after the
         * hardware eventually clears EN. EN == 0 alone is not enough for
         * HAL_DMA_Start_IT(), which also requires a READY handle. Never assign
         * READY manually: board-level DeInit/Init or controlled system recovery
         * must restore a failed handle before this API can report success. */
        if (HAL_DMA_GetState(streams[i]) != HAL_DMA_STATE_READY) {
            recovering = false;
            return HAL_ERROR;
        }
    }
    __DSB();
    if (!Tutorial_DmaRxComplete(rx_buffer, sizeof(rx_buffer), sizeof(rx_buffer))) {
        recovering = false;
        return HAL_ERROR;
    }
    saved = irq_lock();
    tx_busy = false;
    rx_busy = false;
    rx_ready = false;
    rx_length = 0U;
    fault = false;
    recovering = false;
    irq_unlock(saved);
    return HAL_OK;
}
