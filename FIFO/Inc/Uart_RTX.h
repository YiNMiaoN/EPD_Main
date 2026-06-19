//
// Created by YinMi on 2026/4/7.
//

#ifndef UART_CY_FIFO_UART_RTX_H
#define UART_CY_FIFO_UART_RTX_H

#include "usart.h"
#include "FIFO.h"


void Uart_Init_TI(UART_HandleTypeDef *huart);

void Uart_Init_DMA();

// void Uart_Rx_Data_Process();
void UART_DMA_Poll(void);


#endif //UART_CY_FIFO_UART_RTX_H