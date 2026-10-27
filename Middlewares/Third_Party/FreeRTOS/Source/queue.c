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

#include <stdlib.h>
#include <string.h>

/* 定义
MPU_WRAPPERS_INCLUDED_FROM_API_FILE
可防止 task.h 重新定义所有 API 函数以使用 MPU 包装器。  仅当应用程序文件包含 task.h 时才应执行此操作。 */
#define MPU_WRAPPERS_INCLUDED_FROM_API_FILE

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#if ( configUSE_CO_ROUTINES == 1 )
	#include "croutine.h"
#endif

/* Lint e9021、e961 和 e750 被抑制为 MISRA 异常，因为 MPU
端口需要定义 MPU_WRAPPERS_INCLUDED_FROM_API_FILE
为生成正确的特权与非特权链接和代码布局，上述头文件中需要定义该宏，
正确的特权与非特权链接和放置。 */
#undef MPU_WRAPPERS_INCLUDED_FROM_API_FILE /*lint！e961！e750！e9021。 */


/* 与 cRxLock 和 cTxLock 结构成员一起使用的常量。 */
#define queueUNLOCKED					( ( int8_t ) -1 )
#define queueLOCKED_UNMODIFIED			( ( int8_t ) 0 )

/* 当 Queue_t 结构用于表示基本队列时，其
pcHead 和 pcTail 成员用作指向队列存储区域的
指针。  当 Queue_t 结构用于表示互斥量
pcHead 和 pcTail 指针时，不需要，并且
pcHead 指针设置为 NULL 以指示该结构改为保存指
向互斥量 holder（如果有）的指针。  将替代名称映射
到 pcHead 和结构成员，以确保保持代码的可读性。
QueuePointers_t 和
SemaphoreData_t 类型用于形成联合，因为它们的用法是互斥的，具体取决于队列的用途。 */
#define uxQueueType						pcHead
#define queueQUEUE_IS_MUTEX				NULL

typedef struct QueuePointers
{
	int8_t *pcTail;					/*< 指向队列存储区末尾的字节。  一旦分配的字节多于存储队列项所需的字节，则将其用作标记。 */
	int8_t *pcReadFrom;				/*< 指向当结构用作队列时最后读取队列项的位置。 */
} QueuePointers_t;

typedef struct SemaphoreData
{
	TaskHandle_t xMutexHolder;		 /*< 持有互斥体的任务句柄。 */
	UBaseType_t uxRecursiveCallCount;/*< 当该结构用作互斥体时，维护递归互斥体被递归“采用”的次数。 */
} SemaphoreData_t;

/* 信号量实际上并不存储或复制数据，因此项目大小为
零。 */
#define queueSEMAPHORE_QUEUE_ITEM_LENGTH ( ( UBaseType_t ) 0 )
#define queueMUTEX_GIVE_BLOCK_TIME		 ( ( TickType_t ) 0U )

#if( configUSE_PREEMPTION == 0 )
	/* 如果正在使用协作调度程序，则不应仅仅因
	为已唤醒更高优先级的任务而执行让步。 */
	#define queueYIELD_IF_USING_PREEMPTION()
#else
	#define queueYIELD_IF_USING_PREEMPTION() portYIELD_WITHIN_API()
#endif

/*
 * 调度程序使用的队列的定义。项目按副本而不是引用排队。  原理
 * 请参见以下链接：
 * https://www.freertos.org/Embedded-RTOS-Queues.html
 */
typedef struct QueueDefinition 		/* 旧的命名约定用于防止破坏内核感知调试器。 */
{
	int8_t *pcHead;					/*< 指向队列存储区域的开头。 */
	int8_t *pcWriteTo;				/*< 指向存储区中下一个空闲位置。 */

	union
	{
		QueuePointers_t xQueue;		/*< 当该结构用作队列时专门需要的数据。 */
		SemaphoreData_t xSemaphore; /*< 当该结构用作信号量时专门需要的数据。 */
	} u;

	List_t xTasksWaitingToSend;		/*< 被阻止等待发布到此队列的任务列表。  按优先顺序存储。 */
	List_t xTasksWaitingToReceive;	/*< 被阻止等待从此队列读取的任务列表。  按优先顺序存储。 */

	volatile UBaseType_t uxMessagesWaiting;/*< 当前队列中的项目数。 */
	UBaseType_t uxLength;			/*< 队列的长度定义为它将保存的项目数，而不是字节数。 */
	UBaseType_t uxItemSize;			/*< 队列将保存的每个项目的大小。 */

	volatile int8_t cRxLock;		/*< 存储队列锁定时从 queue 接收的项目数（从队列中删除）。  当队列未锁定时设置为queueUNLOCKED。 */
	volatile int8_t cTxLock;		/*< 存储队列锁定时传输到 queue（添加到队列）的项目数。  当队列未锁定时设置为queueUNLOCKED。 */

	#if( ( configSUPPORT_STATIC_ALLOCATION == 1 ) && ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) )
		uint8_t ucStaticallyAllocated;	/*< 如果队列使用的内存是静态分配的，则设置为 pdTRUE 以确保不会尝试释放内存。 */
	#endif

	#if ( configUSE_QUEUE_SETS == 1 )
		struct QueueDefinition *pxQueueSetContainer;
	#endif

	#if ( configUSE_TRACE_FACILITY == 1 )
		UBaseType_t uxQueueNumber;
		uint8_t ucQueueType;
	#endif

} xQUEUE;

/* 旧的 xQUEUE 名称保留在上面，然后键入定义为下面
的新 Queue_t 名称，以启用旧的内核感知调试器。 */
typedef xQUEUE Queue_t;

/*-----------------------------------------------------------*/

/*
 * 队列注册表只是内核感知调试器定位队列结构的一种
 * 手段。  它没有其他用途，因此是一个可选组件。
 */
#if ( configQUEUE_REGISTRY_SIZE > 0 )

	/* 存储在队列注册表数组中的类型。
	这允许为每个队列分配一个名称
	，从而使内核感知调试更加用户友好。 */
	typedef struct QUEUE_REGISTRY_ITEM
	{
		const char *pcQueueName; /*lint !e971 仅字符串和单个字符允许使用非限定字符类型。 */
		QueueHandle_t xHandle;
	} xQueueRegistryItem;

	/* 旧的 xQueueRegistryItem 名称保留在上面，然后键入定义
	为下面的新 xQueueRegistryItem 名称，以启用旧内核感知
	调试器。 */
	typedef xQueueRegistryItem QueueRegistryItem_t;

	/* 队列注册表只是一个
	QueueRegistryItem_t 结构的数
	组。 NULL 结构的 pcQueueName 成员表示数组位置是空的。 */
	PRIVILEGED_DATA QueueRegistryItem_t xQueueRegistry[ configQUEUE_REGISTRY_SIZE ];

#endif /* configQUEUE_REGISTRY_SIZE */

/*
 * 解锁通过调用 prvLockQueue 锁定的队列
 * 。  锁定队列不会阻止 ISR 在队列中添加或删除
 * 项目，但会阻止 ISR 从队列事件列表中删除任务。
 * 如果 ISR 发现队列被锁定，它将增加适当的队
 * 列锁定计数以指示任务可能需要解锁。  当队列解锁时
 * ，将检查这些锁定计数，并采取适当的操作。
 */
static void prvUnlockQueue( Queue_t * const pxQueue ) PRIVILEGED_FUNCTION;

/*
 * 使用临界区来确定队列中是否有数据。
 *
 * 如果队列不包含任何项目，则为 @return pdTRUE，否则为 pdFALSE。
 */
static BaseType_t prvIsQueueEmpty( const Queue_t *pxQueue ) PRIVILEGED_FUNCTION;

/*
 * 使用临界区来确定队列中是否有空间。
 *
 * 如果没有空格则为@return pdTRUE，否则为pdFALSE；
 */
static BaseType_t prvIsQueueFull( const Queue_t *pxQueue ) PRIVILEGED_FUNCTION;

/*
 * 将项目复制到队列中，无论是在
 * 队列的前面还是在队列的后面。
 */
static BaseType_t prvCopyDataToQueue( Queue_t * const pxQueue, const void *pvItemToQueue, const BaseType_t xPosition ) PRIVILEGED_FUNCTION;

/*
 * 从队列中复制项目。
 */
static void prvCopyDataFromQueue( Queue_t * const pxQueue, void * const pvBuffer ) PRIVILEGED_FUNCTION;

#if ( configUSE_QUEUE_SETS == 1 )
	/*
	 * 检查队列是否是队列集的成员，如果
	 * 是，则通知队列集该队列包含数据。
	 */
	static BaseType_t prvNotifyQueueSetContainer( const Queue_t * const pxQueue ) PRIVILEGED_FUNCTION;
#endif

/*
 * 在静态或动态分配 Queue_t
 * 结构以填充结构的成员后调用。
 */
static void prvInitialiseNewQueue( const UBaseType_t uxQueueLength, const UBaseType_t uxItemSize, uint8_t *pucQueueStorage, const uint8_t ucQueueType, Queue_t *pxNewQueue ) PRIVILEGED_FUNCTION;

/*
 * 互斥体是一种特殊类型的队列。  创建互斥体时
 * ，首先创建队列，然后调用
 * prvInitialiseMutex() 将队列配置为互斥体。
 */
#if( configUSE_MUTEXES == 1 )
	static void prvInitialiseMutex( Queue_t *pxNewQueue ) PRIVILEGED_FUNCTION;
#endif

#if( configUSE_MUTEXES == 1 )
	/*
	 * 如果等待互斥锁的任务导致互斥锁持有者
	 * 继承优先级，但等待任务超时，则持有者
	 * 应取消继承优先级 - 但只能降低到等
	 * 待同一互斥锁的任何其他任务的最高优先
	 * 级。  该函数返回该优先级。
	 */
	static UBaseType_t prvGetDisinheritPriorityAfterTimeout( const Queue_t * const pxQueue ) PRIVILEGED_FUNCTION;
#endif
/*-----------------------------------------------------------*/

/*
 * 将队列标记为锁定的宏。  锁定队列
 * 可防止 ISR 访问队列事件列表。
 */
#define prvLockQueue( pxQueue )								\
	taskENTER_CRITICAL();									\
	{														\
		if( ( pxQueue )->cRxLock == queueUNLOCKED )			\
		{													\
			( pxQueue )->cRxLock = queueLOCKED_UNMODIFIED;	\
		}													\
		if( ( pxQueue )->cTxLock == queueUNLOCKED )			\
		{													\
			( pxQueue )->cTxLock = queueLOCKED_UNMODIFIED;	\
		}													\
	}														\
	taskEXIT_CRITICAL()
/*-----------------------------------------------------------*/

BaseType_t xQueueGenericReset( QueueHandle_t xQueue, BaseType_t xNewQueue )
{
Queue_t * const pxQueue = xQueue;

	configASSERT( pxQueue );

	taskENTER_CRITICAL();
	{
		pxQueue->u.xQueue.pcTail = pxQueue->pcHead + ( pxQueue->uxLength * pxQueue->uxItemSize ); /*lint !e9016 char 类型允许进行指针算术，特别是当它有助于传达意图时。 */
		pxQueue->uxMessagesWaiting = ( UBaseType_t ) 0U;
		pxQueue->pcWriteTo = pxQueue->pcHead;
		pxQueue->u.xQueue.pcReadFrom = pxQueue->pcHead + ( ( pxQueue->uxLength - 1U ) * pxQueue->uxItemSize ); /*lint !e9016 char 类型允许进行指针算术，特别是当它有助于传达意图时。 */
		pxQueue->cRxLock = queueUNLOCKED;
		pxQueue->cTxLock = queueUNLOCKED;

		if( xNewQueue == pdFALSE )
		{
			/* 如果有任务被阻塞等待从队列中读取，则
			任务将保持阻塞状态，因为在该函数退出
			后队列仍为空。  如果有任务被阻塞等
			待写入队列，那么应该解除阻塞，因为在
			此函数退出后就可以对其进行写入。 */
			if( listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToSend ) ) == pdFALSE )
			{
				if( xTaskRemoveFromEventList( &( pxQueue->xTasksWaitingToSend ) ) != pdFALSE )
				{
					queueYIELD_IF_USING_PREEMPTION();
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
			/* 确保事件队列以正确的状态启动。 */
			vListInitialise( &( pxQueue->xTasksWaitingToSend ) );
			vListInitialise( &( pxQueue->xTasksWaitingToReceive ) );
		}
	}
	taskEXIT_CRITICAL();

	/* 返回一个值用于调用与先前的语义一致性
	版本。 */
	return pdPASS;
}
/*-----------------------------------------------------------*/

#if( configSUPPORT_STATIC_ALLOCATION == 1 )

	QueueHandle_t xQueueGenericCreateStatic( const UBaseType_t uxQueueLength, const UBaseType_t uxItemSize, uint8_t *pucQueueStorage, StaticQueue_t *pxStaticQueue, const uint8_t ucQueueType )
	{
	Queue_t *pxNewQueue;

		configASSERT( uxQueueLength > ( UBaseType_t ) 0 );

		/* StaticQueue_t结构和队列存储区必须是
		提供。 */
		configASSERT( pxStaticQueue != NULL );

		/* 如果项目大小不为0，则应提供队列存储区；
		如果项目大小为0，则不应提供队列存储区。 */
		configASSERT( !( ( pucQueueStorage != NULL ) && ( uxItemSize == 0 ) ) );
		configASSERT( !( ( pucQueueStorage == NULL ) && ( uxItemSize != 0 ) ) );

		#if( configASSERT_DEFINED == 1 )
		{
			/* 健全性检查用于声明 StaticQueue_t
			或 StaticSemaphore_t 类型变量
			的结构的大小是否等于实际队列和信号量结构的大小。 */
			volatile size_t xSize = sizeof( StaticQueue_t );
			configASSERT( xSize == sizeof( Queue_t ) );
			( void ) xSize; /* 当 configASSERT() 未定义时，保持 lint 安静。 */
		}
		#endif /* configASSERT_DEFINED */

		/* 传入静态分配队列的地址，使
		用它。静态分配的存储区域的
		地址也被传入，但已经设置。 */
		pxNewQueue = ( Queue_t * ) pxStaticQueue; /*lint !e740 !e9087 不寻常的转换是可以的，因为结构被设计为具有相同的对齐方式，并且大小由断言检查。 */

		if( pxNewQueue != NULL )
		{
			#if( configSUPPORT_DYNAMIC_ALLOCATION == 1 )
			{
				/* 队列可以静态或动态分配，因
				此请注意，该队列是静态分配
				的，以防稍后删除该队列。 */
				pxNewQueue->ucStaticallyAllocated = pdTRUE;
			}
			#endif /* configSUPPORT_DYNAMIC_ALLOCATION */

			prvInitialiseNewQueue( uxQueueLength, uxItemSize, pucQueueStorage, ucQueueType, pxNewQueue );
		}
		else
		{
			traceQUEUE_CREATE_FAILED( ucQueueType );
			mtCOVERAGE_TEST_MARKER();
		}

		return pxNewQueue;
	}

#endif /* configSUPPORT_STATIC_ALLOCATION */
/*-----------------------------------------------------------*/

#if( configSUPPORT_DYNAMIC_ALLOCATION == 1 )

	QueueHandle_t xQueueGenericCreate( const UBaseType_t uxQueueLength, const UBaseType_t uxItemSize, const uint8_t ucQueueType )
	{
	Queue_t *pxNewQueue;
	size_t xQueueSizeInBytes;
	uint8_t *pucQueueStorage;

		configASSERT( uxQueueLength > ( UBaseType_t ) 0 );

		/* 分配足够的空间来容纳队列中任何时候可容纳的
		最大数量的项目。  在队列用作信号量的情况
		下，uxItemSize 为零是有效的。 */
		xQueueSizeInBytes = ( size_t ) ( uxQueueLength * uxItemSize ); /*lint !e961 MISRA 例外，因为强制转换仅对某些端口是多余的。 */

		/* 分配队列和存储区域。  MISRA 偏差的理由如
		下：pvPortMalloc() 始终确保返回的
		内存块按照 MCU 堆栈的要求对齐。  在这种情
		况下，pvPortMalloc() 必须返回一个
		保证满足 Queue_t 结构的对齐要求的指针
		- 在这种情况下是 int8_t *。  因此，
		每当堆栈对齐要求大于或等于指向 char 的指针
		要求时，强制转换就是安全的。  在其他情况下，对
		齐要求不是 strict（一个或两个字节）。 */
		pxNewQueue = ( Queue_t * ) pvPortMalloc( sizeof( Queue_t ) + xQueueSizeInBytes ); /*lint !e9087 !e9079 请参阅上面的评论。 */

		if( pxNewQueue != NULL )
		{
			/* 跳过队列结构找到队
			列存储区域的位置。 */
			pucQueueStorage = ( uint8_t * ) pxNewQueue;
			pucQueueStorage += sizeof( Queue_t ); /*lint !e9016 char 类型允许进行指针算术，特别是当它有助于传达意图时。 */

			#if( configSUPPORT_STATIC_ALLOCATION == 1 )
			{
				/* 队列可以静态或动态创建，因此请注意
				此任务是动态创建的，以防稍后出现
				已删除。 */
				pxNewQueue->ucStaticallyAllocated = pdFALSE;
			}
			#endif /* configSUPPORT_STATIC_ALLOCATION */

			prvInitialiseNewQueue( uxQueueLength, uxItemSize, pucQueueStorage, ucQueueType, pxNewQueue );
		}
		else
		{
			traceQUEUE_CREATE_FAILED( ucQueueType );
			mtCOVERAGE_TEST_MARKER();
		}

		return pxNewQueue;
	}

#endif /* configSUPPORT_STATIC_ALLOCATION */
/*-----------------------------------------------------------*/

static void prvInitialiseNewQueue( const UBaseType_t uxQueueLength, const UBaseType_t uxItemSize, uint8_t *pucQueueStorage, const uint8_t ucQueueType, Queue_t *pxNewQueue )
{
	/* 如果
	configUSE_TRACE_FACILITY 未设置为 1，则删除有关未使用参数的编译器警告。 */
	( void ) ucQueueType;

	if( uxItemSize == ( UBaseType_t ) 0 )
	{
		/* 没有为队列存储区域分配RAM，但不能将PC头设
		置为NULL，因为NULL作为key表示队列用
		作互斥体。  因此，只需将 pcHead 设置
		为指向队列作为已知位于内存映射内的良性值即可。 */
		pxNewQueue->pcHead = ( int8_t * ) pxNewQueue;
	}
	else
	{
		/* 将头部设置为队列存储区域的开头。 */
		pxNewQueue->pcHead = ( int8_t * ) pucQueueStorage;
	}

	/* 按照队列类型的描述初始化队列成员
	定义的。 */
	pxNewQueue->uxLength = uxQueueLength;
	pxNewQueue->uxItemSize = uxItemSize;
	( void ) xQueueGenericReset( pxNewQueue, pdTRUE );

	#if ( configUSE_TRACE_FACILITY == 1 )
	{
		pxNewQueue->ucQueueType = ucQueueType;
	}
	#endif /* configUSE_TRACE_FACILITY */

	#if( configUSE_QUEUE_SETS == 1 )
	{
		pxNewQueue->pxQueueSetContainer = NULL;
	}
	#endif /* configUSE_QUEUE_SETS */

	traceQUEUE_CREATE( pxNewQueue );
}
/*-----------------------------------------------------------*/

#if( configUSE_MUTEXES == 1 )

	static void prvInitialiseMutex( Queue_t *pxNewQueue )
	{
		if( pxNewQueue != NULL )
		{
			/* 队列创建函数将为通用队列正确设置所有
			队列结构成员，但该函数正在创建互斥体
			。  覆盖那些需要进行不同设置的成员
			- 特别是优先级继承所需的信息。 */
			pxNewQueue->u.xSemaphore.xMutexHolder = NULL;
			pxNewQueue->uxQueueType = queueQUEUE_IS_MUTEX;

			/* 如果这是一个递归互斥体。 */
			pxNewQueue->u.xSemaphore.uxRecursiveCallCount = 0;

			traceCREATE_MUTEX( pxNewQueue );

			/* 从信号量处于预期状态开始。 */
			( void ) xQueueGenericSend( pxNewQueue, NULL, ( TickType_t ) 0U, queueSEND_TO_BACK );
		}
		else
		{
			traceCREATE_MUTEX_FAILED();
		}
	}

#endif /* configUSE_MUTEXES */
/*-----------------------------------------------------------*/

#if( ( configUSE_MUTEXES == 1 ) && ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) )

	QueueHandle_t xQueueCreateMutex( const uint8_t ucQueueType )
	{
	QueueHandle_t xNewQueue;
	const UBaseType_t uxMutexLength = ( UBaseType_t ) 1, uxMutexSize = ( UBaseType_t ) 0;

		xNewQueue = xQueueGenericCreate( uxMutexLength, uxMutexSize, ucQueueType );
		prvInitialiseMutex( ( Queue_t * ) xNewQueue );

		return xNewQueue;
	}

#endif /* configUSE_MUTEXES */
/*-----------------------------------------------------------*/

#if( ( configUSE_MUTEXES == 1 ) && ( configSUPPORT_STATIC_ALLOCATION == 1 ) )

	QueueHandle_t xQueueCreateMutexStatic( const uint8_t ucQueueType, StaticQueue_t *pxStaticQueue )
	{
	QueueHandle_t xNewQueue;
	const UBaseType_t uxMutexLength = ( UBaseType_t ) 1, uxMutexSize = ( UBaseType_t ) 0;

		/* 如果 configUSE_TRACE_FACILITY
		不等于 1，则防止编译器发出有关未使用参数的警告。 */
		( void ) ucQueueType;

		xNewQueue = xQueueGenericCreateStatic( uxMutexLength, uxMutexSize, NULL, pxStaticQueue, ucQueueType );
		prvInitialiseMutex( ( Queue_t * ) xNewQueue );

		return xNewQueue;
	}

#endif /* configUSE_MUTEXES */
/*-----------------------------------------------------------*/

#if ( ( configUSE_MUTEXES == 1 ) && ( INCLUDE_xSemaphoreGetMutexHolder == 1 ) )

	TaskHandle_t xQueueGetMutexHolder( QueueHandle_t xSemaphore )
	{
	TaskHandle_t pxReturn;
	Queue_t * const pxSemaphore = ( Queue_t * ) xSemaphore;

		/* 该函数由
		xSemaphoreGetMutexHolder()
		调用，不应直接调用。  注意：这是确定调用任务
		是否是互斥锁持有者的好方法，但不是确定互斥锁持有
		者身份的好方法，因为持有者可能会在接下来的临界区退出和函数返回之间发生变化。 */
		taskENTER_CRITICAL();
		{
			if( pxSemaphore->uxQueueType == queueQUEUE_IS_MUTEX )
			{
				pxReturn = pxSemaphore->u.xSemaphore.xMutexHolder;
			}
			else
			{
				pxReturn = NULL;
			}
		}
		taskEXIT_CRITICAL();

		return pxReturn;
	} /*lint !e818 xSemaphore 不能是指向 const 的指针，因为它是 typedef。 */

#endif
/*-----------------------------------------------------------*/

#if ( ( configUSE_MUTEXES == 1 ) && ( INCLUDE_xSemaphoreGetMutexHolder == 1 ) )

	TaskHandle_t xQueueGetMutexHolderFromISR( QueueHandle_t xSemaphore )
	{
	TaskHandle_t pxReturn;

		configASSERT( xSemaphore );

		/* 互斥体不能在中断服务例程中使用，
		因此互斥体持有者不应在 ISR
		中更改，因此这里不需要临界区。 */
		if( ( ( Queue_t * ) xSemaphore )->uxQueueType == queueQUEUE_IS_MUTEX )
		{
			pxReturn = ( ( Queue_t * ) xSemaphore )->u.xSemaphore.xMutexHolder;
		}
		else
		{
			pxReturn = NULL;
		}

		return pxReturn;
	} /*lint !e818 xSemaphore 不能是指向 const 的指针，因为它是 typedef。 */

#endif
/*-----------------------------------------------------------*/

#if ( configUSE_RECURSIVE_MUTEXES == 1 )

	BaseType_t xQueueGiveMutexRecursive( QueueHandle_t xMutex )
	{
	BaseType_t xReturn;
	Queue_t * const pxMutex = ( Queue_t * ) xMutex;

		configASSERT( pxMutex );

		/* 如果这是保存互斥锁的任务，则 xMutexHolder
		将不会在该任务之外发生更改。  如果此任务不持有互斥体，
		则 pxMutexHolder 永远不会巧合地等于任务句
		柄，并且由于这是我们感兴趣的唯一条件，因此
		pxMutexHolder 是否被另一个任务同时访问并不
		重要。  因此，测试 pxMutexHolder 变量不需要互斥。 */
		if( pxMutex->u.xSemaphore.xMutexHolder == xTaskGetCurrentTaskHandle() )
		{
			traceGIVE_MUTEX_RECURSIVE( pxMutex );

			/* 如果 xMutexHolder 等于任务句柄，则
			uxRecursiveCallCount 不能为零，因此不
			需要下溢检查。  另外，
			uxRecursiveCallCount 仅由互斥持有者修
			改，并且由于只能有一个，因此修改 uxRecursiveCallCount 成员不需要互斥。 */
			( pxMutex->u.xSemaphore.uxRecursiveCallCount )--;

			/* 递归调用计数是否已恢复为 0？ */
			if( pxMutex->u.xSemaphore.uxRecursiveCallCount == ( UBaseType_t ) 0 )
			{
				/* 返回互斥体。  这将自动解锁可能正
				在等待访问互斥体的任何其他任务。 */
				( void ) xQueueGenericSend( pxMutex, NULL, queueMUTEX_GIVE_BLOCK_TIME, queueSEND_TO_BACK );
			}
			else
			{
				mtCOVERAGE_TEST_MARKER();
			}

			xReturn = pdPASS;
		}
		else
		{
			/* 无法给出互斥体，因为调用任务不是
			持有者。 */
			xReturn = pdFAIL;

			traceGIVE_MUTEX_RECURSIVE_FAILED( pxMutex );
		}

		return xReturn;
	}

#endif /* configUSE_RECURSIVE_MUTEXES */
/*-----------------------------------------------------------*/

#if ( configUSE_RECURSIVE_MUTEXES == 1 )

	BaseType_t xQueueTakeMutexRecursive( QueueHandle_t xMutex, TickType_t xTicksToWait )
	{
	BaseType_t xReturn;
	Queue_t * const pxMutex = ( Queue_t * ) xMutex;

		configASSERT( pxMutex );

		/* 关于互斥的注释按照
		xQueueGiveMutexRecursive() 中的注释。 */

		traceTAKE_MUTEX_RECURSIVE( pxMutex );

		if( pxMutex->u.xSemaphore.xMutexHolder == xTaskGetCurrentTaskHandle() )
		{
			( pxMutex->u.xSemaphore.uxRecursiveCallCount )++;
			xReturn = pdPASS;
		}
		else
		{
			xReturn = xQueueSemaphoreTake( pxMutex, xTicksToWait );

			/* 只有成功获取互斥锁才会返回
			pdPASS。  调用任务在到
			达此处之前可能已进入阻塞状态。 */
			if( xReturn != pdFAIL )
			{
				( pxMutex->u.xSemaphore.uxRecursiveCallCount )++;
			}
			else
			{
				traceTAKE_MUTEX_RECURSIVE_FAILED( pxMutex );
			}
		}

		return xReturn;
	}

#endif /* configUSE_RECURSIVE_MUTEXES */
/*-----------------------------------------------------------*/

#if( ( configUSE_COUNTING_SEMAPHORES == 1 ) && ( configSUPPORT_STATIC_ALLOCATION == 1 ) )

	QueueHandle_t xQueueCreateCountingSemaphoreStatic( const UBaseType_t uxMaxCount, const UBaseType_t uxInitialCount, StaticQueue_t *pxStaticQueue )
	{
	QueueHandle_t xHandle;

		configASSERT( uxMaxCount != 0 );
		configASSERT( uxInitialCount <= uxMaxCount );

		xHandle = xQueueGenericCreateStatic( uxMaxCount, queueSEMAPHORE_QUEUE_ITEM_LENGTH, NULL, pxStaticQueue, queueQUEUE_TYPE_COUNTING_SEMAPHORE );

		if( xHandle != NULL )
		{
			( ( Queue_t * ) xHandle )->uxMessagesWaiting = uxInitialCount;

			traceCREATE_COUNTING_SEMAPHORE();
		}
		else
		{
			traceCREATE_COUNTING_SEMAPHORE_FAILED();
		}

		return xHandle;
	}

#endif /* ((configUSE_COUNTING_SEMAPHORES==1)&&(configSUPPORT_DYNAMIC_ALLOCATION==1)) */
/*-----------------------------------------------------------*/

#if( ( configUSE_COUNTING_SEMAPHORES == 1 ) && ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) )

	QueueHandle_t xQueueCreateCountingSemaphore( const UBaseType_t uxMaxCount, const UBaseType_t uxInitialCount )
	{
	QueueHandle_t xHandle;

		configASSERT( uxMaxCount != 0 );
		configASSERT( uxInitialCount <= uxMaxCount );

		xHandle = xQueueGenericCreate( uxMaxCount, queueSEMAPHORE_QUEUE_ITEM_LENGTH, queueQUEUE_TYPE_COUNTING_SEMAPHORE );

		if( xHandle != NULL )
		{
			( ( Queue_t * ) xHandle )->uxMessagesWaiting = uxInitialCount;

			traceCREATE_COUNTING_SEMAPHORE();
		}
		else
		{
			traceCREATE_COUNTING_SEMAPHORE_FAILED();
		}

		return xHandle;
	}

#endif /* ((configUSE_COUNTING_SEMAPHORES==1)&&(configSUPPORT_DYNAMIC_ALLOCATION==1)) */
/*-----------------------------------------------------------*/

BaseType_t xQueueGenericSend( QueueHandle_t xQueue, const void * const pvItemToQueue, TickType_t xTicksToWait, const BaseType_t xCopyPosition )
{
BaseType_t xEntryTimeSet = pdFALSE, xYieldRequired;
TimeOut_t xTimeOut;
Queue_t * const pxQueue = xQueue;

	configASSERT( pxQueue );
	configASSERT( !( ( pvItemToQueue == NULL ) && ( pxQueue->uxItemSize != ( UBaseType_t ) 0U ) ) );
	configASSERT( !( ( xCopyPosition == queueOVERWRITE ) && ( pxQueue->uxLength != 1 ) ) );
	#if ( ( INCLUDE_xTaskGetSchedulerState == 1 ) || ( configUSE_TIMERS == 1 ) )
	{
		configASSERT( !( ( xTaskGetSchedulerState() == taskSCHEDULER_SUSPENDED ) && ( xTicksToWait != 0 ) ) );
	}
	#endif


	/*lint -save -e904 此函数在某种程
	度上放宽了编码标准，以允许函数本身内的
	return 语句。  这样做是为了提高执行时间效率。 */
	for( ;; )
	{
		taskENTER_CRITICAL();
		{
			/* 现在队列还有空位吗？  正在运行
			的任务必须是想要访问队列的最高优
			先级任务。  如果要覆盖队列中的
			头项，那么队列是否已满并不重要。 */
			if( ( pxQueue->uxMessagesWaiting < pxQueue->uxLength ) || ( xCopyPosition == queueOVERWRITE ) )
			{
				traceQUEUE_SEND( pxQueue );

				#if ( configUSE_QUEUE_SETS == 1 )
				{
				const UBaseType_t uxPreviousMessagesWaiting = pxQueue->uxMessagesWaiting;

					xYieldRequired = prvCopyDataToQueue( pxQueue, pvItemToQueue, xCopyPosition );

					if( pxQueue->pxQueueSetContainer != NULL )
					{
						if( ( xCopyPosition == queueOVERWRITE ) && ( uxPreviousMessagesWaiting != ( UBaseType_t ) 0 ) )
						{
							/* 请勿通知队列集，因为队列中
							的现有项目已被覆盖，因此队
							列中的项目数并未更改。 */
							mtCOVERAGE_TEST_MARKER();
						}
						else if( prvNotifyQueueSetContainer( pxQueue ) != pdFALSE )
						{
							/* 该队列是队列集的成员，向队列集
							发送消息会导致更高优先级的任务
							解除阻塞。需要上下文切换。 */
							queueYIELD_IF_USING_PREEMPTION();
						}
						else
						{
							mtCOVERAGE_TEST_MARKER();
						}
					}
					else
					{
						/* 如果有一个任务正在等待数据到达
						队列，那么现在就解除它的阻塞。 */
						if( listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToReceive ) ) == pdFALSE )
						{
							if( xTaskRemoveFromEventList( &( pxQueue->xTasksWaitingToReceive ) ) != pdFALSE )
							{
								/* 未阻塞的任务的优先级高于我们自
								己的任务，因此立即屈服。  是
								的，可以从关键部分内执行此操作
								- 内核会处理该问题。 */
								queueYIELD_IF_USING_PREEMPTION();
							}
							else
							{
								mtCOVERAGE_TEST_MARKER();
							}
						}
						else if( xYieldRequired != pdFALSE )
						{
							/* 此路径是一种特殊情况，仅当
							任务持有多个互斥体并且互斥
							体以与获取互斥体的顺序不同
							的顺序返回时才会执行。 */
							queueYIELD_IF_USING_PREEMPTION();
						}
						else
						{
							mtCOVERAGE_TEST_MARKER();
						}
					}
				}
				#else /* configUSE_QUEUE_SETS */
				{
					xYieldRequired = prvCopyDataToQueue( pxQueue, pvItemToQueue, xCopyPosition );

					/* 如果有一个任务正在等待数据到达
					队列，那么现在就解除它的阻塞。 */
					if( listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToReceive ) ) == pdFALSE )
					{
						if( xTaskRemoveFromEventList( &( pxQueue->xTasksWaitingToReceive ) ) != pdFALSE )
						{
							/* 未阻塞的任务的优先级高于我们自
							己的任务，因此立即屈服。  是
							的，可以从关键部分内执行此操作
							- 内核会处理该问题。 */
							queueYIELD_IF_USING_PREEMPTION();
						}
						else
						{
							mtCOVERAGE_TEST_MARKER();
						}
					}
					else if( xYieldRequired != pdFALSE )
					{
						/* 此路径是一种特殊情况，仅当
						任务持有多个互斥体并且互斥
						体以与获取互斥体的顺序不同
						的顺序返回时才会执行。 */
						queueYIELD_IF_USING_PREEMPTION();
					}
					else
					{
						mtCOVERAGE_TEST_MARKER();
					}
				}
				#endif /* configUSE_QUEUE_SETS */

				taskEXIT_CRITICAL();
				return pdPASS;
			}
			else
			{
				if( xTicksToWait == ( TickType_t ) 0 )
				{
					/* 队列已满，无阻塞时间为
					specified（或阻塞时间已过期），因此请立即离开。 */
					taskEXIT_CRITICAL();

					/* 退出函数之前返回到
					原来的权限级别。 */
					traceQUEUE_SEND_FAILED( pxQueue );
					return errQUEUE_FULL;
				}
				else if( xEntryTimeSet == pdFALSE )
				{
					/* 队列已满并且指定了阻塞时
					间，因此配置超时结构。 */
					vTaskInternalSetTimeOutState( &xTimeOut );
					xEntryTimeSet = pdTRUE;
				}
				else
				{
					/* 入场时间已经确定。 */
					mtCOVERAGE_TEST_MARKER();
				}
			}
		}
		taskEXIT_CRITICAL();

		/* 现在临界区已经退出，中断和其
		他任务可以向队列发送和接收。 */

		vTaskSuspendAll();
		prvLockQueue( pxQueue );

		/* 更新超时状态以查看是否已过期。 */
		if( xTaskCheckForTimeOut( &xTimeOut, &xTicksToWait ) == pdFALSE )
		{
			if( prvIsQueueFull( pxQueue ) != pdFALSE )
			{
				traceBLOCKING_ON_QUEUE_SEND( pxQueue );
				vTaskPlaceOnEventList( &( pxQueue->xTasksWaitingToSend ), xTicksToWait );

				/* 解锁队列意味着队列事件可以影响事件列
				表。  现在发生的中断可能会再次从事
				件列表中删除该任务 - 但由于调度程
				序被挂起，该任务将进入挂起的最后就绪
				状态，而不是实际的就绪列表。 */
				prvUnlockQueue( pxQueue );

				/* 恢复调度程序会将任务从挂起的就绪列表移
				动到就绪列表中 - 因此该任务在屈服之
				前可能已经在就绪列表中 - 在这种情况
				下，屈服不会导致上下文切换，除非挂起的
				就绪列表中还有更高优先级的任务。 */
				if( xTaskResumeAll() == pdFALSE )
				{
					portYIELD_WITHIN_API();
				}
			}
			else
			{
				/* 再试一次。 */
				prvUnlockQueue( pxQueue );
				( void ) xTaskResumeAll();
			}
		}
		else
		{
			/* 超时时间已过。 */
			prvUnlockQueue( pxQueue );
			( void ) xTaskResumeAll();

			traceQUEUE_SEND_FAILED( pxQueue );
			return errQUEUE_FULL;
		}
	} /*lint - 恢复 */
}
/*-----------------------------------------------------------*/

BaseType_t xQueueGenericSendFromISR( QueueHandle_t xQueue, const void * const pvItemToQueue, BaseType_t * const pxHigherPriorityTaskWoken, const BaseType_t xCopyPosition )
{
BaseType_t xReturn;
UBaseType_t uxSavedInterruptStatus;
Queue_t * const pxQueue = xQueue;

	configASSERT( pxQueue );
	configASSERT( !( ( pvItemToQueue == NULL ) && ( pxQueue->uxItemSize != ( UBaseType_t ) 0U ) ) );
	configASSERT( !( ( xCopyPosition == queueOVERWRITE ) && ( pxQueue->uxLength != 1 ) ) );

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
	：http://www.freertos.org/RTOS-Cortex-M3-M4.html */
	portASSERT_IF_INTERRUPT_PRIORITY_INVALID();

	/* 与 xQueueGenericSend 类似，但
	如果队列中没有空间则不会阻塞。  另外，不要直接
	唤醒在队列读取时被阻塞的任务，而是返回一个标志来
	说明是否需要上下文切换或 not （即有一个任务
	的优先级比我们被这篇文章唤醒的任务高）。 */
	uxSavedInterruptStatus = portSET_INTERRUPT_MASK_FROM_ISR();
	{
		if( ( pxQueue->uxMessagesWaiting < pxQueue->uxLength ) || ( xCopyPosition == queueOVERWRITE ) )
		{
			const int8_t cTxLock = pxQueue->cTxLock;
			const UBaseType_t uxPreviousMessagesWaiting = pxQueue->uxMessagesWaiting;

			traceQUEUE_SEND_FROM_ISR( pxQueue );

			/* 信号量使用 xQueueGiveFromISR()，因此
			pxQueue 不会是信号量或互斥量。  这意味着
			prvCopyDataToQueue() 不会导致任务取消优
			先级，并且可以在此处调用
			prvCopyDataToQueue()，即使取消继承函数在访问就绪列表之前不会检查调度程序是否已挂起。 */
			( void ) prvCopyDataToQueue( pxQueue, pvItemToQueue, xCopyPosition );

			/* 如果队列被锁定，则事件列表不会更改
			。  这将在稍后解锁队列时完成。 */
			if( cTxLock == queueUNLOCKED )
			{
				#if ( configUSE_QUEUE_SETS == 1 )
				{
					if( pxQueue->pxQueueSetContainer != NULL )
					{
						if( ( xCopyPosition == queueOVERWRITE ) && ( uxPreviousMessagesWaiting != ( UBaseType_t ) 0 ) )
						{
							/* 请勿通知队列集，因为队列中
							的现有项目已被覆盖，因此队
							列中的项目数并未更改。 */
							mtCOVERAGE_TEST_MARKER();
						}
						else if( prvNotifyQueueSetContainer( pxQueue ) != pdFALSE )
						{
							/* 该队列是队列集的成员，向队列集
							发送消息会导致更高优先级的任务
							解除阻塞。  需要上下文切换。 */
							if( pxHigherPriorityTaskWoken != NULL )
							{
								*pxHigherPriorityTaskWoken = pdTRUE;
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
						if( listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToReceive ) ) == pdFALSE )
						{
							if( xTaskRemoveFromEventList( &( pxQueue->xTasksWaitingToReceive ) ) != pdFALSE )
							{
								/* 等待的任务优先级较高，因
								此记录需要上下文切换。 */
								if( pxHigherPriorityTaskWoken != NULL )
								{
									*pxHigherPriorityTaskWoken = pdTRUE;
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
				}
				#else /* configUSE_QUEUE_SETS */
				{
					if( listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToReceive ) ) == pdFALSE )
					{
						if( xTaskRemoveFromEventList( &( pxQueue->xTasksWaitingToReceive ) ) != pdFALSE )
						{
							/* 等待的任务优先级较高，因
							此记录需要上下文切换。 */
							if( pxHigherPriorityTaskWoken != NULL )
							{
								*pxHigherPriorityTaskWoken = pdTRUE;
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
					
					/* 未在此路径中使用。 */
					( void ) uxPreviousMessagesWaiting;
				}
				#endif /* configUSE_QUEUE_SETS */
			}
			else
			{
				/* 增加锁定计数，以便解锁队列的
				任务知道数据在锁定时已发布。 */
				pxQueue->cTxLock = ( int8_t ) ( cTxLock + 1 );
			}

			xReturn = pdPASS;
		}
		else
		{
			traceQUEUE_SEND_FROM_ISR_FAILED( pxQueue );
			xReturn = errQUEUE_FULL;
		}
	}
	portCLEAR_INTERRUPT_MASK_FROM_ISR( uxSavedInterruptStatus );

	return xReturn;
}
/*-----------------------------------------------------------*/

BaseType_t xQueueGiveFromISR( QueueHandle_t xQueue, BaseType_t * const pxHigherPriorityTaskWoken )
{
BaseType_t xReturn;
UBaseType_t uxSavedInterruptStatus;
Queue_t * const pxQueue = xQueue;

	/* 与
	xQueueGenericSendFromISR()
	类似，但与项目大小为 0 的信号量一起使用。不要
	直接唤醒在队列读取时被阻塞的任务，而是返回一个标志
	来说明是否需要上下文切换或 not （即有一个任务的优先级比我们被本文唤醒的任务更高）。 */

	configASSERT( pxQueue );

	/* xQueueGenericSendFromISR() should be used instead of xQueueGiveFromISR()
	if the item size is not 0. */
	configASSERT( pxQueue->uxItemSize == 0 );

	/* 通常，不会从中断中给出互斥体，特别是
	如果存在互斥体持有者，因为优先级继承
	对于中断没有意义，只有任务有意义。 */
	configASSERT( !( ( pxQueue->uxQueueType == queueQUEUE_IS_MUTEX ) && ( pxQueue->u.xSemaphore.xMutexHolder != NULL ) ) );

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
	：http://www.freertos.org/RTOS-Cortex-M3-M4.html */
	portASSERT_IF_INTERRUPT_PRIORITY_INVALID();

	uxSavedInterruptStatus = portSET_INTERRUPT_MASK_FROM_ISR();
	{
		const UBaseType_t uxMessagesWaiting = pxQueue->uxMessagesWaiting;

		/* 当队列用于实现信号量时，不会
		有数据通过队列移动，但查看队
		列是否“有空间”仍然有效。 */
		if( uxMessagesWaiting < pxQueue->uxLength )
		{
			const int8_t cTxLock = pxQueue->cTxLock;

			traceQUEUE_SEND_FROM_ISR( pxQueue );

			/* 如果任务是互斥锁持有者，则它只能具有继承的优先
			级 - 如果存在互斥锁持有者，则不能从 ISR
			给出互斥锁。  由于这是该函数的 ISR 版
			本，因此可以假设没有互斥锁持有者，并且无需确定
			是否需要取消优先级继承。  只需增加可用的
			messages（信号量）的数量即可。 */
			pxQueue->uxMessagesWaiting = uxMessagesWaiting + ( UBaseType_t ) 1;

			/* 如果队列被锁定，则事件列表不会更改
			。  这将在稍后解锁队列时完成。 */
			if( cTxLock == queueUNLOCKED )
			{
				#if ( configUSE_QUEUE_SETS == 1 )
				{
					if( pxQueue->pxQueueSetContainer != NULL )
					{
						if( prvNotifyQueueSetContainer( pxQueue ) != pdFALSE )
						{
							/* 信号量是队列集的成员，发布到队
							列集会导致更高优先级的任务解除
							阻塞。  需要上下文切换。 */
							if( pxHigherPriorityTaskWoken != NULL )
							{
								*pxHigherPriorityTaskWoken = pdTRUE;
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
						if( listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToReceive ) ) == pdFALSE )
						{
							if( xTaskRemoveFromEventList( &( pxQueue->xTasksWaitingToReceive ) ) != pdFALSE )
							{
								/* 等待的任务优先级较高，因
								此记录需要上下文切换。 */
								if( pxHigherPriorityTaskWoken != NULL )
								{
									*pxHigherPriorityTaskWoken = pdTRUE;
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
				}
				#else /* configUSE_QUEUE_SETS */
				{
					if( listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToReceive ) ) == pdFALSE )
					{
						if( xTaskRemoveFromEventList( &( pxQueue->xTasksWaitingToReceive ) ) != pdFALSE )
						{
							/* 等待的任务优先级较高，因
							此记录需要上下文切换。 */
							if( pxHigherPriorityTaskWoken != NULL )
							{
								*pxHigherPriorityTaskWoken = pdTRUE;
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
				#endif /* configUSE_QUEUE_SETS */
			}
			else
			{
				/* 增加锁定计数，以便解锁队列的
				任务知道数据在锁定时已发布。 */
				pxQueue->cTxLock = ( int8_t ) ( cTxLock + 1 );
			}

			xReturn = pdPASS;
		}
		else
		{
			traceQUEUE_SEND_FROM_ISR_FAILED( pxQueue );
			xReturn = errQUEUE_FULL;
		}
	}
	portCLEAR_INTERRUPT_MASK_FROM_ISR( uxSavedInterruptStatus );

	return xReturn;
}
/*-----------------------------------------------------------*/

BaseType_t xQueueReceive( QueueHandle_t xQueue, void * const pvBuffer, TickType_t xTicksToWait )
{
BaseType_t xEntryTimeSet = pdFALSE;
TimeOut_t xTimeOut;
Queue_t * const pxQueue = xQueue;

	/* 检查指针不是NULL。 */
	configASSERT( ( pxQueue ) );

	/* 如果数据大小为 zero，则接收数据的缓冲区只
	能为 NULL（因此不会将数据复制到缓冲区中。 */
	configASSERT( !( ( ( pvBuffer ) == NULL ) && ( ( pxQueue )->uxItemSize != ( UBaseType_t ) 0U ) ) );

	/* 如果调度程序暂停，则无法阻止。 */
	#if ( ( INCLUDE_xTaskGetSchedulerState == 1 ) || ( configUSE_TIMERS == 1 ) )
	{
		configASSERT( !( ( xTaskGetSchedulerState() == taskSCHEDULER_SUSPENDED ) && ( xTicksToWait != 0 ) ) );
	}
	#endif


	/*lint -save -e904 此函数在某种程
	度上放宽了编码标准，以允许函数本身内的
	return 语句。  这样做是为了提高执行时间效率。 */
	for( ;; )
	{
		taskENTER_CRITICAL();
		{
			const UBaseType_t uxMessagesWaiting = pxQueue->uxMessagesWaiting;

			/* 现在队列中有数据吗？  要运行的调用任
			务必须是想要访问队列的最高优先级任务。 */
			if( uxMessagesWaiting > ( UBaseType_t ) 0 )
			{
				/* 数据可用，删除一项。 */
				prvCopyDataFromQueue( pxQueue, pvBuffer );
				traceQUEUE_RECEIVE( pxQueue );
				pxQueue->uxMessagesWaiting = uxMessagesWaiting - ( UBaseType_t ) 1;

				/* 现在队列中有空间，是否有任何任务等待发布到
				队列中？  如果有，则解锁最高优先级等待
				任务。 */
				if( listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToSend ) ) == pdFALSE )
				{
					if( xTaskRemoveFromEventList( &( pxQueue->xTasksWaitingToSend ) ) != pdFALSE )
					{
						queueYIELD_IF_USING_PREEMPTION();
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

				taskEXIT_CRITICAL();
				return pdPASS;
			}
			else
			{
				if( xTicksToWait == ( TickType_t ) 0 )
				{
					/* 队列为空且无阻塞时间为
					specified（或阻塞时间已过期），因此立即离开。 */
					taskEXIT_CRITICAL();
					traceQUEUE_RECEIVE_FAILED( pxQueue );
					return errQUEUE_EMPTY;
				}
				else if( xEntryTimeSet == pdFALSE )
				{
					/* 队列为空并且指定了阻塞时
					间，因此配置超时结构。 */
					vTaskInternalSetTimeOutState( &xTimeOut );
					xEntryTimeSet = pdTRUE;
				}
				else
				{
					/* 入场时间已经确定。 */
					mtCOVERAGE_TEST_MARKER();
				}
			}
		}
		taskEXIT_CRITICAL();

		/* 现在临界区已经退出，中断和其
		他任务可以向队列发送和接收。 */

		vTaskSuspendAll();
		prvLockQueue( pxQueue );

		/* 更新超时状态以查看是否已过期。 */
		if( xTaskCheckForTimeOut( &xTimeOut, &xTicksToWait ) == pdFALSE )
		{
			/* 超时时间尚未到期。  如果队列仍然为空，则
			将该任务放入等待从队列接收的任务列表中。 */
			if( prvIsQueueEmpty( pxQueue ) != pdFALSE )
			{
				traceBLOCKING_ON_QUEUE_RECEIVE( pxQueue );
				vTaskPlaceOnEventList( &( pxQueue->xTasksWaitingToReceive ), xTicksToWait );
				prvUnlockQueue( pxQueue );
				if( xTaskResumeAll() == pdFALSE )
				{
					portYIELD_WITHIN_API();
				}
				else
				{
					mtCOVERAGE_TEST_MARKER();
				}
			}
			else
			{
				/* 队列再次包含数据。  循环回来尝试阅读
				数据。 */
				prvUnlockQueue( pxQueue );
				( void ) xTaskResumeAll();
			}
		}
		else
		{
			/* 超时了。  如果队列中没有数据则退
			出，否则循环返回并尝试读取数据。 */
			prvUnlockQueue( pxQueue );
			( void ) xTaskResumeAll();

			if( prvIsQueueEmpty( pxQueue ) != pdFALSE )
			{
				traceQUEUE_RECEIVE_FAILED( pxQueue );
				return errQUEUE_EMPTY;
			}
			else
			{
				mtCOVERAGE_TEST_MARKER();
			}
		}
	} /*lint - 恢复 */
}
/*-----------------------------------------------------------*/

BaseType_t xQueueSemaphoreTake( QueueHandle_t xQueue, TickType_t xTicksToWait )
{
BaseType_t xEntryTimeSet = pdFALSE;
TimeOut_t xTimeOut;
Queue_t * const pxQueue = xQueue;

#if( configUSE_MUTEXES == 1 )
	BaseType_t xInheritanceOccurred = pdFALSE;
#endif

	/* 检查队列指针不是NULL。 */
	configASSERT( ( pxQueue ) );

	/* 检查这确实是一个信号量，在这种情况下，项目大小将为
	0. */
	configASSERT( pxQueue->uxItemSize == 0 );

	/* 如果调度程序暂停，则无法阻止。 */
	#if ( ( INCLUDE_xTaskGetSchedulerState == 1 ) || ( configUSE_TIMERS == 1 ) )
	{
		configASSERT( !( ( xTaskGetSchedulerState() == taskSCHEDULER_SUSPENDED ) && ( xTicksToWait != 0 ) ) );
	}
	#endif


	/*lint -save -e904 此函数在某种程
	度上放宽了编码标准，以允许函数本身内的
	return 语句。  这样做是为了提高执行时间效率。 */
	for( ;; )
	{
		taskENTER_CRITICAL();
		{
			/* 信号量是项目大小为 0 的队列，其
			中队列中的消息数是信号量的计数值。 */
			const UBaseType_t uxSemaphoreCount = pxQueue->uxMessagesWaiting;

			/* 现在队列中有数据吗？  要运行的调用任
			务必须是想要访问队列的最高优先级任务。 */
			if( uxSemaphoreCount > ( UBaseType_t ) 0 )
			{
				traceQUEUE_RECEIVE( pxQueue );

				/* 信号量是数据大小为零的队列，其中等待
				的消息是信号量的计数。  减少计数。 */
				pxQueue->uxMessagesWaiting = uxSemaphoreCount - ( UBaseType_t ) 1;

				#if ( configUSE_MUTEXES == 1 )
				{
					if( pxQueue->uxQueueType == queueQUEUE_IS_MUTEX )
					{
						/* 如果有必要，请记录实施
						优先级继承所需的信息。 */
						pxQueue->u.xSemaphore.xMutexHolder = pvTaskIncrementMutexHeldCount();
					}
					else
					{
						mtCOVERAGE_TEST_MARKER();
					}
				}
				#endif /* configUSE_MUTEXES */

				/* 检查其他任务是否被阻塞等待给出信号量，如
				果是，则解除对最高优先级的任务的阻塞。 */
				if( listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToSend ) ) == pdFALSE )
				{
					if( xTaskRemoveFromEventList( &( pxQueue->xTasksWaitingToSend ) ) != pdFALSE )
					{
						queueYIELD_IF_USING_PREEMPTION();
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

				taskEXIT_CRITICAL();
				return pdPASS;
			}
			else
			{
				if( xTicksToWait == ( TickType_t ) 0 )
				{
					/* 要发生继承，必须有一个初始超时，并
					且调整后的超时不能变为 0，因为
					if it were 0 the function would have exited. */
					#if( configUSE_MUTEXES == 1 )
					{
						configASSERT( xInheritanceOccurred == pdFALSE );
					}
					#endif /* configUSE_MUTEXES */

					/* 信号量计数为 0，无块时间为
					specified（或块时间已过期），因此立即退出。 */
					taskEXIT_CRITICAL();
					traceQUEUE_RECEIVE_FAILED( pxQueue );
					return errQUEUE_EMPTY;
				}
				else if( xEntryTimeSet == pdFALSE )
				{
					/* 信号量计数为 0，并且指定了阻塞
					时间，因此配置超时结构准备阻塞。 */
					vTaskInternalSetTimeOutState( &xTimeOut );
					xEntryTimeSet = pdTRUE;
				}
				else
				{
					/* 入场时间已经确定。 */
					mtCOVERAGE_TEST_MARKER();
				}
			}
		}
		taskEXIT_CRITICAL();

		/* 现在临界区已经退出，中断和其他任务可
		以向信号量提供或从信号量获取信号量。 */

		vTaskSuspendAll();
		prvLockQueue( pxQueue );

		/* 更新超时状态以查看是否已过期。 */
		if( xTaskCheckForTimeOut( &xTimeOut, &xTicksToWait ) == pdFALSE )
		{
			/* 指定了出块时间且未过期。  如果信号量
			计数为 0，则进入阻塞状态以等待信号量
			变得可用。  由于信号量是通过队列实现
			的，队列为空相当于信号量计数为 0。 */
			if( prvIsQueueEmpty( pxQueue ) != pdFALSE )
			{
				traceBLOCKING_ON_QUEUE_RECEIVE( pxQueue );

				#if ( configUSE_MUTEXES == 1 )
				{
					if( pxQueue->uxQueueType == queueQUEUE_IS_MUTEX )
					{
						taskENTER_CRITICAL();
						{
							xInheritanceOccurred = xTaskPriorityInherit( pxQueue->u.xSemaphore.xMutexHolder );
						}
						taskEXIT_CRITICAL();
					}
					else
					{
						mtCOVERAGE_TEST_MARKER();
					}
				}
				#endif

				vTaskPlaceOnEventList( &( pxQueue->xTasksWaitingToReceive ), xTicksToWait );
				prvUnlockQueue( pxQueue );
				if( xTaskResumeAll() == pdFALSE )
				{
					portYIELD_WITHIN_API();
				}
				else
				{
					mtCOVERAGE_TEST_MARKER();
				}
			}
			else
			{
				/* 没有超时且信号量计数不为 0
				，因此尝试再次获取信号量。 */
				prvUnlockQueue( pxQueue );
				( void ) xTaskResumeAll();
			}
		}
		else
		{
			/* 超时了。 */
			prvUnlockQueue( pxQueue );
			( void ) xTaskResumeAll();

			/* 如果信号量计数为 0，则由于超时已过而
			立即退出。  否则返回尝试获取已知可用
			的信号量。  由于信号量是通过队列实现
			的，队列为空相当于信号量计数为 0。 */
			if( prvIsQueueEmpty( pxQueue ) != pdFALSE )
			{
				#if ( configUSE_MUTEXES == 1 )
				{
					/* 仅当 pxQueue->uxQueueType ==
					queueQUEUE_IS_MUTEX 时才能设置
					xInheritanceOccurred，因此无需再次测试互斥体类型来检查它实际上是互斥体。 */
					if( xInheritanceOccurred != pdFALSE )
					{
						taskENTER_CRITICAL();
						{
							UBaseType_t uxHighestWaitingPriority;

							/* 此任务在互斥体上阻塞导致另一个任
							务继承此任务的优先级。  现在该
							任务已超时，应再次取消优先级，但
							优先级只能低于正在等待同一互斥锁
							的下一个最高优先级任务。 */
							uxHighestWaitingPriority = prvGetDisinheritPriorityAfterTimeout( pxQueue );
							vTaskPriorityDisinheritAfterTimeout( pxQueue->u.xSemaphore.xMutexHolder, uxHighestWaitingPriority );
						}
						taskEXIT_CRITICAL();
					}
				}
				#endif /* configUSE_MUTEXES */

				traceQUEUE_RECEIVE_FAILED( pxQueue );
				return errQUEUE_EMPTY;
			}
			else
			{
				mtCOVERAGE_TEST_MARKER();
			}
		}
	} /*lint - 恢复 */
}
/*-----------------------------------------------------------*/

BaseType_t xQueuePeek( QueueHandle_t xQueue, void * const pvBuffer, TickType_t xTicksToWait )
{
BaseType_t xEntryTimeSet = pdFALSE;
TimeOut_t xTimeOut;
int8_t *pcOriginalReadPosition;
Queue_t * const pxQueue = xQueue;

	/* 检查指针不是NULL。 */
	configASSERT( ( pxQueue ) );

	/* 如果数据大小为 zero，则接收数据的缓冲区只
	能为 NULL（因此不会将数据复制到缓冲区中。 */
	configASSERT( !( ( ( pvBuffer ) == NULL ) && ( ( pxQueue )->uxItemSize != ( UBaseType_t ) 0U ) ) );

	/* 如果调度程序暂停，则无法阻止。 */
	#if ( ( INCLUDE_xTaskGetSchedulerState == 1 ) || ( configUSE_TIMERS == 1 ) )
	{
		configASSERT( !( ( xTaskGetSchedulerState() == taskSCHEDULER_SUSPENDED ) && ( xTicksToWait != 0 ) ) );
	}
	#endif


	/*lint -save -e904 此函数在某种程
	度上放宽了编码标准，以允许函数本身内的
	return 语句。  这样做是为了提高执行时间效率。 */
	for( ;; )
	{
		taskENTER_CRITICAL();
		{
			const UBaseType_t uxMessagesWaiting = pxQueue->uxMessagesWaiting;

			/* 现在队列中有数据吗？  要运行的调用任
			务必须是想要访问队列的最高优先级任务。 */
			if( uxMessagesWaiting > ( UBaseType_t ) 0 )
			{
				/* 记住读取位置，以便在从队列中读
				取数据后可以重置它，因为此函数
				只是查看数据，而不是删除数据。 */
				pcOriginalReadPosition = pxQueue->u.xQueue.pcReadFrom;

				prvCopyDataFromQueue( pxQueue, pvBuffer );
				traceQUEUE_PEEK( pxQueue );

				/* 数据没有被删除，因此重置读指针。 */
				pxQueue->u.xQueue.pcReadFrom = pcOriginalReadPosition;

				/* 数据正在留在队列中，因此请查看
				是否有其他任务正在等待该数据。 */
				if( listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToReceive ) ) == pdFALSE )
				{
					if( xTaskRemoveFromEventList( &( pxQueue->xTasksWaitingToReceive ) ) != pdFALSE )
					{
						/* 等待的任务比该任务具有更高的优先级。 */
						queueYIELD_IF_USING_PREEMPTION();
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

				taskEXIT_CRITICAL();
				return pdPASS;
			}
			else
			{
				if( xTicksToWait == ( TickType_t ) 0 )
				{
					/* 队列为空且无阻塞时间为
					specified（或阻塞时间已过期），因此立即离开。 */
					taskEXIT_CRITICAL();
					traceQUEUE_PEEK_FAILED( pxQueue );
					return errQUEUE_EMPTY;
				}
				else if( xEntryTimeSet == pdFALSE )
				{
					/* 队列为空并且指定了阻塞时间所
					以配置超时结构准备进入阻塞
					状态。 */
					vTaskInternalSetTimeOutState( &xTimeOut );
					xEntryTimeSet = pdTRUE;
				}
				else
				{
					/* 入场时间已经确定。 */
					mtCOVERAGE_TEST_MARKER();
				}
			}
		}
		taskEXIT_CRITICAL();

		/* 现在临界区已经退出，中断和其
		他任务可以向队列发送和接收。 */

		vTaskSuspendAll();
		prvLockQueue( pxQueue );

		/* 更新超时状态以查看是否已过期。 */
		if( xTaskCheckForTimeOut( &xTimeOut, &xTicksToWait ) == pdFALSE )
		{
			/* Timeout还没到，检查队列里现在是否有数据
			，如果没有就进入Blocked状态等待数据。 */
			if( prvIsQueueEmpty( pxQueue ) != pdFALSE )
			{
				traceBLOCKING_ON_QUEUE_PEEK( pxQueue );
				vTaskPlaceOnEventList( &( pxQueue->xTasksWaitingToReceive ), xTicksToWait );
				prvUnlockQueue( pxQueue );
				if( xTaskResumeAll() == pdFALSE )
				{
					portYIELD_WITHIN_API();
				}
				else
				{
					mtCOVERAGE_TEST_MARKER();
				}
			}
			else
			{
				/* 现在队列中有数据，所以不要进入阻
				塞状态，而是返回尝试获取数据。 */
				prvUnlockQueue( pxQueue );
				( void ) xTaskResumeAll();
			}
		}
		else
		{
			/* 超时时间已过。  如果队列中仍然没有数
			据则退出，否则返回尝试再次读取数据。 */
			prvUnlockQueue( pxQueue );
			( void ) xTaskResumeAll();

			if( prvIsQueueEmpty( pxQueue ) != pdFALSE )
			{
				traceQUEUE_PEEK_FAILED( pxQueue );
				return errQUEUE_EMPTY;
			}
			else
			{
				mtCOVERAGE_TEST_MARKER();
			}
		}
	} /*lint - 恢复 */
}
/*-----------------------------------------------------------*/

BaseType_t xQueueReceiveFromISR( QueueHandle_t xQueue, void * const pvBuffer, BaseType_t * const pxHigherPriorityTaskWoken )
{
BaseType_t xReturn;
UBaseType_t uxSavedInterruptStatus;
Queue_t * const pxQueue = xQueue;

	configASSERT( pxQueue );
	configASSERT( !( ( pvBuffer == NULL ) && ( pxQueue->uxItemSize != ( UBaseType_t ) 0U ) ) );

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
	：http://www.freertos.org/RTOS-Cortex-M3-M4.html */
	portASSERT_IF_INTERRUPT_PRIORITY_INVALID();

	uxSavedInterruptStatus = portSET_INTERRUPT_MASK_FROM_ISR();
	{
		const UBaseType_t uxMessagesWaiting = pxQueue->uxMessagesWaiting;

		/* 无法阻止 ISR，因此请检查是否有可用数据。 */
		if( uxMessagesWaiting > ( UBaseType_t ) 0 )
		{
			const int8_t cRxLock = pxQueue->cRxLock;

			traceQUEUE_RECEIVE_FROM_ISR( pxQueue );

			prvCopyDataFromQueue( pxQueue, pvBuffer );
			pxQueue->uxMessagesWaiting = uxMessagesWaiting - ( UBaseType_t ) 1;

			/* 如果队列被锁定，事件列表将不会被修改。相
			反，更新锁定计数，以便解锁队列的任务将知
			道 ISR 在队列被删除时已删除数据。
			锁定。 */
			if( cRxLock == queueUNLOCKED )
			{
				if( listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToSend ) ) == pdFALSE )
				{
					if( xTaskRemoveFromEventList( &( pxQueue->xTasksWaitingToSend ) ) != pdFALSE )
					{
						/* 等待的任务比我们具有更高的优先
						级，因此强制进行上下文切换。 */
						if( pxHigherPriorityTaskWoken != NULL )
						{
							*pxHigherPriorityTaskWoken = pdTRUE;
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
				/* 增加锁定计数，以便解锁队列的任
				务知道数据在锁定时已被删除。 */
				pxQueue->cRxLock = ( int8_t ) ( cRxLock + 1 );
			}

			xReturn = pdPASS;
		}
		else
		{
			xReturn = pdFAIL;
			traceQUEUE_RECEIVE_FROM_ISR_FAILED( pxQueue );
		}
	}
	portCLEAR_INTERRUPT_MASK_FROM_ISR( uxSavedInterruptStatus );

	return xReturn;
}
/*-----------------------------------------------------------*/

BaseType_t xQueuePeekFromISR( QueueHandle_t xQueue,  void * const pvBuffer )
{
BaseType_t xReturn;
UBaseType_t uxSavedInterruptStatus;
int8_t *pcOriginalReadPosition;
Queue_t * const pxQueue = xQueue;

	configASSERT( pxQueue );
	configASSERT( !( ( pvBuffer == NULL ) && ( pxQueue->uxItemSize != ( UBaseType_t ) 0U ) ) );
	configASSERT( pxQueue->uxItemSize != 0 ); /* 无法查看信号量。 */

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
	：http://www.freertos.org/RTOS-Cortex-M3-M4.html */
	portASSERT_IF_INTERRUPT_PRIORITY_INVALID();

	uxSavedInterruptStatus = portSET_INTERRUPT_MASK_FROM_ISR();
	{
		/* 无法阻止 ISR，因此请检查是否有可用数据。 */
		if( pxQueue->uxMessagesWaiting > ( UBaseType_t ) 0 )
		{
			traceQUEUE_PEEK_FROM_ISR( pxQueue );

			/* 记住读取位置，以便可以重置它，因为
			实际上没有从队列中删除任何内容。 */
			pcOriginalReadPosition = pxQueue->u.xQueue.pcReadFrom;
			prvCopyDataFromQueue( pxQueue, pvBuffer );
			pxQueue->u.xQueue.pcReadFrom = pcOriginalReadPosition;

			xReturn = pdPASS;
		}
		else
		{
			xReturn = pdFAIL;
			traceQUEUE_PEEK_FROM_ISR_FAILED( pxQueue );
		}
	}
	portCLEAR_INTERRUPT_MASK_FROM_ISR( uxSavedInterruptStatus );

	return xReturn;
}
/*-----------------------------------------------------------*/

UBaseType_t uxQueueMessagesWaiting( const QueueHandle_t xQueue )
{
UBaseType_t uxReturn;

	configASSERT( xQueue );

	taskENTER_CRITICAL();
	{
		uxReturn = ( ( Queue_t * ) xQueue )->uxMessagesWaiting;
	}
	taskEXIT_CRITICAL();

	return uxReturn;
} /*lint !e818 指针不能声明为 const，因为 xQueue 是 typedef 而不是指针。 */
/*-----------------------------------------------------------*/

UBaseType_t uxQueueSpacesAvailable( const QueueHandle_t xQueue )
{
UBaseType_t uxReturn;
Queue_t * const pxQueue = xQueue;

	configASSERT( pxQueue );

	taskENTER_CRITICAL();
	{
		uxReturn = pxQueue->uxLength - pxQueue->uxMessagesWaiting;
	}
	taskEXIT_CRITICAL();

	return uxReturn;
} /*lint !e818 指针不能声明为 const，因为 xQueue 是 typedef 而不是指针。 */
/*-----------------------------------------------------------*/

UBaseType_t uxQueueMessagesWaitingFromISR( const QueueHandle_t xQueue )
{
UBaseType_t uxReturn;
Queue_t * const pxQueue = xQueue;

	configASSERT( pxQueue );
	uxReturn = pxQueue->uxMessagesWaiting;

	return uxReturn;
} /*lint !e818 指针不能声明为 const，因为 xQueue 是 typedef 而不是指针。 */
/*-----------------------------------------------------------*/

void vQueueDelete( QueueHandle_t xQueue )
{
Queue_t * const pxQueue = xQueue;

	configASSERT( pxQueue );
	traceQUEUE_DELETE( pxQueue );

	#if ( configQUEUE_REGISTRY_SIZE > 0 )
	{
		vQueueUnregisterQueue( pxQueue );
	}
	#endif

	#if( ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) && ( configSUPPORT_STATIC_ALLOCATION == 0 ) )
	{
		/* 队列只能动态分配 - 释放它
		再次。 */
		vPortFree( pxQueue );
	}
	#elif( ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) && ( configSUPPORT_STATIC_ALLOCATION == 1 ) )
	{
		/* 队列可以静态或动态分配，因此
		在尝试释放内存之前进行检查。 */
		if( pxQueue->ucStaticallyAllocated == ( uint8_t ) pdFALSE )
		{
			vPortFree( pxQueue );
		}
		else
		{
			mtCOVERAGE_TEST_MARKER();
		}
	}
	#else
	{
		/* 该队列必须是静态分配的，因此不会被删除。
		避免编译器对未使用的参数发出警告。 */
		( void ) pxQueue;
	}
	#endif /* configSUPPORT_DYNAMIC_ALLOCATION */
}
/*-----------------------------------------------------------*/

#if ( configUSE_TRACE_FACILITY == 1 )

	UBaseType_t uxQueueGetQueueNumber( QueueHandle_t xQueue )
	{
		return ( ( Queue_t * ) xQueue )->uxQueueNumber;
	}

#endif /* configUSE_TRACE_FACILITY */
/*-----------------------------------------------------------*/

#if ( configUSE_TRACE_FACILITY == 1 )

	void vQueueSetQueueNumber( QueueHandle_t xQueue, UBaseType_t uxQueueNumber )
	{
		( ( Queue_t * ) xQueue )->uxQueueNumber = uxQueueNumber;
	}

#endif /* configUSE_TRACE_FACILITY */
/*-----------------------------------------------------------*/

#if ( configUSE_TRACE_FACILITY == 1 )

	uint8_t ucQueueGetQueueType( QueueHandle_t xQueue )
	{
		return ( ( Queue_t * ) xQueue )->ucQueueType;
	}

#endif /* configUSE_TRACE_FACILITY */
/*-----------------------------------------------------------*/

#if( configUSE_MUTEXES == 1 )

	static UBaseType_t prvGetDisinheritPriorityAfterTimeout( const Queue_t * const pxQueue )
	{
	UBaseType_t uxHighestPriorityOfWaitingTasks;

		/* 如果等待互斥锁的任务导致互斥锁持有者继承
		优先级，但等待任务超时，则持有者应取消继
		承优先级 - 但只能降低到等待同一互斥锁
		的任何其他任务的最高优先级。  为此，
		return the priority of the highest priority task that is waiting for the
		互斥体。 */
		if( listCURRENT_LIST_LENGTH( &( pxQueue->xTasksWaitingToReceive ) ) > 0U )
		{
			uxHighestPriorityOfWaitingTasks = ( UBaseType_t ) configMAX_PRIORITIES - ( UBaseType_t ) listGET_ITEM_VALUE_OF_HEAD_ENTRY( &( pxQueue->xTasksWaitingToReceive ) );
		}
		else
		{
			uxHighestPriorityOfWaitingTasks = tskIDLE_PRIORITY;
		}

		return uxHighestPriorityOfWaitingTasks;
	}

#endif /* configUSE_MUTEXES */
/*-----------------------------------------------------------*/

static BaseType_t prvCopyDataToQueue( Queue_t * const pxQueue, const void *pvItemToQueue, const BaseType_t xPosition )
{
BaseType_t xReturn = pdFALSE;
UBaseType_t uxMessagesWaiting;

	/* 该函数是从临界区调用的。 */

	uxMessagesWaiting = pxQueue->uxMessagesWaiting;

	if( pxQueue->uxItemSize == ( UBaseType_t ) 0 )
	{
		#if ( configUSE_MUTEXES == 1 )
		{
			if( pxQueue->uxQueueType == queueQUEUE_IS_MUTEX )
			{
				/* 互斥体不再被持有。 */
				xReturn = xTaskPriorityDisinherit( pxQueue->u.xSemaphore.xMutexHolder );
				pxQueue->u.xSemaphore.xMutexHolder = NULL;
			}
			else
			{
				mtCOVERAGE_TEST_MARKER();
			}
		}
		#endif /* configUSE_MUTEXES */
	}
	else if( xPosition == queueSEND_TO_BACK )
	{
		( void ) memcpy( ( void * ) pxQueue->pcWriteTo, pvItemToQueue, ( size_t ) pxQueue->uxItemSize ); /*lint !e961 !e418 !e9087 MISRA 异常，因为转换仅对某些端口来说是多余的，加上先前的逻辑确保空指针只能在副本大小为 0 的情况下传递到 memcpy()。转换为函数签名所需的 void，并且安全，因为没有对齐要求并且以字节为单位指定复制长度。 */
		pxQueue->pcWriteTo += pxQueue->uxItemSize; /*lint !e9016 char 类型上的指针算术没问题，特别是在这个用例中，它是传达意图的最清晰方式。 */
		if( pxQueue->pcWriteTo >= pxQueue->u.xQueue.pcTail ) /*lint !e946 MISRA 异常合理，因为指针比较是最干净的解决方案。 */
		{
			pxQueue->pcWriteTo = pxQueue->pcHead;
		}
		else
		{
			mtCOVERAGE_TEST_MARKER();
		}
	}
	else
	{
		( void ) memcpy( ( void * ) pxQueue->u.xQueue.pcReadFrom, pvItemToQueue, ( size_t ) pxQueue->uxItemSize ); /*lint !e961 !e9087 !e418 MISRA 例外，因为强制转换仅对某些端口是多余的。  转换为函数签名所需的 void，并且安全，因为没有对齐要求并且以字节为单位指定复制长度。  仅当长度为 0 时才使用断言检查空指针。 */
		pxQueue->u.xQueue.pcReadFrom -= pxQueue->uxItemSize;
		if( pxQueue->u.xQueue.pcReadFrom < pxQueue->pcHead ) /*lint !e946 MISRA 异常合理，因为指针比较是最干净的解决方案。 */
		{
			pxQueue->u.xQueue.pcReadFrom = ( pxQueue->u.xQueue.pcTail - pxQueue->uxItemSize );
		}
		else
		{
			mtCOVERAGE_TEST_MARKER();
		}

		if( xPosition == queueOVERWRITE )
		{
			if( uxMessagesWaiting > ( UBaseType_t ) 0 )
			{
				/* 项目不是被添加而是被覆盖，因此从队列
				中记录的项目数中减去 1，这样当再次
				添加 1 时，记录的项目数仍然保留
				正确的。 */
				--uxMessagesWaiting;
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

	pxQueue->uxMessagesWaiting = uxMessagesWaiting + ( UBaseType_t ) 1;

	return xReturn;
}
/*-----------------------------------------------------------*/

static void prvCopyDataFromQueue( Queue_t * const pxQueue, void * const pvBuffer )
{
	if( pxQueue->uxItemSize != ( UBaseType_t ) 0 )
	{
		pxQueue->u.xQueue.pcReadFrom += pxQueue->uxItemSize; /*lint !e9016 char 类型上的指针算术没问题，特别是在这个用例中，它是传达意图的最清晰方式。 */
		if( pxQueue->u.xQueue.pcReadFrom >= pxQueue->u.xQueue.pcTail ) /*lint !e946 MISRA 异常合理，因为使用关系运算符是最简洁的解决方案。 */
		{
			pxQueue->u.xQueue.pcReadFrom = pxQueue->pcHead;
		}
		else
		{
			mtCOVERAGE_TEST_MARKER();
		}
		( void ) memcpy( ( void * ) pvBuffer, ( void * ) pxQueue->u.xQueue.pcReadFrom, ( size_t ) pxQueue->uxItemSize ); /*lint !e961 !e418 !e9087 MISRA 例外，因为强制转换仅对某些端口是多余的。  此外，先前的逻辑还确保当计数为 0 时，空指针只能传递给 memcpy()。根据函数签名要求转换为 void，并且安全，因为没有对齐要求并且以字节为单位指定复制长度。 */
	}
}
/*-----------------------------------------------------------*/

static void prvUnlockQueue( Queue_t * const pxQueue )
{
	/* 该函数必须在调度程序挂起的情况下调用。 */

	/* 锁定计数包含队列锁定时放入队列或从队列中删
	除的额外数据项的数量。  当队列被锁定时，
	可以添加或删除项目，但无法更改事件列表
	已更新。 */
	taskENTER_CRITICAL();
	{
		int8_t cTxLock = pxQueue->cTxLock;

		/* 查看队列锁定时数据是否添加到队列中。 */
		while( cTxLock > queueLOCKED_UNMODIFIED )
		{
			/* 队列被锁定时数据被发布。  是否
			有任何任务被阻止等待数据可用？ */
			#if ( configUSE_QUEUE_SETS == 1 )
			{
				if( pxQueue->pxQueueSetContainer != NULL )
				{
					if( prvNotifyQueueSetContainer( pxQueue ) != pdFALSE )
					{
						/* 该队列是队列集的成员，向队列集
						发送消息会导致更高优先级的任务
						解除阻塞。需要上下文切换。 */
						vTaskMissedYield();
					}
					else
					{
						mtCOVERAGE_TEST_MARKER();
					}
				}
				else
				{
					/* 从事件列表中删除的任务将被添加到挂起
					的就绪列表中，因为调度程序仍在运行
					暂停。 */
					if( listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToReceive ) ) == pdFALSE )
					{
						if( xTaskRemoveFromEventList( &( pxQueue->xTasksWaitingToReceive ) ) != pdFALSE )
						{
							/* 等待的任务优先级较高，因
							此记录需要上下文切换。 */
							vTaskMissedYield();
						}
						else
						{
							mtCOVERAGE_TEST_MARKER();
						}
					}
					else
					{
						break;
					}
				}
			}
			#else /* configUSE_QUEUE_SETS */
			{
				/* 由于调度程序仍处于挂起状态，因此从事件列
				表中删除的任务将添加到待处理就绪列表中。 */
				if( listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToReceive ) ) == pdFALSE )
				{
					if( xTaskRemoveFromEventList( &( pxQueue->xTasksWaitingToReceive ) ) != pdFALSE )
					{
						/* 等待的任务优先级较高，因
						此记录需要上下文切换。 */
						vTaskMissedYield();
					}
					else
					{
						mtCOVERAGE_TEST_MARKER();
					}
				}
				else
				{
					break;
				}
			}
			#endif /* configUSE_QUEUE_SETS */

			--cTxLock;
		}

		pxQueue->cTxLock = queueUNLOCKED;
	}
	taskEXIT_CRITICAL();

	/* 对 Rx 锁执行相同操作。 */
	taskENTER_CRITICAL();
	{
		int8_t cRxLock = pxQueue->cRxLock;

		while( cRxLock > queueLOCKED_UNMODIFIED )
		{
			if( listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToSend ) ) == pdFALSE )
			{
				if( xTaskRemoveFromEventList( &( pxQueue->xTasksWaitingToSend ) ) != pdFALSE )
				{
					vTaskMissedYield();
				}
				else
				{
					mtCOVERAGE_TEST_MARKER();
				}

				--cRxLock;
			}
			else
			{
				break;
			}
		}

		pxQueue->cRxLock = queueUNLOCKED;
	}
	taskEXIT_CRITICAL();
}
/*-----------------------------------------------------------*/

static BaseType_t prvIsQueueEmpty( const Queue_t *pxQueue )
{
BaseType_t xReturn;

	taskENTER_CRITICAL();
	{
		if( pxQueue->uxMessagesWaiting == ( UBaseType_t )  0 )
		{
			xReturn = pdTRUE;
		}
		else
		{
			xReturn = pdFALSE;
		}
	}
	taskEXIT_CRITICAL();

	return xReturn;
}
/*-----------------------------------------------------------*/

BaseType_t xQueueIsQueueEmptyFromISR( const QueueHandle_t xQueue )
{
BaseType_t xReturn;
Queue_t * const pxQueue = xQueue;

	configASSERT( pxQueue );
	if( pxQueue->uxMessagesWaiting == ( UBaseType_t ) 0 )
	{
		xReturn = pdTRUE;
	}
	else
	{
		xReturn = pdFALSE;
	}

	return xReturn;
} /*lint !e818 xQueue 不能是指向 const 的指针，因为它是 typedef。 */
/*-----------------------------------------------------------*/

static BaseType_t prvIsQueueFull( const Queue_t *pxQueue )
{
BaseType_t xReturn;

	taskENTER_CRITICAL();
	{
		if( pxQueue->uxMessagesWaiting == pxQueue->uxLength )
		{
			xReturn = pdTRUE;
		}
		else
		{
			xReturn = pdFALSE;
		}
	}
	taskEXIT_CRITICAL();

	return xReturn;
}
/*-----------------------------------------------------------*/

BaseType_t xQueueIsQueueFullFromISR( const QueueHandle_t xQueue )
{
BaseType_t xReturn;
Queue_t * const pxQueue = xQueue;

	configASSERT( pxQueue );
	if( pxQueue->uxMessagesWaiting == pxQueue->uxLength )
	{
		xReturn = pdTRUE;
	}
	else
	{
		xReturn = pdFALSE;
	}

	return xReturn;
} /*lint !e818 xQueue 不能是指向 const 的指针，因为它是 typedef。 */
/*-----------------------------------------------------------*/

#if ( configUSE_CO_ROUTINES == 1 )

	BaseType_t xQueueCRSend( QueueHandle_t xQueue, const void *pvItemToQueue, TickType_t xTicksToWait )
	{
	BaseType_t xReturn;
	Queue_t * const pxQueue = xQueue;

		/* 如果队列已经满了，我们可能不得不阻塞。  需
		要一个关键部分来防止中断在检查队列是否已满以
		及队列是否阻塞之间从队列中删除某些内容。 */
		portDISABLE_INTERRUPTS();
		{
			if( prvIsQueueFull( pxQueue ) != pdFALSE )
			{
				/* 队列已满 - 我们要阻止
				还是直接离开而不发帖？ */
				if( xTicksToWait > ( TickType_t ) 0 )
				{
					/* 由于这是从协程调用的，因此我们不能直接阻止，但是
					return indicating that we need to block. */
					vCoRoutineAddToDelayedList( xTicksToWait, &( pxQueue->xTasksWaitingToSend ) );
					portENABLE_INTERRUPTS();
					return errQUEUE_BLOCKED;
				}
				else
				{
					portENABLE_INTERRUPTS();
					return errQUEUE_FULL;
				}
			}
		}
		portENABLE_INTERRUPTS();

		portDISABLE_INTERRUPTS();
		{
			if( pxQueue->uxMessagesWaiting < pxQueue->uxLength )
			{
				/* 队列有空间，将数据复制到队列中。 */
				prvCopyDataToQueue( pxQueue, pvItemToQueue, queueSEND_TO_BACK );
				xReturn = pdPASS;

				/* 是否有 co-routines 正在等待数据可用？ */
				if( listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToReceive ) ) == pdFALSE )
				{
					/* 在这种情况下，co-routine 可
					以直接放入就绪列表中，因为我们位于关键
					部分。相反，使用相同的挂起就绪列表机制
					，就好像事件是由中断引起的一样。 */
					if( xCoRoutineRemoveFromEventList( &( pxQueue->xTasksWaitingToReceive ) ) != pdFALSE )
					{
						/* co-routine 等待具有更高的
						优先级，因此记录产量可能是合适的。 */
						xReturn = errQUEUE_YIELD;
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
				xReturn = errQUEUE_FULL;
			}
		}
		portENABLE_INTERRUPTS();

		return xReturn;
	}

#endif /* configUSE_CO_ROUTINES */
/*-----------------------------------------------------------*/

#if ( configUSE_CO_ROUTINES == 1 )

	BaseType_t xQueueCRReceive( QueueHandle_t xQueue, void *pvBuffer, TickType_t xTicksToWait )
	{
	BaseType_t xReturn;
	Queue_t * const pxQueue = xQueue;

		/* 如果队列已经空了，我们可能不得不阻塞。
		需要一个临界区来防止在检查队列是否为空和阻
		塞队列之间添加某些内容到队列中的中断。 */
		portDISABLE_INTERRUPTS();
		{
			if( pxQueue->uxMessagesWaiting == ( UBaseType_t ) 0 )
			{
				/* 队列中没有消息，我们是要
				阻塞还是什么都没有离开？ */
				if( xTicksToWait > ( TickType_t ) 0 )
				{
					/* 由于这是一个 co-routine 我们
					不能直接阻止，但返回表明我们需要阻止。 */
					vCoRoutineAddToDelayedList( xTicksToWait, &( pxQueue->xTasksWaitingToReceive ) );
					portENABLE_INTERRUPTS();
					return errQUEUE_BLOCKED;
				}
				else
				{
					portENABLE_INTERRUPTS();
					return errQUEUE_FULL;
				}
			}
			else
			{
				mtCOVERAGE_TEST_MARKER();
			}
		}
		portENABLE_INTERRUPTS();

		portDISABLE_INTERRUPTS();
		{
			if( pxQueue->uxMessagesWaiting > ( UBaseType_t ) 0 )
			{
				/* 可以从队列中获取数据。 */
				pxQueue->u.xQueue.pcReadFrom += pxQueue->uxItemSize;
				if( pxQueue->u.xQueue.pcReadFrom >= pxQueue->u.xQueue.pcTail )
				{
					pxQueue->u.xQueue.pcReadFrom = pxQueue->pcHead;
				}
				else
				{
					mtCOVERAGE_TEST_MARKER();
				}
				--( pxQueue->uxMessagesWaiting );
				( void ) memcpy( ( void * ) pvBuffer, ( void * ) pxQueue->u.xQueue.pcReadFrom, ( unsigned ) pxQueue->uxItemSize );

				xReturn = pdPASS;

				/* 是否有 co-routines 正在等待可用空间？ */
				if( listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToSend ) ) == pdFALSE )
				{
					/* 在这种情况下，co-routine 可
					以直接放入就绪列表中，因为我们位于关键
					部分。相反，使用相同的挂起就绪列表机制
					，就好像事件是由中断引起的一样。 */
					if( xCoRoutineRemoveFromEventList( &( pxQueue->xTasksWaitingToSend ) ) != pdFALSE )
					{
						xReturn = errQUEUE_YIELD;
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
				xReturn = pdFAIL;
			}
		}
		portENABLE_INTERRUPTS();

		return xReturn;
	}

#endif /* configUSE_CO_ROUTINES */
/*-----------------------------------------------------------*/

#if ( configUSE_CO_ROUTINES == 1 )

	BaseType_t xQueueCRSendFromISR( QueueHandle_t xQueue, const void *pvItemToQueue, BaseType_t xCoRoutinePreviouslyWoken )
	{
	Queue_t * const pxQueue = xQueue;

		/* 无法在 ISR 内阻塞，因此如果队列
		上没有空间，则退出而不执行任何操作。 */
		if( pxQueue->uxMessagesWaiting < pxQueue->uxLength )
		{
			prvCopyDataToQueue( pxQueue, pvItemToQueue, queueSEND_TO_BACK );

			/* 我们只想为每个 ISR 唤醒一个 co-routine
			，因此请检查 co-routine 是否尚未被唤醒。 */
			if( xCoRoutinePreviouslyWoken == pdFALSE )
			{
				if( listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToReceive ) ) == pdFALSE )
				{
					if( xCoRoutineRemoveFromEventList( &( pxQueue->xTasksWaitingToReceive ) ) != pdFALSE )
					{
						return pdTRUE;
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

		return xCoRoutinePreviouslyWoken;
	}

#endif /* configUSE_CO_ROUTINES */
/*-----------------------------------------------------------*/

#if ( configUSE_CO_ROUTINES == 1 )

	BaseType_t xQueueCRReceiveFromISR( QueueHandle_t xQueue, void *pvBuffer, BaseType_t *pxCoRoutineWoken )
	{
	BaseType_t xReturn;
	Queue_t * const pxQueue = xQueue;

		/* 我们无法阻止 ISR，因此请检查是否有可
		用数据。如果没有，那就什么都不做就离开。 */
		if( pxQueue->uxMessagesWaiting > ( UBaseType_t ) 0 )
		{
			/* 从队列中复制数据。 */
			pxQueue->u.xQueue.pcReadFrom += pxQueue->uxItemSize;
			if( pxQueue->u.xQueue.pcReadFrom >= pxQueue->u.xQueue.pcTail )
			{
				pxQueue->u.xQueue.pcReadFrom = pxQueue->pcHead;
			}
			else
			{
				mtCOVERAGE_TEST_MARKER();
			}
			--( pxQueue->uxMessagesWaiting );
			( void ) memcpy( ( void * ) pvBuffer, ( void * ) pxQueue->u.xQueue.pcReadFrom, ( unsigned ) pxQueue->uxItemSize );

			if( ( *pxCoRoutineWoken ) == pdFALSE )
			{
				if( listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToSend ) ) == pdFALSE )
				{
					if( xCoRoutineRemoveFromEventList( &( pxQueue->xTasksWaitingToSend ) ) != pdFALSE )
					{
						*pxCoRoutineWoken = pdTRUE;
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

			xReturn = pdPASS;
		}
		else
		{
			xReturn = pdFAIL;
		}

		return xReturn;
	}

#endif /* configUSE_CO_ROUTINES */
/*-----------------------------------------------------------*/

#if ( configQUEUE_REGISTRY_SIZE > 0 )

	void vQueueAddToRegistry( QueueHandle_t xQueue, const char *pcQueueName ) /*lint !e971 仅字符串和单个字符允许使用非限定字符类型。 */
	{
	UBaseType_t ux;

		/* 查看注册表中是否有空白区域。
		NULL 名称表示空闲插槽。 */
		for( ux = ( UBaseType_t ) 0U; ux < ( UBaseType_t ) configQUEUE_REGISTRY_SIZE; ux++ )
		{
			if( xQueueRegistry[ ux ].pcQueueName == NULL )
			{
				/* 将信息存储在此队列上。 */
				xQueueRegistry[ ux ].pcQueueName = pcQueueName;
				xQueueRegistry[ ux ].xHandle = xQueue;

				traceQUEUE_REGISTRY_ADD( xQueue, pcQueueName );
				break;
			}
			else
			{
				mtCOVERAGE_TEST_MARKER();
			}
		}
	}

#endif /* configQUEUE_REGISTRY_SIZE */
/*-----------------------------------------------------------*/

#if ( configQUEUE_REGISTRY_SIZE > 0 )

	const char *pcQueueGetName( QueueHandle_t xQueue ) /*lint !e971 仅字符串和单个字符允许使用非限定字符类型。 */
	{
	UBaseType_t ux;
	const char *pcReturn = NULL; /*lint !e971 仅字符串和单个字符允许使用非限定字符类型。 */

		/* 请注意，这里没有任何措施可以防止其他任
		务在搜索注册表时添加或删除注册表项。 */
		for( ux = ( UBaseType_t ) 0U; ux < ( UBaseType_t ) configQUEUE_REGISTRY_SIZE; ux++ )
		{
			if( xQueueRegistry[ ux ].xHandle == xQueue )
			{
				pcReturn = xQueueRegistry[ ux ].pcQueueName;
				break;
			}
			else
			{
				mtCOVERAGE_TEST_MARKER();
			}
		}

		return pcReturn;
	} /*lint !e818 xQueue 不能是指向 const 的指针，因为它是 typedef。 */

#endif /* configQUEUE_REGISTRY_SIZE */
/*-----------------------------------------------------------*/

#if ( configQUEUE_REGISTRY_SIZE > 0 )

	void vQueueUnregisterQueue( QueueHandle_t xQueue )
	{
	UBaseType_t ux;

		/* 查看正在注销的队列句柄是否实际在
		注册表。 */
		for( ux = ( UBaseType_t ) 0U; ux < ( UBaseType_t ) configQUEUE_REGISTRY_SIZE; ux++ )
		{
			if( xQueueRegistry[ ux ].xHandle == xQueue )
			{
				/* 将名称设置为 NULL 以表明该插槽是否再次空闲。 */
				xQueueRegistry[ ux ].pcQueueName = NULL;

				/* 将句柄设置为NULL，以确保在添
				加、删除然后再次添加时，相同的队
				列句柄不会在注册表中出现两次。 */
				xQueueRegistry[ ux ].xHandle = ( QueueHandle_t ) 0;
				break;
			}
			else
			{
				mtCOVERAGE_TEST_MARKER();
			}
		}

	} /*lint !e818 xQueue 不能是指向 const 的指针，因为它是 typedef。 */

#endif /* configQUEUE_REGISTRY_SIZE */
/*-----------------------------------------------------------*/

#if ( configUSE_TIMERS == 1 )

	void vQueueWaitForMessageRestricted( QueueHandle_t xQueue, TickType_t xTicksToWait, const BaseType_t xWaitIndefinitely )
	{
	Queue_t * const pxQueue = xQueue;

		/* 该函数不应由应用程序代码调用，因此其名称中包含“
		Restricted”。  它不是公共 API 的一部
		分。  它是为内核代码使用而设计的，并且有特殊的调用要
		求。它可能会导致 vListInsert() 在一个列
		表上调用，该列表中只能有一项，因此该列表会很快，但即使
		如此，也应该在调度程序锁定的情况下调用它，而不是从关键的
		部分。 */

		/* 仅当队列中没有消息时才执行任何操作。  该函
		数实际上不会导致任务阻塞，只是将其放入阻塞列
		表中。  它不会阻塞，直到调度程序被解锁——
		此时将执行yield。  如果在队列锁定时将
		项目添加到队列中，并且调用任务在队列上阻塞，
		则当队列解锁时，调用任务将立即解除阻塞。 */
		prvLockQueue( pxQueue );
		if( pxQueue->uxMessagesWaiting == ( UBaseType_t ) 0U )
		{
			/* 队列中没有任何内容，在指定时间内阻塞。 */
			vTaskPlaceOnEventListRestricted( &( pxQueue->xTasksWaitingToReceive ), xTicksToWait, xWaitIndefinitely );
		}
		else
		{
			mtCOVERAGE_TEST_MARKER();
		}
		prvUnlockQueue( pxQueue );
	}

#endif /* configUSE_TIMERS */
/*-----------------------------------------------------------*/

#if( ( configUSE_QUEUE_SETS == 1 ) && ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) )

	QueueSetHandle_t xQueueCreateSet( const UBaseType_t uxEventQueueLength )
	{
	QueueSetHandle_t pxQueue;

		pxQueue = xQueueGenericCreate( uxEventQueueLength, ( UBaseType_t ) sizeof( Queue_t * ), queueQUEUE_TYPE_SET );

		return pxQueue;
	}

#endif /* configUSE_QUEUE_SETS */
/*-----------------------------------------------------------*/

#if ( configUSE_QUEUE_SETS == 1 )

	BaseType_t xQueueAddToSet( QueueSetMemberHandle_t xQueueOrSemaphore, QueueSetHandle_t xQueueSet )
	{
	BaseType_t xReturn;

		taskENTER_CRITICAL();
		{
			if( ( ( Queue_t * ) xQueueOrSemaphore )->pxQueueSetContainer != NULL )
			{
				/* 无法将队列/信号量添加到多个队列集。 */
				xReturn = pdFAIL;
			}
			else if( ( ( Queue_t * ) xQueueOrSemaphore )->uxMessagesWaiting != ( UBaseType_t ) 0 )
			{
				/* 如果队列/信号量中已有项目，则无
				法将队列/信号量添加到队列集中。 */
				xReturn = pdFAIL;
			}
			else
			{
				( ( Queue_t * ) xQueueOrSemaphore )->pxQueueSetContainer = xQueueSet;
				xReturn = pdPASS;
			}
		}
		taskEXIT_CRITICAL();

		return xReturn;
	}

#endif /* configUSE_QUEUE_SETS */
/*-----------------------------------------------------------*/

#if ( configUSE_QUEUE_SETS == 1 )

	BaseType_t xQueueRemoveFromSet( QueueSetMemberHandle_t xQueueOrSemaphore, QueueSetHandle_t xQueueSet )
	{
	BaseType_t xReturn;
	Queue_t * const pxQueueOrSemaphore = ( Queue_t * ) xQueueOrSemaphore;

		if( pxQueueOrSemaphore->pxQueueSetContainer != xQueueSet )
		{
			/* 该队列不是该集合的成员。 */
			xReturn = pdFAIL;
		}
		else if( pxQueueOrSemaphore->uxMessagesWaiting != ( UBaseType_t ) 0 )
		{
			/* 当队列不为空时从队列中删除
			队列是危险的，因为队列集仍
			将保留队列的待处理事件。 */
			xReturn = pdFAIL;
		}
		else
		{
			taskENTER_CRITICAL();
			{
				/* 该队列不再包含在集合中。 */
				pxQueueOrSemaphore->pxQueueSetContainer = NULL;
			}
			taskEXIT_CRITICAL();
			xReturn = pdPASS;
		}

		return xReturn;
	} /*lint !e818 xQueueSet 无法声明为指向 const，因为它是 typedef。 */

#endif /* configUSE_QUEUE_SETS */
/*-----------------------------------------------------------*/

#if ( configUSE_QUEUE_SETS == 1 )

	QueueSetMemberHandle_t xQueueSelectFromSet( QueueSetHandle_t xQueueSet, TickType_t const xTicksToWait )
	{
	QueueSetMemberHandle_t xReturn = NULL;

		( void ) xQueueReceive( ( QueueHandle_t ) xQueueSet, &xReturn, xTicksToWait ); /*lint !e961 从一种类型定义转换为另一种类型定义并不是多余的。 */
		return xReturn;
	}

#endif /* configUSE_QUEUE_SETS */
/*-----------------------------------------------------------*/

#if ( configUSE_QUEUE_SETS == 1 )

	QueueSetMemberHandle_t xQueueSelectFromSetFromISR( QueueSetHandle_t xQueueSet )
	{
	QueueSetMemberHandle_t xReturn = NULL;

		( void ) xQueueReceiveFromISR( ( QueueHandle_t ) xQueueSet, &xReturn, NULL ); /*lint !e961 从一种类型定义转换为另一种类型定义并不是多余的。 */
		return xReturn;
	}

#endif /* configUSE_QUEUE_SETS */
/*-----------------------------------------------------------*/

#if ( configUSE_QUEUE_SETS == 1 )

	static BaseType_t prvNotifyQueueSetContainer( const Queue_t * const pxQueue )
	{
	Queue_t *pxQueueSetContainer = pxQueue->pxQueueSetContainer;
	BaseType_t xReturn = pdFALSE;

		/* 该函数必须从临界区调用。 */

		configASSERT( pxQueueSetContainer );
		configASSERT( pxQueueSetContainer->uxMessagesWaiting < pxQueueSetContainer->uxLength );

		if( pxQueueSetContainer->uxMessagesWaiting < pxQueueSetContainer->uxLength )
		{
			const int8_t cTxLock = pxQueueSetContainer->cTxLock;

			traceQUEUE_SEND( pxQueueSetContainer );

			/* 复制的数据是包含数据的队列的句柄。 */
			xReturn = prvCopyDataToQueue( pxQueueSetContainer, &pxQueue, queueSEND_TO_BACK );

			if( cTxLock == queueUNLOCKED )
			{
				if( listLIST_IS_EMPTY( &( pxQueueSetContainer->xTasksWaitingToReceive ) ) == pdFALSE )
				{
					if( xTaskRemoveFromEventList( &( pxQueueSetContainer->xTasksWaitingToReceive ) ) != pdFALSE )
					{
						/* 等待的任务具有更高的优先级。 */
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
				pxQueueSetContainer->cTxLock = ( int8_t ) ( cTxLock + 1 );
			}
		}
		else
		{
			mtCOVERAGE_TEST_MARKER();
		}

		return xReturn;
	}

#endif /* configUSE_QUEUE_SETS */











