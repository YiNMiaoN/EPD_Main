//
// Created by YinMi on 2026/4/3.
//

#ifndef UART_CY_FIFO_FIFO_H
#define UART_CY_FIFO_FIFO_H
#include  "stm32f4xx_hal.h"
#include <stdbool.h>

#define FIFO_SIZE 8192

typedef struct
{
    uint8_t buffer[FIFO_SIZE];
    volatile uint16_t head;
    volatile uint16_t tail;
} Fifo_Type;

void Fifo_Init(Fifo_Type *fifo);
bool Fifo_Is_Empty(Fifo_Type *fifo);
bool Fifo_Is_Full(Fifo_Type *fifo);
bool Fifo_Push(Fifo_Type *fifo, uint8_t data);
bool Fifo_Pop(Fifo_Type *fifo, uint8_t *data);




#endif //UART_CY_FIFO_FIFO_H