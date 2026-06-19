//
// Created by YinMi on 2026/5/25.
//

#ifndef TOPINFO_FONT_H
#define TOPINFO_FONT_H

#include "stm32f4xx_hal.h"
#include "unicode_gbk_map.h"
#include "Flash.h"

#define FONT_GBK16_OFFSET      0x00000000UL
#define FONT_GBK24_OFFSET      0x000BC040UL
#define FONT_ASCII16_OFFSET    0x002630D0UL
#define FONT_ASCII24_OFFSET    0x002636C0UL

#define GBK16_FONT_WIDTH       16
#define GBK16_FONT_HEIGHT      16
#define GBK16_FONT_BYTES       32

#define GBK24_FONT_WIDTH       24
#define GBK24_FONT_HEIGHT      24
#define GBK24_FONT_BYTES       72

#define ASCII16_FONT_WIDTH     8
#define ASCII16_FONT_HEIGHT    16
#define ASCII16_FONT_BYTES     16

#define ASCII24_FONT_WIDTH     12
#define ASCII24_FONT_HEIGHT    24
#define ASCII24_FONT_BYTES     48

#define GBK_HIGH_START         0x81
#define GBK_HIGH_END           0xFE
#define GBK_LOW_START          0x40
#define GBK_LOW_END            0xFE
#define GBK_SLOTS_PER_HIGH     191

#define ASCII_START            0x20
#define ASCII_END              0x7E


typedef enum {
    FONT_SIZE_16 = 16,
    FONT_SIZE_24 = 24,
} FontSize;


uint16_t unicodeToGBK(uint16_t unicode);
uint32_t utf8Next(const char** p);
bool readASCIIBitmap(uint8_t ascii, FontSize size, uint8_t* fontbitmap);
bool readGBKBitmap(uint16_t gbk, FontSize size, uint8_t* fontbitmap);

#endif //TOPINFO_FONT_H