#include "EPD_Hardware.h"
#include <cassert>
#include <cstdio>
#include <vector>

SPI_HandleTypeDef hspi1;
struct Transfer { uint8_t command; std::vector<uint8_t> data; };
static std::vector<Transfer> transfers;
static GPIO_PinState dc, cs = GPIO_PIN_SET;
static bool busy = false;
static uint32_t tick = 0;
static int failAfter = -1;

void HAL_GPIO_WritePin(int, int pin, GPIO_PinState value) {
    if (pin == SPI1_DC_Pin) dc = value;
    if (pin == SPI1_NSS_Pin) cs = value;
}
GPIO_PinState HAL_GPIO_ReadPin(int, int) { return busy ? GPIO_PIN_SET : GPIO_PIN_RESET; }
void HAL_Delay(uint32_t ms) { tick += ms; }
uint32_t HAL_GetTick() { return tick; }
HAL_StatusTypeDef HAL_SPI_Transmit(SPI_HandleTypeDef *, uint8_t *data, uint16_t count, uint32_t) {
    assert(cs == GPIO_PIN_RESET);
    if (failAfter == 0) return HAL_ERROR;
    if (failAfter > 0) --failAfter;
    for (unsigned i = 0; i < count; ++i) {
        if (dc == GPIO_PIN_RESET) transfers.push_back({data[i], {}});
        else { assert(!transfers.empty()); transfers.back().data.push_back(data[i]); }
    }
    return HAL_OK;
}
static const std::vector<uint8_t>& payload(uint8_t command) {
    for (const auto& item : transfers) if (item.command == command) return item.data;
    assert(false);
    return transfers.front().data;
}
static unsigned commandCount(uint8_t command) {
    unsigned count = 0;
    for (const auto& item : transfers) count += item.command == command;
    return count;
}
static void ready(uint8_t *frame) {
    busy = false;
    failAfter = -1;
    EPD_Init();
    EPD_Display(frame);
    assert(EPD_GetStatus() == HAL_OK);
    transfers.clear();
}
static void verifyFull() {
    assert(payload(0x44) == std::vector<uint8_t>({0, 49}));
    assert(payload(0x45) == std::vector<uint8_t>({0, 0, 43, 1}));
    assert(payload(0x21) == std::vector<uint8_t>({0x40, 0}));
    assert(payload(0x3C) == std::vector<uint8_t>({5}));
    assert(payload(0x22) == std::vector<uint8_t>({0xF7}));
    assert(payload(0x24).size() == 15000 && payload(0x26).size() == 15000);
    assert(commandCount(0x4E) == 2 && commandCount(0x4F) == 2);
}

int main() {
    uint8_t frame[15000];
    for (unsigned y = 0; y < 300; ++y)
        for (unsigned x = 0; x < 50; ++x) frame[y * 50 + x] = (y * 17 + x * 3) & 255;

    assert(EPD_DisplayPartial(frame, 80, 200, 200, 50) == HAL_ERROR);
    assert(transfers.empty());
    EPD_Init();
    transfers.clear();
    assert(EPD_DisplayPartial(frame, 80, 200, 200, 50) == HAL_ERROR);
    assert(transfers.empty());
    ready(frame);

    // 非零起点、多行非连续数据，验证没有误发整屏缓冲开头或相邻区域。
    for (unsigned pass = 0; pass < 2; ++pass) {
        frame[201 * 50 + 12] ^= 0xFF;
        assert(EPD_DisplayPartial(frame, 80, 200, 200, 50) == HAL_OK);
        assert(payload(0x44) == std::vector<uint8_t>({10, 34}));
        assert(payload(0x45) == std::vector<uint8_t>({200, 0, 249, 0}));
        assert(payload(0x4E) == std::vector<uint8_t>({10}));
        assert(payload(0x4F) == std::vector<uint8_t>({200, 0}));
        const auto& pixels = payload(0x24);
        assert(pixels.size() == 1250);
        for (unsigned y = 0; y < 50; ++y)
            for (unsigned x = 0; x < 25; ++x)
                assert(pixels[y * 25 + x] == frame[(200 + y) * 50 + 10 + x]);
        assert(payload(0x22) == std::vector<uint8_t>({0xFF}));
        assert(commandCount(0x26) == 0 && commandCount(0x12) == 0);
        transfers.clear();
    }
    assert(EPD_DisplayPartial(frame, 392, 299, 8, 1) == HAL_OK);
    assert(payload(0x24) == std::vector<uint8_t>({frame[14999]}));
    assert(payload(0x45) == std::vector<uint8_t>({43, 1, 43, 1}));
    transfers.clear();

    assert(EPD_DisplayPartial(nullptr, 0, 0, 8, 1) == HAL_ERROR);
    assert(EPD_DisplayPartial(frame, 1, 0, 8, 1) == HAL_ERROR);
    assert(EPD_DisplayPartial(frame, 0, 0, 7, 1) == HAL_ERROR);
    assert(EPD_DisplayPartial(frame, 0, 0, 0, 1) == HAL_ERROR);
    assert(EPD_DisplayPartial(frame, 0, 0, 8, 0) == HAL_ERROR);
    assert(EPD_DisplayPartial(frame, 392, 299, 16, 1) == HAL_ERROR);
    assert(EPD_DisplayPartial(frame, 0, 299, 8, 2) == HAL_ERROR);
    assert(EPD_DisplayPartial(frame, 65535, 65535, 65535, 65535) == HAL_ERROR);
    assert(EPD_PartialDisplay(frame, 8, 0, 0, 1) == HAL_ERROR);
    assert(transfers.empty());

    uint8_t packed[] = {0xAA, 0x55, 0x12, 0x34};
    assert(EPD_PartialDisplay(packed, 8, 10, 24, 12) == HAL_OK);
    assert(payload(0x24) == std::vector<uint8_t>(packed, packed + 4));
    transfers.clear();
    EPD_Display(frame);
    verifyFull();
    assert(payload(0x24) == std::vector<uint8_t>(frame, frame + 15000));
    assert(payload(0x26) == payload(0x24));
    transfers.clear();
    assert(EPD_DisplayPartial(frame, 0, 0, 8, 1) == HAL_OK);
    transfers.clear();
    EPD_Clear();
    verifyFull();
    for (uint8_t pixel : payload(0x24)) assert(pixel == 0xFF);
    EPD_Sleep();
    transfers.clear();
    assert(EPD_DisplayPartial(frame, 0, 0, 8, 1) == HAL_ERROR);
    assert(transfers.empty());

    // BUSY 常高时限时退出，包括系统毫秒计数回绕。
    ready(frame);
    busy = true;
    tick = 0xFFFFFFF0;
    const uint32_t start = tick;
    assert(EPD_DisplayPartial(frame, 0, 0, 8, 1) == HAL_TIMEOUT);
    assert(uint32_t(tick - start) == 15000);
    assert(transfers.empty());
    busy = false;
    assert(EPD_DisplayPartial(frame, 0, 0, 8, 1) == HAL_TIMEOUT);
    EPD_Sleep();
    assert(EPD_GetStatus() == HAL_TIMEOUT);

    // SPI 中途失败后禁止触发刷新，重新初始化和全刷后恢复。
    ready(frame);
    failAfter = 22;
    assert(EPD_DisplayPartial(frame, 80, 200, 200, 50) == HAL_ERROR);
    assert(commandCount(0x20) == 0);
    transfers.clear();
    failAfter = -1;
    assert(EPD_DisplayPartial(frame, 0, 0, 8, 1) == HAL_ERROR);
    assert(transfers.empty());
    ready(frame);
    assert(EPD_DisplayPartial(frame, 0, 0, 8, 1) == HAL_OK);
    std::puts("EPD partial tests passed");
}
