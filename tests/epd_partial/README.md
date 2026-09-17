# EPD 局刷主机测试

在仓库根目录使用原生 G++（不是 Arm 编译器）运行：

```powershell
g++ -std=c++17 -Wall -Wextra -Werror -Itests/epd_partial/stubs -IDisp/Inc tests/epd_partial/test_partial.cpp Disp/Src/EPD_Hardware.cpp -o cmake-build-debug/test_epd_partial.exe
./cmake-build-debug/test_epd_partial.exe
```

测试编译真实的 `EPD_Hardware.cpp`，通过 HAL 替身捕获 SPI 命令和数据，验证：

- 首次全刷前和休眠后拒绝局刷。
- 非零起点窗口从整屏缓冲按 50 字节跨度逐行取数，连续刷新不复位。
- 右下角最后一个字节、Y 高位、非法范围及未对齐参数。
- 旧局部紧密缓冲入口和整屏缓冲入口的数据格式。
- 局刷后直接全刷或清屏，恢复窗口、控制寄存器及两个 RAM 平面的游标。
- BUSY 常高的超时、毫秒计数回绕、SPI 中途失败后不触发刷新，以及故障恢复。

这些测试不模拟墨水屏内部 RAM 自动同步或波形，不证明物理显示效果。
上板步骤见 [局刷说明](../../Disp/PARTIAL_REFRESH.md)。
