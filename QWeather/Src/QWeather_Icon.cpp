//
// QWeather icon package reader.
//

#include "QWeather_Icon.h"

#include "TopInfo_Project.h"
#include <cstring>

namespace {

struct QWeatherIconHeader {
    char magic[4];
    uint16_t version;
    uint16_t flags;
    uint32_t icon_count;
    uint32_t header_size;
    uint32_t entry_size;
    uint32_t data_offset;
    uint32_t data_len;
    uint32_t reserved;
};

struct QWeatherIconEntry {
    char code[16];
    uint32_t offset16;
    uint16_t size16;
    uint16_t bytes16;
    uint32_t offset32;
    uint16_t size32;
    uint16_t bytes32;
};

QWeatherIconHeader icon_header = {};
bool icon_header_valid = false;

uint32_t read_le32_magic(const char *magic)
{
    return ((uint32_t)(uint8_t)magic[0]) |
           ((uint32_t)(uint8_t)magic[1] << 8) |
           ((uint32_t)(uint8_t)magic[2] << 16) |
           ((uint32_t)(uint8_t)magic[3] << 24);
}

bool code_equals(const char *entry_code, const char *code)
{
    return std::strncmp(entry_code, code, QWEATHER_ICON_CODE_MAX_LEN + 1U) == 0;
}

QWeatherIcon_Status validate_header(const QWeatherIconHeader &header)
{
    if (read_le32_magic(header.magic) != QWEATHER_ICON_MAGIC) {
        return QWEATHER_ICON_ERROR_FORMAT;
    }

    if (header.version != QWEATHER_ICON_VERSION ||
        header.header_size != QWEATHER_ICON_HEADER_SIZE ||
        header.entry_size != QWEATHER_ICON_ENTRY_SIZE ||
        header.icon_count == 0U) {
        return QWEATHER_ICON_ERROR_FORMAT;
    }

    if (header.icon_count > (0xFFFFFFFFUL / header.entry_size)) {
        return QWEATHER_ICON_ERROR_FORMAT;
    }

    uint32_t index_len = header.icon_count * header.entry_size;
    uint32_t index_end = header.header_size + index_len;
    if (index_end < header.header_size || header.data_offset < index_end) {
        return QWEATHER_ICON_ERROR_FORMAT;
    }

    return QWEATHER_ICON_OK;
}

QWeatherIcon_Status ensure_header(void)
{
    if (icon_header_valid) {
        return QWEATHER_ICON_OK;
    }

    // Header is stored at the beginning of qweather_icons.bin in external Flash.
    flash.read(QWEATHER_ICON_FLASH_BASE,
               reinterpret_cast<uint8_t *>(&icon_header),
               sizeof(icon_header));

    QWeatherIcon_Status status = validate_header(icon_header);
    icon_header_valid = (status == QWEATHER_ICON_OK);
    return status;
}

QWeatherIcon_Status find_entry(const char *code, QWeatherIconEntry *entry)
{
    if (code == nullptr || entry == nullptr || code[0] == '\0') {
        return QWEATHER_ICON_ERROR_ARG;
    }

    QWeatherIcon_Status status = ensure_header();
    if (status != QWEATHER_ICON_OK) {
        return status;
    }

    // Index entries are fixed-size records after the header.
    for (uint32_t i = 0; i < icon_header.icon_count; i++) {
        uint32_t addr = QWEATHER_ICON_FLASH_BASE +
                        icon_header.header_size +
                        i * icon_header.entry_size;

        flash.read(addr, reinterpret_cast<uint8_t *>(entry), sizeof(*entry));
        entry->code[QWEATHER_ICON_CODE_MAX_LEN] = '\0';

        if (code_equals(entry->code, code)) {
            return QWEATHER_ICON_OK;
        }
    }

    return QWEATHER_ICON_ERROR_NOT_FOUND;
}

QWeatherIcon_Status bitmap_location(const QWeatherIconEntry &entry,
                                    uint16_t size,
                                    uint32_t *offset,
                                    uint16_t *bytes)
{
    if (offset == nullptr || bytes == nullptr) {
        return QWEATHER_ICON_ERROR_ARG;
    }

    if (size == QWEATHER_ICON_SIZE_16) {
        if (entry.size16 != QWEATHER_ICON_SIZE_16 ||
            entry.bytes16 != QWEATHER_ICON_BYTES_16) {
            return QWEATHER_ICON_ERROR_FORMAT;
        }
        *offset = entry.offset16;
        *bytes = entry.bytes16;
    } else if (size == QWEATHER_ICON_SIZE_32) {
        if (entry.size32 != QWEATHER_ICON_SIZE_32 ||
            entry.bytes32 != QWEATHER_ICON_BYTES_32) {
            return QWEATHER_ICON_ERROR_FORMAT;
        }
        *offset = entry.offset32;
        *bytes = entry.bytes32;
    } else {
        return QWEATHER_ICON_ERROR_SIZE;
    }

    uint32_t data_end = icon_header.data_offset + icon_header.data_len;
    if (data_end < icon_header.data_offset ||
        *offset < icon_header.data_offset ||
        *offset > data_end ||
        *bytes > (data_end - *offset)) {
        return QWEATHER_ICON_ERROR_FORMAT;
    }

    return QWEATHER_ICON_OK;
}

} // namespace

extern "C" {

QWeatherIcon_Status QWeatherIcon_Init(void)
{
    icon_header_valid = false;
    return ensure_header();
}

QWeatherIcon_Status QWeatherIcon_GetInfo(QWeatherIcon_Info *info)
{
    if (info == nullptr) {
        return QWEATHER_ICON_ERROR_ARG;
    }

    QWeatherIcon_Status status = ensure_header();
    if (status != QWEATHER_ICON_OK) {
        return status;
    }

    info->icon_count = icon_header.icon_count;
    info->data_offset = icon_header.data_offset;
    info->data_len = icon_header.data_len;
    return QWEATHER_ICON_OK;
}

QWeatherIcon_Status QWeatherIcon_ReadBitmap(const char *code,
                                            uint16_t size,
                                            uint8_t *bitmap,
                                            uint16_t bitmap_len)
{
    if (bitmap == nullptr) {
        return QWEATHER_ICON_ERROR_ARG;
    }

    QWeatherIconEntry entry = {};
    uint32_t offset = 0;
    uint16_t bytes = 0;

    QWeatherIcon_Status status = find_entry(code, &entry);
    if (status != QWEATHER_ICON_OK) {
        return status;
    }

    status = bitmap_location(entry, size, &offset, &bytes);
    if (status != QWEATHER_ICON_OK) {
        return status;
    }

    if (bitmap_len < bytes) {
        return QWEATHER_ICON_ERROR_ARG;
    }

    flash.read(QWEATHER_ICON_FLASH_BASE + offset, bitmap, bytes);
    return QWEATHER_ICON_OK;
}

QWeatherIcon_Status QWeatherIcon_Draw(const char *code,
                                      uint16_t size,
                                      int16_t x,
                                      int16_t y)
{
    uint8_t bitmap[QWEATHER_ICON_BYTES_32];

    QWeatherIcon_Status status = QWeatherIcon_ReadBitmap(code,
                                                         size,
                                                         bitmap,
                                                         sizeof(bitmap));
    if (status != QWEATHER_ICON_OK) {
        return status;
    }

    epd.drawFontBitmap(x, y, bitmap, size, size);
    return QWEATHER_ICON_OK;
}

const char *QWeatherIcon_StatusString(QWeatherIcon_Status status)
{
    switch (status) {
        case QWEATHER_ICON_OK:
            return "ok";
        case QWEATHER_ICON_ERROR_FLASH:
            return "flash_error";
        case QWEATHER_ICON_ERROR_FORMAT:
            return "format_error";
        case QWEATHER_ICON_ERROR_ARG:
            return "arg_error";
        case QWEATHER_ICON_ERROR_NOT_FOUND:
            return "not_found";
        case QWEATHER_ICON_ERROR_SIZE:
            return "size_error";
        default:
            return "unknown_error";
    }
}

} // extern "C"
