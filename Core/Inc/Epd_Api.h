//
// Created by YinMi on 2026/5/13.
//

#ifndef TOPINFO_EPD_API_H
#define TOPINFO_EPD_API_H


#ifndef EPD_API_H
#define EPD_API_H
#include "stm32f4xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

    void EPD_HW_Init(void);
    void EPD_HW_Clear(void);
    void EPD_HW_Sleep(void);
    void EPD_HW_Display();
    // 刷新现有整屏缓冲中的矩形；不绘图、不自动初始化或休眠。
    HAL_StatusTypeDef EPD_HW_DisplayPartial(uint16_t x, uint16_t y, uint16_t width, uint16_t height);
    HAL_StatusTypeDef EPD_HW_PartialTest(void);
    // 全刷业务界面；时钟运行时保留局刷底图，否则刷新后休眠。
    void EPD_UI_Refresh(void);
    void EPD_UI_Poll(void);


    void EPD_Flash_Test(void);
    void EPD_Flash_Init(void);
    void EPD_Flash_Erase(uint32_t address, uint16_t size);
    void EPD_Flash_Read(uint32_t address, uint8_t *data, uint16_t size);
    void EPD_Flash_Write(uint32_t address, uint8_t *data, uint16_t size);


#ifdef __cplusplus
}
#endif

#endif



#endif //TOPINFO_EPD_API_H
