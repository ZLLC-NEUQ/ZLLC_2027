# 2027 裁判系统机器人端接口

依据《RoboMaster 2027 机甲大师高校系列赛通信协议 V1.0.0（20261029）》第 1 章实现。适用代码为 `User/Device/Inc/dvc_referee.h`、`User/Device/Src/dvc_referee.cpp`。第 2 章的 MQTT / Protobuf 属于自定义终端侧，不加入机器人串口解包器。

## 接收接口与物理链路

| 链路 | 输入接口 | 接收命令 |
| --- | --- | --- |
| 常规链路，电源管理模块 User 串口，115200、8N1 | `UART_RxCpltCallback(data, length)` | `0001/0002/0003`、`0101/0104/0105`、`0201/0202/0203/0204/0206/0207/0208/0209/020A/020B/020C/020D/020E`、`0301/0303` |
| 图传链路，图传发送端串口，921600、8N1 | `Video_UART_RxCpltCallback(data, length)` | `0311`，固定 60 字节内容 |
| 雷达无线链路，物理层解调后的协议数据 | `Wireless_RxCpltCallback(data, length)` | `0A01/0A02/0A03/0A04/0A05/0A06` |

输入为 `0xA5` 帧格式的字节流，包含帧头、命令字、数据与 CRC；可分片或一次包含多帧。三个输入各自保存未完成的帧，不共享接收缓存。接收器验证 CRC8、CRC16、命令所属链路与数据长度，错误帧会继续搜索下一个帧头。未列入这份 2027 文档的 `0102/0205/0302/0304` 等旧命令不当作本版有效接收数据；本文档未给出独立键鼠串口报文的命令字和布局，不能据此推定键鼠链路取消。

现有 USART10 常规链路保持原接线和初始化。图传和雷达的实际端口未强制指定，需要在选定硬件后接入相应回调。雷达接口只负责已解调数据的解析，不实现射频接收或物理层解调。

图传接入示例（`referee` 替换为实际使用的裁判系统对象）：

```cpp
static void Referee_Video_Rx(uint8_t *data, uint16_t length)
{
    referee.Video_UART_RxCpltCallback(data, length);
}

void Bind_Referee_Video(UART_HandleTypeDef *video_uart)
{
    // 先在所选 UART 的硬件配置中设置 921600、8N1。
    UART_Init(video_uart, Referee_Video_Rx, UART_BUFFER_SIZE);
    referee.Init_Video_UART(video_uart);
}
```

图传 UART 应独立于电源管理模块串口。发送 `0310` 前必须调用 `Init_Video_UART()`；仅调用接收回调不会自动绑定发送端口。端口配置应在业务收发前完成；图传发送或恢复尚未结束时重新绑定会被拒绝，以保护 DMA 正在读取的缓冲。

## 读取状态

原有常用 getter 保留，新增 `Get_*_Data()` 可读取完整的协议结构。例如：

```cpp
const auto &robot = referee.Get_Robot_Status_Data();
const auto &power_heat = referee.Get_Robot_Power_Heat_Data();
const auto &rfid = referee.Get_Robot_RFID_Data();
const auto &sentry = referee.Get_Sentry_Info();
const auto &radar = referee.Get_Radar_Info();
const auto &diagnostics = referee.Get_Diagnostics();
```

重要字段变化：

- `0201` 为 17 字节，新增 `Bullet_Speed_Limit`（偏移 12，单位 m/s）。
- `0202` 仍为 14 字节，但前 8 字节为保留位，不能作为电压、电流、底盘功率。旧的三个测量 getter 返回 0；缓冲能量和两种射击热量仍有效。
- `0204` 冷却增益和攻击增益均为 16 位，避免读取时截断。
- `0206` 为 7 字节，包含命中序号和有符号的命中坐标。`Get_Hit_Position_X/Y()` 的单位为 mm。
- `0003` 为 24 字节，区分 R/B 前哨、己方/对方基地、伤害差和控制度。`Get_HP(id)` 根据本机 ID 判断己方颜色；11/111 始终对应 R/B 前哨。对方机器人血量应从雷达无线数据读取。
- `0209` 为 6 字节，通过 `Get_RFID_Status()`、`Get_RFID_Status_2()` 或位 getter 读取新布局。只在赛内生效的 RFID 位应按赛事状态解释。

`Get_Diagnostics()` 提供有效帧、CRC 错误、长度错误、未知命令及交互计数。在线检测以成功接收的有效协议帧更新，噪声或未知命令不会使常规裁判链路变为在线。

### 小地图命令与机器人交互

`Get_Map_Command()` 返回最新 `0303`。服务器可能对一次地图操作发送多次相同内容，之后持续以 1Hz 重发：

- `Get_Map_Command_Receive_Count()` 每收到一帧有效 `0303` 都增加。
- `Get_Map_Command_Update_Count()` 仅在 12 字节内容变化时增加，包括 `cmd_source` 信息来源变化。

业务层可按更新计数处理新目标。这种内容比较无法区分两次完全相同的主动操作；协议没有独立的操作事件序号。

`Get_Interaction()` 返回最新 `0301` 队内消息，其子内容需为 `0200..02FF`；用 `Get_Interaction_Data_Length()` 取得实际内容长度，最大 112 字节。`Get_Interaction_Receive_Count()` 用于判断收到新包。发送者和接收者必须是 2027 有效、同队机器人；已知本机 ID 时，接收者还需与本机一致。

`Get_Custom_Command()` 返回图传最新 `0311` 的 60 字节内容，`Get_Custom_Command_Receive_Count()` 记录接收次数。自定义内容的字段、单位及消息版本由队伍双方约定。

## 发送接口

| 接口 | 命令与内容 | 发送限制 |
| --- | --- | --- |
| `Send_Interaction(receiver, content_id, data, length)` | `0301`；队内 `0200..02FF` 最大 112 字节，也支持开放的 UI 子内容 | 整个 `0301` 共用至少 34ms 间隔 |
| `Send_Sentry_Command(command)` | `0301 / 0120`，4 字节 | 仅 7/107，接收者固定服务器 `8080` |
| `Send_Radar_Command(command)` | `0301 / 0121`，13 字节 | 仅 9/109，接收者固定服务器 `8080` |
| `Send_Map_Position(positions)` | `0305`，48 字节；坐标单位 cm | 仅雷达，至少 200ms 间隔；保留机器人槽位自动清零 |
| `Send_Map_Path(path)` | `0307`，105 字节；起点和增量单位 dm | 至少 1000ms 间隔；`intention` 为 1/2/3，发送者自动填本机 ID |
| `Send_Map_Robot_Data(receiver, utf16, byte_length)` | `0308`，34 字节 | 至少 334ms 间隔；发送到己方选手端，内容为最多 30 字节 UTF-16LE，长度须为偶数，余位补零 |
| `Send_Custom_Data(data, length)` | `0310`，固定 300 字节数据段 | 图传专用，至少 20ms 间隔；输入 0..300 字节，余位补零 |

所有发送接口在任务上下文调用，返回 `bool`。`true` 表示数据已交给本地发送队列或 DMA，不能据此认定服务器已处理。`false` 可能表示参数/身份不合法、频率限制、队列满或 UART 忙；调用者需保留发送意图并在合适时机重试。失败不会消耗该接口的发送计数或时间槽。

发送前需已收到 `0201` 以取得有效本机 ID。机器人之间仅允许发给合法的己方其他机器人；UI 通常发给本机 ID 加 `0100` 的选手端，雷达可发给任意己方选手端。队伍还应遵守比赛规则允许的多机通信关系和按基地剩余血量变化的接收带宽额度。

USART10 常规发送与原 UI 共用有界、复制数据的 DMA 队列，并由现有任务每 1ms 调用 `Referee_DMA_Service()` 推进。物理发送端按命令字再次限制频率，UI 和其他 `0301` 子内容共享带宽。绘制内容、位置和原有 UI 格式保持不变。

哨兵兑换、复活和雷达决策接口只封装明确给定的指令；不会自动递增请求次数或反复执行业务动作。业务层应根据 `020D/020E` 反馈及协议要求维护单调递增计数。

`0310` 使用独立、持久的 309 字节整帧 DMA 缓冲，不经过原来 128 字节的 UART 发送缓存。图传无重传机制，`0311` 的 60 字节内容不能直接当作第 2 章的 Protobuf 消息解析。自定义终端侧 `CustomControl` 会转为机器人 `0311`，机器人 `0310` 对应终端侧 `CustomByteBlock`。

## 验证

在工程目录运行：

```powershell
python Tools/check_referee_protocol.py
```

该检查分别编译并运行 `CHASSIS`、`GIMBAL` 两种配置下工程中的 `dvc_referee.cpp`，只对 HAL、UART 驱动及性能插桩使用主机桩。覆盖独立 CRC 已知向量、全部 27 个固定长度接收命令、任意切分/粘包、CRC/噪声重新同步、链路隔离、`0303` 重复包、`0301` 最大内容、身份/接收者、队列拒绝、频率和时钟回绕、所有新增发送格式、图传 309 字节整帧、DMA 缓冲所有权及关键 getter。随后自动调用 `Tools/check_referee_transport.py`，验证实际 UI 共用发送队列的容量、DMA 生命周期、频率限制和恢复行为。硬件 UART/DMA、无线解调和实际裁判系统交互仍需上机联调。
