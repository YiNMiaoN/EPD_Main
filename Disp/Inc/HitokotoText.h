#ifndef TOPINFO_HITOKOTO_TEXT_H
#define TOPINFO_HITOKOTO_TEXT_H

#include <cstddef>

struct HitokotoText {
    char quote[104] = u8"\u300c\u884c\u52a8\u8d8a\u5feb\uff0c\u75db\u82e6\u8d8a\u5c11\u3002\u300d";
    char attribution[104] = u8"\u2014\u2014\u5207\u5229\u5c3c\u5a1c\u00b7\u5fb7\u514b\u8428\u65af";

    int attributionWidth() const;
};

// Parse and format a validated hitokoto cache for the existing two 16px rows.
// Invalid data leaves text unchanged. Long fields end with an ellipsis.
bool HitokotoText_Parse(const char *json, std::size_t length, HitokotoText &text);

#endif
