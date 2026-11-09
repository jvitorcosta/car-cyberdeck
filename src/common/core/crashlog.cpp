#include "crashlog.h"
#include <Arduino.h>
#include <Preferences.h>
#include <esp_attr.h>
#include <esp_heap_caps.h>
#include <esp_rom_sys.h>
#include <esp_system.h>
#include "../../util/common/crashrecord.h"

namespace crashlog {
namespace {
constexpr uint32_t MAGIC = 0xC4A5106B;
// The run in progress; a reset keeps it (not a power-on: MAGIC tells).
RTC_NOINIT_ATTR uint32_t s_magic;
RTC_NOINIT_ATTR crashrecord::Record s_run;
uint32_t s_tickAt = 0;

// Any task, inside the allocator: no allocation, no Serial (its lock), just the counts and a ROM print.
void IRAM_ATTR onFailedAlloc(size_t size, uint32_t caps, const char* fn) {
    s_run.fails++;
    s_run.failSize = static_cast<uint32_t>(size);
    esp_rom_printf("[mem] alloc of %u B failed (caps 0x%x, %s) at %s\n", static_cast<unsigned>(size),
                   static_cast<unsigned>(caps), fn ? fn : "?", s_run.at[0] ? s_run.at : "-");
}
}  // namespace

void begin() {
    const uint8_t reason = static_cast<uint8_t>(esp_reset_reason());
    Serial.printf("[boot] reset reason: %s\n", crashrecord::reasonName(reason));
    Preferences p;
    p.begin("crashlog", false);
    crashrecord::Record ring[crashrecord::HISTORY] = {};
    if (p.getBytesLength("h") == sizeof(ring)) p.getBytes("h", ring, sizeof(ring));
    char line[160];
    if (s_magic == MAGIC) {  // the run before this one: it ended with this boot's reset
        s_run.reason = reason;
        s_run.at[crashrecord::AT_LEN - 1] = 0;
        crashrecord::format(s_run, line, sizeof(line));
        Serial.printf("[crash] last run: %s\n", line);
        crashrecord::push(ring, s_run);
        p.putBytes("h", ring, sizeof(ring));
    }
    for (int i = 0; i < crashrecord::HISTORY; i++) {
        if (!ring[i].reason) break;
        crashrecord::format(ring[i], line, sizeof(line));
        Serial.printf("[crash] %d: %s\n", i + 1, line);
    }
    p.end();
    s_run = crashrecord::Record{};
    s_magic = MAGIC;
    heap_caps_register_failed_alloc_callback(onFailedAlloc);
}

void tick() {
    if (millis() - s_tickAt < 1000) return;
    s_tickAt = millis();
    s_run.upS = millis() / 1000;
    s_run.heap = ESP.getFreeHeap();
    s_run.minHeap = ESP.getMinFreeHeap();
    s_run.largest = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
}

void at(const char* phase) { crashrecord::mark(s_run.at, phase); }

uint32_t fails() { return s_run.fails; }
}  // namespace crashlog
