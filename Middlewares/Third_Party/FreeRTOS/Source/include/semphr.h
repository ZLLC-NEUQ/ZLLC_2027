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

#ifndef SEMAPHORE_H
#define SEMAPHORE_H

#ifndef INC_FREERTOS_H
	#error "include FreeRTOS.h" must appear in source files before "include semphr.h"
#endif

#include "queue.h"

typedef QueueHandle_t SemaphoreHandle_t;

#define semBINARY_SEMAPHORE_QUEUE_LENGTH	( ( uint8_t ) 1U )
#define semSEMAPHORE_QUEUE_ITEM_LENGTH		( ( uint8_t ) 0U )
#define semGIVE_BLOCK_TIME					( ( TickType_t ) 0U )


/**
 * semphr.h
 * <pre>vSemaphoreCreateBinary( SemaphoreHandle_t xSemaphore )</pre>
 *
 * 在许多使用场景中，使用直接任务通知代替
 * 二进制信号量速度更快、内存效率更高！
 * http://www.freertos.org/RTOS-task-notifications.html
 *
 * 这个旧的 vSemaphoreCreateBinary() 宏现已弃
 * 用，取而代之的是 xSemaphoreCreateBinary()
 * 函数。  请注意，使用
 * vSemaphoreCreateBinary() 宏创建的二进制信号
 * 量是在第一次调用“获取”信号量的状态下创建的，而使用
 * xSemaphoreCreateBinary() 创建的二进制信号量是在信号量必须先“给出”才能“获取”的状态下创建的。
 *
 * 使用现有队列机制实现信号量的<i>宏</i>
 * 。队列长度为 1，因为这是一个二进制信号量。
 * 数据大小为 0，因为我们不想实际存储任何
 * 数据 - 我们只想知道队列是空还是满。
 *
 * 这种类型的信号量可用于任务之间或中断与任务之间的纯同
 * 步。  信号量一旦获得就不需要返还，因此一个任务/中
 * 断可以连续“给出”信号量，而另一个任务/中断可以连续
 * “获取”信号量。  因此，这种类型的信号量不使用优先
 * 级继承机制。  对于使用优先级继承的替代方案，请参阅
 * xSemaphoreCreateMutex()。
 *
 * @param xSemaphore 创建的信号量的句柄。  应为 SemaphoreHandle_t 类型。
 *
 * 用法示例：
 <pre>
 SemaphoreHandle_t xSemaphore = NULL;

 void vATask( void * pvParameters )
 {
    //在调用
    //vSemaphoreCreateBinary() 之前不能使用信号量。这是一个宏，因此直接传递变量。
    vSemaphoreCreateBinary( xSemaphore );

    if( xSemaphore != NULL )
    {
        //信号量创建成功。现在
        //可以使用信号量了。
    }
 }
 </pre>
 * \defgroup vSemaphoreCreateBinary vSemaphoreCreateBinary
 * \ingroup Semaphores
 */
#if( configSUPPORT_DYNAMIC_ALLOCATION == 1 )
	#define vSemaphoreCreateBinary( xSemaphore )																							\
		{																																	\
			( xSemaphore ) = xQueueGenericCreate( ( UBaseType_t ) 1, semSEMAPHORE_QUEUE_ITEM_LENGTH, queueQUEUE_TYPE_BINARY_SEMAPHORE );	\
			if( ( xSemaphore ) != NULL )																									\
			{																																\
				( void ) xSemaphoreGive( ( xSemaphore ) );																					\
			}																																\
		}
#endif

/**
 * semphr.h
 * <pre>SemaphoreHandle_t xSemaphoreCreateBinary( void )</pre>
 *
 * 创建一个新的二进制信号量实例，并
 * 返回一个可以引用新信号量的句柄。
 *
 * 在许多使用场景中，使用直接任务通知代替
 * 二进制信号量速度更快、内存效率更高！
 * http://www.freertos.org/RTOS-task-notifications.html
 *
 * 在 FreeRTOS 实现内部，二进制信号量使用存储信号
 * 量结构的内存块。  如果使用
 * xSemaphoreCreateBinary() 创建二进制信号量，则所需的内存为
 * 在 xSemaphoreCreateBinary() 函数内部自动动态分配
 * 功能。  （请参见
 * http://www.freertos.org/a00111.html). 如果使用
 * xSemaphoreCreateBinaryStatic() 创建二进制信号量，则应用
 * 程序编写者必须提供内存。因此，xSemaphoreCreateBinaryStatic() 允许在不使用任何动态内存分配的情况下创建二进制信号量。
 *
 * 旧的 vSemaphoreCreateBinary() 宏现已弃用，
 * 取而代之的是 xSemaphoreCreateBinary() 函数
 * 。  请注意，使用 vSemaphoreCreateBinary()
 * 宏创建的二进制信号量是在第一次调用“获取”信号量的状态下创建的，而
 * 使用 xSemaphoreCreateBinary() 创建的二进制
 * 信号量是在信号量必须先“给出”才能“获取”的状态下创建的。
 *
 * 这种类型的信号量可用于任务之间或中断与任务之间的纯同
 * 步。  信号量一旦获得就不需要返还，因此一个任务/中
 * 断可以连续“给出”信号量，而另一个任务/中断可以连续
 * “获取”信号量。  因此，这种类型的信号量不使用优先
 * 级继承机制。  对于使用优先级继承的替代方案，请参阅
 * xSemaphoreCreateMutex()。
 *
 * @return 创建的信号量的句柄，如果需要内存，则为 NULL
 * 无法分配持有信号量的数据结构。
 *
 * 用法示例：
 <pre>
 SemaphoreHandle_t xSemaphore = NULL;

 void vATask( void * pvParameters )
 {
    //在调用
    //xSemaphoreCreateBinary() 之前不能使用信号量。这是一个宏，因此直接传递变量。
    xSemaphore = xSemaphoreCreateBinary();

    if( xSemaphore != NULL )
    {
        //信号量创建成功。现在
        //可以使用信号量了。
    }
 }
 </pre>
 * \defgroup xSemaphoreCreateBinary xSemaphoreCreateBinary
 * \ingroup Semaphores
 */
#if( configSUPPORT_DYNAMIC_ALLOCATION == 1 )
	#define xSemaphoreCreateBinary() xQueueGenericCreate( ( UBaseType_t ) 1, semSEMAPHORE_QUEUE_ITEM_LENGTH, queueQUEUE_TYPE_BINARY_SEMAPHORE )
#endif

/**
 * semphr.h
 * <pre>SemaphoreHandle_t xSemaphoreCreateBinaryStatic( StaticSemaphore_t *pxSemaphoreBuffer )</pre>
 *
 * 创建一个新的二进制信号量实例，并
 * 返回一个可以引用新信号量的句柄。
 *
 * 注意：在许多使用场景中，使用直接任务通知
 * 代替二进制信号量速度更快、内存效率更高！
 * http://www.freertos.org/RTOS-task-notifications.html
 *
 * 在 FreeRTOS 实现内部，二进制信号量使用存储信号
 * 量结构的内存块。  如果使用
 * xSemaphoreCreateBinary() 创建二进制信号量，则所需的内存为
 * 在 xSemaphoreCreateBinary() 函数内部自动动态分配
 * 功能。  （请参见
 * http://www.freertos.org/a00111.html). 如果使用
 * xSemaphoreCreateBinaryStatic() 创建二进制信号量，则应用
 * 程序编写者必须提供内存。因此，xSemaphoreCreateBinaryStatic() 允许在不使用任何动态内存分配的情况下创建二进制信号量。
 *
 * 这种类型的信号量可用于任务之间或中断与任务之间的纯同
 * 步。  信号量一旦获得就不需要返还，因此一个任务/中
 * 断可以连续“给出”信号量，而另一个任务/中断可以连续
 * “获取”信号量。  因此，这种类型的信号量不使用优先
 * 级继承机制。  对于使用优先级继承的替代方案，请参阅
 * xSemaphoreCreateMutex()。
 *
 * @param pxSemaphoreBuffer 必须指向 StaticSemaphore_t 类型的变量，
 * 然后它将用于保存信号量的数据
 * 结构，从而无需动态分配内存。
 *
 * @return 如果创建了信号量，则创建的信号量的句柄为
 * 回来了。  如果 pxSemaphoreBuffer 是 NULL，则返回 NULL。
 *
 * 用法示例：
 <pre>
 SemaphoreHandle_t xSemaphore = NULL;
 StaticSemaphore_t xSemaphoreBuffer;

 void vATask( void * pvParameters )
 {
    //在调用
    //xSemaphoreCreateBinary() 之
    //前不能使用信号量。信号量的数据结构将放置在
    //xSemaphoreBuffer 变量中，该变量的地
    //址被传递到函数中。  该函数的参数不是NULL，因此该函数不会尝试任何动态内存分配，因此该函数不会返回
    //return NULL.
    xSemaphore = xSemaphoreCreateBinary( &xSemaphoreBuffer );

    //其余的任务代码放在这里。
 }
 </pre>
 * \defgroup xSemaphoreCreateBinaryStatic xSemaphoreCreateBinaryStatic
 * \ingroup Semaphores
 */
#if( configSUPPORT_STATIC_ALLOCATION == 1 )
	#define xSemaphoreCreateBinaryStatic( pxStaticSemaphore ) xQueueGenericCreateStatic( ( UBaseType_t ) 1, semSEMAPHORE_QUEUE_ITEM_LENGTH, NULL, pxStaticSemaphore, queueQUEUE_TYPE_BINARY_SEMAPHORE )
#endif /* configSUPPORT_STATIC_ALLOCATION */

/**
 * semphr.h
 * <pre>xSemaphoreTake(
 *                   SemaphoreHandle_t xSemaphore,
 *                   TickType_t xBlockTime
 *               )</pre>
 *
 * <i>宏</i>获取信号量。  信号量必须事先通过调用
 * xSemaphoreCreateBinary()、
 * xSemaphoreCreateMutex() 或 xSemaphoreCreateCounting() 创建。
 *
 * @param xSemaphore 正在获取的信号量的句柄 - 在以下情况下获得
 * 信号量已创建。
 *
 * @param xBlockTime 等待信号量变为的时间（以刻度为单位）
 * 可用。  宏 portTICK_PERIOD_MS 可用于将其转换为实时
 * 。  零块时间可用于轮询信号量。  portMAX_DELAY 的阻塞时
 * 间可用于阻塞 indefinitely（前提是
 * INCLUDE_vTaskSuspend 在 FreeRTOSConfig.h 中设置为 1）。
 *
 * @return pdTRUE 如果获得了信号量。  pdFALSE
 * if xBlockTime expired without the semaphore becoming available.
 *
 * 用法示例：
 <pre>
 SemaphoreHandle_t xSemaphore = NULL;

 //创建信号量的任务。
 void vATask( void * pvParameters )
 {
    //创建信号量来保护共享资源。
    xSemaphore = xSemaphoreCreateBinary();
 }

 //使用信号量的任务。
 void vAnotherTask( void * pvParameters )
 {
    //……做其他事情。

    if( xSemaphore != NULL )
    {
        //看看是否可以获得信号量。  如果信号量不可
        //用，请等待 10 个周期，看看它是否空闲。
        if( xSemaphoreTake( xSemaphore, ( TickType_t ) 10 ) == pdTRUE )
        {
            //我们能够获取信号量，现
            //在可以访问共享资源。

            //...

            //我们已经完成了对共享资源的访问。  释放
            //semaphore.
            xSemaphoreGive( xSemaphore );
        }
        else
        {
            //我们无法获取信号量，因此
            //无法安全地访问共享资源。
        }
    }
 }
 </pre>
 * \defgroup xSemaphoreTake xSemaphoreTake
 * \ingroup Semaphores
 */
#define xSemaphoreTake( xSemaphore, xBlockTime )		xQueueSemaphoreTake( ( xSemaphore ), ( xBlockTime ) )

/**
 * 塞姆弗。 h
 * xSemaphoreTakeRecursive(
 * SemaphoreHandle_t
 * xMutex, TickType_t xBlockTime
 *                        )
 *
 * <i>宏</i>递归地获取或“获取”互斥类
 * 型信号量。互斥锁必须是之前通过调用创建的
 * xSemaphoreCreateRecursiveMutex();
 *
 * FreeRTOSConfig.h 中的
 * configUSE_RECURSIVE_MUTEXES 必须设置为 1，该宏才可用。
 *
 * 该宏不得用于使用 xSemaphoreCreateMutex() 创建的互斥锁。
 *
 * 递归使用的互斥锁可以被所有者重复“获取”。在所有者为每个成
 * 功的“获取”请求调用
 * xSemaphoreGiveRecursive() 之前，互斥体不会再次变得可用。  例如，
 * if a task successfully 'takes' the same mutex 5 times then the mutex will
 * 不能用于任何其他任务，直到它
 * 也“返回”互斥锁五次为止。
 *
 * @param xMutex 正在获取的互斥体的句柄。  这是
 * xSemaphoreCreateRecursiveMutex() 返回的句柄；
 *
 * @param xBlockTime 等待信号量变为的时间（以刻度为单位）
 * 可用。  宏 portTICK_PERIOD_MS 可用于将其转
 * 换为实时。  零块时间可用于轮询信号量。  如果任务已经拥有信号
 * 量，则 xSemaphoreTakeRecursive() 将
 * return immediately no matter what the value of xBlockTime.
 *
 * @return pdTRUE 如果获得了信号量。  pdFALSE 如果 xBlockTime
 * 信号量过期且信号量不可用。
 *
 * 用法示例：
 <pre>
 SemaphoreHandle_t xMutex = NULL;

 //创建互斥锁的任务。
 void vATask( void * pvParameters )
 {
    //创建互斥锁来保护共享资源。
    xMutex = xSemaphoreCreateRecursiveMutex();
 }

 //使用互斥锁的任务。
 void vAnotherTask( void * pvParameters )
 {
    //……做其他事情。

    if( xMutex != NULL )
    {
        //看看我们是否可以获得互斥锁。  如果互斥体不可
        //用，请等待 10 个周期，看看它是否变为可用。
        if( xSemaphoreTakeRecursive( xSemaphore, ( TickType_t ) 10 ) == pdTRUE )
        {
            //我们能够获取互斥锁，现
            //在可以访问共享资源。

            //...
            //由于代码的性质，由于某种原因，在同一个互斥体
            //上进一步调用
            //xSemaphoreTakeRecursive()
            //。  在实际代码中，这些不仅仅是顺序调用，因
            //为这没有意义。  相反，这些调用可能隐藏在更复杂的调用结构中。
            xSemaphoreTakeRecursive( xMutex, ( TickType_t ) 10 );
            xSemaphoreTakeRecursive( xMutex, ( TickType_t ) 10 );

            //互斥锁现已被“获取”了 3 次，因此在
            //也被归还 3 次之前，该互斥锁将无法用
            //于其他任务。  同样，实际代码不太可能
            //按顺序进行这些调用，而是隐藏在更复杂的
            //调用结构中。  这仅用于说明目的。
            xSemaphoreGiveRecursive( xMutex );
            xSemaphoreGiveRecursive( xMutex );
            xSemaphoreGiveRecursive( xMutex );

            //现在互斥体可以被其他任务占用。
        }
        else
        {
            //我们无法获取互斥体，因此
            //无法安全地访问共享资源。
        }
    }
 }
 </pre>
 * \defgroup xSemaphoreTakeRecursive xSemaphoreTakeRecursive
 * \ingroup Semaphores
 */
#if( configUSE_RECURSIVE_MUTEXES == 1 )
	#define xSemaphoreTakeRecursive( xMutex, xBlockTime )	xQueueTakeMutexRecursive( ( xMutex ), ( xBlockTime ) )
#endif

/**
 * semphr.h
 * <pre>xSemaphoreGive( SemaphoreHandle_t xSemaphore )</pre>
 *
 * <i>宏</i>释放信号量。  信号量必须事先通过调用
 * xSemaphoreCreateBinary()、
 * xSemaphoreCreateMutex() 或 xSemaphoreCreateCounting() 创建。并使用 sSemaphoreTake() 获得。
 *
 * 该宏不得在 ISR 中使用。  请参阅
 * xSemaphoreGiveFromISR () 了解可在 ISR 中使用的替代方案。
 *
 * 该宏也不得用于使用
 * xSemaphoreCreateRecursiveMutex() 创建的信号量。
 *
 * @param xSemaphore 正在释放的信号量的句柄。  这是
 * 创建信号量时返回的句柄。
 *
 * @return pdTRUE 如果信号量已释放。  pdFALSE 如果发生错误。
 * 信号量是使用队列实现的。  如果队列
 * 上没有空间来发布消息，则可能会发生错
 * 误 - 表明未首先正确获取信号量。
 *
 * 用法示例：
 <pre>
 SemaphoreHandle_t xSemaphore = NULL;

 void vATask( void * pvParameters )
 {
    //创建信号量来保护共享资源。
    xSemaphore = vSemaphoreCreateBinary();

    if( xSemaphore != NULL )
    {
        if( xSemaphoreGive( xSemaphore ) != pdTRUE )
        {
            //我们预计此调用会失败，因为如果不先“
            //获取”信号量，我们就无法给出信号量！
        }

        //获取信号量 - 如果信号量
        //不能立即可用，则不要阻塞。
        if( xSemaphoreTake( xSemaphore, ( TickType_t ) 0 ) )
        {
            //我们现在有了信号量并且可以访问共享资源。

            //...

            //我们已经完成了对共享资源的访问，因此可以释放
            //semaphore.
            if( xSemaphoreGive( xSemaphore ) != pdTRUE )
            {
                //我们不希望这个调用失败，因为我
                //们必须获得信号量才能到达这里。
            }
        }
    }
 }
 </pre>
 * \defgroup xSemaphoreGive xSemaphoreGive
 * \ingroup Semaphores
 */
#define xSemaphoreGive( xSemaphore )		xQueueGenericSend( ( QueueHandle_t ) ( xSemaphore ), NULL, semGIVE_BLOCK_TIME, queueSEND_TO_BACK )

/**
 * semphr.h
 * <pre>xSemaphoreGiveRecursive( SemaphoreHandle_t xMutex )</pre>
 *
 * <i>宏</i>递归地释放或“给出”互斥类
 * 型信号量。互斥锁必须是之前通过调用创建的
 * xSemaphoreCreateRecursiveMutex();
 *
 * FreeRTOSConfig.h 中的
 * configUSE_RECURSIVE_MUTEXES 必须设置为 1，该宏才可用。
 *
 * 该宏不得用于使用 xSemaphoreCreateMutex() 创建的互斥锁。
 *
 * 递归使用的互斥锁可以被所有者重复“获取”。在所有者为每个成
 * 功的“获取”请求调用
 * xSemaphoreGiveRecursive() 之前，互斥体不会再次变得可用。  例如，
 * if a task successfully 'takes' the same mutex 5 times then the mutex will
 * 不能用于任何其他任务，直到它
 * 也“返回”互斥锁五次为止。
 *
 * @param xMutex 正在释放或“给出”的互斥体的句柄。  这是
 * xSemaphoreCreateMutex() 返回的句柄；
 *
 * @return pdTRUE（如果给出了信号量）。
 *
 * 用法示例：
 <pre>
 SemaphoreHandle_t xMutex = NULL;

 //创建互斥锁的任务。
 void vATask( void * pvParameters )
 {
    //创建互斥锁来保护共享资源。
    xMutex = xSemaphoreCreateRecursiveMutex();
 }

 //使用互斥锁的任务。
 void vAnotherTask( void * pvParameters )
 {
    //……做其他事情。

    if( xMutex != NULL )
    {
        //看看我们是否可以获得互斥锁。  如果互斥体不可
        //用，请等待 10 个周期，看看它是否变为可用。
        if( xSemaphoreTakeRecursive( xMutex, ( TickType_t ) 10 ) == pdTRUE )
        {
            //我们能够获取互斥锁，现
            //在可以访问共享资源。

            //...
            //由于代码的性质，由于某种原因，在同一个互斥体
			//上进一步调用
			//xSemaphoreTakeRecursive()
			//。  在实际代码中，这些不仅仅是顺序调用，因
			//为这没有意义。  相反，这些调用可能隐藏在更复杂的调用结构中。
            xSemaphoreTakeRecursive( xMutex, ( TickType_t ) 10 );
            xSemaphoreTakeRecursive( xMutex, ( TickType_t ) 10 );

            //互斥锁现已被“获取”了 3 次，因此在也被
			//归还 3 次之前，该互斥锁将无法用于其他任
			//务。  同样，实际代码不太可能按顺序进行这
			//些调用，对
			//xSemaphoreGiveRecursive()
			//的调用更有可能作为调用堆栈展开进行调用。  这仅用于演示目的。
            xSemaphoreGiveRecursive( xMutex );
			xSemaphoreGiveRecursive( xMutex );
			xSemaphoreGiveRecursive( xMutex );

			//现在互斥体可以被其他任务占用。
        }
        else
        {
            //我们无法获取互斥体，因此
            //无法安全地访问共享资源。
        }
    }
 }
 </pre>
 * \defgroup xSemaphoreGiveRecursive xSemaphoreGiveRecursive
 * \ingroup Semaphores
 */
#if( configUSE_RECURSIVE_MUTEXES == 1 )
	#define xSemaphoreGiveRecursive( xMutex )	xQueueGiveMutexRecursive( ( xMutex ) )
#endif

/**
 * semphr.h
 * <pre>
 xSemaphoreGiveFromISR(
                          SemaphoreHandle_t xSemaphore,
                          BaseType_t *pxHigherPriorityTaskWoken
                      )</pre>
 *
 * <i>宏</i>释放信号量。  信号量之前必须通过调用
 * xSemaphoreCreateBinary() 或 xSemaphoreCreateCounting() 创建。
 *
 * 互斥体类型 semaphores（通过调用
 * xSemaphoreCreateMutex() 创建的互斥体类型）不得与此宏一起使用。
 *
 * 该宏可在 ISR 上使用。
 *
 * @param xSemaphore 正在释放的信号量的句柄。  这是
 * 创建信号量时返回的句柄。
 *
 * @param pxHigherPriorityTaskWoken xSemaphoreGiveFromISR() 将设置
 * *pxHigherPriorityTaskWoken 到 pdTRUE
 * 如果给出信号量导致任务解除阻塞，并且解除阻塞的任务的优先级高于当前正
 * 在运行的任务。  如果 xSemaphoreGiveFromISR()
 * 将此值设置为 pdTRUE，则应在退出中断之前请求上下文切换。
 *
 * 如果成功给出信号量，则为 @return pdTRUE，否则为 errQUEUE_FULL。
 *
 * 用法示例：
 <pre>
 \#define LONG_TIME 0xffff
 \#define TICKS_TO_WAIT	10
 SemaphoreHandle_t xSemaphore = NULL;

 //重复性任务。
 void vATask( void * pvParameters )
 {
    for( ;; )
    {
        //我们希望该任务每 10 个计时器滴答运行一
        //次。  信号量是在该任务开始之前创建的。

        //阻止等待信号量变得可用。
        if( xSemaphoreTake( xSemaphore, LONG_TIME ) == pdTRUE )
        {
            //是时候执行了。

            //...

            //我们已经完成了我们的任务。  返回到循环
            //的顶部，我们将阻塞信号量，直到再次执行为
            //止。  请注意，当以这种方式使用信号量与
			//ISR 同步时，无需“返还”信号量。
        }
    }
 }

 //定时器ISR
 void vTimerISR( void * pvParameters )
 {
 static uint8_t ucLocalTickCount = 0;
 static BaseType_t xHigherPriorityTaskWoken;

    //计时器发生了计时。

    //...执行其他时间功能。

    //vATask() 是时候运行了吗？
	xHigherPriorityTaskWoken = pdFALSE;
    ucLocalTickCount++;
    if( ucLocalTickCount >= TICKS_TO_WAIT )
    {
        //通过释放信号量来解锁任务。
        xSemaphoreGiveFromISR( xSemaphore, &xHigherPriorityTaskWoken );

        //重置计数，以便我们在 10 个周期内再次释放信号量。
        ucLocalTickCount = 0;
    }

    if( xHigherPriorityTaskWoken != pdFALSE )
    {
        //我们可以在这里强制进行上下文切换。
        //ISR 的上下文切换使用端口特定语法。
        //检查您的端口的演示任务以查找所需的语法。
    }
 }
 </pre>
 * \defgroup xSemaphoreGiveFromISR xSemaphoreGiveFromISR
 * \ingroup Semaphores
 */
#define xSemaphoreGiveFromISR( xSemaphore, pxHigherPriorityTaskWoken )	xQueueGiveFromISR( ( QueueHandle_t ) ( xSemaphore ), ( pxHigherPriorityTaskWoken ) )

/**
 * semphr.h
 * <pre>
 xSemaphoreTakeFromISR(
                          SemaphoreHandle_t xSemaphore,
                          BaseType_t *pxHigherPriorityTaskWoken
                      )</pre>
 *
 * <i>宏</i>从 ISR 获取信号量。  信号量之前必须通过
 * 调用 xSemaphoreCreateBinary() 或
 * xSemaphoreCreateCounting() 创建。
 *
 * 互斥体类型 semaphores（通过调用
 * xSemaphoreCreateMutex() 创建的互斥体类型）不得与此宏一起使用。
 *
 * 该宏可以在 ISR 中使用，但是从 ISR
 * 获取信号量并不是常见的操作。  它可能仅在中
 * 断从资源 pool 获取对象时获取计数信号量
 * 时有用（当信号量计数指示可用资源数量时）。
 *
 * @param xSemaphore 正在获取的信号量的句柄。  这是
 * 创建信号量时返回的句柄。
 *
 * @param pxHigherPriorityTaskWoken xSemaphoreTakeFromISR() 将设置
 * *pxHigherPriorityTaskWoken 到 pdTRUE
 * 如果获取信号量导致任务解除阻塞，并且解除阻塞的任务的优先级高于当前正
 * 在运行的任务。  如果 xSemaphoreTakeFromISR()
 * 将此值设置为 pdTRUE，则应在退出中断之前请求上下文切换。
 *
 * @return pdTRUE 如果信号量成功获取，否则
 * pdFALSE
 */
#define xSemaphoreTakeFromISR( xSemaphore, pxHigherPriorityTaskWoken )	xQueueReceiveFromISR( ( QueueHandle_t ) ( xSemaphore ), NULL, ( pxHigherPriorityTaskWoken ) )

/**
 * semphr.h
 * <pre>SemaphoreHandle_t xSemaphoreCreateMutex( void )</pre>
 *
 * 创建一个新的互斥体类型信号量实例，
 * 并返回一个可以引用新互斥体的句柄。
 *
 * 在内部，在 FreeRTOS 实现中，互斥信号量使用存储互斥结
 * 构的内存块。  如果使用
 * xSemaphoreCreateMutex() 创建互斥体，则
 * 所需的内存会在 xSemaphoreCreateMutex() 函数内自动动态分配。  （见
 * http://www.freertos.org/a00111.html). 如果互斥体是使用创建的
 * xSemaphoreCreateMutexStatic() 那么应用程
 * 序编写者必须提供内存。  因此，
 * xSemaphoreCreateMutexStatic() 允许在不使用任何动态内存分配的情况下创建互斥体。
 *
 * Mutexes created using this function can be accessed using the xSemaphoreTake()
 * 和 xSemaphoreGive() 宏。  不得使用
 * xSemaphoreTakeRecursive() 和 xSemaphoreGiveRecursive() 宏。
 *
 * 这种类型的信号量使用优先级继承机制，因此，一
 * 旦不再需要信号量，“获取”信号量 MUST
 * ALWAYS 的任务就会“返还”信号量。
 *
 * 互斥类型信号量不能在中断服务例程中使用。
 *
 * 请参阅
 * xSemaphoreCreateBinary()，了解
 * 可用于纯 synchronisation（其中一个任务
 * 或中断始终“给出”信号量，另一个任务或中断始终“获取”信号量）并在中断服务例程中使用的替代实现。
 *
 * @return 如果互斥锁已成功创建，则创建的互斥锁的句柄
 * 返回信号量。  如果没有足够的堆来分
 * 配互斥体数据结构，则返回 NULL。
 *
 * 用法示例：
 <pre>
 SemaphoreHandle_t xSemaphore;

 void vATask( void * pvParameters )
 {
    //在调用 xSemaphoreCreateMutex()
    //之前不能使用信号量。这是一个宏，因此直接传递变量。
    xSemaphore = xSemaphoreCreateMutex();

    if( xSemaphore != NULL )
    {
        //信号量创建成功。现在
        //可以使用信号量了。
    }
 }
 </pre>
 * \defgroup xSemaphoreCreateMutex xSemaphoreCreateMutex
 * \ingroup Semaphores
 */
#if( configSUPPORT_DYNAMIC_ALLOCATION == 1 )
	#define xSemaphoreCreateMutex() xQueueCreateMutex( queueQUEUE_TYPE_MUTEX )
#endif

/**
 * semphr.h
 * <pre>SemaphoreHandle_t xSemaphoreCreateMutexStatic( StaticSemaphore_t *pxMutexBuffer )</pre>
 *
 * 创建一个新的互斥体类型信号量实例，
 * 并返回一个可以引用新互斥体的句柄。
 *
 * 在内部，在 FreeRTOS 实现中，互斥信号量使用存储互斥结
 * 构的内存块。  如果使用
 * xSemaphoreCreateMutex() 创建互斥体，则
 * 所需的内存会在 xSemaphoreCreateMutex() 函数内自动动态分配。  （见
 * http://www.freertos.org/a00111.html). 如果互斥体是使用创建的
 * xSemaphoreCreateMutexStatic() 那么应用程
 * 序编写者必须提供内存。  因此，
 * xSemaphoreCreateMutexStatic() 允许在不使用任何动态内存分配的情况下创建互斥体。
 *
 * Mutexes created using this function can be accessed using the xSemaphoreTake()
 * 和 xSemaphoreGive() 宏。  不得使用
 * xSemaphoreTakeRecursive() 和 xSemaphoreGiveRecursive() 宏。
 *
 * 这种类型的信号量使用优先级继承机制，因此，一
 * 旦不再需要信号量，“获取”信号量 MUST
 * ALWAYS 的任务就会“返还”信号量。
 *
 * 互斥类型信号量不能在中断服务例程中使用。
 *
 * 请参阅
 * xSemaphoreCreateBinary()，了解
 * 可用于纯 synchronisation（其中一个任务
 * 或中断始终“给出”信号量，另一个任务或中断始终“获取”信号量）并在中断服务例程中使用的替代实现。
 *
 * @param pxMutexBuffer 必须指向 StaticSemaphore_t 类型的变量，
 * 它将用于保存互斥体的数据结
 * 构，从而无需动态分配内存。
 *
 * @return 如果互斥锁已成功创建，则创建的互斥锁的句柄
 * 返回互斥体。  如果 pxMutexBuffer 是 NULL，则返回 NULL。
 *
 * 用法示例：
 <pre>
 SemaphoreHandle_t xSemaphore;
 StaticSemaphore_t xMutexBuffer;

 void vATask( void * pvParameters )
 {
    //互斥体在创建之前不能使用。  xMutexBuffer 位于
    //xSemaphoreCreateMutexStatic() 中，因此没有动态内存分配
    //attempted.
    xSemaphore = xSemaphoreCreateMutexStatic( &xMutexBuffer );

    //由于没有进行动态内存分配，所以
    //xSemaphore不可能是NULL，所以不需要检查。
 }
 </pre>
 * \defgroup xSemaphoreCreateMutexStatic xSemaphoreCreateMutexStatic
 * \ingroup Semaphores
 */
 #if( configSUPPORT_STATIC_ALLOCATION == 1 )
	#define xSemaphoreCreateMutexStatic( pxMutexBuffer ) xQueueCreateMutexStatic( queueQUEUE_TYPE_MUTEX, ( pxMutexBuffer ) )
#endif /* configSUPPORT_STATIC_ALLOCATION */


/**
 * semphr.h
 * <pre>SemaphoreHandle_t xSemaphoreCreateRecursiveMutex( void )</pre>
 *
 * 创建一个新的递归互斥体类型信号量实例，并返回
 * 一个句柄，通过该句柄可以引用新的递归互斥体。
 *
 * 在 FreeRTOS 实现内部，递归互斥体使用存储互斥体
 * 结构的内存块。  如果使用
 * xSemaphoreCreateRecursiveMutex()
 * 创建递归互斥体，则所需的内存会在
 * xSemaphoreCreateRecursiveMutex() 函数内自动动态分配。  （见
 * http://www.freertos.org/a00111.html). 如果使用创建递归互斥体
 * xSemaphoreCreateRecursiveMutexStatic()
 * 那么应用程序编写者必须提供互斥体将使用的内存。因此，
 * xSemaphoreCreateRecursiveMutexStatic()
 * 允许在不使用任何动态内存分配的情况下创建递归互斥锁。
 *
 * 使用此宏创建的互斥体可以使用 xSemaphoreTakeRecursive()
 * 和 xSemaphoreGiveRecursive() 宏进行访问。  不得使
 * 用 xSemaphoreTake() 和 xSemaphoreGive() 宏。
 *
 * 递归使用的互斥锁可以被所有者重复“获取”。在所有者为每个成
 * 功的“获取”请求调用
 * xSemaphoreGiveRecursive() 之前，互斥体不会再次变得可用。  例如，
 * if a task successfully 'takes' the same mutex 5 times then the mutex will
 * 不能用于任何其他任务，直到它
 * 也“返回”互斥锁五次为止。
 *
 * 这种类型的信号量使用优先级继承机制，因此，一
 * 旦不再需要信号量，“获取”信号量 MUST
 * ALWAYS 的任务就会“返还”信号量。
 *
 * 互斥类型信号量不能在中断服务例程中使用。
 *
 * 请参阅
 * xSemaphoreCreateBinary()，了解
 * 可用于纯 synchronisation（其中一个任务
 * 或中断始终“给出”信号量，另一个任务或中断始终“获取”信号量）并在中断服务例程中使用的替代实现。
 *
 * @return xSemaphore 创建的互斥信号量的句柄。  应该是类型
 * SemaphoreHandle_t.
 *
 * 用法示例：
 <pre>
 SemaphoreHandle_t xSemaphore;

 void vATask( void * pvParameters )
 {
    //在调用 xSemaphoreCreateMutex()
    //之前不能使用信号量。这是一个宏，因此直接传递变量。
    xSemaphore = xSemaphoreCreateRecursiveMutex();

    if( xSemaphore != NULL )
    {
        //信号量创建成功。现在
        //可以使用信号量了。
    }
 }
 </pre>
 * \defgroup xSemaphoreCreateRecursiveMutex xSemaphoreCreateRecursiveMutex
 * \ingroup Semaphores
 */
#if( ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) && ( configUSE_RECURSIVE_MUTEXES == 1 ) )
	#define xSemaphoreCreateRecursiveMutex() xQueueCreateMutex( queueQUEUE_TYPE_RECURSIVE_MUTEX )
#endif

/**
 * semphr.h
 * <pre>SemaphoreHandle_t xSemaphoreCreateRecursiveMutexStatic( StaticSemaphore_t *pxMutexBuffer )</pre>
 *
 * 创建一个新的递归互斥体类型信号量实例，并返回
 * 一个句柄，通过该句柄可以引用新的递归互斥体。
 *
 * 在 FreeRTOS 实现内部，递归互斥体使用存储互斥体
 * 结构的内存块。  如果使用
 * xSemaphoreCreateRecursiveMutex()
 * 创建递归互斥体，则所需的内存会在
 * xSemaphoreCreateRecursiveMutex() 函数内自动动态分配。  （见
 * http://www.freertos.org/a00111.html). 如果使用创建递归互斥体
 * xSemaphoreCreateRecursiveMutexStatic()
 * 那么应用程序编写者必须提供互斥体将使用的内存。因此，
 * xSemaphoreCreateRecursiveMutexStatic()
 * 允许在不使用任何动态内存分配的情况下创建递归互斥锁。
 *
 * 使用此宏创建的互斥体可以使用 xSemaphoreTakeRecursive()
 * 和 xSemaphoreGiveRecursive() 宏进行访问。  不得使
 * 用 xSemaphoreTake() 和 xSemaphoreGive() 宏。
 *
 * 递归使用的互斥锁可以被所有者重复“获取”。在所有者为每个成
 * 功的“获取”请求调用
 * xSemaphoreGiveRecursive() 之前，互斥体不会再次变得可用。  例如，
 * if a task successfully 'takes' the same mutex 5 times then the mutex will
 * 不能用于任何其他任务，直到它
 * 也“返回”互斥锁五次为止。
 *
 * 这种类型的信号量使用优先级继承机制，因此，一
 * 旦不再需要信号量，“获取”信号量 MUST
 * ALWAYS 的任务就会“返还”信号量。
 *
 * 互斥类型信号量不能在中断服务例程中使用。
 *
 * 请参阅
 * xSemaphoreCreateBinary()，了解
 * 可用于纯 synchronisation（其中一个任务
 * 或中断始终“给出”信号量，另一个任务或中断始终“获取”信号量）并在中断服务例程中使用的替代实现。
 *
 * @param pxMutexBuffer 必须指向 StaticSemaphore_t 类型的变量，
 * 然后它将用于保存递归互斥体的数
 * 据结构，从而无需动态分配内存。
 *
 * @return 如果递归互斥锁已成功创建，则该互斥锁的句柄
 * 返回创建的递归互斥体。  如果 pxMutexBuffer 是 NULL 那么 NULL 是
 * 回来了。
 *
 * 用法示例：
 <pre>
 SemaphoreHandle_t xSemaphore;
 StaticSemaphore_t xMutexBuffer;

 void vATask( void * pvParameters )
 {
    //递归信号量在创建之前不能使用。  这里使用
    //xSemaphoreCreateRecursiveMutexStatic()
    //创建了一个递归互斥体。 xMutexBuffer 的地址被传
    //递到函数中，并将保存互斥体数据结构 - 因此不会进行动态内存分配
    //attempted.
    xSemaphore = xSemaphoreCreateRecursiveMutexStatic( &xMutexBuffer );

    //由于没有进行动态内存分配，所以
    //xSemaphore不可能是NULL，所以不需要检查。
 }
 </pre>
 * \defgroup xSemaphoreCreateRecursiveMutexStatic xSemaphoreCreateRecursiveMutexStatic
 * \ingroup Semaphores
 */
#if( ( configSUPPORT_STATIC_ALLOCATION == 1 ) && ( configUSE_RECURSIVE_MUTEXES == 1 ) )
	#define xSemaphoreCreateRecursiveMutexStatic( pxStaticSemaphore ) xQueueCreateMutexStatic( queueQUEUE_TYPE_RECURSIVE_MUTEX, pxStaticSemaphore )
#endif /* configSUPPORT_STATIC_ALLOCATION */

/**
 * semphr.h
 * <pre>SemaphoreHandle_t xSemaphoreCreateCounting( UBaseType_t uxMaxCount, UBaseType_t uxInitialCount )</pre>
 *
 * 创建一个新的计数信号量实例，并返回一个句
 * 柄，通过该句柄可以引用新的计数信号量。
 *
 * 在许多使用场景中，使用直接任务通知代
 * 替计数信号量速度更快、内存效率更高！
 * http://www.freertos.org/RTOS-task-notifications.html
 *
 * 在内部，在 FreeRTOS 实现中，计数信号量使用存
 * 储计数信号量结构的内存块。  如果使用
 * xSemaphoreCreateCounting()
 * 创建计数信号量，则所需内存将在
 * xSemaphoreCreateCounting() 函数内自动动态分配。  （看
 * http://www.freertos.org/a00111.html). 如果创建计数信号量
 * 使用
 * xSemaphoreCreateCountingStatic()
 * 那么应用程序编写者可以选择提供将由计数信号量使用的内存。  因
 * 此，xSemaphoreCreateCountingStatic() 允许在不使用任何动态内存分配的情况下创建计数信号量。
 *
 * 计数信号量通常用于两件事：
 *
 * 1) 计数事件。
 *
 * 在此使用场景中，事件处理程序将在每次事
 * 件 occurs 时“给出”信号量（增
 * 加信号量计数值），处理程序任务将在每次
 * 处理 event 时“获取”信号量（减
 * 少信号量计数值）。  因此，计数值是已
 * 发生的事件数与已处理的事件数之间的差。
 * 在这种情况下，期望初始计数值为零。
 *
 * 2）资源管理。
 *
 * 在此使用场景中，计数值指示可用资源的数量。
 * 为了获得对资源的控制，任务必须首先获得
 * 信号量 - 递减信号量计数值。  当计数值
 * 达到零时，就没有可用资源。  当任务完成资
 * 源使用时，它会“返回”信号量 - 增加信号
 * 量计数值。  在这种情况下，期望初始计数值
 * 等于最大计数值，指示所有资源都是空闲的。
 *
 * @param uxMaxCount 可以达到的最大计数值。  当
 * 信号量达到这个值就不能再“给出”。
 *
 * @param uxInitialCount 当信号量为0时分配给信号量的计数值
 * 创建的。
 *
 * @return 创建的信号量的句柄。  如果信号量不能为空，则为空
 * 创建的。
 *
 * 用法示例：
 <pre>
 SemaphoreHandle_t xSemaphore;

 void vATask( void * pvParameters )
 {
 SemaphoreHandle_t xSemaphore = NULL;

    //在调用
    //xSemaphoreCreateCounting()
    //之前不能使用信号量。信号量可计数的最大值应为 10，分配给计数的初始值应为 0。
    xSemaphore = xSemaphoreCreateCounting( 10, 0 );

    if( xSemaphore != NULL )
    {
        //信号量创建成功。现在
        //可以使用信号量了。
    }
 }
 </pre>
 * \defgroup xSemaphoreCreateCounting xSemaphoreCreateCounting
 * \ingroup Semaphores
 */
#if( configSUPPORT_DYNAMIC_ALLOCATION == 1 )
	#define xSemaphoreCreateCounting( uxMaxCount, uxInitialCount ) xQueueCreateCountingSemaphore( ( uxMaxCount ), ( uxInitialCount ) )
#endif

/**
 * semphr.h
 * <pre>SemaphoreHandle_t xSemaphoreCreateCountingStatic( UBaseType_t uxMaxCount, UBaseType_t uxInitialCount, StaticSemaphore_t *pxSemaphoreBuffer )</pre>
 *
 * 创建一个新的计数信号量实例，并返回一个句
 * 柄，通过该句柄可以引用新的计数信号量。
 *
 * 在许多使用场景中，使用直接任务通知代
 * 替计数信号量速度更快、内存效率更高！
 * http://www.freertos.org/RTOS-task-notifications.html
 *
 * 在内部，在 FreeRTOS 实现中，计数信号量使用存
 * 储计数信号量结构的内存块。  如果使用
 * xSemaphoreCreateCounting()
 * 创建计数信号量，则所需内存将在
 * xSemaphoreCreateCounting() 函数内自动动态分配。  （看
 * http://www.freertos.org/a00111.html). 如果创建计数信号量
 * 使用 xSemaphoreCreateCountingStatic() 那么应
 * 用程序编写者必须提供内存。  因此，
 * xSemaphoreCreateCountingStatic() 允许在不使用任何动态内存分配的情况下创建计数信号量。
 *
 * 计数信号量通常用于两件事：
 *
 * 1) 计数事件。
 *
 * 在此使用场景中，事件处理程序将在每次事
 * 件 occurs 时“给出”信号量（增
 * 加信号量计数值），处理程序任务将在每次
 * 处理 event 时“获取”信号量（减
 * 少信号量计数值）。  因此，计数值是已
 * 发生的事件数与已处理的事件数之间的差。
 * 在这种情况下，期望初始计数值为零。
 *
 * 2）资源管理。
 *
 * 在此使用场景中，计数值指示可用资源的数量。
 * 为了获得对资源的控制，任务必须首先获得
 * 信号量 - 递减信号量计数值。  当计数值
 * 达到零时，就没有可用资源。  当任务完成资
 * 源使用时，它会“返回”信号量 - 增加信号
 * 量计数值。  在这种情况下，期望初始计数值
 * 等于最大计数值，指示所有资源都是空闲的。
 *
 * @param uxMaxCount 可以达到的最大计数值。  当
 * 信号量达到这个值就不能再“给出”。
 *
 * @param uxInitialCount 当信号量为0时分配给信号量的计数值
 * 创建的。
 *
 * @param pxSemaphoreBuffer 必须指向 StaticSemaphore_t 类型的变量，
 * 然后它将用于保存信号量的数据
 * 结构，从而无需动态分配内存。
 *
 * @return 如果计数信号量已成功创建，则为
 * 返回创建的计数信号量。  如果
 * pxSemaphoreBuffer 是 NULL，则返回 NULL。
 *
 * 用法示例：
 <pre>
 SemaphoreHandle_t xSemaphore;
 StaticSemaphore_t xSemaphoreBuffer;

 void vATask( void * pvParameters )
 {
 SemaphoreHandle_t xSemaphore = NULL;

    //计数信号量在创建之前不能使用。  使用
    //xSemaphoreCreateCountingStatic()
    //创建计数信号量。  信号量可计数的最大值为 1
    //0，分配给计数的初始值为 0。传入
    //xSemaphoreBuffer 的地址，该地址
    //将用于保存信号量结构，因此不会使用动态内存分配。
    xSemaphore = xSemaphoreCreateCounting( 10, 0, &xSemaphoreBuffer );

    //未尝试分配内存，因此
    //xSemaphore 不能是 NULL，因此无需检查其值。
 }
 </pre>
 * \defgroup xSemaphoreCreateCountingStatic xSemaphoreCreateCountingStatic
 * \ingroup Semaphores
 */
#if( configSUPPORT_STATIC_ALLOCATION == 1 )
	#define xSemaphoreCreateCountingStatic( uxMaxCount, uxInitialCount, pxSemaphoreBuffer ) xQueueCreateCountingSemaphoreStatic( ( uxMaxCount ), ( uxInitialCount ), ( pxSemaphoreBuffer ) )
#endif /* configSUPPORT_STATIC_ALLOCATION */

/**
 * semphr.h
 * <pre>void vSemaphoreDelete( SemaphoreHandle_t xSemaphore );</pre>
 *
 * 删除信号量。  该功能必须谨慎使用。  例如，如
 * 果互斥锁由任务持有，则不要删除互斥锁类型信号量。
 *
 * @param xSemaphore 要删除的信号量的句柄。
 *
 * \defgroup vSemaphoreDelete vSemaphoreDelete
 * \ingroup Semaphores
 */
#define vSemaphoreDelete( xSemaphore ) vQueueDelete( ( QueueHandle_t ) ( xSemaphore ) )

/**
 * semphr.h
 * <pre>TaskHandle_t xSemaphoreGetMutexHolder( SemaphoreHandle_t xMutex );</pre>
 *
 * 如果 xMutex 确实是互斥锁类型信号量，则返回当前互斥
 * 锁持有者。如果xMutex不是互斥量类型信号量，或者互斥量
 * 是available（未被任务持有），则返回NULL。
 *
 * 注意：这是确定调用任务是否是互斥锁持
 * 有者的好方法，但不是确定互斥锁持有者
 * 身份的好方法，因为持有者可能会在函数
 * 退出和正在测试的返回值之间发生变化。
 */
#define xSemaphoreGetMutexHolder( xSemaphore ) xQueueGetMutexHolder( ( xSemaphore ) )

/**
 * semphr.h
 * <pre>TaskHandle_t xSemaphoreGetMutexHolderFromISR( SemaphoreHandle_t xMutex );</pre>
 *
 * 如果 xMutex 确实是互斥锁类型信号量，则返回当前互斥
 * 锁持有者。如果xMutex不是互斥量类型信号量，或者互斥量
 * 是available（未被任务持有），则返回NULL。
 *
 */
#define xSemaphoreGetMutexHolderFromISR( xSemaphore ) xQueueGetMutexHolderFromISR( ( xSemaphore ) )

/**
 * semphr.h
 * <pre>UBaseType_t uxSemaphoreGetCount( SemaphoreHandle_t xSemaphore );</pre>
 *
 * 如果信号量是计数信号量，则
 * uxSemaphoreGetCount() 返回其当前计数
 * 值。  如果信号量是二进制信号量，则如果信号量可用，则
 * uxSemaphoreGetCount() 返回 1；如果信号量不可用，则返回 0。
 *
 */
#define uxSemaphoreGetCount( xSemaphore ) uxQueueMessagesWaiting( ( QueueHandle_t ) ( xSemaphore ) )

#endif /* SEMAPHORE_H */

