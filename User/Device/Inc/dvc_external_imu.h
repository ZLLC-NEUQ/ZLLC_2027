#ifndef __DVC_EXTERNAL_IMU_H
#define __DVC_EXTERNAL_IMU_H

#include "dvc_imu.h"

// 本次同步扩展的结构体, 暂时停用
// struct Struct_IMU_Trigger
// {
//   float q[4];
//   uint32_t Trigger_Cnt;   // 第几帧, 1000Hz 递增
//   uint32_t Timestamp_us;  // 该帧到达时刻, 上电起算
// };


class Class_External_IMU : public Class_IMU {

  public:
    // 本次同步扩展, 暂时停用
    // uint32_t Trigger_Cnt = 0;
    // uint32_t Timestamp_us = 0;
    // Struct_IMU_Trigger Published = {};
    // void Get_Trigger_Data(Struct_IMU_Trigger *Data);
    void Init(float yaw_offset);
    void TIM_Calculate_PeriodElapsedCallback(void);
    void UART_BMI_Data_Process(uint8_t *Buffer);
    // void UART_BMI_Data_Process(uint8_t *Buffer, uint16_t Length);
    void TIM1msMod50_Alive_PeriodElapsedCallback(void);
  
  protected:
    // 2027 的 Class_IMU 已移除该成员, 由外置 IMU 自行累计
    float INS_DWT_Dt_Sum = 0.0f;

  private:
    uint32_t Flag;
    uint32_t Pre_Flag;

};


#endif