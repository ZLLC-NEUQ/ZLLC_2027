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
 * 针对 ARM CM4F 端口实现 portable.h 中定义的功能。
 *----------------------------------------------------------*/

/* 调度程序包括。 */
#include "FreeRTOS.h"
#include "task.h"

#ifndef __VFP_FP__
	#error This port can only be used when the project options are configured to enable hardware floating point support.
#endif

#ifndef configSYSTICK_CLOCK_HZ
	#define configSYSTICK_CLOCK_HZ configCPU_CLOCK_HZ
	/* 确保 SysTick 的时钟频率与内核相同。 */
	#define portNVIC_SYSTICK_CLK_BIT	( 1UL << 2UL )
#else
	/* SysTick 的时钟方式不
	会被修改，以防它与内核不同。 */
	#define portNVIC_SYSTICK_CLK_BIT	( 0 )
#endif

/* 操纵核心所需的常数。  先注册... */
#define portNVIC_SYSTICK_CTRL_REG			( * ( ( volatile uint32_t * ) 0xe000e010 ) )
#define portNVIC_SYSTICK_LOAD_REG			( * ( ( volatile uint32_t * ) 0xe000e014 ) )
#define portNVIC_SYSTICK_CURRENT_VALUE_REG	( * ( ( volatile uint32_t * ) 0xe000e018 ) )
#define portNVIC_SYSPRI2_REG				( * ( ( volatile uint32_t * ) 0xe000ed20 ) )
/* ...然后是寄存器中的位。 */
#define portNVIC_SYSTICK_INT_BIT			( 1UL << 1UL )
#define portNVIC_SYSTICK_ENABLE_BIT			( 1UL << 0UL )
#define portNVIC_SYSTICK_COUNT_FLAG_BIT		( 1UL << 16UL )
#define portNVIC_PENDSVCLEAR_BIT 			( 1UL << 27UL )
#define portNVIC_PEND_SYSTICK_CLEAR_BIT		( 1UL << 25UL )

#define portNVIC_PENDSV_PRI					( ( ( uint32_t ) configKERNEL_INTERRUPT_PRIORITY ) << 16UL )
#define portNVIC_SYSTICK_PRI				( ( ( uint32_t ) configKERNEL_INTERRUPT_PRIORITY ) << 24UL )

/* 检查中断优先级有效性所需的常量。 */
#define portFIRST_USER_INTERRUPT_NUMBER		( 16 )
#define portNVIC_IP_REGISTERS_OFFSET_16 	( 0xE000E3F0 )
#define portAIRCR_REG						( * ( ( volatile uint32_t * ) 0xE000ED0C ) )
#define portMAX_8_BIT_VALUE					( ( uint8_t ) 0xff )
#define portTOP_BIT_OF_BYTE					( ( uint8_t ) 0x80 )
#define portMAX_PRIGROUP_BITS				( ( uint8_t ) 7 )
#define portPRIORITY_GROUP_MASK				( 0x07UL << 8UL )
#define portPRIGROUP_SHIFT					( 8UL )

/* 屏蔽 ICSR 寄存器中除 VECTACTIVE 位之外的所有位。 */
#define portVECTACTIVE_MASK					( 0xFFUL )

/* 操作 VFP 所需的常数。 */
#define portFPCCR							( ( volatile uint32_t * ) 0xe000ef34 ) /* 浮点上下文控制寄存器。 */
#define portASPEN_AND_LSPEN_BITS			( 0x3UL << 30UL )

/* 设置初始堆栈所需的常量。 */
#define portINITIAL_XPSR					( 0x01000000 )
#define portINITIAL_EXC_RETURN				( 0xfffffffd )

/* systick 是一个 24 位计数器。 */
#define portMAX_24_BIT_NUMBER				( 0xffffffUL )

/* 为了严格遵守 Cortex-M 规范，任务起始地址应将位
0 清零，因为它在从 ISR 退出时加载到 PC 中。 */
#define portSTART_ADDRESS_MASK		( ( StackType_t ) 0xfffffffeUL )

/* 用于估计在无滴答空闲期间 SysTick 计数器停
止时可能发生的 SysTick 计数的小提琴因子
计算。 */
#define portMISSED_COUNTS_FACTOR			( 45UL )

/* 让用户用 prvTaskExitError() 的地址
覆盖初始 LR 的预加载，以防它弄乱堆栈中的堆栈展开。
调试器。 */
#ifdef configTASK_RETURN_ADDRESS
	#define portTASK_RETURN_ADDRESS	configTASK_RETURN_ADDRESS
#else
	#define portTASK_RETURN_ADDRESS	prvTaskExitError
#endif

/*
 * 设置定时器以生成滴答中断。  该文
 * 件中的实现很弱，无法允许应用程序编
 * 写者更改用于生成滴答中断的计时器。
 */
void vPortSetupTimerInterrupt( void );

/*
 * 异常处理程序。
 */
void xPortPendSVHandler( void ) __attribute__ (( naked ));
void xPortSysTickHandler( void );
void vPortSVCHandler( void ) __attribute__ (( naked ));

/*
 * 启动第一个任务是一个单独的函数，因此可以单独测试它。
 */
static void prvPortStartFirstTask( void ) __attribute__ (( naked ));

/*
 * 启用 VFP 的函数。
 */
static void vPortEnableVFP( void ) __attribute__ (( naked ));

/*
 * 用于捕获尝试从其实现函数返回的任务。
 */
static void prvTaskExitError( void );

/*-----------------------------------------------------------*/

/* 每个任务在关键嵌套中维护自己的中断状态
多变的。 */
static UBaseType_t uxCriticalNesting = 0xaaaaaaaa;

/*
 * 构成一个滴答周期的 SysTick 增量数。
 */
#if( configUSE_TICKLESS_IDLE == 1 )
	static uint32_t ulTimerCountsForOneTick = 0;
#endif /* configUSE_TICKLESS_IDLE */

/*
 * 可抑制的最大节拍周期数受到
 * SysTick 定时器的 24 位分辨率的限制。
 */
#if( configUSE_TICKLESS_IDLE == 1 )
	static uint32_t xMaximumPossibleSuppressedTicks = 0;
#endif /* configUSE_TICKLESS_IDLE */

/*
 * 补偿 SysTick 为 stopped
 * 时经过的 CPU 周期（仅低功耗功能。
 */
#if( configUSE_TICKLESS_IDLE == 1 )
	static uint32_t ulStoppedTimerCompensation = 0;
#endif /* configUSE_TICKLESS_IDLE */

/*
 * 由
 * portASSERT_IF_INTERRUPT_PRIORITY_INVALID()
 * 宏使用，以确保不会从已分配优先级高于 configMAX_SYSCALL_INTERRUPT_PRIORITY 的中断调用 FreeRTOS API 函数。
 */
#if( configASSERT_DEFINED == 1 )
	 static uint8_t ucMaxSysCallPriority = 0;
	 static uint32_t ulMaxPRIGROUPValue = 0;
	 static const volatile uint8_t * const pcInterruptPriorityRegisters = ( const volatile uint8_t * const ) portNVIC_IP_REGISTERS_OFFSET_16;
#endif /* configASSERT_DEFINED */

/*-----------------------------------------------------------*/

/*
 * 参见头文件的描述。
 */
StackType_t *pxPortInitialiseStack( StackType_t *pxTopOfStack, TaskFunction_t pxCode, void *pvParameters )
{
	/* 模拟由上下文切换创建的堆栈帧
	打断。 */

	/* 添加偏移量是为了考虑 MCU 在中断进
	入/退出时使用堆栈的方式，并确保对齐。 */
	pxTopOfStack--;

	*pxTopOfStack = portINITIAL_XPSR;	/* xPSR */
	pxTopOfStack--;
	*pxTopOfStack = ( ( StackType_t ) pxCode ) & portSTART_ADDRESS_MASK;	/* 个人电脑 */
	pxTopOfStack--;
	*pxTopOfStack = ( StackType_t ) portTASK_RETURN_ADDRESS;	/* LR */

	/* 通过跳过寄存器初始化来节省代码空间。 */
	pxTopOfStack -= 5;	/* R12、R3、R2 和 R1。 */
	*pxTopOfStack = ( StackType_t ) pvParameters;	/* R0 */

	/* 使用的 save 方法要求每个任
	务维护自己的 exec 返回值。 */
	pxTopOfStack--;
	*pxTopOfStack = portINITIAL_EXC_RETURN;

	pxTopOfStack -= 8;	/* R11、R10、R9、R8、R7、R6、R5 和 R4。 */

	return pxTopOfStack;
}
/*-----------------------------------------------------------*/

static void prvTaskExitError( void )
{
volatile uint32_t ulDummy = 0;

	/* 实现任务的函数不得退出或尝试返回其调用者，因为
	没有任何内容可返回。  如果任务想要退出，则应
	调用 vTaskDelete( NULL )。

	如果定义了 configASSERT()，则人为地强制触发
	assert()，然后在此停止，以便应用程序编写者可以捕获错误。 */
	configASSERT( uxCriticalNesting == ~0UL );
	portDISABLE_INTERRUPTS();
	while( ulDummy == 0 )
	{
		/* 该文件在调度程序启动后调用
		prvTaskExitError()，以删
		除有关正在定义但从未调用的函数的编译器警告
		。  ulDummy 纯粹用于消除有关调用
		此函数后出现的代码的其他警告 - 使
		ulDummy 易失性使编译器认为该函数可
		以返回，因此不会对其后出现的代码输出“无法访问的代码”警告。 */
	}
}
/*-----------------------------------------------------------*/

void vPortSVCHandler( void )
{
	__asm volatile (
					"	ldr	r3, pxCurrentTCBConst2		\n" /* 恢复上下文。 */
					"	ldr r1, [r3]					\n" /* 使用 pxCurrentTCBConst 获取 pxCurrentTCB 地址。 */
					"	ldr r0, [r1]					\n" /* pxCurrentTCB 中的第一项是任务栈顶。 */
					"	ldmia r0!, {r4-r11, r14}		\n" /* 弹出异常条目和关键嵌套计数时未自动保存的寄存器。 */
					"	msr psp, r0						\n" /* 恢复任务堆栈指针。 */
					"	isb								\n"
					"	mov r0, #0 						\n"
					"	msr	basepri, r0					\n"
					"	bx r14							\n"
					"									\n"
					"	.align 4						\n"
					"pxCurrentTCBConst2: .word pxCurrentTCB				\n"
				);
}
/*-----------------------------------------------------------*/

static void prvPortStartFirstTask( void )
{
	/* 开始第一个任务。  这还会清除指示 FPU 正
	在使用的位，以防在调度程序启动之前使用 FPU
	，否则会导致 SVC 堆栈中不必要地留下空间
	for lazy saving of FPU registers. */
	__asm volatile(
					" ldr r0, =0xE000ED08 	\n" /* 使用 NVIC 偏移寄存器来定位堆栈。 */
					" ldr r0, [r0] 			\n"
					" ldr r0, [r0] 			\n"
					" msr msp, r0			\n" /* 将 msp 设置回堆栈的开头。 */
					" mov r0, #0			\n" /* 清除指示 FPU 正在使用的位，请参阅上面的注释。 */
					" msr control, r0		\n"
					" cpsie i				\n" /* 全局启用中断。 */
					" cpsie f				\n"
					" dsb					\n"
					" isb					\n"
					" svc 0					\n" /* 启动第一个任务的系统调用。 */
					" nop					\n"
				);
}
/*-----------------------------------------------------------*/

/*
 * 参见头文件的描述。
 */
BaseType_t xPortStartScheduler( void )
{
	/* configMAX_SYSCALL_INTERRUPT_PRIORITY 不得设置为 0。请参阅
	http://www.FreeRTOS.org/RTOS-Cortex-M3-M4.html */
	configASSERT( configMAX_SYSCALL_INTERRUPT_PRIORITY );

	#if( configASSERT_DEFINED == 1 )
	{
		volatile uint32_t ulOriginalPriority;
		volatile uint8_t * const pucFirstUserPriorityRegister = ( volatile uint8_t * const ) ( portNVIC_IP_REGISTERS_OFFSET_16 + portFIRST_USER_INTERRUPT_NUMBER );
		volatile uint8_t ucMaxPriorityValue;

		/* 确定可调用 ISR 安全 FreeRTOS API 函数
		的最大优先级。  ISR 安全函数是以“FromISR”
		结尾的函数。  FreeRTOS 维护单独的线程和
		ISR API 函数，以确保中断进入尽可能快速和简单。

		保存即将被破坏的中断优先级值。 */
		ulOriginalPriority = *pucFirstUserPriorityRegister;

		/* 确定可用优先级位的数量。
		首先写入所有可能的位。 */
		*pucFirstUserPriorityRegister = portMAX_8_BIT_VALUE;

		/* 读回该值以查看有多少位被卡住。 */
		ucMaxPriorityValue = *pucFirstUserPriorityRegister;

		/* 对最大系统调用优先级使用相同的掩码。 */
		ucMaxSysCallPriority = configMAX_SYSCALL_INTERRUPT_PRIORITY & ucMaxPriorityValue;

		/* 计算读回的位数的最大
		可接受优先级组值。 */
		ulMaxPRIGROUPValue = portMAX_PRIGROUP_BITS;
		while( ( ucMaxPriorityValue & portTOP_BIT_OF_BYTE ) == portTOP_BIT_OF_BYTE )
		{
			ulMaxPRIGROUPValue--;
			ucMaxPriorityValue <<= ( uint8_t ) 0x01;
		}

		#ifdef __NVIC_PRIO_BITS
		{
			/* 检查定义优先级位数的
			CMSIS 配置是否与从硬
			件实际查询的优先级位数匹配。 */
			configASSERT( ( portMAX_PRIGROUP_BITS - ulMaxPRIGROUPValue ) == __NVIC_PRIO_BITS );
		}
		#endif

		#ifdef configPRIO_BITS
		{
			/* 检查定义优先级位数的
			FreeRTOS 配置是否与
			从硬件实际查询的优先级位数匹配。 */
			configASSERT( ( portMAX_PRIGROUP_BITS - ulMaxPRIGROUPValue ) == configPRIO_BITS );
		}
		#endif

		/* 将优先级组值移回 AIRCR 中的位置
		登记。 */
		ulMaxPRIGROUPValue <<= portPRIGROUP_SHIFT;
		ulMaxPRIGROUPValue &= portPRIORITY_GROUP_MASK;

		/* 将被破坏的中断优先级寄存器恢复到原来的状态
		价值。 */
		*pucFirstUserPriorityRegister = ulOriginalPriority;
	}
	#endif /* conifgASSERT_DEFINED */

	/* 将PendSV和SysTick设置为最低优先级中断。 */
	portNVIC_SYSPRI2_REG |= portNVIC_PENDSV_PRI;
	portNVIC_SYSPRI2_REG |= portNVIC_SYSTICK_PRI;

	/* 启动生成刻度 ISR 的计时
	器。  这里已经禁用了中断。 */
	vPortSetupTimerInterrupt();

	/* 初始化关键嵌套计数，为第一个任务做好准备。 */
	uxCriticalNesting = 0;

	/* 确保 VFP 已启用 - 无论如何都应该启用。 */
	vPortEnableVFP();

	/* 总是懒惰保存。 */
	*( portFPCCR ) |= portASPEN_AND_LSPEN_BITS;

	/* 开始第一个任务。 */
	prvPortStartFirstTask();

	/* 永远不应该到达这里，因为任务现在正在执行！  调用任务退出
	错误函数，以防止在应用程序编写者通过定义
	configTASK_RETURN_ADDRESS 覆盖此
	功能的情况下编译器发出有关未调用静态函数的警告。  调用
	vTaskSwitchContext()，因此链接时间优化不会删除
	象征。 */
	vTaskSwitchContext();
	prvTaskExitError();

	/* 不应该来这里！ */
	return 0;
}
/*-----------------------------------------------------------*/

void vPortEndScheduler( void )
{
	/* 没有在没有什么可返回的端口
	中实现。人为地强制断言。 */
	configASSERT( uxCriticalNesting == 1000UL );
}
/*-----------------------------------------------------------*/

void vPortEnterCritical( void )
{
	portDISABLE_INTERRUPTS();
	uxCriticalNesting++;

	/* 这不是进入关键函数的中断安全版本，因此如果从中断
	上下文调用它，则为 assert()。  只有以
	“FromISR”结尾的 API 函数才能在中断
	中使用。  仅当关键嵌套计数为 1 时才断言，以
	防止在断言函数也使用关键部分时发生递归调用。 */
	if( uxCriticalNesting == 1 )
	{
		configASSERT( ( portNVIC_INT_CTRL_REG & portVECTACTIVE_MASK ) == 0 );
	}
}
/*-----------------------------------------------------------*/

void vPortExitCritical( void )
{
	configASSERT( uxCriticalNesting );
	uxCriticalNesting--;
	if( uxCriticalNesting == 0 )
	{
		portENABLE_INTERRUPTS();
	}
}
/*-----------------------------------------------------------*/

void xPortPendSVHandler( void )
{
	/* 这是一个裸函数。 */

	__asm volatile
	(
	"	mrs r0, psp							\n"
	"	isb									\n"
	"										\n"
	"	ldr	r3, pxCurrentTCBConst			\n" /* 获取当前TCB的位置。 */
	"	ldr	r2, [r3]						\n"
	"										\n"
	"	tst r14, #0x10						\n" /* 该任务是否使用 FPU 上下文？  如果是，则推高 vfp 寄存器。 */
	"	it eq								\n"
	"	vstmdbeq r0!, {s16-s31}				\n"
	"										\n"
	"	stmdb r0!, {r4-r11, r14}			\n" /* 保存核心寄存器。 */
	"	str r0, [r2]						\n" /* 将新的栈顶保存到 TCB 的第一个成员中。 */
	"										\n"
	"	stmdb sp!, {r0, r3}					\n"
	"	mov r0, %0 							\n"
	"	cpsid i								\n" /* 勘误解决方法。 */
	"	msr basepri, r0						\n"
	"	dsb									\n"
	"	isb									\n"
	"	cpsie i								\n" /* 勘误解决方法。 */
	"	bl vTaskSwitchContext				\n"
	"	mov r0, #0							\n"
	"	msr basepri, r0						\n"
	"	ldmia sp!, {r0, r3}					\n"
	"										\n"
	"	ldr r1, [r3]						\n" /* pxCurrentTCB 中的第一项是任务栈顶。 */
	"	ldr r0, [r1]						\n"
	"										\n"
	"	ldmia r0!, {r4-r11, r14}			\n" /* 弹出核心寄存器。 */
	"										\n"
	"	tst r14, #0x10						\n" /* 该任务是否使用 FPU 上下文？  如果是这样，也弹出高 vfp 寄存器。 */
	"	it eq								\n"
	"	vldmiaeq r0!, {s16-s31}				\n"
	"										\n"
	"	msr psp, r0							\n"
	"	isb									\n"
	"										\n"
	#ifdef WORKAROUND_PMU_CM001 /* XMC4000 特定勘误表解决方法。 */
		#if WORKAROUND_PMU_CM001 == 1
	"			push { r14 }				\n"
	"			pop { pc }					\n"
		#endif
	#endif
	"										\n"
	"	bx r14								\n"
	"										\n"
	"	.align 4							\n"
	"pxCurrentTCBConst: .word pxCurrentTCB	\n"
	::"i"(configMAX_SYSCALL_INTERRUPT_PRIORITY)
	);
}
/*-----------------------------------------------------------*/

void xPortSysTickHandler( void )
{
	/* SysTick 以最低中断优先级运行，因此当该
	中断执行时，必须取消屏蔽所有中断。  因此不需
	要保存然后恢复中断屏蔽值，因为它的值已经是
	已知。 */
	portDISABLE_INTERRUPTS();
	{
		/* 增加 RTOS 刻度。 */
		if( xTaskIncrementTick() != pdFALSE )
		{
			/* 需要上下文切换。  上下文切换是在PendSV
			中断中进行的。  挂起 PendSV 中断。 */
			portNVIC_INT_CTRL_REG = portNVIC_PENDSVSET_BIT;
		}
	}
	portENABLE_INTERRUPTS();
}
/*-----------------------------------------------------------*/

#if( configUSE_TICKLESS_IDLE == 1 )

	__attribute__((weak)) void vPortSuppressTicksAndSleep( TickType_t xExpectedIdleTime )
	{
	uint32_t ulReloadValue, ulCompleteTickPeriods, ulCompletedSysTickDecrements;
	TickType_t xModifiableIdleTime;

		/* 确保 SysTick 重载值不会溢出计数器。 */
		if( xExpectedIdleTime > xMaximumPossibleSuppressedTicks )
		{
			xExpectedIdleTime = xMaximumPossibleSuppressedTicks;
		}

		/* 暂时停止 SysTick。
		SysTick 停止的时间已被尽可能地考
		虑在内，但使用无滴答模式将不可避免地导致
		内核维护的时间相对于日历时间出现一些微小的漂移。 */
		portNVIC_SYSTICK_CTRL_REG &= ~portNVIC_SYSTICK_ENABLE_BIT;

		/* 计算等待
		xExpectedIdleTime 周期所
		需的重载值。  使用 -1 是因为此代码将在其中一个刻度周期中部分执行。 */
		ulReloadValue = portNVIC_SYSTICK_CURRENT_VALUE_REG + ( ulTimerCountsForOneTick * ( xExpectedIdleTime - 1UL ) );
		if( ulReloadValue > ulStoppedTimerCompensation )
		{
			ulReloadValue -= ulStoppedTimerCompensation;
		}

		/* 输入临界区，但不要使用
		taskENTER_CRITICAL() 方法，因为这将屏蔽应退出睡眠模式的中断。 */
		__asm volatile( "cpsid i" ::: "memory" );
		__asm volatile( "dsb" );
		__asm volatile( "isb" );

		/* 如果上下文切换挂起或任务正在等待调
		度程序取消挂起，则放弃低功耗条目。 */
		if( eTaskConfirmSleepModeStatus() == eAbortSleep )
		{
			/* 从计数寄存器中剩余的内容重
			新启动以完成此滴答周期。 */
			portNVIC_SYSTICK_LOAD_REG = portNVIC_SYSTICK_CURRENT_VALUE_REG;

			/* 重新启动SysTick。 */
			portNVIC_SYSTICK_CTRL_REG |= portNVIC_SYSTICK_ENABLE_BIT;

			/* 将重载寄存器重置为正常滴答所需的值
			期间。 */
			portNVIC_SYSTICK_LOAD_REG = ulTimerCountsForOneTick - 1UL;

			/* 重新启用中断 - 请参阅 cpsid instruction() 上面的注释
			多于。 */
			__asm volatile( "cpsie i" ::: "memory" );
		}
		else
		{
			/* 设置新的重载值。 */
			portNVIC_SYSTICK_LOAD_REG = ulReloadValue;

			/* 清除SysTick计数标志并将计数值设置回
			零。 */
			portNVIC_SYSTICK_CURRENT_VALUE_REG = 0UL;

			/* 重新启动SysTick。 */
			portNVIC_SYSTICK_CTRL_REG |= portNVIC_SYSTICK_ENABLE_BIT;

			/* 睡觉直到有事发生。
			configPRE_SLEEP_PROCESSING()
			可以将其参数设置为0，表示其实现包含自己的等待
			中断或等待事件指令，因此wfi不应再次执行。
			但是，原始预期空闲时间变量必须保持不变，因此需要复制一份。 */
			xModifiableIdleTime = xExpectedIdleTime;
			configPRE_SLEEP_PROCESSING( xModifiableIdleTime );
			if( xModifiableIdleTime > 0 )
			{
				__asm volatile( "dsb" ::: "memory" );
				__asm volatile( "wfi" );
				__asm volatile( "isb" );
			}
			configPOST_SLEEP_PROCESSING( xExpectedIdleTime );

			/* 重新启用中断以允许使 MCU 脱离睡眠模式的中
			断立即执行。  请参阅上面
			__disable_interrupt() 调用上面的注释。 */
			__asm volatile( "cpsie i" ::: "memory" );
			__asm volatile( "dsb" );
			__asm volatile( "isb" );

			/* 再次禁用中断，因为时钟即将停止，并且
			时钟停止时执行的中断将增加 RTOS
			维护的时间和日历之间的任何滑动
			时间。 */
			__asm volatile( "cpsid i" ::: "memory" );
			__asm volatile( "dsb" );
			__asm volatile( "isb" );

			/* 在不读取
			portNVIC_SYSTICK_CTRL_REG 寄存
			器的情况下禁用 SysTick 时钟，以确保
			portNVIC_SYSTICK_COUNT_FLAG_BIT
			在置位后不会被清除。  同样，SysTick 停止的时
			间已被尽可能地考虑在内，但使用无滴答模式将不可避免地导致内核维护的时间相对于日历出现一些微小的漂移
			时间*/
			portNVIC_SYSTICK_CTRL_REG = ( portNVIC_SYSTICK_CLK_BIT | portNVIC_SYSTICK_INT_BIT );

			/* 确定 SysTick 时钟是否已计数到零并设置
			回当前重载 value（重载返回在整个预期空闲
			时间内正确），或者 SysTick 是否尚未计
			数到 zero（在这种情况下，除
			SysTick 之外的中断必定使系统脱离睡眠模式）。 */
			if( ( portNVIC_SYSTICK_CTRL_REG & portNVIC_SYSTICK_COUNT_FLAG_BIT ) != 0 )
			{
				uint32_t ulCalculatedLoadValue;

				/* 滴答中断已挂起，并且 SysTick 计数已用
				ulReloadValue 重新加载。  使用此刻度的剩
				余内容重置 portNVIC_SYSTICK_LOAD_REG
				时期。 */
				ulCalculatedLoadValue = ( ulTimerCountsForOneTick - 1UL ) - ( ulReloadValue - portNVIC_SYSTICK_CURRENT_VALUE_REG );

				/* 不要允许一个很小的值，或者
				由于睡眠后钩子执行了太长时
				间的操作而导致下溢的值。 */
				if( ( ulCalculatedLoadValue < ulStoppedTimerCompensation ) || ( ulCalculatedLoadValue > ulTimerCountsForOneTick ) )
				{
					ulCalculatedLoadValue = ( ulTimerCountsForOneTick - 1UL );
				}

				portNVIC_SYSTICK_LOAD_REG = ulCalculatedLoadValue;

				/* 由于一旦该函数退出，待处理的报价
				就会被处理，因此该报价所维护的报
				价值会比等待时间少一步前进。 */
				ulCompleteTickPeriods = xExpectedIdleTime - 1UL;
			}
			else
			{
				/* 除了滴答中断之外的其他因素结束了睡
				眠。计算出睡眠持续了多长时间以完成
				刻度 periods（不是占部分刻
				度的 ulReload 值）。 */
				ulCompletedSysTickDecrements = ( xExpectedIdleTime * ulTimerCountsForOneTick ) - portNVIC_SYSTICK_CURRENT_VALUE_REG;

				/* 处理器等待时经过了多
				少个完整的滴答周期？ */
				ulCompleteTickPeriods = ulCompletedSysTickDecrements / ulTimerCountsForOneTick;

				/* 重新加载值设置为单个滴
				答周期剩余的任何部分。 */
				portNVIC_SYSTICK_LOAD_REG = ( ( ulCompleteTickPeriods + 1UL ) * ulTimerCountsForOneTick ) - ulCompletedSysTickDecrements;
			}

			/* 重新启动 SysTick，使其再次从
			portNVIC_SYSTICK_LOAD_REG 运行，然后将 portNVIC_SYSTICK_LOAD_REG 设置回其标准
			价值。 */
			portNVIC_SYSTICK_CURRENT_VALUE_REG = 0UL;
			portNVIC_SYSTICK_CTRL_REG |= portNVIC_SYSTICK_ENABLE_BIT;
			vTaskStepTick( ulCompleteTickPeriods );
			portNVIC_SYSTICK_LOAD_REG = ulTimerCountsForOneTick - 1UL;

			/* 退出并启用中断。 */
			__asm volatile( "cpsie i" ::: "memory" );
		}
	}

#endif /* #if configUSE_TICKLESS_IDLE */
/*-----------------------------------------------------------*/

/*
 * 设置 sysstick 定时器以在需要时生成滴答中断
 * 频率。
 */
__attribute__(( weak )) void vPortSetupTimerInterrupt( void )
{
	/* 计算配置节拍中断所需的常量。 */
	#if( configUSE_TICKLESS_IDLE == 1 )
	{
		ulTimerCountsForOneTick = ( configSYSTICK_CLOCK_HZ / configTICK_RATE_HZ );
		xMaximumPossibleSuppressedTicks = portMAX_24_BIT_NUMBER / ulTimerCountsForOneTick;
		ulStoppedTimerCompensation = portMISSED_COUNTS_FACTOR / ( configCPU_CLOCK_HZ / configSYSTICK_CLOCK_HZ );
	}
	#endif /* configUSE_TICKLESS_IDLE */

	/* 停止并清除SysTick。 */
	portNVIC_SYSTICK_CTRL_REG = 0UL;
	portNVIC_SYSTICK_CURRENT_VALUE_REG = 0UL;

	/* 配置 SysTick 以请求的速率中断。 */
	portNVIC_SYSTICK_LOAD_REG = ( configSYSTICK_CLOCK_HZ / configTICK_RATE_HZ ) - 1UL;
	portNVIC_SYSTICK_CTRL_REG = ( portNVIC_SYSTICK_CLK_BIT | portNVIC_SYSTICK_INT_BIT | portNVIC_SYSTICK_ENABLE_BIT );
}
/*-----------------------------------------------------------*/

/* 这是一个裸函数。 */
static void vPortEnableVFP( void )
{
	__asm volatile
	(
		"	ldr.w r0, =0xE000ED88		\n" /* FPU 使能位位于 CPACR 中。 */
		"	ldr r1, [r0]				\n"
		"								\n"
		"	orr r1, r1, #( 0xf << 20 )	\n" /* 启用 CP10 和 CP11 协处理器，然后保存回来。 */
		"	str r1, [r0]				\n"
		"	bx r14						"
	);
}
/*-----------------------------------------------------------*/

#if( configASSERT_DEFINED == 1 )

	void vPortValidateInterruptPriority( void )
	{
	uint32_t ulCurrentInterrupt;
	uint8_t ucCurrentPriority;

		/* 获取当前正在执行的中断号。 */
		__asm volatile( "mrs %0, ipsr" : "=r"( ulCurrentInterrupt ) :: "memory" );

		/* 中断号是用户定义的中断吗？ */
		if( ulCurrentInterrupt >= portFIRST_USER_INTERRUPT_NUMBER )
		{
			/* 查看中断的优先级。 */
			ucCurrentPriority = pcInterruptPriorityRegisters[ ulCurrentInterrupt ];

			/* 如果已分配优先级高于
			configMAX_SYSCALL_INTERRUPT_PRIORITY
			的中断的服务 routine (ISR) 调用 ISR 安全
			FreeRTOS API 函数，则以下断言将失败。  ISR
			安全 FreeRTOS API 函数必须“仅”从已分配优先级等于或低于优先级的中断中调用
			configMAX_SYSCALL_INTERRUPT_PRIORITY.

			数值低的中断优先级数字代表逻辑上
			高的中断优先级，因此中断的优先级
			必须设置为等于或数值“高于”
			configMAX_SYSCALL_INTERRUPT_PRIORITY.

			使用 FreeRTOS API 的中断不得保留默
			认优先级 0，因为这是可能的最高优先级，保证高于
			
			configMAX_SYSCALL_INTERRUPT_PRIORITY，因此也保证无效。

			FreeRTOS 维护单独的线程和 ISR
			API 函数，以确保中断进入尽可能快速和简单。

			以下链接提供详细信息：
			http://www.freertos.org/RTOS-Cortex-M3-M4.html
			http://www.freertos.org/FAQHelp.html */
			configASSERT( ucCurrentPriority >= ucMaxSysCallPriority );
		}

		/* 优先级分组：中断 controller (
		NVIC) 允许将定义每个中断优先级的位分
		为定义中断抢占优先级的位和定义中断子优先级
		的位。  为简单起见，所有位必须定义为抢占
		优先级位。  如果这不是 case（如果某
		些位代表子优先级），则以下断言将失败。

		如果应用程序仅使用 CMSIS 库进行中断配置，则
		可以通过调用
		NVIC_SetPriorityGrouping(
		0 ); 在所有 Cortex-M 设备上实现正
		确的设置；在启动调度程序之前。  但请注意，某些供
		应商特定的外设库假定非零优先级组设置，在这种情况下使用零值将导致不可预测的行为。 */
		configASSERT( ( portAIRCR_REG & portPRIORITY_GROUP_MASK ) <= ulMaxPRIGROUPValue );
	}

#endif /* configASSERT_DEFINED */


