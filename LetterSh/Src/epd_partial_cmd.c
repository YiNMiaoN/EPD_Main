#include "Epd_Api.h"
#include "shell.h"

extern Shell shell;

int epd_partial_test(int argc, char *argv[])
{
    (void)argv;
    if (argc != 1) {
        shellWriteString(&shell, "usage: epd_partial_test\r\n");
        return HAL_ERROR;
    }
    HAL_StatusTypeDef status = EPD_HW_PartialTest();
    shellPrint(&shell, "partial test status=%d (0=ok 1=error 2=busy 3=timeout)\r\n",
               (int)status);
    return (int)status;
}

SHELL_EXPORT_CMD(
    SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN),
    epd_partial_test, epd_partial_test, full black then wait 5 seconds and partial white once
);
