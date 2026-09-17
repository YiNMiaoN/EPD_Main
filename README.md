# TopInfo

TopInfo 是一个基于 **STM32F401RCTx + ESP8266 + 400×300 墨水屏**的信息看板项目，目标是在低刷新率屏幕上集中展示时间、待办、天气、降雨、预警和一言。

本仓库提供 **STM32 主控固件**，由 STM32CubeMX 生成外设配置，使用 STM32 HAL、C11/C++17 和 CMake/CLion 构建。ESP8266 固件属于独立的 **Net_Node** 工程，负责 Wi-Fi、HTTPS 请求和业务缓存，不包含在本仓库中。

## 当前版本

截至 **2026-09-13**，本版本处于显示基础功能和 ESP 通信联调阶段。本次补齐 NTP 获取 API，作为后续局部刷新测试前的版本；局部刷新实验尚未开始。

| 功能 | 当前状态 |
| --- | --- |
| 墨水屏 | SPI 驱动、帧缓冲、绘图和主界面布局已实现 |
| 字体 | 支持 UTF-8 转 GBK 字模，16/24 像素中文和对应 ASCII 字体 |
| 天气图标 | 从外部 Flash 的 QWIC 图标包读取 16×16、32×32 位图 |
| ESP 通信 | 帧封装、CRC 校验、单字节中断接收、FIFO 和轮询解帧已实现 |
| 数据接口 | 可请求实时天气、三日预报、分钟降雨、预警和一言 |
| API 刷新库 | TIM10 计时、手动触发完整刷新流程，含响应匹配和阶段超时；无周期刷新 |
| 网络时间 | api_time 实时请求 ESP NTP，解析日期、时间、星期与时区；未接入 RTC、走时或显示 |
| 调试控制台 | USART1 LetterShell，支持查询、刷新、查看响应及就绪状态 |
| 数据驱动显示 | 一言手动刷新成功后自动更新 MainUI；天气、时间和待办仍为固定示例 |
| 外部 Flash 写入 | 保留 USART1 DMA 资源写入实验流程，存在已知限制 |
| USB CDC | 已初始化设备栈，接收数据尚未接入业务 |

当前没有 RTOS，使用裸机主循环。上电会初始化并清空墨水屏，不自动请求 API；通过命令绘制主界面。`api_refresh hitokoto` 完成并取得可用正文后，自动更新一言并刷新屏幕。

本次版本更新重点：

- 对齐 ESP 直连和风及一言的 HTTPS 接口约定，不再按旧 MQTT 桥接语义说明业务。
- 完善五类数据源及刷新结果缓存的名称常量、Shell 支持列表和联调说明。
- 三日预报替代旧七日预报入口，Shell 默认读取三日预报。
- Shell 业务命令发送前检查 ESP 就绪线，并返回实际本地发送状态。
- 修复长 JSON 被 Shell 格式化缓冲截断的问题。
- 增加可在 PC 上运行的通信和 Shell 测试。
- 新增 ApiRefresh 库，区分 ACK 受理、刷新结果和最终缓存，保留失败响应供调试。
- MainUI 一言接入成功刷新结果，保留两行布局、正文括号和作者前的破折号。

## 文档导航

| 文档 | 内容 |
| --- | --- |
| [ESP通信README.md](ESP通信README.md) | 当前协议、全部 16 个底层 C/C++ API、业务字段和 Shell 联调 |
| [ApiRefresh/README.md](ApiRefresh/README.md) | 手动刷新库、TIM10 接入、状态/错误及 api_refresh/api_result 命令 |
| [ApiRefresh/TIME_README.md](ApiRefresh/TIME_README.md) | NTP 时间 API、字段与校验、CLI 测试及本次版本边界 |
| [和风README.md](和风README.md) | QWeather 图标资源格式、Flash 布局及图标接口 |
| [tests/esp_com/README.md](tests/esp_com/README.md) | PC 主机测试的构建方式和覆盖范围 |
| [tests/main_ui/README.md](tests/main_ui/README.md) | 一言解析、排版和绘屏触发的主机测试 |
| [STM32通信协议README.md](STM32通信协议README.md) | 早期 ESP/MQTT 桥接协议背景，业务行为以新版通信文档为准 |

ESP 文档中涉及的 lib/HeFeng、lib/Hitokoto、lib/Stm32Com 和 lib/SystemConfig/AppConfig.h 等路径属于 Net_Node 工程。

## 硬件与接线

主控为 STM32F401RCTx，Cortex-M4。当前配置使用 25 MHz 外部晶振，系统时钟为 84 MHz；链接脚本配置片内 Flash 256 KB、RAM 64 KB。

| 功能 | 外设 | STM32 引脚 |
| --- | --- | --- |
| 墨水屏数据 | SPI1 | PA5 SCK、PA7 MOSI、PB4 MISO（工程配置） |
| 墨水屏控制 | GPIO | PA4 CS、PC4 DC、PC5 RES、PA6 BUSY |
| 外部 Flash | SPI2 | PB10 SCK、PB14 MISO、PB15 MOSI、PB12 CS |
| Flash 辅助控制 | GPIO | PB13，工程命名 SPI2_RES，接线须结合实际器件确认 |
| Shell 调试口 | USART1 | PA9 TX、PA10 RX |
| ESP 通信口 | USART2 | PA2 TX → ESP GPIO3/RX；PA3 RX ← ESP GPIO1/TX |
| STM32 就绪输出 | GPIO | PB0 → ESP GPIO4 / NodeMCU D2 |
| ESP 就绪输入 | GPIO | PB1 ← ESP GPIO5 / NodeMCU D1 |
| ESP 复位 | GPIO | PA8 → ESP RST，低电平复位 |
| 调试下载 | SWD | PA13 SWDIO、PA14 SWCLK，连接 ST-Link |

USART1 和 USART2 均为 **2000000 波特率、8N1、无硬件流控**。使用 3.3V 逻辑电平并共地，串口工具需要支持 2 Mbps 和 UTF-8。板载 USB 转串口 TX 与 STM32 TX 不应同时驱动 ESP RX。

SPI1、SPI2 使用软件片选和 8 位数据。外部 Flash 驱动的 ID 检查匹配 EF 40 18，接线和器件容量须与实物一致。I2C1、TIM3 和按键 GPIO 已有初始化代码，目前主业务未使用它们。

## 构建与烧录

### 环境要求

- CMake **4.0 或更高版本**，与当前根目录构建配置一致。
- Arm GNU Toolchain，包含 arm-none-eabi-gcc、arm-none-eabi-g++、arm-none-eabi-objcopy 等程序，并加入 PATH。
- Ninja 或 MinGW Make；也可使用 CLion 配置的工具链。
- ST-Link/OpenOCD 用于下载调试，或使用 STM32CubeProgrammer 烧录 HEX。
- 修改外设配置时使用 STM32CubeMX 打开 [TopInfo.ioc](TopInfo.ioc)。

### 命令行构建

在仓库根目录执行。以下使用独立 Ninja 构建目录，避免与已有 CLion/MinGW 构建缓存混用：

~~~powershell
cmake -S . -B build/debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build/debug --parallel
~~~

该方式需要 Ninja 在 PATH 中；实际已验证的本机构建采用 CLion 的 MinGW Make 配置。已有 cmake-build-debug 目录且对应工具链路径仍有效时，也可执行：

~~~powershell
cmake --build cmake-build-debug --parallel
~~~

| 构建产物 | 用途 |
| --- | --- |
| TopInfo.elf | 调试和带地址的程序映像 |
| TopInfo.hex | Intel HEX 烧录文件 |
| TopInfo.bin | 原始二进制，片内 Flash 起始地址为 0x08000000 |
| TopInfo.map | 链接布局和内存占用分析 |

在 CLion 中打开项目根目录，确认 Arm 编译器和构建工具路径，选择 Debug 配置后构建。新增源文件后应重新加载 CMake：当前使用 GLOB_RECURSE，已有构建缓存不会保证自动发现新文件。

### 下载到 STM32

项目提供 [st_nucleo_f4.cfg](st_nucleo_f4.cfg)，使用 ST-Link 和 SWD。连接好目标板后，可手动运行以下命令；此命令会实际写入固件并复位设备：

~~~powershell
openocd -f st_nucleo_f4.cfg -c "program build/debug/TopInfo.elf verify reset exit"
~~~

使用其他构建目录时替换 ELF 路径，也可在 STM32CubeProgrammer 中选择 HEX 文件下载。OpenOCD/ST-Link 版本兼容性及硬件连接需在设备上验证。

**STM32 固件、ESP 固件、外部 Flash 字库/图标是三类独立资源。** 烧录 STM32 的 ELF/HEX 不会同时配置 ESP 的 Wi-Fi/API，也不会写入外部字库和图标。

## 首次运行与联调

### 显示检查

1. 确认外部 Flash 已写入匹配地址的字库和图标包。
2. 以默认 flash_rw = 0 编译并启动 STM32。
3. 通过 USART1 以 2000000 8N1 打开串口终端，进入 letter:/$。
4. 执行 ref_epd，绘制固定主界面并使墨水屏休眠。

如果只烧录主控程序但未准备外部资源，不能据此判断文字或图标显示正常。ref_epd 不会请求 ESP 数据。

### ESP 接口支持范围

以下数据源支持情况依据当前 Net_Node 对接文档，实际需要烧录对应版本 ESP 固件。

| API 名称 | 获取内容 | 关键约定 |
| --- | --- | --- |
| current | 实时天气 | 平铺 JSON，温湿度等多数数值为字符串，可选字段可能被省略 |
| daily3d | 三日预报 | items 为 1～3 天，最高/最低温度为 JSON 数值并保留小数 |
| minutely5m | 分钟降雨描述和摘要 | count 是完整数据总数，单帧 items 可能只含前几项 |
| alert | 天气预警摘要 | valid=true、count=0、items=[] 表示有效的无预警结果 |
| hitokoto | 一言、出处、作者 | hitokoto、from 为字符串，from_who 可为 null |
| todolist | 所有项目的今日未完成任务摘要 | stage=today；托管刷新与原始 JSON 输出已接入，未接入 UI |
| todolist_inbox | Inbox 今天和明天到期的未完成任务摘要 | stage=inbox_next2d；账户时区下两个自然日，未接入 UI |
| response | 最近一次已受理刷新结果 | 只读结果缓存，检查 valid、业务 api 和 ok；不支持刷新 |
| time | 实时 NTP 时间 | 仅 READ_API，时间直接放在 ACK，无缓存，不走 REFRESH_API |

daily7d 已停用，请使用 daily3d。hourly72h、status 有缓存逻辑但当前无数据源；airquality、indices、sun、moon 的 UART 缓存返回尚未展开。

ESP 的 Wi-Fi、和风地址/密钥、位置及一言参数在 Net_Node 工程配置。分钟降雨使用经纬度，三日预报使用独立坐标，其他天气接口的位置配置也可能不同；部署时应在 ESP 端核对，STM32 不提供这些配置接口。一言不需要和风密钥。

### 推荐命令顺序

当前推荐先用 `esp_ping`、稍后 `esp_last` 验证链路，再运行 `api_refresh current`，随后间隔运行 `api_result`，等待 `state=succeeded` 或 `state=failed`。新库会自动完成 ACK 检查、等待 ESP 就绪、读取 response 和目标缓存。`start=0` 仅表示本地受理；失败时查看 error、stage 和原始 payload。无周期刷新，每次仍需手动触发，详见 [刷新库说明](ApiRefresh/README.md)。

下面是仍然可用的**底层逐步调试流程**，应在新库空闲时使用。逐条执行，每次请求后等待响应再运行 esp_last，不要整段连续粘贴。当前 ESP 启动后缓存为空，需要主动刷新。

| 步骤 | 命令 | 预期结果 |
| --- | --- | --- |
| 1 | esp_apis | 查看数据源支持范围和默认值 |
| 2 | esp_ping，稍等后 esp_last | type=0x81，确认链路响应 |
| 3 | esp_status，稍等后 esp_last | type=0x82；wifi=true、busy=false、esp_ready=true，mqtt=false 正常 |
| 4 | esp_refresh current，稍等后 esp_last | type=0x84 且 ok=true，表示刷新已排队 |
| 5 | 间隔查询 esp_ready | 等待刷新后恢复 esp_ready=1 |
| 6 | esp_cache response，稍等后 esp_last | 确认 valid=true、api=current、ok=true |
| 7 | esp_cache current，稍等后 esp_last | 获取 valid=true 的实时天气缓存 |

测试其他 API 时，将步骤 4、7 的 current 替换为 daily3d、minutely5m、alert、hitokoto、todolist 或 todolist_inbox；步骤 6 的 response 保持不变。每个 API 完成后再测试下一个。

Todoist 托管联调：执行 `api_refresh todolist` 或 `api_refresh todolist_inbox`，随后间隔执行 `api_result`，直到 succeeded/failed。STM32 校验通用控制字段与对应的 today/inbox_next2d 阶段，失败保留 response，成功保存摘要原始 JSON。任务解析与 MainUI 接入尚未实现；count 不是全天总数，has_more 为真也不会自动翻页。详细协议见 [ESP 通信文档](ESP通信README.md)。

tx=0 只说明 STM32 本地发送完成；ACK 只说明 ESP 接受请求；刷新结果缓存中的 ok=true 才表示该次请求及缓存写入成功。刷新失败会保留旧缓存，因此旧数据的 valid=true 不代表最新刷新成功。只有已收到对应刷新 ACK 后，才能按该流程判断本次结果，避免误读此前的 response。

### Shell 命令速查

| 命令 | 作用 |
| --- | --- |
| ref_epd | 绘制主界面（含最近成功的一言）、刷新屏幕并休眠 |
| api_refresh [api] | 手动启动完整刷新流程，默认 current，不启动周期任务 |
| api_time | 手动启动 NTP 取时，完成后用 api_result 查看解析时间 |
| api_result | 查询刷新状态、失败阶段及最近匹配响应，不消费结果 |
| esp_apis | 显示 API 支持范围 |
| esp_ready | 查看 ESP 就绪输入 |
| esp_ready 0 / esp_ready 1 | 设置 STM32 就绪输出；不能代替 ESP 联网 |
| esp_ping | 请求 PONG |
| esp_status | 查询 ESP 自身状态，与 esp_cache status 不同 |
| esp_refresh [api] | 请求 HTTPS 刷新，默认 current |
| esp_cache [api] | 获取 ESP 已有缓存，默认 current |
| esp_read [api] | 默认 daily3d；天气/一言检查缓存，time 实时请求 NTP 并在 ACK 返回时间 |
| esp_last | 打印最近合法帧并清除可用标志 |

业务命令在 ESP 就绪线为低时不发送，返回 HAL_BUSY (2)。底层 C 接口不会自动检查 GPIO；EspCom_ReadApi(NULL) 仍默认 current，与 Shell 默认值不同。

ApiRefresh 忙碌期间，所有 ESP 发送命令、`esp_ready 0/1` 和 `ref_epd` 返回 HAL_BUSY；无参数 `esp_ready`、`esp_last` 和 `api_result` 可继续查看状态。手动刷新库启动时可先等待 ESP 就绪，最长等待时间见库配置。

## 代码结构与运行流程

NTP 联调：`esp_ready` 确认就绪，`api_time` 启动，再间隔执行 `api_result` 等待终态。成功时显示 date/time/weekday、unix_time/utc_offset 和原始 ACK。时间是快照，不会自行走秒或触发屏幕刷新；无需 esp_cache response/time。详细步骤见 [NTP 时间 API](ApiRefresh/TIME_README.md)。

~~~text
Core/                   CubeMX 外设、主循环、C/C++ 桥接和全局对象
Disp/                   墨水屏驱动、Adafruit_GFX、字库读取、MainUI
Flash/                  外部 SPI Flash 读写封装
FIFO/                   ESP 协议、串口缓冲、USART1 DMA 资源写入
ApiRefresh/             手动刷新状态机、TIM10 时间源、Shell 命令和 coreJSON
LetterSh/               LetterShell 和项目调试命令
QWeather/               外部 Flash 天气图标包读取
USB_DEVICE/             USB CDC 应用和底层配置
Drivers/                STM32 HAL、CMSIS
Middlewares/            ST USB Device 库
tests/esp_com/          PC 主机测试及 HAL/Shell 替身
TopInfo.ioc             CubeMX 配置
CMakeLists.txt          当前构建配置
CMakeLists_template.txt 构建配置生成模板
STM32F401RCTX_FLASH.ld   链接脚本
~~~

入口为 [Core/Src/main.c](Core/Src/main.c)。完成 HAL、时钟和外设初始化后，释放 ESP 复位，调用 EspCom_Init()，再根据 flash_rw 选择模式：

| 模式 | 初始化 | 主循环 |
| --- | --- | --- |
| flash_rw = 0（默认） | 墨水屏初始化、清屏、Shell、ApiRefresh 初始化并启动 TIM10 中断 | EspCom_Poll()、ApiRefresh_Poll()、EPD_UI_Poll()、shellTask(&shell) |
| flash_rw = 1（实验） | Flash ID 检查/读测试、USART1 DMA 接收 | UART_DMA_Poll()、Uart_OTA_Rx() |

ESP 接收路径为 USART2 IRQ → HAL_UART_RxCpltCallback → 字节 FIFO → EspCom_Poll → 最近一帧。HAL 接收完成分发入口在 FIFO/Src/Uart_RTX.c，不应在其他文件重复定义。

EspCom_Poll 在接受每条合法帧时调用观察回调，ApiRefresh 复制匹配响应，独立于 esp_last 的最近帧消费。TIM10 回调只调用 ApiRefresh_Tick1ms 累计时间，实际请求与 JSON 检查都在主循环进行。库要求每次 Tick 间隔 1 ms；TIM10 分频和计数值由用户在 CubeMX 配置，本次不调整。

串口帧格式为：

~~~text
AA 55 VER TYPE SEQ LEN_H LEN_L PAYLOAD CRC_H CRC_L
~~~

当前版本 VER=0x01，最大载荷 384 字节，完整帧最大 393 字节。长度和 CRC 为高字节在前；CRC16-CCITT 使用多项式 0x1021、初值 0xFFFF，覆盖 VER 到载荷末尾。完整函数说明见 [ESP通信README.md](ESP通信README.md)。

## 显示与资源

### 对象关系

[Core/Src/TopInfo_Project.cpp](Core/Src/TopInfo_Project.cpp) 唯一定义：

~~~cpp
Flash flash;
EPD epd;
MainUI mainUI(epd);
~~~

EPD 继承 Adafruit_GFX，持有唯一的 15000 字节单色帧缓冲；MainUI 持有 EPD& 并绘制内容，不额外创建帧缓冲。其他 C++ 模块通过 TopInfo_Project.h 使用这些对象，C 代码通过 Epd_Api.h 调用桥接接口。

- EPD_HW_Display()：仅将当前帧缓冲推送到面板。
- EPD_UI_Refresh()：绘制 MainUI 后刷新面板。
- QWeatherIcon_Draw()：读取并绘制图标到帧缓冲，不单独刷新面板。

主界面左侧为待办和进度条，右侧为日期时间、天气、预警、三日预报及降雨，底部为一言。主要分隔位置为 x=245、y=40 和 y=266；布局实现在 [Disp/Src/MainUI.cpp](Disp/Src/MainUI.cpp)。

### 一言刷新接入

运行 `api_refresh hitokoto` 后，用 `api_result` 查看进度。只有完整流程成功且正文通过检查，`EPD_UI_Poll()` 才更新 MainUI，并自动进行一次整屏刷新，随后休眠。无需再执行 `ref_epd`。原始 `esp_refresh hitokoto` 和 `esp_cache hitokoto` 仍只用于通信调试，不自动更新界面。

排版保留 16 像素字体：首行 `(0,268)` 为 `「正文」`，次行 y=284 为 `——作者`，按含破折号的署名宽度计算 `x = 400 - 宽度`，右边距为 0。中文及破折号按 16 像素、ASCII 按 8 像素计宽。优先采用 `from_who`，缺失、null 或空白时回退到 `from`，仍为空则显示“佚名”，回退署名同样右对齐。超过行宽时按完整 UTF-8 字符裁剪并加省略号，保留右括号；正文和作者前缀之后均可容纳 23 个全宽字（混合 ASCII 按实际字宽计算）。

[HitokotoText.cpp](Disp/Src/HitokotoText.cpp) 使用 coreJSON 校验和提取字段，并解码 JSON 转义。控制字符转为空格，避免破坏两行格式。首次成功前保留示例一言；请求失败、缓存无效或正文为空时保留上次文本。若协议流程成功但正文无效，Shell 输出 `hitokoto UI: invalid cache; previous text retained`；`api_result` 中的 succeeded 仍仅表示协议流程成功。

重画前清理一言区域，避免短句覆盖长句留下旧字；每次刷新前初始化面板，支持上次操作已休眠的情况。此处调用现有阻塞式整屏驱动，只在刷新事务结束后执行，不在 TIM10 或 UART 中断内绘屏。中文字模仍依赖外部 Flash，现有 GBK 渲染器不支持的字符（如多数 emoji）不会显示。上板需确认面板 BUSY 能正常释放。

### 外部 Flash 地址

| 资源 | 起始地址 | 格式 |
| --- | --- | --- |
| GBK 16 像素字库 | 0x00000000 | 16×16，每字 32 字节 |
| GBK 24 像素字库 | 0x000BC040 | 24×24，每字 72 字节 |
| ASCII 16 像素字库 | 0x002630D0 | 8×16，每字 16 字节 |
| ASCII 24 像素字库 | 0x002636C0 | 12×24，每字存储 48 字节 |
| QWeather 图标包 | 0x00265000 | QWIC 包头、索引及 16/32 像素位图 |

字库地址定义在 Disp/Inc/Font.h，图标基址定义在 QWeather/Inc/QWeather_Icon.h。字体渲染路径为 UTF-8 解码、Unicode 转 GBK、读取 Flash 字模、绘制位图。资源生成和部署须与这些偏移一致；固件不会自动生成字库或图标包。

### USART1 资源写入模式

Uart_OTA.c 的名称沿用旧命名，当前功能实际是通过 USART1 向**外部 Flash**写入资源，不是 STM32 固件在线升级。

它使用独立协议 55 AA LEN_H LEN_L PAYLOAD CRC_H CRC_L 和 Modbus CRC16，不能使用 ESP 通信帧代替。当前写入地址从 0x265000 开始，与天气图标包区域相同，写入会覆盖该区域。写入完成后需恢复 flash_rw=0 并重新编译烧录。

此模式存在长度校验和地址步进问题，见后面的已知限制；本版本不将其作为经过完整验证的资源部署工具。

## 验证记录

2026-09-13 当前源码已在 Windows 下使用 CLion 配置的 Arm GNU 工具链完成 Debug 构建，生成 ELF、HEX 和 BIN。最近一次链接器报告：

| 区域 | 已用 | 总量 | 占比 |
| --- | --- | --- | --- |
| RAM | 49512 B | 64 KB | 75.55% |
| Flash | 184676 B | 256 KB | 70.45% |

这是当前配置的链接统计，不是运行时栈峰值测量；更改工具链、优化选项或代码后会变化。

PC 主机测试使用本机 GCC，不使用 Arm 交叉编译器。仓库根目录执行以下命令，要求已有 cmake-build-debug 目录：

~~~powershell
gcc -std=c11 -Wall -Wextra -Werror -Itests/esp_com/stubs -IFIFO/Inc -IApiRefresh/Inc -IApiRefresh/ThirdParty/coreJSON tests/esp_com/test_esp_com.c FIFO/Src/Esp_Com.c LetterSh/Src/user_cmd.c ApiRefresh/Src/ApiRefresh.c ApiRefresh/Src/ApiTime.c ApiRefresh/Src/ApiRefresh_Shell.c ApiRefresh/ThirdParty/coreJSON/core_json.c -o cmake-build-debug/test_esp_com.exe
./cmake-build-debug/test_esp_com.exe
~~~

测试已通过，覆盖请求类型/参数/CRC、就绪状态拦截、HAL 返回值、默认 API、接收校验和长 JSON 输出，以及小数温度、无预警、中文一言和空作者等载荷的原样传输。测试源码不参与固件构建。

刷新库测试同时覆盖七类 API 的完整流程、各阶段超时、JSON 控制字段检查、远端失败保留、SEQ 匹配、esp_last 消费互不干扰、忙碌命令拦截以及空闲时不自动发送。Tick 由测试人工推进，不代表真实 TIM10 周期已经测量。

[MainUI 一言测试](tests/main_ui/README.md) 已通过，覆盖正文转义、作者回退、长度边界、括号和破折号、旧内容保留、重复轮询不重刷、面板唤醒与区域清理；测试使用显示替身，不验证实际外部字模或物理屏幕。

本次版本尚未完成更新后固件的全量上板回归。PC 测试不验证真实 UART 时序、ESP HTTPS 成功率、外部 Flash 数据或墨水屏刷新效果。

## 已知限制

- **界面仅一言接入实时数据。** 一言成功刷新后更新屏幕，天气等其他区域仍为示例；未启用周期刷新、时钟同步或待办管理。
- **接收没有完整帧队列。** 底层只保留最近一帧；ApiRefresh 用观察回调另存当前匹配响应，提供串行请求匹配和超时，但不自动重试，也不保存各类 API 的历史缓存。其他调用方需遵守串口占用约定。
- **刷新结果依赖 ESP 协议约定。** response 没有绑定刷新请求的事务 ID，8 位 SEQ 会回绕，无法完全排除同 API 旧结果或跨回绕的迟到响应；有效缓存也不保证观测时间最新。
- **长时间阻塞可能丢字节。** ESP RX FIFO 分配 1024 字节、实际容量 1023；2 Mbps 连续输入时理论上约 5.1 ms 填满。屏幕刷新、日志和延时需要与通信调度协调。当前无专用 UART 错误恢复或半帧超时处理。
- **载荷上限为 384 字节。** 没有分页/分片重组；分钟降雨和预警只返回可容纳的摘要，count 不一定等于 items 长度。
- **通用发送有参数前提。** EspCom_Send() 当前不拒绝 payload=NULL 且 len>0 的组合；调用方必须提供有效缓冲。API 名称直接拼入 JSON，不支持任意未转义输入。
- **资源写入仍有缺陷。** Uart_OTA_Rx() 的非法长度状态会被后续赋值覆盖，超长输入可能越界；写地址固定增加 256，与可变载荷长度不一致。另有 Fifo_Init 声明与 FIFO_Init 实现命名不一致的问题。
- **Flash 读取需进一步修正。** Flash::read() 当前将单字节 dummy 缓冲传给多字节 HAL_SPI_TransmitReceive()，存在发送缓冲越界读取；底层读写错误也未完整向上层传播。
- **实验模式不轮询 ESP。** flash_rw=1 时仍初始化 USART2 接收并拉高 STM32 就绪线，但主循环不处理 ESP FIFO。
- **USB CDC 仅有基础栈。** 接收回调重新启动接收，没有将数据转发到 Shell、ESP 或资源写入模块。

## 开发约定

- CubeMX 管理的源码尽量将自定义代码放在 USER CODE 区域，重新生成后检查差异。
- 根 CMakeLists.txt 标记为模板生成文件；新增自定义目录时，应同步维护 CMakeLists_template.txt 和当前构建注册项。
- 不在业务模块重复定义 flash、epd、mainUI，保持单一显示缓冲。
- 保留链接脚本中的 shellCommand 段及 KEEP，否则导出的 Shell 命令可能被链接优化删除。
- 更新 ESP 业务约定时同步通信 README、名称常量、Shell 支持列表和相关测试，明确 ESP 实现状态与 STM32 显示接入状态。

后续主要工作是完善通信可靠性、解析天气 JSON、建立其他显示数据模型，并把真实天气接入 MainUI。
