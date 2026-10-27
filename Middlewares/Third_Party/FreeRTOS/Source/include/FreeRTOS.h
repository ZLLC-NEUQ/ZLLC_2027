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

#ifndef INC_FREERTOS_H
#define INC_FREERTOS_H

/*
 * 包括正在使用的 FreeRTOS 端口所需的通用标头。
 */
#include <stddef.h>

/*
 * 如果无法找到 stdint.h，则： + 如果使用
 * GCC，请确保“未”使用 -nostdint 选项。
 * + 确保项目的包含路径包含编译器存储 stdint.h
 * 的目录。 + 设置支持 C99 所需的任何编译器选项
 * ，因为从技术上讲，stdint.h 仅对于 C99 是
 * 强制的（FreeRTOS 不以任何其他方式需要 C99
 * ）。 + FreeRTOS 下载包含一个简单的
 * stdint.h 定义，可在编译器未提供任何定义的情况
 * 下使用。  这些文件仅包含构建 FreeRTOS 所需
 * 的 typedef。  请阅读
 * FreeRTOS/source/stdint.readme 中的说明以获取更多信息。
 */
#include <stdint.h> /* 阅读上面的评论。 */

#ifdef __cplusplus
extern "C" {
#endif

/* 应用程序特定的配置选项。 */
#include "FreeRTOSConfig.h"

/* FreeRTOS 基本定义。 */
#include "projdefs.h"

/* 特定于正在使用的端口的定义。 */
#include "portable.h"

/* 下面使用configUSE_NEWLIB_REENTRANT 之前必须默认。 */
#ifndef configUSE_NEWLIB_REENTRANT
	#define configUSE_NEWLIB_REENTRANT 0
#endif

/* 如果使用 struct _reent，则为必需。 */
#if ( configUSE_NEWLIB_REENTRANT == 1 )
	#include <reent.h>
#endif
/*
 * 检查是否已定义所有必需的应用程序特定宏。这些
 * 宏是应用程序特定的 and（如下载的）在
 * FreeRTOSConfig.h 中定义。
 */

#ifndef configMINIMAL_STACK_SIZE
	#error Missing definition:  configMINIMAL_STACK_SIZE must be defined in FreeRTOSConfig.h.  configMINIMAL_STACK_SIZE defines the size (in words) of the stack allocated to the idle task.  Refer to the demo project provided for your port for a suitable value.
#endif

#ifndef configMAX_PRIORITIES
	#error Missing definition:  configMAX_PRIORITIES must be defined in FreeRTOSConfig.h.  See the Configuration section of the FreeRTOS API documentation for details.
#endif

#if configMAX_PRIORITIES < 1
	#error configMAX_PRIORITIES must be defined to be greater than or equal to 1.
#endif

#ifndef configUSE_PREEMPTION
	#error Missing definition:  configUSE_PREEMPTION must be defined in FreeRTOSConfig.h as either 1 or 0.  See the Configuration section of the FreeRTOS API documentation for details.
#endif

#ifndef configUSE_IDLE_HOOK
	#error Missing definition:  configUSE_IDLE_HOOK must be defined in FreeRTOSConfig.h as either 1 or 0.  See the Configuration section of the FreeRTOS API documentation for details.
#endif

#ifndef configUSE_TICK_HOOK
	#error Missing definition:  configUSE_TICK_HOOK must be defined in FreeRTOSConfig.h as either 1 or 0.  See the Configuration section of the FreeRTOS API documentation for details.
#endif

#ifndef configUSE_16_BIT_TICKS
	#error Missing definition:  configUSE_16_BIT_TICKS must be defined in FreeRTOSConfig.h as either 1 or 0.  See the Configuration section of the FreeRTOS API documentation for details.
#endif

#ifndef configUSE_CO_ROUTINES
	#define configUSE_CO_ROUTINES 0
#endif

#ifndef INCLUDE_vTaskPrioritySet
	#define INCLUDE_vTaskPrioritySet 0
#endif

#ifndef INCLUDE_uxTaskPriorityGet
	#define INCLUDE_uxTaskPriorityGet 0
#endif

#ifndef INCLUDE_vTaskDelete
	#define INCLUDE_vTaskDelete 0
#endif

#ifndef INCLUDE_vTaskSuspend
	#define INCLUDE_vTaskSuspend 0
#endif

#ifndef INCLUDE_vTaskDelayUntil
	#define INCLUDE_vTaskDelayUntil 0
#endif

#ifndef INCLUDE_vTaskDelay
	#define INCLUDE_vTaskDelay 0
#endif

#ifndef INCLUDE_xTaskGetIdleTaskHandle
	#define INCLUDE_xTaskGetIdleTaskHandle 0
#endif

#ifndef INCLUDE_xTaskAbortDelay
	#define INCLUDE_xTaskAbortDelay 0
#endif

#ifndef INCLUDE_xQueueGetMutexHolder
	#define INCLUDE_xQueueGetMutexHolder 0
#endif

#ifndef INCLUDE_xSemaphoreGetMutexHolder
	#define INCLUDE_xSemaphoreGetMutexHolder INCLUDE_xQueueGetMutexHolder
#endif

#ifndef INCLUDE_xTaskGetHandle
	#define INCLUDE_xTaskGetHandle 0
#endif

#ifndef INCLUDE_uxTaskGetStackHighWaterMark
	#define INCLUDE_uxTaskGetStackHighWaterMark 0
#endif

#ifndef INCLUDE_uxTaskGetStackHighWaterMark2
	#define INCLUDE_uxTaskGetStackHighWaterMark2 0
#endif

#ifndef INCLUDE_eTaskGetState
	#define INCLUDE_eTaskGetState 0
#endif

#ifndef INCLUDE_xTaskResumeFromISR
	#define INCLUDE_xTaskResumeFromISR 1
#endif

#ifndef INCLUDE_xTimerPendFunctionCall
	#define INCLUDE_xTimerPendFunctionCall 0
#endif

#ifndef INCLUDE_xTaskGetSchedulerState
	#define INCLUDE_xTaskGetSchedulerState 0
#endif

#ifndef INCLUDE_xTaskGetCurrentTaskHandle
	#define INCLUDE_xTaskGetCurrentTaskHandle 0
#endif

#if configUSE_CO_ROUTINES != 0
	#ifndef configMAX_CO_ROUTINE_PRIORITIES
		#error configMAX_CO_ROUTINE_PRIORITIES must be greater than or equal to 1.
	#endif
#endif

#ifndef configUSE_DAEMON_TASK_STARTUP_HOOK
	#define configUSE_DAEMON_TASK_STARTUP_HOOK 0
#endif

#ifndef configUSE_APPLICATION_TASK_TAG
	#define configUSE_APPLICATION_TASK_TAG 0
#endif

#ifndef configNUM_THREAD_LOCAL_STORAGE_POINTERS
	#define configNUM_THREAD_LOCAL_STORAGE_POINTERS 0
#endif

#ifndef configUSE_RECURSIVE_MUTEXES
	#define configUSE_RECURSIVE_MUTEXES 0
#endif

#ifndef configUSE_MUTEXES
	#define configUSE_MUTEXES 0
#endif

#ifndef configUSE_TIMERS
	#define configUSE_TIMERS 0
#endif

#ifndef configUSE_COUNTING_SEMAPHORES
	#define configUSE_COUNTING_SEMAPHORES 0
#endif

#ifndef configUSE_ALTERNATIVE_API
	#define configUSE_ALTERNATIVE_API 0
#endif

#ifndef portCRITICAL_NESTING_IN_TCB
	#define portCRITICAL_NESTING_IN_TCB 0
#endif

#ifndef configMAX_TASK_NAME_LEN
	#define configMAX_TASK_NAME_LEN 16
#endif

#ifndef configIDLE_SHOULD_YIELD
	#define configIDLE_SHOULD_YIELD		1
#endif

#if configMAX_TASK_NAME_LEN < 1
	#error configMAX_TASK_NAME_LEN must be set to a minimum of 1 in FreeRTOSConfig.h
#endif

#ifndef configASSERT
	#define configASSERT( x )
	#define configASSERT_DEFINED 0
#else
	#define configASSERT_DEFINED 1
#endif

/* configPRECONDITION 应定义为
configASSERT。 CBMC 证明需要一种方法来跟
踪假设和断言。 configPRECONDITION 语句
应表达隐含的不变量或所做的假设。
configASSERT 语句应该表达一个不变量，该不变量在调用代码之前必须显式保持。 */
#ifndef configPRECONDITION
	#define configPRECONDITION( X ) configASSERT(X)
	#define configPRECONDITION_DEFINED 0
#else
	#define configPRECONDITION_DEFINED 1
#endif

#ifndef portMEMORY_BARRIER
	#define portMEMORY_BARRIER()
#endif

#ifndef portSOFTWARE_BARRIER
	#define portSOFTWARE_BARRIER()
#endif

/* 定时器模块依赖于xTaskGetSchedulerState()。 */
#if configUSE_TIMERS == 1

	#ifndef configTIMER_TASK_PRIORITY
		#error If configUSE_TIMERS is set to 1 then configTIMER_TASK_PRIORITY must also be defined.
	#endif /* configTIMER_TASK_PRIORITY */

	#ifndef configTIMER_QUEUE_LENGTH
		#error If configUSE_TIMERS is set to 1 then configTIMER_QUEUE_LENGTH must also be defined.
	#endif /* configTIMER_QUEUE_LENGTH */

	#ifndef configTIMER_TASK_STACK_DEPTH
		#error If configUSE_TIMERS is set to 1 then configTIMER_TASK_STACK_DEPTH must also be defined.
	#endif /* configTIMER_TASK_STACK_DEPTH */

#endif /* configUSE_TIMERS */

#ifndef portSET_INTERRUPT_MASK_FROM_ISR
	#define portSET_INTERRUPT_MASK_FROM_ISR() 0
#endif

#ifndef portCLEAR_INTERRUPT_MASK_FROM_ISR
	#define portCLEAR_INTERRUPT_MASK_FROM_ISR( uxSavedStatusValue ) ( void ) uxSavedStatusValue
#endif

#ifndef portCLEAN_UP_TCB
	#define portCLEAN_UP_TCB( pxTCB ) ( void ) pxTCB
#endif

#ifndef portPRE_TASK_DELETE_HOOK
	#define portPRE_TASK_DELETE_HOOK( pvTaskToDelete, pxYieldPending )
#endif

#ifndef portSETUP_TCB
	#define portSETUP_TCB( pxTCB ) ( void ) pxTCB
#endif

#ifndef configQUEUE_REGISTRY_SIZE
	#define configQUEUE_REGISTRY_SIZE 0U
#endif

#if ( configQUEUE_REGISTRY_SIZE < 1 )
	#define vQueueAddToRegistry( xQueue, pcName )
	#define vQueueUnregisterQueue( xQueue )
	#define pcQueueGetName( xQueue )
#endif

#ifndef portPOINTER_SIZE_TYPE
	#define portPOINTER_SIZE_TYPE uint32_t
#endif

/* 删除所有未使用的跟踪宏。 */
#ifndef traceSTART
	/* 用于执行任何必要的初始化 -
	例如，打开要写入跟踪的文件。 */
	#define traceSTART()
#endif

#ifndef traceEND
	/* 用于关闭跟踪，例如关闭已跟踪的文件
	书面。 */
	#define traceEND()
#endif

#ifndef traceTASK_SWITCHED_IN
	/* 选择要运行的任务后调用。
	pxCurrentTCB 保存指向所选任务的任务控制块的指针。 */
	#define traceTASK_SWITCHED_IN()
#endif

#ifndef traceINCREASE_TICK_COUNT
	/* 从无滴答空闲唤醒后，在步进滴答计数之前调用
	睡觉。 */
	#define traceINCREASE_TICK_COUNT( x )
#endif

#ifndef traceLOW_POWER_IDLE_BEGIN
	/* 在进入无滴答空闲状态之前立即调用。 */
	#define traceLOW_POWER_IDLE_BEGIN()
#endif

#ifndef	traceLOW_POWER_IDLE_END
	/* 在无滴答空闲后返回到空闲任务时调用。 */
	#define traceLOW_POWER_IDLE_END()
#endif

#ifndef traceTASK_SWITCHED_OUT
	/* 在选择运行任务之前调用。
	pxCurrentTCB 保存一个指向被切换出的任务的任务控制块的指针。 */
	#define traceTASK_SWITCHED_OUT()
#endif

#ifndef traceTASK_PRIORITY_INHERIT
	/* 当任务尝试获取已由较低优先级任务持有的互斥体时调用。
	pxTCBOfMutexHolder 是指向保存互斥锁的任务的
	TCB 的指针。  uxInheritedPriority 是互
	斥锁持有者 inherit 的优先级（尝试获取互斥锁的任务的优先级）
	静音。 */
	#define traceTASK_PRIORITY_INHERIT( pxTCBOfMutexHolder, uxInheritedPriority )
#endif

#ifndef traceTASK_PRIORITY_DISINHERIT
	/* 当任务释放互斥锁时调用，该互斥锁的持有导致该任务继承更高优先级任
	务的优先级。 pxTCBOfMutexHolder 是指向正在释
	放互斥锁的任务的 TCB 的指针。
	uxOriginalPriority 是任务的 configured（基本）优先级。 */
	#define traceTASK_PRIORITY_DISINHERIT( pxTCBOfMutexHolder, uxOriginalPriority )
#endif

#ifndef traceBLOCKING_ON_QUEUE_RECEIVE
	/* 任务即将阻塞，因为它无法从队列/互斥体/信号量
	中读取。  pxQueue 是指向尝试读取的队
	列/互斥体/信号量的指针。
	pxCurrentTCB 指向尝试读取的任务的 TCB。 */
	#define traceBLOCKING_ON_QUEUE_RECEIVE( pxQueue )
#endif

#ifndef traceBLOCKING_ON_QUEUE_PEEK
	/* 任务即将阻塞，因为它无法从队列/互斥体/信号量
	中读取。  pxQueue 是指向尝试读取的队
	列/互斥体/信号量的指针。
	pxCurrentTCB 指向尝试读取的任务的 TCB。 */
	#define traceBLOCKING_ON_QUEUE_PEEK( pxQueue )
#endif

#ifndef traceBLOCKING_ON_QUEUE_SEND
	/* 任务即将阻塞，因为它无法写入队列/互斥体/信
	号量。  pxQueue 是指向尝试写入的队
	列/互斥体/信号量的指针。
	pxCurrentTCB 指向尝试写入的任务的 TCB。 */
	#define traceBLOCKING_ON_QUEUE_SEND( pxQueue )
#endif

#ifndef configCHECK_FOR_STACK_OVERFLOW
	#define configCHECK_FOR_STACK_OVERFLOW 0
#endif

#ifndef configRECORD_STACK_HIGH_ADDRESS
	#define configRECORD_STACK_HIGH_ADDRESS 0
#endif

#ifndef configINCLUDE_FREERTOS_TASK_C_ADDITIONS_H
	#define configINCLUDE_FREERTOS_TASK_C_ADDITIONS_H 0
#endif

/* 以下事件宏嵌入在内核 API 调用中。 */

#ifndef traceMOVED_TASK_TO_READY_STATE
	#define traceMOVED_TASK_TO_READY_STATE( pxTCB )
#endif

#ifndef tracePOST_MOVED_TASK_TO_READY_STATE
	#define tracePOST_MOVED_TASK_TO_READY_STATE( pxTCB )
#endif

#ifndef traceQUEUE_CREATE
	#define traceQUEUE_CREATE( pxNewQueue )
#endif

#ifndef traceQUEUE_CREATE_FAILED
	#define traceQUEUE_CREATE_FAILED( ucQueueType )
#endif

#ifndef traceCREATE_MUTEX
	#define traceCREATE_MUTEX( pxNewQueue )
#endif

#ifndef traceCREATE_MUTEX_FAILED
	#define traceCREATE_MUTEX_FAILED()
#endif

#ifndef traceGIVE_MUTEX_RECURSIVE
	#define traceGIVE_MUTEX_RECURSIVE( pxMutex )
#endif

#ifndef traceGIVE_MUTEX_RECURSIVE_FAILED
	#define traceGIVE_MUTEX_RECURSIVE_FAILED( pxMutex )
#endif

#ifndef traceTAKE_MUTEX_RECURSIVE
	#define traceTAKE_MUTEX_RECURSIVE( pxMutex )
#endif

#ifndef traceTAKE_MUTEX_RECURSIVE_FAILED
	#define traceTAKE_MUTEX_RECURSIVE_FAILED( pxMutex )
#endif

#ifndef traceCREATE_COUNTING_SEMAPHORE
	#define traceCREATE_COUNTING_SEMAPHORE()
#endif

#ifndef traceCREATE_COUNTING_SEMAPHORE_FAILED
	#define traceCREATE_COUNTING_SEMAPHORE_FAILED()
#endif

#ifndef traceQUEUE_SEND
	#define traceQUEUE_SEND( pxQueue )
#endif

#ifndef traceQUEUE_SEND_FAILED
	#define traceQUEUE_SEND_FAILED( pxQueue )
#endif

#ifndef traceQUEUE_RECEIVE
	#define traceQUEUE_RECEIVE( pxQueue )
#endif

#ifndef traceQUEUE_PEEK
	#define traceQUEUE_PEEK( pxQueue )
#endif

#ifndef traceQUEUE_PEEK_FAILED
	#define traceQUEUE_PEEK_FAILED( pxQueue )
#endif

#ifndef traceQUEUE_PEEK_FROM_ISR
	#define traceQUEUE_PEEK_FROM_ISR( pxQueue )
#endif

#ifndef traceQUEUE_RECEIVE_FAILED
	#define traceQUEUE_RECEIVE_FAILED( pxQueue )
#endif

#ifndef traceQUEUE_SEND_FROM_ISR
	#define traceQUEUE_SEND_FROM_ISR( pxQueue )
#endif

#ifndef traceQUEUE_SEND_FROM_ISR_FAILED
	#define traceQUEUE_SEND_FROM_ISR_FAILED( pxQueue )
#endif

#ifndef traceQUEUE_RECEIVE_FROM_ISR
	#define traceQUEUE_RECEIVE_FROM_ISR( pxQueue )
#endif

#ifndef traceQUEUE_RECEIVE_FROM_ISR_FAILED
	#define traceQUEUE_RECEIVE_FROM_ISR_FAILED( pxQueue )
#endif

#ifndef traceQUEUE_PEEK_FROM_ISR_FAILED
	#define traceQUEUE_PEEK_FROM_ISR_FAILED( pxQueue )
#endif

#ifndef traceQUEUE_DELETE
	#define traceQUEUE_DELETE( pxQueue )
#endif

#ifndef traceTASK_CREATE
	#define traceTASK_CREATE( pxNewTCB )
#endif

#ifndef traceTASK_CREATE_FAILED
	#define traceTASK_CREATE_FAILED()
#endif

#ifndef traceTASK_DELETE
	#define traceTASK_DELETE( pxTaskToDelete )
#endif

#ifndef traceTASK_DELAY_UNTIL
	#define traceTASK_DELAY_UNTIL( x )
#endif

#ifndef traceTASK_DELAY
	#define traceTASK_DELAY()
#endif

#ifndef traceTASK_PRIORITY_SET
	#define traceTASK_PRIORITY_SET( pxTask, uxNewPriority )
#endif

#ifndef traceTASK_SUSPEND
	#define traceTASK_SUSPEND( pxTaskToSuspend )
#endif

#ifndef traceTASK_RESUME
	#define traceTASK_RESUME( pxTaskToResume )
#endif

#ifndef traceTASK_RESUME_FROM_ISR
	#define traceTASK_RESUME_FROM_ISR( pxTaskToResume )
#endif

#ifndef traceTASK_INCREMENT_TICK
	#define traceTASK_INCREMENT_TICK( xTickCount )
#endif

#ifndef traceTIMER_CREATE
	#define traceTIMER_CREATE( pxNewTimer )
#endif

#ifndef traceTIMER_CREATE_FAILED
	#define traceTIMER_CREATE_FAILED()
#endif

#ifndef traceTIMER_COMMAND_SEND
	#define traceTIMER_COMMAND_SEND( xTimer, xMessageID, xMessageValueValue, xReturn )
#endif

#ifndef traceTIMER_EXPIRED
	#define traceTIMER_EXPIRED( pxTimer )
#endif

#ifndef traceTIMER_COMMAND_RECEIVED
	#define traceTIMER_COMMAND_RECEIVED( pxTimer, xMessageID, xMessageValue )
#endif

#ifndef traceMALLOC
    #define traceMALLOC( pvAddress, uiSize )
#endif

#ifndef traceFREE
    #define traceFREE( pvAddress, uiSize )
#endif

#ifndef traceEVENT_GROUP_CREATE
	#define traceEVENT_GROUP_CREATE( xEventGroup )
#endif

#ifndef traceEVENT_GROUP_CREATE_FAILED
	#define traceEVENT_GROUP_CREATE_FAILED()
#endif

#ifndef traceEVENT_GROUP_SYNC_BLOCK
	#define traceEVENT_GROUP_SYNC_BLOCK( xEventGroup, uxBitsToSet, uxBitsToWaitFor )
#endif

#ifndef traceEVENT_GROUP_SYNC_END
	#define traceEVENT_GROUP_SYNC_END( xEventGroup, uxBitsToSet, uxBitsToWaitFor, xTimeoutOccurred ) ( void ) xTimeoutOccurred
#endif

#ifndef traceEVENT_GROUP_WAIT_BITS_BLOCK
	#define traceEVENT_GROUP_WAIT_BITS_BLOCK( xEventGroup, uxBitsToWaitFor )
#endif

#ifndef traceEVENT_GROUP_WAIT_BITS_END
	#define traceEVENT_GROUP_WAIT_BITS_END( xEventGroup, uxBitsToWaitFor, xTimeoutOccurred ) ( void ) xTimeoutOccurred
#endif

#ifndef traceEVENT_GROUP_CLEAR_BITS
	#define traceEVENT_GROUP_CLEAR_BITS( xEventGroup, uxBitsToClear )
#endif

#ifndef traceEVENT_GROUP_CLEAR_BITS_FROM_ISR
	#define traceEVENT_GROUP_CLEAR_BITS_FROM_ISR( xEventGroup, uxBitsToClear )
#endif

#ifndef traceEVENT_GROUP_SET_BITS
	#define traceEVENT_GROUP_SET_BITS( xEventGroup, uxBitsToSet )
#endif

#ifndef traceEVENT_GROUP_SET_BITS_FROM_ISR
	#define traceEVENT_GROUP_SET_BITS_FROM_ISR( xEventGroup, uxBitsToSet )
#endif

#ifndef traceEVENT_GROUP_DELETE
	#define traceEVENT_GROUP_DELETE( xEventGroup )
#endif

#ifndef tracePEND_FUNC_CALL
	#define tracePEND_FUNC_CALL(xFunctionToPend, pvParameter1, ulParameter2, ret)
#endif

#ifndef tracePEND_FUNC_CALL_FROM_ISR
	#define tracePEND_FUNC_CALL_FROM_ISR(xFunctionToPend, pvParameter1, ulParameter2, ret)
#endif

#ifndef traceQUEUE_REGISTRY_ADD
	#define traceQUEUE_REGISTRY_ADD(xQueue, pcQueueName)
#endif

#ifndef traceTASK_NOTIFY_TAKE_BLOCK
	#define traceTASK_NOTIFY_TAKE_BLOCK()
#endif

#ifndef traceTASK_NOTIFY_TAKE
	#define traceTASK_NOTIFY_TAKE()
#endif

#ifndef traceTASK_NOTIFY_WAIT_BLOCK
	#define traceTASK_NOTIFY_WAIT_BLOCK()
#endif

#ifndef traceTASK_NOTIFY_WAIT
	#define traceTASK_NOTIFY_WAIT()
#endif

#ifndef traceTASK_NOTIFY
	#define traceTASK_NOTIFY()
#endif

#ifndef traceTASK_NOTIFY_FROM_ISR
	#define traceTASK_NOTIFY_FROM_ISR()
#endif

#ifndef traceTASK_NOTIFY_GIVE_FROM_ISR
	#define traceTASK_NOTIFY_GIVE_FROM_ISR()
#endif

#ifndef traceSTREAM_BUFFER_CREATE_FAILED
	#define traceSTREAM_BUFFER_CREATE_FAILED( xIsMessageBuffer )
#endif

#ifndef traceSTREAM_BUFFER_CREATE_STATIC_FAILED
	#define traceSTREAM_BUFFER_CREATE_STATIC_FAILED( xReturn, xIsMessageBuffer )
#endif

#ifndef traceSTREAM_BUFFER_CREATE
	#define traceSTREAM_BUFFER_CREATE( pxStreamBuffer, xIsMessageBuffer )
#endif

#ifndef traceSTREAM_BUFFER_DELETE
	#define traceSTREAM_BUFFER_DELETE( xStreamBuffer )
#endif

#ifndef traceSTREAM_BUFFER_RESET
	#define traceSTREAM_BUFFER_RESET( xStreamBuffer )
#endif

#ifndef traceBLOCKING_ON_STREAM_BUFFER_SEND
	#define traceBLOCKING_ON_STREAM_BUFFER_SEND( xStreamBuffer )
#endif

#ifndef traceSTREAM_BUFFER_SEND
	#define traceSTREAM_BUFFER_SEND( xStreamBuffer, xBytesSent )
#endif

#ifndef traceSTREAM_BUFFER_SEND_FAILED
	#define traceSTREAM_BUFFER_SEND_FAILED( xStreamBuffer )
#endif

#ifndef traceSTREAM_BUFFER_SEND_FROM_ISR
	#define traceSTREAM_BUFFER_SEND_FROM_ISR( xStreamBuffer, xBytesSent )
#endif

#ifndef traceBLOCKING_ON_STREAM_BUFFER_RECEIVE
	#define traceBLOCKING_ON_STREAM_BUFFER_RECEIVE( xStreamBuffer )
#endif

#ifndef traceSTREAM_BUFFER_RECEIVE
	#define traceSTREAM_BUFFER_RECEIVE( xStreamBuffer, xReceivedLength )
#endif

#ifndef traceSTREAM_BUFFER_RECEIVE_FAILED
	#define traceSTREAM_BUFFER_RECEIVE_FAILED( xStreamBuffer )
#endif

#ifndef traceSTREAM_BUFFER_RECEIVE_FROM_ISR
	#define traceSTREAM_BUFFER_RECEIVE_FROM_ISR( xStreamBuffer, xReceivedLength )
#endif

#ifndef configGENERATE_RUN_TIME_STATS
	#define configGENERATE_RUN_TIME_STATS 0
#endif

#if ( configGENERATE_RUN_TIME_STATS == 1 )

	#ifndef portCONFIGURE_TIMER_FOR_RUN_TIME_STATS
		#error If configGENERATE_RUN_TIME_STATS is defined then portCONFIGURE_TIMER_FOR_RUN_TIME_STATS must also be defined.  portCONFIGURE_TIMER_FOR_RUN_TIME_STATS should call a port layer function to setup a peripheral timer/counter that can then be used as the run time counter time base.
	#endif /* portCONFIGURE_TIMER_FOR_RUN_TIME_STATS */

	#ifndef portGET_RUN_TIME_COUNTER_VALUE
		#ifndef portALT_GET_RUN_TIME_COUNTER_VALUE
			#error If configGENERATE_RUN_TIME_STATS is defined then either portGET_RUN_TIME_COUNTER_VALUE or portALT_GET_RUN_TIME_COUNTER_VALUE must also be defined.  See the examples provided and the FreeRTOS web site for more information.
		#endif /* portALT_GET_RUN_TIME_COUNTER_VALUE */
	#endif /* portGET_RUN_TIME_COUNTER_VALUE */

#endif /* configGENERATE_RUN_TIME_STATS */

#ifndef portCONFIGURE_TIMER_FOR_RUN_TIME_STATS
	#define portCONFIGURE_TIMER_FOR_RUN_TIME_STATS()
#endif

#ifndef configUSE_MALLOC_FAILED_HOOK
	#define configUSE_MALLOC_FAILED_HOOK 0
#endif

#ifndef portPRIVILEGE_BIT
	#define portPRIVILEGE_BIT ( ( UBaseType_t ) 0x00 )
#endif

#ifndef portYIELD_WITHIN_API
	#define portYIELD_WITHIN_API portYIELD
#endif

#ifndef portSUPPRESS_TICKS_AND_SLEEP
	#define portSUPPRESS_TICKS_AND_SLEEP( xExpectedIdleTime )
#endif

#ifndef configEXPECTED_IDLE_TIME_BEFORE_SLEEP
	#define configEXPECTED_IDLE_TIME_BEFORE_SLEEP 2
#endif

#if configEXPECTED_IDLE_TIME_BEFORE_SLEEP < 2
	#error configEXPECTED_IDLE_TIME_BEFORE_SLEEP must not be less than 2
#endif

#ifndef configUSE_TICKLESS_IDLE
	#define configUSE_TICKLESS_IDLE 0
#endif

#ifndef configPRE_SUPPRESS_TICKS_AND_SLEEP_PROCESSING
	#define configPRE_SUPPRESS_TICKS_AND_SLEEP_PROCESSING( x )
#endif

#ifndef configPRE_SLEEP_PROCESSING
	#define configPRE_SLEEP_PROCESSING( x )
#endif

#ifndef configPOST_SLEEP_PROCESSING
	#define configPOST_SLEEP_PROCESSING( x )
#endif

#ifndef configUSE_QUEUE_SETS
	#define configUSE_QUEUE_SETS 0
#endif

#ifndef portTASK_USES_FLOATING_POINT
	#define portTASK_USES_FLOATING_POINT()
#endif

#ifndef portALLOCATE_SECURE_CONTEXT
	#define portALLOCATE_SECURE_CONTEXT( ulSecureStackSize )
#endif

#ifndef portDONT_DISCARD
	#define portDONT_DISCARD
#endif

#ifndef configUSE_TIME_SLICING
	#define configUSE_TIME_SLICING 1
#endif

#ifndef configINCLUDE_APPLICATION_DEFINED_PRIVILEGED_FUNCTIONS
	#define configINCLUDE_APPLICATION_DEFINED_PRIVILEGED_FUNCTIONS 0
#endif

#ifndef configUSE_STATS_FORMATTING_FUNCTIONS
	#define configUSE_STATS_FORMATTING_FUNCTIONS 0
#endif

#ifndef portASSERT_IF_INTERRUPT_PRIORITY_INVALID
	#define portASSERT_IF_INTERRUPT_PRIORITY_INVALID()
#endif

#ifndef configUSE_TRACE_FACILITY
	#define configUSE_TRACE_FACILITY 0
#endif

#ifndef mtCOVERAGE_TEST_MARKER
	#define mtCOVERAGE_TEST_MARKER()
#endif

#ifndef mtCOVERAGE_TEST_DELAY
	#define mtCOVERAGE_TEST_DELAY()
#endif

#ifndef portASSERT_IF_IN_ISR
	#define portASSERT_IF_IN_ISR()
#endif

#ifndef configUSE_PORT_OPTIMISED_TASK_SELECTION
	#define configUSE_PORT_OPTIMISED_TASK_SELECTION 0
#endif

#ifndef configAPPLICATION_ALLOCATED_HEAP
	#define configAPPLICATION_ALLOCATED_HEAP 0
#endif

#ifndef configUSE_TASK_NOTIFICATIONS
	#define configUSE_TASK_NOTIFICATIONS 1
#endif

#ifndef configUSE_POSIX_ERRNO
	#define configUSE_POSIX_ERRNO 0
#endif

#ifndef portTICK_TYPE_IS_ATOMIC
	#define portTICK_TYPE_IS_ATOMIC 0
#endif

#ifndef configSUPPORT_STATIC_ALLOCATION
	/* 默认为 0 以实现向后兼容。 */
	#define configSUPPORT_STATIC_ALLOCATION 0
#endif

#ifndef configSUPPORT_DYNAMIC_ALLOCATION
	/* 默认为 1 以实现向后兼容性。 */
	#define configSUPPORT_DYNAMIC_ALLOCATION 1
#endif

#ifndef configSTACK_DEPTH_TYPE
	/* 默认为 uint16_t 以实现向后兼容性，但如果
	uint16_t 限制过多，则可以在 FreeRTOSConfig.h 中覆盖。 */
	#define configSTACK_DEPTH_TYPE uint16_t
#endif

#ifndef configMESSAGE_BUFFER_LENGTH_TYPE
	/* 默认为 size_t 以实现向后兼容性，但如果
	长度始终小于 size_t 中的字节数，则可以
	在 FreeRTOSConfig.h 中覆盖。 */
	#define configMESSAGE_BUFFER_LENGTH_TYPE size_t
#endif

/* 健全性检查配置。 */
#if( configUSE_TICKLESS_IDLE != 0 )
	#if( INCLUDE_vTaskSuspend != 1 )
		#error INCLUDE_vTaskSuspend must be set to 1 if configUSE_TICKLESS_IDLE is not set to 0
	#endif /* INCLUDE_vTaskSuspend */
#endif /* configUSE_TICKLESS_IDLE */

#if( ( configSUPPORT_STATIC_ALLOCATION == 0 ) && ( configSUPPORT_DYNAMIC_ALLOCATION == 0 ) )
	#error configSUPPORT_STATIC_ALLOCATION and configSUPPORT_DYNAMIC_ALLOCATION cannot both be 0, but can both be 1.
#endif

#if( ( configUSE_RECURSIVE_MUTEXES == 1 ) && ( configUSE_MUTEXES != 1 ) )
	#error configUSE_MUTEXES must be set to 1 to use recursive mutexes
#endif

#ifndef configINITIAL_TICK_COUNT
	#define configINITIAL_TICK_COUNT 0
#endif

#if( portTICK_TYPE_IS_ATOMIC == 0 )
	/* 无法以原子方式读取刻度类型的变量，或者未设置
	portTICK_TYPE_IS_ATOMIC -
	将刻度计数返回到标准临界区宏时映射使用的临界区。 */
	#define portTICK_TYPE_ENTER_CRITICAL() portENTER_CRITICAL()
	#define portTICK_TYPE_EXIT_CRITICAL() portEXIT_CRITICAL()
	#define portTICK_TYPE_SET_INTERRUPT_MASK_FROM_ISR() portSET_INTERRUPT_MASK_FROM_ISR()
	#define portTICK_TYPE_CLEAR_INTERRUPT_MASK_FROM_ISR( x ) portCLEAR_INTERRUPT_MASK_FROM_ISR( ( x ) )
#else
	/* 可以自动读取刻度类型，因此可以定
	义返回刻度计数时使用的关键部分。 */
	#define portTICK_TYPE_ENTER_CRITICAL()
	#define portTICK_TYPE_EXIT_CRITICAL()
	#define portTICK_TYPE_SET_INTERRUPT_MASK_FROM_ISR() 0
	#define portTICK_TYPE_CLEAR_INTERRUPT_MASK_FROM_ISR( x ) ( void ) x
#endif

/* 如果需要，定义允许向后兼容 V8
之前的 FreeRTOS 版本。 */
#ifndef configENABLE_BACKWARD_COMPATIBILITY
	#define configENABLE_BACKWARD_COMPATIBILITY 1
#endif

#ifndef configPRINTF
	/* configPRINTF() 未定义，因此将其定义为空。  要使用
	configPRINTF()，请将其定义为 follows（其中
	MyPrintFunction() 由应用程序编写者提供）：

	无效 MyPrintFunction(const char *pcFormat, ... );
	#define configPRINTF( X )   MyPrintFunction X

	然后像标准 printf() 函数一样调用，但将所有参
	数放在括号中，以便将它们作为单个参数传递。  例如：
	configPRINTF( ("Value = %d", MyVariable) ); */
	#define configPRINTF( X )
#endif

#ifndef configMAX
	/* 应用程序编写者没有提供自己的
	MAX 宏，因此定义以下通用实现。 */
	#define configMAX( a, b ) ( ( ( a ) > ( b ) ) ? ( a ) : ( b ) )
#endif

#ifndef configMIN
	/* 应用程序编写者没有提供自己的
	MAX 宏，因此定义以下通用实现。 */
	#define configMIN( a, b ) ( ( ( a ) < ( b ) ) ? ( a ) : ( b ) )
#endif

#if configENABLE_BACKWARD_COMPATIBILITY == 1
	#define eTaskStateGet eTaskGetState
	#define portTickType TickType_t
	#define xTaskHandle TaskHandle_t
	#define xQueueHandle QueueHandle_t
	#define xSemaphoreHandle SemaphoreHandle_t
	#define xQueueSetHandle QueueSetHandle_t
	#define xQueueSetMemberHandle QueueSetMemberHandle_t
	#define xTimeOutType TimeOut_t
	#define xMemoryRegion MemoryRegion_t
	#define xTaskParameters TaskParameters_t
	#define xTaskStatusType	TaskStatus_t
	#define xTimerHandle TimerHandle_t
	#define xCoRoutineHandle CoRoutineHandle_t
	#define pdTASK_HOOK_CODE TaskHookFunction_t
	#define portTICK_RATE_MS portTICK_PERIOD_MS
	#define pcTaskGetTaskName pcTaskGetName
	#define pcTimerGetTimerName pcTimerGetName
	#define pcQueueGetQueueName pcQueueGetName
	#define vTaskGetTaskInfo vTaskGetInfo
	#define xTaskGetIdleRunTimeCounter ulTaskGetIdleRunTimeCounter

	/* 仅在调度程序代码中向后兼容 - 这些定义并
	不是真正需要的，但包含在内是为了完整性。 */
	#define tmrTIMER_CALLBACK TimerCallbackFunction_t
	#define pdTASK_CODE TaskFunction_t
	#define xListItem ListItem_t
	#define xList List_t

	/* 对于破坏列表数据隐藏并访问列表结构成员
	directly 的库（不应该这样做）。 */
	#define pxContainer pvContainer
#endif /* configENABLE_BACKWARD_COMPATIBILITY */

#if( configUSE_ALTERNATIVE_API != 0 )
	#error The alternative API was deprecated some time ago, and was removed in FreeRTOS V9.0 0
#endif

/* 将 configUSE_TASK_FPU_SUPPORT 设置为 0 以忽略浮点支持
if floating point hardware is otherwise supported by the FreeRTOS port in use.
并非所有包含浮点支持的
FreeRTOS 端口都支持该常量。 */
#ifndef configUSE_TASK_FPU_SUPPORT
	#define configUSE_TASK_FPU_SUPPORT 1
#endif

/* 将 configENABLE_MPU 设置为 1 以启用
MPU 支持，设置为 0 以禁用它。目前用于 ARMv8M 端口。 */
#ifndef configENABLE_MPU
	#define configENABLE_MPU 0
#endif

/* 将 configENABLE_FPU 设置为 1 以启用
FPU 支持，设置为 0 以禁用它。目前用于 ARMv8M 端口。 */
#ifndef configENABLE_FPU
	#define configENABLE_FPU 1
#endif

/* 将 configENABLE_TRUSTZONE 设置为 1 启用
TrustZone 支持，设置为 0 禁用它。目前用于 ARMv8M 端口。 */
#ifndef configENABLE_TRUSTZONE
	#define configENABLE_TRUSTZONE 1
#endif

/* 将 configRUN_FREERTOS_SECURE_ONLY 设
置为 1 以仅在安全端运行 FreeRTOS ARMv8M 端口。 */
#ifndef configRUN_FREERTOS_SECURE_ONLY
	#define configRUN_FREERTOS_SECURE_ONLY 0
#endif

/* 有时，FreeRTOSConfig.h 设置仅允许使用动态分
 * 配的 RAM 创建任务，在这种情况下，当删除任何任务时，就知
 * 道任务的堆栈和 TCB 都需要释放。  有时，
 * FreeRTOSConfig.h 设置仅允许使用静态分配的
 * RAM 创建任务，在这种情况下，当删除任何任务时，我们知道任
 * 务的堆栈或 TCB 都不应该被释放。  有时，
 * FreeRTOSConfig.h 设置允许使用静态或动态分配
 * 的 RAM 创建任务，在这种情况下，TCB 的成员用于记录堆
 * 栈和/或 TCB 是静态分配还是动态分配，因此当删除任务时，
 * 动态分配的 RAM 将再次释放，并且不会尝试释放该任务。
 * RAM 是静态分配的。仅当可以使用静态或动态分配的 RAM
 * 创建任务时，
 * tskSTATIC_AND_DYNAMIC_ALLOCATION_POSSIBLE
 * 才为 true。  请注意，如果
 * portUSING_MPU_WRAPPERS 为 1，则可以使用静态分配的堆栈和动态分配的 TCB 创建受保护的任务。
 *
 * 下表列出了 portUSING_MPU_WRAPPERS、
 * configSUPPORT_DYNAMIC_ALLOCATION 和
 * configSUPPORT_STATIC_ALLOCATION 的各种组合以及何时可以同时进行静态和动态分配：
 *  +-----+---------+--------+-----------------------------+-----------------------------------+------------------+-----------+
 * | MPU |动态 |静态|     可用功能 |       可能的分配|动态和|需要免费| |     |         |        |
 * |                                   |静电可能 |           |
 * +-----+---------+--------+-----------------------------+-----------------------------------+------------------+-----------+
 * | 0 | 0 | 1 | xTaskCreateStatic | TCB - 静态，堆栈 - 静态 |没有 |没有 |
 * +-----|---------|--------|-----------------------------|-----------------------------------|------------------|-----------|
 * | 0 | 1 | 0 | xTaskCreate | TCB - 动态，堆栈 - 动态| TCB没有 |是的 |
 * +-----|---------|--------|-----------------------------|-----------------------------------|------------------|-----------|
 * | 0 | 1 | 1 | xTaskCreate，| 1. TCB - 动态，堆栈 - 动态 |是的 |是的 | |     |         |
 * | xTaskCreateStatic | 2. TCB - 静态，堆栈 - 静态 |                  |           |
 * +-----|---------|--------|-----------------------------|-----------------------------------|------------------|-----------|
 * | 1 | 0 | 1 | xTaskCreateStatic，| TCB - 静态，堆栈 - 静态 |没有 |没有 | |     |         |        |
 * xTaskCreateRestrictedStatic |                                   |                  |           |
 * +-----|---------|--------|-----------------------------|-----------------------------------|------------------|-----------|
 * | 1 | 1 | 0 | xTaskCreate，| 1. TCB - 动态，堆栈 - 动态 |是的 |是的 | |     |         |
 * | xTaskCreateRestricted | 2. TCB - 动态，堆栈 - 静态 |                  |           |
 * +-----|---------|--------|-----------------------------|-----------------------------------|------------------|-----------|
 * | 1 | 1 | 1 | xTaskCreate，| 1. TCB - 动态，堆栈 - 动态 |是的 |是的 | |     |         |        |
 * xTaskCreateStatic，| 2. TCB - 动态，堆栈 - 静态 |                  |           | |     |         |
 * | xTaskCreateRestricted，| 3. TCB - 静态，堆栈 - 静态 |                  |           | |     |
 * |        | xTaskCreateRestrictedStatic |                                   |                  |           |
 * +-----+---------+--------+-----------------------------+-----------------------------------+------------------+-----------+
 */
#define tskSTATIC_AND_DYNAMIC_ALLOCATION_POSSIBLE	( ( ( portUSING_MPU_WRAPPERS == 0 ) && ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) && ( configSUPPORT_STATIC_ALLOCATION == 1 ) ) || \
													  ( ( portUSING_MPU_WRAPPERS == 1 ) && ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) ) )

/*
 * 根据软件工程最佳实践，FreeRTOS 实施了
 * 严格的数据隐藏策略，因此应用程序代码无法访问
 * FreeRTOS 用于维护任务、队列、信号量等
 * 状态的真实结构。  但是，如果应用程序编写者想
 * 要静态分配这样的对象，则需要知道该对象的大小。
 * 为此目的，使用保证具有与真实对象相同的尺寸
 * 和对齐要求的虚拟结构。  下面的虚拟列表和列表
 * 项结构用于包含在这样的虚拟结构中。
 */
struct xSTATIC_LIST_ITEM
{
	#if( configUSE_LIST_DATA_INTEGRITY_CHECK_BYTES == 1 )
		TickType_t xDummy1;
	#endif
	TickType_t xDummy2;
	void *pvDummy3[ 4 ];
	#if( configUSE_LIST_DATA_INTEGRITY_CHECK_BYTES == 1 )
		TickType_t xDummy4;
	#endif
};
typedef struct xSTATIC_LIST_ITEM StaticListItem_t;

/* 请参阅 struct xSTATIC_LIST_ITEM 定义上面的注释。 */
struct xSTATIC_MINI_LIST_ITEM
{
	#if( configUSE_LIST_DATA_INTEGRITY_CHECK_BYTES == 1 )
		TickType_t xDummy1;
	#endif
	TickType_t xDummy2;
	void *pvDummy3[ 2 ];
};
typedef struct xSTATIC_MINI_LIST_ITEM StaticMiniListItem_t;

/* 请参阅 struct xSTATIC_LIST_ITEM 定义上面的注释。 */
typedef struct xSTATIC_LIST
{
	#if( configUSE_LIST_DATA_INTEGRITY_CHECK_BYTES == 1 )
		TickType_t xDummy1;
	#endif
	UBaseType_t uxDummy2;
	void *pvDummy3;
	StaticMiniListItem_t xDummy4;
	#if( configUSE_LIST_DATA_INTEGRITY_CHECK_BYTES == 1 )
		TickType_t xDummy5;
	#endif
} StaticList_t;

/*
 * 根据软件工程最佳实践，特别是在提供可能在未来版本
 * 中更改的库时，FreeRTOS 实施了严格的数据
 * 隐藏策略。  这意味着应用程序代码无法访问
 * FreeRTOS 内部使用的任务结构。  但是，
 * 如果应用程序编写者想要静态分配创建任务所需的内存
 * ，则需要知道任务对象的大小。  下面的
 * StaticTask_t 结构就是为此目的而提供
 * 的。  无论使用哪种架构，也无论
 * FreeRTOSConfig.h 中的值如何设置
 * ，其尺寸和对齐要求都保证与真实结构相匹配。  其
 * 内容有些模糊，希望用户认识到直接使用结构成员是不明智的。
 */
typedef struct xSTATIC_TCB
{
	void				*pxDummy1;
	#if ( portUSING_MPU_WRAPPERS == 1 )
		xMPU_SETTINGS	xDummy2;
	#endif
	StaticListItem_t	xDummy3[ 2 ];
	UBaseType_t			uxDummy5;
	void				*pxDummy6;
	uint8_t				ucDummy7[ configMAX_TASK_NAME_LEN ];
	#if ( ( portSTACK_GROWTH > 0 ) || ( configRECORD_STACK_HIGH_ADDRESS == 1 ) )
		void			*pxDummy8;
	#endif
	#if ( portCRITICAL_NESTING_IN_TCB == 1 )
		UBaseType_t		uxDummy9;
	#endif
	#if ( configUSE_TRACE_FACILITY == 1 )
		UBaseType_t		uxDummy10[ 2 ];
	#endif
	#if ( configUSE_MUTEXES == 1 )
		UBaseType_t		uxDummy12[ 2 ];
	#endif
	#if ( configUSE_APPLICATION_TASK_TAG == 1 )
		void			*pxDummy14;
	#endif
	#if( configNUM_THREAD_LOCAL_STORAGE_POINTERS > 0 )
		void			*pvDummy15[ configNUM_THREAD_LOCAL_STORAGE_POINTERS ];
	#endif
	#if ( configGENERATE_RUN_TIME_STATS == 1 )
		uint32_t		ulDummy16;
	#endif
	#if ( configUSE_NEWLIB_REENTRANT == 1 )
		struct	_reent	xDummy17;
	#endif
	#if ( configUSE_TASK_NOTIFICATIONS == 1 )
		uint32_t 		ulDummy18;
		uint8_t 		ucDummy19;
	#endif
	#if ( tskSTATIC_AND_DYNAMIC_ALLOCATION_POSSIBLE != 0 )
		uint8_t			uxDummy20;
	#endif

	#if( INCLUDE_xTaskAbortDelay == 1 )
		uint8_t ucDummy21;
	#endif
	#if ( configUSE_POSIX_ERRNO == 1 )
		int				iDummy22;
	#endif
} StaticTask_t;

/*
 * 根据软件工程最佳实践，特别是在提供可能在未来
 * 版本中更改的库时，FreeRTOS 实施了严
 * 格的数据隐藏策略。  这意味着应用程序代码无
 * 法访问 FreeRTOS 内部使用的队列结构
 * 。  但是，如果应用程序编写者想要静态分配创
 * 建队列所需的内存，则需要知道队列对象的大小。
 * 下面的 StaticQueue_t 结构
 * 就是为此目的而提供的。  无论使用哪种架构，
 * 也无论 FreeRTOSConfig.h 中
 * 的值如何设置，其尺寸和对齐要求都保证与真实结
 * 构相匹配。  其内容有些模糊，希望用户认识到
 * 直接使用结构成员是不明智的。
 */
typedef struct xSTATIC_QUEUE
{
	void *pvDummy1[ 3 ];

	union
	{
		void *pvDummy2;
		UBaseType_t uxDummy2;
	} u;

	StaticList_t xDummy3[ 2 ];
	UBaseType_t uxDummy4[ 3 ];
	uint8_t ucDummy5[ 2 ];

	#if( ( configSUPPORT_STATIC_ALLOCATION == 1 ) && ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) )
		uint8_t ucDummy6;
	#endif

	#if ( configUSE_QUEUE_SETS == 1 )
		void *pvDummy7;
	#endif

	#if ( configUSE_TRACE_FACILITY == 1 )
		UBaseType_t uxDummy8;
		uint8_t ucDummy9;
	#endif

} StaticQueue_t;
typedef StaticQueue_t StaticSemaphore_t;

/*
 * 根据软件工程最佳实践，特别是在提供可能在未来
 * 版本中更改的库时，FreeRTOS 实施了严
 * 格的数据隐藏策略。  这意味着应用程序代码无
 * 法访问 FreeRTOS 内部使用的事件组结
 * 构。  但是，如果应用程序编写者想要静态分配
 * 创建事件组所需的内存，则需要知道事件组对象的
 * 大小。  下面的
 * StaticEventGroup_t 结构就
 * 是为此目的而提供的。无论使用哪种架构，也无论
 * FreeRTOSConfig.h 中的值如
 * 何设置，其尺寸和对齐要求都保证与真实结构相匹
 * 配。  其内容有些模糊，希望用户认识到直接使用结构成员是不明智的。
 */
typedef struct xSTATIC_EVENT_GROUP
{
	TickType_t xDummy1;
	StaticList_t xDummy2;

	#if( configUSE_TRACE_FACILITY == 1 )
		UBaseType_t uxDummy3;
	#endif

	#if( ( configSUPPORT_STATIC_ALLOCATION == 1 ) && ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) )
			uint8_t ucDummy4;
	#endif

} StaticEventGroup_t;

/*
 * 根据软件工程最佳实践，特别是在提供可能在未来
 * 版本中更改的库时，FreeRTOS 实施了严
 * 格的数据隐藏策略。  这意味着应用程序代码无
 * 法访问 FreeRTOS 内部使用的软件定时
 * 器结构。  但是，如果应用程序编写者想要静态
 * 分配创建软件计时器所需的内存，则需要知道队列
 * 对象的大小。下面的
 * StaticTimer_t 结构就是为此目的
 * 而提供的。  无论使用哪种架构，也无论
 * FreeRTOSConfig.h 中的值如何
 * 设置，其尺寸和对齐要求都保证与真实结构相匹配
 * 。  其内容有些模糊，希望用户认识到直接使用结构成员是不明智的。
 */
typedef struct xSTATIC_TIMER
{
	void				*pvDummy1;
	StaticListItem_t	xDummy2;
	TickType_t			xDummy3;
	void 				*pvDummy5;
	TaskFunction_t		pvDummy6;
	#if( configUSE_TRACE_FACILITY == 1 )
		UBaseType_t		uxDummy7;
	#endif
	uint8_t 			ucDummy8;

} StaticTimer_t;

/*
* 根据软件工程最佳实践，特别是在提供可能在未来版
* 本中更改的库时，FreeRTOS 实施了严格的
* 数据隐藏策略。  这意味着应用程序代码无法访问
* FreeRTOS 内部使用的流缓冲区结构。
* 但是，如果应用程序编写者想要静态分配创建流
* 缓冲区所需的内存，则需要知道流缓冲区对象的大小
* 。  下面的
* StaticStreamBuffer_t 结构
* 就是为此目的而提供的。无论使用哪种架构，也无论
* FreeRTOSConfig.h 中的值如何
* 设置，其尺寸和对齐要求都保证与真实结构相匹配。
* 其内容有些模糊，希望用户认识到直接使用结构成员是不明智的。
*/
typedef struct xSTATIC_STREAM_BUFFER
{
	size_t uxDummy1[ 4 ];
	void * pvDummy2[ 3 ];
	uint8_t ucDummy3;
	#if ( configUSE_TRACE_FACILITY == 1 )
		UBaseType_t uxDummy4;
	#endif
} StaticStreamBuffer_t;

/* 消息缓冲区建立在流缓冲区之上。 */
typedef StaticStreamBuffer_t StaticMessageBuffer_t;

#ifdef __cplusplus
}
#endif

#endif /* INC_FREERTOS_H */

