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


#ifndef QUEUE_H
#define QUEUE_H

#ifndef INC_FREERTOS_H
	#error "include FreeRTOS.h" must appear in source files before "include queue.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include "task.h"

/**
 * 引用队列的类型。  例如，调用 xQueueCreate() 返
 * 回一个 QueueHandle_t 变量，该变量随后可用作
 * xQueueSend()、xQueueReceive() 等的参数。
 */
struct QueueDefinition; /* 使用旧的命名约定，以免破坏内核感知调试器。 */
typedef struct QueueDefinition * QueueHandle_t;

/**
 * 引用队列集的类型。  例如，调用 xQueueCreateSet() 返
 * 回一个 xQueueSet 变量，该变量随后可用作
 * xQueueSelectFromSet()、xQueueAddToSet() 等的参数。
 */
typedef struct QueueDefinition * QueueSetHandle_t;

/**
 * 队列集可以同时包含队列和信号量，因此
 * QueueSetMemberHandle_t 被定义为在参数或
 * return value can be either an QueueHandle_t or an SemaphoreHandle_t.
 */
typedef struct QueueDefinition * QueueSetMemberHandle_t;

/* 仅供内部使用。 */
#define	queueSEND_TO_BACK		( ( BaseType_t ) 0 )
#define	queueSEND_TO_FRONT		( ( BaseType_t ) 1 )
#define queueOVERWRITE			( ( BaseType_t ) 2 )

/* 仅供内部使用。  这些定义*必须*与 queue.c 中的定义匹配。 */
#define queueQUEUE_TYPE_BASE				( ( uint8_t ) 0U )
#define queueQUEUE_TYPE_SET					( ( uint8_t ) 0U )
#define queueQUEUE_TYPE_MUTEX 				( ( uint8_t ) 1U )
#define queueQUEUE_TYPE_COUNTING_SEMAPHORE	( ( uint8_t ) 2U )
#define queueQUEUE_TYPE_BINARY_SEMAPHORE	( ( uint8_t ) 3U )
#define queueQUEUE_TYPE_RECURSIVE_MUTEX		( ( uint8_t ) 4U )

/**
 * 队列。小时
 * <pre>
 QueueHandle_t xQueueCreate(
							  UBaseType_t uxQueueLength,
							  UBaseType_t uxItemSize
						  );
 * </pre>
 *
 * 创建一个新的队列实例，并返回
 * 一个可以引用新队列的句柄。
 *
 * 在内部，在 FreeRTOS 实现中，队列使用两个内存
 * 块。  第一个块用于保存队列的数据结构。  第二个块用
 * 于保存放入队列中的项目。  如果使用
 * xQueueCreate() 创建队列，则两个内存块都
 * 会在 xQueueCreate() 函数内自动动态分配。  （见
 * http://www.freertos.org/a00111.html). 如果使用以下命令创建队列
 * xQueueCreateStatic() 那么应用程序编写者必
 * 须提供队列将使用的内存。  因此，
 * xQueueCreateStatic() 允许在不使用任何动态内存分配的情况下创建队列。
 *
 * http://www.FreeRTOS.org/Embedded-RTOS-Queues.html
 *
 * @param uxQueueLength 队列可以包含的最大项目数。
 *
 * @param uxItemSize 队列中每个项目所需的字节数。
 * 项目按复制排队，而不是按引用排队，因此
 * 这是将为每个已发布项目复制的字节数。
 * 队列中的每个项目必须具有相同的大小。
 *
 * @return 如果队列成功创建，则新队列的句柄
 * 返回创建的队列。  如果无法创建队列则为 0
 * 回来了。
 *
 * 用法示例：
   <pre>
 struct AMessage
 {
	char ucMessageID;
	char ucData[ 20 ];
 };

 void vATask( void *pvParameters )
 {
 QueueHandle_t xQueue1, xQueue2;

	//创建一个能够包含 10 个 uint32_t 值的队列。
	xQueue1 = xQueueCreate( 10, sizeof( uint32_t ) );
	if( xQueue1 == 0 )
	{
		//队列尚未创建且不得使用。
	}

	//创建一个能够包含 10 个指向 AMessage 结构的
	//指针的队列。这些应该通过指针传递，因为它们包含大量数据。
	xQueue2 = xQueueCreate( 10, sizeof( struct AMessage * ) );
	if( xQueue2 == 0 )
	{
		//队列尚未创建且不得使用。
	}

	//...任务代码的其余部分。
 }
 </pre>
 * \defgroup xQueueCreate xQueueCreate
 * \ingroup QueueManagement
 */
#if( configSUPPORT_DYNAMIC_ALLOCATION == 1 )
	#define xQueueCreate( uxQueueLength, uxItemSize ) xQueueGenericCreate( ( uxQueueLength ), ( uxItemSize ), ( queueQUEUE_TYPE_BASE ) )
#endif

/**
 * 队列。小时
 * <pre>
 QueueHandle_t xQueueCreateStatic(
							  UBaseType_t uxQueueLength,
							  UBaseType_t uxItemSize,
							  uint8_t *pucQueueStorageBuffer,
							  StaticQueue_t *pxQueueBuffer
						  );
 * </pre>
 *
 * 创建一个新的队列实例，并返回
 * 一个可以引用新队列的句柄。
 *
 * 在内部，在 FreeRTOS 实现中，队列使用两个内存
 * 块。  第一个块用于保存队列的数据结构。  第二个块用
 * 于保存放入队列中的项目。  如果使用
 * xQueueCreate() 创建队列，则两个内存块都
 * 会在 xQueueCreate() 函数内自动动态分配。  （见
 * http://www.freertos.org/a00111.html). 如果使用以下命令创建队列
 * xQueueCreateStatic() 那么应用程序编写者必
 * 须提供队列将使用的内存。  因此，
 * xQueueCreateStatic() 允许在不使用任何动态内存分配的情况下创建队列。
 *
 * http://www.FreeRTOS.org/Embedded-RTOS-Queues.html
 *
 * @param uxQueueLength 队列可以包含的最大项目数。
 *
 * @param uxItemSize 队列中每个项目所需的字节数。
 * 项目按复制排队，而不是按引用排队，因此
 * 这是将为每个已发布项目复制的字节数。
 * 队列中的每个项目必须具有相同的大小。
 *
 * @param pucQueueStorageBuffer 如果 uxItemSize 不为零，则
 * pucQueueStorageBuffer 必须指向一个 uint8_t 数组，该
 * 数组至少足够大以容纳任一时间可以在队列中的最大项目数 - 即 is (
 * uxQueueLength * uxItemsSize ) 字节。  如果
 * uxItemSize 为零，则 pucQueueStorageBuffer 可以是 NULL。
 *
 * @param pxQueueBuffer 必须指向 StaticQueue_t 类型的变量，其中
 * 将用于保存队列的数据结构。
 *
 * @return 如果创建了队列，则创建的队列的句柄是
 * 回来了。  如果 pxQueueBuffer 是 NULL，则返回 NULL。
 *
 * 用法示例：
   <pre>
 struct AMessage
 {
	char ucMessageID;
	char ucData[ 20 ];
 };

 #define QUEUE_LENGTH 10
 #define ITEM_SIZE sizeof( uint32_t )

 //xQueueBuffer 将保存队列结构。
 StaticQueue_t xQueueBuffer;

 //ucQueueStorage 将保存发布到队列的项目。  长
 //度必须至少为 [(队列长度) * (队列项目大小)] 字节。
 uint8_t ucQueueStorage[ QUEUE_LENGTH * ITEM_SIZE ];

 void vATask( void *pvParameters )
 {
 QueueHandle_t xQueue1;

	//创建一个能够包含 10 个 uint32_t 值的队列。
	xQueue1 = xQueueCreate( QUEUE_LENGTH, // The number of items the queue can hold.
							ITEM_SIZE	  // The size of each item in the queue
							&( ucQueueStorage[ 0 ] ), // The buffer that will hold the items in the queue.
							&xQueueBuffer ); // The buffer that will hold the queue structure.

	//由于不使用动态内存分配，因此保证队列创建成功。
	//因此，xQueue1 现在是有效队列的句柄。

	//...任务代码的其余部分。
 }
 </pre>
 * \defgroup xQueueCreateStatic xQueueCreateStatic
 * \ingroup QueueManagement
 */
#if( configSUPPORT_STATIC_ALLOCATION == 1 )
	#define xQueueCreateStatic( uxQueueLength, uxItemSize, pucQueueStorage, pxQueueBuffer ) xQueueGenericCreateStatic( ( uxQueueLength ), ( uxItemSize ), ( pucQueueStorage ), ( pxQueueBuffer ), ( queueQUEUE_TYPE_BASE ) )
#endif /* configSUPPORT_STATIC_ALLOCATION */

/**
 * 队列。小时
 * <pre>
 BaseType_t xQueueSendToToFront(
								   QueueHandle_t	xQueue,
								   const void		*pvItemToQueue,
								   TickType_t		xTicksToWait
							   );
 * </pre>
 *
 * 将项目发布到队列的前面。  该项目按副本排队，而
 * 不是按引用排队。  不得从中断服务程序中调用该函
 * 数。  请参阅
 * xQueueSendFromISR () 了解可在 ISR 中使用的替代方案。
 *
 * @param xQueue 要在其上过帐项目的队列的句柄。
 *
 * @param pvItemToQueue 指向要放置在
 * 队列。  队列将保存的项目的大小是在创建队
 * 列时定义的，因此这么多字节将从
 * pvItemToQueue 复制到队列存储区域。
 *
 * @param xTicksToWait 任务应阻塞的最长时间
 * 等待队列上有可用空间（如果队列已满）。  如果将此
 * 设置为 0 并且队列已满，则调用将立即返回。  时
 * 间以刻度周期定义，因此如果需要，应使用常量
 * portTICK_PERIOD_MS 转换为实时。
 *
 * 如果项目已成功过帐，则为 @return pdTRUE，否则为 errQUEUE_FULL。
 *
 * 用法示例：
   <pre>
 struct AMessage
 {
	char ucMessageID;
	char ucData[ 20 ];
 } xMessage;

 uint32_t ulVar = 10UL;

 void vATask( void *pvParameters )
 {
 QueueHandle_t xQueue1, xQueue2;
 struct AMessage *pxMessage;

	//创建一个能够包含 10 个 uint32_t 值的队列。
	xQueue1 = xQueueCreate( 10, sizeof( uint32_t ) );

	//创建一个能够包含 10 个指向 AMessage 结构的
	//指针的队列。这些应该通过指针传递，因为它们包含大量数据。
	xQueue2 = xQueueCreate( 10, sizeof( struct AMessage * ) );

	//...

	if( xQueue1 != 0 )
	{
		//发送 uint32_t。  如有必要，
		//请等待 10 个周期以获得可用空间。
		if( xQueueSendToFront( xQueue1, ( void * ) &ulVar, ( TickType_t ) 10 ) != pdPASS )
		{
			//即使在 10 个刻度之后，仍无法发布消息。
		}
	}

	if( xQueue2 != 0 )
	{
		//发送指向 struct AMessage
		//对象的指针。  如果队列已满，则不要阻塞。
		pxMessage = & xMessage;
		xQueueSendToFront( xQueue2, ( void * ) &pxMessage, ( TickType_t ) 0 );
	}

	//...任务代码的其余部分。
 }
 </pre>
 * \defgroup xQueueSend xQueueSend
 * \ingroup QueueManagement
 */
#define xQueueSendToFront( xQueue, pvItemToQueue, xTicksToWait ) xQueueGenericSend( ( xQueue ), ( pvItemToQueue ), ( xTicksToWait ), queueSEND_TO_FRONT )

/**
 * 队列。小时
 * <pre>
 BaseType_t xQueueSendToBack(
								   QueueHandle_t	xQueue,
								   const void		*pvItemToQueue,
								   TickType_t		xTicksToWait
							   );
 * </pre>
 *
 * 这是一个调用 xQueueGenericSend() 的宏。
 *
 * 将项目发布到队列的末尾。  该项目按副本排队，而
 * 不是按引用排队。  不得从中断服务程序中调用该函
 * 数。  请参阅
 * xQueueSendFromISR () 了解可在 ISR 中使用的替代方案。
 *
 * @param xQueue 要在其上过帐项目的队列的句柄。
 *
 * @param pvItemToQueue 指向要放置在
 * 队列。  队列将保存的项目的大小是在创建队
 * 列时定义的，因此这么多字节将从
 * pvItemToQueue 复制到队列存储区域。
 *
 * @param xTicksToWait 任务应阻塞的最长时间
 * 等待队列上有可用空间（如果队列已满）。  如果将此
 * 设置为 0 并且队列已满，则调用将立即返回。  时
 * 间以刻度周期定义，因此如果需要，应使用常量
 * portTICK_PERIOD_MS 转换为实时。
 *
 * 如果项目已成功过帐，则为 @return pdTRUE，否则为 errQUEUE_FULL。
 *
 * 用法示例：
   <pre>
 struct AMessage
 {
	char ucMessageID;
	char ucData[ 20 ];
 } xMessage;

 uint32_t ulVar = 10UL;

 void vATask( void *pvParameters )
 {
 QueueHandle_t xQueue1, xQueue2;
 struct AMessage *pxMessage;

	//创建一个能够包含 10 个 uint32_t 值的队列。
	xQueue1 = xQueueCreate( 10, sizeof( uint32_t ) );

	//创建一个能够包含 10 个指向 AMessage 结构的
	//指针的队列。这些应该通过指针传递，因为它们包含大量数据。
	xQueue2 = xQueueCreate( 10, sizeof( struct AMessage * ) );

	//...

	if( xQueue1 != 0 )
	{
		//发送 uint32_t。  如有必要，
		//请等待 10 个周期以获得可用空间。
		if( xQueueSendToBack( xQueue1, ( void * ) &ulVar, ( TickType_t ) 10 ) != pdPASS )
		{
			//即使在 10 个刻度之后，仍无法发布消息。
		}
	}

	if( xQueue2 != 0 )
	{
		//发送指向 struct AMessage
		//对象的指针。  如果队列已满，则不要阻塞。
		pxMessage = & xMessage;
		xQueueSendToBack( xQueue2, ( void * ) &pxMessage, ( TickType_t ) 0 );
	}

	//...任务代码的其余部分。
 }
 </pre>
 * \defgroup xQueueSend xQueueSend
 * \ingroup QueueManagement
 */
#define xQueueSendToBack( xQueue, pvItemToQueue, xTicksToWait ) xQueueGenericSend( ( xQueue ), ( pvItemToQueue ), ( xTicksToWait ), queueSEND_TO_BACK )

/**
 * 队列。小时
 * <pre>
 BaseType_t xQueueSend(
							  QueueHandle_t xQueue,
							  const void * pvItemToQueue,
							  TickType_t xTicksToWait
						 );
 * </pre>
 *
 * 这是一个调用 xQueueGenericSend() 的宏。  包
 * 含它是为了向后兼容不包含 xQueueSendToFront()
 * 和 xQueueSendToBack() 宏的
 * FreeRTOS.org 版本。  它相当于xQueueSendToBack()。
 *
 * 将项目发布到队列中。  该项目按副本排队，而不是按引用排队。
 * 不得从中断服务程序中调用该函数。请参阅
 * xQueueSendFromISR () 了解可在 ISR 中使用的替代方案。
 *
 * @param xQueue 要在其上过帐项目的队列的句柄。
 *
 * @param pvItemToQueue 指向要放置在
 * 队列。  队列将保存的项目的大小是在创建队
 * 列时定义的，因此这么多字节将从
 * pvItemToQueue 复制到队列存储区域。
 *
 * @param xTicksToWait 任务应阻塞的最长时间
 * 等待队列上有可用空间（如果队列已满）。  如果将此
 * 设置为 0 并且队列已满，则调用将立即返回。  时
 * 间以刻度周期定义，因此如果需要，应使用常量
 * portTICK_PERIOD_MS 转换为实时。
 *
 * 如果项目已成功过帐，则为 @return pdTRUE，否则为 errQUEUE_FULL。
 *
 * 用法示例：
   <pre>
 struct AMessage
 {
	char ucMessageID;
	char ucData[ 20 ];
 } xMessage;

 uint32_t ulVar = 10UL;

 void vATask( void *pvParameters )
 {
 QueueHandle_t xQueue1, xQueue2;
 struct AMessage *pxMessage;

	//创建一个能够包含 10 个 uint32_t 值的队列。
	xQueue1 = xQueueCreate( 10, sizeof( uint32_t ) );

	//创建一个能够包含 10 个指向 AMessage 结构的
	//指针的队列。这些应该通过指针传递，因为它们包含大量数据。
	xQueue2 = xQueueCreate( 10, sizeof( struct AMessage * ) );

	//...

	if( xQueue1 != 0 )
	{
		//发送 uint32_t。  如有必要，
		//请等待 10 个周期以获得可用空间。
		if( xQueueSend( xQueue1, ( void * ) &ulVar, ( TickType_t ) 10 ) != pdPASS )
		{
			//即使在 10 个刻度之后，仍无法发布消息。
		}
	}

	if( xQueue2 != 0 )
	{
		//发送指向 struct AMessage
		//对象的指针。  如果队列已满，则不要阻塞。
		pxMessage = & xMessage;
		xQueueSend( xQueue2, ( void * ) &pxMessage, ( TickType_t ) 0 );
	}

	//...任务代码的其余部分。
 }
 </pre>
 * \defgroup xQueueSend xQueueSend
 * \ingroup QueueManagement
 */
#define xQueueSend( xQueue, pvItemToQueue, xTicksToWait ) xQueueGenericSend( ( xQueue ), ( pvItemToQueue ), ( xTicksToWait ), queueSEND_TO_BACK )

/**
 * 队列。小时
 * <pre>
 BaseType_t xQueueOverwrite(
							  QueueHandle_t xQueue,
							  const void * pvItemToQueue
						 );
 * </pre>
 *
 * 仅适用于长度为 1 的队列 -
 * 因此队列要么为空，要么已满。
 *
 * 将项目发布到队列中。  如果队列已满，则覆盖队列中
 * 保存的值。  该项目按副本排队，而不是按引用排队。
 *
 * 不得从中断服务程序中调用该函数。请参阅
 * xQueueOverwriteFromISR () 了解可在 ISR 中使用的替代方案。
 *
 * @param xQueue 数据发送到的队列的句柄。
 *
 * @param pvItemToQueue 指向要放置在
 * 队列。  队列将保存的项目的大小是在创建队
 * 列时定义的，因此这么多字节将从
 * pvItemToQueue 复制到队列存储区域。
 *
 * @return xQueueOverwrite() 是调用 xQueueGenericSend() 的宏，并且
 * 因此具有与 xQueueSendToFront() 相同的返
 * 回值。  但是，pdPASS 是唯一可以返回的值，因为即使队
 * 列已满，xQueueOverwrite() 也会写入队列。
 *
 * 用法示例：
   <pre>

 void vFunction( void *pvParameters )
 {
 QueueHandle_t xQueue;
 uint32_t ulVarToSend, ulValReceived;

	//创建一个队列来保存一个 uint32_t 值。
	//强烈建议“不要”在可以包含多个值的队列上使用
	//xQueueOverwrite()，这样做会触发断言
	//if configASSERT() is defined.
	xQueue = xQueueCreate( 1, sizeof( uint32_t ) );

	//使用 xQueueOverwrite() 将值 10 写入队列。
	ulVarToSend = 10;
	xQueueOverwrite( xQueue, &ulVarToSend );

	//现在查看队列应该返回 10，但将值
	//10 保留在队列中。  使用零块
	//时间，因为已知队列保存有一个值。
	ulValReceived = 0;
	xQueuePeek( xQueue, &ulValReceived, 0 );

	if( ulValReceived != 10 )
	{
		//除非该项目被其他任务删除，否则会出错。
	}

	//队列仍然满了。  使用
	//xQueueOverwrite() 用 100 覆盖队列中保存的值。
	ulVarToSend = 100;
	xQueueOverwrite( xQueue, &ulVarToSend );

	//这次从队列中读取，使队列再次为
	//空。再次使用 0 的区块时间。
	xQueueReceive( xQueue, &ulValReceived, 0 );

	//读取的值应该是最后写入的值
	//，即使写入值时队列已满。
	if( ulValReceived != 100 )
	{
		//错误！
	}

	//...
}
 </pre>
 * \defgroup xQueueOverwrite xQueueOverwrite
 * \ingroup QueueManagement
 */
#define xQueueOverwrite( xQueue, pvItemToQueue ) xQueueGenericSend( ( xQueue ), ( pvItemToQueue ), 0, queueOVERWRITE )


/**
 * 队列。小时
 * <pre>
 BaseType_t xQueueGenericSend(
									QueueHandle_t xQueue,
									const void * pvItemToQueue,
									TickType_t xTicksToWait
									BaseType_t xCopyPosition
								);
 * </pre>
 *
 * 最好使用宏 xQueueSend()、
 * xQueueSendToFront() 和 xQueueSendToBack() 来代替直接调用该函数。
 *
 * 将项目发布到队列中。  该项目按副本排队，而不是按引用排队。
 * 不得从中断服务程序中调用该函数。请参阅
 * xQueueSendFromISR () 了解可在 ISR 中使用的替代方案。
 *
 * @param xQueue 要在其上过帐项目的队列的句柄。
 *
 * @param pvItemToQueue 指向要放置在
 * 队列。  队列将保存的项目的大小是在创建队
 * 列时定义的，因此这么多字节将从
 * pvItemToQueue 复制到队列存储区域。
 *
 * @param xTicksToWait 任务应阻塞的最长时间
 * 等待队列上有可用空间（如果队列已满）。  如果将此
 * 设置为 0 并且队列已满，则调用将立即返回。  时
 * 间以刻度周期定义，因此如果需要，应使用常量
 * portTICK_PERIOD_MS 转换为实时。
 *
 * @param xCopyPosition 可以取值queueSEND_TO_BACK来放置
 * 项目放在队列的后面，或者
 * queueSEND_TO_FRONT 将项目放在 queue 的前面（对于高优先级消息）。
 *
 * 如果项目已成功过帐，则为 @return pdTRUE，否则为 errQUEUE_FULL。
 *
 * 用法示例：
   <pre>
 struct AMessage
 {
	char ucMessageID;
	char ucData[ 20 ];
 } xMessage;

 uint32_t ulVar = 10UL;

 void vATask( void *pvParameters )
 {
 QueueHandle_t xQueue1, xQueue2;
 struct AMessage *pxMessage;

	//创建一个能够包含 10 个 uint32_t 值的队列。
	xQueue1 = xQueueCreate( 10, sizeof( uint32_t ) );

	//创建一个能够包含 10 个指向 AMessage 结构的
	//指针的队列。这些应该通过指针传递，因为它们包含大量数据。
	xQueue2 = xQueueCreate( 10, sizeof( struct AMessage * ) );

	//...

	if( xQueue1 != 0 )
	{
		//发送 uint32_t。  如有必要，
		//请等待 10 个周期以获得可用空间。
		if( xQueueGenericSend( xQueue1, ( void * ) &ulVar, ( TickType_t ) 10, queueSEND_TO_BACK ) != pdPASS )
		{
			//即使在 10 个刻度之后，仍无法发布消息。
		}
	}

	if( xQueue2 != 0 )
	{
		//发送指向 struct AMessage
		//对象的指针。  如果队列已满，则不要阻塞。
		pxMessage = & xMessage;
		xQueueGenericSend( xQueue2, ( void * ) &pxMessage, ( TickType_t ) 0, queueSEND_TO_BACK );
	}

	//...任务代码的其余部分。
 }
 </pre>
 * \defgroup xQueueSend xQueueSend
 * \ingroup QueueManagement
 */
BaseType_t xQueueGenericSend( QueueHandle_t xQueue, const void * const pvItemToQueue, TickType_t xTicksToWait, const BaseType_t xCopyPosition ) PRIVILEGED_FUNCTION;

/**
 * 队列。小时
 * <pre>
 BaseType_t xQueuePeek(
							 QueueHandle_t xQueue,
							 void * const pvBuffer,
							 TickType_t xTicksToWait
						 );</pre>
 *
 * 从队列接收项目而不从队列中删除该项目
 * 。该项目是通过副本接收的，因此必须提
 * 供足够大小的缓冲区。  复制到缓冲区
 * 的字节数是在创建队列时定义的。
 *
 * 成功接收的项目保留在队列中，因此将在下一次调用或调
 * 用 xQueueReceive() 时再次返回。
 *
 * 该宏不得在中断服务程序中使用。  请参阅
 * xQueuePeekFromISR()
 * 了解可以从中断服务例程调用的替代方法。
 *
 * @param xQueue 要从中获取项目的队列的句柄
 * 已收到。
 *
 * @param pvBuffer 指向接收到的项目将放入的缓冲区的指针
 * 被复制。
 *
 * @param xTicksToWait 任务应阻塞的最长时间
 * 如果在调用时队列为空，则等待接收项目。	 时间
 * 以刻度周期定义，因此如果需要，应使用常量
 * portTICK_PERIOD_MS 转换为实
 * 时。如果 xTicksToWait 为 0 并
 * 且队列为空，xQueuePeek() 将立即返回。
 *
 * @return pdTRUE 如果从队列成功接收到项目，
 * 否则为 pdFALSE。
 *
 * 用法示例：
   <pre>
 struct AMessage
 {
	char ucMessageID;
	char ucData[ 20 ];
 } xMessage;

 QueueHandle_t xQueue;

 //创建队列并发布值的任务。
 void vATask( void *pvParameters )
 {
 struct AMessage *pxMessage;

	//创建一个能够包含 10 个指向 AMessage 结构的
	//指针的队列。这些应该通过指针传递，因为它们包含大量数据。
	xQueue = xQueueCreate( 10, sizeof( struct AMessage * ) );
	if( xQueue == 0 )
	{
		//创建队列失败。
	}

	//...

	//发送指向 struct AMessage
	//对象的指针。  如果队列已满，则不要阻塞。
	pxMessage = & xMessage;
	xQueueSend( xQueue, ( void * ) &pxMessage, ( TickType_t ) 0 );

	//...任务代码的其余部分。
 }

 //从队列中查看数据的任务。
 void vADifferentTask( void *pvParameters )
 {
 struct AMessage *pxRxedMessage;

	if( xQueue != 0 )
	{
		//查看创建的队列上的消息。  如果消息
		//不能立即可用，则阻塞 10 个周期。
		if( xQueuePeek( xQueue, &( pxRxedMessage ), ( TickType_t ) 10 ) )
		{
			//pcRxedMessage 现在指向 vATask 发布的
			//struct AMessage 变量，但该项目仍然保留在队列中。
		}
	}

	//...任务代码的其余部分。
 }
 </pre>
 * \defgroup xQueuePeek xQueuePeek
 * \ingroup QueueManagement
 */
BaseType_t xQueuePeek( QueueHandle_t xQueue, void * const pvBuffer, TickType_t xTicksToWait ) PRIVILEGED_FUNCTION;

/**
 * 队列。小时
 * <pre>
 BaseType_t xQueuePeekFromISR(
									QueueHandle_t xQueue,
									void *pvBuffer,
								);</pre>
 *
 * 可以从中断服务 routine (ISR)
 * 调用的 xQueuePeek() 版本。
 *
 * 从队列接收项目而不从队列中删除该项目
 * 。该项目是通过副本接收的，因此必须提
 * 供足够大小的缓冲区。  复制到缓冲区
 * 的字节数是在创建队列时定义的。
 *
 * 成功接收的项目保留在队列中，因此将在下一次调用或调
 * 用 xQueueReceive() 时再次返回。
 *
 * @param xQueue 要从中获取项目的队列的句柄
 * 已收到。
 *
 * @param pvBuffer 指向接收到的项目将放入的缓冲区的指针
 * 被复制。
 *
 * @return pdTRUE 如果从队列成功接收到项目，
 * 否则为 pdFALSE。
 *
 * \defgroup xQueuePeekFromISR xQueuePeekFromISR
 * \ingroup QueueManagement
 */
BaseType_t xQueuePeekFromISR( QueueHandle_t xQueue, void * const pvBuffer ) PRIVILEGED_FUNCTION;

/**
 * 队列。小时
 * <pre>
 BaseType_t xQueueReceive(
								 QueueHandle_t xQueue,
								 void *pvBuffer,
								 TickType_t xTicksToWait
							);</pre>
 *
 * 从队列接收一个项目。  该项目是通过副本接
 * 收的，因此必须提供足够大小的缓冲区。  复
 * 制到缓冲区的字节数是在创建队列时定义的。
 *
 * 成功接收的项目将从队列中删除。
 *
 * 该函数不得在中断服务程序中使用。  请参阅
 * xQueueReceiveFromISR 了解可以的替代方案。
 *
 * @param xQueue 要从中获取项目的队列的句柄
 * 已收到。
 *
 * @param pvBuffer 指向接收到的项目将放入的缓冲区的指针
 * 被复制。
 *
 * @param xTicksToWait 任务应阻塞的最长时间
 * 如果在调用时队列为空，则等待接收项目。	 如果
 * xTicksToWait 为零并且队列为空，
 * xQueueReceive() 将立即返回。  时间以刻度周
 * 期定义，因此如果是，则应使用常量 portTICK_PERIOD_MS 转换为实时
 * 必需的。
 *
 * @return pdTRUE 如果从队列成功接收到项目，
 * 否则为 pdFALSE。
 *
 * 用法示例：
   <pre>
 struct AMessage
 {
	char ucMessageID;
	char ucData[ 20 ];
 } xMessage;

 QueueHandle_t xQueue;

 //创建队列并发布值的任务。
 void vATask( void *pvParameters )
 {
 struct AMessage *pxMessage;

	//创建一个能够包含 10 个指向 AMessage 结构的
	//指针的队列。这些应该通过指针传递，因为它们包含大量数据。
	xQueue = xQueueCreate( 10, sizeof( struct AMessage * ) );
	if( xQueue == 0 )
	{
		//创建队列失败。
	}

	//...

	//发送指向 struct AMessage
	//对象的指针。  如果队列已满，则不要阻塞。
	pxMessage = & xMessage;
	xQueueSend( xQueue, ( void * ) &pxMessage, ( TickType_t ) 0 );

	//...任务代码的其余部分。
 }

 //从队列接收的任务。
 void vADifferentTask( void *pvParameters )
 {
 struct AMessage *pxRxedMessage;

	if( xQueue != 0 )
	{
		//在创建的队列上接收消息。  如果消息
		//不能立即可用，则阻塞 10 个周期。
		if( xQueueReceive( xQueue, &( pxRxedMessage ), ( TickType_t ) 10 ) )
		{
			//pcRxedMessage 现在指向 vATask
			//发布的 struct AMessage 变量。
		}
	}

	//...任务代码的其余部分。
 }
 </pre>
 * \defgroup xQueueReceive xQueueReceive
 * \ingroup QueueManagement
 */
BaseType_t xQueueReceive( QueueHandle_t xQueue, void * const pvBuffer, TickType_t xTicksToWait ) PRIVILEGED_FUNCTION;

/**
 * 队列。小时
 * <pre>UBaseType_t uxQueueMessagesWaiting( const QueueHandle_t xQueue );</pre>
 *
 * 返回队列中存储的消息数。
 *
 * @param xQueue 正在查询的队列的句柄。
 *
 * @return 队列中可用的消息数。
 *
 * \defgroup uxQueueMessagesWaiting uxQueueMessagesWaiting
 * \ingroup QueueManagement
 */
UBaseType_t uxQueueMessagesWaiting( const QueueHandle_t xQueue ) PRIVILEGED_FUNCTION;

/**
 * 队列。小时
 * <pre>UBaseType_t uxQueueSpacesAvailable( const QueueHandle_t xQueue );</pre>
 *
 * 返回队列中可用空间的数量。  这等于
 * 队列变满之前可以发送到队列的项目数
 * if no items are removed.
 *
 * @param xQueue 正在查询的队列的句柄。
 *
 * @return 队列中可用的空间数。
 *
 * \defgroup uxQueueMessagesWaiting uxQueueMessagesWaiting
 * \ingroup QueueManagement
 */
UBaseType_t uxQueueSpacesAvailable( const QueueHandle_t xQueue ) PRIVILEGED_FUNCTION;

/**
 * 队列。小时
 * <pre>void vQueueDelete( QueueHandle_t xQueue );</pre>
 *
 * 删除队列 - 释放分配用于存储
 * 放置在队列上的项目的所有内存。
 *
 * @param xQueue 要删除的队列的句柄。
 *
 * \defgroup vQueueDelete vQueueDelete
 * \ingroup QueueManagement
 */
void vQueueDelete( QueueHandle_t xQueue ) PRIVILEGED_FUNCTION;

/**
 * 队列。小时
 * <pre>
 BaseType_t xQueueSendToFrontFromISR(
										 QueueHandle_t xQueue,
										 const void *pvItemToQueue,
										 BaseType_t *pxHigherPriorityTaskWoken
									  );
 </pre>
 *
 * 这是一个调用 xQueueGenericSendFromISR() 的宏。
 *
 * 将项目发布到队列的前面。  在中
 * 断服务例程中使用该宏是安全的。
 *
 * 项目通过复制而不是引用进行排队，因此最好仅对小项
 * 目进行排队，尤其是从 ISR 调用时。  在大多
 * 数情况下，最好存储指向正在排队的项目的指针。
 *
 * @param xQueue 要在其上过帐项目的队列的句柄。
 *
 * @param pvItemToQueue 指向要放置在
 * 队列。  队列将保存的项目的大小是在创建队
 * 列时定义的，因此这么多字节将从
 * pvItemToQueue 复制到队列存储区域。
 *
 * @param pxHigherPriorityTaskWoken xQueueSendToFrontFromISR() 将设置
 * *pxHigherPriorityTaskWoken 到 pdTRUE
 * 如果发送到队列导致任务解除阻塞，并且解除阻塞的任务的优先级高于当前正
 * 在运行的任务。  如果
 * xQueueSendToFromFromISR() 将此值设置为 pdTRUE，则应在退出中断之前请求上下文切换。
 *
 * @return 如果数据成功发送到队列则为pdTRUE，否则
 * errQUEUE_FULL.
 *
 * 缓冲 IO 的用法示例（其中
 * ISR 每次调用可以获得多个值）：
   <pre>
 void vBufferISR( void )
 {
 char cIn;
 BaseType_t xHigherPrioritTaskWoken;

	//我们还没有在ISR启动时唤醒任务。
	xHigherPriorityTaskWoken = pdFALSE;

	//循环直到缓冲区为空。
	do
	{
		//从缓冲区获取一个字节。
		cIn = portINPUT_BYTE( RX_REGISTER_ADDRESS );

		//发布字节。
		xQueueSendToFrontFromISR( xRxQueue, &cIn, &xHigherPriorityTaskWoken );

	} while( portINPUT_BYTE( BUFFER_COUNT ) );

	//现在缓冲区是空的，我们可以根据需要切换上下文。
	if( xHigherPriorityTaskWoken )
	{
		taskYIELD ();
	}
 }
 </pre>
 *
 * \defgroup xQueueSendFromISR xQueueSendFromISR
 * \ingroup QueueManagement
 */
#define xQueueSendToFrontFromISR( xQueue, pvItemToQueue, pxHigherPriorityTaskWoken ) xQueueGenericSendFromISR( ( xQueue ), ( pvItemToQueue ), ( pxHigherPriorityTaskWoken ), queueSEND_TO_FRONT )


/**
 * 队列。小时
 * <pre>
 BaseType_t xQueueSendToBackFromISR(
										 QueueHandle_t xQueue,
										 const void *pvItemToQueue,
										 BaseType_t *pxHigherPriorityTaskWoken
									  );
 </pre>
 *
 * 这是一个调用 xQueueGenericSendFromISR() 的宏。
 *
 * 将项目发布到队列的末尾。  在中
 * 断服务例程中使用该宏是安全的。
 *
 * 项目通过复制而不是引用进行排队，因此最好仅对小项
 * 目进行排队，尤其是从 ISR 调用时。  在大多
 * 数情况下，最好存储指向正在排队的项目的指针。
 *
 * @param xQueue 要在其上过帐项目的队列的句柄。
 *
 * @param pvItemToQueue 指向要放置在
 * 队列。  队列将保存的项目的大小是在创建队
 * 列时定义的，因此这么多字节将从
 * pvItemToQueue 复制到队列存储区域。
 *
 * @param pxHigherPriorityTaskWoken xQueueSendToBackFromISR() 将设置
 * *pxHigherPriorityTaskWoken 到 pdTRUE
 * 如果发送到队列导致任务解除阻塞，并且解除阻塞的任务的优先级高于当前正
 * 在运行的任务。  如果
 * xQueueSendToBackFromISR() 将此值设置为 pdTRUE，则应在退出中断之前请求上下文切换。
 *
 * @return 如果数据成功发送到队列则为pdTRUE，否则
 * errQUEUE_FULL.
 *
 * 缓冲 IO 的用法示例（其中
 * ISR 每次调用可以获得多个值）：
   <pre>
 void vBufferISR( void )
 {
 char cIn;
 BaseType_t xHigherPriorityTaskWoken;

	//我们还没有在ISR启动时唤醒任务。
	xHigherPriorityTaskWoken = pdFALSE;

	//循环直到缓冲区为空。
	do
	{
		//从缓冲区获取一个字节。
		cIn = portINPUT_BYTE( RX_REGISTER_ADDRESS );

		//发布字节。
		xQueueSendToBackFromISR( xRxQueue, &cIn, &xHigherPriorityTaskWoken );

	} while( portINPUT_BYTE( BUFFER_COUNT ) );

	//现在缓冲区是空的，我们可以根据需要切换上下文。
	if( xHigherPriorityTaskWoken )
	{
		taskYIELD ();
	}
 }
 </pre>
 *
 * \defgroup xQueueSendFromISR xQueueSendFromISR
 * \ingroup QueueManagement
 */
#define xQueueSendToBackFromISR( xQueue, pvItemToQueue, pxHigherPriorityTaskWoken ) xQueueGenericSendFromISR( ( xQueue ), ( pvItemToQueue ), ( pxHigherPriorityTaskWoken ), queueSEND_TO_BACK )

/**
 * 队列。小时
 * <pre>
 BaseType_t xQueueOverwriteFromISR(
							  QueueHandle_t xQueue,
							  const void * pvItemToQueue,
							  BaseType_t *pxHigherPriorityTaskWoken
						 );
 * </pre>
 *
 * xQueueOverwrite() 的一个版本
 * ，可用于中断服务 routine (ISR)。
 *
 * 仅适用于可容纳单个项目的队列 -
 * 因此队列要么为空，要么已满。
 *
 * 将项目发布到队列中。  如果队列已满，则覆盖队列中
 * 保存的值。  该项目按副本排队，而不是按引用排队。
 *
 * @param xQueue 要在其上过帐项目的队列的句柄。
 *
 * @param pvItemToQueue 指向要放置在
 * 队列。  队列将保存的项目的大小是在创建队
 * 列时定义的，因此这么多字节将从
 * pvItemToQueue 复制到队列存储区域。
 *
 * @param pxHigherPriorityTaskWoken xQueueOverwriteFromISR() 将设置
 * *pxHigherPriorityTaskWoken 到 pdTRUE
 * 如果发送到队列导致任务解除阻塞，并且解除阻塞的任务的优先级高于当前正
 * 在运行的任务。  如果
 * xQueueOverwriteFromISR() 将此值设置为 pdTRUE，则应在退出中断之前请求上下文切换。
 *
 * @return xQueueOverwriteFromISR() 是一个宏，调用
 * xQueueGenericSendFromISR()，因此具有与
 * xQueueSendToFrontFromISR() 相同的返回值
 * 。  但是，pdPASS 是唯一可以返回的值，因为即使队列已满，
 * xQueueOverwriteFromISR() 也会写入队列。
 *
 * 用法示例：
   <pre>

 QueueHandle_t xQueue;

 void vFunction( void *pvParameters )
 {
 	//创建一个队列来保存一个 uint32_t 值。  强烈
	//建议“不要”在可以包含多个值的队列上使用
	//xQueueOverwriteFromISR()，这样做会触发断言
	//if configASSERT() is defined.
	xQueue = xQueueCreate( 1, sizeof( uint32_t ) );
}

void vAnInterruptHandler( void )
{
//使用前必须将xHigherPriorityTaskWoken设置为pdFALSE。
BaseType_t xHigherPriorityTaskWoken = pdFALSE;
uint32_t ulVarToSend, ulValReceived;

	//使用 xQueueOverwriteFromISR() 将值 10 写入队列。
	ulVarToSend = 10;
	xQueueOverwriteFromISR( xQueue, &ulVarToSend, &xHigherPriorityTaskWoken );

	//队列已满，但再次调用
	//xQueueOverwriteFromISR()
	//仍会通过，因为队列中保存的值将被新值覆盖。
	ulVarToSend = 100;
	xQueueOverwriteFromISR( xQueue, &ulVarToSend, &xHigherPriorityTaskWoken );

	//从队列中读取现在将返回 100。

	//...

	if( xHigherPrioritytaskWoken == pdTRUE )
	{
		//写入队列导致任务解除阻塞，并且解除阻塞的任务
		//的优先级高于或等于当前正在执行的 task（
		//此中断中断的任务）的优先级。  执行上下文切
		//换，以便该中断直接返回到未阻塞的任务。
		portYIELD_FROM_ISR(); // or portEND_SWITCHING_ISR() depending on the port.
	}
}
 </pre>
 * \defgroup xQueueOverwriteFromISR xQueueOverwriteFromISR
 * \ingroup QueueManagement
 */
#define xQueueOverwriteFromISR( xQueue, pvItemToQueue, pxHigherPriorityTaskWoken ) xQueueGenericSendFromISR( ( xQueue ), ( pvItemToQueue ), ( pxHigherPriorityTaskWoken ), queueOVERWRITE )

/**
 * 队列。小时
 * <pre>
 BaseType_t xQueueSendFromISR(
									 QueueHandle_t xQueue,
									 const void *pvItemToQueue,
									 BaseType_t *pxHigherPriorityTaskWoken
								);
 </pre>
 *
 * 这是一个调用 xQueueGenericSendFromISR() 的宏。  包含在内
 * for backward compatibility with versions of FreeRTOS.org that did not
 * 包括 xQueueSendToBackFromISR() 和 xQueueSendToFrontFromISR()
 * 宏。
 *
 * 将项目发布到队列的末尾。  在中
 * 断服务程序中使用该函数是安全的。
 *
 * 项目通过复制而不是引用进行排队，因此最好仅对小项
 * 目进行排队，尤其是从 ISR 调用时。  在大多
 * 数情况下，最好存储指向正在排队的项目的指针。
 *
 * @param xQueue 要在其上过帐项目的队列的句柄。
 *
 * @param pvItemToQueue 指向要放置在
 * 队列。  队列将保存的项目的大小是在创建队
 * 列时定义的，因此这么多字节将从
 * pvItemToQueue 复制到队列存储区域。
 *
 * @param pxHigherPriorityTaskWoken xQueueSendFromISR() 将设置
 * *pxHigherPriorityTaskWoken 到
 * pdTRUE 如果发送到队列导致任务解除阻塞，并且解除阻塞的任务的优
 * 先级高于当前正在运行的任务。  如果
 * xQueueSendFromISR() 将此值设置为 pdTRUE，则应在退出中断之前请求上下文切换。
 *
 * @return 如果数据成功发送到队列则为pdTRUE，否则
 * errQUEUE_FULL.
 *
 * 缓冲 IO 的用法示例（其中
 * ISR 每次调用可以获得多个值）：
   <pre>
 void vBufferISR( void )
 {
 char cIn;
 BaseType_t xHigherPriorityTaskWoken;

	//我们还没有在ISR启动时唤醒任务。
	xHigherPriorityTaskWoken = pdFALSE;

	//循环直到缓冲区为空。
	do
	{
		//从缓冲区获取一个字节。
		cIn = portINPUT_BYTE( RX_REGISTER_ADDRESS );

		//发布字节。
		xQueueSendFromISR( xRxQueue, &cIn, &xHigherPriorityTaskWoken );

	} while( portINPUT_BYTE( BUFFER_COUNT ) );

	//现在缓冲区是空的，我们可以根据需要切换上下文。
	if( xHigherPriorityTaskWoken )
	{
		//这里使用的实际宏是特定于端口的。
		portYIELD_FROM_ISR ();
	}
 }
 </pre>
 *
 * \defgroup xQueueSendFromISR xQueueSendFromISR
 * \ingroup QueueManagement
 */
#define xQueueSendFromISR( xQueue, pvItemToQueue, pxHigherPriorityTaskWoken ) xQueueGenericSendFromISR( ( xQueue ), ( pvItemToQueue ), ( pxHigherPriorityTaskWoken ), queueSEND_TO_BACK )

/**
 * 队列。小时
 * <pre>
 BaseType_t xQueueGenericSendFromISR(
										   QueueHandle_t		xQueue,
										   const	void	*pvItemToQueue,
										   BaseType_t	*pxHigherPriorityTaskWoken,
										   BaseType_t	xCopyPosition
									   );
 </pre>
 *
 * 最好使用宏 xQueueSendFromISR()、
 * xQueueSendToFrontFromISR() 和
 * xQueueSendToBackFromISR() 来代替直接调用该函
 * 数。  xQueueGiveFromISR() 是由实际上不复制任何数据的信号量使用的等效函数。
 *
 * 将项目发布到队列中。  在中断
 * 服务程序中使用该函数是安全的。
 *
 * 项目通过复制而不是引用进行排队，因此最好仅对小项
 * 目进行排队，尤其是从 ISR 调用时。  在大多
 * 数情况下，最好存储指向正在排队的项目的指针。
 *
 * @param xQueue 要在其上过帐项目的队列的句柄。
 *
 * @param pvItemToQueue 指向要放置在
 * 队列。  队列将保存的项目的大小是在创建队
 * 列时定义的，因此这么多字节将从
 * pvItemToQueue 复制到队列存储区域。
 *
 * @param pxHigherPriorityTaskWoken xQueueGenericSendFromISR() 将设置
 * *pxHigherPriorityTaskWoken 到 pdTRUE
 * 如果发送到队列导致任务解除阻塞，并且解除阻塞的任务的优先级高于当前正
 * 在运行的任务。  如果
 * xQueueGenericSendFromISR() 将此值设置为 pdTRUE，则应在退出中断之前请求上下文切换。
 *
 * @param xCopyPosition 可以取值queueSEND_TO_BACK来放置
 * 项目放在队列的后面，或者
 * queueSEND_TO_FRONT 将项目放在 queue 的前面（对于高优先级消息）。
 *
 * @return 如果数据成功发送到队列则为pdTRUE，否则
 * errQUEUE_FULL.
 *
 * 缓冲 IO 的用法示例（其中
 * ISR 每次调用可以获得多个值）：
   <pre>
 void vBufferISR( void )
 {
 char cIn;
 BaseType_t xHigherPriorityTaskWokenByPost;

	//我们还没有在ISR启动时唤醒任务。
	xHigherPriorityTaskWokenByPost = pdFALSE;

	//循环直到缓冲区为空。
	do
	{
		//从缓冲区获取一个字节。
		cIn = portINPUT_BYTE( RX_REGISTER_ADDRESS );

		//发布每个字节。
		xQueueGenericSendFromISR( xRxQueue, &cIn, &xHigherPriorityTaskWokenByPost, queueSEND_TO_BACK );

	} while( portINPUT_BYTE( BUFFER_COUNT ) );

	//现在缓冲区是空的，我们可以根据需要切换上下文。  请
	//注意，所需的yield 函数的名称是特定于端口的。
	if( xHigherPriorityTaskWokenByPost )
	{
		portYIELD_FROM_ISR();
	}
 }
 </pre>
 *
 * \defgroup xQueueSendFromISR xQueueSendFromISR
 * \ingroup QueueManagement
 */
BaseType_t xQueueGenericSendFromISR( QueueHandle_t xQueue, const void * const pvItemToQueue, BaseType_t * const pxHigherPriorityTaskWoken, const BaseType_t xCopyPosition ) PRIVILEGED_FUNCTION;
BaseType_t xQueueGiveFromISR( QueueHandle_t xQueue, BaseType_t * const pxHigherPriorityTaskWoken ) PRIVILEGED_FUNCTION;

/**
 * 队列。小时
 * <pre>
 BaseType_t xQueueReceiveFromISR(
									   QueueHandle_t	xQueue,
									   void	*pvBuffer,
									   BaseType_t *pxTaskWoken
								   );
 * </pre>
 *
 * 从队列接收一个项目。  在中断
 * 服务程序中使用该函数是安全的。
 *
 * @param xQueue 要从中获取项目的队列的句柄
 * 已收到。
 *
 * @param pvBuffer 指向接收到的项目将放入的缓冲区的指针
 * 被复制。
 *
 * @param pxTaskWoken 任务可能被阻塞，等待空间变满
 * 可以在队列中使用。  如果
 * xQueueReceiveFromISR 导致此类任务解锁，*
 * pxTaskWoken 将设置为 pdTRUE，否则 *pxTaskWoken 将保持不变。
 *
 * @return pdTRUE 如果从队列成功接收到项目，
 * 否则为 pdFALSE。
 *
 * 用法示例：
   <pre>

 QueueHandle_t xQueue;

 //创建队列并发布一些值的函数。
 void vAFunction( void *pvParameters )
 {
 char cValueToPost;
 const TickType_t xTicksToWait = ( TickType_t )0xff;

	//创建一个能够包含 10 个字符的队列。
	xQueue = xQueueCreate( 10, sizeof( char ) );
	if( xQueue == 0 )
	{
		//创建队列失败。
	}

	//...

	//发布一些将在 ISR 中使用的字符。  如果队列已满，
	//则此任务将阻塞 xTicksToWait 时钟周期。
	cValueToPost = 'a';
	xQueueSend( xQueue, ( void * ) &cValueToPost, xTicksToWait );
	cValueToPost = 'b';
	xQueueSend( xQueue, ( void * ) &cValueToPost, xTicksToWait );

	//...继续发布字符...当队
	//列已满时，此任务可能会阻塞。

	cValueToPost = 'c';
	xQueueSend( xQueue, ( void * ) &cValueToPost, xTicksToWait );
 }

 //ISR 输出队列中接收到的所有字符。
 void vISR_Routine( void )
 {
 BaseType_t xTaskWokenByReceive = pdFALSE;
 char cRxedChar;

	while( xQueueReceiveFromISR( xQueue, ( void * ) &cRxedChar, &xTaskWokenByReceive) )
	{
		//收到一个字符。  现在输出字符。
		vOutputCharacter( cRxedChar );

		//如果从队列中删除该字符唤醒了发布到队列
		//cTaskWokenByReceive 的任
		//务，则该任务将被设置为 pdTRUE。  无
		//论这个循环迭代多少次，都只会唤醒一个任务。
	}

	if( cTaskWokenByPost != ( char ) pdFALSE;
	{
		taskYIELD ();
	}
 }
 </pre>
 * \defgroup xQueueReceiveFromISR xQueueReceiveFromISR
 * \ingroup QueueManagement
 */
BaseType_t xQueueReceiveFromISR( QueueHandle_t xQueue, void * const pvBuffer, BaseType_t * const pxHigherPriorityTaskWoken ) PRIVILEGED_FUNCTION;

/*
 * 用于查询可从 ISR 安全使用的队列的实用程序。
 * 这些实用程序只能在 ISR 内或关键部分内使用。
 */
BaseType_t xQueueIsQueueEmptyFromISR( const QueueHandle_t xQueue ) PRIVILEGED_FUNCTION;
BaseType_t xQueueIsQueueFullFromISR( const QueueHandle_t xQueue ) PRIVILEGED_FUNCTION;
UBaseType_t uxQueueMessagesWaitingFromISR( const QueueHandle_t xQueue ) PRIVILEGED_FUNCTION;

/*
 * 上面定义的函数用于将数据传入和传出任务
 * 。  下面的函数相当于将数据传入和传出
 * co-routines.
 *
 * 这些函数是从 co-routine 宏实现中调
 * 用的，不应直接从应用程序代码中调用。  而是使
 * 用 croutine.h 中定义的宏包装器。
 */
BaseType_t xQueueCRSendFromISR( QueueHandle_t xQueue, const void *pvItemToQueue, BaseType_t xCoRoutinePreviouslyWoken );
BaseType_t xQueueCRReceiveFromISR( QueueHandle_t xQueue, void *pvBuffer, BaseType_t *pxTaskWoken );
BaseType_t xQueueCRSend( QueueHandle_t xQueue, const void *pvItemToQueue, TickType_t xTicksToWait );
BaseType_t xQueueCRReceive( QueueHandle_t xQueue, void *pvBuffer, TickType_t xTicksToWait );

/*
 * 仅供内部使用。  使用 xSemaphoreCreateMutex()
 * 、xSemaphoreCreateCounting() 或
 * xSemaphoreGetMutexHolder() 而不是直接调用这些函数。
 */
QueueHandle_t xQueueCreateMutex( const uint8_t ucQueueType ) PRIVILEGED_FUNCTION;
QueueHandle_t xQueueCreateMutexStatic( const uint8_t ucQueueType, StaticQueue_t *pxStaticQueue ) PRIVILEGED_FUNCTION;
QueueHandle_t xQueueCreateCountingSemaphore( const UBaseType_t uxMaxCount, const UBaseType_t uxInitialCount ) PRIVILEGED_FUNCTION;
QueueHandle_t xQueueCreateCountingSemaphoreStatic( const UBaseType_t uxMaxCount, const UBaseType_t uxInitialCount, StaticQueue_t *pxStaticQueue ) PRIVILEGED_FUNCTION;
BaseType_t xQueueSemaphoreTake( QueueHandle_t xQueue, TickType_t xTicksToWait ) PRIVILEGED_FUNCTION;
TaskHandle_t xQueueGetMutexHolder( QueueHandle_t xSemaphore ) PRIVILEGED_FUNCTION;
TaskHandle_t xQueueGetMutexHolderFromISR( QueueHandle_t xSemaphore ) PRIVILEGED_FUNCTION;

/*
 * 仅供内部使用。  使用 xSemaphoreTakeMutexRecursive() 或
 * xSemaphoreGiveMutexRecursive() 而不是直接调用这些函数。
 */
BaseType_t xQueueTakeMutexRecursive( QueueHandle_t xMutex, TickType_t xTicksToWait ) PRIVILEGED_FUNCTION;
BaseType_t xQueueGiveMutexRecursive( QueueHandle_t xMutex ) PRIVILEGED_FUNCTION;

/*
 * 将队列重置回其原始空状态。  返回值
 * 现已废弃，始终设置为 pdPASS。
 */
#define xQueueReset( xQueue ) xQueueGenericReset( xQueue, pdFALSE )

/*
 * 注册表是作为内核感知调试器定位队列、信号量和互斥体的
 * 一种手段而提供的。  如果您希望内核感知调试器可以使
 * 用该句柄，请调用
 * vQueueAddToRegistry() 将队列、
 * 信号量或互斥句柄添加到注册表。  如果您没有使用内核感知调试器，则可以忽略此函数。
 *
 * configQUEUE_REGISTRY_SIZE 定义注册
 * 表可以容纳的最大句柄数。  FreeRTOSConfig.h
 * 中的 configQUEUE_REGISTRY_SIZE
 * 必须大于 0，注册表才可用。  它的值不会影响可以创建的队列
 * 、信号量和互斥体的数量 - 只是注册表可以保存的数量。
 *
 * @param xQueue 添加到注册表的队列的句柄。  这个
 * 是调用 xQueueCreate() 返回的
 * 句柄。  信号量和互斥句柄也可以在这里传递。
 *
 * @param pcName 与句柄关联的名称。  这是
 * 内核感知调试器将显示的名称。  队列注册表仅存储指向该字符
 * 串的指针 - 因此该字符串必须是 persistent（全
 * 局或最好在 ROM/Flash 中），而不是在堆栈上。
 */
#if( configQUEUE_REGISTRY_SIZE > 0 )
	void vQueueAddToRegistry( QueueHandle_t xQueue, const char *pcQueueName ) PRIVILEGED_FUNCTION; /*lint !e971 仅字符串和单个字符允许使用非限定字符类型。 */
#endif

/*
 * 注册表是作为内核感知调试器定位队列、信号量和互斥体的一种手段
 * 而提供的。  如果您希望内核感知调试器可以使用该句柄，请调用
 * vQueueAddToRegistry() 将队列、信号量
 * 或互斥量句柄添加到注册表，并调用
 * vQueueUnregisterQueue() 从寄存器中删
 * 除队列、信号量或互斥量。  如果您没有使用内核感知调试器，则可以忽略此函数。
 *
 * @param xQueue 正在从注册表中删除的队列的句柄。
 */
#if( configQUEUE_REGISTRY_SIZE > 0 )
	void vQueueUnregisterQueue( QueueHandle_t xQueue ) PRIVILEGED_FUNCTION;
#endif

/*
 * 队列注册表是作为内核感知调试器定位队列、信号量和互
 * 斥体的一种手段而提供的。  调用
 * pcQueueGetName() 从队列注册表中查找并返回队列名称
 * 处理。
 *
 * @param xQueue 将返回其名称的队列的句柄。
 * @return 如果队列在注册表中，则指向队列名称的指针
 * 队列被返回。  如果队列不在注册表中，则 NULL 是
 * 回来了。
 */
#if( configQUEUE_REGISTRY_SIZE > 0 )
	const char *pcQueueGetName( QueueHandle_t xQueue ) PRIVILEGED_FUNCTION; /*lint !e971 仅字符串和单个字符允许使用非限定字符类型。 */
#endif

/*
 * 用于使用动态内存分配创建队列的函数的通用版本。
 * 这是由其他函数和宏调用的，这些函数和宏创建
 * 使用队列结构作为基础的其他 RTOS 对象。
 */
#if( configSUPPORT_DYNAMIC_ALLOCATION == 1 )
	QueueHandle_t xQueueGenericCreate( const UBaseType_t uxQueueLength, const UBaseType_t uxItemSize, const uint8_t ucQueueType ) PRIVILEGED_FUNCTION;
#endif

/*
 * 用于使用动态内存分配创建队列的函数的通用版本。
 * 这是由其他函数和宏调用的，这些函数和宏创建
 * 使用队列结构作为基础的其他 RTOS 对象。
 */
#if( configSUPPORT_STATIC_ALLOCATION == 1 )
	QueueHandle_t xQueueGenericCreateStatic( const UBaseType_t uxQueueLength, const UBaseType_t uxItemSize, uint8_t *pucQueueStorage, StaticQueue_t *pxStaticQueue, const uint8_t ucQueueType ) PRIVILEGED_FUNCTION;
#endif

/*
 * 队列集提供了一种机制，允许任务同时从多
 * 个队列或信号量执行读取操作（挂起）。
 *
 * 有关使用此的示例，请参阅 FreeRTOS/Source/Demo/Common/Minimal/QueueSet.c
 * 功能。
 *
 * A queue set must be explicitly created using a call to xQueueCreateSet()
 * 在可以使用之前。  创建后，可以使用调用
 * xQueueAddToSet() 将标准
 * FreeRTOS 队列和信号量添加到集合中。然后使用
 * xQueueSelectFromSet() 来确定集合中
 * 包含的队列或信号量（如果有）中的哪一个处于队列读取或信号量获取操作成功的状态。
 *
 * 注 1：参见 http://wwwFreeRTOS.org/RTOS-queue-sets.html 的文档
 * for reasons why queue sets are very rarely needed in practice as there are
 * 阻塞多个对象的更简单的方法。
 *
 * 注 2：在包含互斥锁的队列集上进行阻塞不会
 * 导致互斥锁持有者继承被阻塞任务的优先级。
 *
 * 注3：添加到队列集中的每个队列中的每个空间都
 * 需要额外的4字节RAM。  因此，不应将具有
 * 高最大计数值的计数信号量添加到队列集中。
 *
 * 注 4：不得对队列集的成员执行 receive（对于队列）
 * 或 take（对于信号量）操作，除非调用
 * xQueueSelectFromSet() 首先返回了该集成员的句柄。
 *
 * @param uxEventQueueLength 队列集存储发生在
 * 集合中包含的队列和信号量。
 * uxEventQueueLength 指定一次可以排队
 * 的最大事件数。  为了绝对确定事件不会丢失，
 * uxEventQueueLength 应设置为添加到集
 * 合中的队列长度的总和，其中二进制信号量和互斥体的长度为
 * 1，计数信号量的长度由其最大计数值设置。  示例：
 * + 如果队列集要保存一个长度为 5 的队列、另一个长度
 * 为 12 的队列和一个二进制信号量，则 uxEventQueueLength 应设置为
 * (5 + 12 + 1)，或 18。
 * + 如果队列集要保存三个二进制信号量，则应将
 * uxEventQueueLength 设置为 to (1 +
 * 1 + 1 ) 或 3。 + 如果队列集要保存最大计数为
 * 5 的计数信号量和最大计数为 3 的计数信号量，则应设置
 * uxEventQueueLength to (5 + 3)，或 8。
 *
 * @return 如果队列集创建成功，则创建的句柄
 * 返回队列集。  否则返回 NULL。
 */
QueueSetHandle_t xQueueCreateSet( const UBaseType_t uxEventQueueLength ) PRIVILEGED_FUNCTION;

/*
 * 将队列或信号量添加到先前通过调用
 * xQueueCreateSet() 创建的队列集中。
 *
 * 有关使用此的示例，请参阅 FreeRTOS/Source/Demo/Common/Minimal/QueueSet.c
 * 功能。
 *
 * 注 1：不得对队列集的成员执行 receive（对于队列）
 * 或 take（对于信号量）操作，除非调用
 * xQueueSelectFromSet() 首先返回了该集成员的句柄。
 *
 * @param xQueueOrSemaphore 正在添加的队列或信号量的句柄
 * 队列 set（转换为 QueueSetMemberHandle_t 类型）。
 *
 * @param xQueueSet 队列或信号量所设置的队列句柄
 * 正在添加中。
 *
 * @return 如果队列或信号量已成功添加到队列集中
 * 然后返回 pdPASS。  如果队列无
 * 法成功添加到队列集，因为它已经是不同队
 * 列集的成员，则返回 pdFAIL。
 */
BaseType_t xQueueAddToSet( QueueSetMemberHandle_t xQueueOrSemaphore, QueueSetHandle_t xQueueSet ) PRIVILEGED_FUNCTION;

/*
 * 从队列集中删除队列或信号量。  仅当队列或信
 * 号量为空时，才能从集合中删除队列或信号量。
 *
 * 有关使用此的示例，请参阅 FreeRTOS/Source/Demo/Common/Minimal/QueueSet.c
 * 功能。
 *
 * @param xQueueOrSemaphore 被删除的队列或信号量的句柄
 * 来自队列 set（转换为 QueueSetMemberHandle_t 类型）。
 *
 * @param xQueueSet 队列或信号量所在的队列集的句柄
 * 包括在内。
 *
 * @return 如果队列或信号量已成功从队列集中删除
 * 然后返回 pdPASS。  如果队列不在队列集中，或者
 * queue（或信号量）不为空，则返回 pdFAIL。
 */
BaseType_t xQueueRemoveFromSet( QueueSetMemberHandle_t xQueueOrSemaphore, QueueSetHandle_t xQueueSet ) PRIVILEGED_FUNCTION;

/*
 * xQueueSelectFromSet() 从队列集中的成员
 * 中选择一个队列或信号量，该队列或信号量要么包含 data（在
 * 队列的情况下），要么可用于 take（在信号量的情况下）。
 * xQueueSelectFromSet() 有效地允许任
 * 务同时对队列集中的所有队列和信号量执行读取操作（挂起）。
 *
 * 有关使用此的示例，请参阅 FreeRTOS/Source/Demo/Common/Minimal/QueueSet.c
 * 功能。
 *
 * 注 1：参见 http://wwwFreeRTOS.org/RTOS-queue-sets.html 的文档
 * for reasons why queue sets are very rarely needed in practice as there are
 * 阻塞多个对象的更简单的方法。
 *
 * 注 2：在包含互斥锁的队列集上进行阻塞不会
 * 导致互斥锁持有者继承被阻塞任务的优先级。
 *
 * 注 3：不得对队列集的成员执行 receive（对于队列）
 * 或 take（对于信号量）操作，除非调用
 * xQueueSelectFromSet() 首先返回了该集成员的句柄。
 *
 * @param xQueueSet 任务 will（可能）阻塞的队列集。
 *
 * @param xTicksToWait 调用任务将执行的最长时间（以时钟周期为单位）
 * 保留在阻塞的 state 中（同时执行其他任务）以
 * 等待队列集的成员准备好成功进行队列读取或信号量获取
 * 手术。
 *
 * @return xQueueSelectFromSet() 将返回 queue 的句柄（强制转换为
 * 包含数据的队列集中包含的
 * QueueSetMemberHandle_t 类型），或可用队列集
 * 中包含的 semaphore（转换为
 * QueueSetMemberHandle_t 类型）的句柄，或者 NULL（如果在指定的块时间到期之前不存在此类队列或信号量）。
 */
QueueSetMemberHandle_t xQueueSelectFromSet( QueueSetHandle_t xQueueSet, const TickType_t xTicksToWait ) PRIVILEGED_FUNCTION;

/*
 * 可以从 ISR 使用的 xQueueSelectFromSet() 版本。
 */
QueueSetMemberHandle_t xQueueSelectFromSetFromISR( QueueSetHandle_t xQueueSet ) PRIVILEGED_FUNCTION;

/* 不是公共 API 函数。 */
void vQueueWaitForMessageRestricted( QueueHandle_t xQueue, TickType_t xTicksToWait, const BaseType_t xWaitIndefinitely ) PRIVILEGED_FUNCTION;
BaseType_t xQueueGenericReset( QueueHandle_t xQueue, BaseType_t xNewQueue ) PRIVILEGED_FUNCTION;
void vQueueSetQueueNumber( QueueHandle_t xQueue, UBaseType_t uxQueueNumber ) PRIVILEGED_FUNCTION;
UBaseType_t uxQueueGetQueueNumber( QueueHandle_t xQueue ) PRIVILEGED_FUNCTION;
uint8_t ucQueueGetQueueType( QueueHandle_t xQueue ) PRIVILEGED_FUNCTION;


#ifdef __cplusplus
}
#endif

#endif /* QUEUE_H */

