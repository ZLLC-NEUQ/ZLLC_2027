/**
 * @file dvc_dmmotor.cpp
 * @author HGCSJ by yssickjgd
 * @brief 达妙电机配置与操作
 * @version V1.1
 * @date 2026-10-30 27赛季定稿
 *
 * @copyright ZLLC 2027
 *
 */

/* Includes ------------------------------------------------------------------*/

#include "dvc_dmmotor.h"

/* Private macros ------------------------------------------------------------*/

/* Private types -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/

// 清除电机错误信息
uint8_t DM_Motor_CAN_Message_Clear_Error[8] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfb};
// 使能电机
uint8_t DM_Motor_CAN_Message_Enter[8] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfc};
// 失能电机
uint8_t DM_Motor_CAN_Message_Exit[8] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfd};
// 保存当前电机位置为零点
uint8_t DM_Motor_CAN_Message_Save_Zero[8] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe};

/* Private function declarations ---------------------------------------------*/

/* Function prototypes -------------------------------------------------------*/

/**
 * @brief 分配CAN发送缓冲区
 *
 * @param hcan CAN编号
 * @param __CAN_ID CAN ID
 * @return uint8_t* 缓冲区指针
 */
uint8_t *allocate_tx_data(FDCAN_HandleTypeDef *hcan, Enum_DM_Motor_ID __CAN_ID)
{
    uint8_t *tmp_tx_data_ptr = nullptr;
    if (hcan == &hfdcan1) {
        switch (__CAN_ID) {
            case (DM_Motor_ID_0xA1):
            case (DM_Motor_ID_0x01): {
                tmp_tx_data_ptr = CAN1_0xxf1_Tx_Data;
            } break;
            case (DM_Motor_ID_0xA2):
            case (DM_Motor_ID_0x02): {
                tmp_tx_data_ptr = CAN1_0xxf2_Tx_Data;
            } break;
            case (DM_Motor_ID_0xA3):
            case (DM_Motor_ID_0x03): {
                tmp_tx_data_ptr = CAN1_0xxf3_Tx_Data;
            } break;
            case (DM_Motor_ID_0xA4):
            case (DM_Motor_ID_0x04): {
                tmp_tx_data_ptr = CAN1_0xxf4_Tx_Data;
            } break;
            case (DM_Motor_ID_0xA5):
            case (DM_Motor_ID_0x05): {
                tmp_tx_data_ptr = CAN1_0xxf5_Tx_Data;
            } break;
            case (DM_Motor_ID_0xA6):
            case (DM_Motor_ID_0x06): {
                tmp_tx_data_ptr = CAN1_0xxf6_Tx_Data;
            } break;
            case (DM_Motor_ID_0xA7):
            case (DM_Motor_ID_0x07): {
                tmp_tx_data_ptr = CAN1_0xxf7_Tx_Data;
            } break;
            case (DM_Motor_ID_0xA8):
            case (DM_Motor_ID_0x08): {
                tmp_tx_data_ptr = CAN1_0xxf8_Tx_Data;
            } break;
        }
    } else if (hcan == &hfdcan2) {
        switch (__CAN_ID) {
            case (DM_Motor_ID_0xA1):
            case (DM_Motor_ID_0x01): {
                tmp_tx_data_ptr = CAN2_0xxf1_Tx_Data;
            } break;
            case (DM_Motor_ID_0xA2):
            case (DM_Motor_ID_0x02): {
                tmp_tx_data_ptr = CAN2_0xxf2_Tx_Data;
            } break;
            case (DM_Motor_ID_0xA3):
            case (DM_Motor_ID_0x03): {
                tmp_tx_data_ptr = CAN2_0xxf3_Tx_Data;
            } break;
            case (DM_Motor_ID_0xA4):
            case (DM_Motor_ID_0x04): {
                tmp_tx_data_ptr = CAN2_0xxf4_Tx_Data;
            } break;
            case (DM_Motor_ID_0xA5):
            case (DM_Motor_ID_0x05): {
                tmp_tx_data_ptr = CAN2_0xxf5_Tx_Data;
            } break;
            case (DM_Motor_ID_0xA6):
            case (DM_Motor_ID_0x06): {
                tmp_tx_data_ptr = CAN2_0xxf6_Tx_Data;
            } break;
            case (DM_Motor_ID_0xA7):
            case (DM_Motor_ID_0x07): {
                tmp_tx_data_ptr = CAN2_0xxf7_Tx_Data;
            } break;
            case (DM_Motor_ID_0xA8):
            case (DM_Motor_ID_0x08): {
                tmp_tx_data_ptr = CAN2_0xxf8_Tx_Data;
            } break;
        }
    } else if (hcan == &hfdcan3) {
        switch (__CAN_ID) {
            case (DM_Motor_ID_0xA1):
            case (DM_Motor_ID_0x01):
                tmp_tx_data_ptr = CAN3_0xxf1_Tx_Data;
                break;
            case (DM_Motor_ID_0xA2):
            case (DM_Motor_ID_0x02):
                tmp_tx_data_ptr = CAN3_0xxf2_Tx_Data;
                break;
            case (DM_Motor_ID_0xA3):
            case (DM_Motor_ID_0x03):
                tmp_tx_data_ptr = CAN3_0xxf3_Tx_Data;
                break;
            case (DM_Motor_ID_0xA4):
            case (DM_Motor_ID_0x04):
                tmp_tx_data_ptr = CAN3_0xxf4_Tx_Data;
                break;
            case (DM_Motor_ID_0xA5):
            case (DM_Motor_ID_0x05):
                tmp_tx_data_ptr = CAN3_0xxf5_Tx_Data;
                break;
            case (DM_Motor_ID_0xA6):
            case (DM_Motor_ID_0x06):
                tmp_tx_data_ptr = CAN3_0xxf6_Tx_Data;
                break;
            case (DM_Motor_ID_0xA7):
            case (DM_Motor_ID_0x07):
                tmp_tx_data_ptr = CAN3_0xxf7_Tx_Data;
                break;
            case (DM_Motor_ID_0xA8):
            case (DM_Motor_ID_0x08):
                tmp_tx_data_ptr = CAN3_0xxf8_Tx_Data;
                break;
        }
    }
    return (tmp_tx_data_ptr);
}

Enum_DM_Motor_Status Class_DM_Motor::Get_DM_Motor_Status()
{
    return (DM_Motor_Status);
}

Enum_DM_Motor_Control_Status Class_DM_Motor::Get_DM_Motor_Control_Status()
{
    return (DM_Motor_Control_Status);
}

Enum_DM_Motor_ErrorCode Class_DM_Motor::Get_ErrorCode()
{
    return (Data.ErrorCode);
}

float Class_DM_Motor::Get_Now_Angle()
{
    return (Data.Now_Angle);
}

float Class_DM_Motor::Get_Now_Omega()
{
    return (Data.Now_Omega);
}

float Class_DM_Motor::Get_Now_Torque()
{
    return (Data.Now_Torque);
}

float Class_DM_Motor::Get_Now_MOS_Temperature()
{
    return (Data.Now_MOS_Temperature);
}

float Class_DM_Motor::Get_Now_Rotor_Temperature()
{
    return (Data.Now_Rotor_Temperature);
}

Enum_DM_Motor_Control_Method Class_DM_Motor::Get_Control_Method()
{
    return (DM_Motor_Control_Method);
}

float Class_DM_Motor::Get_MIT_K_P()
{
    return (MIT_K_P);
}

float Class_DM_Motor::Get_MIT_K_D()
{
    return (MIT_K_D);
}

float Class_DM_Motor::Get_Target_Angle()
{
    return (Target_Angle);
}

float Class_DM_Motor::Get_Target_Omega()
{
    return (Target_Omega);
}

float Class_DM_Motor::Get_Target_Torque()
{
    return (Target_Torque);
}

void Class_DM_Motor::Set_DM_Control_Status(Enum_DM_Motor_Control_Status __Control_Status)
{
    DM_Motor_Control_Status = __Control_Status;
}

void Class_DM_Motor::Set_DM_Motor_Control_Method(Enum_DM_Motor_Control_Method __Control_Method)
{
    DM_Motor_Control_Method = __Control_Method;
}

void Class_DM_Motor::Set_MIT_K_P(float __MIT_K_P)
{
    MIT_K_P = __MIT_K_P;
}

void Class_DM_Motor::Set_MIT_K_D(float __MIT_K_D)
{
    MIT_K_D = __MIT_K_D;
}

void Class_DM_Motor::Set_Target_Angle(float __Target_Angle)
{
    Target_Angle = __Target_Angle;
}

void Class_DM_Motor::Set_Target_Omega(float __Target_Omega)
{
    Target_Omega = __Target_Omega;
}

void Class_DM_Motor::Set_Target_Torque(float __Target_Torque)
{
    Target_Torque = __Target_Torque;
}

/**
 * @brief 初始化达妙电机的CAN绑定、控制方式及协议量程
 */
void Class_DM_Motor::Init(FDCAN_HandleTypeDef *hcan, Enum_DM_Motor_ID __CAN_ID, Enum_DM_Motor_Control_Method __Control_Method, float __Omega_Max, float __Torque_Max, float __Angle_Max)
{
    if (hcan->Instance == FDCAN1) {
        CAN_Manage_Object = &CAN1_Manage_Object;
    } else if (hcan->Instance == FDCAN2) {
        CAN_Manage_Object = &CAN2_Manage_Object;
    } else if (hcan->Instance == FDCAN3) {
        CAN_Manage_Object = &CAN3_Manage_Object;
    }
    CAN_ID                  = __CAN_ID;
    DM_Motor_Control_Method = __Control_Method;
    Omega_Max               = __Omega_Max;
    Torque_Max              = __Torque_Max;
    Angle_Max               = __Angle_Max;
    CAN_Tx_Data             = allocate_tx_data(hcan, __CAN_ID);
    Control_Tx_Length       = 0;
}

/**
 * @brief 解码达妙电机反馈数据
 */
void Class_DM_Motor::Data_Process(uint8_t *Rx_Data)
{
    uint16_t tmp_position = (Rx_Data[1] << 8) | Rx_Data[2];
    uint16_t tmp_omega    = (Rx_Data[3] << 4) | (Rx_Data[4] >> 4);
    uint16_t tmp_torque   = ((Rx_Data[4] & 0x0f) << 8) | Rx_Data[5];

    Data.CAN_ID                = Rx_Data[0] & 0x0f;
    Data.ErrorCode             = static_cast<Enum_DM_Motor_ErrorCode>(Rx_Data[0] >> 4);
    Data.Now_Angle             = Math_Int_To_Float(tmp_position, 0, (1 << 16) - 1, -Angle_Max, Angle_Max);
    Data.Now_Omega             = Math_Int_To_Float(tmp_omega, 0, (1 << 12) - 1, -Omega_Max, Omega_Max);
    Data.Now_Torque            = Math_Int_To_Float(tmp_torque, 0, (1 << 12) - 1, -Torque_Max, Torque_Max);
    Data.Now_MOS_Temperature   = Rx_Data[6];
    Data.Now_Rotor_Temperature = Rx_Data[7];
}

/**
 * @brief CAN通信接收回调函数
 */
void Class_DM_Motor::CAN_RxCpltCallback(uint8_t *Rx_Data)
{
    if (Rx_Data == nullptr || CAN_Tx_Data == nullptr) {
        return;
    }

    Data_Process(Rx_Data);
    Flag++;
}

/**
 * @brief 更新在线状态, 按软件控制请求发送使能或失能帧
 */
void Class_DM_Motor::TIM_Alive_PeriodElapsedCallback()
{
    uint32_t current_flag = Flag;
    DM_Motor_Status       = (current_flag == Pre_Flag) ? DM_Motor_Status_DISABLE : DM_Motor_Status_ENABLE;
    Pre_Flag              = current_flag;

    // 沿用现有电机ID槽位和控制帧偏移
    uint16_t tx_id = CAN_ID;
    switch (DM_Motor_Control_Method) {
        case DM_Motor_Control_Method_MIT_POSITION:
        case DM_Motor_Control_Method_MIT_OMEGA:
        case DM_Motor_Control_Method_MIT_ANGLE:
        case DM_Motor_Control_Method_MIT_IMU_Angle:
        case DM_Motor_Control_Method_MIT_OPENLOOP:
            break;
        case DM_Motor_Control_Method_POSITION_OMEGA:
            tx_id += 0x100;
            break;
        case DM_Motor_Control_Method_OMEGA:
            tx_id += 0x200;
            break;
        default:
            return;
    }

    uint8_t *tx_data = (DM_Motor_Control_Status == DM_Motor_Control_Status_ENABLE) ? DM_Motor_CAN_Message_Enter : DM_Motor_CAN_Message_Exit;
    CAN_Send_Data(CAN_Manage_Object->CAN_Handler, tx_id, tx_data, 8);
}

/**
 * @brief 计算默认MIT闭环, 再由具体型号发送控制帧
 */
void Class_DM_Motor::TIM_PID_PeriodElapsedCallback()
{
    switch (DM_Motor_Control_Method) {
        case (DM_Motor_Control_Method_MIT_POSITION):
        case (DM_Motor_Control_Method_POSITION_OMEGA):
        case (DM_Motor_Control_Method_OMEGA):
            break;

        case (DM_Motor_Control_Method_MIT_OMEGA): {
            // 速度环
            PID_Omega.Set_Target(Target_Omega);
            PID_Omega.Set_Now(Data.Now_Omega);

            PID_Omega.TIM_Adjust_PeriodElapsedCallback();

            Target_Torque = PID_Omega.Get_Out();
        } break;

        case (DM_Motor_Control_Method_MIT_ANGLE): {
            PID_Angle.Set_Target(Target_Angle);
            // 角度环
            PID_Angle.Set_Now(Data.Now_Angle);
            PID_Angle.TIM_Adjust_PeriodElapsedCallback();

            Target_Omega = PID_Angle.Get_Out();

            // 速度环
            PID_Omega.Set_Target(Target_Omega);
            PID_Omega.Set_Now(Data.Now_Omega);

            PID_Omega.TIM_Adjust_PeriodElapsedCallback();

            Target_Torque = PID_Omega.Get_Out();
        } break;

        default: {
            Set_DM_Control_Status(DM_Motor_Control_Status_DISABLE);
        } break;
    }

    TIM_Process_PeriodElapsedCallback();
}

/**
 * @brief TIM定时器中断发送出去的回调函数
 *
 */
void Class_DM_Motor::TIM_Process_PeriodElapsedCallback()
{

    Pack_Control_Frame(Target_Angle, Target_Omega, Target_Torque, MIT_K_P, MIT_K_D);
    Send_Control_Frame();
}

/**
 * @brief 生成控制帧并记录对应的CAN ID和长度
 */
void Class_DM_Motor::Pack_Control_Frame(float angle, float omega, float torque, float kp, float kd)
{
    if (CAN_Tx_Data == nullptr) {
        return;
    }

    Control_Tx_Length = 0;
    switch (DM_Motor_Control_Method) {
        case (DM_Motor_Control_Method_MIT_POSITION):
        case (DM_Motor_Control_Method_MIT_OMEGA):
        case (DM_Motor_Control_Method_MIT_ANGLE): {
            Math_Constrain(&angle, -Angle_Max, Angle_Max);
            Math_Constrain(&omega, -Omega_Max, Omega_Max);
            Math_Constrain(&torque, -Torque_Max, Torque_Max);
            Math_Constrain(&kp, 0.0f, 500.0f);
            Math_Constrain(&kd, 0.0f, 5.0f);

            uint16_t tmp_position = Math_Float_To_Int(angle, -Angle_Max, Angle_Max, 0, (1 << 16) - 1);
            uint16_t tmp_velocity = Math_Float_To_Int(omega, -Omega_Max, Omega_Max, 0, (1 << 12) - 1);
            uint16_t tmp_k_p      = 0;
            uint16_t tmp_k_d      = 0;
            if (DM_Motor_Control_Method == DM_Motor_Control_Method_MIT_POSITION) {
                tmp_k_p = Math_Float_To_Int(kp, 0.0f, 500.0f, 0, (1 << 12) - 1);
            }
            if (DM_Motor_Control_Method != DM_Motor_Control_Method_MIT_ANGLE) {
                tmp_k_d = Math_Float_To_Int(kd, 0.0f, 5.0f, 0, (1 << 12) - 1);
            }
            uint16_t tmp_torque = Math_Float_To_Int(torque, -Torque_Max, Torque_Max, 0, (1 << 12) - 1);

            Math_Endian_Reverse_16(&tmp_position);
            memcpy(&CAN_Tx_Data[0], &tmp_position, sizeof(uint16_t));

            uint8_t tmp_velocity_11_4 = tmp_velocity >> 4;
            memcpy(&CAN_Tx_Data[2], &tmp_velocity_11_4, sizeof(uint8_t));

            uint8_t tmp_velocity_3_0_k_p_11_8 = ((tmp_velocity & 0x0f) << 4) | (tmp_k_p >> 8);
            memcpy(&CAN_Tx_Data[3], &tmp_velocity_3_0_k_p_11_8, sizeof(uint8_t));

            uint8_t tmp_k_p_7_0 = tmp_k_p;
            memcpy(&CAN_Tx_Data[4], &tmp_k_p_7_0, sizeof(uint8_t));

            uint8_t tmp_k_d_11_4 = tmp_k_d >> 4;
            memcpy(&CAN_Tx_Data[5], &tmp_k_d_11_4, sizeof(uint8_t));

            uint8_t tmp_k_d_3_0_torque_11_8 = ((tmp_k_d & 0x0f) << 4) | (tmp_torque >> 8);
            memcpy(&CAN_Tx_Data[6], &tmp_k_d_3_0_torque_11_8, sizeof(uint8_t));

            uint8_t tmp_torque_7_0 = tmp_torque;
            memcpy(&CAN_Tx_Data[7], &tmp_torque_7_0, sizeof(uint8_t));

            Control_Tx_ID     = CAN_ID;
            Control_Tx_Length = 8;
        } break;
        case (DM_Motor_Control_Method_POSITION_OMEGA): {
            memcpy(&CAN_Tx_Data[0], &angle, sizeof(float));

            memcpy(&CAN_Tx_Data[4], &omega, sizeof(float));

            Control_Tx_ID     = CAN_ID + 0x100;
            Control_Tx_Length = 8;
        } break;
        case (DM_Motor_Control_Method_OMEGA): {
            memcpy(&CAN_Tx_Data[0], &omega, sizeof(float));

            Control_Tx_ID     = CAN_ID + 0x200;
            Control_Tx_Length = 4;
        } break;
        default: {
        } break;
    }
}

/**
 * @brief 提交最近一次生成的控制帧, 提交成功后清除待发送状态
 */
uint8_t Class_DM_Motor::Send_Control_Frame()
{
    if (Control_Tx_Length == 0) {
        return (HAL_ERROR);
    }

    uint8_t status = CAN_Send_Data(CAN_Manage_Object->CAN_Handler, Control_Tx_ID, CAN_Tx_Data, Control_Tx_Length);
    if (status == HAL_OK) {
        Control_Tx_Length = 0;
    }
    return (status);
}

/************************ COPYRIGHT(C) USTC-ROBOWALKER **************************/
