/**
 * @file alg_VQF.h
 * @brief BMI088六轴VQF姿态解算（含运动/静止陀螺仪零偏估计）
 * @version 0.2
 * @date 2026-11-09
 *
 * 六轴核心移植自 Daniel Laidig 的 VQF，原始许可及版本见 Docs/VQF_FreeRTOS.md。
 * SPDX-FileCopyrightText: 2021 Daniel Laidig <laidig@control.tu-berlin.de>
 * SPDX-License-Identifier: MIT
 */

#ifndef ALG_VQF_H
#define ALG_VQF_H

/* Includes ------------------------------------------------------------------*/

#include <stddef.h>
#include "vqf_tcm.h"

/* Exported types ------------------------------------------------------------*/

/**
 * @brief 与采样率无关的VQF参数；除独立启动时间外，默认值沿用官方实现
 */
struct Struct_VQF_Parameter
{
    float Tau_Acc = 3.0f;                       // 惯性系加速度二阶低通时间常数，s
    float Filter_Init_Time = 0.5f;              // 启动均值时间上限，s；0为首帧初始化，-1沿用各滤波时间常数
    bool Motion_Bias_Est_Enabled = true;        // 运动零偏估计
    bool Rest_Bias_Est_Enabled = true;          // 静止检测及静止零偏估计
    float Bias_Sigma_Init = 0.5f;               // 初始零偏标准差，°/s
    float Bias_Forgetting_Time = 100.0f;        // 零偏协方差增长时间尺度，s
    float Bias_Clip = 2.0f;                     // 各轴零偏估计及新息限幅，°/s
    float Bias_Sigma_Motion = 0.1f;             // 运动零偏标准差，°/s
    float Bias_Vertical_Forgetting_Factor = 0.0001f; // 竖直方向弱约束权重比例，无量纲
    float Bias_Sigma_Rest = 0.03f;              // 静止零偏标准差，°/s
    float Rest_Min_T = 1.5f;                    // 连续满足静止条件的最短时间，s
    float Rest_Filter_Tau = 0.5f;               // 静止检测二阶低通时间常数，s
    float Rest_Th_Gyro = 2.0f;                  // 角速度偏离低通参考值的模长阈值，°/s
    float Rest_Th_Accel = 0.5f;                 // 加速度偏离低通参考值的模长阈值，m/s²
};

/**
 * @brief Reusable，六轴VQF；每个对象独立保存状态，无动态内存及HAL依赖
 *
 * 输入为同一右手机体系的Gyro[3]（rad/s）、Accel[3]（m/s²）。静止水平时
 * Accel应指向+Z。四元数为[w,x,y,z]，将机体系向量主动旋转到Z向上的导航系。
 * 无磁力计，Yaw为相对航向，不能消除长期漂移。
 * 必须先Init；随后每隔D_T秒输入一组新数据。修改周期或参数需重新Init。
 */
class Class_VQF
{
public:
    // 固定周期D_T支持[0.000001,0.1] s；初始化失败不改变已有状态。
    bool Init(float __D_T = 0.001f, float __Tau_Acc = 3.0f);
    bool Init(const Struct_VQF_Parameter &__Parameter, float __D_T = 0.001f);
    void Reset();

    VQF_ITCM_CODE bool Update(const float __Gyro[3], const float __Accel[3]);
    VQF_ITCM_CODE bool Update(float __Gyro_X, float __Gyro_Y, float __Gyro_Z,
                float __Accel_X, float __Accel_Y, float __Accel_Z);

    // 复制输出，不暴露内部可写状态；空指针不操作
    VQF_ITCM_CODE void Get_Quaternion(float __Quaternion[4]) const;
    VQF_ITCM_CODE void Get_Gyro_Bias(float __Gyro_Bias[3]) const; // 输入数据中的剩余零偏，rad/s
    VQF_ITCM_CODE float Get_Bias_Sigma() const;                // 零偏不确定度上界，rad/s
    inline bool Get_Rest_Detected() const { return (State.Rest_Detected); }
    inline bool Get_Initialized() const { return (Initialized); }
    VQF_ITCM_CODE bool Get_Accel_Filter_Ready() const;         // 加速度低通已结束启动均值；不代表温度或零偏已稳定
    inline float Get_D_T() const { return (D_T); } // s

    // 沿用QuaternionEKF / Class_IMU命名：Pitch绕X、Roll绕Y、Yaw绕Z
    inline float Get_Angle_Roll() const { return (Roll); }   // °，[-90,90]
    inline float Get_Angle_Pitch() const { return (Pitch); } // °，[-180,180]
    inline float Get_Angle_Yaw() const { return (Yaw); }     // °，[-180,180]
    inline float Get_Rad_Roll() const { return (Roll * 0.017453292519943295f); }
    inline float Get_Rad_Pitch() const { return (Pitch * 0.017453292519943295f); }
    inline float Get_Rad_Yaw() const { return (Yaw * 0.017453292519943295f); }
    inline float Get_Yaw_Total_Angle() const { return (Yaw_Total_Angle); } // °，连续展开

protected:
    struct Struct_VQF_State
    {
        float Gyro_Quat[4];                     // 机体系到陀螺积分惯性系
        float Accel_Quat[4];                    // 惯性系到倾角修正导航系
        bool Rest_Detected;
        float Last_Accel_Lp[3];                 // 惯性系低通加速度，m/s²
        double Accel_Lp_State[6];
        float Bias[3];                          // 剩余陀螺仪零偏，rad/s
        float Bias_P[9];                        // 行主序协方差，单位(0.01°/s)²
        double Motion_R_Lp_State[18];           // 旋转矩阵二阶低通状态
        double Motion_Bias_Lp_State[4];         // 旋转后零偏二阶低通状态
        float Rest_Squared_Deviations[2];       // [角速度、加速度]偏差模长的平方
        float Rest_T;                          // 连续静止候选时长，s
        float Rest_Last_Gyro_Lp[3];             // rad/s
        double Rest_Gyro_Lp_State[6];
        float Rest_Last_Accel_Lp[3];            // m/s²
        double Rest_Accel_Lp_State[6];
    };

    struct Struct_VQF_Coefficient
    {
        double Accel_Lp_B[3];                  // 分子[b0,b1,b2]
        double Accel_Lp_A[2];                  // 分母[a1,a2]，a0=1
        double Rest_Lp_B[3];                   // 加速度与角速度同采样周期，共用系数
        double Rest_Lp_A[2];
        float Bias_P0;                         // 初始协方差对角元，(0.01°/s)²
        float Bias_V;                          // 每步过程噪声，(0.01°/s)²
        float Bias_Motion_W;                   // 运动观测噪声，(0.01°/s)²
        float Bias_Vertical_W;                 // 竖直弱约束观测噪声，(0.01°/s)²
        float Bias_Rest_W;                     // 静止观测噪声，(0.01°/s)²
    };

    // 初始化相关常量
    Struct_VQF_Parameter Parameter;
    Struct_VQF_Coefficient Coefficient = Struct_VQF_Coefficient();
    float D_T = 0.001f;
    bool Initialized = false;

    // 内部变量
    Struct_VQF_State State = Struct_VQF_State();
    float Pre_Yaw = 0.0f;
    bool Angle_Initialized = false;

    // 读变量
    float Quaternion[4] = {1.0f, 0.0f, 0.0f, 0.0f};
    float Roll = 0.0f;
    float Pitch = 0.0f;
    float Yaw = 0.0f;
    float Yaw_Total_Angle = 0.0f;

    // 内部函数：六轴核心及独立的小矩阵运算
    VQF_ITCM_CODE void Update_Gyro(const float gyro[3]);
    VQF_ITCM_CODE void Update_Accel(const float accel[3]);
    VQF_ITCM_CODE void Calculate_Angle();
    static VQF_ITCM_CODE void Quat_Multiply(const float q1[4], const float q2[4], float out[4]);
    static VQF_ITCM_CODE void Quat_Rotate(const float q[4], const float v[3], float out[3]);
    static VQF_ITCM_CODE float Norm(const float vec[], size_t N);
    static VQF_ITCM_CODE void Normalize(float vec[], size_t N);
    static VQF_ITCM_CODE void Clip(float vec[], size_t N, float min, float max);
    static void Filter_Coefficients(float tau, float Ts, double outB[3], double outA[2]);
    static VQF_ITCM_CODE void Filter_Initial_State(float x0, const double b[3], const double a[2], double out[2]);
    static VQF_ITCM_CODE float Filter_Step(float x, const double b[3], const double a[2], double state[2]);
    VQF_ITCM_CODE void Filter_Vector(const float x[], size_t N, float tau, float Ts,
                       const double b[3], const double a[2], double state[], float out[]);
    static VQF_ITCM_CODE void Matrix_Identity(float scale, float out[9]);
    static VQF_ITCM_CODE void Matrix_Multiply(const float in1[9], const float in2[9], float out[9]);
    static VQF_ITCM_CODE void Matrix_Multiply_Transpose_First(const float in1[9], const float in2[9], float out[9]);
    static VQF_ITCM_CODE void Matrix_Multiply_Transpose_Second(const float in1[9], const float in2[9], float out[9]);
    static VQF_ITCM_CODE bool Matrix_Inverse(const float in[9], float out[9]);
};

#endif
