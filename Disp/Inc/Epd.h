//
// Created by YinMi on 2026/5/4.
//

#ifndef TOPINFO_EPD_H
#define TOPINFO_EPD_H
#include "EPD_Hardware.h"
#include "Adafruit_GFX.h"
#include "Font.h"


class EPD : public Adafruit_GFX{
protected:
    uint8_t EPD_GRam[EPD_WIDTH * EPD_HEIGHT / 8]={};


public:

    EPD();
    ~EPD();
    void drawPixel(int16_t x, int16_t y, uint16_t color) override;
    void init();
    void clear();

    void drawGBK24(uint16_t gbk, int16_t x, int16_t y);

    void drawChineseString(int16_t x, int16_t y, const char* text, FontSize size);

    void drawFontBitmap(int16_t x, int16_t y, const uint8_t* fontbitmap, uint8_t width, uint8_t height);


    void updata_ui();

    void refresh();

    void sleep();
};





#endif //TOPINFO_EPD_H