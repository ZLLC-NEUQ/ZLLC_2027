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

#ifndef EVENT_GROUPS_H
#define EVENT_GROUPS_H

#ifndef INC_FREERTOS_H
	#error "include FreeRTOS.h" must appear in source files before "include event_groups.h"
#endif

/* FreeRTOS 头文件。 */
#include "timers.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 事件组是应用程序可以为其分配含义的位的集合。
 * 例如，应用程序可以创建一个事件组来传达各种
 * CAN 总线相关事件的状态，其中位 0 可能表示
 * “已收到 CAN 消息并准备好处理”，位 1 可
 * 能表示“应用程序已将准备发送到 CAN 网络的消
 * 息排队”，位 2 可能表示“是时候将 SYNC
 * 消息发送到然后，任务可以测试位值以查看哪些事件处
 * 于活动状态，并且可以选择进入阻塞状态以等待指定位
 * 或一组指定位处于活动状态。  继续 CAN 总线
 * 示例，CAN 控制任务可以进入阻塞 state（
 * 因此不消耗任何处理时间），直到位 0、位 1 或
 * 位 2 激活，此时实际激活的位将通知任务它必须对
 * take 执行哪个操作（处理接收到的消息、发送
 * 消息或发送消息）。 SYNC）。
 *
 * 事件组实现包含避免竞争条件的智能，否
 * 则如果应用程序出于相同目的使用简单变
 * 量，就会出现竞争条件。  这对于何时
 * 要清除事件组内的位以及何时必须设置位
 * 然后以原子方式进行测试尤其重要 -
 * 正如事件组用于在多个 tasks（“
 * 集合点”）之间创建同步点的情况。
 *
 * \defgroup EventGroup
 */



/**
 * event_groups.h
 *
 * 输入引用事件组的类型。  例如，对
 * xEventGroupCreate() 的调用返回
 * EventGroupHandle_t 变量，该变量随后可用作其他事件组函数的参数。
 *
 * \defgroup EventGroupHandle_t EventGroupHandle_t
 * \ingroup EventGroup
 */
struct EventGroupDef_t;
typedef struct EventGroupDef_t * EventGroupHandle_t;

/*
 * 保存事件位的类型始终与 TickType_t 匹配 - 因此它保
 * 存的位数由 configUSE_16_BIT_TICKS 设置（
 * 如果设置为 1，则为 16 位；如果设置为 0，则为 32 位。
 *
 * \defgroup EventBits_t EventBits_t
 * \ingroup EventGroup
 */
typedef TickType_t EventBits_t;

/**
 * event_groups.h
 *<pre>
 EventGroupHandle_t xEventGroupCreate( void );
 </pre>
 *
 * 创建一个新的事件组。
 *
 * 在内部，在 FreeRTOS 实现中，事件组使用一个[小]内存
 * 块，其中存储了事件组的结构。  如果使用
 * xEventGropuCreate() 创建事件组，则所需的内
 * 存会在 xEventGroupCreate() 函数内自动动态
 * 分配。 （请参阅
 * http://www.freertos.org/a00111.html).
 * 如果使用 xEventGropuCreateStatic()
 * 创建事件组，则应用程序编写器必须提供事件组将使用的内存。因此
 * ，xEventGroupCreateStatic() 允许在不使用任何动态内存分配的情况下创建事件组。
 *
 * 尽管事件组与刻度无关，但由于内部实现原因，事件组中可用的位数取决于
 * FreeRTOSConfig.h 中的
 * configUSE_16_BIT_TICKS 设置。  如果
 * configUSE_16_BIT_TICKS 为 1，则每个事件组包
 * 含 8 个可用的 bits（位 0 至位 7）。  如果
 * configUSE_16_BIT_TICKS 设置为 0，则每个事件
 * 组有 24 个可用的 bits（位 0 至位 23）。  EventBits_t 类型用于存储事件组内的事件位。
 *
 * @return 如果创建了事件组，则事件组的句柄为
 * 回来了。  如果没有足够的 FreeRTOS 堆可用于创建事件组，则返回 NULL
 * 。  参见 http://www.freertos.org/a00111.html
 *
 * 用法示例：
   <pre>
	//声明一个变量来保存创建的事件组。
	EventGroupHandle_t xCreatedEventGroup;

	//尝试创建事件组。
	xCreatedEventGroup = xEventGroupCreate();

	//活动组创建成功吗？
	if( xCreatedEventGroup == NULL )
	{
		//由于没有足够的可用
		//FreeRTOS 堆，因此未创建事件组。
	}
	else
	{
		//事件组已创建。
	}
   </pre>
 * \defgroup xEventGroupCreate xEventGroupCreate
 * \ingroup EventGroup
 */
#if( configSUPPORT_DYNAMIC_ALLOCATION == 1 )
	EventGroupHandle_t xEventGroupCreate( void ) PRIVILEGED_FUNCTION;
#endif

/**
 * event_groups.h
 *<pre>
 EventGroupHandle_t xEventGroupCreateStatic( EventGroupHandle_t * pxEventGroupBuffer );
 </pre>
 *
 * 创建一个新的事件组。
 *
 * 在内部，在 FreeRTOS 实现中，事件组使用一个[小]内存
 * 块，其中存储了事件组的结构。  如果使用
 * xEventGropuCreate() 创建事件组，则所需的内
 * 存会在 xEventGroupCreate() 函数内自动动态
 * 分配。 （请参阅
 * http://www.freertos.org/a00111.html).
 * 如果使用 xEventGropuCreateStatic()
 * 创建事件组，则应用程序编写器必须提供事件组将使用的内存。因此
 * ，xEventGroupCreateStatic() 允许在不使用任何动态内存分配的情况下创建事件组。
 *
 * 尽管事件组与刻度无关，但由于内部实现原因，事件组中可用的位数取决于
 * FreeRTOSConfig.h 中的
 * configUSE_16_BIT_TICKS 设置。  如果
 * configUSE_16_BIT_TICKS 为 1，则每个事件组包
 * 含 8 个可用的 bits（位 0 至位 7）。  如果
 * configUSE_16_BIT_TICKS 设置为 0，则每个事件
 * 组有 24 个可用的 bits（位 0 至位 23）。  EventBits_t 类型用于存储事件组内的事件位。
 *
 * @param pxEventGroupBuffer pxEventGroupBuffer 必须指向类型的变量
 * StaticEventGroup_t，然后将用
 * 于保存事件组的数据结构，从而无需动态分配内存。
 *
 * @return 如果创建了事件组，则事件组的句柄为
 * 回来了。  如果 pxEventGroupBuffer 是 NULL，则返回 NULL。
 *
 * 用法示例：
   <pre>
	//StaticEventGroup_t 是一个可公开访问
	//的结构，具有与真实事件组结构相同的大小和对齐要求。
	//它作为一种机制提供给应用程序以了解事件 group 的
	//大小（这取决于体系结构和配置文件设置），而不会通过暴露
	//真实事件组内部来破坏严格的数据隐藏策略。  此
	//StaticEventGroup_t 变量被传递到
	//xSemaphoreCreateEventGroupStatic()
	//函数中，用于存储事件组的数据结构
	StaticEventGroup_t xEventGroupBuffer;

	//创建事件组而不动态分配任何内存。
	xEventGroup = xEventGroupCreateStatic( &xEventGroupBuffer );
   </pre>
 */
#if( configSUPPORT_STATIC_ALLOCATION == 1 )
	EventGroupHandle_t xEventGroupCreateStatic( StaticEventGroup_t *pxEventGroupBuffer ) PRIVILEGED_FUNCTION;
#endif

/**
 * event_groups.h
 *<pre>
	EventBits_t xEventGroupWaitBits( 	EventGroupHandle_t xEventGroup,
										const EventBits_t uxBitsToWaitFor,
										const BaseType_t xClearOnExit,
										const BaseType_t xWaitForAllBits,
										const TickType_t xTicksToWait );
 </pre>
 *
 * [可能] 阻止等待在先前创建
 * 的事件组中设置一个或多个位。
 *
 * 该函数不能从中断中调用。
 *
 * @param xEventGroup 正在测试位的事件组。  的
 * 事件组之前必须已通过调用
 * xEventGroupCreate() 创建。
 *
 * @param uxBitsToWaitFor 指示要测试的一个或多个位的按位值
 * 活动组内。  例如，要等待位 0 和/或位 2，请将
 * uxBitsToWaitFor 设置为 0x05。  要等待位 0 和/
 * 或位 1 和/或位 2，请将 uxBitsToWaitFor 设置为 0x07。  等等
 *
 * @param xClearOnExit 如果 xClearOnExit 设置为 pdTRUE，则其中的任何位
 * 如果等待条件为 met（如果函数由于超时以外的原因返回），则在事件
 * 组内设置的 uxBitsToWaitFor 将在
 * xEventGroupWaitBits() 返回之前被清除。  如
 * 果 xClearOnExit 设置为 pdFALSE，则当对
 * xEventGroupWaitBits() 的调用返回时，事件组中设置的位不会更改。
 *
 * @param xWaitForAllBits 如果 xWaitForAllBits 设置为 pdTRUE 则
 * 当 uxBitsToWaitFor 中的所有位均已设置或指定的块时间到期时，
 * xEventGroupWaitBits() 将返回。  如果
 * xWaitForAllBits 设置为 pdFALSE，则当
 * uxBitsToWaitFor 中设置的任何一位被设置或指定的块时间到期时，
 * xEventGroupWaitBits() 将返回。  区块时间由xTicksToWait参数指定。
 *
 * @param xTicksToWait time 的最大等待量（以“ticks”指定）
 * for one/all (depending on the xWaitForAllBits value) of the bits specified by
 * uxBitsToWaitFor 成为设定。
 *
 * @return 正在等待的位时事件组的值
 * for became set, or the block time expired.  Test the return value to know
 * 设置了哪些位。  如果
 * xEventGroupWaitBits() 由于超时而返回，
 * 则并非所有正在等待的位都将被设置。  如果
 * xEventGroupWaitBits() 因为它正在等待的
 * 位被设置而返回，则在 xClearOnExit 参数设置为的情况下，返回的值是自动清除任何位之前的事件组值
 * pdTRUE.
 *
 * 用法示例：
   <pre>
   #define BIT_0	( 1 << 0 )
   #define BIT_4	( 1 << 4 )

   void aFunction( EventGroupHandle_t xEventGroup )
   {
   EventBits_t uxBits;
   const TickType_t xTicksToWait = 100 / portTICK_PERIOD_MS;

		//等待最多 100ms，以便在事件组内设置
		//位 0 或位 4。  退出前清除这些位。
		uxBits = xEventGroupWaitBits(
					xEventGroup,	// The event group being tested.
					BIT_0 | BIT_4,	// The bits within the event group to wait for.
					pdTRUE,			// BIT_0 and BIT_4 should be cleared before returning.
					pdFALSE,		// Don't wait for both bits, either bit will do.
					xTicksToWait );	// Wait a maximum of 100ms for either bit to be set.

		if( ( uxBits & ( BIT_0 | BIT_4 ) ) == ( BIT_0 | BIT_4 ) )
		{
			//xEventGroupWaitBits() 返回，因为两个位均已设置。
		}
		else if( ( uxBits & BIT_0 ) != 0 )
		{
			//xEventGroupWaitBits() 返回，因为仅设置了 BIT_0。
		}
		else if( ( uxBits & BIT_4 ) != 0 )
		{
			//xEventGroupWaitBits() 返回，因为仅设置了 BIT_4。
		}
		else
		{
			//xEventGroupWaitBits() 返回，因为
			//xTicksToWait 时钟周期过去了，而 BIT_0 或 BIT_4 都没有被设置。
		}
   }
   </pre>
 * \defgroup xEventGroupWaitBits xEventGroupWaitBits
 * \ingroup EventGroup
 */
EventBits_t xEventGroupWaitBits( EventGroupHandle_t xEventGroup, const EventBits_t uxBitsToWaitFor, const BaseType_t xClearOnExit, const BaseType_t xWaitForAllBits, TickType_t xTicksToWait ) PRIVILEGED_FUNCTION;

/**
 * event_groups.h
 *<pre>
	EventBits_t xEventGroupClearBits( EventGroupHandle_t xEventGroup, const EventBits_t uxBitsToClear );
 </pre>
 *
 * 清除事件组内的位。  该函数不能从
 * 打断。
 *
 * @param xEventGroup 要清除其中位的事件组。
 *
 * @param uxBitsToClear 指示要清除的一个或多个位的按位值
 * 在活动组中。  例如，要仅清除位 3，请将 uxBitsToClear 设置为 0
 * x08。  要清除位 3 和位 0，请将 uxBitsToClear 设置为 0x09。
 *
 * @return 指定位被清除之前事件组的值。
 *
 * 用法示例：
   <pre>
   #define BIT_0	( 1 << 0 )
   #define BIT_4	( 1 << 4 )

   void aFunction( EventGroupHandle_t xEventGroup )
   {
   EventBits_t uxBits;

		//清除 xEventGroup 中的位 0 和位 4。
		uxBits = xEventGroupClearBits(
								xEventGroup,	// The event group being updated.
								BIT_0 | BIT_4 );// The bits being cleared.

		if( ( uxBits & ( BIT_0 | BIT_4 ) ) == ( BIT_0 | BIT_4 ) )
		{
			//位 0 和位 4 均在调用
			//xEventGroupClearBits() 之前设置。  现在两者都是 clear（未设置）。
		}
		else if( ( uxBits & BIT_0 ) != 0 )
		{
			//位 0 在调用
			//xEventGroupClearBits() 之前设置。  现在一切都清楚了。
		}
		else if( ( uxBits & BIT_4 ) != 0 )
		{
			//位 4 在调用
			//xEventGroupClearBits() 之前设置。  现在一切都清楚了。
		}
		else
		{
			//位 0 和位 4 都没有首先被设置。
		}
   }
   </pre>
 * \defgroup xEventGroupClearBits xEventGroupClearBits
 * \ingroup EventGroup
 */
EventBits_t xEventGroupClearBits( EventGroupHandle_t xEventGroup, const EventBits_t uxBitsToClear ) PRIVILEGED_FUNCTION;

/**
 * event_groups.h
 *<pre>
	BaseType_t xEventGroupClearBitsFromISR( EventGroupHandle_t xEventGroup, const EventBits_t uxBitsToSet );
 </pre>
 *
 * 可以从中断调用的 xEventGroupClearBits() 版本。
 *
 * 设置事件组中的位不是确定性操作，因为可能有
 * 未知数量的任务正在等待设置一个或多个位。
 * FreeRTOS 不允许执行非确定性操作
 * while interrupts are disabled, so protects event groups that are accessed
 * 通过挂起调度程序而不是禁用中断来退出任务。  因此，
 * 事件组不能直接从中断服务程序访问。  因此，
 * xEventGroupClearBitsFromISR()
 * 向定时器任务发送消息，以在定时器上下文中执行清除操作
 * 任务。
 *
 * @param xEventGroup 要清除其中位的事件组。
 *
 * @param uxBitsToClear 指示要清除的一个或多个位的按位值。
 * 例如，要仅清除位 3，请将 uxBitsToClear 设置为 0x08。
 * 要清除位 3 和位 0，请将 uxBitsToClear 设置为 0x09。
 *
 * @return 如果执行该功能的请求已成功发布，则
 * 返回 pdPASS，否则返回 pdFALSE。  pdFALSE 将被退回
 * if the timer service queue was full.
 *
 * 用法示例：
   <pre>
   #define BIT_0	( 1 << 0 )
   #define BIT_4	( 1 << 4 )

   //假设已经通过调用
   //xEventGroupCreate() 创建了事件组。
   EventGroupHandle_t xEventGroup;

   void anInterruptHandler( void )
   {
		//清除 xEventGroup 中的位 0 和位 4。
		xResult = xEventGroupClearBitsFromISR(
							xEventGroup,	 // The event group being updated.
							BIT_0 | BIT_4 ); // The bits being set.

		if( xResult == pdPASS )
		{
			//消息已成功发布。
		}
  }
   </pre>
 * \defgroup xEventGroupClearBitsFromISR xEventGroupClearBitsFromISR
 * \ingroup EventGroup
 */
#if( configUSE_TRACE_FACILITY == 1 )
	BaseType_t xEventGroupClearBitsFromISR( EventGroupHandle_t xEventGroup, const EventBits_t uxBitsToClear ) PRIVILEGED_FUNCTION;
#else
	#define xEventGroupClearBitsFromISR( xEventGroup, uxBitsToClear ) xTimerPendFunctionCallFromISR( vEventGroupClearBitsCallback, ( void * ) xEventGroup, ( uint32_t ) uxBitsToClear, NULL )
#endif

/**
 * event_groups.h
 *<pre>
	EventBits_t xEventGroupSetBits( EventGroupHandle_t xEventGroup, const EventBits_t uxBitsToSet );
 </pre>
 *
 * 设置事件组内的位。该函数不能从中断中调用。
 * 
 * xEventGroupSetBitsFromISR()是可以从中断调用的版本。
 *
 * 设置事件组中的位将自动
 * 解除阻塞等待位的任务。
 *
 * @param xEventGroup 要设置其中位的事件组。
 *
 * @param uxBitsToSet 指示要设置的一个或多个位的按位值。
 * 例如，要仅设置位 3，请将 uxBitsToSet 设置为 0x08。
 * 要设置位 3 和位 0，请将 uxBitsToSet 设置为 0x09。
 *
 * @return 调用时事件组的值
 * xEventGroupSetBits() 返回。  返回值可能会
 * 清除 uxBitsToSet 参数指定的位有两个原因。  首先，
 * if setting a bit results in a task that was waiting for the bit leaving the
 * 阻塞状态，则该位可能会被清除 automatically（参见
 * xEventGroupWaitBits() 的
 * xClearBitOnExit 参数）。  其次，优先级高于调用
 * xEventGroupSetBits() 的任务的任何 unblocked
 * （或其他就绪状态）任务都将执行，并可能在调用 xEventGroupSetBits() 返回之前更改事件组值。
 *
 * 用法示例：
   <pre>
   #define BIT_0	( 1 << 0 )
   #define BIT_4	( 1 << 4 )

   void aFunction( EventGroupHandle_t xEventGroup )
   {
   EventBits_t uxBits;

		//设置 xEventGroup 中的位 0 和位 4。
		uxBits = xEventGroupSetBits(
							xEventGroup,	// The event group being updated.
							BIT_0 | BIT_4 );// The bits being set.

		if( ( uxBits & ( BIT_0 | BIT_4 ) ) == ( BIT_0 | BIT_4 ) )
		{
			//当函数返回时，位 0 和位 4 均保持设置状态。
		}
		else if( ( uxBits & BIT_0 ) != 0 )
		{
			//当函数返回时，位 0 保持设置状态，但位 4
			//被清除。  当正在等待位 4 的任务从阻塞
			//状态中删除时，位 4 可能会被自动清除。
			//state.
		}
		else if( ( uxBits & BIT_4 ) != 0 )
		{
			//当函数返回时，位 4 保持设置状态，但位 0
			//被清除。  可能是因为正在等待位 0 的任
			//务被从阻塞中删除，所以位 0 被自动清除。
			//state.
		}
		else
		{
			//位 0 和位 4 均未保持设置状态。
			//任务可能正在等待这两个位都被设置，
			//并且当任务离开阻塞状态时这些位被清除。
		}
   }
   </pre>
 * \defgroup xEventGroupSetBits xEventGroupSetBits
 * \ingroup EventGroup
 */
EventBits_t xEventGroupSetBits( EventGroupHandle_t xEventGroup, const EventBits_t uxBitsToSet ) PRIVILEGED_FUNCTION;

/**
 * event_groups.h
 *<pre>
	BaseType_t xEventGroupSetBitsFromISR( EventGroupHandle_t xEventGroup, const EventBits_t uxBitsToSet, BaseType_t *pxHigherPriorityTaskWoken );
 </pre>
 *
 * 可以从中断调用的 xEventGroupSetBits() 版本。
 *
 * 设置事件组中的位不是确定性操作，因为可能有未知
 * 数量的任务正在等待设置一个或多个位。
 * FreeRTOS 不允许在中断中或从关键部分执
 * 行非确定性操作。  因此，
 * xEventGroupSetBitsFromISR()
 * 向定时器任务发送一条消息，以在定时器任务的上
 * 下文中执行设置操作 - 其中使用调度程序锁代替关键部分。
 *
 * @param xEventGroup 要设置其中位的事件组。
 *
 * @param uxBitsToSet 指示要设置的一个或多个位的按位值。
 * 例如，要仅设置位 3，请将 uxBitsToSet 设置为 0x08。
 * 要设置位 3 和位 0，请将 uxBitsToSet 设置为 0x09。
 *
 * @param pxHigherPriorityTaskWoken 如上所述，调用该函数
 * 将导致一条消息被发送到计时器守护程序任务。  如果定
 * 时器守护任务的优先级高于当前运行的task（中断中断
 * 的任务）的优先级，则*
 * pxHigherPriorityTaskWoken将
 * 被
 * xEventGroupSetBitsFromISR()
 * 设置为pdTRUE，表示在中断退出之前应请求上下文切
 * 换。  因此，*pxHigherPriorityTaskWoken 必须初始化为 pdFALSE。  请参阅下面的示例代码。
 *
 * @return 如果执行该功能的请求已成功发布，则
 * 返回 pdPASS，否则返回 pdFALSE。  pdFALSE 将被退回
 * if the timer service queue was full.
 *
 * 用法示例：
   <pre>
   #define BIT_0	( 1 << 0 )
   #define BIT_4	( 1 << 4 )

   //假设已经通过调用
   //xEventGroupCreate() 创建了事件组。
   EventGroupHandle_t xEventGroup;

   void anInterruptHandler( void )
   {
   BaseType_t xHigherPriorityTaskWoken, xResult;

		//xHigherPriorityTaskWoken 必须初始化为 pdFALSE。
		xHigherPriorityTaskWoken = pdFALSE;

		//设置 xEventGroup 中的位 0 和位 4。
		xResult = xEventGroupSetBitsFromISR(
							xEventGroup,	// The event group being updated.
							BIT_0 | BIT_4   // The bits being set.
							&xHigherPriorityTaskWoken );

		//消息发布成功了吗？
		if( xResult == pdPASS )
		{
			//如果 xHigherPriorityTaskWoken 现在设置为
			//pdTRUE，则应请求上下文切换。  使用的宏是特定于端口的，可以是
			//portYIELD_FROM_ISR() 或
			//portEND_SWITCHING_ISR() - 请参阅正在使用的端口的文档页面。
			portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
		}
  }
   </pre>
 * \defgroup xEventGroupSetBitsFromISR xEventGroupSetBitsFromISR
 * \ingroup EventGroup
 */
#if( configUSE_TRACE_FACILITY == 1 )
	BaseType_t xEventGroupSetBitsFromISR( EventGroupHandle_t xEventGroup, const EventBits_t uxBitsToSet, BaseType_t *pxHigherPriorityTaskWoken ) PRIVILEGED_FUNCTION;
#else
	#define xEventGroupSetBitsFromISR( xEventGroup, uxBitsToSet, pxHigherPriorityTaskWoken ) xTimerPendFunctionCallFromISR( vEventGroupSetBitsCallback, ( void * ) xEventGroup, ( uint32_t ) uxBitsToSet, pxHigherPriorityTaskWoken )
#endif

/**
 * event_groups.h
 *<pre>
	EventBits_t xEventGroupSync(	EventGroupHandle_t xEventGroup,
									const EventBits_t uxBitsToSet,
									const EventBits_t uxBitsToWaitFor,
									TickType_t xTicksToWait );
 </pre>
 *
 * 在事件组内原子设置位，然后等待在同
 * 一事件组内设置位组合。  此功能通
 * 常用于同步多个任务，其中每个任务必
 * 须等待其他任务到达同步点才能继续。
 *
 * 该函数不能在中断中使用。
 *
 * 如果 uxBitsToWait 参数指定的位已
 * 设置或在该时间内设置，则该函数将在其块时间到期
 * 之前返回。  在这种情况下，
 * uxBitsToWait 指定的所有位将在函数返回之前自动清除。
 *
 * @param xEventGroup 正在测试位的事件组。  的
 * 事件组之前必须已通过调用
 * xEventGroupCreate() 创建。
 *
 * @param uxBitsToSet 确定之前在事件组中设置的位
 * if, and possibly waiting for, all the bits specified by the uxBitsToWait
 * 参数已设置。
 *
 * @param uxBitsToWaitFor 指示要测试的一个或多个位的按位值
 * 活动组内。  例如，要等待位 0 和位 2，请将
 * uxBitsToWaitFor 设置为 0x05。  要等待位 0、
 * 位 1 和位 2，请将 uxBitsToWaitFor 设置为 0x07。  等等
 *
 * @param xTicksToWait time 的最大等待量（以“ticks”指定）
 * for all of the bits specified by uxBitsToWaitFor to become set.
 *
 * @return 正在等待的位时事件组的值
 * for became set, or the block time expired.  Test the return value to know
 * 设置了哪些位。  如果
 * xEventGroupSync() 由于超时而返
 * 回，则并非所有正在等待的位都将被设置。  如果
 * xEventGroupSync() 由于其等待的
 * 所有位均已设置而返回，则返回的值是自动清除任何位之前的事件组值。
 *
 * 用法示例：
 <pre>
 //三个任务使用的位。
 #define TASK_0_BIT		( 1 << 0 )
 #define TASK_1_BIT		( 1 << 1 )
 #define TASK_2_BIT		( 1 << 2 )

 #define ALL_SYNC_BITS ( TASK_0_BIT | TASK_1_BIT | TASK_2_BIT )

 //使用事件组来同步三个任务。
 //假设该事件组已在其他地方创建。
 EventGroupHandle_t xEventBits;

 void vTask0( void *pvParameters )
 {
 EventBits_t uxReturn;
 TickType_t xTicksToWait = 100 / portTICK_PERIOD_MS;

	 for( ;; )
	 {
		//在此执行任务功能。

		//在事件标志中设置位 0 以表明此任务已到达同步点。  另
		//外两个任务将设置 ALL_SYNC_BITS 定义的另外
		//两位。  当所有 ALL_SYNC_BITS 均已设置时
		//，所有三个任务均已达到同步点。  最多等待 100 毫秒
		//for this to happen.
		uxReturn = xEventGroupSync( xEventBits, TASK_0_BIT, ALL_SYNC_BITS, xTicksToWait );

		if( ( uxReturn & ALL_SYNC_BITS ) == ALL_SYNC_BITS )
		{
			//在调用 xEventGroupSync()
			//超时之前，所有三个任务均已达到同步点。
		}
	}
 }

 void vTask1( void *pvParameters )
 {
	 for( ;; )
	 {
		//在此执行任务功能。

		//设置事件标志中的位 1 以表明此任务已到达同步
		//点。  另外两个任务将设置
		//ALL_SYNC_BITS 定义的另外两位。
		//当所有 ALL_SYNC_BITS 均已设
		//置时，所有三个任务均已达到同步点。  无限期地等待这件事发生。
		xEventGroupSync( xEventBits, TASK_1_BIT, ALL_SYNC_BITS, portMAX_DELAY );

		//xEventGroupSync() 的调用具有
		//无限的阻塞时间，因此只有当所有三个任务都进行同
		//步时，该任务才会到达此处，因此无需测试返回值。
	 }
 }

 void vTask2( void *pvParameters )
 {
	 for( ;; )
	 {
		//在此执行任务功能。

		//设置事件标志中的位 2 以表明此任务已到达同步
		//点。  另外两个任务将设置
		//ALL_SYNC_BITS 定义的另外两位。
		//当所有 ALL_SYNC_BITS 均已设
		//置时，所有三个任务均已达到同步点。  无限期地等待这件事发生。
		xEventGroupSync( xEventBits, TASK_2_BIT, ALL_SYNC_BITS, portMAX_DELAY );

		//xEventGroupSync() 的调用具有
		//无限的阻塞时间，因此只有当所有三个任务都进行同
		//步时，该任务才会到达此处，因此无需测试返回值。
	}
 }

 </pre>
 * \defgroup xEventGroupSync xEventGroupSync
 * \ingroup EventGroup
 */
EventBits_t xEventGroupSync( EventGroupHandle_t xEventGroup, const EventBits_t uxBitsToSet, const EventBits_t uxBitsToWaitFor, TickType_t xTicksToWait ) PRIVILEGED_FUNCTION;


/**
 * event_groups.h
 *<pre>
	EventBits_t xEventGroupGetBits( EventGroupHandle_t xEventGroup );
 </pre>
 *
 * 返回事件组中位的当前值。
 * 该函数不能在中断中使用。
 *
 * @param xEventGroup 正在查询的事件组。
 *
 * @return 调用 xEventGroupGetBits() 时的事件组位。
 *
 * \defgroup xEventGroupGetBits xEventGroupGetBits
 * \ingroup EventGroup
 */
#define xEventGroupGetBits( xEventGroup ) xEventGroupClearBits( xEventGroup, 0 )

/**
 * event_groups.h
 *<pre>
	EventBits_t xEventGroupGetBitsFromISR( EventGroupHandle_t xEventGroup );
 </pre>
 *
 * 可以从 ISR 调用的 xEventGroupGetBits() 版本。
 *
 * @param xEventGroup 正在查询的事件组。
 *
 * @return 调用 xEventGroupGetBitsFromISR() 时的事件组位。
 *
 * \defgroup xEventGroupGetBitsFromISR xEventGroupGetBitsFromISR
 * \ingroup EventGroup
 */
EventBits_t xEventGroupGetBitsFromISR( EventGroupHandle_t xEventGroup ) PRIVILEGED_FUNCTION;

/**
 * event_groups.h
 *<pre>
	void xEventGroupDelete( EventGroupHandle_t xEventGroup );
 </pre>
 *
 * 删除先前通过调用
 * xEventGroupCreate() 创建的
 * 事件组。  在事件组上被阻止的任务将被解除阻止并获得 0 作为事件组的值。
 *
 * @param xEventGroup 正在删除的事件组。
 */
void vEventGroupDelete( EventGroupHandle_t xEventGroup ) PRIVILEGED_FUNCTION;

/* 仅供内部使用。 */
void vEventGroupSetBitsCallback( void *pvEventGroup, const uint32_t ulBitsToSet ) PRIVILEGED_FUNCTION;
void vEventGroupClearBitsCallback( void *pvEventGroup, const uint32_t ulBitsToClear ) PRIVILEGED_FUNCTION;


#if (configUSE_TRACE_FACILITY == 1)
	UBaseType_t uxEventGroupGetNumber( void* xEventGroup ) PRIVILEGED_FUNCTION;
	void vEventGroupSetNumber( void* xEventGroup, UBaseType_t uxEventGroupNumber ) PRIVILEGED_FUNCTION;
#endif

#ifdef __cplusplus
}
#endif

#endif /* EVENT_GROUPS_H */


