#ifndef TOPINFO_SYSTEM_HEARTBEAT_H
#define TOPINFO_SYSTEM_HEARTBEAT_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
// 系统共享的 TIM10 毫秒时间源；仅在启动定时器前初始化一次。
void SystemHeartbeat_Init(void);
// TIM10 每次更新中断调用一次；其他任务只读取，不重置、不重复累加。
void SystemHeartbeat_Tick1ms(void);
uint32_t SystemHeartbeat_Millis(void);
#ifdef __cplusplus
}
#endif
#endif
