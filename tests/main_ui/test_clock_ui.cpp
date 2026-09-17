#include "MainUI.h"
#include "LocalClock.h"
#include "SystemHeartbeat.h"
#include "ApiRefresh.h"
extern "C" {
#include "shell.h"
void EPD_HW_Sleep(void);
}
#include <cassert>
#include <cstring>
#include <iostream>

EPD epd;
Flash flash;
MainUI mainUI(epd);
Shell shell{};
static ApiRefresh_Result result{};
static bool busy;
static unsigned requests;
extern "C" const ApiRefresh_Result *ApiRefresh_GetResult() { return &result; }
extern "C" bool ApiRefresh_IsBusy() { return busy; }
extern "C" HAL_StatusTypeDef ApiRefresh_StartTime() {
    ++requests; busy = true; result = {}; result.state = API_REFRESH_WAIT_ACK;
    std::strcpy(result.api,"time"); return HAL_OK;
}
unsigned short shellWriteString(Shell *, const char *s) { return static_cast<unsigned short>(std::strlen(s)); }
static void advance(unsigned ms) {
    for (unsigned i = 0; i < ms; ++i) SystemHeartbeat_Tick1ms();
    testTick += ms;
    EPD_UI_Poll();
}
static void textAt(int x, int y, const std::string &text, FontSize size = FONT_SIZE_16) {
    for (auto it = epd.texts.rbegin(); it != epd.texts.rend(); ++it)
        if (it->x == x && it->y == y) { assert(it->text == text && it->size == size); return; }
    assert(false);
}
int main() {
    SystemHeartbeat_Init(); LocalClock_Init();
    EPD_UI_Refresh(); // 无有效时间不显示示例。
    textAt(250,6,"--:--",FONT_SIZE_24); textAt(322,2,u8"周--"); textAt(320,18,"----/--/--");
    assert(epd.asleep);
    LocalClock_Start(); assert(requests == 1);
    advance(5000); assert(epd.displays == 1);
    busy = false; result.state = API_REFRESH_SUCCEEDED; result.has_time = true;
    result.time.unix_time = 1798732799U; result.time.utc_offset = 28800;
    std::strcpy(result.time.date,"2026-12-31"); std::strcpy(result.time.time,"23:59:59"); result.time.weekday = 4;
    EPD_UI_Poll(); assert(epd.displays == 2 && !epd.asleep && epd.partialDisplays == 0);
    textAt(250,6,"23:59",FONT_SIZE_24); textAt(320,18,"2026-12-31"); textAt(322,2,u8"周四");
    const auto quoteClears = epd.quoteClears;
    advance(1000); assert(epd.displays == 2); // 跨年但仍遵守 5 秒显示间隔。
    advance(4000); assert(epd.displays == 3 && epd.partialDisplays == 1 && !epd.asleep);
    textAt(250,6,"00:00",FONT_SIZE_24); textAt(320,18,"2027-01-01"); textAt(322,2,u8"周五");
    assert(epd.quoteClears == quoteClears);
    const auto w = epd.partialWindows.back(); assert(w.x == 248 && w.y == 0 && w.w == 152 && w.h == 40);
    advance(10000); assert(epd.displays == 3); // 秒变不刷屏。
    busy = true; advance(120000); assert(epd.displays == 3);
    busy = false; EPD_UI_Poll(); assert(epd.displays == 4 && epd.partialDisplays == 2);
    textAt(250,6,"00:02",FONT_SIZE_24); // API 忙期间持续走时，只补刷最新分钟。
    for (unsigned i = 0; i < 28; ++i) advance(60000);
    assert(epd.partialDisplays == 30);
    unsigned fullBefore = epd.displays - epd.partialDisplays;
    advance(60000); assert(epd.displays - epd.partialDisplays == fullBefore + 1);
    EPD_HW_Sleep(); advance(60000);
    assert(epd.displays - epd.partialDisplays == fullBefore + 2 && !epd.asleep);
    // 手动取时响应不会重新校准已运行时钟，也不会产生周期网络请求。
    result.time.unix_time = 1; advance(60000);
    assert(requests == 1 && LocalClock_GetState() == LOCAL_CLOCK_RUNNING);
    LocalClock_Snapshot s{}; LocalClock_GetSnapshot(&s); assert(s.year == 2027);
    // 无效底图/显示故障下一分钟恢复全刷，不连续自动重试。
    testPartialStatus = HAL_TIMEOUT; advance(60000);
    const unsigned failed = epd.displays; advance(5000); assert(epd.displays == failed && epd.asleep);
    testPartialStatus = HAL_OK; testDisplayStatus = HAL_OK; advance(60000); assert(!epd.asleep);
    LocalClock_Init(); LocalClock_Start();
    result.state = API_REFRESH_FAILED; busy = false;
    advance(5000); textAt(250,6,"--:--",FONT_SIZE_24); assert(epd.asleep);
    const unsigned failures = requests, failedDisplays = epd.displays;
    advance(120000); assert(requests == failures && epd.displays == failedDisplays);
    std::cout << "Clock UI placeholders, minute partial refresh, rollover, pacing and recovery tests passed\n";
}
