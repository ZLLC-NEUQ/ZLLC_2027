/**
 * @file ekf_tcm.h
 * @brief EKF 的可选 TCM 段属性及调试计时接口。
 */
#ifndef EKF_TCM_H
#define EKF_TCM_H

#include <stdint.h>
#include "config.h"

/* 只指定存放位置，保留两种模式相同的内联优化策略；宏为 0 时使用原段布局。
 * 当前工程关闭 LTO，跨文件的 EKF 入口保持独立，内部内联代码随调用者进入 ITCM。
 */
#ifndef EKF_ITCM_CODE
#if EKF_USE_TCM
#define EKF_ITCM_CODE __attribute__((section(".itcm_ekf")))
#else
#define EKF_ITCM_CODE
#endif
#endif

#ifndef EKF_DTCM_DATA
#if EKF_USE_TCM
#define EKF_DTCM_DATA __attribute__((section(".bss.dtcm_ekf"), aligned(8)))
#else
#define EKF_DTCM_DATA
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 在调试器 Watch 中添加 EKF_Timing。
 * 计时范围仅为 IMU_QuaternionEKF_Update，不包括 SPI 采样和统计开销。
 * 保留正常中断，耗时包含该区间的中断抢占；CYCCNT 无符号相减支持回绕。
 * 修改宏、重新下载后统计重新开始；不会保留另一份固件的历史结果。
 */
typedef struct
{
    uint32_t use_tcm;          /**< 0 原模式，1 TCM 模式。 */
    uint32_t sample_count;
    uint32_t last_cycles;
    float last_us;
    float avg_us;
    float min_us;
    float max_us;
    uint32_t reset_requested;  /**< 在 Watch 写 1，下次更新前清零统计。 */
} Struct_EKF_Timing;

extern volatile Struct_EKF_Timing EKF_Timing;

#ifdef __cplusplus
}
#endif

#endif /* EKF_TCM_H */
