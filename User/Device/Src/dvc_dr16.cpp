/**
 * @file DR16.cpp
 * @author cjw by yssickjgd
 * @brief DR16遥控器
 * @version 0.1
 * @date 2025-07-1 0.1 26赛季定稿
 *
 * @copyright ZLLC 2026
 *
 */

/* Includes ------------------------------------------------------------------*/

#include "dvc_dr16.h"
#include "buzzer.h"
//#include "tsk_config_and_callback.h"
/* Private macros ------------------------------------------------------------*/

/* Private types -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/

/* Private function declarations ---------------------------------------------*/

/* Function prototypes -------------------------------------------------------*/

/**
 * @brief 遥控器DR16初始化
 *
 * @param huart 指定的UART
 */
void Class_DR16::Init(UART_HandleTypeDef *huart_1,UART_HandleTypeDef *huart_2)
{
    //dr16串口
    if (huart_1->Instance == USART1)
    {
        UART_Manage_Object_1 = &UART1_Manage_Object;
    }
    else if (huart_1->Instance == USART2)
    {
        UART_Manage_Object_1 = &UART2_Manage_Object;
    }
    else if (huart_1->Instance == USART3)
    {
        UART_Manage_Object_1 = &UART3_Manage_Object;
    }
    else if (huart_1->Instance == UART4)
    {
        UART_Manage_Object_1 = &UART4_Manage_Object;
    }
    else if (huart_1->Instance == UART5)
    {
        UART_Manage_Object_1 = &UART5_Manage_Object;
    }
    else if (huart_1->Instance == USART6)
    {
        UART_Manage_Object_1 = &UART6_Manage_Object;
    }

    //图传串口
    if (huart_2->Instance == USART1)
    {
        UART_Manage_Object_2 = &UART1_Manage_Object;
    }
    else if (huart_2->Instance == USART2)
    {
        UART_Manage_Object_2 = &UART2_Manage_Object;
    }
    else if (huart_2->Instance == USART3)
    {
        UART_Manage_Object_2 = &UART3_Manage_Object;
    }
    else if (huart_2->Instance == UART4)
    {
        UART_Manage_Object_2 = &UART4_Manage_Object;
    }
    else if (huart_2->Instance == UART5)
    {
        UART_Manage_Object_2 = &UART5_Manage_Object;
    }
    else if (huart_2->Instance == USART6)
    {
        UART_Manage_Object_2 = &UART6_Manage_Object;
    }
}

/**
 * @brief 判断拨动开关状态
 *
 */
void Class_DR16::Judge_Switch(Enum_DR16_Switch_Status *Switch, uint8_t Status, uint8_t Pre_Status)
{
    //带触发的判断
    switch (Pre_Status)
    {
    case (SWITCH_UP):
    {
        switch (Status)
        {
        case (SWITCH_UP):
        {
            *Switch = DR16_Switch_Status_UP;
        }
        break;
        case (SWITCH_DOWN):
        {
            *Switch = DR16_Switch_Status_TRIG_MIDDLE_DOWN;
        }
        break;
        case (SWITCH_MIDDLE):
        {
            *Switch = DR16_Switch_Status_TRIG_UP_MIDDLE;
        }
        break;
        }
    }
    break;
    case (SWITCH_DOWN):
    {
        switch (Status)
        {
        case (SWITCH_UP):
        {
            *Switch = DR16_Switch_Status_TRIG_MIDDLE_UP;
        }
        break;
        case (SWITCH_DOWN):
        {
            *Switch = DR16_Switch_Status_DOWN;
        }
        break;
        case (SWITCH_MIDDLE):
        {
            *Switch = DR16_Switch_Status_TRIG_DOWN_MIDDLE;
        }
        break;
        }
    }
    break;
    case (SWITCH_MIDDLE):
    {
        switch (Status)
        {
        case (SWITCH_UP):
        {
            *Switch = DR16_Switch_Status_TRIG_MIDDLE_UP;
        }
        break;
        case (SWITCH_DOWN):
        {
            *Switch = DR16_Switch_Status_TRIG_MIDDLE_DOWN;
        }
        break;
        case (SWITCH_MIDDLE):
        {
            *Switch = DR16_Switch_Status_MIDDLE;
        }
        break;
        }
    }
    break;
    }
}

/**
 * @brief 判断按键状态
 *
 */
void Class_DR16::Judge_Key(Enum_DR16_Key_Status *Key, uint8_t Status, uint8_t Pre_Status)
{
    //带触发的判断
    switch (Pre_Status)
    {
    case (KEY_FREE):
    {
        switch (Status)
        {
        case (KEY_FREE):
        {
            *Key = DR16_Key_Status_FREE;
        }
        break;
        case (KEY_PRESSED):
        {
            *Key = DR16_Key_Status_TRIG_FREE_PRESSED;
        }
        break;
        }
    }
    break;
    case (KEY_PRESSED):
    {
        switch (Status)
        {
        case (KEY_FREE):
        {
            *Key = DR16_Key_Status_TRIG_PRESSED_FREE;
        }
        break;
        case (KEY_PRESSED):
        {
            *Key = DR16_Key_Status_PRESSED;
        }
        break;
        }
    }
    break;
    }
}

/**
 * @brief 判断遥控器更新状态
 *
 */
void Class_DR16::Judge_Updata(Struct_DR16_UART_Data Pre_UART_Rx_Data,Struct_DR16_UART_Data Now_UART_Rx_Data)
{
    if((Pre_UART_Rx_Data.Channel_0 == Now_UART_Rx_Data.Channel_0)&&
       (Pre_UART_Rx_Data.Channel_1 == Now_UART_Rx_Data.Channel_1)&&
       (Pre_UART_Rx_Data.Channel_2 == Now_UART_Rx_Data.Channel_2)&&
       (Pre_UART_Rx_Data.Channel_3 == Now_UART_Rx_Data.Channel_3))
    {
        DR16_Updata_Status = DR16_Status_DisUpdata;
    }
    else
    {
        DR16_Updata_Status = DR16_Status_Updata;
    }
}


/**
 * @brief 数据处理过程
 *
 */
void Class_DR16::DR16_Data_Process()
{
    //当前原始值已经按协议解码并通过检查
    Struct_DR16_UART_Data *tmp_buffer = &Now_UART_Rx_Data;

    /*源数据转为对外数据*/

    //摇杆信息
    Data.Right_X = (tmp_buffer->Channel_0 - Rocker_Offset) / Rocker_Num;
    Data.Right_Y = (tmp_buffer->Channel_1 - Rocker_Offset) / Rocker_Num;
    Data.Left_X = (tmp_buffer->Channel_2 - Rocker_Offset) / Rocker_Num;
    Data.Left_Y = (tmp_buffer->Channel_3 - Rocker_Offset) / Rocker_Num;

    //判断拨码触发
    Judge_Switch(&Data.Left_Switch, tmp_buffer->Switch_1, Pre_UART_Rx_Data.Switch_1);
    Judge_Switch(&Data.Right_Switch, tmp_buffer->Switch_2, Pre_UART_Rx_Data.Switch_2);

    // //鼠标信息
    // Data.Mouse_X = tmp_buffer->Mouse_X / 32768.0f;
    // Data.Mouse_Y = tmp_buffer->Mouse_Y / 32768.0f;
    // Data.Mouse_Z = tmp_buffer->Mouse_Z / 32768.0f;

    // //判断鼠标触发
    // Judge_Key(&Data.Mouse_Left_Key, tmp_buffer->Mouse_Left_Key, Pre_UART_Rx_Data.Mouse_Left_Key);
    // Judge_Key(&Data.Mouse_Right_Key, tmp_buffer->Mouse_Right_Key, Pre_UART_Rx_Data.Mouse_Right_Key);

    // //判断键盘触发
    // for (int i = 0; i < 16; i++)
    // {
    //     Judge_Key(&Data.Keyboard_Key[i], ((tmp_buffer->Keyboard_Key) >> i) & 0x1, ((Pre_UART_Rx_Data.Keyboard_Key) >> i) & 0x1);
    // }

    //左前轮信息
    Data.Yaw = (tmp_buffer->Channel_Yaw - Rocker_Offset) / Rocker_Num;

    Judge_Updata(Pre_UART_Rx_Data,Now_UART_Rx_Data);
}

/**
 * @brief 数据处理过程
 *
 */
void Class_DR16::Image_Data_Process(uint8_t* __rx_buffer)
{
    //获取当前原始值数据
    memcpy(&Now_UART_Image_Rx_Data, __rx_buffer,sizeof(Struct_Image_UART_Data));
    //数据处理过程
    Struct_Image_UART_Data *tmp_buffer = (Struct_Image_UART_Data *)__rx_buffer;

    /*源数据转为对外数据*/

    //鼠标信息
    Data.Mouse_X = tmp_buffer->Mouse_X / 32768.0f;
    Data.Mouse_Y = tmp_buffer->Mouse_Y / 32768.0f;
    Data.Mouse_Z = tmp_buffer->Mouse_Z / 32768.0f;


    //判断鼠标触发
    Judge_Key(&Data.Mouse_Left_Key, tmp_buffer->Mouse_Left_Key, Pre_UART_Image_Rx_Data.Mouse_Left_Key);
    Judge_Key(&Data.Mouse_Right_Key, tmp_buffer->Mouse_Right_Key, Pre_UART_Image_Rx_Data.Mouse_Right_Key);

    //判断键盘触发
    for (int i = 0; i < 16; i++)
    {
        Judge_Key(&Data.Keyboard_Key[i], ((tmp_buffer->Keyboard_Key) >> i) & 0x1, ((Pre_UART_Image_Rx_Data.Keyboard_Key) >> i) & 0x1);
    }
}

/**
 * @brief UART通信接收回调函数
 *
 * @param Rx_Data 接收的数据
 * @param Length 本次接收的数据长度，DR16完整帧为18字节
 * @return 是否收到完整且合法的DR16帧
 */
bool Class_DR16::DR16_UART_RxCpltCallback(uint8_t *Rx_Data, uint16_t Length)
{
    if (Rx_Data == NULL || Length != 18)
    {
        Error_Cnt++;
        return false;
    }

    //按协议逐字节解码，不依赖编译器的位域内存布局
    Struct_DR16_UART_Data Decoded_Data = {};
    Decoded_Data.Channel_0 = (Rx_Data[0] | (Rx_Data[1] << 8)) & 0x07ff;
    Decoded_Data.Channel_1 = ((Rx_Data[1] >> 3) | (Rx_Data[2] << 5)) & 0x07ff;
    Decoded_Data.Channel_2 = ((Rx_Data[2] >> 6) | (Rx_Data[3] << 2) | (Rx_Data[4] << 10)) & 0x07ff;
    Decoded_Data.Channel_3 = ((Rx_Data[4] >> 1) | (Rx_Data[5] << 7)) & 0x07ff;
    Decoded_Data.Switch_2 = (Rx_Data[5] >> 4) & 0x03;
    Decoded_Data.Switch_1 = (Rx_Data[5] >> 6) & 0x03;
    Decoded_Data.Mouse_X = (int16_t)(Rx_Data[6] | (Rx_Data[7] << 8));
    Decoded_Data.Mouse_Y = (int16_t)(Rx_Data[8] | (Rx_Data[9] << 8));
    Decoded_Data.Mouse_Z = (int16_t)(Rx_Data[10] | (Rx_Data[11] << 8));
    Decoded_Data.Mouse_Left_Key = Rx_Data[12];
    Decoded_Data.Mouse_Right_Key = Rx_Data[13];
    Decoded_Data.Keyboard_Key = Rx_Data[14] | (Rx_Data[15] << 8);
    //保持原Channel_Yaw位域的行为：滚轮只取低11位，高位不参与通道判断。
    const uint16_t Channel_Yaw = (Rx_Data[16] | (Rx_Data[17] << 8)) & 0x07ff;

    const uint16_t Min_Channel = 364;
    const uint16_t Max_Channel = 1684;
    if (Decoded_Data.Channel_0 < Min_Channel || Decoded_Data.Channel_0 > Max_Channel ||
        Decoded_Data.Channel_1 < Min_Channel || Decoded_Data.Channel_1 > Max_Channel ||
        Decoded_Data.Channel_2 < Min_Channel || Decoded_Data.Channel_2 > Max_Channel ||
        Decoded_Data.Channel_3 < Min_Channel || Decoded_Data.Channel_3 > Max_Channel ||
        Decoded_Data.Switch_1 < SWITCH_UP || Decoded_Data.Switch_1 > SWITCH_MIDDLE ||
        Decoded_Data.Switch_2 < SWITCH_UP || Decoded_Data.Switch_2 > SWITCH_MIDDLE ||
        Decoded_Data.Mouse_Left_Key > KEY_PRESSED || Decoded_Data.Mouse_Right_Key > KEY_PRESSED)
    {
        Error_Cnt++;
        return false;
    }
    Decoded_Data.Channel_Yaw = Channel_Yaw;

    Now_UART_Rx_Data = Decoded_Data;
    if (!DR16_Previous_Valid)
    {
        Pre_UART_Rx_Data = Now_UART_Rx_Data;
    }
    DR16_Data_Process();
    //保留上一次数据
    Pre_UART_Rx_Data = Now_UART_Rx_Data;
    DR16_Previous_Valid = true;
    //只有合法帧参与在线检测
    DR16_Flag += 1;
    return true;
}

/**
 * @brief UART通信接收回调函数
 *
 * @param Rx_Data 接收的数据
 */

void Class_DR16::Image_UART_RxCpltCallback(uint8_t *Rx_Data)
{
    if(Rx_Data[0]==0xA5)
    {
        uint16_t cmd_id,data_length;
        //数据处理过程
        cmd_id=(Rx_Data[6])&0xff;
        cmd_id=(cmd_id<<8)|Rx_Data[5];  
        data_length=Rx_Data[2]&0xff;
        data_length=(data_length<<8)|Rx_Data[1];
        if(cmd_id == 0x0304 && data_length == 12)
        {
            //滑动窗口, 判断遥控器是否在线
            Image_Flag += 1;
            Image_Data_Process(&Rx_Data[7]);
            //保留上一次数据
            memcpy(&Pre_UART_Image_Rx_Data, &Rx_Data[7], sizeof(Struct_Image_UART_Data));            
        }
    }
}

/**
 * @brief TIM定时器中断定期检测遥控器是否存活
 *
 */
void Class_DR16::TIM1msMod50_Alive_PeriodElapsedCallback()
{
    if (DR16_Flag == Pre_DR16_Flag)
    {
        //图传键鼠仍可在线，单独释放失联DR16的摇杆和拨杆触发
        Data.Right_X = 0.0f;
        Data.Right_Y = 0.0f;
        Data.Left_X = 0.0f;
        Data.Left_Y = 0.0f;
        Data.Yaw = 0.0f;
        Data.Left_Switch = DR16_Switch_Status_MIDDLE;
        Data.Right_Switch = DR16_Switch_Status_MIDDLE;
        DR16_Updata_Status = DR16_Status_DisUpdata;
        DR16_Previous_Valid = false;
    }

    //判断该时间段内是否接收过遥控器数据
    if (DR16_Flag == Pre_DR16_Flag && Image_Flag == Pre_Image_Flag)
    {
        //遥控器断开连接
        DR16_Status = DR16_Status_DISABLE;
        Unline_Cnt++;
    }
    else
    {
        //遥控器保持连接
        DR16_Status = DR16_Status_ENABLE;
        Unline_Cnt = 0;
    }
    Pre_DR16_Flag = DR16_Flag;
    Pre_Image_Flag = Image_Flag;
}

/************************ COPYRIGHT(C) USTC-ROBOWALKER **************************/
