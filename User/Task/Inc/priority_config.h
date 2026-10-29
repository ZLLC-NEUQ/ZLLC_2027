/**
 * @file priority_config.h
 * @brief 机器人中断和任务优先级配置。
 * @author HGCSJ
 * @version V1.1
 * @date 2026-10-30 27赛季定稿
 * @copyright ZLLC 2027
 *
 * NVIC 数值越小越优先，GROUP4 的子优先级统一为 0。
 * FreeRTOS 任务数值越大越优先；任务宏在 FreeRTOSConfig.h 定义
 * configMAX_PRIORITIES 后使用。
 */
#ifndef PRIORITY_CONFIG_H
#define PRIORITY_CONFIG_H

/* TIM6 只推进 HAL 时基，不调用 FreeRTOS API；须能抢占 HAL 超时等待。 */
#define ROBOT_IRQ_PRIORITY_HAL_TICK       4U
#define ROBOT_IRQ_PRIORITY_RTOS_API_MIN   5U
#define ROBOT_IRQ_PRIORITY_SENSOR         5U
#define ROBOT_IRQ_PRIORITY_CAN            5U
/* UART5 / USART1：遥控器接收；对应 UART 和 RX DMA 使用同一等级。 */
#define ROBOT_IRQ_PRIORITY_CONTROL_RX     6U
/* UART7 / UART8 / UART9 / USART10：其他串口接收及裁判系统。 */
#define ROBOT_IRQ_PRIORITY_COMM_RX        7U
#define ROBOT_IRQ_PRIORITY_USB            8U
#define ROBOT_IRQ_PRIORITY_UART_TX        9U
#define ROBOT_IRQ_PRIORITY_KERNEL        15U
#define ROBOT_IRQ_SUB_PRIORITY            0U

/* 接收任务已删除：中断直接把消息投进控制队列，不再有高于控制任务的接收中继。 */
#define ROBOT_TASK_PRIORITY_IMU          (configMAX_PRIORITIES - 2U)
#define ROBOT_TASK_PRIORITY_CONTROL      (configMAX_PRIORITIES - 5U)
#define ROBOT_TASK_PRIORITY_TIMER        (configMAX_PRIORITIES - 6U)
/* 必须低于控制任务：在线扫描只能在控制任务本轮完成/阻塞后进入共享状态临界区。 */
#define ROBOT_TASK_PRIORITY_ALIVE        (configMAX_PRIORITIES - 7U)
#define ROBOT_TASK_PRIORITY_BACKGROUND   (configMAX_PRIORITIES - 8U)

#if (ROBOT_IRQ_PRIORITY_RTOS_API_MIN == 0U) || \
    (ROBOT_IRQ_PRIORITY_RTOS_API_MIN > ROBOT_IRQ_PRIORITY_KERNEL)
#error "FreeRTOS ISR priority threshold is invalid"
#endif

#if (ROBOT_IRQ_PRIORITY_SENSOR < ROBOT_IRQ_PRIORITY_RTOS_API_MIN) || \
    (ROBOT_IRQ_PRIORITY_CAN < ROBOT_IRQ_PRIORITY_RTOS_API_MIN) || \
    (ROBOT_IRQ_PRIORITY_CONTROL_RX < ROBOT_IRQ_PRIORITY_RTOS_API_MIN) || \
    (ROBOT_IRQ_PRIORITY_COMM_RX < ROBOT_IRQ_PRIORITY_RTOS_API_MIN) || \
    (ROBOT_IRQ_PRIORITY_USB < ROBOT_IRQ_PRIORITY_RTOS_API_MIN) || \
    (ROBOT_IRQ_PRIORITY_UART_TX < ROBOT_IRQ_PRIORITY_RTOS_API_MIN)
#error "Peripheral ISR priority is above the FreeRTOS API threshold"
#endif

#if (ROBOT_IRQ_PRIORITY_HAL_TICK >= ROBOT_IRQ_PRIORITY_RTOS_API_MIN)
#error "HAL tick must preempt peripheral HAL timeout waits"
#endif

#if (ROBOT_IRQ_PRIORITY_SENSOR > ROBOT_IRQ_PRIORITY_KERNEL) || \
    (ROBOT_IRQ_PRIORITY_CAN > ROBOT_IRQ_PRIORITY_KERNEL) || \
    (ROBOT_IRQ_PRIORITY_CONTROL_RX > ROBOT_IRQ_PRIORITY_KERNEL) || \
    (ROBOT_IRQ_PRIORITY_COMM_RX > ROBOT_IRQ_PRIORITY_KERNEL) || \
    (ROBOT_IRQ_PRIORITY_USB > ROBOT_IRQ_PRIORITY_KERNEL) || \
    (ROBOT_IRQ_PRIORITY_UART_TX > ROBOT_IRQ_PRIORITY_KERNEL)
#error "Peripheral ISR priority is outside the configured NVIC range"
#endif

#endif /* PRIORITY_CONFIG_H */
