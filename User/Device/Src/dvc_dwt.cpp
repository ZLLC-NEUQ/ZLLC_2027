/**
 * @file dvc_dwt.cpp
 * @author cjw
 * @brief DWT
 * @version 0.1
 * @date 2025-07-1 0.1 26赛季定稿
 *
 * @copyright ZLLC 2026
 *
 */

#include "dvc_dwt.h"

/** @brief 原子采集计数器并扩展回绕，只保护这几个共享计数值。 */
static uint64_t DWT_ReadCycles(void)
{
    const uint32_t saved_primask = __get_PRIMASK();
    __disable_irq();

    const uint32_t cnt_now = DWT->CYCCNT;
    if (cnt_now < CYCCNT_LAST)
    {
        ++CYCCNT_RountCount;
    }
    CYCCNT_LAST = cnt_now;
    const uint64_t cycles = ((uint64_t)CYCCNT_RountCount << 32) | cnt_now;

    __set_PRIMASK(saved_primask);
    return cycles;
}

/** @brief 浮点和除法在临界区外计算，发布时拒绝被抢占后恢复的旧样本。 */
static DWT_Time_t DWT_ReadTime(void)
{
    const uint64_t cycles = DWT_ReadCycles();
    const uint64_t seconds = cycles / CPU_FREQ_Hz;
    const uint64_t remainder = cycles - seconds * CPU_FREQ_Hz;
    DWT_Time_t time;
    time.s = (uint32_t)seconds;
    time.ms = (uint16_t)(remainder / CPU_FREQ_Hz_ms);
    time.us = (uint16_t)((remainder - (uint64_t)time.ms * CPU_FREQ_Hz_ms) / CPU_FREQ_Hz_us);

    const uint32_t saved_primask = __get_PRIMASK();
    __disable_irq();
    if (cycles >= CYCCNT64)
    {
        CYCCNT64 = cycles;
        SysTime = time;
    }
    __set_PRIMASK(saved_primask);

    return time;
}

float DWT_GetDeltaT(uint32_t *cnt_last)
{
    const uint32_t cnt_now = (uint32_t)DWT_ReadCycles();
    float dt = ((uint32_t)(cnt_now - *cnt_last)) / ((float)(CPU_FREQ_Hz));
    *cnt_last = cnt_now;

    return dt;
}

double DWT_GetDeltaT64(uint32_t *cnt_last)
{
    const uint32_t cnt_now = (uint32_t)DWT_ReadCycles();
    double dt = ((uint32_t)(cnt_now - *cnt_last)) / ((double)(CPU_FREQ_Hz));
    *cnt_last = cnt_now;

    return dt;
}




void DWT_SysTimeUpdate(void){
    (void)DWT_ReadTime();
}

float DWT_GetTimeline_s(void)
{
    const DWT_Time_t time = DWT_ReadTime();
    return time.s + time.ms * 0.001f + time.us * 0.000001f;
}

float DWT_GetTimeline_ms(void)
{
    const DWT_Time_t time = DWT_ReadTime();
    return time.s * 1000.0f + time.ms + time.us * 0.001f;
}

uint64_t DWT_GetTimeline_us(void)
{
    const DWT_Time_t time = DWT_ReadTime();
    return (uint64_t)time.s * 1000000ULL + (uint64_t)time.ms * 1000ULL + time.us;
}

void DWT_Delay(float Delay)
{
    uint32_t tickstart = DWT->CYCCNT;
    float wait = Delay;

    while ((DWT->CYCCNT - tickstart) < wait * (float)CPU_FREQ_Hz)
    {
    }
}
