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

/*
 * pvPortMalloc() 和
 * vPortFree() 的示例实现，
 * combines 在释放相邻内存块时将其（合并），从而限制内存碎片。
 *
 * 有关替代实现，请参阅 heap_1.c、heap_2.c 和 heap_3.c，以
 * 及 http://www.FreeRTOS.org 的内存管理页面以了解更多信息。
 */
#include <stdlib.h>

/* 定义
MPU_WRAPPERS_INCLUDED_FROM_API_FILE
可防止 task.h 重新定义所有 API 函数以使用 MPU 包装器。  仅当应用程序文件包含 task.h 时才应执行此操作。 */
#define MPU_WRAPPERS_INCLUDED_FROM_API_FILE

#include "FreeRTOS.h"
#include "task.h"

#undef MPU_WRAPPERS_INCLUDED_FROM_API_FILE

#if( configSUPPORT_DYNAMIC_ALLOCATION == 0 )
	#error This file must not be used if configSUPPORT_DYNAMIC_ALLOCATION is 0
#endif

/* 块大小不能太小。 */
#define heapMINIMUM_BLOCK_SIZE	( ( size_t ) ( xHeapStructSize << 1 ) )

/* 假设8bit字节！ */
#define heapBITS_PER_BYTE		( ( size_t ) 8 )

/* 为堆分配内存。 */
#if( configAPPLICATION_ALLOCATED_HEAP == 1 )
	/* 应用程序编写者已经定义了用于 RTOS 堆的数组
	- 可能因此可以将其放置在特殊的段或地址中。 */
	extern uint8_t ucHeap[ configTOTAL_HEAP_SIZE ];
#else
	static uint8_t ucHeap[ configTOTAL_HEAP_SIZE ];
#endif /* configAPPLICATION_ALLOCATED_HEAP */

/* 定义链表结构。  这用于按照
内存地址的顺序链接空闲块。 */
typedef struct A_BLOCK_LINK
{
	struct A_BLOCK_LINK *pxNextFreeBlock;	/*<< 列表中的下一个空闲块。 */
	size_t xBlockSize;						/*<< 空闲块的大小。 */
} BlockLink_t;

/*-----------------------------------------------------------*/

/*
 * 将正在释放的内存块插入到空闲内存
 * 块列表中的正确位置。  如果内存
 * 块彼此相邻，则被释放的块将与其前
 * 面的块和/或后面的块合并。
 */
static void prvInsertBlockIntoFreeList( BlockLink_t *pxBlockToInsert );

/*
 * 第一次调用
 * pvPortMalloc() 时自动调用以设置所需的堆结构。
 */
static void prvHeapInit( void );

/*-----------------------------------------------------------*/

/* 放置在每个分配的内存块开头的结
构的大小必须正确地字节对齐。 */
static const size_t xHeapStructSize	= ( sizeof( BlockLink_t ) + ( ( size_t ) ( portBYTE_ALIGNMENT - 1 ) ) ) & ~( ( size_t ) portBYTE_ALIGNMENT_MASK );

/* 创建几个列表链接来标记列表的开始和结束。 */
static BlockLink_t xStart, *pxEnd = NULL;

/* 跟踪分配和释放内存的调用次数以及
剩余的可用字节数，但不提及碎片。 */
static size_t xFreeBytesRemaining = 0U;
static size_t xMinimumEverFreeBytesRemaining = 0U;
static size_t xNumberOfSuccessfulAllocations = 0;
static size_t xNumberOfSuccessfulFrees = 0;

/* 设置为 size_t 类型的最高位。  当
BlockLink_t 结构的 xBlockSize 成员中的
该位被设置时，该块属于该应用程序。  当该位空闲时，该块仍然是空闲堆的一部分
空间。 */
static size_t xBlockAllocatedBit = 0;

/*-----------------------------------------------------------*/

void *pvPortMalloc( size_t xWantedSize )
{
BlockLink_t *pxBlock, *pxPreviousBlock, *pxNewBlockLink;
void *pvReturn = NULL;

	vTaskSuspendAll();
	{
		/* 如果这是第一次调用 malloc，
		则堆将需要初始化以设置空闲块列表。 */
		if( pxEnd == NULL )
		{
			prvHeapInit();
		}
		else
		{
			mtCOVERAGE_TEST_MARKER();
		}

		/* 检查请求的块大小是否大到设置了最高位。
		BlockLink_t结构的块大小
		成员的最高位用于确定谁拥有该块——应用
		程序还是内核，因此它必须是空闲的。 */
		if( ( xWantedSize & xBlockAllocatedBit ) == 0 )
		{
			/* 所需的大小已增加，因此除了请求的字节数之外，
			它还可以包含 BlockLink_t 结构。 */
			if( xWantedSize > 0 )
			{
				xWantedSize += xHeapStructSize;

				/* 确保块始终与所需
				的字节数对齐。 */
				if( ( xWantedSize & portBYTE_ALIGNMENT_MASK ) != 0x00 )
				{
					/* 需要字节对齐。 */
					xWantedSize += ( portBYTE_ALIGNMENT - ( xWantedSize & portBYTE_ALIGNMENT_MASK ) );
					configASSERT( ( xWantedSize & portBYTE_ALIGNMENT_MASK ) == 0 );
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

			if( ( xWantedSize > 0 ) && ( xWantedSize <= xFreeBytesRemaining ) )
			{
				/* 从 start（最低地址）块开始
				遍历列表，直到找到足够大小的块。 */
				pxPreviousBlock = &xStart;
				pxBlock = xStart.pxNextFreeBlock;
				while( ( pxBlock->xBlockSize < xWantedSize ) && ( pxBlock->pxNextFreeBlock != NULL ) )
				{
					pxPreviousBlock = pxBlock;
					pxBlock = pxBlock->pxNextFreeBlock;
				}

				/* 如果到达结束标记，则
				未找到足够大小的块。 */
				if( pxBlock != pxEnd )
				{
					/* 返回指向的内存空间 - 在其开始处跳
					过 BlockLink_t 结构。 */
					pvReturn = ( void * ) ( ( ( uint8_t * ) pxPreviousBlock->pxNextFreeBlock ) + xHeapStructSize );

					/* 该块正在返回使用，因此必
					须从空闲块列表中取出。 */
					pxPreviousBlock->pxNextFreeBlock = pxBlock->pxNextFreeBlock;

					/* 如果块大于所需的大小，则可以将其拆分为
					二。 */
					if( ( pxBlock->xBlockSize - xWantedSize ) > heapMINIMUM_BLOCK_SIZE )
					{
						/* 该块将被分成两部分。  根据请求
						的字节数创建一个新块。 void
						强制转换用于防止字节对齐警告
						编译器。 */
						pxNewBlockLink = ( void * ) ( ( ( uint8_t * ) pxBlock ) + xWantedSize );
						configASSERT( ( ( ( size_t ) pxNewBlockLink ) & portBYTE_ALIGNMENT_MASK ) == 0 );

						/* 计算从单个块中分割出
						来的两个块的大小。 */
						pxNewBlockLink->xBlockSize = pxBlock->xBlockSize - xWantedSize;
						pxBlock->xBlockSize = xWantedSize;

						/* 将新块插入到空闲块列表中。 */
						prvInsertBlockIntoFreeList( pxNewBlockLink );
					}
					else
					{
						mtCOVERAGE_TEST_MARKER();
					}

					xFreeBytesRemaining -= pxBlock->xBlockSize;

					if( xFreeBytesRemaining < xMinimumEverFreeBytesRemaining )
					{
						xMinimumEverFreeBytesRemaining = xFreeBytesRemaining;
					}
					else
					{
						mtCOVERAGE_TEST_MARKER();
					}

					/* 该块正在被返回 - 它由应用程序分
					配和拥有，并且没有“下一个”块。 */
					pxBlock->xBlockSize |= xBlockAllocatedBit;
					pxBlock->pxNextFreeBlock = NULL;
					xNumberOfSuccessfulAllocations++;
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

		traceMALLOC( pvReturn, xWantedSize );
	}
	( void ) xTaskResumeAll();

	#if( configUSE_MALLOC_FAILED_HOOK == 1 )
	{
		if( pvReturn == NULL )
		{
			extern void vApplicationMallocFailedHook( void );
			vApplicationMallocFailedHook();
		}
		else
		{
			mtCOVERAGE_TEST_MARKER();
		}
	}
	#endif

	configASSERT( ( ( ( size_t ) pvReturn ) & ( size_t ) portBYTE_ALIGNMENT_MASK ) == 0 );
	return pvReturn;
}
/*-----------------------------------------------------------*/

void vPortFree( void *pv )
{
uint8_t *puc = ( uint8_t * ) pv;
BlockLink_t *pxLink;

	if( pv != NULL )
	{
		/* 被释放的内存前面将有一个
		BlockLink_t 结构。 */
		puc -= xHeapStructSize;

		/* 这种转换是为了防止编译器发出警告。 */
		pxLink = ( void * ) puc;

		/* 检查该块是否实际分配。 */
		configASSERT( ( pxLink->xBlockSize & xBlockAllocatedBit ) != 0 );
		configASSERT( pxLink->pxNextFreeBlock == NULL );

		if( ( pxLink->xBlockSize & xBlockAllocatedBit ) != 0 )
		{
			if( pxLink->pxNextFreeBlock == NULL )
			{
				/* 该块正在返回到堆中 - 它不再是
				分配。 */
				pxLink->xBlockSize &= ~xBlockAllocatedBit;

				vTaskSuspendAll();
				{
					/* 将此块添加到空闲块列表中。 */
					xFreeBytesRemaining += pxLink->xBlockSize;
					traceFREE( pv, pxLink->xBlockSize );
					prvInsertBlockIntoFreeList( ( ( BlockLink_t * ) pxLink ) );
					xNumberOfSuccessfulFrees++;
				}
				( void ) xTaskResumeAll();
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
/*-----------------------------------------------------------*/

size_t xPortGetFreeHeapSize( void )
{
	return xFreeBytesRemaining;
}
/*-----------------------------------------------------------*/

size_t xPortGetMinimumEverFreeHeapSize( void )
{
	return xMinimumEverFreeBytesRemaining;
}
/*-----------------------------------------------------------*/

void vPortInitialiseBlocks( void )
{
	/* 这只是为了保持链接器安静而存在。 */
}
/*-----------------------------------------------------------*/

static void prvHeapInit( void )
{
BlockLink_t *pxFirstFreeBlock;
uint8_t *pucAlignedHeap;
size_t uxAddress;
size_t xTotalHeapSize = configTOTAL_HEAP_SIZE;

	/* 确保堆在正确对齐的边界上开始。 */
	uxAddress = ( size_t ) ucHeap;

	if( ( uxAddress & portBYTE_ALIGNMENT_MASK ) != 0 )
	{
		uxAddress += ( portBYTE_ALIGNMENT - 1 );
		uxAddress &= ~( ( size_t ) portBYTE_ALIGNMENT_MASK );
		xTotalHeapSize -= uxAddress - ( size_t ) ucHeap;
	}

	pucAlignedHeap = ( uint8_t * ) uxAddress;

	/* xStart 用于保存指向空闲块列表中第一项的指
	针。  void 强制转换用于防止编译器警告。 */
	xStart.pxNextFreeBlock = ( void * ) pucAlignedHeap;
	xStart.xBlockSize = ( size_t ) 0;

	/* pxEnd用于标记空闲块列表
	的末尾，插入到堆空间的末尾。 */
	uxAddress = ( ( size_t ) pucAlignedHeap ) + xTotalHeapSize;
	uxAddress -= xHeapStructSize;
	uxAddress &= ~( ( size_t ) portBYTE_ALIGNMENT_MASK );
	pxEnd = ( void * ) uxAddress;
	pxEnd->xBlockSize = 0;
	pxEnd->pxNextFreeBlock = NULL;

	/* 首先，有一个空闲块的大小可以占用整个堆
	空间，减去 pxEnd 占用的空间。 */
	pxFirstFreeBlock = ( void * ) pucAlignedHeap;
	pxFirstFreeBlock->xBlockSize = uxAddress - ( size_t ) pxFirstFreeBlock;
	pxFirstFreeBlock->pxNextFreeBlock = pxEnd;

	/* 仅存在一个块 - 并且它覆盖了整个可用堆空间。 */
	xMinimumEverFreeBytesRemaining = pxFirstFreeBlock->xBlockSize;
	xFreeBytesRemaining = pxFirstFreeBlock->xBlockSize;

	/* 计算出 size_t 变量中最高位的位置。 */
	xBlockAllocatedBit = ( ( size_t ) 1 ) << ( ( sizeof( size_t ) * heapBITS_PER_BYTE ) - 1 );
}
/*-----------------------------------------------------------*/

static void prvInsertBlockIntoFreeList( BlockLink_t *pxBlockToInsert )
{
BlockLink_t *pxIterator;
uint8_t *puc;

	/* 迭代列表，直到找到比要插
	入的块具有更高地址的块。 */
	for( pxIterator = &xStart; pxIterator->pxNextFreeBlock < pxBlockToInsert; pxIterator = pxIterator->pxNextFreeBlock )
	{
		/* 这里什么都不用做，只需迭代到正确的位置即可。 */
	}

	/* 正在插入的块以及在其之后插入
	的块是否构成连续的内存块？ */
	puc = ( uint8_t * ) pxIterator;
	if( ( puc + pxIterator->xBlockSize ) == ( uint8_t * ) pxBlockToInsert )
	{
		pxIterator->xBlockSize += pxBlockToInsert->xBlockSize;
		pxBlockToInsert = pxIterator;
	}
	else
	{
		mtCOVERAGE_TEST_MARKER();
	}

	/* 正在插入的块和之前插入的
	块是否构成连续的内存块？ */
	puc = ( uint8_t * ) pxBlockToInsert;
	if( ( puc + pxBlockToInsert->xBlockSize ) == ( uint8_t * ) pxIterator->pxNextFreeBlock )
	{
		if( pxIterator->pxNextFreeBlock != pxEnd )
		{
			/* 将两个块组成一个大块。 */
			pxBlockToInsert->xBlockSize += pxIterator->pxNextFreeBlock->xBlockSize;
			pxBlockToInsert->pxNextFreeBlock = pxIterator->pxNextFreeBlock->pxNextFreeBlock;
		}
		else
		{
			pxBlockToInsert->pxNextFreeBlock = pxEnd;
		}
	}
	else
	{
		pxBlockToInsert->pxNextFreeBlock = pxIterator->pxNextFreeBlock;
	}

	/* 如果插入的块插入了一个缺口，因此与之前的块
	和之后的块合并，那么它的
	pxNextFreeBlock 指针将已经
	被设置，并且不应该在这里设置，因为这会使其指向自身。 */
	if( pxIterator != pxBlockToInsert )
	{
		pxIterator->pxNextFreeBlock = pxBlockToInsert;
	}
	else
	{
		mtCOVERAGE_TEST_MARKER();
	}
}
/*-----------------------------------------------------------*/

void vPortGetHeapStats( HeapStats_t *pxHeapStats )
{
BlockLink_t *pxBlock;
size_t xBlocks = 0, xMaxSize = 0, xMinSize = portMAX_DELAY; /* portMAX_DELAY 用作获取最大值的便携式方式。 */

	vTaskSuspendAll();
	{
		pxBlock = xStart.pxNextFreeBlock;

		/* 如果堆尚未初始化，则 pxBlock 将为
		NULL。  第一次分配时，堆会自动初始化。 */
		if( pxBlock != NULL )
		{
			do
			{
				/* 增加块的数量并记录迄
				今为止看到的最大块。 */
				xBlocks++;

				if( pxBlock->xBlockSize > xMaxSize )
				{
					xMaxSize = pxBlock->xBlockSize;
				}

				if( pxBlock->xBlockSize < xMinSize )
				{
					xMinSize = pxBlock->xBlockSize;
				}

				/* 移动到链中的下一个块，直到最后一个块被
				达到了。 */
				pxBlock = pxBlock->pxNextFreeBlock;
			} while( pxBlock != pxEnd );
		}
	}
	xTaskResumeAll();

	pxHeapStats->xSizeOfLargestFreeBlockInBytes = xMaxSize;
	pxHeapStats->xSizeOfSmallestFreeBlockInBytes = xMinSize;
	pxHeapStats->xNumberOfFreeBlocks = xBlocks;

	taskENTER_CRITICAL();
	{
		pxHeapStats->xAvailableHeapSpaceInBytes = xFreeBytesRemaining;
		pxHeapStats->xNumberOfSuccessfulAllocations = xNumberOfSuccessfulAllocations;
		pxHeapStats->xNumberOfSuccessfulFrees = xNumberOfSuccessfulFrees;
		pxHeapStats->xMinimumEverFreeBytesRemaining = xMinimumEverFreeBytesRemaining;
	}
	taskEXIT_CRITICAL();
}

