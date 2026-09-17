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
static std::string logText;

extern "C" const ApiRefresh_Result *ApiRefresh_GetResult(void) { return &result; }
unsigned short shellWriteString(Shell *, const char *text) {
    logText += text;
    return static_cast<unsigned short>(std::strlen(text));
}

static std::string cache(const std::string &quote, const std::string &author = "null",
                         const std::string &source = "\"source\"")
{
    return "{\"api\":\"hitokoto\",\"valid\":true,\"hitokoto\":\"" + quote +
           "\",\"from_who\":" + author + ",\"from\":" + source + "}";
}

static void parse(const std::string &json, HitokotoText &text)
{
    assert(HitokotoText_Parse(json.data(), json.size(), text));
}

static int pixels(const char *text)
{
    int width = 0;
    for (const auto *p = reinterpret_cast<const unsigned char *>(text); *p; ++p) {
        if (*p < 0x80) width += 8;
        else if ((*p & 0xC0) != 0x80) width += 16;
    }
    return width;
}

static void publish(const std::string &json, ApiRefresh_State state = API_REFRESH_SUCCEEDED,
                    const char *api = "hitokoto")
{
    result = {};
    result.state = API_REFRESH_WAIT_ACK;
    EPD_UI_Poll();
    result.state = state;
    result.has_frame = true;
    std::strcpy(result.api, api);
    assert(json.size() <= ESP_COM_MAX_PAYLOAD);
    result.frame.len = static_cast<uint16_t>(json.size());
    std::memcpy(result.frame.payload, json.data(), json.size());
    EPD_UI_Poll();
}

int main()
{
    HitokotoText text;
    const std::string json = cache(u8"\u4f60\u597d", u8"\"\u4f5c\u8005\"", u8"\"\u51fa\u5904\"");
    parse(json, text);
    assert(std::strcmp(text.quote, u8"\u300c\u4f60\u597d\u300d") == 0);
    assert(std::strcmp(text.attribution, u8"\u2014\u2014\u4f5c\u8005") == 0);
    parse(cache("\\u4f60\\u597d\\\"A\\\"\\nB\\tC\\/\\\\", "null"), text);
    assert(std::strcmp(text.quote, u8"\u300c\u4f60\u597d\"A\" B C/\\\u300d") == 0);
    assert(std::strcmp(text.attribution, u8"\u2014\u2014source") == 0);
    parse(cache("test", "\"\"", "\"\""), text);
    assert(std::strcmp(text.attribution, u8"\u2014\u2014\u4f5a\u540d") == 0);
    parse(cache("test", "\" \\n\"", "\"source\""), text);
    assert(std::strcmp(text.attribution, u8"\u2014\u2014source") == 0);
    parse(cache("\\ud83d\\ude00"), text);
    assert(std::strcmp(text.quote, u8"\u300c\U0001f600\u300d") == 0);

    std::string longQuote;
    for (int i = 0; i < 23; ++i) longQuote += u8"\u5929";
    parse(cache(longQuote), text);
    assert(pixels(text.quote) == 400 && !std::strstr(text.quote, u8"\u2026"));
    parse(cache(longQuote + u8"\u5929", "\"" + std::string(100, 'a') + "\""), text);
    assert(pixels(text.quote) <= 400 && pixels(text.attribution) <= 400);
    assert(std::strstr(text.quote, u8"\u2026\u300d"));
    assert(std::strstr(text.attribution, u8"\u2026"));
    parse(cache(std::string(100, 'x')), text);
    assert(pixels(text.quote) <= 400 && std::strstr(text.quote, u8"\u2026\u300d"));

    const std::string bad[] = {
        "{}", "[]", "{\"api\":\"hitokoto\",\"valid\":true,}",
        "{\"api\":\"current\",\"valid\":true,\"hitokoto\":\"wrong\"}",
        "{\"api\":\"hitokoto\",\"valid\":false,\"hitokoto\":\"old\"}",
        "{\"api\":\"hitokoto\",\"valid\":true,\"hitokoto\":null}",
        "{\"api\":\"hitokoto\",\"valid\":true,\"data\":{\"hitokoto\":\"nested\"}}",
        "{\"api\":\"hitokoto\",\"valid\":true,\"hitokoto\":\"a\",\"hitokoto\":\"b\"}",
        cache(""), cache(" \\n\\u0000"), cache("ok", "123"),
        cache("\\ud800"), cache("\\udc00"), cache("\\ud800\\u0041"),
        cache(std::string("\xC0\xAF")), cache(std::string("\xE4\xBD"))
    };
    const HitokotoText previous = text;
    for (const auto &value : bad) {
        assert(!HitokotoText_Parse(value.data(), value.size(), text));
        assert(std::strcmp(previous.quote, text.quote) == 0);
        assert(std::strcmp(previous.attribution, text.attribution) == 0);
    }

    EPD_UI_Poll();
    assert(epd.displays == 0);
    publish(json);
    assert(epd.displays == 1 && epd.quoteClears == 1 && epd.asleep);
    assert((epd.operations == std::vector<std::string>{"init", "clear-quote", "display", "sleep"}));
    auto quote = epd.texts[epd.texts.size() - 2];
    auto author = epd.texts.back();
    assert(quote.x == 0 && quote.y == 268 && quote.size == FONT_SIZE_16);
    assert(quote.text == u8"\u300c\u4f60\u597d\u300d");
    assert(author.x == 336 && author.y == 284 && author.size == FONT_SIZE_16);
    assert(author.text == u8"\u2014\u2014\u4f5c\u8005");
    for (int i = 0; i < 100; ++i) EPD_UI_Poll();
    assert(epd.displays == 1);
    publish(json, API_REFRESH_FAILED);
    publish(json, API_REFRESH_SUCCEEDED, "current");
    publish(json, API_REFRESH_SUCCEEDED, "time");
    publish("{\"api\":\"todolist\",\"valid\":true,\"stage\":\"today\",\"http_status\":200}",
            API_REFRESH_SUCCEEDED, "todolist");
    publish("{\"api\":\"todolist_inbox\",\"valid\":true,\"stage\":\"inbox_next2d\",\"items\":[]}",
            API_REFRESH_SUCCEEDED, "todolist_inbox");
    publish(cache(""));
    assert(epd.displays == 1 && !logText.empty());
    EPD_UI_Refresh();
    assert(epd.displays == 2 && epd.texts.back().text == author.text);
    publish(cache("A", "null", "\"B\""));
    assert(epd.displays == 3 && epd.quoteClears == 3);
    assert(epd.texts[epd.texts.size() - 2].text == u8"\u300cA\u300d");
    assert(epd.texts.back().text == u8"\u2014\u2014B");
    assert(epd.texts.back().x == 360);

    publish(cache("A", u8"\"A\u4f5cB\""));
    assert(epd.texts.back().x == 336);
    publish(cache("A", "\"" + longQuote + "\""));
    assert(epd.texts.back().x == 0);
    assert(epd.texts.back().text.find(u8"\u2026") == std::string::npos);
    publish(cache("A", "\"" + longQuote + u8"\u5929\""));
    assert(epd.texts.back().x == 0);
    assert(epd.texts.back().text.find(u8"\u2026") != std::string::npos);
    publish(cache("A", "null", u8"\"\u51fa\u5904\""));
    assert(epd.texts.back().x == 336);
    assert(epd.texts.back().x + pixels(epd.texts.back().text.c_str()) == 400);
    std::cout << "Hitokoto parsing, MainUI layout and refresh integration tests passed\n";
}
