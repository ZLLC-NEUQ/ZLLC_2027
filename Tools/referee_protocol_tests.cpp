// Host regression tests. Inputs are constructed independently from the PDF's
// wire layouts; production parsing/transmit code is linked from dvc_referee.cpp.
#include "dvc_referee.h"
#include "dvc_GraphicsSendTask.h"
#include <algorithm>
#include <assert.h>
#include <cstring>
#include <iostream>
#include <vector>

Struct_UART_Manage_Object UART1_Manage_Object = {}, UART2_Manage_Object = {};
Struct_UART_Manage_Object UART3_Manage_Object = {}, UART4_Manage_Object = {};
Struct_UART_Manage_Object UART5_Manage_Object = {}, UART6_Manage_Object = {};
Struct_UART_Manage_Object UART7_Manage_Object = {}, UART8_Manage_Object = {};
Struct_UART_Manage_Object UART9_Manage_Object = {}, UART10_Manage_Object = {};
UART_HandleTypeDef huart1 = {USART1, HAL_UART_STATE_READY}, huart10 = {USART10, HAL_UART_STATE_READY};
static UART_HandleTypeDef huart2 = {USART2, HAL_UART_STATE_READY};
JudgeReceive_t JudgeReceiveData = {};
unsigned test_critical_depth = 0, test_critical_entries = 0;
static uint32_t tick_ms = 0;
static std::vector<std::vector<uint8_t> > transmissions;
static HAL_StatusTypeDef uart_result = HAL_OK;
static uint8_t queue_result = 1;
static UART_HandleTypeDef *last_uart = NULL;
static UART_HandleTypeDef *recovery_uart = NULL;
static const uint8_t *active_dma_data = NULL;
static uint16_t active_dma_length = 0;

uint32_t HAL_GetTick() { return tick_ms; }
uint8_t UART_RecoveryPending(UART_HandleTypeDef *uart) { return uart == recovery_uart; }
uint8_t UART_Send_Data(UART_HandleTypeDef *uart, uint8_t *data, uint16_t size)
{
    if (uart_result == HAL_OK) {
        last_uart = uart;
        active_dma_data = data;
        active_dma_length = size;
        transmissions.push_back(std::vector<uint8_t>(data, data + size));
    }
    return uart_result;
}
HAL_StatusTypeDef HAL_UART_Transmit(UART_HandleTypeDef *uart, uint8_t *data, uint16_t size, uint32_t)
{
    return static_cast<HAL_StatusTypeDef>(UART_Send_Data(uart, data, size));
}
HAL_StatusTypeDef HAL_UART_Transmit_DMA(UART_HandleTypeDef *uart, uint8_t *data, uint16_t size)
{
    return static_cast<HAL_StatusTypeDef>(UART_Send_Data(uart, data, size));
}
void Referee_DMA_EnqueuePacket(const uint8_t *data, uint16_t size)
{
    transmissions.push_back(std::vector<uint8_t>(data, data + size));
}
uint8_t Referee_DMA_TryEnqueuePacket(const uint8_t *data, uint16_t size)
{
    if (queue_result) {
        last_uart = NULL;
        transmissions.push_back(std::vector<uint8_t>(data, data + size));
    }
    return queue_result;
}

static uint8_t crc8(const uint8_t *data, size_t length)
{
    uint8_t crc = 0xff;
    while (length--) {
        crc ^= *data++;
        for (unsigned bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ ((crc & 1) ? 0x8c : 0);
    }
    return crc;
}
static uint16_t crc16(const uint8_t *data, size_t length)
{
    uint16_t crc = 0xffff;
    while (length--) {
        crc ^= *data++;
        for (unsigned bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ ((crc & 1) ? 0x8408 : 0);
    }
    return crc;
}
static uint16_t u16(const uint8_t *data) { return data[0] | (static_cast<uint16_t>(data[1]) << 8); }
static void put16(std::vector<uint8_t> &data, size_t offset, uint16_t value)
{
    data.at(offset) = value & 0xff; data.at(offset + 1) = value >> 8;
}
static std::vector<uint8_t> frame(uint16_t command, const std::vector<uint8_t> &payload, uint8_t seq = 0)
{
    std::vector<uint8_t> result(payload.size() + 9, 0);
    result[0] = 0xa5; put16(result, 1, static_cast<uint16_t>(payload.size())); result[3] = seq;
    result[4] = crc8(result.data(), 4); put16(result, 5, command);
    std::copy(payload.begin(), payload.end(), result.begin() + 7);
    put16(result, result.size() - 2, crc16(result.data(), result.size() - 2));
    return result;
}
static void receive(Class_Referee &referee, std::vector<uint8_t> data)
{
    referee.UART_RxCpltCallback(data.data(), static_cast<uint16_t>(data.size()));
}
static void check_crc()
{
    uint8_t known[] = {'1','2','3','4','5','6','7','8','9'};
    assert(crc8(known, sizeof(known)) == 0x0b);
    assert(crc16(known, sizeof(known)) == 0x6f91);
    assert(Get_CRC8_Check_Sum(known, sizeof(known), 0xff) == 0x0b);
    assert(Get_CRC16_Check_Sum(known, sizeof(known), 0xffff) == 0x6f91);
    std::vector<uint8_t> packet = frame(0x0002, std::vector<uint8_t>(1, 1));
    assert(Verify_CRC8_Check_Sum(packet.data(), 5));
    assert(Verify_CRC16_Check_Sum(packet.data(), packet.size()));
    packet.back() ^= 0x40;
    assert(!Verify_CRC16_Check_Sum(packet.data(), packet.size()));
}

struct RxCase { uint16_t id; size_t length; unsigned link; };
// Byte lengths are independently transcribed from 2027 V1.0.0 table 1-5.
static const RxCase rx_cases[] = {
    {0x0001,11,0}, {0x0002,1,0}, {0x0003,24,0}, {0x0101,8,0},
    {0x0104,3,0}, {0x0105,3,0}, {0x0201,17,0}, {0x0202,14,0},
    {0x0203,12,0}, {0x0204,7,0}, {0x0206,7,0}, {0x0207,7,0},
    {0x0208,8,0}, {0x0209,6,0}, {0x020a,6,0}, {0x020b,40,0},
    {0x020c,2,0}, {0x020d,14,0}, {0x020e,8,0}, {0x0303,12,0},
    {0x0311,60,1}, {0x0a01,24,2}, {0x0a02,12,2}, {0x0a03,10,2},
    {0x0a04,12,2}, {0x0a05,47,2}, {0x0a06,6,2}
};

class TestReferee : public Class_Referee {
public:
    TestReferee() { Init(&huart10); }
    struct Data { const void *pointer; size_t length; };
    Data raw(uint16_t id) const
    {
        switch (id) {
#define DATA(CMD, MEMBER) case CMD: return Data{&MEMBER, sizeof(MEMBER)}
        DATA(0x0001, Game_Status); DATA(0x0002, Game_Result); DATA(0x0003, Game_Robot_HP);
        DATA(0x0101, Event_Data); DATA(0x0104, Event_Referee_Warning); DATA(0x0105, Event_Dart_Remaining_Time);
        DATA(0x0201, Robot_Status); DATA(0x0202, Robot_Power_Heat); DATA(0x0203, Robot_Position);
        DATA(0x0204, Robot_Buff); DATA(0x0206, Robot_Damage); DATA(0x0207, Robot_Booster);
        DATA(0x0208, Robot_Remaining_Ammo); DATA(0x0209, Robot_RFID); DATA(0x020a, Robot_Dart_Command);
        DATA(0x020b, Ground_Position); DATA(0x020c, Radar_Mark); DATA(0x020d, Sentry_Info);
        DATA(0x020e, Radar_Info); DATA(0x0303, Map_Command); DATA(0x0311, Custom_Command);
        DATA(0x0a01, Wireless_Position); DATA(0x0a02, Wireless_HP); DATA(0x0a03, Wireless_Ammo);
        DATA(0x0a04, Wireless_State); DATA(0x0a05, Wireless_Buff); DATA(0x0a06, Wireless_Key);
#undef DATA
        default: assert(false); return Data{NULL, 0};
        }
    }
    void local_id(uint8_t id) { Robot_Status.Robot_ID = static_cast<Enum_Referee_Data_Robots_ID>(id); }
    UART_HandleTypeDef *video_uart() const { return Video_UART_Manage_Object == NULL ? NULL : Video_UART_Manage_Object->UART_Handler; }
};
static void feed(TestReferee &referee, unsigned link, const std::vector<uint8_t> &packet,
                 size_t begin = 0, size_t end = static_cast<size_t>(-1))
{
    if (end == static_cast<size_t>(-1)) end = packet.size();
    uint8_t *data = const_cast<uint8_t *>(packet.data() + begin);
    uint16_t length = static_cast<uint16_t>(end - begin);
    if (link == 0) referee.UART_RxCpltCallback(data, length);
    else if (link == 1) referee.Video_UART_RxCpltCallback(data, length);
    else referee.Wireless_RxCpltCallback(data, length);
}
static std::vector<uint8_t> pattern(size_t length, unsigned seed)
{
    std::vector<uint8_t> data(length);
    for (size_t i = 0; i < length; ++i) data[i] = static_cast<uint8_t>(1 + ((i * 17 + seed) % 127));
    return data;
}
static void expect_data(const TestReferee &referee, uint16_t id, const std::vector<uint8_t> &payload)
{
    TestReferee::Data stored = referee.raw(id);
    assert(stored.length == payload.size());
    assert(std::memcmp(stored.pointer, payload.data(), payload.size()) == 0);
}
static void check_all_rx()
{
    for (const RxCase &item : rx_cases) {
        const std::vector<uint8_t> data = pattern(item.length, item.id);
        const std::vector<uint8_t> packet = frame(item.id, data);
        // Every split point, including inside the CRCs, must preserve the frame.
        for (size_t split = 1; split < packet.size(); ++split) {
            TestReferee referee;
            feed(referee, item.link, packet, 0, split);
            feed(referee, item.link, packet, split);
            expect_data(referee, item.id, data);
        }
        TestReferee bytewise;
        for (size_t i = 0; i < packet.size(); ++i) feed(bytewise, item.link, packet, i, i + 1);
        expect_data(bytewise, item.id, data);
        // A valid CRC cannot make the wrong wire length acceptable.
        for (int delta : {-1, 1}) {
            TestReferee referee;
            feed(referee, item.link, packet);
            feed(referee, item.link, frame(item.id, pattern(item.length + delta, item.id + 77)));
            expect_data(referee, item.id, data);
        }
        // Each physical stream accepts only the command families it carries.
        for (unsigned wrong_link = 0; wrong_link < 3; ++wrong_link) {
            if (wrong_link == item.link) continue;
            TestReferee referee;
            TestReferee::Data stored = referee.raw(item.id);
            std::vector<uint8_t> before(static_cast<const uint8_t *>(stored.pointer),
                                        static_cast<const uint8_t *>(stored.pointer) + stored.length);
            feed(referee, wrong_link, packet);
            expect_data(referee, item.id, before);
        }
    }
    TestReferee glued;
    for (unsigned link = 0; link < 3; ++link) {
        std::vector<uint8_t> joined;
        for (const RxCase &item : rx_cases) if (item.link == link) {
            std::vector<uint8_t> packet = frame(item.id, pattern(item.length, item.id));
            joined.insert(joined.end(), packet.begin(), packet.end());
        }
        feed(glued, link, joined);
    }
    for (const RxCase &item : rx_cases) expect_data(glued, item.id, pattern(item.length, item.id));
}
static void check_resynchronization()
{
    const std::vector<uint8_t> first = frame(0x0201, pattern(17, 23));
    const std::vector<uint8_t> second = frame(0x0202, pattern(14, 31));
    for (unsigned error = 0; error < 4; ++error) {
        std::vector<uint8_t> damaged = first;
        if (error == 0) damaged[4] ^= 1; // invalid header CRC
        if (error == 1) damaged.back() ^= 1; // invalid frame CRC
        if (error == 2) { put16(damaged, 1, 65535); damaged[4] = crc8(damaged.data(), 4); }
        if (error == 3) { // A bad frame can contain the beginning of the next valid frame.
            damaged.assign(7, 0); damaged[0] = 0xa5;
            put16(damaged, 1, static_cast<uint16_t>(second.size() - 2));
            damaged[4] = crc8(damaged.data(), 4);
        }
        damaged.insert(damaged.begin(), {0x00,0xff,0xa4,0x71});
        damaged.insert(damaged.end(), second.begin(), second.end());
        TestReferee referee;
        feed(referee, 0, damaged);
        expect_data(referee, 0x0202, pattern(14, 31));
    }
    TestReferee no_noise_online;
    receive(no_noise_online, std::vector<uint8_t>{0x20,0x30,0x00,0x10});
    no_noise_online.TIM1msMod50_Alive_PeriodElapsedCallback();
    assert(no_noise_online.Get_Referee_Status() == Referee_Status_DISABLE);
    receive(no_noise_online, frame(0x7777, pattern(7, 1)));
    no_noise_online.TIM1msMod50_Alive_PeriodElapsedCallback();
    assert(no_noise_online.Get_Referee_Status() == Referee_Status_DISABLE);
    receive(no_noise_online, second);
    no_noise_online.TIM1msMod50_Alive_PeriodElapsedCallback();
    assert(no_noise_online.Get_Referee_Status() == Referee_Status_ENABLE);
    no_noise_online.TIM1msMod50_Alive_PeriodElapsedCallback();
    assert(no_noise_online.Get_Referee_Status() == Referee_Status_DISABLE);
    // Normal/video/wireless fragments must never share or overwrite stream state.
    TestReferee separate;
    const std::vector<uint8_t> video = frame(0x0311, pattern(60, 99));
    const std::vector<uint8_t> wireless = frame(0x0a05, pattern(47, 111));
    feed(separate, 0, first, 0, 8); feed(separate, 1, video, 0, 13); feed(separate, 2, wireless, 0, 9);
    feed(separate, 0, first, 8); feed(separate, 2, wireless, 9); feed(separate, 1, video, 13);
    expect_data(separate, 0x0201, pattern(17, 23)); expect_data(separate, 0x0311, pattern(60, 99));
    expect_data(separate, 0x0a05, pattern(47, 111));
}
static void check_map_repeat()
{
    TestReferee referee;
    std::vector<uint8_t> data = pattern(12, 5);
    put16(data, 10, 0x0103);
    for (unsigned i = 0; i < 6; ++i) feed(referee, 0, frame(0x0303, data, i));
    assert(referee.Get_Map_Command_Receive_Count() == 6);
    assert(referee.Get_Map_Command_Update_Count() == 1);
    data[10] = 0x04;
    feed(referee, 0, frame(0x0303, data));
    assert(referee.Get_Map_Command_Receive_Count() == 7);
    assert(referee.Get_Map_Command_Update_Count() == 2);
    data[0] ^= 1;
    feed(referee, 0, frame(0x0303, data));
    assert(referee.Get_Map_Command_Update_Count() == 3);
}
static std::vector<uint8_t> interaction(uint16_t sender, uint16_t receiver, uint16_t content,
                                         const std::vector<uint8_t> &user_data)
{
    std::vector<uint8_t> result(6 + user_data.size(), 0);
    put16(result, 0, content); put16(result, 2, sender); put16(result, 4, receiver);
    std::copy(user_data.begin(), user_data.end(), result.begin() + 6);
    return result;
}
static void check_interaction_rx()
{
    TestReferee referee;
    referee.local_id(3);
    for (size_t length : {size_t(0),size_t(1),size_t(30),size_t(112)}) {
        std::vector<uint8_t> contents = pattern(length, 19);
        std::vector<uint8_t> packet = frame(0x0301, interaction(7,3,0x0200,contents));
        for (size_t i = 0; i < packet.size(); ++i) feed(referee, 0, packet, i, i + 1);
        assert(referee.Get_Interaction_Data_Length() == length);
        const Struct_Referee_Data_Interaction_Students &stored = referee.Get_Interaction();
        assert(stored.Header == 0x0200 && stored.Sender == 7 && stored.Receiver == 3);
        assert(std::memcmp(stored.Data, contents.data(), length) == 0);
        for (size_t i = length; i < 112; ++i) assert(stored.Data[i] == 0);
    }
    assert(referee.Get_Interaction_Receive_Count() == 4);
    feed(referee, 0, frame(0x0301, pattern(5, 1)));
    feed(referee, 0, frame(0x0301, interaction(7,3,0x0200,pattern(113, 3))));
    assert(referee.Get_Interaction_Receive_Count() == 4);
    for (const std::vector<uint8_t> &invalid : {
             interaction(107,3,0x0200,pattern(1,3)), interaction(7,4,0x0200,pattern(1,3)),
             interaction(2,3,0x0200,pattern(1,3)), interaction(7,3,0x0101,pattern(15,3))}) {
        feed(referee,0,frame(0x0301,invalid));
        assert(referee.Get_Interaction_Receive_Count() == 4);
    }
}
static const std::vector<uint8_t> &expect_tx(uint16_t id, size_t payload_size)
{
    assert(!transmissions.empty());
    const std::vector<uint8_t> &packet = transmissions.back();
    assert(packet.size() == payload_size + 9 && packet[0] == 0xa5);
    assert(u16(packet.data() + 1) == payload_size && u16(packet.data() + 5) == id);
    assert(packet[4] == crc8(packet.data(), 4));
    assert(u16(packet.data() + packet.size() - 2) == crc16(packet.data(), packet.size() - 2));
    return packet;
}
static void check_interaction_tx()
{
    transmissions.clear(); tick_ms = 1000;
    TestReferee referee;
    const std::vector<uint8_t> data = pattern(112, 44);
    assert(!referee.Send_Interaction(7, 0x0200, data.data(), 112)); // own identity not received yet
    referee.local_id(3);
    assert(referee.Send_Interaction(7, 0x0200, data.data(), 112));
    const std::vector<uint8_t> initial = expect_tx(0x0301, 118);
    assert(u16(initial.data() + 7) == 0x0200 && u16(initial.data() + 9) == 3 && u16(initial.data() + 11) == 7);
    assert(std::memcmp(initial.data() + 13, data.data(), 112) == 0);
    assert(last_uart == NULL); // common link is handed to the existing shared DMA queue
    assert(!referee.Send_Interaction(7,0x0201,data.data(),112));
    tick_ms += 33;
    assert(!referee.Send_Interaction(7,0x0201,data.data(),112));
    ++tick_ms;
    assert(referee.Send_Interaction(7,0x0201,data.data(),112));
    assert(expect_tx(0x0301,118)[3] == static_cast<uint8_t>(initial[3] + 1));
    tick_ms += 100;
    const size_t before = transmissions.size();
    for (uint16_t receiver : {uint16_t(0),uint16_t(2),uint16_t(102),uint16_t(107),uint16_t(0x8080)})
        assert(!referee.Send_Interaction(receiver,0x0200,data.data(),112));
    assert(!referee.Send_Interaction(7,0x0199,data.data(),112));
    assert(!referee.Send_Interaction(7,0x0300,data.data(),112));
    assert(!referee.Send_Interaction(7,0x0200,data.data(),113));
    assert(!referee.Send_Interaction(7,0x0200,NULL,1));
    assert(transmissions.size() == before);
    // Queue rejection must not consume the time slot or packet sequence.
    queue_result = 0;
    assert(!referee.Send_Interaction(7,0x0200,data.data(),112));
    queue_result = 1;
    assert(referee.Send_Interaction(7,0x0200,NULL,0));
    expect_tx(0x0301,6);
    // Unsigned elapsed-time arithmetic must survive the HAL tick rollover.
    TestReferee wrapping;
    wrapping.local_id(103);
    tick_ms = 0xfffffff0u;
    assert(wrapping.Send_Interaction(107,0x02ff,data.data(),1));
    tick_ms = 0x11u;
    assert(!wrapping.Send_Interaction(107,0x02ff,data.data(),1));
    tick_ms = 0x12u;
    assert(wrapping.Send_Interaction(107,0x02ff,data.data(),1));
}
static void check_decision_and_map_tx()
{
    transmissions.clear(); tick_ms = 4000;
    Struct_Referee_Tx_Data_Sentry_Command sentry_data = {};
    sentry_data.sentry_cmd = 0x01600001; // revive, move posture, energy activation; no reserved bits
    Struct_Referee_Tx_Data_Radar_Command radar_data = {};
    radar_data.radar_cmd = 1; radar_data.password_cmd = 2;
    radar_data.password[0] = 'A'; radar_data.armor_fortify_cmd = 1;
    radar_data.fortify_center_x = 1234; radar_data.fortify_center_y = 5678;
    TestReferee infantry;
    infantry.local_id(3);
    assert(!infantry.Send_Sentry_Command(sentry_data));
    assert(!infantry.Send_Radar_Command(radar_data));
    TestReferee sentry;
    sentry.local_id(7);
    assert(sentry.Send_Sentry_Command(sentry_data));
    const std::vector<uint8_t> decision = expect_tx(0x0301,10);
    assert(u16(decision.data() + 7) == 0x0120 && u16(decision.data() + 9) == 7 && u16(decision.data() + 11) == 0x8080);
    assert(std::memcmp(decision.data() + 13,&sentry_data,4) == 0);
    TestReferee radar;
    radar.local_id(109);
    assert(radar.Send_Radar_Command(radar_data));
    const std::vector<uint8_t> radar_decision = expect_tx(0x0301,19);
    assert(u16(radar_decision.data() + 7) == 0x0121 && u16(radar_decision.data() + 9) == 109);
    assert(u16(radar_decision.data() + 11) == 0x8080 && std::memcmp(radar_decision.data() + 13,&radar_data,13) == 0);
    Struct_Referee_Tx_Data_Map_Position positions = {};
    positions.positions[0][0] = 321; positions.positions[11][1] = 654;
    assert(!infantry.Send_Map_Position(positions));
    assert(radar.Send_Map_Position(positions));
    assert(std::memcmp(expect_tx(0x0305,48).data() + 7,&positions,48) == 0);
    tick_ms += 199; assert(!radar.Send_Map_Position(positions));
    ++tick_ms; assert(radar.Send_Map_Position(positions));
    Struct_Referee_Tx_Data_Map_Path path = {};
    path.intention = 3; path.start_position_x = 100; path.start_position_y = 70;
    path.delta_x[0] = -5; path.delta_y[48] = 9; path.sender_id = 7;
    assert(sentry.Send_Map_Path(path));
    const std::vector<uint8_t> path_packet = expect_tx(0x0307,105);
    assert(std::memcmp(path_packet.data() + 7,&path,105) == 0);
    tick_ms += 999; assert(!sentry.Send_Map_Path(path));
    ++tick_ms; assert(sentry.Send_Map_Path(path));
    tick_ms += 1000; path.sender_id = 107;
    assert(sentry.Send_Map_Path(path));
    assert(u16(expect_tx(0x0307,105).data() + 110) == 7); // sender is always the local robot
    path.sender_id = 7; path.intention = 0;
    assert(!sentry.Send_Map_Path(path));
    // UTF-16LE supports up to 15 code units and pads unused bytes.
    const uint8_t message[] = {0x2d,0x4e,0x87,0x65};
    assert(infantry.Send_Map_Robot_Data(0x0104,message,sizeof(message)));
    const std::vector<uint8_t> text = expect_tx(0x0308,34);
    assert(u16(text.data() + 7) == 3 && u16(text.data() + 9) == 0x0104);
    assert(std::memcmp(text.data() + 11,message,sizeof(message)) == 0);
    for (size_t i = 15; i < 41; ++i) assert(text[i] == 0);
    tick_ms += 333; assert(!infantry.Send_Map_Robot_Data(0x0104,message,4));
    ++tick_ms; assert(infantry.Send_Map_Robot_Data(0x0104,message,4));
    tick_ms += 1000;
    assert(!infantry.Send_Map_Robot_Data(0x0167,message,4));
    assert(!infantry.Send_Map_Robot_Data(0x0102,message,4));
    assert(!infantry.Send_Map_Robot_Data(0x0104,message,3));
    assert(!infantry.Send_Map_Robot_Data(0x0104,message,32));
}
static void check_video_tx()
{
    transmissions.clear(); tick_ms = 10000;
    TestReferee referee;
    referee.local_id(3);
    const std::vector<uint8_t> contents = pattern(300,17);
    assert(!referee.Send_Custom_Data(contents.data(),300));
    referee.Init_Video_UART(&huart1);
    assert(referee.Send_Custom_Data(contents.data(),300));
    const std::vector<uint8_t> maximum = expect_tx(0x0310,300);
    assert(maximum.size() == 309 && std::memcmp(maximum.data() + 7,contents.data(),300) == 0);
    assert(last_uart == &huart1);
    tick_ms += 19; assert(!referee.Send_Custom_Data(contents.data(),1));
    ++tick_ms; assert(referee.Send_Custom_Data(contents.data(),1));
    const std::vector<uint8_t> padded = expect_tx(0x0310,300);
    assert(padded[7] == contents[0]);
    for (size_t i = 8; i < 307; ++i) assert(padded[i] == 0);
    tick_ms += 20;
    huart1.gState = HAL_UART_STATE_BUSY_TX;
    assert(!referee.Send_Custom_Data(contents.data(),300));
    huart1.gState = HAL_UART_STATE_READY;
    assert(!referee.Send_Custom_Data(contents.data(),301));
    assert(!referee.Send_Custom_Data(NULL,1));
    uart_result = HAL_BUSY;
    assert(!referee.Send_Custom_Data(contents.data(),300));
    uart_result = HAL_OK;
    assert(referee.Send_Custom_Data(NULL,0));
    expect_tx(0x0310,300);
}
static void check_getters()
{
    TestReferee referee;
    std::vector<uint8_t> hp(24,0);
    put16(hp,0,501); put16(hp,4,503); put16(hp,6,504); put16(hp,8,static_cast<uint16_t>(-234));
    put16(hp,10,507); put16(hp,12,601); put16(hp,14,602); put16(hp,16,603); put16(hp,18,604);
    put16(hp,20,2345); put16(hp,22,3456);
    feed(referee,0,frame(0x0003,hp));
    referee.local_id(103);
    assert(referee.Get_HP(static_cast<Enum_Referee_Data_Robots_ID>(101)) == 501);
    assert(referee.Get_HP(static_cast<Enum_Referee_Data_Robots_ID>(103)) == 503);
    assert(referee.Get_HP(static_cast<Enum_Referee_Data_Robots_ID>(104)) == 504);
    assert(referee.Get_HP(static_cast<Enum_Referee_Data_Robots_ID>(107)) == 507);
    assert(referee.Get_HP(static_cast<Enum_Referee_Data_Robots_ID>(1)) == 0);
    assert(referee.Get_HP(static_cast<Enum_Referee_Data_Robots_ID>(102)) == 0);
    assert(referee.Get_HP(static_cast<Enum_Referee_Data_Robots_ID>(11)) == 601);
    assert(referee.Get_HP(static_cast<Enum_Referee_Data_Robots_ID>(111)) == 603);
    assert(referee.Get_HP(static_cast<Enum_Referee_Data_Robots_ID>(110)) == 602);
    assert(referee.Get_HP(static_cast<Enum_Referee_Data_Robots_ID>(10)) == 604);
    assert(referee.Get_Damage_Difference() == -234 && referee.Get_Ally_Control_Degree() == 2345);
    assert(referee.Get_Enemy_Control_Degree() == 3456);
    std::vector<uint8_t> buff = {10,0xff,0xff,50,30,0x34,0x12};
    feed(referee,0,frame(0x0204,buff));
    assert(referee.Get_Booster_Cooling_Buff_Rate() == 65535 && referee.Get_Damage_Buff_Rate() == 0x1234);
    std::vector<uint8_t> damage(7,0);
    damage[0] = 0x53; put16(damage,1,0xfffe); put16(damage,3,static_cast<uint16_t>(-1234)); put16(damage,5,static_cast<uint16_t>(-2345));
    feed(referee,0,frame(0x0206,damage));
    assert(referee.Get_Armor_Attacked_ID() == 3 && referee.Get_Hit_Sequence() == 0xfffe);
    assert(referee.Get_Hit_Position_X() == -1234 && referee.Get_Hit_Position_Y() == -2345);
    std::vector<uint8_t> power = pattern(14,34);
    put16(power,8,321); put16(power,10,654); put16(power,12,987);
    feed(referee,0,frame(0x0202,power));
    assert(referee.Get_Chassis_Voltage() == 0 && referee.Get_Chassis_Current() == 0 && referee.Get_Chassis_Power() == 0);
    assert(referee.Get_Chassis_Energy_Buffer() == 321 && referee.Get_Booster_17mm_1_Heat() == 654);
    assert(referee.Get_Booster_42mm_Heat() == 987);
    std::vector<uint8_t> status(17,0);
    status[0] = 103;
    const float speed = 29.25f;
    std::memcpy(status.data() + 12,&speed,4);
    feed(referee,0,frame(0x0201,status));
    assert(referee.Get_Bullet_Speed_Limit() == speed);
    feed(referee,0,frame(0x0209,std::vector<uint8_t>{0x01,0,0,0x80,0x01,0x80}));
    assert(referee.Get_RFID_Status() == 0x80000001u && referee.Get_RFID_Status_2() == 0x8001);
    assert(referee.Get_RFID_Bit(0) && referee.Get_RFID_Bit(31) && !referee.Get_RFID_Bit(32));
    assert(referee.Get_RFID_Bit_2(0) && referee.Get_RFID_Bit_2(15) && !referee.Get_RFID_Bit_2(16));
#ifdef GIMBAL
    referee.Set_Robot_ID(static_cast<Enum_Referee_Data_Robots_ID>(104));
    assert(referee.Get_ID() == static_cast<Enum_Referee_Data_Robots_ID>(104));
    referee.Set_Game_Stage(static_cast<Enum_Referee_Game_Status_Stage>(4));
    assert(referee.Get_Game_Stage() == static_cast<Enum_Referee_Game_Status_Stage>(4));
    referee.Set_Booster_17mm_1_Heat(321);
    referee.Set_Booster_17mm_1_Heat_CD(654);
    referee.Set_Booster_17mm_1_Heat_Max(987);
    referee.Set_Booster_Speed(27.5f);
    assert(referee.Get_Booster_17mm_1_Heat() == 321);
    assert(referee.Get_Booster_17mm_1_Heat_CD() == 654 && referee.Get_Booster_17mm_1_Heat_Max() == 987);
    assert(referee.Get_Shoot_Speed() == 27.5f);
#endif
}
static void check_transport_ownership()
{
    tick_ms = 20000; transmissions.clear();
    TestReferee referee;
    referee.local_id(3);
    referee.Init_Video_UART(&huart1);
    const std::vector<uint8_t> contents = pattern(300,43);
    assert(referee.Send_Custom_Data(contents.data(),300));
    assert(active_dma_data != NULL && active_dma_length == 309);
    const std::vector<uint8_t> in_flight(active_dma_data,active_dma_data + active_dma_length);
    huart1.gState = HAL_UART_STATE_BUSY_TX;
    tick_ms += 20;
    referee.Init_Video_UART(&huart2);
    assert(referee.video_uart() == &huart1);
    assert(!referee.Send_Custom_Data(NULL,0));
    assert(std::memcmp(active_dma_data,in_flight.data(),309) == 0);
    huart1.gState = HAL_UART_STATE_READY;
    recovery_uart = &huart1;
    referee.Init_Video_UART(&huart2);
    assert(referee.video_uart() == &huart1);
    assert(!referee.Send_Custom_Data(NULL,0));
    assert(std::memcmp(active_dma_data,in_flight.data(),309) == 0);
    recovery_uart = NULL;
    referee.Init_Video_UART(&huart2);
    assert(referee.video_uart() == &huart2);
    assert(referee.Send_Custom_Data(NULL,0));
    assert(last_uart == &huart2);
    // Selecting the former video UART as a normal link must not retain both bindings.
    referee.Init(&huart2);
    referee.local_id(3);
    assert(referee.video_uart() == NULL);
    assert(!referee.Send_Custom_Data(NULL,0));
    recovery_uart = &huart2;
    std::vector<uint8_t> before(UART2_Manage_Object.Tx_Buffer,UART2_Manage_Object.Tx_Buffer + UART_BUFFER_SIZE);
    assert(!referee.Send_Interaction(7,0x0200,contents.data(),112));
    assert(std::memcmp(UART2_Manage_Object.Tx_Buffer,before.data(),UART_BUFFER_SIZE) == 0);
    recovery_uart = NULL;
    assert(referee.Send_Interaction(7,0x0200,contents.data(),112));
    assert(last_uart == &huart2);
    assert(test_critical_depth == 0 && test_critical_entries > 0);
}

int main()
{
    check_crc();
    std::cout << "PASS: independent CRC8/CRC16 known vectors and production CRC functions.\n";
    check_all_rx();
    std::cout << "PASS: all 27 receive commands, every split point, glued frames, exact lengths and physical links.\n";
    check_resynchronization();
    std::cout << "PASS: noise/CRC/length resynchronization, valid-packet online state and independent streams.\n";
    check_map_repeat();
    std::cout << "PASS: repeated map commands count reception while updating only when content changes.\n";
    check_interaction_rx();
    check_interaction_tx();
    std::cout << "PASS: variable-length 0x0301, identity/recipient checks, queue failure, rate limits and tick wrap.\n";
    check_decision_and_map_tx();
    check_video_tx();
    std::cout << "PASS: sentry/radar decisions, map/path/text frames and 309-byte video packets with padding/busy handling.\n";
    check_getters();
    std::cout << "PASS: blue-team HP mapping, fixed outposts, signed hit data, 16-bit buffs and reserved power fields.\n";
    check_transport_ownership();
    std::cout << "PASS: DMA-buffer ownership during busy/recovery, safe UART rebinding and balanced task critical sections.\n";
    return 0;
}
