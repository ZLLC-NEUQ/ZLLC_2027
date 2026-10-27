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

#include "FreeRTOS.h"
#include "task.h"
#include "croutine.h"

/* 删除整个文件是 co-routines 未被使用。 */
#if( configUSE_CO_ROUTINES != 0 )

/*
 * 一些内核感知调试器要求查看全局
 * 数据，而不是文件范围的数据。
 */
#ifdef portREMOVE_STATIC_QUALIFIER
	#define static
#endif


/* 就绪和阻止的 co-routines 列表。 --------------------*/
static List_t pxReadyCoRoutineLists[ configMAX_CO_ROUTINE_PRIORITIES ];	/*< 优先准备就绪 co-routines。 */
static List_t xDelayedCoRoutineList1;									/*< 延迟 co-routines。 */
static List_t xDelayedCoRoutineList2;									/*< Delayed co-routines（使用两个列表 - 一个用于超出当前滴答计数的延迟。 */
static List_t * pxDelayedCoRoutineList;									/*< 指向当前正在使用的延迟 co-routine 列表。 */
static List_t * pxOverflowDelayedCoRoutineList;							/*< 指向当前用于保存已溢出当前滴答计数的 co-routines 的延迟 co-routine 列表。 */
static List_t xPendingReadyCoRoutineList;								/*< 保存已由外部事件准备好的 co-routines。  它们不能直接添加到就绪列表，因为就绪列表不能通过中断访问。 */

/* 其他文件私有变量。 --------------------------------*/
CRCB_t * pxCurrentCoRoutine = NULL;
static UBaseType_t uxTopCoRoutineReadyPriority = 0;
static TickType_t xCoRoutineTickCount = 0, xLastTickCount = 0, xPassedTicks = 0;

/* co-routine 创建时的初始状态。 */
#define corINITIAL_STATE	( 0 )

/*
 * 将pxCRCB代表的co-routine放入相应的就绪队列中
 * 对应优先级的就绪列表中，并插入到列表末尾。
 *
 * 该宏访问 co-routine 就绪
 * 列表，因此不得在 ISR 内使用。
 */
#define prvAddCoRoutineToReadyQueue( pxCRCB )																		\
{																													\
	if( pxCRCB->uxPriority > uxTopCoRoutineReadyPriority )															\
	{																												\
		uxTopCoRoutineReadyPriority = pxCRCB->uxPriority;															\
	}																												\
	vListInsertEnd( ( List_t * ) &( pxReadyCoRoutineLists[ pxCRCB->uxPriority ] ), &( pxCRCB->xGenericListItem ) );	\
}

/*
 * 用于准备调度程序使用的所有列表的实用程序。  这是
 * 在创建第一个 co-routine 时自动调用的。
 */
static void prvInitialiseCoRoutineLists( void );

/*
 * 由中断准备好的协程不能直接放入准备好的
 * lists中（不存在互斥）。  相反，它们被
 * 放置在挂起的就绪列表中，以便稍后可以由
 * co-routine 调度程序将它们移至就绪列表。
 */
static void prvCheckPendingReadyList( void );

/*
 * 查看当前延迟的 co-routines
 * 列表以查看是否有任何需要唤醒的宏。
 *
 * 协同例程按照其唤醒时间的顺序存储在队列中 -
 * 这意味着一旦找到一个计时器尚未到期的
 * co-routine，我们就不需要再进一步查看列表了。
 */
static void prvCheckDelayedList( void );

/*-----------------------------------------------------------*/

BaseType_t xCoRoutineCreate( crCOROUTINE_CODE pxCoRoutineCode, UBaseType_t uxPriority, UBaseType_t uxIndex )
{
BaseType_t xReturn;
CRCB_t *pxCoRoutine;

	/* 分配用于存储 co-routine 控制块的内存。 */
	pxCoRoutine = ( CRCB_t * ) pvPortMalloc( sizeof( CRCB_t ) );
	if( pxCoRoutine )
	{
		/* 如果 pxCurrentCoRoutine 是 NULL，那么这是第一个要创
		建的 co-routine，并且 co-routine 数据结构需要初始化。 */
		if( pxCurrentCoRoutine == NULL )
		{
			pxCurrentCoRoutine = pxCoRoutine;
			prvInitialiseCoRoutineLists();
		}

		/* 检查优先级是否在限制范围内。 */
		if( uxPriority >= configMAX_CO_ROUTINE_PRIORITIES )
		{
			uxPriority = configMAX_CO_ROUTINE_PRIORITIES - 1;
		}

		/* 从功能参数中填写co-routine控制块。 */
		pxCoRoutine->uxState = corINITIAL_STATE;
		pxCoRoutine->uxPriority = uxPriority;
		pxCoRoutine->uxIndex = uxIndex;
		pxCoRoutine->pxCoRoutineFunction = pxCoRoutineCode;

		/* 初始化所有其他 co-routine 控制块参数。 */
		vListInitialiseItem( &( pxCoRoutine->xGenericListItem ) );
		vListInitialiseItem( &( pxCoRoutine->xEventListItem ) );

		/* 将 co-routine 控制块设置为从
		ListItem_t 返回的链接。这样我们就可
		以从列表中的通用项返回到包含 CRCB 的内容。 */
		listSET_LIST_ITEM_OWNER( &( pxCoRoutine->xGenericListItem ), pxCoRoutine );
		listSET_LIST_ITEM_OWNER( &( pxCoRoutine->xEventListItem ), pxCoRoutine );

		/* 事件列表始终按优先级顺序排列。 */
		listSET_LIST_ITEM_VALUE( &( pxCoRoutine->xEventListItem ), ( ( TickType_t ) configMAX_CO_ROUTINE_PRIORITIES - ( TickType_t ) uxPriority ) );

		/* 现在 co-routine 已初始化，可
		以以正确的优先级将其添加到就绪列表中。 */
		prvAddCoRoutineToReadyQueue( pxCoRoutine );

		xReturn = pdPASS;
	}
	else
	{
		xReturn = errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY;
	}

	return xReturn;
}
/*-----------------------------------------------------------*/

void vCoRoutineAddToDelayedList( TickType_t xTicksToDelay, List_t *pxEventList )
{
TickType_t xTimeToWake;

	/* 计算唤醒时间 - 这可能
	会溢出，但这不是问题。 */
	xTimeToWake = xCoRoutineTickCount + xTicksToDelay;

	/* 在将自己添加到阻止列表之前，我
	们必须将自己从就绪列表中删除，
	因为两个列表使用相同的列表项。 */
	( void ) uxListRemove( ( ListItem_t * ) &( pxCurrentCoRoutine->xGenericListItem ) );

	/* 列表项将按唤醒时间顺序插入。 */
	listSET_LIST_ITEM_VALUE( &( pxCurrentCoRoutine->xGenericListItem ), xTimeToWake );

	if( xTimeToWake < xCoRoutineTickCount )
	{
		/* 起床时间已满。  将此
		项目放入溢出列表中。 */
		vListInsert( ( List_t * ) pxOverflowDelayedCoRoutineList, ( ListItem_t * ) &( pxCurrentCoRoutine->xGenericListItem ) );
	}
	else
	{
		/* 唤醒时间还没有溢出，所以我
		们可以使用当前的块列表。 */
		vListInsert( ( List_t * ) pxDelayedCoRoutineList, ( ListItem_t * ) &( pxCurrentCoRoutine->xGenericListItem ) );
	}

	if( pxEventList )
	{
		/* 还将 co-routine 添加到事件列表。
		如果这样做，则必须在禁用中断的情况下调用该函数。 */
		vListInsert( pxEventList, &( pxCurrentCoRoutine->xEventListItem ) );
	}
}
/*-----------------------------------------------------------*/

static void prvCheckPendingReadyList( void )
{
	/* 是否有任何 co-routines 正在等待移至就
	绪列表？  这些是已由 ISR 准备好的
	co-routines。  ISR 本身无法访问就绪列表。 */
	while( listLIST_IS_EMPTY( &xPendingReadyCoRoutineList ) == pdFALSE )
	{
		CRCB_t *pxUnblockedCRCB;

		/* 挂起的就绪列表可由 ISR 访问。 */
		portDISABLE_INTERRUPTS();
		{
			pxUnblockedCRCB = ( CRCB_t * ) listGET_OWNER_OF_HEAD_ENTRY( (&xPendingReadyCoRoutineList) );
			( void ) uxListRemove( &( pxUnblockedCRCB->xEventListItem ) );
		}
		portENABLE_INTERRUPTS();

		( void ) uxListRemove( &( pxUnblockedCRCB->xGenericListItem ) );
		prvAddCoRoutineToReadyQueue( pxUnblockedCRCB );
	}
}
/*-----------------------------------------------------------*/

static void prvCheckDelayedList( void )
{
CRCB_t *pxCRCB;

	xPassedTicks = xTaskGetTickCount() - xLastTickCount;
	while( xPassedTicks )
	{
		xCoRoutineTickCount++;
		xPassedTicks--;

		/* 如果滴答计数溢出，我们需要交换就绪列表。 */
		if( xCoRoutineTickCount == 0 )
		{
			List_t * pxTemp;

			/* 滴答计数已溢出，因此我们需要交换延迟列表。  如果此处的
			pxDelayedCoRoutineList 中有任何项目，则出现错误！ */
			pxTemp = pxDelayedCoRoutineList;
			pxDelayedCoRoutineList = pxOverflowDelayedCoRoutineList;
			pxOverflowDelayedCoRoutineList = pxTemp;
		}

		/* 查看此勾选是否使超时到期。 */
		while( listLIST_IS_EMPTY( pxDelayedCoRoutineList ) == pdFALSE )
		{
			pxCRCB = ( CRCB_t * ) listGET_OWNER_OF_HEAD_ENTRY( pxDelayedCoRoutineList );

			if( xCoRoutineTickCount < listGET_LIST_ITEM_VALUE( &( pxCRCB->xGenericListItem ) ) )
			{
				/* 超时尚未到期。 */
				break;
			}

			portDISABLE_INTERRUPTS();
			{
				/* 该事件可能发生在该关键部分之前。  如
				果是这种情况，则通用列表项将被移至待处
				理就绪列表，并且以下行仍然有效。  此
				外，pvContainer 参数将被设
				置为 NULL，因此以下行也有效。 */
				( void ) uxListRemove( &( pxCRCB->xGenericListItem ) );

				/* co-routine 也在等待事件吗？ */
				if( pxCRCB->xEventListItem.pxContainer )
				{
					( void ) uxListRemove( &( pxCRCB->xEventListItem ) );
				}
			}
			portENABLE_INTERRUPTS();

			prvAddCoRoutineToReadyQueue( pxCRCB );
		}
	}

	xLastTickCount = xCoRoutineTickCount;
}
/*-----------------------------------------------------------*/

void vCoRoutineSchedule( void )
{
	/* 查看事件准备好的 co-routines 是否需要移至就绪列表。 */
	prvCheckPendingReadyList();

	/* 查看是否有延迟的 co-routines 超时。 */
	prvCheckDelayedList();

	/* 找到包含就绪co-routines的最高优先级队列。 */
	while( listLIST_IS_EMPTY( &( pxReadyCoRoutineLists[ uxTopCoRoutineReadyPriority ] ) ) )
	{
		if( uxTopCoRoutineReadyPriority == 0 )
		{
			/* 不再需要检查 co-routines。 */
			return;
		}
		--uxTopCoRoutineReadyPriority;
	}

	/* listGET_OWNER_OF_NEXT_ENTRY 遍历列表，
	 因此相同优先级的 co-routines 获得同等的处理器时间。 */
	listGET_OWNER_OF_NEXT_ENTRY( pxCurrentCoRoutine, &( pxReadyCoRoutineLists[ uxTopCoRoutineReadyPriority ] ) );

	/* 拨打 co-routine。 */
	( pxCurrentCoRoutine->pxCoRoutineFunction )( pxCurrentCoRoutine, pxCurrentCoRoutine->uxIndex );

	return;
}
/*-----------------------------------------------------------*/

static void prvInitialiseCoRoutineLists( void )
{
UBaseType_t uxPriority;

	for( uxPriority = 0; uxPriority < configMAX_CO_ROUTINE_PRIORITIES; uxPriority++ )
	{
		vListInitialise( ( List_t * ) &( pxReadyCoRoutineLists[ uxPriority ] ) );
	}

	vListInitialise( ( List_t * ) &xDelayedCoRoutineList1 );
	vListInitialise( ( List_t * ) &xDelayedCoRoutineList2 );
	vListInitialise( ( List_t * ) &xPendingReadyCoRoutineList );

	/* 从使用 list1 的 pxDelayedCoRoutineList 开始，从使用
	list2 的 pxOverflowDelayedCoRoutineList 开始。 */
	pxDelayedCoRoutineList = &xDelayedCoRoutineList1;
	pxOverflowDelayedCoRoutineList = &xDelayedCoRoutineList2;
}
/*-----------------------------------------------------------*/

BaseType_t xCoRoutineRemoveFromEventList( const List_t *pxEventList )
{
CRCB_t *pxUnblockedCRCB;
BaseType_t xReturn;

	/* 该函数是从中断中调用的。  它只能访问事件列
	表和待处理就绪列表。  此函数假设已进行检查
	以确保 pxEventList 不为空。 */
	pxUnblockedCRCB = ( CRCB_t * ) listGET_OWNER_OF_HEAD_ENTRY( pxEventList );
	( void ) uxListRemove( &( pxUnblockedCRCB->xEventListItem ) );
	vListInsertEnd( ( List_t * ) &( xPendingReadyCoRoutineList ), &( pxUnblockedCRCB->xEventListItem ) );

	if( pxUnblockedCRCB->uxPriority >= pxCurrentCoRoutine->uxPriority )
	{
		xReturn = pdTRUE;
	}
	else
	{
		xReturn = pdFALSE;
	}

	return xReturn;
}

#endif /* configUSE_CO_ROUTINES == 0 */
