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
#include <string.h>

/* 定义
MPU_WRAPPERS_INCLUDED_FROM_API_FILE
可防止 task.h 重新定义所有 API 函数以使用 MPU 包装器。  仅当应用程序文件包含 task.h 时才应执行此操作。 */
#define MPU_WRAPPERS_INCLUDED_FROM_API_FILE

/* FreeRTOS 头文件。 */
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "stack_macros.h"

/* Lint e9021、e961 和 e750 被抑制为 MISRA 异常，因为 MPU
端口需要定义 MPU_WRAPPERS_INCLUDED_FROM_API_FILE
为生成正确的特权与非特权链接和代码布局，上述头文件中需要定义该宏，
正确的特权与非特权链接和放置。 */
#undef MPU_WRAPPERS_INCLUDED_FROM_API_FILE /*lint！e961！e750！e9021。 */

/* 将
configUSE_STATS_FORMATTING_FUNCTIONS 设置为 2 以包含统计格式化函数，但此处不包含 stdio.h。 */
#if ( configUSE_STATS_FORMATTING_FUNCTIONS == 1 )
	/* 该文件的底部有两个可选函数，可用于根据
	uxTaskGetSystemState() 函数
	生成的原始数据生成人类可读的文本。  注意提供了格式化函数
	仅为方便使用，不被视为内核的一部分。 */
	#include <stdio.h>
#endif /* configUSE_STATS_FORMATTING_FUNCTIONS == 1 ) */

#if( configUSE_PREEMPTION == 0 )
	/* 如果正在使用协作调度程序，则不应仅仅因
	为已唤醒更高优先级的任务而执行让步。 */
	#define taskYIELD_IF_USING_PREEMPTION()
#else
	#define taskYIELD_IF_USING_PREEMPTION() portYIELD_WITHIN_API()
#endif

/* 可以分配给 TCB 的 ucNotifyState 成员的值。 */
#define taskNOT_WAITING_NOTIFICATION	( ( uint8_t ) 0 )
#define taskWAITING_NOTIFICATION		( ( uint8_t ) 1 )
#define taskNOTIFICATION_RECEIVED		( ( uint8_t ) 2 )

/*
 * 创建任务时用于填充任务堆栈的值。
 * 这纯粹用于检查任务的高水位线。
 */
#define tskSTACK_FILL_BYTE	( 0xa5U )

/* 用于记录任务堆栈和 TCB 分配方式的位。 */
#define tskDYNAMICALLY_ALLOCATED_STACK_AND_TCB 		( ( uint8_t ) 0 )
#define tskSTATICALLY_ALLOCATED_STACK_ONLY 			( ( uint8_t ) 1 )
#define tskSTATICALLY_ALLOCATED_STACK_AND_TCB		( ( uint8_t ) 2 )

/* 如果设置了以下任何一项，则任务堆栈将填充已知值，以便可
以确定高水位线。  如果没有设置以下任何一项，则不填充
堆栈，这样就不存在对 memset 不必要的依赖。 */
#if( ( configCHECK_FOR_STACK_OVERFLOW > 1 ) || ( configUSE_TRACE_FACILITY == 1 ) || ( INCLUDE_uxTaskGetStackHighWaterMark == 1 ) || ( INCLUDE_uxTaskGetStackHighWaterMark2 == 1 ) )
	#define tskSET_NEW_STACKS_TO_KNOWN_VALUE	1
#else
	#define tskSET_NEW_STACKS_TO_KNOWN_VALUE	0
#endif

/*
 * vListTask 使用宏来指示任务所处的状态。
 */
#define tskRUNNING_CHAR		( 'X' )
#define tskBLOCKED_CHAR		( 'B' )
#define tskREADY_CHAR		( 'R' )
#define tskDELETED_CHAR		( 'D' )
#define tskSUSPENDED_CHAR	( 'S' )

/*
 * 一些内核感知调试器要求调试器需要访问
 * 的数据是全局的，而不是文件范围的。
 */
#ifdef portREMOVE_STATIC_QUALIFIER
	#define static
#endif

/* 分配给空闲任务的名称。  这可以通过在
FreeRTOSConfig.h 中定义 configIDLE_TASK_NAME 来覆盖。 */
#ifndef configIDLE_TASK_NAME
	#define configIDLE_TASK_NAME "IDLE"
#endif

#if ( configUSE_PORT_OPTIMISED_TASK_SELECTION == 0 )

	/* 如果
	configUSE_PORT_OPTIMISED_TASK_SELECTION
	为 0，则以未针对任何特定微控制器架构进行优化的通用方式执行任务选择。 */

	/* uxTopReadyPriority
	保存最高优先级就绪状态任务的优先级。 */
	#define taskRECORD_READY_PRIORITY( uxPriority )														\
	{																									\
		if( ( uxPriority ) > uxTopReadyPriority )														\
		{																								\
			uxTopReadyPriority = ( uxPriority );														\
		}																								\
	} /* taskRECORD_READY_PRIORITY */

	/*-----------------------------------------------------------*/

	#define taskSELECT_HIGHEST_PRIORITY_TASK()															\
	{																									\
	UBaseType_t uxTopPriority = uxTopReadyPriority;														\
																										\
		/* 查找包含就绪任务的最高优先级队列。 */								\
		while( listLIST_IS_EMPTY( &( pxReadyTasksLists[ uxTopPriority ] ) ) )							\
		{																								\
			configASSERT( uxTopPriority );																\
			--uxTopPriority;																			\
		}																								\
																										\
		/* listGET_OWNER_OF_NEXT_ENTRY 通过列
		表进行索引，因此具有相同优先级的任务将获得同等的处理器时间。 */									\
		listGET_OWNER_OF_NEXT_ENTRY( pxCurrentTCB, &( pxReadyTasksLists[ uxTopPriority ] ) );			\
		uxTopReadyPriority = uxTopPriority;																\
	} /* taskSELECT_HIGHEST_PRIORITY_TASK */

	/*-----------------------------------------------------------*/

	/* 定义
	taskRESET_READY_PRIORITY() 和
	portRESET_READY_PRIORITY()，因为仅当使用端口优化的任务选择方法时才需要它们。 */
	#define taskRESET_READY_PRIORITY( uxPriority )
	#define portRESET_READY_PRIORITY( uxPriority, uxTopReadyPriority )

#else /* configUSE_PORT_OPTIMISED_TASK_SELECTION */

	/* 如果
	configUSE_PORT_OPTIMISED_TASK_SELECTION
	为 1，则以针对所使用的特定微控制器架构定制的方式执行任务选择。 */

	/* 提供了端口优化版本。  调用端口定义的宏。 */
	#define taskRECORD_READY_PRIORITY( uxPriority )	portRECORD_READY_PRIORITY( uxPriority, uxTopReadyPriority )

	/*-----------------------------------------------------------*/

	#define taskSELECT_HIGHEST_PRIORITY_TASK()														\
	{																								\
	UBaseType_t uxTopPriority;																		\
																									\
		/* 查找包含就绪任务的最高优先级列表。 */								\
		portGET_HIGHEST_PRIORITY( uxTopPriority, uxTopReadyPriority );								\
		configASSERT( listCURRENT_LIST_LENGTH( &( pxReadyTasksLists[ uxTopPriority ] ) ) > 0 );		\
		listGET_OWNER_OF_NEXT_ENTRY( pxCurrentTCB, &( pxReadyTasksLists[ uxTopPriority ] ) );		\
	} /* taskSELECT_HIGHEST_PRIORITY_TASK() */

	/*-----------------------------------------------------------*/

	/* 提供了端口优化版本，仅当从就绪列表引用正在重置
	的 TCB 时才调用它。  如果它是从延迟或暂
	停列表中引用的，那么它将不会出现在就绪列表中。 */
	#define taskRESET_READY_PRIORITY( uxPriority )														\
	{																									\
		if( listCURRENT_LIST_LENGTH( &( pxReadyTasksLists[ ( uxPriority ) ] ) ) == ( UBaseType_t ) 0 )	\
		{																								\
			portRESET_READY_PRIORITY( ( uxPriority ), ( uxTopReadyPriority ) );							\
		}																								\
	}

#endif /* configUSE_PORT_OPTIMISED_TASK_SELECTION */

/*-----------------------------------------------------------*/

/* 当滴答计数溢出时，pxDelayedTaskList 和
pxOverflowDelayedTaskList 进行切换。 */
#define taskSWITCH_DELAYED_LISTS()																	\
{																									\
	List_t *pxTemp;																					\
																									\
	/* 切换列表时，延迟任务列表应为空。 */						\
	configASSERT( ( listLIST_IS_EMPTY( pxDelayedTaskList ) ) );										\
																									\
	pxTemp = pxDelayedTaskList;																		\
	pxDelayedTaskList = pxOverflowDelayedTaskList;													\
	pxOverflowDelayedTaskList = pxTemp;																\
	xNumOfOverflows++;																				\
	prvResetNextTaskUnblockTime();																	\
}

/*-----------------------------------------------------------*/

/*
 * 将 pxTCB 表示的任务放入该任务的相
 * 应就绪列表中。  它被插入到列表的末尾。
 */
#define prvAddTaskToReadyList( pxTCB )																\
	traceMOVED_TASK_TO_READY_STATE( pxTCB );														\
	taskRECORD_READY_PRIORITY( ( pxTCB )->uxPriority );												\
	vListInsertEnd( &( pxReadyTasksLists[ ( pxTCB )->uxPriority ] ), &( ( pxTCB )->xStateListItem ) ); \
	tracePOST_MOVED_TASK_TO_READY_STATE( pxTCB )
/*-----------------------------------------------------------*/

/*
 * 有几个函数采用 TaskHandle_t 参数，该参数
 * 可以选择为 NULL，其中 NULL 用于指示应使用当
 * 前正在执行的任务的句柄来代替该参数。  该宏只是检查参
 * 数是否为 NULL 并返回指向相应 TCB 的指针。
 */
#define prvGetTCBFromHandle( pxHandle ) ( ( ( pxHandle ) == NULL ) ? pxCurrentTCB : ( pxHandle ) )

/* 事件列表项的项值通常用于保存其所属任务的优
先级 belongs（编码以允许以相反的优
先级顺序保存）。  然而，偶尔也会被借用于
其他目的。  重要的是，当它被用于其他目的
时，它的值不会由于任务优先级的变化而更新。
以下位定义用于通知调度程序该值不应更改
- 在这种情况下，使用该值的模块有责任确
保在释放该值时将其设置回原始值。 */
#if( configUSE_16_BIT_TICKS == 1 )
	#define taskEVENT_LIST_ITEM_VALUE_IN_USE	0x8000U
#else
	#define taskEVENT_LIST_ITEM_VALUE_IN_USE	0x80000000UL
#endif

/*
 * 任务控制块。  为每个任务分配一个任务控制
 * block（TCB），存储任务状态信息，包括指向任
 * 务的context的指针（任务的运行时环境，包括寄存器值）
 */
typedef struct tskTaskControlBlock 			/* 旧的命名约定用于防止破坏内核感知调试器。 */
{
	volatile StackType_t	*pxTopOfStack;	/*< 指向任务堆栈上最后一项的位置。  THIS MUST BE THE FIRST MEMBER OF THE TCB STRUCT。 */

	#if ( portUSING_MPU_WRAPPERS == 1 )
		xMPU_SETTINGS	xMPUSettings;		/*< MPU 设置被定义为端口层的一部分。  THIS MUST BE THE SECOND MEMBER OF THE TCB STRUCT。 */
	#endif

	ListItem_t			xStateListItem;	/*< 任务的状态列表项所引用的列表表示该 task 的状态（就绪、阻塞、暂停）。 */
	ListItem_t			xEventListItem;		/*< 用于引用事件列表中的任务。 */
	UBaseType_t			uxPriority;			/*< 任务的优先级。  0 是最低优先级。 */
	StackType_t			*pxStack;			/*< 指向堆栈的开头。 */
	char				pcTaskName[ configMAX_TASK_NAME_LEN ];/*< 创建任务时指定的描述性名称。  仅有助于调试。 */ /*lint !e971 仅字符串和单个字符允许使用非限定字符类型。 */

	#if ( ( portSTACK_GROWTH > 0 ) || ( configRECORD_STACK_HIGH_ADDRESS == 1 ) )
		StackType_t		*pxEndOfStack;		/*< 指向堆栈的最高有效地址。 */
	#endif

	#if ( portCRITICAL_NESTING_IN_TCB == 1 )
		UBaseType_t		uxCriticalNesting;	/*< 保留在端口层中不维护自己计数的端口的临界区嵌套深度。 */
	#endif

	#if ( configUSE_TRACE_FACILITY == 1 )
		UBaseType_t		uxTCBNumber;		/*< 存储每次创建 TCB 时递增的数字。  它允许调试器确定任务何时被删除然后重新创建。 */
		UBaseType_t		uxTaskNumber;		/*< 存储专门供第三方跟踪代码使用的数字。 */
	#endif

	#if ( configUSE_MUTEXES == 1 )
		UBaseType_t		uxBasePriority;		/*< 最后分配给任务的优先级 - 由优先级继承机制使用。 */
		UBaseType_t		uxMutexesHeld;
	#endif

	#if ( configUSE_APPLICATION_TASK_TAG == 1 )
		TaskHookFunction_t pxTaskTag;
	#endif

	#if( configNUM_THREAD_LOCAL_STORAGE_POINTERS > 0 )
		void			*pvThreadLocalStoragePointers[ configNUM_THREAD_LOCAL_STORAGE_POINTERS ];
	#endif

	#if( configGENERATE_RUN_TIME_STATS == 1 )
		uint32_t		ulRunTimeCounter;	/*< 存储任务处于运行状态的时间量。 */
	#endif

	#if ( configUSE_NEWLIB_REENTRANT == 1 )
		/* 分配一个特定于此任务的 Newlib reent 结构。
		注意 Newlib 支持已根据大众需求包含在内，但
		FreeRTOS 维护者本身并未使用。
		FreeRTOS 不对由此产生的 newlib 操作负责
		。  用户必须熟悉 newlib 并且必须提供必要存根的
		系统范围实现。请注意 that（在撰写本文时）当前的
		newlib 设计实现了必须提供锁的系统范围 malloc()。

		查看第三方链接 http://www.nadler.com/embedded/newlibAndFreeRTOS.html
		for additional information. */
		struct	_reent xNewLib_reent;
	#endif

	#if( configUSE_TASK_NOTIFICATIONS == 1 )
		volatile uint32_t ulNotifiedValue;
		volatile uint8_t ucNotifyState;
	#endif

	/* 请参阅 FreeRTOS.h 中的注释以及定义
	tskSTATIC_AND_DYNAMIC_ALLOCATION_POSSIBLE. */
	#if( tskSTATIC_AND_DYNAMIC_ALLOCATION_POSSIBLE != 0 ) /*lint !e731 !e9029 出于可读性原因，宏已被合并。 */
		uint8_t	ucStaticallyAllocated; 		/*< 如果任务是静态分配的，则设置为 pdTRUE 以确保不会尝试释放内存。 */
	#endif

	#if( INCLUDE_xTaskAbortDelay == 1 )
		uint8_t ucDelayAborted;
	#endif

	#if( configUSE_POSIX_ERRNO == 1 )
		int iTaskErrno;
	#endif

} tskTCB;

/* 旧的 tskTCB 名称保留在上面，然后键入定义为下
面的新 TCB_t 名称，以启用旧的内核感知调试器。 */
typedef tskTCB TCB_t;

/*lint -save -e956 已使用手动分析和检
查来确定哪些静态变量必须声明为 volatile。 */
PRIVILEGED_DATA TCB_t * volatile pxCurrentTCB = NULL;

/* 就绪任务和阻塞任务的列表。 ---------------
----- xDelayedTaskList1 和
xDelayedTaskList2 可以移动到函数作用域，
但这样做会破坏一些内核感知调试器和依赖于删除静态限定符的调试器。 */
PRIVILEGED_DATA static List_t pxReadyTasksLists[ configMAX_PRIORITIES ];/*< 优先准备好的任务。 */
PRIVILEGED_DATA static List_t xDelayedTaskList1;						/*< 延迟的任务。 */
PRIVILEGED_DATA static List_t xDelayedTaskList2;						/*< Delayed tasks（使用两个列表 - 一个用于超出当前滴答计数的延迟。 */
PRIVILEGED_DATA static List_t * volatile pxDelayedTaskList;				/*< 指向当前正在使用的延迟任务列表。 */
PRIVILEGED_DATA static List_t * volatile pxOverflowDelayedTaskList;		/*< 指向当前用于保存已溢出当前滴答计数的任务的延迟任务列表。 */
PRIVILEGED_DATA static List_t xPendingReadyList;						/*< 调度程序挂起时已准备就绪的任务。  当调度程序恢复时，它们将被移至就绪列表。 */

#if( INCLUDE_vTaskDelete == 1 )

	PRIVILEGED_DATA static List_t xTasksWaitingTermination;				/*< 任务已被删除 - 但其内存尚未释放。 */
	PRIVILEGED_DATA static volatile UBaseType_t uxDeletedTasksWaitingCleanUp = ( UBaseType_t ) 0U;

#endif

#if ( INCLUDE_vTaskSuspend == 1 )

	PRIVILEGED_DATA static List_t xSuspendedTaskList;					/*< 当前暂停的任务。 */

#endif

/* 全局 POSIX 错误号它的值会在上下文切换时
更改，以匹配当前正在运行的任务的 errno。 */
#if ( configUSE_POSIX_ERRNO == 1 )
	int FreeRTOS_errno = 0;
#endif

/* 其他文件私有变量。 --------------------------------*/
PRIVILEGED_DATA static volatile UBaseType_t uxCurrentNumberOfTasks 	= ( UBaseType_t ) 0U;
PRIVILEGED_DATA static volatile TickType_t xTickCount 				= ( TickType_t ) configINITIAL_TICK_COUNT;
PRIVILEGED_DATA static volatile UBaseType_t uxTopReadyPriority 		= tskIDLE_PRIORITY;
PRIVILEGED_DATA static volatile BaseType_t xSchedulerRunning 		= pdFALSE;
PRIVILEGED_DATA static volatile TickType_t xPendedTicks 			= ( TickType_t ) 0U;
PRIVILEGED_DATA static volatile BaseType_t xYieldPending 			= pdFALSE;
PRIVILEGED_DATA static volatile BaseType_t xNumOfOverflows 			= ( BaseType_t ) 0;
PRIVILEGED_DATA static UBaseType_t uxTaskNumber 					= ( UBaseType_t ) 0U;
PRIVILEGED_DATA static volatile TickType_t xNextTaskUnblockTime		= ( TickType_t ) 0U; /* 在调度程序启动之前初始化为 portMAX_DELAY。 */
PRIVILEGED_DATA static TaskHandle_t xIdleTaskHandle					= NULL;			/*< 持有空闲任务的句柄。  空闲任务是在调度程序启动时自动创建的。 */

/* 当调度程序挂起时，上下文切换保持挂起状态。  此外，
如果调度程序被挂起，中断不得操作 TCB 的
xStateListItem 或可引用
xStateListItem 的任何列表。如果中断需
要在调度程序挂起时解除任务的阻塞，那么它会将任务的事
件列表项移动到 xPendingReadyList
中，为内核在调度程序未挂起时将任务从挂起就绪列表移动
到实际就绪列表做好准备。  挂起的就绪列表本身只能从临界区访问。 */
PRIVILEGED_DATA static volatile UBaseType_t uxSchedulerSuspended	= ( UBaseType_t ) pdFALSE;

#if ( configGENERATE_RUN_TIME_STATS == 1 )

	/* 不要将这些变量移动到函数作用域，因为这样做会阻
	止代码与需要删除静态限定符的调试器一起使用。 */
	PRIVILEGED_DATA static uint32_t ulTaskSwitchedInTime = 0UL;	/*< 保存上次切换任务时定时器/计数器的值。 */
	PRIVILEGED_DATA static uint32_t ulTotalRunTime = 0UL;		/*< 保存由运行时间计数器时钟定义的执行时间总量。 */

#endif

/*lint - 恢复 */

/*-----------------------------------------------------------*/

/* 回调函数原型。 --------------------------*/
#if(  configCHECK_FOR_STACK_OVERFLOW > 0 )

	extern void vApplicationStackOverflowHook( TaskHandle_t xTask, char *pcTaskName );

#endif

#if( configUSE_TICK_HOOK > 0 )

	extern void vApplicationTickHook( void ); /*lint !e526 符号未定义，因为它是应用程序回调。 */

#endif

#if( configSUPPORT_STATIC_ALLOCATION == 1 )

	extern void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize ); /*lint !e526 符号未定义，因为它是应用程序回调。 */

#endif

/* 文件私有函数。 --------------------------------*/

/**
 * 如果 xTask 引用的任务当前处于挂起状态，则实用
 * 程序任务仅返回 pdTRUE；如果 xTask 引用
 * 的任务处于任何其他状态，则返回 pdFALSE。
 */
#if ( INCLUDE_vTaskSuspend == 1 )

	static BaseType_t prvTaskIsTaskSuspended( const TaskHandle_t xTask ) PRIVILEGED_FUNCTION;

#endif /* INCLUDE_vTaskSuspend */

/*
 * 用于准备调度程序使用的所有列表的实用程序
 * 。  这是在创建第一个任务时自动调用的。
 */
static void prvInitialiseTaskLists( void ) PRIVILEGED_FUNCTION;

/*
 * 空闲任务，与所有任务一样，都是作为永无止
 * 境的循环实现的。创建第一个用户任务时，会
 * 自动创建空闲任务并将其添加到就绪列表中。
 *
 * portTASK_FUNCTION_PROTO() 宏用于
 * 允许端口/编译器特定的语言扩展。  该函数的等效原型是：
 *
 * 无效 prvIdleTask( 无效 *pvParameters );
 *
 */
static portTASK_FUNCTION_PROTO( prvIdleTask, pvParameters );

/*
 * 用于释放调度程序分配的所有内存以保存
 * TCB 的实用程序，包括 TCB 指向的堆栈。
 *
 * 这不会释放任务 itself 分配的内存（即从任务应用程
 * 序代码中调用 pvPortMalloc 分配的内存）。
 */
#if ( INCLUDE_vTaskDelete == 1 )

	static void prvDeleteTCB( TCB_t *pxTCB ) PRIVILEGED_FUNCTION;

#endif

/*
 * 仅由空闲任务使用。  这会检查是否有任何内
 * 容已放置在等待删除的任务列表中。  如果是
 * 这样，该任务将被清理并删除其 TCB。
 */
static void prvCheckTasksWaitingTermination( void ) PRIVILEGED_FUNCTION;

/*
 * 当前正在执行的任务正在进入阻塞状态。  将任
 * 务添加到当前任务列表或溢出延迟任务列表中。
 */
static void prvAddCurrentTaskToDelayedList( TickType_t xTicksToWait, const BaseType_t xCanBlockIndefinitely ) PRIVILEGED_FUNCTION;

/*
 * 使用从 pxList list 引用的每个任
 * 务的信息填充 TaskStatus_t 结构
 * （可能是就绪列表、延迟列表、挂起列表等）。
 *
 * 此功能仅用于调试、并且
 * 不应从普通应用程序代码中调用。
 */
#if ( configUSE_TRACE_FACILITY == 1 )

	static UBaseType_t prvListTasksWithinSingleList( TaskStatus_t *pxTaskStatusArray, List_t *pxList, eTaskState eState ) PRIVILEGED_FUNCTION;

#endif

/*
 * 在 pxList 中搜索名为 pcNameToQuery 的任务 - 如
 * 果找到该任务，则返回该任务的句柄；如果未找到该任务，则返回 NULL。
 */
#if ( INCLUDE_xTaskGetHandle == 1 )

	static TCB_t *prvSearchForNameWithinSingleList( List_t *pxList, const char pcNameToQuery[] ) PRIVILEGED_FUNCTION;

#endif

/*
 * 当创建任务时，任务的堆栈会填充一个已知
 * 值。该函数通过确定堆栈的剩余量保持原始
 * 预设值来确定任务堆栈的“高水位线”。
 */
#if ( ( configUSE_TRACE_FACILITY == 1 ) || ( INCLUDE_uxTaskGetStackHighWaterMark == 1 ) || ( INCLUDE_uxTaskGetStackHighWaterMark2 == 1 ) )

	static configSTACK_DEPTH_TYPE prvTaskCheckFreeStackSpace( const uint8_t * pucStackByte ) PRIVILEGED_FUNCTION;

#endif

/*
 * 返回内核下次将任务从“阻塞”状态移动到“运行”
 * 状态之前所经过的时间量（以时钟周期为单位）。
 *
 * 此条件编译应使用不等于 0，而不是等于 1。这是为了确保当
 * 用户定义的低功耗模式实现需要将
 * configUSE_TICKLESS_IDLE 设置为 1
 * 以外的值时可以调用 portSUPPRESS_TICKS_AND_SLEEP()。
 */
#if ( configUSE_TICKLESS_IDLE != 0 )

	static TickType_t prvGetExpectedIdleTime( void ) PRIVILEGED_FUNCTION;

#endif

/*
 * 将 xNextTaskUnblockTime
 * 设置为下一个阻塞状态任务退出阻塞状态的时间。
 */
static void prvResetNextTaskUnblockTime( void );

#if ( ( configUSE_TRACE_FACILITY == 1 ) && ( configUSE_STATS_FORMATTING_FUNCTIONS > 0 ) )

	/*
	 * 帮助函数用于在打印人类可读的任
	 * 务信息表时用空格填充任务名称。
	 */
	static char *prvWriteNameToBuffer( char *pcBuffer, const char *pcTaskName ) PRIVILEGED_FUNCTION;

#endif

/*
 * 在静态或动态分配 Task_t
 * 结构以填充结构的成员后调用。
 */
static void prvInitialiseNewTask( 	TaskFunction_t pxTaskCode,
									const char * const pcName, 		/*lint !e971 仅字符串和单个字符允许使用非限定字符类型。 */
									const uint32_t ulStackDepth,
									void * const pvParameters,
									UBaseType_t uxPriority,
									TaskHandle_t * const pxCreatedTask,
									TCB_t *pxNewTCB,
									const MemoryRegion_t * const xRegions ) PRIVILEGED_FUNCTION;

/*
 * 在创建并初始化新任务后调用，以
 * 将任务置于调度程序的控制之下。
 */
static void prvAddNewTaskToReadyList( TCB_t *pxNewTCB ) PRIVILEGED_FUNCTION;

/*
 * 仅当定义了用户可定义宏
 * FREERTOS_TASKS_C_ADDITIONS_INIT()
 * 时才应调用 freertos_tasks_c_additions_init()，因为这是该函数调用的唯一宏。
 */
#ifdef FREERTOS_TASKS_C_ADDITIONS_INIT

	static void freertos_tasks_c_additions_init( void ) PRIVILEGED_FUNCTION;

#endif

/*-----------------------------------------------------------*/

#if( configSUPPORT_STATIC_ALLOCATION == 1 )

	TaskHandle_t xTaskCreateStatic(	TaskFunction_t pxTaskCode,
									const char * const pcName,		/*lint !e971 仅字符串和单个字符允许使用非限定字符类型。 */
									const uint32_t ulStackDepth,
									void * const pvParameters,
									UBaseType_t uxPriority,
									StackType_t * const puxStackBuffer,
									StaticTask_t * const pxTaskBuffer )
	{
	TCB_t *pxNewTCB;
	TaskHandle_t xReturn;

		configASSERT( puxStackBuffer != NULL );
		configASSERT( pxTaskBuffer != NULL );

		#if( configASSERT_DEFINED == 1 )
		{
			/* 健全性检查用于声明 StaticTask_t
			类型变量的结构的大小是否等于实际任务的大小
			结构。 */
			volatile size_t xSize = sizeof( StaticTask_t );
			configASSERT( xSize == sizeof( TCB_t ) );
			( void ) xSize; /* 不使用 configASSERT() 时防止 lint 警告。 */
		}
		#endif /* configASSERT_DEFINED */


		if( ( pxTaskBuffer != NULL ) && ( puxStackBuffer != NULL ) )
		{
			/* 用于任务的 TCB 和堆栈的内存
			被传递到该函数中 - 使用它们。 */
			pxNewTCB = ( TCB_t * ) pxTaskBuffer; /*lint !e740 !e9087 不寻常的转换是可以的，因为结构被设计为具有相同的对齐方式，并且大小由断言检查。 */
			pxNewTCB->pxStack = ( StackType_t * ) puxStackBuffer;

			#if( tskSTATIC_AND_DYNAMIC_ALLOCATION_POSSIBLE != 0 ) /*lint !e731 !e9029 出于可读性原因，宏已被合并。 */
			{
				/* 任务可以静态或动态创建，因此请注意此任
				务是静态创建的，以防任务稍后被删除。 */
				pxNewTCB->ucStaticallyAllocated = tskSTATICALLY_ALLOCATED_STACK_AND_TCB;
			}
			#endif /* tskSTATIC_AND_DYNAMIC_ALLOCATION_POSSIBLE */

			prvInitialiseNewTask( pxTaskCode, pcName, ulStackDepth, pvParameters, uxPriority, &xReturn, pxNewTCB, NULL );
			prvAddNewTaskToReadyList( pxNewTCB );
		}
		else
		{
			xReturn = NULL;
		}

		return xReturn;
	}

#endif /* SUPPORT_STATIC_ALLOCATION */
/*-----------------------------------------------------------*/

#if( ( portUSING_MPU_WRAPPERS == 1 ) && ( configSUPPORT_STATIC_ALLOCATION == 1 ) )

	BaseType_t xTaskCreateRestrictedStatic( const TaskParameters_t * const pxTaskDefinition, TaskHandle_t *pxCreatedTask )
	{
	TCB_t *pxNewTCB;
	BaseType_t xReturn = errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY;

		configASSERT( pxTaskDefinition->puxStackBuffer != NULL );
		configASSERT( pxTaskDefinition->pxTaskBuffer != NULL );

		if( ( pxTaskDefinition->puxStackBuffer != NULL ) && ( pxTaskDefinition->pxTaskBuffer != NULL ) )
		{
			/* 为TCB分配空间。  内存来自哪
			里取决于端口 malloc 函数
			的实现以及是否使用静态分配。 */
			pxNewTCB = ( TCB_t * ) pxTaskDefinition->pxTaskBuffer;

			/* 将堆栈位置存储在 TCB 中。 */
			pxNewTCB->pxStack = pxTaskDefinition->puxStackBuffer;

			#if( tskSTATIC_AND_DYNAMIC_ALLOCATION_POSSIBLE != 0 )
			{
				/* 任务可以静态或动态创建，因此请注意此任
				务是静态创建的，以防任务稍后被删除。 */
				pxNewTCB->ucStaticallyAllocated = tskSTATICALLY_ALLOCATED_STACK_AND_TCB;
			}
			#endif /* tskSTATIC_AND_DYNAMIC_ALLOCATION_POSSIBLE */

			prvInitialiseNewTask(	pxTaskDefinition->pvTaskCode,
									pxTaskDefinition->pcName,
									( uint32_t ) pxTaskDefinition->usStackDepth,
									pxTaskDefinition->pvParameters,
									pxTaskDefinition->uxPriority,
									pxCreatedTask, pxNewTCB,
									pxTaskDefinition->xRegions );

			prvAddNewTaskToReadyList( pxNewTCB );
			xReturn = pdPASS;
		}

		return xReturn;
	}

#endif /* ( portUSING_MPU_WRAPPERS == 1 ) && ( configSUPPORT_STATIC_ALLOCATION == 1 ) */
/*-----------------------------------------------------------*/

#if( ( portUSING_MPU_WRAPPERS == 1 ) && ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) )

	BaseType_t xTaskCreateRestricted( const TaskParameters_t * const pxTaskDefinition, TaskHandle_t *pxCreatedTask )
	{
	TCB_t *pxNewTCB;
	BaseType_t xReturn = errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY;

		configASSERT( pxTaskDefinition->puxStackBuffer );

		if( pxTaskDefinition->puxStackBuffer != NULL )
		{
			/* 为TCB分配空间。  内存来自哪
			里取决于端口 malloc 函数
			的实现以及是否使用静态分配。 */
			pxNewTCB = ( TCB_t * ) pvPortMalloc( sizeof( TCB_t ) );

			if( pxNewTCB != NULL )
			{
				/* 将堆栈位置存储在 TCB 中。 */
				pxNewTCB->pxStack = pxTaskDefinition->puxStackBuffer;

				#if( tskSTATIC_AND_DYNAMIC_ALLOCATION_POSSIBLE != 0 )
				{
					/* 任务可以静态或动态创建，因此请注意，
					该任务有一个静态分配的堆栈，以防以后
					被删除。  TCB 是动态分配的。 */
					pxNewTCB->ucStaticallyAllocated = tskSTATICALLY_ALLOCATED_STACK_ONLY;
				}
				#endif /* tskSTATIC_AND_DYNAMIC_ALLOCATION_POSSIBLE */

				prvInitialiseNewTask(	pxTaskDefinition->pvTaskCode,
										pxTaskDefinition->pcName,
										( uint32_t ) pxTaskDefinition->usStackDepth,
										pxTaskDefinition->pvParameters,
										pxTaskDefinition->uxPriority,
										pxCreatedTask, pxNewTCB,
										pxTaskDefinition->xRegions );

				prvAddNewTaskToReadyList( pxNewTCB );
				xReturn = pdPASS;
			}
		}

		return xReturn;
	}

#endif /* portUSING_MPU_WRAPPERS */
/*-----------------------------------------------------------*/

#if( configSUPPORT_DYNAMIC_ALLOCATION == 1 )

	BaseType_t xTaskCreate(	TaskFunction_t pxTaskCode,
							const char * const pcName,		/*lint !e971 仅字符串和单个字符允许使用非限定字符类型。 */
							const configSTACK_DEPTH_TYPE usStackDepth,
							void * const pvParameters,
							UBaseType_t uxPriority,
							TaskHandle_t * const pxCreatedTask )
	{
	TCB_t *pxNewTCB;
	BaseType_t xReturn;

		/* 如果堆栈向下增长，则先分配堆栈，然后再分配
		TCB，这样堆栈就不会增长到 TCB。  同样
		，如果堆栈增大，则分配 TCB，然后分配堆栈。 */
		#if( portSTACK_GROWTH > 0 )
		{
			/* 为TCB分配空间。  内存来自哪
			里取决于端口 malloc 函数
			的实现以及是否使用静态分配。 */
			pxNewTCB = ( TCB_t * ) pvPortMalloc( sizeof( TCB_t ) );

			if( pxNewTCB != NULL )
			{
				/* 为正在创建的任务使用的堆栈分配空间
				。堆栈内存的基址存储在 TCB 中
				，因此以后可以根据需要删除任务。 */
				pxNewTCB->pxStack = ( StackType_t * ) pvPortMalloc( ( ( ( size_t ) usStackDepth ) * sizeof( StackType_t ) ) ); /*lint !e961 MISRA 例外，因为强制转换仅对某些端口是多余的。 */

				if( pxNewTCB->pxStack == NULL )
				{
					/* 无法分配堆栈。  删除分配的TCB。 */
					vPortFree( pxNewTCB );
					pxNewTCB = NULL;
				}
			}
		}
		#else /* portSTACK_GROWTH */
		{
		StackType_t *pxStack;

			/* 为正在创建的任务使用的堆栈分配空间。 */
			pxStack = pvPortMalloc( ( ( ( size_t ) usStackDepth ) * sizeof( StackType_t ) ) ); /*lint !e9079 pvPortMalloc() 返回的所有值至少具有 MCU 堆栈所需的对齐方式，并且此分配是堆栈。 */

			if( pxStack != NULL )
			{
				/* 为TCB分配空间。 */
				pxNewTCB = ( TCB_t * ) pvPortMalloc( sizeof( TCB_t ) ); /*lint !e9087 !e9079 pvPortMalloc() 返回的所有值至少具有 MCU 堆栈所需的对齐方式，并且 TCB_t 的第一个成员始终是指向任务堆栈的指针。 */

				if( pxNewTCB != NULL )
				{
					/* 将堆栈位置存储在 TCB 中。 */
					pxNewTCB->pxStack = pxStack;
				}
				else
				{
					/* 由于未创建 TCB，因此无法
					使用堆栈。  再次释放它。 */
					vPortFree( pxStack );
				}
			}
			else
			{
				pxNewTCB = NULL;
			}
		}
		#endif /* portSTACK_GROWTH */

		if( pxNewTCB != NULL )
		{
			#if( tskSTATIC_AND_DYNAMIC_ALLOCATION_POSSIBLE != 0 ) /*lint !e9029 !e731 出于可读性原因，宏已被合并。 */
			{
				/* 任务可以静态或动态创建，因此请注意此
				任务是动态创建的，以防以后被删除。 */
				pxNewTCB->ucStaticallyAllocated = tskDYNAMICALLY_ALLOCATED_STACK_AND_TCB;
			}
			#endif /* tskSTATIC_AND_DYNAMIC_ALLOCATION_POSSIBLE */

			prvInitialiseNewTask( pxTaskCode, pcName, ( uint32_t ) usStackDepth, pvParameters, uxPriority, pxCreatedTask, pxNewTCB, NULL );
			prvAddNewTaskToReadyList( pxNewTCB );
			xReturn = pdPASS;
		}
		else
		{
			xReturn = errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY;
		}

		return xReturn;
	}

#endif /* configSUPPORT_DYNAMIC_ALLOCATION */
/*-----------------------------------------------------------*/

static void prvInitialiseNewTask( 	TaskFunction_t pxTaskCode,
									const char * const pcName,		/*lint !e971 仅字符串和单个字符允许使用非限定字符类型。 */
									const uint32_t ulStackDepth,
									void * const pvParameters,
									UBaseType_t uxPriority,
									TaskHandle_t * const pxCreatedTask,
									TCB_t *pxNewTCB,
									const MemoryRegion_t * const xRegions )
{
StackType_t *pxTopOfStack;
UBaseType_t x;

	#if( portUSING_MPU_WRAPPERS == 1 )
		/* 是否应该在特权模式下创建任务？ */
		BaseType_t xRunPrivileged;
		if( ( uxPriority & portPRIVILEGE_BIT ) != 0U )
		{
			xRunPrivileged = pdTRUE;
		}
		else
		{
			xRunPrivileged = pdFALSE;
		}
		uxPriority &= ~portPRIVILEGE_BIT;
	#endif /* portUSING_MPU_WRAPPERS == 1 */

	/* 如果不需要，请避免依赖 memset()。 */
	#if( tskSET_NEW_STACKS_TO_KNOWN_VALUE == 1 )
	{
		/* 用已知值填充堆栈以帮助调试。 */
		( void ) memset( pxNewTCB->pxStack, ( int ) tskSTACK_FILL_BYTE, ( size_t ) ulStackDepth * sizeof( StackType_t ) );
	}
	#endif /* tskSET_NEW_STACKS_TO_KNOWN_VALUE */

	/* 计算栈顶地址。  这取决于堆栈是否从高端内存
	增长到 low（按照 80x86），或者反之
	亦然。 portSTACK_GROWTH 用
	于根据端口的需要将结果设为正值或负值。 */
	#if( portSTACK_GROWTH < 0 )
	{
		pxTopOfStack = &( pxNewTCB->pxStack[ ulStackDepth - ( uint32_t ) 1 ] );
		pxTopOfStack = ( StackType_t * ) ( ( ( portPOINTER_SIZE_TYPE ) pxTopOfStack ) & ( ~( ( portPOINTER_SIZE_TYPE ) portBYTE_ALIGNMENT_MASK ) ) ); /*lint !e923 !e9033 !e9078 MISRA 异常。  避免指针和整数之间的强制转换是不切实际的。  尺寸差异采用portPOINTER_SIZE_TYPE型。  通过 assert() 检查。 */

		/* 检查计算出的栈顶的对齐方式是否正确。 */
		configASSERT( ( ( ( portPOINTER_SIZE_TYPE ) pxTopOfStack & ( portPOINTER_SIZE_TYPE ) portBYTE_ALIGNMENT_MASK ) == 0UL ) );

		#if( configRECORD_STACK_HIGH_ADDRESS == 1 )
		{
			/* 还记录堆栈的高地址，这可能会有所帮助
			调试。 */
			pxNewTCB->pxEndOfStack = pxTopOfStack;
		}
		#endif /* configRECORD_STACK_HIGH_ADDRESS */
	}
	#else /* portSTACK_GROWTH */
	{
		pxTopOfStack = pxNewTCB->pxStack;

		/* 检查堆栈缓冲区的对齐是否正确。 */
		configASSERT( ( ( ( portPOINTER_SIZE_TYPE ) pxNewTCB->pxStack & ( portPOINTER_SIZE_TYPE ) portBYTE_ALIGNMENT_MASK ) == 0UL ) );

		/* 如果进行堆栈检查，则需要堆栈空间的另一个极端
		执行。 */
		pxNewTCB->pxEndOfStack = pxNewTCB->pxStack + ( ulStackDepth - ( uint32_t ) 1 );
	}
	#endif /* portSTACK_GROWTH */

	/* 将任务名称存储在TCB中。 */
	if( pcName != NULL )
	{
		for( x = ( UBaseType_t ) 0; x < ( UBaseType_t ) configMAX_TASK_NAME_LEN; x++ )
		{
			pxNewTCB->pcTaskName[ x ] = pcName[ x ];

			/* 如果字符串短于 configMAX_TASK_NAME_LEN 个
			字符，则不要复制所有
			configMAX_TASK_NAME_LEN，以防字符串后面的内存不是 accessible（极不可能）。 */
			if( pcName[ x ] == ( char ) 0x00 )
			{
				break;
			}
			else
			{
				mtCOVERAGE_TEST_MARKER();
			}
		}

		/* 确保名称字符串在字符串长度大于或等于
		configMAX_TASK_NAME_LEN 时终止。 */
		pxNewTCB->pcTaskName[ configMAX_TASK_NAME_LEN - 1 ] = '\0';
	}
	else
	{
		/* 该任务尚未命名，因此只需确保读
		出时有 NULL 终止符即可。 */
		pxNewTCB->pcTaskName[ 0 ] = 0x00;
	}

	/* 它用作数组索引，因此必须确保它不会太
	大。  首先删除特权位（如果存在）。 */
	if( uxPriority >= ( UBaseType_t ) configMAX_PRIORITIES )
	{
		uxPriority = ( UBaseType_t ) configMAX_PRIORITIES - ( UBaseType_t ) 1U;
	}
	else
	{
		mtCOVERAGE_TEST_MARKER();
	}

	pxNewTCB->uxPriority = uxPriority;
	#if ( configUSE_MUTEXES == 1 )
	{
		pxNewTCB->uxBasePriority = uxPriority;
		pxNewTCB->uxMutexesHeld = 0;
	}
	#endif /* configUSE_MUTEXES */

	vListInitialiseItem( &( pxNewTCB->xStateListItem ) );
	vListInitialiseItem( &( pxNewTCB->xEventListItem ) );

	/* 将 pxNewTCB 设置为从 ListItem_t 返回的链接
	。  这样我们就可以从列表中的通用项返回到包含 TCB 的内容。 */
	listSET_LIST_ITEM_OWNER( &( pxNewTCB->xStateListItem ), pxNewTCB );

	/* 事件列表始终按优先级顺序排列。 */
	listSET_LIST_ITEM_VALUE( &( pxNewTCB->xEventListItem ), ( TickType_t ) configMAX_PRIORITIES - ( TickType_t ) uxPriority ); /*lint !e961 MISRA 例外，因为强制转换仅对某些端口是多余的。 */
	listSET_LIST_ITEM_OWNER( &( pxNewTCB->xEventListItem ), pxNewTCB );

	#if ( portCRITICAL_NESTING_IN_TCB == 1 )
	{
		pxNewTCB->uxCriticalNesting = ( UBaseType_t ) 0U;
	}
	#endif /* portCRITICAL_NESTING_IN_TCB */

	#if ( configUSE_APPLICATION_TASK_TAG == 1 )
	{
		pxNewTCB->pxTaskTag = NULL;
	}
	#endif /* configUSE_APPLICATION_TASK_TAG */

	#if ( configGENERATE_RUN_TIME_STATS == 1 )
	{
		pxNewTCB->ulRunTimeCounter = 0UL;
	}
	#endif /* configGENERATE_RUN_TIME_STATS */

	#if ( portUSING_MPU_WRAPPERS == 1 )
	{
		vPortStoreTaskMPUSettings( &( pxNewTCB->xMPUSettings ), xRegions, pxNewTCB->pxStack, ulStackDepth );
	}
	#else
	{
		/* 避免编译器对未引用参数发出警告。 */
		( void ) xRegions;
	}
	#endif

	#if( configNUM_THREAD_LOCAL_STORAGE_POINTERS != 0 )
	{
		for( x = 0; x < ( UBaseType_t ) configNUM_THREAD_LOCAL_STORAGE_POINTERS; x++ )
		{
			pxNewTCB->pvThreadLocalStoragePointers[ x ] = NULL;
		}
	}
	#endif

	#if ( configUSE_TASK_NOTIFICATIONS == 1 )
	{
		pxNewTCB->ulNotifiedValue = 0;
		pxNewTCB->ucNotifyState = taskNOT_WAITING_NOTIFICATION;
	}
	#endif

	#if ( configUSE_NEWLIB_REENTRANT == 1 )
	{
		/* 初始化该任务的 Newlib reent 结构。查看第三方链接
		http://www.nadler.com/embedded/newlibAndFreeRTOS.html
		for additional information. */
		_REENT_INIT_PTR( ( &( pxNewTCB->xNewLib_reent ) ) );
	}
	#endif

	#if( INCLUDE_xTaskAbortDelay == 1 )
	{
		pxNewTCB->ucDelayAborted = pdFALSE;
	}
	#endif

	/* 初始化 TCB 堆栈，看起来好像任务
	已经在运行，但已被调度程序中断。
	返回地址设置为任务函数的开头。一旦堆
	栈被初始化，堆栈顶部变量就会被更新。 */
	#if( portUSING_MPU_WRAPPERS == 1 )
	{
		/* 如果端口有能力检测堆栈
		溢出，也将堆栈结束地址
		传递给堆栈初始化函数。 */
		#if( portHAS_STACK_OVERFLOW_CHECKING == 1 )
		{
			#if( portSTACK_GROWTH < 0 )
			{
				pxNewTCB->pxTopOfStack = pxPortInitialiseStack( pxTopOfStack, pxNewTCB->pxStack, pxTaskCode, pvParameters, xRunPrivileged );
			}
			#else /* portSTACK_GROWTH */
			{
				pxNewTCB->pxTopOfStack = pxPortInitialiseStack( pxTopOfStack, pxNewTCB->pxEndOfStack, pxTaskCode, pvParameters, xRunPrivileged );
			}
			#endif /* portSTACK_GROWTH */
		}
		#else /* portHAS_STACK_OVERFLOW_CHECKING */
		{
			pxNewTCB->pxTopOfStack = pxPortInitialiseStack( pxTopOfStack, pxTaskCode, pvParameters, xRunPrivileged );
		}
		#endif /* portHAS_STACK_OVERFLOW_CHECKING */
	}
	#else /* portUSING_MPU_WRAPPERS */
	{
		/* 如果端口有能力检测堆栈
		溢出，也将堆栈结束地址
		传递给堆栈初始化函数。 */
		#if( portHAS_STACK_OVERFLOW_CHECKING == 1 )
		{
			#if( portSTACK_GROWTH < 0 )
			{
				pxNewTCB->pxTopOfStack = pxPortInitialiseStack( pxTopOfStack, pxNewTCB->pxStack, pxTaskCode, pvParameters );
			}
			#else /* portSTACK_GROWTH */
			{
				pxNewTCB->pxTopOfStack = pxPortInitialiseStack( pxTopOfStack, pxNewTCB->pxEndOfStack, pxTaskCode, pvParameters );
			}
			#endif /* portSTACK_GROWTH */
		}
		#else /* portHAS_STACK_OVERFLOW_CHECKING */
		{
			pxNewTCB->pxTopOfStack = pxPortInitialiseStack( pxTopOfStack, pxTaskCode, pvParameters );
		}
		#endif /* portHAS_STACK_OVERFLOW_CHECKING */
	}
	#endif /* portUSING_MPU_WRAPPERS */

	if( pxCreatedTask != NULL )
	{
		/* 以匿名方式传递句柄。  该句柄可用于更改
		创建的任务的优先级、删除创建的任务等。*/
		*pxCreatedTask = ( TaskHandle_t ) pxNewTCB;
	}
	else
	{
		mtCOVERAGE_TEST_MARKER();
	}
}
/*-----------------------------------------------------------*/

static void prvAddNewTaskToReadyList( TCB_t *pxNewTCB )
{
	/* 确保在任务列表正在运行时中断不会访问任务列表
	已更新。 */
	taskENTER_CRITICAL();
	{
		uxCurrentNumberOfTasks++;
		if( pxCurrentTCB == NULL )
		{
			/* 没有其他任务，或者所有其他任务都处
			于挂起状态 - 使其成为当前任务。 */
			pxCurrentTCB = pxNewTCB;

			if( uxCurrentNumberOfTasks == ( UBaseType_t ) 1 )
			{
				/* 这是要创建的第一个任务，因此需要进
				行初步初始化。  如果此调用失败，
				我们将无法恢复，但我们会报告失败。 */
				prvInitialiseTaskLists();
			}
			else
			{
				mtCOVERAGE_TEST_MARKER();
			}
		}
		else
		{
			/* 如果调度程序尚未运行，并且该任
			务是迄今为止要创建的最高优先级
			任务，则将该任务设为当前任务。 */
			if( xSchedulerRunning == pdFALSE )
			{
				if( pxCurrentTCB->uxPriority <= pxNewTCB->uxPriority )
				{
					pxCurrentTCB = pxNewTCB;
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
		}

		uxTaskNumber++;

		#if ( configUSE_TRACE_FACILITY == 1 )
		{
			/* 在 TCB 中添加一个计数器仅用于跟踪。 */
			pxNewTCB->uxTCBNumber = uxTaskNumber;
		}
		#endif /* configUSE_TRACE_FACILITY */
		traceTASK_CREATE( pxNewTCB );

		prvAddTaskToReadyList( pxNewTCB );

		portSETUP_TCB( pxNewTCB );
	}
	taskEXIT_CRITICAL();

	if( xSchedulerRunning != pdFALSE )
	{
		/* 如果创建的任务的优先级高于当
		前任务，那么它应该立即运行。 */
		if( pxCurrentTCB->uxPriority < pxNewTCB->uxPriority )
		{
			taskYIELD_IF_USING_PREEMPTION();
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
}
/*-----------------------------------------------------------*/

#if ( INCLUDE_vTaskDelete == 1 )

	void vTaskDelete( TaskHandle_t xTaskToDelete )
	{
	TCB_t *pxTCB;

		taskENTER_CRITICAL();
		{
			/* 如果此处传入 null，
			则正在删除的是调用任务。 */
			pxTCB = prvGetTCBFromHandle( xTaskToDelete );

			/* 从就绪/延迟列表中删除任务。 */
			if( uxListRemove( &( pxTCB->xStateListItem ) ) == ( UBaseType_t ) 0 )
			{
				taskRESET_READY_PRIORITY( pxTCB->uxPriority );
			}
			else
			{
				mtCOVERAGE_TEST_MARKER();
			}

			/* 该任务也在等待事件吗？ */
			if( listLIST_ITEM_CONTAINER( &( pxTCB->xEventListItem ) ) != NULL )
			{
				( void ) uxListRemove( &( pxTCB->xEventListItem ) );
			}
			else
			{
				mtCOVERAGE_TEST_MARKER();
			}

			/* 还增加 uxTaskNumber，以便内核感知调试器
			可以检测到任务列表需要重新生成。  这是在
			portPRE_TASK_DELETE_HOOK()
			之前完成的，因为在 Windows 端口中该宏不会返回。 */
			uxTaskNumber++;

			if( pxTCB == pxCurrentTCB )
			{
				/* 任务正在删除自身。  这无法在任务本身
				内完成，因为需要上下文切换到另一个任务
				。将任务放入终止列表中。  空闲任务将
				检查终止列表并释放调度程序为 TCB
				和已删除任务的堆栈分配的所有内存。 */
				vListInsertEnd( &xTasksWaitingTermination, &( pxTCB->xStateListItem ) );

				/* 增加 ucTasksDeleted 变量，以便空闲
				任务知道有一个任务已被删除，因此它应该检查
				xTasksWaitingTermination 列表。 */
				++uxDeletedTasksWaitingCleanUp;

				/* 在 portPRE_TASK_DELETE_HOOK() 之前调用删除钩子，因为
				portPRE_TASK_DELETE_HOOK() 在 Win32 端口中不会返回。 */
				traceTASK_DELETE( pxTCB );

				/* 预删除钩子主要用于 Windows 模拟器
				，其中执行 Windows 特定的清理操作
				，之后就不可能放弃此任务 - 因此
				xYieldPending 用于锁定上下文切换
				必需的。 */
				portPRE_TASK_DELETE_HOOK( pxTCB, &xYieldPending );
			}
			else
			{
				--uxCurrentNumberOfTasks;
				traceTASK_DELETE( pxTCB );
				prvDeleteTCB( pxTCB );

				/* 重置下一个预期解锁时间，以
				防它引用刚刚删除的任务。 */
				prvResetNextTaskUnblockTime();
			}
		}
		taskEXIT_CRITICAL();

		/* 如果当前正在运行的任务刚刚
		被删除，则强制重新安排。 */
		if( xSchedulerRunning != pdFALSE )
		{
			if( pxTCB == pxCurrentTCB )
			{
				configASSERT( uxSchedulerSuspended == 0 );
				portYIELD_WITHIN_API();
			}
			else
			{
				mtCOVERAGE_TEST_MARKER();
			}
		}
	}

#endif /* INCLUDE_vTaskDelete */
/*-----------------------------------------------------------*/

#if ( INCLUDE_vTaskDelayUntil == 1 )

	void vTaskDelayUntil( TickType_t * const pxPreviousWakeTime, const TickType_t xTimeIncrement )
	{
	TickType_t xTimeToWake;
	BaseType_t xAlreadyYielded, xShouldDelay = pdFALSE;

		configASSERT( pxPreviousWakeTime );
		configASSERT( ( xTimeIncrement > 0U ) );
		configASSERT( uxSchedulerSuspended == 0 );

		vTaskSuspendAll();
		{
			/* 小幅优化。  在此情况下，滴答数无法更改
			堵塞。 */
			const TickType_t xConstTickCount = xTickCount;

			/* 生成任务想要唤醒的滴答时间。 */
			xTimeToWake = *pxPreviousWakeTime + xTimeIncrement;

			if( xConstTickCount < *pxPreviousWakeTime )
			{
				/* 自上次调用此函数以来，滴答计数已溢出
				。  在这种情况下，我们真正应该延迟
				的唯一时间是唤醒时间也溢出，并且唤醒
				时间大于滴答时间。  当这种情况发生
				时，就好像两个时间都没有溢出一样。 */
				if( ( xTimeToWake < *pxPreviousWakeTime ) && ( xTimeToWake > xConstTickCount ) )
				{
					xShouldDelay = pdTRUE;
				}
				else
				{
					mtCOVERAGE_TEST_MARKER();
				}
			}
			else
			{
				/* 滴答时间尚未溢出。  在这种情况
				下，如果唤醒时间溢出，和/或滴答
				时间小于唤醒时间，我们将延迟。 */
				if( ( xTimeToWake < *pxPreviousWakeTime ) || ( xTimeToWake > xConstTickCount ) )
				{
					xShouldDelay = pdTRUE;
				}
				else
				{
					mtCOVERAGE_TEST_MARKER();
				}
			}

			/* 更新唤醒时间，为下一次通话做好准备。 */
			*pxPreviousWakeTime = xTimeToWake;

			if( xShouldDelay != pdFALSE )
			{
				traceTASK_DELAY_UNTIL( xTimeToWake );

				/* prvAddCurrentTaskToDelayedList()
				需要阻塞时间，而不是唤醒时间，因此减去当前的滴答计数。 */
				prvAddCurrentTaskToDelayedList( xTimeToWake - xConstTickCount, pdFALSE );
			}
			else
			{
				mtCOVERAGE_TEST_MARKER();
			}
		}
		xAlreadyYielded = xTaskResumeAll();

		/* 如果 xTaskResumeAll 尚未这样做，
		则强制重新安排时间，我们可能已经让自己睡着了。 */
		if( xAlreadyYielded == pdFALSE )
		{
			portYIELD_WITHIN_API();
		}
		else
		{
			mtCOVERAGE_TEST_MARKER();
		}
	}

#endif /* INCLUDE_vTaskDelayUntil */
/*-----------------------------------------------------------*/

#if ( INCLUDE_vTaskDelay == 1 )

	void vTaskDelay( const TickType_t xTicksToDelay )
	{
	BaseType_t xAlreadyYielded = pdFALSE;

		/* 零延迟时间只会强制重新安排。 */
		if( xTicksToDelay > ( TickType_t ) 0U )
		{
			configASSERT( uxSchedulerSuspended == 0 );
			vTaskSuspendAll();
			{
				traceTASK_DELAY();

				/* 在调度程序挂起时从事件列表
				中删除的任务将不会被放入就
				绪列表或从阻塞列表中删除，
				直到调度程序恢复为止。

				该任务不能位于事件列表中，因
				为它是当前正在执行的任务。 */
				prvAddCurrentTaskToDelayedList( xTicksToDelay, pdFALSE );
			}
			xAlreadyYielded = xTaskResumeAll();
		}
		else
		{
			mtCOVERAGE_TEST_MARKER();
		}

		/* 如果 xTaskResumeAll 尚未这样做，
		则强制重新安排时间，我们可能已经让自己睡着了。 */
		if( xAlreadyYielded == pdFALSE )
		{
			portYIELD_WITHIN_API();
		}
		else
		{
			mtCOVERAGE_TEST_MARKER();
		}
	}

#endif /* INCLUDE_vTaskDelay */
/*-----------------------------------------------------------*/

#if( ( INCLUDE_eTaskGetState == 1 ) || ( configUSE_TRACE_FACILITY == 1 ) || ( INCLUDE_xTaskAbortDelay == 1 ) )

	eTaskState eTaskGetState( TaskHandle_t xTask )
	{
	eTaskState eReturn;
	List_t const * pxStateList, *pxDelayedList, *pxOverflowedDelayedList;
	const TCB_t * const pxTCB = xTask;

		configASSERT( pxTCB );

		if( pxTCB == pxCurrentTCB )
		{
			/* 调用该函数的任务正在查询自己的状态。 */
			eReturn = eRunning;
		}
		else
		{
			taskENTER_CRITICAL();
			{
				pxStateList = listLIST_ITEM_CONTAINER( &( pxTCB->xStateListItem ) );
				pxDelayedList = pxDelayedTaskList;
				pxOverflowedDelayedList = pxOverflowDelayedTaskList;
			}
			taskEXIT_CRITICAL();

			if( ( pxStateList == pxDelayedList ) || ( pxStateList == pxOverflowedDelayedList ) )
			{
				/* 正在查询的任务是从已阻止的任务之一引用的
				列表。 */
				eReturn = eBlocked;
			}

			#if ( INCLUDE_vTaskSuspend == 1 )
				else if( pxStateList == &xSuspendedTaskList )
				{
					/* 正在查询的任务是从挂起
					列表中引用的。  是真
					的暂停还是无限期封锁？ */
					if( listLIST_ITEM_CONTAINER( &( pxTCB->xEventListItem ) ) == NULL )
					{
						#if( configUSE_TASK_NOTIFICATIONS == 1 )
						{
							/* 该任务不会出现在 RTOS
							对象的事件列表项上，但如果它
							正在等待其通知而不是等待对象
							，则仍可能处于阻塞状态。 */
							if( pxTCB->ucNotifyState == taskWAITING_NOTIFICATION )
							{
								eReturn = eBlocked;
							}
							else
							{
								eReturn = eSuspended;
							}
						}
						#else
						{
							eReturn = eSuspended;
						}
						#endif
					}
					else
					{
						eReturn = eBlocked;
					}
				}
			#endif

			#if ( INCLUDE_vTaskDelete == 1 )
				else if( ( pxStateList == &xTasksWaitingTermination ) || ( pxStateList == NULL ) )
				{
					/* 正在查询的任务是从已删除的任务列表中引用的
					，或者没有从以下位置的任何列表中引用的：
					全部。 */
					eReturn = eDeleted;
				}
			#endif

			else /*lint !e525 负缩进旨在使预处理器的使用更加清晰。 */
			{
				/* 如果任务不处于任何其他状态，则它必须处
				于 Ready（包括挂起就绪）状态。 */
				eReturn = eReady;
			}
		}

		return eReturn;
	} /*lint !e818 xTask 不能是指向 const 的指针，因为它是 typedef。 */

#endif /* INCLUDE_eTaskGetState */
/*-----------------------------------------------------------*/

#if ( INCLUDE_uxTaskPriorityGet == 1 )

	UBaseType_t uxTaskPriorityGet( const TaskHandle_t xTask )
	{
	TCB_t const *pxTCB;
	UBaseType_t uxReturn;

		taskENTER_CRITICAL();
		{
			/* 如果这里传入null，则查询的是调用
			uxTaskPriorityGet()的任务的优先级。 */
			pxTCB = prvGetTCBFromHandle( xTask );
			uxReturn = pxTCB->uxPriority;
		}
		taskEXIT_CRITICAL();

		return uxReturn;
	}

#endif /* INCLUDE_uxTaskPriorityGet */
/*-----------------------------------------------------------*/

#if ( INCLUDE_uxTaskPriorityGet == 1 )

	UBaseType_t uxTaskPriorityGetFromISR( const TaskHandle_t xTask )
	{
	TCB_t const *pxTCB;
	UBaseType_t uxReturn, uxSavedInterruptState;

		/* 支持中断嵌套的 RTOS 端口具有最大系统 call（或最
		大 API 调用）中断优先级的概念。即使 RTOS 内核位
		于临界区，高于最大系统调用优先级的中断也将永久保持启用状态
		，但不能对 FreeRTOS API 函数进行任何调用。
		如果 FreeRTOSConfig.h 中定义了
		configASSERT()，则当从已分配优先级高于配置的
		最大系统调用优先级的中断调用 FreeRTOS API 函
		数时，
		portASSERT_IF_INTERRUPT_PRIORITY_INVALID()
		将导致断言失败。  只能调用以FromISR结尾的FreeRTOS函数
		from interrupts	that have been assigned a priority at or (logically)
		低于最大系统调用中断优先级。  FreeRTOS
		维护一个单独的中断安全 API，以确保中断进入尽可
		能快速、简单。  以下链接提供了更多
		information（尽管特定于 Cortex-M）：
		https://www.freertos.org/RTOS-Cortex-M3-M4.html */
		portASSERT_IF_INTERRUPT_PRIORITY_INVALID();

		uxSavedInterruptState = portSET_INTERRUPT_MASK_FROM_ISR();
		{
			/* 如果这里传入null，那么它就
			是正在查询的调用任务的优先级。 */
			pxTCB = prvGetTCBFromHandle( xTask );
			uxReturn = pxTCB->uxPriority;
		}
		portCLEAR_INTERRUPT_MASK_FROM_ISR( uxSavedInterruptState );

		return uxReturn;
	}

#endif /* INCLUDE_uxTaskPriorityGet */
/*-----------------------------------------------------------*/

#if ( INCLUDE_vTaskPrioritySet == 1 )

	void vTaskPrioritySet( TaskHandle_t xTask, UBaseType_t uxNewPriority )
	{
	TCB_t *pxTCB;
	UBaseType_t uxCurrentBasePriority, uxPriorityUsedOnEntry;
	BaseType_t xYieldRequired = pdFALSE;

		configASSERT( ( uxNewPriority < configMAX_PRIORITIES ) );

		/* 确保新的优先级有效。 */
		if( uxNewPriority >= ( UBaseType_t ) configMAX_PRIORITIES )
		{
			uxNewPriority = ( UBaseType_t ) configMAX_PRIORITIES - ( UBaseType_t ) 1U;
		}
		else
		{
			mtCOVERAGE_TEST_MARKER();
		}

		taskENTER_CRITICAL();
		{
			/* 如果此处传入 null，则正
			在更改的调用任务的优先级。 */
			pxTCB = prvGetTCBFromHandle( xTask );

			traceTASK_PRIORITY_SET( pxTCB, uxNewPriority );

			#if ( configUSE_MUTEXES == 1 )
			{
				uxCurrentBasePriority = pxTCB->uxBasePriority;
			}
			#else
			{
				uxCurrentBasePriority = pxTCB->uxPriority;
			}
			#endif

			if( uxCurrentBasePriority != uxNewPriority )
			{
				/* 优先级改变可能已经准备好比
				调用任务优先级更高的任务。 */
				if( uxNewPriority > uxCurrentBasePriority )
				{
					if( pxTCB != pxCurrentTCB )
					{
						/* 当前正在运行的任务以外的任务
						的优先级正在提高。  优先级
						是否高于正在运行的任务？ */
						if( uxNewPriority >= pxCurrentTCB->uxPriority )
						{
							xYieldRequired = pdTRUE;
						}
						else
						{
							mtCOVERAGE_TEST_MARKER();
						}
					}
					else
					{
						/* 正在提高正在运行的任务的优先级，但
						正在运行的任务必须已经是能够运行的
						最高优先级任务，因此不需要屈服。 */
					}
				}
				else if( pxTCB == pxCurrentTCB )
				{
					/* 降低正在运行的任务的优先级
					意味着现在可能有另一个更高
					优先级的任务准备执行。 */
					xYieldRequired = pdTRUE;
				}
				else
				{
					/* 降低任何其他任务的优先级不需要
					屈服，因为正在运行的任务必须高
					于正在修改的任务的新优先级。 */
				}

				/* 请记住，在更改其 uxPriority 成员之前，
				任务可能会引用就绪列表，以便
				taskRESET_READY_PRIORITY() 宏可以正确运行。 */
				uxPriorityUsedOnEntry = pxTCB->uxPriority;

				#if ( configUSE_MUTEXES == 1 )
				{
					/* 仅当任务当前未使用继承的优先
					级时才更改正在使用的优先级。 */
					if( pxTCB->uxBasePriority == pxTCB->uxPriority )
					{
						pxTCB->uxPriority = uxNewPriority;
					}
					else
					{
						mtCOVERAGE_TEST_MARKER();
					}

					/* 不管怎样，基本优先级都会被设置。 */
					pxTCB->uxBasePriority = uxNewPriority;
				}
				#else
				{
					pxTCB->uxPriority = uxNewPriority;
				}
				#endif

				/* 仅当事件列表项的值未用于
				其他用途时才重置该值。 */
				if( ( listGET_LIST_ITEM_VALUE( &( pxTCB->xEventListItem ) ) & taskEVENT_LIST_ITEM_VALUE_IN_USE ) == 0UL )
				{
					listSET_LIST_ITEM_VALUE( &( pxTCB->xEventListItem ), ( ( TickType_t ) configMAX_PRIORITIES - ( TickType_t ) uxNewPriority ) ); /*lint !e961 MISRA 例外，因为强制转换仅对某些端口是多余的。 */
				}
				else
				{
					mtCOVERAGE_TEST_MARKER();
				}

				/* 如果任务位于阻止或挂起列表中，我们
				只需更改其优先级变量即可。但是，如
				果任务位于就绪列表中，则需要将其删
				除并放入适合其新优先级的列表中。 */
				if( listIS_CONTAINED_WITHIN( &( pxReadyTasksLists[ uxPriorityUsedOnEntry ] ), &( pxTCB->xStateListItem ) ) != pdFALSE )
				{
					/* 该任务当前位于其就绪列表中 - 在将其添加到新
					的就绪列表之前将其删除。  由于我们处于关键部
					分，即使调度程序被挂起，我们也可以这样做。 */
					if( uxListRemove( &( pxTCB->xStateListItem ) ) == ( UBaseType_t ) 0 )
					{
						/* 已知该任务在其就绪列表中
						，因此无需再次检查，可以
						直接调用端口级重置宏。 */
						portRESET_READY_PRIORITY( uxPriorityUsedOnEntry, uxTopReadyPriority );
					}
					else
					{
						mtCOVERAGE_TEST_MARKER();
					}
					prvAddTaskToReadyList( pxTCB );
				}
				else
				{
					mtCOVERAGE_TEST_MARKER();
				}

				if( xYieldRequired != pdFALSE )
				{
					taskYIELD_IF_USING_PREEMPTION();
				}
				else
				{
					mtCOVERAGE_TEST_MARKER();
				}

				/* 当未使用端口优化任务选择时，删
				除有关未使用变量的编译器警告。 */
				( void ) uxPriorityUsedOnEntry;
			}
		}
		taskEXIT_CRITICAL();
	}

#endif /* INCLUDE_vTaskPrioritySet */
/*-----------------------------------------------------------*/

#if ( INCLUDE_vTaskSuspend == 1 )

	void vTaskSuspend( TaskHandle_t xTaskToSuspend )
	{
	TCB_t *pxTCB;

		taskENTER_CRITICAL();
		{
			/* 如果此处传入 null，
			则正在运行的任务被挂起。 */
			pxTCB = prvGetTCBFromHandle( xTaskToSuspend );

			traceTASK_SUSPEND( pxTCB );

			/* 从就绪/延迟列表中删除任
			务并将其放入挂起列表中。 */
			if( uxListRemove( &( pxTCB->xStateListItem ) ) == ( UBaseType_t ) 0 )
			{
				taskRESET_READY_PRIORITY( pxTCB->uxPriority );
			}
			else
			{
				mtCOVERAGE_TEST_MARKER();
			}

			/* 该任务也在等待事件吗？ */
			if( listLIST_ITEM_CONTAINER( &( pxTCB->xEventListItem ) ) != NULL )
			{
				( void ) uxListRemove( &( pxTCB->xEventListItem ) );
			}
			else
			{
				mtCOVERAGE_TEST_MARKER();
			}

			vListInsertEnd( &xSuspendedTaskList, &( pxTCB->xStateListItem ) );

			#if( configUSE_TASK_NOTIFICATIONS == 1 )
			{
				if( pxTCB->ucNotifyState == taskWAITING_NOTIFICATION )
				{
					/* 该任务曾被阻止等待通知，但现
					在已挂起，因此未收到通知。 */
					pxTCB->ucNotifyState = taskNOT_WAITING_NOTIFICATION;
				}
			}
			#endif
		}
		taskEXIT_CRITICAL();

		if( xSchedulerRunning != pdFALSE )
		{
			/* 重置下一个预期解锁时间，以防它
			引用现在处于挂起状态的任务。 */
			taskENTER_CRITICAL();
			{
				prvResetNextTaskUnblockTime();
			}
			taskEXIT_CRITICAL();
		}
		else
		{
			mtCOVERAGE_TEST_MARKER();
		}

		if( pxTCB == pxCurrentTCB )
		{
			if( xSchedulerRunning != pdFALSE )
			{
				/* 目前的任务刚刚被暂停。 */
				configASSERT( uxSchedulerSuspended == 0 );
				portYIELD_WITHIN_API();
			}
			else
			{
				/* 调度程序未运行，但
				pxCurrentTCB 指向的任务刚刚
				挂起，必须调整 pxCurrentTCB 指向不同的任务。 */
				if( listCURRENT_LIST_LENGTH( &xSuspendedTaskList ) == uxCurrentNumberOfTasks ) /*lint !e931 Right没有副作用，只是不稳定。 */
				{
					/* 没有其他任务准备就绪，因此将 pxCurrentTCB
					设置回 NULL，这样当创建下一个任务时，
					pxCurrentTCB 将被设置为指向它，无论其相对优先级是什么
					是。 */
					pxCurrentTCB = NULL;
				}
				else
				{
					vTaskSwitchContext();
				}
			}
		}
		else
		{
			mtCOVERAGE_TEST_MARKER();
		}
	}

#endif /* INCLUDE_vTaskSuspend */
/*-----------------------------------------------------------*/

#if ( INCLUDE_vTaskSuspend == 1 )

	static BaseType_t prvTaskIsTaskSuspended( const TaskHandle_t xTask )
	{
	BaseType_t xReturn = pdFALSE;
	const TCB_t * const pxTCB = xTask;

		/* 访问 xPendingReadyList 因此必须从关键位置调用
		部分。 */

		/* 检查调用任务是否挂起是没有意义的。 */
		configASSERT( xTask );

		/* 正在恢复的任务实际上是在挂起列表中吗？ */
		if( listIS_CONTAINED_WITHIN( &xSuspendedTaskList, &( pxTCB->xStateListItem ) ) != pdFALSE )
		{
			/* 任务是否已从 ISR 内恢复？ */
			if( listIS_CONTAINED_WITHIN( &xPendingReadyList, &( pxTCB->xEventListItem ) ) == pdFALSE )
			{
				/* 它在挂起列表中是因为它处于
				Suspending 状态，还是因为它被阻塞而没有超时？ */
				if( listIS_CONTAINED_WITHIN( NULL, &( pxTCB->xEventListItem ) ) != pdFALSE ) /*lint！e961。  仅当使用 NULL 时，强制转换才是多余的。 */
				{
					xReturn = pdTRUE;
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
		}
		else
		{
			mtCOVERAGE_TEST_MARKER();
		}

		return xReturn;
	} /*lint !e818 xTask 不能是指向 const 的指针，因为它是 typedef。 */

#endif /* INCLUDE_vTaskSuspend */
/*-----------------------------------------------------------*/

#if ( INCLUDE_vTaskSuspend == 1 )

	void vTaskResume( TaskHandle_t xTaskToResume )
	{
	TCB_t * const pxTCB = xTaskToResume;

		/* 恢复调用任务是没有意义的。 */
		configASSERT( xTaskToResume );

		/* 该参数不能为 NULL，因为
		无法恢复当前正在执行的任务。 */
		if( ( pxTCB != pxCurrentTCB ) && ( pxTCB != NULL ) )
		{
			taskENTER_CRITICAL();
			{
				if( prvTaskIsTaskSuspended( pxTCB ) != pdFALSE )
				{
					traceTASK_RESUME( pxTCB );

					/* 即使调度程序被挂起，也可以访问就
					绪列表，因为它位于关键部分内。 */
					( void ) uxListRemove(  &( pxTCB->xStateListItem ) );
					prvAddTaskToReadyList( pxTCB );

					/* 更高优先级的任务可能刚刚恢复。 */
					if( pxTCB->uxPriority >= pxCurrentTCB->uxPriority )
					{
						/* 此让出可能不会导致任务刚刚
						恢复运行，但会将列表保留在
						下一个让出的正确状态。 */
						taskYIELD_IF_USING_PREEMPTION();
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
			}
			taskEXIT_CRITICAL();
		}
		else
		{
			mtCOVERAGE_TEST_MARKER();
		}
	}

#endif /* INCLUDE_vTaskSuspend */

/*-----------------------------------------------------------*/

#if ( ( INCLUDE_xTaskResumeFromISR == 1 ) && ( INCLUDE_vTaskSuspend == 1 ) )

	BaseType_t xTaskResumeFromISR( TaskHandle_t xTaskToResume )
	{
	BaseType_t xYieldRequired = pdFALSE;
	TCB_t * const pxTCB = xTaskToResume;
	UBaseType_t uxSavedInterruptStatus;

		configASSERT( xTaskToResume );

		/* 支持中断嵌套的 RTOS 端口具有最大系统 call（或最
		大 API 调用）中断优先级的概念。即使 RTOS 内核位
		于临界区，高于最大系统调用优先级的中断也将永久保持启用状态
		，但不能对 FreeRTOS API 函数进行任何调用。
		如果 FreeRTOSConfig.h 中定义了
		configASSERT()，则当从已分配优先级高于配置的
		最大系统调用优先级的中断调用 FreeRTOS API 函
		数时，
		portASSERT_IF_INTERRUPT_PRIORITY_INVALID()
		将导致断言失败。  只能调用以FromISR结尾的FreeRTOS函数
		from interrupts	that have been assigned a priority at or (logically)
		低于最大系统调用中断优先级。  FreeRTOS
		维护一个单独的中断安全 API，以确保中断进入尽可
		能快速、简单。  以下链接提供了更多
		information（尽管特定于 Cortex-M）：
		https://www.freertos.org/RTOS-Cortex-M3-M4.html */
		portASSERT_IF_INTERRUPT_PRIORITY_INVALID();

		uxSavedInterruptStatus = portSET_INTERRUPT_MASK_FROM_ISR();
		{
			if( prvTaskIsTaskSuspended( pxTCB ) != pdFALSE )
			{
				traceTASK_RESUME_FROM_ISR( pxTCB );

				/* 检查是否可以访问就绪列表。 */
				if( uxSchedulerSuspended == ( UBaseType_t ) pdFALSE )
				{
					/* 可以访问就绪列表，因此可以将任务
					从挂起列表直接移动到就绪列表。 */
					if( pxTCB->uxPriority >= pxCurrentTCB->uxPriority )
					{
						xYieldRequired = pdTRUE;
					}
					else
					{
						mtCOVERAGE_TEST_MARKER();
					}

					( void ) uxListRemove( &( pxTCB->xStateListItem ) );
					prvAddTaskToReadyList( pxTCB );
				}
				else
				{
					/* 无法访问延迟列表或就绪列表，因此任务将
					保留在挂起就绪列表中，直到调度程序完成
					未暂停。 */
					vListInsertEnd( &( xPendingReadyList ), &( pxTCB->xEventListItem ) );
				}
			}
			else
			{
				mtCOVERAGE_TEST_MARKER();
			}
		}
		portCLEAR_INTERRUPT_MASK_FROM_ISR( uxSavedInterruptStatus );

		return xYieldRequired;
	}

#endif /* ((INCLUDE_xTaskResumeFromISR==1)&&(INCLUDE_vTaskSuspend==1)) */
/*-----------------------------------------------------------*/

void vTaskStartScheduler( void )
{
BaseType_t xReturn;

	/* 添加最低优先级的空闲任务。 */
	#if( configSUPPORT_STATIC_ALLOCATION == 1 )
	{
		StaticTask_t *pxIdleTaskTCBBuffer = NULL;
		StackType_t *pxIdleTaskStackBuffer = NULL;
		uint32_t ulIdleTaskStackSize;

		/* 空闲任务是使用用户提供的 RAM 创建的 -
		获取 RAM 的地址，然后创建空闲任务。 */
		vApplicationGetIdleTaskMemory( &pxIdleTaskTCBBuffer, &pxIdleTaskStackBuffer, &ulIdleTaskStackSize );
		xIdleTaskHandle = xTaskCreateStatic(	prvIdleTask,
												configIDLE_TASK_NAME,
												ulIdleTaskStackSize,
												( void * ) NULL, /*lint！e961。  对于所有编译器来说，强制转换并不是多余的。 */
												portPRIVILEGE_BIT, /* 在 effect ( tskIDLE_PRIORITY | portPRIVILEGE_BIT ) 中，但 tskIDLE_PRIORITY 为零。 */
												pxIdleTaskStackBuffer,
												pxIdleTaskTCBBuffer ); /*lint !e961 MISRA 异常，合理的原因是它不是对所有支持的编译器的冗余显式转换。 */

		if( xIdleTaskHandle != NULL )
		{
			xReturn = pdPASS;
		}
		else
		{
			xReturn = pdFAIL;
		}
	}
	#else
	{
		/* 正在使用动态分配的 RAM 创建空闲任务。 */
		xReturn = xTaskCreate(	prvIdleTask,
								configIDLE_TASK_NAME,
								configMINIMAL_STACK_SIZE,
								( void * ) NULL,
								portPRIVILEGE_BIT, /* 在 effect ( tskIDLE_PRIORITY | portPRIVILEGE_BIT ) 中，但 tskIDLE_PRIORITY 为零。 */
								&xIdleTaskHandle ); /*lint !e961 MISRA 异常，合理的原因是它不是对所有支持的编译器的冗余显式转换。 */
	}
	#endif /* configSUPPORT_STATIC_ALLOCATION */

	#if ( configUSE_TIMERS == 1 )
	{
		if( xReturn == pdPASS )
		{
			xReturn = xTimerCreateTimerTask();
		}
		else
		{
			mtCOVERAGE_TEST_MARKER();
		}
	}
	#endif /* configUSE_TIMERS */

	if( xReturn == pdPASS )
	{
		/* 仅当定义了用户可定义宏
		FREERTOS_TASKS_C_ADDITIONS_INIT()
		时才应调用 freertos_tasks_c_additions_init()，因为这是该函数调用的唯一宏。 */
		#ifdef FREERTOS_TASKS_C_ADDITIONS_INIT
		{
			freertos_tasks_c_additions_init();
		}
		#endif

		/* 此处关闭中断，以确保在调用
		xPortStartScheduler()
		之前或期间不会发生滴答。  创建的任务
		的堆栈包含一个状态字，其中中断已打开，因
		此当第一个任务开始运行时，中断将自动重新启用。 */
		portDISABLE_INTERRUPTS();

		#if ( configUSE_NEWLIB_REENTRANT == 1 )
		{
			/* 将 Newlib 的 _impure_ptr 变量切换为指向特定于首先运行的
			任务的 _reent 结构。查看第三方链接
			http://www.nadler.com/embedded/newlibAndFreeRTOS.html
			for additional information. */
			_impure_ptr = &( pxCurrentTCB->xNewLib_reent );
		}
		#endif /* configUSE_NEWLIB_REENTRANT */

		xNextTaskUnblockTime = portMAX_DELAY;
		xSchedulerRunning = pdTRUE;
		xTickCount = ( TickType_t ) configINITIAL_TICK_COUNT;

		/* 如果定义了
		configGENERATE_RUN_TIME_STATS，则必须
		定义以下宏来配置用于生成运行时间计数器时基的定时器/计数器。
		注意：如果
		configGENERATE_RUN_TIME_STATS 设置为
		0 并且以下行无法构建，请确保您的 FreeRTOSConfig.h 文件中没有定义 portCONFIGURE_TIMER_FOR_RUN_TIME_STATS()。 */
		portCONFIGURE_TIMER_FOR_RUN_TIME_STATS();

		traceTASK_SWITCHED_IN();

		/* 设置计时器滴答是特定于硬件的，
		因此是在便携式接口中设置的。 */
		if( xPortStartScheduler() != pdFALSE )
		{
			/* 不应到达此处，因为调度程序
			正在运行该函数将不会返回。 */
		}
		else
		{
			/* 仅当任务调用 xTaskEndScheduler() 时才应到达此处。 */
		}
	}
	else
	{
		/* 仅当内核无法启动时才会到达此行，
		因为没有足够的 FreeRTOS
		堆来创建空闲任务或计时器任务。 */
		configASSERT( xReturn != errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY );
	}

	/* 如果 INCLUDE_xTaskGetIdleTaskHandle 设置为 0，则防
	止编译器发出警告，这意味着 xIdleTaskHandle 不在其他任何地方使用。 */
	( void ) xIdleTaskHandle;
}
/*-----------------------------------------------------------*/

void vTaskEndScheduler( void )
{
	/* 停止调度程序中断并调用可移植调度程序结束例
	程，以便在必要时可以恢复原始 ISRs。
	端口层必须确保中断使能位处于正确的状态。 */
	portDISABLE_INTERRUPTS();
	xSchedulerRunning = pdFALSE;
	vPortEndScheduler();
}
/*----------------------------------------------------------*/

void vTaskSuspendAll( void )
{
	/* 由于变量的类型为 BaseType_t，因此不需要临界区。  在
	将此问题报告为错误之前，请先阅读 Richard Barry 在
	以下链接中对 FreeRTOS 支持论坛中的帖子的回复！ -
	http://goo.gl/wu4acr */

	/* portSOFRWARE_BARRIER()
	仅针对不表现出实时行为的仿真/仿真端口实现。 */
	portSOFTWARE_BARRIER();

	/* 如果 uxSchedulerSuspended 非零，则调度程序被挂起。
	增量用于允许对 vTaskSuspendAll() 的调用进行嵌套。 */
	++uxSchedulerSuspended;

	/* 强制对端口和优化编译器进行排序，否
	则可能会将上述增量放在其他地方。 */
	portMEMORY_BARRIER();
}
/*----------------------------------------------------------*/

#if ( configUSE_TICKLESS_IDLE != 0 )

	static TickType_t prvGetExpectedIdleTime( void )
	{
	TickType_t xReturn;
	UBaseType_t uxHigherPriorityReadyTasks = pdFALSE;

		/* uxHigherPriorityReadyTasks 处理
		configUSE_PREEMPTION 为 0 的情况，因此
		即使空闲任务处于就绪状态，也可能存在高于空闲优先级任务的任务。
		跑步。 */
		#if( configUSE_PORT_OPTIMISED_TASK_SELECTION == 0 )
		{
			if( uxTopReadyPriority > tskIDLE_PRIORITY )
			{
				uxHigherPriorityReadyTasks = pdTRUE;
			}
		}
		#else
		{
			const UBaseType_t uxLeastSignificantBit = ( UBaseType_t ) 0x01;

			/* 当使用端口优化任务选择时，
			uxTopReadyPriority 变
			量用作位图。  如果设置了最低有效位以外
			的位，则存在优先级高于空闲优先级且处于就
			绪状态的任务。  这照顾了使用协作调度程序的情况。 */
			if( uxTopReadyPriority > uxLeastSignificantBit )
			{
				uxHigherPriorityReadyTasks = pdTRUE;
			}
		}
		#endif

		if( pxCurrentTCB->uxPriority > tskIDLE_PRIORITY )
		{
			xReturn = 0;
		}
		else if( listCURRENT_LIST_LENGTH( &( pxReadyTasksLists[ tskIDLE_PRIORITY ] ) ) > 1 )
		{
			/* 还有其他空闲优先级任务处于就绪状态。
			如果使用时间分片，则必须是下一个滴答中断
			已处理。 */
			xReturn = 0;
		}
		else if( uxHigherPriorityReadyTasks != pdFALSE )
		{
			/* 处于就绪状态的任务的优先级高于空闲优先级
			。  仅当
			configUSE_PREEMPTION 为 0 时才能到达此路径。 */
			xReturn = 0;
		}
		else
		{
			xReturn = xNextTaskUnblockTime - xTickCount;
		}

		return xReturn;
	}

#endif /* configUSE_TICKLESS_IDLE */
/*----------------------------------------------------------*/

BaseType_t xTaskResumeAll( void )
{
TCB_t *pxTCB = NULL;
BaseType_t xAlreadyYielded = pdFALSE;

	/* 如果 uxSchedulerSuspended 为零，则此函数
	与之前对 vTaskSuspendAll() 的调用不匹配。 */
	configASSERT( uxSchedulerSuspended );

	/* 当调度程序挂起时，ISR 可能会导致任务从事件
	列表中删除。  如果是这种情况，则删除的任务将
	被添加到 xPendingReadyList。
	一旦调度程序恢复，就可以安全地将所有挂起的
	就绪任务从该列表移动到适当的就绪列表中。 */
	taskENTER_CRITICAL();
	{
		--uxSchedulerSuspended;

		if( uxSchedulerSuspended == ( UBaseType_t ) pdFALSE )
		{
			if( uxCurrentNumberOfTasks > ( UBaseType_t ) 0U )
			{
				/* 将任何已准备好的任务从待处理
				列表移动到适当的准备列表中。 */
				while( listLIST_IS_EMPTY( &xPendingReadyList ) == pdFALSE )
				{
					pxTCB = listGET_OWNER_OF_HEAD_ENTRY( ( &xPendingReadyList ) ); /*使用 lint !e9079 void * 因为该宏也与定时器和 co-routines 一起使用。  众所周知，对齐是很好的，因为存储和检索的指针类型是相同的。 */
					( void ) uxListRemove( &( pxTCB->xEventListItem ) );
					( void ) uxListRemove( &( pxTCB->xStateListItem ) );
					prvAddTaskToReadyList( pxTCB );

					/* 如果移动的任务的优先级高于
					当前任务，则必须执行让步。 */
					if( pxTCB->uxPriority >= pxCurrentTCB->uxPriority )
					{
						xYieldPending = pdTRUE;
					}
					else
					{
						mtCOVERAGE_TEST_MARKER();
					}
				}

				if( pxTCB != NULL )
				{
					/* 当调度程序挂起时，任务被解锁，这
					可能会阻止重新计算下一个解锁时间
					，在这种情况下，请立即重新计算。
					对于低功耗无滴答实现非常重要
					，这可以防止不必要的低功耗退出
					状态。 */
					prvResetNextTaskUnblockTime();
				}

				/* 如果在调度程序暂停时发生任何刻度，那么现在
				应该处理它们。  这确保了滴答计数不会滑动
				，并且任何延迟的任务都会在正确的时间恢复
				时间。 */
				{
					TickType_t xPendedCounts = xPendedTicks; /* 非易失性副本。 */

					if( xPendedCounts > ( TickType_t ) 0U )
					{
						do
						{
							if( xTaskIncrementTick() != pdFALSE )
							{
								xYieldPending = pdTRUE;
							}
							else
							{
								mtCOVERAGE_TEST_MARKER();
							}
							--xPendedCounts;
						} while( xPendedCounts > ( TickType_t ) 0U );

						xPendedTicks = 0;
					}
					else
					{
						mtCOVERAGE_TEST_MARKER();
					}
				}

				if( xYieldPending != pdFALSE )
				{
					#if( configUSE_PREEMPTION != 0 )
					{
						xAlreadyYielded = pdTRUE;
					}
					#endif
					taskYIELD_IF_USING_PREEMPTION();
				}
				else
				{
					mtCOVERAGE_TEST_MARKER();
				}
			}
		}
		else
		{
			mtCOVERAGE_TEST_MARKER();
		}
	}
	taskEXIT_CRITICAL();

	return xAlreadyYielded;
}
/*-----------------------------------------------------------*/

TickType_t xTaskGetTickCount( void )
{
TickType_t xTicks;

	/* 如果在 16 位处理器上运行，则需要临界区。 */
	portTICK_TYPE_ENTER_CRITICAL();
	{
		xTicks = xTickCount;
	}
	portTICK_TYPE_EXIT_CRITICAL();

	return xTicks;
}
/*-----------------------------------------------------------*/

TickType_t xTaskGetTickCountFromISR( void )
{
TickType_t xReturn;
UBaseType_t uxSavedInterruptStatus;

	/* 支持中断嵌套的 RTOS 端口具有最大系统 call（或最大
	API 调用）中断优先级的概念。  即使 RTOS 内核位于临界
	区，高于最大系统调用优先级的中断也将永久保持启用状态，但无法对
	FreeRTOS API 函数进行任何调用。  如果
	FreeRTOSConfig.h 中定义了
	configASSERT()，则当从已分配优先级高于配置的最大系
	统调用优先级的中断调用 FreeRTOS API 函数时，
	portASSERT_IF_INTERRUPT_PRIORITY_INVALID()
	将导致断言失败。只有以 FromISR 结尾的
	FreeRTOS 函数才能从已在 or（逻辑上）低于最大系统调用
	中断优先级的优先级的中断中调用。  FreeRTOS 维护一个单
	独的中断安全 API，以确保中断进入尽可能快速、简单。以下链接提
	供了更多 information（尽管特定于 Cortex-M）
	：https://www.freertos.org/RTOS-Cortex-M3-M4.html */
	portASSERT_IF_INTERRUPT_PRIORITY_INVALID();

	uxSavedInterruptStatus = portTICK_TYPE_SET_INTERRUPT_MASK_FROM_ISR();
	{
		xReturn = xTickCount;
	}
	portTICK_TYPE_CLEAR_INTERRUPT_MASK_FROM_ISR( uxSavedInterruptStatus );

	return xReturn;
}
/*-----------------------------------------------------------*/

UBaseType_t uxTaskGetNumberOfTasks( void )
{
	/* 不需要临界区，因为变量是类型
	BaseType_t. */
	return uxCurrentNumberOfTasks;
}
/*-----------------------------------------------------------*/

char *pcTaskGetName( TaskHandle_t xTaskToQuery ) /*lint !e971 仅字符串和单个字符允许使用非限定字符类型。 */
{
TCB_t *pxTCB;

	/* 如果此处传入 null，则调用任务的名称为
	询问。 */
	pxTCB = prvGetTCBFromHandle( xTaskToQuery );
	configASSERT( pxTCB );
	return &( pxTCB->pcTaskName[ 0 ] );
}
/*-----------------------------------------------------------*/

#if ( INCLUDE_xTaskGetHandle == 1 )

	static TCB_t *prvSearchForNameWithinSingleList( List_t *pxList, const char pcNameToQuery[] )
	{
	TCB_t *pxNextTCB, *pxFirstTCB, *pxReturn = NULL;
	UBaseType_t x;
	char cNextChar;
	BaseType_t xBreakLoop;

		/* 在调度程序挂起的情况下调用此函数。 */

		if( listCURRENT_LIST_LENGTH( pxList ) > ( UBaseType_t ) 0 )
		{
			listGET_OWNER_OF_NEXT_ENTRY( pxFirstTCB, pxList );  /*使用 lint !e9079 void * 因为该宏也与定时器和 co-routines 一起使用。  众所周知，对齐是很好的，因为存储和检索的指针类型是相同的。 */

			do
			{
				listGET_OWNER_OF_NEXT_ENTRY( pxNextTCB, pxList ); /*使用 lint !e9079 void * 因为该宏也与定时器和 co-routines 一起使用。  众所周知，对齐是很好的，因为存储和检索的指针类型是相同的。 */

				/* 检查名称中的每个字符以查找匹配项或
				不匹配。 */
				xBreakLoop = pdFALSE;
				for( x = ( UBaseType_t ) 0; x < ( UBaseType_t ) configMAX_TASK_NAME_LEN; x++ )
				{
					cNextChar = pxNextTCB->pcTaskName[ x ];

					if( cNextChar != pcNameToQuery[ x ] )
					{
						/* 字符不匹配。 */
						xBreakLoop = pdTRUE;
					}
					else if( cNextChar == ( char ) 0x00 )
					{
						/* 两个字符串都终止，必须已匹配
						成立。 */
						pxReturn = pxNextTCB;
						xBreakLoop = pdTRUE;
					}
					else
					{
						mtCOVERAGE_TEST_MARKER();
					}

					if( xBreakLoop != pdFALSE )
					{
						break;
					}
				}

				if( pxReturn != NULL )
				{
					/* 手柄已找到。 */
					break;
				}

			} while( pxNextTCB != pxFirstTCB );
		}
		else
		{
			mtCOVERAGE_TEST_MARKER();
		}

		return pxReturn;
	}

#endif /* INCLUDE_xTaskGetHandle */
/*-----------------------------------------------------------*/

#if ( INCLUDE_xTaskGetHandle == 1 )

	TaskHandle_t xTaskGetHandle( const char *pcNameToQuery ) /*lint !e971 仅字符串和单个字符允许使用非限定字符类型。 */
	{
	UBaseType_t uxQueue = configMAX_PRIORITIES;
	TCB_t* pxTCB;

		/* 任务名称将被截断为 configMAX_TASK_NAME_LEN - 1 个字节。 */
		configASSERT( strlen( pcNameToQuery ) < configMAX_TASK_NAME_LEN );

		vTaskSuspendAll();
		{
			/* 搜索准备好的列表。 */
			do
			{
				uxQueue--;
				pxTCB = prvSearchForNameWithinSingleList( ( List_t * ) &( pxReadyTasksLists[ uxQueue ] ), pcNameToQuery );

				if( pxTCB != NULL )
				{
					/* 找到了把手。 */
					break;
				}

			} while( uxQueue > ( UBaseType_t ) tskIDLE_PRIORITY ); /*lint !e961 MISRA 例外，因为强制转换仅对某些端口是多余的。 */

			/* 搜索延迟列表。 */
			if( pxTCB == NULL )
			{
				pxTCB = prvSearchForNameWithinSingleList( ( List_t * ) pxDelayedTaskList, pcNameToQuery );
			}

			if( pxTCB == NULL )
			{
				pxTCB = prvSearchForNameWithinSingleList( ( List_t * ) pxOverflowDelayedTaskList, pcNameToQuery );
			}

			#if ( INCLUDE_vTaskSuspend == 1 )
			{
				if( pxTCB == NULL )
				{
					/* 搜索暂停列表。 */
					pxTCB = prvSearchForNameWithinSingleList( &xSuspendedTaskList, pcNameToQuery );
				}
			}
			#endif

			#if( INCLUDE_vTaskDelete == 1 )
			{
				if( pxTCB == NULL )
				{
					/* 搜索已删除的列表。 */
					pxTCB = prvSearchForNameWithinSingleList( &xTasksWaitingTermination, pcNameToQuery );
				}
			}
			#endif
		}
		( void ) xTaskResumeAll();

		return pxTCB;
	}

#endif /* INCLUDE_xTaskGetHandle */
/*-----------------------------------------------------------*/

#if ( configUSE_TRACE_FACILITY == 1 )

	UBaseType_t uxTaskGetSystemState( TaskStatus_t * const pxTaskStatusArray, const UBaseType_t uxArraySize, uint32_t * const pulTotalRunTime )
	{
	UBaseType_t uxTask = 0, uxQueue = configMAX_PRIORITIES;

		vTaskSuspendAll();
		{
			/* 数组中是否有一个空间用于系统中的每个任务？ */
			if( uxArraySize >= uxCurrentNumberOfTasks )
			{
				/* 使用就绪状态中每个任务的信息填充
				TaskStatus_t 结构。 */
				do
				{
					uxQueue--;
					uxTask += prvListTasksWithinSingleList( &( pxTaskStatusArray[ uxTask ] ), &( pxReadyTasksLists[ uxQueue ] ), eReady );

				} while( uxQueue > ( UBaseType_t ) tskIDLE_PRIORITY ); /*lint !e961 MISRA 例外，因为强制转换仅对某些端口是多余的。 */

				/* 使用处于阻塞状态的每个任务的信息填充
				TaskStatus_t 结构。 */
				uxTask += prvListTasksWithinSingleList( &( pxTaskStatusArray[ uxTask ] ), ( List_t * ) pxDelayedTaskList, eBlocked );
				uxTask += prvListTasksWithinSingleList( &( pxTaskStatusArray[ uxTask ] ), ( List_t * ) pxOverflowDelayedTaskList, eBlocked );

				#if( INCLUDE_vTaskDelete == 1 )
				{
					/* 使用已删除但尚未清理的每个任务的信息填
					充 TaskStatus_t 结构。 */
					uxTask += prvListTasksWithinSingleList( &( pxTaskStatusArray[ uxTask ] ), &xTasksWaitingTermination, eDeleted );
				}
				#endif

				#if ( INCLUDE_vTaskSuspend == 1 )
				{
					/* 使用处于挂起状态的每个任务的信息填充
					TaskStatus_t 结构。 */
					uxTask += prvListTasksWithinSingleList( &( pxTaskStatusArray[ uxTask ] ), &xSuspendedTaskList, eSuspended );
				}
				#endif

				#if ( configGENERATE_RUN_TIME_STATS == 1)
				{
					if( pulTotalRunTime != NULL )
					{
						#ifdef portALT_GET_RUN_TIME_COUNTER_VALUE
							portALT_GET_RUN_TIME_COUNTER_VALUE( ( *pulTotalRunTime ) );
						#else
							*pulTotalRunTime = portGET_RUN_TIME_COUNTER_VALUE();
						#endif
					}
				}
				#else
				{
					if( pulTotalRunTime != NULL )
					{
						*pulTotalRunTime = 0;
					}
				}
				#endif
			}
			else
			{
				mtCOVERAGE_TEST_MARKER();
			}
		}
		( void ) xTaskResumeAll();

		return uxTask;
	}

#endif /* configUSE_TRACE_FACILITY */
/*----------------------------------------------------------*/

#if ( INCLUDE_xTaskGetIdleTaskHandle == 1 )

	TaskHandle_t xTaskGetIdleTaskHandle( void )
	{
		/* 如果在调度程序启动之前调用
		xTaskGetIdleTaskHandle()，则 xIdleTaskHandle 将是 NULL。 */
		configASSERT( ( xIdleTaskHandle != NULL ) );
		return xIdleTaskHandle;
	}

#endif /* INCLUDE_xTaskGetIdleTaskHandle */
/*----------------------------------------------------------*/

/* 此条件编译应使用不等于 0，而不是等于 1。这是为
了确保当用户定义的低功耗模式实现需要将
configUSE_TICKLESS_IDLE 设置为除
1. */
#if ( configUSE_TICKLESS_IDLE != 0 )

	void vTaskStepTick( const TickType_t xTicksToJump )
	{
		/* 在蜱虫被抑制一段时间后更正蜱虫
		计数值。  请注意，这不会*为
		每个步进刻度调用刻度钩子函数。 */
		configASSERT( ( xTickCount + xTicksToJump ) <= xNextTaskUnblockTime );
		xTickCount += xTicksToJump;
		traceINCREASE_TICK_COUNT( xTicksToJump );
	}

#endif /* configUSE_TICKLESS_IDLE */
/*----------------------------------------------------------*/

BaseType_t xTaskCatchUpTicks( TickType_t xTicksToCatchUp )
{
BaseType_t xYieldRequired = pdFALSE;

	/* 不得在调度程序挂起的情况下调用，因为该实现依赖于
	xPendedTicks 在 xTaskResumeAll() 中降至 0。 */
	configASSERT( uxSchedulerSuspended == 0 );

	/* 使用 xPendedTicks 模拟调度程序挂起时发生的
	xTicksToCatchUp 刻度数，以便在 xTaskResumeAll() 中执行刻度。 */
	vTaskSuspendAll();
	xPendedTicks += xTicksToCatchUp;
	xYieldRequired = xTaskResumeAll();

	return xYieldRequired;
}
/*----------------------------------------------------------*/

#if ( INCLUDE_xTaskAbortDelay == 1 )

	BaseType_t xTaskAbortDelay( TaskHandle_t xTask )
	{
	TCB_t *pxTCB = xTask;
	BaseType_t xReturn;

		configASSERT( pxTCB );

		vTaskSuspendAll();
		{
			/* 只有当任务实际上处于阻塞状态时，
			才能提前将其从阻塞状态中移除。 */
			if( eTaskGetState( xTask ) == eBlocked )
			{
				xReturn = pdPASS;

				/* 从阻止列表中删除对该任务的引用。
				中断不会触及
				xStateListItem，因为调度程序已挂起。 */
				( void ) uxListRemove( &( pxTCB->xStateListItem ) );

				/* 该任务也在等待事件吗？  如果是
				这样，也将其从事件列表中删除。
				即使调度程序被挂起，中断也可
				以触及事件列表项，因此使用临界区。 */
				taskENTER_CRITICAL();
				{
					if( listLIST_ITEM_CONTAINER( &( pxTCB->xEventListItem ) ) != NULL )
					{
						( void ) uxListRemove( &( pxTCB->xEventListItem ) );

						/* 这让任务知道它已被强制从阻塞状
						态中删除，因此它不应该重新评估
						其阻塞时间，然后再次阻塞。 */
						pxTCB->ucDelayAborted = pdTRUE;
					}
					else
					{
						mtCOVERAGE_TEST_MARKER();
					}
				}
				taskEXIT_CRITICAL();

				/* 将未阻塞的任务放入适当的就绪列表中。 */
				prvAddTaskToReadyList( pxTCB );

				/* 如果关闭抢占，则解除阻塞的任
				务不会导致立即上下文切换。 */
				#if (  configUSE_PREEMPTION == 1 )
				{
					/* 抢占已开启，但只有在未阻塞任务
					的优先级等于或高于当前正在执行
					的任务时才应执行上下文切换。 */
					if( pxTCB->uxPriority > pxCurrentTCB->uxPriority )
					{
						/* 当调度程序未挂起时，挂
						起要执行的yield。 */
						xYieldPending = pdTRUE;
					}
					else
					{
						mtCOVERAGE_TEST_MARKER();
					}
				}
				#endif /* configUSE_PREEMPTION */
			}
			else
			{
				xReturn = pdFAIL;
			}
		}
		( void ) xTaskResumeAll();

		return xReturn;
	}

#endif /* INCLUDE_xTaskAbortDelay */
/*----------------------------------------------------------*/

BaseType_t xTaskIncrementTick( void )
{
TCB_t * pxTCB;
TickType_t xItemValue;
BaseType_t xSwitchRequired = pdFALSE;

	/* 每次发生滴答中断时由可移植层调用
	。增加刻度，然后检查新的刻度值是
	否会导致任何任务被解除阻塞。 */
	traceTASK_INCREMENT_TICK( xTickCount );
	if( uxSchedulerSuspended == ( UBaseType_t ) pdFALSE )
	{
		/* 小幅优化。  在此情况下，滴答数无法更改
		堵塞。 */
		const TickType_t xConstTickCount = xTickCount + ( TickType_t ) 1;

		/* 增加 RTOS 刻度，如果它回绕到
		0，则切换延迟列表和溢出延迟列表。 */
		xTickCount = xConstTickCount;

		if( xConstTickCount == ( TickType_t ) 0U ) /*lint !e774 'if' 并不总是计算为 false，因为它正在寻找溢出。 */
		{
			taskSWITCH_DELAYED_LISTS();
		}
		else
		{
			mtCOVERAGE_TEST_MARKER();
		}

		/* 查看此勾选是否使超时到期。  任务按
		照其唤醒时间的顺序存储在队列中 -
		这意味着一旦找到一个其块时间尚未到期
		的任务，则无需进一步查看列表。 */
		if( xConstTickCount >= xNextTaskUnblockTime )
		{
			for( ;; )
			{
				if( listLIST_IS_EMPTY( pxDelayedTaskList ) != pdFALSE )
				{
					/* 延迟列表为空。  将
					xNextTaskUnblockTime
					设置为最大可能值，因此极不可能
					if( xTickCount >= xNextTaskUnblockTime ) test will pass
					下次通过。 */
					xNextTaskUnblockTime = portMAX_DELAY; /*lint !e961 MISRA 例外，因为强制转换仅对某些端口是多余的。 */
					break;
				}
				else
				{
					/* 延迟列表不为空，获取延迟
					列表头部的项的值。  这
					是延迟列表头部的任务必须
					从阻塞状态中删除的时间。 */
					pxTCB = listGET_OWNER_OF_HEAD_ENTRY( pxDelayedTaskList ); /*使用 lint !e9079 void * 因为该宏也与定时器和 co-routines 一起使用。  众所周知，对齐是很好的，因为存储和检索的指针类型是相同的。 */
					xItemValue = listGET_LIST_ITEM_VALUE( &( pxTCB->xStateListItem ) );

					if( xConstTickCount < xItemValue )
					{
						/* 现在还不是解除该项目阻塞的时间
						，但该项目值是必须将阻塞列表头
						部的任务从阻塞状态中删除的时间
						- 因此将项目值记录在
						xNextTaskUnblockTime. */
						xNextTaskUnblockTime = xItemValue;
						break; /*lint !e9011 这里的代码结构通过多次中断被认为更容易理解。 */
					}
					else
					{
						mtCOVERAGE_TEST_MARKER();
					}

					/* 是时候将该项目从“阻止”状态中删除了。 */
					( void ) uxListRemove( &( pxTCB->xStateListItem ) );

					/* 该任务也在等待事件吗？  如果是
					这样，请将其从事件列表中删除。 */
					if( listLIST_ITEM_CONTAINER( &( pxTCB->xEventListItem ) ) != NULL )
					{
						( void ) uxListRemove( &( pxTCB->xEventListItem ) );
					}
					else
					{
						mtCOVERAGE_TEST_MARKER();
					}

					/* 将未阻塞的任务放入适当的就绪状态
					列表。 */
					prvAddTaskToReadyList( pxTCB );

					/* 如果关闭抢占，则解除阻塞的任
					务不会导致立即上下文切换。 */
					#if (  configUSE_PREEMPTION == 1 )
					{
						/* 抢占已开启，但只有在未
						阻塞任务的优先级等于或
						高于当前正在执行的任务
						时才应执行上下文切换。 */
						if( pxTCB->uxPriority >= pxCurrentTCB->uxPriority )
						{
							xSwitchRequired = pdTRUE;
						}
						else
						{
							mtCOVERAGE_TEST_MARKER();
						}
					}
					#endif /* configUSE_PREEMPTION */
				}
			}
		}

		/* 如果抢占已打开，并且应用程序编写者未显式关闭
		时间切片，则与当前正在运行的任务具有相同优先
		级的任务将共享处理 time（时间切片）。 */
		#if ( ( configUSE_PREEMPTION == 1 ) && ( configUSE_TIME_SLICING == 1 ) )
		{
			if( listCURRENT_LIST_LENGTH( &( pxReadyTasksLists[ pxCurrentTCB->uxPriority ] ) ) > ( UBaseType_t ) 1 )
			{
				xSwitchRequired = pdTRUE;
			}
			else
			{
				mtCOVERAGE_TEST_MARKER();
			}
		}
		#endif /* ((configUSE_PREEMPTION==1)&&(configUSE_TIME_SLICING==1)) */

		#if ( configUSE_TICK_HOOK == 1 )
		{
			/* 当挂起的滴答计数为 unwound（当调
			度程序被解锁时）时，防止调用滴答钩子。 */
			if( xPendedTicks == ( TickType_t ) 0 )
			{
				vApplicationTickHook();
			}
			else
			{
				mtCOVERAGE_TEST_MARKER();
			}
		}
		#endif /* configUSE_TICK_HOOK */

		#if ( configUSE_PREEMPTION == 1 )
		{
			if( xYieldPending != pdFALSE )
			{
				xSwitchRequired = pdTRUE;
			}
			else
			{
				mtCOVERAGE_TEST_MARKER();
			}
		}
		#endif /* configUSE_PREEMPTION */
	}
	else
	{
		++xPendedTicks;

		/* 即使调度程序被锁定，tick
		hook 也会定期被调用。 */
		#if ( configUSE_TICK_HOOK == 1 )
		{
			vApplicationTickHook();
		}
		#endif
	}

	return xSwitchRequired;
}
/*-----------------------------------------------------------*/

#if ( configUSE_APPLICATION_TASK_TAG == 1 )

	void vTaskSetApplicationTaskTag( TaskHandle_t xTask, TaskHookFunction_t pxHookFunction )
	{
	TCB_t *xTCB;

		/* 如果 xTask 是 NULL，则
		它是正在设置的调用任务的任务挂钩。 */
		if( xTask == NULL )
		{
			xTCB = ( TCB_t * ) pxCurrentTCB;
		}
		else
		{
			xTCB = xTask;
		}

		/* 将钩子函数保存在TCB中。  需要一
		个临界区，因为可以从中断访问该值。 */
		taskENTER_CRITICAL();
		{
			xTCB->pxTaskTag = pxHookFunction;
		}
		taskEXIT_CRITICAL();
	}

#endif /* configUSE_APPLICATION_TASK_TAG */
/*-----------------------------------------------------------*/

#if ( configUSE_APPLICATION_TASK_TAG == 1 )

	TaskHookFunction_t xTaskGetApplicationTaskTag( TaskHandle_t xTask )
	{
	TCB_t *pxTCB;
	TaskHookFunction_t xReturn;

		/* 如果 xTask 是 NULL 则设置调用任务的钩子。 */
		pxTCB = prvGetTCBFromHandle( xTask );

		/* 将钩子函数保存在TCB中。  需要一
		个临界区，因为可以从中断访问该值。 */
		taskENTER_CRITICAL();
		{
			xReturn = pxTCB->pxTaskTag;
		}
		taskEXIT_CRITICAL();

		return xReturn;
	}

#endif /* configUSE_APPLICATION_TASK_TAG */
/*-----------------------------------------------------------*/

#if ( configUSE_APPLICATION_TASK_TAG == 1 )

	TaskHookFunction_t xTaskGetApplicationTaskTagFromISR( TaskHandle_t xTask )
	{
	TCB_t *pxTCB;
	TaskHookFunction_t xReturn;
	UBaseType_t uxSavedInterruptStatus;

		/* 如果 xTask 是 NULL 则设置调用任务的钩子。 */
		pxTCB = prvGetTCBFromHandle( xTask );

		/* 将钩子函数保存在TCB中。  需要一
		个临界区，因为可以从中断访问该值。 */
		uxSavedInterruptStatus = portSET_INTERRUPT_MASK_FROM_ISR();
		{
			xReturn = pxTCB->pxTaskTag;
		}
		portCLEAR_INTERRUPT_MASK_FROM_ISR( uxSavedInterruptStatus );

		return xReturn;
	}

#endif /* configUSE_APPLICATION_TASK_TAG */
/*-----------------------------------------------------------*/

#if ( configUSE_APPLICATION_TASK_TAG == 1 )

	BaseType_t xTaskCallApplicationTaskHook( TaskHandle_t xTask, void *pvParameter )
	{
	TCB_t *xTCB;
	BaseType_t xReturn;

		/* 如果 xTask 是 NULL 那么我们正在调用我们自己的任务挂钩。 */
		if( xTask == NULL )
		{
			xTCB = pxCurrentTCB;
		}
		else
		{
			xTCB = xTask;
		}

		if( xTCB->pxTaskTag != NULL )
		{
			xReturn = xTCB->pxTaskTag( pvParameter );
		}
		else
		{
			xReturn = pdFAIL;
		}

		return xReturn;
	}

#endif /* configUSE_APPLICATION_TASK_TAG */
/*-----------------------------------------------------------*/

void vTaskSwitchContext( void )
{
	if( uxSchedulerSuspended != ( UBaseType_t ) pdFALSE )
	{
		/* 调度程序当前已暂停 - 不允许上下文
		转变。 */
		xYieldPending = pdTRUE;
	}
	else
	{
		xYieldPending = pdFALSE;
		traceTASK_SWITCHED_OUT();

		#if ( configGENERATE_RUN_TIME_STATS == 1 )
		{
			#ifdef portALT_GET_RUN_TIME_COUNTER_VALUE
				portALT_GET_RUN_TIME_COUNTER_VALUE( ulTotalRunTime );
			#else
				ulTotalRunTime = portGET_RUN_TIME_COUNTER_VALUE();
			#endif

			/* 将任务已运行的时间与迄今为止的累计时间相
			加。  任务开始运行的时间存储在
			ulTaskSwitchedInTime
			中。  请注意，这里没有溢出保护，因此计
			数值仅在定时器溢出之前有效。  对负值的
			防范是为了防止可疑的运行时统计计数器实现
			——这些实现是由应用程序而不是内核提供的。 */
			if( ulTotalRunTime > ulTaskSwitchedInTime )
			{
				pxCurrentTCB->ulRunTimeCounter += ( ulTotalRunTime - ulTaskSwitchedInTime );
			}
			else
			{
				mtCOVERAGE_TEST_MARKER();
			}
			ulTaskSwitchedInTime = ulTotalRunTime;
		}
		#endif /* configGENERATE_RUN_TIME_STATS */

		/* 检查堆栈溢出（如果已配置）。 */
		taskCHECK_FOR_STACK_OVERFLOW();

		/* 在当前运行的任务被切换之前，保存其errno。 */
		#if( configUSE_POSIX_ERRNO == 1 )
		{
			pxCurrentTCB->iTaskErrno = FreeRTOS_errno;
		}
		#endif

		/* 选择要使用通用 C 或端口优化
		的 asm 代码运行的新任务。 */
		taskSELECT_HIGHEST_PRIORITY_TASK(); /*使用 lint !e9079 void * 因为该宏也与定时器和 co-routines 一起使用。  众所周知，对齐是很好的，因为存储和检索的指针类型是相同的。 */
		traceTASK_SWITCHED_IN();

		/* 新任务切入后，更新全局errno。 */
		#if( configUSE_POSIX_ERRNO == 1 )
		{
			FreeRTOS_errno = pxCurrentTCB->iTaskErrno;
		}
		#endif

		#if ( configUSE_NEWLIB_REENTRANT == 1 )
		{
			/* 将 Newlib 的 _impure_ptr 变量切换为指向特定于此任务的
			_reent 结构。查看第三方链接
			http://www.nadler.com/embedded/newlibAndFreeRTOS.html
			for additional information. */
			_impure_ptr = &( pxCurrentTCB->xNewLib_reent );
		}
		#endif /* configUSE_NEWLIB_REENTRANT */
	}
}
/*-----------------------------------------------------------*/

void vTaskPlaceOnEventList( List_t * const pxEventList, const TickType_t xTicksToWait )
{
	configASSERT( pxEventList );

	/* 必须在禁用任一中断的情况下调用此函数
	或者调度程序暂停并且队列被访问锁定。 */

	/* 将TCB的事件列表项放入相应的事件列表中
	。它按优先级顺序放置在列表中，因此优先级
	最高的任务最先被该事件唤醒。  包含事件
	列表的队列被锁定，以防止中断同时访问。 */
	vListInsert( pxEventList, &( pxCurrentTCB->xEventListItem ) );

	prvAddCurrentTaskToDelayedList( xTicksToWait, pdTRUE );
}
/*-----------------------------------------------------------*/

void vTaskPlaceOnUnorderedEventList( List_t * pxEventList, const TickType_t xItemValue, const TickType_t xTicksToWait )
{
	configASSERT( pxEventList );

	/* 调用此函数时必须使用
	THE SCHEDULER SUSPENDED。  它由事件组实现使用。 */
	configASSERT( uxSchedulerSuspended != 0 );

	/* 将项目值存储在事件列表项中。  在这里
	访问事件列表项是安全的，因为中断不会访
	问不处于阻塞状态的任务的事件列表项。 */
	listSET_LIST_ITEM_VALUE( &( pxCurrentTCB->xEventListItem ), xItemValue | taskEVENT_LIST_ITEM_VALUE_IN_USE );

	/* 将 TCB 的事件列表项放置在相应事件列表
	的末尾。  在这里访问事件列表是安全的，因
	为它是事件组实现的一部分 - 并且中断不会
	访问事件组 directly（而是通过对任
	务级别的挂起函数调用来间接访问它们）。 */
	vListInsertEnd( pxEventList, &( pxCurrentTCB->xEventListItem ) );

	prvAddCurrentTaskToDelayedList( xTicksToWait, pdTRUE );
}
/*-----------------------------------------------------------*/

#if( configUSE_TIMERS == 1 )

	void vTaskPlaceOnEventListRestricted( List_t * const pxEventList, TickType_t xTicksToWait, const BaseType_t xWaitIndefinitely )
	{
		configASSERT( pxEventList );

		/* 该函数不应由应用程序代码调用，因此其名称中包含“
		Restricted”。  它不是公共 API 的
		一部分。  它是为内核代码使用而设计的，并且有特殊
		的调用要求——应该在调度程序挂起的情况下调用它。 */


		/* 将TCB的事件列表项放入相应的事件列表中。在这
		种情况下，假设这是在此事件列表上等待的唯一任务
		，因此可以使用更快的
		vListInsertEnd() 函数来代替 vListInsert。 */
		vListInsertEnd( pxEventList, &( pxCurrentTCB->xEventListItem ) );

		/* 如果任务应无限期阻塞，则将阻塞时间设置为一个值，该
		值将在
		prvAddCurrentTaskToDelayedList() 函数内被识别为无限期延迟。 */
		if( xWaitIndefinitely != pdFALSE )
		{
			xTicksToWait = portMAX_DELAY;
		}

		traceTASK_DELAY_UNTIL( ( xTickCount + xTicksToWait ) );
		prvAddCurrentTaskToDelayedList( xTicksToWait, xWaitIndefinitely );
	}

#endif /* configUSE_TIMERS */
/*-----------------------------------------------------------*/

BaseType_t xTaskRemoveFromEventList( const List_t * const pxEventList )
{
TCB_t *pxUnblockedTCB;
BaseType_t xReturn;

	/* 该函数必须从以下位置调用
	CRITICAL SECTION。  也可以从 ISR 内的关键部分调用它。 */

	/* 事件列表按优先级顺序排序，因此可以删除列表中的
	第一个事件，因为已知它具有最高优先级。  将
	TCB从延迟列表中删除，并将其添加到就绪列表中。

	如果事件针对已锁定的队列，则永远不会调用此
	函数 - 队列上的锁定计数将被修改。  这
	意味着这里可以保证对事件列表的独占访问。

	此函数假设已进行检查以确保
	pxEventList 不为空。 */
	pxUnblockedTCB = listGET_OWNER_OF_HEAD_ENTRY( pxEventList ); /*使用 lint !e9079 void * 因为该宏也与定时器和 co-routines 一起使用。  众所周知，对齐是很好的，因为存储和检索的指针类型是相同的。 */
	configASSERT( pxUnblockedTCB );
	( void ) uxListRemove( &( pxUnblockedTCB->xEventListItem ) );

	if( uxSchedulerSuspended == ( UBaseType_t ) pdFALSE )
	{
		( void ) uxListRemove( &( pxUnblockedTCB->xStateListItem ) );
		prvAddTaskToReadyList( pxUnblockedTCB );

		#if( configUSE_TICKLESS_IDLE != 0 )
		{
			/* 如果任务在内核对象上被阻止，则
			xNextTaskUnblockTime 可能会设置为
			被阻止任务的超时时间。  如果任务由于超时以外的原因被
			解除阻塞，xNextTaskUnblockTime 通
			常保持不变，因为当滴答计数等于
			xNextTaskUnblockTime 时，它会自动
			重置为新值。  然而，如果使用无滴答空闲，尽早进入睡眠
			模式可能更重要 - 因此请在此处重置 xNextTaskUnblockTime 以确保它尽早更新。 */
			prvResetNextTaskUnblockTime();
		}
		#endif
	}
	else
	{
		/* 无法访问延迟列表和就绪列表，因此请将此
		任务保持挂起状态，直到调度程序恢复。 */
		vListInsertEnd( &( xPendingReadyList ), &( pxUnblockedTCB->xEventListItem ) );
	}

	if( pxUnblockedTCB->uxPriority > pxCurrentTCB->uxPriority )
	{
		/* 如果从事件列表中删除的任务比调用任务具有更
		高的优先级，则返回 true。  这允许调
		用任务知道现在是否应该强制进行上下文切换。 */
		xReturn = pdTRUE;

		/* 如果用户未将“xHigherPriorityTaskWoken”
		参数用于 ISR 安全 FreeRTOS 函数，则标记为待处理。 */
		xYieldPending = pdTRUE;
	}
	else
	{
		xReturn = pdFALSE;
	}

	return xReturn;
}
/*-----------------------------------------------------------*/

void vTaskRemoveFromUnorderedEventList( ListItem_t * pxEventListItem, const TickType_t xItemValue )
{
TCB_t *pxUnblockedTCB;

	/* 调用此函数时必须使用
	THE SCHEDULER SUSPENDED。  它由事件标志实现使用。 */
	configASSERT( uxSchedulerSuspended != pdFALSE );

	/* 将新项目值存储在事件列表中。 */
	listSET_LIST_ITEM_VALUE( pxEventListItem, xItemValue | taskEVENT_LIST_ITEM_VALUE_IN_USE );

	/* 从事件标志中删除事件列表。
	中断不访问事件标志。 */
	pxUnblockedTCB = listGET_LIST_ITEM_OWNER( pxEventListItem ); /*使用 lint !e9079 void * 因为该宏也与定时器和 co-routines 一起使用。  众所周知，对齐是很好的，因为存储和检索的指针类型是相同的。 */
	configASSERT( pxUnblockedTCB );
	( void ) uxListRemove( pxEventListItem );

	#if( configUSE_TICKLESS_IDLE != 0 )
	{
		/* 如果任务在内核对象上被阻止，则
		xNextTaskUnblockTime 可能会设置为
		被阻止任务的超时时间。  如果任务由于超时以外的原因被
		解除阻塞，xNextTaskUnblockTime 通
		常保持不变，因为当滴答计数等于
		xNextTaskUnblockTime 时，它会自动
		重置为新值。  然而，如果使用无滴答空闲，尽早进入睡眠
		模式可能更重要 - 因此请在此处重置 xNextTaskUnblockTime 以确保它尽早更新。 */
		prvResetNextTaskUnblockTime();
	}
	#endif

	/* 从延迟列表中删除任务并将其添加到就绪列表中。
	调度程序被挂起，因此中断将不会访问就绪状态
	列表。 */
	( void ) uxListRemove( &( pxUnblockedTCB->xStateListItem ) );
	prvAddTaskToReadyList( pxUnblockedTCB );

	if( pxUnblockedTCB->uxPriority > pxCurrentTCB->uxPriority )
	{
		/* 未阻塞任务的优先级高于调用任务，因此需要上下文切
		换。  在调度程序挂起的情况下调用此函数，因此设
		置 xYieldPending，以便在调度程序为
		resumed（未挂起）时立即发生上下文切换。 */
		xYieldPending = pdTRUE;
	}
}
/*-----------------------------------------------------------*/

void vTaskSetTimeOutState( TimeOut_t * const pxTimeOut )
{
	configASSERT( pxTimeOut );
	taskENTER_CRITICAL();
	{
		pxTimeOut->xOverflowCount = xNumOfOverflows;
		pxTimeOut->xTimeOnEntering = xTickCount;
	}
	taskEXIT_CRITICAL();
}
/*-----------------------------------------------------------*/

void vTaskInternalSetTimeOutState( TimeOut_t * const pxTimeOut )
{
	/* 仅供内部使用，因为它不使用关键部分。 */
	pxTimeOut->xOverflowCount = xNumOfOverflows;
	pxTimeOut->xTimeOnEntering = xTickCount;
}
/*-----------------------------------------------------------*/

BaseType_t xTaskCheckForTimeOut( TimeOut_t * const pxTimeOut, TickType_t * const pxTicksToWait )
{
BaseType_t xReturn;

	configASSERT( pxTimeOut );
	configASSERT( pxTicksToWait );

	taskENTER_CRITICAL();
	{
		/* 小幅优化。  此块中的滴答计数无法更改。 */
		const TickType_t xConstTickCount = xTickCount;
		const TickType_t xElapsedTime = xConstTickCount - pxTimeOut->xTimeOnEntering;

		#if( INCLUDE_xTaskAbortDelay == 1 )
			if( pxCurrentTCB->ucDelayAborted != ( uint8_t ) pdFALSE )
			{
				/* 延迟被中止，这与超时
				不同，但结果相同。 */
				pxCurrentTCB->ucDelayAborted = pdFALSE;
				xReturn = pdTRUE;
			}
			else
		#endif

		#if ( INCLUDE_vTaskSuspend == 1 )
			if( *pxTicksToWait == portMAX_DELAY )
			{
				/* 如果
				INCLUDE_vTaskSuspend 设
				置为 1 并且指定的块时间是最大块时间，则任务应无限期地阻塞，因此永远不会超时。 */
				xReturn = pdFALSE;
			}
			else
		#endif

		if( ( xNumOfOverflows != pxTimeOut->xOverflowCount ) && ( xConstTickCount >= pxTimeOut->xTimeOnEntering ) ) /*lint !e525 首选缩进，因为这样可以使预处理器指令中的代码更清晰。 */
		{
			/* 滴答计数大于调用
			vTaskSetTimeout() 的时间，
			但自调用 vTaskSetTimeOut()
			后也已溢出。  一定是绕了一圈又过去了。自
			调用 vTaskSetTimeout() 以来，这已经过去了。 */
			xReturn = pdTRUE;
		}
		else if( xElapsedTime < *pxTicksToWait ) /*lint !e961 显式转换仅对某些编译器来说是多余的，而其他编译器则需要它来防止整数转换错误。 */
		{
			/* 不是真正的超时。调整剩余时间的参数。 */
			*pxTicksToWait -= xElapsedTime;
			vTaskInternalSetTimeOutState( pxTimeOut );
			xReturn = pdFALSE;
		}
		else
		{
			*pxTicksToWait = 0;
			xReturn = pdTRUE;
		}
	}
	taskEXIT_CRITICAL();

	return xReturn;
}
/*-----------------------------------------------------------*/

void vTaskMissedYield( void )
{
	xYieldPending = pdTRUE;
}
/*-----------------------------------------------------------*/

#if ( configUSE_TRACE_FACILITY == 1 )

	UBaseType_t uxTaskGetTaskNumber( TaskHandle_t xTask )
	{
	UBaseType_t uxReturn;
	TCB_t const *pxTCB;

		if( xTask != NULL )
		{
			pxTCB = xTask;
			uxReturn = pxTCB->uxTaskNumber;
		}
		else
		{
			uxReturn = 0U;
		}

		return uxReturn;
	}

#endif /* configUSE_TRACE_FACILITY */
/*-----------------------------------------------------------*/

#if ( configUSE_TRACE_FACILITY == 1 )

	void vTaskSetTaskNumber( TaskHandle_t xTask, const UBaseType_t uxHandle )
	{
	TCB_t * pxTCB;

		if( xTask != NULL )
		{
			pxTCB = xTask;
			pxTCB->uxTaskNumber = uxHandle;
		}
	}

#endif /* configUSE_TRACE_FACILITY */

/*
 * -----------------------------------------------------------
 * 空闲任务。
 * ----------------------------------------------------------
 *
 * portTASK_FUNCTION() 宏用于允许端
 * 口/编译器特定的语言扩展。  该函数的等效原型是：
 *
 * 无效 prvIdleTask( 无效 *pvParameters );
 *
 */
static portTASK_FUNCTION( prvIdleTask, pvParameters )
{
	/* 停止警告。 */
	( void ) pvParameters;

	/** 这是创建的 RTOS 空闲任务
	当调度程序启动时自动执行。 **/

	/* 如果具有安全上下文的任务删除自身，则空
	闲任务负责删除该任务的安全上下文，如果
	任何。 */
	portALLOCATE_SECURE_CONTEXT( configMINIMAL_SECURE_STACK_SIZE );

	for( ;; )
	{
		/* 查看是否有任何任务已删除自身 - 如果是，则空
		闲任务负责释放已删除任务的 TCB 和堆栈。 */
		prvCheckTasksWaitingTermination();

		#if ( configUSE_PREEMPTION == 0 )
		{
			/* 如果我们不使用抢占，我们会继续强制任
			务切换以查看是否有其他任务可用。
			如果我们使用抢占，则不需要这样做，因
			为任何可用的任务都会自动获得处理器。 */
			taskYIELD();
		}
		#endif /* configUSE_PREEMPTION */

		#if ( ( configUSE_PREEMPTION == 1 ) && ( configIDLE_SHOULD_YIELD == 1 ) )
		{
			/* 当使用抢占时，同等优先级的任务将被分
			时。  如果共享空闲优先级的任务已准
			备好运行，则空闲任务应在结束之前让出
			时间片。

			这里不需要关键区域，因为我们只是从列表
			中读取，偶尔的错误值并不重要。  如果
			处于空闲优先级的就绪列表包含多个任务，
			则空闲任务以外的任务也准备好执行。 */
			if( listCURRENT_LIST_LENGTH( &( pxReadyTasksLists[ tskIDLE_PRIORITY ] ) ) > ( UBaseType_t ) 1 )
			{
				taskYIELD();
			}
			else
			{
				mtCOVERAGE_TEST_MARKER();
			}
		}
		#endif /* ((configUSE_PREEMPTION==1)&&(configIDLE_SHOULD_YIELD==1)) */

		#if ( configUSE_IDLE_HOOK == 1 )
		{
			extern void vApplicationIdleHook( void );

			/* 从空闲任务中调用用户定义的函数。  这允许应用程序设计者
			添加后台功能，而无需单独任务的开销。 注意：
			vApplicationIdleHook() 必须
			不、在任何情况下、
			调用可能会阻塞的函数。 */
			vApplicationIdleHook();
		}
		#endif /* configUSE_IDLE_HOOK */

		/* 此条件编译应使用不等于 0，而不是等于 1。这是为了确保当
		用户定义的低功耗模式实现需要将
		configUSE_TICKLESS_IDLE 设置为 1
		以外的值时调用 portSUPPRESS_TICKS_AND_SLEEP()。 */
		#if ( configUSE_TICKLESS_IDLE != 0 )
		{
		TickType_t xExpectedIdleTime;

			/* 不希望在空闲任务的每次迭代中暂停然
			后恢复调度程序。  因此，在调度器
			不挂起的情况下执行预期空闲时间的初
			步测试。  这里的结果不一定是
			有效的。 */
			xExpectedIdleTime = prvGetExpectedIdleTime();

			if( xExpectedIdleTime >= configEXPECTED_IDLE_TIME_BEFORE_SLEEP )
			{
				vTaskSuspendAll();
				{
					/* 现在调度器被挂起，可以再
					次对预期空闲时间进行采样
					，这次可以使用它的值。 */
					configASSERT( xNextTaskUnblockTime >= xTickCount );
					xExpectedIdleTime = prvGetExpectedIdleTime();

					/* 定义以下宏将xExpectedIdleTime设置为0
					if the application does not want
					portSUPPRESS_TICKS_AND_SLEEP() 被调用。 */
					configPRE_SUPPRESS_TICKS_AND_SLEEP_PROCESSING( xExpectedIdleTime );

					if( xExpectedIdleTime >= configEXPECTED_IDLE_TIME_BEFORE_SLEEP )
					{
						traceLOW_POWER_IDLE_BEGIN();
						portSUPPRESS_TICKS_AND_SLEEP( xExpectedIdleTime );
						traceLOW_POWER_IDLE_END();
					}
					else
					{
						mtCOVERAGE_TEST_MARKER();
					}
				}
				( void ) xTaskResumeAll();
			}
			else
			{
				mtCOVERAGE_TEST_MARKER();
			}
		}
		#endif /* configUSE_TICKLESS_IDLE */
	}
}
/*-----------------------------------------------------------*/

#if( configUSE_TICKLESS_IDLE != 0 )

	eSleepModeStatus eTaskConfirmSleepModeStatus( void )
	{
	/* 除了应用程序任务之外，还存在空闲任务。 */
	const UBaseType_t uxNonApplicationTasks = 1;
	eSleepModeStatus eReturn = eStandardSleep;

		/* 该函数必须从临界区调用。 */

		if( listCURRENT_LIST_LENGTH( &xPendingReadyList ) != 0 )
		{
			/* 当调度程序暂停时，任务已准备就绪。 */
			eReturn = eAbortSleep;
		}
		else if( xYieldPending != pdFALSE )
		{
			/* 当调度程序暂停时，产量处于待定状态。 */
			eReturn = eAbortSleep;
		}
		else
		{
			/* 如果所有任务都处于挂起的 list 中（这可
			能意味着它们有无限的块时间而不是实际挂起），
			那么可以安全地关闭所有时钟并等待外部时钟
			中断。 */
			if( listCURRENT_LIST_LENGTH( &xSuspendedTaskList ) == ( uxCurrentNumberOfTasks - uxNonApplicationTasks ) )
			{
				eReturn = eNoTasksWaitingTimeout;
			}
			else
			{
				mtCOVERAGE_TEST_MARKER();
			}
		}

		return eReturn;
	}

#endif /* configUSE_TICKLESS_IDLE */
/*-----------------------------------------------------------*/

#if ( configNUM_THREAD_LOCAL_STORAGE_POINTERS != 0 )

	void vTaskSetThreadLocalStoragePointer( TaskHandle_t xTaskToSet, BaseType_t xIndex, void *pvValue )
	{
	TCB_t *pxTCB;

		if( xIndex < configNUM_THREAD_LOCAL_STORAGE_POINTERS )
		{
			pxTCB = prvGetTCBFromHandle( xTaskToSet );
			configASSERT( pxTCB != NULL );
			pxTCB->pvThreadLocalStoragePointers[ xIndex ] = pvValue;
		}
	}

#endif /* configNUM_THREAD_LOCAL_STORAGE_POINTERS */
/*-----------------------------------------------------------*/

#if ( configNUM_THREAD_LOCAL_STORAGE_POINTERS != 0 )

	void *pvTaskGetThreadLocalStoragePointer( TaskHandle_t xTaskToQuery, BaseType_t xIndex )
	{
	void *pvReturn = NULL;
	TCB_t *pxTCB;

		if( xIndex < configNUM_THREAD_LOCAL_STORAGE_POINTERS )
		{
			pxTCB = prvGetTCBFromHandle( xTaskToQuery );
			pvReturn = pxTCB->pvThreadLocalStoragePointers[ xIndex ];
		}
		else
		{
			pvReturn = NULL;
		}

		return pvReturn;
	}

#endif /* configNUM_THREAD_LOCAL_STORAGE_POINTERS */
/*-----------------------------------------------------------*/

#if ( portUSING_MPU_WRAPPERS == 1 )

	void vTaskAllocateMPURegions( TaskHandle_t xTaskToModify, const MemoryRegion_t * const xRegions )
	{
	TCB_t *pxTCB;

		/* 如果此处传入 null，则我们正
		在修改调用任务的 MPU 设置。 */
		pxTCB = prvGetTCBFromHandle( xTaskToModify );

		vPortStoreTaskMPUSettings( &( pxTCB->xMPUSettings ), xRegions, NULL, 0 );
	}

#endif /* portUSING_MPU_WRAPPERS */
/*-----------------------------------------------------------*/

static void prvInitialiseTaskLists( void )
{
UBaseType_t uxPriority;

	for( uxPriority = ( UBaseType_t ) 0U; uxPriority < ( UBaseType_t ) configMAX_PRIORITIES; uxPriority++ )
	{
		vListInitialise( &( pxReadyTasksLists[ uxPriority ] ) );
	}

	vListInitialise( &xDelayedTaskList1 );
	vListInitialise( &xDelayedTaskList2 );
	vListInitialise( &xPendingReadyList );

	#if ( INCLUDE_vTaskDelete == 1 )
	{
		vListInitialise( &xTasksWaitingTermination );
	}
	#endif /* INCLUDE_vTaskDelete */

	#if ( INCLUDE_vTaskSuspend == 1 )
	{
		vListInitialise( &xSuspendedTaskList );
	}
	#endif /* INCLUDE_vTaskSuspend */

	/* 从使用 list1 的 pxDelayedTaskList 开始，从使用
	list2 的 pxOverflowDelayedTaskList 开始。 */
	pxDelayedTaskList = &xDelayedTaskList1;
	pxOverflowDelayedTaskList = &xDelayedTaskList2;
}
/*-----------------------------------------------------------*/

static void prvCheckTasksWaitingTermination( void )
{

	/** 该函数从 RTOS 空闲任务调用 **/

	#if ( INCLUDE_vTaskDelete == 1 )
	{
		TCB_t *pxTCB;

		/* uxDeletedTasksWaitingCleanUp is used to prevent taskENTER_CRITICAL()
		在空闲任务中被频繁调用。 */
		while( uxDeletedTasksWaitingCleanUp > ( UBaseType_t ) 0U )
		{
			taskENTER_CRITICAL();
			{
				pxTCB = listGET_OWNER_OF_HEAD_ENTRY( ( &xTasksWaitingTermination ) ); /*使用 lint !e9079 void * 因为该宏也与定时器和 co-routines 一起使用。  众所周知，对齐是很好的，因为存储和检索的指针类型是相同的。 */
				( void ) uxListRemove( &( pxTCB->xStateListItem ) );
				--uxCurrentNumberOfTasks;
				--uxDeletedTasksWaitingCleanUp;
			}
			taskEXIT_CRITICAL();

			prvDeleteTCB( pxTCB );
		}
	}
	#endif /* INCLUDE_vTaskDelete */
}
/*-----------------------------------------------------------*/

#if( configUSE_TRACE_FACILITY == 1 )

	void vTaskGetInfo( TaskHandle_t xTask, TaskStatus_t *pxTaskStatus, BaseType_t xGetFreeStackSpace, eTaskState eState )
	{
	TCB_t *pxTCB;

		/* xTask 为 NULL 然后获取调用任务的状态。 */
		pxTCB = prvGetTCBFromHandle( xTask );

		pxTaskStatus->xHandle = ( TaskHandle_t ) pxTCB;
		pxTaskStatus->pcTaskName = ( const char * ) &( pxTCB->pcTaskName [ 0 ] );
		pxTaskStatus->uxCurrentPriority = pxTCB->uxPriority;
		pxTaskStatus->pxStackBase = pxTCB->pxStack;
		pxTaskStatus->xTaskNumber = pxTCB->uxTCBNumber;

		#if ( configUSE_MUTEXES == 1 )
		{
			pxTaskStatus->uxBasePriority = pxTCB->uxBasePriority;
		}
		#else
		{
			pxTaskStatus->uxBasePriority = 0;
		}
		#endif

		#if ( configGENERATE_RUN_TIME_STATS == 1 )
		{
			pxTaskStatus->ulRunTimeCounter = pxTCB->ulRunTimeCounter;
		}
		#else
		{
			pxTaskStatus->ulRunTimeCounter = 0;
		}
		#endif

		/* 获取任务状态有点繁琐，因此只有当传递到该函数的
		eState 的值是 eInvalid 时才
		能完成 - 否则状态将设置为传入的任何值。 */
		if( eState != eInvalid )
		{
			if( pxTCB == pxCurrentTCB )
			{
				pxTaskStatus->eCurrentState = eRunning;
			}
			else
			{
				pxTaskStatus->eCurrentState = eState;

				#if ( INCLUDE_vTaskSuspend == 1 )
				{
					/* 如果任务位于挂起列表中，那么它实际
					上有可能被无限期地阻止 - 因此实
					际上应该将其报告为处于阻止状态。 */
					if( eState == eSuspended )
					{
						vTaskSuspendAll();
						{
							if( listLIST_ITEM_CONTAINER( &( pxTCB->xEventListItem ) ) != NULL )
							{
								pxTaskStatus->eCurrentState = eBlocked;
							}
						}
						( void ) xTaskResumeAll();
					}
				}
				#endif /* INCLUDE_vTaskSuspend */
			}
		}
		else
		{
			pxTaskStatus->eCurrentState = eTaskGetState( pxTCB );
		}

		/* 获取堆栈空间需要一些时间，因此提供
		xGetFreeStackSpace参数以允许跳过它。 */
		if( xGetFreeStackSpace != pdFALSE )
		{
			#if ( portSTACK_GROWTH > 0 )
			{
				pxTaskStatus->usStackHighWaterMark = prvTaskCheckFreeStackSpace( ( uint8_t * ) pxTCB->pxEndOfStack );
			}
			#else
			{
				pxTaskStatus->usStackHighWaterMark = prvTaskCheckFreeStackSpace( ( uint8_t * ) pxTCB->pxStack );
			}
			#endif
		}
		else
		{
			pxTaskStatus->usStackHighWaterMark = 0;
		}
	}

#endif /* configUSE_TRACE_FACILITY */
/*-----------------------------------------------------------*/

#if ( configUSE_TRACE_FACILITY == 1 )

	static UBaseType_t prvListTasksWithinSingleList( TaskStatus_t *pxTaskStatusArray, List_t *pxList, eTaskState eState )
	{
	configLIST_VOLATILE TCB_t *pxNextTCB, *pxFirstTCB;
	UBaseType_t uxTask = 0;

		if( listCURRENT_LIST_LENGTH( pxList ) > ( UBaseType_t ) 0 )
		{
			listGET_OWNER_OF_NEXT_ENTRY( pxFirstTCB, pxList ); /*使用 lint !e9079 void * 因为该宏也与定时器和 co-routines 一起使用。  众所周知，对齐是很好的，因为存储和检索的指针类型是相同的。 */

			/* 为 pxList 引用的每个任务填充
			pxTaskStatusArray 数组中的
			TaskStatus_t 结构。
			TaskStatus_t结构体各成员的含义参见task.h中TaskStatus_t的定义。 */
			do
			{
				listGET_OWNER_OF_NEXT_ENTRY( pxNextTCB, pxList ); /*使用 lint !e9079 void * 因为该宏也与定时器和 co-routines 一起使用。  众所周知，对齐是很好的，因为存储和检索的指针类型是相同的。 */
				vTaskGetInfo( ( TaskHandle_t ) pxNextTCB, &( pxTaskStatusArray[ uxTask ] ), pdTRUE, eState );
				uxTask++;
			} while( pxNextTCB != pxFirstTCB );
		}
		else
		{
			mtCOVERAGE_TEST_MARKER();
		}

		return uxTask;
	}

#endif /* configUSE_TRACE_FACILITY */
/*-----------------------------------------------------------*/

#if ( ( configUSE_TRACE_FACILITY == 1 ) || ( INCLUDE_uxTaskGetStackHighWaterMark == 1 ) || ( INCLUDE_uxTaskGetStackHighWaterMark2 == 1 ) )

	static configSTACK_DEPTH_TYPE prvTaskCheckFreeStackSpace( const uint8_t * pucStackByte )
	{
	uint32_t ulCount = 0U;

		while( *pucStackByte == ( uint8_t ) tskSTACK_FILL_BYTE )
		{
			pucStackByte -= portSTACK_GROWTH;
			ulCount++;
		}

		ulCount /= ( uint32_t ) sizeof( StackType_t ); /*lint !e961 在较小的架构上，转换并不是多余的。 */

		return ( configSTACK_DEPTH_TYPE ) ulCount;
	}

#endif /* ((configUSE_TRACE_FACILITY==1)||(INCLUDE_uxTaskGetStackHighWaterMark==1)||(INCLUDE_uxTaskGetStackHighWaterMark2==1)) */
/*-----------------------------------------------------------*/

#if ( INCLUDE_uxTaskGetStackHighWaterMark2 == 1 )

	/* uxTaskGetStackHighWaterMark() 和
	uxTaskGetStackHighWaterMark2() 除了
	返回类型不同之外其他相同。  使用
	configSTACK_DEPTH_TYPE 允许用户确定返回类型
	。  它解决了 8 位类型上的值溢出问题，而不会破坏需要 8 位返回类型的应用程序的向后兼容性。 */
	configSTACK_DEPTH_TYPE uxTaskGetStackHighWaterMark2( TaskHandle_t xTask )
	{
	TCB_t *pxTCB;
	uint8_t *pucEndOfStack;
	configSTACK_DEPTH_TYPE uxReturn;

		/* uxTaskGetStackHighWaterMark() 和
		uxTaskGetStackHighWaterMark2() 除了
		返回类型不同之外其他相同。  使用
		configSTACK_DEPTH_TYPE 允许用户确定返回类型
		。  它解决了 8 位类型上的值溢出问题，而不会破坏期望 8 位返回的应用程序的向后兼容性
		类型。 */

		pxTCB = prvGetTCBFromHandle( xTask );

		#if portSTACK_GROWTH < 0
		{
			pucEndOfStack = ( uint8_t * ) pxTCB->pxStack;
		}
		#else
		{
			pucEndOfStack = ( uint8_t * ) pxTCB->pxEndOfStack;
		}
		#endif

		uxReturn = prvTaskCheckFreeStackSpace( pucEndOfStack );

		return uxReturn;
	}

#endif /* INCLUDE_uxTaskGetStackHighWaterMark2 */
/*-----------------------------------------------------------*/

#if ( INCLUDE_uxTaskGetStackHighWaterMark == 1 )

	UBaseType_t uxTaskGetStackHighWaterMark( TaskHandle_t xTask )
	{
	TCB_t *pxTCB;
	uint8_t *pucEndOfStack;
	UBaseType_t uxReturn;

		pxTCB = prvGetTCBFromHandle( xTask );

		#if portSTACK_GROWTH < 0
		{
			pucEndOfStack = ( uint8_t * ) pxTCB->pxStack;
		}
		#else
		{
			pucEndOfStack = ( uint8_t * ) pxTCB->pxEndOfStack;
		}
		#endif

		uxReturn = ( UBaseType_t ) prvTaskCheckFreeStackSpace( pucEndOfStack );

		return uxReturn;
	}

#endif /* INCLUDE_uxTaskGetStackHighWaterMark */
/*-----------------------------------------------------------*/

#if ( INCLUDE_vTaskDelete == 1 )

	static void prvDeleteTCB( TCB_t *pxTCB )
	{
		/* TriCore 端口专门需要此调用。  它必须位于
		vPortFree() 调用之上。  该调用也由
		想要静态分配和清理 RAM 的端口/演示使用。 */
		portCLEAN_UP_TCB( pxTCB );

		/* 释放调度程序为任务分配的内存。  释放在应用程序级别分配的任何内存取
		决于任务。查看第三方链接
		http://www.nadler.com/embedded/newlibAndFreeRTOS.html
		for additional information. */
		#if ( configUSE_NEWLIB_REENTRANT == 1 )
		{
			_reclaim_reent( &( pxTCB->xNewLib_reent ) );
		}
		#endif /* configUSE_NEWLIB_REENTRANT */

		#if( ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) && ( configSUPPORT_STATIC_ALLOCATION == 0 ) && ( portUSING_MPU_WRAPPERS == 0 ) )
		{
			/* 该任务只能动态分配 -
			释放堆栈和 TCB。 */
			vPortFree( pxTCB->pxStack );
			vPortFree( pxTCB );
		}
		#elif( tskSTATIC_AND_DYNAMIC_ALLOCATION_POSSIBLE != 0 ) /*lint !e731 !e9029 出于可读性原因，宏已被合并。 */
		{
			/* 该任务可以静态或动态分配，因此在尝
			试释放任务之前检查静态分配的内容
			记忆。 */
			if( pxTCB->ucStaticallyAllocated == tskDYNAMICALLY_ALLOCATED_STACK_AND_TCB )
			{
				/* 堆栈和 TCB 都是动态
				分配的，因此都必须释放。 */
				vPortFree( pxTCB->pxStack );
				vPortFree( pxTCB );
			}
			else if( pxTCB->ucStaticallyAllocated == tskSTATICALLY_ALLOCATED_STACK_ONLY )
			{
				/* 只有堆栈是静态分配的，因此
				TCB 是唯一必须释放的内存。 */
				vPortFree( pxTCB );
			}
			else
			{
				/* 堆栈和 TCB 都不是动态分配
				的，因此不需要释放任何内容。 */
				configASSERT( pxTCB->ucStaticallyAllocated == tskSTATICALLY_ALLOCATED_STACK_AND_TCB	);
				mtCOVERAGE_TEST_MARKER();
			}
		}
		#endif /* configSUPPORT_DYNAMIC_ALLOCATION */
	}

#endif /* INCLUDE_vTaskDelete */
/*-----------------------------------------------------------*/

static void prvResetNextTaskUnblockTime( void )
{
TCB_t *pxTCB;

	if( listLIST_IS_EMPTY( pxDelayedTaskList ) != pdFALSE )
	{
		/* 新的当前延迟列表为空。  将
		xNextTaskUnblockTime 设置为最大可能值，因此极不可能
		if( xTickCount >= xNextTaskUnblockTime ) test will pass until
		延迟列表中有一个项目。 */
		xNextTaskUnblockTime = portMAX_DELAY;
	}
	else
	{
		/* 新的当前延迟列表不为空，获
		取延迟列表头部的项的值。
		这是延迟列表头部的任务
		应该从阻塞状态中删除的时间。 */
		( pxTCB ) = listGET_OWNER_OF_HEAD_ENTRY( pxDelayedTaskList ); /*使用 lint !e9079 void * 因为该宏也与定时器和 co-routines 一起使用。  众所周知，对齐是很好的，因为存储和检索的指针类型是相同的。 */
		xNextTaskUnblockTime = listGET_LIST_ITEM_VALUE( &( ( pxTCB )->xStateListItem ) );
	}
}
/*-----------------------------------------------------------*/

#if ( ( INCLUDE_xTaskGetCurrentTaskHandle == 1 ) || ( configUSE_MUTEXES == 1 ) )

	TaskHandle_t xTaskGetCurrentTaskHandle( void )
	{
	TaskHandle_t xReturn;

		/* 不需要临界区，因为这不是从中断中
		调用的，并且当前的 TCB 对于
		任何单独的执行线程都将始终相同。 */
		xReturn = pxCurrentTCB;

		return xReturn;
	}

#endif /* ((INCLUDE_xTaskGetCurrentTaskHandle==1)||(configUSE_MUTEXES==1)) */
/*-----------------------------------------------------------*/

#if ( ( INCLUDE_xTaskGetSchedulerState == 1 ) || ( configUSE_TIMERS == 1 ) )

	BaseType_t xTaskGetSchedulerState( void )
	{
	BaseType_t xReturn;

		if( xSchedulerRunning == pdFALSE )
		{
			xReturn = taskSCHEDULER_NOT_STARTED;
		}
		else
		{
			if( uxSchedulerSuspended == ( UBaseType_t ) pdFALSE )
			{
				xReturn = taskSCHEDULER_RUNNING;
			}
			else
			{
				xReturn = taskSCHEDULER_SUSPENDED;
			}
		}

		return xReturn;
	}

#endif /* ((INCLUDE_xTaskGetSchedulerState==1)||(configUSE_TIMERS==1)) */
/*-----------------------------------------------------------*/

#if ( configUSE_MUTEXES == 1 )

	BaseType_t xTaskPriorityInherit( TaskHandle_t const pxMutexHolder )
	{
	TCB_t * const pxMutexHolderTCB = pxMutexHolder;
	BaseType_t xReturn = pdFALSE;

		/* 如果在队列锁定时通过中断返回互斥锁，则互斥
		锁持有者现在可能是 NULL。  _RB_
		由于中断不能再使用互斥体，这仍然需要吗？ */
		if( pxMutexHolder != NULL )
		{
			/* 如果互斥锁持有者的优先级低于尝试获取
			互斥锁的任务的优先级，那么它将暂时继
			承尝试获取互斥锁的任务的优先级。 */
			if( pxMutexHolderTCB->uxPriority < pxCurrentTCB->uxPriority )
			{
				/* 调整互斥锁持有者状态以适应其新
				的优先级。  仅当事件列表项的
				值未用于其他用途时才重置该值。 */
				if( ( listGET_LIST_ITEM_VALUE( &( pxMutexHolderTCB->xEventListItem ) ) & taskEVENT_LIST_ITEM_VALUE_IN_USE ) == 0UL )
				{
					listSET_LIST_ITEM_VALUE( &( pxMutexHolderTCB->xEventListItem ), ( TickType_t ) configMAX_PRIORITIES - ( TickType_t ) pxCurrentTCB->uxPriority ); /*lint !e961 MISRA 例外，因为强制转换仅对某些端口是多余的。 */
				}
				else
				{
					mtCOVERAGE_TEST_MARKER();
				}

				/* 如果正在修改的任务处于就绪状
				态，则需要将其移至新列表中。 */
				if( listIS_CONTAINED_WITHIN( &( pxReadyTasksLists[ pxMutexHolderTCB->uxPriority ] ), &( pxMutexHolderTCB->xStateListItem ) ) != pdFALSE )
				{
					if( uxListRemove( &( pxMutexHolderTCB->xStateListItem ) ) == ( UBaseType_t ) 0 )
					{
						/* 已知该任务在其就绪列表中
						，因此无需再次检查，可以
						直接调用端口级重置宏。 */
						portRESET_READY_PRIORITY( pxMutexHolderTCB->uxPriority, uxTopReadyPriority );
					}
					else
					{
						mtCOVERAGE_TEST_MARKER();
					}

					/* 在移入新列表之前继承优先级。 */
					pxMutexHolderTCB->uxPriority = pxCurrentTCB->uxPriority;
					prvAddTaskToReadyList( pxMutexHolderTCB );
				}
				else
				{
					/* 只需继承优先级即可。 */
					pxMutexHolderTCB->uxPriority = pxCurrentTCB->uxPriority;
				}

				traceTASK_PRIORITY_INHERIT( pxMutexHolderTCB, pxCurrentTCB->uxPriority );

				/* 发生了继承。 */
				xReturn = pdTRUE;
			}
			else
			{
				if( pxMutexHolderTCB->uxBasePriority < pxCurrentTCB->uxPriority )
				{
					/* 互斥锁持有者的基本优先级低于
					尝试获取互斥锁的任务的优先级
					，但互斥锁持有者的当前优先级
					不低于尝试获取互斥锁的任务的
					优先级。因此，互斥锁持有者必
					定已经继承了优先级，但如果情
					况并非如此，继承也会发生。 */
					xReturn = pdTRUE;
				}
				else
				{
					mtCOVERAGE_TEST_MARKER();
				}
			}
		}
		else
		{
			mtCOVERAGE_TEST_MARKER();
		}

		return xReturn;
	}

#endif /* configUSE_MUTEXES */
/*-----------------------------------------------------------*/

#if ( configUSE_MUTEXES == 1 )

	BaseType_t xTaskPriorityDisinherit( TaskHandle_t const pxMutexHolder )
	{
	TCB_t * const pxTCB = pxMutexHolder;
	BaseType_t xReturn = pdFALSE;

		if( pxMutexHolder != NULL )
		{
			/* 如果任务拥有互斥体，则它只能具有继承
			的优先级。如果互斥锁由任务持有，那么
			它不能从中断中给出，如果互斥锁由持有
			任务给出，那么它一定是运行状态任务。 */
			configASSERT( pxTCB == pxCurrentTCB );
			configASSERT( pxTCB->uxMutexesHeld );
			( pxTCB->uxMutexesHeld )--;

			/* 互斥体的持有者是否继承
			了另一个任务的优先级？ */
			if( pxTCB->uxPriority != pxTCB->uxBasePriority )
			{
				/* 仅当没有其他互斥体被持有时才取消继承。 */
				if( pxTCB->uxMutexesHeld == ( UBaseType_t ) 0 )
				{
					/* 如果任务拥有互斥体，则它只能具有继承的
					优先级。  如果互斥锁由任务持有，那么
					它不能从中断中给出，如果互斥锁由持有任
					务给出，那么它一定是运行状态任务。
					从就绪/延迟列表中删除等待任务。 */
					if( uxListRemove( &( pxTCB->xStateListItem ) ) == ( UBaseType_t ) 0 )
					{
						taskRESET_READY_PRIORITY( pxTCB->uxPriority );
					}
					else
					{
						mtCOVERAGE_TEST_MARKER();
					}

					/* 在将任务添加到新的就绪
					列表之前取消优先级。 */
					traceTASK_PRIORITY_DISINHERIT( pxTCB, pxTCB->uxBasePriority );
					pxTCB->uxPriority = pxTCB->uxBasePriority;

					/* 重置事件列表项值。  如果此任务正
					在运行，则它不能用于任何其他目的，
					并且必须运行它才能归还互斥锁。 */
					listSET_LIST_ITEM_VALUE( &( pxTCB->xEventListItem ), ( TickType_t ) configMAX_PRIORITIES - ( TickType_t ) pxTCB->uxPriority ); /*lint !e961 MISRA 例外，因为强制转换仅对某些端口是多余的。 */
					prvAddTaskToReadyList( pxTCB );

					/* 返回 true 表示需要上下文切换
					。实际上，这仅在持有多个互斥体并且
					互斥体以不同于获取互斥体的顺序返回
					的极端情况下才需要。如果返回第一个
					互斥体时没有发生上下文切换，即使有
					任务正在等待它，那么无论是否有任务
					正在等待它，当返回最后一个互斥体时
					都应该发生上下文切换。 */
					xReturn = pdTRUE;
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
		}
		else
		{
			mtCOVERAGE_TEST_MARKER();
		}

		return xReturn;
	}

#endif /* configUSE_MUTEXES */
/*-----------------------------------------------------------*/

#if ( configUSE_MUTEXES == 1 )

	void vTaskPriorityDisinheritAfterTimeout( TaskHandle_t const pxMutexHolder, UBaseType_t uxHighestPriorityWaitingTask )
	{
	TCB_t * const pxTCB = pxMutexHolder;
	UBaseType_t uxPriorityUsedOnEntry, uxPriorityToUse;
	const UBaseType_t uxOnlyOneMutexHeld = ( UBaseType_t ) 1;

		if( pxMutexHolder != NULL )
		{
			/* 如果 pxMutexHolder 不是
			NULL，则持有者必须持有至少一个互斥体。 */
			configASSERT( pxTCB->uxMutexesHeld );

			/* 确定持有互斥体的任务的优先级应设
			置的优先级。  这将是保持任务的
			基本优先级和等待获取互斥体的最高
			优先级任务的优先级中的较大者。 */
			if( pxTCB->uxBasePriority < uxHighestPriorityWaitingTask )
			{
				uxPriorityToUse = uxHighestPriorityWaitingTask;
			}
			else
			{
				uxPriorityToUse = pxTCB->uxBasePriority;
			}

			/* 优先级需要改变吗？ */
			if( pxTCB->uxPriority != uxPriorityToUse )
			{
				/* 仅当没有其他互斥体被持有时才取消继承。
				这是优先级继承实现的简化。  如果
				持有互斥锁的任务也持有其他互斥锁，那么
				其他互斥锁可能会导致优先级继承。 */
				if( pxTCB->uxMutexesHeld == uxOnlyOneMutexHeld )
				{
					/* 如果一个任务因为已经拥有它试
					图获取的互斥锁而超时，那么它
					就不能继承自己的优先级。 */
					configASSERT( pxTCB != pxCurrentTCB );

					/* 取消优先级继承，记住之前的优
					先级，以便于确定主题任务的
					状态。 */
					traceTASK_PRIORITY_DISINHERIT( pxTCB, pxTCB->uxBasePriority );
					uxPriorityUsedOnEntry = pxTCB->uxPriority;
					pxTCB->uxPriority = uxPriorityToUse;

					/* 仅当事件列表项的值未用于
					其他用途时才重置该值。 */
					if( ( listGET_LIST_ITEM_VALUE( &( pxTCB->xEventListItem ) ) & taskEVENT_LIST_ITEM_VALUE_IN_USE ) == 0UL )
					{
						listSET_LIST_ITEM_VALUE( &( pxTCB->xEventListItem ), ( TickType_t ) configMAX_PRIORITIES - ( TickType_t ) uxPriorityToUse ); /*lint !e961 MISRA 例外，因为强制转换仅对某些端口是多余的。 */
					}
					else
					{
						mtCOVERAGE_TEST_MARKER();
					}

					/* 如果正在运行的任务不是持有互斥锁的
					任务，则持有互斥锁的任务可能处于就
					绪、阻塞或挂起状态。  仅当任务处
					于就绪状态时才将其从当前状态列表中
					删除，因为任务的优先级将发生变化，
					并且每个优先级都有一个就绪列表。 */
					if( listIS_CONTAINED_WITHIN( &( pxReadyTasksLists[ uxPriorityUsedOnEntry ] ), &( pxTCB->xStateListItem ) ) != pdFALSE )
					{
						if( uxListRemove( &( pxTCB->xStateListItem ) ) == ( UBaseType_t ) 0 )
						{
							/* 已知该任务在其就绪列表中
							，因此无需再次检查，可以
							直接调用端口级重置宏。 */
							portRESET_READY_PRIORITY( pxTCB->uxPriority, uxTopReadyPriority );
						}
						else
						{
							mtCOVERAGE_TEST_MARKER();
						}

						prvAddTaskToReadyList( pxTCB );
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
	}

#endif /* configUSE_MUTEXES */
/*-----------------------------------------------------------*/

#if ( portCRITICAL_NESTING_IN_TCB == 1 )

	void vTaskEnterCritical( void )
	{
		portDISABLE_INTERRUPTS();

		if( xSchedulerRunning != pdFALSE )
		{
			( pxCurrentTCB->uxCriticalNesting )++;

			/* 这不是进入关键函数的中断安全版本，因此如
			果从中断上下文调用它，则为
			assert()。  只有以“
			FromISR”结尾的 API 函数才能
			在中断中使用。  仅当关键嵌套计数为 1
			时才断言，以防止在断言函数也使用关键部分时发生递归调用。 */
			if( pxCurrentTCB->uxCriticalNesting == 1 )
			{
				portASSERT_IF_IN_ISR();
			}
		}
		else
		{
			mtCOVERAGE_TEST_MARKER();
		}
	}

#endif /* portCRITICAL_NESTING_IN_TCB */
/*-----------------------------------------------------------*/

#if ( portCRITICAL_NESTING_IN_TCB == 1 )

	void vTaskExitCritical( void )
	{
		if( xSchedulerRunning != pdFALSE )
		{
			if( pxCurrentTCB->uxCriticalNesting > 0U )
			{
				( pxCurrentTCB->uxCriticalNesting )--;

				if( pxCurrentTCB->uxCriticalNesting == 0U )
				{
					portENABLE_INTERRUPTS();
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
		}
		else
		{
			mtCOVERAGE_TEST_MARKER();
		}
	}

#endif /* portCRITICAL_NESTING_IN_TCB */
/*-----------------------------------------------------------*/

#if ( ( configUSE_TRACE_FACILITY == 1 ) && ( configUSE_STATS_FORMATTING_FUNCTIONS > 0 ) )

	static char *prvWriteNameToBuffer( char *pcBuffer, const char *pcTaskName )
	{
	size_t x;

		/* 首先复制整个字符串。 */
		strcpy( pcBuffer, pcTaskName );

		/* 用空格填充字符串的末尾，
		以确保打印时各列对齐。 */
		for( x = strlen( pcBuffer ); x < ( size_t ) ( configMAX_TASK_NAME_LEN - 1 ); x++ )
		{
			pcBuffer[ x ] = ' ';
		}

		/* 终止。 */
		pcBuffer[ x ] = ( char ) 0x00;

		/* 返回字符串的新结尾。 */
		return &( pcBuffer[ x ] );
	}

#endif /* ( configUSE_TRACE_FACILITY == 1 ) && ( configUSE_STATS_FORMATTING_FUNCTIONS > 0 ) */
/*-----------------------------------------------------------*/

#if ( ( configUSE_TRACE_FACILITY == 1 ) && ( configUSE_STATS_FORMATTING_FUNCTIONS > 0 ) && ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) )

	void vTaskList( char * pcWriteBuffer )
	{
	TaskStatus_t *pxTaskStatusArray;
	UBaseType_t uxArraySize, x;
	char cStatus;

		/*
		 * 请注意：
		 *
		 * 提供此函数只是为了方便，许多演示应用
		 * 程序都使用该函数。  不要将其视为
		 * 调度程序。
		 *
		 * vTaskList() 调用
		 * uxTaskGetSystemState()，然后将
		 * uxTaskGetSystemState() 输出的一部分格式化为人类可读的表，该表显示任务名称、状态和堆栈使用情况。
		 *
		 * vTaskList() 依赖于 sprintf() C
		 * 库函数，该函数可能会使代码大小膨胀、使用大量堆栈并在
		 * 不同平台上提供不同的结果。  sprintf() 的替
		 * 代、小型、第三方和有限功能实现在许多
		 * FreeRTOS/Demo 子目录中的一个名为
		 * printf-stdarg.c 的文件中提供（注意
		 * printf-stdarg.c 不提供完整的 snprintf() 实现！）。
		 *
		 * It is recommended that production systems call uxTaskGetSystemState()
		 * 直接访问原始统计数据，而不是通过调用
		 * vTaskList() 间接访问。
		 */


		/* 确保写入缓冲区不包含字符串。 */
		*pcWriteBuffer = ( char ) 0x00;

		/* 拍摄任务数量的快照，以防该函
		数执行时任务数量发生变化。 */
		uxArraySize = uxCurrentNumberOfTasks;

		/* 为每个任务分配一个数组索引。  NOTE！  如果
		configSUPPORT_DYNAMIC_ALLOCATION
		设置为 0，则 pvPortMalloc() 将等于 NULL。 */
		pxTaskStatusArray = pvPortMalloc( uxCurrentNumberOfTasks * sizeof( TaskStatus_t ) ); /*lint !e9079 pvPortMalloc() 返回的所有值至少具有 MCU 堆栈所需的对齐方式，并且此分配分配一个具有指针对齐要求的结构体。 */

		if( pxTaskStatusArray != NULL )
		{
			/* 生成 the（二进制）数据。 */
			uxArraySize = uxTaskGetSystemState( pxTaskStatusArray, uxArraySize, NULL );

			/* 从二进制数据创建一个人类可读的表。 */
			for( x = 0; x < uxArraySize; x++ )
			{
				switch( pxTaskStatusArray[ x ].eCurrentState )
				{
					case eRunning:		cStatus = tskRUNNING_CHAR;
										break;

					case eReady:		cStatus = tskREADY_CHAR;
										break;

					case eBlocked:		cStatus = tskBLOCKED_CHAR;
										break;

					case eSuspended:	cStatus = tskSUSPENDED_CHAR;
										break;

					case eDeleted:		cStatus = tskDELETED_CHAR;
										break;

					case eInvalid:		/* 跌倒了。 */
					default:			/* 不应该到达这里，但包含它
										是为了防止静态检查错误。 */
										cStatus = ( char ) 0x00;
										break;
				}

				/* 将任务名称写入字符串，并用空格填
				充，以便更容易以表格形式打印。 */
				pcWriteBuffer = prvWriteNameToBuffer( pcWriteBuffer, pxTaskStatusArray[ x ].pcTaskName );

				/* 写出字符串的其余部分。 */
				sprintf( pcWriteBuffer, "\t%c\t%u\t%u\t%u\r\n", cStatus, ( unsigned int ) pxTaskStatusArray[ x ].uxCurrentPriority, ( unsigned int ) pxTaskStatusArray[ x ].usStackHighWaterMark, ( unsigned int ) pxTaskStatusArray[ x ].xTaskNumber ); /*lint !e586 sprintf() 允许，因为它是用许多编译器编译的，并且这只是一个实用函数 - 不是核心内核实现的一部分。 */
				pcWriteBuffer += strlen( pcWriteBuffer ); /*lint !e9016 char 指针上的指针算术可以，特别是在这种情况下，它最好地表示了代码的意图。 */
			}

			/* 再次释放阵列。  NOTE！  如果
			configSUPPORT_DYNAMIC_ALLOCATION 为 0，则 vPortFree() 将被#define 为空。 */
			vPortFree( pxTaskStatusArray );
		}
		else
		{
			mtCOVERAGE_TEST_MARKER();
		}
	}

#endif /* ((configUSE_TRACE_FACILITY==1)&&(configUSE_STATS_FORMATTING_FUNCTIONS>0)&&(configSUPPORT_DYNAMIC_ALLOCATION==1)) */
/*----------------------------------------------------------*/

#if ( ( configGENERATE_RUN_TIME_STATS == 1 ) && ( configUSE_STATS_FORMATTING_FUNCTIONS > 0 ) && ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) )

	void vTaskGetRunTimeStats( char *pcWriteBuffer )
	{
	TaskStatus_t *pxTaskStatusArray;
	UBaseType_t uxArraySize, x;
	uint32_t ulTotalTime, ulStatsAsPercentage;

		#if( configUSE_TRACE_FACILITY != 1 )
		{
			#error configUSE_TRACE_FACILITY must also be set to 1 in FreeRTOSConfig.h to use vTaskGetRunTimeStats().
		}
		#endif

		/*
		 * 请注意：
		 *
		 * 提供此函数只是为了方便，许多演示应用
		 * 程序都使用该函数。  不要将其视为
		 * 调度程序。
		 *
		 * vTaskGetRunTimeStats() 调用
		 * uxTaskGetSystemState()，然后将
		 * uxTaskGetSystemState() 输出的一部分格式化
		 * 为人类可读的表格，该表格以绝对值和百分比形式显示每个任务在“运行”状态下花费的时间量。
		 *
		 * vTaskGetRunTimeStats() 依赖于
		 * sprintf() C 库函数，该函数可能会使代码大小膨胀
		 * 、使用大量堆栈并在不同平台上提供不同的结果。
		 * sprintf() 的替代、小型、第三方和有限功能实现在许
		 * 多 FreeRTOS/Demo 子目录中的一个名为
		 * printf-stdarg.c 的文件中提供（注意
		 * printf-stdarg.c 不提供完整的 snprintf() 实现！）。
		 *
		 * It is recommended that production systems call uxTaskGetSystemState()
		 * 直接访问原始统计数据，而不是通过调用
		 * vTaskGetRunTimeStats() 间接访问。
		 */

		/* 确保写入缓冲区不包含字符串。 */
		*pcWriteBuffer = ( char ) 0x00;

		/* 拍摄任务数量的快照，以防该函
		数执行时任务数量发生变化。 */
		uxArraySize = uxCurrentNumberOfTasks;

		/* 为每个任务分配一个数组索引。  NOTE！  如果
		configSUPPORT_DYNAMIC_ALLOCATION
		设置为 0，则 pvPortMalloc() 将等于 NULL。 */
		pxTaskStatusArray = pvPortMalloc( uxCurrentNumberOfTasks * sizeof( TaskStatus_t ) ); /*lint !e9079 pvPortMalloc() 返回的所有值至少具有 MCU 堆栈所需的对齐方式，并且此分配分配一个具有指针对齐要求的结构体。 */

		if( pxTaskStatusArray != NULL )
		{
			/* 生成 the（二进制）数据。 */
			uxArraySize = uxTaskGetSystemState( pxTaskStatusArray, uxArraySize, &ulTotalTime );

			/* 用于百分比计算。 */
			ulTotalTime /= 100UL;

			/* 避免除以零的错误。 */
			if( ulTotalTime > 0UL )
			{
				/* 从二进制数据创建一个人类可读的表。 */
				for( x = 0; x < uxArraySize; x++ )
				{
					/* 该任务使用了总运行时间的百分比是多少？这将始
					终向下舍入到最接近的整数。
					ulTotalRunTimeDiv100 已被除以 100。 */
					ulStatsAsPercentage = pxTaskStatusArray[ x ].ulRunTimeCounter / ulTotalTime;

					/* 将任务名称写入字符串，用空格填
					充，以便可以以表格形式打印更多
					容易地。 */
					pcWriteBuffer = prvWriteNameToBuffer( pcWriteBuffer, pxTaskStatusArray[ x ].pcTaskName );

					if( ulStatsAsPercentage > 0UL )
					{
						#ifdef portLU_PRINTF_SPECIFIER_REQUIRED
						{
							sprintf( pcWriteBuffer, "\t%lu\t\t%lu%%\r\n", pxTaskStatusArray[ x ].ulRunTimeCounter, ulStatsAsPercentage );
						}
						#else
						{
							/* sizeof( int ) == sizeof(
							long ) 因此可以使用较小的 printf() 库。 */
							sprintf( pcWriteBuffer, "\t%u\t\t%u%%\r\n", ( unsigned int ) pxTaskStatusArray[ x ].ulRunTimeCounter, ( unsigned int ) ulStatsAsPercentage ); /*lint !e586 sprintf() 允许，因为它是用许多编译器编译的，并且这只是一个实用函数 - 不是核心内核实现的一部分。 */
						}
						#endif
					}
					else
					{
						/* 如果此处的百分比为零，则任务消耗
						的总运行时间少于 1% of。 */
						#ifdef portLU_PRINTF_SPECIFIER_REQUIRED
						{
							sprintf( pcWriteBuffer, "\t%lu\t\t<1%%\r\n", pxTaskStatusArray[ x ].ulRunTimeCounter );
						}
						#else
						{
							/* sizeof( int ) == sizeof(
							long ) 因此可以使用较小的 printf() 库。 */
							sprintf( pcWriteBuffer, "\t%u\t\t<1%%\r\n", ( unsigned int ) pxTaskStatusArray[ x ].ulRunTimeCounter ); /*lint !e586 sprintf() 允许，因为它是用许多编译器编译的，并且这只是一个实用函数 - 不是核心内核实现的一部分。 */
						}
						#endif
					}

					pcWriteBuffer += strlen( pcWriteBuffer ); /*lint !e9016 char 指针上的指针算术可以，特别是在这种情况下，它最好地表示了代码的意图。 */
				}
			}
			else
			{
				mtCOVERAGE_TEST_MARKER();
			}

			/* 再次释放阵列。  NOTE！  如果
			configSUPPORT_DYNAMIC_ALLOCATION 为 0，则 vPortFree() 将被#define 为空。 */
			vPortFree( pxTaskStatusArray );
		}
		else
		{
			mtCOVERAGE_TEST_MARKER();
		}
	}

#endif /* ((configGENERATE_RUN_TIME_STATS==1)&&(configUSE_STATS_FORMATTING_FUNCTIONS>0)&&(configSUPPORT_STATIC_ALLOCATION==1)) */
/*-----------------------------------------------------------*/

TickType_t uxTaskResetEventItemValue( void )
{
TickType_t uxReturn;

	uxReturn = listGET_LIST_ITEM_VALUE( &( pxCurrentTCB->xEventListItem ) );

	/* 将事件列表项重置为其正常值 - 因
	此它可以与队列和信号量一起使用。 */
	listSET_LIST_ITEM_VALUE( &( pxCurrentTCB->xEventListItem ), ( ( TickType_t ) configMAX_PRIORITIES - ( TickType_t ) pxCurrentTCB->uxPriority ) ); /*lint !e961 MISRA 例外，因为强制转换仅对某些端口是多余的。 */

	return uxReturn;
}
/*-----------------------------------------------------------*/

#if ( configUSE_MUTEXES == 1 )

	TaskHandle_t pvTaskIncrementMutexHeldCount( void )
	{
		/* 如果在创建任何任务之前调用
		xSemaphoreCreateMutex()，则 pxCurrentTCB 将是 NULL。 */
		if( pxCurrentTCB != NULL )
		{
			( pxCurrentTCB->uxMutexesHeld )++;
		}

		return pxCurrentTCB;
	}

#endif /* configUSE_MUTEXES */
/*-----------------------------------------------------------*/

#if( configUSE_TASK_NOTIFICATIONS == 1 )

	uint32_t ulTaskNotifyTake( BaseType_t xClearCountOnExit, TickType_t xTicksToWait )
	{
	uint32_t ulReturn;

		taskENTER_CRITICAL();
		{
			/* 仅当通知计数尚未非零时才阻止。 */
			if( pxCurrentTCB->ulNotifiedValue == 0UL )
			{
				/* 将此任务标记为等待通知。 */
				pxCurrentTCB->ucNotifyState = taskWAITING_NOTIFICATION;

				if( xTicksToWait > ( TickType_t ) 0 )
				{
					prvAddCurrentTaskToDelayedList( xTicksToWait, pdTRUE );
					traceTASK_NOTIFY_TAKE_BLOCK();

					/* 所有端口都被写入以允许关键
					section 中的屈服（有些将立
					即屈服，其他则等到关键部分退出）
					- 但这不是应用程序代码应该做的事情。 */
					portYIELD_WITHIN_API();
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
		}
		taskEXIT_CRITICAL();

		taskENTER_CRITICAL();
		{
			traceTASK_NOTIFY_TAKE();
			ulReturn = pxCurrentTCB->ulNotifiedValue;

			if( ulReturn != 0UL )
			{
				if( xClearCountOnExit != pdFALSE )
				{
					pxCurrentTCB->ulNotifiedValue = 0UL;
				}
				else
				{
					pxCurrentTCB->ulNotifiedValue = ulReturn - ( uint32_t ) 1;
				}
			}
			else
			{
				mtCOVERAGE_TEST_MARKER();
			}

			pxCurrentTCB->ucNotifyState = taskNOT_WAITING_NOTIFICATION;
		}
		taskEXIT_CRITICAL();

		return ulReturn;
	}

#endif /* configUSE_TASK_NOTIFICATIONS */
/*-----------------------------------------------------------*/

#if( configUSE_TASK_NOTIFICATIONS == 1 )

	BaseType_t xTaskNotifyWait( uint32_t ulBitsToClearOnEntry, uint32_t ulBitsToClearOnExit, uint32_t *pulNotificationValue, TickType_t xTicksToWait )
	{
	BaseType_t xReturn;

		taskENTER_CRITICAL();
		{
			/* 仅当通知尚未挂起时才阻止。 */
			if( pxCurrentTCB->ucNotifyState != taskNOTIFICATION_RECEIVED )
			{
				/* 清除任务通知值中的位，因为
				通知任务或中断可能会设置位
				。  这可用于将值清零。 */
				pxCurrentTCB->ulNotifiedValue &= ~ulBitsToClearOnEntry;

				/* 将此任务标记为等待通知。 */
				pxCurrentTCB->ucNotifyState = taskWAITING_NOTIFICATION;

				if( xTicksToWait > ( TickType_t ) 0 )
				{
					prvAddCurrentTaskToDelayedList( xTicksToWait, pdTRUE );
					traceTASK_NOTIFY_WAIT_BLOCK();

					/* 所有端口都被写入以允许关键
					section 中的屈服（有些将立
					即屈服，其他则等到关键部分退出）
					- 但这不是应用程序代码应该做的事情。 */
					portYIELD_WITHIN_API();
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
		}
		taskEXIT_CRITICAL();

		taskENTER_CRITICAL();
		{
			traceTASK_NOTIFY_WAIT();

			if( pulNotificationValue != NULL )
			{
				/* 输出当前的通知值，该值可
				能已更改，也可能未更改。 */
				*pulNotificationValue = pxCurrentTCB->ulNotifiedValue;
			}

			/* 如果设置了 ucNotifyValue
			，则任务要么从未进入阻止的 state
			（因为通知已挂起），要么任务因通知而解
			除阻止。  否则，任务会因超时而解锁。 */
			if( pxCurrentTCB->ucNotifyState != taskNOTIFICATION_RECEIVED )
			{
				/* 没有收到通知。 */
				xReturn = pdFALSE;
			}
			else
			{
				/* 通知已待处理或在任
				务等待时收到通知。 */
				pxCurrentTCB->ulNotifiedValue &= ~ulBitsToClearOnExit;
				xReturn = pdTRUE;
			}

			pxCurrentTCB->ucNotifyState = taskNOT_WAITING_NOTIFICATION;
		}
		taskEXIT_CRITICAL();

		return xReturn;
	}

#endif /* configUSE_TASK_NOTIFICATIONS */
/*-----------------------------------------------------------*/

#if( configUSE_TASK_NOTIFICATIONS == 1 )

	BaseType_t xTaskGenericNotify( TaskHandle_t xTaskToNotify, uint32_t ulValue, eNotifyAction eAction, uint32_t *pulPreviousNotificationValue )
	{
	TCB_t * pxTCB;
	BaseType_t xReturn = pdPASS;
	uint8_t ucOriginalNotifyState;

		configASSERT( xTaskToNotify );
		pxTCB = xTaskToNotify;

		taskENTER_CRITICAL();
		{
			if( pulPreviousNotificationValue != NULL )
			{
				*pulPreviousNotificationValue = pxTCB->ulNotifiedValue;
			}

			ucOriginalNotifyState = pxTCB->ucNotifyState;

			pxTCB->ucNotifyState = taskNOTIFICATION_RECEIVED;

			switch( eAction )
			{
				case eSetBits	:
					pxTCB->ulNotifiedValue |= ulValue;
					break;

				case eIncrement	:
					( pxTCB->ulNotifiedValue )++;
					break;

				case eSetValueWithOverwrite	:
					pxTCB->ulNotifiedValue = ulValue;
					break;

				case eSetValueWithoutOverwrite :
					if( ucOriginalNotifyState != taskNOTIFICATION_RECEIVED )
					{
						pxTCB->ulNotifiedValue = ulValue;
					}
					else
					{
						/* 无法将该值写入任务。 */
						xReturn = pdFAIL;
					}
					break;

				case eNoAction:
					/* 正在通知该任务，但其通知值不存在
					已更新。 */
					break;

				default:
					/* 如果处理了所有枚举，则不应到达此
					处。通过测试编译器无法假定为
					const 的值来人为地强制断言。 */
					configASSERT( pxTCB->ulNotifiedValue == ~0UL );

					break;
			}

			traceTASK_NOTIFY();

			/* 如果任务处于阻塞状态是为了等
			待通知，那么现在就解除阻塞。 */
			if( ucOriginalNotifyState == taskWAITING_NOTIFICATION )
			{
				( void ) uxListRemove( &( pxTCB->xStateListItem ) );
				prvAddTaskToReadyList( pxTCB );

				/* 该任务不应出现在事件列表中。 */
				configASSERT( listLIST_ITEM_CONTAINER( &( pxTCB->xEventListItem ) ) == NULL );

				#if( configUSE_TICKLESS_IDLE != 0 )
				{
					/* 如果任务被阻止等待通知，则
					xNextTaskUnblockTime 可
					能会设置为被阻止任务的超时时间。  如果任务
					由于超时以外的原因被解除阻塞，
					xNextTaskUnblockTime 通
					常保持不变，因为当滴答计数等于
					xNextTaskUnblockTime 时
					，它将自动重置为新值。  然而，如果使用无滴
					答空闲，尽早进入睡眠模式可能更重要 - 因此
					请在此处重置 xNextTaskUnblockTime 以确保它尽早更新。 */
					prvResetNextTaskUnblockTime();
				}
				#endif

				if( pxTCB->uxPriority > pxCurrentTCB->uxPriority )
				{
					/* 通知的任务的优先级高于当前正
					在执行的任务，因此需要让出。 */
					taskYIELD_IF_USING_PREEMPTION();
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
		}
		taskEXIT_CRITICAL();

		return xReturn;
	}

#endif /* configUSE_TASK_NOTIFICATIONS */
/*-----------------------------------------------------------*/

#if( configUSE_TASK_NOTIFICATIONS == 1 )

	BaseType_t xTaskGenericNotifyFromISR( TaskHandle_t xTaskToNotify, uint32_t ulValue, eNotifyAction eAction, uint32_t *pulPreviousNotificationValue, BaseType_t *pxHigherPriorityTaskWoken )
	{
	TCB_t * pxTCB;
	uint8_t ucOriginalNotifyState;
	BaseType_t xReturn = pdPASS;
	UBaseType_t uxSavedInterruptStatus;

		configASSERT( xTaskToNotify );

		/* 支持中断嵌套的 RTOS 端口具有最大系统 call（或最
		大 API 调用）中断优先级的概念。即使 RTOS 内核位
		于临界区，高于最大系统调用优先级的中断也将永久保持启用状态
		，但不能对 FreeRTOS API 函数进行任何调用。
		如果 FreeRTOSConfig.h 中定义了
		configASSERT()，则当从已分配优先级高于配置的
		最大系统调用优先级的中断调用 FreeRTOS API 函
		数时，
		portASSERT_IF_INTERRUPT_PRIORITY_INVALID()
		将导致断言失败。  只能调用以FromISR结尾的FreeRTOS函数
		from interrupts	that have been assigned a priority at or (logically)
		低于最大系统调用中断优先级。  FreeRTOS
		维护一个单独的中断安全 API，以确保中断进入尽可
		能快速、简单。  以下链接提供了更多
		information（尽管特定于 Cortex-M）：
		http://www.freertos.org/RTOS-Cortex-M3-M4.html */
		portASSERT_IF_INTERRUPT_PRIORITY_INVALID();

		pxTCB = xTaskToNotify;

		uxSavedInterruptStatus = portSET_INTERRUPT_MASK_FROM_ISR();
		{
			if( pulPreviousNotificationValue != NULL )
			{
				*pulPreviousNotificationValue = pxTCB->ulNotifiedValue;
			}

			ucOriginalNotifyState = pxTCB->ucNotifyState;
			pxTCB->ucNotifyState = taskNOTIFICATION_RECEIVED;

			switch( eAction )
			{
				case eSetBits	:
					pxTCB->ulNotifiedValue |= ulValue;
					break;

				case eIncrement	:
					( pxTCB->ulNotifiedValue )++;
					break;

				case eSetValueWithOverwrite	:
					pxTCB->ulNotifiedValue = ulValue;
					break;

				case eSetValueWithoutOverwrite :
					if( ucOriginalNotifyState != taskNOTIFICATION_RECEIVED )
					{
						pxTCB->ulNotifiedValue = ulValue;
					}
					else
					{
						/* 无法将该值写入任务。 */
						xReturn = pdFAIL;
					}
					break;

				case eNoAction :
					/* 正在通知该任务，但其通知值不存在
					已更新。 */
					break;

				default:
					/* 如果处理了所有枚举，则不应到达此
					处。通过测试编译器无法假定为
					const 的值来人为地强制断言。 */
					configASSERT( pxTCB->ulNotifiedValue == ~0UL );
					break;
			}

			traceTASK_NOTIFY_FROM_ISR();

			/* 如果任务处于阻塞状态是为了等
			待通知，那么现在就解除阻塞。 */
			if( ucOriginalNotifyState == taskWAITING_NOTIFICATION )
			{
				/* 该任务不应出现在事件列表中。 */
				configASSERT( listLIST_ITEM_CONTAINER( &( pxTCB->xEventListItem ) ) == NULL );

				if( uxSchedulerSuspended == ( UBaseType_t ) pdFALSE )
				{
					( void ) uxListRemove( &( pxTCB->xStateListItem ) );
					prvAddTaskToReadyList( pxTCB );
				}
				else
				{
					/* 无法访问延迟列表和就绪列表，因此请将此
					任务保持挂起状态，直到调度程序恢复。 */
					vListInsertEnd( &( xPendingReadyList ), &( pxTCB->xEventListItem ) );
				}

				if( pxTCB->uxPriority > pxCurrentTCB->uxPriority )
				{
					/* 通知的任务的优先级高于当前正
					在执行的任务，因此需要让出。 */
					if( pxHigherPriorityTaskWoken != NULL )
					{
						*pxHigherPriorityTaskWoken = pdTRUE;
					}

					/* 如果用户未将“
					xHigherPriorityTaskWoken
					”参数用于 ISR 安全 FreeRTOS 函数，则标记为待处理。 */
					xYieldPending = pdTRUE;
				}
				else
				{
					mtCOVERAGE_TEST_MARKER();
				}
			}
		}
		portCLEAR_INTERRUPT_MASK_FROM_ISR( uxSavedInterruptStatus );

		return xReturn;
	}

#endif /* configUSE_TASK_NOTIFICATIONS */
/*-----------------------------------------------------------*/

#if( configUSE_TASK_NOTIFICATIONS == 1 )

	void vTaskNotifyGiveFromISR( TaskHandle_t xTaskToNotify, BaseType_t *pxHigherPriorityTaskWoken )
	{
	TCB_t * pxTCB;
	uint8_t ucOriginalNotifyState;
	UBaseType_t uxSavedInterruptStatus;

		configASSERT( xTaskToNotify );

		/* 支持中断嵌套的 RTOS 端口具有最大系统 call（或最
		大 API 调用）中断优先级的概念。即使 RTOS 内核位
		于临界区，高于最大系统调用优先级的中断也将永久保持启用状态
		，但不能对 FreeRTOS API 函数进行任何调用。
		如果 FreeRTOSConfig.h 中定义了
		configASSERT()，则当从已分配优先级高于配置的
		最大系统调用优先级的中断调用 FreeRTOS API 函
		数时，
		portASSERT_IF_INTERRUPT_PRIORITY_INVALID()
		将导致断言失败。  只能调用以FromISR结尾的FreeRTOS函数
		from interrupts	that have been assigned a priority at or (logically)
		低于最大系统调用中断优先级。  FreeRTOS
		维护一个单独的中断安全 API，以确保中断进入尽可
		能快速、简单。  以下链接提供了更多
		information（尽管特定于 Cortex-M）：
		http://www.freertos.org/RTOS-Cortex-M3-M4.html */
		portASSERT_IF_INTERRUPT_PRIORITY_INVALID();

		pxTCB = xTaskToNotify;

		uxSavedInterruptStatus = portSET_INTERRUPT_MASK_FROM_ISR();
		{
			ucOriginalNotifyState = pxTCB->ucNotifyState;
			pxTCB->ucNotifyState = taskNOTIFICATION_RECEIVED;

			/* “给予”相当于在计数中增加计数
			信号。 */
			( pxTCB->ulNotifiedValue )++;

			traceTASK_NOTIFY_GIVE_FROM_ISR();

			/* 如果任务处于阻塞状态是为了等
			待通知，那么现在就解除阻塞。 */
			if( ucOriginalNotifyState == taskWAITING_NOTIFICATION )
			{
				/* 该任务不应出现在事件列表中。 */
				configASSERT( listLIST_ITEM_CONTAINER( &( pxTCB->xEventListItem ) ) == NULL );

				if( uxSchedulerSuspended == ( UBaseType_t ) pdFALSE )
				{
					( void ) uxListRemove( &( pxTCB->xStateListItem ) );
					prvAddTaskToReadyList( pxTCB );
				}
				else
				{
					/* 无法访问延迟列表和就绪列表，因此请将此
					任务保持挂起状态，直到调度程序恢复。 */
					vListInsertEnd( &( xPendingReadyList ), &( pxTCB->xEventListItem ) );
				}

				if( pxTCB->uxPriority > pxCurrentTCB->uxPriority )
				{
					/* 通知的任务的优先级高于当前正
					在执行的任务，因此需要让出。 */
					if( pxHigherPriorityTaskWoken != NULL )
					{
						*pxHigherPriorityTaskWoken = pdTRUE;
					}

					/* 如果用户未在 ISR 安全 FreeRTOS
					函数中使用“
					xHigherPriorityTaskWoken”参数，则标记收益率待定。 */
					xYieldPending = pdTRUE;
				}
				else
				{
					mtCOVERAGE_TEST_MARKER();
				}
			}
		}
		portCLEAR_INTERRUPT_MASK_FROM_ISR( uxSavedInterruptStatus );
	}

#endif /* configUSE_TASK_NOTIFICATIONS */
/*-----------------------------------------------------------*/

#if( configUSE_TASK_NOTIFICATIONS == 1 )

	BaseType_t xTaskNotifyStateClear( TaskHandle_t xTask )
	{
	TCB_t *pxTCB;
	BaseType_t xReturn;

		/* 如果此处传入 null，则调
		用任务正在清除其通知状态。 */
		pxTCB = prvGetTCBFromHandle( xTask );

		taskENTER_CRITICAL();
		{
			if( pxTCB->ucNotifyState == taskNOTIFICATION_RECEIVED )
			{
				pxTCB->ucNotifyState = taskNOT_WAITING_NOTIFICATION;
				xReturn = pdPASS;
			}
			else
			{
				xReturn = pdFAIL;
			}
		}
		taskEXIT_CRITICAL();

		return xReturn;
	}

#endif /* configUSE_TASK_NOTIFICATIONS */
/*-----------------------------------------------------------*/

#if( configUSE_TASK_NOTIFICATIONS == 1 )

	uint32_t ulTaskNotifyValueClear( TaskHandle_t xTask, uint32_t ulBitsToClear )
	{
	TCB_t *pxTCB;
	uint32_t ulReturn;

		/* 如果此处传入 null，则调
		用任务正在清除其通知状态。 */
		pxTCB = prvGetTCBFromHandle( xTask );

		taskENTER_CRITICAL();
		{
			/* 返回清除位之前的通知
			，然后清除位掩码。 */
			ulReturn = pxCurrentTCB->ulNotifiedValue;
			pxTCB->ulNotifiedValue &= ~ulBitsToClear;
		}
		taskEXIT_CRITICAL();

		return ulReturn;
	}

#endif /* configUSE_TASK_NOTIFICATIONS */
/*-----------------------------------------------------------*/

#if( ( configGENERATE_RUN_TIME_STATS == 1 ) && ( INCLUDE_xTaskGetIdleTaskHandle == 1 ) )

	uint32_t ulTaskGetIdleRunTimeCounter( void )
	{
		return xIdleTaskHandle->ulRunTimeCounter;
	}

#endif
/*-----------------------------------------------------------*/

static void prvAddCurrentTaskToDelayedList( TickType_t xTicksToWait, const BaseType_t xCanBlockIndefinitely )
{
TickType_t xTimeToWake;
const TickType_t xConstTickCount = xTickCount;

	#if( INCLUDE_xTaskAbortDelay == 1 )
	{
		/* 即将进入延迟列表，因此请确保
		ucDelayAborted 标志重置为
		pdFALSE，以便当任务离开阻塞状态时可以检测到它已设置为 pdTRUE。 */
		pxCurrentTCB->ucDelayAborted = pdFALSE;
	}
	#endif

	/* 在将任务添加到阻止列表之前，将其从就绪列
	表中删除，因为两个列表使用相同的列表项。 */
	if( uxListRemove( &( pxCurrentTCB->xStateListItem ) ) == ( UBaseType_t ) 0 )
	{
		/* 当前任务必须在就绪列表中，所以不需
		要检查，直接调用端口复位宏即可。 */
		portRESET_READY_PRIORITY( pxCurrentTCB->uxPriority, uxTopReadyPriority ); /*lint !e931 pxCurrentTCB 无法更改，因为它是调用任务。  pxCurrentTCB->uxPriority 和 uxTopReadyPriority 不能在调度程序挂起或处于关键部分时进行更改。 */
	}
	else
	{
		mtCOVERAGE_TEST_MARKER();
	}

	#if ( INCLUDE_vTaskSuspend == 1 )
	{
		if( ( xTicksToWait == portMAX_DELAY ) && ( xCanBlockIndefinitely != pdFALSE ) )
		{
			/* 将任务添加到挂起任务列表而不是延迟任务列表中
			，以确保它不会被定时事件唤醒。  它会阻塞
			无限期地。 */
			vListInsertEnd( &xSuspendedTaskList, &( pxCurrentTCB->xStateListItem ) );
		}
		else
		{
			/* 计算如果事件没有发生，任务应该被
			唤醒的时间。  这可能会溢出，但
			这并不重要，内核会正确地管理它。 */
			xTimeToWake = xConstTickCount + xTicksToWait;

			/* 列表项将按唤醒时间顺序插入。 */
			listSET_LIST_ITEM_VALUE( &( pxCurrentTCB->xStateListItem ), xTimeToWake );

			if( xTimeToWake < xConstTickCount )
			{
				/* 起床时间已满。  将此项目放置在溢出中
				列表。 */
				vListInsert( pxOverflowDelayedTaskList, &( pxCurrentTCB->xStateListItem ) );
			}
			else
			{
				/* 唤醒时间尚未溢出，因
				此使用当前的块列表。 */
				vListInsert( pxDelayedTaskList, &( pxCurrentTCB->xStateListItem ) );

				/* 如果进入阻塞状态的任务被放置在阻塞任
				务列表的头部，则
				xNextTaskUnblockTime 也需要更新。 */
				if( xTimeToWake < xNextTaskUnblockTime )
				{
					xNextTaskUnblockTime = xTimeToWake;
				}
				else
				{
					mtCOVERAGE_TEST_MARKER();
				}
			}
		}
	}
	#else /* INCLUDE_vTaskSuspend */
	{
		/* 计算如果事件没有发生，任务应该被
		唤醒的时间。  这可能会溢出，但
		这并不重要，内核会正确地管理它。 */
		xTimeToWake = xConstTickCount + xTicksToWait;

		/* 列表项将按唤醒时间顺序插入。 */
		listSET_LIST_ITEM_VALUE( &( pxCurrentTCB->xStateListItem ), xTimeToWake );

		if( xTimeToWake < xConstTickCount )
		{
			/* 起床时间已满。  将此项目放入溢出列表中。 */
			vListInsert( pxOverflowDelayedTaskList, &( pxCurrentTCB->xStateListItem ) );
		}
		else
		{
			/* 唤醒时间尚未溢出，因此使用当前的块列表。 */
			vListInsert( pxDelayedTaskList, &( pxCurrentTCB->xStateListItem ) );

			/* 如果进入阻塞状态的任务被放置在阻塞任务列表的头部，则
			需要更新 xNextTaskUnblockTime
			也。 */
			if( xTimeToWake < xNextTaskUnblockTime )
			{
				xNextTaskUnblockTime = xTimeToWake;
			}
			else
			{
				mtCOVERAGE_TEST_MARKER();
			}
		}

		/* 当 INCLUDE_vTaskSuspend 不为 1 时避免编译器警告。 */
		( void ) xCanBlockIndefinitely;
	}
	#endif /* INCLUDE_vTaskSuspend */
}

/* 下面的代码允许将其他代码插入到此源文件中，特
别是在对文件范围函数和数据的访问为
needed 的情况下（例如，在执行模块测试时）。 */

#ifdef FREERTOS_MODULE_TEST
	#include "tasks_test_access_functions.h"
#endif


#if( configINCLUDE_FREERTOS_TASK_C_ADDITIONS_H == 1 )

	#include "freertos_tasks_c_additions.h"

	#ifdef FREERTOS_TASKS_C_ADDITIONS_INIT
		static void freertos_tasks_c_additions_init( void )
		{
			FREERTOS_TASKS_C_ADDITIONS_INIT();
		}
	#endif

#endif

