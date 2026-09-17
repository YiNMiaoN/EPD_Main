#include "Esp_Com.h"
#include "ApiRefresh.h"
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
int api_refresh(int, char **);
int api_result(int, char **);
int api_time(int, char **);
int ref_epd(int, char **);

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

static void receive_frame(uint8_t type, uint8_t seq, const uint8_t *payload, uint16_t len, int corrupt)
{
    uint8_t frame[393] = {0xaa, 0x55, 1, type, seq, (uint8_t)(len >> 8), (uint8_t)len};
    memcpy(frame + 7, payload, len);
    uint16_t crc = crc16(frame + 2, len + 5);
    frame[len + 7] = (uint8_t)(crc >> 8);
    frame[len + 8] = (uint8_t)(crc ^ corrupt);
    for (unsigned i = 0; i < len + 9U; ++i) {
        *rx_byte = frame[i];
        EspCom_UartRxCpltCallback(&huart2);
    }
}

static void receive_payload(const uint8_t *payload, uint16_t len, int corrupt)
{
    receive_frame(0x83, 5, payload, len, corrupt);
}

static void tick(unsigned ms)
{
    unsigned before = sends;
    while (ms--) ApiRefresh_Tick1ms();
    assert(sends == before);
}

static void reply(uint8_t type, const char *json)
{
    receive_frame(type, sent[4], (const uint8_t *)json, (uint16_t)strlen(json), 0);
    EspCom_Poll();
    ApiRefresh_Poll();
}

static void begin_refresh(void)
{
    ApiRefresh_Init();
    ready = GPIO_PIN_SET;
    tx_status = HAL_OK;
    assert(ApiRefresh_Start(NULL) == HAL_OK);
    ApiRefresh_Poll();
    assert(ApiRefresh_GetResult()->state == API_REFRESH_WAIT_ACK);
}

static void begin_result(void)
{
    begin_refresh();
    reply(ESP_COM_RSP_ACK, "{\"api\":\"current\",\"ok\":true,\"refresh\":true}");
    tick(API_REFRESH_SETTLE_MS);
    ApiRefresh_Poll();
    assert(ApiRefresh_GetResult()->state == API_REFRESH_WAIT_RESULT);
}

static void expect_failure(ApiRefresh_Error error, ApiRefresh_State stage)
{
    const ApiRefresh_Result *r = ApiRefresh_GetResult();
    assert(r->state == API_REFRESH_FAILED && r->error == error && r->failed_stage == stage);
    assert(!ApiRefresh_IsBusy());
    unsigned before = sends;
    tick(API_REFRESH_FETCH_TIMEOUT_MS);
    ApiRefresh_Poll();
    assert(sends == before);
}

static void test_api_refresh(void)
{
    assert(ApiRefresh_StartTime() == HAL_ERROR);
    assert(ApiRefresh_Start(NULL) == HAL_ERROR);
    ApiRefresh_Init();
    unsigned before = sends;
    tick(100000);
    ApiRefresh_Poll();
    assert(sends == before && ApiRefresh_GetResult()->state == API_REFRESH_IDLE);
    assert(ApiRefresh_Start("response") == HAL_ERROR);
    assert(ApiRefresh_Start("daily7d") == HAL_ERROR);
    assert(ApiRefresh_Start("") == HAL_ERROR);

    const char *apis[] = {"current", "daily3d", "minutely5m", "alert", "hitokoto", ESP_COM_API_TODOLIST, ESP_COM_API_TODOLIST_INBOX};
    for (unsigned i = 0; i < sizeof(apis) / sizeof(apis[0]); ++i) {
        ApiRefresh_Init();
        ready = GPIO_PIN_RESET;
        before = sends;
        assert(ApiRefresh_Start(apis[i]) == HAL_OK);
        ApiRefresh_Poll();
        assert(sends == before);
        assert(ApiRefresh_Start("current") == HAL_BUSY);
        assert(strcmp(ApiRefresh_GetResult()->api, apis[i]) == 0);
        ready = GPIO_PIN_SET;
        ApiRefresh_Poll();
        const char *stage = strcmp(apis[i], ESP_COM_API_TODOLIST_INBOX) == 0 ? "inbox_next2d" : "today";
        char json[384], request[40];
        snprintf(request, sizeof(request), "{\"api\":\"%s\"}", apis[i]);
        expect_request(ESP_COM_CMD_REFRESH_API, request);
        assert(EspCom_GetTxSequence() == sent[4]);
        snprintf(json, sizeof(json), "{\"api\":\"%s\",\"ok\":true,\"refresh\":true}", apis[i]);
        receive_frame(ESP_COM_RSP_ACK, (uint8_t)(sent[4] + 1), (const uint8_t *)json, (uint16_t)strlen(json), 0);
        EspCom_Poll();
        ApiRefresh_Poll();
        assert(ApiRefresh_GetResult()->state == API_REFRESH_WAIT_ACK);
        // The observer retains the matched ACK even if another frame replaces esp_last.
        receive_frame(ESP_COM_RSP_ACK, sent[4], (const uint8_t *)json, (uint16_t)strlen(json), 0);
        receive_frame(ESP_COM_RSP_PONG, (uint8_t)(sent[4] + 1), (const uint8_t *)"{}", 2, 0);
        EspCom_Poll();
        EspCom_ClearFrame();
        ApiRefresh_Poll();
        assert(ApiRefresh_GetResult()->state == API_REFRESH_WAIT_FINISH);
        before = sends;
        tick(API_REFRESH_SETTLE_MS - 1);
        ApiRefresh_Poll();
        assert(sends == before);
        ready = GPIO_PIN_RESET;
        tick(1);
        ApiRefresh_Poll();
        assert(sends == before);
        ready = GPIO_PIN_SET;
        ApiRefresh_Poll();
        expect_request(ESP_COM_CMD_GET_CACHE, "{\"api\":\"response\"}");
        snprintf(json, sizeof(json), "{\"api\":\"%s\",\"valid\":true,\"ok\":true,\"stage\":\"%s\",\"uptime_ms\":123}", apis[i], stage);
        reply(ESP_COM_RSP_CACHE, json);
        assert(ApiRefresh_GetResult()->state == API_REFRESH_WAIT_CACHE_READY);
        ApiRefresh_Poll();
        expect_request(ESP_COM_CMD_GET_CACHE, request);
        snprintf(json, sizeof(json), "{\"api\":\"%s\",\"valid\":true,\"text\":\"\xE4\xBD\xA0\xE5\xA5\xBD\",\"from_who\":null,\"stage\":\"%s\"}", apis[i], stage);
        reply(ESP_COM_RSP_CACHE, json);
        const ApiRefresh_Result *r = ApiRefresh_GetResult();
        assert(r->state == API_REFRESH_SUCCEEDED && r->error == API_REFRESH_ERROR_NONE);
        assert(r->has_frame && strcmp((const char *)r->frame.payload, json) == 0);
        output_len = 0;
        assert(api_result(0, NULL) == 0);
        assert(strstr(output, "state=succeeded") && strstr(output, json));
        before = sends;
        tick(100000);
        ApiRefresh_Poll();
        assert(sends == before);
    }

    begin_result();
    const char failed[] = "{\"api\":\"current\",\"valid\":true,\"ok\":false,\"message\":\"refresh failed; previous cache retained\"}";
    reply(ESP_COM_RSP_CACHE, failed);
    expect_failure(API_REFRESH_ERROR_REMOTE_REFRESH, API_REFRESH_WAIT_RESULT);
    assert(strcmp((const char *)ApiRefresh_GetResult()->frame.payload, failed) == 0);

    const char *bad[] = {
        "{\"api\":\"hitokoto\",\"valid\":true,\"ok\":true}",
        "{\"api\":\"current\",\"valid\":true}",
        "{\"api\":\"current\",\"valid\":true,\"ok\":\"true\"}",
        "{\"api\":\"current\",\"valid\":true,\"ok\":true,\"ok\":false}",
        "{\"api\":\"current\",\"nested\":{\"valid\":true,\"ok\":true}}",
        "{\"api\":\"current\",\"valid\":true,\"ok\":true,}",
        "[]", "{}", "null"
    };
    for (unsigned i = 0; i < sizeof(bad) / sizeof(bad[0]); ++i) {
        begin_result();
        reply(ESP_COM_RSP_CACHE, bad[i]);
        expect_failure(API_REFRESH_ERROR_PROTOCOL, API_REFRESH_WAIT_RESULT);
    }
    begin_refresh();
    reply(ESP_COM_RSP_ACK, "{\"api\":\"current\",\"ok\":false,\"refresh\":false}");
    expect_failure(API_REFRESH_ERROR_REJECTED, API_REFRESH_WAIT_ACK);
    begin_refresh();
    reply(ESP_COM_RSP_ERROR, "{\"ok\":false,\"code\":\"not_ready\"}");
    expect_failure(API_REFRESH_ERROR_ESP, API_REFRESH_WAIT_ACK);
    begin_refresh();
    reply(ESP_COM_RSP_CACHE, "{\"api\":\"current\",\"ok\":true,\"refresh\":true}");
    expect_failure(API_REFRESH_ERROR_PROTOCOL, API_REFRESH_WAIT_ACK);
    begin_result();
    reply(ESP_COM_RSP_CACHE, "{\"api\":\"current\",\"valid\":true,\"ok\":true}");
    ApiRefresh_Poll();
    reply(ESP_COM_RSP_CACHE, "{\"api\":\"current\",\"valid\":false}");
    expect_failure(API_REFRESH_ERROR_INVALID_CACHE, API_REFRESH_WAIT_CACHE);

    // Each timed stage must terminate, including timeouts while Ready stays LOW.
    for (ApiRefresh_State stage = API_REFRESH_WAIT_READY; stage <= API_REFRESH_WAIT_CACHE; ++stage) {
        begin_refresh();
        if (stage == API_REFRESH_WAIT_READY) {
            ApiRefresh_Init();
            assert(ApiRefresh_Start(NULL) == HAL_OK);
        }
        if (stage >= API_REFRESH_WAIT_FINISH) {
            reply(ESP_COM_RSP_ACK, "{\"api\":\"current\",\"ok\":true,\"refresh\":true}");
        }
        if (stage >= API_REFRESH_WAIT_RESULT) {
            tick(API_REFRESH_SETTLE_MS);
            ApiRefresh_Poll();
        }
        if (stage >= API_REFRESH_WAIT_CACHE_READY) {
            reply(ESP_COM_RSP_CACHE, "{\"api\":\"current\",\"valid\":true,\"ok\":true}");
        }
        if (stage == API_REFRESH_WAIT_CACHE) ApiRefresh_Poll();
        assert(ApiRefresh_GetResult()->state == stage);
        ready = GPIO_PIN_RESET;
        unsigned timeout = stage == API_REFRESH_WAIT_FINISH ? API_REFRESH_FETCH_TIMEOUT_MS :
            (stage == API_REFRESH_WAIT_READY || stage == API_REFRESH_WAIT_CACHE_READY ?
             API_REFRESH_READY_TIMEOUT_MS : API_REFRESH_REPLY_TIMEOUT_MS);
        tick(timeout - 1);
        ApiRefresh_Poll();
        assert(ApiRefresh_IsBusy());
        tick(1);
        ApiRefresh_Poll();
        expect_failure(API_REFRESH_ERROR_TIMEOUT, stage);
    }
    ApiRefresh_Init();
    ready = GPIO_PIN_SET;
    tx_status = HAL_TIMEOUT;
    assert(ApiRefresh_Start(NULL) == HAL_OK);
    ApiRefresh_Poll();
    expect_failure(API_REFRESH_ERROR_TX, API_REFRESH_WAIT_READY);
    assert(ApiRefresh_GetResult()->tx_status == HAL_TIMEOUT);

    begin_refresh();
    before = sends;
    char *args[] = {"command", "current"};
    char *set_ready[] = {"esp_ready", "0"};
    output_len = 0;
    assert(esp_cache(2, args) == HAL_BUSY && esp_refresh(2, args) == HAL_BUSY);
    assert(esp_read(2, args) == HAL_BUSY && esp_ping(1, args) == HAL_BUSY);
    assert(esp_status(1, args) == HAL_BUSY && esp_ready(2, set_ready) == HAL_BUSY);
    assert(ref_epd(1, args) == HAL_BUSY && api_refresh(2, args) == HAL_BUSY);
    assert(esp_ready(1, args) == HAL_OK && sends == before);
    // An ACK parsed before the deadline is still valid when Poll runs later.
    const char ack[] = "{\"api\":\"current\",\"ok\":true,\"refresh\":true}";
    tick(API_REFRESH_REPLY_TIMEOUT_MS - 1);
    receive_frame(ESP_COM_RSP_ACK, sent[4], (const uint8_t *)ack, sizeof(ack) - 1, 0);
    output_len = 0;
    assert(esp_last(1, args) == 0);
    tick(2);
    ApiRefresh_Poll();
    assert(ApiRefresh_GetResult()->state == API_REFRESH_WAIT_FINISH);
    puts("API refresh workflow tests passed");
}

static const char time_ack[] = "{\"ok\":true,\"api\":\"time\",\"refresh\":false,\"realtime\":true,\"unix_time\":1799798400,\"utc_offset\":28800,\"date\":\"2027-01-13\",\"time\":\"08:00:00\",\"weekday\":3}";

static void begin_todolist_result(const char *api)
{
    ApiRefresh_Init();
    ready = GPIO_PIN_SET;
    tx_status = HAL_OK;
    char *args[] = {"api_refresh", (char *)api};
    output_len = 0;
    assert(api_refresh(2, args) == HAL_OK);
    ApiRefresh_Poll();
    char request[40], ack[96];
    snprintf(request, sizeof(request), "{\"api\":\"%s\"}", api);
    expect_request(ESP_COM_CMD_REFRESH_API, request);
    assert(ApiRefresh_StartTime() == HAL_BUSY);
    assert(ApiRefresh_Start(ESP_COM_API_TODOLIST) == HAL_BUSY);
    assert(ApiRefresh_Start(ESP_COM_API_TODOLIST_INBOX) == HAL_BUSY);
    assert(esp_refresh(2, args) == HAL_BUSY);
    assert(ref_epd(1, args) == HAL_BUSY);
    snprintf(ack, sizeof(ack), "{\"ok\":true,\"api\":\"%s\",\"refresh\":true}", api);
    reply(ESP_COM_RSP_ACK, ack);
    assert(ApiRefresh_GetResult()->state == API_REFRESH_WAIT_FINISH);
    ready = GPIO_PIN_RESET;
    unsigned before = sends;
    tick(API_REFRESH_SETTLE_MS);
    ApiRefresh_Poll();
    assert(sends == before);
    ready = GPIO_PIN_SET;
    ApiRefresh_Poll();
    expect_request(ESP_COM_CMD_GET_CACHE, "{\"api\":\"response\"}");
}

static void test_todolist(const char *api, const char *stage)
{
    char success[256], cache[384], request[40];
    snprintf(success, sizeof(success), "{\"api\":\"%s\",\"valid\":true,\"ok\":true,\"stage\":\"%s\",\"http_status\":200,\"code\":\"json_ok\"}", api, stage);
    snprintf(request, sizeof(request), "{\"api\":\"%s\"}", api);
    const char *summaries[] = {
        "\"count\":0,\"has_more\":false,\"items\":[]",
        u8"\"count\":2,\"has_more\":true,\"items\":[{\"id\":\"abc123\",\"due\":\"2026-09-17\",\"tz\":null,\"content\":\"检查设备\",\"title_cut\":true}]"
    };
    for (unsigned i = 0; i < sizeof(summaries) / sizeof(summaries[0]); ++i) {
        begin_todolist_result(api);
        reply(ESP_COM_RSP_CACHE, success);
        ApiRefresh_Poll();
        expect_request(ESP_COM_CMD_GET_CACHE, request);
        int len = snprintf(cache, sizeof(cache), "{\"api\":\"%s\",\"valid\":true,\"stage\":\"%s\",\"http_status\":200,\"checked_at_ms\":123456,%s}", api, stage, summaries[i]);
        assert(len > 0 && len < (int)sizeof(cache));
        reply(ESP_COM_RSP_CACHE, cache);
        assert(ApiRefresh_GetResult()->state == API_REFRESH_SUCCEEDED);
        assert(!ApiRefresh_GetResult()->has_time);
        output_len = 0;
        assert(api_result(0, NULL) == 0 && strstr(output, cache));
        unsigned before = sends;
        tick(API_REFRESH_FETCH_TIMEOUT_MS);
        ApiRefresh_Poll();
        assert(sends == before); // No periodic refresh or pagination.
    }
    const struct { const char *code; int status; int transport; } failures[] = {
        {"not_configured", 0, 0}, {"ntp_failed", 0, 0},
        {"tls_or_transport_error", 0, -1}, {"http_error", 401, 0},
        {"http_error", 429, 0}, {"out_of_memory", 200, 0},
        {"cache_rejected", 200, 0}, {"body_too_large", 200, 0},
        {"body_read_failed", 200, 0}, {"bad_json", 200, 0}
    };
    for (unsigned i = 0; i < sizeof(failures) / sizeof(failures[0]); ++i) {
        begin_todolist_result(api);
        char json[384];
        int len = snprintf(json, sizeof(json),
            "{\"api\":\"%s\",\"valid\":true,\"ok\":false,\"stage\":\"%s\",\"http_status\":%d,\"transport_error\":%d,\"code\":\"%s\"}",
            api, stage, failures[i].status, failures[i].transport, failures[i].code);
        assert(len > 0 && len < (int)sizeof(json));
        unsigned before = sends;
        reply(ESP_COM_RSP_CACHE, json);
        expect_failure(API_REFRESH_ERROR_REMOTE_REFRESH, API_REFRESH_WAIT_RESULT);
        assert(sends == before); // Never read old cache after failure, even HTTP 200.
        output_len = 0;
        assert(api_result(0, NULL) == 0 && strstr(output, json));
    }
    // Reject old probe schema, missing/nested/duplicate/wrong-type and cross-view stages.
    const char *bad_stages[] = {
        "", ",\"stage\":\"http_probe\"", ",\"stage\":false",
        ",\"nested\":{\"stage\":\"today\"}",
        ",\"stage\":\"today\",\"stage\":\"inbox_next2d\"",
        ",\"stage\":\"today\"", ",\"stage\":\"inbox_next2d\""
    };
    for (unsigned i = 0; i < sizeof(bad_stages) / sizeof(bad_stages[0]); ++i) {
        if ((i == 5 && strcmp(stage, "today") == 0) ||
            (i == 6 && strcmp(stage, "inbox_next2d") == 0)) continue;
        for (unsigned final_cache = 0; final_cache < 2; ++final_cache) {
            begin_todolist_result(api);
            if (final_cache) {
                reply(ESP_COM_RSP_CACHE, success);
                ApiRefresh_Poll();
            }
            char json[256];
            snprintf(json, sizeof(json), "{\"api\":\"%s\",\"valid\":true,\"ok\":true%s}", api, bad_stages[i]);
            reply(ESP_COM_RSP_CACHE, json);
            expect_failure(API_REFRESH_ERROR_PROTOCOL, final_cache ? API_REFRESH_WAIT_CACHE : API_REFRESH_WAIT_RESULT);
            assert(strcmp((const char *)ApiRefresh_GetResult()->frame.payload, json) == 0);
        }
    }
    begin_todolist_result(api);
    reply(ESP_COM_RSP_CACHE, success);
    ApiRefresh_Poll();
    snprintf(cache, sizeof(cache), "{\"api\":\"%s\",\"valid\":false}", api);
    reply(ESP_COM_RSP_CACHE, cache);
    expect_failure(API_REFRESH_ERROR_INVALID_CACHE, API_REFRESH_WAIT_CACHE);
    // Preserve the observed empty response behavior; no speculative retry.
    begin_todolist_result(api);
    reply(ESP_COM_RSP_CACHE, "{\"api\":\"response\",\"valid\":false,\"message\":\"cache not available\"}");
    expect_failure(API_REFRESH_ERROR_PROTOCOL, API_REFRESH_WAIT_RESULT);
    printf("Todoist summary workflow passed: %s\n", api);
}

static void begin_time(void)
{
    ApiRefresh_Init();
    ready = GPIO_PIN_SET;
    tx_status = HAL_OK;
    assert(ApiRefresh_StartTime() == HAL_OK);
    ApiRefresh_Poll();
    expect_request(ESP_COM_CMD_READ_API, "{\"api\":\"time\"}");
}

static void test_api_time(void)
{
    char *args[] = {"api_time"};
    char *extra[] = {"api_time", "extra"};
    ApiRefresh_Init();
    output_len = 0;
    assert(ApiRefresh_Start("time") == HAL_ERROR);
    assert(api_time(2, extra) == HAL_ERROR);
    ready = GPIO_PIN_RESET;
    unsigned before = sends;
    assert(api_time(1, args) == HAL_OK);
    ApiRefresh_Poll();
    assert(sends == before);
    tick(API_REFRESH_READY_TIMEOUT_MS);
    ApiRefresh_Poll();
    expect_failure(API_REFRESH_ERROR_TIMEOUT, API_REFRESH_WAIT_READY);

    begin_time();
    before = sends;
    assert(ApiRefresh_Start(NULL) == HAL_BUSY);
    assert(ApiRefresh_StartTime() == HAL_BUSY);
    assert(esp_read(1, args) == HAL_BUSY);
    assert(ref_epd(1, args) == HAL_BUSY);
    assert(sends == before);
    receive_frame(ESP_COM_RSP_ACK, (uint8_t)(sent[4] + 1), (const uint8_t *)time_ack, sizeof(time_ack) - 1, 0);
    EspCom_Poll();
    ApiRefresh_Poll();
    assert(ApiRefresh_GetResult()->state == API_REFRESH_WAIT_ACK);
    // NTP has its own deadline, beyond the ordinary 3s ACK timeout.
    tick(API_REFRESH_REPLY_TIMEOUT_MS + 1);
    ApiRefresh_Poll();
    assert(ApiRefresh_IsBusy());
    ready = GPIO_PIN_RESET;
    receive_frame(ESP_COM_RSP_ACK, sent[4], (const uint8_t *)time_ack, sizeof(time_ack) - 1, 0);
    output_len = 0;
    esp_last(1, args);
    ApiRefresh_Poll();
    const ApiRefresh_Result *r = ApiRefresh_GetResult();
    assert(r->state == API_REFRESH_SUCCEEDED && r->has_time);
    assert(r->time.unix_time == 1799798400U && r->time.utc_offset == 28800);
    assert(strcmp(r->time.date, "2027-01-13") == 0 && strcmp(r->time.time, "08:00:00") == 0);
    assert(r->time.weekday == 3 && strcmp((const char *)r->frame.payload, time_ack) == 0);
    output_len = 0;
    api_result(1, args);
    assert(strstr(output, "date=2027-01-13 time=08:00:00 weekday=3"));
    assert(strstr(output, "unix_time=1799798400 utc_offset=28800") && strstr(output, time_ack));
    tick(100000);
    ApiRefresh_Poll();
    assert(sends == before); // No cache/result query, no periodic request.

    begin_time();
    assert(!ApiRefresh_GetResult()->has_time);
    reply(ESP_COM_RSP_ERROR, "{\"ok\":false,\"code\":\"request_failed\",\"message\":\"NTP time request failed\"}");
    expect_failure(API_REFRESH_ERROR_ESP, API_REFRESH_WAIT_ACK);
    assert(!ApiRefresh_GetResult()->has_time);
    begin_time();
    reply(ESP_COM_RSP_CACHE, time_ack);
    expect_failure(API_REFRESH_ERROR_PROTOCOL, API_REFRESH_WAIT_ACK);
    begin_time();
    reply(ESP_COM_RSP_ACK, "{\"api\":\"time\",\"ok\":true,\"refresh\":false}");
    expect_failure(API_REFRESH_ERROR_PROTOCOL, API_REFRESH_WAIT_ACK);
    begin_time();
    tick(API_TIME_REPLY_TIMEOUT_MS - 1);
    ApiRefresh_Poll();
    assert(ApiRefresh_IsBusy());
    tick(1);
    reply(ESP_COM_RSP_ACK, time_ack);
    expect_failure(API_REFRESH_ERROR_TIMEOUT, API_REFRESH_WAIT_ACK);
    begin_refresh();
    assert(ApiRefresh_StartTime() == HAL_BUSY);
    ApiRefresh_Init();
    tx_status = HAL_TIMEOUT;
    ready = GPIO_PIN_SET;
    assert(ApiRefresh_StartTime() == HAL_OK);
    ApiRefresh_Poll();
    expect_failure(API_REFRESH_ERROR_TX, API_REFRESH_WAIT_READY);
    tx_status = HAL_OK;

    // Parser rejects missing, duplicate, nested, wrong-type and out-of-range fields.
    const char *replacements[][2] = {
        {"\"ok\":true", "\"ok\":false"}, {"\"api\":\"time\"", "\"api\":\"current\""},
        {"\"refresh\":false", "\"refresh\":true"}, {"\"realtime\":true", "\"realtime\":\"true\""},
        {"1799798400", "0"}, {"1799798400", "-1"}, {"1799798400", "4294967296"},
        {"1799798400", "1.5"}, {"1799798400", "1e9"}, {"1799798400", "\"1799798400\""},
        {"28800", "-86401"}, {"28800", "86401"},
        {"2027-01-13", "2027-02-29"}, {"2027-01-13", "2100-02-29"},
        {"2027-01-13", "2027-13-01"}, {"2027-01-13", "2027-04-31"},
        {"2027-01-13", "2027-01-00"}, {"2027-01-13", "2027-1-13"},
        {"08:00:00", "24:00:00"}, {"08:00:00", "08:60:00"}, {"08:00:00", "08:00:60"},
        {"\"weekday\":3", "\"weekday\":7"}, {"\"weekday\":3", "\"weekday\":-1"},
        {"\"weekday\":3", "\"weekday\":3,\"weekday\":3"},
        {"\"weekday\":3", "\"other\":3"}, {"\"weekday\":3", "\"nested\":{\"weekday\":3}"}
    };
    EspCom_Frame frame = {.type = ESP_COM_RSP_ACK};
    ApiTime parsed = {0};
    memcpy(frame.payload, time_ack, sizeof(time_ack));
    frame.len = sizeof(time_ack) - 1;
    assert(ApiTime_ParseAck(&frame, &parsed));
    ApiTime previous = parsed;
    for (unsigned i = 0; i < sizeof(replacements) / sizeof(replacements[0]); ++i) {
        const char *match = strstr(time_ack, replacements[i][0]);
        assert(match);
        int len = snprintf((char *)frame.payload, sizeof(frame.payload), "%.*s%s%s",
                           (int)(match - time_ack), time_ack, replacements[i][1], match + strlen(replacements[i][0]));
        assert(len > 0 && len <= ESP_COM_MAX_PAYLOAD);
        frame.len = (uint16_t)len;
        assert(!ApiTime_ParseAck(&frame, &parsed));
        assert(memcmp(&parsed, &previous, sizeof(parsed)) == 0);
    }
    const char boundary[] = "{\"api\":\"time\",\"ok\":true,\"refresh\":false,\"realtime\":true,\"unix_time\":4294967295,\"utc_offset\":-18000,\"date\":\"2028-02-29\",\"time\":\"23:59:59\",\"weekday\":0}";
    memcpy(frame.payload, boundary, sizeof(boundary));
    frame.len = sizeof(boundary) - 1;
    assert(ApiTime_ParseAck(&frame, &parsed));
    assert(parsed.unix_time == UINT32_MAX && parsed.utc_offset == -18000 && parsed.weekday == 0);
    assert(!ApiTime_ParseAck(NULL, &parsed) && !ApiTime_ParseAck(&frame, NULL));
    ApiRefresh_Init();
    output_len = 0;
    char *raw[] = {"esp_read", "time"};
    assert(esp_read(2, raw) == HAL_OK);
    expect_request(ESP_COM_CMD_READ_API, "{\"api\":\"time\"}");
    assert(ApiRefresh_GetResult()->state == API_REFRESH_IDLE);
    puts("NTP time API and parser tests passed");
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

    char *new_apis[] = { ESP_COM_API_DAILY3D, ESP_COM_API_ALERT, ESP_COM_API_HITOKOTO, ESP_COM_API_TODOLIST, ESP_COM_API_TODOLIST_INBOX };
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
    assert(strstr(output, "todolist: today's tasks"));
    assert(strstr(output, "daily7d: removed"));
    puts("ESP communication and Shell tests passed");
    test_api_refresh();
    test_todolist(ESP_COM_API_TODOLIST, "today");
    test_todolist(ESP_COM_API_TODOLIST_INBOX, "inbox_next2d");
    test_api_time();
    return 0;
}
