/**
 * @file ekf_matrix_tcm.inc
 * @brief EKF 专用的 ITCM 矩阵乘法、求逆实现。
 *
 * 直接编译仓库现有 CMSIS-DSP 源码，不维护第二份矩阵数学公式。
 * Keil RTE 可能已直接编译 MatrixFunctions.c，故这里使用独立符号，
 * 仅通过 kalman_filter.h 的 Matrix_Multiply/Matrix_Inverse 切换 EKF。
 * 由已有的 kalman_filter.cpp 包含，不需要在 Keil 工程新增编译文件。
 * 宏关闭时本文件为空，原 CMSIS 库/组件及其调用路径保持不变。
 */

#if EKF_USE_TCM

/* 前置声明上的 EKF_ITCM_CODE 同时作用于以下原样引用的函数定义。 */
#define arm_mat_mult_f32 EKF_arm_mat_mult_f32
#define arm_mat_inverse_f32 EKF_arm_mat_inverse_f32

extern "C"
{
#include "../../../Drivers/CMSIS/DSP/Source/MatrixFunctions/arm_mat_mult_f32.c"
#include "../../../Drivers/CMSIS/DSP/Source/MatrixFunctions/arm_mat_inverse_f32.c"
}

#undef arm_mat_mult_f32
#undef arm_mat_inverse_f32

#endif /* EKF_USE_TCM */
