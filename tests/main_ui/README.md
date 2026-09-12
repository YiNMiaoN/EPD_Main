# MainUI Hitokoto Host Tests

Run from the repository root with native GCC/G++, not the Arm cross compiler.
The test compiles the real `HitokotoText.cpp`, `MainUI.cpp`, `Epd_Api.cpp` and
coreJSON. A forced-include stub substitutes display/Flash hardware and records
draw calls; the refresh result is supplied by the test.

```powershell
gcc -std=c11 -Wall -Wextra -Werror -IApiRefresh/ThirdParty/coreJSON -c ApiRefresh/ThirdParty/coreJSON/core_json.c -o cmake-build-debug/test_core_json.o
g++ -std=c++17 -Wall -Wextra -Werror -Wno-unused-parameter -include tests/main_ui/stubs/display.h -Itests/esp_com/stubs -ICore/Inc -IDisp/Inc -IQWeather/Inc -IFIFO/Inc -IApiRefresh/Inc -IApiRefresh/ThirdParty/coreJSON tests/main_ui/test_hitokoto.cpp Disp/Src/HitokotoText.cpp Disp/Src/MainUI.cpp Core/Src/Epd_Api.cpp cmake-build-debug/test_core_json.o -o cmake-build-debug/test_hitokoto.exe
./cmake-build-debug/test_hitokoto.exe
```

Use an existing `cmake-build-debug` directory, and put the native compiler's
`bin` directory on PATH. Unused parameter warnings are suppressed for the
existing hardware bridge functions compiled as part of this test.

Coverage:

- UTF-8, JSON string escapes, Unicode escapes and surrogate pairs.
- Null/empty authors falling back to source, then unnamed attribution.
- Invalid JSON, duplicate/nested fields, invalid UTF-8, empty text and invalid
  cache preserving the previous formatted text.
- The quote at (0, 268) and attribution at y=284, using the 16-pixel font.
- Zero right margin for short, mixed ASCII/Chinese, full-width and truncated
  attributions, including the fallback source. The start is 400 minus text width.
- Opening/closing quote brackets and two em dashes; truncation by glyph width
  retains the closing bracket and keeps both rows within 400 pixels.
- Successful hitokoto results repaint once; other APIs, failed results and
  invalid quote content do not repaint.
- Clearing the old quote region before drawing and waking the sleeping panel
  before display, then returning to sleep after automatic refresh.
- Manual redraw retains the last accepted quote; a later shorter quote replaces it.

These are parsing, layout-call and application integration tests. They do not
render the external Flash glyphs, validate electrical timing, simulate the
panel BUSY pin, or prove the ESP HTTPS request succeeds. Run the independent
[ESP workflow tests](../esp_com/README.md) as well.
