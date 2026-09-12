#ifndef TOPINFO_API_REFRESH_H
#define TOPINFO_API_REFRESH_H

#include "Esp_Com.h"

#ifdef __cplusplus
extern "C" {
#endif

#define API_REFRESH_READY_TIMEOUT_MS     15000U
#define API_REFRESH_REPLY_TIMEOUT_MS      3000U
#define API_REFRESH_FETCH_TIMEOUT_MS     60000U
#define API_REFRESH_SETTLE_MS              100U

typedef enum {
    API_REFRESH_IDLE,
    API_REFRESH_WAIT_READY,
    API_REFRESH_WAIT_ACK,
    API_REFRESH_WAIT_FINISH,
    API_REFRESH_WAIT_RESULT,
    API_REFRESH_WAIT_CACHE_READY,
    API_REFRESH_WAIT_CACHE,
    API_REFRESH_SUCCEEDED,
    API_REFRESH_FAILED
} ApiRefresh_State;

typedef enum {
    API_REFRESH_ERROR_NONE,
    API_REFRESH_ERROR_TX,
    API_REFRESH_ERROR_TIMEOUT,
    API_REFRESH_ERROR_PROTOCOL,
    API_REFRESH_ERROR_ESP,
    API_REFRESH_ERROR_REJECTED,
    API_REFRESH_ERROR_REMOTE_REFRESH,
    API_REFRESH_ERROR_INVALID_CACHE
} ApiRefresh_Error;

typedef struct {
    char api[16];
    ApiRefresh_State state;
    ApiRefresh_State failed_stage;
    ApiRefresh_Error error;
    HAL_StatusTypeDef tx_status;
    bool has_frame;
    EspCom_Frame frame;
} ApiRefresh_Result;

// Initialize once after EspCom_Init(). Installs the single frame observer.
void ApiRefresh_Init(void);
// Call once per TIM10 1 ms interrupt. Only increments the library timebase.
void ApiRefresh_Tick1ms(void);
// Queue one of current/daily3d/minutely5m/alert/hitokoto; NULL means current.
// HAL_OK means accepted locally. HAL_BUSY leaves the active operation intact.
HAL_StatusTypeDef ApiRefresh_Start(const char *api);
// Call after EspCom_Poll() in the main loop. No network wait or HAL_Delay.
void ApiRefresh_Poll(void);
bool ApiRefresh_IsBusy(void);
// Library-owned snapshot; updated by Poll and replaced on the next Start.
const ApiRefresh_Result *ApiRefresh_GetResult(void);
const char *ApiRefresh_StateString(ApiRefresh_State state);
const char *ApiRefresh_ErrorString(ApiRefresh_Error error);

#ifdef __cplusplus
}
#endif
#endif
