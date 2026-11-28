/**
 * @file dvc_imu.cpp
 * @author HGCSJ
 * @brief imu
 * @version V1.1
 * @date 2026-10-30 27赛季定稿
 *
 * @copyright ZLLC 2027
 *
 */

#include "dvc_imu.h"
#include "ekf_tcm.h"
#include "dvc_performance.h"
#if IMU_USE_VQF && VQF_USE_TCM
#include <new>
// 原始零初始化存储由既有启动代码清零；Init 中原位构造，无堆分配。
// 只迁入算法对象，不将 BMI088/SPI/DMA 缓冲及整个 chariot 放入 DTCM。
static VQF_DTCM_DATA unsigned char VQF_TCM_Storage[sizeof(Class_VQF)];
static const Class_IMU *VQF_TCM_Owner = NULL;
static_assert(alignof(Class_VQF) <= 8, "VQF DTCM storage alignment");
#endif

float offsety = 0.0022f;
void Class_IMU::Init()
{
    // 调度器启动前初始化；运行后计算态和消费态由不同任务分别持有。
    Published_INS = {};
    Published_Sequence = 0;
    Published_Timestamp_ms = 0;
    Published_Started = 0;
    memset(Pre_Published_Accel, 0, sizeof(Pre_Published_Accel));
    IMU_Status = IMU_Status_DISABLE;
    Producer_Sequence = 0;
    Producer_Timestamp_ms = 0;
    imu_start_flag = 0;
    INS = {};
    INS.q[0] = 1.0f;

    // 初始化BMI088传感器，计算零漂 并检查初始化是否成功
    IMU_BMI088.init(&hspi2,&BMI088_Raw_Data);
    HAL_Delay(100);

    // 初始化IST8310传感器
    //  IMU_IST8310.init(&hi2c3);
    //  HAL_Delay(100);

    // 初始化MahonyAHRS算法，并传入初始四元数
    IMU_MahonyAHRS.init(INS_Quat);
 
#if IMU_USE_VQF
#if VQF_USE_TCM
    // Init 仅在调度器启动前调用。重复 Init 不重新构造已绑定的对象。
    if (VQF_TCM_Owner == NULL)
    {
        IMU_VQF = new (VQF_TCM_Storage) Class_VQF;
        VQF_TCM_Owner = this;
    }
    else if (VQF_TCM_Owner == this)
    {
        IMU_VQF = reinterpret_cast<Class_VQF *>(VQF_TCM_Storage);
    }
#endif
    // 加速度低通时间常数 3 s；仅启用静止零偏估计。
    Struct_VQF_Parameter parameter;
    parameter.Tau_Acc = 3.0f;
    parameter.Motion_Bias_Est_Enabled = false;
    parameter.Rest_Bias_Est_Enabled = true;
    if (!IMU_VQF->Init(parameter, 0.001f))
        return;
    IMU_VQF->Get_Quaternion(INS.q);
#else
    // 原 EKF 参数及动态周期接口保留。
    IMU_QuaternionEKF_Init(10, 0.001, 10000000, 0.9996, 0.05, offsety, &QEKF_INS);
#endif

    INS.AccelLPF = 0.05f;

    //初始化温控pid参数 积分和输出限幅是一周期满占空比的计数240M/24/10000=1000
    PID_IMU_Tempture.Init(200, 300, 0, 0.0, 250, 500);
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_4);

    // 不把传感器初始化/延时计入首帧运动加速度滤波周期。
    INS_DWT_Count = DWT->CYCCNT;

}

IMU_ITCM_CODE void Class_IMU::TIM_Calculate_PeriodElapsedCallback(void)
{
    static uint8_t Tempture_Cnt_mod50 = 0;
    Tempture_Cnt_mod50++;

    INS_DWT_Dt = DWT_GetDeltaT(&INS_DWT_Count);

    IMU_BMI088.BMI088_Read(&BMI088_Raw_Data);

#if IMU_USE_VQF
    // 系数固定为 1 ms；DWT 实测间隔仍用于原有运动加速度滤波及调试。
    const bool updated = IMU_VQF->Update(BMI088_Raw_Data.Gyro, BMI088_Raw_Data.Accel);
    if (!updated)
        return; // 无效输入不发布新的成功样本序号。
    IMU_VQF->Get_Quaternion(INS.q);
#else
    PERF_START(EKF_Timing);
    // 核心函数,EKF更新四元数
    IMU_QuaternionEKF_Update(BMI088_Raw_Data.Gyro[0], BMI088_Raw_Data.Gyro[1], BMI088_Raw_Data.Gyro[2],
                             BMI088_Raw_Data.Accel[0], BMI088_Raw_Data.Accel[1], BMI088_Raw_Data.Accel[2], INS_DWT_Dt, &QEKF_INS);
    PERF_STOP(EKF_Timing);

    memcpy(INS.q, QEKF_INS.q, sizeof(QEKF_INS.q));
#endif
    memcpy(INS.Accel, BMI088_Raw_Data.Accel, sizeof(INS.Accel));
    memcpy(INS.Gyro, BMI088_Raw_Data.Gyro, sizeof(INS.Gyro));

    // 机体系基向量转换到导航坐标系，本例选取惯性系为导航系
    BodyFrameToEarthFrame(X_b, INS.xn, INS.q);
    BodyFrameToEarthFrame(Y_b, INS.yn, INS.q);
    BodyFrameToEarthFrame(Z_b, INS.zn, INS.q);

    // 将重力从导航坐标系n转换到机体系b,随后根据加速度计数据计算运动加速度
    
    float tmp_gravity_b[3];
    EarthFrameToBodyFrame(Gravity, tmp_gravity_b, INS.q);
    for (uint8_t i = 0; i < 3; i++) // 同样过一个低通滤波
    {
        INS.MotionAccel_b[i] = (INS.Accel[i] - tmp_gravity_b[i]) * INS_DWT_Dt / (INS.AccelLPF + INS_DWT_Dt) + INS.MotionAccel_b[i] * INS.AccelLPF / (INS.AccelLPF + INS_DWT_Dt);
    }
    BodyFrameToEarthFrame(INS.MotionAccel_b, INS.MotionAccel_n, INS.q); // 转换回导航系n

    // 获取最终数据
    Get_Angle();

    if(Tempture_Cnt_mod50 % 50 == 0)
    {
        PID_IMU_Tempture.Set_Now(BMI088_Raw_Data.Temperature);
        PID_IMU_Tempture.Set_Target(40.);
        PID_IMU_Tempture.TIM_Adjust_PeriodElapsedCallback();
        if(PID_IMU_Tempture.Get_Out() <= 0)TIM_Set_PWM(&htim3, TIM_CHANNEL_4, 0);
        else
        TIM_Set_PWM(&htim3, TIM_CHANNEL_4, (uint16_t)PID_IMU_Tempture.Get_Out());
    }

    imu_start_flag = 1;
    Producer_Sequence++;
    Producer_Timestamp_ms = HAL_GetTick();
}

void Class_IMU::Export_Snapshot(Struct_IMU_Snapshot &snapshot) const
{
    snapshot.data = INS;
    snapshot.sequence = Producer_Sequence;
    snapshot.timestamp_ms = Producer_Timestamp_ms;
    snapshot.started = imu_start_flag;
}

void Class_IMU::Apply_Snapshot(const Struct_IMU_Snapshot &snapshot)
{
    Published_INS = snapshot.data;
    Published_Sequence = snapshot.sequence;
    Published_Timestamp_ms = snapshot.timestamp_ms;
    Published_Started = snapshot.started;
}

void Class_IMU::TIM1msMod50_Alive_PeriodElapsedCallback(void)
{
    if(Published_Started)  //控制任务已经收到完整的 IMU 样本
    {
        if((Pre_Published_Accel[0] == Published_INS.Accel[0])
        && (Pre_Published_Accel[1] == Published_INS.Accel[1])
        && (Pre_Published_Accel[2] == Published_INS.Accel[2]))  //判断陀螺仪是否掉线
        {
            IMU_Status = IMU_Status_DISABLE;
        }
        else IMU_Status = IMU_Status_ENABLE;

        memcpy(Pre_Published_Accel, Published_INS.Accel, sizeof(Pre_Published_Accel));
    }
}

float accel = 0.00035f;
uint64_t cnt =0;
IMU_ITCM_CODE void Class_IMU::Get_Angle()
{
    cnt ++;
    // 获取最终数据
    //INS.Yaw = QEKF_INS.Yaw - accel * cnt;
#if IMU_USE_VQF
    INS.Yaw = IMU_VQF->Get_Angle_Yaw();
    INS.Pitch = IMU_VQF->Get_Angle_Pitch();
    INS.Roll = IMU_VQF->Get_Angle_Roll();
    INS.YawTotalAngle = IMU_VQF->Get_Yaw_Total_Angle();
#else
    INS.Yaw = QEKF_INS.Yaw;
    INS.Pitch = QEKF_INS.Pitch;
    INS.Roll = QEKF_INS.Roll;
    INS.YawTotalAngle = QEKF_INS.YawTotalAngle;
#endif

    memcpy(INS_Quat, INS.q, sizeof(INS_Quat));

    INS_Rad[0] = atan2f(2.0f*(INS_Quat[0]*INS_Quat[3]+INS_Quat[1]*INS_Quat[2]), 2.0f*(INS_Quat[0]*INS_Quat[0]+INS_Quat[1]*INS_Quat[1])-1.0f);
    INS_Rad[1] = asinf(-2.0f*(INS_Quat[1]*INS_Quat[3]-INS_Quat[0]*INS_Quat[2]));
    INS_Rad[2] = atan2f(2.0f*(INS_Quat[0]*INS_Quat[1]+INS_Quat[2]*INS_Quat[3]),2.0f*(INS_Quat[0]*INS_Quat[0]+INS_Quat[3]*INS_Quat[3])-1.0f);
    for(int i=0;i<3;i++){
        INS_Angle[i] = INS_Rad[i] * 180.0f / 3.1415926f;
    }
}

void Class_IMU::TIM_Set_PWM(TIM_HandleTypeDef *tim_pwmHandle, uint8_t Channel, uint16_t value)
{
    if (value > tim_pwmHandle->Instance->ARR)
        value = tim_pwmHandle->Instance->ARR;
    if(value < 0) value = 0;

    switch (Channel)
    {
    case TIM_CHANNEL_1:
        tim_pwmHandle->Instance->CCR1 = value;
        break;
    case TIM_CHANNEL_2:
        tim_pwmHandle->Instance->CCR2 = value;
        break;
    case TIM_CHANNEL_3:
        tim_pwmHandle->Instance->CCR3 = value;
        break;
    case TIM_CHANNEL_4:
        tim_pwmHandle->Instance->CCR4 = value;
        break;
    }
}

float Class_IMU::Get_Angle_Roll(void)
{
    return (Published_INS.Roll);
}

float Class_IMU::Get_Angle_Pitch(void)
{
    return (Published_INS.Pitch);
}

float Class_IMU::Get_Angle_Yaw(void)
{
    return (Published_INS.Yaw);
}

float Class_IMU::Get_Accel_X(void)
{
    return (Published_INS.Accel[0]);
}

float Class_IMU::Get_Accel_Y(void)
{
    return (Published_INS.Accel[1]);
}

float Class_IMU::Get_Accel_Z(void)
{
    return (Published_INS.Accel[2]);
}

float Class_IMU::Get_Accel_X_n()
{
  return (Published_INS.MotionAccel_n[0]);
}

float Class_IMU::Get_Accel_Y_n()
{
  return (Published_INS.MotionAccel_n[1]);
}

float Class_IMU::Get_Accel_Z_n()
{
  return (Published_INS.MotionAccel_n[2]);
}

float Class_IMU::Get_Accel_X_b()
{
  return (Published_INS.MotionAccel_b[0]);
}

float Class_IMU::Get_Accel_Y_b()
{
  return (Published_INS.MotionAccel_b[1]);
}

float Class_IMU::Get_Accel_Z_b()
{
  return (Published_INS.MotionAccel_b[2]);
}

float* Class_IMU::Get_Quaternion(void)
{
  return Published_INS.q;
}

float Class_IMU::Get_Gyro_Roll(void)
{
    return (Published_INS.Gyro[1]);
}

float Class_IMU::Get_Gyro_Pitch(void)
{
    return (Published_INS.Gyro[0]);
}

float Class_IMU::Get_Gyro_Yaw(void)
{
    return (Published_INS.Gyro[2]);
}

float Class_IMU::Get_Rad_Roll(void)
{
    return (Published_INS.Roll/180.f*PI);
}

float Class_IMU::Get_Rad_Pitch(void)
{
    return (Published_INS.Pitch/180.f*PI);
}

float Class_IMU::Get_Rad_Yaw(void)
{
    return (Published_INS.Yaw/180.f*PI);
}

Enum_IMU_Status Class_IMU::Get_IMU_Status(void)
{
    return IMU_Status;
}

/**
 * @brief          Transform 3dvector from BodyFrame to EarthFrame
 * @param[1]       vector in BodyFrame
 * @param[2]       vector in EarthFrame
 * @param[3]       quaternion
 */
IMU_ITCM_CODE void Class_IMU::BodyFrameToEarthFrame(const float *vecBF, float *vecEF, float *q)
{
    vecEF[0] = 2.0f * ((0.5f - q[2] * q[2] - q[3] * q[3]) * vecBF[0] +
                       (q[1] * q[2] - q[0] * q[3]) * vecBF[1] +
                       (q[1] * q[3] + q[0] * q[2]) * vecBF[2]);

    vecEF[1] = 2.0f * ((q[1] * q[2] + q[0] * q[3]) * vecBF[0] +
                       (0.5f - q[1] * q[1] - q[3] * q[3]) * vecBF[1] +
                       (q[2] * q[3] - q[0] * q[1]) * vecBF[2]);

    vecEF[2] = 2.0f * ((q[1] * q[3] - q[0] * q[2]) * vecBF[0] +
                       (q[2] * q[3] + q[0] * q[1]) * vecBF[1] +
                       (0.5f - q[1] * q[1] - q[2] * q[2]) * vecBF[2]);
}

/**
 * @brief          Transform 3dvector from EarthFrame to BodyFrame
 * @param[1]       vector in EarthFrame
 * @param[2]       vector in BodyFrame
 * @param[3]       quaternion
 */
IMU_ITCM_CODE void Class_IMU::EarthFrameToBodyFrame(const float *vecEF, float *vecBF, float *q)
{
    vecBF[0] = 2.0f * ((0.5f - q[2] * q[2] - q[3] * q[3]) * vecEF[0] +
                       (q[1] * q[2] + q[0] * q[3]) * vecEF[1] +
                       (q[1] * q[3] - q[0] * q[2]) * vecEF[2]);

    vecBF[1] = 2.0f * ((q[1] * q[2] - q[0] * q[3]) * vecEF[0] +
                       (0.5f - q[1] * q[1] - q[3] * q[3]) * vecEF[1] +
                       (q[2] * q[3] + q[0] * q[1]) * vecEF[2]);

    vecBF[2] = 2.0f * ((q[1] * q[3] + q[0] * q[2]) * vecEF[0] +
                       (q[2] * q[3] - q[0] * q[1]) * vecEF[1] +
                       (0.5f - q[1] * q[1] - q[2] * q[2]) * vecEF[2]);
}

//====================================================================================================
// END OF CODE
//====================================================================================================

