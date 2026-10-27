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
#include "FreeRTOS.h"
#include "list.h"

/*-----------------------------------------------------------
 * PUBLIC LIST API 记录在 list.h 中
 *----------------------------------------------------------*/

void vListInitialise( List_t * const pxList )
{
	/* 列表结构包含一个列表项，用于标记列
	表的结尾。  为了初始化列表，列表
	末尾被插入作为唯一的列表条目。 */
	pxList->pxIndex = ( ListItem_t * ) &( pxList->xListEnd );			/*lint !e826 !e740 !e9087 以迷你链表结构作为链表末尾保存RAM。  这是经过检查且有效的。 */

	/* 列表结束值是列表中可能的最高值
	，以确保它保留在列表的末尾。 */
	pxList->xListEnd.xItemValue = portMAX_DELAY;

	/* 列表末尾的 next 和 previous
	指针指向自身，因此我们知道列表何时为空。 */
	pxList->xListEnd.pxNext = ( ListItem_t * ) &( pxList->xListEnd );	/*lint !e826 !e740 !e9087 以迷你链表结构作为链表末尾保存RAM。  这是经过检查且有效的。 */
	pxList->xListEnd.pxPrevious = ( ListItem_t * ) &( pxList->xListEnd );/*lint !e826 !e740 !e9087 以迷你链表结构作为链表末尾保存RAM。  这是经过检查且有效的。 */

	pxList->uxNumberOfItems = ( UBaseType_t ) 0U;

	/* 如果
	configUSE_LIST_DATA_INTEGRITY_CHECK_BYTES 设置为 1，则将已知值写入列表中。 */
	listSET_LIST_INTEGRITY_CHECK_1_VALUE( pxList );
	listSET_LIST_INTEGRITY_CHECK_2_VALUE( pxList );
}
/*-----------------------------------------------------------*/

void vListInitialiseItem( ListItem_t * const pxItem )
{
	/* 确保列表项未记录为列表中。 */
	pxItem->pxContainer = NULL;

	/* 如果
	configUSE_LIST_DATA_INTEGRITY_CHECK_BYTES 设置为 1，则将已知值写入列表项。 */
	listSET_FIRST_LIST_ITEM_INTEGRITY_CHECK_VALUE( pxItem );
	listSET_SECOND_LIST_ITEM_INTEGRITY_CHECK_VALUE( pxItem );
}
/*-----------------------------------------------------------*/

void vListInsertEnd( List_t * const pxList, ListItem_t * const pxNewListItem )
{
ListItem_t * const pxIndex = pxList->pxIndex;

	/* 仅当 configASSERT() 也被定义时才有效，这些
	测试可能会捕获内存中被覆盖的列表数据结构。  它们不会捕获
	因 FreeRTOS 的错误配置或使用而导致的数据错误。 */
	listTEST_LIST_INTEGRITY( pxList );
	listTEST_LIST_ITEM_INTEGRITY( pxNewListItem );

	/* 将新列表项插入到 pxList 中，但不对列表进行排序
	，而是通过调用
	listGET_OWNER_OF_NEXT_ENTRY() 使新列表项成为要删除的最后一项。 */
	pxNewListItem->pxNext = pxIndex;
	pxNewListItem->pxPrevious = pxIndex->pxPrevious;

	/* 仅在决策覆盖测试期间使用。 */
	mtCOVERAGE_TEST_DELAY();

	pxIndex->pxPrevious->pxNext = pxNewListItem;
	pxIndex->pxPrevious = pxNewListItem;

	/* 记住该项目位于哪个列表中。 */
	pxNewListItem->pxContainer = pxList;

	( pxList->uxNumberOfItems )++;
}
/*-----------------------------------------------------------*/

void vListInsert( List_t * const pxList, ListItem_t * const pxNewListItem )
{
ListItem_t *pxIterator;
const TickType_t xValueOfInsertion = pxNewListItem->xItemValue;

	/* 仅当 configASSERT() 也被定义时才有效，这些
	测试可能会捕获内存中被覆盖的列表数据结构。  它们不会捕获
	因 FreeRTOS 的错误配置或使用而导致的数据错误。 */
	listTEST_LIST_INTEGRITY( pxList );
	listTEST_LIST_ITEM_INTEGRITY( pxNewListItem );

	/* 将新列表项插入列表中，按 xItemValue 顺序排序。

	如果列表已包含具有相同项目值的列表项，则新列表项应放置
	在其后面。  这确保了存储在就绪 lists 中的
	TCBs（所有这些都具有相同的 xItemValue
	值）获得 CPU 的份额。  但是，如果
	xItemValue 与后标记相同，则下面的迭代循环将
	不会结束。  因此，首先检查该值，并在必要时稍微修改算法。 */
	if( xValueOfInsertion == portMAX_DELAY )
	{
		pxIterator = pxList->xListEnd.pxPrevious;
	}
	else
	{
		/* *** 笔记 **************************************
		****************************** 如果您发现您的应用程序在此处崩
		溃，则可能的原因如下所列。  另外，请参阅
		https://www.freertos.org/FAQHelp.html 了解更多提示，并确保定义了 configASSERT()！
		https://www.freertos.org/a00110.html#configASSERT

			1) 堆栈溢出 - 请参阅
			   https://www.freertos.org/Stacks-and-stack-overflow-checking.html
			2) 中断优先级分配不正确，尤其是在 Cortex-M
			   部件上，其中数字高优先级值表示实际中断优先级低，这似乎违反
			   直觉。  看
			   https://www.freertos.org/RTOS-Cortex-M3-M4.html及其定义
			   configMAX_SYSCALL_INTERRUPT_PRIORITY 上
			   https://www.freertos.org/a00110.html
			3) 从临界区内或调度程序挂起时调用 API
			   函数，或从中断调用不以“FromISR”结尾的
			   API 函数。 4) 在队列或信号量初始化之
			前或在调度程序 started 之前使用队列或
			   信号量（在调用
			   vTaskStartScheduler() 之前是否触发中断？）。
		**********************************************************************/

		for( pxIterator = ( ListItem_t * ) &( pxList->xListEnd ); pxIterator->pxNext->xItemValue <= xValueOfInsertion; pxIterator = pxIterator->pxNext ) /*lint !e826 !e740 !e9087 以迷你链表结构作为链表末尾保存RAM。  这是经过检查且有效的。 *//*lint !e440 迭代器移动到不同的值，而不是 xValueOfInsertion。 */
		{
			/* 这里没有什么可做的，只是
			迭代到想要的插入位置。 */
		}
	}

	pxNewListItem->pxNext = pxIterator->pxNext;
	pxNewListItem->pxNext->pxPrevious = pxNewListItem;
	pxNewListItem->pxPrevious = pxIterator;
	pxIterator->pxNext = pxNewListItem;

	/* 记住该项目位于哪个列表中。这
	样可以稍后快速删除该项目。 */
	pxNewListItem->pxContainer = pxList;

	( pxList->uxNumberOfItems )++;
}
/*-----------------------------------------------------------*/

UBaseType_t uxListRemove( ListItem_t * const pxItemToRemove )
{
/* 列表项知道自己在哪个列表中，从列表中获取列表
物品。 */
List_t * const pxList = pxItemToRemove->pxContainer;

	pxItemToRemove->pxNext->pxPrevious = pxItemToRemove->pxPrevious;
	pxItemToRemove->pxPrevious->pxNext = pxItemToRemove->pxNext;

	/* 仅在决策覆盖测试期间使用。 */
	mtCOVERAGE_TEST_DELAY();

	/* 确保索引指向有效的项目。 */
	if( pxList->pxIndex == pxItemToRemove )
	{
		pxList->pxIndex = pxItemToRemove->pxPrevious;
	}
	else
	{
		mtCOVERAGE_TEST_MARKER();
	}

	pxItemToRemove->pxContainer = NULL;
	( pxList->uxNumberOfItems )--;

	return pxList->uxNumberOfItems;
}
/*-----------------------------------------------------------*/

