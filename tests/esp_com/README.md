# ESP Communication Host Tests

Run from the repository root with a native C compiler (not arm-none-eabi-gcc).
The stubs replace UART, GPIO, Shell output and display calls; production
Esp_Com.c and user_cmd.c are compiled without including the STM32 HAL.

```powershell
gcc -std=c11 -Wall -Wextra -Werror -Itests/esp_com/stubs -IFIFO/Inc tests/esp_com/test_esp_com.c FIFO/Src/Esp_Com.c LetterSh/Src/user_cmd.c -o cmake-build-debug/test_esp_com.exe
./cmake-build-debug/test_esp_com.exe
```

For CLion's bundled MinGW, add its `bin` directory to PATH before compiling
so GCC can find its runtime DLLs. The existing `cmake-build-debug` directory
is used only for the test executable; tests are not included in the firmware.

Coverage includes ready-low command suppression, diagnostics while not ready,
current/minutely5m/response request frames and CRC, HAL status propagation,
ready argument validation, unchanged flat JSON reception, CRC rejection, and
complete Shell output for empty, 226-byte and 384-byte payloads.

The daily3d/alert cases cover refresh/cache/read request frames, ready-low
suppression, numeric forecast temperatures, empty and partial warning lists,
invalid cache and failed-refresh payloads, and their unchanged Shell output.
The Shell read default is daily3d; EspCom_ReadApi(NULL) still requests current.

Hitokoto cases cover refresh/cache/read requests, readiness checks, UTF-8 quote
and source text, an escaped quote, string/null authors, unavailable cache and
failed-refresh payloads. These verify unchanged transport and console output;
the firmware does not yet parse JSON into display data.

These tests do not simulate ESP HTTP timing or prove UART electrical reliability.
