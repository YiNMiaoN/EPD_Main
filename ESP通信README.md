# STM32 与 ESP 通信 README

本文档说明本工程中 STM32F401RCTx 与 ESP 模块之间的 UART 通信方式。工程由 STM32CubeMX 生成，使用 CLion/CMake 编译；ESP 通信代码主要位于 `FIFO/Inc/Esp_Com.h` 和 `FIFO/Src/Esp_Com.c`。

本文于 2026-09-13 根据新版 Net_Node 对接文档同步 NTP 时间约定。本仓库是 TopInfo STM32 工程，ESP 固件属于独立 Net_Node 工程；文中 `lib/Stm32Com/`、`lib/HeFeng/`、`lib/SystemConfig/AppConfig.h`、`src/main.cpp` 和 ESP开发README.md 均指 ESP 侧文件。STM32 当前有 16 个底层公开接口，另有 [ApiRefresh 库](ApiRefresh/README.md)。新增时间请求、解析和完整联调步骤见 [NTP 时间 API](ApiRefresh/TIME_README.md)。本次只接入取时，尚未接入 RTC、时间显示或局部刷新。

当前 ESP 通过 HTTPS 直连和风及一言，不使用 MQTT；已接入实时天气 `current`、三日预报 `daily3d`、分钟降雨 `minutely5m`、天气预警 `alert` 和一言 `hitokoto`。分钟降雨使用 `lib/SystemConfig/AppConfig.h` 中的 `QWEATHER_MINUTELY_LOCATION`（经度,纬度），当前已配置为 `117.65,24.50`。调试与通信共用 UART0，通过 `DEBUG_LOG_ENABLED` 开关控制，当前关闭。

阅读导航：

- 第 1～2 节：工程接入、串口和 GPIO。
- 第 3～5 节：帧格式、CRC、命令与响应类型。
- 第 6 节：请求数据、API 名称、响应字段。
- 第 7 节：函数参数、返回值、状态变化和使用条件。
- 第 8～10 节：Shell 联调、ESP 侧约定和排查方法。
- 第 11～12 节：调用示例、异步业务流程和实现边界。

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

当前主循环在 `flash_rw == 0` 时运行 ESP 通信、请求状态机与一言显示处理。以下省略外设和 TIM10 启动，完整初始化见 ApiRefresh README：

```c
EspCom_Init();
ApiRefresh_Init();

while (1) {
    EspCom_Poll();
    ApiRefresh_Poll();
    EPD_UI_Poll();
    shellTask(&shell);
}
```

如果切换到 `flash_rw == 1`，主循环会走 USART1 OTA/Flash 写入流程，不会轮询 `EspCom_Poll()`。

注意：`EspCom_Init()` 在两个分支之前执行，因此资源写入模式下 USART2 中断仍已开启，STM32 就绪引脚仍为高，但收到的字节不会被主循环解析。ESP 的接收 FIFO 是模块内部独立的 `esp_rx_fifo`，不使用 USART1 资源写入所用的 `Fifo_Uart_Rx`。

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

当前 PA2/PA3 配置为 `GPIO_MODE_AF_PP`、`GPIO_NOPULL`、`GPIO_AF7_USART2`。USART2 使用单字节中断接收和阻塞发送，没有使用 DMA，也没有 RTS/CTS 硬件流控。不要将 USART1 的 DMA 接收流程当作 ESP 接收流程。

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

当前 ESP 上电将 `ESP_Ready` 拉低，Wi-Fi 已连接且没有等待或执行中的刷新时拉高；这不保证指定 API 已有缓存。STM32 侧发送业务命令前应通过 `EspCom_IsEspReady()` 判断状态。

PB0 对接 ESP GPIO4（NodeMCU D2），PB1 对接 GPIO5（D1）；IO4 仅供 ESP 读取状态，不阻止响应。STM32 发送函数不会自动检查 PB1。PA8 对接 ESP RST，低电平复位、高电平释放；`ESP_RST` 不属于 `EspCom_*` API，由 STM32 `main.c` 拉高释放，模块初始化不会复位 ESP 或等待其启动。

UART0 的 GPIO3/RX 接 STM32 PA2，GPIO1/TX 接 PA3，使用 3.3V 电平并共地。日志也使用 UART0、2000000 波特率，不使用 UART1；`DEBUG_LOG_ENABLED=false` 关闭应用日志和 UART 调试输出，协议帧仍正常收发。ROM 上电日志不受该开关控制，接收端仍需按帧头同步。联调时避免板载 USB 转串口 TX 与 STM32 PA2 同时驱动 ESP RX。

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

空载荷帧为 9 字节，最大帧为 `9 + 384 = 393` 字节。`LEN` 按实际字节计数，UTF-8 中文通常占多个字节；JSON 文本末尾的 C 字符串 `\0` 不发送。传输层按原始字节处理载荷，并不解析 JSON。

初始化后首个进入发送封装的合法长度请求使用 `SEQ = 1`，随后按 8 位数递增，`255` 后回绕到 `0`。序号在调用 HAL 发送之前增加，因此 HAL 发送失败也会消耗一个序号；参数长度在进入发送封装前被拒绝时不会增加。当前接口不能指定或查询最近发送序号。

## 4. CRC16

协议使用 CRC16-CCITT：

```text
Polynomial: 0x1021
Initial:    0xFFFF
RefIn:      false
RefOut:     false
XorOut:     0x0000
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
LEN 是 payload 的实际字节数，不包括字符串结尾的 NUL
CRC 按 VER TYPE SEQ LEN_H LEN_L PAYLOAD 计算
```

下面示例均假设 `SEQ = 0x01`。实际运行时第二帧会变成 `SEQ = 0x02`，CRC 也会随之变化。

| 用途 | TYPE | Payload 文本 | LEN |
| --- | --- | --- | --- |
| 测试链路 | `0x01` | 空 | `0` |
| 读取 ESP 状态 | `0x02` | 空 | `0` |
| 读取本地缓存 | `0x03` | `{"api":"current"}` | `17` |
| 请求刷新 API | `0x04` | `{"api":"current"}` | `17` |
| 请求读取 API | `0x05` | `{"api":"daily3d"}` | `17` |

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

READ_API daily3d
AA 55 01 05 01 00 11 7B 22 61 70 69 22 3A 22 64 61 69 6C 79 33 64 22 7D 57 16
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
  "mqtt": false,
  "stm32_ready": true,
  "esp_ready": true,
  "busy": false,
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
  "source_update_time": "2026-09-10T21:50+08:00"
}
```

### REFRESH_API

要求 ESP 通过 HTTPS 刷新指定 API。当前支持 `current`、`daily3d`、`alert`、`hitokoto` 和已配置经纬度的 `minutely5m`，其他 API 尚未接入网络刷新。

STM32 发送：

```json
{"api":"current"}
```

ESP 建议先响应 ACK：

```json
{"ok":true,"api":"current","refresh":true}
```

该 ACK 只表示 ESP 已将刷新排队，不代表数据已更新。随后 ESP 拉低 IO5、发完 ACK 并同步执行 HTTP 请求；请求完成后 IO5 根据联网状态恢复。STM32 应等待 IO5 恢复 HIGH，再发送 `GET_CACHE response` 检查结果及对应 API 的 `GET_CACHE` 读取缓存。刷新期间 UART 接收缓冲为 2048 字节，命令处理会延后，不要连续灌入命令。刷新失败时保留上次成功缓存。

离线或忙碌返回 `ERROR/not_ready`；未实现的 API、分钟降雨经纬度未配置或排队失败返回 `ERROR/request_failed`。启动时缓存为空，不自动请求网络 API。

这里的延时应由主循环计时调度实现，等待期间仍要持续调用 `EspCom_Poll()`；不要用长时间 `HAL_Delay()` 停止接收处理。

### READ_API

当前直连模式下，天气和一言的 READ_API 仅检查 ESP 本地缓存，不调用 MQTT 或 HTTP。有缓存时返回 `ACK`、`refresh:false`，随后用 GET_CACHE 获取数据；无缓存时返回 `ERROR/request_failed`。下面 daily3d 的 ACK 示例仅在已成功刷新并填入缓存时成立。`READ_API time` 为实时 NTP 查询，成功时直接在 ACK 中返回时间，不使用缓存，详见下文。

STM32 发送：

```json
{"api":"daily3d"}
```

ESP 建议响应：

```json
{"ok":true,"api":"daily3d","refresh":false}
```

### API 名称与缓存支持范围

以下为 ESP 协议文档记录的业务名称。STM32 的三个 `api` 参数封装只负责生成 JSON，不做名称白名单校验，也不保证 ESP 支持对应操作。

| `api` | 用途 | 协议文档中的 `GET_CACHE` 支持情况 |
| --- | --- | --- |
| `current` | 实时天气 | 已展开实时天气字段 |
| `daily3d` | 三日天气预报 | 已接入 weather/v1/daily，返回三天的日期、天气描述、最高/最低温度 |
| `hourly72h` | 逐小时预报 | 已实现最多 6 小时摘要转换，未接入网络数据源 |
| `minutely5m` | 分钟级降水 | 已接入 `/v7/minutely/5m`，返回描述及单帧摘要；需配置经纬度 |
| `alert` | 天气预警 | 已接入 `/v7/warning/now`，返回总数和预警摘要；空数组表示无预警 |
| `hitokoto` | 一言 | 已接入 `v1.hitokoto.cn`，返回句子、出处和作者 |
| `time` | 实时网络时间 | 不使用缓存；仅支持 `READ_API time`，直接返回本次 NTP 取时结果 |
| `airquality` | 空气质量 | UART 缓存返回暂未展开 |
| `indices` | 生活指数 | UART 缓存返回暂未展开 |
| `sun` | 日出日落 | UART 缓存返回暂未展开 |
| `moon` | 月亮信息 | UART 缓存返回暂未展开 |
| `status` | 状态缓存 | 已实现平铺缓存写入/读取，当前无数据源，不同于命令 `GET_STATUS` |
| `response` | 最近一次主动请求的响应缓存 | 已支持 |

缓存尚未展开时，ESP 文档给出的响应示例为：

```json
{"api":"airquality","valid":false,"message":"cache api not implemented in bridge"}
```

`GET_STATUS` 查询 ESP 自身联网等状态；`EspCom_GetCache("status")` 查询独立缓存，当前未填入数据，返回 `valid:false`、`cache not available`。已实现但尚未填入的其他缓存也返回此消息；未知 API 返回 `unknown cache api`。`status`、`response` 不支持 REFRESH_API；READ_API 仅在其本地缓存存在时成功。HAL_OK 始终只代表 STM32 本地发送成功。

### 实时网络时间 time

STM32 使用 `EspCom_ReadApi("time")` 或 Shell 命令 `esp_read time`，
发送 TYPE=0x05、Payload=`{"api":"time"}`。每次请求都调用
`NTPClient::forceUpdate()` 向服务器取时，不使用自动更新间隔或上次同步值。
不需要 `esp_refresh time` 或 `esp_cache time`；这两个命令均返回
`ERROR/request_failed`，提示使用 READ_API。

请求受理后 IO5 拉低，NTP 完成后恢复联网就绪状态。成功时只发送一帧
`ACK (0x84)`，SEQ 与请求一致，时间在 ACK 顶层字段中；没有提前发送的排队 ACK，
也不写入 `time` 或 `response` 缓存，不再发送 CACHE 帧。

```json
{"ok":true,"api":"time","refresh":false,"realtime":true,"unix_time":1799798400,"utc_offset":28800,"date":"2027-01-13","time":"08:00:00","weekday":3}
```

| 字段 | 含义 |
| --- | --- |
| `unix_time` | UTC Unix 秒时间戳，未叠加时区偏移 |
| `utc_offset` | 本地时区相对 UTC 的偏移秒数，默认 28800，即 UTC+8 |
| `date` | 本地日期，`YYYY-MM-DD` |
| `time` | 本地时间，`HH:MM:SS`，24 小时制 |
| `weekday` | 本地星期，0=星期日，1=星期一，…，6=星期六 |
| `realtime` / `refresh` | true / false，表示直接取时，不是天气缓存刷新 |

配置在 `lib/SystemConfig/AppConfig.h`：`NTP_SERVER=ntp.aliyun.com`，
`NTP_UTC_OFFSET_SECONDS=28800`。修改后重新编译烧录，使用 UDP 123 端口。
ESP 不在启动时或后台自动校时；STM32 收到时间后可设置自身 RTC，日常走时由
STM32 维护，需重新校时再发送命令。

离线或忙碌返回 `ERROR/not_ready`。NTP 超时、无效时间或取时失败返回
`ERROR/request_failed`、`message="NTP time request failed"`，不携带旧时间。
NTPClient 收包等待约 1 秒，DNS 查询和发送还会增加耗时。STM32 需保持
`EspCom_Poll()` 运行并等待 ACK/ERROR，不能把本地 TX 成功或 IO5 恢复当作取时成功。
该请求同步执行，期间串口命令处理会延后，不要连续发送命令。

联调：先确认就绪，执行 `esp_read time`，等待新响应后执行 `esp_last`，
检查 TYPE=0x84、api=time、ok=true，并从同一帧读取日期时间。

STM32 已提供跟踪请求的入口 `api_time` / `ApiRefresh_StartTime()`，随后用 `api_result` 查看解析结果；此方式自动匹配 SEQ，等待 Ready 最多 15 秒、等待时间 ACK 最多 10 秒，失败保留原始响应，不自动重试。`has_time=true` 且 state=succeeded 才能读取结果结构。原 `esp_read time` 仍是原始调试命令，不填入库结果。时间获取不更新 MainUI，详见 [TIME_README](ApiRefresh/TIME_README.md)。

### 一言缓存 hitokoto

请求为 `https://v1.hitokoto.cn/?c=a&c=c&max_length=23`。
独立模块 `lib/Hitokoto` 使用 `lib/SystemConfig/AppConfig.h` 中的
`HITOKOTO_BASE_URL=https://v1.hitokoto.cn/` 和
`HITOKOTO_QUERY=c=a&c=c&max_length=23` 拼接地址。`c=a` 为动画，`c=c` 为游戏，
`max_length=23` 是服务端句子长度筛选条件；一言无需 token，不携带和风 API Key。
修改配置后重新编译烧录。

GET_CACHE hitokoto 返回以下顶层字段：

| 字段 | 类型 | 含义 |
| --- | --- | --- |
| `api` / `valid` | string / bool | `hitokoto` / 缓存是否有效 |
| `hitokoto` | string | 完整句子，必须非空 |
| `from` | string | 作品或出处 |
| `from_who` | string 或 null | 作者或角色；服务端为 null 或缺失时返回 null |

```json
{"api":"hitokoto","valid":true,"hitokoto":"惊涛入海觅螭虎，风雪归山斩妖邪。","from":"原神","from_who":"铜雀"}
```

不传输 `id`、`uuid`、`type`、`creator`、`length` 等显示不需要的元数据。
该接口请求 `Accept-Encoding: identity`，支持 Content-Length 和 chunked 响应，
响应正文限制 4096 字节，不要求和风的 `code=200` 字段。HTTP 非 200、读取或
JSON 解析失败、字段类型错误、空句子，或最终 JSON 超过 384 个 UTF-8 字节时，
刷新失败并保留旧缓存，`response` 记录 `api:hitokoto`、`ok:false`。
不截断句子或出处；`max_length` 不能代替串口字节长度检查。

STM32 联调顺序：

1. `esp_refresh hitokoto`，等待 ACK；它只表示刷新已排队。
2. 等待 IO5 恢复 HIGH，再执行 `esp_cache response`，确认 `api=hitokoto`、`ok=true`。
3. 执行 `esp_cache hitokoto` 获取句子；每条命令等待响应后用 `esp_last` 查看。

`esp_read hitokoto` 只检查本地缓存，返回 ACK 后仍需 GET_CACHE 取数据，不换新句子。
首次刷新前 GET_CACHE 返回 `valid:false`、`cache not available`，READ_API 返回
`ERROR/request_failed`。刷新完成不会主动推送 CACHE 帧。

### 三日预报缓存 daily3d

三日预报已替代七日预报入口。旧名称 `daily7d` 不再注册：GET_CACHE 返回
`unknown cache api`，REFRESH_API/READ_API 返回 `request_failed`。STM32 端应改为
显式使用 `daily3d`。当前 STM32 Shell 的 `esp_read` 无参默认 daily3d，
底层 `EspCom_ReadApi(NULL)` 默认 current，调用时间接口请显式传 time。

请求为 `{QWEATHER_BASE_URL}/weather/v1/daily/24.62/118.06?key={QWEATHER_API_KEY}&days=3&localTime=true`。
以下配置均位于 `lib/SystemConfig/AppConfig.h`：

| 配置 | 当前值 |
| --- | --- |
| `QWEATHER_DAILY_PATH` | `/weather/v1/daily` |
| `QWEATHER_DAILY_LATITUDE` | `24.62`，纬度在前 |
| `QWEATHER_DAILY_LONGITUDE` | `118.06`，经度在后 |
| `QWEATHER_DAILY_QUERY` | `days=3&localTime=true` |

这里使用用户提供的预报坐标，与实时天气城市 ID、分钟降雨坐标独立。
不额外拼接 location 参数。新接口没有 v7 的 code=200 字段，成功以 HTTP 200
和 days 内容校验为准；旧 v7 接口继续校验业务 code。响应支持 gzip 或普通 JSON，
目前仍要求服务端给出有效 Content-Length，不支持未知长度响应。

每一天只保留以下字段，首项为服务端返回的当地首日预报：

| UART 字段 | 原始字段 | 类型 |
| --- | --- | --- |
| `date` | `days[].forecastStartTime` 的日期部分 | `YYYY-MM-DD` 字符串 |
| `text` | `days[].daytime.condition.text` | 白天天气描述字符串 |
| `temp_max` | `days[].temperatureMax.value` | JSON 数值，摄氏度 |
| `temp_min` | `days[].temperatureMin.value` | JSON 数值，摄氏度 |

最高/最低温度取整天的顶层值，不取 daytime/nighttime 内的温度，不取平均值。
保留小数，不输出天文、风速、湿度、降水等字段。与 v7 实时天气不同，此处温度
是数值而非字符串，STM32 端需按数值读取。示例完整三天均在 384 字节以内：

```json
{"api":"daily3d","valid":true,"count":3,"items":[{"date":"2026-09-12","temp_max":29.81,"temp_min":26.03,"text":"阴"},{"date":"2026-09-13","temp_max":30.87,"temp_min":25.16,"text":"小雨"},{"date":"2026-09-14","temp_max":32.45,"temp_min":24.15,"text":"小雨"}]}
```

允许服务端返回 1～3 天，以 count 和 items 实际长度为准。缺字段、错误类型、
空数组、非摄氏度或最高温度低于最低温度时更新失败。三日缓存不丢弃某一天来
规避长度限制；全部精简数据仍超过 384 字节时保留上次缓存，并记录刷新失败。

联调：`esp_refresh daily3d`，等待 ACK 和 IO5 恢复 HIGH，随后依次执行
`esp_cache response`、`esp_cache daily3d`，每次等待响应后用 `esp_last` 查看。
`esp_read daily3d` 只确认本地缓存存在，不重新请求天气。

### 分钟降雨缓存 minutely5m

和风路径在 `QWEATHER_MINUTELY_PATH` 中配置，当前为 `/v7/minutely/5m`；地址前缀和密钥复用 `QWEATHER_BASE_URL`、`QWEATHER_API_KEY`。`QWEATHER_MINUTELY_LOCATION` 必须填写实际的 `经度,纬度`，不能使用实时天气的城市 ID；修改配置后需重新编译烧录。

ESP 将和风 `summary` 保留在顶层，`updateTime` 映射为 `source_update_time`，`minutely` 转换为 `items`：

| UART 字段 | 来源与含义 |
| --- | --- |
| `api` / `valid` | `minutely5m` / 缓存是否有效 |
| `summary` | 未来两小时降雨描述，完整保留 |
| `source_update_time` | 和风更新时间 |
| `count` | 完整 minutely 数组的条数 |
| `items[].time` | 和风 `fxTime`，完整时间字符串 |
| `items[].precip` | 和风降水量字符串，保留原始精度 |
| `items[].type` | 和风降水类型，例如 `rain` |

按源数组顺序保留 384 字节内能放下的前几项。提供的 24 条“未来两小时无降水”样例返回前 3 项；`count:24` 不表示本帧包含 24 项。当前无分页或分包，无法通过重复 GET_CACHE 获取其余点。

```json
{"api":"minutely5m","valid":true,"summary":"未来两小时无降水","source_update_time":"2026-09-10T21:50+08:00","count":24,"items":[{"time":"2026-09-10T21:50+08:00","precip":"0.00","type":"rain"},{"time":"2026-09-10T21:55+08:00","precip":"0.00","type":"rain"},{"time":"2026-09-10T22:00+08:00","precip":"0.00","type":"rain"}]}
```

空数组、缺少必需字段、任意条目的字段类型错误，或无法放入描述和至少一项数据时，拒绝更新并保留旧缓存。`fxLink` 和 `refer` 不通过 UART 发送。

### 天气预警缓存 alert

`HeFeng::fetchAlert()` 通过 `QWEATHER_ALERT_PATH`（`/v7/warning/now`）请求预警，
复用 `QWEATHER_BASE_URL`、`QWEATHER_API_KEY` 和 `QWEATHER_LOCATION`（城市 ID）。
沿用 HTTPS/gzip/JSON 处理和业务 code=200 校验，不使用分钟降雨的经纬度配置。

GET_CACHE alert 返回 `api`、`valid`、`source_update_time`、`count`、`items`。
`count` 是和风 warning 数组总数，`items` 是按源顺序装入 384 字节的前几项。
非空预警摘要包含以下字段（可选字段缺失时不输出）：

| UART 字段 | 和风字段 | 要求 |
| --- | --- | --- |
| `id` | `id` | 字符串，必需 |
| `title` | `title` | 字符串，必需 |
| `type` | `type` | 可选字符串 |
| `type_name` | `typeName` | 可选字符串 |
| `severity` | `severity` | 可选字符串 |
| `severity_color` | `severityColor` | 可选字符串 |
| `status` | `status` | 可选字符串，保留上游状态 |
| `pub_time` | `pubTime` | 可选字符串 |

预警正文 `text`、`fxLink` 和 `refer` 不通过 UART 传输。不排序或筛选预警，
也不截断标题；单帧放不下的剩余条目不会发送，当前没有分页。
若连第一条摘要都放不下，则本次缓存更新失败并保留旧数据。

你提供的 `warning:[]` 是成功的“无预警”结果，会清除旧预警，返回：

```json
{"api":"alert","valid":true,"source_update_time":"2026-09-12T18:18+08:00","count":0,"items":[]}
```

这与 `valid:false`（尚无缓存）不同。warning 缺失、为 null、不是数组或条目
格式错误均不按无预警处理；请求失败或更新超限时保留旧缓存，并在 response 中
记录 `api:alert`、`ok:false`。不要仅凭旧缓存 `valid:true` 判断本次查询成功。

联调：`esp_refresh alert` 后读取 ACK，等待 IO5 恢复 HIGH，再依次运行
`esp_cache response` 和 `esp_cache alert`，每条命令等待响应后用 `esp_last` 查看。
`esp_read alert` 只检查本地缓存，即使缓存中 count=0 也会成功 ACK，不重新访问和风。

### 刷新结果缓存 response

每次已受理的刷新结束后写入结果，GET_CACHE response 返回顶层字段。`api` 为原始业务名称，`ok` 表示本次刷新及缓存写入是否成功，`valid` 仅表示这条结果记录可读。刷新完成不会主动推送 CACHE 帧。

```json
{"api":"minutely5m","valid":true,"ok":true,"uptime_ms":123456,"message":"refresh completed"}
```

失败时 `ok:false`，`message` 为 `refresh failed; previous cache retained`。请求在排队前被拒绝时返回 ERROR，不覆盖 response 记录；启动后尚无已完成刷新时，response 缓存无效。

### 响应载荷的读取规则

| 响应类型 | 应用层重点检查 |
| --- | --- |
| `PONG (0x81)` | 链路响应，可含 `ok`、`uptime_ms` |
| `STATUS (0x82)` | `ok`、`wifi`、`mqtt`、`stm32_ready`、`uptime_ms` |
| `CACHE (0x83)` | `valid`、`api` 及业务字段；无效缓存不能用于更新界面 |
| `ACK (0x84)` | `ok`、`api`、`refresh`；天气刷新仅确认受理；`time` 的 ACK 直接携带成功获取的时间 |
| `ERROR (0x85)` | `code`、`message`；这是 ESP 业务/协议错误，不是 HAL 返回码 |

`STATUS` 示例中的 `stm32_ready` 是 ESP 读取 STM32 状态线的结果；STM32 读取 ESP 就绪线要调用 `EspCom_IsEspReady()`，不要将二者混淆。

实时天气顶层字段可包含 `temp`、`feels_like`、`humidity`、`text`、`icon`、`wind_dir`、`wind_scale`、`wind_speed`、`precip`、`pressure`、`vis`、`obs_time`、`source_update_time`，不使用 `data.now` 包装；当前不产生 `server_time`、`received_at`。温湿度等很多数值是 JSON 字符串，应用层转换时要检查类型、缺失字段和转换结果。`icon` 可供后续调用 `QWeatherIcon_Draw()`，但当前通信模块不会自动绘制。

若实时天气超过 384 字节，依次省略 `vis`、`pressure`、`precip`、`wind_speed`、`wind_scale`、`wind_dir`、`feels_like`；仍超限则拒绝更新，保留旧缓存。各类预报摘要也按实际 UTF-8 字节数限制，不截断字段或输出不完整 JSON。

以下是用于解释字段的简化缓存示例，不要求 ESP 固定返回这组字段：

```json
{"api":"current","valid":true,"temp":"31","humidity":"66","text":"多云","icon":"101"}
```

`daily3d.items` 的文档字段为 `date`、`temp_max`、`temp_min`、`text`；`hourly72h.items` 为 `time`、`temp`、`text`。`count` 可能描述完整缓存数量，不等于本帧 `items` 数组长度，遍历必须以实际数组长度为准。查询 `response` 缓存时，返回 `api` 可能是原始业务名称（如 `current`），不一定是 `response`。

所有 JSON 示例都是字段说明，实际序列化后仍必须满足 384 字节上限。当前没有分包重组；不要将完整多日数组直接塞入一帧。

## 7. STM32 侧接口详解

### 7.1 头文件与调用约定

```c
#include "Esp_Com.h"
```

头文件自带 `extern "C"` 保护，C 和 C++ 均可直接包含。模块固定操作全局 `huart2` 和工程 GPIO，不支持传入其他 UART 实例。发送、轮询和取帧 API 按当前裸机主循环使用；没有互斥锁或多任务重入保护，不应从多个任务/中断同时调用。中断中只运行第 7.11 节的接收回调。

| 接口 | 主要作用 |
| --- | --- |
| `EspCom_Init` | 初始化协议状态，拉高 STM32 就绪线并启动接收 |
| `EspCom_Poll` | 从接收 FIFO 取字节并解析帧 |
| `EspCom_SetReady` | 设置 STM32 就绪输出 |
| `EspCom_IsEspReady` | 读取 ESP 就绪输入 |
| `EspCom_Send` | 按指定类型封装并发送载荷 |
| `EspCom_Ping` | 发送链路探测 |
| `EspCom_GetStatus` | 请求 ESP 状态 |
| `EspCom_GetCache` | 请求 ESP 本地缓存 |
| `EspCom_RefreshApi` | 请求 ESP 通过 HTTPS 刷新指定 API |
| `EspCom_ReadApi` | 天气和一言检查本地缓存；`time` 实时请求 NTP 并直接返回时间 |
| `EspCom_HasFrame` | 查询是否有已解析且未清除的帧 |
| `EspCom_GetLastFrame` | 将最近一帧复制给调用方 |
| `EspCom_ClearFrame` | 清除帧可用标志 |
| `EspCom_UartRxCpltCallback` | USART2 接收完成回调入口 |
| `EspCom_SetFrameObserver` | 注册唯一合法帧观察回调，当前由 ApiRefresh 使用 |
| `EspCom_GetTxSequence` | 获取最近一次分配的发送序号，供请求匹配使用 |

### 7.2 接收数据结构 `EspCom_Frame`

```c
typedef struct {
    uint8_t ver;
    uint8_t type;
    uint8_t seq;
    uint16_t len;
    uint8_t payload[ESP_COM_MAX_PAYLOAD + 1];
    uint16_t crc;
} EspCom_Frame;
```

| 字段 | 含义 |
| --- | --- |
| `ver` | 已通过版本检查，当前为 `0x01` |
| `type` | 对端发送的类型，调用方需要自行判断 |
| `seq` | 对端发送的序号，协议层不自动匹配请求 |
| `len` | 有效载荷字节数，范围 `0..384` |
| `payload` | 容量 385 字节，有效数据是前 `len` 字节 |
| `crc` | 收到并校验通过的 CRC，已组装为本机 `uint16_t` |

合法帧被接收时会补 `payload[len] = '\0'`，方便读取 JSON 文本。这不表示接收端已经检查了 JSON 语法；二进制载荷或包含 NUL 的数据必须按 `len` 处理，不能用 `strlen()` 或 `%s` 判断完整内容。空载荷时 `payload[0]` 为 NUL。

这是内存数据结构，不是打包后的线上帧，可能包含编译器对齐填充。不要直接发送或接收 `sizeof(EspCom_Frame)` 字节代替协议封装。

### 7.3 初始化 `EspCom_Init`

```c
void EspCom_Init(void);
```

无参数，无返回值。在 HAL、系统时钟、GPIO 和 `MX_USART2_UART_Init()` 完成后调用，USART2 NVIC 和中断分发也必须正常启用。当前 `main.c` 已调用一次。

执行顺序：

1. 清零 RX FIFO 的读写位置、发送序号和帧可用标志。
2. 重置解析状态和正在组装的帧。
3. 调用 `EspCom_SetReady(true)`。
4. 调用 `HAL_UART_Receive_IT(&huart2, &esp_rx_byte, 1)` 启动单字节接收。

本函数不初始化 UART 外设、不复位 ESP、不等待 ESP 就绪，也不清空 UART 硬件中的所有残留数据。当前忽略 `HAL_UART_Receive_IT()` 的返回状态，因此函数结束不保证接收启动成功。运行中重复调用会丢弃软件缓冲状态，且可能与已有接收冲突；不要将它当作已实现的 UART 故障恢复接口。

### 7.4 轮询 `EspCom_Poll`

```c
void EspCom_Poll(void);
```

无参数，无返回值。主循环应频繁调用；FIFO 为空时立即返回，否则持续取字节并推进解析状态，直到观察到 FIFO 为空。它不会等待尚未收到的帧尾，但持续流入数据时执行时间没有固定上限。

半帧状态会跨多次调用保留。完整帧的版本和 CRC 正确、长度不超过 384 时，保存为最近帧并置可用标志。无效帧不向应用层返回错误码，不会自动发送 ERROR 响应。`TYPE` 不做白名单检查，载荷也不做 JSON 校验。

一次调用可能解析多帧，后面的合法帧会覆盖前面的合法帧。因此即使每次 `Poll` 后立即取帧，也不能保证连续到来的每一帧都被业务层读取。

### 7.5 就绪信号

```c
void EspCom_SetReady(bool ready);
bool EspCom_IsEspReady(void);
```

`EspCom_SetReady(ready)`：`true` 将 PB0 拉高，`false` 将 PB0 拉低，无返回值。只修改输出电平，不暂停 UART、不清缓冲、不阻止发送，也不保证 ESP 停发。初始化会主动设置为 `true`。

`EspCom_IsEspReady()`：无参数，PB1 当前为高电平时返回 `true`，否则返回 `false`。它只是即时 GPIO 读取，不发送状态请求，不进行消抖或等待；高电平也不代表指定缓存一定有效。

发送业务命令前，应用可以自行检查此输入。所有 `EspCom_*` 发送函数都不会替调用者检查就绪信号。

### 7.6 通用发送 `EspCom_Send`

```c
HAL_StatusTypeDef EspCom_Send(uint8_t type,
                              const uint8_t *payload,
                              uint16_t len);
```

| 参数 | 调用要求 |
| --- | --- |
| `type` | 命令类型，通常使用 `ESP_COM_CMD_*`；函数不限制类型值 |
| `payload` | 当 `len > 0` 时，必须指向至少 `len` 字节可读数据；空载荷可传 `NULL` |
| `len` | 实际发送的载荷字节数，`0..384`，文本通常不包含结尾 NUL |

函数在栈上封装帧、分配序号、计算 CRC，然后调用 `HAL_UART_Transmit(&huart2, ..., 1000)` 阻塞发送。调用返回后不保留调用者的指针；调用期间数据必须有效。1000 ms 是 HAL 发送等待超时参数，不是 ESP 响应超时。

返回值适用于本函数以及所有发送封装函数：

| 返回值 | 数值 | 含义 |
| --- | --- | --- |
| `HAL_OK` | `0` | 本次本地 UART 发送完成，尚不能判断 ESP 是否接收或处理成功 |
| `HAL_ERROR` | `1` | 如载荷超过 384 字节、封装 JSON 失败，或底层 HAL 返回错误 |
| `HAL_BUSY` | `2` | 底层 UART 发送状态忙 |
| `HAL_TIMEOUT` | `3` | 底层发送等待超时，线上可能已发出部分帧 |

当前仅显式拒绝 `len > 384`。对于 `payload == NULL && len > 0`，源码不会拒绝，而会将未初始化的载荷区参与 CRC 并发送，调用方必须避免这种参数组合。函数也不验证 JSON、不等待响应、不自动重试、不负责请求队列。

```c
static const char request[] = "{\"api\":\"current\"}";
HAL_StatusTypeDef tx = EspCom_Send(ESP_COM_CMD_GET_CACHE,
                                 (const uint8_t *)request,
                                 (uint16_t)(sizeof(request) - 1U));
```

`sizeof(request) - 1` 仅适用于这里的字符数组，若变量是 `char *` 指针，应在确认有效字符串后使用 `strlen()` 并先检查长度。

### 7.7 链路与状态请求

```c
HAL_StatusTypeDef EspCom_Ping(void);
HAL_StatusTypeDef EspCom_GetStatus(void);
```

| 函数 | 参数 | 发送内容 | 正常预期响应 |
| --- | --- | --- | --- |
| `EspCom_Ping()` | 无 | `TYPE=0x01`，空载荷 | `PONG (0x81)` |
| `EspCom_GetStatus()` | 无 | `TYPE=0x02`，空载荷 | `STATUS (0x82)` |

返回值均为第 7.6 节的本地发送状态。调用后必须通过 `Poll` 和取帧接口异步读取响应，也要处理 `ERROR (0x85)`。`GetStatus` 不会直接返回 WiFi/MQTT 状态，更不会修改 GPIO 或自动解析字段。

### 7.8 业务 API 请求

```c
HAL_StatusTypeDef EspCom_GetCache(const char *api);
HAL_StatusTypeDef EspCom_RefreshApi(const char *api);
HAL_StatusTypeDef EspCom_ReadApi(const char *api);
```

三者的 `api` 都是以 NUL 结尾的业务名称字符串，传 `NULL` 时均默认使用 `"current"`。不是函数指针、URL、完整 JSON 或和风 API 密钥。

| 函数 | TYPE | ESP 协议约定的用途 | 正常预期响应 |
| --- | --- | --- | --- |
| `EspCom_GetCache(api)` | `0x03` | 读取 ESP 已有本地缓存，不触发后端刷新 | `CACHE (0x83)` |
| `EspCom_RefreshApi(api)` | `0x04` | 请求 ESP 排队执行指定 API 的 HTTP 刷新 | `ACK (0x84)` |
| `EspCom_ReadApi(api)` | `0x05` | 天气和一言检查缓存；`time` 实时请求 NTP | `ACK (0x84)` |

三者都发送 `{"api":"名称"}`，函数返回值均为本地发送状态，不直接返回业务数据。天气和一言收到 ACK 后，仍需后续 `GetCache` 读取业务缓存；`time` 直接读取 ACK 的时间字段。

封装细节与限制：

- 内部 JSON 缓冲为 40 字节，包含结尾 NUL；固定 JSON 开销为 10 字节，因此名称最多 29 字节。
- 名称为 30 字节或更长、或 `snprintf()` 失败时返回 `HAL_ERROR`，不发送被截断的 JSON。
- 空字符串不会触发默认值，会实际发送 `{"api":""}`，是否支持由 ESP 决定。
- 名称直接通过 `%s` 插入 JSON，不转义双引号、反斜杠或控制字符；应使用第 6 节的已知名称，不传入未经检查的任意文本。
- 长度按字节计数；传入指针必须有效且字符串有 NUL 结尾。

```c
EspCom_GetCache(NULL);         // current
EspCom_GetCache("daily3d");    // Forecast summary from ESP cache
EspCom_RefreshApi("current");  // Queue a QWeather HTTP refresh
EspCom_ReadApi("daily3d");     // Check local cache; fails if not populated
EspCom_ReadApi(NULL);          // current, not daily3d
```

这些是独立调用形式的速查，不应作为连续突发发送的业务流程。C 函数 `EspCom_ReadApi(NULL)` 默认 `current`；当前 STM32 Shell 的无参默认值为 `daily3d`，取时请显式传入 `ESP_COM_API_TIME` / `time`。

### 7.9 帧查询与读取

```c
bool EspCom_HasFrame(void);
bool EspCom_GetLastFrame(EspCom_Frame *frame);
```

`EspCom_HasFrame()` 无参数，返回是否存在已解析且未清除的最近帧。只读标志，不调用 `Poll`，不检查硬件是否有字节，也不消耗帧。

`EspCom_GetLastFrame(frame)` 的 `frame` 必须指向可写的 `EspCom_Frame` 对象。有可用帧且指针非空时，复制完整结构并返回 `true`；没有帧或传入 `NULL` 时返回 `false`，不会写入调用者对象。调用方只有在返回 `true` 时才能把输出当作本次接收结果。

读取不会清除可用标志，所以重复读取可能获得同一帧。可以直接调用 `GetLastFrame` 检查返回值，不必先调用 `HasFrame`。复制后的数据由调用者拥有，后续内部帧覆盖或清除不改变已复制的对象。

### 7.10 消费标志 `EspCom_ClearFrame`

```c
void EspCom_ClearFrame(void);
```

无参数，无返回值，只设置 `esp_frame_ready = false`。不擦除最近帧内容、不清空接收 FIFO、不重置半帧解析状态，也不向 ESP 发 ACK。

推荐在同一主循环中连续执行 `Poll -> GetLastFrame -> ClearFrame -> 处理本地副本`。在读取和清除之间不要再次调用 `Poll`，否则新解析的帧可能被清除标志。当前 ISR 只存字节，不发布完整帧；这一调用约定依赖单一主循环拥有轮询/消费操作。

### 7.11 UART 接收回调

```c
void EspCom_UartRxCpltCallback(UART_HandleTypeDef *huart);
```

参数是 HAL 提供的有效 UART 句柄，无返回值，不支持 `NULL`。当 `huart->Instance == USART2` 时，将内部 `esp_rx_byte` 放入 FIFO，然后再次对全局 `huart2` 启动单字节中断接收；其他实例不处理。

该函数应从已有 HAL 接收完成分发入口调用，不应由主循环手动调用。当前调用链已接好：

```text
USART2_IRQHandler                         Core/Src/stm32f4xx_it.c
  -> HAL_UART_IRQHandler(&huart2)
  -> HAL_UART_RxCpltCallback              FIFO/Src/Uart_RTX.c
  -> EspCom_UartRxCpltCallback
  -> esp_rx_fifo
  -> EspCom_Poll                         主循环
  -> AA55 / 长度 / 版本 / CRC 解析校验
  -> esp_last_frame + esp_frame_ready
  -> EspCom_GetLastFrame                 应用读取
  -> EspCom_ClearFrame                   应用消费标志
```

不要在其他文件再次定义 `HAL_UART_RxCpltCallback()`。若调整分发入口，要保留 USART1 原有分支。当前回调忽略 FIFO 满时的失败以及 HAL 重启接收的返回值，异常恢复限制见第 12 节。

## 8. Shell 调试命令

当前固件另有 `api_time`、`api_refresh [api]`、`api_result` 和 `esp_apis`。api_time 实时取时并解析 ACK；api_refresh 仅支持原五类天气/一言刷新，不接受 time。所有托管请求期间，原 esp_* 发送命令、修改 Ready 输出和 ref_epd 均返回 HAL_BUSY；无参数 esp_ready、esp_last 与 api_result 仍可使用。底层 C 发送函数不自动管理此占用。

工程通过 USART1 运行 Letter Shell，可直接测试 ESP 通信。命令定义在 `LetterSh/Src/user_cmd.c`。

```text
esp_ready [0|1]      有参数时设置 STM32_Ready，并打印 ESP_Ready 状态
esp_ping             发送 PING
esp_status           发送 GET_STATUS
esp_cache [api]      发送 GET_CACHE，默认 api=current
esp_refresh [api]    发送 REFRESH_API，默认 api=current
esp_read [api]       默认 daily3d；time 实时获取 NTP，其他支持 API 检查缓存
esp_last             轮询、打印最近一帧并清除帧可用标志
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

每条请求命令只发送，不等待 ESP 响应。发送后等待对端返回、让主循环继续轮询，再运行 `esp_last`；不要一次粘贴全部命令并假定每次 `esp_last` 都能立即读到对应响应。`esp_last` 成功打印后会消费帧标志，再执行一次可能显示 `no esp frame`。

`esp_ready` 无参数时仅读取并打印 ESP 就绪状态，不修改 PB0，也没有查询 PB0 电平的功能。虽然当前打印文本含 `stm32_ready set`，无参数调用并没有执行设置。带参数时源码只将字符串 `"0"` 解释为低，其他字符串均解释为高，联调请明确使用 `0` 或 `1`。

若业务主循环已经自动读取并清除最近帧，Shell 的 `esp_last` 可能读不到该帧；二者使用的是同一个接收槽，不是两份独立缓存。

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

当前 ESP 已实现 `not_ready`，用于离线或忙碌时拒绝刷新；还会对不支持的协议版本返回 `bad_version`。STM32 接收模块仅保存这些载荷，不解析错误码或自动重试。

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

说明当前没有可读取的帧标志。可能尚未解析出合法完整帧，也可能已经被 `esp_last` 或业务处理代码清除。通信异常时检查帧头、长度、CRC 或协议版本。当前 STM32 只接受：

```text
Header = AA 55
VER    = 0x01
LEN    <= 384
CRC    = CRC16-CCITT(seed=0xFFFF)
```

### Payload 中包含中文

协议层只按字节传输，不限制编码。建议 ESP 侧 JSON 使用 UTF-8；STM32 如只显示到串口终端，需要确保终端也使用 UTF-8。

## 11. 应用接入示例

### 11.1 初始化和主循环位置

当前工程已经在 `main.c` 中完成 GPIO、USART2、NVIC 初始化并调用 `EspCom_Init()`，通常只需增加应用层的请求调度和响应消费，不要再重复初始化 UART 或重复定义 HAL 回调。

从其他工程移植时，相关顺序为以下片段；它不是替代本工程完整 `main()` 的代码：

```c
HAL_Init();
SystemClock_Config();
MX_GPIO_Init();
MX_USART2_UART_Init();
HAL_GPIO_WritePin(ESP_RST_GPIO_Port, ESP_RST_Pin, GPIO_PIN_SET);
EspCom_Init();
```

确保中断处理链与第 7.11 节一致。USART2 的单字节中断接收不依赖 USART1 DMA。

### 11.2 发起一次实时天气缓存请求

以下函数用于应用事件或定时调度中，不能在每次主循环迭代中无条件发送。需要包含 `Esp_Com.h`，以及项目现有的 `shell.h`；调用时 USART1 Shell 应已初始化。

```c
#include "Esp_Com.h"
#include "shell.h"

extern Shell shell;

static bool AppEsp_RequestCurrent(void)
{
    if (!EspCom_IsEspReady()) {
        shellPrint(&shell, "ESP not ready\r\n");
        return false;
    }

    HAL_StatusTypeDef tx = EspCom_GetCache("current");
    if (tx != HAL_OK) {
        shellPrint(&shell, "ESP transmit failed: %d\r\n", (int)tx);
        return false;
    }

    return true;
}
```

这里的 `true` 仅表示请求发送完成。调用者应记录等待状态和发送时间，后续由接收处理决定业务成功、失败或超时。就绪检查是此示例应用自行增加的条件，底层 `EspCom_GetCache()` 没有这个检查。

### 11.3 接收并分派响应

以下函数可与上面的示例放在同一 C 源文件。它展示帧读取和类型分派，当前仅输出载荷用于联调，没有引入项目尚未实现的 JSON 解析或界面数据接口。

```c
static void AppEsp_Poll(void)
{
    EspCom_Frame frame;

    EspCom_Poll();
    if (!EspCom_GetLastFrame(&frame)) {
        return;
    }
    EspCom_ClearFrame();

    switch (frame.type) {
        case ESP_COM_RSP_PONG:
        case ESP_COM_RSP_STATUS:
        case ESP_COM_RSP_CACHE:
        case ESP_COM_RSP_ACK:
            shellPrint(&shell, "ESP type=0x%02X seq=%u len=%u payload=%s\r\n",
                       (unsigned int)frame.type,
                       (unsigned int)frame.seq,
                       (unsigned int)frame.len,
                       (const char *)frame.payload);
            break;

        case ESP_COM_RSP_ERROR:
            shellPrint(&shell, "ESP error seq=%u payload=%s\r\n",
                       (unsigned int)frame.seq,
                       (const char *)frame.payload);
            break;

        default:
            shellPrint(&shell, "ESP unknown type=0x%02X len=%u\r\n",
                       (unsigned int)frame.type,
                       (unsigned int)frame.len);
            break;
    }
}
```

`%s` 在此仅用于约定的 JSON 文本联调，若扩展为二进制协议则应按 `frame.len` 输出或处理。Shell 输出是阻塞的，连续高流量场景应降低日志量。

在原 `flash_rw == 0` 主循环分支中，用 `AppEsp_Poll()` 替换单独的 `EspCom_Poll()`，保留 `shellTask(&shell)`。这两个 `AppEsp_*` 名字是本文示例辅助函数，不是通信模块已有接口。

```c
/* Inside the existing flash_rw == 0 branch. */
AppEsp_Poll();
shellTask(&shell);
```

请求函数由应用事件调用一次。只有开始接入业务自动消费帧时才使用上述轮询辅助函数；这时 `esp_last` 与它共享帧消费权，应统一由一个地方处理接收结果。

收到 `CACHE` 后，后续业务层应依次检查预期请求、解析 JSON、检查 `valid` 和业务名称、提取所需字段，再更新显示数据。即便 `GetLastFrame()` 成功，也不能直接假定载荷是实时天气或有效 JSON。当前示例不提供完整请求匹配机制。

### 11.4 请求刷新后获取新缓存

建议应用按以下状态推进，每一步等待时主循环仍持续轮询：

1. 检查就绪状态，调用 `EspCom_RefreshApi("current")`；本地发送失败时记录 HAL 状态，不进入业务成功状态。
2. 等待对应 `ACK` 或 `ERROR`。ACK 中 `ok=true` 只表示刷新请求已受理；收到 ERROR 时读取 `code`、`message`。
3. 等待 IO5 恢复 HIGH，再查询 `EspCom_GetCache("response")` 检查本次刷新结果，然后调度 `EspCom_GetCache("current")`，分别等待各自的 CACHE 响应。
4. 检查 `valid`，结合可用的 `obs_time`、`source_update_time` 等字段判断数据是否更新。`valid=true` 本身不能证明此次刷新已经完成。
5. 尚未更新时，按业务规定的间隔、次数和总时限继续查询；完成后更新界面，达到时限则记录超时并决定是否保留旧数据。

`EspCom_ReadApi("current")` 或 `EspCom_ReadApi("minutely5m")` 仅检查本地缓存：有缓存时先等 ACK，再用 GET_CACHE 读取；无缓存时返回 ERROR。`daily3d` 支持刷新，首次读取前需先执行 `esp_refresh daily3d`。当前没有主动天气更新推送，不能假定 ACK 后必然自动收到 CACHE。

分钟降雨联调顺序如下，每次发送后应等响应并继续轮询，不要连续粘贴执行：

1. 配置经纬度并烧录后，运行 `esp_status`、`esp_last`，确认 wifi=true、mqtt=false、esp_ready=true。
2. 运行 `esp_refresh minutely5m`、`esp_last`，确认 0x84 ACK 的 api=minutely5m、refresh=true。
3. 等待 IO5 恢复 HIGH，运行 `esp_cache response`、`esp_last`，确认 api=minutely5m、ok=true。
4. 运行 `esp_cache minutely5m`、`esp_last`，读取描述和预报摘要；按 items 实际长度遍历。
5. `esp_read minutely5m` 只确认缓存存在，不访问和风、不更新天气。

响应等待超时由应用自行实现，可使用 `uint32_t` 保存 `HAL_GetTick()`，通过 `(uint32_t)(HAL_GetTick() - start_tick) >= timeout_ms` 判断，以处理计数回绕。具体时限由 ESP 和后端响应速度决定，不能直接把发送函数的 1000 ms 当作业务响应时限。

## 12. 当前实现边界

| 项目 | 当前行为及接入影响 |
| --- | --- |
| 接收方式 | USART2 每字节触发中断，2 Mbps 下连续输入约 200000 字节/秒，中断负载需要上板评估 |
| 字节 FIFO | 分配 1024 字节，环形队列实际可存 1023 字节；满时丢弃新字节且没有计数或通知 |
| 轮询间隔 | 理想连续满速输入时，空 FIFO 约 5.1 ms 填满；这是理论容量估算，不是实测安全调度周期 |
| 完整帧缓存 | 只有一个最近帧槽；一次 `Poll()` 内多帧也会相互覆盖，没有完整帧队列 |
| 帧消费 | `HasFrame`/`GetLastFrame` 不消费；`ClearFrame` 只清标志，Shell 和业务共享状态 |
| 请求匹配 | 底层提供 GetTxSequence 和观察回调，ApiRefresh 匹配 SEQ 与控制字段；8 位序号会回绕，无自动重试 |
| 多请求并发 | ApiRefresh 共用一个天气/一言/取时请求槽，忙碌时拒绝新请求，直接调用底层发送仍需自行协调 |
| 响应等待 | 托管请求使用 TIM10 超时；原始 esp_* 命令只发送，调用方需自行等待 |
| 半帧恢复 | 解析器没有帧间超时；收到半帧后，后续字节可能被当作旧帧续传，直到满足长度和 CRC 阶段后重置 |
| UART 错误恢复 | 项目未提供 USART2 专用 `HAL_UART_ErrorCallback` 恢复逻辑；ORE 等错误可能导致中断接收中止，需要另行处理 |
| 就绪信号 | 两条状态线不是自动流控；拉低 STM32 就绪线不保证 ESP 停发，也不会停止本地 UART |
| 内容处理 | 已校验请求控制字段，解析一言和时间；一言成功后绘屏，time 只解析保存，时间/天气尚未接入显示 |
| 帧校验失败 | 错误版本/CRC 等帧不会成为可用帧；无应用错误回调或统计，也不会清除此前合法帧的可用标志 |
| 大数据 | 单帧载荷最多 384 字节，无分片/重组，API 缓存展开需考虑 UTF-8 实际字节长度 |
| 多任务使用 | 全局状态无锁；仅支持当前约定的单主循环消费，移植 RTOS 需明确串口和帧状态所有权 |

墨水屏刷新、长时间延时、阻塞日志和其他耗时操作都会延后 `EspCom_Poll()`。即使中断还能接收字节，也可能在主循环恢复前填满 FIFO；2 Mbps 只是串口配置，并不意味着当前实现能无损处理任意连续流量。

本模块与 USART1 资源写入协议也不同：ESP 使用帧头 `AA 55`、版本/类型/序号字段及 CRC16-CCITT；USART1 的资源写入使用帧头 `55 AA` 和 Modbus CRC16。调试工具不能混用两套封包方式。
