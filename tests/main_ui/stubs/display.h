#ifndef TEST_DISPLAY_H
#define TEST_DISPLAY_H

// Replace hardware-facing classes while compiling the real MainUI and C bridge.
#define TOPINFO_EPD_H
#define TOPINFO_QWEATHER_ICON_H
#define TOPINFO_PROJECT_H
#define TOPINFO_EPD_API_H
#define TEST_EPD_API_H
#include <cstdint>
#include <string>
#include <vector>
extern "C" {
#include "usart.h"
}
inline uint32_t testTick = 0;
inline HAL_StatusTypeDef testDisplayStatus = HAL_OK;
inline HAL_StatusTypeDef testPartialStatus = HAL_OK;
inline uint32_t HAL_GetTick() { return testTick; }
inline HAL_StatusTypeDef EPD_GetStatus() { return testDisplayStatus; }

enum FontSize { FONT_SIZE_16, FONT_SIZE_24 };
constexpr int EPD_WHITE = 1, EPD_BLACK = 0;
constexpr int QWEATHER_ICON_SIZE_16 = 16, QWEATHER_ICON_SIZE_32 = 32;
struct DrawText { int x, y; std::string text; FontSize size; };
struct DrawFill { int x, y, w, h, color; };
struct DrawIcon { std::string code; int size, x, y; };
inline std::vector<DrawIcon> testIcons;
inline bool testMissingIcon = false;
constexpr int QWEATHER_ICON_OK = 0;
class EPD {
public:
    std::vector<DrawText> texts;
    std::vector<DrawFill> fills;
    unsigned triangles = 0;
    std::vector<std::string> operations;
    bool asleep = true;
    bool baseReady = false;
    unsigned partialDisplays = 0;
    std::vector<DrawFill> partialWindows;
    unsigned displays = 0, quoteClears = 0;
    void init() { asleep = false; baseReady = false; operations.emplace_back("init"); }
    void clear() {}
    void refresh() { operations.emplace_back(asleep ? "invalid-asleep" : "display"); ++displays; baseReady = !asleep && testDisplayStatus == HAL_OK; }
    void sleep() { asleep = true; baseReady = false; operations.emplace_back("sleep"); }
    HAL_StatusTypeDef refreshPartial(int x, int y, int w, int h) {
        operations.emplace_back("partial"); ++displays; ++partialDisplays;
        partialWindows.push_back({x,y,w,h,0});
        if (testPartialStatus != HAL_OK) { testDisplayStatus = testPartialStatus; return testPartialStatus; }
        return baseReady && !asleep ? testDisplayStatus : HAL_ERROR;
    }
    void drawChineseString(int16_t x, int16_t y, const char *text, FontSize size) {
        texts.push_back({x, y, text, size});
    }
    void fillRect(int x, int y, int w, int h, int color) {
        fills.push_back({x, y, w, h, color});
        if (x == 0 && y == 268 && w == 400 && h == 32 && color == EPD_WHITE) {
            ++quoteClears;
            operations.emplace_back("clear-quote");
        }
    }
    void drawRect(int, int, int, int, int) {}
    void drawLine(int, int, int, int, int) {}
    void drawTriangle(int, int, int, int, int, int, int) { ++triangles; }
    void drawPixel(int, int, int) {}
};
class Flash {
public:
    void init() {}
    void test() {}
    void read(uint32_t, uint8_t *, uint16_t) {}
    void write(uint32_t, uint8_t *, uint16_t) {}
    void erase(uint32_t, uint16_t = 0) {}
};
#include "MainUI.h"
extern MainUI mainUI;
extern EPD epd;
inline bool EPD_CanRefreshPartial() { return epd.baseReady && !epd.asleep && testDisplayStatus == HAL_OK; }
extern Flash flash;
inline int QWeatherIcon_Draw(const char *code, int size, int x, int y) {
    testIcons.push_back({code, size, x, y});
    return testMissingIcon ? -4 : QWEATHER_ICON_OK;
}
inline void HAL_Delay(int) {}
extern "C" void EPD_UI_Poll(void);
extern "C" void EPD_UI_Refresh(void);

#endif
