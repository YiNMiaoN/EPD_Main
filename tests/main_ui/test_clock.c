#include "LocalClock.h"
#include "SystemHeartbeat.h"
#include "ApiRefresh.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint32_t now;
static unsigned requests;
static HAL_StatusTypeDef start_status = HAL_OK;
static ApiRefresh_Result result;
uint32_t SystemHeartbeat_Millis(void) { return now; }
HAL_StatusTypeDef ApiRefresh_StartTime(void) {
    ++requests;
    memset(&result, 0, sizeof(result));
    strcpy(result.api, "time");
    result.state = API_REFRESH_WAIT_READY;
    return start_status;
}
const ApiRefresh_Result *ApiRefresh_GetResult(void) { return &result; }

static void begin(uint32_t epoch, int32_t offset, const char *date, const char *time, unsigned weekday) {
    LocalClock_Init();
    unsigned before = requests;
    LocalClock_Start(); LocalClock_Start();
    assert(requests == before + 1 && LocalClock_GetState() == LOCAL_CLOCK_WAITING);
    LocalClock_Poll(); assert(LocalClock_GetState() == LOCAL_CLOCK_WAITING);
    result.state = API_REFRESH_SUCCEEDED; result.has_time = true;
    result.time.unix_time = epoch; result.time.utc_offset = offset;
    strcpy(result.time.date, date); strcpy(result.time.time, time); result.time.weekday = weekday;
    LocalClock_Poll();
    assert(LocalClock_GetState() == LOCAL_CLOCK_RUNNING);
}

static void expect(unsigned year, unsigned month, unsigned day, unsigned hour, unsigned minute, unsigned second, unsigned weekday) {
    LocalClock_Snapshot s;
    LocalClock_GetSnapshot(&s);
    assert(s.valid && s.year == year && s.month == month && s.day == day);
    assert(s.hour == hour && s.minute == minute && s.second == second && s.weekday == weekday);
}

int main(void) {
    now = 1234; LocalClock_Init();
    LocalClock_Snapshot s; LocalClock_GetSnapshot(&s); assert(!s.valid && now == 1234);
    begin(1799798400U, 28800, "2027-01-13", "08:00:00", 3);
    expect(2027,1,13,8,0,0,3);
    now += 999; LocalClock_Poll(); expect(2027,1,13,8,0,0,3);
    ++now; LocalClock_Poll(); expect(2027,1,13,8,0,1,3);
    now += 185999; LocalClock_Poll(); expect(2027,1,13,8,3,6,3);
    ++now; expect(2027,1,13,8,3,7,3); // 保留小数毫秒，无主循环也不丢秒。
    unsigned before = requests;
    result.time.unix_time = 1; result.state = API_REFRESH_FAILED;
    for (unsigned i = 0; i < 100; ++i) { LocalClock_Start(); LocalClock_Poll(); }
    assert(requests == before); expect(2027,1,13,8,3,7,3);

    begin(1709135999U,28800,"2024-02-28","23:59:59",3);
    now += 1000; expect(2024,2,29,0,0,0,4);
    begin(1709222399U,28800,"2024-02-29","23:59:59",4);
    now += 1000; expect(2024,3,1,0,0,0,5);
    begin(4107542399U,0,"2100-02-28","23:59:59",0);
    now += 1000; expect(2100,3,1,0,0,0,1); // 世纪年 2100 不是闰年。
    begin(1798732799U,28800,"2026-12-31","23:59:59",4);
    now += 1000; expect(2027,1,1,0,0,0,5);
    begin(1789880399U,-18000,"2026-09-19","23:59:59",6);
    now += 1000; expect(2026,9,20,0,0,0,0);
    begin(1,-3600,"1969-12-31","23:00:01",3); expect(1969,12,31,23,0,1,3);
    begin(UINT32_MAX,0,"2106-02-07","06:28:15",0);
    now += 1000; expect(2106,2,7,6,28,16,0); // 本地秒数不受 32 位 Unix 边界截断。
    now = UINT32_MAX - 500;
    begin(1799798400U,28800,"2027-01-13","08:00:00",3);
    now += 999; expect(2027,1,13,8,0,0,3);
    ++now; expect(2027,1,13,8,0,1,3);
    now += 86400000U * 2 + 61000; expect(2027,1,15,8,1,2,5);

    LocalClock_Init(); LocalClock_Start();
    result.state = API_REFRESH_FAILED; LocalClock_Poll();
    before = requests;
    for (unsigned i = 0; i < 100; ++i) { LocalClock_Start(); LocalClock_Poll(); }
    LocalClock_GetSnapshot(&s);
    assert(requests == before && !s.valid && LocalClock_GetState() == LOCAL_CLOCK_FAILED);
    start_status = HAL_BUSY;
    LocalClock_Init(); LocalClock_Start(); assert(LocalClock_GetState() == LOCAL_CLOCK_FAILED);
    before = requests; LocalClock_Start(); assert(requests == before);
    start_status = HAL_OK;
    LocalClock_Init(); LocalClock_Start();
    result.state = API_REFRESH_SUCCEEDED; result.has_time = true;
    result.time.unix_time = 1799798400U; result.time.utc_offset = 28800;
    strcpy(result.time.date,"2027-01-13"); strcpy(result.time.time,"08:00:00");
    result.time.weekday = 4; // 校时前拒绝时间戳与日期/星期不一致的 ACK。
    LocalClock_Poll(); assert(LocalClock_GetState() == LOCAL_CLOCK_FAILED);
    LocalClock_GetSnapshot(&s); assert(!s.valid);
    puts("Local clock boot-only sync, calendar, delayed polling and shared tick wrap tests passed");
}
