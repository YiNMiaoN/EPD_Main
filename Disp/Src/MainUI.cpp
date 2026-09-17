//
// Created by YinMi on 2026/7/15.
//
#include "MainUI.h"
#include <cstdio>

namespace {
// 资源包缺少图标时只在原图标框内显示占位符，避免保留上次天气图标。
void drawWeatherIcon(EPD &epd, const char *code, uint16_t size, int16_t x, int16_t y)
{
    if (!code[0] || QWeatherIcon_Draw(code, size, x, y) != QWEATHER_ICON_OK)
        epd.drawChineseString(x, y, "?", FONT_SIZE_16);
}
}



MainUI::MainUI(EPD &display)
    : epd(display) {

}

MainUI::~MainUI() {

}

void MainUI::drawTime() {
    char timeText[16] = "--:--", dateText[16] = "----/--/--";
    static const char *const weekdays[] = {"周日", "周一", "周二", "周三", "周四", "周五", "周六"};
    if (clock.valid) {
        std::snprintf(timeText, sizeof(timeText), "%02u:%02u", (unsigned)clock.hour, (unsigned)clock.minute);
        std::snprintf(dateText, sizeof(dateText), "%04u-%02u-%02u", (unsigned)clock.year,
                      (unsigned)clock.month, (unsigned)clock.day);
    }
    epd.fillRect(246, 0, 154, 40, EPD_WHITE);
    drawChineseString(250,6,timeText,FONT_SIZE_24);
    drawChineseString(322,2,clock.valid && clock.weekday < 7 ? weekdays[clock.weekday] : "周--",FONT_SIZE_16);
    // 完整日期宽 80px，右对齐屏幕边缘。
    drawChineseString(320,18,dateText,FONT_SIZE_16);
}

bool MainUI::setClockSnapshot(const LocalClock_Snapshot &snapshot) {
    const bool changed = clock.valid != snapshot.valid || (snapshot.valid &&
        (clock.year != snapshot.year || clock.month != snapshot.month || clock.day != snapshot.day ||
         clock.hour != snapshot.hour || clock.minute != snapshot.minute || clock.weekday != snapshot.weekday));
    clock = snapshot;
    return changed;
}


void MainUI::drawList() {
    drawChineseString(20,8,"ToDay",FONT_SIZE_24);   //代办标题
    drawLine(20,36,220,36,EPD_BLACK);

    drawChineseString(14,40,"45",FONT_SIZE_16);
    drawChineseString(28,40,"60",FONT_SIZE_16);
    drawVerticalProgress(18, 58, 8, 170, 45);
    drawVerticalProgress(32, 58, 8, 170, 60);
    drawChineseString(18,232,"T",FONT_SIZE_16);
    drawChineseString(32,232,"D",FONT_SIZE_16);

    drawChineseString(56,50,"09:30 例会",FONT_SIZE_16);
    drawChineseString(56,72,"11:00 报告",FONT_SIZE_16);
    drawChineseString(56,94,"14:30 项目同步",FONT_SIZE_16);
    drawChineseString(56,116,"18:00 复盘",FONT_SIZE_16);
    drawChineseString(56,138,"20:00 阅读",FONT_SIZE_16);
    drawChineseString(56,160,"22:30 休息",FONT_SIZE_16);
}

void MainUI::drawVerticalProgress(int16_t x,
                                  int16_t y,
                                  int16_t width,
                                  int16_t height,
                                  uint8_t percent) {
    if (percent > 100) {
        percent = 100;
    }

    int16_t fill_height = (int16_t)((height - 4) * percent / 100);
    int16_t fill_y = (int16_t)(y + 2);

    epd.drawRect(x, y, width, height, EPD_BLACK);
    epd.fillRect(x + 2, fill_y, width - 4, fill_height, EPD_BLACK);
}

void MainUI::drawNowWeather() {
    char temperature[24] = "温--℃", humidity[24] = "湿--%";
    if (weather.current.valid) {
        drawWeatherIcon(epd, weather.current.icon, QWEATHER_ICON_SIZE_32, 252, 46);
        std::snprintf(temperature, sizeof(temperature), "温%d℃", WeatherData_Round(weather.current.temperature));
        std::snprintf(humidity, sizeof(humidity), "湿%d%%", WeatherData_Round(weather.current.humidity));
    }
    drawChineseString(290,46,temperature,FONT_SIZE_16);
    drawChineseString(290,62,humidity,FONT_SIZE_16);
    drawWeatherAlert();
}

void MainUI::drawWeatherAlert() {
    if (!weather.alert.valid) {
        drawChineseString(366,46,"--",FONT_SIZE_16);
        drawChineseString(366,62,"--",FONT_SIZE_16);
        return;
    }
    if (!weather.alert.count) {
        drawNoWeatherAlert();
        return;
    }
    if (QWeatherIcon_Draw(weather.alert.icon, QWEATHER_ICON_SIZE_16, 346, 54) != QWEATHER_ICON_OK) {
        // 未收录预警图标时在原有 16×16 位置绘制通用警示符号。
        epd.drawTriangle(354,54,346,69,361,69,EPD_BLACK);
        epd.drawLine(354,58,354,63,EPD_BLACK);
        epd.drawPixel(354,66,EPD_BLACK);
    }
    drawChineseString(366,46,weather.alert.name,FONT_SIZE_16);
    drawChineseString(366,62,weather.alert.color,FONT_SIZE_16);
}

void MainUI::drawNoWeatherAlert() {
    epd.drawTriangle(362, 48, 350, 72, 374, 72, EPD_BLACK);
    epd.drawLine(362, 56, 362, 64, EPD_BLACK);
    epd.drawPixel(362, 68, EPD_BLACK);
    drawChineseString(378,56,"无",FONT_SIZE_16);
}

void MainUI::drawWeatherForecast() {
    drawLine(252,84,392,84,EPD_BLACK);
    drawChineseString(252,88,"三日预报",FONT_SIZE_16);

    static const char *const labels[] = {"今", "明", "后"};
    for (unsigned i = 0; i < 3; ++i) {
        const int16_t y = static_cast<int16_t>(110 + i * 20);
        char range[24] = "--/--";
        drawChineseString(252,y,labels[i],FONT_SIZE_16);
        if (weather.daily.valid && i < weather.daily.count) {
            const WeatherDay &day = weather.daily.days[i];
            drawWeatherIcon(epd, day.icon, QWEATHER_ICON_SIZE_16, 276, y);
            std::snprintf(range, sizeof(range), "%d/%d", WeatherData_Round(day.high), WeatherData_Round(day.low));
        }
        drawChineseString(298,y,range,FONT_SIZE_16);
    }
}

void MainUI::drawRainForecast() {
    drawLine(252,168,392,168,EPD_BLACK);
    drawChineseString(252,172,"降雨",FONT_SIZE_16);
    drawChineseString(300,172,"2h",FONT_SIZE_16);
    char total[16] = "--mm";
    if (weather.rain.valid) {
        const double mm = weather.rain.total;
        if (mm > 9999) std::snprintf(total, sizeof(total), ">9999mm");
        else if (mm >= 99.95) std::snprintf(total, sizeof(total), "%dmm", WeatherData_Round(mm));
        else if (mm > 0 && mm < 0.05) std::snprintf(total, sizeof(total), "<0.1mm");
        else {
            const int tenths = WeatherData_Round(mm * 10);
            std::snprintf(total, sizeof(total), "%d.%dmm", tenths / 10, tenths % 10);
        }
    }
    // 总量保留在 2h 右侧；前 90 分钟图表下方用短句概括完整两小时。
    drawChineseString(328,172,total,FONT_SIZE_16);
    constexpr int chartX = 252, chartY = 192, chartWidth = 140, chartHeight = 34;
    constexpr unsigned visiblePoints = 18;
    constexpr int innerX = chartX + 1, innerWidth = chartWidth - 2;
    constexpr int innerHeight = chartHeight - 2, baseline = chartY + chartHeight - 1;
    // 即使无数据或零降水也保留空框；刻度单位为分钟。
    epd.drawRect(chartX, chartY, chartWidth, chartHeight, EPD_BLACK);
    static const char *const times[] = {"0", "30", "60", "90"};
    static const int16_t labelX[] = {252, 290, 336, 376};
    for (unsigned i = 0; i < 4; ++i) {
        const int16_t x = static_cast<int16_t>(innerX + i * (innerWidth - 1) / 3);
        epd.drawLine(x, baseline, x, baseline + 2, EPD_BLACK);
        drawChineseString(labelX[i],228,times[i],FONT_SIZE_16);
    }
    char description[40] = "暂无降水数据";
    if (weather.rain.valid) {
        unsigned first = 24, stop = 24;
        bool resumes = false;
        for (unsigned i = 0; i < 24; ++i) {
            if (weather.rain.precip[i] > 0) {
                if (first == 24) first = i;
                if (stop < 24) resumes = true;
            } else if (first < 24 && stop == 24) {
                stop = i;
            }
        }
        // 包括数字在内最多 9 个字符；时间以五分钟预报区间为准。
        if (first == 24) std::snprintf(description, sizeof(description), "未来两小时无降水");
        else if (first > 0) std::snprintf(description, sizeof(description), "%u分钟后有降水", first * 5);
        else if (stop == 24) std::snprintf(description, sizeof(description), "两小时持续降水");
        else if (resumes) std::snprintf(description, sizeof(description), "两小时内间歇降水");
        else std::snprintf(description, sizeof(description), "%u分钟后降水停", stop * 5);
    }
    int descriptionWidth = 0;
    for (const auto *p = reinterpret_cast<const unsigned char *>(description); *p; ++p) {
        if (*p < 0x80) descriptionWidth += 8;
        else if ((*p & 0xC0) != 0x80) descriptionWidth += 16;
    }
    drawChineseString(static_cast<int16_t>(chartX + (chartWidth - descriptionWidth) / 2),
                      248,description,FONT_SIZE_16);
    if (!weather.rain.valid) return;
    double peak = 0;
    for (unsigned i = 0; i < visiblePoints; ++i)
        if (weather.rain.precip[i] > peak) peak = weather.rain.precip[i];
    if (peak == 0) return;
    for (unsigned i = 0; i < visiblePoints; ++i) {
        if (weather.rain.precip[i] == 0) continue;
        int height = WeatherData_Round(weather.rain.precip[i] / peak * innerHeight);
        if (height < 1) height = 1;
        const int left = innerX + i * innerWidth / visiblePoints;
        const int right = innerX + (i + 1) * innerWidth / visiblePoints;
        epd.fillRect(static_cast<int16_t>(left), static_cast<int16_t>(baseline - height),
                     static_cast<int16_t>(right - left), static_cast<int16_t>(height), EPD_BLACK);
    }
}

void MainUI::drawHitokoto() {
    epd.fillRect(0, 268, 400, 32, EPD_WHITE);
    drawChineseString(0,268,hitokoto.quote,FONT_SIZE_16);
    drawChineseString(400 - hitokoto.attributionWidth(),284,hitokoto.attribution,FONT_SIZE_16);
}

bool MainUI::setHitokotoCache(const char *json, std::size_t length) {
    return HitokotoText_Parse(json, length, hitokoto);
}

WeatherUpdate MainUI::setWeatherCache(const char *api, const char *json, std::size_t length) {
    return WeatherData_Parse(api, json, length, weather);
}


void MainUI::drawMainUI() {
    drawLine(245,0,245,266,0);//左右内容分割线
    drawLine(245,40,400,40,0);//时间天气分割线
    drawLine(0,266,400,266,0);//一言分割线

    drawTime();
    drawList();
    // 清理整个天气内容区，覆盖短文本、无预警和零降雨留下的旧像素。
    epd.fillRect(248, 41, 152, 225, EPD_WHITE);
    drawNowWeather();
    drawWeatherForecast();
    drawRainForecast();
    drawHitokoto();
}



void MainUI::updata_ui() {
    drawMainUI();

}

void MainUI::refresh() {
    epd.init(); // The previous display operation may have put the panel to sleep.
    updata_ui();
    HAL_Delay(10);
    epd.refresh();
}

HAL_StatusTypeDef MainUI::refreshClock() {
    drawTime();
    // 字节对齐，包含时间、日期和星期，不触碰天气区和分隔线。
    return epd.refreshPartial(248, 0, 152, 40);
}

void MainUI::drawChineseString(int16_t x, int16_t y, const char *text, FontSize size)
{
    epd.drawChineseString(x, y, text, size);
}

void MainUI::drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color)
{
    epd.drawLine(x0, y0, x1, y1, color);
}
