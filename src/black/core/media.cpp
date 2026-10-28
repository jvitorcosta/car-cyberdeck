#include "media.h"
#ifdef MEDIA_PARTITION
#include <Arduino.h>
#include <atomic>
#include <esp_partition.h>
#include "../../util/black/mediapack.h"

namespace media {
namespace {
constexpr int MAX_FILES = 32;
struct Pack { const uint8_t* data = nullptr; size_t size = 0; int files = 0; };
// Each file's CRC check: 0 not yet, 1 good, 2 bad. Racing checks agree, so a repeat is harmless.
std::atomic<uint8_t> s_checked[MAX_FILES];

Pack map() {
    Pack p;
    const esp_partition_t* part =
        esp_partition_find_first(ESP_PARTITION_TYPE_DATA, (esp_partition_subtype_t)0x40, "media");
    if (!part) {
        Serial.println("[media] none: no media partition (flash the new partition table)");
        return p;
    }
    uint8_t head[mediapack::HEAD];
    const uint32_t size = esp_partition_read(part, 0, head, sizeof head) == ESP_OK
                              ? mediapack::total(head, sizeof head) : 0;
    if (!size || size > part->size) {
        Serial.println("[media] none: the partition holds no pack (pio run -e s3 -t uploadmedia)");
        return p;
    }
    const void* at = nullptr;
    spi_flash_mmap_handle_t h;   // kept for ever: the media are read until power-off
    const esp_err_t err = esp_partition_mmap(part, 0, size, SPI_FLASH_MMAP_DATA, &at, &h);
    if (err != ESP_OK) {
        Serial.printf("[media] none: mapping %u B failed (%s)\n", (unsigned)size, esp_err_to_name(err));
        return p;
    }
    p.data = (const uint8_t*)at;
    p.size = size;
    p.files = mediapack::count(p.data, p.size);
    Serial.printf("[media] %d files, %u B\n", p.files, (unsigned)size);
    return p;
}

const Pack& pack() {
    static const Pack p = map();   // a function-local static: mapped once, thread-safe
    return p;
}
}

void begin() { (void)pack(); }

Blob get(const char* name) {
    const Pack& p = pack();
    const int i = mediapack::find(p.data, p.size, name);
    mediapack::File f{};
    if (i < 0 || !mediapack::at(p.data, p.size, i, &f)) return {nullptr, 0};
    if (i < MAX_FILES && s_checked[i].load() == 0) {
        const bool ok = mediapack::crc32(f.data, f.size) == f.crc;
        if (!ok) Serial.printf("[media] %s: bad CRC, left out\n", name);
        s_checked[i].store(ok ? 1 : 2);
    }
    if (i >= MAX_FILES || s_checked[i].load() != 1) return {nullptr, 0};
    return {f.data, f.size};
}
}
#endif
