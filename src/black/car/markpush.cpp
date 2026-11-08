#include "markpush.h"
#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <time.h>
#include "camsvc.h"
#include "../core/mem.h"
#include "../../common/net/http_config.h"
#include "../../common/net/wifi.h"
#include "../../common/core/watchdog.h"
#include "../../util/black/netplan.h"
#include "../../util/car/markissue.h"
#include "../../config.h"

#ifndef MARKS_REPO
#define MARKS_REPO ""
#endif
#ifndef MARKS_TOKEN
#define MARKS_TOKEN ""
#endif

namespace markpush {

static constexpr bool ON = sizeof(MARKS_REPO) > 1 && sizeof(MARKS_TOKEN) > 1;   // both set
static constexpr uint32_t RETRY_MS = 30000;          // no answer (a dead zone): soon again
static constexpr uint32_t REFUSED_MS = 10 * 60000;   // GitHub said no (key, repo): rarely
static uint32_t s_nextAt = 0;
static bool s_told = false;   // "off" logged once

// The issue for a mark (util/car/markissue), with the dashboard's time and UTC.
static int issueJson(const camsvc::MarkRow& m, char* out, size_t n) {
    char local[24] = "", utc[24] = "";
    if (m.epoch) {
        const time_t t = (time_t)m.epoch;
        struct tm tm;
        localtime_r(&t, &tm);   // TIMEZONE: the time on the dashboard
        strftime(local, sizeof local, "%Y-%m-%d %H:%M:%S", &tm);
        gmtime_r(&t, &tm);
        strftime(utc, sizeof utc, "%Y-%m-%dT%H:%M:%SZ", &tm);
    }
    return markissue::json(out, n, local, utc, m.latE6, m.lonE6, m.course, m.kmh);
}

void tick(uint32_t now) {
    if (!ON) {
        if (!s_told) { s_told = true; Serial.println("[marks] upload off (MARKS_REPO / MARKS_TOKEN)"); }
        return;
    }
    if ((int32_t)(now - s_nextAt) < 0 || !net::isOnline()) return;
    camsvc::MarkRow m;
    if (!camsvc::nextUnsent(&m)) {
        s_nextAt = now + 1000;   // the marks are read from NVS: not every loop
        return;
    }
    if (mem::byteLargest() < netplan::TLS_NEED) { s_nextAt = now + RETRY_MS; return; }
    char body[384];
    const int len = issueJson(m, body, sizeof body);
    if (len <= 0 || len >= (int)sizeof body) { camsvc::markSent(); return; }   // can't happen: skip it, don't loop
    WiFiClientSecure client;
    netcfg::github(client);
    HTTPClient https;
    https.setTimeout(netcfg::HTTP_TIMEOUT_MS);
    char url[128];
    snprintf(url, sizeof url, "https://api.github.com/repos/%s/issues", MARKS_REPO);
    if (!https.begin(client, url)) { s_nextAt = now + RETRY_MS; return; }
    https.addHeader("Authorization", "Bearer " MARKS_TOKEN);
    https.addHeader("Accept", "application/vnd.github+json");
    https.addHeader("Content-Type", "application/json");
    https.addHeader("User-Agent", netcfg::USER_AGENT);
    watchdog::feed();
    const int rc = https.POST((uint8_t*)body, (size_t)len);
    https.end();
    if (rc == 201) {
        camsvc::markSent();
        s_nextAt = millis();   // the next one, if any, on the next tick
        Serial.println("[marks] mark sent");
        return;
    }
    if (rc < 0) netcfg::certRejected(client, "api.github.com");
    const bool refused = rc == 401 || rc == 403 || rc == 404 || rc == 410 || rc == 422;
    s_nextAt = millis() + (refused ? REFUSED_MS : RETRY_MS);
    Serial.printf("[marks] not sent (HTTP %d), again in %lus\n", rc, (unsigned long)((refused ? REFUSED_MS : RETRY_MS) / 1000));
}

}  // namespace markpush
