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

struct Struct_CAN_Recovery_State
{
    Struct_CAN_Manage_Object *Object;
    Struct_CAN_Recovery_Diagnostics *Diagnostics;
    volatile uint8_t Reinit_Required;
    uint8_t Retry_Waiting;
    uint32_t Last_Attempt_ms;
};

static Struct_CAN_Recovery_State CAN_Recovery_States[] =
{
    {&CAN1_Manage_Object, &CAN1_Recovery_Diagnostics, 0U, 0U, 0U},
    {&CAN2_Manage_Object, &CAN2_Recovery_Diagnostics, 0U, 0U, 0U},
    {&CAN3_Manage_Object, &CAN3_Recovery_Diagnostics, 0U, 0U, 0U}
};

static const uint32_t CAN_RECOVERY_RETRY_MS = 100U;
static const uint32_t CAN_RECOVERY_ERROR_MASK =
    HAL_FDCAN_ERROR_RAM_ACCESS | HAL_FDCAN_ERROR_RAM_WDG | HAL_FDCAN_ERROR_RESERVED_AREA;
static const uint32_t CAN_RECOVERY_NOTIFICATIONS =
    FDCAN_IT_BUS_OFF | FDCAN_IT_RAM_ACCESS_FAILURE |
    FDCAN_IT_RAM_WATCHDOG | FDCAN_IT_RESERVED_ADDRESS_ACCESS;

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
        (fifo1 ? FDCAN_IT_RX_FIFO1_NEW_MESSAGE : FDCAN_IT_RX_FIFO0_NEW_MESSAGE), 0U);
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
    if (recovery->Diagnostics->Recovery_Pending != 0U ||
        (hcan->Instance->PSR & FDCAN_PSR_BO) != 0U)
    {
        /* 恢复期间不积压过期控制目标；恢复完成后发送下一周期的新值。 */
        return HAL_BUSY;
    }
    FDCAN_TxHeaderTypeDef pTxHeader;
    pTxHeader.Identifier=ID;
    pTxHeader.IdType=FDCAN_STANDARD_ID;
    pTxHeader.TxFrameType=FDCAN_DATA_FRAME;
	
	if(Length<=8)
		pTxHeader.DataLength = Length;
	if(Length==12)
		pTxHeader.DataLength = FDCAN_DLC_BYTES_12;
	if(Length==16)
		pTxHeader.DataLength = FDCAN_DLC_BYTES_16;
	if(Length==20)
		pTxHeader.DataLength = FDCAN_DLC_BYTES_20;
	if(Length==24)
		pTxHeader.DataLength = FDCAN_DLC_BYTES_24;
	if(Length==32)
		pTxHeader.DataLength = FDCAN_DLC_BYTES_32;
	if(Length==48)
		pTxHeader.DataLength = FDCAN_DLC_BYTES_48;
	if(Length==64)
		pTxHeader.DataLength = FDCAN_DLC_BYTES_64;
	
    pTxHeader.ErrorStateIndicator=FDCAN_ESI_ACTIVE;
    pTxHeader.BitRateSwitch=FDCAN_BRS_OFF;
    pTxHeader.FDFormat=FDCAN_CLASSIC_CAN;
    pTxHeader.TxEventFifoControl=FDCAN_NO_TX_EVENTS;
    pTxHeader.MessageMarker=0;
    // if(HAL_FDCAN_GetTxFifoFreeLevel(hcan)!=0)
	return HAL_FDCAN_AddMessageToTxFifoQ(hcan, &pTxHeader, Data);	
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

/**
 * @brief 辅助函数：把 CAN 帧直接投递给控制任务
 *
 * @param hcan CAN句柄
 * @param fifo 接收 FIFO
 * @param can_instance CAN 实例编号 1/2/3
 * @param pxHigherPriorityTaskWoken 上下文切换标志
 */
static void CAN_Rx_EnqueueFromISR(FDCAN_HandleTypeDef *hcan, uint32_t fifo,
                                   uint8_t can_instance,
                                   BaseType_t *pxHigherPriorityTaskWoken)
{
    FreeRTOS_Control_RxMsg_t msg = {};
    FDCAN_RxHeaderTypeDef *header = &msg.payload.can.header;
    uint8_t rx_data[64];

    msg.type = FREERTOS_CONTROL_INPUT_CAN;
    msg.payload.can.can_instance = can_instance;
    /* ISR 仅读取原始计数；随帧入队，时间差换算留给控制任务。 */
#if PERFORMANCE_TEST_ENABLE
    msg.payload.can.rx_cycle = DWT->CYCCNT;
#endif

    if (HAL_FDCAN_GetRxMessage(hcan, fifo, header, rx_data) != HAL_OK ||
        header->DataLength > FDCAN_DLC_BYTES_8)
    {

        ulCAN_RxDropped++;
        return;
    }
    memcpy(msg.payload.can.data, rx_data, header->DataLength);

    /* 队列满或调度器未启动即丢帧计数，ISR 内绝不阻塞。 */
    if (xQueue_Control_Rx == NULL ||
        xQueueSendFromISR(xQueue_Control_Rx, &msg, pxHigherPriorityTaskWoken) != pdPASS)
    {
        ulCAN_RxDropped++;
        return;
    }
}

void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef *hcan, uint32_t RxFifo0ITs)
{
    //选择回调函数
    if (hcan->Instance == FDCAN1)
    {
        HAL_FDCAN_GetRxMessage(hcan, FDCAN_RX_FIFO0, &CAN1_Manage_Object.Rx_Buffer.Header, CAN1_Manage_Object.Rx_Buffer.Data);
        CAN1_Manage_Object.Callback_Function(&CAN1_Manage_Object.Rx_Buffer);
    }
    else if (hcan->Instance == FDCAN2)
    {
        HAL_FDCAN_GetRxMessage(hcan, FDCAN_RX_FIFO0, &CAN2_Manage_Object.Rx_Buffer.Header, CAN2_Manage_Object.Rx_Buffer.Data);
        CAN2_Manage_Object.Callback_Function(&CAN2_Manage_Object.Rx_Buffer);
    }
    else if (hcan->Instance == FDCAN3)
    {
        HAL_FDCAN_GetRxMessage(hcan, FDCAN_RX_FIFO0, &CAN3_Manage_Object.Rx_Buffer.Header, CAN3_Manage_Object.Rx_Buffer.Data);
        CAN3_Manage_Object.Callback_Function(&CAN3_Manage_Object.Rx_Buffer);
    }
}

void HAL_FDCAN_RxFifo1Callback(FDCAN_HandleTypeDef *hcan,uint32_t RxFifo1ITs)
{
    //选择回调函数
    if (hcan->Instance == FDCAN1)
    {
        HAL_FDCAN_GetRxMessage(hcan, FDCAN_RX_FIFO1, &CAN1_Manage_Object.Rx_Buffer.Header, CAN1_Manage_Object.Rx_Buffer.Data);
        CAN1_Manage_Object.Callback_Function(&CAN1_Manage_Object.Rx_Buffer);
    }
    else if (hcan->Instance == FDCAN2)
    {
        HAL_FDCAN_GetRxMessage(hcan, FDCAN_RX_FIFO1, &CAN2_Manage_Object.Rx_Buffer.Header, CAN2_Manage_Object.Rx_Buffer.Data);
        CAN2_Manage_Object.Callback_Function(&CAN2_Manage_Object.Rx_Buffer);
    }
    else if (hcan->Instance == FDCAN3)
    {
        HAL_FDCAN_GetRxMessage(hcan, FDCAN_RX_FIFO1, &CAN3_Manage_Object.Rx_Buffer.Header, CAN3_Manage_Object.Rx_Buffer.Data);
        CAN3_Manage_Object.Callback_Function(&CAN3_Manage_Object.Rx_Buffer);
    }
}

/**
 * @brief HAL库CAN接收FIFO0中断（FreeRTOS 版本）
 *
 * @param hcan CAN编号
 *
 * @note 仅读取硬件 FIFO 并直接投递控制队列，实际解析在控制任务。
 */
// void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef *hcan, uint32_t RxFifo0ITs)
// {
//     BaseType_t xHigherPriorityTaskWoken = pdFALSE;

//     UNUSED(RxFifo0ITs);

//     if (hcan->Instance == FDCAN1)
//     {
//         CAN_Rx_EnqueueFromISR(hcan, FDCAN_RX_FIFO0, 1, &xHigherPriorityTaskWoken);
//     }
//     else if (hcan->Instance == FDCAN2)
//     {
//         CAN_Rx_EnqueueFromISR(hcan, FDCAN_RX_FIFO0, 2, &xHigherPriorityTaskWoken);
//     }
//     else if (hcan->Instance == FDCAN3)
//     {
//         CAN_Rx_EnqueueFromISR(hcan, FDCAN_RX_FIFO0, 3, &xHigherPriorityTaskWoken);
//     }

//     portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
// }

/**
 * @brief HAL库CAN接收FIFO1中断（FreeRTOS 版本）
 *
 * @param hcan CAN编号
 */
// void HAL_FDCAN_RxFifo1Callback(FDCAN_HandleTypeDef *hcan, uint32_t RxFifo1ITs)
// {
//     BaseType_t xHigherPriorityTaskWoken = pdFALSE;

//     UNUSED(RxFifo1ITs);

//     if (hcan->Instance == FDCAN1)
//     {
//         CAN_Rx_EnqueueFromISR(hcan, FDCAN_RX_FIFO1, 1, &xHigherPriorityTaskWoken);
//     }
//     else if (hcan->Instance == FDCAN2)
//     {
//         CAN_Rx_EnqueueFromISR(hcan, FDCAN_RX_FIFO1, 2, &xHigherPriorityTaskWoken);
//     }
//     else if (hcan->Instance == FDCAN3)
//     {
//         CAN_Rx_EnqueueFromISR(hcan, FDCAN_RX_FIFO1, 3, &xHigherPriorityTaskWoken);
//     }

//     portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
// }

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
