/** VQF shares the existing TCM linker regions and startup initialization. */
#ifndef VQF_TCM_H
#define VQF_TCM_H

#include "config.h"

#if VQF_USE_TCM
#define VQF_ITCM_CODE __attribute__((section(".itcm_ekf.vqf")))
#define VQF_DTCM_DATA __attribute__((section(".bss.dtcm_ekf.vqf"), aligned(8)))
#else
#define VQF_ITCM_CODE
#define VQF_DTCM_DATA
#endif

/* Common IMU functions follow the selected backend's placement switch. */
#if (IMU_USE_VQF && VQF_USE_TCM) || (!IMU_USE_VQF && EKF_USE_TCM)
#define IMU_ITCM_CODE __attribute__((section(".itcm_ekf.imu")))
#else
#define IMU_ITCM_CODE
#endif

#endif
