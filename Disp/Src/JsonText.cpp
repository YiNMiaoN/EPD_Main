#include "JsonText.h"
#include <cstdint>
#include <cstring>

namespace {
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

} // namespace

bool JsonText_Format(const JSONPair_t &pair, char *out, std::size_t capacity, int maxPixels)
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

