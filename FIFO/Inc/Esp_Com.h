//
// Created for ESP8266 UART protocol.
//

#ifndef TOPINFO_ESP_COM_H
#define TOPINFO_ESP_COM_H

#include "usart.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_COM_VERSION             0x01
#define ESP_COM_MAX_PAYLOAD         384

#define ESP_COM_API_CURRENT         "current"
#define ESP_COM_API_DAILY3D         "daily3d"
#define ESP_COM_API_MINUTELY5M      "minutely5m"
#define ESP_COM_API_ALERT           "alert"
#define ESP_COM_API_HITOKOTO        "hitokoto"
#define ESP_COM_API_TODOLIST        "todolist"
#define ESP_COM_API_TODOLIST_INBOX  "todolist_inbox"
#define ESP_COM_API_RESPONSE        "response"
#define ESP_COM_API_TIME            "time"

#define ESP_COM_CMD_PING            0x01
#define ESP_COM_CMD_GET_STATUS      0x02
#define ESP_COM_CMD_GET_CACHE       0x03
#define ESP_COM_CMD_REFRESH_API     0x04
#define ESP_COM_CMD_READ_API        0x05

#define ESP_COM_RSP_PONG            0x81
#define ESP_COM_RSP_STATUS          0x82
#define ESP_COM_RSP_CACHE           0x83
#define ESP_COM_RSP_ACK             0x84
#define ESP_COM_RSP_ERROR           0x85

typedef struct {
    uint8_t ver;
    uint8_t type;
    uint8_t seq;
    uint16_t len;
    uint8_t payload[ESP_COM_MAX_PAYLOAD + 1];
    uint16_t crc;
} EspCom_Frame;

// Invoked from EspCom_Poll(), never from the UART ISR. Do not send/poll recursively.
typedef void (*EspCom_FrameObserver)(const EspCom_Frame *frame);
void EspCom_SetFrameObserver(EspCom_FrameObserver observer);
uint8_t EspCom_GetTxSequence(void);

void EspCom_Init(void);
void EspCom_Poll(void);
void EspCom_SetReady(bool ready);
bool EspCom_IsEspReady(void);

HAL_StatusTypeDef EspCom_Send(uint8_t type, const uint8_t *payload, uint16_t len);
HAL_StatusTypeDef EspCom_Ping(void);
HAL_StatusTypeDef EspCom_GetStatus(void);
// Read ESP-local cache. Callers should check ESP_Ready before business requests.
HAL_StatusTypeDef EspCom_GetCache(const char *api);
// Queue HTTPS refresh for weather, quote, todolist or todolist_inbox.
// Todoist returns bounded task summaries; no automatic pagination.
// ACK confirms queuing, not completion.
// Wait for ESP_Ready to recover, then read response and the requested cache.
HAL_StatusTypeDef EspCom_RefreshApi(const char *api);
// Weather/quote/todolist: check local cache only. time: live NTP, data in ACK.
HAL_StatusTypeDef EspCom_ReadApi(const char *api);

bool EspCom_HasFrame(void);
bool EspCom_GetLastFrame(EspCom_Frame *frame);
void EspCom_ClearFrame(void);

void EspCom_UartRxCpltCallback(UART_HandleTypeDef *huart);

#ifdef __cplusplus
}
#endif

#endif //TOPINFO_ESP_COM_H
