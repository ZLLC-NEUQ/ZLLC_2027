/**
 * @file config.h
 * @author HGCSJ
 * @brief 工程配置文件
 * @version V1.1
 * @date 2026-10-30 27赛季定稿
 *
 * @copyright ZLLC 2027
 *
 */

#ifndef CONFIG_H
#define CONFIG_H

/* Includes ------------------------------------------------------------------*/

/* Exported macros -----------------------------------------------------------*/

/* 性能测试裁剪：1 = 耗时/周期/频率统计及 Watch 变量；0 = 完全移除测试插桩。
 * 业务使用的 DWT 时基、PID/EKF dt、任务调度和硬件延时不受此开关影响。
 * 修改后重新编译、下载；也允许通过 -DPERFORMANCE_TEST_ENABLE=0/1 覆盖。
 */
#ifndef PERFORMANCE_TEST_ENABLE
#define PERFORMANCE_TEST_ENABLE 1
#endif

#if (PERFORMANCE_TEST_ENABLE != 0) && (PERFORMANCE_TEST_ENABLE != 1)
#error "PERFORMANCE_TEST_ENABLE must be 0 or 1"
#endif

/* EKF/IMU 热点存放方式：0 = 原来的 Flash + 动态内存；1 = ITCM + DTCM 静态工作区。
 * 同时控制 IMU 主处理函数、矩阵乘法/求逆、坐标变换及 Get_Angle 的 ITCM 版本。
 * 修改后重新编译并下载；开启性能测试时可观察 EKF_Timing（单位 us）。
 * 允许构建检查通过 -DEKF_USE_TCM=0/1 覆盖，日常只需修改这里。
 */
#ifndef EKF_USE_TCM
#define EKF_USE_TCM 1
#endif

#if (EKF_USE_TCM != 0) && (EKF_USE_TCM != 1)
#error "EKF_USE_TCM must be 0 or 1"
#endif

//底盘或云台状态
#define CHASSIS
//#define GIMBAL

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

/* 兵种/底盘类型选择*/
#define AGV      //舵轮底盘



/* Exported types ------------------------------------------------------------*/


/* Exported variables --------------------------------------------------------*/

/* Exported function declarations --------------------------------------------*/

#endif

/************************ COPYRIGHT(C) USTC-ROBOWALKER **************************/
