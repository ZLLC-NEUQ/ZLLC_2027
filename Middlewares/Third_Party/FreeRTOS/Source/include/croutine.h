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

#ifndef CO_ROUTINE_H
#define CO_ROUTINE_H

#ifndef INC_FREERTOS_H
	#error "include FreeRTOS.h must appear in source files before include croutine.h"
#endif

#include "list.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 用于隐藏 co-routine 控制块的实
现。  然而，由于 co-routine
功能的宏实现，控制块结构必须包含在标头中。 */
typedef void * CoRoutineHandle_t;

/* 定义 co-routine 函数必须符合的原型。 */
typedef void (*crCOROUTINE_CODE)( CoRoutineHandle_t, UBaseType_t );

typedef struct corCoRoutineControlBlock
{
	crCOROUTINE_CODE 	pxCoRoutineFunction;
	ListItem_t			xGenericListItem;	/*< 用于将 CRCB 放入就绪队列和阻塞队列的列表项。 */
	ListItem_t			xEventListItem;		/*< 用于将 CRCB 放入事件列表中的列表项。 */
	UBaseType_t 		uxPriority;			/*< co-routine 相对于其他 co-routines 的优先级。 */
	UBaseType_t 		uxIndex;			/*< 用于当多个co-routines使用相同的co-routine功能时区分co-routines。 */
	uint16_t 			uxState;			/*< 由 co-routine 实现内部使用。 */
} CRCB_t; /* 协同程序控制块。  注：uxPriority 和 TCB_t 的尺寸必须相同。 */

/**
 * 克丁。小时
 *<pre>
 BaseType_t xCoRoutineCreate(
                                 crCOROUTINE_CODE pxCoRoutineCode,
                                 UBaseType_t uxPriority,
                                 UBaseType_t uxIndex
                               );</pre>
 *
 * 创建一个新的 co-routine 并将其添加
 * 到准备运行的 co-routines 列表中。
 *
 * @param pxCoRoutineCode 指向 co-routine 函数的指针。  协程
 * 函数需要特殊语法 - 有关详细信息，请参阅
 * WEB 文档的 co-routine 部分。
 *
 * @param uxPriority 相对于其他 co-routines 的优先级
 * co-routine 将运行。
 *
 * @param uxIndex 用于区分不同的co-routines
 * 执行相同的功能。  有关更多信息，请参阅下面的示例
 * 和 WEB 文档的 co-routine 部分。
 *
 * @return pdPASS 如果 co-routine 已成功创建并添加到就绪状态
 * 列表，否则使用 ProjDefs.h 定义的错误代码。
 *
 * 用法示例：
   <pre>
 //要创建的协同例程。
 void vFlashCoRoutine( CoRoutineHandle_t xHandle, UBaseType_t uxIndex )
 {
 //如果 co-routines 中的变量必须在阻塞调用中保持值，则
 //必须将其声明为静态。对于 const 变量来说这可能不是必需的。
 static const char cLedToFlash[ 2 ] = { 5, 6 };
 static const TickType_t uxFlashRates[ 2 ] = { 200, 400 };

     //必须通过调用 crSTART() 来启动每个 co-routine；
     crSTART( xHandle );

     for( ;; )
     {
         //这个co-routine只是延迟固定的时间，然后
         //切换LED。  使用该函数创建了两个
         //co-routines，因此uxIndex参数用
         //于告诉co-routine哪个LED要闪烁以及
         //int32_t如何延迟。  这假设 xQueue 已经创建。
         vParTestToggleLED( cLedToFlash[ uxIndex ] );
         crDELAY( xHandle, uxFlashRates[ uxIndex ] );
     }

     //必须通过调用 crEND() 来结束每个 co-routine；
     crEND();
 }

 //创建两个 co-routines 的函数。
 void vOtherFunction( void )
 {
 uint8_t ucParameterToPass;
 TaskHandle_t xHandle;

     //创建两个优先级为 0 的 co-routines。第一个给出索引
     //0 so（来自上面的代码）每 200 个周期切换 LED 5。
     //第二个给定索引 1，因此每 400 个周期切换 LED 6。
     for( uxIndex = 0; uxIndex < 2; uxIndex++ )
     {
         xCoRoutineCreate( vFlashCoRoutine, 0, uxIndex );
     }
 }
   </pre>
 * \defgroup xCoRoutineCreate xCoRoutineCreate
 * \ingroup Tasks
 */
BaseType_t xCoRoutineCreate( crCOROUTINE_CODE pxCoRoutineCode, UBaseType_t uxPriority, UBaseType_t uxIndex );


/**
 * 克丁。小时
 *<pre>
 void vCoRoutineSchedule( void );</pre>
 *
 * 运行 co-routine。
 *
 * vCoRoutineSchedule()执行能够运行的最高优先
 * 级co-routine。  co-routine 将一直执行，
 * 直到它阻塞、让出或被任务抢占。  协同例程协同执行，因此一个
 * co-routine 不能被另一个抢占，但可以被任务抢占。
 *
 * 如果应用程序包含任务和
 * co-routines，则应从空闲 task
 * （在空闲任务挂钩中）调用 vCoRoutineSchedule。
 *
 * 用法示例：
   <pre>
 //这个空闲任务钩子每次被调用时都会调度一个co-routine
 //。其余的空闲任务将在 co-routine 调用之间执行。
 void vApplicationIdleHook( void )
 {
	vCoRoutineSchedule();
 }

 //或者，如果您不需要执行空闲任务的任何其他部
 //分，则空闲任务挂钩可以在无限循环内调用
 //vCoRoutineSchedule()。
 void vApplicationIdleHook( void )
 {
    for( ;; )
    {
        vCoRoutineSchedule();
    }
 }
 </pre>
 * \defgroup vCoRoutineSchedule vCoRoutineSchedule
 * \ingroup Tasks
 */
void vCoRoutineSchedule( void );

/**
 * 克丁。小时
 * <pre>
 crSTART( CoRoutineHandle_t xHandle );</pre>
 *
 * 该宏 MUST 始终在 co-routine 函数开始时调用。
 *
 * 用法示例：
   <pre>
 //要创建的协同例程。
 void vACoRoutine( CoRoutineHandle_t xHandle, UBaseType_t uxIndex )
 {
 //如果 co-routines 中的变量必须在阻塞调用中保持值，则必须将其声明为静态。
 static int32_t ulAVariable;

     //必须通过调用 crSTART() 来启动每个 co-routine；
     crSTART( xHandle );

     for( ;; )
     {
          //协同例程功能位于此处。
     }

     //必须通过调用 crEND() 来结束每个 co-routine；
     crEND();
 }</pre>
 * \defgroup crSTART crSTART
 * \ingroup Tasks
 */
#define crSTART( pxCRCB ) switch( ( ( CRCB_t * )( pxCRCB ) )->uxState ) { case 0:

/**
 * 克丁。小时
 * <pre>
 crEND();</pre>
 *
 * 该宏 MUST 始终在 co-routine 函数末尾调用。
 *
 * 用法示例：
   <pre>
 //要创建的协同例程。
 void vACoRoutine( CoRoutineHandle_t xHandle, UBaseType_t uxIndex )
 {
 //如果 co-routines 中的变量必须在阻塞调用中保持值，则必须将其声明为静态。
 static int32_t ulAVariable;

     //必须通过调用 crSTART() 来启动每个 co-routine；
     crSTART( xHandle );

     for( ;; )
     {
          //协同例程功能位于此处。
     }

     //必须通过调用 crEND() 来结束每个 co-routine；
     crEND();
 }</pre>
 * \defgroup crSTART crSTART
 * \ingroup Tasks
 */
#define crEND() }

/*
 * 这些宏仅供 co-routine 实现内部使
 * 用。  应用程序编写者不应直接使用这些宏。
 */
#define crSET_STATE0( xHandle ) ( ( CRCB_t * )( xHandle ) )->uxState = (__LINE__ * 2); return; case (__LINE__ * 2):
#define crSET_STATE1( xHandle ) ( ( CRCB_t * )( xHandle ) )->uxState = ((__LINE__ * 2)+1); return; case ((__LINE__ * 2)+1):

/**
 * 克丁。小时
 *<pre>
 crDELAY( CoRoutineHandle_t xHandle, TickType_t xTicksToDelay );</pre>
 *
 * 将 co-routine 延迟固定的时间。
 *
 * crDELAY 只能从 co-routine 函数本身调
 * 用，而不能从 co-routine 函数调用的函数内部调
 * 用。  这是因为co-routines不维护自己的堆栈。
 *
 * @param xHandle co-routine 延时的手柄。  这是 xHandle
 * co-routine 函数的参数。
 *
 * @param xTickToDelay co-routine 应延迟的滴答数
 * for.  The actual amount of time this equates to is defined by
 * configTICK_RATE_HZ（设置在FreeRTOSConfig.h
 * ）。  常量 portTICK_PERIOD_MS 可用于将刻度转换为毫秒。
 *
 * 用法示例：
   <pre>
 //要创建的协同例程。
 void vACoRoutine( CoRoutineHandle_t xHandle, UBaseType_t uxIndex )
 {
 //如果 co-routines 中的变量必须在阻塞调用
 //中保持值，则必须将其声明为静态。对于 const 变
 //量来说这可能不是必需的。我们要延迟 200 毫秒。
 static const xTickType xDelayTime = 200 / portTICK_PERIOD_MS;

     //必须通过调用 crSTART() 来启动每个 co-routine；
     crSTART( xHandle );

     for( ;; )
     {
        //延迟200ms。
        crDELAY( xHandle, xDelayTime );

        //在这里做点什么。
     }

     //必须通过调用 crEND() 来结束每个 co-routine；
     crEND();
 }</pre>
 * \defgroup crDELAY crDELAY
 * \ingroup Tasks
 */
#define crDELAY( xHandle, xTicksToDelay )												\
	if( ( xTicksToDelay ) > 0 )															\
	{																					\
		vCoRoutineAddToDelayedList( ( xTicksToDelay ), NULL );							\
	}																					\
	crSET_STATE0( ( xHandle ) );

/**
 * <pre>
 crQUEUE_SEND(
                  CoRoutineHandle_t xHandle,
                  QueueHandle_t pxQueue,
                  void *pvItemToQueue,
                  TickType_t xTicksToWait,
                  BaseType_t *pxResult
             )</pre>
 *
 * 该宏的crQUEUE_SEND()和crQUEUE_RECEIVE()是
 * co-routine，相当于任务使用的xQueueSend()和xQueueReceive()函数。
 *
 * crQUEUE_SEND 和 crQUEUE_RECEIVE 只能在 co-routine
 * 中使用，而 xQueueSend() 和 xQueueReceive() 只能在任务中使用。
 *
 * crQUEUE_SEND 只能从 co-routine 函数
 * 本身调用，而不能从 co-routine 函数调用的函数内部
 * 调用。  这是因为co-routines不维护自己的堆栈。
 *
 * 有关在任务和
 * co-routines 之间以及 ISR 和
 * co-routines.
 *
 * @param xHandle 调用 co-routine 的句柄。  这是 xHandle
 * co-routine 函数的参数。
 *
 * @param pxQueue 将在其上发布数据的队列的句柄。
 * 使用 xQueueCreate()
 * API 函数创建队列时，句柄作为返回值获得。
 *
 * @param pvItemToQueue 指向要发布到队列中的数据的指针。
 * 每个排队项目的字节数在创建队列时指定。  该字节
 * 数从 pvItemToQueue 复制到队列中
 * 本身。
 *
 * @param xTickToDelay co-routine 应阻止的刻度数
 * 如果空间不能立即可用，则等待队列上的空间变得可用。这相当于
 * 的实际时间量由 configTICK_RATE_HZ 定义
 * （在 FreeRTOSConfig.h 中设置）。  常量
 * portTICK_PERIOD_MS 可用于将价格变动转
 * 换为 milliseconds（请参见下面的示例）。
 *
 * @param pxResult 如果满足以下条件，则 pxResult 指向的变量将被设置为 pdPASS
 * 数据已成功发布到队列中，否则将被设置为
 * ProjDefs.h 中定义的错误。
 *
 * 用法示例：
   <pre>
 //协同例程函数会阻塞一段固定的时间
 //，然后将一个数字发布到队列中。
 static void prvCoRoutineFlashTask( CoRoutineHandle_t xHandle, UBaseType_t uxIndex )
 {
 //如果 co-routines 中的变量必须在阻塞调用中保持值，则必须将其声明为静态。
 static BaseType_t xNumberToPost = 0;
 static BaseType_t xResult;

    //协同例程必须以调用 crSTART() 开始。
    crSTART( xHandle );

    for( ;; )
    {
        //这假设队列已经创建。
        crQUEUE_SEND( xHandle, xCoRoutineQueue, &xNumberToPost, NO_DELAY, &xResult );

        if( xResult != pdPASS )
        {
            //该消息未发布！
        }

        //增加要发布到队列中的数量。
        xNumberToPost++;

        //延迟 100 个刻度。
        crDELAY( xHandle, 100 );
    }

    //协同例程必须以调用 crEND() 结束。
    crEND();
 }</pre>
 * \defgroup crQUEUE_SEND crQUEUE_SEND
 * \ingroup Tasks
 */
#define crQUEUE_SEND( xHandle, pxQueue, pvItemToQueue, xTicksToWait, pxResult )			\
{																						\
	*( pxResult ) = xQueueCRSend( ( pxQueue) , ( pvItemToQueue) , ( xTicksToWait ) );	\
	if( *( pxResult ) == errQUEUE_BLOCKED )												\
	{																					\
		crSET_STATE0( ( xHandle ) );													\
		*pxResult = xQueueCRSend( ( pxQueue ), ( pvItemToQueue ), 0 );					\
	}																					\
	if( *pxResult == errQUEUE_YIELD )													\
	{																					\
		crSET_STATE1( ( xHandle ) );													\
		*pxResult = pdPASS;																\
	}																					\
}

/**
 * 克丁。小时
 * <pre>
  crQUEUE_RECEIVE(
                     CoRoutineHandle_t xHandle,
                     QueueHandle_t pxQueue,
                     void *pvBuffer,
                     TickType_t xTicksToWait,
                     BaseType_t *pxResult
                 )</pre>
 *
 * 该宏的crQUEUE_SEND()和crQUEUE_RECEIVE()是
 * co-routine，相当于任务使用的xQueueSend()和xQueueReceive()函数。
 *
 * crQUEUE_SEND 和 crQUEUE_RECEIVE 只能在 co-routine
 * 中使用，而 xQueueSend() 和 xQueueReceive() 只能在任务中使用。
 *
 * crQUEUE_RECEIVE 只能从 co-routine
 * 函数本身调用，而不能从 co-routine 函数调用的函数内
 * 部调用。  这是因为co-routines不维护自己的堆栈。
 *
 * 有关在任务和
 * co-routines 之间以及 ISR 和
 * co-routines.
 *
 * @param xHandle 调用 co-routine 的句柄。  这是 xHandle
 * co-routine 函数的参数。
 *
 * @param pxQueue 从中接收数据的队列的句柄。
 * 使用 xQueueCreate()
 * API 函数创建队列时，句柄作为返回值获得。
 *
 * @param pvBuffer 接收到的项目将被复制到的缓冲区。
 * 每个排队项目的字节数在创建队列时指定。
 * 该字节数被复制到 pvBuffer 中。
 *
 * @param xTickToDelay co-routine 应阻止的刻度数
 * 如果数据不能立即可用，则等待队列中的数据可用。这相当于的实际
 * 时间量由 configTICK_RATE_HZ 定义（在
 * FreeRTOSConfig.h 中设置）。  常量
 * portTICK_PERIOD_MS 可用于将刻度转换为
 * milliseconds（请参阅 crQUEUE_SEND 示例）。
 *
 * @param pxResult 如果满足以下条件，则 pxResult 指向的变量将被设置为 pdPASS
 * 数据已成功从队列中检索，否则将被设置为
 * ProjDefs.h 中定义的错误代码。
 *
 * 用法示例：
 <pre>
 //co-routine 接收要从队列中闪存的
 //LED 的编号。  它会阻塞队列，直到收到号码。
 static void prvCoRoutineFlashWorkTask( CoRoutineHandle_t xHandle, UBaseType_t uxIndex )
 {
 //如果 co-routines 中的变量必须在阻塞调用中保持值，则必须将其声明为静态。
 static BaseType_t xResult;
 static UBaseType_t uxLEDToFlash;

    //所有 co-routines 都必须以调用 crSTART() 开始。
    crSTART( xHandle );

    for( ;; )
    {
        //等待队列中的数据可用。
        crQUEUE_RECEIVE( xHandle, xCoRoutineQueue, &uxLEDToFlash, portMAX_DELAY, &xResult );

        if( xResult == pdPASS )
        {
            //我们收到了LED来闪光——闪光吧！
            vParTestToggleLED( uxLEDToFlash );
        }
    }

    crEND();
 }</pre>
 * \defgroup crQUEUE_RECEIVE crQUEUE_RECEIVE
 * \ingroup Tasks
 */
#define crQUEUE_RECEIVE( xHandle, pxQueue, pvBuffer, xTicksToWait, pxResult )			\
{																						\
	*( pxResult ) = xQueueCRReceive( ( pxQueue) , ( pvBuffer ), ( xTicksToWait ) );		\
	if( *( pxResult ) == errQUEUE_BLOCKED ) 											\
	{																					\
		crSET_STATE0( ( xHandle ) );													\
		*( pxResult ) = xQueueCRReceive( ( pxQueue) , ( pvBuffer ), 0 );				\
	}																					\
	if( *( pxResult ) == errQUEUE_YIELD )												\
	{																					\
		crSET_STATE1( ( xHandle ) );													\
		*( pxResult ) = pdPASS;															\
	}																					\
}

/**
 * 克丁。小时
 * <pre>
  crQUEUE_SEND_FROM_ISR(
                            QueueHandle_t pxQueue,
                            void *pvItemToQueue,
                            BaseType_t xCoRoutinePreviouslyWoken
                       )</pre>
 *
 * 该宏的crQUEUE_SEND_FROM_ISR()和
 * crQUEUE_RECEIVE_FROM_ISR()是co-routine，相当
 * 于任务使用的xQueueSendFromISR()和xQueueReceiveFromISR()函数。
 *
 * crQUEUE_SEND_FROM_ISR() 和
 * crQUEUE_RECEIVE_FROM_ISR() 只能用于在 co-routine 和 ISR
 * 之间传递数据，而 xQueueSendFromISR() 和 xQueueReceiveFromISR() 只能用于在任务 和 之间传递数据
 * ISR.
 *
 * crQUEUE_SEND_FROM_ISR 只能从 ISR 调
 * 用，以将数据发送到 co-routine 内正在使用的队列。
 *
 * 有关在任务和
 * co-routines 之间以及 ISR 和
 * co-routines.
 *
 * @param xQueue 要在其上过帐项目的队列的句柄。
 *
 * @param pvItemToQueue 指向要放置在
 * 队列。  队列将保存的项目的大小是在创建队
 * 列时定义的，因此这么多字节将从
 * pvItemToQueue 复制到队列存储区域。
 *
 * @param xCoRoutinePreviouslyWoken 包含此内容，以便 ISR 可以发布到
 * 来自单个中断的多次同一个队列。  第
 * 一次调用应始终传入 pdFALSE。
 * 后续调用应传入上次调用返回的值。
 *
 * @return pdTRUE 如果 co-routine 通过发布到队列而被唤醒。  这是
 * 由 ISR 用于确定在
 * ISR 之后是否需要上下文切换。
 *
 * 用法示例：
 <pre>
 //co-routine 阻塞在队列中等待接收字符。
 static void vReceivingCoRoutine( CoRoutineHandle_t xHandle, UBaseType_t uxIndex )
 {
 char cRxedChar;
 BaseType_t xResult;

     //所有 co-routines 都必须以调用 crSTART() 开始。
     crSTART( xHandle );

     for( ;; )
     {
         //等待队列中的数据可用。  这假设队列
         //xCommsRxQueue 已经创建！
         crQUEUE_RECEIVE( xHandle, xCommsRxQueue, &uxLEDToFlash, portMAX_DELAY, &xResult );

         //收到角色了吗？
         if( xResult == pdPASS )
         {
             //在这里处理字符。
         }
     }

     //所有 co-routines 都必须以调用 crEND() 结束。
     crEND();
 }

 //ISR 使用队列将串行端口接收到的
 //字符发送到 co-routine。
 void vUART_ISR( void )
 {
 char cRxedChar;
 BaseType_t xCRWokenByPost = pdFALSE;

     //我们循环读取字符，直到 UART 中没有剩余字符为止。
     while( UART_RX_REG_NOT_EMPTY() )
     {
         //从UART中获取字符。
         cRxedChar = UART_RX_REG;

         //将角色发布到队列中。  xCRWokenByPost 第一次循
         //环时将是 pdFALSE。  如果该帖子导致
         //co-routine 变为 woken（未阻止），则
         //xCRWokenByPost 将被设置为 pdTRUE。通过这
         //种方式，我们可以确保如果队列中有多个 co-routine 被
         //阻塞，则无论向队列发送多少个字符，只有一个 ISR 能够被该 ISR 唤醒。
         xCRWokenByPost = crQUEUE_SEND_FROM_ISR( xCommsRxQueue, &cRxedChar, xCRWokenByPost );
     }
 }</pre>
 * \defgroup crQUEUE_SEND_FROM_ISR crQUEUE_SEND_FROM_ISR
 * \ingroup Tasks
 */
#define crQUEUE_SEND_FROM_ISR( pxQueue, pvItemToQueue, xCoRoutinePreviouslyWoken ) xQueueCRSendFromISR( ( pxQueue ), ( pvItemToQueue ), ( xCoRoutinePreviouslyWoken ) )


/**
 * 克丁。小时
 * <pre>
  crQUEUE_SEND_FROM_ISR(
                            QueueHandle_t pxQueue,
                            void *pvBuffer,
                            BaseType_t * pxCoRoutineWoken
                       )</pre>
 *
 * 该宏的crQUEUE_SEND_FROM_ISR()和
 * crQUEUE_RECEIVE_FROM_ISR()是co-routine，相当
 * 于任务使用的xQueueSendFromISR()和xQueueReceiveFromISR()函数。
 *
 * crQUEUE_SEND_FROM_ISR() 和
 * crQUEUE_RECEIVE_FROM_ISR() 只能用于在 co-routine 和 ISR
 * 之间传递数据，而 xQueueSendFromISR() 和 xQueueReceiveFromISR() 只能用于在任务 和 之间传递数据
 * ISR.
 *
 * crQUEUE_RECEIVE_FROM_ISR 只能从
 * ISR 调用，以从 co-routine 内正在使用的
 * 队列接收数据（发布到队列的 co-routine）。
 *
 * 有关在任务和
 * co-routines 之间以及 ISR 和
 * co-routines.
 *
 * @param xQueue 要在其上过帐项目的队列的句柄。
 *
 * @param pvBuffer 指向缓冲区的指针，接收到的项目将存入该缓冲区
 * 放置。  队列将保存的项目的大小是在创建队
 * 列时定义的，因此这么多字节将从队列复制到
 * pvBuffer.
 *
 * @param pxCoRoutineWoken A co-routine 可能会被阻塞，等待空间变为
 * 可以在队列中使用。  如果
 * crQUEUE_RECEIVE_FROM_ISR导致这样的
 * co-routine解锁*pxCoRoutineWoken将被设置为pdTRUE，否则*pxCoRoutineWoken将保持不变。
 *
 * @return pdTRUE 已成功从队列接收到项目，否则
 * pdFALSE.
 *
 * 用法示例：
 <pre>
 //co-routine 将一个字符发送到队列，然
 //后阻塞一段固定的时间。  该字符每次都会递增。
 static void vSendingCoRoutine( CoRoutineHandle_t xHandle, UBaseType_t uxIndex )
 {
 //当 co-routine 被阻塞时，
 //cChar 保持其值，因此必须声明为静态。
 static char cCharToTx = 'a';
 BaseType_t xResult;

     //所有 co-routines 都必须以调用 crSTART() 开始。
     crSTART( xHandle );

     for( ;; )
     {
         //将下一个字符发送到队列。
         crQUEUE_SEND( xHandle, xCoRoutineQueue, &cCharToTx, NO_DELAY, &xResult );

         if( xResult == pdPASS )
         {
             //该角色已成功发布到队列中。
         }
		 else
		 {
			//无法将角色发布到队列中。
		 }

         //启用 UART Tx 中断以在该
		 //假设的 UART 中引起中断。
		 //中断将从队列中获取字符并发送。
		 ENABLE_RX_INTERRUPT();

		 //递增到下一个字符，然后阻塞一段固定时
		 //间。 cCharToTx 将在整个延
		 //迟期间保持其值，因为它被声明为静态。
		 cCharToTx++;
		 if( cCharToTx > 'x' )
		 {
			cCharToTx = 'a';
		 }
		 crDELAY( 100 );
     }

     //所有 co-routines 都必须以调用 crEND() 结束。
     crEND();
 }

 //使用队列接收要在 UART 上发送的字符的 ISR。
 void vUART_ISR( void )
 {
 char cCharToTx;
 BaseType_t xCRWokenByPost = pdFALSE;

     while( UART_TX_REG_EMPTY() )
     {
         //队列中是否有字符等待发送？如果 co-routine
		 //被 post 唤醒，xCRWokenByPost
		 //将自动设置为 pdTRUE - 确保无论我们循环此循
		 //环多少次，都只会唤醒一个 co-routine。
         if( crQUEUE_RECEIVE_FROM_ISR( pxQueue, &cCharToTx, &xCRWokenByPost ) )
		 {
			 SEND_CHARACTER( cCharToTx );
		 }
     }
 }</pre>
 * \defgroup crQUEUE_RECEIVE_FROM_ISR crQUEUE_RECEIVE_FROM_ISR
 * \ingroup Tasks
 */
#define crQUEUE_RECEIVE_FROM_ISR( pxQueue, pvBuffer, pxCoRoutineWoken ) xQueueCRReceiveFromISR( ( pxQueue ), ( pvBuffer ), ( pxCoRoutineWoken ) )

/*
 * 该函数仅供 co-routine 宏内部使用
 * 。 co-routine 实现的宏观性质要求
 * 原型出现在此处。  该函数不应被应用程序使用
 * 作家。
 *
 * 从就绪列表中删除当前的
 * co-routine 并将其放入适当的延迟列表中。
 */
void vCoRoutineAddToDelayedList( TickType_t xTicksToDelay, List_t *pxEventList );

/*
 * 此函数仅供队列实现内部使用。应
 * 用程序编写者不应使用该函数。
 *
 * 从事件列表中删除最高优先级的
 * co-routine 并将其放入待处理就绪列表中。
 */
BaseType_t xCoRoutineRemoveFromEventList( const List_t *pxEventList );

#ifdef __cplusplus
}
#endif

#endif /* CO_ROUTINE_H */
