/**
 * @file dvc_referee.cpp
 * @brief RoboMaster 2027 V1.0.0 robot-side serial protocol.
 * @date 2026-10-30 27赛季定稿
 * @author HGCSJ
 * @version V1.1
 * @copyright ZLLC 2027
 */
#include "dvc_referee.h"
#include "dvc_performance.h"
#include "freertos_tasks.h"
#include "dvc_GraphicsSendTask.h"

namespace
{
// Binding also runs before the scheduler starts; preserve the IRQ mask rather
// than entering the FreeRTOS critical nesting state during initialization.
struct Referee_Binding_Guard
{
    uint32_t saved_primask;
    Referee_Binding_Guard() : saved_primask(__get_PRIMASK()) { __disable_irq(); }
    ~Referee_Binding_Guard() { if (saved_primask == 0U) __enable_irq(); }
};

struct Referee_Tx_Guard
{
    Referee_Tx_Guard() { taskENTER_CRITICAL(); }
    ~Referee_Tx_Guard() { taskEXIT_CRITICAL(); }
};

Struct_UART_Manage_Object *Referee_Find_UART(UART_HandleTypeDef *huart)
{
    if (huart == nullptr) return nullptr;
    if (huart->Instance == USART1) return &UART1_Manage_Object;
    if (huart->Instance == USART2) return &UART2_Manage_Object;
    if (huart->Instance == USART3) return &UART3_Manage_Object;
    if (huart->Instance == UART4) return &UART4_Manage_Object;
    if (huart->Instance == UART5) return &UART5_Manage_Object;
    if (huart->Instance == USART6) return &UART6_Manage_Object;
    if (huart->Instance == UART7) return &UART7_Manage_Object;
    if (huart->Instance == UART8) return &UART8_Manage_Object;
    if (huart->Instance == UART9) return &UART9_Manage_Object;
    if (huart->Instance == USART10) return &UART10_Manage_Object;
    return nullptr;
}

uint16_t Referee_Read_U16(const uint8_t *data)
{
    return static_cast<uint16_t>(data[0] | (static_cast<uint16_t>(data[1]) << 8));
}

void Referee_Write_U16(uint8_t *data, uint16_t value)
{
    data[0] = static_cast<uint8_t>(value);
    data[1] = static_cast<uint8_t>(value >> 8);
}

bool Referee_Is_Robot(uint16_t id)
{
    const uint16_t number = id >= 101U && id <= 109U ? id - 100U : id;
    return (id <= 9U || (id >= 101U && id <= 109U)) &&
           number >= 1U && number <= 9U && number != 2U;
}

bool Referee_Same_Team(uint16_t first, uint16_t second)
{
    return Referee_Is_Robot(first) && Referee_Is_Robot(second) &&
           (first >= 101U) == (second >= 101U);
}

bool Referee_Is_Client(uint16_t id, uint16_t robot_id)
{
    if (id < 0x0100U) return false;
    const uint16_t client_robot = id - 0x0100U;
    const uint16_t number = client_robot >= 101U ? client_robot - 100U : client_robot;
    return Referee_Same_Team(robot_id, client_robot) && number <= 6U;
}

// -1: command absent from this link; -2: variable-length robot interaction.
int16_t Referee_Payload_Length(Enum_Referee_Link link, uint16_t command)
{
    if (link == Referee_Link_Video) return command == 0x0311U ? 60 : -1;
    if (link == Referee_Link_Wireless)
    {
        switch (command)
        {
        case 0x0A01: return 24;
        case 0x0A02: return 12;
        case 0x0A03: return 10;
        case 0x0A04: return 12;
        case 0x0A05: return 47;
        case 0x0A06: return 6;
        default: return -1;
        }
    }
    switch (command)
    {
    case 0x0001: return 11;
    case 0x0002: return 1;
    case 0x0003: return 24;
    case 0x0101: return 8;
    case 0x0104: return 3;
    case 0x0105: return 3;
    case 0x0201: return 17;
    case 0x0202: return 14;
    case 0x0203: return 12;
    case 0x0204: return 7;
    case 0x0206: return 7;
    case 0x0207: return 7;
    case 0x0208: return 8;
    case 0x0209: return 6;
    case 0x020A: return 6;
    case 0x020B: return 40;
    case 0x020C: return 2;
    case 0x020D: return 14;
    case 0x020E: return 8;
    case 0x0301: return -2;
    case 0x0303: return 12;
    default: return -1;
    }
}
}

void Class_Referee::Init(UART_HandleTypeDef *huart, uint8_t __Frame_Header)
{
    Referee_Binding_Guard guard;
    Struct_UART_Manage_Object *object = Referee_Find_UART(huart);
    if (object != nullptr && object == Video_UART_Manage_Object)
    {
        if (object->UART_Handler != nullptr &&
            (object->UART_Handler->gState != HAL_UART_STATE_READY ||
             UART_RecoveryPending(object->UART_Handler))) return;
        Video_UART_Manage_Object = nullptr;
    }
    UART_Manage_Object = object;
    if (UART_Manage_Object != nullptr) UART_Manage_Object->UART_Handler = huart;
    Frame_Header = __Frame_Header;
    memset(Rx_Streams, 0, sizeof(Rx_Streams));
    memset(Tx_Last_Tick, 0, sizeof(Tx_Last_Tick));
    memset(Tx_Has_Sent, 0, sizeof(Tx_Has_Sent));
    Diagnostics = {};
    Flag = Pre_Flag = 0;
    seq = Video_seq = 0;
    Referee_Status = Referee_Status_DISABLE;
    Map_Command_Update_Count = Map_Command_Receive_Count = 0;
    Custom_Command_Receive_Count = Interaction_Receive_Count = 0;
    Interaction_Data_Length = 0;
    Game_Status = {};
    Game_Result = {};
    Game_Robot_HP = {};
    Event_Data = {};
    Event_Referee_Warning = {};
    Event_Dart_Remaining_Time = {};
    Robot_Status = {};
    Robot_Power_Heat = {};
    Robot_Position = {};
    Robot_Buff = {};
    Robot_Damage = {};
    Robot_Booster = {};
    Robot_Remaining_Ammo = {};
    Robot_RFID = {};
    Robot_Dart_Command = {};
    Ground_Position = {};
    Radar_Mark = {};
    Sentry_Info = {};
    Radar_Info = {};
    Map_Command = {};
    Custom_Command = {};
    Interaction_Students = {};
    Wireless_Position = {};
    Wireless_HP = {};
    Wireless_Ammo = {};
    Wireless_State = {};
    Wireless_Buff = {};
    Wireless_Key = {};
}

void Class_Referee::Init_Video_UART(UART_HandleTypeDef *huart)
{
    Referee_Binding_Guard guard;
    // The video transmitter is a different physical link (921600, 8N1).
    Struct_UART_Manage_Object *object = Referee_Find_UART(huart);
    if (object != nullptr && object == UART_Manage_Object) return;
    if (Video_UART_Manage_Object != nullptr && Video_UART_Manage_Object->UART_Handler != nullptr &&
        (Video_UART_Manage_Object->UART_Handler->gState != HAL_UART_STATE_READY ||
         UART_RecoveryPending(Video_UART_Manage_Object->UART_Handler))) return;
    Video_UART_Manage_Object = object;
    if (object != nullptr) object->UART_Handler = huart;
    Rx_Streams[Referee_Link_Video].length = 0;
    Video_seq = 0;
    Tx_Has_Sent[4] = false;
}

uint8_t Class_Referee::Verify_CRC_8(uint8_t *message, uint32_t length)
{
    return Get_CRC8_Check_Sum(message, length, CRC8_INIT);
}

uint16_t Class_Referee::Verify_CRC_16(uint8_t *message, uint32_t length)
{
    return Get_CRC16_Check_Sum(message, length, CRC16_INIT);
}

void Class_Referee::Data_Process(uint8_t *data, uint16_t length)
{
    Process_Stream(Referee_Link_Normal, data, length);
}

void Class_Referee::UART_RxCpltCallback(uint8_t *data, uint16_t length)
{
    Data_Process(data, length);
}

void Class_Referee::Video_UART_RxCpltCallback(uint8_t *data, uint16_t length)
{
    Process_Stream(Referee_Link_Video, data, length);
}

void Class_Referee::Wireless_RxCpltCallback(uint8_t *data, uint16_t length)
{
    // Feed bytes AFTER the radar receiver has demodulated the wireless signal.
    Process_Stream(Referee_Link_Wireless, data, length);
}

void Class_Referee::Process_Stream(Enum_Referee_Link link, const uint8_t *data, uint16_t length)
{
    if (data == nullptr || link > Referee_Link_Wireless) return;
    Struct_Referee_Rx_Stream &stream = Rx_Streams[link];
    const uint16_t maximum_payload = link == Referee_Link_Normal ? 118U :
                                     (link == Referee_Link_Video ? 60U : 47U);
    // A DMA/IDLE callback is only a byte chunk, never a packet boundary.
    for (uint16_t input = 0; input < length; ++input)
    {
        if (stream.length == sizeof(stream.data))
        {
            memmove(stream.data, stream.data + 1, --stream.length);
            ++Diagnostics.length_errors;
        }
        stream.data[stream.length++] = data[input];
        while (stream.length != 0U)
        {
            uint16_t consume = 1U;
            if (stream.data[0] == Frame_Header)
            {
                if (stream.length < 5U) break;
                if (!Verify_CRC8_Check_Sum(stream.data, 5U))
                {
                    ++Diagnostics.crc8_errors;
                }
                else
                {
                    const uint16_t payload_length = Referee_Read_U16(stream.data + 1);
                    if (payload_length > maximum_payload)
                    {
                        ++Diagnostics.length_errors;
                    }
                    else
                    {
                        if (stream.length < 7U) break;
                        const uint16_t command = Referee_Read_U16(stream.data + 5);
                        const int16_t expected = Referee_Payload_Length(link, command);
                        if (expected == -1)
                        {
                            ++Diagnostics.unknown_commands;
                        }
                        else if ((expected >= 0 && payload_length != expected) ||
                                 (expected == -2 && (payload_length < 6U || payload_length > 118U)))
                        {
                            ++Diagnostics.length_errors;
                        }
                        else
                        {
                            const uint16_t frame_length = payload_length + 9U;
                            if (stream.length < frame_length) break;
                            if (!Verify_CRC16_Check_Sum(stream.data, frame_length))
                            {
                                // Move one byte so a valid frame inside a corrupt candidate survives.
                                ++Diagnostics.crc16_errors;
                            }
                            else
                            {
                                if (Dispatch_Frame(link, command, stream.data + 7, payload_length))
                                {
                                    ++Diagnostics.valid_frames;
                                    // Custom control and radar reception do not keep the normal link alive.
                                    if (link == Referee_Link_Normal) ++Flag;
                                }
                                consume = frame_length;
                            }
                        }
                    }
                }
            }
            stream.length -= consume;
            if (stream.length != 0U) memmove(stream.data, stream.data + consume, stream.length);
        }
    }
}

bool Class_Referee::Dispatch_Frame(Enum_Referee_Link link, uint16_t command,
                                 const uint8_t *data, uint16_t length)
{
    if (link == Referee_Link_Video)
    {
        memcpy(&Custom_Command, data, sizeof(Custom_Command));
        ++Custom_Command_Receive_Count;
        ++Diagnostics.custom_command_frames;
        return true;
    }
    if (link == Referee_Link_Wireless)
    {
        switch (command)
        {
        case 0x0A01: memcpy(&Wireless_Position, data, sizeof(Wireless_Position)); break;
        case 0x0A02: memcpy(&Wireless_HP, data, sizeof(Wireless_HP)); break;
        case 0x0A03: memcpy(&Wireless_Ammo, data, sizeof(Wireless_Ammo)); break;
        case 0x0A04: memcpy(&Wireless_State, data, sizeof(Wireless_State)); break;
        case 0x0A05: memcpy(&Wireless_Buff, data, sizeof(Wireless_Buff)); break;
        case 0x0A06: memcpy(&Wireless_Key, data, sizeof(Wireless_Key)); break;
        default: return false;
        }
        return true;
    }
    switch (command)
    {
    case 0x0001:
        memcpy(&Game_Status, data, sizeof(Game_Status));
        PERF_EVENT_AT(Referee_Game_Status_Frequency, FreeRTOS_GetRxCycle());
        break;
    case 0x0002: memcpy(&Game_Result, data, sizeof(Game_Result)); break;
    case 0x0003: memcpy(&Game_Robot_HP, data, sizeof(Game_Robot_HP)); break;
    case 0x0101: memcpy(&Event_Data, data, sizeof(Event_Data)); break;
    case 0x0104: memcpy(&Event_Referee_Warning, data, sizeof(Event_Referee_Warning)); break;
    case 0x0105: memcpy(&Event_Dart_Remaining_Time, data, sizeof(Event_Dart_Remaining_Time)); break;
    case 0x0201:
        memcpy(&Robot_Status, data, sizeof(Robot_Status));
        PERF_EVENT_AT(Referee_Robot_Status_Frequency, FreeRTOS_GetRxCycle());
        break;
    case 0x0202:
        memcpy(&Robot_Power_Heat, data, sizeof(Robot_Power_Heat));
        PERF_EVENT_AT(Referee_Power_Heat_Frequency, FreeRTOS_GetRxCycle());
        break;
    case 0x0203: memcpy(&Robot_Position, data, sizeof(Robot_Position)); break;
    case 0x0204: memcpy(&Robot_Buff, data, sizeof(Robot_Buff)); break;
    case 0x0206: memcpy(&Robot_Damage, data, sizeof(Robot_Damage)); break;
    case 0x0207: memcpy(&Robot_Booster, data, sizeof(Robot_Booster)); break;
    case 0x0208: memcpy(&Robot_Remaining_Ammo, data, sizeof(Robot_Remaining_Ammo)); break;
    case 0x0209: memcpy(&Robot_RFID, data, sizeof(Robot_RFID)); break;
    case 0x020A: memcpy(&Robot_Dart_Command, data, sizeof(Robot_Dart_Command)); break;
    case 0x020B: memcpy(&Ground_Position, data, sizeof(Ground_Position)); break;
    case 0x020C: memcpy(&Radar_Mark, data, sizeof(Radar_Mark)); break;
    case 0x020D: memcpy(&Sentry_Info, data, sizeof(Sentry_Info)); break;
    case 0x020E: memcpy(&Radar_Info, data, sizeof(Radar_Info)); break;
    case 0x0301:
    {
        const uint16_t content = Referee_Read_U16(data);
        const uint16_t sender = Referee_Read_U16(data + 2);
        const uint16_t receiver = Referee_Read_U16(data + 4);
        const uint16_t local = Get_ID();
        if (content < 0x0200U || content > 0x02FFU || !Referee_Same_Team(sender, receiver) ||
            (Referee_Is_Robot(local) && receiver != local)) return false;
        memset(&Interaction_Students, 0, sizeof(Interaction_Students));
        memcpy(&Interaction_Students, data, length);
        Interaction_Data_Length = length - 6U;
        ++Interaction_Receive_Count;
        ++Diagnostics.interaction_frames;
        break;
    }
    case 0x0303:
        if (Map_Command_Receive_Count == 0U || memcmp(&Map_Command, data, sizeof(Map_Command)) != 0)
        {
            memcpy(&Map_Command, data, sizeof(Map_Command));
            ++Map_Command_Update_Count;
        }
        ++Map_Command_Receive_Count;
        ++Diagnostics.map_command_frames;
        break;
    default: return false;
    }
    return true;
}

void Class_Referee::TIM1msMod50_Alive_PeriodElapsedCallback()
{
    Referee_Status = Flag == Pre_Flag ? Referee_Status_DISABLE : Referee_Status_ENABLE;
    Pre_Flag = Flag;
}

bool Class_Referee::Check_Send_Rate(uint8_t slot, uint32_t interval_ms)
{
    return slot < 5U && (!Tx_Has_Sent[slot] ||
           static_cast<uint32_t>(HAL_GetTick() - Tx_Last_Tick[slot]) >= interval_ms);
}

bool Class_Referee::Send_Frame(uint16_t command, const uint8_t *data, uint16_t length,
                             Enum_Referee_Link link)
{
    if (length != 0U && data == nullptr) return false;
    // Task-context API: serialize the rate check, persistent DMA buffer write
    // and DMA start. Never overwrite a previous transmission's live buffer.
    Referee_Tx_Guard guard;
    uint8_t slot;
    uint32_t interval;
    switch (command)
    {
    case 0x0301: slot = 0; interval = 34U; break;
    case 0x0305: slot = 1; interval = 200U; break;
    case 0x0307: slot = 2; interval = 1000U; break;
    case 0x0308: slot = 3; interval = 334U; break;
    case 0x0310: slot = 4; interval = 20U; break;
    default: return false;
    }
    if (!Check_Send_Rate(slot, interval)) return false;

    Struct_UART_Manage_Object *object = link == Referee_Link_Video ?
                                       Video_UART_Manage_Object : UART_Manage_Object;
    if (object == nullptr || object->UART_Handler == nullptr) return false;
    const bool video = link == Referee_Link_Video;
    if ((video && (command != 0x0310U || length != 300U)) ||
        (!video && (length > 118U || command == 0x0310U))) return false;
    if (video && (object->UART_Handler->gState != HAL_UART_STATE_READY ||
                  UART_RecoveryPending(object->UART_Handler))) return false;
    // Normal frames fit the existing 128-byte DMA queue. 0310 needs a separate
    // persistent 309-byte DMA buffer, not the driver's 128-byte UART buffer.
    uint8_t normal_frame[127];
    uint8_t *frame = video ? Video_Tx_Buffer : normal_frame;
    frame[0] = Frame_Header;
    Referee_Write_U16(frame + 1, length);
    frame[3] = video ? Video_seq : seq;
    Append_CRC8_Check_Sum(frame, 5U);
    Referee_Write_U16(frame + 5, command);
    if (length != 0U) memcpy(frame + 7, data, length);
    const uint16_t frame_length = length + 9U;
    Append_CRC16_Check_Sum(frame, frame_length);

    bool accepted = false;
    if (video)
    {
        accepted = UART_Send_Data(object->UART_Handler, frame, frame_length) == HAL_OK;
    }
    else if (object->UART_Handler->Instance == USART10)
    {
        // UI and other regular-link packets use the same copied DMA queue.
        accepted = Referee_DMA_TryEnqueuePacket(frame, frame_length) != 0U;
    }
    else if (object->UART_Handler->gState == HAL_UART_STATE_READY &&
             !UART_RecoveryPending(object->UART_Handler))
    {
        memcpy(object->Tx_Buffer, frame, frame_length);
        object->Tx_Length = frame_length;
        accepted = UART_Send_Data(object->UART_Handler, object->Tx_Buffer, frame_length) == HAL_OK;
    }
    if (!accepted) return false;
    if (video) ++Video_seq; else ++seq;
    Tx_Has_Sent[slot] = true;
    Tx_Last_Tick[slot] = HAL_GetTick();
    return true;
}

bool Class_Referee::Send_Interaction(uint16_t receiver, uint16_t content,
                                   const uint8_t *data, uint16_t length)
{
    const uint16_t local = Get_ID();
    if (!Referee_Is_Robot(local) || length > 112U || (length != 0U && data == nullptr)) return false;
    if (content >= 0x0200U && content <= 0x02FFU)
    {
        if (!Referee_Same_Team(local, receiver) || receiver == local) return false;
    }
    else if (content == 0x0120U)
    {
        if ((local != 7U && local != 107U) || receiver != 0x8080U || length != 4U) return false;
    }
    else if (content == 0x0121U)
    {
        if ((local != 9U && local != 109U) || receiver != 0x8080U || length != 13U) return false;
    }
    else
    {
        // UI has its own unchanged draw/pack API; validate the shared envelope
        // here too when a caller already has a ready-to-send UI payload.
        uint16_t expected;
        switch (content)
        {
        case 0x0100: expected = 2; break;
        case 0x0101: expected = 15; break;
        case 0x0102: expected = 30; break;
        case 0x0103: expected = 75; break;
        case 0x0104: expected = 105; break;
        case 0x0110: expected = 45; break;
        default: return false;
        }
        if (length != expected || !Referee_Is_Client(receiver, local) ||
            ((local % 100U) != 9U && receiver != local + 0x0100U)) return false;
    }
    uint8_t payload[118];
    Referee_Write_U16(payload, content);
    Referee_Write_U16(payload + 2, local);
    Referee_Write_U16(payload + 4, receiver);
    if (length != 0U) memcpy(payload + 6, data, length);
    return Send_Frame(0x0301U, payload, length + 6U, Referee_Link_Normal);
}

bool Class_Referee::Send_Sentry_Command(const Struct_Referee_Tx_Data_Sentry_Command &command)
{
    // Bits 25..31 are reserved in V1.0.0. Requests are absolute counters;
    // increment them in the decision layer after checking 020D feedback.
    if ((command.sentry_cmd & 0xFE000000UL) != 0U) return false;
    return Send_Interaction(0x8080U, 0x0120U,
                            reinterpret_cast<const uint8_t *>(&command), sizeof(command));
}

bool Class_Referee::Send_Radar_Command(const Struct_Referee_Tx_Data_Radar_Command &command)
{
    if (command.password_cmd > 2U) return false;
    return Send_Interaction(0x8080U, 0x0121U,
                            reinterpret_cast<const uint8_t *>(&command), sizeof(command));
}

bool Class_Referee::Send_Map_Position(const Struct_Referee_Tx_Data_Map_Position &positions)
{
    if (Get_ID() != 9U && Get_ID() != 109U) return false;
    Struct_Referee_Tx_Data_Map_Position payload = positions;
    // Robot slots 2 and 5 are reserved, on both teams.
    memset(payload.positions[1], 0, sizeof(payload.positions[1]));
    memset(payload.positions[4], 0, sizeof(payload.positions[4]));
    memset(payload.positions[7], 0, sizeof(payload.positions[7]));
    memset(payload.positions[10], 0, sizeof(payload.positions[10]));
    return Send_Frame(0x0305U, reinterpret_cast<const uint8_t *>(&payload), sizeof(payload), Referee_Link_Normal);
}

bool Class_Referee::Send_Map_Path(const Struct_Referee_Tx_Data_Map_Path &path)
{
    if (!Referee_Is_Robot(Get_ID()) || path.intention < 1U || path.intention > 3U) return false;
    // Both sentry and robots using semi-automatic control may report paths.
    Struct_Referee_Tx_Data_Map_Path payload = path;
    payload.sender_id = Get_ID();
    return Send_Frame(0x0307U, reinterpret_cast<const uint8_t *>(&payload), sizeof(payload), Referee_Link_Normal);
}

bool Class_Referee::Send_Map_Robot_Data(uint16_t receiver, const uint8_t *utf16_data, uint16_t byte_length)
{
    if (!Referee_Is_Client(receiver, Get_ID()) || byte_length > 30U || (byte_length & 1U) != 0U ||
        (byte_length != 0U && utf16_data == nullptr)) return false;
    Struct_Referee_Tx_Data_Map_Robot_Data payload = {};
    payload.sender_id = Get_ID();
    payload.receiver_id = receiver;
    if (byte_length != 0U) memcpy(payload.user_data, utf16_data, byte_length);
    return Send_Frame(0x0308U, reinterpret_cast<const uint8_t *>(&payload), sizeof(payload), Referee_Link_Normal);
}

bool Class_Referee::Send_Custom_Data(const uint8_t *data, uint16_t length)
{
    if (!Referee_Is_Robot(Get_ID()) || length > 300U || (length != 0U && data == nullptr)) return false;
    uint8_t payload[300] = {};
    if (length != 0U) memcpy(payload, data, length);
    return Send_Frame(0x0310U, payload, sizeof(payload), Referee_Link_Video);
}


/**
 * @brief 裁判系统发送UI绘图数据
 *
 */
void Class_Referee::UART_Tx_Referee_UI()
{
    Referee_UI_Draw_String(Get_ID(), Referee_UI_Zero , 0 , 0x00, 0, 20, 2, 500, 500, "Chassis", (sizeof("chassis")-1),Referee_UI_ADD);    //配置字符信息
    //Referee_UI_Packed_String(); 
    Referee_UI_Packed_Data(&Interaction_Graphic_String); //打包字符数据
    //UART_Send_Data(UART_Manage_Object->UART_Handler, UART_Manage_Object->Tx_Buffer, UART_Manage_Object->Tx_Buffer_Length); //DMA发送
    HAL_UART_Transmit(UART_Manage_Object->UART_Handler, UART_Manage_Object->Tx_Buffer, UART_Manage_Object->Tx_Length,10); //阻塞发送

    Referee_UI_Draw_String(Get_ID(), Referee_UI_Zero , 0 , 0x00, 0, 20, 2, 500, 800, "Gimbal", (sizeof("Gimbal")-1),Referee_UI_ADD);    //配置字符信息
    //Referee_UI_Packed_String(); 
    Referee_UI_Packed_Data(&Interaction_Graphic_String); //打包字符数据
    //UART_Send_Data(UART_Manage_Object->UART_Handler, UART_Manage_Object->Tx_Buffer, UART_Manage_Object->Tx_Buffer_Length); //DMA发送
    HAL_UART_Transmit(UART_Manage_Object->UART_Handler, UART_Manage_Object->Tx_Buffer, UART_Manage_Object->Tx_Length,10); //阻塞发送

    Referee_UI_Draw_String(Get_ID(), Referee_UI_Zero , 0 , 0x00, 0, 20, 2, 500, 1200, "Fric", (sizeof("Fric")-1),Referee_UI_ADD);    //配置字符信息
    //Referee_UI_Packed_String(); 
    Referee_UI_Packed_Data(&Interaction_Graphic_String); //打包字符数据
    //UART_Send_Data(UART_Manage_Object->UART_Handler, UART_Manage_Object->Tx_Buffer, UART_Manage_Object->Tx_Buffer_Length); //DMA发送
    HAL_UART_Transmit(UART_Manage_Object->UART_Handler, UART_Manage_Object->Tx_Buffer, UART_Manage_Object->Tx_Length,10); //阻塞发送
}


/**
 * @brief 裁判系统字符数据打包
 *
 */
void Class_Referee::Referee_UI_Packed_String()
{
    uint16_t frame_length,data_len,cmd_id;
    
    cmd_id = 0x0301;    //子内容ID
    data_len = sizeof(Interaction_Graphic_String);      //字符操作数据长度
	frame_length = frameheader_len + cmd_len + data_len + crc_len;   //数据帧长度	

	memset(UART_Manage_Object->Tx_Buffer,0,frame_length);  //存储数据的数组清零
	
	/*****帧头打包*****/
	UART_Manage_Object->Tx_Buffer[0] = Frame_Header;//数据帧起始字节
	memcpy(&UART_Manage_Object->Tx_Buffer[1],(uint8_t*)&data_len, 2);//数据帧中data的长度
	UART_Manage_Object->Tx_Buffer[3] = seq;//包序号
	Append_CRC8_Check_Sum(UART_Manage_Object->Tx_Buffer,frameheader_len);  //帧头校验CRC8

	/*****命令码打包*****/
	memcpy(&UART_Manage_Object->Tx_Buffer[frameheader_len],(uint8_t*)&cmd_id, cmd_len);
	
	/*****数据打包*****/
	memcpy(&UART_Manage_Object->Tx_Buffer[frameheader_len+cmd_len], &Interaction_Graphic_String, sizeof(Interaction_Graphic_String));
	Append_CRC16_Check_Sum(UART_Manage_Object->Tx_Buffer,frame_length);  //一帧数据校验CRC16

    UART_Manage_Object->Tx_Length = frame_length;

    seq++;
}




/**
 * @brief 绘制字符串
 *
 */
void Class_Referee::Referee_UI_Draw_String(uint8_t __Robot_ID,Enum_Referee_UI_Group_Index __Group_Index, uint32_t __Serial, uint8_t __Index, uint32_t __Color, uint32_t __Font_Size,uint32_t __Line_Width, uint32_t __Start_X, uint32_t __Start_Y, char *__String ,uint32_t __String_Length, Enum_Referee_UI_Operate_Type __Operate_Type)
{
    Interaction_Graphic_String.Sender = (Enum_Referee_Data_Robots_ID)__Robot_ID;
    Interaction_Graphic_String.Receiver = (Enum_Referee_Data_Robots_Client_ID)(__Robot_ID + 0x0100);

    memcpy(Interaction_Graphic_String.String, __String, __String_Length * sizeof(uint8_t));
    Interaction_Graphic_String.Graphic_String.String.Serial = __Serial;
    Interaction_Graphic_String.Graphic_String.String.Index[0] = __Index;
    Interaction_Graphic_String.Graphic_String.String.Operation_Enum = __Operate_Type;
    Interaction_Graphic_String.Graphic_String.String.Type_Enum = 7;
    Interaction_Graphic_String.Graphic_String.String.Color_Enum = __Color;
    Interaction_Graphic_String.Graphic_String.String.Font_Size = __Font_Size;
    Interaction_Graphic_String.Graphic_String.String.Line_Width = __Line_Width;
    Interaction_Graphic_String.Graphic_String.String.Start_X = __Start_X;
    Interaction_Graphic_String.Graphic_String.String.Start_Y = __Start_Y;
    Interaction_Graphic_String.Graphic_String.String.Length = __String_Length;
}

/**
 * @brief 绘制直线
 *
 * @param __Robot_ID 机器人ID
 * Enum_Referee_UI_Group_Index __Group_Index,
 * @param __Serial 序列号
 * @param __Index 索引
 * @param __Color 颜色
 * @param __Line_Width 线宽
 * @param __Start_X 起始点X坐标
 * @param __Start_Y 起始点Y坐标
 * @param __End_X 结束点X坐标
 * @param __End_Y 结束点Y坐标
 * @param __Operate_Type 操作类型
 */
void Class_Referee::Referee_UI_Draw_Line(uint8_t __Robot_ID,Enum_Referee_UI_Group_Index __Group_Index, uint8_t __Serial, uint8_t __Index, uint32_t __Color, uint32_t __Line_Width, uint32_t __Start_X, uint32_t __Start_Y, uint32_t __End_X, uint32_t __End_Y, Enum_Referee_UI_Operate_Type __Operate_Type)
{
    Interaction_Graphic_7.Sender = (Enum_Referee_Data_Robots_ID)__Robot_ID;
    Interaction_Graphic_7.Receiver = (Enum_Referee_Data_Robots_Client_ID)(__Robot_ID + 0x0100);

    Interaction_Graphic_7.Graphic[__Group_Index].Line.Serial = __Serial;
    Interaction_Graphic_7.Graphic[__Group_Index].Line.Index[0] = __Index;
    Interaction_Graphic_7.Graphic[__Group_Index].Line.Operation_Enum = __Operate_Type;
    Interaction_Graphic_7.Graphic[__Group_Index].Line.Type_Enum = 0;
    Interaction_Graphic_7.Graphic[__Group_Index].Line.Color_Enum = __Color;
    Interaction_Graphic_7.Graphic[__Group_Index].Line.Line_Width = __Line_Width;
    Interaction_Graphic_7.Graphic[__Group_Index].Line.Start_X = __Start_X;
    Interaction_Graphic_7.Graphic[__Group_Index].Line.Start_Y = __Start_Y;
    Interaction_Graphic_7.Graphic[__Group_Index].Line.End_X = __End_X;
    Interaction_Graphic_7.Graphic[__Group_Index].Line.End_Y = __End_Y;
}

/**
 * @brief 绘制矩形
 *
 * @param __Robot_ID 机器人ID
 * @param __data_type 数据类型
 * @param __Serial 序列号
 * @param __Index 索引
 * @param __Color 颜色
 * @param __Line_Width 线宽
 * @param __Start_X 起始点X坐标
 * @param __Start_Y 起始点Y坐标
 * @param __End_X 结束点X坐标
 * @param __End_Y 结束点Y坐标
 * @param __Operate_Type 操作类型
 */
void Class_Referee::Referee_UI_Draw_Rectangle(uint8_t __Robot_ID,Enum_Referee_UI_Group_Index __Group_Index, uint8_t __Serial, uint8_t __Index, uint32_t __Color,uint32_t __Line_Width, uint32_t __Start_X, uint32_t __Start_Y,  uint32_t __End_X, uint32_t __End_Y,Enum_Referee_UI_Operate_Type __Operate_Type)
{
    Interaction_Graphic_7.Sender = (Enum_Referee_Data_Robots_ID)__Robot_ID;
    Interaction_Graphic_7.Receiver = (Enum_Referee_Data_Robots_Client_ID)(__Robot_ID + 0x0100);

    Interaction_Graphic_7.Graphic[__Group_Index].Rectangle.Serial = __Serial;
    Interaction_Graphic_7.Graphic[__Group_Index].Rectangle.Index[0] = __Index;
    Interaction_Graphic_7.Graphic[__Group_Index].Rectangle.Operation_Enum = __Operate_Type;
    Interaction_Graphic_7.Graphic[__Group_Index].Rectangle.Type_Enum = 1;
    Interaction_Graphic_7.Graphic[__Group_Index].Rectangle.Color_Enum = __Color;
    Interaction_Graphic_7.Graphic[__Group_Index].Rectangle.Line_Width = __Line_Width;
    Interaction_Graphic_7.Graphic[__Group_Index].Rectangle.Start_X = __Start_X;
    Interaction_Graphic_7.Graphic[__Group_Index].Rectangle.Start_Y = __Start_Y; 
    Interaction_Graphic_7.Graphic[__Group_Index].Rectangle.End_X = __End_X;
    Interaction_Graphic_7.Graphic[__Group_Index].Rectangle.End_Y = __End_Y;     
}

/**
 * @brief 绘制椭圆
 *
 * @param __Robot_ID 机器人ID
 * @param __data_type 数据类型
 * @param __Serial 序列号
 * @param __Index 索引
 * @param __Color 颜色
 * @param __Line_Width 线宽
 * @param __Center_X 中心点X坐标
 * @param __Center_Y 中心点Y坐标
 * @param __X_Length X轴半径
 * @param __Y_Length Y轴半径
 * @param __Operate_Type 操作类型
 */
void Class_Referee::Referee_UI_Draw_Oval(uint8_t __Robot_ID,Enum_Referee_UI_Group_Index __Group_Index,uint8_t __Serial, uint8_t __Index, uint32_t __Color, uint32_t __Line_Width, uint32_t __Center_X, uint32_t __Center_Y, uint32_t __X_Length, uint32_t __Y_Length, Enum_Referee_UI_Operate_Type __Operate_Type)
{
    Interaction_Graphic_7.Sender = (Enum_Referee_Data_Robots_ID)__Robot_ID;
    Interaction_Graphic_7.Receiver = (Enum_Referee_Data_Robots_Client_ID)(__Robot_ID + 0x0100);

    Interaction_Graphic_7.Graphic[__Group_Index].Oval.Serial = __Serial;
    Interaction_Graphic_7.Graphic[__Group_Index].Oval.Index[0] = __Index;
    Interaction_Graphic_7.Graphic[__Group_Index].Oval.Operation_Enum = __Operate_Type;
    Interaction_Graphic_7.Graphic[__Group_Index].Oval.Type_Enum = 3;
    Interaction_Graphic_7.Graphic[__Group_Index].Oval.Color_Enum = __Color;
    Interaction_Graphic_7.Graphic[__Group_Index].Oval.Line_Width = __Line_Width;
    Interaction_Graphic_7.Graphic[__Group_Index].Oval.Center_X = __Center_X;
    Interaction_Graphic_7.Graphic[__Group_Index].Oval.Center_Y= __Center_Y; 
    Interaction_Graphic_7.Graphic[__Group_Index].Oval.Half_Length_X = __X_Length;
    Interaction_Graphic_7.Graphic[__Group_Index].Oval.Half_Length_Y = __Y_Length;  
}

/**
 * @brief 绘制圆形
 *
 * @param __Robot_ID 机器人ID
 * @param __data_type 数据类型
 * @param __Serial 序列号
 * @param __Index 索引
 * @param __Color 颜色
 * @param __Line_Width 线宽
 * @param __Center_X 中心点X坐标
 * @param __Center_Y 中心点Y坐标
 * @param __Radius 半径
 * @param __Operate_Type 操作类型
 */
void Class_Referee::Referee_UI_Draw_Circle(uint8_t __Robot_ID,Enum_Referee_UI_Group_Index __Group_Index, uint8_t __Serial, uint8_t __Index, uint32_t __Color, uint32_t __Line_Width, uint32_t __Center_X, uint32_t __Center_Y, uint32_t __Radius, Enum_Referee_UI_Operate_Type __Operate_Type)
{
    Interaction_Graphic_7.Sender = (Enum_Referee_Data_Robots_ID)__Robot_ID;
    Interaction_Graphic_7.Receiver = (Enum_Referee_Data_Robots_Client_ID)(__Robot_ID + 0x0100);

    Interaction_Graphic_7.Graphic[__Group_Index].Circle.Serial = __Serial;
    Interaction_Graphic_7.Graphic[__Group_Index].Circle.Index[0] = __Index;
    Interaction_Graphic_7.Graphic[__Group_Index].Circle.Operation_Enum = __Operate_Type;
    Interaction_Graphic_7.Graphic[__Group_Index].Circle.Type_Enum = 2;
    Interaction_Graphic_7.Graphic[__Group_Index].Circle.Color_Enum = __Color;
    Interaction_Graphic_7.Graphic[__Group_Index].Circle.Line_Width = __Line_Width;
    Interaction_Graphic_7.Graphic[__Group_Index].Circle.Center_X = __Center_X;
    Interaction_Graphic_7.Graphic[__Group_Index].Circle.Center_Y= __Center_Y;   
}

/**
 * @brief 绘制浮点数
 *
 * @param __Robot_ID 机器人ID
 * @param __data_type 数据类型
 * @param __Serial 序列号
 * @param __Index 索引
 * @param __Color 颜色
 * @param __Font_Size 字体大小
 * @param __Line_Width 线宽
 * @param __Start_X 起始点X坐标
 * @param __Start_Y 起始点Y坐标
 * @param __Number 浮点数
 * @param __Operate_Type 操作类型
 */
void Class_Referee::Referee_UI_Draw_Float(uint8_t __Robot_ID,Enum_Referee_UI_Group_Index __Group_Index, uint8_t __Serial, uint8_t __Index, uint32_t __Color, uint32_t __Font_Size,uint32_t __Line_Width, uint32_t __Start_X, uint32_t __Start_Y, float __Number, Enum_Referee_UI_Operate_Type __Operate_Type)
{
    Interaction_Graphic_7.Sender = (Enum_Referee_Data_Robots_ID)__Robot_ID;
    Interaction_Graphic_7.Receiver = (Enum_Referee_Data_Robots_Client_ID)(__Robot_ID + 0x0100);

    Interaction_Graphic_7.Graphic[__Group_Index].Float.Serial = __Serial;
    Interaction_Graphic_7.Graphic[__Group_Index].Float.Index[0] = __Index;
    Interaction_Graphic_7.Graphic[__Group_Index].Float.Operation_Enum = __Operate_Type;
    Interaction_Graphic_7.Graphic[__Group_Index].Float.Type_Enum = 5;
    Interaction_Graphic_7.Graphic[__Group_Index].Float.Color_Enum = __Color;
    Interaction_Graphic_7.Graphic[__Group_Index].Float.Font_Size = __Font_Size;
    Interaction_Graphic_7.Graphic[__Group_Index].Float.Line_Width = __Line_Width;
    Interaction_Graphic_7.Graphic[__Group_Index].Float.Start_X = __Start_X;
    Interaction_Graphic_7.Graphic[__Group_Index].Float.Start_Y= __Start_Y;
    Interaction_Graphic_7.Graphic[__Group_Index].Float.Float = (int32_t)__Number*1000;      
}

/**
 * @brief 绘制整数
 *
 * @param __Robot_ID 机器人ID
 * @param __data_type 数据类型
 * @param __Serial 序列号
 * @param __Index 索引
 * @param __Color 颜色
 * @param __Font_Size 字体大小
 * @param __Line_Width 线宽
 * @param __Start_X 起始点X坐标
 * @param __Start_Y 起始点Y坐标
 * @param __Number 整数
 * @param __Operate_Type 操作类型
 */
void Class_Referee::Referee_UI_Draw_Integer(uint8_t __Robot_ID,Enum_Referee_UI_Group_Index __Group_Index, uint8_t __Serial, uint8_t __Index, uint32_t __Color, uint32_t __Font_Size,uint32_t __Line_Width, uint32_t __Start_X, uint32_t __Start_Y, int32_t __Number, Enum_Referee_UI_Operate_Type __Operate_Type)
{
    Interaction_Graphic_7.Sender = (Enum_Referee_Data_Robots_ID)__Robot_ID;
    Interaction_Graphic_7.Receiver = (Enum_Referee_Data_Robots_Client_ID)(__Robot_ID + 0x0100);

    Interaction_Graphic_7.Graphic[__Group_Index].Integer.Serial = __Serial;
    Interaction_Graphic_7.Graphic[__Group_Index].Integer.Index[0] = __Index;
    Interaction_Graphic_7.Graphic[__Group_Index].Integer.Operation_Enum = __Operate_Type;
    Interaction_Graphic_7.Graphic[__Group_Index].Integer.Type_Enum = 6;
    Interaction_Graphic_7.Graphic[__Group_Index].Integer.Color_Enum = __Color;
    Interaction_Graphic_7.Graphic[__Group_Index].Integer.Font_Size = __Font_Size;
    Interaction_Graphic_7.Graphic[__Group_Index].Integer.Line_Width = __Line_Width;
    Interaction_Graphic_7.Graphic[__Group_Index].Integer.Start_X = __Start_X;
    Interaction_Graphic_7.Graphic[__Group_Index].Integer.Start_Y= __Start_Y;
    Interaction_Graphic_7.Graphic[__Group_Index].Integer.Integer = __Number;
}



unsigned char Get_CRC8_Check_Sum(unsigned  char  *pchMessage,unsigned  int dwLength,unsigned char ucCRC8)
{
	unsigned char ucIndex;
	while (dwLength--)
	{
	ucIndex = ucCRC8^(*pchMessage++);
	ucCRC8 = CRC8_TAB[ucIndex];
	}
	return(ucCRC8);
}
/*
** Descriptions: CRC8 Verify function
** Input: Data to Verify,Stream length = Data + checksum
** Output: True or False (CRC Verify Result)
*/
unsigned int Verify_CRC8_Check_Sum(unsigned char *pchMessage, unsigned int dwLength)
{
	unsigned char ucExpected = 0;
	if ((pchMessage == 0) || (dwLength <= 2)) return 0;
	ucExpected = Get_CRC8_Check_Sum (pchMessage, dwLength-1, CRC8_INIT);
	return ( ucExpected == pchMessage[dwLength-1] );
}
/*
** Descriptions: append CRC8 to the end of data
** Input: Data to CRC and append,Stream length = Data + checksum
** Output: True or False (CRC Verify Result)
*/
void Append_CRC8_Check_Sum(unsigned char *pchMessage, unsigned int dwLength)
{
	unsigned char ucCRC = 0;
	if ((pchMessage == 0) || (dwLength <= 2)) return;
	ucCRC = Get_CRC8_Check_Sum ( (unsigned char *)pchMessage, dwLength-1, CRC8_INIT);
	pchMessage[dwLength-1] = ucCRC;
}

/*
** Descriptions: CRC16 checksum function
** Input: Data to check,Stream length, initialized checksum
** Output: CRC checksum
*/
uint16_t Get_CRC16_Check_Sum(uint8_t *pchMessage,uint32_t dwLength,uint16_t wCRC)
{
uint8_t chData;
	if (pchMessage == NULL)
	{
		return 0xFFFF;
	}
	while(dwLength--)
	{
		chData = *pchMessage++;
		(wCRC) = ((uint16_t)(wCRC) >> 8) ^ CRC16_Table[((uint16_t)(wCRC)^(uint16_t)(chData)) & 0x00ff];
	}
	return wCRC;
}

/*
** Descriptions: CRC16 Verify function
** Input: Data to Verify,Stream length = Data + checksum
** Output: True or False (CRC Verify Result)
*/
uint32_t Verify_CRC16_Check_Sum(uint8_t *pchMessage, uint32_t dwLength)
{
	uint16_t wExpected = 0;
	if ((pchMessage == NULL) || (dwLength <= 2))
	{
	return 0;
	}
	wExpected = Get_CRC16_Check_Sum ( pchMessage, dwLength - 2, CRC16_INIT);
	return ((wExpected & 0xff) == pchMessage[dwLength - 2] && ((wExpected >> 8) & 0xff) ==
	pchMessage[dwLength - 1]);
}
/*
** Descriptions: append CRC16 to the end of data
** Input: Data to CRC and append,Stream length = Data + checksum
** Output: True or False (CRC Verify Result)
*/
void Append_CRC16_Check_Sum(uint8_t * pchMessage,uint32_t dwLength)
{
	uint16_t wCRC = 0;
	if ((pchMessage == NULL) || (dwLength <= 2))
	{
	return;
	}
	wCRC = Get_CRC16_Check_Sum ( (uint8_t *)pchMessage, dwLength-2, CRC16_INIT );
	pchMessage[dwLength-2] = (uint8_t)(wCRC & 0x00ff);
	pchMessage[dwLength-1] = (uint8_t)((wCRC >> 8)& 0x00ff);
}
/************************ COPYRIGHT(C) USTC-ROBOWALKER **************************/
