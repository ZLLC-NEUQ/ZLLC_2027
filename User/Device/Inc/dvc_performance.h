/**
 * @file dvc_performance.h
 * @brief 可裁剪的耗时、频率及 RTOS 内存余量统计；业务时基仍由 dvc_dwt 提供。
 * @author HGCSJ
 * @version V1.1
 * @date 2026-10-30 27赛季定稿
 * @copyright ZLLC 2027
 */
#ifndef DVC_PERFORMANCE_H
#define DVC_PERFORMANCE_H

#include "config.h"
#include <stdint.h>

#if PERFORMANCE_TEST_ENABLE
#include "main.h"
#include "FreeRTOS.h"
#include "task.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 每个对象只能由一个任务或一种中断写入；Watch 仅写 reset_requested=1。
 * 耗时单位 us，包含被测区间内的中断/高优先级任务抢占，统计开销在区间外。
 * 内部累计字段由模块管理，不能在 Watch 中单独修改。
 */
typedef struct
{
    uint32_t cpu_clock_hz;
    uint32_t icache_enabled;      /**< 实际 SCB I-Cache 状态。 */
    uint32_t use_tcm;             /**< EKF_USE_TCM 配置值，不代表链接地址检测。 */
    uint32_t sample_count;
    uint32_t last_cycles;
    float last_us;
    float avg_us;
    float min_us;
    float max_us;
    uint32_t reset_requested;
    uint64_t _total_cycles;
} Struct_Performance_Timing;

/** 连续事件间隔；第一帧只建立基准，sample_count 是有效间隔数。
 * avg_hz = 有效间隔数 / 累计间隔秒数，不是瞬时频率的算术平均。
 * 超过 CYCCNT 回绕周期的长时间离线会重建基准，不计入平均值。
 */
typedef struct
{
    uint32_t cpu_clock_hz;
    uint32_t sample_count;
    uint32_t last_cycles;
    float last_us;
    float avg_us;
    float min_us;
    float max_us;
    float last_hz;
    float avg_hz;
    float min_hz;
    float max_hz;
    uint32_t reset_requested;
    uint64_t _total_cycles;
    uint32_t _last_cycle;
    uint32_t _last_tick_ms;
    uint32_t _has_previous;
} Struct_Performance_Frequency;

/** 后台低频读取的内核内存统计，所有余量单位均为字节。
 * 栈余量为任务创建以来未被覆盖的最小空间，堆余量仅覆盖 FreeRTOS heap_4。
 * 内核维护历史最低值，不受耗时对象的 reset_requested 影响。
 */
typedef struct
{
    uint32_t sample_count;
    uint32_t timestamp_ms;                   /**< 最近一次采集完成时的 HAL 时间。 */
    uint32_t imu_stack_min_free_bytes;
    uint32_t control_stack_min_free_bytes;
    uint32_t background_stack_min_free_bytes;
    uint32_t alive_stack_min_free_bytes;      /**< 未绑定在线检测任务时为 0，不查询当前任务。 */
    uint32_t heap_free_bytes;
    uint32_t heap_min_free_bytes;
} Struct_Performance_Memory;

/* 保留原来的两个 Watch 名称；所有测试对象统一在新 cpp 内定义。 */
extern volatile Struct_Performance_Timing Control_Timing;
extern volatile Struct_Performance_Timing EKF_Timing;
extern volatile Struct_Performance_Timing IMU_Task_Timing;
extern volatile Struct_Performance_Timing Background_Timing;
extern volatile Struct_Performance_Timing Alive_Timing;
extern volatile Struct_Performance_Timing Chassis_Speed_Estimate_Timing;

extern volatile Struct_Performance_Frequency Control_Frequency;
extern volatile Struct_Performance_Frequency IMU_Frequency;
extern volatile Struct_Performance_Frequency Background_Frequency;
extern volatile Struct_Performance_Frequency Alive_Frequency;
extern volatile Struct_Performance_Frequency Chassis_UI_Frequency;
extern volatile Struct_Performance_Frequency CAN_Gimbal_Rx_Frequency;
extern volatile Struct_Performance_Frequency CAN_Yaw_Rx_Frequency;
extern volatile Struct_Performance_Frequency CAN_Pitch_Rx_Frequency;
extern volatile Struct_Performance_Frequency Referee_Rx_Frequency;
extern volatile Struct_Performance_Frequency Referee_Game_Status_Frequency;
extern volatile Struct_Performance_Frequency Referee_Robot_Status_Frequency;
extern volatile Struct_Performance_Frequency Referee_Power_Heat_Frequency;
extern volatile Struct_Performance_Frequency Referee_Tx_Frequency;
extern volatile Struct_Performance_Frequency MiniPC_Tx_Frequency;
extern volatile uint32_t ulControl_DeadlineMissed;
extern volatile Struct_Performance_Memory RTOS_Memory;

void Performance_Prepare(volatile Struct_Performance_Timing *timing);
void Performance_Record(volatile Struct_Performance_Timing *timing, uint32_t cycles);
void Performance_Event(volatile Struct_Performance_Frequency *frequency,
                       uint32_t cycle, uint32_t tick_ms);
/** 创建全部任务后、启动调度器前注册，句柄在轮询期间必须保持有效。
 * 清除发布状态及可选的 Alive 绑定，不重置内核保存的历史最低值。
 * 如需监控 Alive 栈，应在本调用之后再调用 Performance_MemoryBindAlive。
 */
void Performance_MemoryBind(TaskHandle_t imu, TaskHandle_t control, TaskHandle_t background);
/** 三个基础任务绑定后，可选注册在线检测任务；NULL 表示不采集该任务。 */
void Performance_MemoryBindAlive(TaskHandle_t alive);
/** 仅后台任务调用：首次立即采集，此后每 1000ms 至多采集一次。 */
void Performance_MemoryPoll(void);

#ifdef __cplusplus
}
#endif

/* 成对放在同一作用域内，不能用于无花括号的 if 分支。
 * 原始 CYCCNT 在调用点采样，避免把记录函数调用计入自身耗时。
 */
#define PERF_START(name) \
    Performance_Prepare(&(name)); \
    __DSB(); \
    __ISB(); \
    const uint32_t name##_start_cycles = DWT->CYCCNT
#define PERF_STOP(name) do { \
    __DSB(); \
    const uint32_t perf_elapsed_cycles = DWT->CYCCNT - name##_start_cycles; \
    Performance_Record(&(name), perf_elapsed_cycles); \
} while (0)
#define PERF_EVENT(name) \
    Performance_Event(&(name), DWT->CYCCNT, HAL_GetTick())
#define PERF_EVENT_AT(name, cycle) \
    Performance_Event(&(name), (cycle), HAL_GetTick())
#define PERF_CONTROL_DEADLINE_MISSED() (++ulControl_DeadlineMissed)
#define PERF_TASK_HANDLE_PTR(handle) (&(handle))
#define PERF_MEMORY_BIND(imu, control, background) \
    Performance_MemoryBind((imu), (control), (background))
#define PERF_MEMORY_BIND_ALIVE(alive) Performance_MemoryBindAlive((alive))
#define PERF_MEMORY_POLL() Performance_MemoryPoll()

#else

/* 关闭后连参数求值、采样、屏障和局部变量也不会进入固件。 */
#define PERF_START(name) ((void)0)
#define PERF_STOP(name) ((void)0)
#define PERF_EVENT(name) ((void)0)
#define PERF_EVENT_AT(name, cycle) ((void)0)
#define PERF_CONTROL_DEADLINE_MISSED() ((void)0)
#define PERF_TASK_HANDLE_PTR(handle) 0
#define PERF_MEMORY_BIND(imu, control, background) ((void)0)
#define PERF_MEMORY_BIND_ALIVE(alive) ((void)0)
#define PERF_MEMORY_POLL() ((void)0)

#endif /* PERFORMANCE_TEST_ENABLE */
#endif /* DVC_PERFORMANCE_H */
