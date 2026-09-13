#ifndef TOPINFO_API_TIME_H
#define TOPINFO_API_TIME_H

#include "Esp_Com.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t unix_time;
    int32_t utc_offset;
    char date[11];
    char time[9];
    uint8_t weekday;
} ApiTime;

// Validate a successful time ACK and copy its fields. Does not match SEQ.
// Failure leaves *time unchanged. Does not set RTC or advance a local clock.
bool ApiTime_ParseAck(const EspCom_Frame *frame, ApiTime *time);

#ifdef __cplusplus
}
#endif
#endif
