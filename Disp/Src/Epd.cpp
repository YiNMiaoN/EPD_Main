//
// Created by YinMi on 2026/5/4.
//

#include "Epd.h"
#include "Flash.h"
#include "unicode_gbk_map.h"
#include "Font.h"


EPD::EPD()
    : Adafruit_GFX(EPD_WIDTH, EPD_HEIGHT){
    for (int i = 0; i < EPD_WIDTH * EPD_HEIGHT / 8; i++) {
        EPD_GRam[i] = 0xFF;
    }
}

EPD::~EPD() {

}

void EPD::drawPixel(int16_t x, int16_t y, uint16_t color)  {
    if(x < 0 || x >= width() ||
   y < 0 || y >= height())
        return;

    uint32_t index = (x + y * width()) / 8;
    uint8_t  mask  = 0x80 >> (x % 8);

    if(color == EPD_BLACK)
        EPD_GRam[index] &= ~mask;
    else
        EPD_GRam[index] |= mask;
}

void EPD::init() {
    EPD_Init();
}

void EPD::clear() {
    EPD_Clear();
    HAL_Delay(10);
}

void EPD::drawFontBitmap(int16_t x, int16_t y, const uint8_t* fontbitmap, uint8_t width, uint8_t height)
{
    drawBitmap(x, y, fontbitmap, width, height, 0);
}


void EPD::drawChineseString(int16_t x, int16_t y, const char* text, FontSize size)
{
    const char* p = text;
    uint8_t bitmap[72];   // 最大字模是 GBK24，72 字节，所以这里够用

    uint8_t gbkWidth;
    uint8_t gbkHeight;
    uint8_t asciiWidth;
    uint8_t asciiHeight;

    if (size == FONT_SIZE_16) {
        gbkWidth = 16;
        gbkHeight = 16;
        asciiWidth = 8;
        asciiHeight = 16;
    } else {
        gbkWidth = 24;
        gbkHeight = 24;
        asciiWidth = 12;
        asciiHeight = 24;
    }

    while (*p) {
        uint32_t unicode = utf8Next(&p);

        if (unicode == 0) {
            break;
        }

        if (unicode == 0xFFFFFFFFUL) {
            continue;
        }

        // ASCII
        if (unicode < 0x80) {
            if (readASCIIBitmap((uint8_t)unicode, size, bitmap)) {
                drawFontBitmap(x, y, bitmap, asciiWidth, asciiHeight);
            }

            x += asciiWidth;
            continue;
        }

        // 4 字节 Unicode，GBK 不支持
        if (unicode > 0xFFFF) {
            x += gbkWidth;
            continue;
        }

        uint16_t gbk = unicodeToGBK((uint16_t)unicode);

        if (gbk == 0x0000) {
            x += gbkWidth;
            continue;
        }

        if (readGBKBitmap(gbk, size, bitmap)) {
            drawFontBitmap(x, y, bitmap, gbkWidth, gbkHeight);
        }

        x += gbkWidth;
    }
}


void EPD::updata_ui() {
    drawChineseString(0,0,"中",FONT_SIZE_24);
    drawChineseString(0,24,"是一种基于堆数据结构实现的高效AAA算法",FONT_SIZE_16);
}


void EPD::refresh() {
    updata_ui();
    HAL_Delay(10);
    EPD_Display(EPD_GRam);
}

void EPD::sleep() {

}

