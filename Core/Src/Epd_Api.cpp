//
// Created by YinMi on 2026/5/13.
//

#include "Epd_Api.h"
extern "C" {
#include "shell.h"
}
#include "TopInfo_Project.h"
#include "ApiRefresh.h"
#include "LocalClock.h"
#include <cstdio>
#include <cstring>

extern Shell shell;

namespace {
bool weatherPending = false;
bool clockPending = false;
bool uiBaseReady = false;
unsigned clockPartialCount = 0;
constexpr unsigned CLOCK_PARTIAL_LIMIT = 30;
uint32_t lastDisplayTick = 0;
constexpr uint32_t DISPLAY_GAP_MS = 5000;

// 记录真实调用路径；HAL_OK 仅代表通信成功，不代表面板显示效果正确。
void logDisplayResult(const char *mode, HAL_StatusTypeDef status, uint32_t start)
{
    char line[112];
    std::snprintf(line, sizeof(line), "UI: %s end status=%d elapsed_ms=%lu partial_count=%u\r\n",
                  mode, static_cast<int>(status),
                  static_cast<unsigned long>(HAL_GetTick() - start), clockPartialCount);
    shellWriteString(&shell, line);
}

void updateClockSnapshot()
{
    LocalClock_Snapshot snapshot{};
    LocalClock_GetSnapshot(&snapshot);
    if (mainUI.setClockSnapshot(snapshot)) clockPending = true;
}
}

// C++对象

extern "C" {
//墨水屏操作C接口
    void EPD_HW_Init(void)
    {
        uiBaseReady = false;
        epd.init();
    }

    void EPD_HW_Clear(void)
    {
        uiBaseReady = false;
        epd.clear();
        lastDisplayTick = HAL_GetTick();
    }
    void EPD_HW_Sleep(void)
    {
        uiBaseReady = false;
        epd.sleep();
        lastDisplayTick = HAL_GetTick();
    }
    void EPD_HW_Display()
    {
        uiBaseReady = false;
        epd.refresh();
        lastDisplayTick = HAL_GetTick();
    }
    void EPD_UI_Refresh(void)
    {
        updateClockSnapshot();
        shellWriteString(&shell, "UI: full begin\r\n");
        const uint32_t start = HAL_GetTick();
        mainUI.refresh();
        uiBaseReady = EPD_GetStatus() == HAL_OK;
        clockPartialCount = 0;
        logDisplayResult("full", EPD_GetStatus(), start);
        weatherPending = false;
        clockPending = false;
        // 时钟运行期间保留控制器底图供后续分钟局刷；失败或未校时才深睡。
        if (LocalClock_GetState() != LOCAL_CLOCK_RUNNING || !uiBaseReady) EPD_HW_Sleep();
        lastDisplayTick = HAL_GetTick();
    }

    void EPD_UI_Poll(void)
    {
        LocalClock_Poll();
        updateClockSnapshot();
        static LocalClock_State previousClockState = LOCAL_CLOCK_UNSYNCED;
        const LocalClock_State clockState = LocalClock_GetState();
        if (clockState != previousClockState) {
            if (clockState == LOCAL_CLOCK_RUNNING)
                shellWriteString(&shell, "clock: boot sync complete; running from TIM10 heartbeat\r\n");
            else if (clockState == LOCAL_CLOCK_FAILED) {
                clockPending = true; // 启动失败也显示占位符，避免清屏后一直空白。
                shellWriteString(&shell, "clock: boot sync failed; no automatic retry\r\n");
            }
            previousClockState = clockState;
        }
        static bool handled = false;
        const ApiRefresh_Result *result = ApiRefresh_GetResult();
        if (result->state != API_REFRESH_SUCCEEDED) {
            handled = false;
        } else if (!handled) {
            handled = true;
            if (result->has_frame && WeatherData_IsApi(result->api)) {
                const WeatherUpdate update = result->frame.type == ESP_COM_RSP_CACHE ?
                    mainUI.setWeatherCache(result->api, reinterpret_cast<const char *>(result->frame.payload), result->frame.len) :
                    WeatherUpdate::Invalid;
                if (update == WeatherUpdate::Invalid) {
                    shellWriteString(&shell, "weather UI: invalid cache; previous data retained\r\n");
                } else if (update == WeatherUpdate::Changed) {
                    weatherPending = true;
                    shellWriteString(&shell, "weather UI: data accepted; display pending\r\n");
                }
            } else if (result->has_frame && std::strcmp(result->api, ESP_COM_API_HITOKOTO) == 0) {
                if (!mainUI.setHitokotoCache(reinterpret_cast<const char *>(result->frame.payload), result->frame.len)) {
                    shellWriteString(&shell, "hitokoto UI: invalid cache; previous text retained\r\n");
                } else {
                    EPD_UI_Refresh();
                }
            }
        }
        // 时钟按分钟更新，与天气合并显示；API 忙碌时继续收包，心跳仍正常累计。
        if ((weatherPending || clockPending) && !ApiRefresh_IsBusy() &&
            static_cast<uint32_t>(HAL_GetTick() - lastDisplayTick) >= DISPLAY_GAP_MS) {
            HAL_StatusTypeDef status;
            if (!weatherPending && clockState == LOCAL_CLOCK_RUNNING && uiBaseReady && EPD_CanRefreshPartial() &&
                clockPartialCount < CLOCK_PARTIAL_LIMIT) {
                shellWriteString(&shell, "UI: clock partial begin x=248 y=0 w=152 h=40 lut=HINK\r\n");
                const uint32_t start = HAL_GetTick();
                status = mainUI.refreshClock();
                clockPending = false;
                ++clockPartialCount;
                logDisplayResult("clock partial", status, start);
                if (status != HAL_OK) EPD_HW_Sleep();
                lastDisplayTick = HAL_GetTick();
            } else {
                EPD_UI_Refresh();
                status = EPD_GetStatus();
            }
            shellWriteString(&shell, status == HAL_OK ?
                             "UI: display complete\r\n" :
                             "UI: display failed; use ref_epd to retry\r\n");
        }
    }
//Flash操作C接口
    void EPD_Flash_Init(void)
    {
        flash.init();
    }
    void EPD_Flash_Test(void)
    {
        flash.test();
    }
    void EPD_Flash_Read(uint32_t address, uint8_t *data, uint16_t size)
    {
        flash.read(address, data, size);
    }
    void EPD_Flash_Write(uint32_t address, uint8_t *data, uint16_t size)
    {
        flash.write(address, data, size);
    }
    void EPD_Flash_Erase(uint32_t address, uint16_t size)
    {
        flash.erase(address);
    }



}
