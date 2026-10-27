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

/**
 * @file atomic.h
 * @brief FreeRTOS 原子操作支持。
 *
 * 该文件通过全局禁用中断来实现原子功
 * 能。可以在每个编译器目录下提供具有
 * 特定于体系结构的原子指令的实现。
 */

#ifndef ATOMIC_H
#define ATOMIC_H

#ifndef INC_FREERTOS_H
	#error "include FreeRTOS.h must appear in source files before include atomic.h"
#endif

/* 标准头文件。 */
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * 端口特定定义——进入/退出关键部分。参考模板--./
 * lib/FreeRTOS/portable/Compiler/Arch/portmacro.h
 *
 * 对 ATOMIC_EXIT_CRITICAL() 的每次调用都必
 * 须与 ATOMIC_ENTER_CRITICAL() 紧密配对。
 *
 */
#if defined( portSET_INTERRUPT_MASK_FROM_ISR )

	/* 该端口支持嵌套中断方案。 */
	#define ATOMIC_ENTER_CRITICAL()	 \
		UBaseType_t uxCriticalSectionType = portSET_INTERRUPT_MASK_FROM_ISR()

	#define ATOMIC_EXIT_CRITICAL()	  \
		portCLEAR_INTERRUPT_MASK_FROM_ISR( uxCriticalSectionType )

#else

	/* 该端口支持嵌套中断方案为NOT。 */
	#define ATOMIC_ENTER_CRITICAL()	 portENTER_CRITICAL()
	#define ATOMIC_EXIT_CRITICAL()	  portEXIT_CRITICAL()

#endif /* portSET_INTERRUPT_MASK_FROM_ISR() */

/*
 * 端口特定定义——“始终内联”。内联是特定于编
 * 译器的，并且可能并不总是内联，具体取决于您的
 * 优化级别。  另外，内联被认为是性能优化
 * for atomic.  Thus, if portFORCE_INLINE is not provided by portmacro.h,
 * 而不是产生错误，只需定义它即可。
 */
#ifndef portFORCE_INLINE
	#define portFORCE_INLINE
#endif

#define ATOMIC_COMPARE_AND_SWAP_SUCCESS	 0x1U		/**< 比较和交换成功，交换。 */
#define ATOMIC_COMPARE_AND_SWAP_FAILURE	 0x0U		/**< 比较和交换失败，没有交换。 */

/*------------------------------ 交换 && CAS ------------------------------*/

/**
 * 原子比较和交换
 *
 * @brief 对指定值执行原子比较和交换操作。
 *
 * @param[in, out] pulDestination 指向值所在内存位置的指针
 * 进行加载和检查。
 * @param[in] ulExchange 如果条件满足，则将该值写入内存。
 * @param[in] ulComparand 交换条件。
 *
 * @return 无符号整数，值为 1 或 0。1 表示已交换，0 表示未交换。
 *
 * @note 此函数仅将 *pulDestination 与 ulExchange 交换（如果之前有）
 * *pulDestination 值等于 ulComparand。
 */
static portFORCE_INLINE uint32_t Atomic_CompareAndSwap_u32( uint32_t volatile * pulDestination,
															uint32_t ulExchange,
															uint32_t ulComparand )
{
uint32_t ulReturnValue;

	ATOMIC_ENTER_CRITICAL();
	{
		if( *pulDestination == ulComparand )
		{
			*pulDestination = ulExchange;
			ulReturnValue = ATOMIC_COMPARE_AND_SWAP_SUCCESS;
		}
		else
		{
			ulReturnValue = ATOMIC_COMPARE_AND_SWAP_FAILURE;
		}
	}
	ATOMIC_EXIT_CRITICAL();

	return ulReturnValue;
}
/*-----------------------------------------------------------*/

/**
 * Atomic swap (pointers)
 *
 * @brief 以原子方式将 *ppvDestination 指向的地址设置为值
 * *pvExchange。
 *
 * @param[in, out] ppvDestination 指向指针所在内存位置的指针
 * 值将被加载并写回。
 * @param[in] pvExchange 要写入 *ppvDestination 的指针值。
 *
 * @return *ppvDestination 的初始值。
 */
static portFORCE_INLINE void * Atomic_SwapPointers_p32( void * volatile * ppvDestination,
														void * pvExchange )
{
void * pReturnValue;

	ATOMIC_ENTER_CRITICAL();
	{
		pReturnValue = *ppvDestination;
		*ppvDestination = pvExchange;
	}
	ATOMIC_EXIT_CRITICAL();

	return pReturnValue;
}
/*-----------------------------------------------------------*/

/**
 * 原子比较 swap（指针）
 *
 * @brief 对指定指针执行原子比较和交换操作
 * 价值观。
 *
 * @param[in, out] ppvDestination 指向指针所在内存位置的指针
 * 值将被加载和检查。
 * @param[in] pvExchange 如果条件满足，则将该值写入内存。
 * @param[in] pvComparand 交换条件。
 *
 * @return 无符号整数，值为 1 或 0。1 表示已交换，0 表示未交换。
 *
 * @note 此函数仅将 *ppvDestination 与 pvExchange 交换（如果之前有）
 * *ppvDestination 值等于 pvComparand。
 */
static portFORCE_INLINE uint32_t Atomic_CompareAndSwapPointers_p32( void * volatile * ppvDestination,
																	void * pvExchange,
																	void * pvComparand )
{
uint32_t ulReturnValue = ATOMIC_COMPARE_AND_SWAP_FAILURE;

	ATOMIC_ENTER_CRITICAL();
	{
		if( *ppvDestination == pvComparand )
		{
			*ppvDestination = pvExchange;
			ulReturnValue = ATOMIC_COMPARE_AND_SWAP_SUCCESS;
		}
	}
	ATOMIC_EXIT_CRITICAL();

	return ulReturnValue;
}


/*-------------------------------------- 算术 ------------------------------*/

/**
 * 原子添加
 *
 * @brief 以原子方式将 count 添加到指定指针指向的值。
 *
 * @param[in,out] pulAddend 指向值所在内存位置的指针
 * 加载并写回。
 * @param[in] ulCount 要添加到 *pulAddend 的值。
 *
 * @return 先前的 *pulAddend 值。
 */
static portFORCE_INLINE uint32_t Atomic_Add_u32( uint32_t volatile * pulAddend,
												 uint32_t ulCount )
{
	uint32_t ulCurrent;

	ATOMIC_ENTER_CRITICAL();
	{
		ulCurrent = *pulAddend;
		*pulAddend += ulCount;
	}
	ATOMIC_EXIT_CRITICAL();

	return ulCurrent;
}
/*-----------------------------------------------------------*/

/**
 * 原子减法
 *
 * @brief 以原子方式从指定指针的值中减去 count
 * 指向.
 *
 * @param[in,out] pulAddend 指向值所在内存位置的指针
 * 加载并写回。
 * @param[in] ulCount 要从 *pulAddend 中减去的值。
 *
 * @return 先前的 *pulAddend 值。
 */
static portFORCE_INLINE uint32_t Atomic_Subtract_u32( uint32_t volatile * pulAddend,
													  uint32_t ulCount )
{
	uint32_t ulCurrent;

	ATOMIC_ENTER_CRITICAL();
	{
		ulCurrent = *pulAddend;
		*pulAddend -= ulCount;
	}
	ATOMIC_EXIT_CRITICAL();

	return ulCurrent;
}
/*-----------------------------------------------------------*/

/**
 * 原子增量
 *
 * @brief 以原子方式递增指定指针指向的值。
 *
 * @param[in,out] pulAddend 指向值所在内存位置的指针
 * 加载并写回。
 *
 * @return *pulAddend 增量前的值。
 */
static portFORCE_INLINE uint32_t Atomic_Increment_u32( uint32_t volatile * pulAddend )
{
uint32_t ulCurrent;

	ATOMIC_ENTER_CRITICAL();
	{
		ulCurrent = *pulAddend;
		*pulAddend += 1;
	}
	ATOMIC_EXIT_CRITICAL();

	return ulCurrent;
}
/*-----------------------------------------------------------*/

/**
 * 原子递减
 *
 * @brief 原子地递减指定指针指向的值
 *
 * @param[in,out] pulAddend 指向值所在内存位置的指针
 * 加载并写回。
 *
 * @return *pulAddend 递减前的值。
 */
static portFORCE_INLINE uint32_t Atomic_Decrement_u32( uint32_t volatile * pulAddend )
{
uint32_t ulCurrent;

	ATOMIC_ENTER_CRITICAL();
	{
		ulCurrent = *pulAddend;
		*pulAddend -= 1;
	}
	ATOMIC_EXIT_CRITICAL();

	return ulCurrent;
}

/*-------------------------------------- 按位逻辑 ------------------------------*/

/**
 * 原子OR
 *
 * @brief 对指定值执行原子 OR 操作。
 *
 * @param [in, out] pulDestination 指向值所在内存位置的指针
 * 被加载并写回。
 * @param [输入] ulValue 值为 ORed，带 *pulDestination。
 *
 * @return *pulDestination 的原始值。
 */
static portFORCE_INLINE uint32_t Atomic_OR_u32( uint32_t volatile * pulDestination,
												uint32_t ulValue )
{
uint32_t ulCurrent;

	ATOMIC_ENTER_CRITICAL();
	{
		ulCurrent = *pulDestination;
		*pulDestination |= ulValue;
	}
	ATOMIC_EXIT_CRITICAL();

	return ulCurrent;
}
/*-----------------------------------------------------------*/

/**
 * 原子AND
 *
 * @brief 对指定值执行原子 AND 操作。
 *
 * @param [in, out] pulDestination 指向值所在内存位置的指针
 * 被加载并写回。
 * @param [输入] ulValue 值为 ANDed，带 *pulDestination。
 *
 * @return *pulDestination 的原始值。
 */
static portFORCE_INLINE uint32_t Atomic_AND_u32( uint32_t volatile * pulDestination,
												 uint32_t ulValue )
{
uint32_t ulCurrent;

	ATOMIC_ENTER_CRITICAL();
	{
		ulCurrent = *pulDestination;
		*pulDestination &= ulValue;
	}
	ATOMIC_EXIT_CRITICAL();

	return ulCurrent;
}
/*-----------------------------------------------------------*/

/**
 * 原子NAND
 *
 * @brief 对指定值执行原子 NAND 操作。
 *
 * @param [in, out] pulDestination 指向值所在内存位置的指针
 * 被加载并写回。
 * @param [输入] ulValue 值为 NANDed，带 *pulDestination。
 *
 * @return *pulDestination 的原始值。
 */
static portFORCE_INLINE uint32_t Atomic_NAND_u32( uint32_t volatile * pulDestination,
												  uint32_t ulValue )
{
uint32_t ulCurrent;

	ATOMIC_ENTER_CRITICAL();
	{
		ulCurrent = *pulDestination;
		*pulDestination = ~( ulCurrent & ulValue );
	}
	ATOMIC_EXIT_CRITICAL();

	return ulCurrent;
}
/*-----------------------------------------------------------*/

/**
 * 原子XOR
 *
 * @brief 对指定值执行原子 XOR 操作。
 *
 * @param [in, out] pulDestination 指向值所在内存位置的指针
 * 被加载并写回。
 * @param [输入] ulValue 值为 XORed，带 *pulDestination。
 *
 * @return *pulDestination 的原始值。
 */
static portFORCE_INLINE uint32_t Atomic_XOR_u32( uint32_t volatile * pulDestination,
												 uint32_t ulValue )
{
uint32_t ulCurrent;

	ATOMIC_ENTER_CRITICAL();
	{
		ulCurrent = *pulDestination;
		*pulDestination ^= ulValue;
	}
	ATOMIC_EXIT_CRITICAL();

	return ulCurrent;
}

#ifdef __cplusplus
}
#endif

#endif /* ATOMIC_H */
