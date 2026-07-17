# QWeather Icons Flash Bin Generator

This project converts QWeather outline SVG icons into a binary asset package for embedded firmware Flash storage.

## What Gets Generated

Run:

```powershell
py main.py
```

Outputs are written to `generated/`:

- `qweather_icons.bin`: binary file for Flash programming.
- `qweather_icons.index.csv`: human-readable index with icon code and bitmap offsets.
- `qweather_icons.preview.png`: random visual sample of generated icons.
- `qweather_icons.preview.txt`: ASCII preview for the same random sample.

Only outline icons are included. Files ending in `-fill.svg` are skipped.

## Bitmap Rule

Each bitmap uses:

```text
scan      = row
bit-order = msb
polarity  = normal
```

That means rows are stored from top to bottom. Inside each byte, the leftmost pixel is bit 7. A `1` bit means icon pixel on, and a `0` bit means background.

Each icon has two bitmaps:

- `16x16`: 32 bytes
- `32x32`: 128 bytes

## Bin Format

All multi-byte fields are little-endian.

Header, 32 bytes:

```c
struct qwic_header {
    char     magic[4];       // "QWIC"
    uint16_t version;        // 1
    uint16_t flags;          // 0
    uint32_t icon_count;
    uint32_t header_size;    // 32
    uint32_t entry_size;     // 32
    uint32_t data_offset;
    uint32_t data_len;
    uint32_t reserved;       // 0
};
```

Index entry, 32 bytes each:

```c
struct qwic_entry {
    char     code[16];       // NUL-terminated icon code, for example "100"
    uint32_t offset16;       // absolute offset in qweather_icons.bin
    uint16_t size16;         // 16
    uint16_t bytes16;        // 32
    uint32_t offset32;       // absolute offset in qweather_icons.bin
    uint16_t size32;         // 32
    uint16_t bytes32;        // 128
};
```

Bitmap data starts at `data_offset`. For each icon, the 16x16 bitmap is stored first, followed by the 32x32 bitmap.

## STM32 Firmware Library

The firmware-side reader is in:

```text
QWeather/Inc/QWeather_Icon.h
QWeather/Src/QWeather_Icon.cpp
```

The icon package must be programmed into external SPI Flash at:

```text
0x265000
```

This address is defined by the firmware as:

```c
#define QWEATHER_ICON_FLASH_BASE 0x00265000UL
```

The library reads the `QWIC` header from that base address, finds an icon by QWeather icon code, and reads the selected bitmap by the offsets stored in the package index. The offsets inside `qweather_icons.bin` are relative to the start of that bin, so the runtime Flash read address is:

```text
QWEATHER_ICON_FLASH_BASE + offset
```

### Firmware API

```c
#include "QWeather_Icon.h"

QWeatherIcon_Status QWeatherIcon_Init(void);
QWeatherIcon_Status QWeatherIcon_GetInfo(QWeatherIcon_Info *info);
QWeatherIcon_Status QWeatherIcon_ReadBitmap(const char *code,
                                            uint16_t size,
                                            uint8_t *bitmap,
                                            uint16_t bitmap_len);
QWeatherIcon_Status QWeatherIcon_Draw(const char *code,
                                      uint16_t size,
                                      int16_t x,
                                      int16_t y);
const char *QWeatherIcon_StatusString(QWeatherIcon_Status status);
```

Supported bitmap sizes:

```text
QWEATHER_ICON_SIZE_16 = 16
QWEATHER_ICON_SIZE_32 = 32
```

Example:

```c
QWeatherIcon_Status st;

st = QWeatherIcon_Init();
if (st == QWEATHER_ICON_OK) {
    QWeatherIcon_Draw("100", QWEATHER_ICON_SIZE_32, 0, 0);
    EPD_HW_Display();
}
```

To read the raw bitmap:

```cpp
#include "Epd.h"

extern EPD epd;

uint8_t icon[QWEATHER_ICON_BYTES_32];

if (QWeatherIcon_ReadBitmap("101", QWEATHER_ICON_SIZE_32, icon, sizeof(icon)) == QWEATHER_ICON_OK) {
    epd.drawFontBitmap(40, 0, icon, 32, 32);
}
```

`QWeatherIcon_Draw()` draws into the single project EPD GRAM buffer. Call `EPD_HW_Display()` afterward to refresh the e-paper screen.

## Preview Options

The preview is intentionally a random subset, not all icons.

```powershell
py main.py --preview-count 32 --preview-seed 1234
```

Use `--no-preview` when only the Flash bin and CSV index are needed.

## Dependencies

The script needs Python packages:

```powershell
py -m pip install pillow svg.path
```

`Pillow` is used for PNG preview generation. `svg.path` is used to parse SVG path data before rasterizing to 1bpp bitmaps.
