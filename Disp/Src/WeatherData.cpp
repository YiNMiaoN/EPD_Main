#include "WeatherData.h"
#include "JsonText.h"
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

namespace {
bool equal(const char *p, std::size_t n, const char *expected)
{
    return p && n == std::strlen(expected) && std::memcmp(p, expected, n) == 0;
}

template<std::size_t N>
bool fields(const char *json, std::size_t length, const char *const (&keys)[N], JSONPair_t (&out)[N])
{
    std::size_t start = 0, next = 0;
    JSONPair_t pair{};
    JSONStatus_t status;
    while ((status = JSON_Iterate(json, length, &start, &next, &pair)) == JSONSuccess) {
        if (!pair.key) return false;
        for (std::size_t i = 0; i < N; ++i) {
            if (!equal(pair.key, pair.keyLength, keys[i])) continue;
            if (out[i].value) return false;
            out[i] = pair;
            break;
        }
    }
    return status == JSONNotFound;
}

bool text(const JSONPair_t &pair, char *out, std::size_t capacity, int pixels, bool allowEmpty = false)
{
    if (pair.jsonType != JSONString || !JsonText_Format(pair, out, capacity, pixels)) return false;
    const char *p = out;
    while (*p == ' ') ++p;
    return allowEmpty || *p;
}

bool code(const JSONPair_t &pair, char *out)
{
    // 图标包使用未转义的 ASCII 编号；拒绝被裁剪的图标码。
    if (pair.jsonType != JSONString || !pair.valueLength || pair.valueLength > 15) return false;
    for (std::size_t i = 0; i < pair.valueLength; ++i)
        if (pair.value[i] < '0' || pair.value[i] > '9') return false;
    std::memcpy(out, pair.value, pair.valueLength);
    out[pair.valueLength] = 0;
    return true;
}

bool number(const JSONPair_t &pair, JSONTypes_t type, double low, double high, double &out)
{
    if (pair.jsonType != type || !pair.valueLength || pair.valueLength >= 48) return false;
    // 字符串数值也必须符合 JSON 十进制数语法，拒绝空白、单位和十六进制。
    char buffer[48];
    std::memcpy(buffer, pair.value, pair.valueLength);
    buffer[pair.valueLength] = 0;
    if ((buffer[0] != '-' && (buffer[0] < '0' || buffer[0] > '9')) ||
        JSON_Validate(buffer, pair.valueLength) != JSONSuccess) return false;
    for (std::size_t i = 0; i < pair.valueLength; ++i)
        if (buffer[i] == ' ' || buffer[i] == '\t' || buffer[i] == '\r' || buffer[i] == '\n') return false;
    char *end = nullptr;
    const double value = std::strtod(buffer, &end);
    if (end != buffer + pair.valueLength || !std::isfinite(value) || value < low || value > high) return false;
    out = value;
    return true;
}

bool integer(const JSONPair_t &pair, unsigned maximum, unsigned &out)
{
    if (pair.jsonType != JSONNumber || !pair.valueLength) return false;
    unsigned value = 0;
    for (std::size_t i = 0; i < pair.valueLength; ++i) {
        unsigned digit = static_cast<unsigned char>(pair.value[i]) - '0';
        if (digit > 9 || value > maximum / 10 ||
            (value == maximum / 10 && digit > maximum % 10)) return false;
        value = value * 10 + digit;
    }
    out = value;
    return true;
}

unsigned two(const char *s) { return (s[0] - '0') * 10U + s[1] - '0'; }
bool date(const char *s, std::size_t n)
{
    if (n != 10) return false;
    for (unsigned i = 0; i < 10; ++i) {
        if (i == 4 || i == 7) { if (s[i] != '-') return false; }
        else if (s[i] < '0' || s[i] > '9') return false;
    }
    const unsigned year = two(s) * 100 + two(s + 2), month = two(s + 5), day = two(s + 8);
    static const unsigned days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    if (!year || !month || month > 12 || !day) return false;
    const bool leap = year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
    return day <= days[month - 1] + (month == 2 && leap ? 1U : 0U);
}

bool timestamp(const JSONPair_t &pair, char *out)
{
    if (pair.jsonType != JSONString) return false;
    const char *s = pair.value;
    const std::size_t n = pair.valueLength;
    if (n < 17 || n > 25 || !date(s, 10) || s[10] != 'T' || s[13] != ':') return false;
    for (unsigned i : {11U,12U,14U,15U}) if (s[i] < '0' || s[i] > '9') return false;
    if (two(s + 11) > 23 || two(s + 14) > 59) return false;
    std::size_t zone = 16;
    if (s[zone] == ':') {
        if (n < 20 || s[17] < '0' || s[17] > '9' || s[18] < '0' || s[18] > '9' || two(s + 17) > 59) return false;
        zone = 19;
    }
    if (s[zone] == 'Z') { if (n != zone + 1) return false; }
    else {
        if (n != zone + 6 || (s[zone] != '+' && s[zone] != '-') || s[zone + 3] != ':') return false;
        for (unsigned i : {1U,2U,4U,5U}) if (s[zone+i] < '0' || s[zone+i] > '9') return false;
        if (two(s + zone + 1) > 23 || two(s + zone + 4) > 59) return false;
    }
    std::memcpy(out, s, n);
    out[n] = 0;
    return true;
}

bool parseCurrent(const char *json, std::size_t n, WeatherCurrent &out)
{
    const char *const keys[] = {"temp", "humidity", "icon"};
    JSONPair_t f[3]{};
    if (!fields(json, n, keys, f) || !number(f[0], JSONString, -100, 100, out.temperature) ||
        !number(f[1], JSONString, 0, 100, out.humidity) || !code(f[2], out.icon)) return false;
    out.valid = true;
    return true;
}

bool parseDaily(const JSONPair_t &items, unsigned count, WeatherDaily &out)
{
    if (items.jsonType != JSONArray || !count || count > 3) return false;
    std::size_t start = 0, next = 0;
    unsigned index = 0;
    JSONPair_t item{};
    JSONStatus_t status;
    while ((status = JSON_Iterate(items.value, items.valueLength, &start, &next, &item)) == JSONSuccess) {
        if (index >= count || item.jsonType != JSONObject) return false;
        const char *const keys[] = {"date", "temp_max", "temp_min", "text", "icon"};
        JSONPair_t f[5]{};
        WeatherDay &d = out.days[index];
        char unusedText[64];
        if (!fields(item.value, item.valueLength, keys, f) || f[0].jsonType != JSONString ||
            !date(f[0].value, f[0].valueLength) || !number(f[1], JSONNumber, -100, 100, d.high) ||
            !number(f[2], JSONNumber, -100, 100, d.low) || d.high < d.low ||
            !text(f[3], unusedText, sizeof(unusedText), 144) || !code(f[4], d.icon)) return false;
        std::memcpy(d.date, f[0].value, 10);
        if (index && std::strcmp(out.days[index-1].date, d.date) >= 0) return false;
        ++index;
    }
    if (status != JSONNotFound || index != count) return false;
    out.count = count;
    out.valid = true;
    return true;
}

bool parseAlert(const JSONPair_t &items, unsigned count, WeatherAlert &out)
{
    if (items.jsonType != JSONArray) return false;
    std::size_t start = 0, next = 0;
    unsigned index = 0;
    JSONPair_t item{};
    JSONStatus_t status;
    while ((status = JSON_Iterate(items.value, items.valueLength, &start, &next, &item)) == JSONSuccess) {
        if (index >= count || item.jsonType != JSONObject) return false;
        const char *const keys[] = {"id", "title", "type", "type_name", "severity_color"};
        JSONPair_t f[5]{};
        char unusedText[64], icon[16]{}, name[16]{}, color[64]{};
        if (!fields(item.value, item.valueLength, keys, f) ||
            !text(f[0], unusedText, sizeof(unusedText), 144) ||
            !text(f[1], unusedText, sizeof(unusedText), 144) || !code(f[2], icon) ||
            !text(f[3], name, sizeof(name), 32) || !text(f[4], color, sizeof(color), 144, true)) return false;
        if (index == 0) {
            std::strcpy(out.icon, icon);
            std::strcpy(out.name, name);
            const char *mapped = nullptr;
            if (!std::strcmp(color, "Blue") || !std::strcmp(color, "blue")) mapped = u8"蓝色";
            else if (!std::strcmp(color, "Yellow") || !std::strcmp(color, "yellow")) mapped = u8"黄色";
            else if (!std::strcmp(color, "Orange") || !std::strcmp(color, "orange")) mapped = u8"橙色";
            else if (!std::strcmp(color, "Red") || !std::strcmp(color, "red")) mapped = u8"红色";
            if (mapped) std::strcpy(out.color, mapped);
            else if (!text(f[4], out.color, sizeof(out.color), 32, true)) return false;
            if (!out.color[0]) std::strcpy(out.color, "--");
        }
        ++index;
    }
    if (status != JSONNotFound || (count && !index)) return false;
    out.count = count;
    out.valid = true;
    return true;
}

bool parseRain(const char *json, std::size_t n, unsigned count, WeatherRain &out)
{
    const char *const keys[] = {"start_time", "interval_minutes", "precip", "total_precip"};
    JSONPair_t f[4]{};
    unsigned interval;
    if (count != 24 || !fields(json, n, keys, f) || !timestamp(f[0], out.startTime) ||
        !integer(f[1], 5, interval) || interval != 5 || f[2].jsonType != JSONArray ||
        !number(f[3], JSONNumber, 0, 1000000, out.total)) return false;
    std::size_t start = 0, next = 0;
    unsigned index = 0;
    double sum = 0;
    JSONPair_t item{};
    JSONStatus_t status;
    while ((status = JSON_Iterate(f[2].value, f[2].valueLength, &start, &next, &item)) == JSONSuccess) {
        if (index >= 24 || !number(item, JSONNumber, 0, 1000000, out.precip[index])) return false;
        sum += out.precip[index++];
    }
    if (status != JSONNotFound || index != 24 ||
        std::fabs(sum - out.total) > 0.000001 * (sum > 1 ? sum : 1)) return false;
    out.valid = true;
    return true;
}
}

int WeatherData_Round(double value) { return static_cast<int>(value < 0 ? value - 0.5 : value + 0.5); }

bool WeatherData_IsApi(const char *api)
{
    return api && (!std::strcmp(api, "current") || !std::strcmp(api, "daily3d") ||
                   !std::strcmp(api, "alert") || !std::strcmp(api, "minutely5m"));
}

WeatherUpdate WeatherData_Parse(const char *api, const char *json, std::size_t n, WeatherData &data)
{
    if (!WeatherData_IsApi(api) || !json || n > 384 || JSON_Validate(json, n) != JSONSuccess) return WeatherUpdate::Invalid;
    const char *const keys[] = {"api", "valid", "count", "items"};
    JSONPair_t f[4]{};
    if (!fields(json, n, keys, f) || f[0].jsonType != JSONString ||
        !equal(f[0].value, f[0].valueLength, api) || f[1].jsonType != JSONTrue) return WeatherUpdate::Invalid;
    bool same = false;
    unsigned count = 0;
    if (!std::strcmp(api, "current")) {
        WeatherCurrent c;
        if (!parseCurrent(json, n, c)) return WeatherUpdate::Invalid;
        same = data.current.valid && c.temperature == data.current.temperature &&
               c.humidity == data.current.humidity && !std::strcmp(c.icon, data.current.icon);
        data.current = c;
    } else {
        if (!integer(f[2], 65535, count)) return WeatherUpdate::Invalid;
        if (!std::strcmp(api, "daily3d")) {
            WeatherDaily c;
            if (!parseDaily(f[3], count, c)) return WeatherUpdate::Invalid;
            same = data.daily.valid && c.count == data.daily.count;
            for (unsigned i = 0; i < c.count; ++i) {
                const auto &a = c.days[i], &b = data.daily.days[i];
                same = same && a.high == b.high && a.low == b.low &&
                       !std::strcmp(a.date, b.date) && !std::strcmp(a.icon, b.icon);
            }
            data.daily = c;
        } else if (!std::strcmp(api, "alert")) {
            WeatherAlert c;
            if (!parseAlert(f[3], count, c)) return WeatherUpdate::Invalid;
            same = data.alert.valid && c.count == data.alert.count &&
                   !std::strcmp(c.icon, data.alert.icon) && !std::strcmp(c.name, data.alert.name) &&
                   !std::strcmp(c.color, data.alert.color);
            data.alert = c;
        } else {
            WeatherRain c;
            if (!parseRain(json, n, count, c)) return WeatherUpdate::Invalid;
            same = data.rain.valid && c.total == data.rain.total && !std::strcmp(c.startTime, data.rain.startTime);
            for (unsigned i = 0; i < 24; ++i) same = same && c.precip[i] == data.rain.precip[i];
            data.rain = c;
        }
    }
    return same ? WeatherUpdate::Unchanged : WeatherUpdate::Changed;
}
