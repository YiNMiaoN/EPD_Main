//
// Created by YinMi on 2026/7/15.
//
#include "MainUI.h"



MainUI::MainUI(EPD &display)
    : epd(display) {

}

MainUI::~MainUI() {

}

void MainUI::drawTime() {
    drawChineseString(250,6,"12:00",FONT_SIZE_24);  //时间
    drawChineseString(250+72,2,"周四",FONT_SIZE_16);  //星期
    drawChineseString(250+72,18,"2026-7-15",FONT_SIZE_16);  //日期
    //显示时间与日期 占用的位置为 250px-400px横向 0px-32px纵向
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
    QWeatherIcon_Draw("100", QWEATHER_ICON_SIZE_32, 252, 46); //天气图标
    drawChineseString(290,46,"45℃",FONT_SIZE_16);
    drawChineseString(290,62,"湿45%",FONT_SIZE_16);
    drawWeatherAlert();
}

void MainUI::drawWeatherAlert() {
    QWeatherIcon_Draw("900", QWEATHER_ICON_SIZE_16, 346, 54);
    drawChineseString(366,46,"高温",FONT_SIZE_16);
    drawChineseString(366,62,"蓝色",FONT_SIZE_16);
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

    drawChineseString(252,110,"今",FONT_SIZE_16);
    QWeatherIcon_Draw("100", QWEATHER_ICON_SIZE_16, 276, 110);
    drawChineseString(298,110,"33/26",FONT_SIZE_16);

    drawChineseString(252,130,"明",FONT_SIZE_16);
    QWeatherIcon_Draw("101", QWEATHER_ICON_SIZE_16, 276, 130);
    drawChineseString(298,130,"31/25",FONT_SIZE_16);

    drawChineseString(252,150,"后",FONT_SIZE_16);
    QWeatherIcon_Draw("104", QWEATHER_ICON_SIZE_16, 276, 150);
    drawChineseString(298,150,"30/24",FONT_SIZE_16);
}

void MainUI::drawRainForecast() {
    drawLine(252,168,392,168,EPD_BLACK);
    drawChineseString(252,172,"降雨",FONT_SIZE_16);
    drawChineseString(300,172,"2h",FONT_SIZE_16);
    drawChineseString(252,192,"0.0mm",FONT_SIZE_16);

    epd.fillRect(306, 201, 6, 3, EPD_BLACK);
    epd.fillRect(318, 199, 6, 5, EPD_BLACK);
    epd.fillRect(330, 196, 6, 8, EPD_BLACK);
    epd.fillRect(342, 201, 6, 3, EPD_BLACK);
    epd.fillRect(354, 202, 6, 2, EPD_BLACK);
    epd.fillRect(366, 200, 6, 4, EPD_BLACK);
}

void MainUI::drawHitokoto() {
    drawChineseString(0,268,"「行动越快，痛苦越少。」",FONT_SIZE_16);
    drawChineseString(200,284,"------切利尼娜·德克萨斯",FONT_SIZE_16);
}



void MainUI::drawMainUI() {
    drawLine(245,0,245,266,0);//左右内容分割线
    drawLine(245,40,400,40,0);//时间天气分割线
    drawLine(0,266,400,266,0);//一言分割线

    drawTime();
    drawList();
    drawNowWeather();
    drawWeatherForecast();
    drawRainForecast();
    drawHitokoto();
}



void MainUI::updata_ui() {
    drawMainUI();

}

void MainUI::refresh() {
    updata_ui();
    HAL_Delay(10);
    epd.refresh();
}

void MainUI::drawChineseString(int16_t x, int16_t y, const char *text, FontSize size)
{
    epd.drawChineseString(x, y, text, size);
}

void MainUI::drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color)
{
    epd.drawLine(x0, y0, x1, y1, color);
}
