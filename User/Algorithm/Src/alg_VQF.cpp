/**
 * @file alg_VQF.cpp
 * @brief 六轴VQF：陀螺积分、惯性系加速度低通、倾角修正、零偏估计
 * @version 0.2
 * @date 2026-11-09
 *
 * SPDX-FileCopyrightText: 2021 Daniel Laidig <laidig@control.tu-berlin.de>
 * SPDX-License-Identifier: MIT
 * 原始代码：https://github.com/dlaidig/vqf
 * 固定版本及完整MIT许可见文件末尾和 Docs/VQF_FreeRTOS.md。
 */

/* Includes ------------------------------------------------------------------*/

#include "alg_VQF.h"
#include <algorithm>
#include <cmath>
#include <limits>

/* Private variables ---------------------------------------------------------*/

namespace
{
const double VQF_PI = 3.14159265358979323846264338327950288;
const double VQF_SQRT2 = 1.41421356237309504880168872420969808;
const float VQF_EPS = std::numeric_limits<float>::epsilon();
VQF_ITCM_CODE inline float Square(float x) { return (x * x); }
}

/* Function prototypes -------------------------------------------------------*/

/**
 * @brief 初始化；D_T为固定更新周期(s)，Tau_Acc为加速度低通时间常数(s)
 * @return bool 参数有效且初始化成功；失败时保留原状态
 */
bool Class_VQF::Init(float __D_T, float __Tau_Acc)
{
    Struct_VQF_Parameter parameter;
    parameter.Tau_Acc = __Tau_Acc;
    return (Init(parameter, __D_T));
}

bool Class_VQF::Init(const Struct_VQF_Parameter &__Parameter, float __D_T)
{
    // 支持10Hz~1MHz的等周期输入；不能将停机时长作为一次更新周期。
    if (!std::isfinite(__D_T) || __D_T < 0.000001f || __D_T > 0.1f)
    {
        return (false);
    }
    const float positive[] = {__Parameter.Tau_Acc, __Parameter.Bias_Sigma_Init,
        __Parameter.Bias_Forgetting_Time, __Parameter.Bias_Clip, __Parameter.Bias_Sigma_Motion,
        __Parameter.Bias_Vertical_Forgetting_Factor, __Parameter.Bias_Sigma_Rest,
        __Parameter.Rest_Filter_Tau, __Parameter.Rest_Th_Gyro, __Parameter.Rest_Th_Accel};
    for (size_t i = 0; i < sizeof(positive) / sizeof(positive[0]); i++)
    {
        if (!std::isfinite(positive[i]) || positive[i] <= 0.0f)
        {
            return (false);
        }
    }
    if (!std::isfinite(__Parameter.Rest_Min_T) || __Parameter.Rest_Min_T < 0.0f
        || __Parameter.Bias_Vertical_Forgetting_Factor > 1.0f
        || !std::isfinite(__Parameter.Filter_Init_Time)
        || (__Parameter.Filter_Init_Time < 0.0f && __Parameter.Filter_Init_Time != -1.0f))
    {
        return (false);
    }

    Struct_VQF_Coefficient coefficient = Struct_VQF_Coefficient();
    Filter_Coefficients(__Parameter.Tau_Acc, __D_T, coefficient.Accel_Lp_B, coefficient.Accel_Lp_A);
    Filter_Coefficients(__Parameter.Rest_Filter_Tau, __D_T, coefficient.Rest_Lp_B, coefficient.Rest_Lp_A);
    // 沿用官方以0.01°/s为内部零偏协方差单位的缩放。
    coefficient.Bias_P0 = Square(__Parameter.Bias_Sigma_Init * 100.0f);
    coefficient.Bias_V = 100.0f * __D_T / __Parameter.Bias_Forgetting_Time;
    const float p_motion = Square(__Parameter.Bias_Sigma_Motion * 100.0f);
    const float p_rest = Square(__Parameter.Bias_Sigma_Rest * 100.0f);
    coefficient.Bias_Motion_W = Square(p_motion) / coefficient.Bias_V + p_motion;
    coefficient.Bias_Vertical_W = coefficient.Bias_Motion_W
        / (std::max)(__Parameter.Bias_Vertical_Forgetting_Factor, 1e-10f);
    coefficient.Bias_Rest_W = Square(p_rest) / coefficient.Bias_V + p_rest;
    const float noise[] = {coefficient.Bias_P0, coefficient.Bias_V, coefficient.Bias_Motion_W,
        coefficient.Bias_Vertical_W, coefficient.Bias_Rest_W};
    for (size_t i = 0; i < sizeof(noise) / sizeof(noise[0]); i++)
    {
        if (!std::isfinite(noise[i]) || noise[i] <= 0.0f)
        {
            return (false);
        }
    }

    Parameter = __Parameter;
    Coefficient = coefficient;
    D_T = __D_T;
    Initialized = true;
    Reset();
    return (true);
}

/**
 * @brief 保留参数与周期，清空姿态、滤波历史、静止判断和零偏估计
 */
void Class_VQF::Reset()
{
    State = Struct_VQF_State();
    State.Gyro_Quat[0] = 1.0f;
    State.Accel_Quat[0] = 1.0f;
    Matrix_Identity(Coefficient.Bias_P0, State.Bias_P);
    // NaN只用于内部低通启动标志，不能用-ffast-math破坏该语义。
    const double nan = std::numeric_limits<double>::quiet_NaN();
    std::fill(State.Accel_Lp_State, State.Accel_Lp_State + 6, nan);
    std::fill(State.Motion_R_Lp_State, State.Motion_R_Lp_State + 18, nan);
    std::fill(State.Motion_Bias_Lp_State, State.Motion_Bias_Lp_State + 4, nan);
    std::fill(State.Rest_Gyro_Lp_State, State.Rest_Gyro_Lp_State + 6, nan);
    std::fill(State.Rest_Accel_Lp_State, State.Rest_Accel_Lp_State + 6, nan);
    Quaternion[0] = 1.0f;
    Quaternion[1] = Quaternion[2] = Quaternion[3] = 0.0f;
    Roll = Pitch = Yaw = Yaw_Total_Angle = Pre_Yaw = 0.0f;
    Angle_Initialized = false;
}

/**
 * @brief 每个固定周期调用一次；不修改输入，不进行SPI读取和温控
 * @return bool 未初始化、空指针、非有限数据时返回false且不改变状态；
 *              零加速度时只进行陀螺积分并清除静止判定，返回true
 */
bool Class_VQF::Update(const float __Gyro[3], const float __Accel[3])
{
    if (!Initialized || __Gyro == NULL || __Accel == NULL)
    {
        return (false);
    }
    for (size_t i = 0; i < 3; i++)
    {
        if (!std::isfinite(__Gyro[i]) || !std::isfinite(__Accel[i]))
        {
            return (false);
        }
    }
    if (!std::isfinite(Norm(__Gyro, 3)) || !std::isfinite(Norm(__Accel, 3)))
    {
        return (false);
    }
    Update_Gyro(__Gyro);
    if (Norm(__Accel, 3) > VQF_EPS)
    {
        Update_Accel(__Accel);
    }
    else
    {
        State.Rest_T = 0.0f;
        State.Rest_Detected = false;
    }
    Quat_Multiply(State.Accel_Quat, State.Gyro_Quat, Quaternion);
    Normalize(Quaternion, 4);
    Calculate_Angle();
    return (true);
}

bool Class_VQF::Update(float __Gyro_X, float __Gyro_Y, float __Gyro_Z,
                       float __Accel_X, float __Accel_Y, float __Accel_Z)
{
    const float gyro[3] = {__Gyro_X, __Gyro_Y, __Gyro_Z};
    const float accel[3] = {__Accel_X, __Accel_Y, __Accel_Z};
    return (Update(gyro, accel));
}

void Class_VQF::Get_Quaternion(float __Quaternion[4]) const
{
    if (__Quaternion != NULL)
    {
        std::copy(Quaternion, Quaternion + 4, __Quaternion);
    }
}

void Class_VQF::Get_Gyro_Bias(float __Gyro_Bias[3]) const
{
    if (__Gyro_Bias != NULL)
    {
        std::copy(State.Bias, State.Bias + 3, __Gyro_Bias);
    }
}

bool Class_VQF::Get_Accel_Filter_Ready() const
{
    return (Initialized && std::isfinite(State.Accel_Lp_State[0]));
}

float Class_VQF::Get_Bias_Sigma() const
{
    if (!Initialized)
    {
        return (Parameter.Bias_Sigma_Init * float(VQF_PI / 180.0));
    }
    // Gershgorin行和给出最大特征值的上界，最后换算到rad/s。
    const float s0 = std::fabs(State.Bias_P[0]) + std::fabs(State.Bias_P[1]) + std::fabs(State.Bias_P[2]);
    const float s1 = std::fabs(State.Bias_P[3]) + std::fabs(State.Bias_P[4]) + std::fabs(State.Bias_P[5]);
    const float s2 = std::fabs(State.Bias_P[6]) + std::fabs(State.Bias_P[7]) + std::fabs(State.Bias_P[8]);
    const float p = (std::min)((std::max)((std::max)(s0, s1), s2), Coefficient.Bias_P0);
    return (std::sqrt(p) * float(VQF_PI / 18000.0));
}

void Class_VQF::Calculate_Angle()
{
    const float *q = Quaternion;
    Yaw = std::atan2(2.0f * (q[0]*q[3] + q[1]*q[2]),
        2.0f * (q[0]*q[0] + q[1]*q[1]) - 1.0f) * 57.295779513f;
    Pitch = std::atan2(2.0f * (q[0]*q[1] + q[2]*q[3]),
        2.0f * (q[0]*q[0] + q[3]*q[3]) - 1.0f) * 57.295779513f;
    float sin_roll = -2.0f * (q[1]*q[3] - q[0]*q[2]);
    Clip(&sin_roll, 1, -1.0f, 1.0f);
    Roll = std::asin(sin_roll) * 57.295779513f;
    if (!Angle_Initialized)
    {
        Yaw_Total_Angle = Yaw;
        Angle_Initialized = true;
    }
    else
    {
        float delta_yaw = Yaw - Pre_Yaw;
        if (delta_yaw > 180.0f) delta_yaw -= 360.0f;
        else if (delta_yaw < -180.0f) delta_yaw += 360.0f;
        Yaw_Total_Angle += delta_yaw;
    }
    Pre_Yaw = Yaw;
}

/* VQF six-axis core (adapted from upstream) ----------------------------------*/

    void Class_VQF::Update_Gyro(const float gyr[3])
{
    // rest detection
    if (Parameter.Rest_Bias_Est_Enabled)
    {
        Filter_Vector(gyr, 3, Parameter.Rest_Filter_Tau, D_T, Coefficient.Rest_Lp_B, Coefficient.Rest_Lp_A,
            State.Rest_Gyro_Lp_State, State.Rest_Last_Gyro_Lp);

        State.Rest_Squared_Deviations[0] = Square(gyr[0] - State.Rest_Last_Gyro_Lp[0])
            + Square(gyr[1] - State.Rest_Last_Gyro_Lp[1]) + Square(gyr[2] - State.Rest_Last_Gyro_Lp[2]);

        float biasClip = Parameter.Bias_Clip*float(VQF_PI/180.0);
        if (State.Rest_Squared_Deviations[0] >= Square(Parameter.Rest_Th_Gyro*float(VQF_PI/180.0))
            || std::fabs(State.Rest_Last_Gyro_Lp[0]) > biasClip || std::fabs(State.Rest_Last_Gyro_Lp[1]) > biasClip
            || std::fabs(State.Rest_Last_Gyro_Lp[2]) > biasClip)
        {
            State.Rest_T = 0.0;
            State.Rest_Detected = false;
        }
    }

    // remove estimated gyro bias
    float gyrNoBias[3] = {gyr[0]-State.Bias[0], gyr[1]-State.Bias[1], gyr[2]-State.Bias[2]};

    // gyroscope prediction step
    float gyrNorm = Norm(gyrNoBias, 3);
    float angle = gyrNorm * D_T;
    if (gyrNorm > VQF_EPS)
    {
        float c = std::cos(angle/2);
        float s = std::sin(angle/2)/gyrNorm;
        float gyrStepQuat[4] = {c, s*gyrNoBias[0], s*gyrNoBias[1], s*gyrNoBias[2]};
        Quat_Multiply(State.Gyro_Quat, gyrStepQuat, State.Gyro_Quat);
        Normalize(State.Gyro_Quat, 4);
    }
}

void Class_VQF::Update_Accel(const float acc[3])
{
    // ignore [0 0 0] samples
    if (acc[0] == float(0.0) && acc[1] == float(0.0) && acc[2] == float(0.0))
    {
        return;
    }

    // rest detection
    if (Parameter.Rest_Bias_Est_Enabled)
    {
        Filter_Vector(acc, 3, Parameter.Rest_Filter_Tau, D_T, Coefficient.Rest_Lp_B, Coefficient.Rest_Lp_A,
            State.Rest_Accel_Lp_State, State.Rest_Last_Accel_Lp);

        State.Rest_Squared_Deviations[1] = Square(acc[0] - State.Rest_Last_Accel_Lp[0])
            + Square(acc[1] - State.Rest_Last_Accel_Lp[1]) + Square(acc[2] - State.Rest_Last_Accel_Lp[2]);

        if (State.Rest_Squared_Deviations[1] >= Square(Parameter.Rest_Th_Accel))
        {
            State.Rest_T = 0.0;
            State.Rest_Detected = false;
        }
        else
        {
            State.Rest_T += D_T;
            if (State.Rest_T >= Parameter.Rest_Min_T)
            {
                State.Rest_Detected = true;
            }
        }
    }

    float accEarth[3];

    // filter acc in inertial frame
    Quat_Rotate(State.Gyro_Quat, acc, accEarth);
    Filter_Vector(accEarth, 3, Parameter.Tau_Acc, D_T, Coefficient.Accel_Lp_B, Coefficient.Accel_Lp_A, State.Accel_Lp_State, State.Last_Accel_Lp);

    // transform to 6D earth frame and normalize
    Quat_Rotate(State.Accel_Quat, State.Last_Accel_Lp, accEarth);
    // 低通向量可能因相反方向样本抵消为零，此时没有可用的重力方向。
    if (Norm(accEarth, 3) <= VQF_EPS)
    {
        State.Rest_T = 0.0f;
        State.Rest_Detected = false;
        return;
    }
    Normalize(accEarth, 3);
    Clip(accEarth + 2, 1, -1.0f, 1.0f);

    // inclination correction
    float accCorrQuat[4];
    float q_w = std::sqrt((accEarth[2]+1)/2);
    if (q_w > float(1e-6))
    {
        accCorrQuat[0] = q_w;
        accCorrQuat[1] = float(0.5)*accEarth[1]/q_w;
        accCorrQuat[2] = float(-0.5)*accEarth[0]/q_w;
        accCorrQuat[3] = 0;
    }
    else
    {
        // to avoid numeric issues when acc is close to [0 0 -1], i.e. the correction step is close (<= 0.00011°) to 180°:
        accCorrQuat[0] = 0;
        accCorrQuat[1] = 1;
        accCorrQuat[2] = 0;
        accCorrQuat[3] = 0;
    }
    Quat_Multiply(accCorrQuat, State.Accel_Quat, State.Accel_Quat);
    Normalize(State.Accel_Quat, 4);

    // bias estimation
    if (Parameter.Motion_Bias_Est_Enabled || Parameter.Rest_Bias_Est_Enabled)
    {
        float biasClip = Parameter.Bias_Clip*float(VQF_PI/180.0);

        float accGyrQuat[4];
        float R[9];
        float biasLp[2];

        // get rotation matrix corresponding to accGyrQuat
        Quat_Multiply(State.Accel_Quat, State.Gyro_Quat, accGyrQuat);
        R[0] = 1 - 2*Square(accGyrQuat[2]) - 2*Square(accGyrQuat[3]); // r11
        R[1] = 2*(accGyrQuat[2]*accGyrQuat[1] - accGyrQuat[0]*accGyrQuat[3]); // r12
        R[2] = 2*(accGyrQuat[0]*accGyrQuat[2] + accGyrQuat[3]*accGyrQuat[1]); // r13
        R[3] = 2*(accGyrQuat[0]*accGyrQuat[3] + accGyrQuat[2]*accGyrQuat[1]); // r21
        R[4] = 1 - 2*Square(accGyrQuat[1]) - 2*Square(accGyrQuat[3]); // r22
        R[5] = 2*(accGyrQuat[2]*accGyrQuat[3] - accGyrQuat[1]*accGyrQuat[0]); // r23
        R[6] = 2*(accGyrQuat[3]*accGyrQuat[1] - accGyrQuat[0]*accGyrQuat[2]); // r31
        R[7] = 2*(accGyrQuat[0]*accGyrQuat[1] + accGyrQuat[3]*accGyrQuat[2]); // r32
        R[8] = 1 - 2*Square(accGyrQuat[1]) - 2*Square(accGyrQuat[2]); // r33

        // calculate R*b_hat (only the x and y component, as z is not needed)
        biasLp[0] = R[0]*State.Bias[0] + R[1]*State.Bias[1] + R[2]*State.Bias[2];
        biasLp[1] = R[3]*State.Bias[0] + R[4]*State.Bias[1] + R[5]*State.Bias[2];

        // low-pass filter R and R*b_hat
        Filter_Vector(R, 9, Parameter.Tau_Acc, D_T, Coefficient.Accel_Lp_B, Coefficient.Accel_Lp_A, State.Motion_R_Lp_State, R);
        Filter_Vector(biasLp, 2, Parameter.Tau_Acc, D_T, Coefficient.Accel_Lp_B, Coefficient.Accel_Lp_A, State.Motion_Bias_Lp_State,
            biasLp);

        // set measurement error and covariance for the respective Kalman filter update
        float w[3];
        float e[3];
        if (State.Rest_Detected && Parameter.Rest_Bias_Est_Enabled)
        {
            e[0] = State.Rest_Last_Gyro_Lp[0] - State.Bias[0];
            e[1] = State.Rest_Last_Gyro_Lp[1] - State.Bias[1];
            e[2] = State.Rest_Last_Gyro_Lp[2] - State.Bias[2];
            Matrix_Identity(1.0, R);
            std::fill(w, w+3, Coefficient.Bias_Rest_W);
        }
        else if (Parameter.Motion_Bias_Est_Enabled)
        {
            e[0] = -accEarth[1]/D_T + biasLp[0] - R[0]*State.Bias[0] - R[1]*State.Bias[1] - R[2]*State.Bias[2];
            e[1] = accEarth[0]/D_T + biasLp[1] - R[3]*State.Bias[0] - R[4]*State.Bias[1] - R[5]*State.Bias[2];
            e[2] = - R[6]*State.Bias[0] - R[7]*State.Bias[1] - R[8]*State.Bias[2];
            w[0] = Coefficient.Bias_Motion_W;
            w[1] = Coefficient.Bias_Motion_W;
            w[2] = Coefficient.Bias_Vertical_W;
        }
        else
        {
            std::fill(w, w+3, -1); // disable update
        }

        // Kalman filter update
        // step 1: P = P + V (also increase covariance if there is no measurement update!)
        if (State.Bias_P[0] < Coefficient.Bias_P0)
        {
            State.Bias_P[0] += Coefficient.Bias_V;
        }
        if (State.Bias_P[4] < Coefficient.Bias_P0)
        {
            State.Bias_P[4] += Coefficient.Bias_V;
        }
        if (State.Bias_P[8] < Coefficient.Bias_P0)
        {
            State.Bias_P[8] += Coefficient.Bias_V;
        }
        if (w[0] >= 0)
        {
            // clip disagreement to -2..2 °/s
            // (this also effectively limits the harm done by the first inclination correction step)
            Clip(e, 3, -biasClip, biasClip);

            // step 2: K = P R^T inv(W + R P R^T)
            float K[9];
            Matrix_Multiply_Transpose_Second(State.Bias_P, R, K); // K = P R^T
            Matrix_Multiply(R, K, K); // K = R P R^T
            K[0] += w[0];
            K[4] += w[1];
            K[8] += w[2]; // K = W + R P R^T
            if (!Matrix_Inverse(K, K)) // K = inv(W + R P R^T)
            {
                return; // 奇异时保留本次已完成的姿态更新，跳过零偏量测更新。
            }
            Matrix_Multiply_Transpose_First(R, K, K); // K = R^T inv(W + R P R^T)
            Matrix_Multiply(State.Bias_P, K, K); // K = P R^T inv(W + R P R^T)

            // step 3: bias = bias + K (y - R bias) = bias + K e
            State.Bias[0] += K[0]*e[0] + K[1]*e[1] + K[2]*e[2];
            State.Bias[1] += K[3]*e[0] + K[4]*e[1] + K[5]*e[2];
            State.Bias[2] += K[6]*e[0] + K[7]*e[1] + K[8]*e[2];

            // step 4: P = P - K R P
            Matrix_Multiply(K, R, K); // K = K R
            Matrix_Multiply(K, State.Bias_P, K); // K = K R P
            for (size_t i = 0; i < 9; i++)
            {
                State.Bias_P[i] -= K[i];
            }

            // clip bias estimate to -2..2 °/s
            Clip(State.Bias, 3, -biasClip, biasClip);
        }
    }

}

void Class_VQF::Quat_Multiply(const float q1[4], const float q2[4], float out[4])
{
    float w = q1[0] * q2[0] - q1[1] * q2[1] - q1[2] * q2[2] - q1[3] * q2[3];
    float x = q1[0] * q2[1] + q1[1] * q2[0] + q1[2] * q2[3] - q1[3] * q2[2];
    float y = q1[0] * q2[2] - q1[1] * q2[3] + q1[2] * q2[0] + q1[3] * q2[1];
    float z = q1[0] * q2[3] + q1[1] * q2[2] - q1[2] * q2[1] + q1[3] * q2[0];
    out[0] = w; out[1] = x; out[2] = y; out[3] = z;
}

void Class_VQF::Quat_Rotate(const float q[4], const float v[3], float out[3])
{
    float x = (1 - 2*q[2]*q[2] - 2*q[3]*q[3])*v[0] + 2*v[1]*(q[2]*q[1] - q[0]*q[3]) + 2*v[2]*(q[0]*q[2] + q[3]*q[1]);
    float y = 2*v[0]*(q[0]*q[3] + q[2]*q[1]) + v[1]*(1 - 2*q[1]*q[1] - 2*q[3]*q[3]) + 2*v[2]*(q[2]*q[3] - q[1]*q[0]);
    float z = 2*v[0]*(q[3]*q[1] - q[0]*q[2]) + 2*v[1]*(q[0]*q[1] + q[3]*q[2]) + v[2]*(1 - 2*q[1]*q[1] - 2*q[2]*q[2]);
    out[0] = x; out[1] = y; out[2] = z;
}

float Class_VQF::Norm(const float vec[], size_t N)
{
    float s = 0;
    for (size_t i = 0; i < N; i++)
    {
        s += vec[i]*vec[i];
    }
    return std::sqrt(s);
}

void Class_VQF::Normalize(float vec[], size_t N)
{
    float n = Norm(vec, N);
    if (n < VQF_EPS)
    {
        return;
    }
    for (size_t i = 0; i < N; i++)
    {
        vec[i] /= n;
    }
}

void Class_VQF::Clip(float vec[], size_t N, float min, float max)
{
    for (size_t i = 0; i < N; i++)
    {
        if (vec[i] < min)
        {
            vec[i] = min;
        }
        else if (vec[i] > max)
        {
            vec[i] = max;
        }
    }
}

void Class_VQF::Filter_Coefficients(float tau, float Ts, double outB[3], double outA[2])
{

    // disable filter and use direct passthrough when tau < Ts/2 to avoid instability
    // (this corresponds to fc exceeding 90% of the Nyquist frequency)
    if (tau < Ts/2)
    {
        outB[0] = 1;
        outB[1] = 0;
        outB[2] = 0;
        outA[0] = 0;
        outA[1] = 0;
        return;
    }

    // second order Butterworth filter based on https://stackoverflow.com/a/52764064
    double fc = (VQF_SQRT2 / (2.0*VQF_PI))/double(tau); // time constant of dampened, non-oscillating part of step response
    double C = std::tan(VQF_PI*fc*double(Ts));
    double D = C*C + std::sqrt(2)*C + 1;
    double b0 = C*C/D;
    outB[0] = b0;
    outB[1] = 2*b0;
    outB[2] = b0;
    // a0 = 1.0
    outA[0] = 2*(C*C-1)/D; // a1
    outA[1] = (1-std::sqrt(2)*C+C*C)/D; // a2
}

void Class_VQF::Filter_Initial_State(float x0, const double b[3], const double a[2], double out[2])
{
    // initial state for steady state (equivalent to scipy.signal.lfilter_zi, obtained by setting y=x=x0 in the filter
    // update equation)
    out[0] = double(x0)*(1 - b[0]);
    out[1] = double(x0)*(b[2] - a[1]);
}

float Class_VQF::Filter_Step(float x, const double b[3], const double a[2], double state[2])
{
    // difference equations based on scipy.signal.lfilter documentation
    // assumes that a0 == 1.0
    double y = b[0]*double(x) + state[0];
    state[0] = b[1]*double(x) - a[0]*y + state[1];
    state[1] = b[2]*double(x) - a[1]*y;
    return y;
}

void Class_VQF::Filter_Vector(const float x[], size_t N, float tau, float Ts, const double b[3],
    const double a[2], double state[], float out[])
{
    // 启动均值只用于建立非零初始状态，不改变上层按tau计算的稳态滤波系数。
    // 采用共同时间上限，保证加速度、运动零偏中的R及R*b滤波同步结束初始化。
    // -1保留官方行为；0在第一组有效输入后建立状态。
    if (std::isnan(state[0]))
    {
        const float init_time = Parameter.Filter_Init_Time < 0.0f
            ? tau : (std::min)(tau, Parameter.Filter_Init_Time);
        if (std::isnan(state[1]))
        {
            state[1] = 0; // state[1] is used to store the sample count
            for (size_t i = 0; i < N; i++)
            {
                state[2+i] = 0; // state[2+i] is used to store the sum
            }
        }
        state[1]++;
        for (size_t i = 0; i < N; i++)
        {
            state[2+i] += double(x[i]);
            out[i] = state[2+i]/state[1];
        }
        if (float(state[1])*Ts >= init_time)
        {
            for (size_t i = 0; i < N; i++)
            {
                Filter_Initial_State(out[i], b, a, state+2*i);
            }
        }
        return;
    }

    for (size_t i = 0; i < N; i++)
    {
        out[i] = Filter_Step(x[i], b, a, state+2*i);
    }
}

void Class_VQF::Matrix_Identity(float scale, float out[9])
{
    out[0] = scale;
    out[1] = 0.0;
    out[2] = 0.0;
    out[3] = 0.0;
    out[4] = scale;
    out[5] = 0.0;
    out[6] = 0.0;
    out[7] = 0.0;
    out[8] = scale;
}

void Class_VQF::Matrix_Multiply(const float in1[9], const float in2[9], float out[9])
{
    float tmp[9];
    tmp[0] = in1[0]*in2[0] + in1[1]*in2[3] + in1[2]*in2[6];
    tmp[1] = in1[0]*in2[1] + in1[1]*in2[4] + in1[2]*in2[7];
    tmp[2] = in1[0]*in2[2] + in1[1]*in2[5] + in1[2]*in2[8];
    tmp[3] = in1[3]*in2[0] + in1[4]*in2[3] + in1[5]*in2[6];
    tmp[4] = in1[3]*in2[1] + in1[4]*in2[4] + in1[5]*in2[7];
    tmp[5] = in1[3]*in2[2] + in1[4]*in2[5] + in1[5]*in2[8];
    tmp[6] = in1[6]*in2[0] + in1[7]*in2[3] + in1[8]*in2[6];
    tmp[7] = in1[6]*in2[1] + in1[7]*in2[4] + in1[8]*in2[7];
    tmp[8] = in1[6]*in2[2] + in1[7]*in2[5] + in1[8]*in2[8];
    std::copy(tmp, tmp+9, out);
}

void Class_VQF::Matrix_Multiply_Transpose_First(const float in1[9], const float in2[9], float out[9])
{
    float tmp[9];
    tmp[0] = in1[0]*in2[0] + in1[3]*in2[3] + in1[6]*in2[6];
    tmp[1] = in1[0]*in2[1] + in1[3]*in2[4] + in1[6]*in2[7];
    tmp[2] = in1[0]*in2[2] + in1[3]*in2[5] + in1[6]*in2[8];
    tmp[3] = in1[1]*in2[0] + in1[4]*in2[3] + in1[7]*in2[6];
    tmp[4] = in1[1]*in2[1] + in1[4]*in2[4] + in1[7]*in2[7];
    tmp[5] = in1[1]*in2[2] + in1[4]*in2[5] + in1[7]*in2[8];
    tmp[6] = in1[2]*in2[0] + in1[5]*in2[3] + in1[8]*in2[6];
    tmp[7] = in1[2]*in2[1] + in1[5]*in2[4] + in1[8]*in2[7];
    tmp[8] = in1[2]*in2[2] + in1[5]*in2[5] + in1[8]*in2[8];
    std::copy(tmp, tmp+9, out);
}

void Class_VQF::Matrix_Multiply_Transpose_Second(const float in1[9], const float in2[9], float out[9])
{
    float tmp[9];
    tmp[0] = in1[0]*in2[0] + in1[1]*in2[1] + in1[2]*in2[2];
    tmp[1] = in1[0]*in2[3] + in1[1]*in2[4] + in1[2]*in2[5];
    tmp[2] = in1[0]*in2[6] + in1[1]*in2[7] + in1[2]*in2[8];
    tmp[3] = in1[3]*in2[0] + in1[4]*in2[1] + in1[5]*in2[2];
    tmp[4] = in1[3]*in2[3] + in1[4]*in2[4] + in1[5]*in2[5];
    tmp[5] = in1[3]*in2[6] + in1[4]*in2[7] + in1[5]*in2[8];
    tmp[6] = in1[6]*in2[0] + in1[7]*in2[1] + in1[8]*in2[2];
    tmp[7] = in1[6]*in2[3] + in1[7]*in2[4] + in1[8]*in2[5];
    tmp[8] = in1[6]*in2[6] + in1[7]*in2[7] + in1[8]*in2[8];
    std::copy(tmp, tmp+9, out);
}

bool Class_VQF::Matrix_Inverse(const float in[9], float out[9])
{
    // in = [a b c; d e f; g h i]
    double A = double(in[4]*in[8] - in[5]*in[7]); // (e*i - f*h)
    double D = double(in[2]*in[7] - in[1]*in[8]); // -(b*i - c*h)
    double G = double(in[1]*in[5] - in[2]*in[4]); // (b*f - c*e)
    double B = double(in[5]*in[6] - in[3]*in[8]); // -(d*i - f*g)
    double E = double(in[0]*in[8] - in[2]*in[6]); // (a*i - c*g)
    double H = double(in[2]*in[3] - in[0]*in[5]); // -(a*f - c*d)
    double C = double(in[3]*in[7] - in[4]*in[6]); // (d*h - e*g)
    double F = double(in[1]*in[6] - in[0]*in[7]); // -(a*h - b*g)
    double I = double(in[0]*in[4] - in[1]*in[3]); // (a*e - b*d)

    double det = double(in[0])*A + double(in[1])*B + double(in[2])*C; // a*A + b*B + c*C;

    if (!std::isfinite(det) || (det >= double(-VQF_EPS) && det <= double(VQF_EPS)))
    {
        std::fill(out, out+9, 0);
        return false;
    }

    // out = [A D G; B E H; C F I]/det
    out[0] = A/det;
    out[1] = D/det;
    out[2] = G/det;
    out[3] = B/det;
    out[4] = E/det;
    out[5] = H/det;
    out[6] = C/det;
    out[7] = F/det;
    out[8] = I/det;

    return true;
}

/*
MIT License

Copyright (c) 2021 Daniel Laidig <laidig@control.tu-berlin.de>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is furnished
to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS
OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
