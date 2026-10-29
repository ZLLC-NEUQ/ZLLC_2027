# STM32H723 TCM / Cache 参考代码

这些文件是独立教学示例，不会自动加入当前固件。目标是 STM32H723VGTx、GNU Arm GCC、本仓库的 CMSIS/HAL。这里只覆盖 **DMA1/DMA2 + D2 SRAM + 普通模式 8 位 UART DMA**；不直接覆盖 MDMA、BDMA、循环 DMA、UART IDLE、双缓冲、USB、Ethernet 描述符或其他 H7 型号。

## 文件和入口

| 文件 | 作用 |
| --- | --- |
| `cache_dma.h` | 所有示例的函数声明、段属性和使用约定 |
| `cache_dma.c` | D2 SRAM 时钟和 ECC 初始化，TX Clean、RX 前后 Invalidate |
| `cache_dma_uart_example.c` | 普通模式 UART DMA 所有权、持久缓冲、回调和失败恢复 |
| `mpu_dma_example.c` | 另一种方案：整个 D2 SRAM 配为 Normal non-cacheable |
| `tutorial_sections.ld` | GNU ld 段布局片段，Flash LMA、TCM VMA、D2 NOLOAD 保留区 |
| `tcm_startup_example.c` | 可选启动复制/清零函数，与链接片段严格配套 |

## 先选择一种 DMA 内存策略

**方案 A：D2 SRAM 可缓存，手动维护。** 使用 `cache_dma.c` 和 UART 示例。每个 DMA 分配起点按 32 字节对齐，容量为 32 的倍数，所有缓存行只能由该缓冲区占用。CPU 与 DMA 交接整个分配的所有权；DMA 运行期间，CPU、任务、ISR 都不能读写其内容。`length` 是实际字节数，维护范围向上取整，但不会超出 `capacity`。

**方案 B：D2 SRAM 不缓存。** 启动时调用 `Tutorial_MpuDmaNonCacheable()`，把 `0x30000000–0x30007FFF` 整个 32 KiB 划为 Normal、non-cacheable、shareable、XN。示例占用 MPU 区域 7。采用 `.dma_nocache` 缓冲，直接使用 HAL DMA；在启动 DMA 前以及完成确认后使用 `__DSB()`，仍须遵守所有权和任务同步。不要同时使用手动维护 UART 示例。MPU 并不能让 DMA1/DMA2 访问 DTCM，也不会消除总线排序要求。

链接片段保留整个 D2 SRAM，并通过 ASSERT 拒绝同时出现 `.dma_cacheable` 与 `.dma_nocache` 分配。其他段、堆、栈、共享驱动都不能再使用该区域。若工程需要复用此内存，必须重新设计内存布局及 MPU 区域，不能直接套用本例。

## SRAM 和 Cache 启动顺序

先检查链接布局与所有现有驱动，确认整个 D2 SRAM 确实可以交给示例。在新工程中，主函数早期可以采用下面的顺序：

```c
#include "cache_dma.h"

/* After normal .data/.bss initialization; no active DMA yet. */
if (!Tutorial_DmaSramInit()) {
    Error_Handler();
}
/* For strategy B ONLY, before enabling D-cache: */
/* if (!Tutorial_MpuDmaNonCacheable()) { Error_Handler(); } */
SCB_EnableICache();
SCB_EnableDCache();
```

`Tutorial_DmaSramInit()` 开启 D2 SRAM1/SRAM2 两个时钟，用对齐 32 位写入初始化整个区域，随后执行 DSB。这样 NOLOAD 区在字节访问和 DMA 前已有确定内容及 ECC。它会破坏整个区域的旧数据，必须在 D-cache 关闭且没有任何使用者时调用一次；不能用作运行期的“清空缓冲”函数。本函数不负责打开 DMA、DMAMUX、UART 时钟，它们由 CubeMX/HAL 外设初始化负责。

不要在活动 DMA 期间开关 D-cache 或更改 MPU。MPU 示例在 D-cache 或 MPU 已启用时返回 false，避免把启动配置误用于运行期。已有 MPU 布局时应把相同属性合并到统一配置，并检查重叠区域优先级，不应直接绕过检查。

## 手动维护 UART 示例接入

1. 配置一个专用 UART 为 8 位、无校验、TX/RX 模式；TX/RX 使用独立 DMA1/2 流。内存地址递增、外设地址固定、两侧按字节传输、Normal 模式、single burst。
2. 配好 DMA/DMAMUX 请求及 UART/DMA IRQ，在初始化完成且 UART 尚未使用时调用 `Tutorial_UartBind(&huartX)`。此函数只能成功绑定一次；其他驱动不得再操作这个 UART 或 DMA 流。
3. 将以下 hooks 合入现有 HAL 回调。不要再定义一组同名全局 HAL 回调覆盖原工程逻辑。

```c
/* Add the matching call to each existing callback body. */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    Tutorial_UartTxCompleteHook(huart);
    /* Existing application dispatch, if any. */
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    Tutorial_UartRxCompleteHook(huart);
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    Tutorial_UartErrorHook(huart);
}
```

若 `USE_HAL_UART_REGISTER_CALLBACKS == 1`，应在实际注册的回调函数内调用这些 hooks。普通 TX DMA 完成还需要 UART TC 中断才能到达 HAL 的 TX 完成回调，不能只使能 DMA IRQ。

任务上下文的调用示例：

```c
static const uint8_t message[] = "cache dma\r\n";
uint8_t frame[64];
size_t received;

HAL_StatusTypeDef tx = Tutorial_UartSend(message, sizeof(message) - 1U);
HAL_StatusTypeDef rx = Tutorial_UartReceive(sizeof(frame));
/* HAL_OK means started, not completed. HAL_BUSY can mean a previous operation
 * or unread RX data is pending. Check the returned status in real code. */
(void)tx;
(void)rx;

/* Later, in a task/main loop, not immediately assuming completion: */
if (Tutorial_UartTakeRx(frame, sizeof(frame), &received)) {
    /* frame[0..received-1] now belongs to the caller. */
}
if (Tutorial_UartHasFault()) {
    HAL_StatusTypeDef reset = Tutorial_UartAbortAndReset();
    /* Retry only if reset == HAL_OK; otherwise keep the buffers unavailable. */
    (void)reset;
}
```

TX API 先把输入复制到私有静态缓冲，因此函数返回后调用者可复用 `message`；私有缓冲仍由 DMA 占用，直到 HAL 的 TX 完成 hook。RX 在启动前 Invalidate，在真正完成后再次 Invalidate，之后才把 `rx_ready` 置位。`Tutorial_UartTakeRx()` 复制完成才释放接收区，因此不能提前启动下一次接收。接收长度固定为启动时指定的长度，不能把半完成、IDLE 或循环 DMA 事件直接视作同一完成事件。

所有任务 API 必须由**同一应用层互斥锁**串行化，或者只允许一个任务调用。内部短暂关中断只保护状态交接，不替代 RTOS 互斥锁。`volatile` 也不替代 Cache 维护、硬件屏障或锁。hooks 只从对应 HAL 回调调用，不得调用任务 API 或阻塞等待。

任意启动失败/错误回调都会保留故障状态及缓冲所有权；错误回调并不证明两个 DMA 流均已停止。任务必须在中断及 HAL tick 正常运行时调用 `Tutorial_UartAbortAndReset()`。它同步中止 UART、检查 DMA 流 EN 位、补充中止仍活跃的流，并确认两个 DMA handle 均为 `HAL_DMA_STATE_READY`，然后才丢弃 RX 缓冲并复位状态。失败时不能重用缓冲或继续启动 DMA。特别是 DMA Abort 超时可能让 handle 停留在 ERROR 状态，即使硬件稍后已把 EN 清零，恢复 API 仍返回失败并保留 fault；此时需要板级代码在确认硬件停止后执行正确的 DMA DeInit/Init 或系统受控恢复，不能直接把 handle 的 State 改成 READY。示例不会假报自动恢复成功。示例选择丢弃出错时的部分接收数据；需要保留部分帧时，应按协议单独设计。

## 独立 TCM 段示例

本工程已经有 `.itcm_ekf`、`.bss.dtcm_ekf` 及对应 GNU startup 复制/清零逻辑；继续使用这些段时**不需要**再接入 `Tutorial_TcmInit()`。新增示例段只用于说明一般机制，与原段名称、边界符号均不同，不能把同一对象放入两套段。

下面这些声明需要本例链接片段与启动函数配套：

```c
static uint32_t gain TUTORIAL_DTCM_DATA = 3U; /* Flash initializer -> DTCM */
static uint32_t history[16] TUTORIAL_DTCM_BSS; /* startup clears to zero */

TUTORIAL_ITCM uint32_t FastAccumulate(uint32_t sample)
{
    history[0] += sample * gain;
    return history[0];
}
```

`TUTORIAL_DTCM_BSS` 只用于未初始化或零初始化对象，不能放有非零初值的数据。`TUTORIAL_ITCM` 只把函数本体和随函数生成的段内内容放入 ITCM；其调用的外部函数、独立常量表和编译器库函数不会自动搬迁。TCM 容量应与本型号、选项字节分配和现有工程一致；示例沿用当前链接脚本的 ITCM 64 KiB、DTCM 128 KiB。TCM 也不能解决算法或调度上的实时性问题。

把 `tutorial_sections.ld` 的内容/INCLUDE 加入主链接脚本的 `SECTIONS` 内，保留现有 `.itcm_ekf/.dtcm_ekf` 段。输出段使用 `ITCMRAM`、`DTCMRAM`、`RAM_D2`、`FLASH` 四个已有 MEMORY 名称。注意加入片段会保留整个 D2 SRAM；只学习 TCM 时可仅集成三个 TCM 输出段及其符号。

在 `Reset_Handler` 的 `SystemInit` 之后、`__libc_init_array` 之前调用：

```asm
  bl SystemInit
  /* Existing project startup initialization remains here. */
  bl Tutorial_TcmInit
  /* Ordinary .data copy/.bss clear must also happen before constructors. */
  bl __libc_init_array
```

这段是顺序示意，不能覆盖本工程完整 startup。调用时必须有有效的普通 SRAM 栈，相关存储器可访问，Cache 尚未启用，且该函数自身仍在 Flash。它重新设置 VTOR 指向 Flash 向量表，开启 TCM 接口，然后以 64 位存储复制 ITCM、以 32 位存储复制/清零 DTCM，最后执行 DSB/ISB。原工程 startup 也会处理现有 EKF 段，两套符号必须各初始化一次。

该启动文件使用 GNU 属性 `target("general-regs-only")`、volatile 和关闭循环库函数替换的优化属性，避免出现 FPU 指令或隐式 memcpy/memset。编译此文件时禁用 LTO，并用反汇编确认复制循环采用 LDRD/STRD、无库调用。它没有 `main`、构造器或标准库依赖；UART 文件中的 memcpy 则只在正常 C 运行时使用。

## 验证范围

已用本机 GNU Arm Embedded GCC 10.2.1，针对 Cortex-M7、hard-float、`-O2 -std=c11 -Wall -Wextra -Werror -ffreestanding -fno-builtin -fno-lto` 和仓库现有 CMSIS/HAL 头文件独立编译四个 `.c` 文件。另用合成固件执行链接检查：原有 EKF 与新 TCM 段互不覆盖，TCM 加载地址在 Flash，D2 DMA 区为 NOLOAD 且占满 `0x30000000–0x30007FFF`；同时分配两种 DMA 策略的缓冲、分配超过 32 KiB 的缓冲均被链接器拒绝。检查了启动函数反汇编，ITCM 使用 LDRD/STRD，没有 FPU 或标准库调用。

这些验证不包含烧录、UART 回环、电源复位、Cache 压力测试、MPU 异常测试或实际 FreeRTOS 并发测试。集成到板级固件后仍需按主教程做 DMA 连续收发、缓存污染、长度边界及错误恢复验证。
