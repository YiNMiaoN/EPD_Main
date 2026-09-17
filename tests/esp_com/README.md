# ESP Communication Host Tests

Run from the repository root with a native C compiler (not arm-none-eabi-gcc).
The stubs replace UART, GPIO, Shell output and display calls; production
Esp_Com.c, ApiRefresh.c, coreJSON and both Shell command modules are compiled
without including the STM32 HAL.

```powershell
gcc -std=c11 -Wall -Wextra -Werror -Itests/esp_com/stubs -IFIFO/Inc -IApiRefresh/Inc -IApiRefresh/ThirdParty/coreJSON tests/esp_com/test_esp_com.c FIFO/Src/Esp_Com.c LetterSh/Src/user_cmd.c ApiRefresh/Src/ApiRefresh.c ApiRefresh/Src/ApiTime.c ApiRefresh/Src/ApiRefresh_Shell.c ApiRefresh/ThirdParty/coreJSON/core_json.c -o cmake-build-debug/test_esp_com.exe
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

Refresh workflow tests use manually advanced TIM10 ticks and cover all seven
APIs, idle/completed inactivity (no periodic refresh), no sends from Tick,
Ready transitions, ACK settling, sequence matching, response/cache validation,
UTF-8 preservation, malformed/duplicate/nested/wrong-type JSON fields, ESP
errors, rejected and failed refreshes, invalid cache, transmit failure, and
timeouts at every waiting stage. They also cover busy command suppression,
esp_last consumption, multiple frames in one Poll, and an on-time parsed ACK
processed after the deadline. No HAL_GetTick implementation is linked.

These tests do not measure TIM10's physical interrupt period, simulate actual
ESP HTTP timing, or prove UART electrical reliability.

NTP tests cover READ_API time framing, its single final ACK (no cache queries),
the separate 10s deadline, wrong SEQ, ERROR and invalid replies, transmit failure,
mutual exclusion with refresh/display commands, esp_last consumption, clearing
previous time on a new request, parsed Shell output and no automatic polling.
Parser tests cover missing/duplicate/nested fields, integer types and overflow,
signed timezone offsets, valid leap day, invalid dates/times and weekday range.
They do not contact an NTP server or establish that the returned clock is correct.

Todoist tests cover both today and Inbox summaries: raw refresh/cache/read framing,
ready-low suppression, shared request exclusion, ACK/Ready/response/cache flow,
empty lists and partial UTF-8 task summaries with count greater than items length.
Stage validation rejects missing, duplicate, nested, wrong-type, old http_probe and
cross-view stages in both result and cache frames. Failures retain diagnostic JSON
and never read old cache, including HTTP 200 with body/JSON/cache failures.
The observed empty response is kept as a regression case without automatic retry.
No tests contact Todoist or validate task fields into a display model.
