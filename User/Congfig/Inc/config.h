/**
 * @file config.h
 * @author cjw
 * @brief 工程配置文件
 * @version 0.1
 * @date 2025-07-1 0.1 26赛季定稿
 *
 * @copyright ZLLC 2026
 *
 */

#ifndef CONFIG_H
#define CONFIG_H

/* Includes ------------------------------------------------------------------*/

/* Exported macros -----------------------------------------------------------*/

/* EKF/IMU 热点存放方式：0 = 原来的 Flash + 动态内存；1 = ITCM + DTCM 静态工作区。
 * 同时控制 IMU 主处理函数、矩阵乘法/求逆、坐标变换及 Get_Angle 的 ITCM 版本。
 * 修改后重新编译并下载；两种模式都可在调试器观察 EKF_Timing（单位 us）。
 * 允许构建检查通过 -DEKF_USE_TCM=0/1 覆盖，日常只需修改这里。
 */
#ifndef EKF_USE_TCM
#define EKF_USE_TCM 1
#endif

#if (EKF_USE_TCM != 0) && (EKF_USE_TCM != 1)
#error "EKF_USE_TCM must be 0 or 1"
#endif

//底盘或云台状态
//#define CHASSIS
#define GIMBAL

//底盘标定参考正方向角度(数据来源yaw电机)
#define Reference_Angle  -52.29f
#define Reference_Radian  (Reference_Angle * PI / 180.f)

//调试或比赛状态
#define DEBUG
//#define BATTLE


//功率控制相关
#define POWER_LIMIT_JH
//#define POWER_LIMIT_GY
#define POWER_CONTROL 0 //启用功率控制
//#define BUFFER_LOOP

//#define Heat_Detect_ENABLE
#define Heat_Detect_DISABLE

#define Forward_Power

#define SuperCap 0

//遥控器选择
#define USE_VT13
//#define USE_DR16

/* 兵种/底盘类型选择*/
#define AGV      //舵轮底盘



/* Exported types ------------------------------------------------------------*/


/* Exported variables --------------------------------------------------------*/

/* Exported function declarations --------------------------------------------*/

#endif

/************************ COPYRIGHT(C) USTC-ROBOWALKER **************************/
