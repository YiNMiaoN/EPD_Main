#ifndef TOPINFO_LOCAL_CLOCK_H
#define TOPINFO_LOCAL_CLOCK_H
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef enum {
    LOCAL_CLOCK_UNSYNCED,
    LOCAL_CLOCK_WAITING,
    LOCAL_CLOCK_RUNNING,
    LOCAL_CLOCK_FAILED
} LocalClock_State;

typedef struct {
    bool valid;
    uint16_t year;
    uint8_t month, day, hour, minute, second, weekday;
} LocalClock_Snapshot;

// 以下接口仅在主循环调用；不重置系统共享心跳。
void LocalClock_Init(void);
// 上电启动一次 NTP 请求；重复调用不再联网，失败不自动重试。
void LocalClock_Start(void);
// 在 ApiRefresh_Poll() 后处理启动校时和本地走时。
void LocalClock_Poll(void);
LocalClock_State LocalClock_GetState(void);
// 获取一致快照，补算主循环阻塞期间累计的心跳；无时间时返回 valid=false。
void LocalClock_GetSnapshot(LocalClock_Snapshot *snapshot);
#ifdef __cplusplus
}
#endif
#endif
