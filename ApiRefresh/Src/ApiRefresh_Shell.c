#include "ApiRefresh.h"
#include "shell.h"

extern Shell shell;

int api_refresh(int argc, char *argv[])
{
    if (argc > 2) {
        shellWriteString(&shell, "usage: api_refresh [current|daily3d|minutely5m|alert|hitokoto]\r\n");
        return HAL_ERROR;
    }
    HAL_StatusTypeDef status = ApiRefresh_Start(argc == 2 ? argv[1] : NULL);
    shellPrint(&shell, "api refresh start=%d\r\n", (int)status);
    return (int)status;
}

SHELL_EXPORT_CMD(
    SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN),
    api_refresh, api_refresh, api_refresh [api]
);

int api_result(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    const ApiRefresh_Result *result = ApiRefresh_GetResult();
    shellPrint(&shell, "api=%s state=%s\r\n", result->api,
               ApiRefresh_StateString(result->state));
    shellPrint(&shell, "error=%s stage=%s tx=%d\r\n",
               ApiRefresh_ErrorString(result->error),
               ApiRefresh_StateString(result->failed_stage), (int)result->tx_status);
    if (result->has_frame) {
        shellPrint(&shell, "type=0x%02X seq=%u len=%u payload=",
                   (unsigned)result->frame.type, (unsigned)result->frame.seq,
                   (unsigned)result->frame.len);
        shell.write((char *)result->frame.payload, result->frame.len);
        shellWriteString(&shell, "\r\n");
    }
    return 0;
}

SHELL_EXPORT_CMD(
    SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN),
    api_result, api_result, api_result
);
