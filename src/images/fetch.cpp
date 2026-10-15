#include "fetch.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "../net/http_config.h"
#include "../util/readguard.h"

namespace fetch {

bool httpsGetInto(const char* url, uint8_t* buf, size_t cap, size_t* outLen, int* httpCode,
                  bool allowPartial) {
    *outLen = 0;
    if (httpCode) *httpCode = 0;
    if (!url || !url[0] || !buf || cap < 2) return false;
    if (!netcfg::linkUp()) return false;
    WiFiClientSecure client;
    netcfg::secure(client);
    HTTPClient https;
    netcfg::streamed(https);   // plain (non-chunked) body, so getSize() is the length
    if (!https.begin(client, url)) return false;
    int code = https.GET();
    if (httpCode) *httpCode = code;
    if (code != 200) {
        Serial.printf("[fetch] http %d\n", code);
        https.end();
        return false;
    }
    int len = https.getSize();
    if (len > 0 && (size_t)len + 1 > cap && allowPartial) {
        len = (int)cap - 1;              // caller only needs the beginning (e.g. AnimData.xml)
    }
    if (len <= 0 || (size_t)len + 1 > cap) {
        Serial.printf("[fetch] bad length %d (cap %u)\n", len, (unsigned)cap);
        if (httpCode && len > 0) *httpCode = 413;   // too big for the buffer: retrying won't help
        https.end();
        return false;
    }
    WiFiClient* s = https.getStreamPtr();
    int got = 0;
    readguard::Guard guard(millis(), netcfg::STALL_MS, netcfg::IMAGE_DEADLINE_MS);
    while (got < len && (https.connected() || s->available())) {
        size_t a = s->available();
        if (a) {
            size_t want = (size_t)(len - got);
            if (a < want) want = a;
            got += s->readBytes(buf + got, want);
            guard.onData(millis());
            if (guard.check(millis()) == readguard::State::Deadline) break;
        } else {
            if (guard.check(millis()) != readguard::State::Ok) break;   // stalled or too slow
            delay(1);
        }
    }
    https.end();
    if (got != len) return false;
    buf[len] = 0;
    *outLen = (size_t)len;
    return true;
}

}
