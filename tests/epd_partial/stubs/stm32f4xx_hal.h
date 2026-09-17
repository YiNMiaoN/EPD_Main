#pragma once
#include <cstdint>
enum HAL_StatusTypeDef { HAL_OK, HAL_ERROR, HAL_BUSY, HAL_TIMEOUT };
enum GPIO_PinState { GPIO_PIN_RESET, GPIO_PIN_SET };
struct SPI_HandleTypeDef {};
#define SPI1_RES_GPIO_Port 0
#define SPI1_RES_Pin 1
#define SPI1_DC_GPIO_Port 0
#define SPI1_DC_Pin 2
#define SPI1_NSS_GPIO_Port 0
#define SPI1_NSS_Pin 3
#define SPI1_BUSY_GPIO_Port 0
#define SPI1_BUSY_Pin 4
void HAL_GPIO_WritePin(int, int, GPIO_PinState);
GPIO_PinState HAL_GPIO_ReadPin(int, int);
HAL_StatusTypeDef HAL_SPI_Transmit(SPI_HandleTypeDef *, uint8_t *, uint16_t, uint32_t);
void HAL_Delay(uint32_t);
uint32_t HAL_GetTick();
