#include "LocalClock.h"
#include "SystemHeartbeat.h"
#include "ApiRefresh.h"
#include <stdio.h>
#include <string.h>

static LocalClock_State state;
static LocalClock_Snapshot current;
static int64_t local_seconds;
static uint32_t last_tick;
static unsigned remainder_ms;

static unsigned days_in_year(unsigned year)
{
    return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0) ? 366U : 365U;
}

static LocalClock_Snapshot calendar(int64_t seconds)
{
    LocalClock_Snapshot value = {0};
    int64_t days = seconds / 86400;
    int64_t day_seconds = seconds % 86400;
    if (day_seconds < 0) { day_seconds += 86400; --days; }
    int weekday = (int)((days + 4) % 7); // 1970-01-01 是星期四。
    if (weekday < 0) weekday += 7;
    unsigned year = 1970;
    while (days < 0) days += days_in_year(--year);
    while (days >= days_in_year(year)) days -= days_in_year(year++);
    static const unsigned lengths[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    unsigned month = 0;
    for (;;) {
        unsigned length = lengths[month] + (month == 1 && days_in_year(year) == 366 ? 1U : 0U);
        if (days < length) break;
        days -= length;
        ++month;
    }
    value.valid = true;
    value.year = (uint16_t)year;
    value.month = (uint8_t)(month + 1);
    value.day = (uint8_t)(days + 1);
    value.hour = (uint8_t)(day_seconds / 3600);
    value.minute = (uint8_t)(day_seconds / 60 % 60);
    value.second = (uint8_t)(day_seconds % 60);
    value.weekday = (uint8_t)weekday;
    return value;
}

static bool synchronize(const ApiTime *time)
{
    if (!time->unix_time || time->utc_offset < -86400 || time->utc_offset > 86400) return false;
    // Unix 秒为 UTC，只加一次偏移；与返回的本地年月日和星期交叉核对。
    const int64_t seconds = (int64_t)time->unix_time + time->utc_offset;
    const LocalClock_Snapshot value = calendar(seconds);
    char date[16], clock[16];
    snprintf(date, sizeof(date), "%04u-%02u-%02u", (unsigned)value.year, (unsigned)value.month, (unsigned)value.day);
    snprintf(clock, sizeof(clock), "%02u:%02u:%02u", (unsigned)value.hour, (unsigned)value.minute, (unsigned)value.second);
    if (strncmp(date, time->date, sizeof(time->date)) ||
        strncmp(clock, time->time, sizeof(time->time)) || value.weekday != time->weekday) return false;
    local_seconds = seconds;
    current = value;
    last_tick = SystemHeartbeat_Millis();
    remainder_ms = 0;
    return true;
}

static void advance(void)
{
    if (state != LOCAL_CLOCK_RUNNING) return;
    uint32_t now = SystemHeartbeat_Millis();
    uint32_t elapsed = now - last_tick; // 无符号差值支持毫秒计数回绕。
    last_tick = now;
    uint32_t seconds = elapsed / 1000;
    remainder_ms += elapsed % 1000;
    seconds += remainder_ms / 1000;
    remainder_ms %= 1000;
    if (seconds) {
        local_seconds += seconds;
        current = calendar(local_seconds);
    }
}

void LocalClock_Init(void)
{
    state = LOCAL_CLOCK_UNSYNCED;
    memset(&current, 0, sizeof(current));
    local_seconds = 0;
    remainder_ms = 0;
    last_tick = SystemHeartbeat_Millis();
}

void LocalClock_Start(void)
{
    if (state != LOCAL_CLOCK_UNSYNCED) return;
    state = ApiRefresh_StartTime() == HAL_OK ? LOCAL_CLOCK_WAITING : LOCAL_CLOCK_FAILED;
}

void LocalClock_Poll(void)
{
    if (state == LOCAL_CLOCK_WAITING) {
        const ApiRefresh_Result *result = ApiRefresh_GetResult();
        if (result->state == API_REFRESH_FAILED) state = LOCAL_CLOCK_FAILED;
        else if (result->state == API_REFRESH_SUCCEEDED) {
            state = result->has_time && !strcmp(result->api, ESP_COM_API_TIME) && synchronize(&result->time) ?
                    LOCAL_CLOCK_RUNNING : LOCAL_CLOCK_FAILED;
        }
    }
    advance();
}

LocalClock_State LocalClock_GetState(void) { return state; }

void LocalClock_GetSnapshot(LocalClock_Snapshot *snapshot)
{
    if (!snapshot) return;
    advance();
    *snapshot = current;
}
