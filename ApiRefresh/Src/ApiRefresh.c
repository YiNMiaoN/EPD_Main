#include "ApiRefresh.h"
#include "SystemHeartbeat.h"
#include "core_json.h"
#include <string.h>

static ApiRefresh_Result result;
static bool initialized;
static bool pending;
static uint8_t expected_seq;
static uint32_t stage_tick;
static uint32_t received_tick;

void ApiRefresh_Tick1ms(void)
{
    SystemHeartbeat_Tick1ms();
}

typedef struct {
    bool api_matches;
    bool stage_matches;
    JSONTypes_t ok;
    JSONTypes_t valid;
    JSONTypes_t refresh;
} ResponseFields;

static void fail(ApiRefresh_Error error)
{
    result.failed_stage = result.state;
    result.error = error;
    result.state = API_REFRESH_FAILED;
    pending = false;
}

bool ApiRefresh_IsBusy(void)
{
    return result.state >= API_REFRESH_WAIT_READY && result.state <= API_REFRESH_WAIT_CACHE;
}

static uint32_t stage_timeout(void)
{
    switch (result.state) {
        case API_REFRESH_WAIT_READY:
        case API_REFRESH_WAIT_CACHE_READY:
            return API_REFRESH_READY_TIMEOUT_MS;
        case API_REFRESH_WAIT_FINISH:
            return API_REFRESH_FETCH_TIMEOUT_MS;
        case API_REFRESH_WAIT_ACK:
            if (strcmp(result.api, ESP_COM_API_TIME) == 0) return API_TIME_REPLY_TIMEOUT_MS;
            return API_REFRESH_REPLY_TIMEOUT_MS;
        default:
            return API_REFRESH_REPLY_TIMEOUT_MS;
    }
}

static void observe_frame(const EspCom_Frame *frame)
{
    if ((result.state == API_REFRESH_WAIT_ACK ||
         result.state == API_REFRESH_WAIT_RESULT ||
         result.state == API_REFRESH_WAIT_CACHE) &&
        frame->seq == expected_seq && !pending) {
        result.frame = *frame;
        result.has_frame = true;
        received_tick = SystemHeartbeat_Millis();
        pending = true;
    }
}

void ApiRefresh_Init(void)
{
    memset(&result, 0, sizeof(result));
    pending = false;
    initialized = true;
    EspCom_SetFrameObserver(observe_frame);
}

static HAL_StatusTypeDef queue_request(const char *api)
{
    if (!initialized) return HAL_ERROR;
    if (ApiRefresh_IsBusy()) return HAL_BUSY;
    memset(&result, 0, sizeof(result));
    strcpy(result.api, api);
    result.state = API_REFRESH_WAIT_READY;
    stage_tick = SystemHeartbeat_Millis();
    pending = false;
    return HAL_OK;
}

HAL_StatusTypeDef ApiRefresh_StartTime(void)
{
    return queue_request(ESP_COM_API_TIME);
}

HAL_StatusTypeDef ApiRefresh_Start(const char *api)
{
    static const char *const supported[] = {
        ESP_COM_API_CURRENT, ESP_COM_API_DAILY3D, ESP_COM_API_MINUTELY5M,
        ESP_COM_API_ALERT, ESP_COM_API_HITOKOTO, ESP_COM_API_TODOLIST,
        ESP_COM_API_TODOLIST_INBOX
    };
    if (!initialized) {
        return HAL_ERROR;
    }
    if (ApiRefresh_IsBusy()) {
        return HAL_BUSY;
    }
    if (api == NULL) {
        api = ESP_COM_API_CURRENT;
    }
    const char *selected = NULL;
    for (unsigned i = 0; i < sizeof(supported) / sizeof(supported[0]); ++i) {
        if (strcmp(api, supported[i]) == 0) {
            selected = supported[i];
            break;
        }
    }
    if (selected == NULL) {
        return HAL_ERROR;
    }
    return queue_request(selected);
}

static bool key_is(const JSONPair_t *pair, const char *key)
{
    return pair->key != NULL && pair->keyLength == strlen(key) &&
           memcmp(pair->key, key, pair->keyLength) == 0;
}

static bool boolean_type(JSONTypes_t type)
{
    return type == JSONTrue || type == JSONFalse;
}

static const char *todo_stage(void)
{
    if (strcmp(result.api, ESP_COM_API_TODOLIST) == 0) return "today";
    if (strcmp(result.api, ESP_COM_API_TODOLIST_INBOX) == 0) return "inbox_next2d";
    return NULL;
}

static bool parse_fields(ResponseFields *fields)
{
    const char *json = (const char *)result.frame.payload;
    size_t length = result.frame.len;
    if (JSON_Validate(json, length) != JSONSuccess) {
        return false;
    }
    size_t start = 0, next = 0;
    unsigned seen = 0;
    JSONPair_t pair;
    JSONStatus_t status;
    const char *expected_stage = todo_stage();
    memset(fields, 0, sizeof(*fields));
    // Iterate only top-level pairs so nested business fields cannot pass checks.
    while ((status = JSON_Iterate(json, length, &start, &next, &pair)) == JSONSuccess) {
        if (pair.key == NULL) {
            return false;
        }
        unsigned bit = 0;
        if (key_is(&pair, "api")) {
            bit = 1U;
            if (pair.jsonType != JSONString) return false;
            fields->api_matches = pair.valueLength == strlen(result.api) &&
                                  memcmp(pair.value, result.api, pair.valueLength) == 0;
        } else if (key_is(&pair, "ok")) {
            bit = 2U;
            if (!boolean_type(pair.jsonType)) return false;
            fields->ok = pair.jsonType;
        } else if (key_is(&pair, "valid")) {
            bit = 4U;
            if (!boolean_type(pair.jsonType)) return false;
            fields->valid = pair.jsonType;
        } else if (key_is(&pair, "refresh")) {
            bit = 8U;
            if (!boolean_type(pair.jsonType)) return false;
            fields->refresh = pair.jsonType;
        } else if (expected_stage != NULL && key_is(&pair, "stage")) {
            bit = 16U;
            if (pair.jsonType != JSONString) return false;
            fields->stage_matches = pair.valueLength == strlen(expected_stage) &&
                                    memcmp(pair.value, expected_stage, pair.valueLength) == 0;
        }
        if ((seen & bit) != 0U) return false;
        seen |= bit;
    }
    return status == JSONNotFound;
}

static void handle_response(void)
{
    pending = false;
    ResponseFields fields;
    if (!parse_fields(&fields)) {
        fail(API_REFRESH_ERROR_PROTOCOL);
        return;
    }
    if (result.frame.type == ESP_COM_RSP_ERROR) {
        fail(API_REFRESH_ERROR_ESP);
        return;
    }
    if (!fields.api_matches) {
        fail(API_REFRESH_ERROR_PROTOCOL);
        return;
    }
    if (strcmp(result.api, ESP_COM_API_TIME) == 0) {
        if (!ApiTime_ParseAck(&result.frame, &result.time)) {
            fail(API_REFRESH_ERROR_PROTOCOL);
        } else {
            result.has_time = true;
            result.state = API_REFRESH_SUCCEEDED;
        }
        return;
    }
    if (result.state == API_REFRESH_WAIT_ACK) {
        if (result.frame.type != ESP_COM_RSP_ACK || !boolean_type(fields.ok) ||
            !boolean_type(fields.refresh)) {
            fail(API_REFRESH_ERROR_PROTOCOL);
        } else if (fields.ok != JSONTrue || fields.refresh != JSONTrue) {
            fail(API_REFRESH_ERROR_REJECTED);
        } else {
            result.state = API_REFRESH_WAIT_FINISH;
            stage_tick = SystemHeartbeat_Millis();
        }
        return;
    }
    if (result.frame.type != ESP_COM_RSP_CACHE || !boolean_type(fields.valid)) {
        fail(API_REFRESH_ERROR_PROTOCOL);
    } else if (fields.valid != JSONTrue) {
        fail(API_REFRESH_ERROR_INVALID_CACHE);
    } else if (todo_stage() != NULL && !fields.stage_matches) {
        // ACK has no stage. Valid result/cache frames must identify the new schema.
        fail(API_REFRESH_ERROR_PROTOCOL);
    } else if (result.state == API_REFRESH_WAIT_RESULT) {
        if (!boolean_type(fields.ok)) {
            fail(API_REFRESH_ERROR_PROTOCOL);
        } else if (fields.ok != JSONTrue) {
            fail(API_REFRESH_ERROR_REMOTE_REFRESH);
        } else {
            result.state = API_REFRESH_WAIT_CACHE_READY;
            stage_tick = SystemHeartbeat_Millis();
        }
    } else {
        result.state = API_REFRESH_SUCCEEDED;
    }
}

static void send_request(uint8_t command, const char *api, ApiRefresh_State waiting)
{
    if (command == ESP_COM_CMD_READ_API) result.tx_status = EspCom_ReadApi(api);
    else if (command == ESP_COM_CMD_REFRESH_API) result.tx_status = EspCom_RefreshApi(api);
    else result.tx_status = EspCom_GetCache(api);
    if (result.tx_status != HAL_OK) {
        fail(API_REFRESH_ERROR_TX);
        return;
    }
    expected_seq = EspCom_GetTxSequence();
    result.state = waiting;
    stage_tick = SystemHeartbeat_Millis();
}

void ApiRefresh_Poll(void)
{
    if (!ApiRefresh_IsBusy()) return;
    uint32_t now = SystemHeartbeat_Millis();
    // A frame parsed before its deadline can be handled on the next loop pass.
    uint32_t elapsed = (pending ? received_tick : now) - stage_tick;
    if (elapsed >= stage_timeout()) {
        fail(API_REFRESH_ERROR_TIMEOUT);
        return;
    }
    if (pending) {
        handle_response();
        return;
    }
    if (!EspCom_IsEspReady()) return;
    switch (result.state) {
        case API_REFRESH_WAIT_READY:
            send_request(strcmp(result.api, ESP_COM_API_TIME) == 0 ? ESP_COM_CMD_READ_API :
                         ESP_COM_CMD_REFRESH_API, result.api, API_REFRESH_WAIT_ACK);
            break;
        case API_REFRESH_WAIT_FINISH:
            // ESP may already have finished before we sample Ready. Do not require
            // observing LOW, but leave an ACK settling interval before querying.
            if ((uint32_t)(now - stage_tick) >= API_REFRESH_SETTLE_MS) {
                send_request(ESP_COM_CMD_GET_CACHE, ESP_COM_API_RESPONSE, API_REFRESH_WAIT_RESULT);
            }
            break;
        case API_REFRESH_WAIT_CACHE_READY:
            send_request(ESP_COM_CMD_GET_CACHE, result.api, API_REFRESH_WAIT_CACHE);
            break;
        default:
            break;
    }
}

const ApiRefresh_Result *ApiRefresh_GetResult(void)
{
    return &result;
}

const char *ApiRefresh_StateString(ApiRefresh_State state)
{
    switch (state) {
        case API_REFRESH_IDLE: return "idle";
        case API_REFRESH_WAIT_READY: return "wait_ready";
        case API_REFRESH_WAIT_ACK: return "wait_ack";
        case API_REFRESH_WAIT_FINISH: return "wait_finish";
        case API_REFRESH_WAIT_RESULT: return "wait_result";
        case API_REFRESH_WAIT_CACHE_READY: return "wait_cache_ready";
        case API_REFRESH_WAIT_CACHE: return "wait_cache";
        case API_REFRESH_SUCCEEDED: return "succeeded";
        case API_REFRESH_FAILED: return "failed";
        default: return "unknown";
    }
}

const char *ApiRefresh_ErrorString(ApiRefresh_Error error)
{
    switch (error) {
        case API_REFRESH_ERROR_NONE: return "none";
        case API_REFRESH_ERROR_TX: return "tx_failed";
        case API_REFRESH_ERROR_TIMEOUT: return "timeout";
        case API_REFRESH_ERROR_PROTOCOL: return "invalid_response";
        case API_REFRESH_ERROR_ESP: return "esp_error";
        case API_REFRESH_ERROR_REJECTED: return "rejected";
        case API_REFRESH_ERROR_REMOTE_REFRESH: return "remote_refresh_failed";
        case API_REFRESH_ERROR_INVALID_CACHE: return "invalid_cache";
        default: return "unknown";
    }
}
