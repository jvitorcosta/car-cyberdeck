#include "sdcard.h"
#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <esp_heap_caps.h>
#include "../../common/core/watchdog.h"

namespace sdcard {
namespace {
constexpr int CS = 10, MOSI_PIN = 11, SCK_PIN = 12, MISO_PIN = 13;   // the board's card slot
constexpr uint32_t HZ = 20000000;                                    // read at ~1.2 MB/s on the bench
constexpr size_t CHUNK = 16 * 1024;
SPIClass s_spi(HSPI);   // the display holds the other SPI host
bool s_mounted = false;

File openFile(const char* name) {
    char path[48];
    snprintf(path, sizeof path, "/cardeck/%s", name);
    return SD.open(path, FILE_READ);
}
}  // namespace

bool begin() {
    if (s_mounted) return true;
    // A card left mid-read by a reset can refuse its first mount: ask again.
    for (int t = 0; t < 3 && !s_mounted; ++t) {
        if (t) delay(300);
        s_spi.begin(SCK_PIN, MISO_PIN, MOSI_PIN, CS);
        s_mounted = SD.begin(CS, s_spi, HZ, "/sd", 1 /* one file at a time */, false /* never format */);
        if (!s_mounted) {
            SD.end();
            s_spi.end();
        }
    }
    if (s_mounted) Serial.printf("[sd] card %lu MB\n", static_cast<unsigned long>(SD.cardSize() >> 20));
    else Serial.println("[sd] none (no card, or not FAT32): no media");
    return s_mounted;
}

void end() {
    if (!s_mounted) return;
    SD.end();
    s_spi.end();
    s_mounted = false;
}

bool has(const char* name) {
    if (!s_mounted) return false;
    File f = openFile(name);
    return f && !f.isDirectory() && f.size() > 0;
}

Blob load(const char* name) {
    Blob none{nullptr, 0};
    if (!s_mounted) return none;
    const uint32_t t0 = millis();
    File f = openFile(name);
    if (!f || f.isDirectory() || f.size() == 0) {
        Serial.printf("[sd] %s: not on the card\n", name);
        return none;
    }
    const size_t size = f.size();
    Blob b{static_cast<uint8_t*>(heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)), size};
    if (!b.data) {
        Serial.printf("[sd] %s: no memory for %u B\n", name, static_cast<unsigned>(size));
        return none;
    }
    size_t got = 0;
    while (got < size) {
        watchdog::feed();
        const size_t want = size - got < CHUNK ? size - got : CHUNK;
        const int n = f.read(b.data + got, want);
        if (n <= 0) break;
        got += static_cast<size_t>(n);
    }
    if (got != size) {
        Serial.printf("[sd] %s: read %u of %u B\n", name, static_cast<unsigned>(got), static_cast<unsigned>(size));
        release(b);
        return none;
    }
    Serial.printf("[sd] %s: %u B in %lu ms\n", name, static_cast<unsigned>(size),
                  static_cast<unsigned long>(millis() - t0));
    return b;
}

void release(Blob& b) {
    if (b.data) heap_caps_free(b.data);
    b = {nullptr, 0};
}
}  // namespace sdcard
