# NTP 时间 API

2026-09-13：本版本完成 STM32 对 ESP 实时 NTP 接口的请求、响应校验、字段解析与 Shell 联调。用于局部刷新实验前的版本记录；本次未修改 MainUI 时间、RTC、走时、定时器配置、一言显示或墨水屏驱动，也未启用局部刷新或周期取时。

## 协议

业务名称为 `time`，宏 `ESP_COM_API_TIME`。请求 TYPE=0x05（READ_API），载荷 `{"api":"time"}`。ESP 每次执行实时 NTP 请求，成功直接返回与请求 SEQ 相同的一帧 ACK（0x84）：

```json
{"ok":true,"api":"time","refresh":false,"realtime":true,"unix_time":1799798400,"utc_offset":28800,"date":"2027-01-13","time":"08:00:00","weekday":3}
```

此 ACK 已包含取时结果，不是排队通知。不需要 GET_CACHE，也不读取或更新 ESP 的 response 缓存。`REFRESH_API time` 和 `GET_CACHE time` 在 ESP 返回 ERROR；STM32 的 `api_refresh time` 则直接返回 HAL_ERROR，不发送。

按 ESP 对接文档，NTP 服务器为 `ntp.aliyun.com`，UDP 123，默认时区 UTC+8；配置位于独立 Net_Node 工程的 `lib/SystemConfig/AppConfig.h`。STM32 不持有 NTP 客户端，也不修改 ESP 配置。

## CLI 测试顺序

使用默认 `flash_rw=0` 固件、匹配版本 ESP、USART1 的 2000000 波特率 8N1。逐条执行，等待主循环处理，不要连续粘贴一组请求：

1. `esp_ready`：检查是否为 1；为 0 时先检查 ESP 联网及忙碌状态。
2. `esp_ping`，稍后 `esp_last`：确认收到 PONG（0x81）。
3. `api_time`：启动一次取时，`start=0` 只表示 STM32 本地受理。
4. 间隔执行 `api_result`，直到 `api=time state=succeeded` 或 `state=failed`。
5. 成功后核对日期、时间、星期和时区。再次取时重复步骤 3、4。

成功输出示例（实际值以 ESP 返回为准）：

```text
api=time state=succeeded
error=none stage=idle tx=0
date=2027-01-13 time=08:00:00 weekday=3
unix_time=1799798400 utc_offset=28800
type=0x84 seq=... len=... payload={...}
```

`api_result` 可重复查看，不消费结果。显示时间是本次响应的快照，等待后再查看不会自行走秒。无失败时 `stage=idle` 没有错误含义；Shell 的 `Return: 0` 仅表示命令执行成功，须查看 state。取时不触发屏幕刷新，MainUI 时间仍保留原示例。

也可用底层调试顺序 `esp_read time`，稍后 `esp_last`，从 ACK 读取原始时间字段。此方式不启动 ApiRefresh 跟踪、不填入 `ApiRefresh_Result.time`；`api_result` 仍显示上一次受理的库请求。两种方式选一种，不要交错测试。

## C/C++ 调用

现有初始化与主循环已经提供 TIM10 毫秒计时、`EspCom_Poll()` 和 `ApiRefresh_Poll()`，本次无需增加启动操作。

```c
#include "ApiRefresh.h"

// Call once from a user event in the main loop, never from an ISR.
HAL_StatusTypeDef status = ApiRefresh_StartTime();

// On later loop passes, after EspCom_Poll() and ApiRefresh_Poll():
const ApiRefresh_Result *result = ApiRefresh_GetResult();
if (result->state == API_REFRESH_SUCCEEDED && result->has_time) {
    ApiTime snapshot = result->time;
    // The application may retain snapshot for a future clock/RTC integration.
    (void)snapshot;
}
```

不要每次循环无条件调用 StartTime；HAL_OK 是本地受理，HAL_BUSY 表示天气/一言/取时请求尚在执行，HAL_ERROR 表示尚未初始化。拒绝请求不覆盖现有结果。合法新请求会清空旧结果，包括 has_time；取时失败不把旧时间当成本次成功。需要长期保留快照时由调用者复制，库无历史缓存、取消、重试或后台取时。

`ApiRefresh_GetResult()` 返回内部只读地址，不分配内存。`state=succeeded && has_time=true` 是读取 time 字段的条件；单独 tx_status=HAL_OK 或 has_frame=true 不够。

| ApiTime 字段 | 类型 | 含义 |
| --- | --- | --- |
| `unix_time` | uint32_t | UTC Unix 秒数，尚未加 utc_offset；本接口接受 1～4294967295 |
| `utc_offset` | int32_t | 相对 UTC 的秒偏移，可为负值，本地日期时间已经包含该偏移 |
| `date[11]` | char 数组 | 本地日期 YYYY-MM-DD，以零结束 |
| `time[9]` | char 数组 | 本地时间 HH:MM:SS，以零结束，24 小时制 |
| `weekday` | uint8_t | 本地星期：0 日、1 一、2 二、3 三、4 四、5 五、6 六 |

不要再次给 date/time 叠加时区；如未来从 unix_time 生成本地时间，应使用足够宽的有符号中间值加 utc_offset。

`ApiTime_ParseAck(const EspCom_Frame *frame, ApiTime *time)` 是可单独调用的纯解析接口，在 `ApiTime.h` 声明。成功返回 true 并写入字段，失败返回 false 且不改变输出对象。它检查 ACK 类型、载荷长度和字段，但不检查 SEQ 或代替协议 CRC 校验；正常应由 EspCom 收到合法帧后使用。StartTime 流程另行完成 SEQ 匹配。

## 校验与失败

使用现有 coreJSON 检查 JSON 语法，只读取顶层字段。api 必须为 time，ok/realtime 必须为 true，refresh 必须为 false；九个约定字段均必需，重复字段、嵌套替代字段和错误类型拒绝。未知扩展字段忽略。字段名及日期时间采用约定的未转义 ASCII 格式。

数字必须为 JSON 整数文本，不接受数字字符串、小数或指数；时间戳拒绝 0、负数和 uint32_t 溢出。时区偏移接受 -86400～86400 秒，星期接受 0～6。日期检查月份、天数和闰年，时分秒范围为 0～23、0～59、0～59。当前不做 unix_time 与 date/time/weekday 的交叉换算一致性检查，也不评估网络延迟或 NTP 服务可信度；后续校时前仍需按实际设备测试核对。

| 条件 | state / error | 说明 |
| --- | --- | --- |
| ESP 未就绪超过 15 秒 | failed / timeout，stage=wait_ready | 未发送，先检查联网 |
| 发送失败 | failed / tx_failed | 查看 tx 和原有 UART 状态 |
| 10 秒内没有有效匹配响应 | failed / timeout，stage=wait_ack | NTP 等待、DNS 或 UART 故障均可能导致 |
| 匹配 ERROR 帧 | failed / esp_error | 原始 payload 保留，例如 not_ready 或 NTP time request failed |
| 匹配帧类型/字段非法 | failed / invalid_response | 不输出已解析的时间，不读取缓存补救 |
| ACK 校验通过 | succeeded，has_time=true | 保存解析结果及原始 ACK，停止请求 |

时间限制定义在 `ApiRefresh.h`：复用 `API_REFRESH_READY_TIMEOUT_MS=15000`，NTP 专用 `API_TIME_REPLY_TIMEOUT_MS=10000`。天气 ACK 超时仍为 3000 ms。均依赖 TIM10 Tick，未调整 PSC/ARR；收到匹配 ACK 后直接处理，即便此刻还读到 Ready LOW。Ready 恢复或本地 TX 完成不能代替成功 ACK。

取时期间 `ApiRefresh_IsBusy()` 为 true，已有 Shell 冲突拦截继续生效，包括 esp_* 发送、修改 Ready 输出及 ref_epd；api_result、esp_last 和无参数 esp_ready 可用。其他 C/C++ 代码也应遵守同一请求槽的使用约定。8 位 SEQ 回绕和 UART 接收限制沿用 [刷新库说明](README.md)。

## 版本验证

已通过主机通信/刷新/NTP 测试和 STM32 Debug 构建。一言显示回归确认 time 结果不会触发一言绘屏。测试覆盖直接 ACK、错误/超时、SEQ、并发拦截、字段边界和重复读取不自动取时。主机测试不连接实际 NTP；本次未烧录、未测局部刷新，硬件取时成功率仍待上板验证。

本阶段代码与文档可用于用户自行进行测试前的版本管理。本次没有创建提交、标签或执行 push。
