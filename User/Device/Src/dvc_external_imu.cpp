#include "dvc_external_imu.h"

void Class_External_IMU::TIM1msMod50_Alive_PeriodElapsedCallback(void)
{
    if (Flag == Pre_Flag) // 判断陀螺仪是否掉线
    {
        IMU_Status = IMU_Status_DISABLE;
    }
    else
        IMU_Status = IMU_Status_ENABLE;

    Pre_Flag = Flag;
}

uint32_t imu_cnt = 0;
float IMU_Dt;
void Class_External_IMU::UART_BMI_Data_Process(uint8_t *Buffer){
	
  if(Buffer[0] == 0xA5 && Buffer[13] == 0xB5){
    Flag ++;
    IMU_Dt = DWT_GetDeltaT(&imu_cnt);
    BMI088_Raw_Data.Accel[0] = (float)((int16_t)(Buffer[1] << 8) | (Buffer[2])) / 100.0f;
    BMI088_Raw_Data.Accel[1] = (float)((int16_t)(Buffer[3] << 8) | (Buffer[4])) / 100.0f;
    BMI088_Raw_Data.Accel[2] = (float)((int16_t)(Buffer[5] << 8) | (Buffer[6])) / 100.0f;

    BMI088_Raw_Data.Gyro[0] = (float)((int16_t)(Buffer[7] << 8) | (Buffer[8])) / 100.0f;
    BMI088_Raw_Data.Gyro[1] = (float)((int16_t)(Buffer[9] << 8) | (Buffer[10])) / 100.0f;
    BMI088_Raw_Data.Gyro[2] = (float)((int16_t)(Buffer[11] << 8) | (Buffer[12])) / 100.0f;
  }
}

// 本次同步的全帧解析, 暂时停用
// void Class_External_IMU::UART_BMI_Data_Process(uint8_t *Buffer, uint16_t Length)
// {
//   const uint16_t frame_length = 14;
//   uint16_t offset = 0;
//
//   // 一次DMA回调可能带有两帧，逐帧解析，不能只处理Buffer开头的一帧
//   while (offset + frame_length <= Length)
//   {
//     uint8_t *frame = Buffer + offset;
//     if (frame[0] != 0xA5 || frame[frame_length - 1] != 0xB5)
//     {
//       offset++;
//       continue;
//     }
//
//     Flag++;
//     Trigger_Cnt++;
//     Timestamp_us = (uint32_t)DWT_GetTimeline_us();
//     IMU_Dt = DWT_GetDeltaT(&imu_cnt);
//
//     // 每5帧(1000Hz/5 = 200Hz)触发一次相机, 与Trigger_Cnt对齐便于视觉端取帧
//     // TODO: 触发引脚/脉冲宽度待硬件确定; 建议用TIM PWM或One-Pulse输出固定宽度脉冲, 不要用GPIO直接拉高再拉低
//     if (Trigger_Cnt % 5 == 0)
//     {
//       HAL_GPIO_WritePin(CAM_TRIG_GPIO_Port, CAM_TRIG_Pin, GPIO_PIN_SET);
//       for (volatile uint32_t i = 0; i < 20; i++);      // 几us级脉宽, 实际按硬件调整
//       HAL_GPIO_WritePin(CAM_TRIG_GPIO_Port, CAM_TRIG_Pin, GPIO_PIN_RESET);
//     }
//
//     BMI088_Raw_Data.Accel[0] = (float)((int16_t)(frame[1] << 8) | frame[2]) / 100.0f;
//     BMI088_Raw_Data.Accel[1] = (float)((int16_t)(frame[3] << 8) | frame[4]) / 100.0f;
//     BMI088_Raw_Data.Accel[2] = (float)((int16_t)(frame[5] << 8) | frame[6]) / 100.0f;
//
//     BMI088_Raw_Data.Gyro[0] = (float)((int16_t)(frame[7] << 8) | frame[8]) / 100.0f;
//     BMI088_Raw_Data.Gyro[1] = (float)((int16_t)(frame[9] << 8) | frame[10]) / 100.0f;
//     BMI088_Raw_Data.Gyro[2] = (float)((int16_t)(frame[11] << 8) | frame[12]) / 100.0f;
//
//     offset += frame_length;
//   }
// }

// 本次同步的快照读取, 暂时停用
// void Class_External_IMU::Get_Trigger_Data(Struct_IMU_Trigger *Data)
// {
//   // TIM4发布与TIM5读取可能互相抢占，保证q、计数和时间戳来自同一帧
//   uint32_t primask = __get_PRIMASK();
//   __disable_irq();
//   *Data = Published;
//   __set_PRIMASK(primask);
// }

void Class_External_IMU::Init(float yaw_offset)
{ 
    //EKF初始化
    IMU_QuaternionEKF_Init(10, 0.001, 10000000, 1, 0, yaw_offset, &QEKF_INS);
}

static float tmp_gravity_b[3];
void Class_External_IMU::TIM_Calculate_PeriodElapsedCallback(void)
{
  static uint8_t Tempture_Cnt_mod50 = 0;
    Tempture_Cnt_mod50++;

    INS_DWT_Dt = DWT_GetDeltaT(&INS_DWT_Count);
    INS_DWT_Dt_Sum += INS_DWT_Dt;

    // 本次同步的锁存, 暂时停用
    // uint32_t sample_trigger_cnt;
    // uint32_t sample_timestamp_us;
    // uint32_t primask = __get_PRIMASK();
    // __disable_irq();
    INS.Accel[0] = BMI088_Raw_Data.Accel[0];
    INS.Accel[1] = BMI088_Raw_Data.Accel[1];
    INS.Accel[2] = BMI088_Raw_Data.Accel[2];
    INS.Gyro[0] = BMI088_Raw_Data.Gyro[0];
    INS.Gyro[1] = BMI088_Raw_Data.Gyro[1];
    INS.Gyro[2] = BMI088_Raw_Data.Gyro[2];
    // sample_trigger_cnt = Trigger_Cnt;
    // sample_timestamp_us = Timestamp_us;
    // __set_PRIMASK(primask);

    // 核心函数,EKF更新四元数
    IMU_QuaternionEKF_Update(INS.Gyro[0], INS.Gyro[1], INS.Gyro[2], INS.Accel[0], INS.Accel[1], INS.Accel[2], INS_DWT_Dt, &QEKF_INS);

    memcpy(INS.q, QEKF_INS.q, sizeof(QEKF_INS.q));

    // 本次同步发布的姿态快照, 暂时停用
    // primask = __get_PRIMASK();
    // __disable_irq();
    // memcpy(Published.q, INS.q, sizeof(INS.q));
    // Published.Trigger_Cnt = sample_trigger_cnt;
    // Published.Timestamp_us = sample_timestamp_us;
    // __set_PRIMASK(primask);

    // 机体系基向量转换到导航坐标系，本例选取惯性系为导航系
    BodyFrameToEarthFrame(X_b, INS.xn, INS.q);
    BodyFrameToEarthFrame(Y_b, INS.yn, INS.q);
    BodyFrameToEarthFrame(Z_b, INS.zn, INS.q);

    // 将重力从导航坐标系n转换到机体系b,随后根据加速度计数据计算运动加速度
    
    EarthFrameToBodyFrame(Gravity, tmp_gravity_b, INS.q);
    for (uint8_t i = 0; i < 3; i++) // 同样过一个低通滤波
    {
        INS.MotionAccel_b[i] = (INS.Accel[i] - tmp_gravity_b[i]) * INS_DWT_Dt / (INS.AccelLPF + INS_DWT_Dt) + INS.MotionAccel_b[i] * INS.AccelLPF / (INS.AccelLPF + INS_DWT_Dt);
    }
    BodyFrameToEarthFrame(INS.MotionAccel_b, INS.MotionAccel_n, INS.q); // 转换回导航系n

    // 获取最终数据
    Get_Angle();

    // 2027基类getter读取Published_INS；外置IMU解算完成后同步发布，避免姿态恒为零。
    Published_INS = INS;
    Published_Sequence++;
    Published_Timestamp_ms = HAL_GetTick();
    Published_Started = 1;

    imu_start_flag = 1;
}