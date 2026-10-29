#include "cache_dma.h"

bool Tutorial_MpuDmaNonCacheable(void)
{
    if (((SCB->CCR & SCB_CCR_DC_Msk) != 0U) ||
        ((MPU->CTRL & MPU_CTRL_ENABLE_Msk) != 0U)) {
        return false;
    }

    /* Startup integration must reserve region 7. On this M7, a higher-numbered
     * overlapping region wins. Audit all MPU regions in the actual project. */
    MPU_Region_InitTypeDef region = {0};
    HAL_MPU_Disable();
    region.Enable = MPU_REGION_ENABLE;
    region.Number = MPU_REGION_NUMBER7;
    region.BaseAddress = TUTORIAL_DMA_BASE;
    region.Size = MPU_REGION_SIZE_32KB;
    region.SubRegionDisable = 0x00U;
    region.TypeExtField = MPU_TEX_LEVEL1;
    region.AccessPermission = MPU_REGION_FULL_ACCESS;
    region.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
    region.IsShareable = MPU_ACCESS_SHAREABLE;
    region.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
    region.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;
    /* TEX=1, C=0, B=0: Normal non-cacheable, not Device/Strongly-ordered.
     * S=1 does not add hardware DMA cache coherence. XN prevents execution. */
    HAL_MPU_ConfigRegion(&region);
    HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);
    __DSB();
    __ISB();
    return true;
}
