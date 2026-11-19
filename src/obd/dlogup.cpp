#include "dlogup.h"
#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <esp_heap_caps.h>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <new>
#include "obdlink.h"
#include "../config.h"
#include "../common/core/watchdog.h"
#include "../common/net/clock.h"
#include "../common/net/http_config.h"
#include "../common/net/ota.h"
#include "../common/net/wifi.h"
#include "../util/car/drivelog.h"

#ifndef DLOG_REPO
#define DLOG_REPO ""
#endif
#ifndef DLOG_TOKEN
#define DLOG_TOKEN ""
#endif

namespace dlogup {
namespace {
using obd::Pid;

#ifdef OBD_BENCH
constexpr bool ON = false;   // the bench's simulated drive is never logged
#else
constexpr bool ON = sizeof(DLOG_REPO) > 1 && sizeof(DLOG_TOKEN) > 1;   // both set
#endif
constexpr uint32_t MAX_WAIT_MS = 60000;      // a short part goes after this (the drive's last minute)
constexpr uint32_t NEXT_MS = 2000;           // after a sent part: the next one soon (catching up)
constexpr uint32_t RETRY_MS = 30000;         // no answer, no heap: again after this
constexpr uint32_t REFUSED_MS = 10 * 60000;  // GitHub said no (token, repo): rarely
// TLS on the classic ESP32: two record buffers (16 KB in, 4 KB out) and the handshake in one block,
// and the body beside it; NimBLE and the server keep the rest. Below either: the part waits.
constexpr size_t TLS_LARGEST = 40000, TLS_FREE = 60000;
constexpr uint32_t STACK = 8192;             // the handshake runs on this task

SemaphoreHandle_t s_lock = nullptr;
drivelog::Ring* s_ring = nullptr;            // under s_lock; allocated only when on (~7 KB)
bool s_tripOn = false;                       // under s_lock: the trip's first record exists
uint32_t s_tripMs = 0;                       // under s_lock: millis() of that record
TaskHandle_t s_task = nullptr;

// The upload task's own (statics: off its stack).
char s_dir[32] = "";                         // "drive/20261119-225001", once the clock is known
uint32_t s_part = 0;                         // parts sent
uint32_t s_pendFirst = 0, s_pendCount = 0;   // the part being retried: the same rows, the same file
uint32_t s_linkupSent = 0;                   // the link-up number last sent
drivelog::Row s_rows[drivelog::BATCH];
char s_linkupText[1024];

// Why nothing went, logged once per change.
enum class Wait : uint8_t { None, Offline, Clock, Heap };
Wait s_wait = Wait::None;
void waitFor(Wait w) {
    if (w == s_wait) return;
    s_wait = w;
    if (w == Wait::Offline) Serial.println("[dlog] waiting: no hotspot (or an update): the rows stay in RAM");
    if (w == Wait::Clock) Serial.println("[dlog] waiting: no clock yet");
    if (w == Wait::Heap)
        Serial.printf("[dlog] waiting: heap %u, largest %u (TLS needs %u)\n",
                      static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_8BIT)),
                      static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)),
                      static_cast<unsigned>(TLS_LARGEST));
}

// PUT path as a new file. *http: the status (or HTTPClient's negative code).
drivelog::Outcome put(const char* path, const uint8_t* data, size_t n, int* http) {
    *http = -1;
    char msg[64];
    snprintf(msg, sizeof msg, "cardeck %s", path);
    const size_t cap = drivelog::bodyLen(msg, n) + 1;
    char* body = static_cast<char*>(malloc(cap));
    if (!body) return drivelog::Outcome::Retry;
    const size_t len = drivelog::putBody(msg, data, n, body, cap);
    char url[160];
    snprintf(url, sizeof url, "https://api.github.com/repos/%s/contents/%s", DLOG_REPO, path);
    int rc = -1;
    {
        WiFiClientSecure client;
        netcfg::github(client);   // the bundled roots (USERTrust: GitHub's), bounded handshake
        HTTPClient https;
        https.setConnectTimeout(netcfg::HTTP_TIMEOUT_MS);
        https.setTimeout(netcfg::HTTP_TIMEOUT_MS);
        if (len && https.begin(client, url)) {
            https.addHeader("Authorization", "Bearer " DLOG_TOKEN);
            https.addHeader("Accept", "application/vnd.github+json");
            https.addHeader("X-GitHub-Api-Version", "2022-11-28");
            https.addHeader("Content-Type", "application/json");
            https.addHeader("User-Agent", netcfg::USER_AGENT);
            watchdog::feed();   // DNS 15 s + connect 8 s + handshake 8 s + the answer 8 s, at worst
            rc = https.PUT(reinterpret_cast<uint8_t*>(body), len);
            https.end();
            if (rc < 0) netcfg::certRejected(client, "api.github.com");
        }
    }
    free(body);
    *http = rc;
    return drivelog::outcome(rc);
}

// The latest link-up's raw replies, once per link-up. True when one was due (sent or not).
bool sendLinkup(uint32_t* nextAt) {
    const uint32_t k = obdlink::linkupRaw(nullptr, 0);
    if (k == 0 || k == s_linkupSent) return false;
    obdlink::linkupRaw(s_linkupText, sizeof s_linkupText);
    char path[64];
    drivelog::linkupPath(s_dir, k, path, sizeof path);
    int http;
    const uint32_t t0 = millis();
    const drivelog::Outcome o = put(path, reinterpret_cast<const uint8_t*>(s_linkupText), strlen(s_linkupText), &http);
    if (o == drivelog::Outcome::Done) s_linkupSent = k;
    *nextAt = millis() + (o == drivelog::Outcome::Done ? NEXT_MS : o == drivelog::Outcome::Refused ? REFUSED_MS : RETRY_MS);
    Serial.printf("[dlog] %s %s (HTTP %d, %lu ms)\n", o == drivelog::Outcome::Done ? "sent" : "not sent", path, http,
                  static_cast<unsigned long>(millis() - t0));
    return true;
}

// The oldest rows as the next part (the same rows again while it's being retried).
void sendPart(int64_t unixNow, uint32_t nowMs, uint32_t* nextAt) {
    uint32_t first, count;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    first = s_ring->firstSeq();
    count = s_ring->size() < drivelog::BATCH ? s_ring->size() : drivelog::BATCH;
    if (s_pendCount && s_pendFirst == first && s_pendCount < count) count = s_pendCount;
    for (uint32_t i = 0; i < count; ++i) s_rows[i] = s_ring->at(i);
    const uint32_t dropped = s_ring->dropped();
    xSemaphoreGive(s_lock);
    s_pendFirst = first;
    s_pendCount = count;
    const size_t cap = strlen(drivelog::HEADER) + 1 + count * 96;
    char* csv = static_cast<char*>(malloc(cap));
    if (!csv) {
        *nextAt = millis() + RETRY_MS;
        return;
    }
    size_t len = strlcpy(csv, drivelog::HEADER, cap);
    for (uint32_t i = 0; i < count; ++i)
        len += drivelog::csvLine(s_rows[i], drivelog::rowUnix(unixNow, nowMs, s_rows[i].ms), csv + len, cap - len);
    char path[64];
    drivelog::partPath(s_dir, s_part + 1, path, sizeof path);
    int http;
    const uint32_t t0 = millis();
    const drivelog::Outcome o = put(path, reinterpret_cast<const uint8_t*>(csv), len, &http);
    free(csv);
    if (o == drivelog::Outcome::Done) {
        xSemaphoreTake(s_lock, portMAX_DELAY);
        s_ring->release(first + count);
        xSemaphoreGive(s_lock);
        ++s_part;
        s_pendCount = 0;
    }
    *nextAt = millis() + (o == drivelog::Outcome::Done ? NEXT_MS : o == drivelog::Outcome::Refused ? REFUSED_MS : RETRY_MS);
    Serial.printf("[dlog] %s %s: %lu rows (HTTP %d, %lu ms, %lu dropped so far, stack left %u)\n",
                  o == drivelog::Outcome::Done ? "sent" : "not sent", path, static_cast<unsigned long>(count), http,
                  static_cast<unsigned long>(millis() - t0), static_cast<unsigned long>(dropped),
                  static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)));
}

void task(void*) {
    watchdog::watch();   // put() feeds before its one request, ~40 s at worst
    uint32_t nextAt = 0;
    for (;;) {
        watchdog::feed();
        vTaskDelay(pdMS_TO_TICKS(1000));
        const uint32_t now = millis();
        if (static_cast<int32_t>(now - nextAt) < 0) continue;
        bool tripOn;
        uint32_t tripMs, pending, oldestMs;
        xSemaphoreTake(s_lock, portMAX_DELAY);
        tripOn = s_tripOn;
        tripMs = s_tripMs;
        pending = s_ring->size();
        oldestMs = pending ? s_ring->at(0).ms : now;
        xSemaphoreGive(s_lock);
        const uint32_t k = obdlink::linkupRaw(nullptr, 0);
        const bool linkupDue = k && k != s_linkupSent;
        if (!tripOn || (!linkupDue && !drivelog::partDue(pending, oldestMs, now, MAX_WAIT_MS))) continue;
        // Nothing is tried without the hotspot: no DNS, no connect, no waits; the rows stay.
        if (!net::isOnline() || ota::active()) {
            waitFor(Wait::Offline);
            continue;
        }
        int64_t unixNow;
        if (!netclock::unixNow(&unixNow) ||
            (!s_dir[0] && !drivelog::tripDir(static_cast<int64_t>(drivelog::rowUnix(unixNow, now, tripMs)), s_dir, sizeof s_dir))) {
            waitFor(Wait::Clock);
            continue;
        }
        if (heap_caps_get_largest_free_block(MALLOC_CAP_8BIT) < TLS_LARGEST ||
            heap_caps_get_free_size(MALLOC_CAP_8BIT) < TLS_FREE) {
            waitFor(Wait::Heap);
            nextAt = now + RETRY_MS;
            continue;
        }
        waitFor(Wait::None);
        if (!sendLinkup(&nextAt)) sendPart(unixNow, now, &nextAt);
    }
}
}  // namespace

void begin() {
    if (!ON) {
        Serial.println("[dlog] off (DLOG_REPO / DLOG_TOKEN empty, or the bench)");
        return;
    }
    s_ring = new (std::nothrow) drivelog::Ring();
    s_lock = xSemaphoreCreateMutex();
    if (!s_ring || !s_lock) {
        Serial.println("[dlog] off: no memory");
        return;
    }
    // Core 1, priority 0: below loop() (which takes the rows), beside the server task; the OBD tasks
    // have core 0 to themselves.
    xTaskCreatePinnedToCore(task, "dlog", STACK, nullptr, 0, &s_task, 1);
    Serial.printf("[dlog] on: %s, one part per %lu rows\n", DLOG_REPO, static_cast<unsigned long>(drivelog::BATCH));
}

void second() {
    if (!s_task) return;
    static obdlink::View v;   // loop task only
    obdlink::snapshot(v);
    const uint32_t now = millis();
    const bool live = v.link == obdplan::Link::Live;
    const bool linkup = obdlink::linkupRaw(nullptr, 0) != 0;
    if (!live && !linkup) return;
    drivelog::Row row{};
    if (live) {
        float pedal = obdlink::fresh(v, Pid::PedalD, now);
        if (std::isnan(pedal)) pedal = obdlink::fresh(v, Pid::PedalE, now);
        const drivelog::Sample s{obdlink::fresh(v, Pid::Speed, now),    obdlink::fresh(v, Pid::Rpm, now),
                                 obdlink::fresh(v, Pid::Coolant, now),  obdlink::fresh(v, Pid::Intake, now),
                                 obdlink::fresh(v, Pid::Throttle, now), pedal,
                                 obdlink::fresh(v, Pid::Load, now),     obdlink::fresh(v, Pid::Battery, now)};
        row = drivelog::pack(now, s);
    }
    xSemaphoreTake(s_lock, portMAX_DELAY);
    if (!s_tripOn) {
        s_tripOn = true;
        s_tripMs = now;
    }
    // A row only while the car answers, and only with speed or rpm in it (a stale link writes none).
    if (live && (row.kmh10 != drivelog::NONE || row.rpm != drivelog::NONE)) s_ring->push(row);
    xSemaphoreGive(s_lock);
}
}  // namespace dlogup
