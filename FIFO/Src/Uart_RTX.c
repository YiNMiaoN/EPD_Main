//
// Created by YinMi on 2026/4/7.
//

#include "Uart_RTX.h"
#include "Esp_Com.h"


extern Fifo_Type Fifo_Uart_Rx;





//中断单字节接收
uint8_t Temp_Data_Bety;

void Uart_Init_TI(UART_HandleTypeDef *huart) {
    HAL_UART_Receive_IT(huart, &Temp_Data_Bety, 1);
}

void Uart_Rx_CCB_TI(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART1) {
        HAL_UART_Receive_IT(&huart1, &Temp_Data_Bety,1);
        Fifo_Push(&Fifo_Uart_Rx, Temp_Data_Bety);
        HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_2);
    }
}
//测试中断
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART2) {
        EspCom_UartRxCpltCallback(huart);
    } else {
        Uart_Rx_CCB_TI(huart);
    }
}


//DMA接收循环接收至FIFO
#define Temp_Buff_Size 10240
uint8_t Temp_Buff_Bety_DMA[Temp_Buff_Size];
uint16_t Temp_Buff_Size_DMA = 0;
uint8_t Uart_Rx_Flag = 0x00;
static uint16_t Old_Pos = 0;


void Uart_Init_DMA() {
    HAL_UART_Receive_DMA(&huart1, Temp_Buff_Bety_DMA, Temp_Buff_Size);
}

void UART_DMA_Poll(void)
{
    uint16_t pos = Temp_Buff_Size - __HAL_DMA_GET_COUNTER(huart1.hdmarx);

    if (pos != Old_Pos)
    {
        if (pos > Old_Pos)
        {
            // 正常段
            for (uint16_t i = Old_Pos; i < pos; i++)
            {
                Fifo_Push(&Fifo_Uart_Rx, Temp_Buff_Bety_DMA[i]);
            }
        }
        else
        {
            // 回绕段
            for (uint16_t i = Old_Pos; i < Temp_Buff_Size; i++)
            {
                Fifo_Push(&Fifo_Uart_Rx, Temp_Buff_Bety_DMA[i]);
            }
            for (uint16_t i = 0; i < pos; i++)
            {
                Fifo_Push(&Fifo_Uart_Rx, Temp_Buff_Bety_DMA[i]);
            }
        }

        Old_Pos = pos;
    }
}

/* 废案，量数据大传输丢包 DMA不使用循环模式，但在小数据量下，响应性不错
void Uart_Init_DMA() {
    HAL_UARTEx_ReceiveToIdle_DMA(&huart1, Temp_Buff_Bety_DMA, Temp_Buff_Size);
}

void Uart_Rx_CCB_DMA(UART_HandleTypeDef *huart, uint16_t size) {
    if (huart->Instance == USART1) {
        Uart_Rx_Flag |= 0x01;
        Temp_Buff_Size_DMA = size;
        HAL_UARTEx_ReceiveToIdle_DMA(&huart1, Temp_Buff_Bety_DMA, Temp_Buff_Size);
    }

}

void Uart_Rx_Data_Process() {
    switch (Uart_Rx_Flag) {
        case 0x01:
            for (int i = 0; i < Temp_Buff_Size_DMA; ++i) {
                Fifo_Push(&Fifo_Uart_Rx, Temp_Buff_Bety_DMA[i]);
            }
            Uart_Rx_Flag &= ~0x01;
            break;
        default:
            break;
    }
}

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t size){
    Uart_Rx_CCB_DMA(huart,size);
}
*/




