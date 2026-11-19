#include "dlogup.h"
#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <esp_heap_caps.h>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <new>
#include "carfs.h"
#include "obdlink.h"
#include "../config.h"
#include "../common/core/watchdog.h"
#include "../common/net/clock.h"
#include "../common/net/http_config.h"
#include "../common/net/ota.h"
#include "../common/net/wifi.h"
#include "../util/car/drivelog.h"

#ifndef DLOG_URL
#define DLOG_URL ""
#endif
#ifndef DLOG_KEY
#define DLOG_KEY ""
#endif

namespace dlogup {
namespace {
using drivelog::Outcome;
using obd::Pid;

#ifdef OBD_BENCH
constexpr bool ON = false;   // the bench's simulated drive is never logged
#else
constexpr bool ON = sizeof(DLOG_URL) > 1 && sizeof(DLOG_KEY) > 1;   // both set
#endif
constexpr uint32_t MAX_WAIT_MS = 60000;      // a short part goes after this (the drive's last minute)
constexpr uint32_t NEXT_MS = 2000;           // after a part went: the next one soon (catching up)
constexpr uint32_t RETRY_MS = 30000;         // no answer (a dead zone), no heap: again after this
constexpr uint32_t REFUSED_MS = 10 * 60000;  // the web app said no (key, URL, deployment): rarely
constexpr int LINKUP_TRIES = 3;              // a link-up record that won't go is let go after this
// TLS on the classic ESP32: two record buffers (16 KB in, 4 KB out) and the handshake in one block;
// NimBLE and the server keep the rest. Checked with the part's body already allocated.
constexpr size_t TLS_LARGEST = 40000, TLS_FREE = 50000;
constexpr uint32_t STACK = 10240;            // the handshake runs on this task (the S3's net task: 10 KB)
constexpr const char* SPOOL = "/dlog.bin";   // rows waiting on flash (carfs; triplog lists only /t*.jsonl)

SemaphoreHandle_t s_lock = nullptr;
drivelog::Ring* s_ring = nullptr;            // under s_lock; allocated only when on (~7 KB)
bool s_tripOn = false;                       // under s_lock: the trip's first record exists
uint32_t s_tripMs = 0;                       // under s_lock: millis() of that record
uint32_t s_spoolRows = 0;                    // under s_lock (written by the loop task only)
uint32_t s_spoolDropped = 0;                 // loop task: rows lost with flash full too
TaskHandle_t s_task = nullptr;

// The loop task's: the spool on flash (only the loop task touches LittleFS, carfs).
carfs::Store s_store;
long s_spoolAt = 0;                          // the oldest spooled row's offset

// The upload task's own (statics: off its stack).
char s_trip[20] = "";                        // the Sheet tab, "20261119-225001", once the clock is known
uint32_t s_part = 0;                         // parts the Sheet has
uint32_t s_pendFirst = 0, s_pendCount = 0;   // the part being retried: the same rows again
uint32_t s_linkupSent = 0;                   // the link-up number last sent (or let go)
int s_linkupTries = 0;
drivelog::Row s_rows[drivelog::BATCH];
char s_linkupText[1024];

// Why nothing went, logged once per change.
enum class Wait : uint8_t { None, Offline, Clock, Heap };
Wait s_wait = Wait::None;
void waitFor(Wait w) {
    if (w == s_wait) return;
    s_wait = w;
    if (w == Wait::Offline) Serial.println("[dlog] waiting: no hotspot (or an update): the rows wait");
    if (w == Wait::Clock) Serial.println("[dlog] waiting: no clock yet");
    if (w == Wait::Heap)
        Serial.printf("[dlog] waiting: heap %u, largest %u (TLS needs %u)\n",
                      static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_8BIT)),
                      static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)),
                      static_cast<unsigned>(TLS_LARGEST));
}

bool heapOk() {
    return heap_caps_get_largest_free_block(MALLOC_CAP_8BIT) >= TLS_LARGEST &&
           heap_caps_get_free_size(MALLOC_CAP_8BIT) >= TLS_FREE;
}

// The web app's reply: its first line, at most out's size.
void readReply(HTTPClient& h, char* out, size_t n) {
    WiFiClient& s = h.getStream();
    s.setTimeout(netcfg::HTTP_TIMEOUT_MS / 1000);
    const size_t got = s.readBytes(out, n - 1);
    out[got] = '\0';
    out[strcspn(out, "\r\n")] = '\0';
}

// POSTs body to the web app, then follows its 302 to read what doPost answered. *http: that
// answer's status (or HTTPClient's negative code). Two requests, one after the other: one TLS
// session at a time.
Outcome post(const char* body, size_t len, int* http) {
    static const char* KEEP[] = {"Location"};   // HTTPClient takes a non-const array
    char reply[64] = "";
    int rc = -1;
    String location;
    {
        WiFiClientSecure client;
        netcfg::google(client);
        HTTPClient https;
        https.useHTTP10(true);   // never chunked
        https.setConnectTimeout(netcfg::HTTP_TIMEOUT_MS);
        https.setTimeout(netcfg::HTTP_TIMEOUT_MS);
        https.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);   // a POST redirect is read with a GET
        https.collectHeaders(KEEP, 1);
        if (https.begin(client, DLOG_URL)) {
            https.addHeader("Content-Type", "text/plain");
            https.setUserAgent(netcfg::USER_AGENT);
            watchdog::feed();   // DNS 15 s + connect 8 s + handshake 8 s + the answer 8 s, at worst
            rc = https.POST(reinterpret_cast<uint8_t*>(const_cast<char*>(body)), len);
            if (rc == 302 || rc == 303) location = https.header("Location");
            else if (rc == 200) readReply(https, reply, sizeof reply);
            https.end();
            if (rc < 0) netcfg::certRejected(client, "script.google.com");
        }
    }
    if (location.length()) {   // doPost has run: its answer waits there
        WiFiClientSecure client;
        netcfg::google(client);
        HTTPClient https;
        https.useHTTP10(true);
        https.setConnectTimeout(netcfg::HTTP_TIMEOUT_MS);
        https.setTimeout(netcfg::HTTP_TIMEOUT_MS);
        rc = -1;
        if (https.begin(client, location)) {
            https.setUserAgent(netcfg::USER_AGENT);
            watchdog::feed();
            rc = https.GET();
            if (rc == 200) readReply(https, reply, sizeof reply);
            https.end();
            if (rc < 0) netcfg::certRejected(client, "script.googleusercontent.com");
        }
    }
    *http = rc;
    const Outcome o = drivelog::outcome(rc, reply);
    if (o != Outcome::Done) Serial.printf("[dlog] the web app said \"%.40s\"\n", reply);
    return o;
}

uint32_t after(Outcome o) {
    return millis() + (o == Outcome::Done ? NEXT_MS : o == Outcome::Refused ? REFUSED_MS : RETRY_MS);
}

const char* said(Outcome o) { return o == Outcome::Done ? "sent" : o == Outcome::Refused ? "refused" : "not sent"; }

// The latest link-up's raw replies, once per link-up (let go after LINKUP_TRIES). True when one
// was tried, so the parts wait for the next pass.
bool sendLinkup(uint32_t* nextAt) {
    const uint32_t k = obdlink::linkupRaw(nullptr, 0);
    if (k == 0 || k == s_linkupSent) return false;
    obdlink::linkupRaw(s_linkupText, sizeof s_linkupText);
    char head[96];
    const size_t h = drivelog::postHead(head, sizeof head, DLOG_KEY, "linkup", s_trip, k);
    const size_t n = strlen(s_linkupText);
    char* body = static_cast<char*>(malloc(h + n + 1));
    if (!h || !body) {
        free(body);
        s_linkupSent = k;   // can't be built: never will be
        return false;
    }
    memcpy(body, head, h);
    memcpy(body + h, s_linkupText, n + 1);
    int http = -1;
    const uint32_t t0 = millis();
    const Outcome o = heapOk() ? post(body, h + n, &http) : Outcome::Retry;
    free(body);
    if (o == Outcome::Done || ++s_linkupTries >= LINKUP_TRIES) {
        s_linkupSent = k;
        s_linkupTries = 0;
    }
    *nextAt = after(o);
    Serial.printf("[dlog] link-up %lu %s (HTTP %d, %lu ms)\n", static_cast<unsigned long>(k), said(o), http,
                  static_cast<unsigned long>(millis() - t0));
    return true;
}

// The oldest rows as the next part. A retry sends the same first row again (the Sheet drops a part
// number it already has, "ok dup").
void sendPart(int64_t unixNow, uint32_t nowMs, uint32_t* nextAt) {
    uint32_t first, count;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    first = s_ring->firstSeq();
    count = s_ring->size() < drivelog::BATCH ? s_ring->size() : drivelog::BATCH;
    // a retry: exactly the rows of the try whose answer was lost (the Sheet keys a part by its number)
    if (s_pendCount && s_pendFirst == first && s_pendCount < count) count = s_pendCount;
    for (uint32_t i = 0; i < count; ++i) s_rows[i] = s_ring->at(i);
    const uint32_t dropped = s_ring->dropped();
    xSemaphoreGive(s_lock);
    if (count == 0) {   // the rows are still on their way back from flash
        *nextAt = millis() + 1000;
        return;
    }
    s_pendFirst = first;
    s_pendCount = count;
    char head[96];
    const size_t h = drivelog::postHead(head, sizeof head, DLOG_KEY, "rows", s_trip, s_part + 1);
    const size_t cap = h + strlen(drivelog::HEADER) + count * drivelog::LINE_CAP + 1;
    char* body = h ? static_cast<char*>(malloc(cap)) : nullptr;
    if (!body) {
        *nextAt = millis() + RETRY_MS;
        return;
    }
    memcpy(body, head, h + 1);
    size_t len = h + strlcpy(body + h, drivelog::HEADER, cap - h);
    for (uint32_t i = 0; i < count; ++i)
        len += drivelog::csvLine(s_rows[i], drivelog::rowUnix(unixNow, nowMs, s_rows[i].ms), body + len, cap - len);
    if (!heapOk()) {   // with the body taken: what TLS has left
        free(body);
        waitFor(Wait::Heap);
        *nextAt = millis() + RETRY_MS;
        return;
    }
    int http = -1;
    const uint32_t t0 = millis();
    const Outcome o = post(body, len, &http);
    free(body);
    if (o == Outcome::Done) {
        xSemaphoreTake(s_lock, portMAX_DELAY);
        s_ring->release(first + count);
        xSemaphoreGive(s_lock);
        ++s_part;
        s_pendCount = 0;
    }
    *nextAt = after(o);
    Serial.printf("[dlog] part %lu %s: %lu rows (HTTP %d, %lu ms, %lu dropped, stack left %u)\n",
                  static_cast<unsigned long>(s_part + (o == Outcome::Done ? 0 : 1)), said(o),
                  static_cast<unsigned long>(count), http, static_cast<unsigned long>(millis() - t0),
                  static_cast<unsigned long>(dropped + s_spoolDropped),
                  static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)));
}

void task(void*) {
    watchdog::watch();   // post() feeds before each of its two requests, ~40 s each at worst
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
        if (s_spoolRows) pending = drivelog::BATCH;   // older rows on flash: a full part's worth waits
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
            (!s_trip[0] &&
             !drivelog::tripName(static_cast<int64_t>(drivelog::rowUnix(unixNow, now, tripMs)), s_trip, sizeof s_trip))) {
            waitFor(Wait::Clock);
            continue;
        }
        if (!heapOk()) {
            waitFor(Wait::Heap);
            nextAt = now + RETRY_MS;
            continue;
        }
        waitFor(Wait::None);
        if (!sendLinkup(&nextAt) && pending) sendPart(unixNow, now, &nextAt);
    }
}

// Loop task: rows from flash back to the ring once it drains, oldest first.
void refill() {
    if (!s_ring) return;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    const uint32_t k = drivelog::refill(s_ring->size(), s_spoolRows);
    xSemaphoreGive(s_lock);
    if (!k) return;
    drivelog::Row rows[drivelog::BATCH];
    const size_t got = s_store.read(SPOOL, s_spoolAt, reinterpret_cast<char*>(rows), k * sizeof(drivelog::Row));
    const uint32_t n = static_cast<uint32_t>(got / sizeof(drivelog::Row));
    xSemaphoreTake(s_lock, portMAX_DELAY);
    for (uint32_t i = 0; i < n; ++i) s_ring->push(rows[i]);
    s_spoolRows = n == k ? s_spoolRows - n : 0;   // a short read: the file is gone or cut, start over
    xSemaphoreGive(s_lock);
    s_spoolAt += static_cast<long>(n * sizeof(drivelog::Row));
    if (s_spoolRows == 0) {
        s_store.remove(SPOOL);
        s_spoolAt = 0;
    }
}
}  // namespace

void begin() {
    if (!ON) {
        Serial.println("[dlog] off (DLOG_URL / DLOG_KEY empty, or the bench)");
        return;
    }
    // rows of an earlier power-on can't be placed in time (their millis() are another boot's)
    if (s_store.size(SPOOL) > 0) {
        Serial.printf("[dlog] %ld bytes of an earlier power-on's rows dropped\n", s_store.size(SPOOL));
        s_store.remove(SPOOL);
    }
    s_ring = new (std::nothrow) drivelog::Ring();
    s_lock = xSemaphoreCreateMutex();
    // Core 1, priority 0: below loop() (which takes the rows), beside the server task; the OBD tasks
    // have core 0 to themselves.
    if (!s_ring || !s_lock || xTaskCreatePinnedToCore(task, "dlog", STACK, nullptr, 0, &s_task, 1) != pdPASS) {
        s_task = nullptr;
        Serial.println("[dlog] off: no memory");
        return;
    }
    Serial.printf("[dlog] on, one part per %lu rows\n", static_cast<unsigned long>(drivelog::BATCH));
}

void second() {
    if (!s_task) return;
    static obdlink::View v;   // loop task only
    obdlink::snapshot(v);
    const uint32_t now = millis();
    const bool live = v.link == obdplan::Link::Live;
    const bool linkup = obdlink::linkupRaw(nullptr, 0) != 0;
    if (live || linkup) {
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
        // A row only while the car answers, and only with speed or rpm in it (a stale link writes none).
        const bool keep = live && (row.kmh10 != drivelog::NONE || row.rpm != drivelog::NONE);
        xSemaphoreTake(s_lock, portMAX_DELAY);
        if (!s_tripOn) {
            s_tripOn = true;
            s_tripMs = now;
        }
        const drivelog::Route r = keep ? drivelog::route(s_ring->size(), s_spoolRows) : drivelog::Route::Drop;
        if (keep && r == drivelog::Route::Ring) s_ring->push(row);
        xSemaphoreGive(s_lock);
        if (keep && r == drivelog::Route::Spool) {   // the hotspot has been away a while: to flash
            const bool ok = s_store.append(SPOOL, reinterpret_cast<const char*>(&row), sizeof row);
            xSemaphoreTake(s_lock, portMAX_DELAY);
            if (ok) ++s_spoolRows;
            xSemaphoreGive(s_lock);
            if (!ok) ++s_spoolDropped;
        } else if (keep && r == drivelog::Route::Drop) {
            ++s_spoolDropped;
        }
    }
    refill();
}
}  // namespace dlogup
