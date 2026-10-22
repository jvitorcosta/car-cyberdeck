#include "ota.h"
#include <Arduino.h>
#include <ArduinoOTA.h>
#include <Preferences.h>
#include <atomic>
#include <cstring>
#include <esp_ota_ops.h>
#include <esp_system.h>
#include "remote_config.h"
#include "wifi.h"
#include "../audio/greeting.h"
#include "../util/bootguard.h"
#include "../util/otaview.h"

namespace ota {
namespace {
// Survive a crash reset (not a power-on): the crash count and the last rollback, for its log line.
RTC_NOINIT_ATTR uint32_t s_magic;
RTC_NOINIT_ATTR uint32_t s_crashes;
RTC_NOINIT_ATTR uint32_t s_rolledBack;   // MAGIC: the boot before this one rolled back

// The update's state (util/otaview), written by ArduinoOTA's callbacks on this task, read by the UI.
otaview::Tracker s_tracker;
portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
template <typename F> void locked(F f) {
    portENTER_CRITICAL(&s_mux);
    f();
    portEXIT_CRITICAL(&s_mux);
}
std::atomic<bool> s_good{false};   // the running firmware has proven itself (nvs "ota"/"good")
bool (*s_hold)() = nullptr;

// The running firmware's ELF SHA-256 (hex, first 16 chars): what nvs remembers as good.
void runningId(char* out, size_t n) {
    char sha[65] = "";
    esp_ota_get_app_elf_sha256(sha, sizeof sha);
    strlcpy(out, sha, n < 17 ? n : 17);
}

bool isGood() {
    char id[17], good[17] = "";
    runningId(id, sizeof id);
    Preferences p;
    p.begin("ota", true);
    p.getString("good", good, sizeof good);
    p.end();
    return strcmp(id, good) == 0;
}

void task(void*) {
    while ((s_hold && s_hold()) || !net::isOnline()) vTaskDelay(pdMS_TO_TICKS(200));
    ArduinoOTA.setPort(3232);
    ArduinoOTA.setHostname(REMOTE_NAME);
    ArduinoOTA.setPassword(REMOTE_TOKEN);
    ArduinoOTA.setMdnsEnabled(false);   // net/remote already answers to REMOTE_NAME.local
    ArduinoOTA.setRebootOnSuccess(false);   // the deck reboots itself, after the screen's flash
    ArduinoOTA.onStart([] {
        locked([] { s_tracker.onStart(); });
        greeting::stopTunes();
        Serial.println("[ota] update started: Spotify polls paused");
    });
    ArduinoOTA.onProgress([](unsigned done, unsigned total) {
        const int pct = total ? (int)((uint64_t)done * 100 / total) : 0;
        int was = 0;
        locked([&] { was = s_tracker.pct(); s_tracker.onProgress(pct); });
        if (pct / 10 != was / 10) Serial.printf("[ota] %d %%\n", pct);
    });
    ArduinoOTA.onEnd([] {
        locked([] { s_tracker.onEnd(millis()); });
        Serial.println("[ota] done, restarting into the new firmware");
    });
    ArduinoOTA.onError([](ota_error_t e) {
        locked([] { s_tracker.onError(millis()); });
        Serial.printf("[ota] failed (error %d): the running firmware stays\n", (int)e);
    });
    ArduinoOTA.begin();
    Serial.printf("[ota] listening on %s:3232\n", net::deviceIp().c_str());
    for (;;) {
        ArduinoOTA.handle();   // an update runs to the end inside this call
        // Still Receiving: it was cut off on a path that skips onError. Resume the deck.
        bool cut = false, reboot = false;
        locked([&] {
            cut = s_tracker.phase(millis()) == otaview::Phase::Receiving;
            s_tracker.onHandleReturned(millis());
            reboot = s_tracker.rebootDue(millis());
        });
        if (cut) Serial.println("[ota] update ended without the new firmware: the running one stays");
        if (reboot) {   // DONE_MS after the end: the screen has flashed
            delay(100);
            esp_restart();
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
}

void checkBoot() {
    const esp_reset_reason_t r = esp_reset_reason();
    const bool crashed = r == ESP_RST_PANIC || r == ESP_RST_INT_WDT || r == ESP_RST_TASK_WDT ||
                         r == ESP_RST_WDT;
    if (s_rolledBack == bootguard::MAGIC) Serial.println("[ota] rolled back to the previous firmware");
    s_rolledBack = 0;
    s_crashes = bootguard::afterBoot(s_magic, s_crashes, crashed);
    s_magic = bootguard::MAGIC;
    const esp_partition_t* running = esp_ota_get_running_partition();
    s_good.store(isGood());
    Serial.printf("[ota] running %s%s, crashes in a row %u\n", running ? running->label : "?",
                  s_good.load() ? " (good)" : "", (unsigned)s_crashes);
    if (!s_crashes) return;
    const esp_partition_t* other = esp_ota_get_next_update_partition(nullptr);
    esp_app_desc_t desc;
    const bool otherValid = other && esp_ota_get_partition_description(other, &desc) == ESP_OK;
    if (!bootguard::rollBack(s_crashes, s_good.load(), otherValid)) return;
    if (esp_ota_set_boot_partition(other) != ESP_OK) {
        Serial.printf("[ota] rollback to %s failed: its image does not verify\n", other->label);
        return;
    }
    // The build's date: Arduino builds leave desc.version at the IDF version.
    Serial.printf("[ota] rolled back: %u crashes in a row, booting %s (built %s %s)\n",
                  (unsigned)s_crashes, other->label, desc.date, desc.time);
    s_crashes = 0;
    s_rolledBack = bootguard::MAGIC;
    delay(100);
    esp_restart();
}

void begin(bool (*hold)()) {
    if (!REMOTE_TOKEN[0]) { Serial.println("[ota] off (REMOTE_TOKEN empty)"); return; }
    s_hold = hold;
    // Core 0 with the network task, which idles during an update. 4 KB: ArduinoOTA's 1460-byte
    // buffer is static, not on the stack.
    xTaskCreatePinnedToCore(task, "ota", 4096, nullptr, 1, nullptr, 0);
}

View view() {
    View v{otaview::Phase::Idle, -1};
    locked([&] { v = {s_tracker.phase(millis()), s_tracker.pct()}; });
    if (v.phase == otaview::Phase::Idle) v.pct = -1;
    return v;
}

bool active() {
    const otaview::Phase p = view().phase;
    return p == otaview::Phase::Receiving || p == otaview::Phase::Done;
}
int progress() { return active() ? view().pct : -1; }

void onGoodPoll() {
    if (s_good.load() || !bootguard::healthy(millis(), true)) return;
    char id[17];
    runningId(id, sizeof id);
    Preferences p;
    p.begin("ota", false);
    p.putString("good", id);
    p.end();
    s_good.store(true);
    s_crashes = 0;
    Serial.printf("[ota] this firmware is good (%s)\n", id);
}
}
