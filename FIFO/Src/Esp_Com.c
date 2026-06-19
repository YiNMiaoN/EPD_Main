//
// Created for ESP8266 UART protocol.
//

#include "Esp_Com.h"
#include "gpio.h"
#include <string.h>
#include <stdio.h>

#define ESP_COM_RX_FIFO_SIZE        1024
#define ESP_COM_FRAME_OVERHEAD      9
#define ESP_COM_UART_TIMEOUT_MS     1000

typedef enum {
    ESP_PARSE_HEAD_AA = 0,
    ESP_PARSE_HEAD_55,
    ESP_PARSE_VER,
    ESP_PARSE_TYPE,
    ESP_PARSE_SEQ,
    ESP_PARSE_LEN_H,
    ESP_PARSE_LEN_L,
    ESP_PARSE_PAYLOAD,
    ESP_PARSE_CRC_H,
    ESP_PARSE_CRC_L
} EspCom_ParseState;

static uint8_t esp_rx_byte;
static uint8_t esp_rx_fifo[ESP_COM_RX_FIFO_SIZE];
static volatile uint16_t esp_rx_head;
static volatile uint16_t esp_rx_tail;

static uint8_t esp_seq;
static EspCom_ParseState esp_state;
static EspCom_Frame esp_work_frame;
static EspCom_Frame esp_last_frame;
static uint16_t esp_payload_index;
static uint16_t esp_rx_crc;
static bool esp_frame_ready;

static bool EspCom_RxFifoPush(uint8_t data)
{
    uint16_t next = (uint16_t)((esp_rx_head + 1U) % ESP_COM_RX_FIFO_SIZE);

    if (next == esp_rx_tail) {
        return false;
    }

    esp_rx_fifo[esp_rx_head] = data;
    esp_rx_head = next;
    return true;
}

static bool EspCom_RxFifoPop(uint8_t *data)
{
    if (esp_rx_head == esp_rx_tail) {
        return false;
    }

    *data = esp_rx_fifo[esp_rx_tail];
    esp_rx_tail = (uint16_t)((esp_rx_tail + 1U) % ESP_COM_RX_FIFO_SIZE);
    return true;
}

static uint16_t EspCom_Crc16Ccitt(const uint8_t *data, uint16_t length, uint16_t seed)
{
    uint16_t crc = seed;

    for (uint16_t i = 0; i < length; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (uint8_t bit = 0; bit < 8; bit++) {
            if ((crc & 0x8000U) != 0U) {
                crc = (uint16_t)((crc << 1) ^ 0x1021U);
            } else {
                crc <<= 1;
            }
        }
    }

    return crc;
}

static void EspCom_ResetParser(void)
{
    esp_state = ESP_PARSE_HEAD_AA;
    esp_payload_index = 0;
    esp_rx_crc = 0;
    memset(&esp_work_frame, 0, sizeof(esp_work_frame));
}

static void EspCom_AcceptFrame(void)
{
    uint8_t crc_data[5 + ESP_COM_MAX_PAYLOAD];
    uint16_t calc_crc;

    crc_data[0] = esp_work_frame.ver;
    crc_data[1] = esp_work_frame.type;
    crc_data[2] = esp_work_frame.seq;
    crc_data[3] = (uint8_t)(esp_work_frame.len >> 8);
    crc_data[4] = (uint8_t)(esp_work_frame.len & 0xFFU);

    if (esp_work_frame.len > 0U) {
        memcpy(&crc_data[5], esp_work_frame.payload, esp_work_frame.len);
    }

    calc_crc = EspCom_Crc16Ccitt(crc_data, (uint16_t)(5U + esp_work_frame.len), 0xFFFF);
    esp_work_frame.crc = esp_rx_crc;

    if (esp_work_frame.ver == ESP_COM_VERSION && calc_crc == esp_rx_crc) {
        if (esp_work_frame.len <= ESP_COM_MAX_PAYLOAD) {
            esp_work_frame.payload[esp_work_frame.len] = '\0';
        }
        esp_last_frame = esp_work_frame;
        esp_frame_ready = true;
    }
}

static void EspCom_ParseByte(uint8_t data)
{
    switch (esp_state) {
        case ESP_PARSE_HEAD_AA:
            if (data == 0xAAU) {
                esp_state = ESP_PARSE_HEAD_55;
            }
            break;

        case ESP_PARSE_HEAD_55:
            if (data == 0x55U) {
                esp_state = ESP_PARSE_VER;
            } else if (data != 0xAAU) {
                EspCom_ResetParser();
            }
            break;

        case ESP_PARSE_VER:
            esp_work_frame.ver = data;
            esp_state = ESP_PARSE_TYPE;
            break;

        case ESP_PARSE_TYPE:
            esp_work_frame.type = data;
            esp_state = ESP_PARSE_SEQ;
            break;

        case ESP_PARSE_SEQ:
            esp_work_frame.seq = data;
            esp_state = ESP_PARSE_LEN_H;
            break;

        case ESP_PARSE_LEN_H:
            esp_work_frame.len = (uint16_t)data << 8;
            esp_state = ESP_PARSE_LEN_L;
            break;

        case ESP_PARSE_LEN_L:
            esp_work_frame.len |= data;
            if (esp_work_frame.len > ESP_COM_MAX_PAYLOAD) {
                EspCom_ResetParser();
            } else {
                esp_payload_index = 0;
                esp_state = (esp_work_frame.len == 0U) ? ESP_PARSE_CRC_H : ESP_PARSE_PAYLOAD;
            }
            break;

        case ESP_PARSE_PAYLOAD:
            esp_work_frame.payload[esp_payload_index++] = data;
            if (esp_payload_index >= esp_work_frame.len) {
                esp_state = ESP_PARSE_CRC_H;
            }
            break;

        case ESP_PARSE_CRC_H:
            esp_rx_crc = (uint16_t)data << 8;
            esp_state = ESP_PARSE_CRC_L;
            break;

        case ESP_PARSE_CRC_L:
            esp_rx_crc |= data;
            EspCom_AcceptFrame();
            EspCom_ResetParser();
            break;

        default:
            EspCom_ResetParser();
            break;
    }
}

void EspCom_Init(void)
{
    esp_rx_head = 0;
    esp_rx_tail = 0;
    esp_seq = 0;
    esp_frame_ready = false;
    EspCom_ResetParser();
    EspCom_SetReady(true);
    HAL_UART_Receive_IT(&huart2, &esp_rx_byte, 1);
}

void EspCom_Poll(void)
{
    uint8_t data;

    while (EspCom_RxFifoPop(&data)) {
        EspCom_ParseByte(data);
    }
}

void EspCom_SetReady(bool ready)
{
    HAL_GPIO_WritePin(STM32_Ready_GPIO_Port, STM32_Ready_Pin, ready ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

bool EspCom_IsEspReady(void)
{
    return HAL_GPIO_ReadPin(ESP_Ready_GPIO_Port, ESP_Ready_Pin) == GPIO_PIN_SET;
}

HAL_StatusTypeDef EspCom_Send(uint8_t type, const uint8_t *payload, uint16_t len)
{
    uint8_t frame[ESP_COM_FRAME_OVERHEAD + ESP_COM_MAX_PAYLOAD];
    uint16_t crc;

    if (len > ESP_COM_MAX_PAYLOAD) {
        return HAL_ERROR;
    }

    frame[0] = 0xAA;
    frame[1] = 0x55;
    frame[2] = ESP_COM_VERSION;
    frame[3] = type;
    frame[4] = ++esp_seq;
    frame[5] = (uint8_t)(len >> 8);
    frame[6] = (uint8_t)(len & 0xFFU);

    if (len > 0U && payload != NULL) {
        memcpy(&frame[7], payload, len);
    }

    crc = EspCom_Crc16Ccitt(&frame[2], (uint16_t)(5U + len), 0xFFFF);
    frame[7U + len] = (uint8_t)(crc >> 8);
    frame[8U + len] = (uint8_t)(crc & 0xFFU);

    return HAL_UART_Transmit(&huart2, frame, (uint16_t)(ESP_COM_FRAME_OVERHEAD + len), ESP_COM_UART_TIMEOUT_MS);
}

HAL_StatusTypeDef EspCom_Ping(void)
{
    return EspCom_Send(ESP_COM_CMD_PING, NULL, 0);
}

HAL_StatusTypeDef EspCom_GetStatus(void)
{
    return EspCom_Send(ESP_COM_CMD_GET_STATUS, NULL, 0);
}

HAL_StatusTypeDef EspCom_GetCache(const char *api)
{
    char payload[40];
    int len;

    if (api == NULL) {
        api = "current";
    }

    len = snprintf(payload, sizeof(payload), "{\"api\":\"%s\"}", api);
    if (len < 0 || len >= (int)sizeof(payload)) {
        return HAL_ERROR;
    }

    return EspCom_Send(ESP_COM_CMD_GET_CACHE, (const uint8_t *)payload, (uint16_t)len);
}

HAL_StatusTypeDef EspCom_RefreshApi(const char *api)
{
    char payload[40];
    int len;

    if (api == NULL) {
        api = "current";
    }

    len = snprintf(payload, sizeof(payload), "{\"api\":\"%s\"}", api);
    if (len < 0 || len >= (int)sizeof(payload)) {
        return HAL_ERROR;
    }

    return EspCom_Send(ESP_COM_CMD_REFRESH_API, (const uint8_t *)payload, (uint16_t)len);
}

HAL_StatusTypeDef EspCom_ReadApi(const char *api)
{
    char payload[40];
    int len;

    if (api == NULL) {
        api = "current";
    }

    len = snprintf(payload, sizeof(payload), "{\"api\":\"%s\"}", api);
    if (len < 0 || len >= (int)sizeof(payload)) {
        return HAL_ERROR;
    }

    return EspCom_Send(ESP_COM_CMD_READ_API, (const uint8_t *)payload, (uint16_t)len);
}

bool EspCom_HasFrame(void)
{
    return esp_frame_ready;
}

bool EspCom_GetLastFrame(EspCom_Frame *frame)
{
    if (!esp_frame_ready || frame == NULL) {
        return false;
    }

    *frame = esp_last_frame;
    return true;
}

void EspCom_ClearFrame(void)
{
    esp_frame_ready = false;
}

void EspCom_UartRxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2) {
        (void)EspCom_RxFifoPush(esp_rx_byte);
        HAL_UART_Receive_IT(&huart2, &esp_rx_byte, 1);
    }
}
