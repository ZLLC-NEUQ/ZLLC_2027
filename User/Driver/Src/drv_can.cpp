/**
 * @file drv_can.cpp
 * @author HGCSJ
 * @brief CAN通信初始化与配置流程
 * @version V1.1
 * @date 2026-10-30 27赛季定稿
 *
 * @copyright ZLLC 2027
 *
 */

/* Includes ------------------------------------------------------------------*/
#include "ita_chariot.h"
#include "drv_can.h"
#include "main.h"
#include "freertos_tasks.h"
/* Private macros ------------------------------------------------------------*/

/* Private types -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/

Struct_CAN_Manage_Object CAN1_Manage_Object = {0};
Struct_CAN_Manage_Object CAN2_Manage_Object = {0};
Struct_CAN_Manage_Object CAN3_Manage_Object = {0};

Struct_CAN_Recovery_Diagnostics CAN1_Recovery_Diagnostics = {};
Struct_CAN_Recovery_Diagnostics CAN2_Recovery_Diagnostics = {};
Struct_CAN_Recovery_Diagnostics CAN3_Recovery_Diagnostics = {};

Struct_CAN_IO_Diagnostics CAN1_IO_Diagnostics = {};
Struct_CAN_IO_Diagnostics CAN2_IO_Diagnostics = {};
Struct_CAN_IO_Diagnostics CAN3_IO_Diagnostics = {};

struct Struct_CAN_Recovery_State
{
    Struct_CAN_Manage_Object *Object;
    Struct_CAN_Recovery_Diagnostics *Diagnostics;
    volatile uint8_t Reinit_Required;
    uint8_t Retry_Waiting;
    uint32_t Last_Attempt_ms;
    Struct_CAN_IO_Diagnostics *IO;
};

static Struct_CAN_Recovery_State CAN_Recovery_States[] =
{
    {&CAN1_Manage_Object, &CAN1_Recovery_Diagnostics, 0U, 0U, 0U, &CAN1_IO_Diagnostics},
    {&CAN2_Manage_Object, &CAN2_Recovery_Diagnostics, 0U, 0U, 0U, &CAN2_IO_Diagnostics},
    {&CAN3_Manage_Object, &CAN3_Recovery_Diagnostics, 0U, 0U, 0U, &CAN3_IO_Diagnostics}
};

static const uint32_t CAN_RECOVERY_RETRY_MS = 100U;
static const uint32_t CAN_RECOVERY_ERROR_MASK =
    HAL_FDCAN_ERROR_RAM_ACCESS | HAL_FDCAN_ERROR_RAM_WDG | HAL_FDCAN_ERROR_RESERVED_AREA;
static const uint32_t CAN_RECOVERY_NOTIFICATIONS =
    FDCAN_IT_BUS_OFF | FDCAN_IT_RAM_ACCESS_FAILURE |
    FDCAN_IT_RAM_WATCHDOG | FDCAN_IT_RESERVED_ADDRESS_ACCESS;

static const uint8_t CAN_DLC_TO_BYTES[16] =
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 12, 16, 20, 24, 32, 48, 64};

/* 仅包围无等待的 RAM 复制/寄存器操作；保存原状态，兼容启动期及嵌套临界区。
 * 不在此临界区调用回调、Stop/Init/Start 或任何可能等待的操作。 */
static uint32_t CAN_Lock(void)
{
    const uint32_t mask = __get_PRIMASK();
    __disable_irq();
    __DMB();
    return mask;
}

static void CAN_Unlock(uint32_t mask)
{
    __DMB();
    __set_PRIMASK(mask);
}

static Struct_CAN_Recovery_State *CAN_Get_Recovery_State(FDCAN_HandleTypeDef *hcan)
{
    if (hcan == NULL) { return NULL; }
    if (hcan->Instance == FDCAN1) { return &CAN_Recovery_States[0]; }
    if (hcan->Instance == FDCAN2) { return &CAN_Recovery_States[1]; }
    if (hcan->Instance == FDCAN3) { return &CAN_Recovery_States[2]; }
    return NULL;
}

static IRQn_Type CAN_Get_IRQn(FDCAN_HandleTypeDef *hcan)
{
    if (hcan->Instance == FDCAN1) { return FDCAN1_IT0_IRQn; }
    if (hcan->Instance == FDCAN2) { return FDCAN2_IT0_IRQn; }
    return FDCAN3_IT0_IRQn;
}

/* 首次初始化与严重错误后重初始化共用，保留每路原有 FIFO/过滤策略。 */
static HAL_StatusTypeDef CAN_Configure_Reception(FDCAN_HandleTypeDef *hcan)
{
    const uint8_t fifo1 = (hcan->Instance == FDCAN2);
    FDCAN_FilterTypeDef filter = {};
    filter.IdType = FDCAN_STANDARD_ID;
    filter.FilterIndex = 0U;
    filter.FilterType = FDCAN_FILTER_MASK;
    filter.FilterConfig = fifo1 ? FDCAN_FILTER_TO_RXFIFO1 : FDCAN_FILTER_TO_RXFIFO0;
    filter.FilterID1 = 0U;
    filter.FilterID2 = 0U;

    HAL_StatusTypeDef status = HAL_FDCAN_ConfigFilter(hcan, &filter);
    if (status != HAL_OK) { return status; }
    if (hcan->Instance == FDCAN3)
    {
        status = HAL_FDCAN_ConfigGlobalFilter(hcan, FDCAN_REJECT, FDCAN_REJECT,
                                            FDCAN_REJECT_REMOTE, FDCAN_REJECT_REMOTE);
        if (status != HAL_OK) { return status; }
        status = HAL_FDCAN_ConfigFifoWatermark(hcan, FDCAN_CFG_RX_FIFO0, 1U);
        if (status != HAL_OK) { return status; }
    }
    return HAL_FDCAN_ActivateNotification(hcan,
        CAN_RECOVERY_NOTIFICATIONS |
        (fifo1 ? (FDCAN_IT_RX_FIFO1_NEW_MESSAGE | FDCAN_IT_RX_FIFO1_FULL | FDCAN_IT_RX_FIFO1_MESSAGE_LOST)
               : (FDCAN_IT_RX_FIFO0_NEW_MESSAGE | FDCAN_IT_RX_FIFO0_FULL | FDCAN_IT_RX_FIFO0_MESSAGE_LOST)), 0U);
}

static void CAN_Recover_One(Struct_CAN_Recovery_State *recovery)
{
    FDCAN_HandleTypeDef *hcan = recovery->Object->CAN_Handler;
    Struct_CAN_Recovery_Diagnostics *diagnostics = recovery->Diagnostics;
    if (hcan == NULL) { return; }

    /* BO 在硬件恢复期间仍可为 1；INIT 已清除时不能重复打断恢复序列。 */
    if (diagnostics->Recovery_Pending == 0U &&
        (hcan->Instance->PSR & FDCAN_PSR_BO) != 0U &&
        (hcan->Instance->CCCR & FDCAN_CCCR_INIT) != 0U)
    {
        diagnostics->Recovery_Pending = 1U;
    }
    if (diagnostics->Recovery_Pending == 0U) { return; }

    const uint32_t now = HAL_GetTick();
    if (recovery->Retry_Waiting != 0U &&
        (uint32_t)(now - recovery->Last_Attempt_ms) < CAN_RECOVERY_RETRY_MS)
    {
        return;
    }
    recovery->Retry_Waiting = 1U;
    recovery->Last_Attempt_ms = now;
    diagnostics->Restart_Attempts++;

    /* 仅屏蔽本路 IRQ，HAL_Init 的超时仍需 TIM6 时基继续运行。 */
    const IRQn_Type irqn = CAN_Get_IRQn(hcan);
    const uint32_t irq_enabled = NVIC_GetEnableIRQ(irqn);
    HAL_NVIC_DisableIRQ(irqn);
    /* 不清空 UART/USB 共用队列；只使本路旧 FD 消息失效。 */
    recovery->Object->FD_Rx_Generation++;

    HAL_StatusTypeDef status = HAL_OK;
    if (hcan->State == HAL_FDCAN_STATE_BUSY)
    {
        /* Stop 设置 CCE，同时清空旧收发 FIFO；周期任务随后发送最新控制量。 */
        status = HAL_FDCAN_Stop(hcan);
    }
    if (status == HAL_OK &&
        (recovery->Reinit_Required != 0U || hcan->State != HAL_FDCAN_STATE_READY))
    {
        /* ERROR 状态不能再次 Stop；只重建本路配置/RAM，不复位共享 FDCAN 时钟。 */
        status = HAL_FDCAN_Init(hcan);
    }
    if (status == HAL_OK)
    {
        status = CAN_Configure_Reception(hcan);
    }
    if (status == HAL_OK)
    {
        __HAL_FDCAN_CLEAR_FLAG(hcan,
            CAN_RECOVERY_NOTIFICATIONS |
            FDCAN_FLAG_RX_FIFO0_NEW_MESSAGE | FDCAN_FLAG_RX_FIFO1_NEW_MESSAGE);
        status = HAL_FDCAN_Start(hcan);
    }

    diagnostics->Last_Recovery_Status = status;
    if (status == HAL_OK)
    {
        diagnostics->Restart_Successes++;
        diagnostics->Recovery_Pending = 0U;
        recovery->Reinit_Required = 0U;
    }
    else
    {
        diagnostics->Last_Error = hcan->ErrorCode;
        diagnostics->Restart_Failures++;
        /* 下次尝试重建 ERROR/部分配置失败状态，避免永久卡在 Stop 的前置条件。 */
        recovery->Reinit_Required = 1U;
    }
    if (irq_enabled != 0U) { HAL_NVIC_EnableIRQ(irqn); }
}

void CAN_Error_Service(void)
{
    for (uint32_t i = 0U; i < sizeof(CAN_Recovery_States) / sizeof(CAN_Recovery_States[0]); ++i)
    {
        CAN_Recover_One(&CAN_Recovery_States[i]);
    }
}

// CAN通信发送缓冲区
uint8_t CAN1_0x1ff_Tx_Data[8];
uint8_t CAN1_0x1fe_Tx_Data[8];
uint8_t CAN1_0x200_Tx_Data[8];
uint8_t CAN1_0x2ff_Tx_Data[8];
uint8_t CAN1_0xxf1_Tx_Data[8];
uint8_t CAN1_0xxf2_Tx_Data[8];
uint8_t CAN1_0xxf3_Tx_Data[8];
uint8_t CAN1_0xxf4_Tx_Data[8];
uint8_t CAN1_0xxf5_Tx_Data[8];
uint8_t CAN1_0xxf6_Tx_Data[8];
uint8_t CAN1_0xxf7_Tx_Data[8];
uint8_t CAN1_0xxf8_Tx_Data[8];

uint8_t CAN2_0x1ff_Tx_Data[8];
uint8_t CAN2_0x1fe_Tx_Data[8];
uint8_t CAN2_0x200_Tx_Data[8];
uint8_t CAN2_0x2ff_Tx_Data[8];
uint8_t CAN2_0xxf1_Tx_Data[8];
uint8_t CAN2_0xxf2_Tx_Data[8];
uint8_t CAN2_0xxf3_Tx_Data[8];
uint8_t CAN2_0xxf4_Tx_Data[8];
uint8_t CAN2_0xxf5_Tx_Data[8];
uint8_t CAN2_0xxf6_Tx_Data[8];
uint8_t CAN2_0xxf7_Tx_Data[8];
uint8_t CAN2_0xxf8_Tx_Data[8];
uint8_t CAN2_Gimbal_Tx_Chassis_Data[8];   // 云台给底盘发送缓冲区
uint8_t CAN2_Chassis_Tx_Gimbal_Data[8];   // 底盘给云台发送缓冲区
uint8_t CAN2_Gimbal_Tx_Chassis_Data_1[8]; // 云台给底盘发送缓冲区
uint8_t CAN2_Chassis_Tx_Gimbal_Data_1[8]; // 底盘给云台发送缓冲区
uint8_t CAN2_Gimbal_Tx_Chassis_Data_2[8]; // 云台给底盘发送缓冲区

uint8_t CAN3_0x1ff_Tx_Data[8];
uint8_t CAN3_0x1fe_Tx_Data[8];
uint8_t CAN3_0x200_Tx_Data[8];
uint8_t CAN3_0x2ff_Tx_Data[8];
uint8_t CAN3_0xxf1_Tx_Data[8];
uint8_t CAN3_0xxf2_Tx_Data[8];
uint8_t CAN3_0xxf3_Tx_Data[8];
uint8_t CAN3_0xxf4_Tx_Data[8];
uint8_t CAN3_0xxf5_Tx_Data[8];
uint8_t CAN3_0xxf6_Tx_Data[8];
uint8_t CAN3_0xxf7_Tx_Data[8];
uint8_t CAN3_0xxf8_Tx_Data[8];

uint8_t CAN_Supercap_Tx_Data[8];
uint8_t CAN3_Chassis_Tx_Data_A[8];   //底盘给云台发送缓冲区
uint8_t CAN3_Chassis_Tx_Data_B[8];   //底盘给云台发送缓冲区
uint8_t CAN3_Chassis_Tx_Data_C[8];   //底盘给云台发送缓冲区
uint8_t CAN3_Chassis_Tx_Data_D[8];   //底盘给云台发送缓冲区
uint8_t CAN3_Chassis_Tx_Data_E[8];   //底盘给云台发送缓冲区
uint8_t CAN3_Chassis_Tx_Data_F[8];   //底盘给云台发送缓冲区
uint8_t CAN3_Chassis_Tx_Data_G[8];   //底盘给云台发送缓冲区
uint8_t CAN3_MiniPC_Tx_Data_A[8];   //下位机发送缓冲区
uint8_t CAN3_MiniPC_Tx_Data_B[8];   //下位机发送缓冲区
uint8_t CAN3_MiniPC_Tx_Data_C[8];   //下位机发送缓冲区
uint8_t CAN3_MiniPC_Tx_Data_D[8];   //下位机发送缓冲区

uint8_t CAN1_MiniPc_Tx_Data[8];
uint8_t CAN2_MiniPc_Tx_Data[8];
/*********LK电机 控制缓冲区***********/
uint8_t CAN1_0x141_Tx_Data[8];
uint8_t CAN1_0x142_Tx_Data[8];
uint8_t CAN1_0x143_Tx_Data[8];
uint8_t CAN1_0x144_Tx_Data[8];
uint8_t CAN1_0x145_Tx_Data[8];
uint8_t CAN1_0x146_Tx_Data[8];
uint8_t CAN1_0x147_Tx_Data[8];
uint8_t CAN1_0x148_Tx_Data[8];

uint8_t CAN2_0x141_Tx_Data[8];    
uint8_t CAN2_0x142_Tx_Data[8];
uint8_t CAN2_0x143_Tx_Data[8];
uint8_t CAN2_0x144_Tx_Data[8];
uint8_t CAN2_0x145_Tx_Data[8];    
uint8_t CAN2_0x146_Tx_Data[8];
uint8_t CAN2_0x147_Tx_Data[8];
uint8_t CAN2_0x148_Tx_Data[8];

uint8_t CAN3_0x141_Tx_Data[8];
uint8_t CAN3_0x142_Tx_Data[8];
uint8_t CAN3_0x143_Tx_Data[8];
uint8_t CAN3_0x144_Tx_Data[8];
uint8_t CAN3_0x145_Tx_Data[8];
uint8_t CAN3_0x146_Tx_Data[8];
uint8_t CAN3_0x147_Tx_Data[8];
uint8_t CAN3_0x148_Tx_Data[8];


uint8_t CAN_DM4340_Reset[8]={0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFB};
/* Private function declarations ---------------------------------------------*/

/* function prototypes -------------------------------------------------------*/

/**
 * @brief 配置CAN的过滤器
 *
 * @param hcan CAN编号
 * @param Object_Para 筛选器编号0-27 | FIFOx | ID类型 | 帧类型
 * @param ID 验证码
 * @param Mask_ID 屏蔽码(0x3ff, 0x1fffffff)
 */
//void can_filter_mask_config(FDCAN_HandleTypeDef *hcan, uint8_t Object_Para, uint32_t ID, uint32_t Mask_ID)
//{
//	
//    //检测传参是否正确
//    assert_param(hcan != NULL);

//	   //CAN过滤器初始化结构体
//    FDCAN_FilterTypeDef can_filter_init_structure;
//    //滤波器序号, 0-27, 共28个滤波器
//    can_filter_init_structure.FilterBank = Object_Para >> 3;
//    //滤波器模式，设置ID掩码模式
//    can_filter_init_structure.FilterMode = CAN_FILTERMODE_IDMASK;
//    
//	
//    if ((Object_Para & 0x02))
//    {   
//        //29位 拓展帧
//			  // 32位滤波
//        can_filter_init_structure.FilterScale = CAN_FILTERSCALE_32BIT;
//        //验证码 高16bit
//        can_filter_init_structure.FilterIdHigh = (ID << 3) >> 16;
//        //验证码 低16bit
//        can_filter_init_structure.FilterIdLow = ID << 3 | (Object_Para & 0x03) << 1;
//        //屏蔽码 高16bit
//        can_filter_init_structure.FilterMaskIdHigh = (Mask_ID << 3) >> 16;
//        //屏蔽码 低16bit
//        can_filter_init_structure.FilterMaskIdLow = Mask_ID << 3 | (0x03) << 1 ;
//    }
//    else
//    {
//        //11位 标准帧
//			  // 32位滤波
//        can_filter_init_structure.FilterScale = CAN_FILTERSCALE_16BIT;
//        //标准帧验证码 高16bit不启用
//        can_filter_init_structure.FilterIdHigh = 0x0000 ; 
//        //验证码 低16bit
//			  can_filter_init_structure.FilterIdLow =ID << 5 | (Object_Para & 0x02) << 4;  
//        //标准帧屏蔽码 高16bit不启用
//        can_filter_init_structure.FilterMaskIdHigh =  0x0000 ;
//        //屏蔽码 低16bit
//        can_filter_init_structure.FilterMaskIdLow =(Mask_ID << 5) | 0x01 << 4 ; 
//    }

//    //滤波器绑定FIFO0或FIFO1
//    can_filter_init_structure.FilterFIFOAssignment = (Object_Para >> 2) & 0x01;
//    //从机模式选择开始单元 , 前14个在CAN1, 后14个在CAN2
//    can_filter_init_structure.SlaveStartFilterBank = 14;
//    //使能滤波器
//    can_filter_init_structure.FilterActivation = ENABLE;

//    // 过滤器配置
//    if(HAL_CAN_ConfigFilter(hcan, &can_filter_init_structure)!=HAL_OK)
//    {
//        Error_Handler();
//    }
//	
//}

/**
************************************************************************
* @brief:      	can_filter_init(void)
* @param:       void
* @retval:     	void
* @details:    	CAN滤波器初始化
************************************************************************
**/
void can_filter_init(FDCAN_HandleTypeDef *hfdcan)
{
	FDCAN_FilterTypeDef fdcan_filter;
	
	fdcan_filter.IdType = FDCAN_STANDARD_ID;                       //标准ID
	fdcan_filter.FilterIndex = 0;                                  //滤波器索引                   
	fdcan_filter.FilterType = FDCAN_FILTER_MASK;                   
	fdcan_filter.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;           //过滤器0关联到FIFO0  
	fdcan_filter.FilterID1 = 0x00;                               
	fdcan_filter.FilterID2 = 0x00;

	HAL_FDCAN_ConfigFilter(hfdcan,&fdcan_filter); 		 				  //接收ID2
	//拒绝接收匹配不成功的标准ID和扩展ID,不接受远程帧
	HAL_FDCAN_ConfigGlobalFilter(hfdcan,FDCAN_REJECT,FDCAN_REJECT,FDCAN_REJECT_REMOTE,FDCAN_REJECT_REMOTE);
	HAL_FDCAN_ConfigFifoWatermark(hfdcan, FDCAN_CFG_RX_FIFO0, 1);
//	HAL_FDCAN_ConfigFifoWatermark(&hfdcan1, FDCAN_CFG_RX_FIFO1, 1);
//	HAL_FDCAN_ActivateNotification(&hfdcan1, FDCAN_IT_TX_COMPLETE, FDCAN_TX_BUFFER0);
}


/**
 * @brief 初始化CAN总线
 *
 * @param hcan CAN编号
 * @param Callback_Function 处理回调函数
 */
void CAN_Init(FDCAN_HandleTypeDef *hcan, CAN_Call_Back Callback_Function)
{
    Struct_CAN_Recovery_State *recovery = CAN_Get_Recovery_State(hcan);
    if (recovery == NULL) { return; }
    recovery->Object->CAN_Handler = hcan;
    recovery->Object->Callback_Function = Callback_Function;
    recovery->Object->FD_Rx_Generation++;

    HAL_StatusTypeDef status = CAN_Configure_Reception(hcan);
    if (status == HAL_OK) { status = HAL_FDCAN_Start(hcan); }
    recovery->Diagnostics->Last_Recovery_Status = status;
    if (status != HAL_OK)
    {
        recovery->Diagnostics->Last_Error = hcan->ErrorCode;
        recovery->Diagnostics->Restart_Failures++;
        recovery->Diagnostics->Recovery_Pending = 1U;
        recovery->Reinit_Required = 1U;
    }
}

HAL_StatusTypeDef CAN_Register_FD_Callback(FDCAN_HandleTypeDef *hcan, CANFD_Call_Back Callback_Function)
{
    Struct_CAN_Recovery_State *recovery = CAN_Get_Recovery_State(hcan);
    if (recovery == NULL || recovery->Object->CAN_Handler != hcan || __get_IPSR() != 0U)
    {
        return HAL_ERROR;
    }
    /* 明确限制为启动注册，避免 Control 正在调用旧回调时更换其所有者。 */
    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) { return HAL_BUSY; }
    const uint32_t mask = CAN_Lock();
    recovery->Object->FD_Callback_Function = Callback_Function;
    recovery->Object->FD_Rx_Generation++;
    CAN_Unlock(mask);
    return HAL_OK;
}

void CANFD_Dispatch_Rx(uint8_t CAN_Instance, const FDCAN_RxHeaderTypeDef *Header,
                       const uint8_t *Data, uint32_t Generation)
{
    if (CAN_Instance < 1U || CAN_Instance > 3U) { return; }
    Struct_CAN_Recovery_State *recovery = &CAN_Recovery_States[CAN_Instance - 1U];
    if (__get_IPSR() != 0U || Header == NULL || Data == NULL || Header->FDFormat != FDCAN_FD_CAN ||
        Header->RxFrameType != FDCAN_DATA_FRAME || Header->DataLength > FDCAN_DLC_BYTES_64)
    {
        const uint32_t mask = CAN_Lock();
        recovery->IO->Rx_Invalid++;
        ulCAN_RxDropped++;
        CAN_Unlock(mask);
        return;
    }
    const uint32_t mask = CAN_Lock();
    FDCAN_HandleTypeDef *hcan = recovery->Object->CAN_Handler;
    CANFD_Call_Back callback = recovery->Object->FD_Callback_Function;
    const bool stale = (hcan == NULL || Generation != recovery->Object->FD_Rx_Generation ||
                        recovery->Diagnostics->Recovery_Pending != 0U ||
                        (hcan->Instance->PSR & FDCAN_PSR_BO) != 0U);
    if (stale) { recovery->IO->Rx_Stale++; }
    else if (callback == NULL) { recovery->IO->Rx_No_Callback++; }
    if (stale || callback == NULL) { ulCAN_RxDropped++; }
    CAN_Unlock(mask);
    if (stale || callback == NULL) { return; }

    Struct_CANFD_Rx_Buffer frame = {};
    frame.Header = *Header;
    frame.Length = CAN_DLC_TO_BYTES[Header->DataLength];
    memcpy(frame.Data, Data, frame.Length);
    /* 回调不持锁；允许从回调发送，但不得等待、延时或保留 frame 指针。 */
    callback(&frame);
}

/**
 * @brief 发送数据帧
 *
 * @param hcan CAN编号
 * @param ID ID
 * @param Data 被发送的数据指针
 * @param Length 长度
 * @return uint8_t 执行状态
 */
uint8_t CAN_Send_Data(FDCAN_HandleTypeDef *hcan, uint16_t ID, uint8_t *Data, uint16_t Length)
{
    Struct_CAN_Recovery_State *recovery = CAN_Get_Recovery_State(hcan);
    if (recovery == NULL) { return HAL_ERROR; }
    if (ID > 0x7FFU || Length > 64U || (Length != 0U && Data == NULL))
    {
        const uint32_t mask = CAN_Lock();
        recovery->IO->Tx_Invalid++;
        CAN_Unlock(mask);
        return HAL_ERROR;
    }
    uint32_t dlc = 0U;
    while (CAN_DLC_TO_BYTES[dlc] < Length) { dlc++; }
    /* HAL 按 4 字节读取源数组；即使 Length=1/2/3，也只读本地安全副本。 */
    uint8_t payload[64] = {};
    if (Length != 0U) { memcpy(payload, Data, Length); }
    FDCAN_TxHeaderTypeDef header = {};
    header.Identifier = ID;
    header.IdType = FDCAN_STANDARD_ID;
    header.TxFrameType = FDCAN_DATA_FRAME;
    header.DataLength = dlc;
    header.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
    header.BitRateSwitch = FDCAN_BRS_OFF;
    header.FDFormat = (Length <= 8U) ? FDCAN_CLASSIC_CAN : FDCAN_FD_CAN;
    header.TxEventFifoControl = FDCAN_NO_TX_EVENTS;

    /* 多任务/中断不能同时取得同一 FIFO PutIndex；准备数据在临界区之外。 */
    const uint32_t mask = CAN_Lock();
    HAL_StatusTypeDef status;
    if (recovery->Object->CAN_Handler != hcan ||
        hcan->Init.TxElmtSize < FDCAN_DATA_BYTES_8 ||
        hcan->Init.TxElmtSize > FDCAN_DATA_BYTES_64 ||
        CAN_DLC_TO_BYTES[dlc] > (hcan->Init.TxElmtSize - 2U) * 4U ||
        (Length > 8U && hcan->Init.FrameFormat == FDCAN_FRAME_CLASSIC))
    {
        recovery->IO->Tx_Invalid++;
        CAN_Unlock(mask);
        return HAL_ERROR;
    }
    if (recovery->Diagnostics->Recovery_Pending != 0U ||
        (hcan->Instance->PSR & FDCAN_PSR_BO) != 0U ||
        hcan->State != HAL_FDCAN_STATE_BUSY || HAL_FDCAN_GetTxFifoFreeLevel(hcan) == 0U)
    {
        status = HAL_BUSY;
    }
    else
    {
        status = HAL_FDCAN_AddMessageToTxFifoQ(hcan, &header, payload);
    }
    if (status == HAL_OK)
    {
        if (Length <= 8U) { recovery->IO->Tx_Classic_Accepted++; }
        else { recovery->IO->Tx_FD_Accepted++; }
    }
    else if (status == HAL_BUSY) { recovery->IO->Tx_Busy++; }
    else { recovery->IO->Tx_Errors++; }
    CAN_Unlock(mask);
    return (uint8_t)status;
}
uint32_t ECR_value = FDCAN1->ECR;
int test = 0;
/**
 * @brief CAN的TIM定时器中断发送回调函数
 *
 */
void TIM_CAN_PeriodElapsedCallback()
{
    
    #ifdef CHASSIS
    static uint8_t mod2 = 0,mod100 = 0,mod20 = 0;
    static uint8_t flag2 = 0;
    mod2++, mod100++,mod20++;
    if (mod2 == 2) // 250Hz
    {
        mod2 = 0;
        if (flag2 == 0) 
        {
            // 轮向 3508
            CAN_Send_Data(&hfdcan1, 0x200, CAN1_0x200_Tx_Data, 8);
            // 舵向 3508
            CAN_Send_Data(&hfdcan1, 0x1ff, CAN1_0x1ff_Tx_Data, 8);
        }
        else
        {
            // CAN_Send_Data(&hfdcan2, 0x68, CAN2_Chassis_Tx_Gimbal_Data, 8);
            // CAN_Send_Data(&hfdcan2, 0x89, CAN2_Chassis_Tx_Gimbal_Data_1, 8);
        }
        flag2++;
        flag2 = flag2 % 2;
    }

    if (mod100 == 10) //100Hz
    {
        CAN_Send_Data(&hfdcan2, 0x68, CAN2_Chassis_Tx_Gimbal_Data, 8);
        CAN_Send_Data(&hfdcan2, 0x89, CAN2_Chassis_Tx_Gimbal_Data_1, 8);		
//        CAN_Send_Data(&hfdcan3, 0x191, CAN3_Chassis_Tx_Data_G, 8);
        mod100 = 0;
    }
    if (mod20 == 20) //50Hz
    {
        //上板
        //CAN_Send_Data(&hfdcan2, 0x188, CAN3_Chassis_Tx_Data_A, 8);
//        CAN_Send_Data(&hfdcan3, 0x199, CAN3_Chassis_Tx_Data_B, 8);
//        CAN_Send_Data(&hfdcan3, 0x178, CAN3_Chassis_Tx_Data_C, 8);      
//        CAN_Send_Data(&hfdcan3, 0x197, CAN3_Chassis_Tx_Data_E, 8);
//        CAN_Send_Data(&hfdcan3, 0x198, CAN3_Chassis_Tx_Data_D, 8);
//        CAN_Send_Data(&hfdcan3, 0x196, CAN3_Chassis_Tx_Data_F, 8);
        //超电
        CAN_Send_Data(&hfdcan3, 0x66, CAN_Supercap_Tx_Data, 8);
        mod20 = 0;
    }
    #elif defined (GIMBAL)
        // CAN_Send_Data(&hfdcan2, 0x141, CAN2_0x141_Tx_Data, 8);
        //CAN_Send_Data(&hfdcan2, 0xf1, CAN2_0xxf1_Tx_Data, 8);

    static uint8_t mod5 = 0,mod2 = 0,mod10 = 0;
    mod5++;
    mod2++;
    mod10++;
    
    if(mod5 == 5)
    {
        mod5 = 0;
        CAN_Send_Data(&hfdcan2, 0x77, CAN2_Gimbal_Tx_Chassis_Data, 8); //给底盘发送控制命令 按照0x77 ID 发送
        CAN_Send_Data(&hfdcan2, 0x78, CAN2_Gimbal_Tx_Chassis_Data_1, 8);
        CAN_Send_Data(&hfdcan2, 0x79, CAN2_Gimbal_Tx_Chassis_Data_2, 8);
        
        // CAN_Send_Data(&hfdcan1, 0xf1, CAN1_0xxf1_Tx_Data, 8);
        CAN_Send_Data(&hfdcan1, 0x200, CAN1_0x200_Tx_Data, 8);
        CAN_Send_Data(&hfdcan2, 0x200, CAN2_0x200_Tx_Data, 8);
        
    }

    if(mod2 == 2)
    {
        CAN_Send_Data(&hfdcan2, 0xf1, CAN2_0xxf1_Tx_Data, 8);
        CAN_Send_Data(&hfdcan1, 0xa0, CAN1_MiniPc_Tx_Data, 8);
			//   CAN_Send_Data(&hfdcan2, 0xf1, CAN2_0xxf1_Tx_Data, 8);
        mod2 = 0;
    }
    CAN_Send_Data(&hfdcan2, 0x141, CAN2_0x141_Tx_Data, 8);  
    
    if (mod10 == 10) //100Hz
    {
        mod10 = 0;
    }

    if (Reset_Pitch_2)
    {
        CAN_Send_Data(&hfdcan1, 0xf2, CAN_DM4340_Reset, 8);
        Reset_Pitch_2 = false; // 重置标志位
    }
    #endif

}

/** @brief 有界排空本次 IRQ 已有消息：经典 CAN 原地回调，FD 完整副本入队。 */
static void CAN_Rx_EnqueueFromISR(FDCAN_HandleTypeDef *hcan, uint32_t fifo,
                                  uint32_t fifo_its, BaseType_t *higher_priority_woken)
{
    Struct_CAN_Recovery_State *recovery = CAN_Get_Recovery_State(hcan);
    if (recovery == NULL) { return; }
    const bool fifo1 = (fifo == FDCAN_RX_FIFO1);
    if ((fifo_its & (fifo1 ? FDCAN_IT_RX_FIFO1_FULL : FDCAN_IT_RX_FIFO0_FULL)) != 0U)
    {
        recovery->IO->Rx_FIFO_Full_Events++;
    }
    if ((fifo_its & (fifo1 ? FDCAN_IT_RX_FIFO1_MESSAGE_LOST : FDCAN_IT_RX_FIFO0_MESSAGE_LOST)) != 0U)
    {
        recovery->IO->Rx_FIFO_Lost_Events++;
        ulCAN_RxDropped++; // IRQ 标志会合并，计数表示事件次数而非精确丢失帧数
    }
    uint32_t remaining = HAL_FDCAN_GetRxFifoFillLevel(hcan, fifo);
    const uint32_t capacity = fifo1 ? hcan->Init.RxFifo1ElmtsNbr : hcan->Init.RxFifo0ElmtsNbr;
    if (remaining > capacity) { remaining = capacity; }
    while (remaining-- > 0U)
    {
        FreeRTOS_Control_RxMsg_t msg = {};
        FreeRTOS_CAN_RxMsg_t *frame = &msg.payload.can;
        msg.type = FREERTOS_CONTROL_INPUT_CAN;
        frame->can_instance = (uint8_t)(recovery - CAN_Recovery_States + 1U);
        frame->generation = recovery->Object->FD_Rx_Generation;
#if PERFORMANCE_TEST_ENABLE
        frame->rx_cycle = DWT->CYCCNT;
#endif
        /* HAL 根据 DLC 直接复制数据，目标必须从一开始就有 64 字节容量。 */
        if (HAL_FDCAN_GetRxMessage(hcan, fifo, &frame->header, frame->data) != HAL_OK)
        {
            recovery->IO->Rx_Read_Errors++;
            ulCAN_RxDropped++;
            break; // 避免在同一条无法读取的 FIFO 元素上忙等
        }
        const FDCAN_RxHeaderTypeDef *header = &frame->header;
        if (header->DataLength > FDCAN_DLC_BYTES_64 ||
            header->RxFrameType != FDCAN_DATA_FRAME ||
            (header->FDFormat != FDCAN_CLASSIC_CAN && header->FDFormat != FDCAN_FD_CAN) ||
            (header->FDFormat == FDCAN_CLASSIC_CAN && header->DataLength > FDCAN_DLC_BYTES_8))
        {
            recovery->IO->Rx_Invalid++;
            ulCAN_RxDropped++;
            continue;
        }
        if (header->FDFormat == FDCAN_CLASSIC_CAN)
        {
            recovery->IO->Rx_Classic++;
            Struct_CAN_Rx_Buffer *buffer = &recovery->Object->Rx_Buffer;
            buffer->Header = *header;
            memset(buffer->Data, 0, sizeof(buffer->Data));
            memcpy(buffer->Data, frame->data, CAN_DLC_TO_BYTES[header->DataLength]);
#if PERFORMANCE_TEST_ENABLE
            buffer->Rx_Cycle = frame->rx_cycle;
#endif
            if (recovery->Object->Callback_Function != NULL)
            {
                recovery->Object->Callback_Function(buffer);
            }
            else
            {
                recovery->IO->Rx_No_Callback++;
                ulCAN_RxDropped++;
            }
            continue;
        }
        recovery->IO->Rx_FD++;
        if (recovery->Diagnostics->Recovery_Pending != 0U || (hcan->Instance->PSR & FDCAN_PSR_BO) != 0U)
        {
            recovery->IO->Rx_Stale++;
            ulCAN_RxDropped++;
        }
        else if (recovery->Object->FD_Callback_Function == NULL)
        {
            recovery->IO->Rx_No_Callback++;
            ulCAN_RxDropped++;
        }
        else if (xQueue_Control_Rx == NULL || xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED)
        {
            recovery->IO->Rx_Queue_Not_Ready++;
            ulCAN_RxDropped++;
        }
        else if (xQueueSendFromISR(xQueue_Control_Rx, &msg, higher_priority_woken) != pdPASS)
        {
            recovery->IO->Rx_Queue_Full++;
            ulCAN_RxDropped++;
        }
    }
}

void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef *hcan, uint32_t RxFifo0ITs)
{
    BaseType_t higher_priority_woken = pdFALSE;
    CAN_Rx_EnqueueFromISR(hcan, FDCAN_RX_FIFO0, RxFifo0ITs, &higher_priority_woken);
    portYIELD_FROM_ISR(higher_priority_woken);
}

void HAL_FDCAN_RxFifo1Callback(FDCAN_HandleTypeDef *hcan, uint32_t RxFifo1ITs)
{
    BaseType_t higher_priority_woken = pdFALSE;
    CAN_Rx_EnqueueFromISR(hcan, FDCAN_RX_FIFO1, RxFifo1ITs, &higher_priority_woken);
    portYIELD_FROM_ISR(higher_priority_woken);
}

int ppp=0;
void HAL_FDCAN_TxBufferCompleteCallback(FDCAN_HandleTypeDef *hfdcan, uint32_t BufferIndexes)
{
  /* Prevent unused argument(s) compilation warning */
  UNUSED(hfdcan);
  UNUSED(BufferIndexes);
	ppp++;
  /* NOTE: This function Should not be modified, when the callback is needed,
           the HAL_FDCAN_TxBufferCompleteCallback could be implemented in the user file
   */
}

/**
 * @brief CAN错误状态回调函数
 *
 * @param hfdcan CAN句柄
 * @param ErrorStatusITs 错误状态中断掩码
 */
extern "C" void HAL_FDCAN_ErrorStatusCallback(FDCAN_HandleTypeDef *hfdcan, uint32_t ErrorStatusITs)
{
    Struct_CAN_Recovery_State *recovery = CAN_Get_Recovery_State(hfdcan);
    if (recovery == NULL || recovery->Object->CAN_Handler == NULL) { return; }
    recovery->Diagnostics->Last_Error_Status_ITs = ErrorStatusITs;
    /* BO 通知也会在退出 Bus-Off 时触发，该次通知不能再次请求重启。 */
    if ((ErrorStatusITs & FDCAN_IT_BUS_OFF) != 0U &&
        (hfdcan->Instance->PSR & FDCAN_PSR_BO) != 0U)
    {
        recovery->Diagnostics->Error_Events++;
        recovery->Diagnostics->BusOff_Events++;
        recovery->Diagnostics->Last_Error = hfdcan->ErrorCode;
        recovery->Diagnostics->Recovery_Pending = 1U;
    }
}

extern "C" void HAL_FDCAN_ErrorCallback(FDCAN_HandleTypeDef *hfdcan)
{
    Struct_CAN_Recovery_State *recovery = CAN_Get_Recovery_State(hfdcan);
    if (recovery == NULL || recovery->Object->CAN_Handler == NULL) { return; }
    /* FIFO 满/空和普通协议错误不重启，避免中断中的粘滞 ErrorCode 引发重启循环。 */
    if ((hfdcan->ErrorCode & CAN_RECOVERY_ERROR_MASK) != 0U)
    {
        recovery->Diagnostics->Error_Events++;
        recovery->Diagnostics->Last_Error = hfdcan->ErrorCode;
        recovery->Reinit_Required = 1U;
        recovery->Diagnostics->Recovery_Pending = 1U;
    }
}


/************************ COPYRIGHT(C) USTC-ROBOWALKER **************************/
