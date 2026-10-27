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

/* FreeRTOS 头文件。 */
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "event_groups.h"

/* Lint e961、e750 和 e9021 被抑制为 MISRA 异常，因为 MPU
端口需要定义 MPU_WRAPPERS_INCLUDED_FROM_API_FILE
为生成正确的特权与非特权链接和代码布局，上述头文件中需要定义该宏，
正确的特权与非特权链接和放置。 */
#undef MPU_WRAPPERS_INCLUDED_FROM_API_FILE /*lint !e961 !e750 !e9021 请参阅上面的评论。 */

/* 以下位字段传达任务事件列表项值中的控制信息。
重要的是它们不与
taskEVENT_LIST_ITEM_VALUE_IN_USE 定义冲突。 */
#if configUSE_16_BIT_TICKS == 1
	#define eventCLEAR_EVENTS_ON_EXIT_BIT	0x0100U
	#define eventUNBLOCKED_DUE_TO_BIT_SET	0x0200U
	#define eventWAIT_FOR_ALL_BITS			0x0400U
	#define eventEVENT_BITS_CONTROL_BYTES	0xff00U
#else
	#define eventCLEAR_EVENTS_ON_EXIT_BIT	0x01000000UL
	#define eventUNBLOCKED_DUE_TO_BIT_SET	0x02000000UL
	#define eventWAIT_FOR_ALL_BITS			0x04000000UL
	#define eventEVENT_BITS_CONTROL_BYTES	0xff000000UL
#endif

typedef struct EventGroupDef_t
{
	EventBits_t uxEventBits;
	List_t xTasksWaitingForBits;		/*< 等待设置位的任务列表。 */

	#if( configUSE_TRACE_FACILITY == 1 )
		UBaseType_t uxEventGroupNumber;
	#endif

	#if( ( configSUPPORT_STATIC_ALLOCATION == 1 ) && ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) )
		uint8_t ucStaticallyAllocated; /*< 如果事件组是静态分配的，则设置为 pdTRUE 以确保不会尝试释放内存。 */
	#endif
} EventGroup_t;

/*-----------------------------------------------------------*/

/*
 * 测试 uxCurrentEventBits 中设置的位以查看是否满足等待条件。等待
 * 条件由xWaitForAllBits 定义。  如果
 * xWaitForAllBits 是 pdTRUE，则如果
 * uxBitsToWaitFor 中设置的所有位也在
 * uxCurrentEventBits 中设置，则满足等待条件。  如果
 * xWaitForAllBits 是 pdFALSE，则如果 uxBitsToWait 中设置的任何位也在 uxCurrentEventBits 中设置，则满足等待条件。
 */
static BaseType_t prvTestWaitCondition( const EventBits_t uxCurrentEventBits, const EventBits_t uxBitsToWaitFor, const BaseType_t xWaitForAllBits ) PRIVILEGED_FUNCTION;

/*-----------------------------------------------------------*/

#if( configSUPPORT_STATIC_ALLOCATION == 1 )

	EventGroupHandle_t xEventGroupCreateStatic( StaticEventGroup_t *pxEventGroupBuffer )
	{
	EventGroup_t *pxEventBits;

		/* 必须提供 StaticEventGroup_t 对象。 */
		configASSERT( pxEventGroupBuffer );

		#if( configASSERT_DEFINED == 1 )
		{
			/* 健全性检查用于声明
			StaticEventGroup_t
			类型变量的结构的大小是否等于实际事件组结构的大小。 */
			volatile size_t xSize = sizeof( StaticEventGroup_t );
			configASSERT( xSize == sizeof( EventGroup_t ) );
		} /*lint !e529 如果定义了 configASSERT()，则引用 xSize。 */
		#endif /* configASSERT_DEFINED */

		/* 用户提供了静态分配的事件组 - 使用它。 */
		pxEventBits = ( EventGroup_t * ) pxEventGroupBuffer; /*lint !e740 !e9087 EventGroup_t 和 StaticEventGroup_t 出于数据隐藏目的而故意使用别名，并保证具有相同的大小和对齐要求 - 由 configASSERT() 检查。 */

		if( pxEventBits != NULL )
		{
			pxEventBits->uxEventBits = 0;
			vListInitialise( &( pxEventBits->xTasksWaitingForBits ) );

			#if( configSUPPORT_DYNAMIC_ALLOCATION == 1 )
			{
				/* 静态和动态分配都可以使用，因
				此请注意，该事件组是静态创建
				的，以防稍后删除该事件组。 */
				pxEventBits->ucStaticallyAllocated = pdTRUE;
			}
			#endif /* configSUPPORT_DYNAMIC_ALLOCATION */

			traceEVENT_GROUP_CREATE( pxEventBits );
		}
		else
		{
			/* xEventGroupCreateStatic 只能在
			pxEventGroupBuffer 指向预 allocated
			（编译时分配）StaticEventGroup_t 变量的情况下调用。 */
			traceEVENT_GROUP_CREATE_FAILED();
		}

		return pxEventBits;
	}

#endif /* configSUPPORT_STATIC_ALLOCATION */
/*-----------------------------------------------------------*/

#if( configSUPPORT_DYNAMIC_ALLOCATION == 1 )

	EventGroupHandle_t xEventGroupCreate( void )
	{
	EventGroup_t *pxEventBits;

		/* 分配事件组。  MISRA 偏差的理由如下：
		pvPortMalloc() 始终确保返回的内存块
		按照 MCU 堆栈的要求对齐。  在这种情况下，
		pvPortMalloc() 必须返回一个保证满足
		EventGroup_t 结构的对齐要求的指针
		- which（如果您遵循它）是
		TickType_t type 的对齐要求（
		EventBits_t 是 TickType_t
		本身）。  因此，只要堆栈对齐要求大于或等于
		TickType_t 对齐要求，转换就是安全的。
		在其他情况下，如果架构的自然字大小小于
		sizeof( TickType_t )，则
		TickType_t 变量将在两次或多次读取操作中访问，并且对齐要求仅为每次单独读取的对齐要求。 */
		pxEventBits = ( EventGroup_t * ) pvPortMalloc( sizeof( EventGroup_t ) ); /*lint !e9087 !e9079 请参阅上面的评论。 */

		if( pxEventBits != NULL )
		{
			pxEventBits->uxEventBits = 0;
			vListInitialise( &( pxEventBits->xTasksWaitingForBits ) );

			#if( configSUPPORT_STATIC_ALLOCATION == 1 )
			{
				/* 静态和动态分配都可以使用，因
				此请注意此事件组是静态分配的
				，以防事件组稍后被删除。 */
				pxEventBits->ucStaticallyAllocated = pdFALSE;
			}
			#endif /* configSUPPORT_STATIC_ALLOCATION */

			traceEVENT_GROUP_CREATE( pxEventBits );
		}
		else
		{
			traceEVENT_GROUP_CREATE_FAILED(); /*lint !e9063 Else 分支仅存在以允许跟踪，如果未定义跟踪宏，则不会生成代码。 */
		}

		return pxEventBits;
	}

#endif /* configSUPPORT_DYNAMIC_ALLOCATION */
/*-----------------------------------------------------------*/

EventBits_t xEventGroupSync( EventGroupHandle_t xEventGroup, const EventBits_t uxBitsToSet, const EventBits_t uxBitsToWaitFor, TickType_t xTicksToWait )
{
EventBits_t uxOriginalBitValue, uxReturn;
EventGroup_t *pxEventBits = xEventGroup;
BaseType_t xAlreadyYielded;
BaseType_t xTimeoutOccurred = pdFALSE;

	configASSERT( ( uxBitsToWaitFor & eventEVENT_BITS_CONTROL_BYTES ) == 0 );
	configASSERT( uxBitsToWaitFor != 0 );
	#if ( ( INCLUDE_xTaskGetSchedulerState == 1 ) || ( configUSE_TIMERS == 1 ) )
	{
		configASSERT( !( ( xTaskGetSchedulerState() == taskSCHEDULER_SUSPENDED ) && ( xTicksToWait != 0 ) ) );
	}
	#endif

	vTaskSuspendAll();
	{
		uxOriginalBitValue = pxEventBits->uxEventBits;

		( void ) xEventGroupSetBits( xEventGroup, uxBitsToSet );

		if( ( ( uxOriginalBitValue | uxBitsToSet ) & uxBitsToWaitFor ) == uxBitsToWaitFor )
		{
			/* 现在所有交会位均已设置 - 无需阻塞。 */
			uxReturn = ( uxOriginalBitValue | uxBitsToSet );

			/* Rendezvous 总是清除这些位。  除非
			这是集合点中的唯一任务，否则他们已经被清除了。 */
			pxEventBits->uxEventBits &= ~uxBitsToWaitFor;

			xTicksToWait = 0;
		}
		else
		{
			if( xTicksToWait != ( TickType_t ) 0 )
			{
				traceEVENT_GROUP_SYNC_BLOCK( xEventGroup, uxBitsToSet, uxBitsToWaitFor );

				/* 将调用任务正在等待的位存储在任务的
				事件列表项中，以便内核知道何时找到
				匹配项。  然后进入阻塞状态。 */
				vTaskPlaceOnUnorderedEventList( &( pxEventBits->xTasksWaitingForBits ), ( uxBitsToWaitFor | eventCLEAR_EVENTS_ON_EXIT_BIT | eventWAIT_FOR_ALL_BITS ), xTicksToWait );

				/* 此分配已过时，因为 uxReturn
				将在任务取消阻止后设置，但某些编译器会
				错误地生成警告，说明如果省略分配，则返
				回 uxReturn 而未设置。 */
				uxReturn = 0;
			}
			else
			{
				/* 未设置集合位，但未指定块时间
				- 仅返回当前事件位值。 */
				uxReturn = pxEventBits->uxEventBits;
				xTimeoutOccurred = pdTRUE;
			}
		}
	}
	xAlreadyYielded = xTaskResumeAll();

	if( xTicksToWait != ( TickType_t ) 0 )
	{
		if( xAlreadyYielded == pdFALSE )
		{
			portYIELD_WITHIN_API();
		}
		else
		{
			mtCOVERAGE_TEST_MARKER();
		}

		/* 任务被阻塞以等待其所需位被设置 - 此时，
		要么所需位已设置，要么阻塞时间已到期。
		如果设置了所需的位，它们将被存储在任务的事
		件列表项中，现在应该检索然后清除它们。 */
		uxReturn = uxTaskResetEventItemValue();

		if( ( uxReturn & eventUNBLOCKED_DUE_TO_BIT_SET ) == ( EventBits_t ) 0 )
		{
			/* 任务超时，仅返回当前事件位值。 */
			taskENTER_CRITICAL();
			{
				uxReturn = pxEventBits->uxEventBits;

				/* 尽管该任务到达这里是因为它在等待的位被
				设置之前超时，但由于它解除了阻塞，另一
				个任务可能已经设置了这些位。  如果是
				这种情况，则需要在退出之前清除这些位。 */
				if( ( uxReturn & uxBitsToWaitFor ) == uxBitsToWaitFor )
				{
					pxEventBits->uxEventBits &= ~uxBitsToWaitFor;
				}
				else
				{
					mtCOVERAGE_TEST_MARKER();
				}
			}
			taskEXIT_CRITICAL();

			xTimeoutOccurred = pdTRUE;
		}
		else
		{
			/* 由于位已设置，任务已解除阻塞。 */
		}

		/* 控制位可能会被设置，因为任务已被阻止不应该被设置
		回来了。 */
		uxReturn &= ~eventEVENT_BITS_CONTROL_BYTES;
	}

	traceEVENT_GROUP_SYNC_END( xEventGroup, uxBitsToSet, uxBitsToWaitFor, xTimeoutOccurred );

	/* 当不使用跟踪宏时防止编译器发出警告。 */
	( void ) xTimeoutOccurred;

	return uxReturn;
}
/*-----------------------------------------------------------*/

EventBits_t xEventGroupWaitBits( EventGroupHandle_t xEventGroup, const EventBits_t uxBitsToWaitFor, const BaseType_t xClearOnExit, const BaseType_t xWaitForAllBits, TickType_t xTicksToWait )
{
EventGroup_t *pxEventBits = xEventGroup;
EventBits_t uxReturn, uxControlBits = 0;
BaseType_t xWaitConditionMet, xAlreadyYielded;
BaseType_t xTimeoutOccurred = pdFALSE;

	/* 检查用户没有尝试等待内核本身使
	用的位，并且至少正在请求一位。 */
	configASSERT( xEventGroup );
	configASSERT( ( uxBitsToWaitFor & eventEVENT_BITS_CONTROL_BYTES ) == 0 );
	configASSERT( uxBitsToWaitFor != 0 );
	#if ( ( INCLUDE_xTaskGetSchedulerState == 1 ) || ( configUSE_TIMERS == 1 ) )
	{
		configASSERT( !( ( xTaskGetSchedulerState() == taskSCHEDULER_SUSPENDED ) && ( xTicksToWait != 0 ) ) );
	}
	#endif

	vTaskSuspendAll();
	{
		const EventBits_t uxCurrentEventBits = pxEventBits->uxEventBits;

		/* 检查等待条件是否已满足。 */
		xWaitConditionMet = prvTestWaitCondition( uxCurrentEventBits, uxBitsToWaitFor, xWaitForAllBits );

		if( xWaitConditionMet != pdFALSE )
		{
			/* 等待条件已经满足，所以不需要
			堵塞。 */
			uxReturn = uxCurrentEventBits;
			xTicksToWait = ( TickType_t ) 0;

			/* 如果需要，请清除等待位。 */
			if( xClearOnExit != pdFALSE )
			{
				pxEventBits->uxEventBits &= ~uxBitsToWaitFor;
			}
			else
			{
				mtCOVERAGE_TEST_MARKER();
			}
		}
		else if( xTicksToWait == ( TickType_t ) 0 )
		{
			/* 等待条件尚未满足，但未指定阻
			塞时间，因此仅返回当前值。 */
			uxReturn = uxCurrentEventBits;
			xTimeoutOccurred = pdTRUE;
		}
		else
		{
			/* 该任务将阻塞以等待其所需的位被设置。
			uxControlBits 用于记住对
			xEventGroupWaitBits()
			的调用的指定行为 - 以便在事件位解锁任务时使用。 */
			if( xClearOnExit != pdFALSE )
			{
				uxControlBits |= eventCLEAR_EVENTS_ON_EXIT_BIT;
			}
			else
			{
				mtCOVERAGE_TEST_MARKER();
			}

			if( xWaitForAllBits != pdFALSE )
			{
				uxControlBits |= eventWAIT_FOR_ALL_BITS;
			}
			else
			{
				mtCOVERAGE_TEST_MARKER();
			}

			/* 将调用任务正在等待的位存储在任务的
			事件列表项中，以便内核知道何时找到
			匹配项。  然后进入阻塞状态。 */
			vTaskPlaceOnUnorderedEventList( &( pxEventBits->xTasksWaitingForBits ), ( uxBitsToWaitFor | uxControlBits ), xTicksToWait );

			/* 这是过时的，因为它会在任务解除阻塞后被设
			置，但一些编译器会错误地生成一个警告，说
			明如果未完成任务，变量将被返回而未设置。 */
			uxReturn = 0;

			traceEVENT_GROUP_WAIT_BITS_BLOCK( xEventGroup, uxBitsToWaitFor );
		}
	}
	xAlreadyYielded = xTaskResumeAll();

	if( xTicksToWait != ( TickType_t ) 0 )
	{
		if( xAlreadyYielded == pdFALSE )
		{
			portYIELD_WITHIN_API();
		}
		else
		{
			mtCOVERAGE_TEST_MARKER();
		}

		/* 任务被阻塞以等待其所需位被设置 - 此时，
		要么所需位已设置，要么阻塞时间已到期。
		如果设置了所需的位，它们将被存储在任务的事
		件列表项中，现在应该检索然后清除它们。 */
		uxReturn = uxTaskResetEventItemValue();

		if( ( uxReturn & eventUNBLOCKED_DUE_TO_BIT_SET ) == ( EventBits_t ) 0 )
		{
			taskENTER_CRITICAL();
			{
				/* 任务超时，仅返回当前事件位值。 */
				uxReturn = pxEventBits->uxEventBits;

				/* 在此任务离开阻塞状态和再次运
				行之间，事件位可能已更新。 */
				if( prvTestWaitCondition( uxReturn, uxBitsToWaitFor, xWaitForAllBits ) != pdFALSE )
				{
					if( xClearOnExit != pdFALSE )
					{
						pxEventBits->uxEventBits &= ~uxBitsToWaitFor;
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
				xTimeoutOccurred = pdTRUE;
			}
			taskEXIT_CRITICAL();
		}
		else
		{
			/* 由于位已设置，任务已解除阻塞。 */
		}

		/* 任务被阻止，因此控制位可能已被设置。 */
		uxReturn &= ~eventEVENT_BITS_CONTROL_BYTES;
	}
	traceEVENT_GROUP_WAIT_BITS_END( xEventGroup, uxBitsToWaitFor, xTimeoutOccurred );

	/* 当不使用跟踪宏时防止编译器发出警告。 */
	( void ) xTimeoutOccurred;

	return uxReturn;
}
/*-----------------------------------------------------------*/

EventBits_t xEventGroupClearBits( EventGroupHandle_t xEventGroup, const EventBits_t uxBitsToClear )
{
EventGroup_t *pxEventBits = xEventGroup;
EventBits_t uxReturn;

	/* 检查用户是否未尝试清除内核使用的位
	本身。 */
	configASSERT( xEventGroup );
	configASSERT( ( uxBitsToClear & eventEVENT_BITS_CONTROL_BYTES ) == 0 );

	taskENTER_CRITICAL();
	{
		traceEVENT_GROUP_CLEAR_BITS( xEventGroup, uxBitsToClear );

		/* 返回的值是位之前的事件组值
		已清除。 */
		uxReturn = pxEventBits->uxEventBits;

		/* 清除位。 */
		pxEventBits->uxEventBits &= ~uxBitsToClear;
	}
	taskEXIT_CRITICAL();

	return uxReturn;
}
/*-----------------------------------------------------------*/

#if ( ( configUSE_TRACE_FACILITY == 1 ) && ( INCLUDE_xTimerPendFunctionCall == 1 ) && ( configUSE_TIMERS == 1 ) )

	BaseType_t xEventGroupClearBitsFromISR( EventGroupHandle_t xEventGroup, const EventBits_t uxBitsToClear )
	{
		BaseType_t xReturn;

		traceEVENT_GROUP_CLEAR_BITS_FROM_ISR( xEventGroup, uxBitsToClear );
		xReturn = xTimerPendFunctionCallFromISR( vEventGroupClearBitsCallback, ( void * ) xEventGroup, ( uint32_t ) uxBitsToClear, NULL ); /*lint !e9087 无法避免强制转换为 void* 作为不特定于此用例的通用回调函数。回调会强制转换回原始类型，因此非常安全。 */

		return xReturn;
	}

#endif
/*-----------------------------------------------------------*/

EventBits_t xEventGroupGetBitsFromISR( EventGroupHandle_t xEventGroup )
{
UBaseType_t uxSavedInterruptStatus;
EventGroup_t const * const pxEventBits = xEventGroup;
EventBits_t uxReturn;

	uxSavedInterruptStatus = portSET_INTERRUPT_MASK_FROM_ISR();
	{
		uxReturn = pxEventBits->uxEventBits;
	}
	portCLEAR_INTERRUPT_MASK_FROM_ISR( uxSavedInterruptStatus );

	return uxReturn;
} /*lint !e818 EventGroupHandle_t 是其他函数中使用的 typedef，因此不能是指向 const 的指针。 */
/*-----------------------------------------------------------*/

EventBits_t xEventGroupSetBits( EventGroupHandle_t xEventGroup, const EventBits_t uxBitsToSet )
{
ListItem_t *pxListItem, *pxNext;
ListItem_t const *pxListEnd;
List_t const * pxList;
EventBits_t uxBitsToClear = 0, uxBitsWaitedFor, uxControlBits;
EventGroup_t *pxEventBits = xEventGroup;
BaseType_t xMatchFound = pdFALSE;

	/* 检查用户是否未尝试设置内核使用的位
	本身。 */
	configASSERT( xEventGroup );
	configASSERT( ( uxBitsToSet & eventEVENT_BITS_CONTROL_BYTES ) == 0 );

	pxList = &( pxEventBits->xTasksWaitingForBits );
	pxListEnd = listGET_END_MARKER( pxList ); /*lint !e826 !e740 !e9087 以迷你链表结构作为链表末尾保存RAM。  这是经过检查且有效的。 */
	vTaskSuspendAll();
	{
		traceEVENT_GROUP_SET_BITS( xEventGroup, uxBitsToSet );

		pxListItem = listGET_HEAD_ENTRY( pxList );

		/* 设置位。 */
		pxEventBits->uxEventBits |= uxBitsToSet;

		/* 查看新的位值是否应该解锁任何任务。 */
		while( pxListItem != pxListEnd )
		{
			pxNext = listGET_NEXT( pxListItem );
			uxBitsWaitedFor = listGET_LIST_ITEM_VALUE( pxListItem );
			xMatchFound = pdFALSE;

			/* 将等待的位与控制位分开。 */
			uxControlBits = uxBitsWaitedFor & eventEVENT_BITS_CONTROL_BYTES;
			uxBitsWaitedFor &= ~eventEVENT_BITS_CONTROL_BYTES;

			if( ( uxControlBits & eventWAIT_FOR_ALL_BITS ) == ( EventBits_t ) 0 )
			{
				/* 只是寻找正在设置的单个位。 */
				if( ( uxBitsWaitedFor & pxEventBits->uxEventBits ) != ( EventBits_t ) 0 )
				{
					xMatchFound = pdTRUE;
				}
				else
				{
					mtCOVERAGE_TEST_MARKER();
				}
			}
			else if( ( uxBitsWaitedFor & pxEventBits->uxEventBits ) == uxBitsWaitedFor )
			{
				/* 所有位均已设置。 */
				xMatchFound = pdTRUE;
			}
			else
			{
				/* 需要设置所有位，但并非所有位都已设置。 */
			}

			if( xMatchFound != pdFALSE )
			{
				/* 位匹配。  退出时是否应该清除这些位？ */
				if( ( uxControlBits & eventCLEAR_EVENTS_ON_EXIT_BIT ) != ( EventBits_t ) 0 )
				{
					uxBitsToClear |= uxBitsWaitedFor;
				}
				else
				{
					mtCOVERAGE_TEST_MARKER();
				}

				/* 从事件列表中删除任务之前，将实际事件标志
				值存储在任务的事件列表项中。
				eventUNBLOCKED_DUE_TO_BIT_SET
				位被设置，以便任务知道由于其所需的位匹
				配而被解锁，而不是因为超时。 */
				vTaskRemoveFromUnorderedEventList( pxListItem, pxEventBits->uxEventBits | eventUNBLOCKED_DUE_TO_BIT_SET );
			}

			/* 移至下一个列表项。  注意此处不使用
			pxListItem->pxNext，因为该列
			表项可能已从事件列表中删除并插入到就绪/待读列表中。 */
			pxListItem = pxNext;
		}

		/* 清除控制字中设置
		eventCLEAR_EVENTS_ON_EXIT_BIT 位时匹配的所有位。 */
		pxEventBits->uxEventBits &= ~uxBitsToClear;
	}
	( void ) xTaskResumeAll();

	return pxEventBits->uxEventBits;
}
/*-----------------------------------------------------------*/

void vEventGroupDelete( EventGroupHandle_t xEventGroup )
{
EventGroup_t *pxEventBits = xEventGroup;
const List_t *pxTasksWaitingForBits = &( pxEventBits->xTasksWaitingForBits );

	vTaskSuspendAll();
	{
		traceEVENT_GROUP_DELETE( xEventGroup );

		while( listCURRENT_LIST_LENGTH( pxTasksWaitingForBits ) > ( UBaseType_t ) 0 )
		{
			/* 取消阻止任务，返回 0，因为事件列
			表正在被删除，因此无法设置任何位。 */
			configASSERT( pxTasksWaitingForBits->xListEnd.pxNext != ( const ListItem_t * ) &( pxTasksWaitingForBits->xListEnd ) );
			vTaskRemoveFromUnorderedEventList( pxTasksWaitingForBits->xListEnd.pxNext, eventUNBLOCKED_DUE_TO_BIT_SET );
		}

		#if( ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) && ( configSUPPORT_STATIC_ALLOCATION == 0 ) )
		{
			/* 事件组只能动态分配
			- 再次释放它。 */
			vPortFree( pxEventBits );
		}
		#elif( ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) && ( configSUPPORT_STATIC_ALLOCATION == 1 ) )
		{
			/* 事件组可能已静态或动态分配，因此
			请在尝试释放内存之前进行检查。 */
			if( pxEventBits->ucStaticallyAllocated == ( uint8_t ) pdFALSE )
			{
				vPortFree( pxEventBits );
			}
			else
			{
				mtCOVERAGE_TEST_MARKER();
			}
		}
		#endif /* configSUPPORT_DYNAMIC_ALLOCATION */
	}
	( void ) xTaskResumeAll();
}
/*-----------------------------------------------------------*/

/* 仅供内部使用 - 执行因中
断而挂起的“设置位”命令。 */
void vEventGroupSetBitsCallback( void *pvEventGroup, const uint32_t ulBitsToSet )
{
	( void ) xEventGroupSetBits( pvEventGroup, ( EventBits_t ) ulBitsToSet ); /*lint !e9079 无法避免强制转换为 void* 作为通用计时器回调原型。回调会强制转换回原始类型，因此非常安全。 */
}
/*-----------------------------------------------------------*/

/* 仅供内部使用 - 执行因中
断而挂起的“清除位”命令。 */
void vEventGroupClearBitsCallback( void *pvEventGroup, const uint32_t ulBitsToClear )
{
	( void ) xEventGroupClearBits( pvEventGroup, ( EventBits_t ) ulBitsToClear ); /*lint !e9079 无法避免强制转换为 void* 作为通用计时器回调原型。回调会强制转换回原始类型，因此非常安全。 */
}
/*-----------------------------------------------------------*/

static BaseType_t prvTestWaitCondition( const EventBits_t uxCurrentEventBits, const EventBits_t uxBitsToWaitFor, const BaseType_t xWaitForAllBits )
{
BaseType_t xWaitConditionMet = pdFALSE;

	if( xWaitForAllBits == pdFALSE )
	{
		/* 任务只需等待
		uxBitsToWaitFor 中的一位被设置。  已经定好了吗？ */
		if( ( uxCurrentEventBits & uxBitsToWaitFor ) != ( EventBits_t ) 0 )
		{
			xWaitConditionMet = pdTRUE;
		}
		else
		{
			mtCOVERAGE_TEST_MARKER();
		}
	}
	else
	{
		/* 任务必须等待
		uxBitsToWaitFor 中的所有位被设置。他们已经定好了吗？ */
		if( ( uxCurrentEventBits & uxBitsToWaitFor ) == uxBitsToWaitFor )
		{
			xWaitConditionMet = pdTRUE;
		}
		else
		{
			mtCOVERAGE_TEST_MARKER();
		}
	}

	return xWaitConditionMet;
}
/*-----------------------------------------------------------*/

#if ( ( configUSE_TRACE_FACILITY == 1 ) && ( INCLUDE_xTimerPendFunctionCall == 1 ) && ( configUSE_TIMERS == 1 ) )

	BaseType_t xEventGroupSetBitsFromISR( EventGroupHandle_t xEventGroup, const EventBits_t uxBitsToSet, BaseType_t *pxHigherPriorityTaskWoken )
	{
	BaseType_t xReturn;

		traceEVENT_GROUP_SET_BITS_FROM_ISR( xEventGroup, uxBitsToSet );
		xReturn = xTimerPendFunctionCallFromISR( vEventGroupSetBitsCallback, ( void * ) xEventGroup, ( uint32_t ) uxBitsToSet, pxHigherPriorityTaskWoken ); /*lint !e9087 无法避免强制转换为 void* 作为不特定于此用例的通用回调函数。回调会强制转换回原始类型，因此非常安全。 */

		return xReturn;
	}

#endif
/*-----------------------------------------------------------*/

#if (configUSE_TRACE_FACILITY == 1)

	UBaseType_t uxEventGroupGetNumber( void* xEventGroup )
	{
	UBaseType_t xReturn;
	EventGroup_t const *pxEventBits = ( EventGroup_t * ) xEventGroup; /*lint !e9087 !e9079 EventGroupHandle_t 是指向 EventGroup_t 的指针，但出于数据隐藏目的，EventGroupHandle_t 在该文件之外保持不透明。 */

		if( xEventGroup == NULL )
		{
			xReturn = 0;
		}
		else
		{
			xReturn = pxEventBits->uxEventGroupNumber;
		}

		return xReturn;
	}

#endif /* configUSE_TRACE_FACILITY */
/*-----------------------------------------------------------*/

#if ( configUSE_TRACE_FACILITY == 1 )

	void vEventGroupSetNumber( void * xEventGroup, UBaseType_t uxEventGroupNumber )
	{
		( ( EventGroup_t * ) xEventGroup )->uxEventGroupNumber = uxEventGroupNumber; /*lint !e9087 !e9079 EventGroupHandle_t 是指向 EventGroup_t 的指针，但出于数据隐藏目的，EventGroupHandle_t 在该文件之外保持不透明。 */
	}

#endif /* configUSE_TRACE_FACILITY */
/*-----------------------------------------------------------*/

