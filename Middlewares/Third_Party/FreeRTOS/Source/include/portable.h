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

/*-----------------------------------------------------------
 * 便携式层API。  必须为每个端口定义每个功能。
 *----------------------------------------------------------*/

#ifndef PORTABLE_H
#define PORTABLE_H

/* 每个 FreeRTOS 端口都有一个唯一的
portmacro.h 头文件。  最初，预处理器定义用于
确保预处理器为所使用的端口找到正确的
portmacro.h 文件。  该方案已被弃用，有利于设
置编译器的包含路径，以便找到正确的 portmacro.h
文件 - 消除了对常量的需要，并允许
portmacro.h 文件位于与所使用的端口相关的任何位
置。纯粹出于向后兼容性的原因，旧方法仍然有效，但为了明确新
项目不应使用它，对端口特定常量的支持已移至 deprecated_definitions.h 标头中
文件。 */
#include "deprecated_definitions.h"

/* 如果未定义 portENTER_CRITICAL，则包含
deprecated_definitions.h 不会导致包含
portmacro.h 头文件 - 并且应将其包含在此处。  在这种
情况下，必须在编译器的包含路径中设置正确的 portmacro.h 头文件的路径。 */
#ifndef portENTER_CRITICAL
	#include "portmacro.h"
#endif

#if portBYTE_ALIGNMENT == 32
	#define portBYTE_ALIGNMENT_MASK ( 0x001f )
#endif

#if portBYTE_ALIGNMENT == 16
	#define portBYTE_ALIGNMENT_MASK ( 0x000f )
#endif

#if portBYTE_ALIGNMENT == 8
	#define portBYTE_ALIGNMENT_MASK ( 0x0007 )
#endif

#if portBYTE_ALIGNMENT == 4
	#define portBYTE_ALIGNMENT_MASK	( 0x0003 )
#endif

#if portBYTE_ALIGNMENT == 2
	#define portBYTE_ALIGNMENT_MASK	( 0x0001 )
#endif

#if portBYTE_ALIGNMENT == 1
	#define portBYTE_ALIGNMENT_MASK	( 0x0000 )
#endif

#ifndef portBYTE_ALIGNMENT_MASK
	#error "Invalid portBYTE_ALIGNMENT definition"
#endif

#ifndef portNUM_CONFIGURABLE_REGIONS
	#define portNUM_CONFIGURABLE_REGIONS 1
#endif

#ifndef portHAS_STACK_OVERFLOW_CHECKING
	#define portHAS_STACK_OVERFLOW_CHECKING 0
#endif

#ifndef portARCH_NAME
	#define portARCH_NAME NULL
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include "mpu_wrappers.h"

/*
 * 设置新任务的堆栈，以便将其置于调度
 * 程序控制之下。  寄存器必须按照端
 * 口期望找到它们的顺序放置在堆栈上。
 *
 */
#if( portUSING_MPU_WRAPPERS == 1 )
	#if( portHAS_STACK_OVERFLOW_CHECKING == 1 )
		StackType_t *pxPortInitialiseStack( StackType_t *pxTopOfStack, StackType_t *pxEndOfStack, TaskFunction_t pxCode, void *pvParameters, BaseType_t xRunPrivileged ) PRIVILEGED_FUNCTION;
	#else
		StackType_t *pxPortInitialiseStack( StackType_t *pxTopOfStack, TaskFunction_t pxCode, void *pvParameters, BaseType_t xRunPrivileged ) PRIVILEGED_FUNCTION;
	#endif
#else
	#if( portHAS_STACK_OVERFLOW_CHECKING == 1 )
		StackType_t *pxPortInitialiseStack( StackType_t *pxTopOfStack, StackType_t *pxEndOfStack, TaskFunction_t pxCode, void *pvParameters ) PRIVILEGED_FUNCTION;
	#else
		StackType_t *pxPortInitialiseStack( StackType_t *pxTopOfStack, TaskFunction_t pxCode, void *pvParameters ) PRIVILEGED_FUNCTION;
	#endif
#endif

/* 由 heap_5.c 用于定义共同构成总
FreeRTOS 堆空间的每个内存区域的起始地址和大小。 */
typedef struct HeapRegion
{
	uint8_t *pucStartAddress;
	size_t xSizeInBytes;
} HeapRegion_t;

/* 用于从 vPortGetHeapStats() 传递有关堆的信息。 */
typedef struct xHeapStats
{
	size_t xAvailableHeapSpaceInBytes;		/* 当前可用的总堆大小 - 这是所有空闲块的总和，而不是可以分配的最大块。 */
	size_t xSizeOfLargestFreeBlockInBytes; 	/* 调用 vPortGetHeapStats() 时堆内所有空闲块的最大大小（以字节为单位）。 */
	size_t xSizeOfSmallestFreeBlockInBytes; /* 调用 vPortGetHeapStats() 时堆内所有空闲块的最小大小（以字节为单位）。 */
	size_t xNumberOfFreeBlocks;				/* 调用 vPortGetHeapStats() 时堆内的空闲内存块数。 */
	size_t xMinimumEverFreeBytesRemaining;	/* 自系统启动以来堆中的总空闲量 memory（所有空闲块的总和）的最小量。 */
	size_t xNumberOfSuccessfulAllocations;	/* 返回有效内存块的 pvPortMalloc() 调用次数。 */
	size_t xNumberOfSuccessfulFrees;		/* 已成功释放内存块的 vPortFree() 调用次数。 */
} HeapStats_t;

/*
 * 用于定义多个堆区域以供 heap_5.c 使用。
 * 必须在调用 pvPortMalloc() 之前调用此
 * 函数 - 不创建任务、队列、信号量、互斥体、软件定时
 * 器、事件组等将导致调用 pvPortMalloc。
 *
 * pxHeapRegions 传入一个
 * HeapRegion_t 结构数组 - 每个结构都定义一个
 * 可用作堆的内存区域。  该数组以大小为 0 的
 * HeapRegions_t 结构终止。具有最低起始地址的区域必须首先出现在数组中。
 */
void vPortDefineHeapRegions( const HeapRegion_t * const pxHeapRegions ) PRIVILEGED_FUNCTION;

/*
 * 返回一个 HeapStats_t 结
 * 构，其中填充有关当前堆状态的信息。
 */
void vPortGetHeapStats( HeapStats_t *pxHeapStats );

/*
 * 映射到端口所需的内存管理例程。
 */
void *pvPortMalloc( size_t xSize ) PRIVILEGED_FUNCTION;
void vPortFree( void *pv ) PRIVILEGED_FUNCTION;
void vPortInitialiseBlocks( void ) PRIVILEGED_FUNCTION;
size_t xPortGetFreeHeapSize( void ) PRIVILEGED_FUNCTION;
size_t xPortGetMinimumEverFreeHeapSize( void ) PRIVILEGED_FUNCTION;

/*
 * 设置硬件准备好让调度程序进行控制。  这通常会
 * 设置一个滴答中断并为正确的滴答频率设置计时器。
 */
BaseType_t xPortStartScheduler( void ) PRIVILEGED_FUNCTION;

/*
 * 撤消由 xPortStartScheduler() 执行的任
 * 何硬件/ISR 设置，以便在调度程序停止后硬件保持其原始状态
 * 执行。
 */
void vPortEndScheduler( void ) PRIVILEGED_FUNCTION;

/*
 * 操作 MPU 的结构和
 * 方法包含在端口层内。
 *
 * 使用 xRegions 中包含的内存区域信
 * 息填充 xMPUSettings 结构。
 */
#if( portUSING_MPU_WRAPPERS == 1 )
	struct xMEMORY_REGION;
	void vPortStoreTaskMPUSettings( xMPU_SETTINGS *xMPUSettings, const struct xMEMORY_REGION * const xRegions, StackType_t *pxBottomOfStack, uint32_t ulStackDepth ) PRIVILEGED_FUNCTION;
#endif

#ifdef __cplusplus
}
#endif

#endif /* PORTABLE_H */

