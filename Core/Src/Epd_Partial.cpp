#include "Epd_Api.h"
#include "TopInfo_Project.h"
#include "ApiRefresh.h"
extern "C" {
#include "shell.h"
}

extern Shell shell;

extern "C" HAL_StatusTypeDef EPD_HW_DisplayPartial(uint16_t x, uint16_t y,
                                                  uint16_t width, uint16_t height)
{
    if (ApiRefresh_IsBusy()) return HAL_BUSY;
    return epd.refreshPartial(x, y, width, height);
}

extern "C" HAL_StatusTypeDef EPD_HW_PartialTest(void)
{
    if (ApiRefresh_IsBusy()) return HAL_BUSY;

    // 全屏黑色作为底图，只局刷一次白色矩形，保留结果供观察。
    epd.init();
    HAL_StatusTypeDef status = EPD_GetStatus();
    if (status == HAL_OK) {
        epd.fillScreen(EPD_BLACK);
        epd.refresh();
        status = EPD_GetStatus();
    }
    shellPrint(&shell, "partial black base status=%d\r\n", (int)status);

    if (status == HAL_OK) {
        shellWriteString(&shell, "black base ready; wait 5000 ms before one white partial update\r\n");
        // 全刷已等待 BUSY 结束，再留出五秒观察黑色底图。
        const uint32_t pause = HAL_GetTick();
        while (static_cast<uint32_t>(HAL_GetTick() - pause) < 5000) {
            EspCom_Poll();
            HAL_Delay(1);
        }
        epd.fillRect(80, 200, 200, 50, EPD_WHITE);
        const uint32_t start = HAL_GetTick();
        status = EPD_HW_DisplayPartial(80, 200, 200, 50);
        shellPrint(&shell, "partial white rect x=80 y=200 w=200 h=50 status=%d elapsed_ms=%lu\r\n",
                   (int)status,
                   (unsigned long)(HAL_GetTick() - start));
    }
    epd.sleep();
    if (status == HAL_OK) status = EPD_GetStatus();
    // 保留面板上的测试结果；清理 MCU 缓冲，避免下次 ref_epd 遗留测试图案。
    epd.fillScreen(EPD_WHITE);
    return status;
}
