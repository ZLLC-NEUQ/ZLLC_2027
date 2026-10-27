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

/* 标准头文件。 */
#include <stdlib.h>

/* 定义
MPU_WRAPPERS_INCLUDED_FROM_API_FILE
可防止 task.h 重新定义所有 API 函数以使用 MPU 包装器。  仅当应用程序文件包含 task.h 时才应执行此操作。 */
#define MPU_WRAPPERS_INCLUDED_FROM_API_FILE

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"

#if ( INCLUDE_xTimerPendFunctionCall == 1 ) && ( configUSE_TIMERS == 0 )
	#error configUSE_TIMERS must be set to 1 to make the xTimerPendFunctionCall() function available.
#endif

/* Lint e9021、e961 和 e750 被抑制为 MISRA 异常，因为 MPU
端口需要定义 MPU_WRAPPERS_INCLUDED_FROM_API_FILE
为生成正确的特权与非特权链接和代码布局，上述头文件中需要定义该宏，
正确的特权与非特权链接和放置。 */
#undef MPU_WRAPPERS_INCLUDED_FROM_API_FILE /*lint !e9021 !e961 !e750。 */


/* 如果应用程序未配置为包含软件计时器功能，则将跳过整个源文
件。  该#if 在此文件的最底部关闭。  如果要包含软
件定时器功能，请确保 FreeRTOSConfig.h
中的 configUSE_TIMERS 设置为 1。 */
#if ( configUSE_TIMERS == 1 )

/* 杂项定义。 */
#define tmrNO_DELAY		( TickType_t ) 0U

/* 分配给定时器服务任务的名称。  这可以通过在
FreeRTOSConfig.h 中定义 trmTIMER_SERVICE_TASK_NAME 来覆盖。 */
#ifndef configTIMER_SERVICE_TASK_NAME
	#define configTIMER_SERVICE_TASK_NAME "Tmr Svc"
#endif

/* 定时器结构的 ucStatus 成员中使用的位定义。 */
#define tmrSTATUS_IS_ACTIVE					( ( uint8_t ) 0x01 )
#define tmrSTATUS_IS_STATICALLY_ALLOCATED	( ( uint8_t ) 0x02 )
#define tmrSTATUS_IS_AUTORELOAD				( ( uint8_t ) 0x04 )

/* 定时器本身的定义。 */
typedef struct tmrTimerControl /* 旧的命名约定用于防止破坏内核感知调试器。 */
{
	const char				*pcTimerName;		/*<< 文本名称。  内核不使用它，包含它只是为了使调试更容易。 */ /*lint !e971 仅字符串和单个字符允许使用非限定字符类型。 */
	ListItem_t				xTimerListItem;		/*<< 所有内核功能用于事件管理的标准链接列表项。 */
	TickType_t				xTimerPeriodInTicks;/*<< 计时器到期的速度和频率。 */
	void 					*pvTimerID;			/*<< 用于识别定时器的 ID。  当同一个回调用于多个计时器时，这允许识别计时器。 */
	TimerCallbackFunction_t	pxCallbackFunction;	/*<< 定时器到期时将调用的函数。 */
	#if( configUSE_TRACE_FACILITY == 1 )
		UBaseType_t			uxTimerNumber;		/*<< 由 FreeRTOS+Trace 等跟踪工具分配的 ID */
	#endif
	uint8_t 				ucStatus;			/*<< 保存表示计时器是否静态分配以及是否处于活动状态的位。 */
} xTIMER;

/* 旧的 xTIMER 名称保留在上面，然后键入定义为下面
的新 Timer_t 名称，以启用旧的内核感知调试器。 */
typedef xTIMER Timer_t;

/* 可以在计时器队列上发送和接收的消息的定义。可以对两种类
型的消息进行排队：操作软件计时器的消息和请求执行非计时
器相关回调的消息。  这两种消息类型分别定义在两个单独
的结构体中：xTimerParametersType
和 xCallbackParametersType。 */
typedef struct tmrTimerParameters
{
	TickType_t			xMessageValue;		/*<< 命令子集使用的可选值，例如，在更改计时器的周期时。 */
	Timer_t *			pxTimer;			/*<< 将应用命令的计时器。 */
} TimerParameter_t;


typedef struct tmrCallbackParameters
{
	PendedFunction_t	pxCallbackFunction;	/* << 要执行的回调函数。 */
	void *pvParameter1;						/* << 将用作回调函数第一个参数的值。 */
	uint32_t ulParameter2;					/* << 将用作回调函数第二个参数的值。 */
} CallbackParameters_t;

/* 该结构包含两种消息类型以及用于
确定哪种消息类型有效的标识符。 */
typedef struct tmrTimerQueueMessage
{
	BaseType_t			xMessageID;			/*<< 发送到定时器服务任务的命令。 */
	union
	{
		TimerParameter_t xTimerParameters;

		/* 如果不打算使用 xCallbackParameters，则不要
		包含它，因为它会使 structure（以及计时器队列）更大。 */
		#if ( INCLUDE_xTimerPendFunctionCall == 1 )
			CallbackParameters_t xCallbackParameters;
		#endif /* INCLUDE_xTimerPendFunctionCall */
	} u;
} DaemonTaskMessage_t;

/*lint -save -e956 已使用手动分析和检
查来确定哪些静态变量必须声明为 volatile。 */

/* 存储活动计时器的列表。  计时器按到期时间顺序引
用，最近的到期时间位于列表的前面。  仅允许定时
器服务任务访问这些列表。
xActiveTimerList1 和
xActiveTimerList2 可能位于函数
范围内，但这会破坏某些内核感知调试器以及响应删除静态限定符的调试器。 */
PRIVILEGED_DATA static List_t xActiveTimerList1;
PRIVILEGED_DATA static List_t xActiveTimerList2;
PRIVILEGED_DATA static List_t *pxCurrentTimerList;
PRIVILEGED_DATA static List_t *pxOverflowTimerList;

/* 用于向定时器服务任务发送命令的队列。 */
PRIVILEGED_DATA static QueueHandle_t xTimerQueue = NULL;
PRIVILEGED_DATA static TaskHandle_t xTimerTaskHandle = NULL;

/*lint - 恢复 */

/*-----------------------------------------------------------*/

#if( configSUPPORT_STATIC_ALLOCATION == 1 )

	/* 如果支持静态分配，则应用程序必须
	提供以下回调函数 - 这使应用程
	序能够选择提供将由计时器任务用作
	任务堆栈和 TCB 的内存。 */
	extern void vApplicationGetTimerTaskMemory( StaticTask_t **ppxTimerTaskTCBBuffer, StackType_t **ppxTimerTaskStackBuffer, uint32_t *pulTimerTaskStackSize );

#endif

/*
 * 如果计时器服务任务尚未初始
 * 化，则初始化该基础设施。
 */
static void prvCheckForValidListAndQueue( void ) PRIVILEGED_FUNCTION;

/*
 * 定时器服务 task（守护进程）。  定时器
 * 功能由该任务控制。  其他任务使用
 * xTimerQueue 队列与定时器服务任务通信。
 */
static portTASK_FUNCTION_PROTO( prvTimerTask, pvParameters ) PRIVILEGED_FUNCTION;

/*
 * 由计时器服务任务调用，以解释和处
 * 理它在计时器队列上收到的命令。
 */
static void prvProcessReceivedCommands( void ) PRIVILEGED_FUNCTION;

/*
 * 将定时器插入 xActiveTimerList1 或
 * xActiveTimerList2，具体取决于到期时间是否导致定时器计数器溢出。
 */
static BaseType_t prvInsertTimerInActiveList( Timer_t * const pxTimer, const TickType_t xNextExpiryTime, const TickType_t xTimeNow, const TickType_t xCommandTime ) PRIVILEGED_FUNCTION;

/*
 * 活动计时器已达到其到期时间。  如果是自动重新
 * 加载定时器，则重新加载定时器，然后调用其回调。
 */
static void prvProcessExpiredTimer( const TickType_t xNextExpireTime, const TickType_t xTimeNow ) PRIVILEGED_FUNCTION;

/*
 * 滴答计数已溢出。  确保当前定时器列表
 * 不再引用某些定时器后，切换定时器列表。
 */
static void prvSwitchTimerLists( void ) PRIVILEGED_FUNCTION;

/*
 * 获取当前tick计数，将*pxTimerListsWereSwitched设置为pdTRUE
 * if a tick count overflow occurred since prvSampleTimeNow() was last called.
 */
static TickType_t prvSampleTimeNow( BaseType_t * const pxTimerListsWereSwitched ) PRIVILEGED_FUNCTION;

/*
 * 如果定时器列表包含任何活动定时器，则返回最先到期的定时器
 * 的到期时间，并将 *pxListWasEmpty 设置为
 * false。  如果定时器列表不包含任何定时器，则返回
 * 0并将*pxListWasEmpty设置为pdTRUE。
 */
static TickType_t prvGetNextExpireTime( BaseType_t * const pxListWasEmpty ) PRIVILEGED_FUNCTION;

/*
 * 如果计时器已过期，则对其进行处理。  否则，阻
 * 止计时器服务任务，直到计时器到期或收到命令。
 */
static void prvProcessTimerOrBlockTask( const TickType_t xNextExpireTime, BaseType_t xListWasEmpty ) PRIVILEGED_FUNCTION;

/*
 * 在静态或动态分配 Timer_t
 * 结构以填充结构的成员后调用。
 */
static void prvInitialiseNewTimer(	const char * const pcTimerName,			/*lint !e971 仅字符串和单个字符允许使用非限定字符类型。 */
									const TickType_t xTimerPeriodInTicks,
									const UBaseType_t uxAutoReload,
									void * const pvTimerID,
									TimerCallbackFunction_t pxCallbackFunction,
									Timer_t *pxNewTimer ) PRIVILEGED_FUNCTION;
/*-----------------------------------------------------------*/

BaseType_t xTimerCreateTimerTask( void )
{
BaseType_t xReturn = pdFAIL;

	/* 如果 configUSE_TIMERS 设置为
	1，则在启动调度程序时调用此函数。检查计时器
	服务任务使用的基础设施是否已创建/初始化。
	如果定时器已经创建，那么初始化就已经执行了。 */
	prvCheckForValidListAndQueue();

	if( xTimerQueue != NULL )
	{
		#if( configSUPPORT_STATIC_ALLOCATION == 1 )
		{
			StaticTask_t *pxTimerTaskTCBBuffer = NULL;
			StackType_t *pxTimerTaskStackBuffer = NULL;
			uint32_t ulTimerTaskStackSize;

			vApplicationGetTimerTaskMemory( &pxTimerTaskTCBBuffer, &pxTimerTaskStackBuffer, &ulTimerTaskStackSize );
			xTimerTaskHandle = xTaskCreateStatic(	prvTimerTask,
													configTIMER_SERVICE_TASK_NAME,
													ulTimerTaskStackSize,
													NULL,
													( ( UBaseType_t ) configTIMER_TASK_PRIORITY ) | portPRIVILEGE_BIT,
													pxTimerTaskStackBuffer,
													pxTimerTaskTCBBuffer );

			if( xTimerTaskHandle != NULL )
			{
				xReturn = pdPASS;
			}
		}
		#else
		{
			xReturn = xTaskCreate(	prvTimerTask,
									configTIMER_SERVICE_TASK_NAME,
									configTIMER_TASK_STACK_DEPTH,
									NULL,
									( ( UBaseType_t ) configTIMER_TASK_PRIORITY ) | portPRIVILEGE_BIT,
									&xTimerTaskHandle );
		}
		#endif /* configSUPPORT_STATIC_ALLOCATION */
	}
	else
	{
		mtCOVERAGE_TEST_MARKER();
	}

	configASSERT( xReturn );
	return xReturn;
}
/*-----------------------------------------------------------*/

#if( configSUPPORT_DYNAMIC_ALLOCATION == 1 )

	TimerHandle_t xTimerCreate(	const char * const pcTimerName,			/*lint !e971 仅字符串和单个字符允许使用非限定字符类型。 */
								const TickType_t xTimerPeriodInTicks,
								const UBaseType_t uxAutoReload,
								void * const pvTimerID,
								TimerCallbackFunction_t pxCallbackFunction )
	{
	Timer_t *pxNewTimer;

		pxNewTimer = ( Timer_t * ) pvPortMalloc( sizeof( Timer_t ) ); /*lint !e9087 !e9079 pvPortMalloc() 返回的所有值至少具有 MCU 堆栈所需的对齐方式，并且 Timer_t 的第一个成员始终是指向计时器的指针。 */

		if( pxNewTimer != NULL )
		{
			/* 到目前为止，状态为零，因为计时器不是静态创建的
			并且尚未启动。  自动重新加载位可能会被设置
			prvInitialiseNewTimer. */
			pxNewTimer->ucStatus = 0x00;
			prvInitialiseNewTimer( pcTimerName, xTimerPeriodInTicks, uxAutoReload, pvTimerID, pxCallbackFunction, pxNewTimer );
		}

		return pxNewTimer;
	}

#endif /* configSUPPORT_DYNAMIC_ALLOCATION */
/*-----------------------------------------------------------*/

#if( configSUPPORT_STATIC_ALLOCATION == 1 )

	TimerHandle_t xTimerCreateStatic(	const char * const pcTimerName,		/*lint !e971 仅字符串和单个字符允许使用非限定字符类型。 */
										const TickType_t xTimerPeriodInTicks,
										const UBaseType_t uxAutoReload,
										void * const pvTimerID,
										TimerCallbackFunction_t pxCallbackFunction,
										StaticTimer_t *pxTimerBuffer )
	{
	Timer_t *pxNewTimer;

		#if( configASSERT_DEFINED == 1 )
		{
			/* 健全性检查用于声明 StaticTimer_t
			类型变量的结构的大小是否等于实时计时器的大小
			结构。 */
			volatile size_t xSize = sizeof( StaticTimer_t );
			configASSERT( xSize == sizeof( Timer_t ) );
			( void ) xSize; /* 当 configASSERT() 未定义时，保持 lint 安静。 */
		}
		#endif /* configASSERT_DEFINED */

		/* 提供了指向 StaticTimer_t 结构 MUST 的指针，请使用它。 */
		configASSERT( pxTimerBuffer );
		pxNewTimer = ( Timer_t * ) pxTimerBuffer; /*lint !e740 !e9087 StaticTimer_t 是指向 Timer_t 的指针，因此保证 correctly 对齐并调整大小（由 assert() 检查），因此这是安全的。 */

		if( pxNewTimer != NULL )
		{
			/* 计时器可以静态或动态创建，因此请注意，此计时器是静态
			创建的，以防以后被删除。  自动重载位可以在
			prvInitialiseNewTimer() 中设置。 */
			pxNewTimer->ucStatus = tmrSTATUS_IS_STATICALLY_ALLOCATED;

			prvInitialiseNewTimer( pcTimerName, xTimerPeriodInTicks, uxAutoReload, pvTimerID, pxCallbackFunction, pxNewTimer );
		}

		return pxNewTimer;
	}

#endif /* configSUPPORT_STATIC_ALLOCATION */
/*-----------------------------------------------------------*/

static void prvInitialiseNewTimer(	const char * const pcTimerName,			/*lint !e971 仅字符串和单个字符允许使用非限定字符类型。 */
									const TickType_t xTimerPeriodInTicks,
									const UBaseType_t uxAutoReload,
									void * const pvTimerID,
									TimerCallbackFunction_t pxCallbackFunction,
									Timer_t *pxNewTimer )
{
	/* 0 不是 xTimerPeriodInTicks 的有效值。 */
	configASSERT( ( xTimerPeriodInTicks > 0 ) );

	if( pxNewTimer != NULL )
	{
		/* 确保定时器服务任务使用的基础设施已
		创建/初始化。 */
		prvCheckForValidListAndQueue();

		/* 使用函数初始化计时器结构体成员
		参数。 */
		pxNewTimer->pcTimerName = pcTimerName;
		pxNewTimer->xTimerPeriodInTicks = xTimerPeriodInTicks;
		pxNewTimer->pvTimerID = pvTimerID;
		pxNewTimer->pxCallbackFunction = pxCallbackFunction;
		vListInitialiseItem( &( pxNewTimer->xTimerListItem ) );
		if( uxAutoReload != pdFALSE )
		{
			pxNewTimer->ucStatus |= tmrSTATUS_IS_AUTORELOAD;
		}
		traceTIMER_CREATE( pxNewTimer );
	}
}
/*-----------------------------------------------------------*/

BaseType_t xTimerGenericCommand( TimerHandle_t xTimer, const BaseType_t xCommandID, const TickType_t xOptionalValue, BaseType_t * const pxHigherPriorityTaskWoken, const TickType_t xTicksToWait )
{
BaseType_t xReturn = pdFAIL;
DaemonTaskMessage_t xMessage;

	configASSERT( xTimer );

	/* 向定时器服务任务发送消息以对
	特定定时器定义执行特定操作。 */
	if( xTimerQueue != NULL )
	{
		/* 向定时器服务任务发送命令以启动xTimer定时器。 */
		xMessage.xMessageID = xCommandID;
		xMessage.u.xTimerParameters.xMessageValue = xOptionalValue;
		xMessage.u.xTimerParameters.pxTimer = xTimer;

		if( xCommandID < tmrFIRST_FROM_ISR_COMMAND )
		{
			if( xTaskGetSchedulerState() == taskSCHEDULER_RUNNING )
			{
				xReturn = xQueueSendToBack( xTimerQueue, &xMessage, xTicksToWait );
			}
			else
			{
				xReturn = xQueueSendToBack( xTimerQueue, &xMessage, tmrNO_DELAY );
			}
		}
		else
		{
			xReturn = xQueueSendToBackFromISR( xTimerQueue, &xMessage, pxHigherPriorityTaskWoken );
		}

		traceTIMER_COMMAND_SEND( xTimer, xCommandID, xOptionalValue, xReturn );
	}
	else
	{
		mtCOVERAGE_TEST_MARKER();
	}

	return xReturn;
}
/*-----------------------------------------------------------*/

TaskHandle_t xTimerGetTimerDaemonTaskHandle( void )
{
	/* 如果在调度程序启动之前调用
	xTimerGetTimerDaemonTaskHandle()，则 xTimerTaskHandle 将是 NULL。 */
	configASSERT( ( xTimerTaskHandle != NULL ) );
	return xTimerTaskHandle;
}
/*-----------------------------------------------------------*/

TickType_t xTimerGetPeriod( TimerHandle_t xTimer )
{
Timer_t *pxTimer = xTimer;

	configASSERT( xTimer );
	return pxTimer->xTimerPeriodInTicks;
}
/*-----------------------------------------------------------*/

void vTimerSetReloadMode( TimerHandle_t xTimer, const UBaseType_t uxAutoReload )
{
Timer_t * pxTimer =  xTimer;

	configASSERT( xTimer );
	taskENTER_CRITICAL();
	{
		if( uxAutoReload != pdFALSE )
		{
			pxTimer->ucStatus |= tmrSTATUS_IS_AUTORELOAD;
		}
		else
		{
			pxTimer->ucStatus &= ~tmrSTATUS_IS_AUTORELOAD;
		}
	}
	taskEXIT_CRITICAL();
}
/*-----------------------------------------------------------*/

UBaseType_t uxTimerGetReloadMode( TimerHandle_t xTimer )
{
Timer_t * pxTimer =  xTimer;
UBaseType_t uxReturn;

	configASSERT( xTimer );
	taskENTER_CRITICAL();
	{
		if( ( pxTimer->ucStatus & tmrSTATUS_IS_AUTORELOAD ) == 0 )
		{
			/* 不是自动重新加载计时器。 */
			uxReturn = ( UBaseType_t ) pdFALSE;
		}
		else
		{
			/* 是一个自动重新加载计时器。 */
			uxReturn = ( UBaseType_t ) pdTRUE;
		}
	}
	taskEXIT_CRITICAL();

	return uxReturn;
}
/*-----------------------------------------------------------*/

TickType_t xTimerGetExpiryTime( TimerHandle_t xTimer )
{
Timer_t * pxTimer =  xTimer;
TickType_t xReturn;

	configASSERT( xTimer );
	xReturn = listGET_LIST_ITEM_VALUE( &( pxTimer->xTimerListItem ) );
	return xReturn;
}
/*-----------------------------------------------------------*/

const char * pcTimerGetName( TimerHandle_t xTimer ) /*lint !e971 仅字符串和单个字符允许使用非限定字符类型。 */
{
Timer_t *pxTimer = xTimer;

	configASSERT( xTimer );
	return pxTimer->pcTimerName;
}
/*-----------------------------------------------------------*/

static void prvProcessExpiredTimer( const TickType_t xNextExpireTime, const TickType_t xTimeNow )
{
BaseType_t xResult;
Timer_t * const pxTimer = ( Timer_t * ) listGET_OWNER_OF_HEAD_ENTRY( pxCurrentTimerList ); /*使用 lint !e9087 !e9079 void * 因为该宏也与任务一起使用，也与 co-routines 一起使用。  众所周知，对齐是很好的，因为存储和检索的指针类型是相同的。 */

	/* 从活动计时器列表中删除计时器。
	已执行检查以确保列表不为空。 */
	( void ) uxListRemove( &( pxTimer->xTimerListItem ) );
	traceTIMER_EXPIRED( pxTimer );

	/* 如果计时器是自动重新加载计时器，则计算下一个
	到期时间并将计时器重新插入活动计时器列表中。 */
	if( ( pxTimer->ucStatus & tmrSTATUS_IS_AUTORELOAD ) != 0 )
	{
		/* 使用相对于当前时间以外的任何时间的时间将计
		时器插入到列表中。  因此，它将被插入到相
		对于该任务认为现在的时间的正确列表中。 */
		if( prvInsertTimerInActiveList( pxTimer, ( xNextExpireTime + pxTimer->xTimerPeriodInTicks ), xTimeNow, xNextExpireTime ) != pdFALSE )
		{
			/* 计时器在添加到活动计时器列表之
			前就已过期。  立即重新加载。  */
			xResult = xTimerGenericCommand( pxTimer, tmrCOMMAND_START_DONT_TRACE, xNextExpireTime, NULL, tmrNO_DELAY );
			configASSERT( xResult );
			( void ) xResult;
		}
		else
		{
			mtCOVERAGE_TEST_MARKER();
		}
	}
	else
	{
		pxTimer->ucStatus &= ~tmrSTATUS_IS_ACTIVE;
		mtCOVERAGE_TEST_MARKER();
	}

	/* 调用定时器回调。 */
	pxTimer->pxCallbackFunction( ( TimerHandle_t ) pxTimer );
}
/*-----------------------------------------------------------*/

static portTASK_FUNCTION( prvTimerTask, pvParameters )
{
TickType_t xNextExpireTime;
BaseType_t xListWasEmpty;

	/* 只是为了避免编译器警告。 */
	( void ) pvParameters;

	#if( configUSE_DAEMON_TASK_STARTUP_HOOK == 1 )
	{
		extern void vApplicationDaemonTaskStartupHook( void );

		/* 允许应用程序编写者在任务开始执行时在此
		任务的上下文中执行一些代码。  如果应
		用程序包含初始化代码，并且在调度程序启
		动后执行该代码会受益，那么这非常有用。 */
		vApplicationDaemonTaskStartupHook();
	}
	#endif /* configUSE_DAEMON_TASK_STARTUP_HOOK */

	for( ;; )
	{
		/* 查询定时器列表中是否有定时器，如果
		有，则获取下一个定时器到期的时间。 */
		xNextExpireTime = prvGetNextExpireTime( &xListWasEmpty );

		/* 如果计时器已过期，则对其进行处理。  否则
		，阻止此任务，直到计时器到期或收到命令。 */
		prvProcessTimerOrBlockTask( xNextExpireTime, xListWasEmpty );

		/* 清空命令队列。 */
		prvProcessReceivedCommands();
	}
}
/*-----------------------------------------------------------*/

static void prvProcessTimerOrBlockTask( const TickType_t xNextExpireTime, BaseType_t xListWasEmpty )
{
TickType_t xTimeNow;
BaseType_t xTimerListsWereSwitched;

	vTaskSuspendAll();
	{
		/* 现在获取时间来评估计时器是否已过期。
		如果获取时间导致列表切换，则不要处
		理此计时器，因为列表切换时保留在列表中
		的任何计时器都将在
		prvSampleTimeNow() 函数内进行处理。 */
		xTimeNow = prvSampleTimeNow( &xTimerListsWereSwitched );
		if( xTimerListsWereSwitched == pdFALSE )
		{
			/* 滴答计数尚未溢出，计时器是否已过期？ */
			if( ( xListWasEmpty == pdFALSE ) && ( xNextExpireTime <= xTimeNow ) )
			{
				( void ) xTaskResumeAll();
				prvProcessExpiredTimer( xNextExpireTime, xTimeNow );
			}
			else
			{
				/* 滴答计数尚未溢出，并且尚未到达下一个到期
				时间。  因此，该任务应该阻塞以等待下一
				个到期时间或接收命令 - 以先到者为准。
				除非 xNextExpireTime
				> xTimeNow，否则无法到达以下
				行，当前定时器列表为空的情况除外。 */
				if( xListWasEmpty != pdFALSE )
				{
					/* 当前计时器列表为空 -
					溢出列表也为空吗？ */
					xListWasEmpty = listLIST_IS_EMPTY( pxOverflowTimerList );
				}

				vQueueWaitForMessageRestricted( xTimerQueue, ( xNextExpireTime - xTimeNow ), xListWasEmpty );

				if( xTaskResumeAll() == pdFALSE )
				{
					/* 等待命令到达或块时间到期
					。  如果命令在退出临界
					区和此让出之间到达，则让
					出不会导致任务阻塞。 */
					portYIELD_WITHIN_API();
				}
				else
				{
					mtCOVERAGE_TEST_MARKER();
				}
			}
		}
		else
		{
			( void ) xTaskResumeAll();
		}
	}
}
/*-----------------------------------------------------------*/

static TickType_t prvGetNextExpireTime( BaseType_t * const pxListWasEmpty )
{
TickType_t xNextExpireTime;

	/* 计时器按到期时间顺序列出，列表的头部引用首
	先到期的任务。  获取最接近到期​​时间的
	计时器到期的时间。  如果没有活动的计时器
	，则只需将下一个到期时间设置为 0。这将导
	致该任务在滴答计数溢出时解除阻塞，此时计时
	器列表将被切换，并且下一个到期时间可以是
	重新评估。  */
	*pxListWasEmpty = listLIST_IS_EMPTY( pxCurrentTimerList );
	if( *pxListWasEmpty == pdFALSE )
	{
		xNextExpireTime = listGET_ITEM_VALUE_OF_HEAD_ENTRY( pxCurrentTimerList );
	}
	else
	{
		/* 确保当滴答计数翻转时任务解除阻塞。 */
		xNextExpireTime = ( TickType_t ) 0U;
	}

	return xNextExpireTime;
}
/*-----------------------------------------------------------*/

static TickType_t prvSampleTimeNow( BaseType_t * const pxTimerListsWereSwitched )
{
TickType_t xTimeNow;
PRIVILEGED_DATA static TickType_t xLastTime = ( TickType_t ) 0U; /*lint !e956 变量只能由一项任务访问。 */

	xTimeNow = xTaskGetTickCount();

	if( xTimeNow < xLastTime )
	{
		prvSwitchTimerLists();
		*pxTimerListsWereSwitched = pdTRUE;
	}
	else
	{
		*pxTimerListsWereSwitched = pdFALSE;
	}

	xLastTime = xTimeNow;

	return xTimeNow;
}
/*-----------------------------------------------------------*/

static BaseType_t prvInsertTimerInActiveList( Timer_t * const pxTimer, const TickType_t xNextExpiryTime, const TickType_t xTimeNow, const TickType_t xCommandTime )
{
BaseType_t xProcessTimerNow = pdFALSE;

	listSET_LIST_ITEM_VALUE( &( pxTimer->xTimerListItem ), xNextExpiryTime );
	listSET_LIST_ITEM_OWNER( &( pxTimer->xTimerListItem ), pxTimer );

	if( xNextExpiryTime <= xTimeNow )
	{
		/* 发出启动/重置计时器的命令与处理命
		令的时间之间是否经过了到期时间？ */
		if( ( ( TickType_t ) ( xTimeNow - xCommandTime ) ) >= pxTimer->xTimerPeriodInTicks ) /*lint !e961 MISRA 例外，因为强制转换仅对某些端口是多余的。 */
		{
			/* 发出命令和处理命令之间的时
			间实际上超过了计时器周期。  */
			xProcessTimerNow = pdTRUE;
		}
		else
		{
			vListInsert( pxOverflowTimerList, &( pxTimer->xTimerListItem ) );
		}
	}
	else
	{
		if( ( xTimeNow < xCommandTime ) && ( xNextExpiryTime >= xCommandTime ) )
		{
			/* 如果自发出命令以来，滴答计数已溢出
			，但到期时间尚未溢出，则计时器必定
			已超过其到期时间，应立即处理。 */
			xProcessTimerNow = pdTRUE;
		}
		else
		{
			vListInsert( pxCurrentTimerList, &( pxTimer->xTimerListItem ) );
		}
	}

	return xProcessTimerNow;
}
/*-----------------------------------------------------------*/

static void	prvProcessReceivedCommands( void )
{
DaemonTaskMessage_t xMessage;
Timer_t *pxTimer;
BaseType_t xTimerListsWereSwitched, xResult;
TickType_t xTimeNow;

	while( xQueueReceive( xTimerQueue, &xMessage, tmrNO_DELAY ) != pdFAIL ) /*lint !e603 xMessage 不必初始化，因为它是传出而不是传入，并且除非 xQueueReceive() 返回 pdTRUE，否则不会使用它。 */
	{
		#if ( INCLUDE_xTimerPendFunctionCall == 1 )
		{
			/* 负命令是挂起的函数调用而不是计时器
			命令。 */
			if( xMessage.xMessageID < ( BaseType_t ) 0 )
			{
				const CallbackParameters_t * const pxCallback = &( xMessage.u.xCallbackParameters );

				/* 计时器使用 xCallbackParameters
				成员来请求执行回调。  检查回调不是NULL。 */
				configASSERT( pxCallback );

				/* 调用该函数。 */
				pxCallback->pxCallbackFunction( pxCallback->pvParameter1, pxCallback->ulParameter2 );
			}
			else
			{
				mtCOVERAGE_TEST_MARKER();
			}
		}
		#endif /* INCLUDE_xTimerPendFunctionCall */

		/* 正的命令是定时器命令而
		不是挂起的函数调用。 */
		if( xMessage.xMessageID >= ( BaseType_t ) 0 )
		{
			/* 这些消息使用
			xTimerParameters 成员来处理软件定时器。 */
			pxTimer = xMessage.u.xTimerParameters.pxTimer;

			if( listIS_CONTAINED_WITHIN( NULL, &( pxTimer->xTimerListItem ) ) == pdFALSE ) /*lint！e961。仅当 NULL 传递到宏中时，转换才是多余的。 */
			{
				/* 计时器在列表中，将其删除。 */
				( void ) uxListRemove( &( pxTimer->xTimerListItem ) );
			}
			else
			{
				mtCOVERAGE_TEST_MARKER();
			}

			traceTIMER_COMMAND_RECEIVED( pxTimer, xMessage.xMessageID, xMessage.u.xTimerParameters.xMessageValue );

			/* 在这种情况下，不使用
			xTimerListsWereSwitched 参数，但它
			必须出现在函数调用中。  从 xTimerQueue 收到
			消息后必须调用 prvSampleTimeNow()，因此
			更高优先级的任务不可能将消息添加到消息队列中，其时间早于计
			时器守护程序 task（因为它在设置 xTimeNow 值后抢占了计时器守护程序任务）。 */
			xTimeNow = prvSampleTimeNow( &xTimerListsWereSwitched );

			switch( xMessage.xMessageID )
			{
				case tmrCOMMAND_START :
				case tmrCOMMAND_START_FROM_ISR :
				case tmrCOMMAND_RESET :
				case tmrCOMMAND_RESET_FROM_ISR :
				case tmrCOMMAND_START_DONT_TRACE :
					/* 启动或重新启动计时器。 */
					pxTimer->ucStatus |= tmrSTATUS_IS_ACTIVE;
					if( prvInsertTimerInActiveList( pxTimer,  xMessage.u.xTimerParameters.xMessageValue + pxTimer->xTimerPeriodInTicks, xTimeNow, xMessage.u.xTimerParameters.xMessageValue ) != pdFALSE )
					{
						/* 计时器在添加到活动计时器列表之
						前就已过期。  现在就处理吧。 */
						pxTimer->pxCallbackFunction( ( TimerHandle_t ) pxTimer );
						traceTIMER_EXPIRED( pxTimer );

						if( ( pxTimer->ucStatus & tmrSTATUS_IS_AUTORELOAD ) != 0 )
						{
							xResult = xTimerGenericCommand( pxTimer, tmrCOMMAND_START_DONT_TRACE, xMessage.u.xTimerParameters.xMessageValue + pxTimer->xTimerPeriodInTicks, NULL, tmrNO_DELAY );
							configASSERT( xResult );
							( void ) xResult;
						}
						else
						{
							mtCOVERAGE_TEST_MARKER();
						}
					}
					else
					{
						mtCOVERAGE_TEST_MARKER();
					}
					break;

				case tmrCOMMAND_STOP :
				case tmrCOMMAND_STOP_FROM_ISR :
					/* 该计时器已从活动列表中删除。 */
					pxTimer->ucStatus &= ~tmrSTATUS_IS_ACTIVE;
					break;

				case tmrCOMMAND_CHANGE_PERIOD :
				case tmrCOMMAND_CHANGE_PERIOD_FROM_ISR :
					pxTimer->ucStatus |= tmrSTATUS_IS_ACTIVE;
					pxTimer->xTimerPeriodInTicks = xMessage.u.xTimerParameters.xMessageValue;
					configASSERT( ( pxTimer->xTimerPeriodInTicks > 0 ) );

					/* 新的时期并没有真正的参考，可以比旧的更
					长或更短。  因此，命令时间设置为当前
					时间，并且由于周期不能为零，因此下一个
					到期时间只能是未来的 meaning（
					与上面的 xTimerStart()
					情况不同），这里不需要处理失败情况。 */
					( void ) prvInsertTimerInActiveList( pxTimer, ( xTimeNow + pxTimer->xTimerPeriodInTicks ), xTimeNow, xTimeNow );
					break;

				case tmrCOMMAND_DELETE :
					#if ( configSUPPORT_DYNAMIC_ALLOCATION == 1 )
					{
						/* 定时器已经从活动列表中删除，如果
						内存是动态的，只需释放内存即可
						分配。 */
						if( ( pxTimer->ucStatus & tmrSTATUS_IS_STATICALLY_ALLOCATED ) == ( uint8_t ) 0 )
						{
							vPortFree( pxTimer );
						}
						else
						{
							pxTimer->ucStatus &= ~tmrSTATUS_IS_ACTIVE;
						}
					}
					#else
					{
						/* 如果未启用动态分配，则无
						法动态分配内存。因此无需
						释放内存 - 只需将计时
						器标记为“不活动”即可。 */
						pxTimer->ucStatus &= ~tmrSTATUS_IS_ACTIVE;
					}
					#endif /* configSUPPORT_DYNAMIC_ALLOCATION */
					break;

				default	:
					/* 别指望能到达这里。 */
					break;
			}
		}
	}
}
/*-----------------------------------------------------------*/

static void prvSwitchTimerLists( void )
{
TickType_t xNextExpireTime, xReloadTime;
List_t *pxTemp;
Timer_t *pxTimer;
BaseType_t xResult;

	/* 滴答计数已溢出。  必须切换定时器
	列表。如果当前计时器列表中仍有任何
	计时器被引用，那么它们一定已经过期
	，并且应该在列表切换之前进行处理。 */
	while( listLIST_IS_EMPTY( pxCurrentTimerList ) == pdFALSE )
	{
		xNextExpireTime = listGET_ITEM_VALUE_OF_HEAD_ENTRY( pxCurrentTimerList );

		/* 从列表中删除计时器。 */
		pxTimer = ( Timer_t * ) listGET_OWNER_OF_HEAD_ENTRY( pxCurrentTimerList ); /*使用 lint !e9087 !e9079 void * 因为该宏也与任务一起使用，也与 co-routines 一起使用。  众所周知，对齐是很好的，因为存储和检索的指针类型是相同的。 */
		( void ) uxListRemove( &( pxTimer->xTimerListItem ) );
		traceTIMER_EXPIRED( pxTimer );

		/* 执行其回调，然后发送命令以重新启动计时器
		（如果它是自动重新加载计时器）。  由于
		列表尚未切换，因此无法在此处重新启动。 */
		pxTimer->pxCallbackFunction( ( TimerHandle_t ) pxTimer );

		if( ( pxTimer->ucStatus & tmrSTATUS_IS_AUTORELOAD ) != 0 )
		{
			/* 计算重新加载值，如果重新加载值导致计
			时器进入同一个计时器列表，则它已经过
			期，并且应该将计时器重新插入到当前列
			表中，以便在该循环中再次处理它。
			否则，应发送命令来重新启动计时器，以
			确保它仅在列表交换后才插入到列表中。 */
			xReloadTime = ( xNextExpireTime + pxTimer->xTimerPeriodInTicks );
			if( xReloadTime > xNextExpireTime )
			{
				listSET_LIST_ITEM_VALUE( &( pxTimer->xTimerListItem ), xReloadTime );
				listSET_LIST_ITEM_OWNER( &( pxTimer->xTimerListItem ), pxTimer );
				vListInsert( pxCurrentTimerList, &( pxTimer->xTimerListItem ) );
			}
			else
			{
				xResult = xTimerGenericCommand( pxTimer, tmrCOMMAND_START_DONT_TRACE, xNextExpireTime, NULL, tmrNO_DELAY );
				configASSERT( xResult );
				( void ) xResult;
			}
		}
		else
		{
			mtCOVERAGE_TEST_MARKER();
		}
	}

	pxTemp = pxCurrentTimerList;
	pxCurrentTimerList = pxOverflowTimerList;
	pxOverflowTimerList = pxTemp;
}
/*-----------------------------------------------------------*/

static void prvCheckForValidListAndQueue( void )
{
	/* 检查引用活动定时器的列表以及用
	于与定时器服务通信的队列是否已
	已初始化。 */
	taskENTER_CRITICAL();
	{
		if( xTimerQueue == NULL )
		{
			vListInitialise( &xActiveTimerList1 );
			vListInitialise( &xActiveTimerList2 );
			pxCurrentTimerList = &xActiveTimerList1;
			pxOverflowTimerList = &xActiveTimerList2;

			#if( configSUPPORT_STATIC_ALLOCATION == 1 )
			{
				/* 如果
				configSUPPORT_DYNAMIC_ALLOCATION 为 0，则静态分配定时器队列。 */
				static StaticQueue_t xStaticTimerQueue; /*lint !e956 可以以这种方式声明，以防止其他位置出现额外的条件编译防护。 */
				static uint8_t ucStaticTimerQueueStorage[ ( size_t ) configTIMER_QUEUE_LENGTH * sizeof( DaemonTaskMessage_t ) ]; /*lint !e956 可以以这种方式声明，以防止其他位置出现额外的条件编译防护。 */

				xTimerQueue = xQueueCreateStatic( ( UBaseType_t ) configTIMER_QUEUE_LENGTH, ( UBaseType_t ) sizeof( DaemonTaskMessage_t ), &( ucStaticTimerQueueStorage[ 0 ] ), &xStaticTimerQueue );
			}
			#else
			{
				xTimerQueue = xQueueCreate( ( UBaseType_t ) configTIMER_QUEUE_LENGTH, sizeof( DaemonTaskMessage_t ) );
			}
			#endif

			#if ( configQUEUE_REGISTRY_SIZE > 0 )
			{
				if( xTimerQueue != NULL )
				{
					vQueueAddToRegistry( xTimerQueue, "TmrQ" );
				}
				else
				{
					mtCOVERAGE_TEST_MARKER();
				}
			}
			#endif /* configQUEUE_REGISTRY_SIZE */
		}
		else
		{
			mtCOVERAGE_TEST_MARKER();
		}
	}
	taskEXIT_CRITICAL();
}
/*-----------------------------------------------------------*/

BaseType_t xTimerIsTimerActive( TimerHandle_t xTimer )
{
BaseType_t xReturn;
Timer_t *pxTimer = xTimer;

	configASSERT( xTimer );

	/* 该计时器是否在活动计时器列表中？ */
	taskENTER_CRITICAL();
	{
		if( ( pxTimer->ucStatus & tmrSTATUS_IS_ACTIVE ) == 0 )
		{
			xReturn = pdFALSE;
		}
		else
		{
			xReturn = pdTRUE;
		}
	}
	taskEXIT_CRITICAL();

	return xReturn;
} /*lint !e818 由于 typedef 的原因，无法成为指向 const 的指针。 */
/*-----------------------------------------------------------*/

void *pvTimerGetTimerID( const TimerHandle_t xTimer )
{
Timer_t * const pxTimer = xTimer;
void *pvReturn;

	configASSERT( xTimer );

	taskENTER_CRITICAL();
	{
		pvReturn = pxTimer->pvTimerID;
	}
	taskEXIT_CRITICAL();

	return pvReturn;
}
/*-----------------------------------------------------------*/

void vTimerSetTimerID( TimerHandle_t xTimer, void *pvNewID )
{
Timer_t * const pxTimer = xTimer;

	configASSERT( xTimer );

	taskENTER_CRITICAL();
	{
		pxTimer->pvTimerID = pvNewID;
	}
	taskEXIT_CRITICAL();
}
/*-----------------------------------------------------------*/

#if( INCLUDE_xTimerPendFunctionCall == 1 )

	BaseType_t xTimerPendFunctionCallFromISR( PendedFunction_t xFunctionToPend, void *pvParameter1, uint32_t ulParameter2, BaseType_t *pxHigherPriorityTaskWoken )
	{
	DaemonTaskMessage_t xMessage;
	BaseType_t xReturn;

		/* 使用函数参数完成消息并将
		其发布到守护程序任务。 */
		xMessage.xMessageID = tmrCOMMAND_EXECUTE_CALLBACK_FROM_ISR;
		xMessage.u.xCallbackParameters.pxCallbackFunction = xFunctionToPend;
		xMessage.u.xCallbackParameters.pvParameter1 = pvParameter1;
		xMessage.u.xCallbackParameters.ulParameter2 = ulParameter2;

		xReturn = xQueueSendFromISR( xTimerQueue, &xMessage, pxHigherPriorityTaskWoken );

		tracePEND_FUNC_CALL_FROM_ISR( xFunctionToPend, pvParameter1, ulParameter2, xReturn );

		return xReturn;
	}

#endif /* INCLUDE_xTimerPendFunctionCall */
/*-----------------------------------------------------------*/

#if( INCLUDE_xTimerPendFunctionCall == 1 )

	BaseType_t xTimerPendFunctionCall( PendedFunction_t xFunctionToPend, void *pvParameter1, uint32_t ulParameter2, TickType_t xTicksToWait )
	{
	DaemonTaskMessage_t xMessage;
	BaseType_t xReturn;

		/* 该函数只能在创建计时器或启
		动调度程序后调用，因为在此
		之前计时器队列不存在。 */
		configASSERT( xTimerQueue );

		/* 使用函数参数完成消息并将
		其发布到守护程序任务。 */
		xMessage.xMessageID = tmrCOMMAND_EXECUTE_CALLBACK;
		xMessage.u.xCallbackParameters.pxCallbackFunction = xFunctionToPend;
		xMessage.u.xCallbackParameters.pvParameter1 = pvParameter1;
		xMessage.u.xCallbackParameters.ulParameter2 = ulParameter2;

		xReturn = xQueueSendToBack( xTimerQueue, &xMessage, xTicksToWait );

		tracePEND_FUNC_CALL( xFunctionToPend, pvParameter1, ulParameter2, xReturn );

		return xReturn;
	}

#endif /* INCLUDE_xTimerPendFunctionCall */
/*-----------------------------------------------------------*/

#if ( configUSE_TRACE_FACILITY == 1 )

	UBaseType_t uxTimerGetTimerNumber( TimerHandle_t xTimer )
	{
		return ( ( Timer_t * ) xTimer )->uxTimerNumber;
	}

#endif /* configUSE_TRACE_FACILITY */
/*-----------------------------------------------------------*/

#if ( configUSE_TRACE_FACILITY == 1 )

	void vTimerSetTimerNumber( TimerHandle_t xTimer, UBaseType_t uxTimerNumber )
	{
		( ( Timer_t * ) xTimer )->uxTimerNumber = uxTimerNumber;
	}

#endif /* configUSE_TRACE_FACILITY */
/*-----------------------------------------------------------*/

/* 如果应用程序未配置为包含软件计时器功能，则将跳过整个源文件。
如果要包含软件定时器功能，请确保
FreeRTOSConfig.h 中的 configUSE_TIMERS 设置为 1。 */
#endif /* configUSE_TIMERS == 1 */


