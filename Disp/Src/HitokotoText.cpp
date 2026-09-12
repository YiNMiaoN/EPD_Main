#include "HitokotoText.h"
#include "core_json.h"
#include <cstdint>
#include <cstring>

namespace {
bool keyIs(const JSONPair_t &pair, const char *key)
{
    return pair.key && pair.keyLength == std::strlen(key) &&
           std::memcmp(pair.key, key, pair.keyLength) == 0;
}

uint32_t hex4(const char *p)
{
    uint32_t value = 0;
    for (unsigned i = 0; i < 4; ++i) {
        const unsigned char c = p[i];
        value = (value << 4) | (c <= '9' ? c - '0' : (c | 32U) - 'a' + 10U);
    }
    return value;
}

// coreJSON validates syntax; its string slices still contain JSON escapes.
bool nextCharacter(const char *&p, const char *end, uint32_t &cp)
{
    const auto first = static_cast<unsigned char>(*p++);
    if (first == '\\') {
        if (p == end) return false;
        switch (*p++) {
            case '"': cp = '"'; return true;
            case '\\': cp = '\\'; return true;
            case '/': cp = '/'; return true;
            case 'b': case 'f': case 'n': case 'r': case 't': cp = ' '; return true;
            case 'u':
                if (end - p < 4) return false;
                cp = hex4(p);
                p += 4;
                if (cp >= 0xD800 && cp <= 0xDBFF) {
                    if (end - p < 6 || p[0] != '\\' || p[1] != 'u') return false;
                    const uint32_t low = hex4(p + 2);
                    if (low < 0xDC00 || low > 0xDFFF) return false;
                    cp = 0x10000 + ((cp - 0xD800) << 10) + low - 0xDC00;
                    p += 6;
                } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
                    return false;
                }
                return true;
            default: return false;
        }
    }
    if (first < 0x80) {
        cp = first;
        return true;
    }
    const unsigned extra = first >= 0xF0 ? 3U : first >= 0xE0 ? 2U : 1U;
    if (first < 0xC2 || first > 0xF4 || static_cast<std::size_t>(end - p) < extra) return false;
    cp = first & (0x7FU >> (extra + 1));
    for (unsigned i = 0; i < extra; ++i) {
        const auto c = static_cast<unsigned char>(*p++);
        if ((c & 0xC0) != 0x80) return false;
        cp = (cp << 6) | (c & 0x3F);
    }
    const uint32_t minimum[] = {0, 0x80, 0x800, 0x10000};
    return cp >= minimum[extra] && cp <= 0x10FFFF && !(cp >= 0xD800 && cp <= 0xDFFF);
}

std::size_t encode(uint32_t cp, char *out)
{
    if (cp < 0x80) { out[0] = static_cast<char>(cp); return 1; }
    const std::size_t count = cp < 0x800 ? 2 : cp < 0x10000 ? 3 : 4;
    for (std::size_t i = count - 1; i > 0; --i) {
        out[i] = static_cast<char>(0x80 | (cp & 0x3F));
        cp >>= 6;
    }
    out[0] = static_cast<char>((count == 2 ? 0xC0 : count == 3 ? 0xE0 : 0xF0) | cp);
    return count;
}

bool formatField(const JSONPair_t &pair, char *out, std::size_t capacity, int maxPixels)
{
    out[0] = '\0';
    if (pair.jsonType == JSONInvalid || pair.jsonType == JSONNull) return true;
    if (pair.jsonType != JSONString) return false;
    const char *p = pair.value, *end = p + pair.valueLength;
    std::size_t used = 0;
    int pixels = 0;
    bool truncated = false;
    while (p < end) {
        uint32_t cp;
        if (!nextCharacter(p, end, cp)) return false;
        if (cp < 0x20 || cp == 0x7F || cp == 0x2028 || cp == 0x2029) cp = ' ';
        char bytes[4];
        const std::size_t count = encode(cp, bytes);
        const int width = cp < 0x80 ? 8 : 16;
        if (truncated || pixels + width > maxPixels || used + count >= capacity) {
            truncated = true;
            continue;
        }
        std::memcpy(out + used, bytes, count);
        used += count;
        pixels += width;
    }
    if (truncated) {
        while (used && (pixels + 16 > maxPixels || used + 3 >= capacity)) {
            std::size_t start = used - 1;
            while (start && (static_cast<unsigned char>(out[start]) & 0xC0) == 0x80) --start;
            pixels -= used - start == 1 ? 8 : 16;
            used = start;
        }
        if (used + 3 >= capacity) return false;
        std::memcpy(out + used, u8"\u2026", 3);
        used += 3;
    }
    out[used] = '\0';
    return true;
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
    if (!formatField(quote, candidate.quote + 3, sizeof(candidate.quote) - 6, 368) ||
        !hasText(candidate.quote + 3)) return false;
    std::memcpy(candidate.quote, u8"\u300c", 3);
    std::strcat(candidate.quote, u8"\u300d");
    if (!formatField(author, candidate.attribution + 6, sizeof(candidate.attribution) - 6, 368)) return false;
    if (!hasText(candidate.attribution + 6) &&
        !formatField(source, candidate.attribution + 6, sizeof(candidate.attribution) - 6, 368)) return false;
    if (!hasText(candidate.attribution + 6)) std::strcpy(candidate.attribution + 6, u8"\u4f5a\u540d");
    std::memcpy(candidate.attribution, u8"\u2014\u2014", 6);
    text = candidate;
    return true;
}
