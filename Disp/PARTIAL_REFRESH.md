# 局部刷新接口与实验

本接口面向当前 400×300 单色帧缓冲，沿用现有微雪 4.2 V2 风格的局刷命令。
用户提供的面板型号为 HINK-E042A13-A0（待实物确认）。当前验证覆盖主机端
SPI 命令/数据及固件构建；实际面板的残影、耗时和连续局刷效果仍需上板确认。

## C++ 接口

```cpp
epd.init();
epd.fillScreen(EPD_BLACK);
epd.refresh();                       // 全刷，建立局刷底图
if (EPD_GetStatus() == HAL_OK) {
    HAL_Delay(5000);                 // 全刷完成后留出观察时间
    epd.fillRect(80, 200, 200, 50, EPD_WHITE);
    HAL_StatusTypeDef status = epd.refreshPartial(80, 200, 200, 50);
    // 检查 status，保留这次局刷结果。
    (void)status;
}
epd.sleep();
```

- 参数为 `x, y, width, height`，单位像素；起点包含，右下边界不包含。
- X 起点与宽度必须为 8 的倍数；Y 和高度按像素设置。空区域、越界和
  非字节对齐区域返回 `HAL_ERROR`，不会裁剪或发送 SPI 命令。
- 局刷从唯一的整屏缓冲逐行取数，每行跨度固定为 50 字节，不分配第二帧缓冲。
- 绘图只改变 MCU 缓冲；缩短文字前应先用白色清理整个目标区域，再绘制新内容。
- 第一次局刷前须 `init()` 并成功执行 `refresh()` 或 `clear()`。休眠、重新初始化、
  SPI 失败或 BUSY 超时会使局刷底图失效，须重新初始化并全刷。
- `refresh()` 和 `clear()` 会恢复整屏窗口、游标和全刷控制配置，可直接用于局刷后全刷。
- 当前帧缓冲绘制未实现旋转映射；这些接口使用默认未旋转的物理坐标。
- 接口是阻塞调用，须在主循环使用，不在中断内调用；不支持并发访问。

## C 接口和底层入口

`EPD_HW_DisplayPartial(x, y, width, height)` 刷新同一份 `epd` 缓冲，不绘制内容、
不自动唤醒或休眠。ApiRefresh 忙碌时返回 `HAL_BUSY`。

`EPD_DisplayPartial(Image, x, y, width, height)` 要求 Image 指向完整的 15000 字节
单色帧缓冲。旧入口 `EPD_PartialDisplay(Image, Xstart, Ystart, Xend, Yend)` 保留
紧密排列的局部图像语义，X 起止均需字节对齐，终点不包含；Image 至少容纳
`(Xend-Xstart)/8 * (Yend-Ystart)` 字节。不要混用两种缓冲格式。

新接口返回 `HAL_OK/HAL_ERROR/HAL_BUSY/HAL_TIMEOUT`。已有 void 硬件接口可通过
`EPD_GetStatus()` 查询 SPI/BUSY 结果；参数校验失败以局刷接口的返回值为准。
BUSY 连续为高达到 15000 ms 会返回超时。SPI/BUSY 故障后停止发送刷新命令，
错误保持到下次初始化；休眠仍会尽力发送，但保留原始错误。

## Shell 实验

烧录本次固件后，在 USART1（2000000、8N1）执行：

```text
epd_partial_test
```

测试会覆盖当前显示：先全刷黑屏，等待 BUSY 结束，再等待 5 秒，随后仅将
`(80,200)` 的 200×50 区域局刷为白色一次。局刷结束后休眠，保留黑底白色矩形，
不循环刷新，也不追加全刷。测试不依赖字体、外部 Flash 或网络取时。
5 秒是本次实验的观察间隔，不代表该面板的最小刷新间隔。

日志包含黑色底图和单次局刷的状态，以及局刷调用总耗时 `elapsed_ms`。
耗时包括 SPI 传输和 BUSY 等待，不是单独的面板波形时长。状态 0=成功、1=错误、
2=忙、3=超时。SPI 返回成功及 BUSY 释放不能证明面板图像正确，需观察：

1. 先确认全屏均为黑色。
2. 五秒后，只有 x=80～279、y=200～249 的矩形变白，其余位置保持黑色。
3. 结果应保持不变，检查白色矩形是否有错行、偏移或残影。

ApiRefresh 忙碌时命令拒绝执行。屏幕驱动阻塞期间不轮询 ESP，测试时避免同时
发起网络请求或向串口持续灌入数据。恢复业务画面可执行 `ref_epd`。
本次没有将一言或时钟业务切换为局刷，也没有自动定时刷新。

## 参考

- [微雪 V2 驱动](https://github.com/waveshareteam/e-Paper/blob/master/RaspberryPi_JetsonNano/c/lib/e-Paper/EPD_4in2_V2.c)
- [微雪局刷测试](https://github.com/waveshareteam/e-Paper/blob/master/RaspberryPi_JetsonNano/c/examples/EPD_4in2_V2_test.c)
- [HINK-E042A13-A0 社区讨论](https://github.com/ZinggJM/GxEPD2/discussions/107)
