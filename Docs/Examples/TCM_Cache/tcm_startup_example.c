#include "cache_dma.h"

/* These symbols exist only after adding tutorial_sections.ld to SECTIONS.
 * The initialized images are 8-byte aligned and padded in Flash. */
extern const uint64_t __tutorial_itcm_load[];
extern uint64_t __tutorial_itcm_start[];
extern uint64_t __tutorial_itcm_end[];
extern const uint32_t __tutorial_dtcm_data_load[];
extern uint32_t __tutorial_dtcm_data_start[];
extern uint32_t __tutorial_dtcm_data_end[];
extern uint32_t __tutorial_dtcm_bss_start[];
extern uint32_t __tutorial_dtcm_bss_end[];
extern const uint32_t g_pfnVectors[];

/* Keep the initializer in Flash. Volatile accesses prevent GCC from replacing
 * the loops with memcpy/memset before the C runtime exists. Do not enable LTO
 * for this translation unit; inspect its generated instructions after changes.
 * It uses no global state, heap, floating point or library calls. */
__attribute__((section(".text.Tutorial_TcmInit"), noinline,
               target("general-regs-only"),
               optimize("no-tree-loop-distribute-patterns")))
void Tutorial_TcmInit(void)
{
    /* Required before writing ITCM at address zero: exceptions must continue
     * fetching the Flash vector table, not a boot alias overlaid by ITCM. */
    SCB->VTOR = (uint32_t)(uintptr_t)g_pfnVectors;
    SCB->ITCMCR |= SCB_ITCMCR_EN_Msk;
    SCB->DTCMCR |= SCB_DTCMCR_EN_Msk;
    __DSB();
    __ISB();

    volatile uint64_t *instruction = __tutorial_itcm_start;
    const volatile uint64_t *image = __tutorial_itcm_load;
    while ((uintptr_t)instruction < (uintptr_t)__tutorial_itcm_end) {
        /* Full 64-bit stores initialize the ITCM ECC word. */
        *instruction++ = *image++;
    }

    volatile uint32_t *data = __tutorial_dtcm_data_start;
    const volatile uint32_t *initializer = __tutorial_dtcm_data_load;
    while ((uintptr_t)data < (uintptr_t)__tutorial_dtcm_data_end) {
        *data++ = *initializer++;
    }
    data = __tutorial_dtcm_bss_start;
    while ((uintptr_t)data < (uintptr_t)__tutorial_dtcm_bss_end) {
        *data++ = 0U;
    }
    __DSB();
    __ISB();
}
