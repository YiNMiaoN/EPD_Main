#include "HitokotoText.h"
#include "JsonText.h"
#include "core_json.h"
#include <cstdint>
#include <cstring>

namespace {
bool keyIs(const JSONPair_t &pair, const char *key)
{
    return pair.key && pair.keyLength == std::strlen(key) &&
           std::memcmp(pair.key, key, pair.keyLength) == 0;
}

bool hasText(const char *text)
{
    while (*text == ' ') ++text;
    return *text != '\0';
}
}

int HitokotoText::attributionWidth() const
{
    int pixels = 0;
    // Match EPD::drawChineseString at FONT_SIZE_16 for validated UTF-8.
    for (const auto *p = reinterpret_cast<const unsigned char *>(attribution); *p; ++p) {
        if (*p < 0x80) pixels += 8;
        else if ((*p & 0xC0) != 0x80) pixels += 16;
    }
    return pixels;
}

bool HitokotoText_Parse(const char *json, std::size_t length, HitokotoText &text)
{
    if (!json || JSON_Validate(json, length) != JSONSuccess) return false;
    JSONPair_t quote{}, author{}, source{}, pair{};
    std::size_t start = 0, next = 0;
    unsigned seen = 0;
    bool apiMatches = false, valid = false;
    JSONStatus_t status;
    while ((status = JSON_Iterate(json, length, &start, &next, &pair)) == JSONSuccess) {
        if (!pair.key) return false;
        unsigned bit = 0;
        if (keyIs(pair, "api")) {
            bit = 1;
            apiMatches = pair.jsonType == JSONString && pair.valueLength == 8 &&
                         std::memcmp(pair.value, "hitokoto", 8) == 0;
        } else if (keyIs(pair, "valid")) {
            bit = 2;
            valid = pair.jsonType == JSONTrue;
        } else if (keyIs(pair, "hitokoto")) {
            bit = 4;
            quote = pair;
        } else if (keyIs(pair, "from_who")) {
            bit = 8;
            author = pair;
        } else if (keyIs(pair, "from")) {
            bit = 16;
            source = pair;
        }
        if (seen & bit) return false;
        seen |= bit;
    }
    if (status != JSONNotFound || !apiMatches || !valid || quote.jsonType != JSONString) return false;

    HitokotoText candidate;
    // Reserve 32px for the quote brackets and for the two attribution dashes.
    if (!JsonText_Format(quote, candidate.quote + 3, sizeof(candidate.quote) - 6, 368) ||
        !hasText(candidate.quote + 3)) return false;
    std::memcpy(candidate.quote, u8"\u300c", 3);
    std::strcat(candidate.quote, u8"\u300d");
    if (!JsonText_Format(author, candidate.attribution + 6, sizeof(candidate.attribution) - 6, 368)) return false;
    if (!hasText(candidate.attribution + 6) &&
        !JsonText_Format(source, candidate.attribution + 6, sizeof(candidate.attribution) - 6, 368)) return false;
    if (!hasText(candidate.attribution + 6)) std::strcpy(candidate.attribution + 6, u8"\u4f5a\u540d");
    std::memcpy(candidate.attribution, u8"\u2014\u2014", 6);
    text = candidate;
    return true;
}
