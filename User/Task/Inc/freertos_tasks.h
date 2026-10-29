/**
 * @file freertos_tasks.h
 * @brief 控制任务、1ms 存活任务、ISR 接收队列与完整快照
 * @author HGCSJ
 * @version V1.1
 * @date 2026-10-30 27赛季定稿
 * @copyright ZLLC 2027
 */
#ifndef FREERTOS_TASKS_H
#define FREERTOS_TASKS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "stm32h7xx_hal.h"
#include "config.h"

#define FREERTOS_UART_RX_MAX_LEN        128U
#define FREERTOS_USB_RX_MAX_LEN         48U
/* CAN/UART/USB 三条来源共用这条队列，深度按最坏突发留余量。 */
#define FREERTOS_CONTROL_RX_QUEUE_LEN   128U
/* 每周期最多处理 64 条，后到消息留到下个周期。 */
#define FREERTOS_CONTROL_RX_BUDGET      64U
#define FREERTOS_ALIVE_PERIOD_MS        1U
#define FREERTOS_MODULE_ALIVE_PERIOD_MS 50U

typedef struct
{
    uint8_t can_instance;
#if PERFORMANCE_TEST_ENABLE
    uint32_t rx_cycle;                 /**< 本帧 ISR 采集的测试用 DWT 周期。 */
#endif
    FDCAN_RxHeaderTypeDef header;
    uint8_t data[8];
} FreeRTOS_CAN_RxMsg_t;

typedef struct
{
    uint8_t uart_instance;
#if PERFORMANCE_TEST_ENABLE
    uint32_t rx_cycle;
#endif
    uint16_t length;
    uint8_t data[FREERTOS_UART_RX_MAX_LEN];
} FreeRTOS_UART_RxMsg_t;

typedef struct
{
    uint16_t length;
    uint8_t data[FREERTOS_USB_RX_MAX_LEN];
} FreeRTOS_USB_RxMsg_t;

typedef enum
{
    FREERTOS_CONTROL_INPUT_CAN,
    FREERTOS_CONTROL_INPUT_UART,
    FREERTOS_CONTROL_INPUT_USB
} FreeRTOS_Control_InputType_t;

/** @brief 中断按值构造后直接入队，消息不持有 DMA 缓冲区或其它上下文栈的指针。 */
typedef struct
{
    FreeRTOS_Control_InputType_t type;
    union
    {
        FreeRTOS_CAN_RxMsg_t can;
        FreeRTOS_UART_RxMsg_t uart;
        FreeRTOS_USB_RxMsg_t usb;
    } payload;
} FreeRTOS_Control_RxMsg_t;

/** @brief 控制任务发布，后台只读取自己的 UI 副本。 */
typedef struct
{
    uint8_t ready;
    uint8_t refresh;
    uint8_t robot_id;
    uint8_t chassis_control_type;
    uint8_t gimbal_control_type;
    uint8_t fric_status;
    float pitch_angle;
    float supercap_voltage;
    float chassis_gimbal_diff;
} FreeRTOS_UI_Snapshot_t;

extern QueueHandle_t xQueue_Control_Rx;
extern QueueHandle_t xQueue_IMU_Snapshot;
extern QueueHandle_t xQueue_UI_Snapshot;

/* 调试器可查看队列溢出/启动期间丢帧。性能测试见 dvc_performance.h。 */
extern volatile uint32_t ulCAN_RxDropped;
extern volatile uint32_t ulUART_RxDropped;
extern volatile uint32_t ulUSB_RxDropped;

void FreeRTOS_Start(void);
void Task_100us_Fast(void *pvParameters);
void Task_1ms_Control(void *pvParameters);
void Task_1ms_Alive(void *pvParameters);
void Task_Background(void *pvParameters);

/** @brief USB 中断提交完整副本，业务回调只在控制任务执行。 */
BaseType_t FreeRTOS_SubmitUSBFromISR(const uint8_t *data, uint32_t length,
                                  BaseType_t *pxHigherPriorityTaskWoken);
/** @brief 当前控制任务正在分派的消息接收时间，单位 DWT 周期。 */
#if PERFORMANCE_TEST_ENABLE
uint32_t FreeRTOS_GetRxCycle(void);
#endif

#ifdef __cplusplus
}
#endif
#endif
