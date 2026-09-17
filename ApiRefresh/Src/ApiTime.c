#include "ApiTime.h"
#include "core_json.h"
#include <string.h>

static bool equals(const char *value, size_t length, const char *expected)
{
    return value && length == strlen(expected) && memcmp(value, expected, length) == 0;
}

static bool integer(const JSONPair_t *pair, bool signed_value, uint32_t limit, int64_t *out)
{
    if (pair->jsonType != JSONNumber || !pair->valueLength) return false;
    size_t i = 0;
    bool negative = pair->value[0] == '-';
    if (negative) {
        if (!signed_value || pair->valueLength == 1) return false;
        ++i;
    }
    uint32_t value = 0;
    for (; i < pair->valueLength; ++i) {
        unsigned digit = (unsigned char)pair->value[i] - (unsigned)'0';
        if (digit > 9 || value > limit / 10 ||
            (value == limit / 10 && digit > limit % 10)) return false;
        value = value * 10 + digit;
    }
    *out = negative ? -(int64_t)value : (int64_t)value;
    return true;
}

static bool copy_calendar(const JSONPair_t *pair, char *out, const char *pattern)
{
    size_t length = strlen(pattern);
    if (pair->jsonType != JSONString || pair->valueLength != length) return false;
    for (size_t i = 0; i < length; ++i) {
        char c = pair->value[i];
        if (pattern[i] == 'd' ? (c < '0' || c > '9') : c != pattern[i]) return false;
    }
    memcpy(out, pair->value, length);
    out[length] = '\0';
    return true;
}

static unsigned two_digits(const char *p)
{
    return (unsigned)(p[0] - '0') * 10U + (unsigned)(p[1] - '0');
}

bool ApiTime_ParseAck(const EspCom_Frame *frame, ApiTime *time)
{
    if (!frame || !time || frame->type != ESP_COM_RSP_ACK || frame->len > ESP_COM_MAX_PAYLOAD ||
        JSON_Validate((const char *)frame->payload, frame->len) != JSONSuccess) return false;
    static const char *const keys[] = {
        "api", "ok", "refresh", "realtime", "unix_time", "utc_offset", "date", "time", "weekday"
    };
    ApiTime candidate = {0};
    unsigned seen = 0;
    size_t start = 0, next = 0;
    JSONPair_t pair;
    JSONStatus_t status;
    while ((status = JSON_Iterate((const char *)frame->payload, frame->len, &start, &next, &pair)) == JSONSuccess) {
        if (!pair.key) return false;
        unsigned field;
        for (field = 0; field < sizeof(keys) / sizeof(keys[0]); ++field) {
            if (equals(pair.key, pair.keyLength, keys[field])) break;
        }
        if (field == sizeof(keys) / sizeof(keys[0])) continue;
        if (seen & (1U << field)) return false;
        seen |= 1U << field;
        int64_t number;
        switch (field) {
            case 0:
                if (pair.jsonType != JSONString || !equals(pair.value, pair.valueLength, ESP_COM_API_TIME)) return false;
                break;
            case 1: case 3:
                if (pair.jsonType != JSONTrue) return false;
                break;
            case 2:
                if (pair.jsonType != JSONFalse) return false;
                break;
            case 4:
                if (!integer(&pair, false, UINT32_MAX, &number) || number == 0) return false;
                candidate.unix_time = (uint32_t)number;
                break;
            case 5:
                if (!integer(&pair, true, 86400U, &number)) return false;
                candidate.utc_offset = (int32_t)number;
                break;
            case 6:
                if (!copy_calendar(&pair, candidate.date, "dddd-dd-dd")) return false;
                break;
            case 7:
                if (!copy_calendar(&pair, candidate.time, "dd:dd:dd")) return false;
                break;
            case 8:
                if (!integer(&pair, false, 6U, &number)) return false;
                candidate.weekday = (uint8_t)number;
                break;
        }
    }
    if (status != JSONNotFound || seen != 0x1FFU) return false;
    unsigned year = two_digits(candidate.date) * 100U + two_digits(candidate.date + 2);
    unsigned month = two_digits(candidate.date + 5), day = two_digits(candidate.date + 8);
    static const unsigned days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (year == 0 || month < 1 || month > 12 || day < 1) return false;
    unsigned max_day = days[month - 1];
    if (month == 2 && year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)) ++max_day;
    if (day > max_day || two_digits(candidate.time) > 23 ||
        two_digits(candidate.time + 3) > 59 || two_digits(candidate.time + 6) > 59) return false;
    *time = candidate;
    return true;
}
