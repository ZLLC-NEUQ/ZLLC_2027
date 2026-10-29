/**
 * @file drv_usb.cpp
 * @author HGCSJ by yssickjgd
 * @brief  USB虚拟串口通信初始化与配置流程
 * @version V1.1
 * @date 2026-10-30 27赛季定稿
 *
 * @copyright ZLLC 2027
 *
 */

/* Includes ------------------------------------------------------------------*/

#include "drv_usb.h"
#include "usbd_cdc_if.h"

/* Private macros ------------------------------------------------------------*/

/* Private types -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/

//��λ��USBͨѶ����
Struct_USB_Manage_Object MiniPC_USB_Manage_Object = {0};
// uint8_t USB_Tx_Buffer[8] = {0};
// uint8_t USB_Rx_Buffer[64] = {0};


/* Private function declarations ---------------------------------------------*/

/* function prototypes -------------------------------------------------------*/

/**
 * @brief ��ʼ��USB
 *
 * @param Callback_Function �����ص�����
 */
void USB_Init(Struct_USB_Manage_Object* MiniPC_USB_Manage_Object, USB_Call_Back __Callback_Function)
{
	MiniPC_USB_Manage_Object->Callback_Function = __Callback_Function;
}

/**
 * @brief USB��TIM��ʱ���жϷ��ͻص�����
 *
 */
void TIM_USB_PeriodElapsedCallback(Struct_USB_Manage_Object* MiniPC_USB_Manage_Object)
{
	CDC_Transmit_HS(MiniPC_USB_Manage_Object->Tx_Buffer, MiniPC_USB_Manage_Object->Tx_Buffer_Length+2);  //֡ͷ+֡β
}

/************************ COPYRIGHT(C) USTC-ROBOWALKER **************************/
