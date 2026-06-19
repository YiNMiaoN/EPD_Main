//
// Created by YinMi on 2026/5/17.
//


#include "Epd_Api.h"
#include "Esp_Com.h"
#include "shell.h"
#include <string.h>

extern Shell shell;

void ref_epd(int argc, char *argv[])
{
    shellPrint(&shell, "ref_epd\r\n");
    EPD_HW_Display();
    EPD_HW_Sleep();
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
                   "type=0x%02X seq=%u len=%u payload=%s\r\n",
                   frame.type,
                   frame.seq,
                   frame.len,
                   frame.len ? (char *)frame.payload : "");
        EspCom_ClearFrame();
    } else {
        shellPrint(&shell, "no esp frame\r\n");
    }
}

void esp_ready(int argc, char *argv[])
{
    if (argc >= 2) {
        EspCom_SetReady(strcmp(argv[1], "0") != 0);
    }

    shellPrint(&shell, "stm32_ready set, esp_ready=%u\r\n", EspCom_IsEspReady() ? 1 : 0);
}

SHELL_EXPORT_CMD(
    SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN),
    esp_ready,
    esp_ready,
    esp_ready [0|1]
    );

void esp_ping(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    shellPrint(&shell, "esp ping tx=%d\r\n", EspCom_Ping());
}

SHELL_EXPORT_CMD(
    SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN),
    esp_ping,
    esp_ping,
    esp_ping
    );

void esp_status(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    shellPrint(&shell, "esp status tx=%d esp_ready=%u\r\n", EspCom_GetStatus(), EspCom_IsEspReady() ? 1 : 0);
}

SHELL_EXPORT_CMD(
    SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN),
    esp_status,
    esp_status,
    esp_status
    );

void esp_cache(int argc, char *argv[])
{
    const char *api = argc >= 2 ? argv[1] : "current";
    shellPrint(&shell, "esp cache %s tx=%d\r\n", api, EspCom_GetCache(api));
}

SHELL_EXPORT_CMD(
    SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN),
    esp_cache,
    esp_cache,
    esp_cache [api]
    );

void esp_refresh(int argc, char *argv[])
{
    const char *api = argc >= 2 ? argv[1] : "current";
    shellPrint(&shell, "esp refresh %s tx=%d\r\n", api, EspCom_RefreshApi(api));
}

SHELL_EXPORT_CMD(
    SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN),
    esp_refresh,
    esp_refresh,
    esp_refresh [api]
    );

void esp_read(int argc, char *argv[])
{
    const char *api = argc >= 2 ? argv[1] : "daily7d";
    shellPrint(&shell, "esp read %s tx=%d\r\n", api, EspCom_ReadApi(api));
}

SHELL_EXPORT_CMD(
    SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN),
    esp_read,
    esp_read,
    esp_read [api]
    );

void esp_last(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    esp_print_last_frame();
}

SHELL_EXPORT_CMD(
    SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN),
    esp_last,
    esp_last,
    esp_last
    );
