//
// Created by YinMi on 2026/5/13.
//

#include "Epd_Api.h"
extern "C" {
#include "shell.h"
}
#include "TopInfo_Project.h"
#include "ApiRefresh.h"
#include <cstring>

extern Shell shell;

// C++对象

extern "C" {
//墨水屏操作C接口
    void EPD_HW_Init(void)
    {
        epd.init();
    }

    void EPD_HW_Clear(void)
    {
        epd.clear();
    }
    void EPD_HW_Sleep(void)
    {
        epd.sleep();
    }
    void EPD_HW_Display()
    {
        epd.refresh();
    }
    void EPD_UI_Refresh(void)
    {
        mainUI.refresh();
    }

    void EPD_UI_Poll(void)
    {
        static bool handled = false;
        const ApiRefresh_Result *result = ApiRefresh_GetResult();
        if (result->state != API_REFRESH_SUCCEEDED) {
            handled = false;
            return;
        }
        if (handled) return;
        handled = true;
        if (!result->has_frame || std::strcmp(result->api, ESP_COM_API_HITOKOTO) != 0) return;
        if (!mainUI.setHitokotoCache(reinterpret_cast<const char *>(result->frame.payload), result->frame.len)) {
            shellWriteString(&shell, "hitokoto UI: invalid cache; previous text retained\r\n");
            return;
        }
        mainUI.refresh();
        epd.sleep();
    }
//Flash操作C接口
    void EPD_Flash_Init(void)
    {
        flash.init();
    }
    void EPD_Flash_Test(void)
    {
        flash.test();
    }
    void EPD_Flash_Read(uint32_t address, uint8_t *data, uint16_t size)
    {
        flash.read(address, data, size);
    }
    void EPD_Flash_Write(uint32_t address, uint8_t *data, uint16_t size)
    {
        flash.write(address, data, size);
    }
    void EPD_Flash_Erase(uint32_t address, uint16_t size)
    {
        flash.erase(address);
    }



}
