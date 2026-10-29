/* EXAMPLE FRAGMENT: include INSIDE the existing SECTIONS block. It is not a
 * replacement linker script. It uses the project's FLASH, ITCMRAM, DTCMRAM and
 * RAM_D2 MEMORY definitions. Do not enable it without matching startup code.
 * Existing .itcm_ekf/.dtcm_ekf sections and symbols remain independently owned.
 * All D2 SRAM is reserved for this example, including unused bytes. */

.tutorial_itcm : ALIGN(8)
{
    __tutorial_itcm_start = .;
    KEEP(*(.tutorial_itcm .tutorial_itcm.*))
    . = ALIGN(8);
    __tutorial_itcm_end = .;
} > ITCMRAM AT> FLASH
__tutorial_itcm_load = LOADADDR(.tutorial_itcm);

.tutorial_dtcm_data : ALIGN(8)
{
    __tutorial_dtcm_data_start = .;
    KEEP(*(.tutorial_dtcm_data .tutorial_dtcm_data.*))
    . = ALIGN(8);
    __tutorial_dtcm_data_end = .;
} > DTCMRAM AT> FLASH
__tutorial_dtcm_data_load = LOADADDR(.tutorial_dtcm_data);

.tutorial_dtcm_bss (NOLOAD) : ALIGN(8)
{
    __tutorial_dtcm_bss_start = .;
    KEEP(*(.tutorial_dtcm_bss .tutorial_dtcm_bss.*))
    . = ALIGN(8);
    __tutorial_dtcm_bss_end = .;
} > DTCMRAM

.tutorial_dma ORIGIN(RAM_D2) (NOLOAD) :
{
    __tutorial_dma_start = .;
    __tutorial_dma_cacheable_start = .;
    KEEP(*(.dma_cacheable .dma_cacheable.*))
    . = ALIGN(32);
    __tutorial_dma_cacheable_end = .;
    __tutorial_dma_nocache_start = .;
    KEEP(*(.dma_nocache .dma_nocache.*))
    . = ALIGN(32);
    __tutorial_dma_nocache_end = .;
    __tutorial_dma_used_end = .;
    /* Reserve the entire physical region, not just the currently used buffers. */
    __tutorial_dma_end = __tutorial_dma_start + LENGTH(RAM_D2);
    . = MAX(., __tutorial_dma_end);
} > RAM_D2

ASSERT(ORIGIN(RAM_D2) == 0x30000000 && LENGTH(RAM_D2) == 32K,
       "Tutorial DMA assumes the H723 32 KiB D2 SRAM layout")
ASSERT(__tutorial_dma_used_end <= __tutorial_dma_end,
       "Tutorial DMA buffers exceed reserved D2 SRAM")
ASSERT((__tutorial_dma_cacheable_end == __tutorial_dma_cacheable_start) ||
       (__tutorial_dma_nocache_end == __tutorial_dma_nocache_start),
       "Choose either manual cacheable DMA buffers or MPU noncacheable buffers")
