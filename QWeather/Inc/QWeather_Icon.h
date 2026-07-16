//
// QWeather icon package reader.
//

#ifndef TOPINFO_QWEATHER_ICON_H
#define TOPINFO_QWEATHER_ICON_H

#include "stm32f4xx_hal.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define QWEATHER_ICON_FLASH_BASE      0x00265000UL
#define QWEATHER_ICON_MAGIC           0x43495751UL
#define QWEATHER_ICON_VERSION         1U
#define QWEATHER_ICON_HEADER_SIZE     32U
#define QWEATHER_ICON_ENTRY_SIZE      32U

#define QWEATHER_ICON_CODE_MAX_LEN    15U
#define QWEATHER_ICON_SIZE_16         16U
#define QWEATHER_ICON_SIZE_32         32U
#define QWEATHER_ICON_BYTES_16        32U
#define QWEATHER_ICON_BYTES_32        128U

typedef enum {
    QWEATHER_ICON_OK = 0,
    QWEATHER_ICON_ERROR_FLASH = -1,
    QWEATHER_ICON_ERROR_FORMAT = -2,
    QWEATHER_ICON_ERROR_ARG = -3,
    QWEATHER_ICON_ERROR_NOT_FOUND = -4,
    QWEATHER_ICON_ERROR_SIZE = -5,
} QWeatherIcon_Status;

typedef struct {
    uint32_t icon_count;
    uint32_t data_offset;
    uint32_t data_len;
} QWeatherIcon_Info;

// Read and validate the QWIC icon package header from external Flash.
// Call once after SPI Flash is ready. Other APIs will also lazy-init if needed.
QWeatherIcon_Status QWeatherIcon_Init(void);

// Get basic package information after QWeatherIcon_Init().
QWeatherIcon_Status QWeatherIcon_GetInfo(QWeatherIcon_Info *info);

// Read one icon bitmap by QWeather icon code, for example "100" or "101".
// size must be QWEATHER_ICON_SIZE_16 or QWEATHER_ICON_SIZE_32.
// bitmap_len must be at least QWEATHER_ICON_BYTES_16 or QWEATHER_ICON_BYTES_32.
QWeatherIcon_Status QWeatherIcon_ReadBitmap(const char *code,
                                            uint16_t size,
                                            uint8_t *bitmap,
                                            uint16_t bitmap_len);

// Draw one icon into the EPD GRAM buffer. Call EPD_HW_Display() afterward
// to refresh the e-paper screen.
QWeatherIcon_Status QWeatherIcon_Draw(const char *code,
                                      uint16_t size,
                                      int16_t x,
                                      int16_t y);

// Convert a QWeatherIcon_Status value to a short debug string.
const char *QWeatherIcon_StatusString(QWeatherIcon_Status status);

#ifdef __cplusplus
}
#endif

#endif // TOPINFO_QWEATHER_ICON_H
