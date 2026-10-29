/**
 * @file drv_uart.cpp
 * @author HGCSJ
 * @brief UART通信初始化与配置流程
 * @version V1.1
 * @date 2026-10-30 27赛季定稿
 *
 * @copyright ZLLC 2027
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
Struct_UART_Recovery_Diagnostics UART1_Recovery_Diagnostics = {};
Struct_UART_Recovery_Diagnostics UART5_Recovery_Diagnostics = {};
Struct_UART_Recovery_Diagnostics UART10_Recovery_Diagnostics = {};
static volatile uint8_t UART5_Rx_Aligned = 0;

/* Private function declarations ---------------------------------------------*/

/* function prototypes -------------------------------------------------------*/

static uint16_t UART_Rx_DMA_Length(uint16_t length, uint8_t multiplier)
{
    uint32_t dma_length = (uint32_t)length * multiplier;
    return (dma_length <= UART_BUFFER_SIZE) ? (uint16_t)dma_length : UART_BUFFER_SIZE;
}

static Struct_UART_Recovery_Diagnostics *UART_Get_Recovery(UART_HandleTypeDef *huart)
{
    if (huart == NULL) { return NULL; }
    if (huart->Instance == USART1) { return &UART1_Recovery_Diagnostics; }
    if (huart->Instance == UART5) { return &UART5_Recovery_Diagnostics; }
    if (huart->Instance == USART10) { return &UART10_Recovery_Diagnostics; }
    return NULL;
}

static HAL_StatusTypeDef UART_Rx_Start(Struct_UART_Manage_Object *object,
                                      Struct_UART_Recovery_Diagnostics *diagnostics,
                                      uint16_t dma_length)
{
    HAL_StatusTypeDef status = HAL_UARTEx_ReceiveToIdle_DMA(
        object->UART_Handler, object->Rx_Buffer, dma_length);
    diagnostics->Last_Start_Status = status;
    if (status == HAL_OK)
    {
        __HAL_DMA_DISABLE_IT(object->UART_Handler->hdmarx, DMA_IT_HT);
    }
    else
    {
        diagnostics->Restart_Failures++;
        diagnostics->Rx_Restart_Pending = 1;
        if (object->UART_Handler->Instance == UART5) { UART5_Rx_Aligned = 0; }
    }
    return status;
}

uint8_t UART_RecoveryPending(UART_HandleTypeDef *huart)
{
    Struct_UART_Recovery_Diagnostics *diagnostics = UART_Get_Recovery(huart);
    if (diagnostics != NULL)
    {
        return diagnostics->Rx_Restart_Pending || diagnostics->Tx_Restart_Pending;
    }
    return 0;
}

/* HAL 的 DMA 错误处理可能已清 DMAR/DMAT，但另一方向的 DMA 仍在运行。
 * 先让 UART HAL 结束传输，再异步终止遗留 DMA；不在临界区等待硬件/中断。
 */
static HAL_StatusTypeDef UART_Abort_DMA_Remainder(DMA_HandleTypeDef *hdma)
{
    if (hdma != NULL && hdma->State == HAL_DMA_STATE_BUSY)
    {
        hdma->XferAbortCallback = NULL;
        return HAL_DMA_Abort_IT(hdma);
    }
    if (hdma != NULL && hdma->State == HAL_DMA_STATE_ERROR)
    {
        /* TE 后 HAL 可能留下 ERROR，Start_IT 只接受 READY。
         * 确认 EN 已清才允许 Init，使它的停流等待循环不会在临界区执行。
         * 若硬件尚未停流，留到下个周期检查，不复位其他外设共用的 DMA 控制器。
         */
        if (IS_DMA_STREAM_INSTANCE(hdma->Instance) == 0U ||
            (((DMA_Stream_TypeDef *)hdma->Instance)->CR & DMA_SxCR_EN) != 0U)
        {
            return HAL_BUSY;
        }
        return HAL_DMA_Init(hdma);
    }
    return HAL_OK;
}

static void UART_Recovery_Service(Struct_UART_Manage_Object *object,
                                  Struct_UART_Recovery_Diagnostics *diagnostics,
                                  uint8_t multiplier)
{
    UART_HandleTypeDef *huart = object->UART_Handler;
    if (huart == NULL ||
        (diagnostics->Rx_Restart_Pending == 0 && diagnostics->Tx_Restart_Pending == 0))
    {
        return;
    }

    taskENTER_CRITICAL();
    /* 等待 HAL 锁释放及异步终止完成；发送启动通过 UART_Send_Data 的临界区互斥。 */
    if (huart->Lock == HAL_LOCKED ||
        (huart->hdmarx != NULL && huart->hdmarx->State == HAL_DMA_STATE_ABORT) ||
        (huart->hdmatx != NULL && huart->hdmatx->State == HAL_DMA_STATE_ABORT))
    {
        taskEXIT_CRITICAL();
        return;
    }

    HAL_StatusTypeDef status = HAL_OK;
    if (diagnostics->Tx_Restart_Pending != 0)
    {
        status = HAL_UART_AbortTransmit_IT(huart);
        if (status == HAL_OK) { status = UART_Abort_DMA_Remainder(huart->hdmatx); }
        if (status == HAL_OK &&
            (huart->hdmatx == NULL || huart->hdmatx->State != HAL_DMA_STATE_ABORT))
        {
            /* 错误帧交给业务层丢弃；这里只恢复下一次发送所需的 READY 状态。 */
            diagnostics->Tx_Restart_Pending = 0;
        }
    }

    if (status == HAL_OK && diagnostics->Rx_Restart_Pending != 0)
    {
        /* DR16 只保留协议分帧状态：恢复后首个 IDLE 用于重建帧边界。 */
        if (huart->Instance == UART5) { UART5_Rx_Aligned = 0; }
        status = HAL_UART_AbortReceive_IT(huart);
        if (status == HAL_OK) { status = UART_Abort_DMA_Remainder(huart->hdmarx); }
        if (status == HAL_OK && huart->hdmarx->State != HAL_DMA_STATE_ABORT)
        {
            __HAL_UART_CLEAR_FLAG(huart, UART_CLEAR_OREF | UART_CLEAR_NEF |
                                        UART_CLEAR_PEF | UART_CLEAR_FEF);
            __HAL_UART_SEND_REQ(huart, UART_RXDATA_FLUSH_REQUEST);
            diagnostics->Rx_Restart_Pending = 0;
            UART_Rx_Start(object, diagnostics,
                          UART_Rx_DMA_Length(object->Rx_Buffer_Length, multiplier));
        }
    }

    if (status != HAL_OK)
    {
        diagnostics->Last_Start_Status = status;
        diagnostics->Restart_Failures++;
    }
    if (diagnostics->Rx_Restart_Pending == 0 && diagnostics->Tx_Restart_Pending == 0)
    {
        diagnostics->Recovery_Count++;
    }
    taskEXIT_CRITICAL();
}

void UART_Error_Service(void)
{
    UART_Recovery_Service(&UART1_Manage_Object, &UART1_Recovery_Diagnostics, 2);
    UART_Recovery_Service(&UART5_Manage_Object, &UART5_Recovery_Diagnostics, 2);
    UART_Recovery_Service(&UART10_Manage_Object, &UART10_Recovery_Diagnostics, 1);
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    Struct_UART_Recovery_Diagnostics *diagnostics = UART_Get_Recovery(huart);
    if (diagnostics != NULL)
    {
        diagnostics->Error_Events++;
        diagnostics->Last_Error = huart->ErrorCode;
        diagnostics->Rx_Restart_Pending = 1;
        if (huart->Instance == UART5) { UART5_Rx_Aligned = 0; }
        /* UART_DMAError 会同时结束 UART 的收发状态，恢复遗留的 TX DMA。
         * UART5 只接收、没有 TX DMA，不额外发起发送恢复。
         */
        if ((huart->ErrorCode & HAL_UART_ERROR_DMA) != 0U && huart->hdmatx != NULL)
        {
            diagnostics->Tx_Restart_Pending = 1;
        }
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
        memset(UART1_Manage_Object.Rx_Buffer,0,UART1_Manage_Object.Rx_Buffer_Length);
        UART_Rx_Start(&UART1_Manage_Object, &UART1_Recovery_Diagnostics,
                      UART1_Manage_Object.Rx_Buffer_Length);
    }
    else if (huart->Instance == UART5)
    {
        UART5_Manage_Object.UART_Handler = huart;
        UART5_Manage_Object.Callback_Function = Callback_Function;
        UART5_Manage_Object.Rx_Buffer_Length = Rx_Buffer_Length;
        UART5_Rx_Aligned = 0;
        UART5_Recovery_Diagnostics.Rx_Restart_Pending = 0;
        UART5_Recovery_Diagnostics.Tx_Restart_Pending = 0;
        UART_Rx_Start(&UART5_Manage_Object, &UART5_Recovery_Diagnostics,
                      UART_Rx_DMA_Length(UART5_Manage_Object.Rx_Buffer_Length, 2));
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
        memset(UART10_Manage_Object.Rx_Buffer,0,UART10_Manage_Object.Rx_Buffer_Length);
        UART_Rx_Start(&UART10_Manage_Object, &UART10_Recovery_Diagnostics,
                      UART10_Manage_Object.Rx_Buffer_Length);
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
    /* HAL 发送 DMA 启动不等待硬件；保护启动步骤，避免控制任务中途执行恢复。 */
    taskENTER_CRITICAL();
    HAL_StatusTypeDef status = HAL_BUSY;
    if (!UART_RecoveryPending(huart))
    {
        status = HAL_UART_Transmit_DMA(huart, Data, Length);
        if (status == HAL_ERROR && (huart->ErrorCode & HAL_UART_ERROR_DMA) != 0U)
        {
            HAL_UART_ErrorCallback(huart);
        }
    }
    taskEXIT_CRITICAL();
    return status;
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
#if PERFORMANCE_TEST_ENABLE
    msg.payload.uart.rx_cycle = DWT->CYCCNT;
#endif
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
        if (UART1_Recovery_Diagnostics.Rx_Restart_Pending != 0) { return; }
        UART_Rx_EnqueueFromISR(&UART1_Manage_Object, 1, Size, &xHigherPriorityTaskWoken);
        UART_Rx_Start(&UART1_Manage_Object, &UART1_Recovery_Diagnostics,
                      UART_Rx_DMA_Length(UART1_Manage_Object.Rx_Buffer_Length, 2));
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
        if (UART5_Recovery_Diagnostics.Rx_Restart_Pending != 0)
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
        UART_Rx_Start(&UART5_Manage_Object, &UART5_Recovery_Diagnostics,
                      UART_Rx_DMA_Length(UART5_Manage_Object.Rx_Buffer_Length, 2));
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
        if (UART10_Recovery_Diagnostics.Rx_Restart_Pending != 0) { return; }
        UART_Rx_EnqueueFromISR(&UART10_Manage_Object, 10, Size, &xHigherPriorityTaskWoken);
        UART_Rx_Start(&UART10_Manage_Object, &UART10_Recovery_Diagnostics,
                      UART10_Manage_Object.Rx_Buffer_Length);
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
