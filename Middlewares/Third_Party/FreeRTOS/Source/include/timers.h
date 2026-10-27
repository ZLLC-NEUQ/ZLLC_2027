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


#ifndef TIMERS_H
#define TIMERS_H

#ifndef INC_FREERTOS_H
	#error "include FreeRTOS.h must appear in source files before include timers.h"
#endif

/*lint -save -e537 仅当应用程序代码
恰好也包含 task.h 时，才会多次包含此标头。 */
#include "task.h"
/*lint - 恢复 */

#ifdef __cplusplus
extern "C" {
#endif

/*-----------------------------------------------------------
 * 宏和定义
 *----------------------------------------------------------*/

/* IDs 用于可以在定时器队列上发送/接收的命令。
这些只能通过构成公共软件定时器 API 的宏来使用，
如下定义。  从中断发送的命令必须使用最高编号，因为
tmrFIRST_FROM_ISR_COMMAND
用于确定是否应使用队列发送函数的任务或中断版本。 */
#define tmrCOMMAND_EXECUTE_CALLBACK_FROM_ISR 	( ( BaseType_t ) -2 )
#define tmrCOMMAND_EXECUTE_CALLBACK				( ( BaseType_t ) -1 )
#define tmrCOMMAND_START_DONT_TRACE				( ( BaseType_t ) 0 )
#define tmrCOMMAND_START					    ( ( BaseType_t ) 1 )
#define tmrCOMMAND_RESET						( ( BaseType_t ) 2 )
#define tmrCOMMAND_STOP							( ( BaseType_t ) 3 )
#define tmrCOMMAND_CHANGE_PERIOD				( ( BaseType_t ) 4 )
#define tmrCOMMAND_DELETE						( ( BaseType_t ) 5 )

#define tmrFIRST_FROM_ISR_COMMAND				( ( BaseType_t ) 6 )
#define tmrCOMMAND_START_FROM_ISR				( ( BaseType_t ) 6 )
#define tmrCOMMAND_RESET_FROM_ISR				( ( BaseType_t ) 7 )
#define tmrCOMMAND_STOP_FROM_ISR				( ( BaseType_t ) 8 )
#define tmrCOMMAND_CHANGE_PERIOD_FROM_ISR		( ( BaseType_t ) 9 )


/**
 * 引用软件定时器的类型。  例如，对 xTimerCreate() 的
 * 调用返回一个 TimerHandle_t 变量，该变量随后可用于在对
 * 其他软件定时器 API functions 的调用中引用主题定时器（
 * 例如，xTimerStart()、xTimerReset() 等）。
 */
struct tmrTimerControl; /* 旧的命名约定用于防止破坏内核感知调试器。 */
typedef struct tmrTimerControl * TimerHandle_t;

/*
 * 定义计时器回调函数必须符合的原型。
 */
typedef void (*TimerCallbackFunction_t)( TimerHandle_t xTimer );

/*
 * 定义与
 * xTimerPendFunctionCallFromISR() 函数一起使用的函数必须符合的原型。
 */
typedef void (*PendedFunction_t)( void *, uint32_t );

/**
 * TimerHandle_t xTimerCreate(常量
 * 字符*常量pcTimerName，TickType_t
 * xTimerPeriodInTicks, UBaseType_t
 * uxAutoReload，无效 * pvTimerID，
 * TimerCallbackFunction_t pxCallbackFunction );
 *
 * 创建一个新的软件定时器实例，并返回一个句柄
 * ，通过该句柄可以引用所创建的软件定时器。
 *
 * 在内部，在 FreeRTOS 实现中，软件定时器使用一个内
 * 存块，其中存储了定时器数据结构。  如果使用
 * xTimerCreate() 创建软件定时器，则所需的内存
 * 会在 xTimerCreate() 函数内自动动态分配。  （见
 * http://www.freertos.org/a00111.html). 如果使用以下命令创建软件定时器
 * xTimerCreateStatic() 那么应用程序编写者必须提
 * 供软件定时器将使用的内存。  因此，
 * xTimerCreateStatic() 允许在不使用任何动态内存分配的情况下创建软件定时器。
 *
 * 定时器是在休眠状态下创建的。  xTimerStart()、
 * xTimerReset()、xTimerStartFromISR()、
 * xTimerResetFromISR()、
 * xTimerChangePeriod() 和 xTimerChangePeriodFromISR() API 函数都可用于将定时器转换为活动状态。
 *
 * @param pcTimerName 分配给定时器的文本名称。  这样就完成了
 * 纯粹是为了辅助调试。  内核本身仅通过其句柄
 * 来引用计时器，而从不通过其名称来引用计时器。
 *
 * @param xTimerPeriodInTicks 定时器周期。  时间以刻度为单位定义
 * 周期，因此常数 portTICK_PERIOD_MS 可用于转换以毫秒为
 * 单位指定的时间。  例如，如果定时器必须在 100 个周期后到期，则
 * xTimerPeriodInTicks 应设置为 100。或者，如果定时
 * 器必须在 500ms 后到期，则可以将 xPeriod 设置为 to (
 * 500 / portTICK_PERIOD_MS )，前提是
 * configTICK_RATE_HZ 小于或等于 1000。周期必须大于 0。
 *
 * @param uxAutoReload 如果 uxAutoReload 设置为 pdTRUE，则定时器将
 * 以 xTimerPeriodInTicks 参数设置的频
 * 率重复过期。如果uxAutoReload设置为
 * pdFALSE，则定时器将是一次性定时器，并在超时后进入休眠状态。
 *
 * @param pvTimerID 分配给正在创建的计时器的标识符。
 * 通常，这将在计时器回调函数中使用，以在将同一回
 * 调函数分配给多个计时器时识别哪个计时器到期。
 * 定时器。
 *
 * @param pxCallbackFunction 定时器到期时调用的函数。
 * 回调函数必须具有 TimerCallbackFunction_t 定义的原型，即“void
 * vCallbackFunction( TimerHandle_t xTimer );”。
 *
 * @return 如果计时器成功创建，则新计时器的句柄
 * 返回创建的计时器。  如果由于没有足够的
 * FreeRTOS 堆剩余来分配计时器结
 * 构而无法创建计时器，则返回 NULL。
 *
 * 用法示例：
 * @verbatim
 * #define NUM_TIMERS 5
 *
 * // An array to hold handles to the created timers.
 * TimerHandle_t xTimers[ NUM_TIMERS ];
 *
 * // An array to hold a count of the number of times each timer expires.
 * int32_t lExpireCounters[ NUM_TIMERS ] = { 0 };
 *
 * // Define a callback function that will be used by multiple timer instances.
 * // The callback function does nothing but count the number of times the
 * // associated timer expires, and stop the timer once the timer has expired
 * // 10 times.
 * void vTimerCallback( TimerHandle_t pxTimer )
 * {
 * int32_t lArrayIndex;
 * const int32_t xMaxExpiryCountBeforeStopping = 10;
 *
 * 	   // Optionally do something if the pxTimer parameter is NULL.
 * 	   configASSERT( pxTimer );
 *
 *     // Which timer expired?
 *     lArrayIndex = ( int32_t ) pvTimerGetTimerID( pxTimer );
 *
 *     // Increment the number of times that pxTimer has expired.
 * lExpireCounters[ lArrayIndex ] += 1;
 *
 *     // If the timer has expired 10 times then stop it from running.
 *     if( lExpireCounters[ lArrayIndex ] == xMaxExpiryCountBeforeStopping )
 *     {
 *         // Do not use a block time if calling a timer API function from a
 *         // timer callback function, as doing so could cause a deadlock!
 *         xTimerStop( pxTimer, 0 );
 *     }
 * }
 *
 * void main( void )
 * {
 * int32_t x;
 *
 *     // Create then start some timers.  Starting the timers before the scheduler
 *     // has been started means the timers will start running immediately that
 *     // the scheduler starts.
 *     for( x = 0; x < NUM_TIMERS; x++ )
 *     {
 *         xTimers[ x ] = xTimerCreate(    "Timer",       // Just a text name, not used by the kernel.
 * ( 100 * x ), // 计时器周期（以滴答为单位）。
 * pdTRUE, // 定时器到期时将自动重新加载。 ( void *
 * ) x, // 为每个计时器分配一个与其数组索引相同的唯一 ID。
 * vTimerCallback // 每个定时器到期时都会调用相同的回调。
 *                                     );
 *
 *         if( xTimers[ x ] == NULL )
 *         {
 *             // The timer was not created.
 *         }
 *         else
 *         {
 *             // Start the timer.  No block time is specified, and even if one was
 *             // it would be ignored because the scheduler has not yet been
 *             // started.
 *             if( xTimerStart( xTimers[ x ], 0 ) != pdPASS )
 *             {
 *                 // The timer could not be set into the Active state.
 *             }
 *         }
 *     }
 *
 *     // ...
 *     // Create tasks here.
 *     // ...
 *
 *     // Starting the scheduler will start the timers running as they have already
 *     // been set into the active state.
 *     vTaskStartScheduler();
 *
 *     // Should not reach here.
 *     for( ;; );
 * }
 * @endverbatim
 */
#if( configSUPPORT_DYNAMIC_ALLOCATION == 1 )
	TimerHandle_t xTimerCreate(	const char * const pcTimerName,			/*lint !e971 仅字符串和单个字符允许使用非限定字符类型。 */
								const TickType_t xTimerPeriodInTicks,
								const UBaseType_t uxAutoReload,
								void * const pvTimerID,
								TimerCallbackFunction_t pxCallbackFunction ) PRIVILEGED_FUNCTION;
#endif

/**
 * TimerHandle_t xTimerCreateStatic（
 * const char * const pcTimerName，
 * TickType_t xTimerPeriodInTicks，
 * UBaseType_t uxAutoReload，无效*
 * pvTimerID，TimerCallbackFunction_t
 *	pxCallbackFunction， StaticTimer_t *pxTimerBuffer );
 *
 * 创建一个新的软件定时器实例，并返回一个句柄
 * ，通过该句柄可以引用所创建的软件定时器。
 *
 * 在内部，在 FreeRTOS 实现中，软件定时器使用一个内
 * 存块，其中存储了定时器数据结构。  如果使用
 * xTimerCreate() 创建软件定时器，则所需的内存
 * 会在 xTimerCreate() 函数内自动动态分配。  （见
 * http://www.freertos.org/a00111.html). 如果使用以下命令创建软件定时器
 * xTimerCreateStatic() 那么应用程序编写者必须提
 * 供软件定时器将使用的内存。  因此，
 * xTimerCreateStatic() 允许在不使用任何动态内存分配的情况下创建软件定时器。
 *
 * 定时器是在休眠状态下创建的。  xTimerStart()、
 * xTimerReset()、xTimerStartFromISR()、
 * xTimerResetFromISR()、
 * xTimerChangePeriod() 和 xTimerChangePeriodFromISR() API 函数都可用于将定时器转换为活动状态。
 *
 * @param pcTimerName 分配给定时器的文本名称。  这样就完成了
 * 纯粹是为了辅助调试。  内核本身仅通过其句柄
 * 来引用计时器，而从不通过其名称来引用计时器。
 *
 * @param xTimerPeriodInTicks 定时器周期。  时间以刻度为单位定义
 * 周期，因此常数 portTICK_PERIOD_MS 可用于转换以毫秒为
 * 单位指定的时间。  例如，如果定时器必须在 100 个周期后到期，则
 * xTimerPeriodInTicks 应设置为 100。或者，如果定时
 * 器必须在 500ms 后到期，则可以将 xPeriod 设置为 to (
 * 500 / portTICK_PERIOD_MS )，前提是
 * configTICK_RATE_HZ 小于或等于 1000。周期必须大于 0。
 *
 * @param uxAutoReload 如果 uxAutoReload 设置为 pdTRUE，则定时器将
 * 以 xTimerPeriodInTicks 参数设置的频
 * 率重复过期。如果uxAutoReload设置为
 * pdFALSE，则定时器将是一次性定时器，并在超时后进入休眠状态。
 *
 * @param pvTimerID 分配给正在创建的计时器的标识符。
 * 通常，这将在计时器回调函数中使用，以在将同一回
 * 调函数分配给多个计时器时识别哪个计时器到期。
 * 定时器。
 *
 * @param pxCallbackFunction 定时器到期时调用的函数。
 * 回调函数必须具有 TimerCallbackFunction_t 定义的原型，即“void
 * vCallbackFunction( TimerHandle_t xTimer );”。
 *
 * @param pxTimerBuffer 必须指向 StaticTimer_t 类型的变量，其中
 * 然后将用于保存软件定时器的数据
 * 结构，从而无需动态分配内存。
 *
 * @return 如果创建了定时器，则创建的定时器的句柄是
 * 回来了。  如果 pxTimerBuffer 是 NULL，则返回 NULL。
 *
 * 用法示例：
 * @verbatim
 *
 * // The buffer used to hold the software timer's data structure.
 * 静态 StaticTimer_t xTimerBuffer；
 *
 * // A variable that will be incremented by the software timer's callback
 * // function.
 * UBaseType_t uxVariableToIncrement = 0;
 *
 * // A software timer callback function that increments a variable passed to
 * // it when the software timer was created.  After the 5th increment the
 * // callback function stops the software timer.
 * static void prvTimerCallback( TimerHandle_t xExpiredTimer )
 * {
 * UBaseType_t *
 * puxVariableToIncrement; BaseType_t xReturned；
 *
 *     // Obtain the address of the variable to increment from the timer ID.
 *     puxVariableToIncrement = ( UBaseType_t * ) pvTimerGetTimerID( xExpiredTimer );
 *
 *     // Increment the variable to show the timer callback has executed.
 * (*puxVariableToIncrement)++;
 *
 *     // If this callback has executed the required number of times, stop the
 *     // timer.
 *     if( *puxVariableToIncrement == 5 )
 *     {
 *         // This is called from a timer callback so must not block.
 *         xTimerStop( xExpiredTimer, staticDONT_BLOCK );
 *     }
 * }
 *
 *
 * void main( void )
 * {
 *     // Create the software time.  xTimerCreateStatic() has an extra parameter
 *     // than the normal xTimerCreate() API function.  The parameter is a pointer
 *     // to the StaticTimer_t structure that will hold the software timer
 *     // structure.  If the parameter is passed as NULL then the structure will be
 *     // allocated dynamically, just as if xTimerCreate() had been called.
 *     xTimer = xTimerCreateStatic( "T1",             // Text name for the task.  Helps debugging only.  Not used by FreeRTOS.
 * xTimerPeriod, // 计时器的周期（以滴答为单位）。
 * pdTRUE, // 这是一个自动重载计时器。 ( void * ) &
 * uxVariableToIncrement, // 由软件定时器的回调函数
 * 递增的变量 prvTimerCallback, // 定时器到期时执行的函
 * 数。 &xTimerBuffer);  // 将保存软件定时器结构的缓冲区。
 *
 *     // The scheduler has not started yet so a block time is not used.
 *     xReturned = xTimerStart( xTimer, 0 );
 *
 *     // ...
 *     // Create tasks here.
 *     // ...
 *
 *     // Starting the scheduler will start the timers running as they have already
 *     // been set into the active state.
 *     vTaskStartScheduler();
 *
 *     // Should not reach here.
 *     for( ;; );
 * }
 * @endverbatim
 */
#if( configSUPPORT_STATIC_ALLOCATION == 1 )
	TimerHandle_t xTimerCreateStatic(	const char * const pcTimerName,			/*lint !e971 仅字符串和单个字符允许使用非限定字符类型。 */
										const TickType_t xTimerPeriodInTicks,
										const UBaseType_t uxAutoReload,
										void * const pvTimerID,
										TimerCallbackFunction_t pxCallbackFunction,
										StaticTimer_t *pxTimerBuffer ) PRIVILEGED_FUNCTION;
#endif /* configSUPPORT_STATIC_ALLOCATION */

/**
 * 无效 *pvTimerGetTimerID( TimerHandle_t xTimer );
 *
 * 返回分配给定时器的 ID。
 *
 * 使用调用用于创建计时器的 xTimerCreated()
 * 的 pvTimerID 参数以及调用
 * vTimerSetTimerID() API 函数，将 IDs 分配给计时器。
 *
 * 如果将同一个回调函数分配给多个定时器，则定时器 ID
 * 可以用作时间 specific（定时器本地）存储。
 *
 * @param xTimer 正在查询的定时器。
 *
 * @return 分配给正在查询的定时器的 ID。
 *
 * 用法示例：
 *
 * 请参阅 xTimerCreate() API 函数示例使用场景。
 */
void *pvTimerGetTimerID( const TimerHandle_t xTimer ) PRIVILEGED_FUNCTION;

/**
 * 无效 vTimerSetTimerID( TimerHandle_t xTimer, 无效 *pvNewID );
 *
 * 设置分配给定时器的 ID。
 *
 * 使用调用 xTimerCreated()（用于创建计时器
 * ）的 pvTimerID 参数将 IDs 分配给计时器。
 *
 * 如果将同一个回调函数分配给多个定时器，则定时器 ID
 * 可以用作时间 specific（定时器本地）存储。
 *
 * @param xTimer 定时器正在更新。
 *
 * @param pvNewID 要分配给定时器的 ID。
 *
 * 用法示例：
 *
 * 请参阅 xTimerCreate() API 函数示例使用场景。
 */
void vTimerSetTimerID( TimerHandle_t xTimer, void *pvNewID ) PRIVILEGED_FUNCTION;

/**
 * BaseType_t xTimerIsTimerActive( TimerHandle_t xTimer );
 *
 * 查询计时器以查看它是活动的还是休眠的。
 *
 * 如果满足以下条件，计时器将处于休眠状态
 * ：1) 计时器已创建但尚未启动，或者
 * 2) 计时器已过期且尚未重新启动。
 *
 * 定时器是在休眠状态下创建的。  xTimerStart()、
 * xTimerReset()、xTimerStartFromISR()、
 * xTimerResetFromISR()、
 * xTimerChangePeriod() 和 xTimerChangePeriodFromISR() API 函数都可用于将定时器转换为活动状态。
 *
 * @param xTimer 正在查询的定时器。
 *
 * 如果定时器休眠，则返回 @return pdFALSE。  除以下值外的值
 * 如果定时器处于活动状态，将返回 pdFALSE。
 *
 * 用法示例：
 * @verbatim
 * // This function assumes xTimer has already been created.
 * void vAFunction( TimerHandle_t xTimer )
 * {
 *     if( xTimerIsTimerActive( xTimer ) != pdFALSE ) // or more simply and equivalently "if( xTimerIsTimerActive( xTimer ) )"
 *     {
 *         // xTimer is active, do something.
 *     }
 *     else
 *     {
 *         // xTimer is not active, do something else.
 *     }
 * }
 * @endverbatim
 */
BaseType_t xTimerIsTimerActive( TimerHandle_t xTimer ) PRIVILEGED_FUNCTION;

/**
 * TaskHandle_t xTimerGetTimerDaemonTaskHandle（无效）；
 *
 * 简单地返回定时器服务/守护进程任务的句柄。  在调度程序启动之前调用
 * xTimerGetTimerDaemonTaskHandle() 无效。
 */
TaskHandle_t xTimerGetTimerDaemonTaskHandle( void ) PRIVILEGED_FUNCTION;

/**
 * BaseType_t xTimerStart( TimerHandle_t xTimer, TickType_t xTicksToWait );
 *
 * 计时器功能由计时器服务/守护程序任务提供。  许多
 * 公共 FreeRTOS 定时器 API 函数通过称
 * 为定时器命令队列的队列向定时器服务任务发送命令。
 * 定时器命令队列是内核本身私有的，应用程序代码不
 * 能直接访问。  定时器命令队列的长度由
 * configTIMER_QUEUE_LENGTH配置常量设置。
 *
 * xTimerStart() 启动先前使用
 * xTimerCreate() API 函数创建的计时器。
 * 如果定时器已启动并且已处于活动状态，则
 * xTimerStart() 具有与 xTimerReset() API 函数等效的功能。
 *
 * 启动定时器可确保定时器处于活动状态。  如果计时器
 * 没有同时停止、删除或重置，则在调用
 * xTimerStart() 后，与计时器关联的回调
 * 函数将在“n”个周期内被调用，其中“n”是计时器定义的周期。
 *
 * 在调度程序启动之前调用 xTimerStart() 是
 * 有效的，但是这样做时，计时器在调度程序启动之前不会真正
 * 启动，并且计时器到期时间将相对于调度程序启动的时间，而
 * 不是相对于调用 xTimerStart() 的时间。
 *
 * The configUSE_TIMERS configuration constant must be set to 1 for xTimerStart()
 * 可用。
 *
 * @param xTimer 正在启动/重新启动的定时器的句柄。
 *
 * @param xTicksToWait 指定调用任务应执行的时间（以刻度为单位）
 * 如果在调用 xTimerStart() 时队列已满，
 * 则保持在阻塞状态以等待启动命令成功发送到计时器命令队
 * 列。  如果在调度程序启动之前调用
 * xTimerStart()，则 xTicksToWait 将被忽略。
 *
 * 如果无法发送启动命令，将返回 @return pdFAIL
 * 即使在 xTicksToWait 滴答过去之后，计时器命
 * 令队列仍然存在。  如果命令成功发送到定时器命令队列，将
 * 返回 pdPASS。实际处理命令的时间取决于计时器服务/
 * 守护程序任务相对于系统中其他任务的优先级，尽管计时器到期
 * 时间与实际调用 xTimerStart() 的时间有关。
 * 定时器服务/守护程序任务优先级由
 * configTIMER_TASK_PRIORITY 配置常量设置。
 *
 * 用法示例：
 *
 * 请参阅 xTimerCreate() API 函数示例使用场景。
 *
 */
#define xTimerStart( xTimer, xTicksToWait ) xTimerGenericCommand( ( xTimer ), tmrCOMMAND_START, ( xTaskGetTickCount() ), NULL, ( xTicksToWait ) )

/**
 * BaseType_t xTimerStop( TimerHandle_t xTimer, TickType_t xTicksToWait );
 *
 * 计时器功能由计时器服务/守护程序任务提供。  许多
 * 公共 FreeRTOS 定时器 API 函数通过称
 * 为定时器命令队列的队列向定时器服务任务发送命令。
 * 定时器命令队列是内核本身私有的，应用程序代码不
 * 能直接访问。  定时器命令队列的长度由
 * configTIMER_QUEUE_LENGTH配置常量设置。
 *
 * xTimerStop() 停止之前使用 xTimerStart()、xTimerReset()、
 * xTimerStartFromISR()、xTimerResetFromISR()、
 * xTimerChangePeriod() 或 xTimerChangePeriodFromISR() API 函数启动的定时器。
 *
 * 停止计时器可确保计时器不处于活动状态。
 *
 * The configUSE_TIMERS configuration constant must be set to 1 for xTimerStop()
 * 可用。
 *
 * @param xTimer 正在停止的定时器的句柄。
 *
 * @param xTicksToWait 指定调用任务应执行的时间（以刻度为单位）
 * 如果在调用 xTimerStop() 时队列已满，则
 * 保持在阻塞状态以等待停止命令成功发送到计时器命令队列
 * 。  如果在调度程序启动之前调用
 * xTimerStop()，则 xTicksToWait 将被忽略。
 *
 * 如果无法发送停止命令，则返回 @return pdFAIL
 * 即使在 xTicksToWait 滴答过去之后，计时器
 * 命令队列仍然存在。  如果命令成功发送到定时器命令队列
 * ，将返回 pdPASS。实际处理命令的时间将取决于计时
 * 器服务/守护程序任务相对于系统中其他任务的优先级。
 * 定时器服务/守护程序任务优先级由
 * configTIMER_TASK_PRIORITY 配置常量设置。
 *
 * 用法示例：
 *
 * 请参阅 xTimerCreate() API 函数示例使用场景。
 *
 */
#define xTimerStop( xTimer, xTicksToWait ) xTimerGenericCommand( ( xTimer ), tmrCOMMAND_STOP, 0U, NULL, ( xTicksToWait ) )

/**
 * BaseType_t xTimerChangePeriod(
 *	TimerHandle_t xTimer, TickType_t
 *	xNewPeriod, TickType_t xTicksToWait );
 *
 * 计时器功能由计时器服务/守护程序任务提供。  许多
 * 公共 FreeRTOS 定时器 API 函数通过称
 * 为定时器命令队列的队列向定时器服务任务发送命令。
 * 定时器命令队列是内核本身私有的，应用程序代码不
 * 能直接访问。  定时器命令队列的长度由
 * configTIMER_QUEUE_LENGTH配置常量设置。
 *
 * xTimerChangePeriod() 更改先前使用
 * xTimerCreate() API 函数创建的定时器的周期。
 *
 * 可以调用
 * xTimerChangePeriod() 来更改活动或休眠状态定时器的周期。
 *
 * configUSE_TIMERS 配置常量必须设置为
 * 1，xTimerChangePeriod() 才可用。
 *
 * @param xTimer 周期发生变化的定时器的句柄。
 *
 * @param xNewPeriod xTimer 的新周期。定时器周期在中指定
 * 刻度周期，因此常量 portTICK_PERIOD_MS 可用于转
 * 换以毫秒为单位指定的时间。  例如，如果计时器必须在 100 个时
 * 钟周期后到期，则 xNewPeriod 应设置为 100。或者，
 * if the timer must expire after 500ms, then xNewPeriod can be set to
 * ( 500 / portTICK_PERIOD_MS ) 前提是
 * configTICK_RATE_HZ 小于或等于 1000。
 *
 * @param xTicksToWait 指定调用任务应执行的时间（以刻度为单位）
 * 如果调用 xTimerChangePeriod() 时队列已
 * 满，则保持阻塞状态以等待更改周期命令成功发送到计时器命令队列
 * 。  如果在调度程序启动之前调用
 * xTimerChangePeriod()，则 xTicksToWait 将被忽略。
 *
 * 如果无法更改周期命令，则返回 @return pdFAIL
 * 即使在 xTicksToWait 时钟周期过去后，也会
 * 发送到计时器命令队列。如果命令成功发送到定时器命令队列
 * ，将返回 pdPASS。  实际处理命令的时间将取决于
 * 计时器服务/守护程序任务相对于系统中其他任务的优先级。
 * 定时器服务/守护程序任务优先级由
 * configTIMER_TASK_PRIORITY 配置常量设置。
 *
 * 用法示例：
 * @verbatim
 * // This function assumes xTimer has already been created.  If the timer
 * // referenced by xTimer is already active when it is called, then the timer
 * // is deleted.  If the timer referenced by xTimer is not active when it is
 * // called, then the period of the timer is set to 500ms and the timer is
 * // started.
 * void vAFunction( TimerHandle_t xTimer )
 * {
 *     if( xTimerIsTimerActive( xTimer ) != pdFALSE ) // or more simply and equivalently "if( xTimerIsTimerActive( xTimer ) )"
 *     {
 *         // xTimer is already active - delete it.
 *         xTimerDelete( xTimer );
 *     }
 *     else
 *     {
 *         // xTimer is not active, change its period to 500ms.  This will also
 *         // cause the timer to start.  Block for a maximum of 100 ticks if the
 *         // change period command cannot immediately be sent to the timer
 *         // command queue.
 *         if( xTimerChangePeriod( xTimer, 500 / portTICK_PERIOD_MS, 100 ) == pdPASS )
 *         {
 *             // The command was successfully sent.
 *         }
 *         else
 *         {
 *             // The command could not be sent, even after waiting for 100 ticks
 *             // to pass.  Take appropriate action here.
 *         }
 *     }
 * }
 * @endverbatim
 */
 #define xTimerChangePeriod( xTimer, xNewPeriod, xTicksToWait ) xTimerGenericCommand( ( xTimer ), tmrCOMMAND_CHANGE_PERIOD, ( xNewPeriod ), NULL, ( xTicksToWait ) )

/**
 * BaseType_t xTimerDelete( TimerHandle_t xTimer, TickType_t xTicksToWait );
 *
 * 计时器功能由计时器服务/守护程序任务提供。  许多
 * 公共 FreeRTOS 定时器 API 函数通过称
 * 为定时器命令队列的队列向定时器服务任务发送命令。
 * 定时器命令队列是内核本身私有的，应用程序代码不
 * 能直接访问。  定时器命令队列的长度由
 * configTIMER_QUEUE_LENGTH配置常量设置。
 *
 * xTimerDelete() 删除先前使用
 * xTimerCreate() API 函数创建的计时器。
 *
 * configUSE_TIMERS 配置常量必须设
 * 置为 1，xTimerDelete() 才可用。
 *
 * @param xTimer 被删除的定时器的句柄。
 *
 * @param xTicksToWait 指定调用任务应执行的时间（以刻度为单位）
 * 如果调用 xTimerDelete() 时队列已满，
 * 则保持阻塞状态以等待删除命令成功发送到计时器命令队列
 * 。  如果在调度程序启动之前调用
 * xTimerDelete()，则 xTicksToWait 将被忽略。
 *
 * 如果删除命令无法发送到，将返回 @return pdFAIL
 * 即使在 xTicksToWait 滴答过去之后，计时器
 * 命令队列仍然存在。  如果命令成功发送到定时器命令队列
 * ，将返回 pdPASS。实际处理命令的时间将取决于计时
 * 器服务/守护程序任务相对于系统中其他任务的优先级。
 * 定时器服务/守护程序任务优先级由
 * configTIMER_TASK_PRIORITY 配置常量设置。
 *
 * 用法示例：
 *
 * 请参阅 xTimerChangePeriod() API 函数示例使用场景。
 */
#define xTimerDelete( xTimer, xTicksToWait ) xTimerGenericCommand( ( xTimer ), tmrCOMMAND_DELETE, 0U, NULL, ( xTicksToWait ) )

/**
 * BaseType_t xTimerReset( TimerHandle_t xTimer, TickType_t xTicksToWait );
 *
 * 计时器功能由计时器服务/守护程序任务提供。  许多
 * 公共 FreeRTOS 定时器 API 函数通过称
 * 为定时器命令队列的队列向定时器服务任务发送命令。
 * 定时器命令队列是内核本身私有的，应用程序代码不
 * 能直接访问。  定时器命令队列的长度由
 * configTIMER_QUEUE_LENGTH配置常量设置。
 *
 * xTimerReset() 重新启动之前使用
 * xTimerCreate() API 函数创建的计时器。  如果
 * 定时器已经启动并且已经处于活动状态，则
 * xTimerReset() 将导致定时器重新评估其到期时间，以便
 * 它相对于调用 xTimerReset() 的时间。  如果定时器
 * 处于休眠状态，则 xTimerReset() 具有与 xTimerStart() API 函数等效的功能。
 *
 * 重置定时器可确保定时器处于活动状态。  如果计时器
 * 没有同时停止、删除或重置，则在调用
 * xTimerReset() 后，与计时器关联的回调
 * 函数将在“n”个周期内被调用，其中“n”是计时器定义的周期。
 *
 * 在调度程序启动之前调用 xTimerReset() 是
 * 有效的，但是这样做时，计时器在调度程序启动之前不会真正
 * 启动，并且计时器到期时间将相对于调度程序启动的时间，而
 * 不是相对于调用 xTimerReset() 的时间。
 *
 * The configUSE_TIMERS configuration constant must be set to 1 for xTimerReset()
 * 可用。
 *
 * @param xTimer 定时器复位/启动/重启的句柄。
 *
 * @param xTicksToWait 指定调用任务应执行的时间（以刻度为单位）
 * 如果在调用 xTimerReset() 时队列已满，
 * 则保持在阻塞状态以等待重置命令成功发送到计时器命令队
 * 列。  如果在调度程序启动之前调用
 * xTimerReset()，则 xTicksToWait 将被忽略。
 *
 * 如果复位命令无法发送到，将返回 @return pdFAIL
 * 即使在 xTicksToWait 滴答过去之后，计时器命
 * 令队列仍然存在。  如果命令成功发送到定时器命令队列，将
 * 返回 pdPASS。实际处理命令的时间取决于计时器服务/
 * 守护程序任务相对于系统中其他任务的优先级，尽管计时器到期
 * 时间与实际调用 xTimerStart() 的时间有关。
 * 定时器服务/守护程序任务优先级由
 * configTIMER_TASK_PRIORITY 配置常量设置。
 *
 * 用法示例：
 * @verbatim
 * // When a key is pressed, an LCD back-light is switched on.  If 5 seconds pass
 * // without a key being pressed, then the LCD back-light is switched off.  In
 * // this case, the timer is a one-shot timer.
 *
 * TimerHandle_t xBacklightTimer = NULL;
 *
 * // The callback function assigned to the one-shot timer.  In this case the
 * // parameter is not used.
 * void vBacklightTimerCallback( TimerHandle_t pxTimer )
 * {
 *     // The timer expired, therefore 5 seconds must have passed since a key
 *     // was pressed.  Switch off the LCD back-light.
 *     vSetBacklightState( BACKLIGHT_OFF );
 * }
 *
 * // The key press event handler.
 * void vKeyPressEventHandler( char cKey )
 * {
 *     // Ensure the LCD back-light is on, then reset the timer that is
 *     // responsible for turning the back-light off after 5 seconds of
 *     // key inactivity.  Wait 10 ticks for the command to be successfully sent
 *     // if it cannot be sent immediately.
 *     vSetBacklightState( BACKLIGHT_ON );
 *     if( xTimerReset( xBacklightTimer, 100 ) != pdPASS )
 *     {
 *         // The reset command was not executed successfully.  Take appropriate
 *         // action here.
 *     }
 *
 *     // Perform the rest of the key processing here.
 * }
 *
 * void main( void )
 * {
 * int32_t x;
 *
 *     // Create then start the one-shot timer that is responsible for turning
 *     // the back-light off if no keys are pressed within a 5 second period.
 *     xBacklightTimer = xTimerCreate( "BacklightTimer",           // Just a text name, not used by the kernel.
 * ( 5000 / portTICK_PERIOD_MS), // 计时
 * 器周期（以滴答为单位）。 pdFALSE, // 该定时器是一次性定时
 * 器。 0, // 回调不使用该 id，因此可以采用任何值。
 * vBacklightTimerCallback //关闭LCD背光的回调函数。
 *                                   );
 *
 *     if( xBacklightTimer == NULL )
 *     {
 *         // The timer was not created.
 *     }
 *     else
 *     {
 *         // Start the timer.  No block time is specified, and even if one was
 *         // it would be ignored because the scheduler has not yet been
 *         // started.
 *         if( xTimerStart( xBacklightTimer, 0 ) != pdPASS )
 *         {
 *             // The timer could not be set into the Active state.
 *         }
 *     }
 *
 *     // ...
 *     // Create tasks here.
 *     // ...
 *
 *     // Starting the scheduler will start the timer running as it has already
 *     // been set into the active state.
 *     vTaskStartScheduler();
 *
 *     // Should not reach here.
 *     for( ;; );
 * }
 * @endverbatim
 */
#define xTimerReset( xTimer, xTicksToWait ) xTimerGenericCommand( ( xTimer ), tmrCOMMAND_RESET, ( xTaskGetTickCount() ), NULL, ( xTicksToWait ) )

/**
 * BaseType_t xTimerStartFromISR( TimerHandle_t
 *	xTimer, BaseType_t *pxHigherPriorityTaskWoken );
 *
 * 可以从中断服务调用的 xTimerStart() 版本
 * 常规。
 *
 * @param xTimer 正在启动/重新启动的定时器的句柄。
 *
 * @param pxHigherPriorityTaskWoken 定时器服务/守护任务花费最多
 * 处于阻塞状态的时间，等待消息到达计时器命令队列。  调用
 * xTimerStartFromISR() 会将消息写入计时器命
 * 令队列，因此有可能将计时器服务/守护程序任务转出阻塞状态。
 * 如果调用 xTimerStartFromISR() 导致定时器
 * 服务/守护程序任务离开阻塞状态，并且定时器服务/守护程序任务的
 * 优先级等于或大于当前执行的 task（被中断的任务），则 *
 * pxHigherPriorityTaskWoken 将在
 * xTimerStartFromISR() 函数内部设置为
 * pdTRUE。  如果
 * xTimerStartFromISR() 将此值设置为 pdTRUE，则应在中断退出之前执行上下文切换。
 *
 * 如果无法发送启动命令，将返回 @return pdFAIL
 * 定时器命令队列。  如果命令成功发送到定时器命令队
 * 列，则返回pdPASS。  实际处理命令的时间取决
 * 于计时器服务/守护程序任务相对于系统中其他任务的优
 * 先级，尽管计时器到期时间与实际调用
 * xTimerStartFromISR() 的时间有
 * 关。  定时器服务/守护程序任务优先级由
 * configTIMER_TASK_PRIORITY 配置常量设置。
 *
 * 用法示例：
 * @verbatim
 * // This scenario assumes xBacklightTimer has already been created.  When a
 * // key is pressed, an LCD back-light is switched on.  If 5 seconds pass
 * // without a key being pressed, then the LCD back-light is switched off.  In
 * // this case, the timer is a one-shot timer, and unlike the example given for
 * // the xTimerReset() function, the key press event handler is an interrupt
 * // service routine.
 *
 * // The callback function assigned to the one-shot timer.  In this case the
 * // parameter is not used.
 * void vBacklightTimerCallback( TimerHandle_t pxTimer )
 * {
 *     // The timer expired, therefore 5 seconds must have passed since a key
 *     // was pressed.  Switch off the LCD back-light.
 *     vSetBacklightState( BACKLIGHT_OFF );
 * }
 *
 * // The key press interrupt service routine.
 * void vKeyPressEventInterruptHandler( void )
 * {
 * BaseType_t xHigherPriorityTaskWoken = pdFALSE;
 *
 *     // Ensure the LCD back-light is on, then restart the timer that is
 *     // responsible for turning the back-light off after 5 seconds of
 *     // key inactivity.  This is an interrupt service routine so can only
 *     // call FreeRTOS API functions that end in "FromISR".
 *     vSetBacklightState( BACKLIGHT_ON );
 *
 *     // xTimerStartFromISR() or xTimerResetFromISR() could be called here
 *     // as both cause the timer to re-calculate its expiry time.
 *     // xHigherPriorityTaskWoken was initialised to pdFALSE when it was
 *     // declared (in this function).
 *     if( xTimerStartFromISR( xBacklightTimer, &xHigherPriorityTaskWoken ) != pdPASS )
 *     {
 *         // The start command was not executed successfully.  Take appropriate
 *         // action here.
 *     }
 *
 *     // Perform the rest of the key processing here.
 *
 *     // If xHigherPriorityTaskWoken equals pdTRUE, then a context switch
 *     // should be performed.  The syntax required to perform a context switch
 *     // from inside an ISR varies from port to port, and from compiler to
 *     // compiler.  Inspect the demos for the port you are using to find the
 *     // actual syntax required.
 *     if( xHigherPriorityTaskWoken != pdFALSE )
 *     {
 *         // Call the interrupt safe yield function here (actual function
 *         // depends on the FreeRTOS port being used).
 *     }
 * }
 * @endverbatim
 */
#define xTimerStartFromISR( xTimer, pxHigherPriorityTaskWoken ) xTimerGenericCommand( ( xTimer ), tmrCOMMAND_START_FROM_ISR, ( xTaskGetTickCountFromISR() ), ( pxHigherPriorityTaskWoken ), 0U )

/**
 * BaseType_t xTimerStopFromISR( TimerHandle_t
 *	xTimer, BaseType_t *pxHigherPriorityTaskWoken );
 *
 * 可以从中断服务调用的 xTimerStop() 版本
 * 常规。
 *
 * @param xTimer 正在停止的定时器的句柄。
 *
 * @param pxHigherPriorityTaskWoken 定时器服务/守护任务花费最多
 * 处于阻塞状态的时间，等待消息到达计时器命令队列。  调用
 * xTimerStopFromISR() 会将消息写入计时器命令
 * 队列，因此有可能将计时器服务/守护程序任务转出阻塞状态。  如
 * 果调用 xTimerStopFromISR() 导致定时器服务
 * /守护程序任务离开阻塞状态，并且定时器服务/守护程序任务的优先
 * 级等于或大于当前执行的 task（被中断的任务），则 *
 * pxHigherPriorityTaskWoken 将在
 * xTimerStopFromISR() 函数内部设置为
 * pdTRUE。  如果 xTimerStopFromISR()
 * 将此值设置为 pdTRUE，则应在中断退出之前执行上下文切换。
 *
 * 如果无法发送停止命令，则返回 @return pdFAIL
 * 定时器命令队列。  如果命令成功发送到定时器命令队列，
 * 则返回pdPASS。  实际处理命令的时间将取决于计时
 * 器服务/守护程序任务相对于系统中其他任务的优先级。
 * 定时器服务/守护程序任务优先级由
 * configTIMER_TASK_PRIORITY 配置常量设置。
 *
 * 用法示例：
 * @verbatim
 * // This scenario assumes xTimer has already been created and started.  When
 * // an interrupt occurs, the timer should be simply stopped.
 *
 * // The interrupt service routine that stops the timer.
 * void vAnExampleInterruptServiceRoutine( void )
 * {
 * BaseType_t xHigherPriorityTaskWoken = pdFALSE;
 *
 *     // The interrupt has occurred - simply stop the timer.
 *     // xHigherPriorityTaskWoken was set to pdFALSE where it was defined
 *     // (within this function).  As this is an interrupt service routine, only
 *     // FreeRTOS API functions that end in "FromISR" can be used.
 *     if( xTimerStopFromISR( xTimer, &xHigherPriorityTaskWoken ) != pdPASS )
 *     {
 *         // The stop command was not executed successfully.  Take appropriate
 *         // action here.
 *     }
 *
 *     // If xHigherPriorityTaskWoken equals pdTRUE, then a context switch
 *     // should be performed.  The syntax required to perform a context switch
 *     // from inside an ISR varies from port to port, and from compiler to
 *     // compiler.  Inspect the demos for the port you are using to find the
 *     // actual syntax required.
 *     if( xHigherPriorityTaskWoken != pdFALSE )
 *     {
 *         // Call the interrupt safe yield function here (actual function
 *         // depends on the FreeRTOS port being used).
 *     }
 * }
 * @endverbatim
 */
#define xTimerStopFromISR( xTimer, pxHigherPriorityTaskWoken ) xTimerGenericCommand( ( xTimer ), tmrCOMMAND_STOP_FROM_ISR, 0, ( pxHigherPriorityTaskWoken ), 0U )

/**
 * BaseType_t xTimerChangePeriodFromISR(
 *	TimerHandle_t xTimer, TickType_t
 *	xNewPeriod, BaseType_t *pxHigherPriorityTaskWoken );
 *
 * 可以从中断服务例程调用的
 * xTimerChangePeriod() 版本。
 *
 * @param xTimer 周期发生变化的定时器的句柄。
 *
 * @param xNewPeriod xTimer 的新周期。定时器周期在中指定
 * 刻度周期，因此常量 portTICK_PERIOD_MS 可用于转
 * 换以毫秒为单位指定的时间。  例如，如果计时器必须在 100 个时
 * 钟周期后到期，则 xNewPeriod 应设置为 100。或者，
 * if the timer must expire after 500ms, then xNewPeriod can be set to
 * ( 500 / portTICK_PERIOD_MS ) 前提是
 * configTICK_RATE_HZ 小于或等于 1000。
 *
 * @param pxHigherPriorityTaskWoken 定时器服务/守护任务花费最多
 * 处于阻塞状态的时间，等待消息到达计时器命令队列。  调用
 * xTimerChangePeriodFromISR() 会将消
 * 息写入计时器命令队列，因此有可能将计时器服务/守护程序任务转出
 * 阻塞状态。  如果调用
 * xTimerChangePeriodFromISR() 导致定
 * 时器服务/守护程序任务离开阻塞状态，并且定时器服务/守护程序任
 * 务的优先级等于或大于当前执行的 task（被中断的任务），则
 * *pxHigherPriorityTaskWoken 将在
 * xTimerChangePeriodFromISR() 函数内
 * 部设置为 pdTRUE。  如果
 * xTimerChangePeriodFromISR() 将此值设置为 pdTRUE，则应在中断退出之前执行上下文切换。
 *
 * 如果命令更改定时器周期，将返回 @return pdFAIL
 * 无法发送到计时器命令队列。  如果命令成功发送到定时器命
 * 令队列，则返回pdPASS。  实际处理命令的时间将取决
 * 于计时器服务/守护程序任务相对于系统中其他任务的优先级。
 * 定时器服务/守护程序任务优先级由
 * configTIMER_TASK_PRIORITY 配置常量设置。
 *
 * 用法示例：
 * @verbatim
 * // This scenario assumes xTimer has already been created and started.  When
 * // an interrupt occurs, the period of xTimer should be changed to 500ms.
 *
 * // The interrupt service routine that changes the period of xTimer.
 * void vAnExampleInterruptServiceRoutine( void )
 * {
 * BaseType_t xHigherPriorityTaskWoken = pdFALSE;
 *
 *     // The interrupt has occurred - change the period of xTimer to 500ms.
 *     // xHigherPriorityTaskWoken was set to pdFALSE where it was defined
 *     // (within this function).  As this is an interrupt service routine, only
 *     // FreeRTOS API functions that end in "FromISR" can be used.
 *     if( xTimerChangePeriodFromISR( xTimer, &xHigherPriorityTaskWoken ) != pdPASS )
 *     {
 *         // The command to change the timers period was not executed
 *         // successfully.  Take appropriate action here.
 *     }
 *
 *     // If xHigherPriorityTaskWoken equals pdTRUE, then a context switch
 *     // should be performed.  The syntax required to perform a context switch
 *     // from inside an ISR varies from port to port, and from compiler to
 *     // compiler.  Inspect the demos for the port you are using to find the
 *     // actual syntax required.
 *     if( xHigherPriorityTaskWoken != pdFALSE )
 *     {
 *         // Call the interrupt safe yield function here (actual function
 *         // depends on the FreeRTOS port being used).
 *     }
 * }
 * @endverbatim
 */
#define xTimerChangePeriodFromISR( xTimer, xNewPeriod, pxHigherPriorityTaskWoken ) xTimerGenericCommand( ( xTimer ), tmrCOMMAND_CHANGE_PERIOD_FROM_ISR, ( xNewPeriod ), ( pxHigherPriorityTaskWoken ), 0U )

/**
 * BaseType_t xTimerResetFromISR( TimerHandle_t
 *	xTimer, BaseType_t *pxHigherPriorityTaskWoken );
 *
 * 可以从中断服务调用的 xTimerReset() 版本
 * 常规。
 *
 * @param xTimer 要启动、复位或复位的定时器的句柄
 * 重新启动。
 *
 * @param pxHigherPriorityTaskWoken 定时器服务/守护任务花费最多
 * 处于阻塞状态的时间，等待消息到达计时器命令队列。  调用
 * xTimerResetFromISR() 会将消息写入计时器命
 * 令队列，因此有可能将计时器服务/守护程序任务转出阻塞状态。
 * 如果调用 xTimerResetFromISR() 导致定时器
 * 服务/守护程序任务离开阻塞状态，并且定时器服务/守护程序任务的
 * 优先级等于或大于当前执行的 task（被中断的任务），则 *
 * pxHigherPriorityTaskWoken 将在
 * xTimerResetFromISR() 函数内部设置为
 * pdTRUE。  如果
 * xTimerResetFromISR() 将此值设置为 pdTRUE，则应在中断退出之前执行上下文切换。
 *
 * 如果复位命令无法发送到，将返回 @return pdFAIL
 * 定时器命令队列。  如果命令成功发送到定时器命令队列，则返
 * 回pdPASS。  实际处理命令的时间取决于计时器服务/守
 * 护程序任务相对于系统中其他任务的优先级，尽管计时器到期时间
 * 与实际调用 xTimerResetFromISR() 的时
 * 间有关。  定时器服务/守护程序任务优先级由
 * configTIMER_TASK_PRIORITY 配置常量设置。
 *
 * 用法示例：
 * @verbatim
 * // This scenario assumes xBacklightTimer has already been created.  When a
 * // key is pressed, an LCD back-light is switched on.  If 5 seconds pass
 * // without a key being pressed, then the LCD back-light is switched off.  In
 * // this case, the timer is a one-shot timer, and unlike the example given for
 * // the xTimerReset() function, the key press event handler is an interrupt
 * // service routine.
 *
 * // The callback function assigned to the one-shot timer.  In this case the
 * // parameter is not used.
 * void vBacklightTimerCallback( TimerHandle_t pxTimer )
 * {
 *     // The timer expired, therefore 5 seconds must have passed since a key
 *     // was pressed.  Switch off the LCD back-light.
 *     vSetBacklightState( BACKLIGHT_OFF );
 * }
 *
 * // The key press interrupt service routine.
 * void vKeyPressEventInterruptHandler( void )
 * {
 * BaseType_t xHigherPriorityTaskWoken = pdFALSE;
 *
 *     // Ensure the LCD back-light is on, then reset the timer that is
 *     // responsible for turning the back-light off after 5 seconds of
 *     // key inactivity.  This is an interrupt service routine so can only
 *     // call FreeRTOS API functions that end in "FromISR".
 *     vSetBacklightState( BACKLIGHT_ON );
 *
 *     // xTimerStartFromISR() or xTimerResetFromISR() could be called here
 *     // as both cause the timer to re-calculate its expiry time.
 *     // xHigherPriorityTaskWoken was initialised to pdFALSE when it was
 *     // declared (in this function).
 *     if( xTimerResetFromISR( xBacklightTimer, &xHigherPriorityTaskWoken ) != pdPASS )
 *     {
 *         // The reset command was not executed successfully.  Take appropriate
 *         // action here.
 *     }
 *
 *     // Perform the rest of the key processing here.
 *
 *     // If xHigherPriorityTaskWoken equals pdTRUE, then a context switch
 *     // should be performed.  The syntax required to perform a context switch
 *     // from inside an ISR varies from port to port, and from compiler to
 *     // compiler.  Inspect the demos for the port you are using to find the
 *     // actual syntax required.
 *     if( xHigherPriorityTaskWoken != pdFALSE )
 *     {
 *         // Call the interrupt safe yield function here (actual function
 *         // depends on the FreeRTOS port being used).
 *     }
 * }
 * @endverbatim
 */
#define xTimerResetFromISR( xTimer, pxHigherPriorityTaskWoken ) xTimerGenericCommand( ( xTimer ), tmrCOMMAND_RESET_FROM_ISR, ( xTaskGetTickCountFromISR() ), ( pxHigherPriorityTaskWoken ), 0U )


/**
 * BaseType_t
 * xTimerPendFunctionCallFromISR(
 * PendedFunction_t xFunctionToPend，无效 *
 * pvParameter1, uint32_t ulParameter2, BaseType_t *pxHigherPriorityTaskWoken );
 *
 *
 * 从应用程序中断服务例程中使用，将函数的执行推迟到
 * RTOS 守护进程 task（定时器服务任务，因此该函数
 * 在 timers.c 中实现，并以“Timer”为前缀）。
 *
 * 理想情况下，中断服务 routine (ISR) 保
 * 持尽可能短，但有时 ISR 要么有大量处理要做，要么
 * 需要执行不确定的处理。  在这些情况下，
 * xTimerPendFunctionCallFromISR()
 * 可用于将函数的处理推迟到 RTOS 守护程序任务。
 *
 * 提供了一种机制，允许中断直接返回到随
 * 后执行挂起的回调函数的任务。  这允
 * 许回调函数与中断一起连续执行——就像
 * 回调函数在中断本身中执行一样。
 *
 * @param xFunctionToPend 从定时器服务执行的函数/
 * 守护进程任务。  该功能必须符合PendedFunction_t
 * 原型。
 *
 * @param pvParameter1 回调函数第一个参数的值。
 * 该参数具有 void * 类型，允许它用于传递
 * 任何类型。例如，无符号长整型可以转换为
 * void *，或者 void * 可用于指向结构。
 *
 * @param ulParameter2 回调函数第二个参数的值。
 *
 * @param pxHigherPriorityTaskWoken 如上所述，调用该函数
 * 将导致一条消息被发送到计时器守护程序任务。  如果定时器守护进
 * 程 task（在 FreeRTOSConfig.h 中使用
 * configTIMER_TASK_PRIORITY 设置）的优
 * 先级高于当前运行的 task（中断中断的任务）的优先级，则 *
 * pxHigherPriorityTaskWoken 将在
 * xTimerPendFunctionCallFromISR()
 * 中设置为 pdTRUE，表示上下文切换应在中断退出之前请求。
 * 因此，*pxHigherPriorityTaskWoken
 * 必须初始化为 pdFALSE。  请参阅下面的示例代码。
 *
 * 如果消息发送成功，则返回 @return pdPASS
 * 定时器守护进程任务，否则返回 pdFALSE。
 *
 * 用法示例：
 * @verbatim
 *
 *	// The callback function that will execute in the context of the daemon task.
 *  // Note callback functions must all use this same prototype.
 *  void vProcessInterface( void *pvParameter1, uint32_t ulParameter2 )
 *	{
 *	BaseType_t xInterfaceToService;
 *
 *		// The interface that requires servicing is passed in the second
 *      // parameter.  The first parameter is not used in this case.
 *		xInterfaceToService = ( BaseType_t ) ulParameter2;
 *
 *		// ...Perform the processing here...
 *	}
 *
 *	// An ISR that receives data packets from multiple interfaces
 *  void vAnISR( void )
 *	{
 *	BaseType_t xInterfaceToService、xHigherPriorityTaskWoken；
 *
 *		// Query the hardware to determine which interface needs processing.
 *		xInterfaceToService = prvCheckInterfaces();
 *
 *      // The actual processing is to be deferred to a task.  Request the
 *      // vProcessInterface() callback function is executed, passing in the
 *		// number of the interface that needs processing.  The interface to
 *		// service is passed in the second parameter.  The first parameter is
 *		// not used in this case.
 *		xHigherPriorityTaskWoken = pdFALSE;
 *		xTimerPendFunctionCallFromISR( vProcessInterface, NULL, ( uint32_t ) xInterfaceToService, &xHigherPriorityTaskWoken );
 *
 *		// If xHigherPriorityTaskWoken is now set to pdTRUE then a context
 *		// switch should be requested.  The macro used is port specific and will
 *		// be either portYIELD_FROM_ISR() or portEND_SWITCHING_ISR() - refer to
 *		// the documentation page for the port being used.
 *		portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
 *
 *	}
 * @endverbatim
 */
BaseType_t xTimerPendFunctionCallFromISR( PendedFunction_t xFunctionToPend, void *pvParameter1, uint32_t ulParameter2, BaseType_t *pxHigherPriorityTaskWoken ) PRIVILEGED_FUNCTION;

 /**
  * BaseType_t xTimerPendFunctionCall(
  * PendedFunction_t xFunctionToPend,
  * 无效*pvParameter1，uint32_t
  * ulParameter2, TickType_t xTicksToWait );
  *
  *
  * 用于将函数的执行推迟到RTOS守护进程
  * task（定时器服务任务，因此该函数在
  * timers.c中实现，并以“Timer”为前缀）。
  *
  * @param xFunctionToPend 从定时器服务执行的函数/
  * 守护进程任务。  该功能必须符合PendedFunction_t
  * 原型。
  *
  * @param pvParameter1 回调函数第一个参数的值。
  * 该参数具有 void * 类型，允许它用于传递
  * 任何类型。例如，无符号长整型可以转换为
  * void *，或者 void * 可用于指向结构。
  *
  * @param ulParameter2 回调函数第二个参数的值。
  *
  * @param xTicksToWait 调用此函数将导致一条消息
  * 发送到队列上的计时器守护程序任务。
  * xTicksToWait 是调用任务应保留在阻塞
  * state 中的时间量（因此不使用任何处理时间
  * ），以便在发现队列已满时计时器队列上的空间变得可用。
  *
  * 如果消息发送成功，则返回 @return pdPASS
  * 定时器守护进程任务，否则返回 pdFALSE。
  *
  */
BaseType_t xTimerPendFunctionCall( PendedFunction_t xFunctionToPend, void *pvParameter1, uint32_t ulParameter2, TickType_t xTicksToWait ) PRIVILEGED_FUNCTION;

/**
 * 常量字符 * 常量 pcTimerGetName( TimerHandle_t xTimer );
 *
 * 返回创建计时器时分配给计时器的名称。
 *
 * @param xTimer 正在查询的定时器的句柄。
 *
 * @return 分配给 xTimer 参数指定的定时器的名称。
 */
const char * pcTimerGetName( TimerHandle_t xTimer ) PRIVILEGED_FUNCTION; /*lint !e971 仅字符串和单个字符允许使用非限定字符类型。 */

/**
 * 无效vTimerSetReloadMode（TimerHandle_t xTimer，常量UBaseType_t uxAutoReload）;
 *
 * 将计时器更新为自动重新加载计时器（在这种情况下，计
 * 时器每次到期时都会自动重置）或一次性计时器（在这种
 * 情况下，计时器只会到期一次，除非手动重新启动）。
 *
 * @param xTimer 正在更新的定时器的句柄。
 *
 * @param uxAutoReload 如果 uxAutoReload 设置为 pdTRUE，则定时器将
 * 以定时器的 period 设置的频率重复过期（请参阅
 * xTimerCreate() API 函数的
 * xTimerPeriodInTicks 参数）。  如果
 * uxAutoReload设置为pdFALSE，则定时器将是一次性定时器，并在超时后进入休眠状态。
 */
void vTimerSetReloadMode( TimerHandle_t xTimer, const UBaseType_t uxAutoReload ) PRIVILEGED_FUNCTION;

/**
* UBaseType_t uxTimerGetReloadMode( TimerHandle_t xTimer );
*
* 查询计时器以确定它是否是自动重载计时器（在这种情况下
* ，计时器每次到期时都会自动重置）还是一次性计时器（在
* 这种情况下，计时器只会到期一次，除非手动重新启动）。
*
* @param xTimer 正在查询的定时器的句柄。
*
* @return 如果定时器是自动重载定时器则返回 pdTRUE，否则返回
* 返回 pdFALSE。
*/
UBaseType_t uxTimerGetReloadMode( TimerHandle_t xTimer ) PRIVILEGED_FUNCTION;

/**
 * TickType_t xTimerGetPeriod( TimerHandle_t xTimer );
 *
 * 返回计时器的周期。
 *
 * @param xTimer 正在查询的定时器的句柄。
 *
 * @return 定时器的周期（以滴答为单位）。
 */
TickType_t xTimerGetPeriod( TimerHandle_t xTimer ) PRIVILEGED_FUNCTION;

/**
* TickType_t xTimerGetExpiryTime( TimerHandle_t xTimer );
*
* 返回计时器到期的时间（以刻度为单
* 位）。  如果该值小于当前滴答计
* 数，则到期时间已从当前时间溢出。
*
* @param xTimer 正在查询的定时器的句柄。
*
* @return 如果计时器正在运行，则计时器启动的时间（以滴答为单位）
* 下次过期会被退回。  如果计
* 时器未运行，则返回值未定义。
*/
TickType_t xTimerGetExpiryTime( TimerHandle_t xTimer ) PRIVILEGED_FUNCTION;

/*
 * 本部分之外的功能不属于公共 API 的一部分，旨在
 * for use by the kernel only.
 */
BaseType_t xTimerCreateTimerTask( void ) PRIVILEGED_FUNCTION;
BaseType_t xTimerGenericCommand( TimerHandle_t xTimer, const BaseType_t xCommandID, const TickType_t xOptionalValue, BaseType_t * const pxHigherPriorityTaskWoken, const TickType_t xTicksToWait ) PRIVILEGED_FUNCTION;

#if( configUSE_TRACE_FACILITY == 1 )
	void vTimerSetTimerNumber( TimerHandle_t xTimer, UBaseType_t uxTimerNumber ) PRIVILEGED_FUNCTION;
	UBaseType_t uxTimerGetTimerNumber( TimerHandle_t xTimer ) PRIVILEGED_FUNCTION;
#endif

#ifdef __cplusplus
}
#endif
#endif /* TIMERS_H */



