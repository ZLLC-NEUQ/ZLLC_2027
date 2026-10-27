/**
 * @file freertos_tasks.cpp
 * @brief 控制任务独占机器人状态；中断直接把接收消息投进控制队列，IMU 交接完整快照。
 *
 * 接收链：ISR 构造 FreeRTOS_Control_RxMsg_t -> xQueue_Control_Rx -> 控制任务业务回调。
 * IMU 链：IMU 私有解算区 -> 最新完整快照 -> 控制侧 IMU 副本。
 * 在线检测、板间打包和 UI 快照均由控制任务按原周期执行。
 */
#include "freertos_tasks.h"
#include "tsk_config_and_callback.h"
#include "drv_can.h"
#include "drv_uart.h"
#include "drv_usb.h"
#include "dvc_imu.h"
#include "string.h"

QueueHandle_t xQueue_Control_Rx = NULL;
QueueHandle_t xQueue_IMU_Snapshot = NULL;
QueueHandle_t xQueue_UI_Snapshot = NULL;

volatile uint32_t ulCAN_RxDropped = 0;
volatile uint32_t ulUART_RxDropped = 0;
volatile uint32_t ulUSB_RxDropped = 0;
volatile uint32_t ulControl_DeadlineMissed = 0;
static uint32_t ulCurrentRxCycle = 0;

static void FreeRTOS_CreateObjects(void)
{
    xQueue_Control_Rx = xQueueCreate(FREERTOS_CONTROL_RX_QUEUE_LEN, sizeof(FreeRTOS_Control_RxMsg_t));
    configASSERT(xQueue_Control_Rx);
    xQueue_IMU_Snapshot = xQueueCreate(1U, sizeof(Struct_IMU_Snapshot));
    configASSERT(xQueue_IMU_Snapshot);
    xQueue_UI_Snapshot = xQueueCreate(1U, sizeof(FreeRTOS_UI_Snapshot_t));
    configASSERT(xQueue_UI_Snapshot);
}

static void FreeRTOS_CreateTasks(void)
{
    BaseType_t result;
    result = xTaskCreate(Task_100us_Fast, "TaskIMU", configMINIMAL_STACK_SIZE,
                         NULL, ROBOT_TASK_PRIORITY_IMU, NULL);
    configASSERT(result == pdPASS);
    result = xTaskCreate(Task_1ms_Control, "TaskControl", configMINIMAL_STACK_SIZE * 2,
                         NULL, ROBOT_TASK_PRIORITY_CONTROL, NULL);
    configASSERT(result == pdPASS);
    result = xTaskCreate(Task_Background, "TaskBkgnd", configMINIMAL_STACK_SIZE * 2,
                         NULL, ROBOT_TASK_PRIORITY_BACKGROUND, NULL);
    configASSERT(result == pdPASS);
}

void FreeRTOS_Start(void)
{
    FreeRTOS_CreateObjects();
    FreeRTOS_CreateTasks();
    vTaskStartScheduler();
    for (;;) {}
}

uint32_t FreeRTOS_GetRxCycle(void)
{
    return ulCurrentRxCycle;
}

BaseType_t FreeRTOS_SubmitUSBFromISR(const uint8_t *data, uint32_t length,
                                  BaseType_t *pxHigherPriorityTaskWoken)
{
    if ((data == NULL) || (length == 0U) || (length > FREERTOS_USB_RX_MAX_LEN) ||
        (xQueue_Control_Rx == NULL))
    {
        ++ulUSB_RxDropped;
        return pdFALSE;
    }
    FreeRTOS_Control_RxMsg_t message = {};
    message.type = FREERTOS_CONTROL_INPUT_USB;
    message.payload.usb.length = (uint16_t)length;
    memcpy(message.payload.usb.data, data, length);
    const BaseType_t result = xQueueSendFromISR(xQueue_Control_Rx, &message, pxHigherPriorityTaskWoken);
    if (result != pdTRUE) { ++ulUSB_RxDropped; }
    return result;
}

static void FreeRTOS_DispatchCAN(FreeRTOS_CAN_RxMsg_t &message)
{
    // Struct_CAN_Manage_Object *object = NULL;
    // switch (message.can_instance)
    // {
    //     case 1: object = &CAN1_Manage_Object; break;
    //     case 2: object = &CAN2_Manage_Object; break;
    //     case 3: object = &CAN3_Manage_Object; break;
    //     default: break;
    // }
    // if ((object != NULL) && (object->Callback_Function != NULL))
    // {
    //     ulCurrentRxCycle = message.rx_cycle;
    //     /* CAN 管理接收副本仅由控制任务写；ISR 使用自己的队列消息。 */
    //     object->Rx_Buffer.Header = message.header;
    //     memcpy(object->Rx_Buffer.Data, message.data, sizeof(message.data));
    //     object->Rx_Buffer.Rx_Cycle = message.rx_cycle;
    //     object->Callback_Function(&object->Rx_Buffer);
    // }
}

static void FreeRTOS_DispatchUART(FreeRTOS_UART_RxMsg_t &message)
{
    Struct_UART_Manage_Object *object = NULL;
    switch (message.uart_instance)
    {
        case 1: object = &UART1_Manage_Object; break;
        case 5: object = &UART5_Manage_Object; break;
        case 7: object = &UART7_Manage_Object; break;
        case 8: object = &UART8_Manage_Object; break;
        case 9: object = &UART9_Manage_Object; break;
        case 10: object = &UART10_Manage_Object; break;
        default: break;
    }
    if ((object != NULL) && (object->Callback_Function != NULL) &&
        (message.length > 0U) && (message.length <= sizeof(message.data)))
    {
        ulCurrentRxCycle = message.rx_cycle;
        /* 直接解析消息副本；不写回仍被 DMA 使用的 Rx_Buffer。 */
        object->Callback_Function(message.data, message.length);
    }
}

static void FreeRTOS_ProcessControlInputs(void)
{
    /* 只处理本周期开始时已有的消息，防止持续来包使控制循环无法结束。 */
    UBaseType_t count = uxQueueMessagesWaiting(xQueue_Control_Rx);
    if (count > FREERTOS_CONTROL_RX_BUDGET)
        count = FREERTOS_CONTROL_RX_BUDGET;
    FreeRTOS_Control_RxMsg_t message;
    while ((count-- > 0U) && (xQueueReceive(xQueue_Control_Rx, &message, 0U) == pdTRUE))
    {
        switch (message.type)
        {
            case FREERTOS_CONTROL_INPUT_CAN:
                FreeRTOS_DispatchCAN(message.payload.can);  //延迟过大,实际上是直接在ISR中解析了,这里不再解析
                break;
            case FREERTOS_CONTROL_INPUT_UART:
                FreeRTOS_DispatchUART(message.payload.uart);
                break;
            case FREERTOS_CONTROL_INPUT_USB:
                // if (MiniPC_USB_Manage_Object.Callback_Function != NULL)
                // {
                //     MiniPC_USB_Manage_Object.Rx_Buffer_Length = message.payload.usb.length;
                //     memcpy(MiniPC_USB_Manage_Object.Rx_Buffer, message.payload.usb.data,
                //            message.payload.usb.length);
                //     MiniPC_USB_Manage_Object.Callback_Function(message.payload.usb.data,
                //                                               message.payload.usb.length);
                // }
                break;
            default: break;
        }
    }
}

uint32_t TIM4_delta = 0;
void Task_100us_Fast(void *pvParameters)
{
    (void)pvParameters;
    const TickType_t period = pdMS_TO_TICKS(1U);
    TickType_t lastWake = xTaskGetTickCount();
    configASSERT(period > 0U);
    for (;;)
    {
        vTaskDelayUntil(&lastWake, period);
        /* 历史入口名称保留，实际周期 1ms；这里只操作 IMU 私有解算区。 */
        // taskENTER_CRITICAL();
        uint32_t time1 = DWT_GetTimeline_us();
        Task100us_TIM4_Callback();
        TIM4_delta = DWT_GetTimeline_us() - time1;
        // taskEXIT_CRITICAL();
    }
}

void Task_1ms_Control(void *pvParameters)
{
    (void)pvParameters;
    const TickType_t period = pdMS_TO_TICKS(1U);
    TickType_t lastWake = xTaskGetTickCount();
    TickType_t lastAlive = lastWake;
    TickType_t lastBoardComm = lastWake;
    TickType_t lastUI = lastWake;
    configASSERT(period > 0U);
    for (;;)
    {
        vTaskDelayUntil(&lastWake, period);
        const TickType_t now = xTaskGetTickCount();
        Task_IMU_ApplyLatestSnapshot();
        FreeRTOS_ProcessControlInputs();
        if ((TickType_t)(now - lastAlive) >= pdMS_TO_TICKS(50U))
        {
            lastAlive = now;
            Task50ms_Alive_Callback();
        }
        if (Task_Control_IsReady() == 0U)
        {
            lastBoardComm = now;
        }
        else if ((TickType_t)(now - lastBoardComm) >= pdMS_TO_TICKS(10U))
        {
            lastBoardComm = now;
            Task10ms_BoardComm_Callback();
        }
        Task1ms_TIM5_Callback();
        if ((TickType_t)(now - lastUI) >= pdMS_TO_TICKS(50U))
        {
            lastUI = now;
            Task_UI_PublishSnapshot();
        }
        if ((TickType_t)(xTaskGetTickCount() - lastWake) >= period)
        {
            ++ulControl_DeadlineMissed;
        }
    }
}

float back_dt;
void Task_Background(void *pvParameters)
{
    (void)pvParameters;
    for (;;)
    {
        static uint32_t last_back_cnt = 0;
        back_dt = DWT_GetDeltaT(&last_back_cnt);
        Task_Loop();
        vTaskDelay(pdMS_TO_TICKS(50U));
    }
}

extern "C" void vApplicationMallocFailedHook(void)
{
    taskDISABLE_INTERRUPTS();
    for (;;) {}
}

extern "C" void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    (void)pcTaskName;
    taskDISABLE_INTERRUPTS();
    for (;;) {}
}
