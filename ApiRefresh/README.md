# ApiRefresh

STM32 侧 API 刷新流程库，位于 ESP UART 协议层之上。当前仅支持**手动触发、TIM10 计时、主循环推进**：上电不刷新，完成或失败后不重试、不周期刷新。HTTPS、Wi-Fi、密钥与 ESP 缓存仍由独立 Net_Node 工程负责。

## 文件与依赖

| 文件 | 用途 |
| --- | --- |
| `Inc/ApiRefresh.h` | C/C++ 公共接口、结果结构、状态和超时常量 |
| `Src/ApiRefresh.c` | 请求串行调度、响应匹配、JSON 控制字段校验 |
| `Src/ApiRefresh_Shell.c` | `api_refresh`、`api_result` 命令 |
| `Inc/ApiTime.h`、`Src/ApiTime.c` | NTP 时间结果结构和 ACK 校验，入口为 `api_time` |
| `ThirdParty/coreJSON/` | MIT 授权的 coreJSON v3.3.0，校验及遍历 JSON，无动态内存分配 |

底层依赖 `FIFO/Inc/Esp_Com.h`。协议与业务载荷见 [ESP 通信文档](../ESP通信README.md)。两个 CMake 文件已注册此目录；重新生成工程时需保留模板中的注册项。Shell 接入单独放在源文件中，移植核心库时可不编译该文件。

## 快速测试

默认 `flash_rw=0` 模式、TIM10 每次中断间隔为 1 ms、ESP 已烧录对应版本固件时，在 USART1 Shell 逐条执行：

```text
api_refresh current
api_result
```

`api_refresh` 输出 `start=0` 表示请求已在 STM32 本地排入流程，还未证明串口发送成功。稍后再次执行 `api_result`，直至 `state=succeeded` 或 `state=failed`。**不需要手工追加 `esp_cache response` 或 `esp_cache current`**，流程库会完成这两次请求。

```text
api=current state=succeeded
error=none stage=idle tx=0
type=0x83 seq=... len=... payload={...}
```

`stage` 只在失败时表示失败阶段，正常时的 `idle` 没有错误含义。进行中可看到最近收到的 ACK 或结果缓存，不能仅因输出存在 payload 就认定成功。`api_result` 可重复查询，不清除结果；其 Shell 返回值 0 只表示查询命令执行完毕，应读取 `state` 和 `error`。

其他支持的触发命令：

```text
api_refresh daily3d
api_refresh minutely5m
api_refresh alert
api_refresh hitokoto
```

每个 API 完成或失败后，再触发下一个。无参数默认 `current`。`response` 是只读结果缓存，不能作为刷新目标；`daily7d`、空字符串和其他未支持名称返回 `HAL_ERROR (1)`。

当前 MainUI 已接入一言：`api_refresh hitokoto` 成功后，应用层自动提取正文与作者并刷新一次屏幕，保留 `「正文」` 和 `——作者` 的两行格式；失败保留旧文。作者为空时显示出处。正文校验失败会在 Shell 提示 `hitokoto UI: invalid cache; previous text retained`，此时协议 state 仍可能是 succeeded。显示规则见 [项目 README](../README.md#一言刷新接入)。其他 API 暂不自动更新屏幕。

原 `esp_refresh` 仍是单次底层请求，不会启动本库。新流程执行期间，`esp_ping`、`esp_status`、`esp_cache`、`esp_refresh`、`esp_read`、修改就绪输出的 `esp_ready 0/1` 和阻塞绘屏的 `ref_epd` 返回 `HAL_BUSY (2)`。`api_result`、`esp_last`、`esp_apis` 和无参数 `esp_ready` 可继续使用。运行中再次 `api_refresh` 也返回 `HAL_BUSY`，不会覆盖当前任务。

## TIM10 与主循环

本库也支持独立的手动 NTP 取时，使用同一请求槽和时间源。完整用法、结果字段与测试步骤见 [NTP 时间 API](TIME_README.md)。`api_time` / `ApiRefresh_StartTime()` 发送 READ_API time，成功 ACK 直接结束；不经过天气的 REFRESH/response/cache 流程，`api_refresh time` 不受理。

库将每次 `ApiRefresh_Tick1ms()` 调用视作 1 ms。**TIM10 分频和计数值由用户在 CubeMX 配置，本次只接入中断计时，不修改这两个值。** 实际中断周期不是 1 ms 时，下面的超时和等待时间会按比例偏移。

当前 `main.c` 在 `EspCom_Init()`、显示和 Shell 初始化之后，默认模式下执行：

```c
ApiRefresh_Init();
__HAL_TIM_SET_COUNTER(&htim10, 0);
__HAL_TIM_CLEAR_FLAG(&htim10, TIM_FLAG_UPDATE);
if (HAL_TIM_Base_Start_IT(&htim10) != HAL_OK) {
    Error_Handler();
}
```

计数器归零和更新标志清理只用于启动，不改变 PSC/ARR 配置。`MX_TIM10_Init()` 仍由 CubeMX 生成。中断链路为：

```text
TIM1_UP_TIM10_IRQHandler
  -> HAL_TIM_IRQHandler(&htim10)
  -> HAL_TIM_PeriodElapsedCallback
  -> ApiRefresh_Tick1ms
```

`tim.c` 现有回调的 TIM10 分支中仅调用 `ApiRefresh_Tick1ms()`，库内部只增加 `volatile uint32_t` 毫秒计数。中断内不发 UART、不解析 JSON、不打印、不调用 `HAL_Delay()`。不要额外定义第二个 HAL 定时器回调。

默认主循环按以下顺序持续执行：

```c
EspCom_Poll();
ApiRefresh_Poll();
EPD_UI_Poll();
shellTask(&shell);
```

`flash_rw=1` 实验模式不初始化本库、不启动此刷新计时。时间源独立于 `HAL_GetTick()`；未启动 TIM10 或不调用 Tick 时，超时和 ACK 后等待不会前进。中断停用或长时间屏蔽导致丢失更新事件时，计数也不等于真实墙钟时间。

## 公共 API

包含 `ApiRefresh.h`，头文件提供 `extern "C"` 保护。除 Tick 外，所有函数按裸机单一主循环调用，不能从 ISR 或并发任务中启动/轮询刷新。

| 接口 | 参数与结果 |
| --- | --- |
| `void ApiRefresh_Init(void)` | 在 `EspCom_Init()` 后、TIM10 启动前调用一次；清空状态和时间，安装唯一帧观察回调。运行中重调会丢弃当前流程与结果，不作为取消接口使用 |
| `void ApiRefresh_Tick1ms(void)` | 每 1 ms 从 TIM10 回调调用一次，仅更新时间源 |
| `HAL_StatusTypeDef ApiRefresh_Start(const char *api)` | `NULL` 默认 current；白名单为上述五类。HAL_OK 仅表示本地受理，HAL_BUSY 表示已有流程，HAL_ERROR 表示未初始化或非法名称。拒绝请求不改变现有结果，合法名称复制到内部缓冲 |
| `HAL_StatusTypeDef ApiRefresh_StartTime(void)` | 手动发起实时 NTP 取时，复用忙碌检查；HAL_OK 本地受理，HAL_BUSY 已有请求，HAL_ERROR 未初始化 |
| `void ApiRefresh_Poll(void)` | 紧接 `EspCom_Poll()` 调用，处理匹配帧、Ready 和超时；没有网络阻塞等待，但底层 UART 发送仍调用阻塞 HAL 接口，最长超时参数为 1000 ms |
| `bool ApiRefresh_IsBusy(void)` | 仅等待阶段返回 true；idle/succeeded/failed 返回 false |
| `const ApiRefresh_Result *ApiRefresh_GetResult(void)` | 返回库拥有的只读结果地址，不清除结果；收到匹配帧、Poll 推进或下次成功 Start/Init 时会更新，需要长期保存时自行复制 |
| `const char *ApiRefresh_StateString(ApiRefresh_State state)` | 返回状态的静态英文名称，非法值返回 unknown |
| `const char *ApiRefresh_ErrorString(ApiRefresh_Error error)` | 返回错误的静态英文名称，非法值返回 unknown |

`ApiRefresh_Result` 字段：

| 字段 | 含义 |
| --- | --- |
| `api[16]` | 当前刷新目标，以零结束 |
| `state` | 当前阶段或终态 |
| `failed_stage` | 失败时所在阶段；没有失败时为 idle |
| `error` | 具体错误分类 |
| `tx_status` | 最近一次实际发送的 HAL 状态，初始为 HAL_OK；不能单独作为业务成功标志 |
| `has_frame` | 是否保存过本流程匹配 SEQ 的响应 |
| `frame` | 最近一条匹配响应，含 type/seq/len/原始 JSON；失败时可能为 ERROR、失败结果，也可能仍为之前 ACK，应结合 failed_stage 判断 |
| `has_time`、`time` | 本次 time ACK 是否解析成功及 ApiTime 字段；仅 succeeded 且 has_time=true 时读取，下次受理的请求会清空 |

结果中仅有一个帧副本，天气/一言成功时保存最终目标缓存，time 成功时保存 ACK；不同时保存历史请求结果。载荷仍限制为 384 字节。`api_result` 分段输出元数据与原始载荷，避免 Shell 128 字节格式化缓冲截断。

## 流程与判断

以下表格描述天气和一言刷新。取时仅经过 idle -> wait_ready -> wait_ack -> succeeded/failed，不要求观察 Ready LOW 或恢复 HIGH；匹配的成功 ACK 本身就是取时结果。

| 状态 | 动作与转移条件 |
| --- | --- |
| `idle` | 等待手动 Start，不发送请求 |
| `wait_ready` | 等 ESP Ready HIGH，发送 REFRESH_API(api) |
| `wait_ack` | 等匹配 SEQ 的 ACK，校验 api、ok=true、refresh=true |
| `wait_finish` | ACK 后至少等 100 ms，再等 Ready HIGH，发送 GET_CACHE(response) |
| `wait_result` | 等匹配 SEQ 的 CACHE，校验业务 api、valid=true、ok=true |
| `wait_cache_ready` | 等 Ready HIGH，发送 GET_CACHE(api) |
| `wait_cache` | 等匹配 SEQ 的 CACHE，校验 api、valid=true |
| `succeeded` | 保存目标缓存，停止调度 |
| `failed` | 保存错误、失败阶段及最近匹配帧，停止调度，不自动重试 |

每次发出请求后记录底层分配的 8 位 SEQ，忽略不匹配的帧；匹配 SEQ 但类型或控制字段错误会失败。接收观察回调在 `EspCom_Poll()` 内逐帧执行，因此同一次 Poll 中其他帧覆盖 `esp_last`，或 `esp_last` 清除最近帧标志，都不会删除本库已复制的响应。

JSON 先通过 coreJSON 语法校验，再仅遍历顶层字段；api 必须是匹配的字符串，ok/valid/refresh 在相关阶段必须是布尔值。重复控制字段、错误类型、嵌套伪造控制字段不能通过校验。控制字段名与 API 名称按 ESP 当前使用的未转义 ASCII 文本匹配；库中业务文本、未知字段和 UTF-8 内容原样保留。MainUI 的一言文本转换位于 Disp/HitokotoText 模块，由应用层在流程成功后调用。

不要求一定观测到 Ready 的 LOW 边沿，以允许 HTTP 很快完成的情况；ACK 后的 100 ms 是查询前等待，并非网络请求成功的证明。本流程依赖 ESP 遵守“刷新期间 LOW、完成后 HIGH”的约定。response 未包含可与 REFRESH 请求绑定的事务 ID，8 位 SEQ 也会回绕，因此无法完全排除跨回绕的迟到响应或 ESP 错误提供的同 API 旧结果；100 ms 等待不能代替协议级事务标识。

## 超时与错误

超时在 `ApiRefresh.h` 集中定义，单位为 Tick 次数对应的毫秒：

| 常量 | 默认值 | 使用阶段 |
| --- | --- | --- |
| `API_REFRESH_READY_TIMEOUT_MS` | 15000 | wait_ready、wait_cache_ready |
| `API_REFRESH_REPLY_TIMEOUT_MS` | 3000 | wait_ack、wait_result、wait_cache |
| `API_REFRESH_FETCH_TIMEOUT_MS` | 60000 | wait_finish，从 ACK 被处理后开始 |
| `API_REFRESH_SETTLE_MS` | 100 | ACK 后查询 response 前的最短间隔 |
| `API_TIME_REPLY_TIMEOUT_MS` | 10000 | 仅 NTP 的 wait_ack，覆盖服务器等待及 DNS 等耗时 |

使用无符号时间差处理计数回绕。到达阶段超时时限即失败，不无限等待；超时由主循环检查，长时间阻塞主循环会推迟报告。若完整帧已在时限内由 EspCom_Poll 解析并复制，即使稍后才调用 ApiRefresh_Poll 处理，仍按解析时间判断；UART 字节刚到达但尚未解帧不算已收到完整响应。

| error 输出 | 含义 |
| --- | --- |
| `none` | 尚未失败，是否成功仍需看 state |
| `tx_failed` | HAL 发送失败，查看 tx_status |
| `timeout` | 当前阶段等待超时，查看 failed_stage |
| `invalid_response` | JSON、类型、API 或必需控制字段不符合约定 |
| `esp_error` | 收到匹配的 ERROR 帧，查看载荷的 code/message |
| `rejected` | ACK 的 ok 或 refresh 为 false |
| `remote_refresh_failed` | response 有效但 ok=false，ESP 的 HTTP 刷新或缓存更新失败 |
| `invalid_cache` | response 或最终目标缓存的 valid=false |

例如之前的一言输出 `valid:true,ok:false,message:"refresh failed; previous cache retained"`，会得到 `state=failed,error=remote_refresh_failed,stage=wait_result`，保留完整载荷并停止；不会继续读取旧一言缓存并误报本次成功。排查网络、TLS、服务响应仍需 ESP 端信息。

## 使用边界与验证

- 单实例、单请求；没有请求队列、取消接口、自动重试或周期任务。下一次合法 Start 会替换上次结果。
- `ApiRefresh_Init()` 占用 `EspCom_SetFrameObserver()` 的唯一回调槽。使用本库期间不要替换该回调；回调内不可递归 Poll 或发送请求。
- Shell 已拦截流程内的冲突命令；其他 C/C++ 代码仍须先检查 `ApiRefresh_IsBusy()`，不要在流程运行时直接发送 EspCom 请求或执行长时间阻塞操作。
- `succeeded` 说明按当前协议取得有效缓存封装，不保证业务内容完整、观测时间最新或屏幕已更新。应用层的 EPD_UI_Poll 已接入一言解析和绘屏；其他数据模型与缓存持久化仍待接入。
- 底层 FIFO、UART 错误恢复、384 字节上限等限制仍在，详见通信文档。

主机测试编译真实 EspCom、ApiRefresh、coreJSON 和 Shell 命令，使用模拟 UART/GPIO 与人工 Tick；覆盖五类成功流程、SEQ 匹配、帧覆盖与消费、JSON 异常、ESP/刷新失败、各阶段超时和手动命令互斥。运行方法见 [测试说明](../tests/esp_com/README.md)。已完成 Debug 固件构建，尚未验证真实 TIM10 周期、UART 电气与 ESP HTTPS 时序。
