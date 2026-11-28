// Executed with check_can_fd.py. Exercises the production driver, not a reimplementation.
#include "drv_can.h"
#include "freertos_tasks.h"
#include <assert.h>
#include <deque>
#include <stdio.h>

FDCAN_GlobalTypeDef mock_can[3] = {};
Mock_DWT mock_dwt = {1234};
FDCAN_HandleTypeDef hfdcan1 = {}, hfdcan2 = {}, hfdcan3 = {};
QueueHandle_t xQueue_Control_Rx = NULL, xQueue_IMU_Snapshot = NULL, xQueue_UI_Snapshot = NULL;
volatile uint32_t ulCAN_RxDropped = 0, ulUART_RxDropped = 0, ulUSB_RxDropped = 0;
bool Reset_Pitch_2 = false;

static uint32_t primask = 0, ipsr = 0, ticks = 1000, free_tx = 12;
static bool irq_enabled[3] = {true, true, true};
static unsigned starts[3] = {}, stops[3] = {}, inits[3] = {}, tx_calls = 0;
static uint32_t notifications[3] = {};
static int scheduler = taskSCHEDULER_NOT_STARTED;
static bool queue_full = false, fail_read = false, append_during_read = false;
static unsigned classic_calls = 0, fd_calls = 0, yields = 0;
static HAL_StatusTypeDef tx_status = HAL_OK, stop_status = HAL_OK, init_status = HAL_OK, start_status = HAL_OK;
static FDCAN_TxHeaderTypeDef sent_header = {};
static uint8_t sent_data[64] = {};
static Struct_CAN_Rx_Buffer received_classic = {};
static Struct_CANFD_Rx_Buffer received_fd = {};
static std::deque<FreeRTOS_Control_RxMsg_t> control_queue;
struct Frame { FDCAN_RxHeaderTypeDef header; uint8_t data[64]; };
static std::deque<Frame> rx[3][2];
static const uint8_t wire_lengths[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 12, 16, 20, 24, 32, 48, 64};

static unsigned bus(const FDCAN_HandleTypeDef *h) { return (unsigned)(h->Instance - mock_can); }
extern "C" uint32_t __get_PRIMASK(void) { return primask; }
extern "C" void __disable_irq(void) { primask = 1; }
extern "C" void __set_PRIMASK(uint32_t value) { primask = value; }
extern "C" void __DMB(void) {}
extern "C" uint32_t __get_IPSR(void) { return ipsr; }
extern "C" uint32_t HAL_GetTick(void) { return ticks; }
extern "C" uint32_t NVIC_GetEnableIRQ(IRQn_Type irq) { return irq_enabled[irq] ? 1U : 0U; }
extern "C" void HAL_NVIC_DisableIRQ(IRQn_Type irq) { irq_enabled[irq] = false; }
extern "C" void HAL_NVIC_EnableIRQ(IRQn_Type irq) { irq_enabled[irq] = true; }
extern "C" HAL_StatusTypeDef HAL_FDCAN_ConfigFilter(FDCAN_HandleTypeDef *h, const FDCAN_FilterTypeDef *filter)
{
    assert(primask == 0);
    assert(filter->FilterConfig == (bus(h) == 1 ? FDCAN_FILTER_TO_RXFIFO1 : FDCAN_FILTER_TO_RXFIFO0));
    return HAL_OK;
}
extern "C" HAL_StatusTypeDef HAL_FDCAN_ConfigGlobalFilter(FDCAN_HandleTypeDef *, uint32_t, uint32_t, uint32_t, uint32_t) { return HAL_OK; }
extern "C" HAL_StatusTypeDef HAL_FDCAN_ConfigFifoWatermark(FDCAN_HandleTypeDef *, uint32_t, uint32_t) { return HAL_OK; }
extern "C" HAL_StatusTypeDef HAL_FDCAN_ActivateNotification(FDCAN_HandleTypeDef *h, uint32_t value, uint32_t)
{
    notifications[bus(h)] = value;
    return HAL_OK;
}
extern "C" HAL_StatusTypeDef HAL_FDCAN_Start(FDCAN_HandleTypeDef *h)
{
    assert(primask == 0);
    starts[bus(h)]++;
    if (start_status != HAL_OK) { return start_status; }
    h->State = HAL_FDCAN_STATE_BUSY;
    h->Instance->CCCR &= ~FDCAN_CCCR_INIT;
    return HAL_OK;
}
extern "C" HAL_StatusTypeDef HAL_FDCAN_Stop(FDCAN_HandleTypeDef *h)
{
    assert(primask == 0);
    stops[bus(h)]++;
    h->State = stop_status == HAL_OK ? HAL_FDCAN_STATE_READY : HAL_FDCAN_STATE_ERROR;
    h->Instance->CCCR |= FDCAN_CCCR_INIT;
    return stop_status;
}
extern "C" HAL_StatusTypeDef HAL_FDCAN_Init(FDCAN_HandleTypeDef *h)
{
    assert(primask == 0 && h->Init.FrameFormat == FDCAN_FRAME_FD_NO_BRS);
    inits[bus(h)]++;
    h->State = init_status == HAL_OK ? HAL_FDCAN_STATE_READY : HAL_FDCAN_STATE_ERROR;
    return init_status;
}
extern "C" uint32_t HAL_FDCAN_GetTxFifoFreeLevel(const FDCAN_HandleTypeDef *) { return free_tx; }
extern "C" HAL_StatusTypeDef HAL_FDCAN_AddMessageToTxFifoQ(FDCAN_HandleTypeDef *, const FDCAN_TxHeaderTypeDef *header, const uint8_t *data)
{
    assert(primask == 1); // FIFO selection + RAM copy must exclude another sender.
    tx_calls++;
    sent_header = *header;
    memset(sent_data, 0xCC, sizeof(sent_data));
    // Model the production HAL's four-byte reads, including its final partial word.
    const unsigned bytes = (wire_lengths[header->DataLength] + 3U) & ~3U;
    memcpy(sent_data, data, bytes);
    return tx_status;
}
extern "C" uint32_t HAL_FDCAN_GetRxFifoFillLevel(const FDCAN_HandleTypeDef *h, uint32_t fifo)
{
    return (uint32_t)rx[bus(h)][fifo].size();
}
extern "C" HAL_StatusTypeDef HAL_FDCAN_GetRxMessage(FDCAN_HandleTypeDef *h, uint32_t fifo, FDCAN_RxHeaderTypeDef *header, uint8_t *data)
{
    if (fail_read) { return HAL_ERROR; }
    std::deque<Frame> &pending = rx[bus(h)][fifo];
    assert(!pending.empty());
    const Frame frame = pending.front();
    pending.pop_front();
    *header = frame.header;
    if (header->DataLength <= 15) { memcpy(data, frame.data, wire_lengths[header->DataLength]); }
    if (append_during_read) { pending.push_back(frame); }
    return HAL_OK;
}
extern "C" BaseType_t xTaskGetSchedulerState(void) { return scheduler; }
extern "C" BaseType_t xQueueSendFromISR(QueueHandle_t q, const void *message, BaseType_t *woken)
{
    assert(q == xQueue_Control_Rx && ipsr != 0 && primask == 0);
    if (queue_full) { return pdFALSE; }
    control_queue.push_back(*static_cast<const FreeRTOS_Control_RxMsg_t *>(message));
    *woken = pdTRUE;
    return pdPASS;
}
extern "C" void mock_yield(BaseType_t woken) { if (woken) { yields++; } }

static void classic_callback(Struct_CAN_Rx_Buffer *frame)
{
    assert(ipsr != 0 && primask == 0);
    received_classic = *frame;
    classic_calls++;
}
static void fd_callback(Struct_CANFD_Rx_Buffer *frame)
{
    assert(ipsr == 0 && primask == 0);
    received_fd = *frame;
    fd_calls++;
}
static Frame make_frame(bool fd, uint32_t dlc, uint8_t seed)
{
    Frame frame = {};
    frame.header.Identifier = 0x321;
    frame.header.FDFormat = fd ? FDCAN_FD_CAN : FDCAN_CLASSIC_CAN;
    frame.header.DataLength = dlc;
    for (unsigned i = 0; i < 64; ++i) { frame.data[i] = (uint8_t)(seed + i); }
    return frame;
}
static void interrupt(FDCAN_HandleTypeDef *h, unsigned fifo, uint32_t extra = 0)
{
    ipsr = 16;
    if (fifo == 0) { HAL_FDCAN_RxFifo0Callback(h, FDCAN_IT_RX_FIFO0_NEW_MESSAGE | extra); }
    else { HAL_FDCAN_RxFifo1Callback(h, FDCAN_IT_RX_FIFO1_NEW_MESSAGE | extra); }
    ipsr = 0;
}
static void dispatch(const FreeRTOS_Control_RxMsg_t &message)
{
    assert(message.type == FREERTOS_CONTROL_INPUT_CAN);
    CANFD_Dispatch_Rx(message.payload.can.can_instance, &message.payload.can.header,
                     message.payload.can.data, message.payload.can.generation);
}
static void dispatch_all(void)
{
    while (!control_queue.empty()) { dispatch(control_queue.front()); control_queue.pop_front(); }
}

static void test_transmit(void)
{
    uint8_t data[65];
    for (unsigned length = 0; length <= 64; ++length)
    {
        memset(data, 0xA5, sizeof(data));
        assert(CAN_Send_Data(&hfdcan2, 0x7FF, data, (uint16_t)length) == HAL_OK);
        unsigned wire = 0;
        while (wire_lengths[wire] < length) { ++wire; }
        assert(sent_header.DataLength == wire && sent_header.BitRateSwitch == FDCAN_BRS_OFF);
        assert(sent_header.FDFormat == (length <= 8 ? FDCAN_CLASSIC_CAN : FDCAN_FD_CAN));
        assert(sent_header.Identifier == 0x7FF && sent_header.IdType == FDCAN_STANDARD_ID);
        const unsigned copied = (wire_lengths[wire] + 3U) & ~3U;
        for (unsigned i = 0; i < copied; ++i) { assert(sent_data[i] == (i < length ? 0xA5 : 0)); }
        assert(primask == 0);
    }
    assert(CAN2_IO_Diagnostics.Tx_Classic_Accepted == 9 && CAN2_IO_Diagnostics.Tx_FD_Accepted == 56);
    assert(CAN_Send_Data(&hfdcan2, 0, NULL, 0) == HAL_OK);
    unsigned before = tx_calls;
    assert(CAN_Send_Data(NULL, 0, data, 8) == HAL_ERROR);
    assert(CAN_Send_Data(&hfdcan2, 0x800, data, 8) == HAL_ERROR);
    assert(CAN_Send_Data(&hfdcan2, 1, NULL, 8) == HAL_ERROR);
    assert(CAN_Send_Data(&hfdcan2, 1, data, 65) == HAL_ERROR);
    assert(CAN_Send_Data(&hfdcan2, 1, data, 65535) == HAL_ERROR);
    hfdcan2.Init.TxElmtSize = FDCAN_DATA_BYTES_8;
    assert(CAN_Send_Data(&hfdcan2, 1, data, 64) == HAL_ERROR);
    assert(CAN_Send_Data(&hfdcan2, 1, data, 8) == HAL_OK);
    hfdcan2.Init.TxElmtSize = FDCAN_DATA_BYTES_64;
    hfdcan2.Init.FrameFormat = FDCAN_FRAME_CLASSIC;
    assert(CAN_Send_Data(&hfdcan2, 1, data, 64) == HAL_ERROR);
    hfdcan2.Init.FrameFormat = FDCAN_FRAME_FD_NO_BRS;
    assert(tx_calls == before + 1);
    free_tx = 0;
    assert(CAN_Send_Data(&hfdcan2, 1, data, 64) == HAL_BUSY);
    assert(tx_calls == before + 1);
    free_tx = 12;
    tx_status = HAL_ERROR;
    assert(CAN_Send_Data(&hfdcan2, 1, data, 64) == HAL_ERROR);
    tx_status = HAL_OK;
    primask = 1;
    assert(CAN_Send_Data(&hfdcan2, 1, data, 8) == HAL_OK && primask == 1);
    primask = 0;
    puts("PASS: lengths 0..64, format/BRS/DLC, zero padding, HAL word reads, invalid inputs and FIFO busy.");
}

static void test_receive(void)
{
    scheduler = taskSCHEDULER_RUNNING;
    xQueue_Control_Rx = &control_queue;
    rx[0][0].push_back(make_frame(false, 8, 1));
    rx[0][0].push_back(make_frame(true, 15, 2));
    rx[0][0].push_back(make_frame(true, 8, 3));
    rx[0][0].push_back(make_frame(false, 2, 4));
    interrupt(&hfdcan1, 0);
    assert(classic_calls == 2 && fd_calls == 0 && control_queue.size() == 2 && yields == 1);
    assert(received_classic.Data[0] == 4 && received_classic.Data[1] == 5);
    for (unsigned i = 2; i < 8; ++i) { assert(received_classic.Data[i] == 0); }
    assert(CAN1_Manage_Object.Callback_Function == classic_callback);
    dispatch(control_queue.front()); control_queue.pop_front();
    assert(fd_calls == 1 && received_fd.Length == 64 && received_fd.Data[63] == 65);
    dispatch_all();
    assert(fd_calls == 2 && received_fd.Length == 8 && received_fd.Data[7] == 10 && received_fd.Data[8] == 0);
    for (uint32_t dlc = 0; dlc <= 15; ++dlc)
    {
        rx[1][1].push_back(make_frame(true, dlc, 9));
        interrupt(&hfdcan2, 1);
        dispatch_all();
        assert(received_fd.Length == wire_lengths[dlc]);
    }
    // Lost/full signals are diagnostic events, not reasons to restart the peripheral.
    interrupt(&hfdcan1, 0, FDCAN_IT_RX_FIFO0_FULL | FDCAN_IT_RX_FIFO0_MESSAGE_LOST);
    assert(CAN1_IO_Diagnostics.Rx_FIFO_Full_Events == 1 && CAN1_IO_Diagnostics.Rx_FIFO_Lost_Events == 1);
    assert(CAN1_Recovery_Diagnostics.Recovery_Pending == 0);
    rx[0][0].push_back(make_frame(true, 15, 1));
    queue_full = true;
    interrupt(&hfdcan1, 0);
    queue_full = false;
    assert(CAN1_IO_Diagnostics.Rx_Queue_Full == 1 && control_queue.empty());
    xQueue_Control_Rx = NULL;
    rx[0][0].push_back(make_frame(true, 15, 1));
    interrupt(&hfdcan1, 0);
    xQueue_Control_Rx = &control_queue;
    assert(CAN1_IO_Diagnostics.Rx_Queue_Not_Ready == 1);
    rx[2][0].push_back(make_frame(true, 15, 1)); // no FD callback registered on CAN3
    interrupt(&hfdcan3, 0);
    assert(CAN3_IO_Diagnostics.Rx_No_Callback == 1 && control_queue.empty());
    rx[0][0].push_back(make_frame(false, 9, 1));
    rx[0][0].push_back(make_frame(true, 16, 1));
    interrupt(&hfdcan1, 0);
    assert(CAN1_IO_Diagnostics.Rx_Invalid == 2);
    unsigned before = classic_calls;
    rx[0][0].push_back(make_frame(false, 8, 1));
    fail_read = true;
    interrupt(&hfdcan1, 0);
    fail_read = false;
    assert(classic_calls == before && CAN1_IO_Diagnostics.Rx_Read_Errors == 1);
    rx[0][0].clear();
    // Continuous arrivals cannot turn one IRQ invocation into an unbounded loop.
    rx[0][0].push_back(make_frame(false, 8, 1));
    append_during_read = true;
    interrupt(&hfdcan1, 0);
    append_during_read = false;
    assert(classic_calls == before + 1 && rx[0][0].size() == 1);
    rx[0][0].clear();
    assert(CAN_Register_FD_Callback(&hfdcan1, fd_callback) == HAL_BUSY);
    puts("PASS: classic ISR/FD task isolation, all FD DLCs, FIFO0/1, bounded draining and failure/drop paths.");
}

static void test_recovery(void)
{
    rx[1][1].push_back(make_frame(true, 15, 7));
    interrupt(&hfdcan2, 1);
    const unsigned before_fd = fd_calls;
    const unsigned other_starts = starts[0] + starts[2];
    hfdcan2.Instance->PSR = FDCAN_PSR_BO;
    hfdcan2.Instance->CCCR = FDCAN_CCCR_INIT;
    HAL_FDCAN_ErrorStatusCallback(&hfdcan2, FDCAN_IT_BUS_OFF);
    uint8_t data[64] = {};
    assert(CAN_Send_Data(&hfdcan2, 1, data, 64) == HAL_BUSY);
    CAN_Error_Service();
    assert(CAN2_Recovery_Diagnostics.Restart_Successes == 1 && irq_enabled[1]);
    assert(starts[0] + starts[2] == other_starts && hfdcan2.Init.FrameFormat == FDCAN_FRAME_FD_NO_BRS);
    CAN_Error_Service(); // BO still high but INIT cleared: do not interrupt hardware recovery.
    assert(CAN2_Recovery_Diagnostics.Restart_Attempts == 1);
    hfdcan2.Instance->PSR = 0;
    dispatch_all();
    assert(fd_calls == before_fd && CAN2_IO_Diagnostics.Rx_Stale == 1);
    assert(CAN_Send_Data(&hfdcan2, 1, data, 64) == HAL_OK);
    rx[1][1].push_back(make_frame(true, 15, 8));
    interrupt(&hfdcan2, 1); dispatch_all();
    assert(fd_calls == before_fd + 1);

    // Serious RAM error: failed reinitialization must retain recovery and throttle retries.
    ticks += 100;
    hfdcan2.ErrorCode = HAL_FDCAN_ERROR_RAM_ACCESS;
    HAL_FDCAN_ErrorCallback(&hfdcan2);
    init_status = HAL_ERROR;
    CAN_Error_Service();
    assert(CAN2_Recovery_Diagnostics.Restart_Failures == 1 && CAN2_Recovery_Diagnostics.Recovery_Pending == 1);
    const unsigned attempts = CAN2_Recovery_Diagnostics.Restart_Attempts;
    ticks += 99;
    CAN_Error_Service();
    assert(CAN2_Recovery_Diagnostics.Restart_Attempts == attempts);
    ticks += 1;
    init_status = HAL_OK;
    irq_enabled[1] = false;
    CAN_Error_Service();
    assert(!irq_enabled[1] && CAN2_Recovery_Diagnostics.Recovery_Pending == 0);
    assert(CAN2_Recovery_Diagnostics.Restart_Successes == 2 && inits[1] == 2);
    assert(CAN2_Manage_Object.FD_Callback_Function == fd_callback);
    assert(CAN_Send_Data(&hfdcan2, 1, data, 64) == HAL_OK);
    assert(primask == 0);
    puts("PASS: Bus-Off gating, deferred recovery, stale-generation rejection, per-bus isolation and 100ms retry.");
}

int main(void)
{
    FDCAN_HandleTypeDef *handles[] = {&hfdcan1, &hfdcan2, &hfdcan3};
    for (unsigned i = 0; i < 3; ++i)
    {
        handles[i]->Instance = &mock_can[i];
        handles[i]->Init.FrameFormat = FDCAN_FRAME_FD_NO_BRS;
        handles[i]->Init.TxElmtSize = FDCAN_DATA_BYTES_64;
        handles[i]->Init.RxFifo0ElmtsNbr = i == 1 ? 0U : 12U;
        handles[i]->Init.RxFifo1ElmtsNbr = i == 1 ? 12U : 0U;
        CAN_Init(handles[i], classic_callback);
        assert((notifications[i] & FDCAN_IT_BUS_OFF) != 0);
    }
    assert(CAN_Register_FD_Callback(&hfdcan1, fd_callback) == HAL_OK);
    assert(CAN_Register_FD_Callback(&hfdcan2, fd_callback) == HAL_OK);
    assert(CAN_Register_FD_Callback(NULL, fd_callback) == HAL_ERROR);
    test_transmit();
    test_receive();
    test_recovery();
    // Existing UART payload remains the largest member: adding FD does not grow this queue item.
    assert(sizeof(FreeRTOS_CAN_RxMsg_t) <= sizeof(FreeRTOS_UART_RxMsg_t));
    printf("Queue item: %u bytes, CAN payload item: %u bytes.\n",
           (unsigned)sizeof(FreeRTOS_Control_RxMsg_t), (unsigned)sizeof(FreeRTOS_CAN_RxMsg_t));
    return 0;
}
