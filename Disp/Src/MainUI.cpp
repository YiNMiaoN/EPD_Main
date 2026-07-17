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
}

void MainUI::drawNowWeather() {
    QWeatherIcon_Draw("100", QWEATHER_ICON_SIZE_32, 252, 43); //天气图标
    drawChineseString(252+32+4+18,43,"45℃",FONT_SIZE_16);
    drawChineseString(252+32+4+18,43+16,"45%",FONT_SIZE_16);
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
