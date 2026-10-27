/**
 * @file dvc_imu.h
 * @author cjw by wanghongxi
 * @brief imu
 * @version 0.1
 * @date 2025-07-1 0.1 26赛季定稿
 *
 * @copyright ZLLC 2026
 *
 */

#ifndef DVC_IMU_H
#define DVC_IMU_H

#include "main.h"
#include "dvc_dwt.h"
#include "dvc_boardc_bmi088.h"
//#include "dvc_boardc_ist8310.h"
#include "alg_MahonyAHRS.h"
#include "alg_pid.h"
#include "ekf_tcm.h"

//----------------------------------------------------------------------------------------------------

typedef struct
{
    float q[4]; // 四元数估计值

    float Gyro[3];  // 角速度
    float Accel[3]; // 加速度
    float MotionAccel_b[3]; // 机体坐标加速度
    float MotionAccel_n[3]; // 绝对系加速度

    float AccelLPF; // 加速度低通滤波系数

    // 加速度在绝对系的向量表示
    float xn[3];
    float yn[3];
    float zn[3];

    float atanxz;
    float atanyz;

    // 位姿
    float Roll;
    float Pitch;
    float Yaw;
    float YawTotalAngle;
} INS_t;

// IMU 任务完成一轮解算后，通过队列交给控制任务的完整样本。
typedef struct
{
    INS_t data;
    uint32_t sequence;
    uint32_t timestamp_ms;
    uint8_t started;
} Struct_IMU_Snapshot;

enum Enum_IMU_Status 
{
    IMU_Status_DISABLE = 0,
    IMU_Status_ENABLE,
};

// Variable declaration

class Class_IMU
{
    public:

    void Init(void);
    // 仅 IMU 任务调用解算和导出，控制任务调用应用快照和在线检测。
    EKF_ITCM_CODE void TIM_Calculate_PeriodElapsedCallback(void);
    void Export_Snapshot(Struct_IMU_Snapshot &snapshot) const;
    void Apply_Snapshot(const Struct_IMU_Snapshot &snapshot);
    void TIM1msMod50_Alive_PeriodElapsedCallback(void);

    float Get_Angle_Roll(void);
    float Get_Angle_Pitch(void);
    float Get_Angle_Yaw(void);

    float Get_Rad_Roll(void);
    float Get_Rad_Pitch(void);
    float Get_Rad_Yaw(void);

    float Get_Accel_X(void);
    float Get_Accel_Y(void);
    float Get_Accel_Z(void);

    float Get_Accel_X_n();
    float Get_Accel_Y_n();
    float Get_Accel_Z_n();

    float Get_Accel_X_b();
    float Get_Accel_Y_b();
    float Get_Accel_Z_b();

    float *Get_Quaternion(void);


    float Get_Gyro_Roll(void);
    float Get_Gyro_Pitch(void);
    float Get_Gyro_Yaw(void);

    Enum_IMU_Status Get_IMU_Status(void);

    protected:

    Class_MahonyAHRS IMU_MahonyAHRS;
    Class_BoardC_BMI IMU_BMI088;
    //Class_BoardC_IST8310 IMU_IST8310;
    Class_PID PID_IMU_Tempture;
    
    // 控制任务独占：业务 getter 和在线检测只访问这个副本。
    INS_t Published_INS = {};
    uint32_t Published_Sequence = 0;
    uint32_t Published_Timestamp_ms = 0;
    uint8_t Published_Started = 0;
    float Pre_Published_Accel[3] = {0.0f, 0.0f, 0.0f};
    Enum_IMU_Status IMU_Status = IMU_Status_DISABLE;

    EKF_ITCM_CODE void Get_Angle(void);
    void TIM_Set_PWM(TIM_HandleTypeDef *tim_pwmHandle, uint8_t Channel, uint16_t value);

    // IMU 任务独占：解算工作区、原始数据和采样信息。
    INS_t INS;
    QEKF_INS_t QEKF_INS;
    IMU_Data_t BMI088_Raw_Data;
    uint32_t Producer_Sequence = 0;
    uint32_t Producer_Timestamp_ms = 0;
    //IST8310_Real_Data_t IST8310_Real_Data;

    const float X_b[3] = {1, 0, 0};
    const float Y_b[3] = {0, 1, 0};
    const float Z_b[3] = {0, 0, 1};

    float INS_Quat[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    float INS_Rad[3] = {0.0f, 0.0f, 0.0f};      //euler angle, unit rad.欧拉角 单位 rad
    float INS_Angle[3] = {0.0f, 0.0f, 0.0f};

    //重力加速度
	const float Gravity[3] = {0, 0, 9.81f};
  
  float Target_Tempreture = 40.;

    //DWT计数
    float INS_DWT_Dt;
    float INS_DWT_Dt_Sum;
    uint32_t INS_DWT_Count;

    uint8_t gyro_update_flag = 0;
    uint8_t accel_update_flag = 0;
    uint8_t accel_temp_update_flag = 0;
    uint8_t mag_update_flag = 0;
    uint8_t imu_start_flag = 0;    

    EKF_ITCM_CODE void BodyFrameToEarthFrame(const float *vecBF, float *vecEF, float *q);
    EKF_ITCM_CODE void EarthFrameToBodyFrame(const float *vecEF, float *vecBF, float *q);

    float invSqrt(float x);
};

//---------------------------------------------------------------------------------------------------
// Function declarations



#endif
//=====================================================================================================
// End of file
//=====================================================================================================
