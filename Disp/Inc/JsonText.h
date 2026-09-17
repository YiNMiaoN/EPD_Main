#ifndef TOPINFO_JSON_TEXT_H
#define TOPINFO_JSON_TEXT_H
#include "core_json.h"
#include <cstddef>

// 解码已通过 coreJSON 语法校验的字段，按 16 像素字体字宽裁剪。
bool JsonText_Format(const JSONPair_t &pair, char *out, std::size_t capacity, int maxPixels);
#endif
