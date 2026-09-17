# ESP8266 与 STM32 通信协议说明

本文档描述当前 ESP8266 联网模块暴露给 STM32 主控的接口。ESP8266 负责 WiFi、MQTT、天气数据缓存和天气 API 请求；STM32 通过 UART 请求数据或触发刷新。

## 硬件连接

### UART

```text
Baudrate: 2000000
Data bits: 8
Parity: none
Stop bits: 1
Flow control: none
```

ESP8266 当前使用 `Serial` 与 STM32 通信。ESP 侧调试日志默认关闭，不会向串口输出普通日志。

### GPIO 状态线

```text
IO5: ESP -> STM32
IO4: STM32 -> ESP
```

IO5 由 ESP 输出：

```text
LOW   ESP 未就绪
HIGH  ESP 已连接 WiFi 且 MQTT 已连接，可接受 STM32 命令
```

IO4 由 STM32 输出：

```text
LOW   STM32 未就绪
HIGH  STM32 就绪
```

当前 ESP 只在 `GET_STATUS` 响应里读取 IO4 状态，不会因为 IO4 为 LOW 而阻止发送响应。

## 串口帧格式

所有 STM32 与 ESP 的 UART 数据都使用同一种帧格式。

```text
AA 55 VER TYPE SEQ LEN_H LEN_L PAYLOAD CRC_H CRC_L
```

字段说明：

```text
AA 55      固定帧头
VER        协议版本，当前固定为 0x01
TYPE       命令或响应类型
SEQ        序号，STM32 请求时设置，ESP 响应时原样返回
LEN_H      payload 长度高字节
LEN_L      payload 长度低字节
PAYLOAD    JSON 字节流，可为空
CRC_H      CRC16 高字节
CRC_L      CRC16 低字节
```

长度为大端序：

```text
LEN = (LEN_H << 8) | LEN_L
```

当前 ESP 侧最大 payload 长度：

```text
384 bytes
```

## CRC16

CRC 算法：

```text
CRC16-CCITT
Polynomial: 0x1021
Initial value: 0xFFFF
```

CRC 计算范围不包含 `AA 55`，也不包含 CRC 自身。

计算范围：

```text
VER TYPE SEQ LEN_H LEN_L PAYLOAD
```

CRC 输出为大端序：

```text
CRC_H = crc >> 8
CRC_L = crc & 0xFF
```

参考 C 实现：

```c
uint16_t crc16_ccitt(const uint8_t *data, uint16_t length, uint16_t seed)
{
    uint16_t crc = seed;
    for (uint16_t i = 0; i < length; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (uint8_t bit = 0; bit < 8; bit++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}
```

## 命令类型

STM32 -> ESP：

```text
0x01  PING
0x02  GET_STATUS
0x03  GET_CACHE
0x04  REFRESH_API
0x05  READ_API
```

### PING

用途：测试通信链路。

Payload 可为空。

响应：

```text
TYPE = 0x81
```

示例 payload：

```json
{"ok":true,"uptime_ms":123456}
```

### GET_STATUS

用途：读取 ESP 当前状态。

Payload 可为空。

响应：

```text
TYPE = 0x82
```

示例 payload：

```json
{
  "ok": true,
  "wifi": true,
  "mqtt": true,
  "stm32_ready": true,
  "uptime_ms": 123456
}
```

### GET_CACHE

用途：读取 ESP 已缓存的数据，不触发后端刷新。

请求 payload：

```json
{"api":"current"}
```

响应：

```text
TYPE = 0x83
```

当前桥接层已实现的缓存返回：

```text
current
status
response
daily7d   摘要返回
hourly72h 摘要返回
```

其他 API 数据已经在 ESP 内部缓存，但当前 UART 返回暂未展开，会返回：

```json
{"api":"alert","valid":false,"message":"cache api not implemented in bridge"}
```

### REFRESH_API

用途：请求 ESP 通过 MQTT 通知后端刷新指定 API。

请求 payload：

```json
{"api":"current"}
```

ESP 会向 MQTT `weather/request` 发布：

```json
{"type":"refresh","api":"current"}
```

响应：

```text
TYPE = 0x84
```

示例 payload：

```json
{"ok":true,"api":"current","refresh":true}
```

注意：该响应只表示 ESP 已成功发布刷新请求，不表示后端刷新完成。后端返回后，ESP 会更新内部缓存；STM32 可稍后再发送 `GET_CACHE` 读取。

### READ_API

用途：请求 ESP 通过 MQTT 读取后端缓存，不主动刷新后端 API。

请求 payload：

```json
{"api":"daily7d"}
```

ESP 会向 MQTT `weather/request` 发布：

```json
{"type":"daily7d"}
```

响应：

```text
TYPE = 0x84
```

示例 payload：

```json
{"ok":true,"api":"daily7d","refresh":false}
```

## 响应类型

ESP -> STM32：

```text
0x81  PONG
0x82  STATUS
0x83  CACHE
0x84  ACK
0x85  ERROR
```

错误响应 payload 示例：

```json
{
  "ok": false,
  "code": "crc_error",
  "message": "crc mismatch"
}
```

可能的错误 code：

```text
frame_too_large
crc_error
bad_json
unknown_cmd
request_failed
payload_overflow
```

## API 名称

可用于 `GET_CACHE`、`REFRESH_API`、`READ_API` 的 `api` 字段：

```text
current
minutely5m
alert
airquality
hourly72h
daily7d
indices
sun
moon
status
response
```

其中 `status` 主要用于读取后端状态，`response` 用于读取最近一次主动请求响应缓存。

## GET_CACHE 返回示例

### current

请求：

```json
{"api":"current"}
```

响应 payload：

```json
{
  "api": "current",
  "valid": true,
  "received_at": 123456,
  "temp": "31",
  "feels_like": "33",
  "humidity": "66",
  "text": "多云",
  "icon": "101",
  "wind_dir": "南风",
  "wind_scale": "3",
  "wind_speed": "13",
  "precip": "0.0",
  "pressure": "1003",
  "vis": "30",
  "obs_time": "2026-06-19T11:22+08:00",
  "source_update_time": "2026-06-19T11:28+08:00",
  "server_time": "2026-06-19T03:31:12.426907+00:00"
}
```

### status

请求：

```json
{"api":"status"}
```

响应 payload：

```json
{
  "api": "status",
  "valid": true,
  "ok": true,
  "message": "string",
  "updated_at": "string",
  "api_count": 9
}
```

### response

请求：

```json
{"api":"response"}
```

响应 payload：

```json
{
  "api": "current",
  "valid": true,
  "refreshed": true,
  "id": "req-1",
  "device_id": "esp8266_001",
  "type": "refresh"
}
```

### daily7d

当前返回摘要，最多 3 天：

```json
{
  "api": "daily7d",
  "valid": true,
  "count": 7,
  "items": [
    {
      "date": "2026-06-19",
      "temp_max": "33",
      "temp_min": "26",
      "text": "多云"
    }
  ]
}
```

### hourly72h

当前返回摘要，最多 6 小时：

```json
{
  "api": "hourly72h",
  "valid": true,
  "count": 24,
  "items": [
    {
      "time": "2026-06-19T12:00+08:00",
      "temp": "31",
      "text": "多云"
    }
  ]
}
```

## STM32 端推荐实现

建议 STM32 端实现：

```text
1. UART DMA ring buffer 接收
2. 按 AA 55 帧头同步
3. 读取固定头 VER TYPE SEQ LEN
4. 读取 LEN 字节 payload
5. 读取 CRC 并校验
6. payload 按 JSON 解析
7. 根据 SEQ 匹配请求和响应
```

建议先实现最小闭环：

```text
PING
GET_STATUS
GET_CACHE current
REFRESH_API current
```

等串口帧稳定后，再扩展：

```text
GET_CACHE daily7d
GET_CACHE hourly72h
READ_API / REFRESH_API 其他天气接口
```

## 当前限制

- ESP 当前不会主动推送天气更新事件，通信模式是 STM32 请求、ESP 响应。
- `GET_CACHE` 对大数组接口只返回摘要，避免首版 payload 过大。
- IO4 目前只作为状态读取，不参与硬件流控。
- UART payload 当前使用 JSON，便于调试；后续如果 STM32 资源紧张，可以改成二进制 payload。
