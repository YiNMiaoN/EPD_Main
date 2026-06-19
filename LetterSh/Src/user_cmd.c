//
// Created by YinMi on 2026/5/17.
//


#include "Epd_Api.h"
#include "shell.h"

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