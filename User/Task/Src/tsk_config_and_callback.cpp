/**
 * @file tsk_config_and_callback.cpp
 * @author cjw by yssickjgd
 * @brief 临时任务调度测试用函数, 后续用来存放个人定义的回调函数以及若干任务
 * @version 0.1
 * @date 2025-07-1 0.1 26赛季定稿
 * @copyright ZLLC 2026
 */

/**
 * @brief 注意, 每个类的对象分为专属对象Specialized, 同类可复用对象Reusable以及通用对象Generic
 *
 * 专属对象:
 * 单对单来独打独
 * 比如交互类的底盘对象, 只需要交互对象调用且全局只有一个, 这样看来, 底盘就是交互类的专属对象
 * 这种对象直接封装在上层类里面, 初始化在上层类里面, 调用在上层类里面
 *
 * 同类可复用对象:
 * 各调各的
 * 比如电机的对象, 底盘可以调用, 云台可以调用, 而两者调用的是不同的对象, 这种就是同类可复用对象
 * 电机的pid对象也算同类可复用对象, 它们都在底盘类里初始化
 * 这种对象直接封装在上层类里面, 初始化在最近的一个上层专属对象的类里面, 调用在上层类里面
 *
 * 通用对象:
 * 多个调用同一个
 * 比如裁判系统对象, 底盘类要调用它做功率控制, 发射机构要调用它做出膛速度与射击频率的控制, 因此裁判系统是通用对象.
 * 这种对象以指针形式进行指定, 初始化在包含所有调用它的上层的类里面, 调用在上层类里面
 *
 */

/**
 * @brief TIM开头的默认任务均1ms, 特殊任务需额外标记时间
 *
 */

/* Includes ------------------------------------------------------------------*/
#include "tsk_config_and_callback.h"
#include "freertos_tasks.h"
#include "drv_bsp-boarda.h"
#include "drv_tim.h"
#include "dvc_boardc_bmi088.h"
#include "dvc_dmmotor.h"
#include "ita_chariot.h"
#include "dvc_imu.h"
#include "usbd_cdc.h"
#include "usbd_cdc_if.h"
#include "config.h"
#include "dvc_GraphicsSendTask.h"
#include "buzzer.h"
#include "arm_math.h"
/* Private macros ------------------------------------------------------------*/

/* Private types -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/

uint32_t init_finished =0 ;
bool start_flag=0;
//机器人控制对象
Class_Chariot chariot;

/* 用 ISR 采集时间测接收频率，避免控制任务批量解析改变测频结果。 */
static float Task_RxFrequency(uint32_t *last_cycle)
{
    const uint32_t cycle = FreeRTOS_GetRxCycle();
    const uint32_t elapsed = cycle - *last_cycle;
    *last_cycle = cycle;
    return (elapsed != 0U) ? ((float)CPU_FREQ_Hz / (float)elapsed) : 0.0f;
}

/* Private function declarations ---------------------------------------------*/
/* Function prototypes -------------------------------------------------------*/

/**
 * @brief Chassis_CAN1回调函数
 *
 * @param CAN_RxMessage CAN1收到的消息
 */
 uint16_t can[3];

#ifdef CHASSIS
float Dtcha;
uint32_t last_cntcha = 0;
void Chassis_Device_CAN1_Callback(Struct_CAN_Rx_Buffer *CAN_RxMessage)
{
	can[0]++;
    switch (CAN_RxMessage->Header.Identifier)
    {
        case (0x201):
        {;
            chariot.Chassis.Motor_Wheel[0].CAN_RxCpltCallback(CAN_RxMessage->Data);
        }
        break;
        case (0x203):
        {
            chariot.Chassis.Motor_Wheel[1].CAN_RxCpltCallback(CAN_RxMessage->Data);
        }
        break;
        case (0x205):
        {
            chariot.Chassis.Motor_Wheel[2].CAN_RxCpltCallback(CAN_RxMessage->Data);
        }
        break;
        case (0x207):
        {
            chariot.Chassis.Motor_Wheel[3].CAN_RxCpltCallback(CAN_RxMessage->Data);
        }
        break;	
        case (0x202):
        {
            chariot.Chassis.Motor_Steer[0].CAN_RxCpltCallback(CAN_RxMessage->Data);
        }
        break;
        case (0x204):
        {
            chariot.Chassis.Motor_Steer[1].CAN_RxCpltCallback(CAN_RxMessage->Data);
        }
        break;					
        case (0x206):
        {
            chariot.Chassis.Motor_Steer[2].CAN_RxCpltCallback(CAN_RxMessage->Data);
        }
        break;
        case (0x208):
        {
            chariot.Chassis.Motor_Steer[3].CAN_RxCpltCallback(CAN_RxMessage->Data);
        }
        break;						
    }
}
#endif
/**
 * @brief Chassis_CAN2回调函数
 *
 * @param CAN_RxMessage CAN2收到的消息
 */
#ifdef CHASSIS
float DtYAW;
uint32_t last_cntyaw = 0;
float Dtgim;
uint32_t last_cntgim = 0;
void Chassis_Device_CAN2_Callback(Struct_CAN_Rx_Buffer *CAN_RxMessage)
{
	can[1]++;
    switch (CAN_RxMessage->Header.Identifier)
    {
        case (0x77): // 留给上板通讯
        {
            chariot.CAN_Chassis_Rx_Gimbal_Callback(CAN_RxMessage->Data);
        }
        break;
        case (0x78):
        {
            Dtgim = 1.0f/DWT_GetDeltaT(&last_cntgim);
            chariot.CAN_Chassis_Rx_Gimbal_Callback_1();
        }
        break;
        case (0x79):
        {
            chariot.CAN_Chassis_Rx_Gimbal_Callback_2();
        }
        break;						
        case(0x141)://给yaw进行通信
        { 
            if(CAN_RxMessage->Data[1] != 0)
            {
                chariot.Motor_Yaw.CAN_RxCpltCallback(CAN_RxMessage->Data);
            }
        }
        break;	 
    }
}
#endif
/* CAN3留给磁编*/ 
#ifdef CHASSIS
void Chassis_Device_CAN3_Callback(Struct_CAN_Rx_Buffer *CAN_RxMessage)
{
	can[2]++;
    switch (CAN_RxMessage->Header.Identifier)
    {
        case(0xD1):
        {
//					  Dt = 1.0f/DWT_GetDeltaT(&last_cnt);
            chariot.Chassis.Motor_Steer[0].MA600_Data_Process(CAN_RxMessage);
        }
        break;
        case(0xD2):
        {
            chariot.Chassis.Motor_Steer[1].MA600_Data_Process(CAN_RxMessage);
        }
        break;
        case(0xD3):
        {
            chariot.Chassis.Motor_Steer[2].MA600_Data_Process(CAN_RxMessage);
        }
        break;
        case(0xD4):
        {
            chariot.Chassis.Motor_Steer[3].MA600_Data_Process(CAN_RxMessage);
        }   
        break;     
        case (0x67)://超电接收
        {
            chariot.Chassis.Supercap.CAN_RxCpltCallback(CAN_RxMessage->Data);
        }
        break;
    }
}
#endif

#ifdef GIMBAL
/**
 * @brief Gimbal_CAN1回调函数.按照结构划分，在云台上部存在MiniPC、两个摩擦轮以及Pitch电机
 * @brief MiniPC直接绑定CAN1通道，不按照形参顺序走 
 * @param CAN_RxMessage CAN1收到的消息
 */
void Gimbal_Device_CAN1_Callback(Struct_CAN_Rx_Buffer *CAN_RxMessage)
{
    switch (CAN_RxMessage->Header.Identifier)
    {
        case(0x201):
        {
            chariot.Booster.Motor_Friction_Left.CAN_RxCpltCallback(CAN_RxMessage->Data);
        }
		break;
		case(0x202):
		{
			chariot.Booster.Motor_Friction_Right.CAN_RxCpltCallback(CAN_RxMessage->Data);
		}
		break;
        case(0x203):
		{
			chariot.Booster.Motor_Friction_Down.CAN_RxCpltCallback(CAN_RxMessage->Data);
		}
		break;
        case (0xa1):
        {
            chariot.MiniPC.CAN_RxCpltCallback(CAN_RxMessage->Data);
        }
        break;
        case(0xA2):
        {				
            chariot.Gimbal.Motor_Pitch_2.CAN_RxCpltCallback(CAN_RxMessage->Data);
		}
        break;
	}
}

/**
 * @brief Gimbal_CAN2回调函数
 * @brief 在中间，有yaw电机(给gimbal和chassis)和波弹盘电机
 * @param CAN_RxMessage CAN2收到的消息
 */
float Dts ;
uint32_t last_cnts = 0;
float DtYAW;
uint32_t last_cntyaw = 0;
void Gimbal_Device_CAN2_Callback(Struct_CAN_Rx_Buffer *CAN_RxMessage)
{
    switch (CAN_RxMessage->Header.Identifier)
    {
        case(0x141):
        {
            if(CAN_RxMessage->Data[1] != 0)
            {
			    DtYAW = Task_RxFrequency(&last_cntyaw);
            }
            chariot.Gimbal.Motor_Yaw.CAN_RxCpltCallback(CAN_RxMessage->Data);
        }
        break;
        case(0x68)://与下板进行通讯
        {
            chariot.CAN_Gimbal_Rx_Chassis_Callback();//利用can通信，让云台接收底盘的回调信息
        }
        break;
        case (0x89):
        {
            chariot.CAN_Gimbal_Rx_Chassis_Callback_1();
        }
        break;
        case(0xA1):
        {
			Dts = Task_RxFrequency(&last_cnts);
            // if(Dts<100.f)
            // {
                // chariot.Gimbal.Set_Gimbal_Control_Type(Gimbal_Control_Type_DISABLE);
            //     buzzer_setTask(&buzzer, BUZZER_DJI_STARTUP_PRIORITY);
            // }
            chariot.Gimbal.Motor_Pitch.CAN_RxCpltCallback(CAN_RxMessage->Data);
		}
        break;
        case(0x204):
        {
                chariot.Booster.Motor_Driver.CAN_RxCpltCallback(CAN_RxMessage->Data);
        }
        break;
//      break;
    }	
}


/**
 * @brief Gimbal_CAN3回调函数
 * @brief 底盘和云台的交互层，ita_chariot.h中直接绑定了CAN3
 * @param CAN_RxMessage CAN3收到的消息
 */
void Gimbal_Device_CAN3_Callback(Struct_CAN_Rx_Buffer *CAN_RxMessage){
    switch (CAN_RxMessage->Header.Identifier)
    {
        // case (0x11):
		// 		{
		// 				chariot.Gimbal.DM_IMU.IMU_UpdateData(CAN_RxMessage->Data);
		// 		}                          
	  }
}
#endif
/**
 * @brief SPI5回调函数
 *
 * @param Tx_Buffer SPI5发送的消息
 * @param Rx_Buffer SPI5接收的消息
 * @param Length 长度
 */
//void Device_SPI5_Callback(uint8_t *Tx_Buffer, uint8_t *Rx_Buffer, uint16_t Length)
//{
//    if (SPI5_Manage_Object.Now_GPIOx == BoardA_MPU6500_CS_GPIO_Port && SPI5_Manage_Object.Now_GPIO_Pin == BoardA_MPU6500_CS_Pin)
//    {
//        boarda_mpu.SPI_TxRxCpltCallback(Tx_Buffer, Rx_Buffer);
//    }
//}

/**
 * @brief SPI1回调函数
 *
 * @param Tx_Buffer SPI1发送的消息
 * @param Rx_Buffer SPI1接收的消息
 * @param Length 长度
 */
void Device_SPI2_Callback(uint8_t *Tx_Buffer, uint8_t *Rx_Buffer, uint16_t Length)
{

}


#ifdef GIMBAL
/**
 * @brief UART5遥控器回调函数
 *
 * @param Buffer UART9收到的消息
 * @param Length 长度
 */
void DR16_UART5_Callback(uint8_t *Buffer, uint16_t Length)
{
    if (!chariot.DR16.DR16_UART_RxCpltCallback(Buffer, Length)) { return; }
    //底盘 云台 发射机构 的控制策略
    chariot.TIM_Control_Callback();
}

/**
 * @brief USART1遥控器回调函数
 *
 * @param Buffer USART1收到的消息
 * @param Length 长度
 */
void VT13_UART_Callback(uint8_t *Buffer, uint16_t Length)
{
    if (Length < 21U) { return; }
    chariot.VT13.VT13_UART_RxCpltCallback(Buffer);

    //底盘 云台 发射机构 的控制策略
    if (*(Buffer + 0) == 0xA9 && *(Buffer + 1) == 0x53)
    {
        chariot.TIM_Control_Callback();
    }
}
#endif

#ifdef GIMBAL
void MiniPC_USB_Callback(uint8_t *Buffer, uint32_t Length)
{
    // chariot.MiniPC.USB_RxCpltCallback(Buffer);
}
#endif

/**
 * @brief IIC磁力计回调函数
 *
 * @param Buffer IIC收到的消息
 * @param Length 长度
 */
void Ist8310_IIC3_Callback(uint8_t* Tx_Buffer, uint8_t* Rx_Buffer, uint16_t Tx_Length, uint16_t Rx_Length)
{
    
}

/**
 * @brief UART裁判系统回调函数
 *
 * @param Buffer UART收到的消息
 * @param Length 长度
 */
#ifdef CHASSIS
float Dts ;
uint32_t last_cnts = 0;
void Referee_UART10_Callback(uint8_t *Buffer, uint16_t Length)
{   
    Dts = Task_RxFrequency(&last_cnts);
    chariot.Referee.UART_RxCpltCallback(Buffer,Length);
}
#endif
/**
 * @brief UART1超电回调函数
 *
 * @param Buffer UART1收到的消息
 * @param Length 长度
 */
#if defined CHASSIS
void SuperCAP_UART1_Callback(uint8_t *Buffer, uint16_t Length)
{
    //chariot.Chassis.Supercap.UART_RxCpltCallback(Buffer);
}
#endif

/**
 * @brief TIM4任务回调函数
 *
 */
float Dtm;
float Imu_Update_Frequency;
uint32_t last_cntm = 0;
uint32_t imu_delta = 0;
void Task100us_TIM4_Callback()
{
    Struct_IMU_Snapshot snapshot = {};
    #ifdef CHASSIS
    uint32_t cnt_time = DWT_GetTimeline_us();
    chariot.Boardc_BMI.TIM_Calculate_PeriodElapsedCallback();
    imu_delta = DWT_GetTimeline_us() - cnt_time;

    chariot.Boardc_BMI.Export_Snapshot(snapshot);
    #elif defined(GIMBAL)
    Imu_Update_Frequency = 1.0f/DWT_GetDeltaT(&last_cntm);
    chariot.Gimbal.Boardc_BMI.TIM_Calculate_PeriodElapsedCallback();
    chariot.Gimbal.Boardc_BMI.Export_Snapshot(snapshot);
    #endif
    const BaseType_t result = xQueueOverwrite(xQueue_IMU_Snapshot, &snapshot);
    configASSERT(result == pdPASS);
}

/* 控制周期开始时应用一份完整样本；本周期 getter 始终读取这份副本。 */
void Task_IMU_ApplyLatestSnapshot(void)
{
    Struct_IMU_Snapshot snapshot;
    if (xQueueReceive(xQueue_IMU_Snapshot, &snapshot, 0U) == pdTRUE)
    {
    #ifdef CHASSIS
        chariot.Boardc_BMI.Apply_Snapshot(snapshot);
    #elif defined(GIMBAL)
        chariot.Gimbal.Boardc_BMI.Apply_Snapshot(snapshot);
    #endif
    }
}



/**
 * @brief 1ms 控制回调：状态机 -> 2ms 控制解算 -> CAN 发送
 *
 */
void Task1ms_TIM5_Callback()    
{
    #ifdef GIMBAL
        UART5_Rx_Service();
    #endif

    /* 控制耗时与 IMU 周期使用独立计时变量，避免任务之间覆盖。 */
    static uint32_t control_start_cnt = 0;
    Dtm = DWT_GetDeltaT(&control_start_cnt);
    // DWT_GetDeltaT(&control_start_cnt);

    if (!start_flag)
    {
        init_finished++;
        start_flag = (init_finished > 2000U);
    }

    /* 在线检测和模式保护均由控制任务执行。 */
    chariot.TIM_Alive_Protect_PeriodElapsedCallback();

    if (start_flag)
    {
    #ifdef GIMBAL
        chariot.FSM_Alive_Control.Reload_TIM_Status_PeriodElapsedCallback();
        chariot.FSM_Alive_Control_VT13.Reload_TIM_Status_PeriodElapsedCallback();
    #endif

        static uint8_t mod2 = 0;
        mod2++;
        if (mod2 == 2U)
        {
            chariot.TIM_Calculate_PeriodElapsedCallback();
            mod2 = 0;
        }

        /* 驱动内部的 1/2/5/10/20ms 发送分频保持不变。 */
        TIM_CAN_PeriodElapsedCallback();
    }
}

uint8_t Task_Control_IsReady(void)
{
    return start_flag ? 1U : 0U;
}

/** @brief 每 50ms 执行一次在线检测，不再在入口内累加 50 次调用 */
void Task50ms_Alive_Callback(void)
{
    chariot.TIM50ms_Alive_PeriodElapsedCallback();
}

/** @brief 控制任务每 10ms 打包两帧，CAN 缓冲区只有控制任务写入。 */
void Task10ms_BoardComm_Callback(void)
{
    /* 保留字节必须清零，部分消息仅更新前 1/2 字节。 */
    uint8_t data[8] = {};
    uint8_t data_1[8] = {};

#ifdef GIMBAL
    chariot.CAN_Gimbal_Tx_Chassis_Callback(data);
    chariot.CAN_Gimbal_Tx_Chassis_Callback_1(data_1);
    memcpy(CAN2_Gimbal_Tx_Chassis_Data, data, sizeof(data));
    memcpy(CAN2_Gimbal_Tx_Chassis_Data_1, data_1, sizeof(data_1));
#elif defined(CHASSIS)
    chariot.CAN_Chassis_Tx_Gimbal_Callback(data);
    chariot.CAN_Chassis_Tx_Gimbal_Callback_1(data_1);
    memcpy(CAN2_Chassis_Tx_Gimbal_Data, data, sizeof(data));
    memcpy(CAN2_Chassis_Tx_Gimbal_Data_1, data_1, sizeof(data_1));
#endif
}

void Task_UI_PublishSnapshot(void)
{
#ifdef CHASSIS
    FreeRTOS_UI_Snapshot_t snapshot = {};
    snapshot.ready = Task_Control_IsReady();
    snapshot.refresh = (chariot.Referee_UI_Refresh_Status == Referee_UI_Refresh_Status_ENABLE);
    snapshot.robot_id = chariot.Referee.Get_ID();
    snapshot.chassis_control_type = chariot.Chassis.Get_Chassis_Control_Type();
    snapshot.gimbal_control_type = chariot.Gimbal_Tx_Control_Type;
    snapshot.fric_status = chariot.Fric_Status;
    snapshot.pitch_angle = chariot.Gimbal_Tx_Pitch_Angle;
    snapshot.supercap_voltage = chariot.Chassis.Supercap.Get_Supercap_Charge_Percentage();
    snapshot.chassis_gimbal_diff = chariot.Motor_Yaw.Get_Now_Angle();
    const BaseType_t result = xQueueOverwrite(xQueue_UI_Snapshot, &snapshot);
    configASSERT(result == pdPASS);
#endif
}

/**
 * @brief 初始化任务
 *
 */
extern "C" void Task_Init()
{  

    DWT_Init(480);

    /********************************** 驱动层初始化 **********************************/
	#ifdef CHASSIS

        //集中总线can1/can2
        CAN_Init(&hfdcan1, Chassis_Device_CAN1_Callback);
        CAN_Init(&hfdcan2, Chassis_Device_CAN2_Callback);
        CAN_Init(&hfdcan3, Chassis_Device_CAN3_Callback);

        //裁判系统
        UART_Init(&huart10, Referee_UART10_Callback, 128);//并未使用环形队列 尽量给长范围增加检索时间 减少丢包

        //c板陀螺仪spi外设
        SPI_Init(&hspi2,Device_SPI2_Callback);
	
        #ifdef POWER_LIMIT


        #endif

    #endif

    #ifdef GIMBAL

        //集中总线can1/can2
        CAN_Init(&hfdcan1, Gimbal_Device_CAN1_Callback);
        CAN_Init(&hfdcan2, Gimbal_Device_CAN2_Callback);
        CAN_Init(&hfdcan3, Gimbal_Device_CAN3_Callback);

        //c板陀螺仪spi外设
        SPI_Init(&hspi2,Device_SPI2_Callback);
        //磁力计iic外设
        //IIC_Init(&hi2c3, Ist8310_IIC3_Callback);    //达妙无磁力计
        //遥控器接收
        UART_Init(&huart5, DR16_UART5_Callback, 18);
        UART_Init(&huart1, VT13_UART_Callback, 60);

        USB_Init(&MiniPC_USB_Manage_Object, MiniPC_USB_Callback);

    #endif

    // 原 TIM4/TIM5 周期任务由 FreeRTOS 延时调度，不再注册硬件定时器回调

    /********************************* 设备层初始化 *********************************/

    //设备层集成在交互层初始化中，没有显视地初始化

    /********************************* 交互层初始化 *********************************/

    //__disable_irq();
    chariot.Init();
    //__enable_irq();

    /********************************* 使能调度时钟 *********************************/

    buzzer_setTask(&buzzer, BUZZER_DJI_STARTUP_PRIORITY);
    // TIM4/TIM5 不再启动，周期控制由 FreeRTOS 任务负责
}

/**
 * @brief 前台循环任务
 *
 */
 extern "C" void Task_Loop()
{
#ifdef GIMBAL
    // float now_angle_yaw = chariot.Gimbal.Motor_Yaw.Get_True_Angle_Yaw();
    // float target_angle_yaw = chariot.Gimbal.MiniPC->Get_Rx_Yaw_Angle();
//    // 如果是自瞄开启并且距离装甲板的瞄准弧度小于0.1m
//    if (chariot.Gimbal.Get_Gimbal_Control_Type() == Gimbal_Control_Type_MINIPC &&
//        (chariot.Gimbal.MiniPC->Get_Distance() * abs(now_angle_yaw - target_angle_yaw) / 180.0f * PI) < 0.1)
//    {
//        chariot.MiniPC_Aim_Status = MinPC_Aim_Status_ENABLE;
//    }
//    else
//    {
//        chariot.MiniPC_Aim_Status = MinPC_Aim_Status_DISABLE;
	//  }//不同车的逻辑
#endif
#ifdef CHASSIS
    /* 后台只使用控制任务交付的副本，不读取 chariot 或 start_flag。 */
    static uint8_t ui_ready = 0;
    FreeRTOS_UI_Snapshot_t snapshot;
    if (xQueueReceive(xQueue_UI_Snapshot, &snapshot, 0U) == pdTRUE)
    {
        ui_ready = snapshot.ready;
        JudgeReceiveData.robot_id = snapshot.robot_id;
        JudgeReceiveData.Chassis_Control_Type = snapshot.chassis_control_type;
        JudgeReceiveData.Gimbal_Control_Type = snapshot.gimbal_control_type;
        JudgeReceiveData.Fric_Status = snapshot.fric_status;
        JudgeReceiveData.Pitch_Angle = snapshot.pitch_angle;
        JudgeReceiveData.Supercap_Voltage = snapshot.supercap_voltage;
        JudgeReceiveData.Chassis_Gimbal_Diff = snapshot.chassis_gimbal_diff;
        if (snapshot.refresh != 0U) { Init_Cnt = 255; }
    }
    if (ui_ready == 0U) { return; }

    GraphicSendtask();

    static float freq;
    static uint32_t time_s;
    freq = 1 / DWT_GetDeltaT(&time_s);
#endif
}

/************************ COPYRIGHT(C) USTC-ROBOWALKER **************************/
