//
// Created by YinMi on 2026/7/15.
//

#ifndef TOPINFO_MAINUI_H
#define TOPINFO_MAINUI_H
#include "Epd.h"

class MainUI : EPD{
    public:
        MainUI();
        ~MainUI();
        void drawMainUI();

        void updata_ui();

        void refresh();

    private:
        void drawTime();
        void drawList();

        void drawNowWeather();

        void drawHitokoto();
};

#endif //TOPINFO_MAINUI_H