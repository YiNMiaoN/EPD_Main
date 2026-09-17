#ifndef TOPINFO_WEATHER_DATA_H
#define TOPINFO_WEATHER_DATA_H
#include <cstddef>
#include <cstdint>

struct WeatherCurrent {
    bool valid = false;
    double temperature = 0, humidity = 0;
    char icon[16] = {};
};
struct WeatherDay {
    char date[11] = {}, icon[16] = {};
    double high = 0, low = 0;
};
struct WeatherDaily {
    bool valid = false;
    unsigned count = 0;
    WeatherDay days[3] = {};
};
struct WeatherAlert {
    bool valid = false;
    unsigned count = 0;
    char icon[16] = {}, name[16] = {}, color[16] = {};
};
struct WeatherRain {
    bool valid = false;
    char startTime[33] = {};
    double precip[24] = {}, total = 0;
};
struct WeatherData {
    WeatherCurrent current;
    WeatherDaily daily;
    WeatherAlert alert;
    WeatherRain rain;
};
enum class WeatherUpdate { Invalid, Unchanged, Changed };
bool WeatherData_IsApi(const char *api);
// 每个 API 独立保存；完整校验通过后提交，失败保留旧值。
WeatherUpdate WeatherData_Parse(const char *api, const char *json, std::size_t length,
                                WeatherData &data);
int WeatherData_Round(double value);
#endif
