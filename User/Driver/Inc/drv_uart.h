/**
 * @file drv_uart.h
 * @author HGCSJ
 * @brief UART通信初始化与配置流程
 * @version V1.1
 * @date 2026-10-30 27赛季定稿
 *
 * @copyright ZLLC 2027
 * 
 */

#ifndef DRV_UART_H
#define DRV_UART_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/

#ifdef STM32H723xx 
#include "stm32h7xx_hal.h"
#endif  
#ifdef STM32F407xx
#include "stm32f4xx_hal.h"
#endif   
  
#include "usart.h"

  
/* Exported macros -----------------------------------------------------------*/

// 缓冲区字节长度
#define UART_BUFFER_SIZE 128

/* Exported types ------------------------------------------------------------*/

/**
 * @brief UART通信接收回调函数数据类型
 *
 */
typedef void (*UART_Call_Back)(uint8_t *Buffer, uint16_t Length);

/**
 * @brief UART通信处理结构体
 */
struct Struct_UART_Manage_Object
{
    UART_HandleTypeDef *UART_Handler;
    uint8_t Tx_Buffer[UART_BUFFER_SIZE];
    uint8_t Rx_Buffer[UART_BUFFER_SIZE];
    uint16_t Rx_Buffer_Length;
    uint16_t Tx_Buffer_Length;
    uint16_t Rx_Length;
    uint16_t Tx_Length;
    UART_Call_Back Callback_Function;
};

/* DR16 正常接收与帧边界诊断；错误恢复见 UART5_Recovery_Diagnostics。 */
struct Struct_UART5_Rx_Diagnostics
{
    volatile uint32_t Idle_Events;
    volatile uint32_t Overflow_Events;
    volatile uint16_t Last_Size;
};

/* USART1 / UART5 / USART10 共用的恢复状态；业务功能不受性能测试宏裁剪。 */
struct Struct_UART_Recovery_Diagnostics
{
    volatile uint32_t Error_Events;
    volatile uint32_t Recovery_Count;
    volatile uint32_t Restart_Failures;
    volatile uint32_t Last_Error;
    volatile HAL_StatusTypeDef Last_Start_Status;
    volatile uint8_t Rx_Restart_Pending;
    volatile uint8_t Tx_Restart_Pending;
};

/* Exported variables --------------------------------------------------------*/



//extern UART_HandleTypeDef huart1;
////extern UART_HandleTypeDef huart2;
//extern UART_HandleTypeDef huart3;
//extern UART_HandleTypeDef huart6;
////extern UART_HandleTypeDef huart7;
extern DMA_HandleTypeDef hdma_uart5_rx;
extern DMA_HandleTypeDef hdma_usart1_rx;
extern DMA_HandleTypeDef hdma_usart1_tx;
extern DMA_HandleTypeDef hdma_usart10_rx;

extern struct Struct_UART_Manage_Object UART1_Manage_Object;
extern struct Struct_UART_Manage_Object UART2_Manage_Object;
extern struct Struct_UART_Manage_Object UART3_Manage_Object;
extern struct Struct_UART_Manage_Object UART4_Manage_Object;
extern struct Struct_UART_Manage_Object UART5_Manage_Object;
extern struct Struct_UART_Manage_Object UART6_Manage_Object;
extern struct Struct_UART_Manage_Object UART7_Manage_Object;
extern struct Struct_UART_Manage_Object UART8_Manage_Object;
extern struct Struct_UART_Manage_Object UART9_Manage_Object;
extern struct Struct_UART_Manage_Object UART10_Manage_Object;

extern float temp_power;
extern struct Struct_UART5_Rx_Diagnostics UART5_Rx_Diagnostics;
extern struct Struct_UART_Recovery_Diagnostics UART1_Recovery_Diagnostics;
extern struct Struct_UART_Recovery_Diagnostics UART5_Recovery_Diagnostics;
extern struct Struct_UART_Recovery_Diagnostics UART10_Recovery_Diagnostics;

/* Exported function declarations --------------------------------------------*/

void UART_Init(UART_HandleTypeDef *huart, UART_Call_Back Callback_Function, uint16_t Rx_Buffer_Length);

/* 每 1ms 由控制任务调用，三路串口共用恢复流程；错误中断只请求恢复。 */
void UART_Error_Service(void);
uint8_t UART_RecoveryPending(UART_HandleTypeDef *huart);

/* 任务上下文的 DMA 发送入口，与错误恢复互斥；恢复期间返回 HAL_BUSY。 */
uint8_t UART_Send_Data(UART_HandleTypeDef *huart, uint8_t *Data, uint16_t Length);

void TIM_UART_PeriodElapsedCallback();

#ifdef __cplusplus
}
#endif

#endif

/************************ COPYRIGHT(C) USTC-ROBOWALKER **************************/
