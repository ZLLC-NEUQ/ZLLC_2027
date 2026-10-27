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

/* 标准头文件。 */
#include <stdint.h>
#include <string.h>

/* 定义
MPU_WRAPPERS_INCLUDED_FROM_API_FILE
可防止 task.h 重新定义所有 API 函数以使用 MPU 包装器。  仅当应用程序文件包含 task.h 时才应执行此操作。 */
#define MPU_WRAPPERS_INCLUDED_FROM_API_FILE

/* FreeRTOS 头文件。 */
#include "FreeRTOS.h"
#include "task.h"
#include "stream_buffer.h"

#if( configUSE_TASK_NOTIFICATIONS != 1 )
	#error configUSE_TASK_NOTIFICATIONS must be set to 1 to build stream_buffer.c
#endif

/* Lint e961、e9021 和 e750 被抑制为 MISRA 异常，因为 MPU
端口需要定义 MPU_WRAPPERS_INCLUDED_FROM_API_FILE
为生成正确的特权与非特权链接和代码布局，上述头文件中需要定义该宏，
正确的特权与非特权链接和放置。 */
#undef MPU_WRAPPERS_INCLUDED_FROM_API_FILE /*lint！e961！e750！e9021。 */

/* 如果用户没有提供应用程序特定的 Rx
通知宏，或者 #define 通知宏，
它们会提供使用任务通知的默认实现。 */
/*lint -save -e9026 此处允许和需要类似宏的功能，以便可以覆盖它们。 */
#ifndef sbRECEIVE_COMPLETED
	#define sbRECEIVE_COMPLETED( pxStreamBuffer )										\
		vTaskSuspendAll();																\
		{																				\
			if( ( pxStreamBuffer )->xTaskWaitingToSend != NULL )						\
			{																			\
				( void ) xTaskNotify( ( pxStreamBuffer )->xTaskWaitingToSend,			\
									  ( uint32_t ) 0,									\
									  eNoAction );										\
				( pxStreamBuffer )->xTaskWaitingToSend = NULL;							\
			}																			\
		}																				\
		( void ) xTaskResumeAll();
#endif /* sbRECEIVE_COMPLETED */

#ifndef sbRECEIVE_COMPLETED_FROM_ISR
	#define sbRECEIVE_COMPLETED_FROM_ISR( pxStreamBuffer,								\
										  pxHigherPriorityTaskWoken )					\
	{																					\
	UBaseType_t uxSavedInterruptStatus;													\
																						\
		uxSavedInterruptStatus = ( UBaseType_t ) portSET_INTERRUPT_MASK_FROM_ISR();		\
		{																				\
			if( ( pxStreamBuffer )->xTaskWaitingToSend != NULL )						\
			{																			\
				( void ) xTaskNotifyFromISR( ( pxStreamBuffer )->xTaskWaitingToSend,	\
											 ( uint32_t ) 0,							\
											 eNoAction,									\
											 pxHigherPriorityTaskWoken );				\
				( pxStreamBuffer )->xTaskWaitingToSend = NULL;							\
			}																			\
		}																				\
		portCLEAR_INTERRUPT_MASK_FROM_ISR( uxSavedInterruptStatus );					\
	}
#endif /* sbRECEIVE_COMPLETED_FROM_ISR */

/* 如果用户没有提供特定于应用程序的 Tx
通知宏，或者 #define 通知宏
，则它们会提供使用任务通知的默认实现。 */
#ifndef sbSEND_COMPLETED
	#define sbSEND_COMPLETED( pxStreamBuffer )											\
		vTaskSuspendAll();																\
		{																				\
			if( ( pxStreamBuffer )->xTaskWaitingToReceive != NULL )						\
			{																			\
				( void ) xTaskNotify( ( pxStreamBuffer )->xTaskWaitingToReceive,		\
									  ( uint32_t ) 0,									\
									  eNoAction );										\
				( pxStreamBuffer )->xTaskWaitingToReceive = NULL;						\
			}																			\
		}																				\
		( void ) xTaskResumeAll();
#endif /* sbSEND_COMPLETED */

#ifndef sbSEND_COMPLETE_FROM_ISR
	#define sbSEND_COMPLETE_FROM_ISR( pxStreamBuffer, pxHigherPriorityTaskWoken )		\
	{																					\
	UBaseType_t uxSavedInterruptStatus;													\
																						\
		uxSavedInterruptStatus = ( UBaseType_t ) portSET_INTERRUPT_MASK_FROM_ISR();		\
		{																				\
			if( ( pxStreamBuffer )->xTaskWaitingToReceive != NULL )						\
			{																			\
				( void ) xTaskNotifyFromISR( ( pxStreamBuffer )->xTaskWaitingToReceive,	\
											 ( uint32_t ) 0,							\
											 eNoAction,									\
											 pxHigherPriorityTaskWoken );				\
				( pxStreamBuffer )->xTaskWaitingToReceive = NULL;						\
			}																			\
		}																				\
		portCLEAR_INTERRUPT_MASK_FROM_ISR( uxSavedInterruptStatus );					\
	}
#endif /* sbSEND_COMPLETE_FROM_ISR */
/*lint -restore (9026) */

/* 用于保存缓冲区中消息长度的字节数。 */
#define sbBYTES_TO_STORE_MESSAGE_LENGTH ( sizeof( configMESSAGE_BUFFER_LENGTH_TYPE ) )

/* 存储在流缓冲区的 ucFlags 字段中的位。 */
#define sbFLAGS_IS_MESSAGE_BUFFER		( ( uint8_t ) 1 ) /* 如果流缓冲区被创建为消息缓冲区，则设置，在这种情况下，它保存离散消息而不是流。 */
#define sbFLAGS_IS_STATICALLY_ALLOCATED ( ( uint8_t ) 2 ) /* 设置是否使用静态分配的内存创建流缓冲区。 */

/*-----------------------------------------------------------*/

/* 保存缓冲区状态信息的结构。 */
typedef struct StreamBufferDef_t /*lint !e9058 样式约定使用标签。 */
{
	volatile size_t xTail;				/* 缓冲区内要读取的下一个项目的索引。 */
	volatile size_t xHead;				/* 缓冲区中要写入的下一个项目的索引。 */
	size_t xLength;						/* pucBuffer 指向的缓冲区的长度。 */
	size_t xTriggerLevelBytes;			/* 在等待数据的任务被解除阻塞之前流缓冲区中必须存在的字节数。 */
	volatile TaskHandle_t xTaskWaitingToReceive; /* 持有等待数据的任务的句柄，如果没有任务正在等待，则持有 NULL。 */
	volatile TaskHandle_t xTaskWaitingToSend;	/* 保存等待将数据发送到已满消息缓冲区的任务的句柄。 */
	uint8_t *pucBuffer;					/* 指向缓冲区本身 - 即存储通过缓冲区传递的数据的 RAM。 */
	uint8_t ucFlags;

	#if ( configUSE_TRACE_FACILITY == 1 )
		UBaseType_t uxStreamBufferNumber;		/* 用于追踪目的。 */
	#endif
} StreamBuffer_t;

/*
 * 可从缓冲区读取的字节数。
 */
static size_t prvBytesInBuffer( const StreamBuffer_t * const pxStreamBuffer ) PRIVILEGED_FUNCTION;

/*
 * 将 pucData 中的 xCount 字节添加到
 * pxStreamBuffer 消息缓冲区中。返回写入的字节
 * 数，在成功情况下等于 xCount，如果 buffer 中
 * 没有足够的空间（在这种情况下没有数据写入缓冲区），则返回 0。
 */
static size_t prvWriteBytesToBuffer( StreamBuffer_t * const pxStreamBuffer, const uint8_t *pucData, size_t xCount ) PRIVILEGED_FUNCTION;

/*
 * 如果流缓冲区用作消息缓冲区，则从缓冲区中读
 * 取整个消息。  如果流缓冲区用作流缓冲区，
 * 则从缓冲区读取尽可能多的字节。调用
 * prvReadBytesFromBuffer()
 * 来实际从缓冲区的数据存储区域中提取字节。
 */
static size_t prvReadMessageFromBuffer( StreamBuffer_t *pxStreamBuffer,
										void *pvRxData,
										size_t xBufferLengthBytes,
										size_t xBytesAvailable,
										size_t xBytesToStoreMessageLength ) PRIVILEGED_FUNCTION;

/*
 * 如果流缓冲区用作消息缓冲区，则将整个消息写
 * 入缓冲区。  如果流缓冲区用作流缓冲区，则
 * 将尽可能多的字节写入缓冲区。调用
 * prvWriteBytestoBuffer()
 * 来实际将字节发送到缓冲区的数据存储区域。
 */
static size_t prvWriteMessageToBuffer(  StreamBuffer_t * const pxStreamBuffer,
										const void * pvTxData,
										size_t xDataLengthBytes,
										size_t xSpace,
										size_t xRequiredSpace ) PRIVILEGED_FUNCTION;

/*
 * 从 pxStreamBuffer 消息缓冲区中读取
 * xMaxCount 字节并将其写入 pucData。
 */
static size_t prvReadBytesFromBuffer( StreamBuffer_t *pxStreamBuffer,
									  uint8_t *pucData,
									  size_t xMaxCount,
									  size_t xBytesAvailable ) PRIVILEGED_FUNCTION;

/*
 * 由 pxStreamBufferCreate() 和
 * pxStreamBufferCreateStatic() 调用以初始化新创建的流缓冲区结构的成员。
 */
static void prvInitialiseNewStreamBuffer( StreamBuffer_t * const pxStreamBuffer,
										  uint8_t * const pucBuffer,
										  size_t xBufferSizeBytes,
										  size_t xTriggerLevelBytes,
										  uint8_t ucFlags ) PRIVILEGED_FUNCTION;

/*-----------------------------------------------------------*/

#if( configSUPPORT_DYNAMIC_ALLOCATION == 1 )

	StreamBufferHandle_t xStreamBufferGenericCreate( size_t xBufferSizeBytes, size_t xTriggerLevelBytes, BaseType_t xIsMessageBuffer )
	{
	uint8_t *pucAllocatedMemory;
	uint8_t ucFlags;

		/* 如果流缓冲区将用作消息 buffer（
		也就是说，它将保存带有少量元数据的离散
		消息，说明下一条消息有多大），请检查缓
		冲区是否足够大以容纳至少一条消息。 */
		if( xIsMessageBuffer == pdTRUE )
		{
			/* 是一个消息缓冲区，但不是静态分配的。 */
			ucFlags = sbFLAGS_IS_MESSAGE_BUFFER;
			configASSERT( xBufferSizeBytes > sbBYTES_TO_STORE_MESSAGE_LENGTH );
		}
		else
		{
			/* 不是消息缓冲区，也不是静态分配的。 */
			ucFlags = 0;
			configASSERT( xBufferSizeBytes > 0 );
		}
		configASSERT( xTriggerLevelBytes <= xBufferSizeBytes );

		/* 即使缓冲区为空，触发级别 0
		也会导致等待任务解除阻塞。 */
		if( xTriggerLevelBytes == ( size_t ) 0 )
		{
			xTriggerLevelBytes = ( size_t ) 1;
		}

		/* 流缓冲区需要 StreamBuffer_t 结
		构和缓冲区。两者都是在对
		pvPortMalloc() 的一次调用中分配
		的。  StreamBuffer_t 结构放置
		在已分配内存的开头，缓冲区紧随其后。  请求的
		大小会增加，以便按照用户期望返回可用空间 -
		这是实现的一个怪癖，意味着否则可用空间将报告为比逻辑上小一个字节
		预期的。 */
		xBufferSizeBytes++;
		pucAllocatedMemory = ( uint8_t * ) pvPortMalloc( xBufferSizeBytes + sizeof( StreamBuffer_t ) ); /*lint !e9079 malloc() 只返回 void*。 */

		if( pucAllocatedMemory != NULL )
		{
			prvInitialiseNewStreamBuffer( ( StreamBuffer_t * ) pucAllocatedMemory, /* 已分配内存开头的结构。 */ /*lint !e9087 安全转换，分配的内存已对齐。 */ /*lint !e826 区域不太小，只要 malloc() 按预期运行并返回对齐的缓冲区，就可以保证对齐。 */
										   pucAllocatedMemory + sizeof( StreamBuffer_t ),  /* 存储区如下。 */ /*lint !e9016 索引过去的结构对 uint8_t 指针有效，而且存储区域没有对齐要求。 */
										   xBufferSizeBytes,
										   xTriggerLevelBytes,
										   ucFlags );

			traceSTREAM_BUFFER_CREATE( ( ( StreamBuffer_t * ) pucAllocatedMemory ), xIsMessageBuffer );
		}
		else
		{
			traceSTREAM_BUFFER_CREATE_FAILED( xIsMessageBuffer );
		}

		return ( StreamBufferHandle_t ) pucAllocatedMemory; /*lint !e9087 !e826 安全转换，分配的内存已对齐。 */
	}

#endif /* configSUPPORT_DYNAMIC_ALLOCATION */
/*-----------------------------------------------------------*/

#if( configSUPPORT_STATIC_ALLOCATION == 1 )

	StreamBufferHandle_t xStreamBufferGenericCreateStatic( size_t xBufferSizeBytes,
														   size_t xTriggerLevelBytes,
														   BaseType_t xIsMessageBuffer,
														   uint8_t * const pucStreamBufferStorageArea,
														   StaticStreamBuffer_t * const pxStaticStreamBuffer )
	{
	StreamBuffer_t * const pxStreamBuffer = ( StreamBuffer_t * ) pxStaticStreamBuffer; /*lint !e740 !e9087 安全转换为 StaticStreamBuffer_t 是不透明的 Streambuffer_t。 */
	StreamBufferHandle_t xReturn;
	uint8_t ucFlags;

		configASSERT( pucStreamBufferStorageArea );
		configASSERT( pxStaticStreamBuffer );
		configASSERT( xTriggerLevelBytes <= xBufferSizeBytes );

		/* 即使缓冲区为空，触发级别 0
		也会导致等待任务解除阻塞。 */
		if( xTriggerLevelBytes == ( size_t ) 0 )
		{
			xTriggerLevelBytes = ( size_t ) 1;
		}

		if( xIsMessageBuffer != pdFALSE )
		{
			/* 静态分配的消息缓冲区。 */
			ucFlags = sbFLAGS_IS_MESSAGE_BUFFER | sbFLAGS_IS_STATICALLY_ALLOCATED;
		}
		else
		{
			/* 静态分配的流缓冲区。 */
			ucFlags = sbFLAGS_IS_STATICALLY_ALLOCATED;
		}

		/* 如果流缓冲区将用作消息 buffer（
		也就是说，它将保存带有少量元数据的离散
		消息，说明下一条消息有多大），请检查缓
		冲区是否足够大以容纳至少一条消息。 */
		configASSERT( xBufferSizeBytes > sbBYTES_TO_STORE_MESSAGE_LENGTH );

		#if( configASSERT_DEFINED == 1 )
		{
			/* 健全性检查用于声明
			StaticStreamBuffer_t
			类型变量的结构的大小是否等于实际消息缓冲区结构的大小。 */
			volatile size_t xSize = sizeof( StaticStreamBuffer_t );
			configASSERT( xSize == sizeof( StreamBuffer_t ) );
		} /*lint !e529 xSize 被引用是 configASSERT() 被定义。 */
		#endif /* configASSERT_DEFINED */

		if( ( pucStreamBufferStorageArea != NULL ) && ( pxStaticStreamBuffer != NULL ) )
		{
			prvInitialiseNewStreamBuffer( pxStreamBuffer,
										  pucStreamBufferStorageArea,
										  xBufferSizeBytes,
										  xTriggerLevelBytes,
										  ucFlags );

			/* 请记住，这是静态分配的，以防它被删除
			再次。 */
			pxStreamBuffer->ucFlags |= sbFLAGS_IS_STATICALLY_ALLOCATED;

			traceSTREAM_BUFFER_CREATE( pxStreamBuffer, xIsMessageBuffer );

			xReturn = ( StreamBufferHandle_t ) pxStaticStreamBuffer; /*lint !e9087 数据隐藏需要转换为不透明类型。 */
		}
		else
		{
			xReturn = NULL;
			traceSTREAM_BUFFER_CREATE_STATIC_FAILED( xReturn, xIsMessageBuffer );
		}

		return xReturn;
	}

#endif /* (configSUPPORT_STATIC_ALLOCATION==1) */
/*-----------------------------------------------------------*/

void vStreamBufferDelete( StreamBufferHandle_t xStreamBuffer )
{
StreamBuffer_t * pxStreamBuffer = xStreamBuffer;

	configASSERT( pxStreamBuffer );

	traceSTREAM_BUFFER_DELETE( xStreamBuffer );

	if( ( pxStreamBuffer->ucFlags & sbFLAGS_IS_STATICALLY_ALLOCATED ) == ( uint8_t ) pdFALSE )
	{
		#if( configSUPPORT_DYNAMIC_ALLOCATION == 1 )
		{
			/* 结构体和缓冲区都是通过对 pvPortMalloc() 的一次调
			用来分配的，因此只需要对 vPortFree() 的一次调用。 */
			vPortFree( ( void * ) pxStreamBuffer ); /*lint !e9087 标准 free() 语义需要 void *，加上 pxStreamBuffer 是由 pvPortMalloc() 分配的。 */
		}
		#else
		{
			/* 应该不可能到达这里，
			ucFlags 一定已损坏。强制断言。 */
			configASSERT( xStreamBuffer == ( StreamBufferHandle_t ) ~0 );
		}
		#endif
	}
	else
	{
		/* 结构和缓冲区不是动态分配的，也不能释放
		- 只需清理结构，以便将来的使用会断言。 */
		( void ) memset( pxStreamBuffer, 0x00, sizeof( StreamBuffer_t ) );
	}
}
/*-----------------------------------------------------------*/

BaseType_t xStreamBufferReset( StreamBufferHandle_t xStreamBuffer )
{
StreamBuffer_t * const pxStreamBuffer = xStreamBuffer;
BaseType_t xReturn = pdFAIL;

#if( configUSE_TRACE_FACILITY == 1 )
	UBaseType_t uxStreamBufferNumber;
#endif

	configASSERT( pxStreamBuffer );

	#if( configUSE_TRACE_FACILITY == 1 )
	{
		/* 存储流缓冲区编号，以便在之后可以恢复
		重置。 */
		uxStreamBufferNumber = pxStreamBuffer->uxStreamBufferNumber;
	}
	#endif

	/* 仅当消息缓冲区上没有阻塞任务时才能重置消息缓冲区。 */
	taskENTER_CRITICAL();
	{
		if( pxStreamBuffer->xTaskWaitingToReceive == NULL )
		{
			if( pxStreamBuffer->xTaskWaitingToSend == NULL )
			{
				prvInitialiseNewStreamBuffer( pxStreamBuffer,
											  pxStreamBuffer->pucBuffer,
											  pxStreamBuffer->xLength,
											  pxStreamBuffer->xTriggerLevelBytes,
											  pxStreamBuffer->ucFlags );
				xReturn = pdPASS;

				#if( configUSE_TRACE_FACILITY == 1 )
				{
					pxStreamBuffer->uxStreamBufferNumber = uxStreamBufferNumber;
				}
				#endif

				traceSTREAM_BUFFER_RESET( xStreamBuffer );
			}
		}
	}
	taskEXIT_CRITICAL();

	return xReturn;
}
/*-----------------------------------------------------------*/

BaseType_t xStreamBufferSetTriggerLevel( StreamBufferHandle_t xStreamBuffer, size_t xTriggerLevel )
{
StreamBuffer_t * const pxStreamBuffer = xStreamBuffer;
BaseType_t xReturn;

	configASSERT( pxStreamBuffer );

	/* 触发电平为0时无效。 */
	if( xTriggerLevel == ( size_t ) 0 )
	{
		xTriggerLevel = ( size_t ) 1;
	}

	/* 触发级别是在等待数据的任务被解除阻
	塞之前流缓冲区中必须存在的字节数。 */
	if( xTriggerLevel <= pxStreamBuffer->xLength )
	{
		pxStreamBuffer->xTriggerLevelBytes = xTriggerLevel;
		xReturn = pdPASS;
	}
	else
	{
		xReturn = pdFALSE;
	}

	return xReturn;
}
/*-----------------------------------------------------------*/

size_t xStreamBufferSpacesAvailable( StreamBufferHandle_t xStreamBuffer )
{
const StreamBuffer_t * const pxStreamBuffer = xStreamBuffer;
size_t xSpace;

	configASSERT( pxStreamBuffer );

	xSpace = pxStreamBuffer->xLength + pxStreamBuffer->xTail;
	xSpace -= pxStreamBuffer->xHead;
	xSpace -= ( size_t ) 1;

	if( xSpace >= pxStreamBuffer->xLength )
	{
		xSpace -= pxStreamBuffer->xLength;
	}
	else
	{
		mtCOVERAGE_TEST_MARKER();
	}

	return xSpace;
}
/*-----------------------------------------------------------*/

size_t xStreamBufferBytesAvailable( StreamBufferHandle_t xStreamBuffer )
{
const StreamBuffer_t * const pxStreamBuffer = xStreamBuffer;
size_t xReturn;

	configASSERT( pxStreamBuffer );

	xReturn = prvBytesInBuffer( pxStreamBuffer );
	return xReturn;
}
/*-----------------------------------------------------------*/

size_t xStreamBufferSend( StreamBufferHandle_t xStreamBuffer,
						  const void *pvTxData,
						  size_t xDataLengthBytes,
						  TickType_t xTicksToWait )
{
StreamBuffer_t * const pxStreamBuffer = xStreamBuffer;
size_t xReturn, xSpace = 0;
size_t xRequiredSpace = xDataLengthBytes;
TimeOut_t xTimeOut;

	configASSERT( pvTxData );
	configASSERT( pxStreamBuffer );

	/* 该发送函数用于写入消息缓冲区和流缓冲区
	。  如果这是一个消息缓冲区，则所需的
	空间必须增加存储消息长度所需的字节数。
	信息。 */
	if( ( pxStreamBuffer->ucFlags & sbFLAGS_IS_MESSAGE_BUFFER ) != ( uint8_t ) 0 )
	{
		xRequiredSpace += sbBYTES_TO_STORE_MESSAGE_LENGTH;

		/* 溢出？ */
		configASSERT( xRequiredSpace > xDataLengthBytes );
	}
	else
	{
		mtCOVERAGE_TEST_MARKER();
	}

	if( xTicksToWait != ( TickType_t ) 0 )
	{
		vTaskSetTimeOutState( &xTimeOut );

		do
		{
			/* 等待消息中达到所需字节数为止
			缓冲。 */
			taskENTER_CRITICAL();
			{
				xSpace = xStreamBufferSpacesAvailable( pxStreamBuffer );

				if( xSpace < xRequiredSpace )
				{
					/* 清除通知状态为等待空间。 */
					( void ) xTaskNotifyStateClear( NULL );

					/* 应该只有一位作家。 */
					configASSERT( pxStreamBuffer->xTaskWaitingToSend == NULL );
					pxStreamBuffer->xTaskWaitingToSend = xTaskGetCurrentTaskHandle();
				}
				else
				{
					taskEXIT_CRITICAL();
					break;
				}
			}
			taskEXIT_CRITICAL();

			traceBLOCKING_ON_STREAM_BUFFER_SEND( xStreamBuffer );
			( void ) xTaskNotifyWait( ( uint32_t ) 0, ( uint32_t ) 0, NULL, xTicksToWait );
			pxStreamBuffer->xTaskWaitingToSend = NULL;

		} while( xTaskCheckForTimeOut( &xTimeOut, &xTicksToWait ) == pdFALSE );
	}
	else
	{
		mtCOVERAGE_TEST_MARKER();
	}

	if( xSpace == ( size_t ) 0 )
	{
		xSpace = xStreamBufferSpacesAvailable( pxStreamBuffer );
	}
	else
	{
		mtCOVERAGE_TEST_MARKER();
	}

	xReturn = prvWriteMessageToBuffer( pxStreamBuffer, pvTxData, xDataLengthBytes, xSpace, xRequiredSpace );

	if( xReturn > ( size_t ) 0 )
	{
		traceSTREAM_BUFFER_SEND( xStreamBuffer, xReturn );

		/* 是否有任务在等待数据？ */
		if( prvBytesInBuffer( pxStreamBuffer ) >= pxStreamBuffer->xTriggerLevelBytes )
		{
			sbSEND_COMPLETED( pxStreamBuffer );
		}
		else
		{
			mtCOVERAGE_TEST_MARKER();
		}
	}
	else
	{
		mtCOVERAGE_TEST_MARKER();
		traceSTREAM_BUFFER_SEND_FAILED( xStreamBuffer );
	}

	return xReturn;
}
/*-----------------------------------------------------------*/

size_t xStreamBufferSendFromISR( StreamBufferHandle_t xStreamBuffer,
								 const void *pvTxData,
								 size_t xDataLengthBytes,
								 BaseType_t * const pxHigherPriorityTaskWoken )
{
StreamBuffer_t * const pxStreamBuffer = xStreamBuffer;
size_t xReturn, xSpace;
size_t xRequiredSpace = xDataLengthBytes;

	configASSERT( pvTxData );
	configASSERT( pxStreamBuffer );

	/* 该发送函数用于写入消息缓冲区和流缓冲区
	。  如果这是一个消息缓冲区，则所需的
	空间必须增加存储消息长度所需的字节数。
	信息。 */
	if( ( pxStreamBuffer->ucFlags & sbFLAGS_IS_MESSAGE_BUFFER ) != ( uint8_t ) 0 )
	{
		xRequiredSpace += sbBYTES_TO_STORE_MESSAGE_LENGTH;
	}
	else
	{
		mtCOVERAGE_TEST_MARKER();
	}

	xSpace = xStreamBufferSpacesAvailable( pxStreamBuffer );
	xReturn = prvWriteMessageToBuffer( pxStreamBuffer, pvTxData, xDataLengthBytes, xSpace, xRequiredSpace );

	if( xReturn > ( size_t ) 0 )
	{
		/* 是否有任务在等待数据？ */
		if( prvBytesInBuffer( pxStreamBuffer ) >= pxStreamBuffer->xTriggerLevelBytes )
		{
			sbSEND_COMPLETE_FROM_ISR( pxStreamBuffer, pxHigherPriorityTaskWoken );
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

	traceSTREAM_BUFFER_SEND_FROM_ISR( xStreamBuffer, xReturn );

	return xReturn;
}
/*-----------------------------------------------------------*/

static size_t prvWriteMessageToBuffer( StreamBuffer_t * const pxStreamBuffer,
									   const void * pvTxData,
									   size_t xDataLengthBytes,
									   size_t xSpace,
									   size_t xRequiredSpace )
{
	BaseType_t xShouldWrite;
	size_t xReturn;

	if( xSpace == ( size_t ) 0 )
	{
		/* 无论这是流缓冲区还是消息
		缓冲区，都没有空间可写。 */
		xShouldWrite = pdFALSE;
	}
	else if( ( pxStreamBuffer->ucFlags & sbFLAGS_IS_MESSAGE_BUFFER ) == ( uint8_t ) 0 )
	{
		/* 这是一个流缓冲区，而不是消息缓冲区，因此写入
		字节流而不是离散消息。  写入尽可能多的字节
		可能的。 */
		xShouldWrite = pdTRUE;
		xDataLengthBytes = configMIN( xDataLengthBytes, xSpace );
	}
	else if( xSpace >= xRequiredSpace )
	{
		/* 这是一个消息缓冲区，而不是流缓冲区，
		并且有足够的空间将消息长度和消息本身
		写入缓冲区。  首先写入数据的长度，
		稍后将在此函数中写入数据本身。 */
		xShouldWrite = pdTRUE;
		( void ) prvWriteBytesToBuffer( pxStreamBuffer, ( const uint8_t * ) &( xDataLengthBytes ), sbBYTES_TO_STORE_MESSAGE_LENGTH );
	}
	else
	{
		/* 有可用空间，但空间不足。 */
		xShouldWrite = pdFALSE;
	}

	if( xShouldWrite != pdFALSE )
	{
		/* 写入数据本身。 */
		xReturn = prvWriteBytesToBuffer( pxStreamBuffer, ( const uint8_t * ) pvTxData, xDataLengthBytes ); /*lint !e9079 存储缓冲区被实现为 uint8_t，以便于调整大小、对齐和访问。 */
	}
	else
	{
		xReturn = 0;
	}

	return xReturn;
}
/*-----------------------------------------------------------*/

size_t xStreamBufferReceive( StreamBufferHandle_t xStreamBuffer,
							 void *pvRxData,
							 size_t xBufferLengthBytes,
							 TickType_t xTicksToWait )
{
StreamBuffer_t * const pxStreamBuffer = xStreamBuffer;
size_t xReceivedLength = 0, xBytesAvailable, xBytesToStoreMessageLength;

	configASSERT( pvRxData );
	configASSERT( pxStreamBuffer );

	/* 该接收函数由存储离散消息的消息缓冲区和存储连续
	字节流的流缓冲区使用。  离散消息包括一个附加
	的
	sbBYTES_TO_STORE_MESSAGE_LENGTH 字节，用于保存消息的长度
	信息。 */
	if( ( pxStreamBuffer->ucFlags & sbFLAGS_IS_MESSAGE_BUFFER ) != ( uint8_t ) 0 )
	{
		xBytesToStoreMessageLength = sbBYTES_TO_STORE_MESSAGE_LENGTH;
	}
	else
	{
		xBytesToStoreMessageLength = 0;
	}

	if( xTicksToWait != ( TickType_t ) 0 )
	{
		/* 检查是否有数据并清除通知
		状态必须以原子方式执行。 */
		taskENTER_CRITICAL();
		{
			xBytesAvailable = prvBytesInBuffer( pxStreamBuffer );

			/* 如果此函数是由消息缓冲区读取调用的，则
			xBytesToStoreMessageLength 保存
			用于保存下一个离散消息长度的字节数。  如果此函数是由流缓
			冲区读取调用的，则 xBytesToStoreMessageLength 将
			为 0。 */
			if( xBytesAvailable <= xBytesToStoreMessageLength )
			{
				/* 清除通知状态为等待数据。 */
				( void ) xTaskNotifyStateClear( NULL );

				/* 应该只有一名读者。 */
				configASSERT( pxStreamBuffer->xTaskWaitingToReceive == NULL );
				pxStreamBuffer->xTaskWaitingToReceive = xTaskGetCurrentTaskHandle();
			}
			else
			{
				mtCOVERAGE_TEST_MARKER();
			}
		}
		taskEXIT_CRITICAL();

		if( xBytesAvailable <= xBytesToStoreMessageLength )
		{
			/* 等待数据可用。 */
			traceBLOCKING_ON_STREAM_BUFFER_RECEIVE( xStreamBuffer );
			( void ) xTaskNotifyWait( ( uint32_t ) 0, ( uint32_t ) 0, NULL, xTicksToWait );
			pxStreamBuffer->xTaskWaitingToReceive = NULL;

			/* 阻塞后重新检查可用数据。 */
			xBytesAvailable = prvBytesInBuffer( pxStreamBuffer );
		}
		else
		{
			mtCOVERAGE_TEST_MARKER();
		}
	}
	else
	{
		xBytesAvailable = prvBytesInBuffer( pxStreamBuffer );
	}

	/* 无论是接收离散的 message（其中
	xBytesToStoreMessageLength 保存用于
	存储消息长度的字节数）还是 bytes 流（其中
	xBytesToStoreMessageLength 为零），
	可用字节数必须大于 xBytesToStoreMessageLength 才能从缓冲区读取字节。 */
	if( xBytesAvailable > xBytesToStoreMessageLength )
	{
		xReceivedLength = prvReadMessageFromBuffer( pxStreamBuffer, pvRxData, xBufferLengthBytes, xBytesAvailable, xBytesToStoreMessageLength );

		/* 任务是否正在等待缓冲区中的空间？ */
		if( xReceivedLength != ( size_t ) 0 )
		{
			traceSTREAM_BUFFER_RECEIVE( xStreamBuffer, xReceivedLength );
			sbRECEIVE_COMPLETED( pxStreamBuffer );
		}
		else
		{
			mtCOVERAGE_TEST_MARKER();
		}
	}
	else
	{
		traceSTREAM_BUFFER_RECEIVE_FAILED( xStreamBuffer );
		mtCOVERAGE_TEST_MARKER();
	}

	return xReceivedLength;
}
/*-----------------------------------------------------------*/

size_t xStreamBufferNextMessageLengthBytes( StreamBufferHandle_t xStreamBuffer )
{
StreamBuffer_t * const pxStreamBuffer = xStreamBuffer;
size_t xReturn, xBytesAvailable, xOriginalTail;
configMESSAGE_BUFFER_LENGTH_TYPE xTempReturn;

	configASSERT( pxStreamBuffer );

	/* 确保流缓冲区用作消息缓冲区。 */
	if( ( pxStreamBuffer->ucFlags & sbFLAGS_IS_MESSAGE_BUFFER ) != ( uint8_t ) 0 )
	{
		xBytesAvailable = prvBytesInBuffer( pxStreamBuffer );
		if( xBytesAvailable > sbBYTES_TO_STORE_MESSAGE_LENGTH )
		{
			/* 可用字节数大于保存下一条消息长度所
			需的字节数，因此另一条消息可用。
			返回其长度，而不从缓冲区中删除
			长度字节。  存储尾部的副本，以便
			缓冲区可以返回到其之前的状态，因为
			消息实际上并未从缓冲区中删除。 */
			xOriginalTail = pxStreamBuffer->xTail;
			( void ) prvReadBytesFromBuffer( pxStreamBuffer, ( uint8_t * ) &xTempReturn, sbBYTES_TO_STORE_MESSAGE_LENGTH, xBytesAvailable );
			xReturn = ( size_t ) xTempReturn;
			pxStreamBuffer->xTail = xOriginalTail;
		}
		else
		{
			/* 消息缓冲区中的最小字节数 is (
			sbBYTES_TO_STORE_MESSAGE_LENGTH
			+ 1 )，因此如果 xBytesAvailable 小于
			sbBYTES_TO_STORE_MESSAGE_LENGTH 则唯一的其他有效值为 0。 */
			configASSERT( xBytesAvailable == 0 );
			xReturn = 0;
		}
	}
	else
	{
		xReturn = 0;
	}

	return xReturn;
}
/*-----------------------------------------------------------*/

size_t xStreamBufferReceiveFromISR( StreamBufferHandle_t xStreamBuffer,
									void *pvRxData,
									size_t xBufferLengthBytes,
									BaseType_t * const pxHigherPriorityTaskWoken )
{
StreamBuffer_t * const pxStreamBuffer = xStreamBuffer;
size_t xReceivedLength = 0, xBytesAvailable, xBytesToStoreMessageLength;

	configASSERT( pvRxData );
	configASSERT( pxStreamBuffer );

	/* 该接收函数由存储离散消息的消息缓冲区和存储连续
	字节流的流缓冲区使用。  离散消息包括一个附加
	的
	sbBYTES_TO_STORE_MESSAGE_LENGTH 字节，用于保存消息的长度
	信息。 */
	if( ( pxStreamBuffer->ucFlags & sbFLAGS_IS_MESSAGE_BUFFER ) != ( uint8_t ) 0 )
	{
		xBytesToStoreMessageLength = sbBYTES_TO_STORE_MESSAGE_LENGTH;
	}
	else
	{
		xBytesToStoreMessageLength = 0;
	}

	xBytesAvailable = prvBytesInBuffer( pxStreamBuffer );

	/* 无论是接收离散的 message（其中
	xBytesToStoreMessageLength 保存用于
	存储消息长度的字节数）还是 bytes 流（其中
	xBytesToStoreMessageLength 为零），
	可用字节数必须大于 xBytesToStoreMessageLength 才能从缓冲区读取字节。 */
	if( xBytesAvailable > xBytesToStoreMessageLength )
	{
		xReceivedLength = prvReadMessageFromBuffer( pxStreamBuffer, pvRxData, xBufferLengthBytes, xBytesAvailable, xBytesToStoreMessageLength );

		/* 任务是否正在等待缓冲区中的空间？ */
		if( xReceivedLength != ( size_t ) 0 )
		{
			sbRECEIVE_COMPLETED_FROM_ISR( pxStreamBuffer, pxHigherPriorityTaskWoken );
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

	traceSTREAM_BUFFER_RECEIVE_FROM_ISR( xStreamBuffer, xReceivedLength );

	return xReceivedLength;
}
/*-----------------------------------------------------------*/

static size_t prvReadMessageFromBuffer( StreamBuffer_t *pxStreamBuffer,
										void *pvRxData,
										size_t xBufferLengthBytes,
										size_t xBytesAvailable,
										size_t xBytesToStoreMessageLength )
{
size_t xOriginalTail, xReceivedLength, xNextMessageLength;
configMESSAGE_BUFFER_LENGTH_TYPE xTempNextMessageLength;

	if( xBytesToStoreMessageLength != ( size_t ) 0 )
	{
		/* 正在接收一条离散消息。  首先接收消
		息的长度。  存储尾部的副本，以便如
		果消息的长度对于所提供的缓冲区来说太
		大，则缓冲区可以返回到其先前的状态。 */
		xOriginalTail = pxStreamBuffer->xTail;
		( void ) prvReadBytesFromBuffer( pxStreamBuffer, ( uint8_t * ) &xTempNextMessageLength, xBytesToStoreMessageLength, xBytesAvailable );
		xNextMessageLength = ( size_t ) xTempNextMessageLength;

		/* 将可用字节数减少到
		刚刚读出的字节数。 */
		xBytesAvailable -= xBytesToStoreMessageLength;

		/* 检查缓冲区提供的空间是否足够
		用户。 */
		if( xNextMessageLength > xBufferLengthBytes )
		{
			/* 用户提供的空间不足以读取消息，因此
			将缓冲区返回到之前的 state（
			因此消息的长度再次位于缓冲区中）。 */
			pxStreamBuffer->xTail = xOriginalTail;
			xNextMessageLength = 0;
		}
		else
		{
			mtCOVERAGE_TEST_MARKER();
		}
	}
	else
	{
		/* 字节流是 received（与离散消
		息相对），因此读取尽可能多的字节。 */
		xNextMessageLength = xBufferLengthBytes;
	}

	/* 读取实际数据。 */
	xReceivedLength = prvReadBytesFromBuffer( pxStreamBuffer, ( uint8_t * ) pvRxData, xNextMessageLength, xBytesAvailable ); /*lint !e9079 数据存储区域实现为 uint8_t 数组，以便于调整大小、索引和对齐。 */

	return xReceivedLength;
}
/*-----------------------------------------------------------*/

BaseType_t xStreamBufferIsEmpty( StreamBufferHandle_t xStreamBuffer )
{
const StreamBuffer_t * const pxStreamBuffer = xStreamBuffer;
BaseType_t xReturn;
size_t xTail;

	configASSERT( pxStreamBuffer );

	/* 如果没有可用字节则为 True。 */
	xTail = pxStreamBuffer->xTail;
	if( pxStreamBuffer->xHead == xTail )
	{
		xReturn = pdTRUE;
	}
	else
	{
		xReturn = pdFALSE;
	}

	return xReturn;
}
/*-----------------------------------------------------------*/

BaseType_t xStreamBufferIsFull( StreamBufferHandle_t xStreamBuffer )
{
BaseType_t xReturn;
size_t xBytesToStoreMessageLength;
const StreamBuffer_t * const pxStreamBuffer = xStreamBuffer;

	configASSERT( pxStreamBuffer );

	/* 接收函数的通用版本由存储离散消息的消息缓冲区和存储
	连续字节流的流缓冲区使用。  离散消息包括一个附加
	的
	sbBYTES_TO_STORE_MESSAGE_LENGTH 字节，用于保存消息的长度。 */
	if( ( pxStreamBuffer->ucFlags & sbFLAGS_IS_MESSAGE_BUFFER ) != ( uint8_t ) 0 )
	{
		xBytesToStoreMessageLength = sbBYTES_TO_STORE_MESSAGE_LENGTH;
	}
	else
	{
		xBytesToStoreMessageLength = 0;
	}

	/* 如果可用空间为零，则为 True。 */
	if( xStreamBufferSpacesAvailable( xStreamBuffer ) <= xBytesToStoreMessageLength )
	{
		xReturn = pdTRUE;
	}
	else
	{
		xReturn = pdFALSE;
	}

	return xReturn;
}
/*-----------------------------------------------------------*/

BaseType_t xStreamBufferSendCompletedFromISR( StreamBufferHandle_t xStreamBuffer, BaseType_t *pxHigherPriorityTaskWoken )
{
StreamBuffer_t * const pxStreamBuffer = xStreamBuffer;
BaseType_t xReturn;
UBaseType_t uxSavedInterruptStatus;

	configASSERT( pxStreamBuffer );

	uxSavedInterruptStatus = ( UBaseType_t ) portSET_INTERRUPT_MASK_FROM_ISR();
	{
		if( ( pxStreamBuffer )->xTaskWaitingToReceive != NULL )
		{
			( void ) xTaskNotifyFromISR( ( pxStreamBuffer )->xTaskWaitingToReceive,
										 ( uint32_t ) 0,
										 eNoAction,
										 pxHigherPriorityTaskWoken );
			( pxStreamBuffer )->xTaskWaitingToReceive = NULL;
			xReturn = pdTRUE;
		}
		else
		{
			xReturn = pdFALSE;
		}
	}
	portCLEAR_INTERRUPT_MASK_FROM_ISR( uxSavedInterruptStatus );

	return xReturn;
}
/*-----------------------------------------------------------*/

BaseType_t xStreamBufferReceiveCompletedFromISR( StreamBufferHandle_t xStreamBuffer, BaseType_t *pxHigherPriorityTaskWoken )
{
StreamBuffer_t * const pxStreamBuffer = xStreamBuffer;
BaseType_t xReturn;
UBaseType_t uxSavedInterruptStatus;

	configASSERT( pxStreamBuffer );

	uxSavedInterruptStatus = ( UBaseType_t ) portSET_INTERRUPT_MASK_FROM_ISR();
	{
		if( ( pxStreamBuffer )->xTaskWaitingToSend != NULL )
		{
			( void ) xTaskNotifyFromISR( ( pxStreamBuffer )->xTaskWaitingToSend,
										 ( uint32_t ) 0,
										 eNoAction,
										 pxHigherPriorityTaskWoken );
			( pxStreamBuffer )->xTaskWaitingToSend = NULL;
			xReturn = pdTRUE;
		}
		else
		{
			xReturn = pdFALSE;
		}
	}
	portCLEAR_INTERRUPT_MASK_FROM_ISR( uxSavedInterruptStatus );

	return xReturn;
}
/*-----------------------------------------------------------*/

static size_t prvWriteBytesToBuffer( StreamBuffer_t * const pxStreamBuffer, const uint8_t *pucData, size_t xCount )
{
size_t xNextHead, xFirstLength;

	configASSERT( xCount > ( size_t ) 0 );

	xNextHead = pxStreamBuffer->xHead;

	/* 计算在第一次写入中可以添加的字节数
	- 如果缓冲区将回滚到开头，则该
	字节数可能小于需要添加的字节总数。 */
	xFirstLength = configMIN( pxStreamBuffer->xLength - xNextHead, xCount );

	/* 写入第一次写入时可以写入的尽可能多的字节。 */
	configASSERT( ( xNextHead + xFirstLength ) <= pxStreamBuffer->xLength );
	( void ) memcpy( ( void* ) ( &( pxStreamBuffer->pucBuffer[ xNextHead ] ) ), ( const void * ) pucData, xFirstLength ); /*lint !e9087 memcpy() 需要 void *。 */

	/* 如果写入的字节数小于第一次写
	入时可以写入的字节数... */
	if( xCount > xFirstLength )
	{
		/* ...然后将剩余字节写入缓冲区的开头。 */
		configASSERT( ( xCount - xFirstLength ) <= pxStreamBuffer->xLength );
		( void ) memcpy( ( void * ) pxStreamBuffer->pucBuffer, ( const void * ) &( pucData[ xFirstLength ] ), xCount - xFirstLength ); /*lint !e9087 memcpy() 需要 void *。 */
	}
	else
	{
		mtCOVERAGE_TEST_MARKER();
	}

	xNextHead += xCount;
	if( xNextHead >= pxStreamBuffer->xLength )
	{
		xNextHead -= pxStreamBuffer->xLength;
	}
	else
	{
		mtCOVERAGE_TEST_MARKER();
	}

	pxStreamBuffer->xHead = xNextHead;

	return xCount;
}
/*-----------------------------------------------------------*/

static size_t prvReadBytesFromBuffer( StreamBuffer_t *pxStreamBuffer, uint8_t *pucData, size_t xMaxCount, size_t xBytesAvailable )
{
size_t xCount, xFirstLength, xNextTail;

	/* 使用所需字节和可用字节中的最小值。 */
	xCount = configMIN( xBytesAvailable, xMaxCount );

	if( xCount > ( size_t ) 0 )
	{
		xNextTail = pxStreamBuffer->xTail;

		/* 计算可以读取的字节数 - 如
		果数据回绕到缓冲区的开头，则
		该字节数可能小于所需的数量。 */
		xFirstLength = configMIN( pxStreamBuffer->xLength - xNextTail, xCount );

		/* 获取第一次读取时可能获得的字节数
		。  断言检查读取和写入的范围。 */
		configASSERT( xFirstLength <= xMaxCount );
		configASSERT( ( xNextTail + xFirstLength ) <= pxStreamBuffer->xLength );
		( void ) memcpy( ( void * ) pucData, ( const void * ) &( pxStreamBuffer->pucBuffer[ xNextTail ] ), xFirstLength ); /*lint !e9087 memcpy() 需要 void *。 */

		/* 如果所需字节总数大于第一次读
		取中可以读取的字节数... */
		if( xCount > xFirstLength )
		{
			/*...然后从缓冲区的开头读取剩余的字节。 */
			configASSERT( xCount <= xMaxCount );
			( void ) memcpy( ( void * ) &( pucData[ xFirstLength ] ), ( void * ) ( pxStreamBuffer->pucBuffer ), xCount - xFirstLength ); /*lint !e9087 memcpy() 需要 void *。 */
		}
		else
		{
			mtCOVERAGE_TEST_MARKER();
		}

		/* 移动尾部指针可以有效地移
		除从缓冲区读取的数据。 */
		xNextTail += xCount;

		if( xNextTail >= pxStreamBuffer->xLength )
		{
			xNextTail -= pxStreamBuffer->xLength;
		}

		pxStreamBuffer->xTail = xNextTail;
	}
	else
	{
		mtCOVERAGE_TEST_MARKER();
	}

	return xCount;
}
/*-----------------------------------------------------------*/

static size_t prvBytesInBuffer( const StreamBuffer_t * const pxStreamBuffer )
{
/* 返回 xTail 和 xHead 之间的距离。 */
size_t xCount;

	xCount = pxStreamBuffer->xLength + pxStreamBuffer->xHead;
	xCount -= pxStreamBuffer->xTail;
	if ( xCount >= pxStreamBuffer->xLength )
	{
		xCount -= pxStreamBuffer->xLength;
	}
	else
	{
		mtCOVERAGE_TEST_MARKER();
	}

	return xCount;
}
/*-----------------------------------------------------------*/

static void prvInitialiseNewStreamBuffer( StreamBuffer_t * const pxStreamBuffer,
										  uint8_t * const pucBuffer,
										  size_t xBufferSizeBytes,
										  size_t xTriggerLevelBytes,
										  uint8_t ucFlags )
{
	/* 这里的断言是故意写入整个缓冲区，以
	确保可以写入而不会产生异常，并将缓
	冲区设置为已知值以协助开发/调试。 */
	#if( configASSERT_DEFINED == 1 )
	{
		/* 当查看内存时，写入的值必须是可识别的。
		不要使用 0xA5，因为这是堆栈填充
		值，可能会导致对实际观察到的内容感到困惑。 */
		const BaseType_t xWriteValue = 0x55;
		configASSERT( memset( pucBuffer, ( int ) xWriteValue, xBufferSizeBytes ) == pucBuffer );
	} /*lint !e529 !e438 xWriteValue 仅在定义了 configASSERT() 时才使用。 */
	#endif

	( void ) memset( ( void * ) pxStreamBuffer, 0x00, sizeof( StreamBuffer_t ) ); /*lint !e9087 memset() 需要 void *。 */
	pxStreamBuffer->pucBuffer = pucBuffer;
	pxStreamBuffer->xLength = xBufferSizeBytes;
	pxStreamBuffer->xTriggerLevelBytes = xTriggerLevelBytes;
	pxStreamBuffer->ucFlags = ucFlags;
}

#if ( configUSE_TRACE_FACILITY == 1 )

	UBaseType_t uxStreamBufferGetStreamBufferNumber( StreamBufferHandle_t xStreamBuffer )
	{
		return xStreamBuffer->uxStreamBufferNumber;
	}

#endif /* configUSE_TRACE_FACILITY */
/*-----------------------------------------------------------*/

#if ( configUSE_TRACE_FACILITY == 1 )

	void vStreamBufferSetStreamBufferNumber( StreamBufferHandle_t xStreamBuffer, UBaseType_t uxStreamBufferNumber )
	{
		xStreamBuffer->uxStreamBufferNumber = uxStreamBufferNumber;
	}

#endif /* configUSE_TRACE_FACILITY */
/*-----------------------------------------------------------*/

#if ( configUSE_TRACE_FACILITY == 1 )

	uint8_t ucStreamBufferGetStreamBufferType( StreamBufferHandle_t xStreamBuffer )
	{
		return ( xStreamBuffer->ucFlags & sbFLAGS_IS_MESSAGE_BUFFER );
	}

#endif /* configUSE_TRACE_FACILITY */
/*-----------------------------------------------------------*/
