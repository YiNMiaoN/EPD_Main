//
// Created by YinMi on 2026/5/25.
//

#include "Font.h"

#include <cstring>

Flash flash;

uint16_t unicodeToGBK(uint16_t unicode)
{
    int32_t left = 0;
    int32_t right = UNICODE_GBK_MAP_COUNT - 1;
    while (left <= right) {
        int32_t mid = (left + right) / 2;
        uint32_t item = unicode_gbk_map[mid];
        uint16_t u = (uint16_t)(item >> 16);
        uint16_t g = (uint16_t)(item & 0xFFFF);
        if (unicode == u) {
            return g;
        } else if (unicode < u) {
            right = mid - 1;
        } else {
            left = mid + 1;
        }
    }
    return 0x0000;
}


uint32_t utf8Next(const char** p)
{
    const uint8_t* s = (const uint8_t*)(*p);

    if (s[0] == 0) {
        return 0;
    }

    // ASCII
    if (s[0] < 0x80) {
        (*p) += 1;
        return s[0];
    }

    // 2 字节 UTF-8
    if ((s[0] & 0xE0) == 0xC0) {
        uint32_t code =
            ((uint32_t)(s[0] & 0x1F) << 6) |
            ((uint32_t)(s[1] & 0x3F));

        (*p) += 2;
        return code;
    }

    // 3 字节 UTF-8，常用中文基本都在这里
    if ((s[0] & 0xF0) == 0xE0) {
        uint32_t code =
            ((uint32_t)(s[0] & 0x0F) << 12) |
            ((uint32_t)(s[1] & 0x3F) << 6) |
            ((uint32_t)(s[2] & 0x3F));

        (*p) += 3;
        return code;
    }

    // 4 字节 UTF-8，GBK 一般不支持，跳过
    if ((s[0] & 0xF8) == 0xF0) {
        uint32_t code =
            ((uint32_t)(s[0] & 0x07) << 18) |
            ((uint32_t)(s[1] & 0x3F) << 12) |
            ((uint32_t)(s[2] & 0x3F) << 6) |
            ((uint32_t)(s[3] & 0x3F));

        (*p) += 4;
        return code;
    }

    // 非法字节，跳过
    (*p) += 1;
    return 0xFFFFFFFFUL;
}

uint32_t gbkIndex(uint16_t gbk)
{
    uint8_t high = gbk >> 8;
    uint8_t low  = gbk & 0xFF;

    if (high < GBK_HIGH_START || high > GBK_HIGH_END) {
        return 0xFFFFFFFFUL;
    }

    if (low < GBK_LOW_START || low > GBK_LOW_END) {
        return 0xFFFFFFFFUL;
    }

    return (uint32_t)(high - GBK_HIGH_START) * GBK_SLOTS_PER_HIGH
         + (uint32_t)(low - GBK_LOW_START);
}



uint32_t gbkFontOffset(uint16_t gbk, FontSize size)
{
    uint32_t index = gbkIndex(gbk);

    if (index == 0xFFFFFFFFUL) {
        return 0xFFFFFFFFUL;
    }

    if (size == FONT_SIZE_16) {
        return FONT_GBK16_OFFSET + index * GBK16_FONT_BYTES;
    } else if (size == FONT_SIZE_24) {
        return FONT_GBK24_OFFSET + index * GBK24_FONT_BYTES;
    }

    return 0xFFFFFFFFUL;
}

uint16_t gbkFontBytes(FontSize size)
{
    if (size == FONT_SIZE_16) {
        return GBK16_FONT_BYTES;
    } else if (size == FONT_SIZE_24) {
        return GBK24_FONT_BYTES;
    }

    return 0;
}

uint32_t asciiFontOffset(uint8_t ascii, FontSize size)
{
    if (ascii < ASCII_START || ascii > ASCII_END) {
        return 0xFFFFFFFFUL;
    }

    uint32_t index = ascii - ASCII_START;

    if (size == FONT_SIZE_16) {
        return FONT_ASCII16_OFFSET + index * ASCII16_FONT_BYTES;
    } else if (size == FONT_SIZE_24) {
        return FONT_ASCII24_OFFSET + index * ASCII24_FONT_BYTES;
    }

    return 0xFFFFFFFFUL;
}

uint16_t asciiFontBytes(FontSize size)
{
    if (size == FONT_SIZE_16) {
        return ASCII16_FONT_BYTES;
    } else if (size == FONT_SIZE_24) {
        return ASCII24_FONT_BYTES;
    }

    return 0;
}

bool readASCIIBitmap(uint8_t ascii, FontSize size, uint8_t* fontbitmap)
{
    uint32_t offset = asciiFontOffset(ascii, size);
    uint16_t bytes = asciiFontBytes(size);

    if (offset == 0xFFFFFFFFUL || bytes == 0) {
        return false;
    }

    flash.read(offset, fontbitmap, bytes);

    return true;
}

bool readGBKBitmap(uint16_t gbk, FontSize size, uint8_t* fontbitmap)
{
    uint32_t offset = gbkFontOffset(gbk, size);
    uint16_t bytes = gbkFontBytes(size);

    if (offset == 0xFFFFFFFFUL || bytes == 0) {
        return false;
    }

    flash.read(offset, fontbitmap, bytes);

    return true;
}