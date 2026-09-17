# MainUI Clock, Hitokoto and Weather Host Tests

Run from the repository root with native GCC/G++, not the Arm cross compiler.
The tests compile the real clock, shared heartbeat, text/weather parsers,
MainUI, C bridge and coreJSON. A forced-include stub substitutes display/Flash hardware and records
draw calls; the refresh result is supplied by the test.

```powershell
./tests/main_ui/run.ps1
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
- Successful hitokoto results repaint once; unrelated APIs, failed results and
  invalid quote content do not repaint.
- Successful todolist and todolist_inbox summaries do not repaint or replace the accepted quote.
- Clearing the old quote region before drawing and waking the sleeping panel
  before display; without a synchronized clock, automatic refresh returns to sleep.
- Manual redraw retains the last accepted quote; a later shorter quote replaces it.

Weather coverage: the four compact schemas, independent snapshots, duplicate/type/range/date/UTF-8 checks, exact 24-point rainfall and total consistency, first-warning selection and clearing, unchanged layout, precipitation total to the right of 2h, 18 adjoining bars for the first 90 minutes, short descriptions limited to nine characters (dry, continuous, starting, stopping and intermittent rain), two-hour totals and descriptions including rain beyond the visible range, and zero-rain clearing, missing-icon fallback, five-second pacing, request-busy suppression, merged results, manual redraw, display failure without retry loops, and tick wraparound.

These are parsing, layout-call and application integration tests. They do not
render the external Flash glyphs, validate electrical timing, simulate the
panel BUSY pin, or prove the ESP HTTPS request succeeds. Run the independent
[ESP workflow tests](../esp_com/README.md) as well.

Clock coverage: startup requests once, failure without retry, positive/negative UTC offsets, ACK calendar consistency, midnight/month/year/leap-day/weekday transitions, 2100 non-leap year, Unix 32-bit boundary, delayed polling with subsecond remainder, millisecond wraparound, placeholders, unchanged font sizes, full-width date fitting, minute-only partial window (248,0,152,40), busy-request deferral, 30 partials followed by a maintenance full refresh, sleep invalidation and partial-transfer failure recovery. The pure clock test injects time jumps; the UI test uses the production SystemHeartbeat counter. These tests do not measure actual TIM10 clock accuracy or physical partial-refresh ghosting.
