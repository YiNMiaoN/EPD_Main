//
// Created by YinMi on 2026/5/17.
//


#include "Epd_Api.h"
#include "Esp_Com.h"
#include "ApiRefresh.h"
#include "shell.h"
#include <string.h>

extern Shell shell;

static bool esp_manual_allowed(void)
{
    if (ApiRefresh_IsBusy()) {
        shellWriteString(&shell, "API refresh active; use api_result to inspect progress\r\n");
        return false;
    }
    return true;
}

int ref_epd(int argc, char *argv[])
{
    if (!esp_manual_allowed()) return HAL_BUSY;
    (void)argc;
    (void)argv;
    shellPrint(&shell, "ref_epd\r\n");
    EPD_UI_Refresh();
    EPD_HW_Sleep();
    return 0;
}

SHELL_EXPORT_CMD(
    SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN),
    ref_epd,
    ref_epd,
    ref_epd
    );

static void esp_print_last_frame(void)
{
    EspCom_Frame frame;

    EspCom_Poll();
    if (EspCom_GetLastFrame(&frame)) {
        shellPrint(&shell,
                   "type=0x%02X seq=%u len=%u payload=",
                   (unsigned int)frame.type,
                   (unsigned int)frame.seq,
                   (unsigned int)frame.len);
        // Keep the payload outside shellPrint's small formatting buffer.
        if (frame.len > 0U) {
            shell.write((char *)frame.payload, frame.len);
        }
        shellWriteString(&shell, "\r\n");
        EspCom_ClearFrame();
    } else {
        shellPrint(&shell, "no esp frame\r\n");
    }
}

int esp_ready(int argc, char *argv[])
{
    if (argc >= 2) {
        if (!esp_manual_allowed()) return HAL_BUSY;
        if (argc != 2 || (strcmp(argv[1], "0") != 0 && strcmp(argv[1], "1") != 0)) {
            shellWriteString(&shell, "usage: esp_ready [0|1]\r\n");
            return HAL_ERROR;
        }
        EspCom_SetReady(strcmp(argv[1], "1") == 0);
        shellPrint(&shell, "stm32_ready=%s ", argv[1]);
    }

    shellPrint(&shell, "esp_ready=%u\r\n", EspCom_IsEspReady() ? 1U : 0U);
    return 0;
}

SHELL_EXPORT_CMD(
    SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN),
    esp_ready,
    esp_ready,
    esp_ready [0|1]
    );

int esp_ping(int argc, char *argv[])
{
    if (!esp_manual_allowed()) return HAL_BUSY;
    (void)argc;
    (void)argv;
    HAL_StatusTypeDef tx = EspCom_Ping();
    shellPrint(&shell, "esp ping tx=%d\r\n", (int)tx);
    return (int)tx;
}

SHELL_EXPORT_CMD(
    SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN),
    esp_ping,
    esp_ping,
    esp_ping
    );

int esp_status(int argc, char *argv[])
{
    if (!esp_manual_allowed()) return HAL_BUSY;
    (void)argc;
    (void)argv;
    HAL_StatusTypeDef tx = EspCom_GetStatus();
    shellPrint(&shell, "esp status tx=%d esp_ready=%u\r\n", (int)tx, EspCom_IsEspReady() ? 1U : 0U);
    return (int)tx;
}

SHELL_EXPORT_CMD(
    SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN),
    esp_status,
    esp_status,
    esp_status
    );

static bool esp_business_ready(void)
{
    if (!esp_manual_allowed()) return false;
    if (!EspCom_IsEspReady()) {
        shellWriteString(&shell, "ESP not ready (offline or refreshing); command not sent\r\n");
        return false;
    }
    return true;
}

int esp_cache(int argc, char *argv[])
{
    const char *api = argc >= 2 ? argv[1] : ESP_COM_API_CURRENT;
    if (!esp_business_ready()) {
        return HAL_BUSY;
    }
    HAL_StatusTypeDef tx = EspCom_GetCache(api);
    shellPrint(&shell, "esp cache %s tx=%d\r\n", api, (int)tx);
    return (int)tx;
}

SHELL_EXPORT_CMD(
    SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN),
    esp_cache,
    esp_cache,
    esp_cache [api]
    );

int esp_refresh(int argc, char *argv[])
{
    const char *api = argc >= 2 ? argv[1] : ESP_COM_API_CURRENT;
    if (!esp_business_ready()) {
        return HAL_BUSY;
    }
    HAL_StatusTypeDef tx = EspCom_RefreshApi(api);
    shellPrint(&shell, "esp refresh %s tx=%d\r\n", api, (int)tx);
    return (int)tx;
}

SHELL_EXPORT_CMD(
    SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN),
    esp_refresh,
    esp_refresh,
    esp_refresh [api]
    );

int esp_read(int argc, char *argv[])
{
    const char *api = argc >= 2 ? argv[1] : ESP_COM_API_DAILY3D;
    if (!esp_business_ready()) {
        return HAL_BUSY;
    }
    HAL_StatusTypeDef tx = EspCom_ReadApi(api);
    shellPrint(&shell, "esp read %s tx=%d\r\n", api, (int)tx);
    return (int)tx;
}

SHELL_EXPORT_CMD(
    SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN),
    esp_read,
    esp_read,
    esp_read [api] (time: live NTP)
    );

int esp_apis(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    shellWriteString(&shell, "current: direct QWeather refresh + local cache\r\n");
    shellWriteString(&shell, "daily3d: three-day forecast; numeric temp_max/temp_min in Celsius\r\n");
    shellWriteString(&shell, "minutely5m: refresh + summary; ESP longitude,latitude config required\r\n");
    shellWriteString(&shell, "alert: refresh + warning summary; valid=true,count=0 means no warnings\r\n");
    shellWriteString(&shell, "hitokoto: refresh + quote; hitokoto/from/from_who (author may be null)\r\n");
    shellWriteString(&shell, "time: live NTP via esp_read time; ACK contains time; no cache/refresh\r\n");
    shellWriteString(&shell, "api_time: tracked NTP request; inspect parsed time with api_result\r\n");
    shellWriteString(&shell, "response: last completed refresh result; inspect valid and ok\r\n");
    shellWriteString(&shell, "hourly72h/status: cache support only, no active data source\r\n");
    shellWriteString(&shell, "airquality/indices/sun/moon: UART cache not implemented\r\n");
    shellWriteString(&shell, "daily7d: removed by ESP; use daily3d explicitly\r\n");
    shellWriteString(&shell, "Refresh: wait for ACK and ESP_Ready HIGH, then query response and target cache\r\n");
    shellWriteString(&shell, "esp_read defaults to daily3d and only checks local cache; startup caches are empty\r\n");
    return 0;
}

SHELL_EXPORT_CMD(
    SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN),
    esp_apis,
    esp_apis,
    esp_apis
    );

int esp_last(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    esp_print_last_frame();
    return 0;
}

SHELL_EXPORT_CMD(
    SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN),
    esp_last,
    esp_last,
    esp_last
    );
