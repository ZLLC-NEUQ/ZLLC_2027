/**
 * @file dvc_Fs_i6x.cpp
 * @brief FS-i6X / FS-iA6B 遥控器（S.BUS 接收）。
 * @note 解包参考 M72_Gripper 的 S.BUS 驱动。
 */

#include "dvc_Fs_i6x.h"

namespace
{
constexpr uint8_t Frame_Header = 0x0fU;
constexpr uint8_t Flag_Frame_Lost = 0x04U;
constexpr uint8_t Flag_Failsafe = 0x08U;
constexpr float Rocker_Center = 1023.0f;
constexpr float Rocker_Range = 783.0f;
constexpr uint16_t Switch_Up_Max = 600U;
constexpr uint16_t Switch_Down_Min = 1400U;

struct Struct_Stick_Channels
{
    uint8_t Right_X;
    uint8_t Right_Y;
    uint8_t Left_X;
    uint8_t Left_Y;
};

// 下标为 Sticks Mode 减一。CH1 副翼，CH2 升降，CH3 油门，CH4 方向。
constexpr Struct_Stick_Channels Stick_Channels[] = {
    {0U, 2U, 3U, 1U}, // Mode 1
    {0U, 1U, 3U, 2U}, // Mode 2
    {3U, 2U, 0U, 1U}, // Mode 3
    {3U, 1U, 0U, 2U}, // Mode 4
};

static_assert(static_cast<unsigned int>(FS_I6X_SWA_CHANNEL) < FS_I6X_CHANNEL_COUNT &&
                  static_cast<unsigned int>(FS_I6X_SWB_CHANNEL) < FS_I6X_CHANNEL_COUNT &&
                  static_cast<unsigned int>(FS_I6X_SWC_CHANNEL) < FS_I6X_CHANNEL_COUNT &&
                  static_cast<unsigned int>(FS_I6X_SWD_CHANNEL) < FS_I6X_CHANNEL_COUNT,
              "S.BUS auxiliary channel index must be 0 to 15");

uint16_t Decode_Channel(const uint8_t *frame, uint8_t channel)
{
    const uint16_t bit_index = static_cast<uint16_t>(channel) * 11U;
    const uint8_t byte_index = 1U + bit_index / 8U;
    const uint8_t bit_offset = bit_index % 8U;
    const uint32_t packed = static_cast<uint32_t>(frame[byte_index]) |
                            (static_cast<uint32_t>(frame[byte_index + 1U]) << 8U) |
                            (static_cast<uint32_t>(frame[byte_index + 2U]) << 16U);
    return static_cast<uint16_t>((packed >> bit_offset) & 0x07ffU);
}

float Normalize_Rocker(uint16_t raw)
{
    const float value = (static_cast<float>(raw) - Rocker_Center) / Rocker_Range;
    if (value > 1.0f) return 1.0f;
    if (value < -1.0f) return -1.0f;
    return value;
}

Enum_FS_Switch_Status Decode_Switch(uint16_t raw)
{
    if (raw < Switch_Up_Max) return FS_Switch_Status_UP;
    if (raw > Switch_Down_Min) return FS_Switch_Status_DOWN;
    return FS_Switch_Status_MIDDLE;
}

bool Is_Valid_End_Byte(uint8_t end)
{
    return end == 0x00U || end == 0x04U || end == 0x14U ||
           end == 0x24U || end == 0x34U || end == 0xfeU;
}
} // namespace

void Class_Fs_i6x::Init(Enum_FS_I6X_Stick_Mode stick_mode)
{
    *this = Class_Fs_i6x{};
    if (stick_mode >= FS_I6X_Stick_Mode_1 && stick_mode <= FS_I6X_Stick_Mode_4)
    {
        Stick_Mode = stick_mode;
    }
}

uint16_t Class_Fs_i6x::Get_Channel_Raw(uint8_t channel) const
{
    return channel < FS_I6X_CHANNEL_COUNT ? Now_UART_Rx_Data.Channel[channel] : 0U;
}

bool Class_Fs_i6x::FS_UART_RxCpltCallback(const uint8_t *rx_data, uint16_t length)
{
    if (Stick_Mode == FS_I6X_Stick_Mode_INVALID || rx_data == nullptr ||
        length != FS_I6X_FRAME_LENGTH || rx_data[0] != Frame_Header || !Is_Valid_End_Byte(rx_data[24]))
    {
        return false;
    }

    for (uint8_t channel = 0; channel < FS_I6X_CHANNEL_COUNT; ++channel)
    {
        Now_UART_Rx_Data.Channel[channel] = Decode_Channel(rx_data, channel);
    }
    Now_UART_Rx_Data.Flags = rx_data[23];
    Now_UART_Rx_Data.End = rx_data[24];

    if ((Now_UART_Rx_Data.Flags & (Flag_Frame_Lost | Flag_Failsafe)) != 0U)
    {
        FS_Status = FS_Status_DISABLE;
        return false;
    }

    const Struct_Stick_Channels &channels = Stick_Channels[Stick_Mode - FS_I6X_Stick_Mode_1];
    Right_X = Normalize_Rocker(Now_UART_Rx_Data.Channel[channels.Right_X]);
    Right_Y = Normalize_Rocker(Now_UART_Rx_Data.Channel[channels.Right_Y]);
    Left_X = Normalize_Rocker(Now_UART_Rx_Data.Channel[channels.Left_X]);
    Left_Y = Normalize_Rocker(Now_UART_Rx_Data.Channel[channels.Left_Y]);
    SWA = Decode_Switch(Now_UART_Rx_Data.Channel[FS_I6X_SWA_CHANNEL]);
    SWB = Decode_Switch(Now_UART_Rx_Data.Channel[FS_I6X_SWB_CHANNEL]);
    SWC = Decode_Switch(Now_UART_Rx_Data.Channel[FS_I6X_SWC_CHANNEL]);
    SWD = Decode_Switch(Now_UART_Rx_Data.Channel[FS_I6X_SWD_CHANNEL]);

    ++FS_Flag;
    FS_Status = FS_Status_ENABLE;
    return true;
}

void Class_Fs_i6x::TIM1msMod50_Alive_PeriodElapsedCallback()
{
    if (FS_Flag == Pre_FS_Flag)
    {
        FS_Status = FS_Status_DISABLE;
    }
    Pre_FS_Flag = FS_Flag;
}
