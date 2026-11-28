/**
 * @file crt_gimbal.cpp
 * @author cjw
 * @brief 云台
 * @version 0.1
 * @date 2025-07-1 0.1 26赛季定稿
 *
 * @copyright ZLLC 2026
 *
 */

/* Includes ------------------------------------------------------------------*/

#include "crt_gimbal.h"

/* Private macros ------------------------------------------------------------*/

/* Private types -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/

/* Private function declarations ---------------------------------------------*/

/* Function prototypes -------------------------------------------------------*/
#define LOCK_Omega 5.0f

void Class_DM_Motor_J4310::Init(FDCAN_HandleTypeDef *hcan, Enum_DM_Motor_ID __CAN_ID,
                                Enum_DM_Motor_Control_Method __Control_Method,
                                int32_t __Position_Offset, float __Omega_Max, float __Torque_Max)
{
    // 2026云台仅使用零位置偏移；角度反馈仍按原协议保持[0, 2PI]。
    (void)__Position_Offset;
    Class_DM_Motor::Init(hcan, __CAN_ID, __Control_Method,
                         __Omega_Max, __Torque_Max, PI);
    Torque_Only = (__Control_Method == DM_Motor_Control_Method_MIT_TORQUE);
}

void Class_DM_Motor_J4310::Init(FDCAN_HandleTypeDef *hcan, Enum_DM_Motor_ID __CAN_ID,
                                Enum_DM_Motor_Control_Method __Control_Method, float __Angle_Max)
{
    Class_DM_Motor::Init(hcan, __CAN_ID, __Control_Method,
                         20.94359f, 7.0f, __Angle_Max);
    Torque_Only = (__Control_Method == DM_Motor_Control_Method_MIT_TORQUE);
}

void Class_DM_Motor_J4310::Set_DM_Motor_Control_Method(Enum_DM_Motor_Control_Method method)
{
    DM_Motor_Control_Method = method;
    Torque_Only = (method == DM_Motor_Control_Method_MIT_TORQUE);
}

void Class_DM_Motor_J4310::Data_Process(uint8_t *Rx_Data)
{
    uint16_t tmp_position = (Rx_Data[1] << 8) | Rx_Data[2];
    uint16_t tmp_omega = (Rx_Data[3] << 4) | (Rx_Data[4] >> 4);
    uint16_t tmp_torque = ((Rx_Data[4] & 0x0f) << 8) | Rx_Data[5];

    Data.CAN_ID = Rx_Data[0] & 0x0f;
    Data.ErrorCode = static_cast<Enum_DM_Motor_ErrorCode>(Rx_Data[0] >> 4);
    Data.Now_Angle = (float)tmp_position / 65536.0f * 2.0f * PI;
    Data.Now_Omega = Math_Int_To_Float(tmp_omega, 0, (1 << 12) - 1, -Omega_Max, Omega_Max);
    Data.Now_Torque = Math_Int_To_Float(tmp_torque, 0, (1 << 12) - 1, -Torque_Max, Torque_Max);
    Data.Now_MOS_Temperature = Rx_Data[6] + CELSIUS_TO_KELVIN;
    Data.Now_Rotor_Temperature = Rx_Data[7] + CELSIUS_TO_KELVIN;
}

void Class_DM_Motor_J4310::TIM_PID_PeriodElapsedCallback()
{
    if (DM_Motor_Control_Method == DM_Motor_Control_Method_MIT_TORQUE)
    {
        if (DM_Motor_Control_Alg == DM_PID_Omega)
        {
            PID_Omega.Set_Now(Transform_Omega);
            PID_Omega.Set_Target(Target_Omega);
            PID_Omega.TIM_Adjust_PeriodElapsedCallback();

            Out = PID_Omega.Get_Out();
            Output_Torque = Torque_Max / 2048.0f * Out;
        }
        else if (DM_Motor_Control_Alg == DM_PID_Angle)
        {
            PID_Angle.Set_Now(Transform_Angle);
            PID_Angle.Set_Target(Target_Angle);
            PID_Angle.TIM_Adjust_PeriodElapsedCallback();
            Target_Omega = PID_Angle.Get_Out();

            PID_Omega.Set_Now(Transform_Omega);
            PID_Omega.Set_Target(Target_Omega);
            PID_Omega.TIM_Adjust_PeriodElapsedCallback();

            Out = PID_Omega.Get_Out();
            float tmp_Torque = J * Transform_Target_Acc + B * Transform_Target_Vel;
            Out += tmp_Torque / (Torque_Max / 2048.0f);
            Output_Torque = Torque_Max / 2048.0f * Out;
        }
        else if (DM_Motor_Control_Alg == DM_Motor_DISANLE)
        {
            Out = 0.0f;
            Output_Angle = 0.0f;
            Output_Omega = 0.0f;
            Output_Torque = 0.0f;
        }
    }
    else if (DM_Motor_Control_Method == DM_Motor_Control_Method_POSITION_OMEGA)
    {
        Output_Angle = Target_Angle;
        Output_Omega = Target_Omega;
    }
    else
    {
        Out = 0.0f;
        Output_Angle = 0.0f;
        Output_Omega = 0.0f;
        Output_Torque = 0.0f;
    }
}

void Class_DM_Motor_J4310::TIM_Process_PeriodElapsedCallback()
{
    Pack_Control_Frame(Output_Angle, Output_Omega, Output_Torque,
                       Torque_Only ? 0.0f : MIT_K_P,
                       Torque_Only ? 0.0f : MIT_K_D);
    Send_Control_Frame();
}

void Class_DM_Motor_J4310::Compensite_Out(float Compensite_Value)
{
    Output_Torque += Compensite_Value;
    Math_Constrain(&Output_Torque, -Torque_Max, Torque_Max);
}

void Class_Gimbal_Yaw_Motor_GM6020::TIM_PID_PeriodElapsedCallback()
{
    switch (Class_DJI_Motor_GM6020::DJI_Motor_Control_Method)
    {
    case DJI_Motor_Control_Method_OPENLOOP:
        Class_DJI_Motor_GM6020::Out = Class_DJI_Motor_GM6020::Target_Torque /
                                      Class_DJI_Motor_GM6020::Omega_Max *
                                      Class_DJI_Motor_GM6020::Output_Max;
        break;
    case DJI_Motor_Control_Method_TORQUE:
        PID_Torque.Set_Target(Class_DJI_Motor_GM6020::Target_Torque);
        PID_Torque.Set_Now(Class_DJI_Motor_GM6020::Data.Now_Torque);
        PID_Torque.TIM_Adjust_PeriodElapsedCallback();
        Class_DJI_Motor_GM6020::Out = PID_Torque.Get_Out();
        break;
    case DJI_Motor_Control_Method_OMEGA:
        PID_Omega.Set_Target(Class_DJI_Motor_GM6020::Target_Omega_Angle);
        PID_Omega.Set_Now(Class_DJI_Motor_GM6020::Transform_Omega);
        PID_Omega.TIM_Adjust_PeriodElapsedCallback();
        Class_DJI_Motor_GM6020::Out = PID_Omega.Get_Out();
        break;
    case DJI_Motor_Control_Method_ANGLE:
    {
        PID_Angle.Set_Target(Class_DJI_Motor_GM6020::Target_Angle);
        PID_Angle.Set_Now(Class_DJI_Motor_GM6020::Transform_Angle);
        PID_Angle.TIM_Adjust_PeriodElapsedCallback();
        Class_DJI_Motor_GM6020::Target_Omega_Angle = PID_Angle.Get_Out();

        PID_Omega.Set_Target(Class_DJI_Motor_GM6020::Target_Omega_Angle);
        PID_Omega.Set_Now(Class_DJI_Motor_GM6020::Transform_Omega);
        PID_Omega.TIM_Adjust_PeriodElapsedCallback();

        float tmp_Torque = J * Transform_Target_Acc + B * Transform_Target_Vel +
                           Mgl * arm_cos_f32(Class_DJI_Motor_GM6020::Transform_Angle / 57.3f) + C;
        float limited_out = PID_Omega.Get_Out() +
                            tmp_Torque * 16384.0f / (3.0f * 0.741f);
        Math_Constrain(&limited_out,
                       -(float)Class_DJI_Motor_GM6020::Output_Max,
                       (float)Class_DJI_Motor_GM6020::Output_Max);
        Class_DJI_Motor_GM6020::Out = limited_out;
    }
        break;
    case DJI_Motor_Control_Method_AGV_MODE:
        break;
    default:
        Class_DJI_Motor_GM6020::Out = 0.0f;
        break;
    }
    Class_DJI_Motor_GM6020::Output();
}

void Class_Gimbal_Yaw_Motor_GM6020::TIM_SMC_PeriodElapsedCallback()
{
    // 保留2026入口；原工程实际始终采用PID路径，这里不引入另一套输出语义。
    TIM_PID_PeriodElapsedCallback();
}

/**
 * @brief 对于电机角度控制时的突变点处理
 * @param Target_Angle 
 * @param Now_Angle 
 */
void Angle_Continuity_Process(float* Target_Angle, float Now_Angle){
    float Diff_Angle = *Target_Angle - Now_Angle;
    while (Diff_Angle > 180.0f)
    {
        *Target_Angle -= (2 * 180.0f);
        Diff_Angle = *Target_Angle - Now_Angle;
    }
    while (Diff_Angle < -180.0f)
    {
        *Target_Angle += (2 * 180.0f);
        Diff_Angle = *Target_Angle - Now_Angle;
    }
}

void Class_Gimbal_Main_Yaw_Motor_LK::TIM_Process_PeriodElapsedCallback()
{
    switch (LK_Motor_Control_Method)
    {
    case LK_Motor_Control_Method_TORQUE:
        PID_Torque.Set_Target(Target_Torque);
        PID_Torque.Set_Now(Data.Now_Current);
        PID_Torque.TIM_Adjust_PeriodElapsedCallback();
        Out = static_cast<int16_t>(PID_Torque.Get_Out());
        break;
    case LK_Motor_Control_Method_OMEGA:
        PID_Omega.Set_Target(Target_Omega_Angle);
        PID_Omega.Set_Now(Transform_Omega);
        PID_Omega.TIM_Adjust_PeriodElapsedCallback();
        Target_Current = PID_Omega.Get_Out();
        Out = static_cast<int16_t>(Target_Current);
        break;
    case LK_Motor_Control_Method_ANGLE:
        PID_Angle.Set_Target(Target_Angle);
        PID_Angle.Set_Now(Transform_Angle);
        PID_Angle.TIM_Adjust_PeriodElapsedCallback();
        Target_Omega_Angle = PID_Angle.Get_Out();
        PID_Omega.Set_Target(Target_Omega_Angle);
        PID_Omega.Set_Now(Transform_Omega);
        PID_Omega.TIM_Adjust_PeriodElapsedCallback();
        Out = static_cast<int16_t>(PID_Omega.Get_Out());
        break;
    default:
        Out = 0;
        break;
    }
    Output();
}

/**
 * @brief 云台初始化
 *
 */
void Class_Gimbal::Init()
{
    // imu初始化
    Boardc_BMI.Init();
    // 2027 的 Class_IMU 不再提供 Motor_Main_Yaw 融合接口
    // Boardc_BMI.Motor_Main_Yaw = &Motor_Main_Yaw;

    External_IMU.Init(7.9999998e-05);

    // main电机
    Motor_Main_Yaw.PID_Angle.Init(0.30f, 0.0f, 0.002f, 0.0f, 3, 25);
    Motor_Main_Yaw.PID_Omega.Init(1000.0f, 0.0f, 0.0f, 0.0f, 400.0f, 4096.0f);
    Motor_Main_Yaw.PID_Torque.Init(0.f, 0.0f, 0.0f, 0.0f, Motor_Main_Yaw.Get_Output_Max(), Motor_Main_Yaw.Get_Output_Max());            //这样一直输出的都是0，失能
   
    Motor_Main_Yaw.Init(&hfdcan2, LK_Motor_ID_0x141, 200.0f,
                        MAIN_YAW_ENCODER_OFFSET, 33.0f,
                        LK_Motor_Control_Method_ANGLE, LK_Motor_Control_Torque);

    // yaw轴电机  0.6876f `
    Motor_Yaw.PID_Angle.Init(0.3f, 0.0f, 0.0f, 0.012f, 10, 10);
    //Kp给大容易因为大小Yaw联动的噪声出问题，达不到理想的想要，用大Ki补偿误差，还有Ki对抖动不敏感（积分，相位延迟）强制补偿掉，也可以尝试LESO，但他可能对噪声敏感一些（重在抗扰动）
    //Ki太大对阶跃信号抖动滞后，不用了
    Motor_Yaw.PID_Omega.Init(1300.0f, 500.0f, 0.0f, 0.0f, Motor_Yaw.Get_Output_Max(), Motor_Yaw.Get_Output_Max());
    Motor_Yaw.PID_Torque.Init(0.f, 0.0f, 0.0f, 0.0f, Motor_Yaw.Get_Output_Max(), Motor_Yaw.Get_Output_Max());

    Motor_Yaw.SMC_Control.Init(0.005, 85.0, 85.0, 5.0);

    Motor_Yaw.Init(&hfdcan1, DJI_Motor_ID_0x206, DJI_Motor_Control_Method_ANGLE, YAW_ENCODER_OFFSET);
    
    // pitch轴电机
    // Motor_Pitch.PID_Angle.Init(0.70f, 0.0f, 0.00f, 0.0f, 10.0f, 10.0f);
    // Motor_Pitch.PID_Omega.Init(120.0f, 0.0f, 0.0f, 0.0f, 2048.0f, 2048.0f);
    // Motor_Pitch.PID_Angle.Init(0.30f, 0.0f, 0.0f, 0.0f, 7.0f, 7.0f);
    // Motor_Pitch.PID_Omega.Init(5000.0f, 0.0f, 0.0f, 0.0f, 16384.0f, 16384.0f);
    // Motor_Pitch.PID_Angle.Init(0.55f, 0.0f, 0.0017, 0.0f, 7.0f, 7.0f);
    // Motor_Pitch.PID_Omega.Init(7000.0f, 5000.0f, 0.0f, 0.0f, 16384.0f, 16384.0f);
    // Motor_Pitch.PID_Angle.Init(0.0f, 0.0f, 0.0f, 0.0f, 10.0f, 10.0f);
    // Motor_Pitch.PID_Omega.Init(0.0f, 0.0f, 0.0f, 0.0f, 2048.0f, 2048.0f);
    // Motor_Pitch.Init(&hfdcan1, DM_Motor_ID_0xA1, DM_Motor_Control_Method_MIT_TORQUE, 0, 20.94359f, 3.0f);
    // Motor_Pitch.Init(&hfdcan1, DJI_Motor_ID_0x206, DJI_Motor_Control_Method_ANGLE);
    // Motor_Pitch_LESO.Init(0.08, 20.0, 1.0, Observe_Motor_Omega, Motor_GM6020, 0.002f);

    Motor_Pitch.PID_Angle.Init(1.0f, 0.0f, 0.0f, 0.0f, 2.0f, 15.0f);
    Motor_Pitch.PID_Omega.Init(130.0f, 0.f, 0.0f, 0.0f, 2048.0f, 2048.0f);
    Motor_Pitch.Init(&hfdcan1, DM_Motor_ID_0x02, DM_Motor_Control_Method_MIT_TORQUE, 0.0f, 30.0f, 7.0f);

    // Motor_Pitch.Set_Motor_Parameters(0.0f, 0.0f, 0.0f, 0.0f);
    Motor_Yaw.Set_Motor_Parameters(0.016f, 0.0f, 0.0f, 0.0f);
    Motor_Pitch.Set_Motor_Parameters(0.014f, 0.0f, 0.0f, 0.0f);

    Motor_Roll.Init(&hfdcan1, DM_Motor_ID_0x01, DM_Motor_Control_Method_POSITION_OMEGA, PI);
    Motor_Roll.Set_Target_Omega(0.0f);

    // 2027基类的在线回调按DM_Motor_Control_Status选择使能/失能帧, 该状态默认DISABLE且无人置位,
    // 会导致每50ms发出失能帧, 电机一直红灯。此处置为ENABLE, 恢复2026的"保持使能"行为。
    Motor_Pitch.Set_DM_Control_Status(DM_Motor_Control_Status_ENABLE);
    Motor_Roll.Set_DM_Control_Status(DM_Motor_Control_Status_ENABLE);

    External_IMU_Gyro_Yaw.Init(0.99,0.07);
    External_IMU_Gyro_Pitch.Init(0.99,0.07);
    Motor_Yaw_Angle_Filter.Init(0.34, 0.34);
}


/**
 * @brief 输出到电机
 *
 */
float tmp_Target_Angle = 0.0f, tmp_Target_Pitch_Angle = 0.0f;
//float test_c =  0.5895f;
float test_c =  0.5f;
extern float Sin_Single;

void Class_Gimbal::Output()
{
    static uint32_t camera_switch_time = 0;
    static uint8_t camera_switch_status = 0;
    static uint8_t last_cam_id = 0xFF ;//增
    static float pre_yaw_angle = 0.0f, pre_pitch_angle = 0.0f, pre_main_yaw_angle = 0.0f, pre_roll_angle = 0.0f;
    static uint8_t main_yaw_aligned = 0;
    static uint8_t fold_step = 0;
    // unfold_pending 是 Class_Gimbal 的成员变量, 由 ita_chariot 在 Fold_Cmd==0 退出 FOLD 时置 1

    if (Gimbal_Control_Type == Gimbal_Control_Type_DISABLE)
    {
        Gimbal_Disable();

        camera_switch_status = 0;
        camera_switch_time = 0;
        main_yaw_aligned = 0;           //失能时大Yaw轴是自由的, 重新使能后必须重新回正
        fold_step = 0;
        unfold_pending = 0;             //失能时清掉待展开标记, 避免下次使能误触发
    }
    else // 非失能模式
    {
        // Fold_Cmd==0 退出折叠时, 先把 Roll/Pitch/小Yaw 拉回正前方
        // 放在所有 control type 分支之前, 避免 MiniPC 离线时被 MINIPC 分支外层条件挡住
        if (unfold_pending)
        {
            unfold_pending = 0;
            Motor_Roll.Set_Target_Angle(ROLL_VERTICAL);
            Motor_Pitch.Set_Target_Angle(0.0f);
            Motor_Yaw.Set_DJI_Motor_Control_Method(DJI_Motor_Control_Method_ANGLE);
            Motor_Yaw.Set_Target_Angle(-15.0f);
            Motor_Yaw.Set_Transform_Target_Vel(0.0f);
            Motor_Yaw.Set_Transform_Target_Acc(0.0f);
            Motor_Pitch.Set_Transform_Target_Vel(0.0f);
            Motor_Pitch.Set_Transform_Target_Acc(0.0f);
        }

        if(Gimbal_Control_Type != Gimbal_Control_Type_FOLD)
        {
            fold_step = 0;
        }
        if (Gimbal_Control_Type == Gimbal_Control_Type_NORMAL)
        {
             

           //控制方式
            Motor_Yaw.Set_DJI_Motor_Control_Method(DJI_Motor_Control_Method_ANGLE);
            Motor_Main_Yaw.Set_LK_Motor_Control_Method(LK_Motor_Control_Method_ANGLE);
            Motor_Pitch.Set_DM_Motor_Control_Alg(DM_PID_Angle);
            
            //一次性回正
            if(main_yaw_aligned == 0){
                float encoder_err = MAIN_YAW_ALIGN_ENCODER_ANGLE - Motor_Main_Yaw.Get_Now_Angle();
                Target_Main_Yaw_Angle = Boardc_BMI.Get_Angle_Yaw() + MAIN_YAW_IMU_ENCODER_SAME_DIR * encoder_err;
                main_yaw_aligned = 1;
            }

            Target_Yaw_Angle = tmp_Target_Angle;
            //Target_Pitch_Angle=Sin_Single;
           // Target_Pitch_Angle = tmp_Target_Pitch_Angle;//会和dr16的遥控器输入冲突
            //对于大Yaw控制的突变点与优劣弧处理       0--2*PI

            Angle_Continuity_Process(&Target_Main_Yaw_Angle, Boardc_BMI.Get_Angle_Yaw());
            Angle_Continuity_Process(&Target_Yaw_Angle, Motor_Yaw.Get_Zero_Offset_Angle());

            // 限制角度
            Math_Constrain(&Target_Pitch_Angle, Min_Pitch_Angle, Max_Pitch_Angle);
            Math_Constrain(&Target_Roll_Angle, Min_Roll_Angle, Max_Roll_Angle);

            // 设置目标角度    Motor_Yaw的角度是以偏置零点为原点，改Encoder_offset实现校准
            Motor_Yaw.Set_Target_Angle(Target_Yaw_Angle - 15.0f);                        //可能可以加前馈
            // Motor_Yaw.Set_Target_Angle(test_1);
            Motor_Pitch.Set_Target_Angle(Target_Pitch_Angle);
            Motor_Main_Yaw.Set_Target_Angle(Target_Main_Yaw_Angle);
            Motor_Roll. Set_DM_Motor_Control_Method (DM_Motor_Control_Method_POSITION_OMEGA);
            Motor_Roll.Set_Target_Angle(ROLL_VERTICAL);
            Motor_Roll.Set_Target_Omega(LOCK_Omega);

            Motor_Yaw.Set_Transform_Target_Vel(0.0f);
            Motor_Yaw.Set_Transform_Target_Acc(0.0f);
            Motor_Pitch.Set_Transform_Target_Vel(0.0f);
            Motor_Pitch.Set_Transform_Target_Acc(0.0f);

            pre_yaw_angle      = 0.0f;
            pre_pitch_angle    = Motor_Pitch.Get_Transform_Angle();
            pre_main_yaw_angle = Boardc_BMI.Get_Angle_Yaw();
            pre_roll_angle = 0.0f;

            camera_switch_status = 0;
            camera_switch_time = 0;
            last_mode_for_cruise = -1;       //重置巡航状态
        }
        // else if (Get_Gimbal_Control_Type() == Gimbal_Control_Type_FOLD)
        // {
        //     Motor_Main_Yaw.Set_LK_Motor_Control_Method(LK_Motor_Control_Method_ANGLE);
        //     Motor_Roll.Set_DM_Motor_Control_Method(DM_Motor_Control_Method_POSITION_OMEGA);
        //     Motor_Pitch.Set_DM_Motor_Control_Alg(DM_PID_Angle);
        //     Target_Roll_Angle = ROLL_VERTICAL + FOLD_ROLL;      //折叠目标角, 见第3条

        //     // crt_gimbal.cpp FOLD 分支里，Set_Target_Angle 之前加
        //     Math_Constrain(&Target_Roll_Angle, Min_Roll_Angle, Max_Roll_Angle);
        //     //大Yaw目标沿用Control_Gimbal里摇杆累加的Target_Main_Yaw_Angle, 只做连续性处理, 随动环跟随大Yaw实现折叠时旋转
        //     Angle_Continuity_Process(&Target_Main_Yaw_Angle, Boardc_BMI.Get_Angle_Yaw());
        //     Motor_Main_Yaw.Set_Target_Angle(Target_Main_Yaw_Angle);
        //     Motor_Roll.Set_Target_Angle(Target_Roll_Angle);
        //     Motor_Pitch.Set_Target_Angle(5.90486765f);
        //     Motor_Roll.Set_Target_Omega(5.0f);
        // }
        else if (Get_Gimbal_Control_Type() == Gimbal_Control_Type_FOLD)
        {
            Motor_Main_Yaw.Set_LK_Motor_Control_Method(LK_Motor_Control_Method_ANGLE);
            Motor_Roll.Set_DM_Motor_Control_Method(DM_Motor_Control_Method_POSITION_OMEGA);
            Motor_Pitch.Set_DM_Motor_Control_Alg(DM_PID_Angle);

            //折叠时小Yaw不参与折叠动作: 保持进入折叠时的角度, 并清掉上位机残留的速度/加速度前馈
            Motor_Yaw.Set_DJI_Motor_Control_Method(DJI_Motor_Control_Method_ANGLE);
            Motor_Yaw.Set_Transform_Target_Vel(0.0f);
            Motor_Yaw.Set_Transform_Target_Acc(0.0f);
            Target_Yaw_Angle = tmp_Target_Angle;
            Angle_Continuity_Process(&Target_Yaw_Angle, Motor_Yaw.Get_Zero_Offset_Angle());
            Motor_Yaw.Set_Target_Angle(Target_Yaw_Angle - 15.0f);

            //上位机折叠指令: 先让大Yaw回到车体正前方, 到位后再低头
            if((MiniPC->Get_Gimbal_Fold_Cmd() == 1) && (fold_step == 0))
            {
                //把"正前方"换算到IMU系: 编码器角转到MAIN_YAW_ALIGN_ENCODER_ANGLE
                Target_Main_Yaw_Angle = Boardc_BMI.Get_Angle_Yaw() + MAIN_YAW_IMU_ENCODER_SAME_DIR * (MAIN_YAW_ALIGN_ENCODER_ANGLE - Motor_Main_Yaw.Get_Now_Angle());
                Angle_Continuity_Process(&Target_Main_Yaw_Angle, Boardc_BMI.Get_Angle_Yaw());
                Motor_Main_Yaw.Set_Target_Angle(Target_Main_Yaw_Angle);

                //回正期间: Roll保持垂直, Pitch原地保持
                Motor_Roll.Set_Target_Angle(ROLL_VERTICAL);
                Motor_Roll.Set_Target_Omega(LOCK_Omega);
                Motor_Pitch.Set_Target_Angle(-External_IMU.Get_Angle_Pitch());

                //回正到位判断
                if(fabs(MAIN_YAW_ALIGN_ENCODER_ANGLE - Motor_Main_Yaw.Get_Now_Angle()) < 1.0f)
                {
                    fold_step = 1;
                }
            }
            else
            {
                Target_Roll_Angle = ROLL_VERTICAL + FOLD_ROLL;      //折叠目标角
                Math_Constrain(&Target_Roll_Angle, Min_Roll_Angle, Max_Roll_Angle);
                //大Yaw沿用Control_Gimbal里摇杆累加的目标, 只做连续性处理
                Angle_Continuity_Process(&Target_Main_Yaw_Angle, Boardc_BMI.Get_Angle_Yaw());
                Motor_Main_Yaw.Set_Target_Angle(Target_Main_Yaw_Angle);
                Motor_Roll.Set_Target_Angle(Target_Roll_Angle);
                Motor_Pitch.Set_Target_Angle(5.50486765f);
                Motor_Roll.Set_Target_Omega(5.0f);
            }
        }
        else if ((Get_Gimbal_Control_Type() == Gimbal_Control_Type_MINIPC) && (MiniPC->Get_MiniPC_Status() != MiniPC_Status_DISABLE))
        {
            uint8_t cur_cam_id = MiniPC-> Get_Camera_Id ();//增
            if (cur_cam_id == 0 || cur_cam_id == 4 ){
                last_cam_id = 0xFF ;
            }//增
            if(MiniPC->Get_Camera_Id() != 0 && MiniPC->Get_Camera_Id() != 4 && cur_cam_id!= last_cam_id && camera_switch_status == 0 && camera_switch_time > 500)
            {
                camera_switch_time = 0;
                camera_switch_status = 1;
                last_cam_id = cur_cam_id;//增

                Target_Yaw_Angle      = 0.0f;
                Target_Pitch_Angle    = 0.0f;
                //Target_Main_Yaw_Angle = Boardc_BMI.Get_Angle_Yaw() + MiniPC->Get_Camera_Id() * 90.0f;
                if (MiniPC->Get_Camera_Id() == 1)
                {
                    Target_Main_Yaw_Angle = Boardc_BMI.Get_Angle_Yaw() +  120.0f;
                }
                else if (MiniPC->Get_Camera_Id() == 2)
                {
                    Target_Main_Yaw_Angle = Boardc_BMI.Get_Angle_Yaw() -  120.0f;
                }
                
                
                Motor_Yaw.Set_DJI_Motor_Control_Method(DJI_Motor_Control_Method_ANGLE);
                Motor_Main_Yaw.Set_LK_Motor_Control_Method(LK_Motor_Control_Method_ANGLE);
                Motor_Pitch.Set_DM_Motor_Control_Alg(DM_PID_Angle);
            }

            if(camera_switch_status == 1){          //正在转动大Yaw
                Angle_Continuity_Process(&Target_Yaw_Angle, Motor_Yaw.Get_Zero_Offset_Angle());
                Angle_Continuity_Process(&Target_Main_Yaw_Angle, Boardc_BMI.Get_Angle_Yaw());

                Motor_Yaw.Set_Target_Angle(Target_Yaw_Angle-15.0f);                       //可能可以加前馈
                Motor_Pitch.Set_Target_Angle(Target_Pitch_Angle);
                Motor_Main_Yaw.Set_Target_Angle(Target_Main_Yaw_Angle);

                if(fabs(Motor_Main_Yaw.Get_Transform_Angle() - Target_Main_Yaw_Angle) < 1.0f){
                    camera_switch_status = 2;             //清空状态
                    camera_switch_time = 0;
                }

                camera_switch_time ++;              //记录距离上一次全向感知执行完成的时间间隔

                pre_yaw_angle      = 0.0f;
                pre_pitch_angle    = 0.0f;
                pre_main_yaw_angle = Boardc_BMI.Get_Angle_Yaw();

                return;
            }
            else if(camera_switch_status == 2){         //大Yaw转动完成，保持一定时间不动
                Motor_Main_Yaw.Set_LK_Motor_Control_Method(LK_Motor_Control_Method_ANGLE);
                Target_Main_Yaw_Angle = pre_main_yaw_angle;
                Angle_Continuity_Process(&Target_Main_Yaw_Angle, Boardc_BMI.Get_Angle_Yaw());
                Motor_Main_Yaw.Set_Target_Angle(Target_Main_Yaw_Angle);

                camera_switch_time ++;              //
                if(camera_switch_time > 2000){
                    camera_switch_status = 0;
                    camera_switch_time = 0;
                }
            }
            else if(camera_switch_status == 0){         //上位机可以控制大Yaw，正常状态
                camera_switch_time ++;              //记录距离上一次全向感知执行完成的时间间隔
                
                if(fabs(MiniPC->Get_Rx_Target_Omega_Yaw_Main()) < 0.01f){
                    // 导航的控制
                    Motor_Main_Yaw.Set_LK_Motor_Control_Method(LK_Motor_Control_Method_ANGLE);
                    Target_Main_Yaw_Angle = pre_main_yaw_angle;
                    Angle_Continuity_Process(&Target_Main_Yaw_Angle, Boardc_BMI.Get_Angle_Yaw());
                    Motor_Main_Yaw.Set_Target_Angle(Target_Main_Yaw_Angle);
                }
                else{
                    //全向感知的控制
                    Motor_Main_Yaw.Set_LK_Motor_Control_Method(LK_Motor_Control_Method_OMEGA);
                    float Target_Main_Yaw_Omega = MiniPC->Get_Rx_Target_Omega_Yaw_Main();
                    Motor_Main_Yaw.Set_Target_Omega_Angle(Target_Main_Yaw_Omega);               //rad/s
                    
                    Target_Main_Yaw_Angle= Boardc_BMI.Get_Angle_Yaw();
                    pre_main_yaw_angle = Boardc_BMI.Get_Angle_Yaw();
                }

            }

            if (MiniPC->Get_mode() == 1 || MiniPC->Get_mode() == 2 || MiniPC->Get_mode() == 3)
            { // 当进入巡航或者目标丢失时，进入巡航状态
                Motor_Yaw.Set_DJI_Motor_Control_Method(DJI_Motor_Control_Method_ANGLE);
                Motor_Pitch.Set_DM_Motor_Control_Alg(DM_PID_Angle);

                float MiniPC_Target_Yaw = MiniPC->Get_Rx_Yaw_Angle();
                float MiniPC_Target_Pitch = MiniPC->Get_Rx_Pitch_Angle();

                if (MiniPC->Get_mode() == 3)
                {
                    Target_Yaw_Angle = pre_yaw_angle;
                    Target_Pitch_Angle = pre_pitch_angle;
                }
                else
                {
                    if(MiniPC->Get_Camera_Id() == 1 || MiniPC->Get_Camera_Id() == 2)
                    {
                        Target_Yaw_Angle = 0.0f;
                    }
                    else
                    {
                    Target_Yaw_Angle = MiniPC_Target_Yaw - Motor_Main_Yaw.Get_Transform_Angle();
                    }
                    Target_Pitch_Angle = MiniPC_Target_Pitch;
                    
                    pre_yaw_angle = Target_Yaw_Angle;
                    pre_pitch_angle = Target_Pitch_Angle;
                }

                // 怕超出限位到死区
                Math_Constrain(&Target_Yaw_Angle, -LIMIT_YAW_ANGLE, LIMIT_YAW_ANGLE);
                Math_Constrain(&Target_Pitch_Angle, Min_Pitch_Angle, Max_Pitch_Angle);

                Angle_Continuity_Process(&Target_Yaw_Angle, Motor_Yaw.Get_Zero_Offset_Angle());

                Motor_Yaw.Set_Target_Angle(Target_Yaw_Angle-15.0f);
                Motor_Pitch.Set_Target_Angle(Target_Pitch_Angle);

                Motor_Yaw.Set_Transform_Target_Vel(MiniPC->Get_yaw_vel() / 57.3f);
                Motor_Yaw.Set_Transform_Target_Acc(MiniPC->Get_yaw_acc() / 57.3f);
                Motor_Pitch.Set_Transform_Target_Vel(MiniPC->Get_pitch_vel() / 57.3f);
                Motor_Pitch.Set_Transform_Target_Acc(MiniPC->Get_pitch_acc() / 57.3f);

                if (MiniPC->Get_mode() == 3)
                {
                    last_mode_for_cruise = 1;
                }
                else
                {
                    last_mode_for_cruise = MiniPC->Get_mode();
                }
            }
            else
            {
                // 检测是否刚进入巡航
                if (last_mode_for_cruise == 1 || last_mode_for_cruise == 2 || last_mode_for_cruise == -1)
                {
                    float cur_yaw = Motor_Yaw.Get_Transform_Angle();
                    float cur_pitch = Motor_Pitch.Get_Transform_Angle();

                    float ratio_yaw = (cur_yaw - YAW_OFFSET) / YAW_AMPLITUDE;
                    float ratio_pitch = (cur_pitch - PITCH_OFFSET) / PITCH_AMPLITUDE;

                    // 钳位到 [-1, 1]
                    if (ratio_yaw > 1.0f)
                        ratio_yaw = 1.0f;
                    if (ratio_yaw < -1.0f)
                        ratio_yaw = -1.0f;
                    if (ratio_pitch > 1.0f)
                        ratio_pitch = 1.0f;
                    if (ratio_pitch < -1.0f)
                        ratio_pitch = -1.0f;

                    phi0_yaw = asinf(ratio_yaw);
                    phi0_pitch = asinf(ratio_pitch);
                    cruise_start_time = Single_time;
                }
                last_mode_for_cruise = 0;

                float t = (float)(Single_time - cruise_start_time) / 1000.0f;

                Target_Yaw_Angle = YAW_AMPLITUDE * sinf(2.0f * PI * YAW_FREQ * t + phi0_yaw) + YAW_OFFSET;
                Target_Pitch_Angle = PITCH_AMPLITUDE * sinf(2.0f * PI * PITCH_FREQ * t + phi0_pitch) + PITCH_OFFSET;

                Motor_Yaw.Set_DJI_Motor_Control_Method(DJI_Motor_Control_Method_ANGLE);
                Motor_Pitch.Set_DM_Motor_Control_Alg(DM_PID_Angle);
                Angle_Continuity_Process(&Target_Yaw_Angle, Motor_Yaw.Get_Zero_Offset_Angle());
                Math_Constrain(&Target_Yaw_Angle, -LIMIT_YAW_ANGLE, LIMIT_YAW_ANGLE);
                Math_Constrain(&Target_Pitch_Angle, Min_Pitch_Angle, Max_Pitch_Angle);
                Motor_Yaw.Set_Target_Angle(Target_Yaw_Angle-15.0f);
                Motor_Pitch.Set_Target_Angle(Target_Pitch_Angle);
                Motor_Yaw.Set_Transform_Target_Vel(0.0f);
                Motor_Yaw.Set_Transform_Target_Acc(0.0f);
                Motor_Pitch.Set_Transform_Target_Vel(0.0f);
                Motor_Pitch.Set_Transform_Target_Acc(0.0f);

                pre_yaw_angle = Motor_Yaw.Get_Zero_Offset_Angle();
                pre_pitch_angle = Motor_Pitch.Get_Transform_Angle();
            }
        }
       else if ((Get_Gimbal_Control_Type() == Gimbal_Control_Type_MINIPC) && (MiniPC->Get_MiniPC_Status() == MiniPC_Status_DISABLE))
        {
            Motor_Yaw.Set_DJI_Motor_Control_Method(DJI_Motor_Control_Method_ANGLE);
            Motor_Pitch.Set_DM_Motor_Control_Alg(DM_PID_Angle);
            Motor_Main_Yaw.Set_LK_Motor_Control_Method(LK_Motor_Control_Method_ANGLE);

            Target_Yaw_Angle      = pre_yaw_angle;
            Target_Pitch_Angle    = pre_pitch_angle;
            Target_Main_Yaw_Angle = pre_main_yaw_angle;
            Angle_Continuity_Process(&Target_Main_Yaw_Angle, Boardc_BMI.Get_Angle_Yaw());
            Angle_Continuity_Process(&Target_Yaw_Angle, Motor_Yaw.Get_Zero_Offset_Angle());

            // 限制角度
            Math_Constrain(&Target_Yaw_Angle,-LIMIT_YAW_ANGLE, LIMIT_YAW_ANGLE);            
            Math_Constrain(&Target_Pitch_Angle, Min_Pitch_Angle, Max_Pitch_Angle);

            // 设置目标角度
            Motor_Yaw.Set_Target_Angle(Target_Yaw_Angle-15.0f);
            Motor_Pitch.Set_Target_Angle(Target_Pitch_Angle);
            Motor_Main_Yaw.Set_Target_Angle(Target_Main_Yaw_Angle);

            Motor_Yaw.Set_Transform_Target_Vel(0.0f);
            Motor_Yaw.Set_Transform_Target_Acc(0.0f);
            Motor_Pitch.Set_Transform_Target_Vel(0.0f);
            Motor_Pitch.Set_Transform_Target_Acc(0.0f);

            camera_switch_status = 0;
            camera_switch_time = 0;
            last_mode_for_cruise = -1;       //重置巡航状态
        }

        //小Yaw掉线保护: 只让该轴目标跟随当前角度并清积分, 输出为0, 不影响Pitch/Roll/大Yaw
        if(Motor_Yaw.Get_DJI_Motor_Status() == DJI_Motor_Status_DISABLE)
        {
            Motor_Yaw.Set_Target_Angle(Motor_Yaw.Get_Zero_Offset_Angle());
            Motor_Yaw.PID_Angle.Set_Integral_Error(0.0f);
            Motor_Yaw.PID_Omega.Set_Integral_Error(0.0f);
        }
    }
}

void Class_Gimbal::Gimbal_Disable()
{
    // 云台失能
    Motor_Yaw.Set_DJI_Motor_Control_Method(DJI_Motor_Control_Method_OPENLOOP);
    Motor_Main_Yaw.Set_LK_Motor_Control_Method(LK_Motor_Control_Method_TORQUE);
    Motor_Pitch.Set_DM_Motor_Control_Alg(DM_Motor_DISANLE);
    Motor_Roll.Set_DM_Motor_Control_Method(DM_Motor_Control_Method_MIT_TORQUE);
    Motor_Roll.Set_DM_Motor_Control_Alg(DM_Motor_DISANLE);   //发0扭矩, 电机瘫

    Motor_Yaw.PID_Angle.Set_Integral_Error(0.0f);
    Motor_Yaw.PID_Omega.Set_Integral_Error(0.0f);
    Motor_Yaw.PID_Torque.Set_Integral_Error(0.0f);
    Motor_Pitch.PID_Angle.Set_Integral_Error(0.0f);
    Motor_Pitch.PID_Omega.Set_Integral_Error(0.0f);
    Motor_Main_Yaw.PID_Angle.Set_Integral_Error(0.0f);
    Motor_Main_Yaw.PID_Omega.Set_Integral_Error(0.0f);
    Motor_Main_Yaw.PID_Torque.Set_Integral_Error(0.0f);

    Motor_Yaw.Set_Target_Torque(0.0f);
    Motor_Pitch.Set_Target_Torque(0.0f);
    Motor_Main_Yaw.Set_Target_Torque(0.0f);

    Motor_Roll.Set_Target_Omega(0.0f);

    last_mode_for_cruise = -1;       //重置巡航状态
}

/**
 * @brief TIM定时器中断计算回调函数
 *
 */
void Class_Gimbal::TIM_Calculate_PeriodElapsedCallback()
{
    External_IMU_Gyro_Yaw.Set_Now(External_IMU.Get_Gyro_Yaw());
    External_IMU_Gyro_Yaw.Recv_Adjust_PeriodElapsedCallback();              //滤除由于大Yaw转动带来的联动噪声
    External_IMU_Gyro_Pitch.Set_Now(External_IMU.Get_Gyro_Pitch());
    External_IMU_Gyro_Pitch.Recv_Adjust_PeriodElapsedCallback();              //滤除由于大Yaw转动带来的联动噪声
    // Motor_Yaw_Angle_Filter.Set_Now(Motor_Yaw.Get_Zero_Offset_Angle());
    // Motor_Yaw_Angle_Filter.Recv_Adjust_PeriodElapsedCallback();

    //数据传输更新        记得对方向
    Motor_Yaw.Set_Transform_Omega(External_IMU_Gyro_Yaw.Get_Out());
    Motor_Yaw.Set_Transform_Angle(Motor_Yaw.Get_Zero_Offset_Angle());

    Motor_Main_Yaw.Set_Transform_Omega(Boardc_BMI.Get_Gyro_Yaw());
    Motor_Main_Yaw.Set_Transform_Angle(Boardc_BMI.Get_Angle_Yaw());

    //调试观测: 分别记录两种Pitch角度来源
    Debug_Pitch_Encoder_Angle = (Motor_Pitch.Get_Now_Angle() - 5.37f) * 57.2958f;
    Debug_Pitch_External_IMU_Angle = -External_IMU.Get_Angle_Pitch();
    //角速度统一换成°/s, 方便和外置IMU陀螺同轴对比
    Debug_Pitch_Encoder_Omega = Motor_Pitch.Get_Now_Omega() * 57.2958f;
    Debug_External_IMU_Gyro_Pitch = External_IMU.Get_Gyro_Pitch() * 57.2958f;

    //Pitch速度反馈: 用电机自带速度(无串口延迟), 一阶低通截止约50Hz抑制零速量化噪声
    //alpha = dt/(dt+RC), dt=1ms, fc=50Hz -> RC=3.18ms -> alpha≈0.24
    Pitch_Omega_LPF += 0.15f * (Motor_Pitch.Get_Now_Omega() - Pitch_Omega_LPF);
    Debug_Pitch_Omega_Feedback = Pitch_Omega_LPF * 57.2958f;
//    Debug_Pitch_Target_Angle = Motor_Pitch.Get_Target_Angle();
    Debug_Pitch_Out_Torque = Motor_Pitch.Get_Output_Torque();

    if (Get_Gimbal_Control_Type() == Gimbal_Control_Type_FOLD)
    {
    //折叠时Roll翻转导致IMU坐标失效, 小Yaw/Pitch改用电机自身编码器闭环
    Motor_Yaw.Set_Transform_Omega(Motor_Yaw.Get_Now_Omega_Radian());
    Motor_Yaw.Set_Transform_Angle(Motor_Yaw.Get_Zero_Offset_Angle());
    Motor_Pitch.Set_Transform_Omega(Motor_Pitch.Get_Now_Omega());
    Motor_Pitch.Set_Transform_Angle(Motor_Pitch.Get_Now_Angle());
    }
    else
    {
    //速度反馈用电机自带速度的低通结果, 避开外置IMU串口延迟
    Motor_Pitch.Set_Transform_Omega(Pitch_Omega_LPF);
    Motor_Pitch.Set_Transform_Angle((Motor_Pitch.Get_Now_Angle() - 5.37f) * 57.2958f);
    }
    

    //控制更新
    Output();
    

    //可能得写死区严重时的强制保护

    // Motor_Pitch_LESO.Set_CMD_Torque(Motor_Pitch.Get_Out());               //上一时刻的输出力矩
    // Motor_Pitch_LESO.Set_Now_Omega(Motor_Pitch.Get_Transform_Omega());
    // Motor_Pitch_LESO.TIM_Adjust_PeriodElapsedCallback();

    // if(Motor_Yaw.Get_Control_Method() == DJI_Motor_Control_Method_ANGLE){
    //     Motor_Yaw.TIM_SMC_PeriodElapsedCallback();
    // }
    // else{
    //     Motor_Yaw.TIM_PID_PeriodElapsedCallback();
    // }

    //PID输出
    Motor_Yaw.TIM_PID_PeriodElapsedCallback();
    Motor_Pitch.TIM_PID_PeriodElapsedCallback();
    Motor_Main_Yaw.TIM_Process_PeriodElapsedCallback();
    Motor_Roll.TIM_PID_PeriodElapsedCallback();
    Motor_Roll.TIM_Process_PeriodElapsedCallback();
    // Pitch重力前馈: T_ff = C * cos(theta + phi)
    // 角度统一由编码器原始弧度换算(与FOLD分支无关), 避免Transform_Angle在折叠时是弧度导致补偿变成常数
    if((Get_Gimbal_Control_Type() != Gimbal_Control_Type_DISABLE) && (Get_Gimbal_Control_Type() != Gimbal_Control_Type_FOLD)){
        Pitch_Compensite_Output = test_c * cosf((Motor_Pitch.Get_Now_Angle() - 5.37f) + atan2f(9.718f, 58.285f));
        Motor_Pitch.Compensite_Out(Pitch_Compensite_Output);
    }
    else{
        Pitch_Compensite_Output = 0.0f;
        Motor_Pitch.Compensite_Out(0.0f);
    }

    // 补偿必须进入本周期控制帧；派生类已关闭基类自动发送，Pitch每周期仅发送一次。
    Motor_Pitch.TIM_Process_PeriodElapsedCallback();

}

/************************ COPYRIGHT(C) USTC-ROBOWALKER **************************/
