#include "SystemHeartbeat.h"

// Cortex-M4 对齐的 32 位读写为单次访问；中断是唯一写入者。
static volatile uint32_t heartbeat_ms;
void SystemHeartbeat_Init(void) { heartbeat_ms = 0; }
void SystemHeartbeat_Tick1ms(void) { ++heartbeat_ms; }
uint32_t SystemHeartbeat_Millis(void) { return heartbeat_ms; }
