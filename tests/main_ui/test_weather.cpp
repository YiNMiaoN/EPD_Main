#include "MainUI.h"
#include "ApiRefresh.h"
extern "C" {
#include "shell.h"
}
#include <cassert>
#include <cstring>
#include <iostream>

EPD epd;
Flash flash;
MainUI mainUI(epd);
Shell shell{};
static ApiRefresh_Result result{};
static bool busy = false;
static std::string logText;
extern "C" const ApiRefresh_Result *ApiRefresh_GetResult() { return &result; }
extern "C" bool ApiRefresh_IsBusy() { return busy; }
extern "C" HAL_StatusTypeDef ApiRefresh_StartTime() { return HAL_ERROR; }
unsigned short shellWriteString(Shell *, const char *s) {
    logText += s;
    return static_cast<unsigned short>(std::strlen(s));
}

static const std::string current = R"({"api":"current","valid":true,"temp":"31","humidity":"66","icon":"101"})";
static const std::string daily = u8R"({"api":"daily3d","valid":true,"count":3,"items":[{"date":"2026-09-17","temp_max":29.81,"temp_min":26.03,"text":"阴","icon":"104"},{"date":"2026-09-18","temp_max":30.87,"temp_min":25.16,"text":"小雨","icon":"305"},{"date":"2026-09-19","temp_max":32.45,"temp_min":24.15,"text":"小雨","icon":"305"}]})";
static const std::string alert = R"({"api":"alert","valid":true,"count":2,"items":[{"id":"a","title":"warning","type":"1010","type_name":"\u9ad8\u6e29","severity_color":"Blue"}]})";
static const std::string noAlert = R"({"api":"alert","valid":true,"count":0,"items":[]})";
static std::string rain(const char *value = "0.1", const char *total = "2.4", unsigned count = 24) {
    std::string json = R"({"api":"minutely5m","valid":true,"start_time":"2026-09-17T21:50+08:00","interval_minutes":5,"count":24,"precip":[)";
    for (unsigned i = 0; i < count; ++i) { if (i) json += ','; json += value; }
    return json + "],\"total_precip\":" + total + "}";
}
static std::string replace(std::string s, const std::string &from, const std::string &to) {
    auto pos = s.find(from);
    assert(pos != std::string::npos);
    s.replace(pos, from.size(), to);
    return s;
}
static WeatherUpdate parse(WeatherData &data, const char *api, const std::string &s) {
    return WeatherData_Parse(api, s.data(), s.size(), data);
}
static void invalid(WeatherData &data, const char *api, const std::string &s) {
    // A rejected candidate must leave every API's accepted snapshot intact.
    unsigned char before[sizeof(data)];
    std::memcpy(before, &data, sizeof(data));
    assert(parse(data, api, s) == WeatherUpdate::Invalid);
    assert(std::memcmp(before, &data, sizeof(data)) == 0);
}
static void publish(const char *api, const std::string &json,
                    ApiRefresh_State state = API_REFRESH_SUCCEEDED, bool cacheFrame = true) {
    busy = true;
    result.state = API_REFRESH_WAIT_ACK;
    EPD_UI_Poll();
    result = {};
    result.state = state;
    result.has_frame = true;
    std::strcpy(result.api, api);
    assert(json.size() <= ESP_COM_MAX_PAYLOAD);
    result.frame.type = cacheFrame ? ESP_COM_RSP_CACHE : ESP_COM_RSP_ACK;
    result.frame.len = static_cast<uint16_t>(json.size());
    std::memcpy(result.frame.payload, json.data(), json.size());
    busy = false;
    EPD_UI_Poll();
}
static void textAt(int x, int y, const std::string &s) {
    for (auto it = epd.texts.rbegin(); it != epd.texts.rend(); ++it)
        if (it->x == x && it->y == y) {
            if (it->text != s) std::cerr << "Text at " << x << ',' << y << ": expected [" << s << "], got [" << it->text << "]\n";
            assert(it->text == s && it->size == FONT_SIZE_16); return;
        }
    assert(false);
}
static std::vector<DrawFill> bars() {
    std::vector<DrawFill> found;
    for (const auto &f : epd.fills)
        if (f.color == EPD_BLACK && f.x >= 253 && f.y >= 193 && f.y < 225) found.push_back(f);
    return found;
}

int main() {
    WeatherData data;
    assert(parse(data, "current", current) == WeatherUpdate::Changed);
    assert(parse(data, "current", current) == WeatherUpdate::Unchanged);
    assert(data.current.temperature == 31 && data.current.humidity == 66);
    assert(parse(data, "daily3d", daily) == WeatherUpdate::Changed);
    assert(parse(data, "daily3d", daily) == WeatherUpdate::Unchanged);
    assert(data.daily.count == 3 && WeatherData_Round(data.daily.days[0].high) == 30);
    assert(WeatherData_Round(-1.5) == -2 && WeatherData_Round(-1.4) == -1);
    assert(parse(data, "alert", alert) == WeatherUpdate::Changed);
    assert(data.alert.count == 2 && std::string(data.alert.name) == u8"高温");
    assert(std::string(data.alert.color) == u8"蓝色");
    assert(parse(data, "alert", alert) == WeatherUpdate::Unchanged);
    assert(parse(data, "minutely5m", rain()) == WeatherUpdate::Changed);
    assert(parse(data, "minutely5m", rain()) == WeatherUpdate::Unchanged);
    assert(data.rain.total == 2.4 && data.rain.precip[23] == 0.1);
    assert(data.current.temperature == 31); // independent API updates
    for (const auto &s : {std::string("{}"), std::string("[]"),
         replace(current, "true", "false"), replace(current, "true", "\"true\""),
         replace(current, "\"temp\":\"31\"", "\"temp\":31"),
         replace(current, "\"temp\":\"31\"", "\"nested\":{\"temp\":\"31\"}"),
         replace(current, "\"31\"", "\"31\",\"temp\":\"32\""),
         replace(current, "\"31\"", "\"NaN\""), replace(current, "\"31\"", "\"1e309\""),
         replace(current, "\"31\"", "\"31C\""), replace(current, "\"31\"", "\" 31\""),
         replace(current, "\"66\"", "\"101\""), replace(current, "\"101\"", "null"),
         replace(current, "\"api\":", "\"api\":\"current\",\"api\":"),
         replace(current, "\"valid\":", "\"valid\":true,\"valid\":"), current + std::string(384, ' ')})
        invalid(data, "current", s);
    invalid(data, "daily3d", current);
    for (const auto &s : {replace(daily, "\"count\":3", "\"count\":2"),
         replace(daily, "2026-09-17", "2026-02-29"), replace(daily, "2026-09-18", "2026-09-17"),
         replace(daily, "29.81", "20"), replace(daily, "29.81", "\"29.81\""),
         replace(daily, u8"阴", "\\ud800"), replace(daily, u8"阴", std::string("\xC0\xAF")),
         replace(daily, "\"icon\":\"104\"", "\"icon\":\"104\",\"icon\":\"101\"")})
        invalid(data, "daily3d", s);
    for (const auto &s : {replace(alert, "\"count\":2", "\"count\":0"),
         replace(noAlert, "\"count\":0", "\"count\":1"), replace(alert, "\"1010\"", "\"\""),
         replace(alert, "\"Blue\"", "null"), replace(alert, "\\u9ad8\\u6e29", "")})
        invalid(data, "alert", s);
    for (const auto &s : {rain("0", "0", 23), rain("0", "0", 25), rain("-1", "0"),
         rain("\"0\"", "0"), rain("1e309", "0"), rain("0.1", "24"),
         replace(rain(), "\"interval_minutes\":5", "\"interval_minutes\":10"),
         replace(rain(), "\"count\":24", "\"count\":23"), replace(rain(), "+08:00", ""),
         replace(rain(), "+08:00", "+24:00"), replace(rain(), "21:50", "24:50"),
         replace(rain(), "2026-09-17", "2026-02-29"), replace(rain(), "\"precip\":", "\"items\":"),
         replace(rain(), "\"total_precip\":2.4", "\"total_precip\":2.4,\"total_precip\":0")})
        invalid(data, "minutely5m", s);
    assert(parse(data, "minutely5m", replace(rain(), "2026-09-17T21:50+08:00", "2024-02-29T21:50:00Z")) == WeatherUpdate::Changed);
    assert(parse(data, "alert", noAlert) == WeatherUpdate::Changed);
    assert(data.alert.count == 0 && !data.alert.icon[0]);

    // Actual application bridge: merge four APIs, preserve layout, sleep after display.
    EPD_UI_Poll();
    publish("current", current);
    publish("daily3d", daily);
    publish("alert", alert);
    publish("minutely5m", rain());
    assert(epd.displays == 0);
    testTick = 4999; EPD_UI_Poll(); assert(epd.displays == 0);
    busy = true; testTick = 5000; EPD_UI_Poll(); assert(epd.displays == 0);
    busy = false; EPD_UI_Poll(); assert(epd.displays == 1 && epd.asleep);
    assert((epd.operations == std::vector<std::string>{"init", "clear-quote", "display", "sleep"}));
    textAt(290,46,u8"温31℃"); textAt(290,62,u8"湿66%");
    textAt(252,110,u8"今"); textAt(252,130,u8"明"); textAt(252,150,u8"后");
    textAt(298,110,"30/26"); textAt(298,130,"31/25"); textAt(298,150,"32/24");
    textAt(366,46,u8"高温"); textAt(366,62,u8"蓝色");
    textAt(300,172,"2h"); textAt(328,172,"2.4mm");
    assert(testIcons.size() == 5 && testIcons[0].code == "101" && testIcons[0].size == 32);
    assert(testIcons[0].x == 252 && testIcons[0].y == 46);
    textAt(252,228,"0"); textAt(290,228,"30"); textAt(336,228,"60"); textAt(376,228,"90");
    textAt(266,248,u8"两小时持续降水");
    auto drawnBars = bars(); assert(drawnBars.size() == 18);
    for (unsigned i = 0; i < 18; ++i)
        assert(drawnBars[i].x == 253 + int(i)*138/18 &&
               drawnBars[i].x + drawnBars[i].w == 253 + int(i+1)*138/18 &&
               drawnBars[i].y == 193 && drawnBars[i].h == 32);
    for (int i = 0; i < 100; ++i) EPD_UI_Poll();
    testTick = 10000;
    publish("current", current); publish("daily3d", daily); publish("alert", alert); publish("minutely5m", rain());
    publish("current", replace(current, "\"31\"", "\"32\""), API_REFRESH_FAILED);
    publish("current", "{}"); publish("current", current, API_REFRESH_SUCCEEDED, false);
    assert(epd.displays == 1);

    epd.fills.clear();
    publish("minutely5m", rain("0", "0"));
    assert(epd.displays == 2 && bars().empty()); textAt(328,172,"0.0mm");
    textAt(258,248,u8"未来两小时无降水");
    bool weatherCleared = false;
    for (const auto &f : epd.fills) if (f.x == 248 && f.y == 41 && f.w == 152 && f.color == EPD_WHITE) weatherCleared = true;
    assert(weatherCleared);
    publish("alert", noAlert);
    publish("current", "{}", API_REFRESH_FAILED); // failure must not lose pending alert
    testTick = 15000; EPD_UI_Poll(); assert(epd.displays == 3); textAt(378,56,u8"无");
    publish("alert", alert);
    testMissingIcon = true;
    unsigned oldTriangles = epd.triangles;
    EPD_UI_Refresh(); // manual redraw consumes pending data
    assert(epd.displays == 4 && epd.triangles == oldTriangles + 1);
    textAt(252,46,"?");
    testTick = 20000; EPD_UI_Poll(); assert(epd.displays == 4);
    testMissingIcon = false;
    epd.fills.clear();
    publish("minutely5m", rain("0.001", "0.024"));
    textAt(328,172,"<0.1mm"); assert(bars().size() == 18);
    publish("current", replace(current, "\"31\"", "\"32\""));
    testDisplayStatus = HAL_TIMEOUT;
    testTick = 25000; EPD_UI_Poll();
    assert(logText.find("display failed") != std::string::npos);
    const auto failedDisplays = epd.displays;
    testTick = 30000; EPD_UI_Poll(); assert(epd.displays == failedDisplays);
    // Unsigned tick arithmetic retains the five-second gap through wraparound.
    testDisplayStatus = HAL_OK;
    testTick = UINT32_MAX - 1000; EPD_UI_Refresh();
    publish("alert", noAlert);
    auto beforeWrap = epd.displays;
    testTick = 3998; EPD_UI_Poll(); assert(epd.displays == beforeWrap);
    testTick = 3999; EPD_UI_Poll(); assert(epd.displays == beforeWrap + 1);
    // Rain outside the visible 90 minutes still affects the two-hour total.
    std::string lateRain = rain("0", "1");
    const auto lastPoint = lateRain.find("],");
    assert(lastPoint != std::string::npos);
    lateRain[lastPoint - 1] = '1';
    epd.fills.clear();
    testTick += 5000; publish("minutely5m", lateRain);
    assert(bars().empty()); textAt(328,172,"1.0mm");
    textAt(262,248,u8"115分钟后有降水");
    // Short descriptions cover stopping and intermittent rain without truncation.
    std::string stoppingRain = replace(rain("0", "1"), "[0,", "[1,");
    testTick += 5000; publish("minutely5m", stoppingRain);
    textAt(270,248,u8"5分钟后降水停");
    std::string intermittent = replace(lateRain, "[0,", "[1,");
    intermittent = replace(intermittent, "\"total_precip\":1", "\"total_precip\":2");
    testTick += 5000; publish("minutely5m", intermittent);
    textAt(258,248,u8"两小时内间歇降水");
    for (const auto &t : epd.texts) {
        if (t.y != 248) continue;
        unsigned characters = 0, width = 0;
        for (unsigned char c : t.text) {
            if ((c & 0xC0) == 0x80) continue;
            ++characters; width += c < 0x80 ? 8 : 16;
        }
        assert(characters <= 9 && width <= 140);
        assert(t.x - 252 == 392 - (t.x + int(width)));
    }
    std::cout << "Weather parsing, preserved layout, rain bars and paced refresh tests passed\n";
}
