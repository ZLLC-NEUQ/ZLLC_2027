/*
 * FreeRTOS Kernel V10.2.1
 * Copyright (C) 2019 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
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

#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/*-----------------------------------------------------------
 * 这是一个模板配置文件
 *
 * 应根据您的特定硬件和应用
 * 程序要求调整这些定义。
 *
 * 这些参数及更多参数在 FreeRTOS.org 网站上提供的
 * FreeRTOS API 文档的“配置”部分中进行了描述。
 *
 * 参见 http://www.freertos.org/a00110.html
 *----------------------------------------------------------*/

/* 确保 stdint 仅由编译器使用，而不是汇编器使用。 */
#if defined(__ICCARM__) || defined(__CC_ARM) || defined(__GNUC__)
 #include <stdint.h>
 extern uint32_t SystemCoreClock;
#endif

/*  CMSIS-RTOSv2定义了56级优先级。为了能够全部使用它们并避免应用程序错误
 * 行为，
 * configUSE_PORT_OPTIMISED_TASK_SELECTION 必须设置为 0，configMAX_PRIORITIES 必须设置为 56
 *
 */
/* #define configUSE_PORT_OPTIMISED_TASK_SELECTION	0*/
/* #define configMAX_PRIORITIES					( 56 ) */
#define configUSE_PREEMPTION              1
#define configUSE_IDLE_HOOK               0
#define configUSE_TICK_HOOK               0
#define configMAX_PRIORITIES              (7)
#define configSUPPORT_STATIC_ALLOCATION   0
#define configCPU_CLOCK_HZ                (SystemCoreClock)
#define configTICK_RATE_HZ                ((TickType_t)1000)
#define configMINIMAL_STACK_SIZE          ((uint16_t)128)
#define configTOTAL_HEAP_SIZE             ((size_t)(15 * 1024))
#define configMAX_TASK_NAME_LEN           (16)
#define configUSE_TRACE_FACILITY          1
#define configUSE_16_BIT_TICKS            0
#define configIDLE_SHOULD_YIELD           1
#define configUSE_MUTEXES                 1
#define configQUEUE_REGISTRY_SIZE         8
#define configCHECK_FOR_STACK_OVERFLOW    0
#define configUSE_RECURSIVE_MUTEXES       1
#define configUSE_MALLOC_FAILED_HOOK      0
#define configUSE_APPLICATION_TASK_TAG    0
#define configUSE_COUNTING_SEMAPHORES     1
#define configGENERATE_RUN_TIME_STATS     0

/* 协同例程定义。 */
#define configUSE_CO_ROUTINES           0
#define configMAX_CO_ROUTINE_PRIORITIES (2)

/* 软件定时器定义。 */
#define configUSE_TIMERS             0
#define configTIMER_TASK_PRIORITY    (2)
#define configTIMER_QUEUE_LENGTH     10
#define configTIMER_TASK_STACK_DEPTH (configMINIMAL_STACK_SIZE * 2)

/* 将以下定义设置为 1 以包含 API
函数，或设置为零以排除 API 函数。 */
#define INCLUDE_vTaskPrioritySet       1
#define INCLUDE_uxTaskPriorityGet      1
#define INCLUDE_vTaskDelete            1
#define INCLUDE_vTaskCleanUpResources  0
#define INCLUDE_vTaskSuspend           1
#define INCLUDE_vTaskDelayUntil        0
#define INCLUDE_vTaskDelay             1
#define INCLUDE_xTaskGetSchedulerState 1

/*------------- CMSIS-RTOS V2 具体定义 ------------*/
/* 当使用 CMSIS-RTOSv2 时，必须将
 * configSUPPORT_STATIC_ALLOCATION
 * 设置为 1，以避免编译错误。 CMSIS-RTOS V2 实现需要以下定义
 *
#define configSUPPORT_STATIC_ALLOCATION          1   <-- cmsis_os threads are created using xTaskCreateStatic() API
#define configMAX_PRIORITIES                    (56) <-- Priority range in CMSIS-RTOS V2 is [0 .. 56]
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 0    <-- when set to 1, configMAX_PRIORITIES can't be more than 32 which is not suitable for the new CMSIS-RTOS v2 priority range
*/

/* CMSIS-RTOS V2 FreeRTOS 包装器取决
 * 于应用程序使用的堆实现，因此需要从列表中启用正确的定义
 * 以下
 *
//define USE_FreeRTOS_HEAP_1
//define USE_FreeRTOS_HEAP_2
//define USE_FreeRTOS_HEAP_3
//define USE_FreeRTOS_HEAP_4
//define USE_FreeRTOS_HEAP_5

*/


/* Cortex-M 的具体定义。 */
#ifdef __NVIC_PRIO_BITS
 /* 当使用 CMSIS 时，将指定 __BVIC_PRIO_BITS。 */
 #define configPRIO_BITS         __NVIC_PRIO_BITS
#else
 #define configPRIO_BITS         4        /* 15 个优先级 */
#endif

/* 可用于调用“设置优先级”的最低中断优先级
功能。 */
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY   0xf

/* 任何调用中断安全 FreeRTOS API 函数的中断服务例程均可使用的最高中断优
先级。不得在优先级高于此值的中断中调用中断安全的 FreeRTOS
API 函数！（优先级越高，数值越低。） */
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 5

/* 内核端口层本身使用的中断优先级。  这些对于所有
Cortex-M 端口都是通用的，并且不依赖于任何特定的库函数。 */
#define configKERNEL_INTERRUPT_PRIORITY   ( configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8 - configPRIO_BITS) )
/* ！！！ configMAX_SYSCALL_INTERRUPT_PRIORITY 不得设置为零！！！
！见http://www.FreeRTOS.org/RTOS-Cortex-M3-M4.html. */
#define configMAX_SYSCALL_INTERRUPT_PRIORITY  ( configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8 - configPRIO_BITS) )

/* 正常的 assert() 语义，不依
赖于提供 assert.h 头文件。 */
#define configASSERT( x ) if( ( x ) == 0 ) { taskDISABLE_INTERRUPTS(); for( ;; ); }

/* 将 FreeRTOS 端口中断处理程序
   映射到其 CMSIS 标准名称的定义。 */
#define vPortSVCHandler    SVC_Handler
#define xPortPendSVHandler PendSV_Handler

/* 重要： FreeRTOS 使用 SysTick 作为内部时间基准，
              从而确保系统和外设使用不同的时间 base（例如基于 TIM）。
 */
#define xPortSysTickHandler SysTick_Handler

#endif /* FREERTOS_CONFIG_H */
