# STM32 与 ESP 通信 README

本文档说明本工程中 STM32F401RCTx 与 ESP 模块之间的 UART 通信方式。工程由 STM32CubeMX 生成，使用 CLion/CMake 编译；ESP 通信代码主要位于 `FIFO/Inc/Esp_Com.h` 和 `FIFO/Src/Esp_Com.c`。

## 1. 工程相关文件

```text
Core/Src/main.c          外设初始化、主循环调度
Core/Src/usart.c         USART1/USART2 初始化
Core/Src/gpio.c          ESP 握手/复位 GPIO 初始化
FIFO/Inc/Esp_Com.h       ESP 通信协议接口和命令定义
FIFO/Src/Esp_Com.c       ESP UART 收发、帧解析、CRC 校验
FIFO/Src/Uart_RTX.c      HAL_UART_RxCpltCallback 分发入口
LetterSh/Src/user_cmd.c  Letter Shell 中的 ESP 调试命令
```

当前主循环在 `flash_rw == 0` 时运行 ESP 通信：

```c
EspCom_Init();

while (1) {
    EspCom_Poll();
    shellTask(&shell);
}
```

如果切换到 `flash_rw == 1`，主循环会走 USART1 OTA/Flash 写入流程，不会轮询 `EspCom_Poll()`。

## 2. 硬件连接

### UART

ESP 与 STM32 使用 `USART2` 通信：

```text
STM32 PA2 / USART2_TX  ->  ESP RX
STM32 PA3 / USART2_RX  <-  ESP TX
GND                    --  GND
3.3V                   --  ESP 供电
```

串口参数来自 `Core/Src/usart.c`：

```text
Baudrate:    2000000
Data bits:   8
Parity:      None
Stop bits:   1
Flow control: None
```

注意：USART1 当前用于 Letter Shell 调试口，默认也是 `2000000 8N1`，引脚为 `PA9/PA10`。

### GPIO 握手与复位

```text
STM32 PB0 / STM32_Ready  ->  ESP 输入，STM32 就绪信号
STM32 PB1 / ESP_Ready    <-  ESP 输出，ESP 就绪信号
STM32 PA8 / ESP_RST      ->  ESP 复位控制
```

当前代码行为：

```text
EspCom_Init()       将 STM32_Ready 置高，并启动 USART2 单字节中断接收
EspCom_SetReady()   控制 PB0
EspCom_IsEspReady() 读取 PB1
main.c              初始化后将 ESP_RST 置高
```

建议 ESP 侧在 WiFi/MQTT/API 缓存可用后拉高 `ESP_Ready`，STM32 侧发送业务命令前可通过 `EspCom_IsEspReady()` 判断状态。

## 3. UART 帧格式

STM32 与 ESP 双向都使用同一种二进制帧：

```text
AA 55 VER TYPE SEQ LEN_H LEN_L PAYLOAD CRC_H CRC_L
```

字段说明：

```text
AA 55     固定帧头
VER       协议版本，当前为 0x01
TYPE      命令或响应类型
SEQ       帧序号，STM32 每发送一帧自动递增
LEN_H     Payload 长度高字节
LEN_L     Payload 长度低字节
PAYLOAD   JSON 字节流，可为空
CRC_H     CRC16 高字节
CRC_L     CRC16 低字节
```

长度字段使用大端序：

```c
len = (LEN_H << 8) | LEN_L;
```

当前 STM32 端最大 Payload 长度：

```text
ESP_COM_MAX_PAYLOAD = 384 bytes
```

## 4. CRC16

协议使用 CRC16-CCITT：

```text
Polynomial: 0x1021
Initial:    0xFFFF
Output:     Big endian
```

CRC 计算范围不包含帧头 `AA 55`，也不包含 CRC 字段本身，只覆盖：

```text
VER TYPE SEQ LEN_H LEN_L PAYLOAD
```

C 参考实现与工程一致：

```c
static uint16_t crc16_ccitt(const uint8_t *data, uint16_t length, uint16_t seed)
{
    uint16_t crc = seed;

    for (uint16_t i = 0; i < length; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (uint8_t bit = 0; bit < 8; bit++) {
            if ((crc & 0x8000U) != 0U) {
                crc = (uint16_t)((crc << 1) ^ 0x1021U);
            } else {
                crc <<= 1;
            }
        }
    }

    return crc;
}
```

## 5. 命令与响应类型

### STM32 -> ESP

定义在 `FIFO/Inc/Esp_Com.h`：

```text
0x01  ESP_COM_CMD_PING
0x02  ESP_COM_CMD_GET_STATUS
0x03  ESP_COM_CMD_GET_CACHE
0x04  ESP_COM_CMD_REFRESH_API
0x05  ESP_COM_CMD_READ_API
```

### ESP -> STM32

```text
0x81  ESP_COM_RSP_PONG
0x82  ESP_COM_RSP_STATUS
0x83  ESP_COM_RSP_CACHE
0x84  ESP_COM_RSP_ACK
0x85  ESP_COM_RSP_ERROR
```

建议 ESP 响应时沿用请求帧的 `SEQ`，便于 STM32 后续扩展请求/响应匹配。

## 6. Payload 约定

当前 STM32 发送给 ESP 的业务 Payload 使用 JSON 文本。

### 请求帧速查

STM32 请求 ESP 时，实际发送的内容就是一帧完整 UART 数据：

```text
AA 55 01 TYPE SEQ LEN_H LEN_L PAYLOAD CRC_H CRC_L
```

其中：

```text
VER 固定为 0x01
SEQ 由 EspCom_Send() 每次发送前自动加 1
LEN 是 payload 字节数，不是字符串长度字段
CRC 按 VER TYPE SEQ LEN_H LEN_L PAYLOAD 计算
```

下面示例均假设 `SEQ = 0x01`。实际运行时第二帧会变成 `SEQ = 0x02`，CRC 也会随之变化。

| 用途 | TYPE | Payload 文本 | LEN |
| --- | --- | --- | --- |
| 测试链路 | `0x01` | 空 | `0` |
| 读取 ESP 状态 | `0x02` | 空 | `0` |
| 读取本地缓存 | `0x03` | `{"api":"current"}` | `17` |
| 请求刷新 API | `0x04` | `{"api":"current"}` | `17` |
| 请求读取 API | `0x05` | `{"api":"daily7d"}` | `17` |

完整示例帧：

```text
PING
AA 55 01 01 01 00 00 FA D9

GET_STATUS
AA 55 01 02 01 00 00 61 05

GET_CACHE current
AA 55 01 03 01 00 11 7B 22 61 70 69 22 3A 22 63 75 72 72 65 6E 74 22 7D 6E CE

REFRESH_API current
AA 55 01 04 01 00 11 7B 22 61 70 69 22 3A 22 63 75 72 72 65 6E 74 22 7D 67 4E

READ_API daily7d
AA 55 01 05 01 00 11 7B 22 61 70 69 22 3A 22 64 61 69 6C 79 37 64 22 7D 9D E7
```

如果要换 API 名，只需要换 Payload 中的字符串并重新计算 `LEN` 和 `CRC`。例如 `EspCom_GetCache("status")` 会发送：

```text
TYPE = 0x03
PAYLOAD = {"api":"status"}
```

工程里的三个带 API 参数的函数都会按同样格式组 JSON：

```c
EspCom_GetCache(api);    // TYPE = 0x03, payload = {"api":"xxx"}
EspCom_RefreshApi(api);  // TYPE = 0x04, payload = {"api":"xxx"}
EspCom_ReadApi(api);     // TYPE = 0x05, payload = {"api":"xxx"}
```

### PING

STM32 发送：

```text
TYPE = 0x01
PAYLOAD = empty
```

ESP 建议响应：

```text
TYPE = 0x81
PAYLOAD = {"ok":true,"uptime_ms":123456}
```

### GET_STATUS

STM32 发送：

```text
TYPE = 0x02
PAYLOAD = empty
```

ESP 建议响应：

```json
{
  "ok": true,
  "wifi": true,
  "mqtt": true,
  "esp_ready": true,
  "uptime_ms": 123456
}
```

### GET_CACHE

读取 ESP 已缓存的数据，不要求 ESP 主动访问后端。

STM32 发送：

```json
{"api":"current"}
```

ESP 响应：

```text
TYPE = 0x83
```

示例：

```json
{
  "api": "current",
  "valid": true,
  "temp": "31",
  "humidity": "66",
  "text": "cloudy",
  "updated_at": "2026-06-19T11:28:00+08:00"
}
```

### REFRESH_API

要求 ESP 刷新指定 API，例如通过 MQTT/HTTP 请求后端更新数据。

STM32 发送：

```json
{"api":"current"}
```

ESP 建议先响应 ACK：

```json
{"ok":true,"api":"current","refresh":true}
```

该 ACK 只表示 ESP 已收到并发起刷新，不代表后端数据已经刷新完成。STM32 可延时后再发送 `GET_CACHE` 读取新缓存。

### READ_API

要求 ESP 读取指定 API 的后端缓存或触发一次读请求，但不强制刷新真实数据源。

STM32 发送：

```json
{"api":"daily7d"}
```

ESP 建议响应：

```json
{"ok":true,"api":"daily7d","refresh":false}
```

## 7. STM32 侧接口

发送接口：

```c
HAL_StatusTypeDef EspCom_Send(uint8_t type, const uint8_t *payload, uint16_t len);
HAL_StatusTypeDef EspCom_Ping(void);
HAL_StatusTypeDef EspCom_GetStatus(void);
HAL_StatusTypeDef EspCom_GetCache(const char *api);
HAL_StatusTypeDef EspCom_RefreshApi(const char *api);
HAL_StatusTypeDef EspCom_ReadApi(const char *api);
```

接收接口：

```c
void EspCom_Init(void);
void EspCom_Poll(void);
bool EspCom_HasFrame(void);
bool EspCom_GetLastFrame(EspCom_Frame *frame);
void EspCom_ClearFrame(void);
void EspCom_UartRxCpltCallback(UART_HandleTypeDef *huart);
```

接收流程：

```text
USART2 收到 1 字节
  -> HAL_UART_RxCpltCallback()
  -> EspCom_UartRxCpltCallback()
  -> 写入 esp_rx_fifo
  -> 主循环 EspCom_Poll()
  -> 状态机查找 AA 55 帧头并解析完整帧
  -> 校验版本号、长度、CRC
  -> 保存到 esp_last_frame，置位 esp_frame_ready
```

应用层读取示例：

```c
EspCom_Frame frame;

EspCom_Poll();
if (EspCom_GetLastFrame(&frame)) {
    // frame.type 是响应类型
    // frame.seq 是帧序号
    // frame.payload 是以 '\0' 结尾的 JSON 字符串
    EspCom_ClearFrame();
}
```

注意：当前只保存最后一帧 `esp_last_frame`。如果 ESP 连续快速返回多帧，应用层需要及时调用 `EspCom_GetLastFrame()` 和 `EspCom_ClearFrame()`，否则后到的帧会覆盖前一帧。

## 8. Shell 调试命令

工程通过 USART1 运行 Letter Shell，可直接测试 ESP 通信。命令定义在 `LetterSh/Src/user_cmd.c`。

```text
esp_ready [0|1]      设置 STM32_Ready，并打印 ESP_Ready 状态
esp_ping             发送 PING
esp_status           发送 GET_STATUS
esp_cache [api]      发送 GET_CACHE，默认 api=current
esp_refresh [api]    发送 REFRESH_API，默认 api=current
esp_read [api]       发送 READ_API，默认 api=daily7d
esp_last             打印最近一次收到的 ESP 帧
```

典型调试流程：

```text
esp_ready 1
esp_status
esp_last
esp_ping
esp_last
esp_cache current
esp_last
esp_refresh current
esp_last
```

`HAL_UART_Transmit()` 返回 `0` 表示 `HAL_OK`。

## 9. ESP 侧实现要求

ESP 侧至少需要实现：

```text
1. 串口初始化为 2000000 8N1
2. 按 AA 55 同步帧头
3. 读取 VER/TYPE/SEQ/LEN/PAYLOAD/CRC
4. 校验 LEN 不超过 384
5. 用 CRC16-CCITT 校验 VER 到 PAYLOAD
6. 根据 TYPE 执行业务逻辑
7. 响应时使用同样帧格式，建议复用请求 SEQ
8. ESP 就绪后拉高 ESP_Ready
```

错误响应建议统一使用 `TYPE = 0x85`，Payload 例如：

```json
{
  "ok": false,
  "code": "crc_error",
  "message": "crc mismatch"
}
```

建议错误码：

```text
frame_too_large
crc_error
bad_json
unknown_cmd
request_failed
payload_overflow
not_ready
```

## 10. 常见问题

### STM32 发出命令但收不到响应

检查：

```text
1. ESP 与 STM32 是否共地
2. PA2/PA3 是否交叉连接到 ESP RX/TX
3. 两端波特率是否都是 2000000
4. ESP 是否误把调试日志打印到了协议串口
5. ESP 响应帧 CRC 是否按 VER 到 PAYLOAD 计算
6. main.c 是否处于 flash_rw == 0 路径
7. 主循环是否持续调用 EspCom_Poll()
```

### `esp_last` 一直显示 `no esp frame`

说明 STM32 端还没有解析出合法完整帧。常见原因是帧头、长度、CRC 或协议版本错误。当前 STM32 只接受：

```text
Header = AA 55
VER    = 0x01
LEN    <= 384
CRC    = CRC16-CCITT(seed=0xFFFF)
```

### Payload 中包含中文

协议层只按字节传输，不限制编码。建议 ESP 侧 JSON 使用 UTF-8；STM32 如只显示到串口终端，需要确保终端也使用 UTF-8。
