//
// Created by YinMi on 2026/5/15.
//

#ifndef TOPINFO_FLASH_H
#define TOPINFO_FLASH_H
#include "stm32f4xx_hal.h"
#include "spi.h"

class Flash{
private:
    SPI_HandleTypeDef *hspi = &hspi2;
    uint8_t Flash_Buff[128];
    uint8_t Flash_SPI_Tx(uint8_t *data, uint16_t size);
    uint8_t Flash_SPI_Tx(uint8_t data);
    uint8_t Flash_SPI_Rx(uint8_t *data, uint16_t size);
    uint8_t Flash_SPI_Rx(uint8_t data);
    uint8_t Flash_SPI_RTx(uint16_t size);


public:
    Flash();
    ~Flash();
    void test();
    uint8_t init();
    void write(uint32_t address, uint8_t *data, uint16_t size);
    void read(uint32_t address, uint8_t *data, uint16_t size);

    uint8_t read_status();

    void Flash_WaitBusy();

    void write_page(uint32_t addr, uint8_t *data, uint16_t len);

    void erase(uint32_t address);

};

#endif //TOPINFO_FLASH_H