/**
 * @file ekf_tcm.h
 * @brief EKF 的可选 TCM 段属性。测试统计见 Device/dvc_performance.h。
 * @author HGCSJ
 * @version V1.1
 * @date 2026-10-30 27赛季定稿
 * @copyright ZLLC 2027
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

#endif /* EKF_TCM_H */
