//
// Created by YinMi on 2026/5/15.
//

#include "Flash.h"

#include <cstring>

#include "spi.h"
#include "usart.h"

Flash::Flash() {

}

Flash::~Flash() {

}

uint8_t Flash_Id_Cmd = 0x9F;
uint8_t Flash_Write_Cmd = 0x02;
uint8_t Flash_Read_Cmd = 0x03;
uint8_t Flash_Erase_Cmd = 0xD8;
uint8_t Flash_Status_Cmd = 0x05;
uint8_t Flash_Dummy_Cmd = 0xFF;

uint8_t Flash::Flash_SPI_Tx(uint8_t data) {
    return HAL_SPI_Transmit(this->hspi,&data,1,100);
}

uint8_t Flash::Flash_SPI_Tx(uint8_t *data, uint16_t size) {
    return HAL_SPI_Transmit(this->hspi,data,size,100);
}

uint8_t Flash::Flash_SPI_Rx(uint8_t data) {
    return HAL_SPI_Receive(this->hspi,&data,1,100);
}

uint8_t Flash::Flash_SPI_Rx(uint8_t *data, uint16_t size) {
    return HAL_SPI_Receive(this->hspi,data,size,100);
}





uint8_t Flash::init() {
    uint8_t cmd = 0x9F;
    uint8_t id[3];

    HAL_GPIO_WritePin(SPI2_NSS_GPIO_Port, SPI2_NSS_Pin, GPIO_PIN_RESET);

    HAL_SPI_Transmit(hspi, &cmd, 1, 100);
    HAL_SPI_Receive(hspi, id, 3, 100);

    HAL_GPIO_WritePin(SPI2_NSS_GPIO_Port, SPI2_NSS_Pin, GPIO_PIN_SET);

    if (id[0] == 0xEF && id[1] == 0x40 && id[2] == 0x18) {
        return HAL_OK;
    }
    // HAL_UART_Transmit(&huart1, id, 3, 100);
    return HAL_ERROR;
}

void Flash::read(uint32_t address, uint8_t *data, uint16_t size)
{
    address &= 0x00FFFFFF;

    uint8_t cmd[4] = {
        0x03,
        (uint8_t)(address >> 16),
        (uint8_t)(address >> 8),
        (uint8_t)(address)
    };

    HAL_GPIO_WritePin(SPI2_NSS_GPIO_Port, SPI2_NSS_Pin, GPIO_PIN_RESET);

    HAL_SPI_Transmit(this->hspi, cmd, 4, 100);

    uint8_t dummy = 0xFF;
    HAL_SPI_TransmitReceive(this->hspi,
                            &dummy,
                            data,
                            size,
                            100);

    HAL_GPIO_WritePin(SPI2_NSS_GPIO_Port, SPI2_NSS_Pin, GPIO_PIN_SET);

}

uint8_t Flash::read_status()
{
    uint8_t cmd = 0x05;
    uint8_t status = 0;

    HAL_GPIO_WritePin(SPI2_NSS_GPIO_Port, SPI2_NSS_Pin, GPIO_PIN_RESET);

    HAL_SPI_Transmit(this->hspi, &cmd, 1, 100);
    HAL_SPI_Receive(this->hspi, &status, 1, 100);

    HAL_GPIO_WritePin(SPI2_NSS_GPIO_Port, SPI2_NSS_Pin, GPIO_PIN_SET);
    // HAL_UART_Transmit(&huart1, &status, 1, 100);
    return status;
}


void Flash::Flash_WaitBusy()
{
    while (read_status() & 0x01)
    {
        ;
    }
}

void Flash::write_page(uint32_t addr, uint8_t *data, uint16_t len)
{
    // W25Q32 page size = 256 bytes
    if (len > 256) len = 256;

    uint8_t cmd[4];

    // 1. Write Enable
    uint8_t we = 0x06;
    HAL_GPIO_WritePin(SPI2_NSS_GPIO_Port, SPI2_NSS_Pin, GPIO_PIN_RESET);
    HAL_SPI_Transmit(this->hspi, &we, 1, 100);
    HAL_GPIO_WritePin(SPI2_NSS_GPIO_Port, SPI2_NSS_Pin, GPIO_PIN_SET);

    // 2. Page Program command
    cmd[0] = 0x02;
    cmd[1] = (addr >> 16) & 0xFF;
    cmd[2] = (addr >> 8) & 0xFF;
    cmd[3] = addr & 0xFF;

    HAL_GPIO_WritePin(SPI2_NSS_GPIO_Port, SPI2_NSS_Pin, GPIO_PIN_RESET);

    HAL_SPI_Transmit(this->hspi, cmd, 4, 100);

    // 3. 写数据
    HAL_SPI_Transmit(this->hspi, data, len, 100);

    HAL_GPIO_WritePin(SPI2_NSS_GPIO_Port, SPI2_NSS_Pin, GPIO_PIN_SET);

    // 4. 等待完成
    Flash_WaitBusy();
}


void Flash::write(uint32_t addr, uint8_t *data, uint16_t size)
{
    uint16_t offset = 0;

    while (offset < size)
    {
        uint16_t page_offset = addr % 256;
        uint16_t space_in_page = 256 - page_offset;

        uint16_t chunk = (size - offset < space_in_page) ? (size - offset) : space_in_page;

        write_page(addr, data + offset, chunk);

        addr += chunk;
        offset += chunk;
    }
}

void Flash::erase(uint32_t address) {
    uint32_t addr = address & 0xFFFFF000; // 4KB对齐

    uint8_t we = 0x06;
    uint8_t cmd[4] = {0x20,
                      (uint8_t)(addr >> 16),
                      (uint8_t)(addr >> 8),
                      (uint8_t)(addr)};

    // 1. Write Enable（必须单独CS）
    HAL_GPIO_WritePin(SPI2_NSS_GPIO_Port, SPI2_NSS_Pin, GPIO_PIN_RESET);
    HAL_SPI_Transmit(hspi, &we, 1, 100);
    HAL_GPIO_WritePin(SPI2_NSS_GPIO_Port, SPI2_NSS_Pin, GPIO_PIN_SET);

    HAL_Delay(1);

    // 2. Erase
    HAL_GPIO_WritePin(SPI2_NSS_GPIO_Port, SPI2_NSS_Pin, GPIO_PIN_RESET);
    HAL_SPI_Transmit(hspi, cmd, 4, 100);
    HAL_GPIO_WritePin(SPI2_NSS_GPIO_Port, SPI2_NSS_Pin, GPIO_PIN_SET);

    // 3. 等待
    Flash_WaitBusy();
}

void Flash::test() {
    uint8_t data1[4] = {0x00, 0x01, 0x02, 0x03};
    uint8_t data[1024] = {};
    // write(0x00000000,data1,4);
    read(0x00000000,data,1024);
    HAL_UART_Transmit(&huart1, data, 1024, 100);
}