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
 * 消息缓冲区在 FreeRTOS 流缓冲区之
 * 上构建功能。流缓冲区用于将连续数据流从一个
 * 任务或中断发送到另一任务，而消息缓冲区则用
 * 于将可变长度离散消息从一个任务或中断发送到
 * 另一任务或中断。  它们的实现重量轻，特别
 * 适合任务中断和核心到核心通信场景。
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
 * xMessageBufferSend()），并将发送块时
 * 间设置为 0。同样，如果有多个不同的读取器，则应用程序写
 * 入器必须将每次调用放置为读取 API function（
 * 例如xMessageBufferRead()) 在关键部分内并将接收超时设置为 0。
 *
 * 消息缓冲区保存可变长度的消息。  为了实现这一点，当消
 * 息写入消息缓冲区时，还会写入附加的 sizeof(
 * size_t ) 字节来存储消息的 length （通
 * 过 API 函数在内部发生）。  sizeof(
 * size_t ) 在 32 位架构上通常为 4 个字节
 * ，因此在 32 位架构上将 10 字节消息写入消息缓冲
 * 区实际上会将消息缓冲区中的可用空间减少 14
 * bytes （10 字节用于消息，4 字节用于保存消息的长度）。
 */

#ifndef FREERTOS_MESSAGE_BUFFER_H
#define FREERTOS_MESSAGE_BUFFER_H

#ifndef INC_FREERTOS_H
	#error "include FreeRTOS.h must appear in source files before include message_buffer.h"
#endif

/* 消息缓冲区构建在流缓冲区之上。 */
#include "stream_buffer.h"

#if defined( __cplusplus )
extern "C" {
#endif

/**
 * 引用消息缓冲区的类型。  例如，调用 xMessageBufferCreate() 返回
 * 一个 MessageBufferHandle_t 变量，该变量随后可以用作
 * xMessageBufferSend()、xMessageBufferReceive() 的参数，
 * ETC。
 */
typedef void * MessageBufferHandle_t;

/*-----------------------------------------------------------*/

/**
 * message_buffer.h
 *
<pre>
MessageBufferHandle_t xMessageBufferCreate( size_t xBufferSizeBytes );
</pre>
 *
 * 使用动态分配的内存创建新的消息缓冲区。  请参阅
 * xMessageBufferCreateStatic()
 * 了解使用静态分配的 memory（在编译时分配的内存）的版本。
 *
 * configSUPPORT_DYNAMIC_ALLOCATION 必须设置为 1 或在
 * FreeRTOSConfig.h 中保持未定义，xMessageBufferCreate() 才可用。
 *
 * @param xBufferSizeBytes bytes（不是消息）消息总数
 * 缓冲区将能够在任何一个时间保存。  当消息写入消息缓冲区时
 * ，还会写入附加的 sizeof( size_t ) 字节来
 * 存储消息的长度。  sizeof( size_t ) 在
 * 32 位架构上通常为 4 字节，因此在大多数 32 位架构
 * 上，10 字节消息将占用 14 字节消息缓冲区空间。
 *
 * @return 如果返回NULL，则无法创建消息缓冲区
 * 因为没有足够的堆内存可供 FreeRTOS
 * 分配消息缓冲区数据结构和存储区域。  返
 * 回的非 NULL 值表示消息缓冲区已成功创
 * 建 - 返回的值应存储为所创建消息的句柄
 * 缓冲。
 *
 * 使用示例：
<pre>

void vAFunction( void )
{
MessageBufferHandle_t xMessageBuffer;
const size_t xMessageBufferSizeBytes = 100;

    //创建可容纳 100 字节的消息缓冲区。
    //用于保存消息缓冲区结构和消息本身的内存是动
    //态分配的。  添加到缓冲区的每条消息都会消
    //耗额外的 4 个字节，用于保存消息的长度。
    xMessageBuffer = xMessageBufferCreate( xMessageBufferSizeBytes );

    if( xMessageBuffer == NULL )
    {
        //没有足够的堆内存空间可
        //用于创建消息缓冲区。
    }
    else
    {
        //消息缓冲区已成功创建，现在可以使用。
    }

</pre>
 * \defgroup xMessageBufferCreate xMessageBufferCreate
 * \ingroup MessageBufferManagement
 */
#define xMessageBufferCreate( xBufferSizeBytes ) ( MessageBufferHandle_t ) xStreamBufferGenericCreate( xBufferSizeBytes, ( size_t ) 0, pdTRUE )

/**
 * message_buffer.h
 *
<pre>
MessageBufferHandle_t xMessageBufferCreateStatic( size_t xBufferSizeBytes,
                                                  uint8_t *pucMessageBufferStorageArea,
                                                  StaticMessageBuffer_t *pxStaticMessageBuffer );
</pre>
 * 使用静态分配的内存创建新的消息缓冲区。  请参阅
 * xMessageBufferCreate() 了解使用动态分配内存的版本。
 *
 * @param xBufferSizeBytes 所指向的缓冲区的大小（以字节为单位）
 * pucMessageBufferStorageArea参数。  当消息
 * 写入消息缓冲区时，还会写入附加的 sizeof( size_t ) 字
 * 节来存储消息的长度。  sizeof( size_t ) 在 32 位
 * 架构上通常为 4 字节，因此在大多数 32 位架构上，10 字节消息将
 * 占用 14 字节消息缓冲区空间。  消息缓冲区中可存储的最大字节数为
 * actually (xBufferSizeBytes - 1)。
 *
 * @param pucMessageBufferStorageArea 必须指向位于以下位置的 uint8_t 数组
 * 至少 xBufferSizeBytes + 1 大。
 * 这是将消息写入消息缓冲区时将消息复制到的数组。
 *
 * @param pxStaticMessageBuffer 必须指向类型的变量
 * StaticMessageBuffer_t，用于保存消息缓冲区的数据
 * 结构。
 *
 * @return 如果消息缓冲区创建成功，则消息缓冲区的句柄
 * 返回创建的消息缓冲区。如果 pucMessageBufferStorageArea
 * 或 pxStaticmessageBuffer 是 NULL，则返回 NULL。
 *
 * 使用示例：
<pre>

//用于指定用于保存消息的数组的尺寸。  可
//用空间实际上会比这个少 1，即 999。
#define STORAGE_SIZE_BYTES 1000

//定义实际保存消息中消息的内存
//buffer.
static uint8_t ucStorageBuffer[ STORAGE_SIZE_BYTES ];

//该变量用于保存消息缓冲区结构。
StaticMessageBuffer_t xMessageBufferStruct;

void MyFunction( void )
{
MessageBufferHandle_t xMessageBuffer;

    xMessageBuffer = xMessageBufferCreateStatic( sizeof( ucBufferStorage ),
                                                 ucBufferStorage,
                                                 &xMessageBufferStruct );

    //由于 pucMessageBufferStorageArea 或
    //pxStaticMessageBuffer 参数都不是 NULL，因此
    //xMessageBuffer 不会是 NULL，并且可用于在其他消息缓冲区 API 调用中引用创建的消息缓冲区。

    //使用消息缓冲区的其他代码可以放在此处。
}

</pre>
 * \defgroup xMessageBufferCreateStatic xMessageBufferCreateStatic
 * \ingroup MessageBufferManagement
 */
#define xMessageBufferCreateStatic( xBufferSizeBytes, pucMessageBufferStorageArea, pxStaticMessageBuffer ) ( MessageBufferHandle_t ) xStreamBufferGenericCreateStatic( xBufferSizeBytes, 0, pdTRUE, pucMessageBufferStorageArea, pxStaticMessageBuffer )

/**
 * message_buffer.h
 *
<pre>
size_t xMessageBufferSend( MessageBufferHandle_t xMessageBuffer,
                           const void *pvTxData,
                           size_t xDataLengthBytes,
                           TickType_t xTicksToWait );
<pre>
 *
 * Sends a discrete message to the message buffer.  The message can be any
 * length that fits within the buffer's free space, and is copied into the
 * 缓冲。
 *
 * ***NOTE***:  Uniquely among FreeRTOS objects, the stream buffer
 * implementation (so also the message buffer implementation, as message buffers
 * are built on top of stream buffers) assumes there is only one task or
 * interrupt that will write to the buffer (the writer), and only one task or
 * interrupt that will read from the buffer (the reader).  It is safe for the
 * writer and reader to be different tasks or interrupts, but, unlike other
 * FreeRTOS objects, it is not safe to have multiple different writers or
 * multiple different readers.  If there are to be multiple different writers
 * then the application writer must place each call to a writing API function
 * (such as xMessageBufferSend()) inside a critical section and set the send
 * block time to 0.  Likewise, if there are to be multiple different readers
 * then the application writer must place each call to a reading API function
 * (such as xMessageBufferRead()) inside a critical section and set the receive
 * block time to 0.
 *
 * Use xMessageBufferSend() to write to a message buffer from a task.  Use
 * xMessageBufferSendFromISR() to write to a message buffer from an interrupt
 * service routine (ISR).
 *
 * @param xMessageBuffer The handle of the message buffer to which a message is
 * being sent.
 *
 * @param pvTxData A pointer to the message that is to be copied into the
 * message buffer.
 *
 * @param xDataLengthBytes The length of the message.  That is, the number of
 * bytes to copy from pvTxData into the message buffer.  When a message is
 * written to the message buffer an additional sizeof( size_t ) bytes are also
 * written to store the message's length.  sizeof( size_t ) is typically 4 bytes
 * on a 32-bit architecture, so on most 32-bit architecture setting
 * xDataLengthBytes to 20 will reduce the free space in the message buffer by 24
 * bytes (20 bytes of message data and 4 bytes to hold the message length).
 *
 * @param xTicksToWait The maximum amount of time the calling task should remain
 * in the Blocked state to wait for enough space to become available in the
 * message buffer, should the message buffer have insufficient space when
 * xMessageBufferSend() is called.  The calling task will never block if
 * xTicksToWait is zero.  The block time is specified in tick periods, so the
 * absolute time it represents is dependent on the tick frequency.  The macro
 * pdMS_TO_TICKS() can be used to convert a time specified in milliseconds into
 * a time specified in ticks.  Setting xTicksToWait to portMAX_DELAY will cause
 * the task to wait indefinitely (without timing out), provided
 * INCLUDE_vTaskSuspend is set to 1 in FreeRTOSConfig.h.  Tasks do not use any
 * CPU time when they are in the Blocked state.
 *
 * @return The number of bytes written to the message buffer.  If the call to
 * xMessageBufferSend() times out before there was enough space to write the
 * message into the message buffer then zero is returned.  If the call did not
 * time out then xDataLengthBytes is returned.
 *
 * Example use:
<pre>
void vAFunction( MessageBufferHandle_t xMessageBuffer )
{
size_t xBytesSent;
uint8_t ucArrayToSend[] = { 0, 1, 2, 3 };
char *pcStringToSend = "String to send";
const TickType_t x100ms = pdMS_TO_TICKS( 100 );

    //将数组发送到消息缓冲区，最多阻塞 100
    //毫秒，以等待消息缓冲区中有足够的可用空间。
    xBytesSent = xMessageBufferSend( xMessageBuffer, ( void * ) ucArrayToSend, sizeof( ucArrayToSend ), x100ms );

    if( xBytesSent != sizeof( ucArrayToSend ) )
    {
        //在缓冲区中有足够的空间用于写入数据之前，对
        //xMessageBufferSend() 的调用超时。
    }

    //将字符串发送到消息缓冲区。  如
    //果缓冲区空间不足，则立即返回。
    xBytesSent = xMessageBufferSend( xMessageBuffer, ( void * ) pcStringToSend, strlen( pcStringToSend ), 0 );

    if( xBytesSent != strlen( pcStringToSend ) )
    {
        //无法将字符串添加到消息缓冲区，因
        //为缓冲区中没有足够的可用空间。
    }
}
</pre>
 * \defgroup xMessageBufferSend xMessageBufferSend
 * \ingroup MessageBufferManagement
 */
#define xMessageBufferSend( xMessageBuffer, pvTxData, xDataLengthBytes, xTicksToWait ) xStreamBufferSend( ( StreamBufferHandle_t ) xMessageBuffer, pvTxData, xDataLengthBytes, xTicksToWait )

/**
 * message_buffer.h
 *
<pre>
size_t xMessageBufferSendFromISR( MessageBufferHandle_t xMessageBuffer,
                                  const void *pvTxData,
                                  size_t xDataLengthBytes,
                                  BaseType_t *pxHigherPriorityTaskWoken );
<pre>
 *
 * Interrupt safe version of the API function that sends a discrete message to
 * the message buffer.  The message can be any length that fits within the
 * buffer's free space, and is copied into the buffer.
 *
 * ***NOTE***:  Uniquely among FreeRTOS objects, the stream buffer
 * implementation (so also the message buffer implementation, as message buffers
 * are built on top of stream buffers) assumes there is only one task or
 * interrupt that will write to the buffer (the writer), and only one task or
 * interrupt that will read from the buffer (the reader).  It is safe for the
 * writer and reader to be different tasks or interrupts, but, unlike other
 * FreeRTOS objects, it is not safe to have multiple different writers or
 * multiple different readers.  If there are to be multiple different writers
 * then the application writer must place each call to a writing API function
 * (such as xMessageBufferSend()) inside a critical section and set the send
 * block time to 0.  Likewise, if there are to be multiple different readers
 * then the application writer must place each call to a reading API function
 * (such as xMessageBufferRead()) inside a critical section and set the receive
 * block time to 0.
 *
 * Use xMessageBufferSend() to write to a message buffer from a task.  Use
 * xMessageBufferSendFromISR() to write to a message buffer from an interrupt
 * service routine (ISR).
 *
 * @param xMessageBuffer The handle of the message buffer to which a message is
 * being sent.
 *
 * @param pvTxData A pointer to the message that is to be copied into the
 * message buffer.
 *
 * @param xDataLengthBytes The length of the message.  That is, the number of
 * bytes to copy from pvTxData into the message buffer.  When a message is
 * written to the message buffer an additional sizeof( size_t ) bytes are also
 * written to store the message's length.  sizeof( size_t ) is typically 4 bytes
 * on a 32-bit architecture, so on most 32-bit architecture setting
 * xDataLengthBytes to 20 will reduce the free space in the message buffer by 24
 * bytes (20 bytes of message data and 4 bytes to hold the message length).
 *
 * @param pxHigherPriorityTaskWoken  It is possible that a message buffer will
 * have a task blocked on it waiting for data.  Calling
 * xMessageBufferSendFromISR() can make data available, and so cause a task that
 * was waiting for data to leave the Blocked state.  If calling
 * xMessageBufferSendFromISR() causes a task to leave the Blocked state, and the
 * unblocked task has a priority higher than the currently executing task (the
 * task that was interrupted), then, internally, xMessageBufferSendFromISR()
 * will set *pxHigherPriorityTaskWoken to pdTRUE.  If
 * xMessageBufferSendFromISR() sets this value to pdTRUE, then normally a
 * context switch should be performed before the interrupt is exited.  This will
 * ensure that the interrupt returns directly to the highest priority Ready
 * state task.  *pxHigherPriorityTaskWoken should be set to pdFALSE before it
 * is passed into the function.  See the code example below for an example.
 *
 * @return The number of bytes actually written to the message buffer.  If the
 * message buffer didn't have enough free space for the message to be stored
 * then 0 is returned, otherwise xDataLengthBytes is returned.
 *
 * Example use:
<pre>
//已经创建的消息缓冲区。
MessageBufferHandle_t xMessageBuffer;

void vAnInterruptServiceRoutine( void )
{
size_t xBytesSent;
char *pcStringToSend = "String to send";
BaseType_t xHigherPriorityTaskWoken = pdFALSE; // Initialised to pdFALSE.

    //尝试将字符串发送到消息缓冲区。
    xBytesSent = xMessageBufferSendFromISR( xMessageBuffer,
                                            ( void * ) pcStringToSend,
                                            strlen( pcStringToSend ),
                                            &xHigherPriorityTaskWoken );

    if( xBytesSent != strlen( pcStringToSend ) )
    {
        //无法将字符串添加到消息缓冲区，因
        //为缓冲区中没有足够的可用空间。
    }

    //如果 xHigherPriorityTaskWoken 在
    //xMessageBufferSendFromISR() 中设置为
    //pdTRUE，则优先级高于当前执行任务优先级的任务将被解锁，并且应执
    //行上下文切换以确保 ISR 返回到解锁任务。  在大多数
    //FreeRTOS 端口中，只需将
    //xHigherPriorityTaskWoken 传递到
    //portYIELD_FROM_ISR() 即可完成，
    //portYIELD_FROM_ISR() 将测试变量值，并在必要时执行上下文切换。  检查正在使用的端口的文档以获取特定于端口的说明。
    portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
}
</pre>
 * \defgroup xMessageBufferSendFromISR xMessageBufferSendFromISR
 * \ingroup MessageBufferManagement
 */
#define xMessageBufferSendFromISR( xMessageBuffer, pvTxData, xDataLengthBytes, pxHigherPriorityTaskWoken ) xStreamBufferSendFromISR( ( StreamBufferHandle_t ) xMessageBuffer, pvTxData, xDataLengthBytes, pxHigherPriorityTaskWoken )

/**
 * message_buffer.h
 *
<pre>
size_t xMessageBufferReceive( MessageBufferHandle_t xMessageBuffer,
                              void *pvRxData,
                              size_t xBufferLengthBytes,
                              TickType_t xTicksToWait );
</pre>
 *
 * 从消息缓冲区接收离散消息。  消息的长度可
 * 以是可变的，并且可以从缓冲区中复制出来。
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
 * xMessageBufferSend()），并将发送块时
 * 间设置为 0。同样，如果有多个不同的读取器，则应用程序写
 * 入器必须将每次调用放置为读取 API function（
 * 例如xMessageBufferRead()) 在关键部分内并将接收块时间设置为 0。
 *
 * 使用 xMessageBufferReceive() 从任务的消息缓冲
 * 区中读取。  使用
 * xMessageBufferReceiveFromISR() 从中断服务 routine (ISR) 中读取消息缓冲区。
 *
 * @param xMessageBuffer 消息缓冲区的句柄
 * 正在接收中。
 *
 * @param pvRxData 指向接收到的消息所在缓冲区的指针
 * 被复制。
 *
 * @param xBufferLengthBytes pvRxData 指向的缓冲区长度
 * 参数。  这设置了可以接收的消息的最大长度。如果
 * xBufferLengthBytes 太小而无法容纳
 * 下一条消息，则该消息将留在消息缓冲区中并返回 0。
 *
 * @param xTicksToWait 任务应在任务中保留的最长时间
 * 如果消息缓冲区为空，则处于阻塞状态以等待消息。如果
 * xTicksToWait 为零并且消息缓冲区为空，
 * xMessageBufferReceive() 将立即返回。  区块
 * 时间以滴答周期为单位指定，因此它表示的绝对时间取决于滴答频率。  宏
 * pdMS_TO_TICKS() 可用于将以毫秒为单位的时间转换为以
 * 刻度为单位的时间。  将 xTicksToWait 设置为
 * portMAX_DELAY 将导致任务等待 indefinitely
 * （不会超时），前提是 FreeRTOSConfig.h 中的
 * INCLUDE_vTaskSuspend 设置为 1。  任务处于阻塞状态时不使用任何 CPU 时间。
 *
 * @return 从消息缓冲区读取的消息的长度（以字节为单位），如果
 * 任何。  如果
 * xMessageBufferReceive() 在
 * 消息可用之前超时，则返回零。  如果消息的长度大于
 * xBufferLengthBytes，则该消息将保留在消息缓冲区中并返回零。
 *
 * 使用示例：
<pre>
void vAFunction( MessageBuffer_t xMessageBuffer )
{
uint8_t ucRxData[ 20 ];
size_t xReceivedBytes;
const TickType_t xBlockTime = pdMS_TO_TICKS( 20 );

    //从消息缓冲区接收下一条消息。  在阻塞的
    //state 中等待（因此不使用任何 CPU 处
    //理时间）最多 100 毫秒，以便消息变得可用。
    xReceivedBytes = xMessageBufferReceive( xMessageBuffer,
                                            ( void * ) ucRxData,
                                            sizeof( ucRxData ),
                                            xBlockTime );

    if( xReceivedBytes > 0 )
    {
        //ucRxData 包含长度为
        //xReceivedBytes 的消息。  在这里处理消息....
    }
}
</pre>
 * \defgroup xMessageBufferReceive xMessageBufferReceive
 * \ingroup MessageBufferManagement
 */
#define xMessageBufferReceive( xMessageBuffer, pvRxData, xBufferLengthBytes, xTicksToWait ) xStreamBufferReceive( ( StreamBufferHandle_t ) xMessageBuffer, pvRxData, xBufferLengthBytes, xTicksToWait )


/**
 * message_buffer.h
 *
<pre>
size_t xMessageBufferReceiveFromISR( MessageBufferHandle_t xMessageBuffer,
                                     void *pvRxData,
                                     size_t xBufferLengthBytes,
                                     BaseType_t *pxHigherPriorityTaskWoken );
</pre>
 *
 * API 函数的中断安全版本，用于从消息
 * 缓冲区接收离散消息。  消息的长度可以
 * 是可变的，并且可以从缓冲区中复制出来。
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
 * xMessageBufferSend()），并将发送块时
 * 间设置为 0。同样，如果有多个不同的读取器，则应用程序写
 * 入器必须将每次调用放置为读取 API function（
 * 例如xMessageBufferRead()) 在关键部分内并将接收块时间设置为 0。
 *
 * 使用 xMessageBufferReceive() 从任务的消息缓冲
 * 区中读取。  使用
 * xMessageBufferReceiveFromISR() 从中断服务 routine (ISR) 中读取消息缓冲区。
 *
 * @param xMessageBuffer 消息缓冲区的句柄
 * 正在接收中。
 *
 * @param pvRxData 指向接收到的消息所在缓冲区的指针
 * 被复制。
 *
 * @param xBufferLengthBytes pvRxData 指向的缓冲区长度
 * 参数。  这设置了可以接收的消息的最大长度。如果
 * xBufferLengthBytes 太小而无法容纳
 * 下一条消息，则该消息将留在消息缓冲区中并返回 0。
 *
 * @param pxHigherPriorityTaskWoken 消息缓冲区可能会
 * 有一个任务被阻塞，等待空间可用。  调用
 * xMessageBufferReceiveFromISR() 可以使
 * 空间可用，从而导致正在等待空间的任务离开阻塞状态。  如果调用
 * xMessageBufferReceiveFromISR() 导致任
 * 务离开阻塞状态，并且未阻塞任务的优先级高于当前正在执行的 task（
 * 被中断的任务），则
 * xMessageBufferReceiveFromISR() 在内部
 * 会将 *pxHigherPriorityTaskWoken 设置为
 * pdTRUE。如果
 * xMessageBufferReceiveFromISR()将此值设
 * 置为pdTRUE，则通常应在退出中断之前执行上下文切换。  这将确保
 * 中断直接返回到最高优先级的就绪状态任务。  *pxHigherPriorityTaskWoken 在传递给函数之前应设置为 pdFALSE。  有关示例，请参阅下面的代码示例。
 *
 * @return 从消息缓冲区读取的消息的长度（以字节为单位），如果
 * 任何。
 *
 * 使用示例：
<pre>
//已经创建的消息缓冲区。
MessageBuffer_t xMessageBuffer;

void vAnInterruptServiceRoutine( void )
{
uint8_t ucRxData[ 20 ];
size_t xReceivedBytes;
BaseType_t xHigherPriorityTaskWoken = pdFALSE;  // Initialised to pdFALSE.

    //从消息缓冲区接收下一条消息。
    xReceivedBytes = xMessageBufferReceiveFromISR( xMessageBuffer,
                                                  ( void * ) ucRxData,
                                                  sizeof( ucRxData ),
                                                  &xHigherPriorityTaskWoken );

    if( xReceivedBytes > 0 )
    {
        //ucRxData 包含长度为
        //xReceivedBytes 的消息。  在这里处理消息....
    }

    //如果 xHigherPriorityTaskWoken 在
    //xMessageBufferReceiveFromISR() 中设置
    //为 pdTRUE，则优先级高于当前执行任务优先级的任务将被解锁，并且
    //应执行上下文切换以确保 ISR 返回到解锁任务。  在大多数
    //FreeRTOS 端口中，只需将
    //xHigherPriorityTaskWoken 传递到
    //portYIELD_FROM_ISR() 即可完成，
    //portYIELD_FROM_ISR() 将测试变量值，并在必要时执行上下文切换。  检查正在使用的端口的文档以获取特定于端口的说明。
    portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
}
</pre>
 * \defgroup xMessageBufferReceiveFromISR xMessageBufferReceiveFromISR
 * \ingroup MessageBufferManagement
 */
#define xMessageBufferReceiveFromISR( xMessageBuffer, pvRxData, xBufferLengthBytes, pxHigherPriorityTaskWoken ) xStreamBufferReceiveFromISR( ( StreamBufferHandle_t ) xMessageBuffer, pvRxData, xBufferLengthBytes, pxHigherPriorityTaskWoken )

/**
 * message_buffer.h
 *
<pre>
void vMessageBufferDelete( MessageBufferHandle_t xMessageBuffer );
</pre>
 *
 * 删除先前使用调用 xMessageBufferCreate() 或
 * xMessageBufferCreateStatic() 创建的
 * 消息缓冲区。  如果消息缓冲区是使用动态memory（即通过
 * xMessageBufferCreate()）创建的，则释放分配的内存。
 *
 * 消息缓冲区被使用后，不得使用消息缓冲区句柄
 * 已删除。
 *
 * @param xMessageBuffer 要删除的消息缓冲区的句柄。
 *
 */
#define vMessageBufferDelete( xMessageBuffer ) vStreamBufferDelete( ( StreamBufferHandle_t ) xMessageBuffer )

/**
 * message_buffer.h
<pre>
BaseType_t xMessageBufferIsFull( MessageBufferHandle_t xMessageBuffer ) );
</pre>
 *
 * 测试消息缓冲区是否已满。  如果消息缓冲区无
 * 法再接受任何大小的消息，则消息缓冲区已满，直
 * 到从消息缓冲区中删除消息而获得可用空间为止。
 *
 * @param xMessageBuffer 正在查询的消息缓冲区的句柄。
 *
 * @return 如果 xMessageBuffer 引用的消息缓冲区已满，则
 * 返回 pdTRUE。  否则返回 pdFALSE。
 */
#define xMessageBufferIsFull( xMessageBuffer ) xStreamBufferIsFull( ( StreamBufferHandle_t ) xMessageBuffer )

/**
 * message_buffer.h
<pre>
BaseType_t xMessageBufferIsEmpty( MessageBufferHandle_t xMessageBuffer ) );
</pre>
 *
 * 测试消息缓冲区是否为 empty（不包含任何消息）。
 *
 * @param xMessageBuffer 正在查询的消息缓冲区的句柄。
 *
 * @return 如果 xMessageBuffer 引用的消息缓冲区为空，则
 * 返回 pdTRUE。  否则返回 pdFALSE。
 *
 */
#define xMessageBufferIsEmpty( xMessageBuffer ) xStreamBufferIsEmpty( ( StreamBufferHandle_t ) xMessageBuffer )

/**
 * message_buffer.h
<pre>
BaseType_t xMessageBufferReset( MessageBufferHandle_t xMessageBuffer );
</pre>
 *
 * 将消息缓冲区重置为其初始空状态，丢弃其中的任何消息
 * 包含。
 *
 * 仅当消息缓冲区上没有任何任务被阻塞时才能重置消息缓冲区。
 *
 * @param xMessageBuffer 正在重置的消息缓冲区的句柄。
 *
 * @return 如果消息缓冲区被重置，则返回 pdPASS。  如果
 * 消息缓冲区无法重置，因为消息队列上有
 * 一个任务被阻塞以等待空间可用，或者等
 * 待消息可用，然后返回 pdFAIL。
 *
 * \defgroup xMessageBufferReset xMessageBufferReset
 * \ingroup MessageBufferManagement
 */
#define xMessageBufferReset( xMessageBuffer ) xStreamBufferReset( ( StreamBufferHandle_t ) xMessageBuffer )


/**
 * message_buffer.h
<pre>
size_t xMessageBufferSpaceAvailable( MessageBufferHandle_t xMessageBuffer ) );
</pre>
 * 返回消息缓冲区中可用空间的字节数。
 *
 * @param xMessageBuffer 正在查询的消息缓冲区的句柄。
 *
 * @return 之前可以写入消息缓冲区的字节数
 * 消息缓冲区将满。  当消息写入消息缓冲区时，还会写入附加的
 * sizeof( size_t ) 字节来存储消息的长度。
 * sizeof( size_t ) 在 32 位架构上通常为 4 字
 * 节，因此如果
 * xMessageBufferSpacesAvailable() 返回 10，则可写入消息缓冲区的最大消息大小为 6 字节。
 *
 * \defgroup xMessageBufferSpaceAvailable xMessageBufferSpaceAvailable
 * \ingroup MessageBufferManagement
 */
#define xMessageBufferSpaceAvailable( xMessageBuffer ) xStreamBufferSpacesAvailable( ( StreamBufferHandle_t ) xMessageBuffer )
#define xMessageBufferSpacesAvailable( xMessageBuffer ) xStreamBufferSpacesAvailable( ( StreamBufferHandle_t ) xMessageBuffer ) /* 更正原始宏名称中的拼写错误。 */

/**
 * message_buffer.h
 <pre>
 size_t xMessageBufferNextLengthBytes( MessageBufferHandle_t xMessageBuffer ) );
 </pre>
 * 返回消息缓冲区中下一条消息的 length（以字节为单位）。如果
 * xMessageBufferReceive() 返回 0，则很有用，因为传递到
 * xMessageBufferReceive() 的缓冲区大小太小，无法容纳下一条消息。
 *
 * @param xMessageBuffer 正在查询的消息缓冲区的句柄。
 *
 * @return 消息缓冲区中下一条消息的 length（以字节为单位），或 0
 * if the message buffer is empty.
 *
 * \defgroup xMessageBufferNextLengthBytes xMessageBufferNextLengthBytes
 * \ingroup MessageBufferManagement
 */
#define xMessageBufferNextLengthBytes( xMessageBuffer ) xStreamBufferNextMessageLengthBytes( ( StreamBufferHandle_t ) xMessageBuffer ) PRIVILEGED_FUNCTION;

/**
 * message_buffer.h
 *
<pre>
BaseType_t xMessageBufferSendCompletedFromISR( MessageBufferHandle_t xStreamBuffer, BaseType_t *pxHigherPriorityTaskWoken );
</pre>
 *
 * 仅适用于高级用户。
 *
 * 当数据发送到消息缓冲区或流缓冲区时，从 FreeRTOS APIs 内部调
 * 用 sbSEND_COMPLETED() 宏。  如果有一个任务被阻塞在消
 * 息或流缓冲区上等待数据到达，则 sbSEND_COMPLETED() 宏会
 * 向该任务发送通知以将其从阻塞状态中删除。
 * xMessageBufferSendCompletedFromISR()
 * 做同样的事情。  提供它是为了使应用程序编写者能够实现自己版本的
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
 * xMessageBufferSendCompletedFromISR()
 * 之前初始化为 pdFALSE。  如果调用
 * xMessageBufferSendCompletedFromISR()
 * 将任务从阻塞状态中删除，并且该任务的优先级高于当前正在运行的任务的
 * 优先级，则 *pxHigherPriorityTaskWoken 将设置为 pdTRUE，指示在退出 ISR 之前应执行上下文切换。
 *
 * @return 如果任务已从阻塞状态中删除，则返回 pdTRUE。
 * 否则返回 pdFALSE。
 *
 * \defgroup xMessageBufferSendCompletedFromISR xMessageBufferSendCompletedFromISR
 * \ingroup StreamBufferManagement
 */
#define xMessageBufferSendCompletedFromISR( xMessageBuffer, pxHigherPriorityTaskWoken ) xStreamBufferSendCompletedFromISR( ( StreamBufferHandle_t ) xMessageBuffer, pxHigherPriorityTaskWoken )

/**
 * message_buffer.h
 *
<pre>
BaseType_t xMessageBufferReceiveCompletedFromISR( MessageBufferHandle_t xStreamBuffer, BaseType_t *pxHigherPriorityTaskWoken );
</pre>
 *
 * 仅适用于高级用户。
 *
 * 当从消息缓冲区或流缓冲区读取数据时，从 FreeRTOS APIs
 * 内部调用 sbRECEIVE_COMPLETED() 宏。  如果有
 * 一个任务被阻塞在消息或流缓冲区上等待数据到达，则
 * sbRECEIVE_COMPLETED() 宏会向该任务发送通知以将
 * 其从阻塞状态中删除。
 * xMessageBufferReceiveCompletedFromISR()
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
 * xMessageBufferReceiveCompletedFromISR()
 * 之前初始化为 pdFALSE。  如果调用
 * xMessageBufferReceiveCompletedFromISR()
 * 将任务从阻塞状态中删除，并且该任务的优先级高于当前正在运行的任务的优
 * 先级，则 *pxHigherPriorityTaskWoken 将设置为 pdTRUE，指示在退出 ISR 之前应执行上下文切换。
 *
 * @return 如果任务已从阻塞状态中删除，则返回 pdTRUE。
 * 否则返回 pdFALSE。
 *
 * \defgroup xMessageBufferReceiveCompletedFromISR xMessageBufferReceiveCompletedFromISR
 * \ingroup StreamBufferManagement
 */
#define xMessageBufferReceiveCompletedFromISR( xMessageBuffer, pxHigherPriorityTaskWoken ) xStreamBufferReceiveCompletedFromISR( ( StreamBufferHandle_t ) xMessageBuffer, pxHigherPriorityTaskWoken )

#if defined( __cplusplus )
} /* 外部“C” */
#endif

#endif	/* !defined( FREERTOS_MESSAGE_BUFFER_H ) */
