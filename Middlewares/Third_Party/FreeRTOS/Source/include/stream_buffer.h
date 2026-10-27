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
 * 流缓冲区用于将连续的数据流从一个任务或中断
 * 发送到另一任务。  它们的实现是轻量级的，
 * 使它们特别适合任务中断和核心到核心通信
 * 场景。
 *
 * ***NOTE***：在 FreeRTOS 对象中，流缓冲
 * 区 implementation（消息缓冲区实现也是如此，
 * 因为消息缓冲区构建在流缓冲区之上）假设只有一个任务或中断将
 * 写入 buffer（写入器），并且只有一个任务或中断将从
 * buffer（写入器）读取数据。读者）。  对于不同的任务
 * 或中断，写入器和读取器是安全的，但是，与其他
 * FreeRTOS 对象不同，拥有多个不同的写入器或多个不同
 * 的读取器是不安全的。  如果有多个不同的写入器，则应用程序
 * 编写器必须将每次调用放置在关键部分内的写入 API
 * function（例如
 * xStreamBufferSend()），并将发送块时间设
 * 置为 0。同样，如果有多个不同的读取器，则应用程序写入器必
 * 须将每次调用放置为读取 API function（例如
 * xStreamBufferReceive()) 在临界区部分内并将接收块时间设置为 0。
 *
 */

#ifndef STREAM_BUFFER_H
#define STREAM_BUFFER_H

#ifndef INC_FREERTOS_H
	#error "include FreeRTOS.h must appear in source files before include stream_buffer.h"
#endif

#if defined( __cplusplus )
extern "C" {
#endif

/**
 * 引用流缓冲区的类型。  例如，调用 xStreamBufferCreate() 返回
 * 一个 StreamBufferHandle_t 变量，该变量随后可以用作
 * xStreamBufferSend()、xStreamBufferReceive() 的参数，
 * ETC。
 */
struct StreamBufferDef_t;
typedef struct StreamBufferDef_t * StreamBufferHandle_t;


/**
 * message_buffer.h
 *
<pre>
StreamBufferHandle_t xStreamBufferCreate( size_t xBufferSizeBytes, size_t xTriggerLevelBytes );
</pre>
 *
 * 使用动态分配的内存创建新的流缓冲区。  请参阅
 * xStreamBufferCreateStatic()
 * 了解使用静态分配的 memory（在编译时分配的内存）的版本。
 *
 * configSUPPORT_DYNAMIC_ALLOCATION 必须设置为 1 或在
 * FreeRTOSConfig.h 中保持未定义，xStreamBufferCreate() 才可用。
 *
 * @param xBufferSizeBytes 流缓冲区的总字节数
 * 任何时候都可以持有。
 *
 * @param xTriggerLevelBytes 流中必须存在的字节数
 * 在流缓冲区上阻塞等待数据移出阻塞状态的任务之
 * 前。  例如，如果任务在读取触发级别为 1
 * 的空流缓冲区时被阻塞，则当向缓冲区写入单个字
 * 节或任务的阻塞时间到期时，该任务将被解除阻塞
 * 。  作为另一个示例，如果任务在读取触发级别
 * 为 10 的空流缓冲区时被阻塞，则该任务将不
 * 会被解除阻塞，直到流缓冲区包含至少 10 个
 * 字节或任务的阻塞时间到期。  如果读取任务的
 * 阻塞时间在达到触发级别之前到期，则该任务仍将
 * 接收实际可用的字节数。  将触发电平设置为
 * 0 将导致使用触发电平 1。  指定大于缓冲
 * 区大小的触发级别无效。
 *
 * @return 如果返回NULL，则无法创建流缓冲区
 * 因为没有足够的堆内存可供
 * FreeRTOS 分配流缓冲区数据结构和
 * 存储区域。  返回的非 NULL 值表示
 * 流缓冲区已成功创建 - 返回的值应存储为创建的流的句柄
 * 缓冲。
 *
 * 使用示例：
<pre>

void vAFunction( void )
{
StreamBufferHandle_t xStreamBuffer;
const size_t xStreamBufferSizeBytes = 100, xTriggerLevel = 10;

    //创建一个可以容纳 100 字节的流缓
    //冲区。  用于保存流缓冲区结构和流缓
    //冲区中的数据的内存是动态分配的。
    xStreamBuffer = xStreamBufferCreate( xStreamBufferSizeBytes, xTriggerLevel );

    if( xStreamBuffer == NULL )
    {
        //没有足够的堆内存空间
        //可用于创建流缓冲区。
    }
    else
    {
        //流缓冲区已成功创建，现在可以使用。
    }
}
</pre>
 * \defgroup xStreamBufferCreate xStreamBufferCreate
 * \ingroup StreamBufferManagement
 */
#define xStreamBufferCreate( xBufferSizeBytes, xTriggerLevelBytes ) xStreamBufferGenericCreate( xBufferSizeBytes, xTriggerLevelBytes, pdFALSE )

/**
 * stream_buffer.h
 *
<pre>
StreamBufferHandle_t xStreamBufferCreateStatic( size_t xBufferSizeBytes,
                                                size_t xTriggerLevelBytes,
                                                uint8_t *pucStreamBufferStorageArea,
                                                StaticStreamBuffer_t *pxStaticStreamBuffer );
</pre>
 * 使用静态分配的内存创建新的流缓冲区。  请参阅
 * xStreamBufferCreate() 了解使用动态分配内存的版本。
 *
 * configSUPPORT_STATIC_ALLOCATION 必须在
 * FreeRTOSConfig.h 中设置为 1，xStreamBufferCreateStatic() 才可用。
 *
 * @param xBufferSizeBytes 所指向的缓冲区的大小（以字节为单位）
 * pucStreamBufferStorageArea参数。
 *
 * @param xTriggerLevelBytes 流中必须存在的字节数
 * 在流缓冲区上阻塞等待数据移出阻塞状态的任务之
 * 前。  例如，如果任务在读取触发级别为 1
 * 的空流缓冲区时被阻塞，则当向缓冲区写入单个字
 * 节或任务的阻塞时间到期时，该任务将被解除阻塞
 * 。  作为另一个示例，如果任务在读取触发级别
 * 为 10 的空流缓冲区时被阻塞，则该任务将不
 * 会被解除阻塞，直到流缓冲区包含至少 10 个
 * 字节或任务的阻塞时间到期。  如果读取任务的
 * 阻塞时间在达到触发级别之前到期，则该任务仍将
 * 接收实际可用的字节数。  将触发电平设置为
 * 0 将导致使用触发电平 1。  指定大于缓冲
 * 区大小的触发级别无效。
 *
 * @param pucStreamBufferStorageArea 必须指向位于以下位置的 uint8_t 数组
 * 至少 xBufferSizeBytes + 1
 * 大。  这是流写入流缓冲区时复制到的数组。
 *
 * @param pxStaticStreamBuffer 必须指向类型的变量
 * StaticStreamBuffer_t，将用于保存流缓冲区的数据
 * 结构。
 *
 * @return 如果流缓冲区创建成功，则流缓冲区的句柄
 * 返回创建的流缓冲区。如果 pucStreamBufferStorageArea
 * 或 pxStaticstreamBuffer 是 NULL，则返回 NULL。
 *
 * 使用示例：
<pre>

//用于指定用于保存流的数组的尺寸。  可用
//空间实际上会比这个少 1，即 999。
#define STORAGE_SIZE_BYTES 1000

//定义实际保存流中流的内存
//buffer.
static uint8_t ucStorageBuffer[ STORAGE_SIZE_BYTES ];

//该变量用于保存流缓冲区结构。
StaticStreamBuffer_t xStreamBufferStruct;

void MyFunction( void )
{
StreamBufferHandle_t xStreamBuffer;
const size_t xTriggerLevel = 1;

    xStreamBuffer = xStreamBufferCreateStatic( sizeof( ucBufferStorage ),
                                               xTriggerLevel,
                                               ucBufferStorage,
                                               &xStreamBufferStruct );

    //由于 pucStreamBufferStorageArea 或
    //pxStaticStreamBuffer 参数都不是 NULL，因此
    //xStreamBuffer 不会是 NULL，并且可以用于在其他流缓冲区 API 调用中引用创建的流缓冲区。

    //使用流缓冲区的其他代码可以放在此处。
}

</pre>
 * \defgroup xStreamBufferCreateStatic xStreamBufferCreateStatic
 * \ingroup StreamBufferManagement
 */
#define xStreamBufferCreateStatic( xBufferSizeBytes, xTriggerLevelBytes, pucStreamBufferStorageArea, pxStaticStreamBuffer ) xStreamBufferGenericCreateStatic( xBufferSizeBytes, xTriggerLevelBytes, pdFALSE, pucStreamBufferStorageArea, pxStaticStreamBuffer )

/**
 * stream_buffer.h
 *
<pre>
size_t xStreamBufferSend( StreamBufferHandle_t xStreamBuffer,
                          const void *pvTxData,
                          size_t xDataLengthBytes,
                          TickType_t xTicksToWait );
</pre>
 *
 * 将字节发送到流缓冲区。  这些字节被复制到流缓冲区中。
 *
 * ***NOTE***：在 FreeRTOS 对象中，流缓
 * 冲区 implementation（消息缓冲区实现也是如
 * 此，因为消息缓冲区构建在流缓冲区之上）假设只有一个任务或
 * 中断将写入 buffer（写入器），并且只有一个任务或中
 * 断将从 buffer（写入器）读取数据。读者）。  对于
 * 不同的任务或中断，写入器和读取器是安全的，但是，与其他
 * FreeRTOS 对象不同，拥有多个不同的写入器或多个不
 * 同的读取器是不安全的。  如果有多个不同的写入器，则应用
 * 程序编写器必须将每次调用放置在关键部分内的写入 API
 * function（例如
 * xStreamBufferSend()），并将发送块时间
 * 设置为 0。同样，如果有多个不同的读取器，则应用程序写入
 * 器必须将每次调用放置为读取 API function（例
 * 如xStreamBufferReceive()) 在关键部分内并将接收块时间设置为 0。
 *
 * 使用 xStreamBufferSend() 从任务写入流缓冲
 * 区。  使用
 * xStreamBufferSendFromISR() 从中断服务 routine (ISR) 写入流缓冲区。
 *
 * @param xStreamBuffer 流所在的流缓冲区的句柄
 * 正在发送。
 *
 * @param pvTxData 指向保存要复制的字节的缓冲区的指针
 * 进入流缓冲区。
 *
 * @param xDataLengthBytes 从 pvTxData 复制的最大字节数
 * 进入流缓冲区。
 *
 * @param xTicksToWait 任务应在任务中保留的最长时间
 * 如果流缓冲区包含的空间太少而无法容纳另一个
 * xDataLengthBytes 字节，则处于阻塞状态以等待流
 * 缓冲区中有足够的空间可用。  区块时间以滴答周期为单位指定，因
 * 此它表示的绝对时间取决于滴答频率。  宏
 * pdMS_TO_TICKS() 可用于将以毫秒为单位的时间转换
 * 为以刻度为单位的时间。  如果
 * INCLUDE_vTaskSuspend 在
 * FreeRTOSConfig.h 中设置为 1，则将
 * xTicksToWait 设置为 portMAX_DELAY
 * 将导致任务等待 indefinitely（不会超时）。  如果
 * 任务在将所有 xDataLengthBytes 写入缓冲区之前超时，它仍会写入尽可能多的字节。  任务处于阻塞状态时不使用任何 CPU 时间。
 *
 * @return 写入流缓冲区的字节数。  如果一个任务时间
 * 在将所有 xDataLengthBytes
 * 写入缓冲区之前，它仍会写入尽可能多的字节。
 *
 * 使用示例：
<pre>
void vAFunction( StreamBufferHandle_t xStreamBuffer )
{
size_t xBytesSent;
uint8_t ucArrayToSend[] = { 0, 1, 2, 3 };
char *pcStringToSend = "String to send";
const TickType_t x100ms = pdMS_TO_TICKS( 100 );

    //将数组发送到流缓冲区，最多阻塞 100
    //毫秒，以等待流缓冲区中有足够的可用空间。
    xBytesSent = xStreamBufferSend( xStreamBuffer, ( void * ) ucArrayToSend, sizeof( ucArrayToSend ), x100ms );

    if( xBytesSent != sizeof( ucArrayToSend ) )
    {
        //在缓冲区中有足够的空间用于写入数据之前，对
        //xStreamBufferSend() 的调用超
        //时，但它确实成功写入了 xBytesSent 字节。
    }

    //将字符串发送到流缓冲区。  如
    //果缓冲区空间不足，则立即返回。
    xBytesSent = xStreamBufferSend( xStreamBuffer, ( void * ) pcStringToSend, strlen( pcStringToSend ), 0 );

    if( xBytesSent != strlen( pcStringToSend ) )
    {
        //由于缓冲区中没有足够的可用空间，无法将整个字符
        //串添加到流缓冲区中，但已发送
        //xBytesSent 字节。  可以再次尝试发送剩余字节。
    }
}
</pre>
 * \defgroup xStreamBufferSend xStreamBufferSend
 * \ingroup StreamBufferManagement
 */
size_t xStreamBufferSend( StreamBufferHandle_t xStreamBuffer,
						  const void *pvTxData,
						  size_t xDataLengthBytes,
						  TickType_t xTicksToWait ) PRIVILEGED_FUNCTION;

/**
 * stream_buffer.h
 *
<pre>
size_t xStreamBufferSendFromISR( StreamBufferHandle_t xStreamBuffer,
                                 const void *pvTxData,
                                 size_t xDataLengthBytes,
                                 BaseType_t *pxHigherPriorityTaskWoken );
</pre>
 *
 * API 函数的中断安全版本
 * ，将字节流发送到流缓冲区。
 *
 * ***NOTE***：在 FreeRTOS 对象中，流缓
 * 冲区 implementation（消息缓冲区实现也是如
 * 此，因为消息缓冲区构建在流缓冲区之上）假设只有一个任务或
 * 中断将写入 buffer（写入器），并且只有一个任务或中
 * 断将从 buffer（写入器）读取数据。读者）。  对于
 * 不同的任务或中断，写入器和读取器是安全的，但是，与其他
 * FreeRTOS 对象不同，拥有多个不同的写入器或多个不
 * 同的读取器是不安全的。  如果有多个不同的写入器，则应用
 * 程序编写器必须将每次调用放置在关键部分内的写入 API
 * function（例如
 * xStreamBufferSend()），并将发送块时间
 * 设置为 0。同样，如果有多个不同的读取器，则应用程序写入
 * 器必须将每次调用放置为读取 API function（例
 * 如xStreamBufferReceive()) 在关键部分内并将接收块时间设置为 0。
 *
 * 使用 xStreamBufferSend() 从任务写入流缓冲
 * 区。  使用
 * xStreamBufferSendFromISR() 从中断服务 routine (ISR) 写入流缓冲区。
 *
 * @param xStreamBuffer 流所在的流缓冲区的句柄
 * 正在发送。
 *
 * @param pvTxData 指向要复制到流中的数据的指针
 * 缓冲。
 *
 * @param xDataLengthBytes 从 pvTxData 复制的最大字节数
 * 进入流缓冲区。
 *
 * @param pxHigherPriorityTaskWoken 流缓冲区可能会
 * 有一个任务被阻塞等待数据。  调用
 * xStreamBufferSendFromISR() 可以使数据
 * 可用，从而导致正在等待数据的任务离开阻塞状态。  如果调用
 * xStreamBufferSendFromISR() 导致任务离
 * 开阻塞状态，并且未阻塞任务的优先级高于当前正在执行的 task（
 * 被中断的任务），则
 * xStreamBufferSendFromISR() 在内部会将
 * *pxHigherPriorityTaskWoken 设置为
 * pdTRUE。  如果
 * xStreamBufferSendFromISR()将此值设置为
 * pdTRUE，则通常应在退出中断之前执行上下文切换。  这将确保
 * 中断直接返回到最高优先级的就绪状态任务。  *pxHigherPriorityTaskWoken 在传递给函数之前应设置为 pdFALSE。  有关示例，请参阅下面的示例代码。
 *
 * @return 实际写入流缓冲区的字节数，这将
 * 如果流缓冲区没有足够的可用空间来写入所有字节，
 * 则应小于 xDataLengthBytes。
 *
 * 使用示例：
<pre>
//已经创建的流缓冲区。
StreamBufferHandle_t xStreamBuffer;

void vAnInterruptServiceRoutine( void )
{
size_t xBytesSent;
char *pcStringToSend = "String to send";
BaseType_t xHigherPriorityTaskWoken = pdFALSE; // Initialised to pdFALSE.

    //尝试将字符串发送到流缓冲区。
    xBytesSent = xStreamBufferSendFromISR( xStreamBuffer,
                                           ( void * ) pcStringToSend,
                                           strlen( pcStringToSend ),
                                           &xHigherPriorityTaskWoken );

    if( xBytesSent != strlen( pcStringToSend ) )
    {
        //流缓冲区中没有足够的可用空间来写入整个字符
        //串，但写入了 xBytesSent 字节。
    }

    //如果 xHigherPriorityTaskWoken 在
    //xStreamBufferSendFromISR() 中设置为
    //pdTRUE，则优先级高于当前执行任务优先级的任务将被解锁，并且应
    //执行上下文切换以确保 ISR 返回到解锁任务。  在大多数
    //FreeRTOS 端口中，只需将
    //xHigherPriorityTaskWoken 传递到
    //taskYIELD_FROM_ISR() 即可完成，
    //taskYIELD_FROM_ISR() 将测试变量值，并在必要时执行上下文切换。  检查正在使用的端口的文档以获取特定于端口的说明。
    taskYIELD_FROM_ISR( xHigherPriorityTaskWoken );
}
</pre>
 * \defgroup xStreamBufferSendFromISR xStreamBufferSendFromISR
 * \ingroup StreamBufferManagement
 */
size_t xStreamBufferSendFromISR( StreamBufferHandle_t xStreamBuffer,
								 const void *pvTxData,
								 size_t xDataLengthBytes,
								 BaseType_t * const pxHigherPriorityTaskWoken ) PRIVILEGED_FUNCTION;

/**
 * stream_buffer.h
 *
<pre>
size_t xStreamBufferReceive( StreamBufferHandle_t xStreamBuffer,
                             void *pvRxData,
                             size_t xBufferLengthBytes,
                             TickType_t xTicksToWait );
</pre>
 *
 * 从流缓冲区接收字节。
 *
 * ***NOTE***：在 FreeRTOS 对象中，流缓
 * 冲区 implementation（消息缓冲区实现也是如
 * 此，因为消息缓冲区构建在流缓冲区之上）假设只有一个任务或
 * 中断将写入 buffer（写入器），并且只有一个任务或中
 * 断将从 buffer（写入器）读取数据。读者）。  对于
 * 不同的任务或中断，写入器和读取器是安全的，但是，与其他
 * FreeRTOS 对象不同，拥有多个不同的写入器或多个不
 * 同的读取器是不安全的。  如果有多个不同的写入器，则应用
 * 程序编写器必须将每次调用放置在关键部分内的写入 API
 * function（例如
 * xStreamBufferSend()），并将发送块时间
 * 设置为 0。同样，如果有多个不同的读取器，则应用程序写入
 * 器必须将每次调用放置为读取 API function（例
 * 如xStreamBufferReceive()) 在关键部分内并将接收块时间设置为 0。
 *
 * 使用 xStreamBufferReceive() 从任务的流缓冲区中
 * 读取。  使用
 * xStreamBufferReceiveFromISR() 从中断服务 routine (ISR) 的流缓冲区中读取数据。
 *
 * @param xStreamBuffer 从中传送字节的流缓冲区的句柄
 * 被接收。
 *
 * @param pvRxData 指向缓冲区的指针，接收到的字节将存入该缓冲区
 * 复制的。
 *
 * @param xBufferLengthBytes 所指向的缓冲区的长度
 * pvRxData参数。  这设置了一次调用中接收的最大字节数。
 * xStreamBufferReceive 将返回尽可能多的字
 * 节，直至达到 xBufferLengthBytes 设置的最大值。
 *
 * @param xTicksToWait 任务应在任务中保留的最长时间
 * 如果流缓冲区为空，则处于阻塞状态，等待数据变得可用。  如果
 * xTicksToWait 为零，
 * xStreamBufferReceive() 将立即返回。  区块
 * 时间以滴答周期为单位指定，因此它表示的绝对时间取决于滴答频率。
 * 宏 pdMS_TO_TICKS() 可用于将以毫秒为单位的时间转换
 * 为以刻度为单位的时间。  将 xTicksToWait 设置为
 * portMAX_DELAY 将导致任务等待
 * indefinitely（不会超时），前提是
 * FreeRTOSConfig.h 中的 INCLUDE_vTaskSuspend 设置为 1。  任务处于阻塞状态时不使用任何 CPU 时间。
 *
 * @return 实际从流缓冲区读取的字节数，这将
 * 如果在 xBufferLengthBytes 可用之前对
 * xStreamBufferReceive() 的调用超时，则该值小于 xBufferLengthBytes。
 *
 * 使用示例：
<pre>
void vAFunction( StreamBuffer_t xStreamBuffer )
{
uint8_t ucRxData[ 20 ];
size_t xReceivedBytes;
const TickType_t xBlockTime = pdMS_TO_TICKS( 20 );

    //从流缓冲区接收最多另一个 sizeof( ucRxData ) 字节。
    //在阻塞的 state 中等待（因此不使用任何 CPU 处理时间）最多
    //100ms，以获取完整的 sizeof( ucRxData ) 字节数
    //available.
    xReceivedBytes = xStreamBufferReceive( xStreamBuffer,
                                           ( void * ) ucRxData,
                                           sizeof( ucRxData ),
                                           xBlockTime );

    if( xReceivedBytes > 0 )
    {
        //一个ucRxData包含另一个
        //xRecievedBytes字节的数据，可以在这里处理......
    }
}
</pre>
 * \defgroup xStreamBufferReceive xStreamBufferReceive
 * \ingroup StreamBufferManagement
 */
size_t xStreamBufferReceive( StreamBufferHandle_t xStreamBuffer,
							 void *pvRxData,
							 size_t xBufferLengthBytes,
							 TickType_t xTicksToWait ) PRIVILEGED_FUNCTION;

/**
 * stream_buffer.h
 *
<pre>
size_t xStreamBufferReceiveFromISR( StreamBufferHandle_t xStreamBuffer,
                                    void *pvRxData,
                                    size_t xBufferLengthBytes,
                                    BaseType_t *pxHigherPriorityTaskWoken );
</pre>
 *
 * API 函数的中断安全版本
 * ，用于从流缓冲区接收字节。
 *
 * 使用 xStreamBufferReceive() 从任务的流缓冲区
 * 读取字节。使用
 * xStreamBufferReceiveFromISR() 从中断服务 routine (ISR) 的流缓冲区读取字节。
 *
 * @param xStreamBuffer 流缓冲区的句柄
 * 正在接收中。
 *
 * @param pvRxData 指向接收字节的缓冲区的指针
 * 复制的。
 *
 * @param xBufferLengthBytes 所指向的缓冲区的长度
 * pvRxData参数。  这设置了一次调用中接收的最大字节数。
 * xStreamBufferReceive 将返回尽可能多的字
 * 节，直至达到 xBufferLengthBytes 设置的最大值。
 *
 * @param pxHigherPriorityTaskWoken 流缓冲区可能会
 * 有一个任务被阻塞，等待空间可用。  调用
 * xStreamBufferReceiveFromISR() 可以使
 * 空间可用，从而导致正在等待空间的任务离开阻塞状态。  如果调用
 * xStreamBufferReceiveFromISR() 导致任
 * 务离开阻塞状态，并且未阻塞任务的优先级高于当前正在执行的 task
 * （被中断的任务），则
 * xStreamBufferReceiveFromISR() 在内部
 * 会将 *pxHigherPriorityTaskWoken 设置为
 * pdTRUE。如果
 * xStreamBufferReceiveFromISR()将此值设
 * 置为pdTRUE，则通常应在退出中断之前执行上下文切换。  这将确
 * 保中断直接返回到最高优先级的就绪状态任务。  *pxHigherPriorityTaskWoken 在传递给函数之前应设置为 pdFALSE。  有关示例，请参阅下面的代码示例。
 *
 * @return 从流缓冲区读取的字节数（如果有）。
 *
 * 使用示例：
<pre>
//已经创建的流缓冲区。
StreamBuffer_t xStreamBuffer;

void vAnInterruptServiceRoutine( void )
{
uint8_t ucRxData[ 20 ];
size_t xReceivedBytes;
BaseType_t xHigherPriorityTaskWoken = pdFALSE;  // Initialised to pdFALSE.

    //从流缓冲区接收下一个流。
    xReceivedBytes = xStreamBufferReceiveFromISR( xStreamBuffer,
                                                  ( void * ) ucRxData,
                                                  sizeof( ucRxData ),
                                                  &xHigherPriorityTaskWoken );

    if( xReceivedBytes > 0 )
    {
        //ucRxData 包含从流缓冲区读取的
        //xReceivedBytes。在这里处理流......
    }

    //如果 xHigherPriorityTaskWoken 在
    //xStreamBufferReceiveFromISR() 中设置为
    //pdTRUE，则优先级高于当前执行任务优先级的任务将被解锁，并且应
    //执行上下文切换以确保 ISR 返回到解锁任务。  在大多数
    //FreeRTOS 端口中，只需将
    //xHigherPriorityTaskWoken 传递到
    //taskYIELD_FROM_ISR() 即可完成，
    //taskYIELD_FROM_ISR() 将测试变量值，并在必要时执行上下文切换。  检查正在使用的端口的文档以获取特定于端口的说明。
    taskYIELD_FROM_ISR( xHigherPriorityTaskWoken );
}
</pre>
 * \defgroup xStreamBufferReceiveFromISR xStreamBufferReceiveFromISR
 * \ingroup StreamBufferManagement
 */
size_t xStreamBufferReceiveFromISR( StreamBufferHandle_t xStreamBuffer,
									void *pvRxData,
									size_t xBufferLengthBytes,
									BaseType_t * const pxHigherPriorityTaskWoken ) PRIVILEGED_FUNCTION;

/**
 * stream_buffer.h
 *
<pre>
void vStreamBufferDelete( StreamBufferHandle_t xStreamBuffer );
</pre>
 *
 * 删除之前通过调用 xStreamBufferCreate() 或
 * xStreamBufferCreateStatic() 创建的流缓
 * 冲区。  如果流缓冲区是使用动态 memory（即通过
 * xStreamBufferCreate()）创建的，则分配的内存将被释放。
 *
 * 在流缓冲区被使用后，不得使用流缓冲区句柄
 * 已删除。
 *
 * @param xStreamBuffer 要删除的流缓冲区的句柄。
 *
 * \defgroup vStreamBufferDelete vStreamBufferDelete
 * \ingroup StreamBufferManagement
 */
void vStreamBufferDelete( StreamBufferHandle_t xStreamBuffer ) PRIVILEGED_FUNCTION;

/**
 * stream_buffer.h
 *
<pre>
BaseType_t xStreamBufferIsFull( StreamBufferHandle_t xStreamBuffer );
</pre>
 *
 * 查询流缓冲区以查看其是否已满。  如果流缓冲区没
 * 有任何可用空间，则它已满，因此无法接受更多数据。
 *
 * @param xStreamBuffer 正在查询的流缓冲区的句柄。
 *
 * @return 如果流缓冲区已满，则返回 pdTRUE。  否则
 * 返回 pdFALSE。
 *
 * \defgroup xStreamBufferIsFull xStreamBufferIsFull
 * \ingroup StreamBufferManagement
 */
BaseType_t xStreamBufferIsFull( StreamBufferHandle_t xStreamBuffer ) PRIVILEGED_FUNCTION;

/**
 * stream_buffer.h
 *
<pre>
BaseType_t xStreamBufferIsEmpty( StreamBufferHandle_t xStreamBuffer );
</pre>
 *
 * 查询流缓冲区以查看它是否为空。  如果
 * 流缓冲区不包含任何数据，则它是空的。
 *
 * @param xStreamBuffer 正在查询的流缓冲区的句柄。
 *
 * @return 如果流缓冲区为空，则返回 pdTRUE。  否则
 * 返回 pdFALSE。
 *
 * \defgroup xStreamBufferIsEmpty xStreamBufferIsEmpty
 * \ingroup StreamBufferManagement
 */
BaseType_t xStreamBufferIsEmpty( StreamBufferHandle_t xStreamBuffer ) PRIVILEGED_FUNCTION;

/**
 * stream_buffer.h
 *
<pre>
BaseType_t xStreamBufferReset( StreamBufferHandle_t xStreamBuffer );
</pre>
 *
 * 将流缓冲区重置为其初始空状态。  流缓冲区中
 * 的所有数据都将被丢弃。  仅当没有任务被阻塞
 * 等待发送到流或从流接收时，才可以重置流缓冲区
 * 缓冲。
 *
 * @param xStreamBuffer 正在重置的流缓冲区的句柄。
 *
 * @return 如果流缓冲区被重置，则返回 pdPASS。  如果有
 * 任务阻塞等待发送到流缓冲区或从流缓冲区读取
 * ，则流缓冲区不会重置并返回 pdFAIL。
 *
 * \defgroup xStreamBufferReset xStreamBufferReset
 * \ingroup StreamBufferManagement
 */
BaseType_t xStreamBufferReset( StreamBufferHandle_t xStreamBuffer ) PRIVILEGED_FUNCTION;

/**
 * stream_buffer.h
 *
<pre>
size_t xStreamBufferSpacesAvailable( StreamBufferHandle_t xStreamBuffer );
</pre>
 *
 * 查询流缓冲区以查看其包含多少可
 * 用空间，该空间等于流缓冲区满之
 * 前可以发送到流缓冲区的数据量。
 *
 * @param xStreamBuffer 正在查询的流缓冲区的句柄。
 *
 * @return 之前可以写入流缓冲区的字节数
 * 流缓冲区将满。
 *
 * \defgroup xStreamBufferSpacesAvailable xStreamBufferSpacesAvailable
 * \ingroup StreamBufferManagement
 */
size_t xStreamBufferSpacesAvailable( StreamBufferHandle_t xStreamBuffer ) PRIVILEGED_FUNCTION;

/**
 * stream_buffer.h
 *
<pre>
size_t xStreamBufferBytesAvailable( StreamBufferHandle_t xStreamBuffer );
</pre>
 *
 * 查询流缓冲区以查看它包含多少数
 * 据，该数据等于在流缓冲区为空之
 * 前可以从流缓冲区读取的字节数。
 *
 * @param xStreamBuffer 正在查询的流缓冲区的句柄。
 *
 * @return 之前可以从流缓冲区读取的字节数
 * 流缓冲区将为空。
 *
 * \defgroup xStreamBufferBytesAvailable xStreamBufferBytesAvailable
 * \ingroup StreamBufferManagement
 */
size_t xStreamBufferBytesAvailable( StreamBufferHandle_t xStreamBuffer ) PRIVILEGED_FUNCTION;

/**
 * stream_buffer.h
 *
<pre>
BaseType_t xStreamBufferSetTriggerLevel( StreamBufferHandle_t xStreamBuffer, size_t xTriggerLevel );
</pre>
 *
 * 流缓冲区的触发级别是在流缓冲区上阻塞以等待数
 * 据移出阻塞状态的任务之前流缓冲区中必须存在的
 * 字节数。  例如，如果任务在读取触发级别为
 * 1 的空流缓冲区时被阻塞，则当向缓冲区写入单
 * 个字节或任务的阻塞时间到期时，该任务将被解除
 * 阻塞。  作为另一个示例，如果任务在读取触发
 * 级别为 10 的空流缓冲区时被阻塞，则该任务
 * 将不会被解除阻塞，直到流缓冲区包含至少 10
 * 个字节或任务的阻塞时间到期。  如果读取任
 * 务的阻塞时间在达到触发级别之前到期，则该任务
 * 仍将接收实际可用的字节数。  将触发电平设置
 * 为 0 将导致使用触发电平 1。  指定大于
 * 缓冲区大小的触发级别无效。
 *
 * 触发级别在创建流缓冲区时设置，并且可以使用
 * xStreamBufferSetTriggerLevel() 进行修改。
 *
 * @param xStreamBuffer 正在更新的流缓冲区的句柄。
 *
 * @param xTriggerLevel 流缓冲区的新触发级别。
 *
 * @return 如果 xTriggerLevel 小于或等于流缓冲区的长度
 * 那么触发电平将被更新并返回
 * pdTRUE。  否则返回 pdFALSE。
 *
 * \defgroup xStreamBufferSetTriggerLevel xStreamBufferSetTriggerLevel
 * \ingroup StreamBufferManagement
 */
BaseType_t xStreamBufferSetTriggerLevel( StreamBufferHandle_t xStreamBuffer, size_t xTriggerLevel ) PRIVILEGED_FUNCTION;

/**
 * stream_buffer.h
 *
<pre>
BaseType_t xStreamBufferSendCompletedFromISR( StreamBufferHandle_t xStreamBuffer, BaseType_t *pxHigherPriorityTaskWoken );
</pre>
 *
 * 仅适用于高级用户。
 *
 * 当数据发送到消息缓冲区或流缓冲区时，从 FreeRTOS APIs 内部调
 * 用 sbSEND_COMPLETED() 宏。  如果有一个任务被阻塞在消
 * 息或流缓冲区上等待数据到达，则 sbSEND_COMPLETED() 宏会
 * 向该任务发送通知以将其从阻塞状态中删除。
 * xStreamBufferSendCompletedFromISR() 做
 * 同样的事情。  提供它是为了使应用程序编写者能够实现自己版本的
 * sbSEND_COMPLETED() 并且不得在任何其他时间使用。
 *
 * 有关更多信息，请参阅
 * FreeRTOS/Demo/Minimal/MessageBufferAMP.c 中实现的示例。
 *
 * @param xStreamBuffer 数据所在的流缓冲区的句柄
 * 书面。
 *
 * @param pxHigherPriorityTaskWoken *pxHigherPriorityTaskWoken 应该是
 * 在传递给
 * xStreamBufferSendCompletedFromISR()
 * 之前初始化为 pdFALSE。  如果调用
 * xStreamBufferSendCompletedFromISR()
 * 将任务从阻塞状态中删除，并且该任务的优先级高于当前正在运行的任务
 * 的优先级，则 *pxHigherPriorityTaskWoken 将设置为 pdTRUE，指示在退出 ISR 之前应执行上下文切换。
 *
 * @return 如果任务已从阻塞状态中删除，则返回 pdTRUE。
 * 否则返回 pdFALSE。
 *
 * \defgroup xStreamBufferSendCompletedFromISR xStreamBufferSendCompletedFromISR
 * \ingroup StreamBufferManagement
 */
BaseType_t xStreamBufferSendCompletedFromISR( StreamBufferHandle_t xStreamBuffer, BaseType_t *pxHigherPriorityTaskWoken ) PRIVILEGED_FUNCTION;

/**
 * stream_buffer.h
 *
<pre>
BaseType_t xStreamBufferReceiveCompletedFromISR( StreamBufferHandle_t xStreamBuffer, BaseType_t *pxHigherPriorityTaskWoken );
</pre>
 *
 * 仅适用于高级用户。
 *
 * 当从消息缓冲区或流缓冲区读取数据时，从 FreeRTOS APIs
 * 内部调用 sbRECEIVE_COMPLETED() 宏。  如果有
 * 一个任务被阻塞在消息或流缓冲区上等待数据到达，则
 * sbRECEIVE_COMPLETED() 宏会向该任务发送通知以将
 * 其从阻塞状态中删除。
 * xStreamBufferReceiveCompletedFromISR()
 * 做同样的事情。  提供它是为了使应用程序编写者能够实现自己版本的
 * sbRECEIVE_COMPLETED() 并且不得在任何其他时间使用。
 *
 * 有关更多信息，请参阅
 * FreeRTOS/Demo/Minimal/MessageBufferAMP.c 中实现的示例。
 *
 * @param xStreamBuffer 数据来自的流缓冲区的句柄
 * 读。
 *
 * @param pxHigherPriorityTaskWoken *pxHigherPriorityTaskWoken 应该是
 * 在传递给
 * xStreamBufferReceiveCompletedFromISR()
 * 之前初始化为 pdFALSE。  如果调用
 * xStreamBufferReceiveCompletedFromISR()
 * 将任务从阻塞状态中删除，并且该任务的优先级高于当前正在运行的任务的
 * 优先级，则 *pxHigherPriorityTaskWoken 将设置为 pdTRUE，指示在退出 ISR 之前应执行上下文切换。
 *
 * @return 如果任务已从阻塞状态中删除，则返回 pdTRUE。
 * 否则返回 pdFALSE。
 *
 * \defgroup xStreamBufferReceiveCompletedFromISR xStreamBufferReceiveCompletedFromISR
 * \ingroup StreamBufferManagement
 */
BaseType_t xStreamBufferReceiveCompletedFromISR( StreamBufferHandle_t xStreamBuffer, BaseType_t *pxHigherPriorityTaskWoken ) PRIVILEGED_FUNCTION;

/* 这里下面的函数不属于公共API的一部分。 */
StreamBufferHandle_t xStreamBufferGenericCreate( size_t xBufferSizeBytes,
												 size_t xTriggerLevelBytes,
												 BaseType_t xIsMessageBuffer ) PRIVILEGED_FUNCTION;

StreamBufferHandle_t xStreamBufferGenericCreateStatic( size_t xBufferSizeBytes,
													   size_t xTriggerLevelBytes,
													   BaseType_t xIsMessageBuffer,
													   uint8_t * const pucStreamBufferStorageArea,
													   StaticStreamBuffer_t * const pxStaticStreamBuffer ) PRIVILEGED_FUNCTION;

size_t xStreamBufferNextMessageLengthBytes( StreamBufferHandle_t xStreamBuffer ) PRIVILEGED_FUNCTION;

#if( configUSE_TRACE_FACILITY == 1 )
	void vStreamBufferSetStreamBufferNumber( StreamBufferHandle_t xStreamBuffer, UBaseType_t uxStreamBufferNumber ) PRIVILEGED_FUNCTION;
	UBaseType_t uxStreamBufferGetStreamBufferNumber( StreamBufferHandle_t xStreamBuffer ) PRIVILEGED_FUNCTION;
	uint8_t ucStreamBufferGetStreamBufferType( StreamBufferHandle_t xStreamBuffer ) PRIVILEGED_FUNCTION;
#endif

#if defined( __cplusplus )
}
#endif

#endif	/* !defined( STREAM_BUFFER_H ) */
