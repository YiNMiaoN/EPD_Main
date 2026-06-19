//
// Created by YinMi on 2026/4/7.
//

#include "Uart_OTA.h"
#include "Epd_Api.h"
#include <string.h>


extern Fifo_Type Fifo_Uart_Rx;

uint8_t OTA_Flag = 0x00;
uint16_t OTA_Data_Len = 0x00;
uint8_t OTA_Data[1024];
uint8_t OTA_Data_Flash[1024];
uint16_t OTA_Data_Crc16 = 0x00;
uint16_t OTA_Data_Index = 0x00;
uint8_t OTA_Data_Realy = 0x00;
volatile uint32_t Flash_Write_Addr = 0x00;

uint8_t OTA_St_Realy[] = {0x55,0xAA,0x01};
uint8_t OTA_St_Wait[] = {0x55,0xAA,0x02};
uint8_t OTA_St_Error[] = {0x55,0xAA,0x03};

enum OTA_Tx_St{
    Realy = 1,
    Wait,
    Error
};

void OTA_Tx_St(uint8_t st) {
    switch (st) {
        case Realy:
            HAL_UART_Transmit(&huart1, OTA_St_Realy, 3, 1000);
            break;
        case Wait:
            HAL_UART_Transmit(&huart1, OTA_St_Wait, 3, 1000);
            break;
        case Error:
            HAL_UART_Transmit(&huart1, OTA_St_Error, 3, 1000);

            break;
        default:
            break;
    }
}


uint16_t CRC16_Modbus(uint8_t *buf, uint16_t len)
{
    uint16_t crc = 0xFFFF;

    for (uint16_t i = 0; i < len; i++)
    {
        crc ^= buf[i];

        for (uint8_t j = 0; j < 8; j++)
        {
            if (crc & 0x0001)
                crc = (crc >> 1) ^ 0xA001;
            else
                crc >>= 1;
        }
    }
    return crc;
}


void Uart_OTA_Rx() {
    uint8_t data;
    if (Fifo_Is_Empty(&Fifo_Uart_Rx)) {
        return;
    }
    Fifo_Pop(&Fifo_Uart_Rx,&data);
    switch (OTA_Flag){
        case 0x00:
            if (data == 0x55) {
                OTA_Flag = 0x01;
            }else {
                OTA_Flag = 0x00;
            }
            break;
        case 0x01:
            if (data == 0xAA) {
                OTA_Flag = 0x02;
            }
            break;
        case 0x02:
            OTA_Data_Len = (uint16_t)data << 8;
            OTA_Data_Index = 0x00;
            OTA_Data[0] = data;
            OTA_Flag = 0x03;
            break;
        case 0x03:
            OTA_Data_Len |= data;
            OTA_Data[1] = data;

            if (OTA_Data_Len == 0 || OTA_Data_Len > 512) {
                OTA_Flag = 0x00;
            }
            OTA_Data_Index = 0;
            OTA_Flag = 0x04;
            break;
        case 0x04:
            OTA_Data[OTA_Data_Index + 2] = data;
            OTA_Data_Flash[OTA_Data_Index] = data;
            OTA_Data_Index++;
            if (OTA_Data_Index >= OTA_Data_Len)
                OTA_Flag = 0x05;
            break;
        case 0x05:
            OTA_Data_Crc16 = (uint16_t)data<<8;
            OTA_Flag = 0x06;
            break;
        case 0x06:
            OTA_Data_Crc16 |= data;
            if (CRC16_Modbus(OTA_Data, OTA_Data_Len+2) == OTA_Data_Crc16) {
                OTA_Data_Realy = 1;
                if ((Flash_Write_Addr % 4096) == 0)
                {
                    EPD_Flash_Erase(Flash_Write_Addr,1);
                }
                EPD_Flash_Write(Flash_Write_Addr, OTA_Data_Flash, OTA_Data_Len);
                Flash_Write_Addr += 256;
                OTA_Tx_St(Realy);
            }else {
                OTA_Tx_St(Error);
                OTA_Flag = 0x00;
            }
            OTA_Flag = 0x00;

            break;
        default:
            break;
    }

}