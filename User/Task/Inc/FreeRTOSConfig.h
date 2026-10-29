/*
 * FreeRTOS Kernel V10.3.1
 * Copyright (C) 2020 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * http://www.FreeRTOS.org
 * http://aws.amazon.com/freertos
 *
 * 1 tab == 4 spaces!
 */

/**
 * @file FreeRTOSConfig.h
 * @brief FreeRTOS 内核配置文件（适配 STM32H723 + GCC + ARM_CM7 r0p1）
 * @version V1.1
 * @date 2026-10-30 27赛季定稿
 * @author HGCSJ
 * @copyright ZLLC 2027
 *
 * @note
 *  1. HAL 时基已从 SysTick 迁移到 TIM6，SysTick 完全交给 FreeRTOS。
 *  2. 中断优先级分组使用 NVIC_PRIORITYGROUP_4（4 位抢占优先级）。
 *  3. 调用 FreeRTOS From-ISR API 的中断，其抢占优先级数值必须 >=
 *     configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY（当前为 5）。
 */

#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

/* 包含文件 ------------------------------------------------------------------*/
#include <stdint.h>
#include "priority_config.h"

/* CMSIS 设备头文件，用于 __NVIC_PRIO_BITS */
#ifndef CMSIS_device_header
#define CMSIS_device_header "stm32h7xx.h"
#endif

/* 导出 SystemCoreClock 变量，HAL_RCC_ClockConfig() 会在运行时更新它 */
extern uint32_t SystemCoreClock;

/*-----------------------------------------------------------
 * 内核功能开关
 *----------------------------------------------------------*/

/* 使用抢占式调度 */
#define configUSE_PREEMPTION                    1

/* 空闲任务与 tick 钩子（当前未使用） */
#define configUSE_IDLE_HOOK                     0
#define configUSE_TICK_HOOK                     0

/* CPU 时钟与 tick 频率 */
#define configCPU_CLOCK_HZ                      ( SystemCoreClock )
#define configTICK_RATE_HZ                      ( ( TickType_t ) 1000 )

/* 任务与内存 */
#define configMINIMAL_STACK_SIZE                ( ( uint16_t ) 512 )
#define configTOTAL_HEAP_SIZE                   ( ( size_t ) ( 64 * 1024 ) )
#define configMAX_TASK_NAME_LEN                 ( 16 )
#define configMAX_PRIORITIES                    ( 32 )
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 1

#if (configMAX_PRIORITIES <= 8)
#error "Robot task priorities require configMAX_PRIORITIES > 8"
#endif

#if (ROBOT_TASK_PRIORITY_IMU >= configMAX_PRIORITIES) || \
    (ROBOT_TASK_PRIORITY_BACKGROUND == 0U)
#error "Robot task priorities must be above idle and below configMAX_PRIORITIES"
#endif

#if (ROBOT_TASK_PRIORITY_ALIVE == 0U) || \
    (ROBOT_TASK_PRIORITY_ALIVE >= ROBOT_TASK_PRIORITY_CONTROL)
#error "Alive task must be above idle and below control to serialize shared robot state"
#endif

// /* IMU 先发布快照，控制独占业务状态，后台处理 UI；接收任务已删除，中断直接投控制队列。 */
// #if (ROBOT_TASK_PRIORITY_IMU <= ROBOT_TASK_PRIORITY_CONTROL) || \
//     (ROBOT_TASK_PRIORITY_CONTROL <= ROBOT_TASK_PRIORITY_TIMER) || \
//     (ROBOT_TASK_PRIORITY_TIMER <= ROBOT_TASK_PRIORITY_BACKGROUND)
// #error "Robot task priority order must be IMU > control > timer > background"
// #endif

/* 动态内存分配（简化使用，暂不启用静态分配） */
#define configSUPPORT_STATIC_ALLOCATION         0
#define configSUPPORT_DYNAMIC_ALLOCATION        1

/* 任务同步机制 */
#define configUSE_MUTEXES                       1
#define configUSE_RECURSIVE_MUTEXES             1
#define configUSE_COUNTING_SEMAPHORES           1
#define configUSE_TASK_NOTIFICATIONS            1

/* 队列与软件定时器 */
#define configUSE_QUEUE_SETS                    0
#define configQUEUE_REGISTRY_SIZE               8
#define configUSE_TIMERS                        1
/* 软件定时器低于实时业务任务，避免其回调抢占解算、接收和控制。 */
#define configTIMER_TASK_PRIORITY               ROBOT_TASK_PRIORITY_TIMER
#define configTIMER_QUEUE_LENGTH                16
#define configTIMER_TASK_STACK_DEPTH            ( configMINIMAL_STACK_SIZE )

/* 调试与诊断 */
#define configUSE_TRACE_FACILITY                1
#define configUSE_16_BIT_TICKS                  0
#define configIDLE_SHOULD_YIELD                 1
#define configCHECK_FOR_STACK_OVERFLOW          2
#define configUSE_MALLOC_FAILED_HOOK            1
#define configUSE_APPLICATION_TASK_TAG          0
#define configGENERATE_RUN_TIME_STATS           0

/* 协程（不使用） */
#define configUSE_CO_ROUTINES                   0
#define configMAX_CO_ROUTINE_PRIORITIES         ( 2 )

/*-----------------------------------------------------------
 * 可选 API 包含
 *----------------------------------------------------------*/
#define INCLUDE_vTaskPrioritySet                1
#define INCLUDE_uxTaskPriorityGet               1
#define INCLUDE_vTaskDelete                     1
#define INCLUDE_vTaskCleanUpResources           1
#define INCLUDE_vTaskSuspend                    1
#define INCLUDE_vTaskDelayUntil                 1
#define INCLUDE_vTaskDelay                      1
#define INCLUDE_xQueueGetMutexHolder            1
#define INCLUDE_xTaskGetSchedulerState          1
#define INCLUDE_eTaskGetState                   1
#define INCLUDE_uxTaskGetStackHighWaterMark     1
#define INCLUDE_xTimerPendFunctionCall          1
#define INCLUDE_xTaskGetCurrentTaskHandle       1
#define INCLUDE_xTaskGetIdleTaskHandle          0

/*-----------------------------------------------------------
 * 中断优先级配置
 *----------------------------------------------------------*/

/* NVIC 优先级位数 */
#ifdef __NVIC_PRIO_BITS
    #define configPRIO_BITS                     __NVIC_PRIO_BITS
#else
    #define configPRIO_BITS                     4
#endif

/* 库优先级（数值越大优先级越低） */
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY         ROBOT_IRQ_PRIORITY_KERNEL
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY    ROBOT_IRQ_PRIORITY_RTOS_API_MIN

#if (configPRIO_BITS < 1) || (configPRIO_BITS > 8)
#error "configPRIO_BITS must be between 1 and 8"
#endif

#if (configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY == 0U) || \
    (configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY > configLIBRARY_LOWEST_INTERRUPT_PRIORITY) || \
    (configLIBRARY_LOWEST_INTERRUPT_PRIORITY >= (1U << configPRIO_BITS))
#error "FreeRTOS interrupt priorities are outside the configured NVIC range"
#endif

/* 内核中断优先级（用于 PendSV、SysTick） */
#define configKERNEL_INTERRUPT_PRIORITY \
    ( configLIBRARY_LOWEST_INTERRUPT_PRIORITY << ( 8 - configPRIO_BITS ) )

/* 可调用 FreeRTOS From-ISR API 的最高中断优先级 */
#define configMAX_SYSCALL_INTERRUPT_PRIORITY \
    ( configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << ( 8 - configPRIO_BITS ) )

/*-----------------------------------------------------------
 * 断言与钩子
 *----------------------------------------------------------*/

/* 断言：失败时关中断并死循环，便于定位 */
#define configASSERT( x ) \
    if( ( x ) == 0 ) { taskDISABLE_INTERRUPTS(); for( ;; ); }

/* malloc / 栈溢出钩子函数在 freertos_tasks.cpp 中实现，
 * 此处无需声明（FreeRTOS.h 会根据配置自动声明原型）。 */

/*-----------------------------------------------------------
 * Cortex-M7 特定配置
 *----------------------------------------------------------*/
#define configENABLE_FPU                        1
#define configENABLE_MPU                        0
#define configENABLE_TRUSTZONE                  0

/*-----------------------------------------------------------
 * 将 FreeRTOS 端口异常处理函数映射到启动文件中的符号
 *----------------------------------------------------------*/
#define vPortSVCHandler                         SVC_Handler
#define xPortPendSVHandler                      PendSV_Handler
#define xPortSysTickHandler                     SysTick_Handler

#ifdef __cplusplus
}
#endif

#endif /* FREERTOS_CONFIG_H */
