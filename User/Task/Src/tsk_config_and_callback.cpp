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
#include "drv_bsp-boarda.h"
#include "drv_tim.h"
#include "dvc_boardc_bmi088.h"
#include "dvc_dmmotor.h"
#include "ita_chariot.h"
#include "dvc_external_imu.h"
#include "dvc_GraphicsSendTask.h"
#include "dvc_imu.h"
#include "usbd_cdc.h"
#include "usbd_cdc_if.h"
#include "config.h"
#include "freertos_tasks.h"
/* Private macros ------------------------------------------------------------*/

/* Private types -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/

uint32_t init_finished =0 ;
bool start_flag=0;
//机器人控制对象
Class_Chariot chariot;

/* Private function declarations ---------------------------------------------*/
/* Function prototypes -------------------------------------------------------*/

/**
 * @brief Chassis_CAN1回调函数
 *
 * @param CAN_RxMessage CAN1收到的消息
 */
#ifdef CHASSIS
uint32_t steer1,steer2,steer3,steer4;
float Dt4= 0.0f, DT5,DT6,DT7;
void Chassis_Device_CAN1_Callback(Struct_CAN_Rx_Buffer *CAN_RxMessage)
{
    switch (CAN_RxMessage->Header.Identifier)
    {
        case (0x201):
        {
            chariot.Chassis.Motor_Wheel[0].CAN_RxCpltCallback(CAN_RxMessage->Data);
            break;
        }
        case (0x202):
        {
            chariot.Chassis.Motor_Wheel[1].CAN_RxCpltCallback(CAN_RxMessage->Data);
            break;
        }
        case (0x203):
        {
            chariot.Chassis.Motor_Wheel[2].CAN_RxCpltCallback(CAN_RxMessage->Data);
            break;
        }
        case (0x204):
        {
            chariot.Chassis.Motor_Wheel[3].CAN_RxCpltCallback(CAN_RxMessage->Data);
            break;
        }
        #ifdef AGV
        case (0x205):
        {
            chariot.Chassis.Motor_Steer[0].CAN_RxCpltCallback(CAN_RxMessage->Data);
            break;
        }
        case (0x206):
        {
            DT5 = DWT_GetDeltaT(&steer2);
            chariot.Chassis.Motor_Steer[1].CAN_RxCpltCallback(CAN_RxMessage->Data);
            break;
        }
        case (0x207):
        {
            chariot.Chassis.Motor_Steer[2].CAN_RxCpltCallback(CAN_RxMessage->Data);
            break;
        }
        case (0x208):
        {
            chariot.Chassis.Motor_Steer[3].CAN_RxCpltCallback(CAN_RxMessage->Data);
            break;
        }
        #endif
    }
}
#endif
/**
 * @brief Chassis_CAN2回调函数
 *
 * @param CAN_RxMessage CAN2收到的消息
 */
#ifdef CHASSIS
uint32_t add;
float Dt_add;
void Chassis_Device_CAN2_Callback(Struct_CAN_Rx_Buffer *CAN_RxMessage)
{
    switch (CAN_RxMessage->Header.Identifier)
    {   
        case (0x95):
        case (0x77):            //上板通讯
        {
            chariot.CAN_Chassis_Rx_Gimbal_Callback(CAN_RxMessage->Data);
            break;
        }
        case (0x67):            //超电接收
        {
            chariot.Chassis.Supercap.CAN_RxCpltCallback(CAN_RxMessage->Data);
            break;
        }
        case (0x55):
        {
            chariot.Chassis.Supercap.CAN_RxCpltCallback(CAN_RxMessage->Data);
            break;
        }
        case (0x141):
        {
            //Dt_add= DWT_GetDeltaT(&add);
            if(CAN_RxMessage->Data[1] != 0)
            {
            chariot.Motor_Main_Yaw.CAN_RxCpltCallback(CAN_RxMessage->Data);
            }
            break;
        }
        // case (0x205):
        // {
        //     Dt4 = DWT_GetDeltaT(&steer1);
        //     chariot.Chassis.Motor_Steer[0].CAN_RxCpltCallback(CAN_RxMessage->Data);
        //     break;
        // }
        // case (0x206):
        // {
        //     DT5 = DWT_GetDeltaT(&steer2);
        //     chariot.Chassis.Motor_Steer[1].CAN_RxCpltCallback(CAN_RxMessage->Data);
        //     break;
        // }
        // case (0x207):
        // {
        //     DT6 = DWT_GetDeltaT(&steer3);
        //     chariot.Chassis.Motor_Steer[2].CAN_RxCpltCallback(CAN_RxMessage->Data);
        //     break;
        // }
        // case (0x208):
        // {
        //     DT7 = DWT_GetDeltaT(&steer4);
        //     chariot.Chassis.Motor_Steer[3].CAN_RxCpltCallback(CAN_RxMessage->Data);
        //     break;
        // }
    }
}
#endif

#ifdef CHASSIS
uint32_t MA600_0,MA600_2,MA600_1,MA600_3;
float Dti= 0.0f, DT1,DT2,DT3;
// void Chassis_Device_CAN3_Callback(Struct_CAN_Rx_Buffer *CAN_RxMessage){
//     switch (CAN_RxMessage->Header.Identifier)
//     {
//         case(0xD1):
//         {
//             Dti = DWT_GetDeltaT(&MA600_0);
//             chariot.Chassis.Motor_Steer[0].MA600_Data_Process(CAN_RxMessage);
//             break;
//         }
//         case(0xD2):
//         {
//             DT1 = DWT_GetDeltaT(&MA600_1);
//             chariot.Chassis.Motor_Steer[1].MA600_Data_Process(CAN_RxMessage);
//             break;
//         }
//         case(0xD3):
//         {
//             DT2 = DWT_GetDeltaT(&MA600_2);
//             chariot.Chassis.Motor_Steer[2].MA600_Data_Process(CAN_RxMessage);
//             break;
//         }
//         case(0xD4):
//         {
//             DT3 = DWT_GetDeltaT(&MA600_3);
//             chariot.Chassis.Motor_Steer[3].MA600_Data_Process(CAN_RxMessage);
//             break;
//         }
//     }
// }
#endif
/**
 * @brief Gimbal_CAN1回调函数
 *
 * @param CAN_RxMessage CAN1收到的消息
 */
#ifdef GIMBAL
uint32_t CNTTT = 0, Y_CNTT;
float Dt = 0.0f, Y_Dt;
void Gimbal_Device_CAN1_Callback(Struct_CAN_Rx_Buffer *CAN_RxMessage)
{
    switch (CAN_RxMessage->Header.Identifier)
    {
        case(0x201):
        {
            chariot.Booster.Motor_Friction_Left.CAN_RxCpltCallback(CAN_RxMessage->Data);
            break;
        }
        case(0x202):
        {
            chariot.Booster.Motor_Friction_Right.CAN_RxCpltCallback(CAN_RxMessage->Data);
            break;
        }
        case(0x203):
        {
            chariot.Booster.Motor_Friction_Down.CAN_RxCpltCallback(CAN_RxMessage->Data);
            break;
        }
        case(0x205):
        {
            
            chariot.Gimbal.Motor_Yaw.CAN_RxCpltCallback(CAN_RxMessage->Data);
            break;
        }
        case(0x206):
        {
            chariot.Gimbal.Motor_Yaw.CAN_RxCpltCallback(CAN_RxMessage->Data);
            break;
        }
        // case(0x206):
        // {
        //     Y_Dt = DWT_GetDeltaT(&Y_CNTT);
        //     chariot.Gimbal.Motor_Pitch.CAN_RxCpltCallback(CAN_RxMessage->Data);
        //     break;
        // }
        case(0x11):
        case(0x01):
        {
            chariot.Gimbal.Motor_Roll.CAN_RxCpltCallback(CAN_RxMessage->Data);
            break;
        }  
        
        case(0x12):
        case(0x15):
        case(0x02):
        {
            Dt = DWT_GetDeltaT(&CNTTT);
            chariot.Gimbal.Motor_Pitch.CAN_RxCpltCallback(CAN_RxMessage->Data);
            break;
        }
	}
}
#endif

/**
 * @brief Gimbal_CAN2回调函数
 *
 * @param CAN_RxMessage CAN2收到的消息
 */
#ifdef GIMBAL
void Gimbal_Device_CAN2_Callback(Struct_CAN_Rx_Buffer *CAN_RxMessage)
{
    switch (CAN_RxMessage->Header.Identifier)
    {
        case(0x141):
        {
            chariot.Gimbal.Motor_Main_Yaw.CAN_RxCpltCallback(CAN_RxMessage->Data);
            break;
        }
        case(0x202):
        {
            chariot.Booster.Motor_Driver.CAN_RxCpltCallback(CAN_RxMessage->Data);
            break;
        }

        //上下板通信部分
        case(0x178):
        case(0x188):
        case(0x199):
        case(0x198):
        case(0x197):
        case(0x196):
        case(0x191):
        {
            chariot.CAN_Gimbal_Rx_Chassis_Callback();
            break;
        }

        case(0x78):
        {
            chariot.CAN_Gimbal_Rx_Chassis_Callback();
            break;
        }

    }
		
}
#endif

#ifdef GIMBAL
void Gimbal_Device_CAN3_Callback(Struct_CAN_Rx_Buffer *CAN_RxMessage){
    switch (CAN_RxMessage->Header.Identifier)
    {

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



/**
 * @brief UART5遥控器回调函数
 *
 * @param Buffer UART9收到的消息
 * @param Length 长度
 */
#ifdef GIMBAL
void DR16_UART5_Callback(uint8_t *Buffer, uint16_t Length)
{
    if(Length == 18){               //长度小于18的应该是错误的数据
        chariot.DR16.DR16_UART_RxCpltCallback(Buffer);
    }
}
#endif
/**
 * @brief UART9遥控器回调函数
 *
 * @param Buffer UART9收到的消息
 * @param Length 长度
 */
#ifdef GIMBAL
void VT13_UART_Callback(uint8_t *Buffer, uint16_t Length)
{
    chariot.VT13.VT13_UART_RxCpltCallback(Buffer);

    //底盘 云台 发射机构 的控制策略
    chariot.TIM_Control_Callback();
}
#endif
/**
 * @brief UART9遥控器回调函数
 *
 * @param Buffer UART9收到的消息
 * @param Length 长度
 */
uint32_t adc;
float Dt_adc;
#ifdef GIMBAL
void FS_i6X_UART5_Callback(uint8_t *Buffer, uint16_t Length)
{
        if(Length == 25){               //长度小于25的应该是错误的数据
            Dt_adc = DWT_GetDeltaT(&adc);
            chariot.FS_i6X.FS_UART_RxCpltCallback(Buffer, Length);
    }
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
void Referee_UART10_Callback(uint8_t *Buffer, uint16_t Length)
{
    chariot.Referee.UART_RxCpltCallback(Buffer,Length);
}

/**
 * @brief UART1超电回调函数
 *
 * @param Buffer UART1收到的消息
 * @param Length 长度
 */
#if defined CHASSIS
// 2027 的 Class_Supercap 走 CAN（dvc_supercap::CAN_RxCpltCallback），没有 UART_RxCpltCallback
// UART1 在 CHASSIS 模式下通常已借给裁判系统(HUART10)，这个回调留空
void SuperCAP_UART1_Callback(uint8_t *Buffer, uint16_t Length)
{
    (void)Buffer; (void)Length;
}
#endif

/**
 * @brief UART1陀螺仪回调函数
 *
 * @param Buffer UART1收到的消息
 * @param Length 长度
 */

#ifdef GIMBAL
void IMUB_USART7_Callback(uint8_t *Buffer, uint16_t Length)
{
    chariot.Gimbal.External_IMU.UART_BMI_Data_Process(Buffer);
    // chariot.Gimbal.External_IMU.UART_BMI_Data_Process(Buffer, Length);
}
#endif

/**
 * @brief USB MiniPC回调函数
 *
 * @param Buffer USB收到的消息
 *
 * @param Length 长度
 */
#ifdef GIMBAL
void MiniPC_USB_Callback(uint8_t *Buffer, uint32_t Length)
{
    chariot.MiniPC.USB_RxCpltCallback(Buffer, Length);
}
#endif
/**
 * @brief UART MiniPC回调函数
 *
 * @param Buffer UART收到的消息
 *
 * @param Length 长度
 */
#ifdef GIMBAL
void MiniPC_UART_Callback(uint8_t *Buffer, uint16_t Length)
{
    chariot.MiniPC.UART_RxCpltCallback(Buffer, Length);
}
#endif
/**
 * @brief TIM4任务回调函数
 *
 */
extern Referee_Rx_A_t CAN3_Chassis_Rx_Data_A;
float Sin_Single;
 uint32_t Single_Time;
uint32_t ad;
float Dt_ad;

void Task1ms_TIM4_Callback()
{
//   static uint16_t count = 0;
//   count++;
//   if(count>=90)
//   {
//      uint8_t current_mode=chariot.MiniPC.Get_mode();
//      if(current_mode==1) 
//      {
//       chariot.MiniPC.Set_mode(2);}
//       else if(current_mode==2)       {
//        chariot.MiniPC.Set_mode(1);
//       }
//       count = 0;
//   }
    #ifdef CHASSIS
    // IMU解算已迁移至Task100us_TIM4_Callback。
    #elif defined(GIMBAL)
        // 外置IMU及遥控状态机保留原1 ms业务路径；板载IMU由独立任务解算。

        //开始比赛但是遥控器断连了
         #ifdef USE_DR16  
        if(chariot.Referee.Get_Game_Stage() == Referee_Game_Status_Stage_BATTLE && chariot.DR16.Get_DR16_Status() == DR16_Status_DISABLE && chariot.Referee.Get_Referee_Status() == Referee_Status_ENABLE){                               //比赛开始状态
            chariot.DR16.Set_Left_Switch(DR16_Switch_Status_DOWN);                  //保险 强制上位机
            chariot.DR16.Set_Right_Switch(DR16_Switch_Status_DOWN);                 //强制上位机自动打弹
            chariot.TIM_Control_Callback();                                         //里面上位机离线的相关处理了
        }
        else{      
                                                                          //非比赛状态/遥控器在线
            chariot.FSM_Alive_Control.Reload_TIM_Status_PeriodElapsedCallback();    //遥控器离线失能保护
           
            
            if(chariot.DR16.Get_DR16_Status() == DR16_Status_ENABLE && 
                chariot.Gimbal.External_IMU.Get_IMU_Status() == IMU_Status_ENABLE){               //如果遥控器不在线也跑，就会和FSM抢状态，车子不能正确失能了
                chariot.TIM_Control_Callback();
            }
        }
        
        // chariot.FSM_Alive_Control.Reload_TIM_Status_PeriodElapsedCallback();    //遥控器离线失能保护

        // if(chariot.DR16.Get_DR16_Status() == DR16_Status_ENABLE && 
        //     chariot.Gimbal.External_IMU.Get_IMU_Status() == IMU_Status_ENABLE){               //如果遥控器不在线也跑，就会和FSM抢状态，车子不能正确失能了
        //     chariot.TIM_Control_Callback();
        // }
        #elif defined(USE_FS_i6X)
         if(chariot.Referee.Get_Game_Stage() == Referee_Game_Status_Stage_BATTLE
          && chariot.FS_i6X.Get_FS_Connection_Status() == FS_Connection_Status_OFFLINE 
          && chariot.Referee.Get_Referee_Status() == Referee_Status_ENABLE){                               //比赛开始状态
             chariot.FS_i6X.Set_Force_MiniPC(true);                           //保险及自动打弹均强制上位机
             chariot.TIM_Control_Callback();                                         //里面上位机离线的相关处理了
            // chariot.FSM_Alive_Control_Fs_i6x.Reload_TIM_Status_PeriodElapsedCallback();    //遥控器离线失能保护
            
            // if(chariot.FS_i6X.Get_FS_Status() == FS_Status_ENABLE && 
            //     chariot.Gimbal.External_IMU.Get_IMU_Status() == IMU_Status_ENABLE){               //如果遥控器不在线也跑，就会和FSM抢状态，车子不能正确失能了
            //     chariot.TIM_Control_Callback();
            // }
        }
        else{
            chariot.FS_i6X.Set_Force_MiniPC(false);
            chariot.FSM_Alive_Control_Fs_i6x.Reload_TIM_Status_PeriodElapsedCallback();    //遥控器离线失能保护
            
            if(chariot.FS_i6X.Get_FS_Connection_Status() == FS_Connection_Status_ONLINE && 
                chariot.Gimbal.External_IMU.Get_IMU_Status() == IMU_Status_ENABLE){               //如果遥控器不在线也跑，就会和FSM抢状态，车子不能正确失能了
                chariot.TIM_Control_Callback();
            }
        }
            #endif
        if (chariot.Gimbal.External_IMU.Get_IMU_Status() == IMU_Status_DISABLE)
        { // IMU失能保护
            chariot.Booster.Set_Booster_Control_Type(Booster_Control_Type_DISABLE);
            chariot.Gimbal.Set_Gimbal_Control_Type(Gimbal_Control_Type_DISABLE);
            chariot.Chassis.Set_Chassis_Control_Type(Chassis_Control_Type_DISABLE);
        }
        else
        {
            chariot.Gimbal.External_IMU.TIM_Calculate_PeriodElapsedCallback();
        }

        //生成正弦信号测试
         static uint16_t count = 0;
         count++;
         if(count==1){
        Single_Time ++;
        count =0;
    }
        Sin_Single = 2.0f * sinf(2.0f * PI * 5.0f * Single_Time / 1000.0f);
        //用于生成控制巡航的正弦信号
        if (chariot.Gimbal.Get_last_Cruise_Mode()==0){
            chariot.Gimbal.Single_time ++;
        }

#endif
}



/**
 * @brief TIM5任务回调函数
 *
 */
extern Referee_Rx_A_t CAN3_Chassis_Rx_Data_A;
uint8_t message[3] = "12";  // "123"
void Task100us_TIM4_Callback()
{
#ifdef CHASSIS
    chariot.Boardc_BMI.TIM_Calculate_PeriodElapsedCallback();
    if (xQueue_IMU_Snapshot != NULL)
    {
        Struct_IMU_Snapshot snapshot;
        chariot.Boardc_BMI.Export_Snapshot(snapshot);
        xQueueOverwrite(xQueue_IMU_Snapshot, &snapshot);
    }
#elif defined(GIMBAL)
    chariot.Gimbal.Boardc_BMI.TIM_Calculate_PeriodElapsedCallback();
    if (xQueue_IMU_Snapshot != NULL)
    {
        Struct_IMU_Snapshot snapshot;
        chariot.Gimbal.Boardc_BMI.Export_Snapshot(snapshot);
        xQueueOverwrite(xQueue_IMU_Snapshot, &snapshot);
    }
#endif
}

void Task_IMU_ApplyLatestSnapshot()
{
    if (xQueue_IMU_Snapshot == NULL)
    {
        return;
    }
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

uint8_t Task_Control_IsReady()
{
    return start_flag ? 1U : 0U;
}

void Task10ms_BoardComm_Callback()
{
#ifdef CHASSIS
    chariot.CAN_Chassis_Tx_Gimbal_Callback();
#elif defined(GIMBAL)
    chariot.CAN_Gimbal_Tx_Chassis_Callback();
#endif
}

void Task1ms_TIM5_Callback()
{
    static uint8_t mod2 = 0;
    static uint8_t mod100 = 0;
    static uint8_t mod68 = 0;

    // 2027 驱动的错误中断只置恢复请求，必须由任务在每个1ms周期落实重启；
    // 初始化等待期间也要恢复，否则一次串口溢出/CAN Bus-Off就会永久断链。
    UART_Error_Service();
    Referee_DMA_Service();
    CAN_Error_Service();

    if (init_finished <= 2000)
    {
        init_finished++;
        if (init_finished > 2000)
        {
            start_flag = true;
        }
        return;
    }

    // 遥控状态机在Alive任务执行，这里保留原500 Hz控制解算和1 kHz CAN发送节拍。
    mod2++;
    if (mod2 == 2)
    {
        chariot.TIM_Calculate_PeriodElapsedCallback();
        TIM_UART_PeriodElapsedCallback();
        mod2 = 0;
    }
    TIM_CAN_PeriodElapsedCallback();

#ifdef GIMBAL
    // 补发 DJI GM6020 小 Yaw 控制帧:
    // 2027 原版 TIM_CAN_PeriodElapsedCallback 漏掉了 CAN1 0x1fe (GM6020 专用帧)
    // 而 2026 原版每 1ms 都发 CAN1 0x1fe.
    // DJI 驱动把 motor_id 5/6 的输出填在 CAN1_0x1ff_Tx_Data[0-3],
    // 这里拷贝到 CAN1_0x1fe_Tx_Data 并发送, 让 GM6020 能收到控制帧.
    CAN1_0x1fe_Tx_Data[0] = CAN1_0x1ff_Tx_Data[0];
    CAN1_0x1fe_Tx_Data[1] = CAN1_0x1ff_Tx_Data[1];
    CAN1_0x1fe_Tx_Data[2] = CAN1_0x1ff_Tx_Data[2];
    CAN1_0x1fe_Tx_Data[3] = CAN1_0x1ff_Tx_Data[3];
    CAN_Send_Data(&hfdcan1, 0x1fe, CAN1_0x1fe_Tx_Data, 8);

    mod100++;
    if (mod100 == 100)
    {
        // 原0x0200哨兵到雷达位置包，由2027任务上下文发送接口限速并组帧。
        uint16_t radar_id = 0;
        if (chariot.Referee.Get_ID() == Referee_Data_Robots_ID_RED_SENTRY_7)
        {
            radar_id = Referee_Data_Robots_ID_RED_RADAR_9;
        }
        else if (chariot.Referee.Get_ID() == Referee_Data_Robots_ID_BLUE_SENTRY_7)
        {
            radar_id = Referee_Data_Robots_ID_BLUE_RADAR_9;
        }
        if (radar_id != 0)
        {
            const uint16_t position[2] = {0, 0};
            chariot.Referee.Send_Interaction(radar_id, 0x0200,
                reinterpret_cast<const uint8_t *>(position), sizeof(position));
        }
        mod100 = 0;
    }

    mod68++;
    if (mod68 == 68)
    {
        Struct_Referee_Tx_Data_Sentry_Command command = {};
        command.sentry_cmd = chariot.MiniPC.Get_Sentry_Cmd();
        chariot.Referee.Send_Sentry_Command(command);
        mod68 = 0;
    }
#endif
    // Pitch已在重力补偿后单次发送，避免原TIM5额外发送造成重复帧。
}

void Task_UI_PublishSnapshot()
{
    if (xQueue_UI_Snapshot == NULL)
    {
        return;
    }
    FreeRTOS_UI_Snapshot_t snapshot = {};
    snapshot.ready = Task_Control_IsReady();
    snapshot.robot_id = static_cast<uint8_t>(chariot.Referee.Get_ID());
    snapshot.chassis_control_type = static_cast<uint8_t>(chariot.Chassis.Get_Chassis_Control_Type());
    snapshot.supercap_voltage = chariot.Chassis.Supercap.Get_Supercap_Charge_Percentage();
#ifdef CHASSIS
    snapshot.refresh = static_cast<uint8_t>(chariot.Get_Gimbal_Status());
    snapshot.pitch_angle = chariot.Boardc_BMI.Get_Angle_Pitch();
    snapshot.chassis_gimbal_diff = chariot.Motor_Main_Yaw.Get_Now_Radian();
#elif defined(GIMBAL)
    snapshot.refresh = static_cast<uint8_t>(chariot.Referee_UI_Refresh_Status);
    snapshot.gimbal_control_type = static_cast<uint8_t>(chariot.Gimbal.Get_Gimbal_Control_Type());
    snapshot.fric_status = static_cast<uint8_t>(chariot.Fric_Status);
    snapshot.pitch_angle = chariot.Gimbal.External_IMU.Get_Angle_Pitch();
    snapshot.chassis_gimbal_diff = chariot.MiniPC.Get_Now_Relative_Angle();
#endif
    xQueueOverwrite(xQueue_UI_Snapshot, &snapshot);
}

void Task1ms_Alive_Callback(uint32_t elapsedMs, uint8_t runModuleScan)
{
    run_time = static_cast<uint16_t>(run_time + elapsedMs);
    // 保留原遥控源选择、离线保护状态机和外置IMU解算入口。
    Task1ms_TIM4_Callback();
    chariot.TIM_Unline_Protect_PeriodElapsedCallback();
    if (runModuleScan != 0U)
    {
        chariot.TIM1msMod50_Alive_PeriodElapsedCallback();
    }
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
        //CAN_Init(&hfdcan3, Chassis_Device_CAN3_Callback);

        SPI_Init(&hspi2,Device_SPI2_Callback);

        //裁判系统
        huart10.Init.BaudRate = 115200;
        HAL_UART_Init(&huart10);
        UART_Init(&huart10, Referee_UART10_Callback, 128);//并未使用环形队列 尽量给长范围增加检索时间 减少丢包

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
        #ifdef USE_DR16
        UART_Init(&huart5, DR16_UART5_Callback, 18);
        #elif defined(USE_VT13)
        UART_Init(&huart9, VT13_UART_Callback, 30);
        #elif defined(USE_FS_i6X)
        UART_Init(&huart5, FS_i6X_UART5_Callback, 25);      
        #endif
        //上位机USB
        USB_Init(&MiniPC_USB_Manage_Object,MiniPC_USB_Callback);
        //上位机串口
        UART_Init(&huart8, MiniPC_UART_Callback, 56);

        UART_Init(&huart7, Referee_UART10_Callback, 128);

        //外置IMU串口   接收长度14，但是串口空闲中断刚好接满14的时候不会进入空闲事件中断，只进入接收满中断
        //2027的CubeMX把USART10默认配成115200，而外置IMU是1kHz x 14字节(约140kbps)，
        //115200下一帧要1.2ms才收完，会持续ORE；这里按2026恢复为230400。
        huart10.Init.BaudRate = 230400;
        HAL_UART_Init(&huart10);
        UART_Init(&huart10, IMUB_USART7_Callback, 14 * 2);     

    #endif

    // 业务周期由FreeRTOS任务调度，不再启动旧TIM4/TIM5裸机回调。

    /********************************* 设备层初始化 *********************************/

    //设备层集成在交互层初始化中，没有显视地初始化

    /********************************* 交互层初始化 *********************************/

    chariot.Init(0.0);

    // FreeRTOS_Start()由main在初始化完成后调用。
}

/**
 * @brief 前台循环任务
 *
 */
 extern "C" void Task_Loop()
{
    #ifdef GIMBAL

    #endif
    #ifdef CHASSIS

    #endif
}

/************************ COPYRIGHT(C) USTC-ROBOWALKER **************************/
