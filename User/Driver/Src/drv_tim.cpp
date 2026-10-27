/**
 * @file drv_tim.cpp
 * @author cjw by yssickjgd
 * @brief TIM定时器初始化与中断处理（FreeRTOS 版本）
 * @version 1.0
 * @date 2026-07-28
 *
 * @copyright ZLLC 2026
 *
 * @note
 *  本文件在裸机版本基础上改造：
 *  1. 原 TIM4/TIM5 周期任务由 FreeRTOS 延时调度，不再使用这两个定时器中断。
 *  2. TIM6 作为 HAL 时基，1ms 调用 HAL_IncTick()，替代原 SysTick 时基。
 *  3. 保留其他定时器的回调注册机制，供未来扩展。
 */

/* Includes ------------------------------------------------------------------*/

#include "drv_tim.h"

/* Private macros ------------------------------------------------------------*/

/* Private types -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/

Struct_TIM_Manage_Object TIM1_Manage_Object;
Struct_TIM_Manage_Object TIM2_Manage_Object;
Struct_TIM_Manage_Object TIM3_Manage_Object;
Struct_TIM_Manage_Object TIM4_Manage_Object;
Struct_TIM_Manage_Object TIM5_Manage_Object;
Struct_TIM_Manage_Object TIM6_Manage_Object;
Struct_TIM_Manage_Object TIM7_Manage_Object;
Struct_TIM_Manage_Object TIM8_Manage_Object;
Struct_TIM_Manage_Object TIM9_Manage_Object;
Struct_TIM_Manage_Object TIM10_Manage_Object;
Struct_TIM_Manage_Object TIM11_Manage_Object;
Struct_TIM_Manage_Object TIM12_Manage_Object;
Struct_TIM_Manage_Object TIM13_Manage_Object;
Struct_TIM_Manage_Object TIM14_Manage_Object;

/** @brief HAL 时基使用的 TIM6 句柄（覆盖 HAL 弱定义） */
static TIM_HandleTypeDef TimHandle;

/* Private function declarations ---------------------------------------------*/

/* function prototypes -------------------------------------------------------*/

/**
 * @brief 初始化TIM定时器
 *
 * @param htim 定时器编号
 * @param Callback_Function 处理回调函数
 */
void TIM_Init(TIM_HandleTypeDef *htim, TIM_Call_Back Callback_Function)
{
    if (htim->Instance == TIM1)
    {
        TIM1_Manage_Object.TIM_Handler = htim;
        TIM1_Manage_Object.Callback_Function = Callback_Function;
    }
    else if (htim->Instance == TIM2)
    {
        TIM2_Manage_Object.TIM_Handler = htim;
        TIM2_Manage_Object.Callback_Function = Callback_Function;
    }
    else if (htim->Instance == TIM3)
    {
        TIM3_Manage_Object.TIM_Handler = htim;
        TIM3_Manage_Object.Callback_Function = Callback_Function;
    }
    else if (htim->Instance == TIM4)
    {
        TIM4_Manage_Object.TIM_Handler = htim;
        TIM4_Manage_Object.Callback_Function = Callback_Function;
    }
    else if (htim->Instance == TIM5)
    {
        TIM5_Manage_Object.TIM_Handler = htim;
        TIM5_Manage_Object.Callback_Function = Callback_Function;
    }
    else if (htim->Instance == TIM6)
    {
        TIM6_Manage_Object.TIM_Handler = htim;
        TIM6_Manage_Object.Callback_Function = Callback_Function;
    }
    else if (htim->Instance == TIM7)
    {
        TIM7_Manage_Object.TIM_Handler = htim;
        TIM7_Manage_Object.Callback_Function = Callback_Function;
    }
    else if (htim->Instance == TIM8)
    {
        TIM8_Manage_Object.TIM_Handler = htim;
        TIM8_Manage_Object.Callback_Function = Callback_Function;
    }
}

/**
 * @brief HAL库TIM定时器中断回调（FreeRTOS 版本）
 *
 * @param htim TIM编号
 *
 * @note
 *  - TIM6: HAL 时基，调用 HAL_IncTick()
 *  - 其他: 调用注册的传统回调（保留兼容）
 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM6)
    {
        HAL_IncTick();
    }
    else if (htim->Instance == TIM1)
    {
        TIM1_Manage_Object.Callback_Function();
    }
    else if (htim->Instance == TIM2)
    {
        TIM2_Manage_Object.Callback_Function();
    }
    else if (htim->Instance == TIM3)
    {
        TIM3_Manage_Object.Callback_Function();
    }
    else if (htim->Instance == TIM7)
    {
        TIM7_Manage_Object.Callback_Function();
    }
    else if (htim->Instance == TIM8)
    {
        TIM8_Manage_Object.Callback_Function();
    }
}

/**
 * @brief 配置 TIM6 作为 HAL 时基
 *
 * @param TickPriority HAL tick 中断优先级
 * @retval HAL status
 *
 * @note
 *  覆盖 HAL 的弱定义 HAL_InitTick()。SysTick 已交给 FreeRTOS，
 *  HAL 的 1ms tick 由 TIM6 提供。
 */
extern "C" HAL_StatusTypeDef HAL_InitTick(uint32_t TickPriority)
{
    RCC_ClkInitTypeDef clkconfig;
    uint32_t uwTimclock;
    uint32_t uwPrescalerValue;
    uint32_t pFLatency;

    /* 设置 TIM6 中断优先级并使能 */
    if (TickPriority < (1UL << __NVIC_PRIO_BITS))
    {
        HAL_NVIC_SetPriority(TIM6_DAC_IRQn, TickPriority, 0U);
        HAL_NVIC_EnableIRQ(TIM6_DAC_IRQn);
        uwTickPrio = TickPriority;
    }
    else
    {
        return HAL_ERROR;
    }

    /* 使能 TIM6 时钟 */
    __HAL_RCC_TIM6_CLK_ENABLE();

    /* 获取当前时钟配置，计算 TIM6 输入时钟 */
    HAL_RCC_GetClockConfig(&clkconfig, &pFLatency);
    if (clkconfig.APB1CLKDivider == RCC_HCLK_DIV1)
    {
        uwTimclock = HAL_RCC_GetPCLK1Freq();
    }
    else
    {
        uwTimclock = 2UL * HAL_RCC_GetPCLK1Freq();
    }

    /* 预分频到 1MHz，周期 1ms */
    uwPrescalerValue = (uint32_t)((uwTimclock / 1000000U) - 1U);

    TimHandle.Instance = TIM6;
    TimHandle.Init.Period = (1000000U / 1000U) - 1U;
    TimHandle.Init.Prescaler = uwPrescalerValue;
    TimHandle.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    TimHandle.Init.CounterMode = TIM_COUNTERMODE_UP;
    TimHandle.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

    if (HAL_TIM_Base_Init(&TimHandle) == HAL_OK)
    {
        return HAL_TIM_Base_Start_IT(&TimHandle);
    }

    return HAL_ERROR;
}

/**
 * @brief 暂停 HAL tick
 */
extern "C" void HAL_SuspendTick(void)
{
    __HAL_TIM_DISABLE_IT(&TimHandle, TIM_IT_UPDATE);
}

/**
 * @brief 恢复 HAL tick
 */
extern "C" void HAL_ResumeTick(void)
{
    __HAL_TIM_ENABLE_IT(&TimHandle, TIM_IT_UPDATE);
}

/**
 * @brief TIM6 中断服务函数
 */
extern "C" void TIM6_DAC_IRQHandler(void)
{
    HAL_TIM_IRQHandler(&TimHandle);
}

/************************ COPYRIGHT(C) USTC-ROBOWALKER **************************/
