//
// Created by YinMi on 2026/5/13.
//

#ifndef TOPINFO_EPD_HARDWARE_H
#define TOPINFO_EPD_HARDWARE_H
#include "stm32f4xx_hal.h"

#define EPD_Fast_Mode   0
#define EPD_WIDTH       400
#define EPD_HEIGHT      300

#define EPD_BLACK 0
#define EPD_WHITE 1

/**
 * e-Paper GPIO
**/
#define EPD_RST_PIN     SPI1_RES_GPIO_Port, SPI1_RES_Pin
#define EPD_DC_PIN      SPI1_DC_GPIO_Port, SPI1_DC_Pin
// #define EPD_PWR_PIN     PWR_GPIO_Port, PWR_Pin
#define EPD_CS_PIN      SPI1_NSS_GPIO_Port, SPI1_NSS_Pin
#define EPD_BUSY_PIN    SPI1_BUSY_GPIO_Port, SPI1_BUSY_Pin
// #define EPD_MOSI_PIN    DIN_GPIO_Port, DIN_Pin
// #define EPD_SCLK_PIN    SCK_GPIO_Port, SCK_Pin

/**
 * GPIO read and write
**/
#define DEV_Digital_Write(_pin, _value) HAL_GPIO_WritePin(_pin, _value == 0? GPIO_PIN_RESET:GPIO_PIN_SET)
#define DEV_Digital_Read(_pin) HAL_GPIO_ReadPin(_pin)
extern uint8_t gImage_4in2[];


void EPD_Init();
void EPD_Clear();
void EPD_Sleep();
void EPD_Display(uint8_t *Image);
// 最近一次硬件操作的结果；通信失败后须重新初始化并全刷。
HAL_StatusTypeDef EPD_GetStatus(void);
// 复位、休眠或通信失败后底图无效，须先全刷。
bool EPD_CanRefreshPartial(void);
// 整屏缓冲固定为 400×300、每行 50 字节；x 和 width 必须为 8 的倍数。
HAL_StatusTypeDef EPD_DisplayPartial(const uint8_t *Image, uint16_t x, uint16_t y,
                                     uint16_t width, uint16_t height);
// 紧密排列的局部图像；终点不包含在区域内，X 起止均须字节对齐。
HAL_StatusTypeDef EPD_PartialDisplay(uint8_t *Image, uint32_t Xstart, uint32_t Ystart,
                                    uint32_t Xend, uint32_t Yend);



#endif //TOPINFO_EPD_HARDWARE_H
