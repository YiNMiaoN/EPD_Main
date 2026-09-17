#ifndef TEST_GPIO_H
#define TEST_GPIO_H
#include <stdint.h>
typedef enum { GPIO_PIN_RESET, GPIO_PIN_SET } GPIO_PinState;
#define STM32_Ready_GPIO_Port ((void *)0)
#define ESP_Ready_GPIO_Port ((void *)0)
#define STM32_Ready_Pin 1
#define ESP_Ready_Pin 2
void HAL_GPIO_WritePin(void *, uint16_t, GPIO_PinState);
GPIO_PinState HAL_GPIO_ReadPin(void *, uint16_t);
#endif
