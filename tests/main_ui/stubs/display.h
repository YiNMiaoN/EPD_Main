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

enum FontSize { FONT_SIZE_16, FONT_SIZE_24 };
constexpr int EPD_WHITE = 1, EPD_BLACK = 0;
constexpr int QWEATHER_ICON_SIZE_16 = 16, QWEATHER_ICON_SIZE_32 = 32;
struct DrawText { int x, y; std::string text; FontSize size; };
class EPD {
public:
    std::vector<DrawText> texts;
    std::vector<std::string> operations;
    bool asleep = true;
    unsigned displays = 0, quoteClears = 0;
    void init() { asleep = false; operations.emplace_back("init"); }
    void clear() {}
    void refresh() { operations.emplace_back(asleep ? "invalid-asleep" : "display"); ++displays; }
    void sleep() { asleep = true; operations.emplace_back("sleep"); }
    void drawChineseString(int16_t x, int16_t y, const char *text, FontSize size) {
        texts.push_back({x, y, text, size});
    }
    void fillRect(int x, int y, int w, int h, int color) {
        if (x == 0 && y == 268 && w == 400 && h == 32 && color == EPD_WHITE) {
            ++quoteClears;
            operations.emplace_back("clear-quote");
        }
    }
    void drawRect(int, int, int, int, int) {}
    void drawLine(int, int, int, int, int) {}
    void drawTriangle(int, int, int, int, int, int, int) {}
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
extern Flash flash;
inline void QWeatherIcon_Draw(const char *, int, int, int) {}
inline void HAL_Delay(int) {}
extern "C" void EPD_UI_Poll(void);
extern "C" void EPD_UI_Refresh(void);

#endif
