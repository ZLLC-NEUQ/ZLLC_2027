/**
 * @file crt_gimbal.h
 * @author cjw
 * @brief 云台
 * @version 0.1
 * @date 2025-07-1 0.1 26赛季定稿
 *
 * @copyright ZLLC 2026
 *
 */

#ifndef CRT_GIMBAL_H
#define CRT_GIMBAL_H

/* Includes ------------------------------------------------------------------*/

#include "dvc_djimotor.h"
#include "dvc_minipc.h"
#include "dvc_external_imu.h"
#include "dvc_imu.h"
#include "dvc_lkmotor.h"
#include "dvc_dmmotor.h"
#include "alg_LESO.h"

/* Exported macros -----------------------------------------------------------*/

/* Exported types ------------------------------------------------------------*/


//Yaw轴的编码器偏移值
#define YAW_ENCODER_OFFSET  3400
#define MAIN_YAW_ENCODER_OFFSET 2048
//大Yaw遥控回正
 #define MAIN_YAW_ALIGN_ENCODER_ANGLE  (-17.6058655f)
//编码器角与IMU角旋转方向是否一致: 实测回正时转向反了就改成 -1.0f
#define MAIN_YAW_IMU_ENCODER_SAME_DIR (+1.0f)
#define MAIN_YAW_FOLD_ANGLE (25.2799263f)

#define CRUISE_PITCH_SPEED 0.025 * 57.3f          //angle
#define CRUISE_YAW_SPEED   1.3f                 //rad
#define LIMIT_YAW_ANGLE    70.0f
#define LOCK_ROLL 0.0f  //得测
//#define ROLL_VERTICAL -2.6651f   //实测: 垂直姿态下Roll电机的绝对角(rad), 编码器未做零点校准前以此为准
//#define FOLD_ROLL -1.7349f //实测: 垂直->水平(折叠) 的Roll角增量, rad; 3.5097-2.4968, 方向为正
#define ROLL_VERTICAL 3.5555555555f   //实测: 垂直姿态下Roll电机的绝对角(rad), 编码器未做零点校准前以此为准 
#define FOLD_ROLL -1.67f //实测: 垂直->水平(折叠) 的Roll角增量, rad; 3.5097-2.4968, 方向为正
/**
 * @brief 云台控制类型
 *
 */
enum Enum_Gimbal_Control_Type :uint8_t
{
    Gimbal_Control_Type_DISABLE = 0,
    Gimbal_Control_Type_NORMAL,
    Gimbal_Control_Type_MINIPC,
    Gimbal_Control_Type_FOLD,
};

enum Enum_Motor_Yaw_Type :uint8_t
{
    Yaw_A = 0,
    Yaw_B,
};

struct IMU_Data
{
    float Pitch;
    float Roll;
    float Yaw;
    float Omega_X;
    float Omega_Y;
    float Omega_Z;
};

// 2026纯力矩模式在2027协议中复用MIT帧，Kp/Kd均置零。
static const Enum_DM_Motor_Control_Method DM_Motor_Control_Method_MIT_TORQUE =
    DM_Motor_Control_Method_MIT_OMEGA;

enum Enum_DM_Motor_Control_Alg : uint8_t
{
    DM_Motor_DISANLE = 0,
    DM_PID_Omega,
    DM_PID_Angle,
};

/**
 * @brief 保留2026云台双环、前馈和力矩语义的2027达妙适配类
 */
class Class_DM_Motor_J4310 : public Class_DM_Motor
{
public:
    void Init(FDCAN_HandleTypeDef *hcan, Enum_DM_Motor_ID __CAN_ID,
              Enum_DM_Motor_Control_Method __Control_Method,
              int32_t __Position_Offset, float __Omega_Max, float __Torque_Max);
    void Init(FDCAN_HandleTypeDef *hcan, Enum_DM_Motor_ID __CAN_ID,
              Enum_DM_Motor_Control_Method __Control_Method, float __Angle_Max);

    inline float Get_Transform_Angle() { return Transform_Angle; }
    inline float Get_Transform_Omega() { return Transform_Omega; }
    inline float Get_Output_Torque() { return Output_Torque; }

    inline void Set_Transform_Angle(float value) { Transform_Angle = value; }
    inline void Set_Transform_Omega(float value) { Transform_Omega = value; }
    inline void Set_Transform_Target_Vel(float value) { Transform_Target_Vel = value; }
    inline void Set_Transform_Target_Acc(float value) { Transform_Target_Acc = value; }
    inline void Set_Motor_Parameters(float __J, float __B, float __Mgl, float __C)
    {
        J = __J;
        B = __B;
        Mgl = __Mgl;
        C = __C;
    }

    void Set_DM_Motor_Control_Method(Enum_DM_Motor_Control_Method method);
    inline void Set_DM_Motor_Control_Alg(Enum_DM_Motor_Control_Alg alg)
    {
        DM_Motor_Control_Alg = alg;
    }

    void TIM_PID_PeriodElapsedCallback() override;
    void TIM_Process_PeriodElapsedCallback() override;
    void Compensite_Out(float Compensite_Value);

protected:
    void Data_Process(uint8_t *Rx_Data) override;

    Enum_DM_Motor_Control_Alg DM_Motor_Control_Alg = DM_PID_Omega;
    bool Torque_Only = true;
    float J = 0.0f;
    float B = 0.0f;
    float Mgl = 0.0f;
    float C = 0.0f;
    float Transform_Target_Vel = 0.0f;
    float Transform_Target_Acc = 0.0f;
    float Transform_Angle = 0.0f;
    float Transform_Omega = 0.0f;
    float Out = 0.0f;
    float Output_Angle = 0.0f;
    float Output_Omega = 0.0f;
    float Output_Torque = 0.0f;
};

/**
 * @brief Specialized, yaw轴电机类
 *
 */
class Class_Gimbal_Yaw_SMC_Entry
{
public:
    // 2026实际输出固定走PID；保留原SMC调参入口，不切换未验证的控制算法。
    void Init(float, float, float, float) {}
};

class Class_Gimbal_Yaw_Motor_GM6020 : public Class_DJI_Motor_GM6020
{
public:
    Class_Gimbal_Yaw_Motor_GM6020()
    {
        Zero_Position = 1.79245651f;
    }

    //陀螺仪获取云台角速度
    Class_IMU *IMU;
    Class_Gimbal_Yaw_SMC_Entry SMC_Control;

    inline float Get_Trer_Rad_Yaw();
    inline float Get_True_Gyro_Yaw();
    inline float Get_True_Angle_Yaw();
    inline float Get_Zero_Offset_Radian()
    {
        return Normalize_Angle_Radian_PI_to_PI(
            Data.Now_Radian + (float)Encoder_Offset * 2.0f * PI / 8192.0f - Zero_Position);
    }
    inline float Get_Zero_Offset_Angle()
    {
        return Get_Zero_Offset_Radian() * 180.0f / PI;
    }

    inline void Set_Transform_Target_Vel(float value) { Transform_Target_Vel = value; }
    inline void Set_Transform_Target_Acc(float value) { Transform_Target_Acc = value; }
    inline void Set_Motor_Parameters(float __J, float __B, float __Mgl, float __C)
    {
        J = __J;
        B = __B;
        Mgl = __Mgl;
        C = __C;
    }

    void Transform_Angle();
    void TIM_PID_PeriodElapsedCallback();
    void TIM_SMC_PeriodElapsedCallback();

protected:
    float True_Rad_Yaw = 0.0f;
    float True_Angle_Yaw = 0.0f;
    float True_Gyro_Yaw = 0.0f;
    float J = 0.0f;
    float B = 0.0f;
    float Mgl = 0.0f;
    float C = 0.0f;
    float Transform_Target_Vel = 0.0f;
    float Transform_Target_Acc = 0.0f;
};

float Class_Gimbal_Yaw_Motor_GM6020::Get_Trer_Rad_Yaw()
{
    return (True_Rad_Yaw);
} 

float Class_Gimbal_Yaw_Motor_GM6020::Get_True_Gyro_Yaw()
{
    return (True_Gyro_Yaw);
}

float Class_Gimbal_Yaw_Motor_GM6020::Get_True_Angle_Yaw()
{
    return (True_Angle_Yaw);
}

/**
 * @brief Specialized, pitch轴电机类
 *
 */
class Class_Gimbal_Pitch_Motor_GM6020 : public Class_DJI_Motor_GM6020
{
public:
    //陀螺仪获取云台角速度
    Class_IMU* IMU;


    inline float Get_True_Rad_Pitch();
    inline float Get_True_Gyro_Pitch();
    inline float Get_True_Angle_Pitch();

    void Transform_Angle();

    void TIM_PID_PeriodElapsedCallback();

protected:
    //初始化相关变量

    //常量
    

    // 重力补偿
    float Gravity_Compensate = 0.0f;

    //内部变量
    float True_Rad_Pitch = 0.0f;
    float True_Angle_Pitch = 0.0f;
    float True_Gyro_Pitch = 0.0f;
    //读变量

    //写变量

    //读写变量

    //内部函数
};

float Class_Gimbal_Pitch_Motor_GM6020::Get_True_Rad_Pitch()
{
    return (True_Rad_Pitch);
}

float Class_Gimbal_Pitch_Motor_GM6020::Get_True_Angle_Pitch()
{
    return (True_Angle_Pitch);
}

float Class_Gimbal_Pitch_Motor_GM6020::Get_True_Gyro_Pitch()
{
    return (True_Gyro_Pitch);
}


/**
 * @brief Specialized, pitch轴电机类
 *
 */
class Class_Gimbal_Pitch_Motor_LK6010 : public Class_LK_Motor
{
public:
    //陀螺仪获取云台角速度
    Class_IMU* IMU;
    
    inline float Get_True_Rad_Pitch();
    inline float Get_True_Gyro_Pitch();
    inline float Get_True_Angle_Pitch();

    void Transform_Angle();

    void TIM_PID_PeriodElapsedCallback();

protected:
    //初始化相关变量

    //常量

    // 重力补偿
    float Gravity_Compensate = 0.0f;

    //内部变量 
    float True_Rad_Pitch = 0.0f;
    float True_Angle_Pitch = 0.0f;
    float True_Gyro_Pitch = 0.0f;
    //读变量

    //写变量

    //读写变量

    //内部函数
};

float Class_Gimbal_Pitch_Motor_LK6010::Get_True_Rad_Pitch()
{
    return (True_Rad_Pitch);
}

float Class_Gimbal_Pitch_Motor_LK6010::Get_True_Angle_Pitch()
{
    return (True_Angle_Pitch);
}

float Class_Gimbal_Pitch_Motor_LK6010::Get_True_Gyro_Pitch()
{
    return (True_Gyro_Pitch);

}


/**
 * @brief Specialized, roll轴电机类
 * 
 */
class Class_Gimbal_Roll_Motor_DM4310 : public Class_DM_Motor_J4310
{
    public:
    //陀螺仪获取云台角速度
    Class_IMU* IMU;


    inline float Get_True_Rad_Roll();
    inline float Get_True_Gyro_Roll();
    inline float Get_True_Angle_Roll();

    void Transform_Angle();

    void TIM_PID_PeriodElapsedCallback();

protected:
    //初始化相关变量

    //常量
    

    // 重力补偿
    float Gravity_Compensate = 0.0f;

    //内部变量
    float True_Rad_Roll = 0.0f;
    float True_Angle_Roll = 0.0f;
    float True_Gyro_Roll = 0.0f;
    //读变量

    //写变量

    //读写变量

    //内部函数

};
float Class_Gimbal_Roll_Motor_DM4310::Get_True_Rad_Roll()
{
    return (True_Rad_Roll);
}

float Class_Gimbal_Roll_Motor_DM4310::Get_True_Angle_Roll()
{
    return (True_Angle_Roll);
}

float Class_Gimbal_Roll_Motor_DM4310::Get_True_Gyro_Roll()
{
    return (True_Gyro_Roll);
}

class Class_Gimbal_Main_Yaw_Motor_LK : public Class_LK_Motor
{
public:
    float Get_Transform_Angle() const { return Transform_Angle; }
    float Get_Transform_Omega() const { return Transform_Omega; }
    void Set_Transform_Omega(float omega) { Transform_Omega = omega; }
    void Set_Transform_Angle(float angle) { Transform_Angle = angle; }
    void TIM_Process_PeriodElapsedCallback();

private:
    float Transform_Angle = 0.0f;
    float Transform_Omega = 0.0f;
};

/**
 * @brief Specialized, 云台类
 *
 */
class Class_Gimbal
{
public:

    //imu对象
    Class_IMU Boardc_BMI;

    Class_External_IMU External_IMU;

    Class_MiniPC *MiniPC;

    /*后期yaw pitch这两个类要换成其父类，大疆电机类*/

    // LK电机 大Yaw
    Class_Gimbal_Main_Yaw_Motor_LK Motor_Main_Yaw;

    // yaw轴电机
    Class_Gimbal_Yaw_Motor_GM6020 Motor_Yaw;

    // pitch轴电机
    Class_DM_Motor_J4310 Motor_Pitch;
    // Class_DJI_Motor_GM6020 Motor_Pitch;

    //roll轴电机
    Class_DM_Motor_J4310 Motor_Roll;

    Class_LESO Motor_Pitch_LESO;

    Class_Filter_Kalman External_IMU_Gyro_Yaw;
    Class_Filter_Kalman External_IMU_Gyro_Pitch;
    Class_Filter_Kalman Motor_Yaw_Angle_Filter;

    uint8_t unfold_pending = 0;  // Fold_Cmd==0 时由 ita_chariot 置 1, MINIPC 分支第一帧消费, 把三轴拉回正前方

    void Init();

    inline float Get_Target_Main_Yaw_Angle();
    inline float Get_Target_Pitch_Angle();
    inline float Get_Target_Roll_Angle();
    inline Enum_Gimbal_Control_Type Get_Gimbal_Control_Type();
    inline int Get_last_Cruise_Mode();

    inline void Set_Gimbal_Control_Type(Enum_Gimbal_Control_Type __Gimbal_Control_Type);
    inline void Set_Target_Main_Yaw_Angle(float __Target_Yaw_Angle);
    inline void Set_Target_Pitch_Angle(float __Target_Pitch_Angle);
    inline void Set_Target_Roll_Angle(float __Target_Roll_Angle);
    inline void Set_last_Cruise_Mode(int __last_mode_for_cruise);

    void TIM_Calculate_PeriodElapsedCallback();

    uint32_t Single_time = 0;
protected:
    //初始化相关常量
    float Gimbal_Head_Angle;
    //常量
    float CRUISE_SPEED_YAW = 100.f;
    float CRUISE_SPEED_PITCH = 70.f;
    // yaw轴最小值
    float Min_Yaw_Angle = - 180.0f;
    // yaw轴最大值
    float Max_Yaw_Angle = 180.0f;

    //yaw总角度
    float Yaw_Total_Angle;
    float Yaw_Half_Turns;

    // pitch轴最小值
    float Min_Pitch_Angle = -18.0f;
    // pitch轴最大值
    float Max_Pitch_Angle = 18.0f ; //多10°
    // roll轴最小值
    float Min_Roll_Angle = ROLL_VERTICAL + FOLD_ROLL;
    // roll轴最大值
    float Max_Roll_Angle = ROLL_VERTICAL;
    // 云台折叠角度
    float Fold_Roll_angle = FOLD_ROLL;

    float Yaw_Compensite_KF = 150.0f;
    float Yaw_Compensite_Output = 0.0f;
    float Pitch_Compensite_Output = 0.0f;

    //调试观测量
    float Debug_Pitch_Encoder_Angle = 0.0f;         //编码器换算的Pitch角度(°)
    float Debug_Pitch_External_IMU_Angle = 0.0f;    //外置IMU的Pitch角度(°)
    float Debug_Pitch_Encoder_Omega = 0.0f;         //编码器换算的Pitch角速度(°/s)
    float Debug_External_IMU_Gyro_Pitch = 0.0f;     //外置IMU的Pitch陀螺仪角速度(°/s)
    float Debug_Pitch_Omega_Feedback = 0.0f;        //Pitch速度环实际使用的反馈(°/s)
    float Debug_Pitch_Out_Torque = 0.0f;            //Pitch输出力矩(N·m)

    //内部变量 
    float Pitch_Omega_LPF = 0.0f;                   //Pitch速度反馈的一阶低通状态(rad/s)

    //读变量
// ---------- 巡航正弦参数（常量）----------
    static constexpr float YAW_AMPLITUDE   = 60.0f;   // 振幅，范围 [-60, 60]
    static constexpr float YAW_OFFSET      = 0.0f;    // 偏置
    static constexpr float YAW_FREQ        = 0.3f;    // 频率 (Hz)

    static constexpr float PITCH_AMPLITUDE = 18.0f;   // 振幅，范围 [-25, 22]
    static constexpr float PITCH_OFFSET    = -0.0f;   // 偏置
    static constexpr float PITCH_FREQ      = 1.5f;    // 频率 (Hz)

    // ---------- 巡航状态变量 ----------
    float phi0_yaw          = 0.0f;   // Yaw 初始相位（弧度）
    float phi0_pitch        = 0.0f;   // Pitch 初始相位（弧度）
    uint32_t cruise_start_time = 0;   // 进入巡航时刻的时间戳
    int last_mode_for_cruise  = -1;   // 上一次上位机 mode 值

    //写变量

    //云台状态
    Enum_Gimbal_Control_Type Gimbal_Control_Type = Gimbal_Control_Type_DISABLE ;

    //读写变量

    // Main_Yaw轴角度
    float Target_Main_Yaw_Angle = 0.0f;

    // 小Yaw目标角度
    float Target_Yaw_Angle = 0.0f;

    // pitch轴角度
    float Target_Pitch_Angle = 0.0f;

    //roll轴角度
    float Target_Roll_Angle = 0.0f;


    //内部函数

    void Output();
    void Gimbal_Disable();
};

/* Exported variables --------------------------------------------------------*/

/* Exported function declarations --------------------------------------------*/


/**
 * @brief 获取yaw轴角度
 *
 * @return float yaw轴角度
 */
float Class_Gimbal::Get_Target_Main_Yaw_Angle()
{
    return (Target_Main_Yaw_Angle);
}
/**
 * @brief 获取yaw轴角度
 *
 * @return float yaw轴角度
 */
float Class_Gimbal::Get_Target_Pitch_Angle()
{
    return (Target_Pitch_Angle);
}
/**
 * @brief 获取上一次巡航模式
 *
 * @return int 上一次巡航模式
 */
int Class_Gimbal::Get_last_Cruise_Mode()
{
    return (last_mode_for_cruise);
}
/**
 * @brief 获取云台控制类型
 *
 * @return Enum_Gimbal_Control_Type 获取云台控制类型
 */
Enum_Gimbal_Control_Type Class_Gimbal::Get_Gimbal_Control_Type()
{
    return (Gimbal_Control_Type);
}

/**
 * @brief 设定云台状态
 *
 * @param __Gimbal_Control_Type 云台状态
 */
void Class_Gimbal::Set_Gimbal_Control_Type(Enum_Gimbal_Control_Type __Gimbal_Control_Type)
{
    Gimbal_Control_Type = __Gimbal_Control_Type;
}
/**
 * @brief 设定yaw轴角度
 *
 */
void Class_Gimbal::Set_Target_Main_Yaw_Angle(float __Target_Main_Yaw_Angle)
{
    Target_Main_Yaw_Angle = __Target_Main_Yaw_Angle;
}
/**
 * @brief 设定pitch轴角度
 *
 */
void Class_Gimbal::Set_Target_Pitch_Angle(float __Target_Pitch_Angle)
{
    Target_Pitch_Angle = __Target_Pitch_Angle;
}
/**
 * @brief 设定巡航标志位
 *
 */void Class_Gimbal::Set_last_Cruise_Mode(int __last_mode_for_cruise)
{    
    last_mode_for_cruise = __last_mode_for_cruise;
}

/**
 * @brief 获取roll轴角度
 * 
 */
float Class_Gimbal::Get_Target_Roll_Angle()
{
    return (Target_Roll_Angle);
}
/**
 * @brief 设定roll轴角度
 * 
 */
void Class_Gimbal::Set_Target_Roll_Angle(float __Target_Roll_Angle)
{
    Target_Roll_Angle = __Target_Roll_Angle;
}

#endif

/************************ COPYRIGHT(C) USTC-ROBOWALKER **************************/
