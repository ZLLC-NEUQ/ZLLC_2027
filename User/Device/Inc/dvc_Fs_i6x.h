/**
 * @file dvc_Fs_i6x.h
 * @brief FS-i6X / FS-iA6B 遥控器（S.BUS 接收）。
 */

#ifndef DVC_FS_I6X_H
#define DVC_FS_I6X_H

#include <stdint.h>

constexpr uint16_t FS_I6X_FRAME_LENGTH = 25U;
constexpr uint8_t FS_I6X_CHANNEL_COUNT = 16U;

// 与遥控器 Aux. Channels 一致。数组下标从 0 开始，CH5 对应 4。
#ifndef FS_I6X_SWA_CHANNEL
#define FS_I6X_SWA_CHANNEL 4U
#endif
#ifndef FS_I6X_SWB_CHANNEL
#define FS_I6X_SWB_CHANNEL 5U
#endif
#ifndef FS_I6X_SWC_CHANNEL
#define FS_I6X_SWC_CHANNEL 6U
#endif
#ifndef FS_I6X_SWD_CHANNEL
#define FS_I6X_SWD_CHANNEL 7U
#endif
#ifndef FS_I6X_VRA_CHANNEL
#define FS_I6X_VRA_CHANNEL 8U
#endif
#ifndef FS_I6X_VRB_CHANNEL
#define FS_I6X_VRB_CHANNEL 9U
#endif

enum Enum_FS_I6X_Stick_Mode : uint8_t
{
    FS_I6X_Stick_Mode_INVALID = 0,
    FS_I6X_Stick_Mode_1,
    FS_I6X_Stick_Mode_2,
    FS_I6X_Stick_Mode_3,
    FS_I6X_Stick_Mode_4,
};

enum Enum_FS_Connection_Status : uint8_t
{
    FS_Connection_Status_OFFLINE = 0, // 本轮存活检测没有收到可用控制帧
    FS_Connection_Status_ONLINE,      // 本轮存活检测收到可用控制帧
};

enum Enum_FS_Switch_Status : uint8_t
{
    FS_Switch_Status_UP = 0,
    FS_Switch_Status_MIDDLE,
    FS_Switch_Status_DOWN,
};

struct Struct_FS_UART_Data
{
    uint16_t Channel[FS_I6X_CHANNEL_COUNT];
    uint8_t Flags;
    uint8_t End;
};

class Class_Fs_i6x
{
public:
    void Init(Enum_FS_I6X_Stick_Mode stick_mode);

    // 传入 UART 回调交付的完整帧。仅接受可用于控制的帧时返回 true。
    bool FS_UART_RxCpltCallback(const uint8_t *rx_data, uint16_t length);
    void TIM1msMod50_Alive_PeriodElapsedCallback();

    Enum_FS_Connection_Status Get_FS_Connection_Status() const { return FS_Connection_Status; }
    // 最近一帧格式正确的 S.BUS 数据中的失控位。UART 超时后仍保留，供排查。
    bool Get_FS_Failsafe() const { return FS_Failsafe; }
    // 保留最后一帧格式正确的数据，包括接收机上报的失控帧，便于调试。
    const Struct_FS_UART_Data &Get_UART_Data() const { return Now_UART_Rx_Data; }
    uint16_t Get_Channel_Raw(uint8_t channel) const;

    // 摇杆、旋钮和拨杆按最后一帧格式正确的数据解码。用于控制前先检查在线状态。
    float Get_Right_X() const { return Right_X; }
    float Get_Right_Y() const { return Right_Y; }
    float Get_Left_X() const { return Left_X; }
    float Get_Left_Y() const { return Left_Y; }
    float Get_VRA() const { return VRA; }
    float Get_VRB() const { return VRB; }
    Enum_FS_Switch_Status Get_SWA() const { return SWA; }
    Enum_FS_Switch_Status Get_SWB() const { return SWB; }
    Enum_FS_Switch_Status Get_SWC() const { return SWC; }
    Enum_FS_Switch_Status Get_SWD() const { return SWD; }

private:
    Enum_FS_I6X_Stick_Mode Stick_Mode = FS_I6X_Stick_Mode_INVALID;
    Struct_FS_UART_Data Now_UART_Rx_Data = {};
    Enum_FS_Connection_Status FS_Connection_Status = FS_Connection_Status_OFFLINE;
    bool FS_Failsafe = false;
    uint32_t FS_Flag = 0; // 可用于控制的帧计数
    uint32_t Pre_FS_Flag = 0;

    float Right_X = 0.0f;
    float Right_Y = 0.0f;
    float Left_X = 0.0f;
    float Left_Y = 0.0f;
    float VRA = 0.0f;
    float VRB = 0.0f;
    Enum_FS_Switch_Status SWA = FS_Switch_Status_MIDDLE;
    Enum_FS_Switch_Status SWB = FS_Switch_Status_MIDDLE;
    Enum_FS_Switch_Status SWC = FS_Switch_Status_MIDDLE;
    Enum_FS_Switch_Status SWD = FS_Switch_Status_MIDDLE;
};

#endif
