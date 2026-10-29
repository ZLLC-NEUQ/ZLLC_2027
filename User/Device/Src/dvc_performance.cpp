/**
 * @file dvc_performance.cpp
 * @brief 测试统计统一实现；不初始化/复位 DWT，避免干扰 PID、EKF 等业务时基。
 * @author HGCSJ
 * @version V1.1
 * @date 2026-10-30 27赛季定稿
 * @copyright ZLLC 2027
 */
#include "dvc_performance.h"

#if PERFORMANCE_TEST_ENABLE

volatile Struct_Performance_Timing Control_Timing = {};
volatile Struct_Performance_Timing EKF_Timing = {};
volatile Struct_Performance_Timing IMU_Task_Timing = {};
volatile Struct_Performance_Timing Background_Timing = {};
volatile Struct_Performance_Timing Alive_Timing = {};
volatile Struct_Performance_Timing Chassis_Speed_Estimate_Timing = {};

volatile Struct_Performance_Frequency Control_Frequency = {};
volatile Struct_Performance_Frequency IMU_Frequency = {};
volatile Struct_Performance_Frequency Background_Frequency = {};
volatile Struct_Performance_Frequency Alive_Frequency = {};
volatile Struct_Performance_Frequency Chassis_UI_Frequency = {};
volatile Struct_Performance_Frequency CAN_Gimbal_Rx_Frequency = {};
volatile Struct_Performance_Frequency CAN_Yaw_Rx_Frequency = {};
volatile Struct_Performance_Frequency CAN_Pitch_Rx_Frequency = {};
volatile Struct_Performance_Frequency Referee_Rx_Frequency = {};
volatile Struct_Performance_Frequency Referee_Game_Status_Frequency = {};
volatile Struct_Performance_Frequency Referee_Robot_Status_Frequency = {};
volatile Struct_Performance_Frequency Referee_Power_Heat_Frequency = {};
volatile Struct_Performance_Frequency Referee_Tx_Frequency = {};
volatile Struct_Performance_Frequency MiniPC_Tx_Frequency = {};
volatile uint32_t ulControl_DeadlineMissed = 0U;
volatile Struct_Performance_Memory RTOS_Memory = {};

static TaskHandle_t memory_imu_task = NULL;
static TaskHandle_t memory_control_task = NULL;
static TaskHandle_t memory_background_task = NULL;
static TaskHandle_t memory_alive_task = NULL;
static TickType_t memory_last_sample_tick = 0U;
static uint32_t memory_has_sample = 0U;

void Performance_MemoryBind(TaskHandle_t imu, TaskHandle_t control, TaskHandle_t background)
{
    memory_imu_task = imu;
    memory_control_task = control;
    memory_background_task = background;
    memory_alive_task = NULL;
    memory_last_sample_tick = 0U;
    memory_has_sample = 0U;
    RTOS_Memory.sample_count = 0U;
    RTOS_Memory.timestamp_ms = 0U;
    RTOS_Memory.imu_stack_min_free_bytes = 0U;
    RTOS_Memory.control_stack_min_free_bytes = 0U;
    RTOS_Memory.background_stack_min_free_bytes = 0U;
    RTOS_Memory.alive_stack_min_free_bytes = 0U;
    RTOS_Memory.heap_free_bytes = 0U;
    RTOS_Memory.heap_min_free_bytes = 0U;
}

void Performance_MemoryBindAlive(TaskHandle_t alive)
{
    memory_alive_task = alive;
    RTOS_Memory.alive_stack_min_free_bytes = 0U;
}

void Performance_MemoryPoll(void)
{
    /* NULL 会被 FreeRTOS 当成当前任务；注册未完成时不查询，避免误报。 */
    if ((memory_imu_task == NULL) || (memory_control_task == NULL) ||
        (memory_background_task == NULL))
        return;

    const TickType_t now = xTaskGetTickCount();
    if ((memory_has_sample != 0U) &&
        ((TickType_t)(now - memory_last_sample_tick) < pdMS_TO_TICKS(1000U)))
        return;
    memory_last_sample_tick = now;
    memory_has_sample = 1U;

    /* 扫描栈会花费时间，因此只在后台执行，不关闭中断/调度，不创建额外任务。
     * 内核已记录历史低值，无需在这里再次维护 min。
     */
    RTOS_Memory.imu_stack_min_free_bytes =
        (uint32_t)uxTaskGetStackHighWaterMark(memory_imu_task) * sizeof(StackType_t);
    RTOS_Memory.control_stack_min_free_bytes =
        (uint32_t)uxTaskGetStackHighWaterMark(memory_control_task) * sizeof(StackType_t);
    RTOS_Memory.background_stack_min_free_bytes =
        (uint32_t)uxTaskGetStackHighWaterMark(memory_background_task) * sizeof(StackType_t);
    RTOS_Memory.alive_stack_min_free_bytes = (memory_alive_task != NULL) ?
        (uint32_t)uxTaskGetStackHighWaterMark(memory_alive_task) * sizeof(StackType_t) : 0U;
    RTOS_Memory.heap_free_bytes = (uint32_t)xPortGetFreeHeapSize();
    RTOS_Memory.heap_min_free_bytes = (uint32_t)xPortGetMinimumEverFreeHeapSize();
    RTOS_Memory.timestamp_ms = HAL_GetTick();
    if (RTOS_Memory.sample_count != UINT32_MAX)
        ++RTOS_Memory.sample_count;
}

void Performance_Prepare(volatile Struct_Performance_Timing *timing)
{
    if ((timing->reset_requested != 0U) || (timing->sample_count == UINT32_MAX) ||
        (timing->cpu_clock_hz != SystemCoreClock))
    {
        timing->cpu_clock_hz = SystemCoreClock;
        timing->sample_count = 0U;
        timing->last_cycles = 0U;
        timing->last_us = 0.0f;
        timing->avg_us = 0.0f;
        timing->min_us = 0.0f;
        timing->max_us = 0.0f;
        timing->_total_cycles = 0U;
        timing->reset_requested = 0U;
    }
    timing->icache_enabled = (SCB->CCR & SCB_CCR_IC_Msk) != 0U;
    timing->use_tcm = EKF_USE_TCM;
}

void Performance_Record(volatile Struct_Performance_Timing *timing, uint32_t cycles)
{
    const uint32_t clock_hz = timing->cpu_clock_hz;
    if (clock_hz == 0U)
        return;
    const uint32_t count = timing->sample_count + 1U;
    const float cycles_to_us = 1000000.0f / (float)clock_hz;
    const float elapsed_us = (float)cycles * cycles_to_us;
    timing->_total_cycles += cycles;
    timing->last_cycles = cycles;
    timing->last_us = elapsed_us;
    timing->avg_us = ((float)timing->_total_cycles / (float)count) * cycles_to_us;
    if ((count == 1U) || (elapsed_us < timing->min_us))
        timing->min_us = elapsed_us;
    if ((count == 1U) || (elapsed_us > timing->max_us))
        timing->max_us = elapsed_us;
    timing->sample_count = count;
}

static void Performance_ResetFrequency(volatile Struct_Performance_Frequency *frequency,
                                       uint32_t clock_hz)
{
    frequency->cpu_clock_hz = clock_hz;
    frequency->sample_count = 0U;
    frequency->last_cycles = 0U;
    frequency->last_us = 0.0f;
    frequency->avg_us = 0.0f;
    frequency->min_us = 0.0f;
    frequency->max_us = 0.0f;
    frequency->last_hz = 0.0f;
    frequency->avg_hz = 0.0f;
    frequency->min_hz = 0.0f;
    frequency->max_hz = 0.0f;
    frequency->_total_cycles = 0U;
    frequency->_has_previous = 0U;
    frequency->reset_requested = 0U;
}

void Performance_Event(volatile Struct_Performance_Frequency *frequency,
                       uint32_t cycle, uint32_t tick_ms)
{
    const uint32_t clock_hz = SystemCoreClock;
    if ((frequency->reset_requested != 0U) || (frequency->sample_count == UINT32_MAX) ||
        (frequency->cpu_clock_hz != clock_hz))
        Performance_ResetFrequency(frequency, clock_hz);
    if (clock_hz < 1000U)
        return;

    const uint32_t elapsed_ms = tick_ms - frequency->_last_tick_ms;
    const uint32_t cycles = cycle - frequency->_last_cycle;
    const uint32_t wrap_ms = UINT32_MAX / (clock_hz / 1000U);
    const uint32_t has_previous = frequency->_has_previous;
    frequency->_last_cycle = cycle;
    frequency->_last_tick_ms = tick_ms;
    frequency->_has_previous = 1U;
    /* ms 只用于排除多圈回绕；精确周期始终来自 CYCCNT。
     * 接近回绕时保守丢弃该间隔（480MHz 约 8.947s）。
     */
    if ((has_previous == 0U) || (elapsed_ms >= wrap_ms))
    {
        frequency->last_cycles = 0U;
        frequency->last_us = 0.0f;
        frequency->last_hz = 0.0f;
        return;
    }
    if (cycles == 0U)
        return;

    const uint32_t count = frequency->sample_count + 1U;
    const float elapsed_us = (float)cycles * (1000000.0f / (float)clock_hz);
    const float hz = (float)clock_hz / (float)cycles;
    frequency->_total_cycles += cycles;
    frequency->last_cycles = cycles;
    frequency->last_us = elapsed_us;
    frequency->last_hz = hz;
    frequency->avg_us = ((float)frequency->_total_cycles / (float)count) *
                        (1000000.0f / (float)clock_hz);
    frequency->avg_hz = ((float)count * (float)clock_hz) / (float)frequency->_total_cycles;
    if ((count == 1U) || (elapsed_us < frequency->min_us))
        frequency->min_us = elapsed_us;
    if ((count == 1U) || (elapsed_us > frequency->max_us))
        frequency->max_us = elapsed_us;
    if ((count == 1U) || (hz < frequency->min_hz))
        frequency->min_hz = hz;
    if ((count == 1U) || (hz > frequency->max_hz))
        frequency->max_hz = hz;
    frequency->sample_count = count;
}

#endif /* PERFORMANCE_TEST_ENABLE */
