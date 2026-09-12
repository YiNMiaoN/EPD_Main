#include "Esp_Com.h"
#include "gpio.h"
#include "shell.h"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

int esp_cache(int, char **);
int esp_refresh(int, char **);
int esp_read(int, char **);
int esp_ping(int, char **);
int esp_status(int, char **);
int esp_ready(int, char **);
int esp_last(int, char **);
int esp_apis(int, char **);

UART_HandleTypeDef huart2 = { USART2 };
static GPIO_PinState ready, stm32_ready;
static uint8_t sent[393], *rx_byte;
static uint16_t sent_len;
static unsigned sends;
static HAL_StatusTypeDef tx_status;
static char output[4096];
static size_t output_len;

static short capture(char *data, unsigned short len)
{
    assert(output_len + len < sizeof(output));
    memcpy(output + output_len, data, len);
    output_len += len;
    output[output_len] = '\0';
    return (short)len;
}

Shell shell = { capture };

void shellPrint(Shell *sh, const char *format, ...)
{
    char buffer[128];
    va_list args;
    va_start(args, format);
    int len = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    assert(len >= 0);
    if (len > 128) len = 128;
    sh->write(buffer, (unsigned short)len);
}

unsigned short shellWriteString(Shell *sh, const char *text)
{
    return sh->write((char *)text, (unsigned short)strlen(text));
}

void EPD_UI_Refresh(void) {}
void EPD_HW_Sleep(void) {}

void HAL_GPIO_WritePin(void *port, uint16_t pin, GPIO_PinState state)
{
    (void)port;
    assert(pin == STM32_Ready_Pin);
    stm32_ready = state;
}

GPIO_PinState HAL_GPIO_ReadPin(void *port, uint16_t pin)
{
    (void)port;
    assert(pin == ESP_Ready_Pin);
    return ready;
}

HAL_StatusTypeDef HAL_UART_Receive_IT(UART_HandleTypeDef *uart, uint8_t *data, uint16_t len)
{
    assert(uart == &huart2 && len == 1);
    rx_byte = data;
    return HAL_OK;
}

HAL_StatusTypeDef HAL_UART_Transmit(UART_HandleTypeDef *uart, const uint8_t *data,
                                  uint16_t len, uint32_t timeout)
{
    assert(uart == &huart2 && len <= sizeof(sent) && timeout == 1000);
    memcpy(sent, data, len);
    sent_len = len;
    sends++;
    return tx_status;
}

static uint16_t crc16(const uint8_t *data, size_t length)
{
    uint16_t crc = 0xffff;
    for (size_t i = 0; i < length; ++i) {
        crc ^= (uint16_t)data[i] << 8;
        for (int j = 0; j < 8; ++j) {
            crc = (uint16_t)((crc << 1) ^ ((crc & 0x8000) ? 0x1021 : 0));
        }
    }
    return crc;
}

static void expect_request(uint8_t type, const char *payload)
{
    size_t len = strlen(payload);
    assert(sent_len == len + 9 && sent[0] == 0xaa && sent[1] == 0x55);
    assert(sent[2] == 1 && sent[3] == type);
    assert((((unsigned)sent[5] << 8) | sent[6]) == len);
    assert(memcmp(sent + 7, payload, len) == 0);
    assert(crc16(sent + 2, len + 5) == (((unsigned)sent[7 + len] << 8) | sent[8 + len]));
}

static void receive_payload(const uint8_t *payload, uint16_t len, int corrupt)
{
    uint8_t frame[393] = {0xaa, 0x55, 1, 0x83, 5, (uint8_t)(len >> 8), (uint8_t)len};
    memcpy(frame + 7, payload, len);
    uint16_t crc = crc16(frame + 2, len + 5);
    frame[len + 7] = (uint8_t)(crc >> 8);
    frame[len + 8] = (uint8_t)(crc ^ corrupt);
    for (unsigned i = 0; i < len + 9U; ++i) {
        *rx_byte = frame[i];
        EspCom_UartRxCpltCallback(&huart2);
    }
}

int main(void)
{
    char *no_args[] = { "command" };
    char *rain[] = { "command", "minutely5m" };
    char *result[] = { "command", "response" };
    char *invalid[] = { "esp_ready", "bad" };
    EspCom_Init();
    assert(stm32_ready == GPIO_PIN_SET && rx_byte);

    // Busy/offline business commands must not enter ESP's HTTP-time UART backlog.
    assert(esp_cache(1, no_args) == HAL_BUSY);
    assert(esp_refresh(2, rain) == HAL_BUSY);
    assert(esp_read(1, no_args) == HAL_BUSY);
    assert(sends == 0);
    assert(esp_ping(1, no_args) == HAL_OK);
    const uint8_t ping[] = {0xaa, 0x55, 1, 1, 1, 0, 0, 0xfa, 0xd9};
    assert(sent_len == sizeof(ping) && memcmp(sent, ping, sizeof(ping)) == 0);
    assert(esp_status(1, no_args) == HAL_OK);

    ready = GPIO_PIN_SET;
    assert(esp_read(1, no_args) == HAL_OK);
    expect_request(ESP_COM_CMD_READ_API, "{\"api\":\"daily3d\"}");
    assert(EspCom_ReadApi(NULL) == HAL_OK);
    expect_request(ESP_COM_CMD_READ_API, "{\"api\":\"current\"}");
    assert(esp_refresh(2, rain) == HAL_OK);
    expect_request(ESP_COM_CMD_REFRESH_API, "{\"api\":\"minutely5m\"}");
    assert(esp_cache(2, result) == HAL_OK);
    expect_request(ESP_COM_CMD_GET_CACHE, "{\"api\":\"response\"}");
    assert(esp_read(2, rain) == HAL_OK);
    expect_request(ESP_COM_CMD_READ_API, "{\"api\":\"minutely5m\"}");

    char *new_apis[] = { ESP_COM_API_DAILY3D, ESP_COM_API_ALERT, ESP_COM_API_HITOKOTO };
    for (unsigned i = 0; i < sizeof(new_apis) / sizeof(new_apis[0]); ++i) {
        char *args[] = { "command", new_apis[i] };
        char json[40];
        snprintf(json, sizeof(json), "{\"api\":\"%s\"}", new_apis[i]);
        unsigned before = sends;
        ready = GPIO_PIN_RESET;
        assert(esp_refresh(2, args) == HAL_BUSY);
        assert(esp_cache(2, args) == HAL_BUSY);
        assert(esp_read(2, args) == HAL_BUSY);
        assert(sends == before);
        ready = GPIO_PIN_SET;
        assert(esp_refresh(2, args) == HAL_OK);
        expect_request(ESP_COM_CMD_REFRESH_API, json);
        assert(esp_cache(2, args) == HAL_OK);
        expect_request(ESP_COM_CMD_GET_CACHE, json);
        assert(esp_read(2, args) == HAL_OK);
        expect_request(ESP_COM_CMD_READ_API, json);
    }

    tx_status = HAL_TIMEOUT;
    assert(esp_cache(1, no_args) == HAL_TIMEOUT);
    assert(esp_refresh(1, no_args) == HAL_TIMEOUT);
    assert(esp_read(1, no_args) == HAL_TIMEOUT);
    assert(esp_ping(1, no_args) == HAL_TIMEOUT);
    assert(esp_status(1, no_args) == HAL_TIMEOUT);
    tx_status = HAL_OK;
    assert(esp_ready(2, invalid) == HAL_ERROR && stm32_ready == GPIO_PIN_SET);

    // New flat JSON travels unchanged, including optional/unknown fields.
    const char flat[] = "{\"api\":\"current\",\"valid\":true,\"temp\":\"31\",\"source_update_time\":\"2026-09-12T12:00+08:00\"}";
    receive_payload((const uint8_t *)flat, sizeof(flat) - 1, 0);
    EspCom_Poll();
    EspCom_Frame received;
    assert(EspCom_GetLastFrame(&received));
    assert(strcmp((const char *)received.payload, flat) == 0);
    EspCom_ClearFrame();

    // Transport preserves numeric temperatures and the valid no-warning result.
    const char *new_payloads[] = {
        "{\"api\":\"daily3d\",\"valid\":true,\"count\":3,\"items\":[{\"date\":\"2026-09-12\",\"temp_max\":29.81,\"temp_min\":26.03,\"text\":\"Cloudy\"},{\"date\":\"2026-09-13\",\"temp_max\":30.87,\"temp_min\":25.16,\"text\":\"Rain\"},{\"date\":\"2026-09-14\",\"temp_max\":32.45,\"temp_min\":24.15,\"text\":\"Rain\"}]}",
        "{\"api\":\"alert\",\"valid\":true,\"source_update_time\":\"2026-09-12T18:18+08:00\",\"count\":0,\"items\":[]}",
        "{\"api\":\"alert\",\"valid\":true,\"count\":2,\"items\":[{\"id\":\"w1\",\"title\":\"Heavy rain\",\"severity_color\":\"Blue\"}]}",
        "{\"api\":\"alert\",\"valid\":false,\"message\":\"cache not available\"}",
        "{\"api\":\"daily3d\",\"valid\":true,\"ok\":false,\"message\":\"refresh failed; previous cache retained\"}",
        u8"{\"api\":\"hitokoto\",\"valid\":true,\"hitokoto\":\"\u884c\u52a8\u8d8a\u5feb\uff0c\u75db\u82e6\u8d8a\u5c11\u3002\",\"from\":\"\u6e38\u620f\",\"from_who\":\"\u5fb7\u514b\u8428\u65af\"}",
        u8"{\"api\":\"hitokoto\",\"valid\":true,\"hitokoto\":\"\u4f60\u597d\uff0c\\\"world\\\"\",\"from\":\"\u52a8\u753b\",\"from_who\":null}",
        "{\"api\":\"hitokoto\",\"valid\":false,\"message\":\"cache not available\"}",
        "{\"api\":\"hitokoto\",\"valid\":true,\"ok\":false,\"message\":\"refresh failed; previous cache retained\"}"
    };
    for (unsigned i = 0; i < sizeof(new_payloads) / sizeof(new_payloads[0]); ++i) {
        const char *json = new_payloads[i];
        size_t len = strlen(json);
        assert(len <= ESP_COM_MAX_PAYLOAD);
        receive_payload((const uint8_t *)json, (uint16_t)len, 0);
        EspCom_Poll();
        assert(EspCom_GetLastFrame(&received) && received.len == len);
        assert(memcmp(received.payload, json, len + 1) == 0);
        output_len = 0;
        assert(esp_last(1, no_args) == 0);
        assert(strstr(output, json) && !EspCom_HasFrame());
    }

    // Entire 226-byte and maximum-size JSON must survive Shell's 128-byte buffer.
    const uint16_t lengths[] = {0, 226, 384};
    for (unsigned n = 0; n < sizeof(lengths) / sizeof(lengths[0]); ++n) {
        uint16_t len = lengths[n];
        uint8_t payload[384];
        memset(payload, 'x', sizeof(payload));
        if (len) {
            memcpy(payload, "{\"text\":\"", 9);
            memcpy(payload + len - 2, "\"}", 2);
        }
        receive_payload(payload, len, 0);
        output_len = 0;
        assert(esp_last(1, no_args) == 0);
        char prefix[64];
        int prefix_len = snprintf(prefix, sizeof(prefix), "type=0x83 seq=5 len=%u payload=", len);
        assert(output_len == (size_t)prefix_len + len + 2);
        assert(memcmp(output, prefix, prefix_len) == 0);
        assert(memcmp(output + prefix_len, payload, len) == 0);
        assert(memcmp(output + prefix_len + len, "\r\n", 2) == 0);
        assert(!EspCom_HasFrame());
    }
    receive_payload((const uint8_t *)flat, sizeof(flat) - 1, 1);
    EspCom_Poll();
    assert(!EspCom_HasFrame());
    output_len = 0;
    assert(esp_apis(1, no_args) == 0);
    assert(strstr(output, "minutely5m") && strstr(output, "response"));
    assert(strstr(output, "daily3d: three-day forecast"));
    assert(strstr(output, "alert: refresh + warning summary"));
    assert(strstr(output, "hitokoto: refresh + quote"));
    assert(strstr(output, "daily7d: removed"));
    puts("ESP communication and Shell tests passed");
    return 0;
}
