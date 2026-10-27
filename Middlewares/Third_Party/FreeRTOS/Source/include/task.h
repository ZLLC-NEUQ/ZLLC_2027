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


#ifndef INC_TASK_H
#define INC_TASK_H

#ifndef INC_FREERTOS_H
	#error "include FreeRTOS.h must appear in source files before include task.h"
#endif

#include "list.h"

#ifdef __cplusplus
extern "C" {
#endif

/*-----------------------------------------------------------
 * 宏和定义
 *----------------------------------------------------------*/

#define tskKERNEL_VERSION_NUMBER "V10.3.1"
#define tskKERNEL_VERSION_MAJOR 10
#define tskKERNEL_VERSION_MINOR 3
#define tskKERNEL_VERSION_BUILD 1

/* MPU 区域参数在 MemoryRegion_t
 * 结构体的 ulParameters 中传递。 */
#define tskMPU_REGION_READ_ONLY			( 1UL << 0UL )
#define tskMPU_REGION_READ_WRITE		( 1UL << 1UL )
#define tskMPU_REGION_EXECUTE_NEVER		( 1UL << 2UL )
#define tskMPU_REGION_NORMAL_MEMORY		( 1UL << 3UL )
#define tskMPU_REGION_DEVICE_MEMORY		( 1UL << 4UL )

/**
 * 任务。小时
 *
 * 引用任务的类型。  例如，调用 xTaskCreate
 * returns（通过指针参数）TaskHandle_t 变量
 * ，然后可以将其用作 vTaskDelete 的参数来删除任务。
 *
 * \defgroup TaskHandle_t TaskHandle_t
 * \ingroup Tasks
 */
struct tskTaskControlBlock; /* 旧的命名约定用于防止破坏内核感知调试器。 */
typedef struct tskTaskControlBlock* TaskHandle_t;

/*
 * 定义应用程序任务挂钩函数必须达到的原型
 * 符合。
 */
typedef BaseType_t (*TaskHookFunction_t)( void * );

/* eTaskGetState 返回的任务状态。 */
typedef enum
{
	eRunning = 0,	/* 任务正在查询自身的状态，因此必须正在运行。 */
	eReady,			/* 正在查询的任务位于已读或待处理就绪列表中。 */
	eBlocked,		/* 正在查询的任务处于阻塞状态。 */
	eSuspended,		/* 正在查询的任务处于 Suspending 状态，或者处于无限超时的 Blocked 状态。 */
	eDeleted,		/* 正在查询的任务已被删除，但其TCB尚未释放。 */
	eInvalid		/* 用作“无效状态”值。 */
} eTaskState;

/* 调用 vTaskNotify() 时可以执行的操作。 */
typedef enum
{
	eNoAction = 0,				/* 通知任务而不更新其通知值。 */
	eSetBits,					/* 设置任务通知值中的位。 */
	eIncrement,					/* 增加任务的通知值。 */
	eSetValueWithOverwrite,		/* 即使任务尚未读取先前的值，也将任务的通知值设置为特定值。 */
	eSetValueWithoutOverwrite	/* 如果任务已读取先前的值，则设置任务的通知值。 */
} eNotifyAction;

/*
 * 仅供内部使用。
 */
typedef struct xTIME_OUT
{
	BaseType_t xOverflowCount;
	TickType_t xTimeOnEntering;
} TimeOut_t;

/*
 * 定义使用 MPU 时分配给任务的内存范围。
 */
typedef struct xMEMORY_REGION
{
	void *pvBaseAddress;
	uint32_t ulLengthInBytes;
	uint32_t ulParameters;
} MemoryRegion_t;

/*
 * 创建 MPU 受保护任务所需的参数。
 */
typedef struct xTASK_PARAMETERS
{
	TaskFunction_t pvTaskCode;
	const char * const pcName;	/*lint !e971 仅字符串和单个字符允许使用非限定字符类型。 */
	configSTACK_DEPTH_TYPE usStackDepth;
	void *pvParameters;
	UBaseType_t uxPriority;
	StackType_t *puxStackBuffer;
	MemoryRegion_t xRegions[ portNUM_CONFIGURABLE_REGIONS ];
	#if ( ( portUSING_MPU_WRAPPERS == 1 ) && ( configSUPPORT_STATIC_ALLOCATION == 1 ) )
		StaticTask_t * const pxTaskBuffer;
	#endif
} TaskParameters_t;

/* 与
uxTaskGetSystemState()函数一起使用，返回系统中每个任务的状态。 */
typedef struct xTASK_STATUS
{
	TaskHandle_t xHandle;			/* 结构中其余信息与之相关的任务句柄。 */
	const char *pcTaskName;			/* 指向任务名称的指针。  如果任务在填充结构后被删除，则该值将无效！ */ /*lint !e971 仅字符串和单个字符允许使用非限定字符类型。 */
	UBaseType_t xTaskNumber;		/* 任务特有的数字。 */
	eTaskState eCurrentState;		/* 填充结构时任务存在的状态。 */
	UBaseType_t uxCurrentPriority;	/* 填充结构时任务的优先级为 running（可以继承）。 */
	UBaseType_t uxBasePriority;		/* 如果继承了任务的当前优先级，则任务将返回的优先级，以避免在获取互斥锁时出现无限制的优先级反转。  仅当 configUSE_MUTEXES 在 FreeRTOSConfig.h 中定义为 1 时才有效。 */
	uint32_t ulRunTimeCounter;		/* 到目前为止分配给任务的总运行时间，由运行时间统计时钟定义。  参见 http://www.freertos.org/rtos-run-time-stats.html. 仅当 configGENERATE_RUN_TIME_STATS 在 FreeRTOSConfig.h 中定义为 1 时有效。 */
	StackType_t *pxStackBase;		/* 指向任务堆栈区域的最低地址。 */
	configSTACK_DEPTH_TYPE usStackHighWaterMark;	/* 自创建任务以来为任务保留的最小堆栈空间量。  该值越接近零，任务就越接近溢出其堆栈。 */
} TaskStatus_t;

/* eTaskConfirmSleepModeStatus() 的可能返回值。 */
typedef enum
{
	eAbortSleep = 0,		/* 自调用 portSUPPORESS_TICKS_AND_SLEEP() 以来，任务已准备就绪或上下文切换挂起 - 中止进入睡眠模式。 */
	eStandardSleep,			/* 进入睡眠模式，该模式的持续时间不会超过预期的空闲时间。 */
	eNoTasksWaitingTimeout	/* 没有任务等待超时，因此可以安全地进入只能通过外部中断退出的睡眠模式。 */
} eSleepModeStatus;

/**
 * 定义空闲任务使用的优先级。  不得修改此内容。
 *
 * \ingroup TaskUtils
 */
#define tskIDLE_PRIORITY			( ( UBaseType_t ) 0U )

/**
 * 任务。小时
 *
 * 用于强制上下文切换的宏。
 *
 * \defgroup taskYIELD taskYIELD
 * \ingroup SchedulerControl
 */
#define taskYIELD()					portYIELD()

/**
 * 任务。小时
 *
 * 用于标记关键代码区域开始的宏。  当处于
 * 关键区域时，无法发生抢占式上下文切换。
 *
 * 注意：这可能会改变 stack（取决
 * 于便携式实现），因此必须小心使用！
 *
 * \defgroup taskENTER_CRITICAL taskENTER_CRITICAL
 * \ingroup SchedulerControl
 */
#define taskENTER_CRITICAL()		portENTER_CRITICAL()
#define taskENTER_CRITICAL_FROM_ISR() portSET_INTERRUPT_MASK_FROM_ISR()

/**
 * 任务。小时
 *
 * 用于标记关键代码区域结束的宏。  当处于
 * 关键区域时，无法发生抢占式上下文切换。
 *
 * 注意：这可能会改变 stack（取决
 * 于便携式实现），因此必须小心使用！
 *
 * \defgroup taskEXIT_CRITICAL taskEXIT_CRITICAL
 * \ingroup SchedulerControl
 */
#define taskEXIT_CRITICAL()			portEXIT_CRITICAL()
#define taskEXIT_CRITICAL_FROM_ISR( x ) portCLEAR_INTERRUPT_MASK_FROM_ISR( x )
/**
 * 任务。小时
 *
 * 用于禁用所有可屏蔽中断的宏。
 *
 * \defgroup taskDISABLE_INTERRUPTS taskDISABLE_INTERRUPTS
 * \ingroup SchedulerControl
 */
#define taskDISABLE_INTERRUPTS()	portDISABLE_INTERRUPTS()

/**
 * 任务。小时
 *
 * 启用微控制器中断的宏。
 *
 * \defgroup taskENABLE_INTERRUPTS taskENABLE_INTERRUPTS
 * \ingroup SchedulerControl
 */
#define taskENABLE_INTERRUPTS()		portENABLE_INTERRUPTS()

/* xTaskGetSchedulerState() 返回的定义。  当
configASSERT() 被定义为 assert() 语句中使用的常量时
，taskSCHEDULER_SUSPENDED 为 0 以生成更优化的代码。 */
#define taskSCHEDULER_SUSPENDED		( ( BaseType_t ) 0 )
#define taskSCHEDULER_NOT_STARTED	( ( BaseType_t ) 1 )
#define taskSCHEDULER_RUNNING		( ( BaseType_t ) 2 )


/*-----------------------------------------------------------
 * 任务创建 API
 *----------------------------------------------------------*/

/**
 * 任务。小时
 *<pre>
 BaseType_t xTaskCreate(
							  TaskFunction_t pvTaskCode,
							  const char * const pcName,
							  configSTACK_DEPTH_TYPE usStackDepth,
							  void *pvParameters,
							  UBaseType_t uxPriority,
							  TaskHandle_t *pvCreatedTask
						  );</pre>
 *
 * 创建一个新任务并将其添加到准备运行的任务列表中。
 *
 * 在 FreeRTOS 实现内部，任务使用两个内存块
 * 。  第一个块用于保存任务的数据结构。  第二个块
 * 被任务用作其堆栈。  如果使用
 * xTaskCreate() 创建任务，则两个内存块
 * 都会在 xTaskCreate() 函数内自动动态分配。  （见
 * http://www.freertos.org/a00111.html). 如果任务是使用以下命令创建的
 * xTaskCreateStatic() 那么应用程序编写者
 * 必须提供所需的内存。  因此，
 * xTaskCreateStatic() 允许在不使用任何动态内存分配的情况下创建任务。
 *
 * 请参阅 xTaskCreateStatic() 了解不使用任何动态内存的版本
 * 分配。
 *
 * xTaskCreate() 只能用于创建可以不受限
 * 制地访问整个微控制器内存映射的任务。  包含
 * MPU 支持的系统也可以使用
 * xTaskCreateRestricted() 创建 MPU 约束任务。
 *
 * @param pvTaskCode 指向任务入口函数的指针。  任务
 * 必须实现为永不 return（即连续循环）。
 *
 * @param pcName 任务的描述性名称。  这个主要是用来
 * 方便调试。  最大长度由 configMAX_TASK_NAME_LEN 定义 - 默认
 * 是 16。
 *
 * @param usStackDepth 任务堆栈的大小指定为数量
 * 堆栈可以保存的变量 - 不是字节数。  例如，如果堆
 * 栈为 16 位宽，且 usStackDepth 定义
 * 为 100，则将分配 200 个字节用于堆栈存储。
 *
 * @param pvParameters 将用作任务参数的指针
 * 正在被创建。
 *
 * @param uxPriority 任务应运行的优先级。  系统
 * include MPU support can optionally create tasks in a privileged (system)
 * 通过设置优先级参数的位 portPRIVILEGE_BIT 来选择模式
 * 。  例如，要创建优先级为 2 的特权任务，uxPriority 参数
 * 应设置为 to ( 2 | portPRIVILEGE_BIT )。
 *
 * @param pvCreatedTask 用于传回创建任务的句柄
 * 可以参考。
 *
 * @return pdPASS 如果任务已成功创建并添加到就绪状态
 * 列表，否则文件 projdefs.h 中定义的错误代码
 *
 * 用法示例：
   <pre>
 //待创建的任务。
 void vTaskCode( void * pvParameters )
 {
	 for( ;; )
	 {
		 //任务代码位于此处。
	 }
 }

 //创建任务的函数。
 void vOtherFunction( void )
 {
 static uint8_t ucParameterToPass;
 TaskHandle_t xHandle = NULL;

	 //创建任务，存储句柄。  请注意，传递的参数
	 //ucParameterToPass 必须在任务的生命周期
	 //内存在，因此在本例中声明为静态。  如果它只是一个自动堆
	 //栈变量，那么当新任务尝试访问它时，它可能不再存在，或者至少已经损坏。
	 xTaskCreate( vTaskCode, "NAME", STACK_SIZE, &ucParameterToPass, tskIDLE_PRIORITY, &xHandle );
	 configASSERT( xHandle );

	 //使用句柄删除任务。
	 if( xHandle != NULL )
	 {
	 	vTaskDelete( xHandle );
	 }
 }
   </pre>
 * \defgroup xTaskCreate xTaskCreate
 * \ingroup Tasks
 */
#if( configSUPPORT_DYNAMIC_ALLOCATION == 1 )
	BaseType_t xTaskCreate(	TaskFunction_t pxTaskCode,
							const char * const pcName,	/*lint !e971 仅字符串和单个字符允许使用非限定字符类型。 */
							const configSTACK_DEPTH_TYPE usStackDepth,
							void * const pvParameters,
							UBaseType_t uxPriority,
							TaskHandle_t * const pxCreatedTask ) PRIVILEGED_FUNCTION;
#endif

/**
 * 任务。小时
 *<pre>
 TaskHandle_t xTaskCreateStatic( TaskFunction_t pvTaskCode,
								 const char * const pcName,
								 uint32_t ulStackDepth,
								 void *pvParameters,
								 UBaseType_t uxPriority,
								 StackType_t *pxStackBuffer,
								 StaticTask_t *pxTaskBuffer );</pre>
 *
 * 创建一个新任务并将其添加到准备运行的任务列表中。
 *
 * 在 FreeRTOS 实现内部，任务使用两个内存块
 * 。  第一个块用于保存任务的数据结构。  第二个块
 * 被任务用作其堆栈。  如果使用
 * xTaskCreate() 创建任务，则两个内存块
 * 都会在 xTaskCreate() 函数内自动动态分配。  （见
 * http://www.freertos.org/a00111.html). 如果任务是使用以下命令创建的
 * xTaskCreateStatic() 那么应用程序编写者
 * 必须提供所需的内存。  因此，
 * xTaskCreateStatic() 允许在不使用任何动态内存分配的情况下创建任务。
 *
 * @param pvTaskCode 指向任务入口函数的指针。  任务
 * 必须实现为永不 return（即连续循环）。
 *
 * @param pcName 任务的描述性名称。  这个主要是用来
 * 方便调试。  字符串的最大长度由
 * FreeRTOSConfig.h中的configMAX_TASK_NAME_LEN定义。
 *
 * @param ulStackDepth 任务堆栈的大小指定为数量
 * 堆栈可以保存的变量 - 不是字节数。  例如，如果堆
 * 栈为 32 位宽，并且 ulStackDepth 定
 * 义为 100，则将分配 400 字节用于堆栈存储。
 *
 * @param pvParameters 将用作任务参数的指针
 * 正在被创建。
 *
 * @param uxPriority 任务运行的优先级。
 *
 * @param pxStackBuffer 必须指向一个 StackType_t 数组，该数组至少有
 * ulStackDepth 索引 - 该数组
 * 将用作任务的堆栈，从而无需动态分配堆栈。
 *
 * @param pxTaskBuffer 必须指向 StaticTask_t 类型的变量，该变量将
 * 然后用于保存任务的数据结构
 * ，从而无需动态分配内存。
 *
 * @return 如果 pxStackBuffer 或 pxTaskBuffer 都不是 NULL，则任务将
 * 被创建并返回所创建任务的句柄。  如果
 * pxStackBuffer 或
 * pxTaskBuffer 是 NULL，则不会创建任务并返回 NULL。
 *
 * 用法示例：
   <pre>

    //确定正在创建的任务将用作其堆栈的缓冲区的尺寸。 注意
    //：这是堆栈将保存的字数，而不是字节数。  例如，如果
    //每个堆栈项都是 32 位，并且设置为 100，则将分
    //配 400 bytes（100 * 32 位）。
    #define STACK_SIZE 200

    //将保存正在创建的任务的 TCB 的结构。
    StaticTask_t xTaskBuffer;

    //正在创建的任务将用作其堆栈的缓冲区。  请注意，这
    //是 StackType_t 变量的数组。
    //StackType_t 的大小取决于 RTOS 端口。
    StackType_t xStack[ STACK_SIZE ];

    //实现正在创建的任务的函数。
    void vTaskCode( void * pvParameters )
    {
        //该参数值预计为 1，因为在调用
        //xTaskCreateStatic() 时，pvParameters 值中传递了 1。
        configASSERT( ( uint32_t ) pvParameters == 1UL );

        for( ;; )
        {
            //任务代码位于此处。
        }
    }

    //创建任务的函数。
    void vOtherFunction( void )
    {
        TaskHandle_t xHandle = NULL;

        //创建任务时不使用任何动态内存分配。
        xHandle = xTaskCreateStatic(
                      vTaskCode,       // Function that implements the task.
                      "NAME",          // Text name for the task.
                      STACK_SIZE,      // Stack size in words, not bytes.
                      ( void * ) 1,    // Parameter passed into the task.
                      tskIDLE_PRIORITY,// Priority at which the task is created.
                      xStack,          // Array to use as the task's stack.
                      &xTaskBuffer );  // Variable to hold the task's data structure.

        //puxStackBuffer 和
        //pxTaskBuffer 不是 NULL，因此任务将
        //已创建，并且 xHandle 将是任务的句柄。  使用句柄暂停任务。
        vTaskSuspend( xHandle );
    }
   </pre>
 * \defgroup xTaskCreateStatic xTaskCreateStatic
 * \ingroup Tasks
 */
#if( configSUPPORT_STATIC_ALLOCATION == 1 )
	TaskHandle_t xTaskCreateStatic(	TaskFunction_t pxTaskCode,
									const char * const pcName, /*lint !e971 仅字符串和单个字符允许使用非限定字符类型。 */
									const uint32_t ulStackDepth,
									void * const pvParameters,
									UBaseType_t uxPriority,
									StackType_t * const puxStackBuffer,
									StaticTask_t * const pxTaskBuffer ) PRIVILEGED_FUNCTION;
#endif /* configSUPPORT_STATIC_ALLOCATION */

/**
 * 任务。小时
 *<pre>
 BaseType_t xTaskCreateRestricted( TaskParameters_t *pxTaskDefinition, TaskHandle_t *pxCreatedTask );</pre>
 *
 * 仅当 configSUPPORT_DYNAMIC_ALLOCATION 设置为 1 时可用。
 *
 * xTaskCreateRestricted() 只能在包含 MPU 的系统中使用
 * 执行。
 *
 * 创建一个新任务并将其添加到准备运行
 * 的任务列表中。函数参数定义分配给任
 * 务的内存区域和关联的访问权限。
 *
 * 请参阅
 * xTaskCreateRestrictedStatic() 了解不使用任何动态内存分配的版本。
 *
 * @param pxTaskDefinition 指向包含成员的结构体的指针
 * for each of the normal xTaskCreate() parameters (see the xTaskCreate() API
 * 文档）加上可选的堆栈缓冲区和内存区域
 * 定义。
 *
 * @param pxCreatedTask 用于传回创建任务的句柄
 * 可以参考。
 *
 * @return pdPASS 如果任务已成功创建并添加到就绪状态
 * 列表，否则文件 projdefs.h 中定义的错误代码
 *
 * 用法示例：
   <pre>
//创建一个 TaskParameters_t 结构体来定义要创建的任务。
static const TaskParameters_t xCheckTaskParameters =
{
	vATask,		// pvTaskCode - the function that implements the task.
	"ATask",	// pcName - just a text name for the task to assist debugging.
	100,		// usStackDepth	- the stack size DEFINED IN WORDS.
	NULL,		// pvParameters - passed into the task function as the function parameters.
	( 1UL | portPRIVILEGE_BIT ),// uxPriority - task priority, set the portPRIVILEGE_BIT if the task should run in a privileged state.
	cStackBuffer,// puxStackBuffer - the buffer to be used as the task stack.

	//xRegions - 分配最多三个独立的内存区域供任
	//务访问，并具有适当的访问权限。  不同的处理器有不同
	//的内存对齐要求 - 请参阅 FreeRTOS 文档
	//for full information.
	{
		//基地址 长度 参数
		{ cReadWriteArray,				32,		portMPU_REGION_READ_WRITE },
		{ cReadOnlyArray,				32,		portMPU_REGION_READ_ONLY },
		{ cPrivilegedOnlyAccessArray,	128,	portMPU_REGION_PRIVILEGED_READ_WRITE }
	}
};

int main( void )
{
TaskHandle_t xHandle;

	//从上面定义的 const 结构创建一个任务。  任务
	//句柄是 requested（第二个参数不是 NULL
	//），但在本例中仅用于演示目的，因为它并未实际使用。
	xTaskCreateRestricted( &xRegTest1Parameters, &xHandle );

	//启动调度程序。
	vTaskStartScheduler();

	//仅当没有足够的内存来创建空闲和
	///或计时器任务时才会到达此处。
	for( ;; );
}
   </pre>
 * \defgroup xTaskCreateRestricted xTaskCreateRestricted
 * \ingroup Tasks
 */
#if( portUSING_MPU_WRAPPERS == 1 )
	BaseType_t xTaskCreateRestricted( const TaskParameters_t * const pxTaskDefinition, TaskHandle_t *pxCreatedTask ) PRIVILEGED_FUNCTION;
#endif

/**
 * 任务。小时
 *<pre>
 BaseType_t xTaskCreateRestrictedStatic( TaskParameters_t *pxTaskDefinition, TaskHandle_t *pxCreatedTask );</pre>
 *
 * 仅当 configSUPPORT_STATIC_ALLOCATION 设置为 1 时可用。
 *
 * xTaskCreateRestrictedStatic()
 * 只能在包含 MPU 实现的系统中使用。
 *
 * 在 FreeRTOS 实现内部，任务使用两个内存块。  第
 * 一个块用于保存任务的数据结构。  第二个块被任务用作其堆栈
 * 。  如果使用
 * xTaskCreateRestricted() 创建任务，
 * 则堆栈由应用程序编写器提供，并且用于保存任务数据结构的内存
 * 在 xTaskCreateRestricted() 函数内
 * 自动动态分配。  如果使用
 * xTaskCreateRestrictedStatic()
 * 创建任务，那么应用程序编写者也必须提供用于保存任务数据结
 * 构的内存。因此，xTaskCreateRestrictedStatic() 允许在不使用任何动态内存分配的情况下创建内存保护任务。
 *
 * @param pxTaskDefinition 指向包含成员的结构体的指针
 * for each of the normal xTaskCreate() parameters (see the xTaskCreate() API
 * 文档）加上可选的堆栈缓冲区和内存区域定义。  如果
 * configSUPPORT_STATIC_ALLOCATION
 * 设置为 1，则该结构包含一个附加成员，该成员用于指向
 * StaticTask_t 类型的变量 - 然后用于保存任务的数据结构。
 *
 * @param pxCreatedTask 用于传回创建任务的句柄
 * 可以参考。
 *
 * @return pdPASS 如果任务已成功创建并添加到就绪状态
 * 列表，否则文件 projdefs.h 中定义的错误代码
 *
 * 用法示例：
   <pre>
//创建一个 TaskParameters_t 结构体来定义要创建的任务。仅
//当 configSUPPORT_STATIC_ALLOCATION 设置
//为 1 时，StaticTask_t 变量才包含在结构中。
//PRIVILEGED_DATA 宏可用于强制变量进入 RTOS 内核的特权数据区。
static PRIVILEGED_DATA StaticTask_t xTaskBuffer;
static const TaskParameters_t xCheckTaskParameters =
{
	vATask,		// pvTaskCode - the function that implements the task.
	"ATask",	// pcName - just a text name for the task to assist debugging.
	100,		// usStackDepth	- the stack size DEFINED IN WORDS.
	NULL,		// pvParameters - passed into the task function as the function parameters.
	( 1UL | portPRIVILEGE_BIT ),// uxPriority - task priority, set the portPRIVILEGE_BIT if the task should run in a privileged state.
	cStackBuffer,// puxStackBuffer - the buffer to be used as the task stack.

	//xRegions - 分配最多三个独立的内存区域供任
	//务访问，并具有适当的访问权限。  不同的处理器有不同
	//的内存对齐要求 - 请参阅 FreeRTOS 文档
	//for full information.
	{
		//基地址 长度 参数
		{ cReadWriteArray,				32,		portMPU_REGION_READ_WRITE },
		{ cReadOnlyArray,				32,		portMPU_REGION_READ_ONLY },
		{ cPrivilegedOnlyAccessArray,	128,	portMPU_REGION_PRIVILEGED_READ_WRITE }
	}

	&xTaskBuffer; // Holds the task's data structure.
};

int main( void )
{
TaskHandle_t xHandle;

	//从上面定义的 const 结构创建一个任务。  任务
	//句柄是 requested（第二个参数不是 NULL
	//），但在本例中仅用于演示目的，因为它并未实际使用。
	xTaskCreateRestricted( &xRegTest1Parameters, &xHandle );

	//启动调度程序。
	vTaskStartScheduler();

	//仅当没有足够的内存来创建空闲和
	///或计时器任务时才会到达此处。
	for( ;; );
}
   </pre>
 * \defgroup xTaskCreateRestrictedStatic xTaskCreateRestrictedStatic
 * \ingroup Tasks
 */
#if( ( portUSING_MPU_WRAPPERS == 1 ) && ( configSUPPORT_STATIC_ALLOCATION == 1 ) )
	BaseType_t xTaskCreateRestrictedStatic( const TaskParameters_t * const pxTaskDefinition, TaskHandle_t *pxCreatedTask ) PRIVILEGED_FUNCTION;
#endif

/**
 * 任务。小时
 *<pre>
 void vTaskAllocateMPURegions( TaskHandle_t xTask, const MemoryRegion_t * const pxRegions );</pre>
 *
 * 当通过调用 xTaskCreateRestricted() 创
 * 建任务时，内存区域会分配给受限任务。  这些区域可以使用
 * vTaskAllocateMPURegions() 重新定义。
 *
 * @param xTask 正在更新的任务的句柄。
 *
 * @param xRegions 指向 MemoryRegion_t 结构的指针，该结构包含
 * 新的内存区域定义。
 *
 * 用法示例：
   <pre>
//定义 MemoryRegion_t 结构数组，配置
//MPU 区域，允许从 ucOneKByte 数组
//开头开始读取/写入 1024 字节。  最多 3
//个可定义区域中的另外两个未使用，因此设置为零。
static const MemoryRegion_t xAltRegions[ portNUM_CONFIGURABLE_REGIONS ] =
{
	//基地址 长度 参数
	{ ucOneKByte,		1024,		portMPU_REGION_READ_WRITE },
	{ 0,				0,			0 },
	{ 0,				0,			0 }
};

void vATask( void *pvParameters )
{
	//创建此任务时，它可以访问由 MPU 配置定义的某些内存区域。
	//在某些时候，需要将这些 MPU 区域替换为上面
	//xAltRegions const 结构中定义的区域。  使
	//用对 vTaskAllocateMPURegions() 的调用
	//for this purpose.  NULL is used as the task handle to indicate that this
	//函数应修改调用任务的 MPU 区域。
	vTaskAllocateMPURegions( NULL, xAltRegions );

	//现在任务可以继续其功能，但从此时起只能访问其堆
	//栈和 ucOneKByte array（除非已
	//在其他地方声明了任何其他静态定义或共享区域）。
}
   </pre>
 * \defgroup xTaskCreateRestricted xTaskCreateRestricted
 * \ingroup Tasks
 */
void vTaskAllocateMPURegions( TaskHandle_t xTask, const MemoryRegion_t * const pxRegions ) PRIVILEGED_FUNCTION;

/**
 * 任务。小时
 * <pre>void vTaskDelete( TaskHandle_t xTask );</pre>
 *
 * INCLUDE_vTaskDelete 必须定义为
 * 1，此功能才可用。有关详细信息，请参阅配置部分。
 *
 * 从 RTOS 实时内核的管理中删除任务。  被删除
 * 的任务将从所有就绪、阻塞、暂停和事件列表中删除。
 *
 * 注意：空闲任务负责从已删除的任务中释放内
 * 核分配的内存。  因此，如果您的应用程序
 * 对 vTaskDelete () 进行任
 * 何调用，空闲任务不会缺乏微控制器处理时间
 * ，这一点很重要。  任务代码分配的内存不
 * 会自动释放，应在删除任务之前释放。
 *
 * 有关使用 vTaskDelete () 的示例
 * 代码，请参阅演示应用程序文件 death.c。
 *
 * @param xTask 待删除任务的句柄。  通过 NULL 将
 * 导致调用任务被删除。
 *
 * 用法示例：
   <pre>
 void vOtherFunction( void )
 {
 TaskHandle_t xHandle;

	 //创建任务，存储句柄。
	 xTaskCreate( vTaskCode, "NAME", STACK_SIZE, NULL, tskIDLE_PRIORITY, &xHandle );

	 //使用句柄删除任务。
	 vTaskDelete( xHandle );
 }
   </pre>
 * \defgroup vTaskDelete vTaskDelete
 * \ingroup Tasks
 */
void vTaskDelete( TaskHandle_t xTaskToDelete ) PRIVILEGED_FUNCTION;

/*-----------------------------------------------------------
 * 任务控制 API
 *----------------------------------------------------------*/

/**
 * 任务。小时
 * <pre>void vTaskDelay( const TickType_t xTicksToDelay );</pre>
 *
 * 将任务延迟给定的刻度数。  任务保持阻塞的
 * 实际时间取决于滴答率。  常数
 * portTICK_PERIOD_MS 可用
 * 于根据滴答率计算实时时间 - 分辨率为一个滴答周期。
 *
 * INCLUDE_vTaskDelay 必须定义为
 * 1，此功能才可用。有关详细信息，请参阅配置部分。
 *
 *
 * vTaskDelay() 指定任务希望解除阻塞的时间（相对于
 * 调用 vTaskDelay() 的时间）。  例如，指定 1
 * 00 个时钟周期的阻塞周期将导致任务在调用
 * vTaskDelay() 后取消阻塞 100 个时钟周期。
 * 因此，vTaskDelay() 并没有提供控制周期性任务
 * 频率的良好方法，因为代码所采用的路径以及其他任务和中断活动将
 * 影响调用 vTaskDelay() 的频率，从而影响任务下次
 * 执行的时间。  请参阅 vTaskDelayUntil()
 * 了解旨在促进固定频率执行的替代 API 函数。  它通过指定
 * 调用任务应该的绝对 time （而不是相对时间）来实现这一点。
 * 解锁。
 *
 * @param xTicksToDelay 以刻度周期为单位的时间量，
 * 调用任务应该阻塞。
 *
 * 用法示例：

 void vTaskFunction( void * pvParameters )
 {
 // Block for 500ms.
 const TickType_t xDelay = 500 / portTICK_PERIOD_MS;

	 for( ;; )
	 {
		 // Simply toggle the LED every 500ms, blocking between each toggle.
		 vToggleLED();
		 vTaskDelay( xDelay );
	 }
 }

 * \defgroup vTaskDelay vTaskDelay
 * \ingroup TaskCtrl
 */
void vTaskDelay( const TickType_t xTicksToDelay ) PRIVILEGED_FUNCTION;

/**
 * 任务。小时
 * <pre>void vTaskDelayUntil( TickType_t *pxPreviousWakeTime, const TickType_t xTimeIncrement );</pre>
 *
 * INCLUDE_vTaskDelayUntil 必须定
 * 义为 1，此功能才可用。有关详细信息，请参阅配置部分。
 *
 * 将任务延迟到指定时间。  周期性任务
 * 可以使用此功能来确保恒定的执行频率。
 *
 * 该函数与 vTaskDelay () 的不同之处在于一个重要
 * 方面：vTaskDelay () 将导致任务从调用
 * vTaskDelay () 时起阻塞指定的时钟周期数。  因
 * 此，很难单独使用 vTaskDelay () 来生成固定的执
 * 行频率，因为任务开始执行和调用 vTaskDelay ()
 * 的任务之间的时间可能不固定[任务可能会在调用之间通过代码采取
 * 不同的路径，或者每次执行时可能会被中断或抢占不同的次数]。
 *
 * vTaskDelay () 指定相对于调用函数的时间的唤醒时间，而
 * vTaskDelayUntil () 指定它希望的 absolute（精确）时间。
 * 解锁。
 *
 * 常数 portTICK_PERIOD_MS 可用于
 * 根据滴答率计算实时时间 - 分辨率为一个滴答周期。
 *
 * @param pxPreviousWakeTime 指向保存时间的变量的指针
 * 任务最后被解锁。  该变量必须使用其第一个 use 之前
 * 的当前时间进行初始化（请参见下面的示例）。  此后，变量
 * 会在 vTaskDelayUntil () 内自动更新。
 *
 * @param xTimeIncrement 循环时间段。  任务将在以下时间解锁
 * 时间 *pxPreviousWakeTime +
 * xTimeIncrement。  使用相同的
 * xTimeIncrement 参数值调用 vTaskDelayUntil 将导致任务以固定接口周期执行。
 *
 * 用法示例：
   <pre>
 //每 10 个刻度执行一次操作。
 void vTaskFunction( void * pvParameters )
 {
 TickType_t xLastWakeTime;
 const TickType_t xFrequency = 10;

	 //使用当前时间初始化 xLastWakeTime 变量。
	 xLastWakeTime = xTaskGetTickCount ();
	 for( ;; )
	 {
		 //等待下一个周期。
		 vTaskDelayUntil( &xLastWakeTime, xFrequency );

		 //在此执行操作。
	 }
 }
   </pre>
 * \defgroup vTaskDelayUntil vTaskDelayUntil
 * \ingroup TaskCtrl
 */
void vTaskDelayUntil( TickType_t * const pxPreviousWakeTime, const TickType_t xTimeIncrement ) PRIVILEGED_FUNCTION;

/**
 * 任务。小时
 * <pre>BaseType_t xTaskAbortDelay( TaskHandle_t xTask );</pre>
 *
 * INCLUDE_xTaskAbortDelay 必须在
 * FreeRTOSConfig.h 中定义为 1，此功能才可用。
 *
 * 任务在等待事件时会进入阻塞状态。  它等待的事件可以是临时
 * event（等待一段时间），例如调用
 * vTaskDelay() 时，也可以是对象上的事件，例如调
 * 用 xQueueReceive() 或
 * ulTaskNotifyTake() 时。  如果在对
 * xTaskAbortDelay() 的调用中使用处于阻塞状
 * 态的任务的句柄，则该任务将离开阻塞状态，并从将该任务置于阻塞状态的任何函数调用返回。
 *
 * 此函数没有“FromISR”版本，因为中断
 * 需要知道任务被哪个对象阻塞，以便知道要采取
 * 哪些操作。  例如，如果任务在队列上被阻塞
 * ，则中断处理程序将需要知道队列是否被锁定。
 *
 * @param xTask 要从阻塞状态中删除的任务的句柄。
 *
 * @return 如果 xTask 引用的任务不处于阻塞状态，则
 * 返回 pdFAIL。  否则返回 pdPASS。
 *
 * \defgroup xTaskAbortDelay xTaskAbortDelay
 * \ingroup TaskCtrl
 */
BaseType_t xTaskAbortDelay( TaskHandle_t xTask ) PRIVILEGED_FUNCTION;

/**
 * 任务。小时
 * <pre>UBaseType_t uxTaskPriorityGet( const TaskHandle_t xTask );</pre>
 *
 * INCLUDE_uxTaskPriorityGet 必须
 * 定义为 1，此功能才可用。有关详细信息，请参阅配置部分。
 *
 * 获取任何任务的优先级。
 *
 * @param xTask 待查询任务的句柄。  通过 NULL
 * 处理结果返回调用任务的优先级。
 *
 * @return xTask 的优先级。
 *
 * 用法示例：
   <pre>
 void vAFunction( void )
 {
 TaskHandle_t xHandle;

	 //创建一个任务，存储句柄。
	 xTaskCreate( vTaskCode, "NAME", STACK_SIZE, NULL, tskIDLE_PRIORITY, &xHandle );

	 //...

	 //使用句柄获取创建的任务的优先级。它
	 //是用
	 //tskIDLE_PRIORITY 创建的，但本身可能已更改。
	 if( uxTaskPriorityGet( xHandle ) != tskIDLE_PRIORITY )
	 {
		 //该任务已更改其优先级。
	 }

	 //...

	 //我们的优先级比创建的任务高吗？
	 if( uxTaskPriorityGet( xHandle ) < uxTaskPriorityGet( NULL ) )
	 {
		 //我们的priority（使用NULL手柄获得）更高。
	 }
 }
   </pre>
 * \defgroup uxTaskPriorityGet uxTaskPriorityGet
 * \ingroup TaskCtrl
 */
UBaseType_t uxTaskPriorityGet( const TaskHandle_t xTask ) PRIVILEGED_FUNCTION;

/**
 * 任务。小时
 * <pre>UBaseType_t uxTaskPriorityGetFromISR( const TaskHandle_t xTask );</pre>
 *
 * 可以从 ISR 使用的 uxTaskPriorityGet() 版本。
 */
UBaseType_t uxTaskPriorityGetFromISR( const TaskHandle_t xTask ) PRIVILEGED_FUNCTION;

/**
 * 任务。小时
 * <pre>eTaskState eTaskGetState( TaskHandle_t xTask );</pre>
 *
 * INCLUDE_eTaskGetState 必须定义
 * 为 1，此功能才可用。有关详细信息，请参阅配置部分。
 *
 * 获取任意任务的状态。  状态由
 * eTaskState 枚举类型编码。
 *
 * @param xTask 待查询任务的句柄。
 *
 * @return 调用函数时 xTask 的状态。  请注意
 * 任务的状态可能会在被调用的函数和调用
 * 任务测试的函数返回值之间发生变化。
 */
eTaskState eTaskGetState( TaskHandle_t xTask ) PRIVILEGED_FUNCTION;

/**
 * 任务。小时
 * <pre>void vTaskGetInfo( TaskHandle_t xTask, TaskStatus_t *pxTaskStatus, BaseType_t xGetFreeStackSpace, eTaskState eState );</pre>
 *
 * configUSE_TRACE_FACILITY 必须定义
 * 为 1，此功能才可用。  有关详细信息，请参阅配置部分。
 *
 * 使用有关任务的信息填充 TaskStatus_t 结构。
 *
 * @param xTask 正在查询的任务的句柄。  如果 xTask 是 NULL 那么
 * 将返回有关调用任务的信息。
 *
 * @param pxTaskStatus 指向 TaskStatus_t 结构的指针
 * 填充有关使用 xTask 参数
 * 传递的句柄引用的任务的信息。
 *
 * @xGetFreeStackSpace TaskStatus_t 结构包含要报告的成员
 * 正在查询的任务的堆栈高水位线。  计算堆栈高水位线需
 * 要相对较长的时间，并且可能使系统暂时无响应 - 因此
 * 提供 xGetFreeStackSpace 参数以允
 * 许跳过高水位线检查。  如果
 * xGetFreeStackSpace 没有设置为
 * pdFALSE，则高水印值只会写入 TaskStatus_t 结构体；
 *
 * @param eState TaskStatus_t 结构包含一个成员来报告
 * 正在查询的任务的状态。  获取任务状态并不像简单赋值那么快 -
 * 因此提供 eState 参数以允许从
 * TaskStatus_t 结构中省略状态信息。  要获取状态信
 * 息，请将 eState 设置为 eInvalid - 否则
 * eState 中传递的值将在 TaskStatus_t 结构中报告为任务状态。
 *
 * 用法示例：
   <pre>
 void vAFunction( void )
 {
 TaskHandle_t xHandle;
 TaskStatus_t xTaskDetails;

    //从任务名称获取任务句柄。
    xHandle = xTaskGetHandle( "Task_Name" );

    //检查手柄不是NULL。
    configASSERT( xHandle );

    //使用句柄获取有关任务的更多信息。
    vTaskGetInfo( xHandle,
                  &xTaskDetails,
                  pdTRUE, // Include the high water mark in xTaskDetails.
                  eInvalid ); // Include the task state in xTaskDetails.
 }
   </pre>
 * \defgroup vTaskGetInfo vTaskGetInfo
 * \ingroup TaskCtrl
 */
void vTaskGetInfo( TaskHandle_t xTask, TaskStatus_t *pxTaskStatus, BaseType_t xGetFreeStackSpace, eTaskState eState ) PRIVILEGED_FUNCTION;

/**
 * 任务。小时
 * <pre>void vTaskPrioritySet( TaskHandle_t xTask, UBaseType_t uxNewPriority );</pre>
 *
 * INCLUDE_vTaskPrioritySet 必须定
 * 义为 1，此功能才可用。有关详细信息，请参阅配置部分。
 *
 * 设置任何任务的优先级。
 *
 * 如果设置的优先级高于当前正在执行的任务
 * ，则在函数返回之前将发生上下文切换。
 *
 * @param xTask 正在设置优先级的任务的句柄。
 * 传递 NULL 句柄会导致设置调用任务的优先级。
 *
 * @param uxNewPriority 任务将设置的优先级。
 *
 * 用法示例：
   <pre>
 void vAFunction( void )
 {
 TaskHandle_t xHandle;

	 //创建一个任务，存储句柄。
	 xTaskCreate( vTaskCode, "NAME", STACK_SIZE, NULL, tskIDLE_PRIORITY, &xHandle );

	 //...

	 //使用句柄提高创建任务的优先级。
	 vTaskPrioritySet( xHandle, tskIDLE_PRIORITY + 1 );

	 //...

	 //使用 NULL 句柄将我们的优先级提高到相同的值。
	 vTaskPrioritySet( NULL, tskIDLE_PRIORITY + 1 );
 }
   </pre>
 * \defgroup vTaskPrioritySet vTaskPrioritySet
 * \ingroup TaskCtrl
 */
void vTaskPrioritySet( TaskHandle_t xTask, UBaseType_t uxNewPriority ) PRIVILEGED_FUNCTION;

/**
 * 任务。小时
 * <pre>void vTaskSuspend( TaskHandle_t xTaskToSuspend );</pre>
 *
 * INCLUDE_vTaskSuspend 必须定义为
 * 1，此功能才可用。有关详细信息，请参阅配置部分。
 *
 * 暂停任何任务。  当任务挂起时，无论其优先级
 * 如何，都永远不会获得任何微控制器处理时间。
 *
 * 对 vTaskSuspend 的调用不可累积 - 即对同一任
 * 务调用 vTaskSuspend () 两次仍然只需要调用一
 * 次 vTaskResume () 即可准备好挂起的任务。
 *
 * @param xTaskToSuspend 正在挂起的任务的句柄。  通过 NULL
 * 句柄将导致调用任务被挂起。
 *
 * 用法示例：
   <pre>
 void vAFunction( void )
 {
 TaskHandle_t xHandle;

	 //创建一个任务，存储句柄。
	 xTaskCreate( vTaskCode, "NAME", STACK_SIZE, NULL, tskIDLE_PRIORITY, &xHandle );

	 //...

	 //使用句柄挂起创建的任务。
	 vTaskSuspend( xHandle );

	 //...

	 //创建的任务在此期间不会运行，除非另一个任务调用
	 //vTaskResume( xHandle )。

	 //...


	 //暂停我们自己。
	 vTaskSuspend( NULL );

	 //除非另一个任务以我们的句柄作为参数调用
	 //vTaskResume，否则我们无法到达这里。
 }
   </pre>
 * \defgroup vTaskSuspend vTaskSuspend
 * \ingroup TaskCtrl
 */
void vTaskSuspend( TaskHandle_t xTaskToSuspend ) PRIVILEGED_FUNCTION;

/**
 * 任务。小时
 * <pre>void vTaskResume( TaskHandle_t xTaskToResume );</pre>
 *
 * INCLUDE_vTaskSuspend 必须定义为
 * 1，此功能才可用。有关详细信息，请参阅配置部分。
 *
 * 恢复暂停的任务。
 *
 * A task that has been suspended by one or more calls to vTaskSuspend ()
 * 通过一次调用
 * vTaskResume () 即可再次运行。
 *
 * @param xTaskToResume 正在准备的任务的句柄。
 *
 * 用法示例：
   <pre>
 void vAFunction( void )
 {
 TaskHandle_t xHandle;

	 //创建一个任务，存储句柄。
	 xTaskCreate( vTaskCode, "NAME", STACK_SIZE, NULL, tskIDLE_PRIORITY, &xHandle );

	 //...

	 //使用句柄挂起创建的任务。
	 vTaskSuspend( xHandle );

	 //...

	 //创建的任务在此期间不会运行，除非另一个任务调用
	 //vTaskResume( xHandle )。

	 //...


	 //我们自己恢复挂起的任务。
	 vTaskResume( xHandle );

	 //创建的任务将根据其在系统中的优
	 //先级再次获得微控制器处理时间。
 }
   </pre>
 * \defgroup vTaskResume vTaskResume
 * \ingroup TaskCtrl
 */
void vTaskResume( TaskHandle_t xTaskToResume ) PRIVILEGED_FUNCTION;

/**
 * 任务。小时
 * <pre>void xTaskResumeFromISR( TaskHandle_t xTaskToResume );</pre>
 *
 * INCLUDE_xTaskResumeFromISR 必须定
 * 义为 1，此功能才可用。  有关详细信息，请参阅配置部分。
 *
 * 可以从 ISR 内部调用的 vTaskResume() 的实现。
 *
 * A task that has been suspended by one or more calls to vTaskSuspend ()
 * 通过一次调用
 * xTaskResumeFromISR () 即可再次运行。
 *
 * 如果中断有可能在任务挂起之前到达，则不应使用
 * xTaskResumeFromISR()
 * 来同步任务与中断 - 因为这可能导致错过中断
 * 。使用信号量作为同步机制可以避免这种可能性。
 *
 * @param xTaskToResume 正在准备的任务的句柄。
 *
 * @return pdTRUE 如果恢复任务应导致上下文切换，
 * 否则为 pdFALSE。 ISR 使用它来
 * 确定在 ISR 之后是否需要上下文切换。
 *
 * \defgroup vTaskResumeFromISR vTaskResumeFromISR
 * \ingroup TaskCtrl
 */
BaseType_t xTaskResumeFromISR( TaskHandle_t xTaskToResume ) PRIVILEGED_FUNCTION;

/*-----------------------------------------------------------
 * 调度程序控制
 *----------------------------------------------------------*/

/**
 * 任务。小时
 * <pre>void vTaskStartScheduler( void );</pre>
 *
 * 启动实时内核刻度处理。  调用后，内
 * 核可以控制执行哪些任务以及何时执行。
 *
 * 有关创建任务和启动内核的示例，请参
 * 阅演示应用程序文件 main.c。
 *
 * 用法示例：
   <pre>
 void vAFunction( void )
 {
	 //在启动内核之前至少创建一项任务。
	 xTaskCreate( vTaskCode, "NAME", STACK_SIZE, NULL, tskIDLE_PRIORITY, NULL );

	 //以抢占方式启动实时内核。
	 vTaskStartScheduler ();

	 //除非任务调用 vTaskEndScheduler ()，否则不会到达这里
 }
   </pre>
 *
 * \defgroup vTaskStartScheduler vTaskStartScheduler
 * \ingroup SchedulerControl
 */
void vTaskStartScheduler( void ) PRIVILEGED_FUNCTION;

/**
 * 任务。小时
 * <pre>void vTaskEndScheduler( void );</pre>
 *
 * 注意：在撰写本文时，仅在 PC（而不是 DOS
 * ）上运行的 x86 实模式端口实现了此功能。
 *
 * 停止实时内核滴答。  所有创建的任务将被自动删除，
 * multitasking（抢占式或协作式）将停止。  然后从调
 * 用 vTaskStartScheduler () 的位置恢复执
 * 行，就像 vTaskStartScheduler () 刚刚返回一样。
 *
 * 请参阅演示应用程序文件 main. demo/PC 目录中的
 * c 为使用 vTaskEndScheduler () 的示例。
 *
 * vTaskEndScheduler () 需要在可移植 layer 中定
 * 义退出函数（有关 PC 端口，请参阅端口.c 中的
 * vPortEndScheduler ()）。  这会执行特定于硬件的操作，例如停止内核滴答。
 *
 * vTaskEndScheduler()将导致内核分
 * 配的所有资源被释放——但不会释放应用程序分配的资源
 * 任务。
 *
 * 用法示例：
   <pre>
 void vTaskCode( void * pvParameters )
 {
	 for( ;; )
	 {
		 //任务代码位于此处。

		 //在某些时候，我们想要结束实
		 //时内核处理，因此调用...
		 vTaskEndScheduler ();
	 }
 }

 void vAFunction( void )
 {
	 //在启动内核之前至少创建一项任务。
	 xTaskCreate( vTaskCode, "NAME", STACK_SIZE, NULL, tskIDLE_PRIORITY, NULL );

	 //以抢占方式启动实时内核。
	 vTaskStartScheduler ();

	 //只有当vTaskCode()任务调用了
	 //vTaskEndScheduler()时才会到达这里。  当我们到达这里时，我们又回到单一任务
	 //execution.
 }
   </pre>
 *
 * \defgroup vTaskEndScheduler vTaskEndScheduler
 * \ingroup SchedulerControl
 */
void vTaskEndScheduler( void ) PRIVILEGED_FUNCTION;

/**
 * 任务。小时
 * <pre>void vTaskSuspendAll( void );</pre>
 *
 * 挂起调度程序而不禁用中断。  当调
 * 度程序挂起时，不会发生上下文切换。
 *
 * 调用 vTaskSuspendAll () 后，调用任务将继续执行，不会有被
 * 换出的风险，直到对 xTaskResumeAll () 的调用被执行为止。
 * 制成。
 *
 * 当调度程序挂起时，不得调用可能导致上下文
 * switch 的 API 函数（例如，
 * vTaskDelayUntil()、xQueueSend() 等）。
 *
 * 用法示例：
   <pre>
 void vTask1( void * pvParameters )
 {
	 for( ;; )
	 {
		 //任务代码位于此处。

		 //...

		 //在某些时候，任务想要执行长时间操作，在此期间它不希望
		 //被换出。  不能使用
		 //taskENTER_CRITICAL ()/
		 //taskEXIT_CRITICAL ()，因为操作的长度可能会导致错过中断 - 包括
		 //ticks.

		 //防止实时内核交换任务。
		 vTaskSuspendAll ();

		 //在此进行操作。  无需使用关键
		 //部分，因为我们拥有所有微控制器
		 //处理时间。在此期间，中断仍将运
		 //行并且内核滴答计数将保持不变。

		 //...

		 //操作完成。  重新启动内核。
		 xTaskResumeAll ();
	 }
 }
   </pre>
 * \defgroup vTaskSuspendAll vTaskSuspendAll
 * \ingroup SchedulerControl
 */
void vTaskSuspendAll( void ) PRIVILEGED_FUNCTION;

/**
 * 任务。小时
 * <pre>BaseType_t xTaskResumeAll( void );</pre>
 *
 * 在调用
 * vTaskSuspendAll() 暂停后恢复调度程序活动。
 *
 * xTaskResumeAll() 仅恢复调度程序。  它不会
 * 取消先前通过调用 vTaskSuspend() 暂停的任务。
 *
 * @return 如果恢复调度程序导致上下文切换，则 pdTRUE
 *	返回，否则返回 pdFALSE。
 *
 * 用法示例：
   <pre>
 void vTask1( void * pvParameters )
 {
	 for( ;; )
	 {
		 //任务代码位于此处。

		 //...

		 //在某些时候，任务想要执行长时间操作，在此期间它不希望
		 //被换出。  不能使用
		 //taskENTER_CRITICAL ()/
		 //taskEXIT_CRITICAL ()，因为操作的长度可能会导致错过中断 - 包括
		 //ticks.

		 //防止实时内核交换任务。
		 vTaskSuspendAll ();

		 //在此进行操作。  无需使用关键部
		 //分，因为我们拥有所有微控制器处理
		 //时间。在此期间，中断仍将运行，并
		 //且实时内核滴答计数将保持不变。

		 //...

		 //操作完成。  重新启动内核。  我们想要
		 //强制进行上下文切换 - 但如果恢复调度程
		 //序已经导致了上下文切换，那就没有意义了。
		 if( !xTaskResumeAll () )
		 {
			  taskYIELD ();
		 }
	 }
 }
   </pre>
 * \defgroup xTaskResumeAll xTaskResumeAll
 * \ingroup SchedulerControl
 */
BaseType_t xTaskResumeAll( void ) PRIVILEGED_FUNCTION;

/*-----------------------------------------------------------
 * 任务实用程序
 *----------------------------------------------------------*/

/**
 * 任务。 h <PRE>TickType_t
 * xTaskGetTickCount(无效);</PRE>
 *
 * @return 自调用 vTaskStartScheduler 以来的刻度数。
 *
 * \defgroup xTaskGetTickCount xTaskGetTickCount
 * \ingroup TaskUtils
 */
TickType_t xTaskGetTickCount( void ) PRIVILEGED_FUNCTION;

/**
 * 任务。 h <PRE>TickType_t
 * xTaskGetTickCountFromISR(无效);</PRE>
 *
 * @return 自调用 vTaskStartScheduler 以来的刻度数。
 *
 * 这是 xTaskGetTickCount()
 * 的一个版本，可以安全地从 ISR 调用 - 前
 * 提是 TickType_t 是正在使用的微控制
 * 器的自然字大小，或者不支持或不使用中断嵌套。
 *
 * \defgroup xTaskGetTickCountFromISR xTaskGetTickCountFromISR
 * \ingroup TaskUtils
 */
TickType_t xTaskGetTickCountFromISR( void ) PRIVILEGED_FUNCTION;

/**
 * 任务。 h <PRE>uint16_t
 * uxTaskGetNumberOfTasks(无效);</PRE>
 *
 * @return 实时内核当前正在管理的任务数。
 * 这包括所有就绪、阻塞和挂起的任
 * 务。  已删除但尚未被空闲任务
 * 释放的任务也将包含在计数中。
 *
 * \defgroup uxTaskGetNumberOfTasks uxTaskGetNumberOfTasks
 * \ingroup TaskUtils
 */
UBaseType_t uxTaskGetNumberOfTasks( void ) PRIVILEGED_FUNCTION;

/**
 * 任务。 h <PRE>char *pcTaskGetName(
 * TaskHandle_t xTaskToQuery );</PRE>
 *
 * @return 句柄引用的任务的 text（人类可读）名称
 * xTaskToQuery。  任务可以通过传入自己的句柄或将
 * xTaskToQuery 设置为 NULL 来查询自己的名称。
 *
 * \defgroup pcTaskGetName pcTaskGetName
 * \ingroup TaskUtils
 */
char *pcTaskGetName( TaskHandle_t xTaskToQuery ) PRIVILEGED_FUNCTION; /*lint !e971 仅字符串和单个字符允许使用非限定字符类型。 */

/**
 * 任务。 h <PRE>TaskHandle_t
 * xTaskGetHandle( const char *pcNameToQuery );</PRE>
 *
 * 注意：该功能需要较长时间
 * 才能完成，请谨慎使用。
 *
 * @return 具有人类可读名称 pcNameToQuery 的任务的句柄。
 * 如果没有找到匹配的名称，则返回 NULL。  FreeRTOSConfig.h 中的
 * INCLUDE_xTaskGetHandle 必须设置为 1，pcTaskGetHandle() 才可用。
 *
 * \defgroup pcTaskGetHandle pcTaskGetHandle
 * \ingroup TaskUtils
 */
TaskHandle_t xTaskGetHandle( const char *pcNameToQuery ) PRIVILEGED_FUNCTION; /*lint !e971 仅字符串和单个字符允许使用非限定字符类型。 */

/**
 * task.h
 * <PRE>UBaseType_t uxTaskGetStackHighWaterMark( TaskHandle_t xTask );</PRE>
 *
 * 必须将 FreeRTOSConfig.h 中的
 * INCLUDE_uxTaskGetStackHighWaterMark 设置为 1，此功能才可用。
 *
 * 返回与 xTask 关联的堆栈的高水位线。  也就是
 * 说，自任务启动以来，最小可用堆栈空间为 been（用
 * 字来说，因此在 32 位机器上，值 1 表示 4 个
 * 字节）。  返回的数字越小，任务就越接近溢出堆栈。
 *
 * uxTaskGetStackHighWaterMark() 和
 * uxTaskGetStackHighWaterMark2() 除了
 * 返回类型不同之外其他相同。  使用
 * configSTACK_DEPTH_TYPE 允许用户确定返回类型
 * 。  它解决了 8 位类型上的值溢出问题，而不会破坏需要 8 位返回类型的应用程序的向后兼容性。
 *
 * @param xTask 与要检查的堆栈关联的任务的句柄。
 * 将xTask设置为NULL以检查调用任务的堆栈。
 *
 * @return 最小的可用堆栈空间是 been （换句话说，所以
 * 自创建 xTask 引用的任务以来
 * ，堆栈上的实际空间而不是字节）。
 */
UBaseType_t uxTaskGetStackHighWaterMark( TaskHandle_t xTask ) PRIVILEGED_FUNCTION;

/**
 * task.h
 * <PRE>configSTACK_DEPTH_TYPE uxTaskGetStackHighWaterMark2( TaskHandle_t xTask );</PRE>
 *
 * 必须将 FreeRTOSConfig.h 中的
 * INCLUDE_uxTaskGetStackHighWaterMark2 设置为 1，此功能才可用。
 *
 * 返回与 xTask 关联的堆栈的高水位线。  也就是
 * 说，自任务启动以来，最小可用堆栈空间为 been（用
 * 字来说，因此在 32 位机器上，值 1 表示 4 个
 * 字节）。  返回的数字越小，任务就越接近溢出堆栈。
 *
 * uxTaskGetStackHighWaterMark() 和
 * uxTaskGetStackHighWaterMark2() 除了
 * 返回类型不同之外其他相同。  使用
 * configSTACK_DEPTH_TYPE 允许用户确定返回类型
 * 。  它解决了 8 位类型上的值溢出问题，而不会破坏需要 8 位返回类型的应用程序的向后兼容性。
 *
 * @param xTask 与要检查的堆栈关联的任务的句柄。
 * 将xTask设置为NULL以检查调用任务的堆栈。
 *
 * @return 最小的可用堆栈空间是 been （换句话说，所以
 * 自创建 xTask 引用的任务以来
 * ，堆栈上的实际空间而不是字节）。
 */
configSTACK_DEPTH_TYPE uxTaskGetStackHighWaterMark2( TaskHandle_t xTask ) PRIVILEGED_FUNCTION;

/* 使用跟踪宏时，有时需要在 FreeRTOS.h 之前包含
task.h。  完成此操作后，
TaskHookFunction_t 尚未定义，因此以下两个
原型将导致编译错误。  这可以通过简单地防止包含这两个原型来
解决，除非 configUSE_APPLICATION_TASK_TAG 配置明确要求它们
持续的。 */
#ifdef configUSE_APPLICATION_TASK_TAG
	#if configUSE_APPLICATION_TASK_TAG == 1
		/**
		 * task.h
		 * <pre>void vTaskSetApplicationTaskTag( TaskHandle_t xTask, TaskHookFunction_t pxHookFunction );</pre>
		 *
		 * 将 pxHookFunction 设置为任务 xTask 使用的任务挂
		 * 钩函数。将 xTask 作为 NULL 传递具有设置调用任务挂钩的效果
		 * 功能。
		 */
		void vTaskSetApplicationTaskTag( TaskHandle_t xTask, TaskHookFunction_t pxHookFunction ) PRIVILEGED_FUNCTION;

		/**
		 * task.h
		 * <pre>void xTaskGetApplicationTaskTag( TaskHandle_t xTask );</pre>
		 *
		 * 返回分配给任务 xTask 的 pxHookFunction
		 * 值。  不要从中断服务例程调用 - 而是调用
		 * xTaskGetApplicationTaskTagFromISR()。
		 */
		TaskHookFunction_t xTaskGetApplicationTaskTag( TaskHandle_t xTask ) PRIVILEGED_FUNCTION;

		/**
		 * task.h
		 * <pre>void xTaskGetApplicationTaskTagFromISR( TaskHandle_t xTask );</pre>
		 *
		 * 返回分配给任务 xTask 的
		 * pxHookFunction 值。  可以从中断服务程序中调用。
		 */
		TaskHookFunction_t xTaskGetApplicationTaskTagFromISR( TaskHandle_t xTask ) PRIVILEGED_FUNCTION;
	#endif /* configUSE_APPLICATION_TASK_TAG ==1 */
#endif /* ifdef configUSE_APPLICATION_TASK_TAG */

#if( configNUM_THREAD_LOCAL_STORAGE_POINTERS > 0 )

	/* 每个任务都包含一个指针数组，其大小由
	FreeRTOSConfig.h 中的
	configNUM_THREAD_LOCAL_STORAGE_POINTERS
	设置确定。  内核本身并不使用指针，因此应用程序编写者可
	以将指针用于他们希望的任何目的。  下面两个函数分别用于设置和查询指针。 */
	void vTaskSetThreadLocalStoragePointer( TaskHandle_t xTaskToSet, BaseType_t xIndex, void *pvValue ) PRIVILEGED_FUNCTION;
	void *pvTaskGetThreadLocalStoragePointer( TaskHandle_t xTaskToQuery, BaseType_t xIndex ) PRIVILEGED_FUNCTION;

#endif

/**
 * task.h
 * <pre>BaseType_t xTaskCallApplicationTaskHook( TaskHandle_t xTask, void *pvParameter );</pre>
 *
 * 调用与xTask关联的钩子函数。  将 xTask 作为 NULL
 * 传递具有调用正在运行的 tasks（调用任务）挂钩函数的效果。
 *
 * pvParameter 被传递给钩子函
 * 数，以便任务根据需要进行解释。  返回
 * 值是用户注册的任务钩子函数返回的值。
 */
BaseType_t xTaskCallApplicationTaskHook( TaskHandle_t xTask, void *pvParameter ) PRIVILEGED_FUNCTION;

/**
 * 仅当 INCLUDE_xTaskGetIdleTaskHandle 在
 * FreeRTOSConfig.h 中设置为 1 时，xTaskGetIdleTaskHandle() 才可用。
 *
 * 简单地返回空闲任务的句柄。  在调度程序启动之前调用
 * xTaskGetIdleTaskHandle() 无效。
 */
TaskHandle_t xTaskGetIdleTaskHandle( void ) PRIVILEGED_FUNCTION;

/**
 * configUSE_TRACE_FACILITY 必须在
 * FreeRTOSConfig.h 中定义为 1，uxTaskGetSystemState() 才可用。
 *
 * uxTaskGetSystemState() 为系统中的每个任务填充
 * TaskStatus_t 结构。  TaskStatus_t 结构除其他外还包含成员
 * for the task handle, task name, task priority, task state, and total amount
 * 任务消耗的运行时间。  有关完整成员列表，请参阅
 * 此文件中的 TaskStatus_t 结构定义。
 *
 * 注意：此函数仅用于调试用途，因为它的使
 * 用会导致调度程序长时间处于暂停状态。
 *
 * @param pxTaskStatusArray 指向 TaskStatus_t 结构数组的指针。
 * 对于 RTOS 控制下的每个任务，该数组必须至少包含一个
 * TaskStatus_t 结构。  RTOS 控制下的任务数量可以
 * 使用 uxTaskGetNumberOfTasks() API 函数确定。
 *
 * @param uxArraySize pxTaskStatusArray 指向的数组的大小
 * 参数。  大小指定为数组中的索引数或
 * 数组中包含的
 * TaskStatus_t 结构数，而不是数组中的字节数。
 *
 * @param pulTotalRunTime 如果 configGENERATE_RUN_TIME_STATS 设置为 1
 * FreeRTOSConfig.h 然后 *pulTotalRunTime 由
 * uxTaskGetSystemState() 设置为总运行 time （由运行时间统计时钟定义，请参见
 * 自目标启动以来为 http://www.freertos.org/rtos-run-time-stats.html)。
 * pulTotalRunTime 可以设置为 NULL 以省略总运行时间信息。
 *
 * @return 填充的 TaskStatus_t 结构的数量
 * uxTaskGetSystemState()。  这应该等于
 * uxTaskGetNumberOfTasks() API 函数返回的
 * 数字，但如果 uxArraySize 参数中传递的值太小，则该数字为零。
 *
 * 用法示例：
   <pre>
    //此示例演示了如何根据
	//uxTaskGetSystemState() 提供的
	//原始数据生成人类可读的运行时统计信息表。人类可读的表写入pcWriteBuffer
	void vTaskGetRunTimeStats( char *pcWriteBuffer )
	{
	TaskStatus_t *pxTaskStatusArray;
	volatile UBaseType_t uxArraySize, x;
	uint32_t ulTotalRunTime, ulStatsAsPercentage;

		//确保写入缓冲区不包含字符串。
		*pcWriteBuffer = 0x00;

		//拍摄任务数量的快照，以防该函
		//数执行时任务数量发生变化。
		uxArraySize = uxTaskGetNumberOfTasks();

		//为每个任务分配一个
		//TaskStatus_t 结构。  数组可以在编译时静态分配。
		pxTaskStatusArray = pvPortMalloc( uxArraySize * sizeof( TaskStatus_t ) );

		if( pxTaskStatusArray != NULL )
		{
			//生成有关每个任务的原始状态信息。
			uxArraySize = uxTaskGetSystemState( pxTaskStatusArray, uxArraySize, &ulTotalRunTime );

			//用于百分比计算。
			ulTotalRunTime /= 100UL;

			//避免除以零的错误。
			if( ulTotalRunTime > 0 )
			{
				//对于 pxTaskStatusArray 数组中的每个填
				//充位置，将原始数据格式化为人类可读的 ASCII 数据
				for( x = 0; x < uxArraySize; x++ )
				{
					//该任务使用了总运行时间的百分比是多少？这将始
					//终向下舍入到最接近的整数。
					//ulTotalRunTimeDiv100 已被除以 100。
					ulStatsAsPercentage = pxTaskStatusArray[ x ].ulRunTimeCounter / ulTotalRunTime;

					if( ulStatsAsPercentage > 0UL )
					{
						sprintf( pcWriteBuffer, "%s\t\t%lu\t\t%lu%%\r\n", pxTaskStatusArray[ x ].pcTaskName, pxTaskStatusArray[ x ].ulRunTimeCounter, ulStatsAsPercentage );
					}
					else
					{
						//如果此处的百分比为零，则任务消耗
						//的总运行时间少于 1% of。
						sprintf( pcWriteBuffer, "%s\t\t%lu\t\t<1%%\r\n", pxTaskStatusArray[ x ].pcTaskName, pxTaskStatusArray[ x ].ulRunTimeCounter );
					}

					pcWriteBuffer += strlen( ( char * ) pcWriteBuffer );
				}
			}

			//不再需要该数组，释放它消耗的内存。
			vPortFree( pxTaskStatusArray );
		}
	}
	</pre>
 */
UBaseType_t uxTaskGetSystemState( TaskStatus_t * const pxTaskStatusArray, const UBaseType_t uxArraySize, uint32_t * const pulTotalRunTime ) PRIVILEGED_FUNCTION;

/**
 * 任务。 h <PRE>void vTaskList(
 * 字符 *pcWriteBuffer );</PRE>
 *
 * configUSE_TRACE_FACILITY 和
 * configUSE_STATS_FORMATTING_FUNCTIONS 必
 * 须都定义为 1，此功能才可用。  有关详细信息，请参阅 FreeRTOS.org 网站的配置部分。
 *
 * NOTE 1：该函数将在其持续时间内禁用中断。  它不
 * 适用于正常的应用程序运行时使用，而是作为调试辅助工具。
 *
 * 列出所有当前任务，及其当前
 * 状态和堆栈使用高水位线。
 *
 * 任务报告为 blocked ('B')、ready ('R')、
 * deleted ('D') 或 suspended ('S')。
 *
 * 请注意：
 *
 * 提供此函数只是为了方便，许多演示应用程序都使
 * 用该函数。  不要将其视为调度程序的一部分。
 *
 * vTaskList() 调用
 * uxTaskGetSystemState()，然后将
 * uxTaskGetSystemState() 输出的一部分格式化为人类可读的表，该表显示任务名称、状态和堆栈使用情况。
 *
 * vTaskList() 依赖于 sprintf() C 库函数，
 * 该函数可能会使代码大小膨胀、使用大量堆栈并在不同平台上提供不同的
 * 结果。  sprintf() 的替代、小型、第三方和有限功能实现
 * 在许多 FreeRTOS/Demo 子目录中的一个名为
 * printf-stdarg.c 的文件中提供（注意
 * printf-stdarg.c 不提供完整的 snprintf() 实现！）。
 *
 * It is recommended that production systems call uxTaskGetSystemState()
 * 直接访问原始统计数据，而不是通过调用
 * vTaskList() 间接访问。
 *
 * @param pcWriteBuffer 一个缓冲区，其中包含上述详细信息
 * 将以 ASCII 形式写入。  假定
 * 该缓冲区足够大以包含生成的报告。
 * 每个任务大约 40 个字节就足够了。
 *
 * \defgroup vTaskList vTaskList
 * \ingroup TaskUtils
 */
void vTaskList( char * pcWriteBuffer ) PRIVILEGED_FUNCTION; /*lint !e971 仅字符串和单个字符允许使用非限定字符类型。 */

/**
 * 任务。 h <PRE>void
 * vTaskGetRunTimeStats( 字符 *pcWriteBuffer );</PRE>
 *
 * configGENERATE_RUN_TIME_STATS 和
 * configUSE_STATS_FORMATTING_FUNCTIONS
 * 必须都定义为 1，此功能才可用。  应用程序还必须提供定义
 * portCONFIGURE_TIMER_FOR_RUN_TIME_STATS() and portGET_RUN_TIME_COUNTER_VALUE()
 * 配置外设定时器/计数器并分别返回
 * 定时器当前计数值。  计数器应至
 * 少是滴答计数频率的 10 倍。
 *
 * NOTE 1：该函数将在其持续时间内禁用中断。  它不
 * 适用于正常的应用程序运行时使用，而是作为调试辅助工具。
 *
 * 将
 * configGENERATE_RUN_TIME_STATS
 * 设置为 1 将导致存储每个任务的总累计执行时间。
 * 累计时间值的分辨率取决于
 * portCONFIGURE_TIMER_FOR_RUN_TIME_STATS()
 * 宏配置的定时器的频率。调用
 * vTaskGetRunTimeStats() 将每个任务的总执行时间写入缓冲区，既作为绝对计数值又作为系统总执行时间的百分比。
 *
 * 注2：
 *
 * 提供此函数只是为了方便，许多演示应用程序都使
 * 用该函数。  不要将其视为调度程序的一部分。
 *
 * vTaskGetRunTimeStats() 调用
 * uxTaskGetSystemState()，然后将
 * uxTaskGetSystemState() 输出的一部分格式化
 * 为人类可读的表格，该表格以绝对值和百分比形式显示每个任务在“运行”状态下花费的时间量。
 *
 * vTaskGetRunTimeStats() 依赖于
 * sprintf() C 库函数，该函数可能会使代码大小膨胀、使用大量
 * 堆栈并在不同平台上提供不同的结果。  sprintf() 的替代、小
 * 型、第三方和有限功能实现在许多 FreeRTOS/Demo 子目录中
 * 的一个名为 printf-stdarg.c 的文件中提供（注意
 * printf-stdarg.c 不提供完整的 snprintf() 实现！）。
 *
 * 建议生产系统直接调用
 * uxTaskGetSystemState() 来访问原
 * 始统计数据，而不是通过调用 vTaskGetRunTimeStats() 来间接访问。
 *
 * @param pcWriteBuffer 执行时间将写入的缓冲区
 * 书面形式为 ASCII 形式。  假定
 * 该缓冲区足够大以包含生成的报告。  每
 * 个任务大约 40 个字节就足够了。
 *
 * \defgroup vTaskGetRunTimeStats vTaskGetRunTimeStats
 * \ingroup TaskUtils
 */
void vTaskGetRunTimeStats( char *pcWriteBuffer ) PRIVILEGED_FUNCTION; /*lint !e971 仅字符串和单个字符允许使用非限定字符类型。 */

/**
* 任务。 h <PRE>uint32_t
* ulTaskGetIdleRunTimeCounter(无效);</PRE>
*
* configGENERATE_RUN_TIME_STATS 和
* configUSE_STATS_FORMATTING_FUNCTIONS
* 必须都定义为 1，此功能才可用。  应用程序还必须提供定义
* portCONFIGURE_TIMER_FOR_RUN_TIME_STATS() and portGET_RUN_TIME_COUNTER_VALUE()
* 配置外设定时器/计数器并分别返回
* 定时器当前计数值。  计数器应至
* 少是滴答计数频率的 10 倍。
*
* 将 configGENERATE_RUN_TIME_STATS 设置
* 为 1 将导致存储每个任务的总累计执行时间。  累计时间值的分辨率取
* 决于
* portCONFIGURE_TIMER_FOR_RUN_TIME_STATS()
* 宏配置的定时器的频率。 uxTaskGetSystemState()
* 和 vTaskGetRunTimeStats() 将每个任务的总执
* 行时间写入缓冲区，而 ulTaskGetIdleRunTimeCounter() 仅返回空闲任务的总执行时间。
*
* @return 空闲任务的总运行时间。  这是时间量
* 空闲任务实际上一直在执行。  时间单位取决于使用
* portCONFIGURE_TIMER_FOR_RUN_TIME_STATS()
* 和 portGET_RUN_TIME_COUNTER_VALUE() 宏配置的频率。
*
* \defgroup ulTaskGetIdleRunTimeCounter ulTaskGetIdleRunTimeCounter
* \ingroup TaskUtils
*/
uint32_t ulTaskGetIdleRunTimeCounter( void ) PRIVILEGED_FUNCTION;

/**
 * 任务。 h <PRE>BaseType_t xTaskNotify( TaskHandle_t
 * xTaskToNotify、uint32_t、ulValue、eNotifyAction、eAction）；</PRE>
 *
 * configUSE_TASK_NOTIFICATIONS
 * 必须未定义或定义为 1，此函数才可用。
 *
 * 当 configUSE_TASK_NOTIFICATIONS 设置为 1 时，每个任务都
 * 有自己的私有“通知值”，该值是 32 位无符号 integer (uint32_t)。
 *
 * 可以使用中间对象将事件发送到任务。
 * 此类对象的示例有队列、信号量、互斥体和
 * 事件组。  任务通知是一种直接向任务发
 * 送事件的方法，无需此类中间对象。
 *
 * 发送到任务的通知可以选择执行操作，例如更
 * 新、覆盖或增加任务的通知值。  通过这种
 * 方式，任务通知可用于向任务发送数据，或用
 * 作轻量且快速的二进制或计数信号量。
 *
 * 发送到任务的通知将保持挂起状态，直到被调用
 * xTaskNotifyWait() 或
 * ulTaskNotifyTake() 的任务清除。
 * 如果通知到达时任务已经处于阻塞状态等待通知，则
 * 该任务将自动从阻塞状态 state 中移除（解除阻塞）并清除通知。
 *
 * 任务可以使用 xTaskNotifyWait() 来[可
 * 选]阻止等待通知待处理，或者使用
 * ulTaskNotifyTake() 来[可选]阻止等待
 * 其通知值具有非零值。  该任务在处于阻塞状态时不会消耗任何 CPU 时间。
 *
 * 详细信息请参见 http://www.FreeRTOS.org/RTOS-task-notifications.html。
 *
 * @param xTaskToNotify 正在通知的任务的句柄。  的句柄
 * 任务可以从用于创建任务的
 * xTaskCreate()API函数返回，并且可以通过
 * 调用xTaskGetCurrentTaskHandle()获取当前运行任务的句柄。
 *
 * @param ulValue 可以与通知一起发送的数据。  数据如何
 * 使用取决于 eAction 参数的值。
 *
 * @param eAction 指定通知如何更新任务的通知
 * 价值，如果有的话。  eAction 的有效值如下：
 *
 * eSetBits - 任务的通知值按位 ORed
 * 与 ulValue。  在这种情况下，
 * xTaskNofify() 始终返回 pdPASS。
 *
 * eIncrement - 任务的通知值递增。
 * 在这种情况下，不使用 ulValue，并且
 * xTaskNotify() 始终返回 pdPASS。
 *
 * eSetValueWithOverwrite -
 * 任务的通知值设置为 ulValue 的值，即使被通
 * 知的任务尚未处理之前的 notification（
 * 该任务已有待处理的通知）。  在这种情况下，
 * xTaskNotify() 始终返回 pdPASS。
 *
 * eSetValueWithoutOverwrite - 如
 * 果被通知的任务尚未有待处理的通知，则该任务的通知值将设置为
 * ulValue，并且 xTaskNotify() 将
 * return pdPASS.  If the task being notified already had a notification
 * 待处理，则不执行任何操作并返回 pdFAIL。
 *
 * eNoAction - 任务收到通知但其
 * 通知值未更新。  在这种情况下，不使用
 * ulValue，并且
 * xTaskNotify() 始终返回 pdPASS。
 *
 * pulPreviousNotificationValue
 * - 可用于在通知函数修改任何位之前传
 * 递主题任务的通知值。
 *
 * @return 取决于 eAction 的值。  请参阅描述
 * eAction参数。
 *
 * \defgroup xTaskNotify xTaskNotify
 * \ingroup TaskNotifications
 */
BaseType_t xTaskGenericNotify( TaskHandle_t xTaskToNotify, uint32_t ulValue, eNotifyAction eAction, uint32_t *pulPreviousNotificationValue ) PRIVILEGED_FUNCTION;
#define xTaskNotify( xTaskToNotify, ulValue, eAction ) xTaskGenericNotify( ( xTaskToNotify ), ( ulValue ), ( eAction ), NULL )
#define xTaskNotifyAndQuery( xTaskToNotify, ulValue, eAction, pulPreviousNotifyValue ) xTaskGenericNotify( ( xTaskToNotify ), ( ulValue ), ( eAction ), ( pulPreviousNotifyValue ) )

/**
 * 任务。 h <PRE>BaseType_t xTaskNotifyFromISR( TaskHandle_t xTaskToNotify、uint32_t
 * ulValue、eNotifyAction eAction、BaseType_t *pxHigherPriorityTaskWoken);</PRE>
 *
 * configUSE_TASK_NOTIFICATIONS
 * 必须未定义或定义为 1，此函数才可用。
 *
 * 当 configUSE_TASK_NOTIFICATIONS 设置为 1 时，每个任务都
 * 有自己的私有“通知值”，该值是 32 位无符号 integer (uint32_t)。
 *
 * 可从中断服务 routine (ISR)
 * 使用的 xTaskNotify() 版本。
 *
 * 可以使用中间对象将事件发送到任务。
 * 此类对象的示例有队列、信号量、互斥体和
 * 事件组。  任务通知是一种直接向任务发
 * 送事件的方法，无需此类中间对象。
 *
 * 发送到任务的通知可以选择执行操作，例如更
 * 新、覆盖或增加任务的通知值。  通过这种
 * 方式，任务通知可用于向任务发送数据，或用
 * 作轻量且快速的二进制或计数信号量。
 *
 * 发送到任务的通知将保持挂起状态，直到被调用
 * xTaskNotifyWait() 或
 * ulTaskNotifyTake() 的任务清除。
 * 如果通知到达时任务已经处于阻塞状态等待通知，则
 * 该任务将自动从阻塞状态 state 中移除（解除阻塞）并清除通知。
 *
 * 任务可以使用 xTaskNotifyWait() 来[可
 * 选]阻止等待通知待处理，或者使用
 * ulTaskNotifyTake() 来[可选]阻止等待
 * 其通知值具有非零值。  该任务在处于阻塞状态时不会消耗任何 CPU 时间。
 *
 * 详细信息请参见 http://www.FreeRTOS.org/RTOS-task-notifications.html。
 *
 * @param xTaskToNotify 正在通知的任务的句柄。  的句柄
 * 任务可以从用于创建任务的
 * xTaskCreate()API函数返回，并且可以通过
 * 调用xTaskGetCurrentTaskHandle()获取当前运行任务的句柄。
 *
 * @param ulValue 可以与通知一起发送的数据。  数据如何
 * 使用取决于 eAction 参数的值。
 *
 * @param eAction 指定通知如何更新任务的通知
 * 价值，如果有的话。  eAction 的有效值如下：
 *
 * eSetBits - 任务的通知值按位 ORed
 * 与 ulValue。  在这种情况下，
 * xTaskNofify() 始终返回 pdPASS。
 *
 * eIncrement - 任务的通知值递增。
 * 在这种情况下，不使用 ulValue，并且
 * xTaskNotify() 始终返回 pdPASS。
 *
 * eSetValueWithOverwrite -
 * 任务的通知值设置为 ulValue 的值，即使被通
 * 知的任务尚未处理之前的 notification（
 * 该任务已有待处理的通知）。  在这种情况下，
 * xTaskNotify() 始终返回 pdPASS。
 *
 * eSetValueWithoutOverwrite - 如
 * 果被通知的任务尚未有待处理的通知，则该任务的通知值将设置为
 * ulValue，并且 xTaskNotify() 将
 * return pdPASS.  If the task being notified already had a notification
 * 待处理，则不执行任何操作并返回 pdFAIL。
 *
 * eNoAction - 任务收到通知但其
 * 通知值未更新。  在这种情况下，不使用
 * ulValue，并且
 * xTaskNotify() 始终返回 pdPASS。
 *
 * @param pxHigherPriorityTaskWoken xTaskNotifyFromISR() 将设置
 * *pxHigherPriorityTaskWoken 到
 * pdTRUE 如果发送通知导致通知发送到的任务离开阻塞状态
 * ，并且未阻塞的任务的优先级高于当前正在运行的任务。  如果
 * xTaskNotifyFromISR() 将此值设置为
 * pdTRUE，则应在退出中断之前请求上下文切换。  如何从
 * ISR 请求上下文切换取决于端口 - 请参阅文档页面
 * for the port in use.
 *
 * @return 取决于 eAction 的值。  请参阅描述
 * eAction参数。
 *
 * \defgroup xTaskNotify xTaskNotify
 * \ingroup TaskNotifications
 */
BaseType_t xTaskGenericNotifyFromISR( TaskHandle_t xTaskToNotify, uint32_t ulValue, eNotifyAction eAction, uint32_t *pulPreviousNotificationValue, BaseType_t *pxHigherPriorityTaskWoken ) PRIVILEGED_FUNCTION;
#define xTaskNotifyFromISR( xTaskToNotify, ulValue, eAction, pxHigherPriorityTaskWoken ) xTaskGenericNotifyFromISR( ( xTaskToNotify ), ( ulValue ), ( eAction ), NULL, ( pxHigherPriorityTaskWoken ) )
#define xTaskNotifyAndQueryFromISR( xTaskToNotify, ulValue, eAction, pulPreviousNotificationValue, pxHigherPriorityTaskWoken ) xTaskGenericNotifyFromISR( ( xTaskToNotify ), ( ulValue ), ( eAction ), ( pulPreviousNotificationValue ), ( pxHigherPriorityTaskWoken ) )

/**
 * 任务。 h <PRE>BaseType_t xTaskNotifyWait( uint32_t ulBitsToClearOnEntry, uint32_t
 * ulBitsToClearOnExit、uint32_t *pulNotificationValue、TickType_t xTicksToWait);</pre>
 *
 * configUSE_TASK_NOTIFICATIONS
 * 必须未定义或定义为 1，此函数才可用。
 *
 * 当 configUSE_TASK_NOTIFICATIONS 设置为 1 时，每个任务都
 * 有自己的私有“通知值”，该值是 32 位无符号 integer (uint32_t)。
 *
 * 可以使用中间对象将事件发送到任务。
 * 此类对象的示例有队列、信号量、互斥体和
 * 事件组。  任务通知是一种直接向任务发
 * 送事件的方法，无需此类中间对象。
 *
 * 发送到任务的通知可以选择执行操作，例如更
 * 新、覆盖或增加任务的通知值。  通过这种
 * 方式，任务通知可用于向任务发送数据，或用
 * 作轻量且快速的二进制或计数信号量。
 *
 * 发送到任务的通知将保持挂起状态，直到被调用
 * xTaskNotifyWait() 或
 * ulTaskNotifyTake() 的任务清除。
 * 如果通知到达时任务已经处于阻塞状态等待通知，则
 * 该任务将自动从阻塞状态 state 中移除（解除阻塞）并清除通知。
 *
 * 任务可以使用 xTaskNotifyWait() 来[可
 * 选]阻止等待通知待处理，或者使用
 * ulTaskNotifyTake() 来[可选]阻止等待
 * 其通知值具有非零值。  该任务在处于阻塞状态时不会消耗任何 CPU 时间。
 *
 * 详细信息请参见 http://www.FreeRTOS.org/RTOS-task-notifications.html。
 *
 * @param ulBitsToClearOnEntry ulBitsToClearOnEntry 值中设置的位
 * 在任务检查是否有任何通知待处理之前，将在调用任务的通知值中清除，并
 * 且如果没有通知待处理，则可以选择阻止。  将
 * ulBitsToClearOnEntry 设置为
 * ULONG_MAX（如果包含 limits.h）或 0
 * xffffffffUL（如果不包含 limits.h）将具有将任务
 * 的通知值重置为 0 的效果。将 ulBitsToClearOnEntry 设置为 0 将使任务的通知值保持不变。
 *
 * @param ulBitsToClearOnExit 如果之前有通知待处理或收到
 * 调用任务退出 xTaskNotifyWait() 函数，然后使用
 * pulNotificationValue 参数传递任务的通知 value
 * （请参阅 xTaskNotify() API 函数）。  然后，在
 * ulBitsToClearOnExit 中设置的任何位都将在任务的通知
 * value 中被清除（注意 *pulNotificationValue
 * 在清除任何位之前设置）。  将 ulBitsToClearOnExit
 * 设置为 ULONG_MAX（如果包含 limits.h）或 0
 * xffffffffUL（如果不包含 limits.h）将具有在函数退出之
 * 前将任务的通知值重置为 0 的效果。  将
 * ulBitsToClearOnExit 设置为 0 将使函数 exits
 * 时任务的通知值保持不变（在这种情况下，pulNotificationValue 中传递的值将与任务的通知值匹配）。
 *
 * @param pulNotificationValue 用于将任务的通知值传出
 * 的函数。  请注意，传递的值不会受到因
 * ulBitsToClearOnExit 非零而导致的任何位清除的影响。
 *
 * @param xTicksToWait 任务应等待的最长时间
 * 如果调用 xTaskNotifyWait() 时通知尚未
 * 挂起，则为要接收的通知的阻塞状态。  当任务处于阻塞状态
 * 时，不会消耗任何处理时间。  这是在内核刻度中指定的，宏
 * pdMS_TO_TICSK( value_in_ms
 * ) 可用于将以毫秒指定的时间转换为以毫秒指定的时间
 * 滴答声。
 *
 * @return 如果通知是 received（包括之前的通知）
 * 当 xTaskNotifyWait 被调用时已经挂起）
 * ，然后返回 pdPASS。  否则返回 pdFAIL。
 *
 * \defgroup xTaskNotifyWait xTaskNotifyWait
 * \ingroup TaskNotifications
 */
BaseType_t xTaskNotifyWait( uint32_t ulBitsToClearOnEntry, uint32_t ulBitsToClearOnExit, uint32_t *pulNotificationValue, TickType_t xTicksToWait ) PRIVILEGED_FUNCTION;

/**
 * 任务。 h <PRE>BaseType_t
 * xTaskNotifyGive( TaskHandle_t xTaskToNotify );</PRE>
 *
 * configUSE_TASK_NOTIFICATIONS
 * 必须未定义或定义为 1 才能使该宏可用。
 *
 * 当 configUSE_TASK_NOTIFICATIONS 设置为 1 时，每个任务都
 * 有自己的私有“通知值”，该值是 32 位无符号 integer (uint32_t)。
 *
 * 可以使用中间对象将事件发送到任务。
 * 此类对象的示例有队列、信号量、互斥体和
 * 事件组。  任务通知是一种直接向任务发
 * 送事件的方法，无需此类中间对象。
 *
 * 发送到任务的通知可以选择执行操作，例如更
 * 新、覆盖或增加任务的通知值。  通过这种
 * 方式，任务通知可用于向任务发送数据，或用
 * 作轻量且快速的二进制或计数信号量。
 *
 * xTaskNotifyGive() 是一个辅助宏，旨在
 * 将任务通知用作轻量且更快的二进制或计数信号量等价物时使
 * 用。实际的 FreeRTOS 信号量是使用
 * xSemaphoreGive() API 函数给出的，
 * 使用任务通知的等效操作是 xTaskNotifyGive()。
 *
 * 当任务通知用作二进制或计数信号量时，被通知的任
 * 务应使用
 * ulTaskNotificationTake()
 * API 函数而不是 xTaskNotifyWait() API 函数等待通知。
 *
 * 有关详细信息，请参阅 http://www.FreeRTOS.org/RTOS-task-notifications.html。
 *
 * @param xTaskToNotify 正在通知的任务的句柄。  的句柄
 * 任务可以从用于创建任务的
 * xTaskCreate()API函数返回，并且可以通过
 * 调用xTaskGetCurrentTaskHandle()获取当前运行任务的句柄。
 *
 * @return xTaskNotifyGive() 是一个调用 xTaskNotify() 的宏
 * eAction 参数设置为 eIncrement - 因此始终返回 pdPASS。
 *
 * \defgroup xTaskNotifyGive xTaskNotifyGive
 * \ingroup TaskNotifications
 */
#define xTaskNotifyGive( xTaskToNotify ) xTaskGenericNotify( ( xTaskToNotify ), ( 0 ), eIncrement, NULL )

/**
 * 任务。 h <PRE>void vTaskNotifyGiveFromISR( TaskHandle_t
 * xTaskHandle, BaseType_t *pxHigherPriorityTaskWoken );
 *
 * configUSE_TASK_NOTIFICATIONS
 * 必须未定义或定义为 1 才能使该宏可用。
 *
 * 当 configUSE_TASK_NOTIFICATIONS 设置为 1 时，每个任务都
 * 有自己的私有“通知值”，该值是 32 位无符号 integer (uint32_t)。
 *
 * 可以从中断服务 routine (ISR) 调用
 * 的 xTaskNotifyGive() 版本。
 *
 * 可以使用中间对象将事件发送到任务。
 * 此类对象的示例有队列、信号量、互斥体和
 * 事件组。  任务通知是一种直接向任务发
 * 送事件的方法，无需此类中间对象。
 *
 * 发送到任务的通知可以选择执行操作，例如更
 * 新、覆盖或增加任务的通知值。  通过这种
 * 方式，任务通知可用于向任务发送数据，或用
 * 作轻量且快速的二进制或计数信号量。
 *
 * vTaskNotifyGiveFromISR() 适用于任务通
 * 知用作轻量级且更快的二进制或计数信号量等价物时。实际的
 * FreeRTOS 信号量是使用
 * xSemaphoreGiveFromISR() API 函数从
 * ISR 给出的，使用任务通知的等效操作是 vTaskNotifyGiveFromISR()。
 *
 * 当任务通知用作二进制或计数信号量时，被通知的任
 * 务应使用
 * ulTaskNotificationTake()
 * API 函数而不是 xTaskNotifyWait() API 函数等待通知。
 *
 * 有关详细信息，请参阅 http://www.FreeRTOS.org/RTOS-task-notifications.html。
 *
 * @param xTaskToNotify 正在通知的任务的句柄。  的句柄
 * 任务可以从用于创建任务的
 * xTaskCreate()API函数返回，并且可以通过
 * 调用xTaskGetCurrentTaskHandle()获取当前运行任务的句柄。
 *
 * @param pxHigherPriorityTaskWoken vTaskNotifyGiveFromISR() 将设置
 * *pxHigherPriorityTaskWoken 到
 * pdTRUE 如果发送通知导致通知发送到的任务离开阻塞状态，
 * 并且未阻塞的任务的优先级高于当前正在运行的任务。  如果
 * vTaskNotifyGiveFromISR() 将此值设置
 * 为 pdTRUE，则应在退出中断之前请求上下文切换。  如何
 * 从 ISR 请求上下文切换取决于端口 - 请参阅文档页面
 * for the port in use.
 *
 * \defgroup xTaskNotifyWait xTaskNotifyWait
 * \ingroup TaskNotifications
 */
void vTaskNotifyGiveFromISR( TaskHandle_t xTaskToNotify, BaseType_t *pxHigherPriorityTaskWoken ) PRIVILEGED_FUNCTION;

/**
 * 任务。 h <PRE>uint32_t ulTaskNotifyTake( BaseType_t
 * xClearCountOnExit、TickType_t xTicksToWait );</pre>
 *
 * configUSE_TASK_NOTIFICATIONS
 * 必须未定义或定义为 1，此函数才可用。
 *
 * 当 configUSE_TASK_NOTIFICATIONS 设置为 1 时，每个任务都
 * 有自己的私有“通知值”，该值是 32 位无符号 integer (uint32_t)。
 *
 * 可以使用中间对象将事件发送到任务。
 * 此类对象的示例有队列、信号量、互斥体和
 * 事件组。  任务通知是一种直接向任务发
 * 送事件的方法，无需此类中间对象。
 *
 * 发送到任务的通知可以选择执行操作，例如更
 * 新、覆盖或增加任务的通知值。  通过这种
 * 方式，任务通知可用于向任务发送数据，或用
 * 作轻量且快速的二进制或计数信号量。
 *
 * ulTaskNotifyTake() 适用于将任务通知
 * 用作更快、更轻量的二进制或计数信号量替代方案时使用。
 * 实际的 FreeRTOS 信号量是使用
 * xSemaphoreTake() API 函数获取的，
 * 使用任务通知的等效操作是 ulTaskNotifyTake()。
 *
 * 当任务使用其通知值作为二进制或计数信号量时
 * other tasks should send notifications to it using the xTaskNotifyGive()
 * 宏，或 xTaskNotify() 函数，其中 eAction 参数设置为
 * eIncrement.
 *
 * ulTaskNotifyTake() 可以在退出时
 * 将任务的通知值清除为零，在这种情况下，通知值的作用
 * 类似于二进制信号量，或者在退出时递减任务的通知值，
 * 在这种情况下，通知值的作用类似于计数信号量。
 *
 * 任务可以使用
 * ulTaskNotifyTake() 来[可选]
 * 阻塞以等待任务的通知值非零。  该任务在处于阻塞状态时不消耗任何 CPU 时间。
 *
 * 当通知待处理时，
 * xTaskNotifyWait() 将返回，
 * 而当任务的通知值不为零时，ulTaskNotifyTake() 将返回。
 *
 * 详细信息请参见 http://www.FreeRTOS.org/RTOS-task-notifications.html。
 *
 * @param xClearCountOnExit 如果 xClearCountOnExit 是 pdFALSE 则任务的
 * 当函数退出时，通知值会递减。  通过这种方式，通知值就像
 * 一个计数信号量。  如果
 * xClearCountOnExit 不是 pdFALSE
 * ，则当函数退出时，任务的通知值将清除为零。  通过这种方式，通知值就像二进制文件一样
 * 信号。
 *
 * @param xTicksToWait 任务应等待的最长时间
 * 如果调用 ulTaskNotifyTake()
 * 时计数尚未大于零，则任务通知值的阻塞状态将大于零
 * 。  当任务处于阻塞状态时，不会消耗任何处理时间
 * 。  这是在内核刻度中指定的，宏
 * pdMS_TO_TICSK(
 * value_in_ms ) 可用于将以毫秒指定的时间转换为以刻度指定的时间。
 *
 * @return 任务在清零或之前的通知计数
 * decremented（参见xClearCountOnExit参数）。
 *
 * \defgroup ulTaskNotifyTake ulTaskNotifyTake
 * \ingroup TaskNotifications
 */
uint32_t ulTaskNotifyTake( BaseType_t xClearCountOnExit, TickType_t xTicksToWait ) PRIVILEGED_FUNCTION;

/**
 * 任务。 h <PRE>BaseType_t
 * xTaskNotifyStateClear( TaskHandle_t xTask );</pre>
 *
 * 如果句柄xTask引用的任务的通知状态为
 * eNotified，则将该任务的通知状态设置为
 * eNotWaitingNotification。任务的
 * 通知值不会更改。  设置xTask为NULL，清除调用任务的通知状态。
 *
 * @return pdTRUE 如果任务的通知状态设置为
 * eNotWaitingNotification，否则为 pdFALSE。
 * \defgroup xTaskNotifyStateClear xTaskNotifyStateClear
 * \ingroup TaskNotifications
 */
BaseType_t xTaskNotifyStateClear( TaskHandle_t xTask );

/**
* 任务。 h <PRE>uint32_t ulTaskNotifyValueClear(
* TaskHandle_t xTask、uint32_t ulBitsToClear );</pre>
*
* 清除 xTask 引用的任务的通知值中由
* ulBitsToClear 位掩码指定的位。
*
* 将 ulBitsToClear 设置为 0xffffffff（32
* 位架构上为 UINT_MAX）可将通知值清除为 0。将
* ulBitsToClear 设置为 0 可查询任务的通知值而不清除任何位。
*
* @return 目标任务的通知值之前的位的值
* ulBitsToClear 指定的值被清除。
* \defgroup ulTaskNotifyValueClear ulTaskNotifyValueClear
* \ingroup TaskNotifications
*/
uint32_t ulTaskNotifyValueClear( TaskHandle_t xTask, uint32_t ulBitsToClear ) PRIVILEGED_FUNCTION;

/**
 * task.h
 * <pre>void vTaskSetTimeOutState( TimeOut_t * const pxTimeOut )</pre>
 *
 * 使用 xTaskCheckForTimeOut() 捕获当前时间以供将来使用。
 *
 * @param pxTimeOut 指向超时对象的指针，当前时间放入其中
 * 是要被捕获的。  捕获的时间包括滴答计数以
 * 及自系统首次启动以来滴答计数溢出的次数。
 * \defgroup vTaskSetTimeOutState vTaskSetTimeOutState
 * \ingroup TaskCtrl
 */
void vTaskSetTimeOutState( TimeOut_t * const pxTimeOut ) PRIVILEGED_FUNCTION;

/**
 * task.h
 * <pre>BaseType_t xTaskCheckForTimeOut( TimeOut_t * const pxTimeOut, TickType_t * const pxTicksToWait );</pre>
 *
 * 确定自使用调用
 * vTaskSetTimeOutState() 捕获时间以
 * 来 pxTicksToWait 刻度是否已过去。  捕获的时间包括滴答计数和滴答计数溢出的次数。
 *
 * @param pxTimeOut 先前使用捕获的时间状态
 * vTaskSetTimeOutState。如果
 * 尚未发生超时，则会更新它以反映当前时间状态。
 * @param pxTicksToWait 检查超时的滴答数，即如果
 * 自 pxTimeOut 是最后一个 updated（通过
 * vTaskSetTimeOutState() 或
 * xTaskCheckForTimeOut()）以来，
 * pxTicksToWait 刻度已过，已发生超时。如果未发生超时，则更新 pxTIcksToWait 以反映剩余滴答数。
 *
 * @return 如果发生超时，则返回 pdTRUE。否则 pdFALSE 是
 * 返回并更新 pxTicksToWait 以反映剩余数量
 * 滴答声。
 *
 * @see https://www.freertos.org/xTaskCheckForTimeOut.html
 *
 * 用法示例：
 * <pre>
	//驱动程序库函数用于从由 UART 中断填充的 Rx 缓
	//冲区接收 uxWantedBytes。如果 Rx 缓冲
	//区中没有足够的字节，则任务进入阻塞状态，直到通知它有更
	//多数据已放入缓冲区。如果仍然没有足够的数据，则任务重新
	//进入Blocked状态，并使用
	//xTaskCheckForTimeOut()重新计算
	//Block时间，以确保处于Blocked状态的总时间不
	//超过MAX_TIME_TO_WAIT。这种情况一直持续
	//到缓冲区至少包含 uxWantedBytes 字节，或
	//者处于阻塞状态的总时间达到
	//MAX_TIME_TO_WAIT，此时任务会读取可用的字节数，最多可达 uxWantedBytes。

	size_t xUART_Receive( uint8_t *pucBuffer, size_t uxWantedBytes )
	{
	size_t uxReceived = 0;
	TickType_t xTicksToWait = MAX_TIME_TO_WAIT;
	TimeOut_t xTimeOut;

		//初始化xTimeOut。
		//这记录了进入该功能的时间。
		vTaskSetTimeOutState( &xTimeOut );

		//循环直到缓冲区包含所需的
		//字节数，或者发生超时。
		while( UART_bytes_in_rx_buffer( pxUARTInstance ) < uxWantedBytes )
		{
			//缓冲区没有包含足够的数据，因此该任务将进
			//入阻塞状态。调整
			//xTicksToWait 以考虑到目前为
			//止该函数内处于阻塞状态的任何时间，以确保
			//处于阻塞状态的总时间不超过 MAX_TIME_TO_WAIT。
			if( xTaskCheckForTimeOut( &xTimeOut, &xTicksToWait ) != pdFALSE )
			{
				//在所需的字节数可用之
				//前超时，退出循环。
				break;
			}

			//等待最多 xTicksToWait 时钟周
			//期以通知接收中断已将更多数据放入缓冲区。
			ulTaskNotifyTake( pdTRUE, xTicksToWait );
		}

		//尝试将 uxWantedBytes 从接收缓冲区
		//读入 pucBuffer。返回实际字节数
		//read（可能小于 uxWantedBytes）。
		uxReceived = UART_read_from_receive_buffer( pxUARTInstance,
													pucBuffer,
													uxWantedBytes );

		return uxReceived;
	}
 </pre>
 * \defgroup xTaskCheckForTimeOut xTaskCheckForTimeOut
 * \ingroup TaskCtrl
 */
BaseType_t xTaskCheckForTimeOut( TimeOut_t * const pxTimeOut, TickType_t * const pxTicksToWait ) PRIVILEGED_FUNCTION;

/*-----------------------------------------------------------
 * 可用于移植目的的调度程序内部
 *----------------------------------------------------------*/

/*
 * 此功能不得在应用程序代码中使用。它只是
 * 旨在实现调度程序的端口时使用，并且
 * 是调度程序专用的接口。
 *
 * 从实时内核 tick（抢占式或协作式）调用，
 * 这会增加滴答计数并检查是否有任何任务被阻塞
 * for a finite period required removing from a blocked list and placing on
 * 一份准备好的清单。  如果返回非零值
 * ，则需要上下文切换，因为： + 任务
 * 由于超时已过期而被从阻止列表中删除，
 * 或者
 * + 时间切片正在使用，并且有一个任务与
 * 当前正在运行的任务具有相同的优先级。
 */
BaseType_t xTaskIncrementTick( void ) PRIVILEGED_FUNCTION;

/*
 * 此功能不得在应用程序代码中使用。它是
 * 调度程序专用的接口。
 *
 * 必须在禁用中断的情况下调用此函数。
 *
 * 从就绪列表中删除调用任务，并将其放
 * 入等待特定事件的任务列表和延迟任务
 * 列表中。  如果事件 occur（
 * 并且没有更高优先级的任务在等待同一
 * 事件）或延迟期到期，则该任务将从两
 * 个列表中删除并替换为就绪列表。
 *
 * “无序”版本将事件列表项值替换为
 * xItemValue 值，并将列表项插入到列表末尾。
 *
 * “有序”版本使用现有的事件列表项
 * value（这是所属任务优先级）将列
 * 表项插入到事件列表中是任务优先级顺序。
 *
 * @param pxEventList 包含阻塞等待任务的列表
 * for the event to occur.
 *
 * @param xItemValue 事件列表项使用的项值
 * 事件列表不按任务优先级排序。
 *
 * @param xTicksToWait 任务应等待的最长时间
 * for the event to occur.  This is specified in kernel ticks,the constant
 * portTICK_PERIOD_MS 可用于将内核刻度转换为实时
 * 时期。
 */
void vTaskPlaceOnEventList( List_t * const pxEventList, const TickType_t xTicksToWait ) PRIVILEGED_FUNCTION;
void vTaskPlaceOnUnorderedEventList( List_t * pxEventList, const TickType_t xItemValue, const TickType_t xTicksToWait ) PRIVILEGED_FUNCTION;

/*
 * 此功能不得在应用程序代码中使用。它是
 * 调度程序专用的接口。
 *
 * 必须在禁用中断的情况下调用此函数。
 *
 * 该函数执行的功能与
 * vTaskPlaceOnEventList() 几乎相同。区
 * 别在于该函数不允许任务无限期地阻塞，而 vTaskPlaceOnEventList() 则允许。
 *
 */
void vTaskPlaceOnEventListRestricted( List_t * const pxEventList, TickType_t xTicksToWait, const BaseType_t xWaitIndefinitely ) PRIVILEGED_FUNCTION;

/*
 * 此功能不得在应用程序代码中使用。它是
 * 调度程序专用的接口。
 *
 * 必须在禁用中断的情况下调用此函数。
 *
 * 从指定的事件列表和阻塞任务列表中
 * 删除任务，并将其放入就绪队列中。
 *
 * xTaskRemoveFromEventList()/vTaskRemoveFromUnorderedEventList()将被调用
 * if either an event occurs to unblock a task, or the block timeout period
 * 过期。
 *
 * 当事件列表按任务优先级顺序排列时，使用
 * xTaskRemoveFromEventList()。  它
 * 从事件列表的头部删除列表项，因为该列表项将具有事件列表上所有
 * 任务的最高优先级拥有任务。当事件列表未排序并且事件列表项包含
 * 除所属任务优先级之外的其他内容时，使用
 * vTaskRemoveFromUnorderedEventList()
 * 。  在这种情况下，事件列表项值将更新为 xItemValue 参数中传递的值。
 *
 * @return pdTRUE 如果正在删除的任务比该任务具有更高的优先级
 * 拨打电话，否则为 pdFALSE。
 */
BaseType_t xTaskRemoveFromEventList( const List_t * const pxEventList ) PRIVILEGED_FUNCTION;
void vTaskRemoveFromUnorderedEventList( ListItem_t * pxEventListItem, const TickType_t xItemValue ) PRIVILEGED_FUNCTION;

/*
 * 此功能不得在应用程序代码中使用。它只是
 * 旨在实现调度程序的端口时使用，并且
 * 是调度程序专用的接口。
 *
 * 将当前TCB的指针设置为准备运
 * 行的最高优先级任务的TCB。
 */
portDONT_DISCARD void vTaskSwitchContext( void ) PRIVILEGED_FUNCTION;

/*
 * 这些功能不得使用
 * 应用程序代码。它们由事件位模块使用。
 */
TickType_t uxTaskResetEventItemValue( void ) PRIVILEGED_FUNCTION;

/*
 * 返回调用任务的句柄。
 */
TaskHandle_t xTaskGetCurrentTaskHandle( void ) PRIVILEGED_FUNCTION;

/*
 * 队列实现使用的快捷方式来防止不必要的调用
 * taskYIELD();
 */
void vTaskMissedYield( void ) PRIVILEGED_FUNCTION;

/*
 * 返回调度程序状态为 taskSCHEDULER_RUNNING、
 * taskSCHEDULER_NOT_STARTED 或 taskSCHEDULER_SUSPENDED。
 */
BaseType_t xTaskGetSchedulerState( void ) PRIVILEGED_FUNCTION;

/*
 * 如果互斥锁持有者的优先级低于调用任务，则将
 * 互斥持有者的优先级提高到调用任务的优先级。
 */
BaseType_t xTaskPriorityInherit( TaskHandle_t const pxMutexHolder ) PRIVILEGED_FUNCTION;

/*
 * 如果任务在持有信号量时继承了更高的优先级
 * ，则将任务的优先级设置回其适当的优先级。
 */
BaseType_t xTaskPriorityDisinherit( TaskHandle_t const pxMutexHolder ) PRIVILEGED_FUNCTION;

/*
 * 如果较高优先级任务尝试获取互斥量导致较低优
 * 先级任务继承较高优先级任务的优先级 - 但
 * 较高优先级任务在未获取互斥量的情况下超时，
 * 则较低优先级任务将再次取消优先级 - 但仅
 * 降低到仍在等待 mutex 的最高优先级任
 * 务（如果有多个任务在等待互斥量）。
 */
void vTaskPriorityDisinheritAfterTimeout( TaskHandle_t const pxMutexHolder, UBaseType_t uxHighestPriorityWaitingTask ) PRIVILEGED_FUNCTION;

/*
 * 获取分配给 xTask 参数引用的任务的 uxTCBNumber。
 */
UBaseType_t uxTaskGetTaskNumber( TaskHandle_t xTask ) PRIVILEGED_FUNCTION;

/*
 * 将xTask参数引用的任务的uxTaskNumber设置为
 * uxHandle.
 */
void vTaskSetTaskNumber( TaskHandle_t xTask, const UBaseType_t uxHandle ) PRIVILEGED_FUNCTION;

/*
 * 仅当
 * configUSE_TICKLESS_IDLE
 * 设置为 1 时可用。如果使用无滴答模式或
 * 实现低功耗模式，则滴答中断将不会在空闲期间
 * 执行。  在这种情况下，调度程序维护的滴答
 * 计数值需要通过向前跳过等于空闲周期的时间来与实际执行时间保持同步。
 */
void vTaskStepTick( const TickType_t xTicksToJump ) PRIVILEGED_FUNCTION;

/* 在应用程序代码长时间禁用中断后，更
正滴答计数值。
xTicksToCatchUp 是
由于中断被禁用而错过的节拍中断数。
它的值不会自动计算，因此必须由应用程序编写者计算。

此函数类似于 vTaskStepTick()，但是，与
vTaskStepTick() 不同，
xTaskCatchUpTicks() 可能会将滴答计数向前移
动超过任务应从阻塞状态中删除的时间。  这意味着任务可能必须从阻塞状态中删除，因为滴答计数为
感动了。 */
BaseType_t xTaskCatchUpTicks( TickType_t xTicksToCatchUp ) PRIVILEGED_FUNCTION;

/*
 * 仅当 configUSE_TICKLESS_IDLE 设置
 * 为 1 时可用。提供在
 * portSUPPRESS_TICKS_AND_SLEEP()
 * 内使用，以允许端口特定睡眠函数确定是否可以继续睡眠、是否可以继续、是否可以无限期睡眠。
 *
 * 该函数是必要的，因为
 * portSUPPRESS_TICKS_AND_SLEEP()
 * 仅在调度程序挂起时调用，而不是在关键部分内调用。  因此
 * ，中断可以请求
 * portSUPPRESS_TICKS_AND_SLEEP()
 * 和实际进入的低功耗模式之间的上下文切换。
 * eTaskConfirmSleepModeStatus() 应该从定时器停止和进入睡眠模式之间的一个短关键部分调用，以确保可以进入睡眠模式。
 */
eSleepModeStatus eTaskConfirmSleepModeStatus( void ) PRIVILEGED_FUNCTION;

/*
 * 仅供内部使用。  当获取互斥锁时，增加互斥锁
 * 持有的计数，并返回获取该互斥锁的任务的句柄。
 */
TaskHandle_t pvTaskIncrementMutexHeldCount( void ) PRIVILEGED_FUNCTION;

/*
 * 仅供内部使用。  与 vTaskSetTimeOutState() 相同，但没有临界区
 * 部分。
 */
void vTaskInternalSetTimeOutState( TimeOut_t * const pxTimeOut ) PRIVILEGED_FUNCTION;


#ifdef __cplusplus
}
#endif
#endif /* INC_TASK_H */



