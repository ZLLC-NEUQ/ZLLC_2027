/**
 * @file drv_uart.cpp
 * @author cjw by yssickjgd
 * @brief UART通信初始化与配置流程
 * @version 0.1
 * @date 2025-07-1 0.1 26赛季定稿
 *
 * @copyright ZLLC 2026
 * 
 */

/* Includes ------------------------------------------------------------------*/

#include "drv_uart.h"
#include "string.h"
#include "dvc_dwt.h"
#include "freertos_tasks.h"
/* Private macros ------------------------------------------------------------*/

/* Private types -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/

Struct_UART_Manage_Object UART1_Manage_Object = {0};
Struct_UART_Manage_Object UART2_Manage_Object = {0};
Struct_UART_Manage_Object UART3_Manage_Object = {0};
Struct_UART_Manage_Object UART4_Manage_Object = {0};
Struct_UART_Manage_Object UART5_Manage_Object = {0};
Struct_UART_Manage_Object UART6_Manage_Object = {0};
Struct_UART_Manage_Object UART7_Manage_Object = {0};
Struct_UART_Manage_Object UART8_Manage_Object = {0};
Struct_UART_Manage_Object UART9_Manage_Object = {0};
Struct_UART_Manage_Object UART10_Manage_Object = {0};

Struct_UART5_Rx_Diagnostics UART5_Rx_Diagnostics = {};
static volatile uint8_t UART5_Rx_Restart_Pending = 0;
static volatile uint8_t UART5_Rx_Aligned = 0;

/* Private function declarations ---------------------------------------------*/

/* function prototypes -------------------------------------------------------*/

static uint16_t UART_Rx_DMA_Length(uint16_t length, uint8_t multiplier)
{
    uint32_t dma_length = (uint32_t)length * multiplier;
    return (dma_length <= UART_BUFFER_SIZE) ? (uint16_t)dma_length : UART_BUFFER_SIZE;
}

/* UART5 固定使用普通 DMA 的两帧缓冲，正常接收只由 IDLE 分帧。 */
static HAL_StatusTypeDef UART5_Rx_Start(void)
{
    UART_HandleTypeDef *huart = UART5_Manage_Object.UART_Handler;
    HAL_StatusTypeDef status = HAL_UARTEx_ReceiveToIdle_DMA(
        huart, UART5_Manage_Object.Rx_Buffer,
        UART_Rx_DMA_Length(UART5_Manage_Object.Rx_Buffer_Length, 2));
    UART5_Rx_Diagnostics.Last_Start_Status = status;
    if (status == HAL_OK)
    {
        __HAL_DMA_DISABLE_IT(huart->hdmarx, DMA_IT_HT);
    }
    else
    {
        UART5_Rx_Diagnostics.Restart_Failures++;
        UART5_Rx_Aligned = 0;
        UART5_Rx_Restart_Pending = 1;
    }
    return status;
}

uint8_t UART5_Rx_RecoveryPending(void)
{
    return UART5_Rx_Restart_Pending;
}

void UART5_Rx_Service(void)
{
    UART_HandleTypeDef *huart = UART5_Manage_Object.UART_Handler;
    if (huart == NULL || UART5_Rx_Restart_Pending == 0)
    {
        return;
    }

    /* 与 UART5/RX DMA ISR 互斥，避免正在恢复时另一个回调重启 DMA。 */
    taskENTER_CRITICAL();
    if (huart->hdmarx->State == HAL_DMA_STATE_ABORT)
    {
        /* 等 DMA 中断完成异步错误终止，临界区内不能等待该中断。 */
        taskEXIT_CRITICAL();
        return;
    }
    UART5_Rx_Restart_Pending = 0;
    UART5_Rx_Aligned = 0;
    HAL_StatusTypeDef status = HAL_UART_AbortReceive(huart);
    if (status == HAL_OK)
    {
        /* AbortReceive 清除错误标志及 RDR；首个 IDLE 只用于找回帧边界。 */
        UART5_Rx_Start();
    }
    else
    {
        UART5_Rx_Diagnostics.Last_Start_Status = status;
        UART5_Rx_Diagnostics.Restart_Failures++;
        UART5_Rx_Restart_Pending = 1;
    }
    taskEXIT_CRITICAL();
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == UART5)
    {
        UART5_Rx_Diagnostics.Error_Events++;
        UART5_Rx_Diagnostics.Last_Error = huart->ErrorCode;
        UART5_Rx_Aligned = 0;
        UART5_Rx_Restart_Pending = 1;
    }
}

static void USART_RxDMA_MultiBufferStart(UART_HandleTypeDef *huart, uint32_t *SrcAddress, uint32_t *DstAddress, uint32_t *SecondMemAddress, uint32_t DataLength){


 huart->ReceptionType = HAL_UART_RECEPTION_TOIDLE;

 huart->RxEventType = HAL_UART_RXEVENT_TC;

 huart->RxXferSize    = DataLength;

 SET_BIT(huart->Instance->CR3,USART_CR3_DMAR);

 __HAL_UART_ENABLE_IT(huart, UART_IT_IDLE); 

 HAL_DMAEx_MultiBufferStart(&hdma_uart5_rx,(uint32_t)SrcAddress,(uint32_t)DstAddress,(uint32_t)SecondMemAddress,DataLength);
	

}
uint8_t Power_receive_data[32];
float temp_power = 0;
/**
 * @brief 初始化UART
 *
 * @param huart UART编号
 * @param Callback_Function 处理回调函数
 */
void UART_Init(UART_HandleTypeDef *huart, UART_Call_Back Callback_Function, uint16_t Rx_Buffer_Length)
{
    if (Rx_Buffer_Length == 0 || Rx_Buffer_Length > UART_BUFFER_SIZE)
    {
        return;
    }

    if (huart->Instance == USART1)
    {
        UART1_Manage_Object.UART_Handler = huart;
        UART1_Manage_Object.Callback_Function = Callback_Function;
        UART1_Manage_Object.Rx_Buffer_Length = Rx_Buffer_Length;
        HAL_UARTEx_ReceiveToIdle_DMA(huart, UART1_Manage_Object.Rx_Buffer, UART1_Manage_Object.Rx_Buffer_Length);
        __HAL_DMA_DISABLE_IT(&hdma_usart1_rx, DMA_IT_HT);
        memset(UART1_Manage_Object.Rx_Buffer,0,UART1_Manage_Object.Rx_Buffer_Length);
    }
    else if (huart->Instance == UART5)
    {
        UART5_Manage_Object.UART_Handler = huart;
        UART5_Manage_Object.Callback_Function = Callback_Function;
        UART5_Manage_Object.Rx_Buffer_Length = Rx_Buffer_Length;
        UART5_Rx_Aligned = 0;
        UART5_Rx_Restart_Pending = 0;
        UART5_Rx_Start();
    }
    else if (huart->Instance == UART7)
    {
        UART7_Manage_Object.UART_Handler = huart;
        UART7_Manage_Object.Callback_Function = Callback_Function;
        UART7_Manage_Object.Rx_Buffer_Length = Rx_Buffer_Length;
        HAL_UARTEx_ReceiveToIdle_DMA(huart, UART7_Manage_Object.Rx_Buffer, UART7_Manage_Object.Rx_Buffer_Length);
    }
    else if (huart->Instance == UART8)
    {
        UART8_Manage_Object.UART_Handler = huart;
        UART8_Manage_Object.Callback_Function = Callback_Function;
        UART8_Manage_Object.Rx_Buffer_Length = Rx_Buffer_Length;
        HAL_UARTEx_ReceiveToIdle_DMA(huart, UART8_Manage_Object.Rx_Buffer, UART8_Manage_Object.Rx_Buffer_Length);
    }
    else if (huart->Instance == UART9)
    {
        UART9_Manage_Object.UART_Handler = huart;
        UART9_Manage_Object.Callback_Function = Callback_Function;
        UART9_Manage_Object.Rx_Buffer_Length = Rx_Buffer_Length;
        HAL_UARTEx_ReceiveToIdle_DMA(huart, UART9_Manage_Object.Rx_Buffer, UART9_Manage_Object.Rx_Buffer_Length);
    }
    else if (huart->Instance == USART10)
    {
        UART10_Manage_Object.UART_Handler = huart;
        UART10_Manage_Object.Callback_Function = Callback_Function;
        UART10_Manage_Object.Rx_Buffer_Length = Rx_Buffer_Length;
        HAL_UARTEx_ReceiveToIdle_DMA(huart, UART10_Manage_Object.Rx_Buffer, UART10_Manage_Object.Rx_Buffer_Length);
        __HAL_DMA_DISABLE_IT(&hdma_usart10_rx, DMA_IT_HT);
        memset(UART10_Manage_Object.Rx_Buffer,0,UART10_Manage_Object.Rx_Buffer_Length);
    }
    
}

/**
 * @brief 发送数据帧
 *
 * @param huart UART编号
 * @param Data 被发送的数据指针
 * @param Length 长度
 * @return uint8_t 执行状态
 */
uint8_t UART_Send_Data(UART_HandleTypeDef *huart, uint8_t *Data, uint16_t Length)
{
    return (HAL_UART_Transmit_DMA(huart, Data, Length));
}

/**
 * @brief UART的TIM定时器中断发送回调函数
 *
 */
void TIM_UART_PeriodElapsedCallback()
{
	
}


/**
 * @brief 辅助函数：把 UART 接收数据直接投递给控制任务
 *
 * @param obj UART 管理对象
 * @param instance_id UART 实例编号
 * @param Size 接收长度
 * @param pxHigherPriorityTaskWoken 上下文切换标志
 */
static void UART_Rx_EnqueueFromISR(Struct_UART_Manage_Object *obj, uint8_t instance_id,
                                    uint16_t Size,
                                    BaseType_t *pxHigherPriorityTaskWoken)
{
    if (Size == 0 || Size > FREERTOS_UART_RX_MAX_LEN || Size > UART_BUFFER_SIZE ||
        xQueue_Control_Rx == NULL)
    {
        ulUART_RxDropped++;
        return;
    }

    FreeRTOS_Control_RxMsg_t msg = {};
    const uint16_t copy_len = Size;

    msg.type = FREERTOS_CONTROL_INPUT_UART;
    msg.payload.uart.uart_instance = instance_id;
    msg.payload.uart.length = copy_len;
    msg.payload.uart.rx_cycle = DWT->CYCCNT;
    memcpy(msg.payload.uart.data, obj->Rx_Buffer, copy_len);

    /* 队列满即丢帧计数，ISR 内绝不阻塞。 */
    if (xQueueSendFromISR(xQueue_Control_Rx, &msg, pxHigherPriorityTaskWoken) != pdPASS)
    {
        ulUART_RxDropped++;
        return;
    }
}

/**
 * @brief HAL库UART接收DMA空闲中断（FreeRTOS 版本）
 *
 * @param huart UART编号
 * @param Size 长度
 *
 * @note 仅将数据直接投递控制队列并重启 DMA，实际解析在控制任务。
 */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    //选择回调函数
    if (huart->Instance == USART1)
    {
        UART_Rx_EnqueueFromISR(&UART1_Manage_Object, 1, Size, &xHigherPriorityTaskWoken);
        HAL_UARTEx_ReceiveToIdle_DMA(huart, UART1_Manage_Object.Rx_Buffer, UART_Rx_DMA_Length(UART1_Manage_Object.Rx_Buffer_Length, 2));
        __HAL_DMA_DISABLE_IT(&hdma_usart1_rx, DMA_IT_HT);
    }
    else if (huart->Instance == UART5)
    {
        const HAL_UART_RxEventTypeTypeDef event = HAL_UARTEx_GetRxEventType(huart);
        UART5_Rx_Diagnostics.Last_Size = Size;
        if (event == HAL_UART_RXEVENT_HT)
        {
            /* 半帧绝不提交，也不重启仍在运行的 DMA。 */
            return;
        }
        if (UART5_Rx_Restart_Pending != 0)
        {
            return;
        }
        if (event == HAL_UART_RXEVENT_IDLE)
        {
            UART5_Rx_Diagnostics.Idle_Events++;
            if (UART5_Rx_Aligned != 0 && Size == UART5_Manage_Object.Rx_Buffer_Length)
            {
                UART_Rx_EnqueueFromISR(&UART5_Manage_Object, 5, Size, &xHigherPriorityTaskWoken);
            }
            else
            {
                ulUART_RxDropped++;
            }
            /* 不管本段是否完整，IDLE 后下一次 DMA 都从新帧的第 0 字节接收。 */
            UART5_Rx_Aligned = 1;
        }
        else
        {
            /* 缓冲收满不能证明协议帧结束；丢弃到下一次 IDLE 再建立边界。 */
            UART5_Rx_Diagnostics.Overflow_Events++;
            UART5_Rx_Aligned = 0;
            ulUART_RxDropped++;
        }
        UART5_Rx_Start();
    }
    else if (huart->Instance == UART7)
    {
        UART_Rx_EnqueueFromISR(&UART7_Manage_Object, 7, Size, &xHigherPriorityTaskWoken);
        HAL_UARTEx_ReceiveToIdle_DMA(huart, UART7_Manage_Object.Rx_Buffer, UART_Rx_DMA_Length(UART7_Manage_Object.Rx_Buffer_Length, 2));
    }
    else if (huart->Instance == UART8)
    {
        if (Size <= UART8_Manage_Object.Rx_Buffer_Length)
        {
            UART_Rx_EnqueueFromISR(&UART8_Manage_Object, 8, Size, &xHigherPriorityTaskWoken);
        }
        else
        {
            ulUART_RxDropped++;
        }
        HAL_UARTEx_ReceiveToIdle_DMA(huart, UART8_Manage_Object.Rx_Buffer, UART8_Manage_Object.Rx_Buffer_Length);
    }
    else if (huart->Instance == UART9)
    {
        if (Size <= UART9_Manage_Object.Rx_Buffer_Length)
        {
            UART_Rx_EnqueueFromISR(&UART9_Manage_Object, 9, Size, &xHigherPriorityTaskWoken);
        }
        else
        {
            ulUART_RxDropped++;
        }
        HAL_UARTEx_ReceiveToIdle_DMA(huart, UART9_Manage_Object.Rx_Buffer, UART9_Manage_Object.Rx_Buffer_Length);
    }
    else if (huart->Instance == USART10)
    {
        UART_Rx_EnqueueFromISR(&UART10_Manage_Object, 10, Size, &xHigherPriorityTaskWoken);
        HAL_UARTEx_ReceiveToIdle_DMA(huart, UART10_Manage_Object.Rx_Buffer, UART10_Manage_Object.Rx_Buffer_Length);
        __HAL_DMA_DISABLE_IT(&hdma_usart10_rx, DMA_IT_HT);
    }

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

// void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
// {
//     if (huart->Instance == USART1)
//     {
//         UART1_Manage_Object.Rx_Length = 21;
//         if( UART1_Manage_Object.Rx_Length<=UART1_Manage_Object.Rx_Buffer_Length)
//             UART1_Manage_Object.Callback_Function(UART1_Manage_Object.Rx_Buffer, 21);
//         else
//         memset( UART1_Manage_Object.Rx_Buffer, 0, UART1_Manage_Object.Rx_Buffer_Length);
        
//         HAL_UART_Receive_DMA(huart, UART1_Manage_Object.Rx_Buffer, UART1_Manage_Object.Rx_Buffer_Length);
//         //HAL_UART_Receive_IT(huart, UART1_Manage_Object.Rx_Buffer, UART1_Manage_Object.Rx_Buffer_Length);//再开启接收中断
//     }
// }

/************************ COPYRIGHT(C) USTC-ROBOWALKER **************************/
