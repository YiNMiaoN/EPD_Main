//
// Created by YinMi on 2026/7/15.
//

#ifndef TOPINFO_MAINUI_H
#define TOPINFO_MAINUI_H
#include "Epd.h"
#include "QWeather_Icon.h"

class MainUI {
    public:
        explicit MainUI(EPD &display);
        ~MainUI();
        void drawMainUI();

        void updata_ui();

        void refresh();

    private:
        void drawTime();
        void drawList();

        void drawNowWeather();

        void drawHitokoto();

        void drawChineseString(int16_t x, int16_t y, const char *text, FontSize size);
        void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color);

        EPD &epd;
};

#endif //TOPINFO_MAINUI_H
