#include "fetch.h"
#include "../net/http_config.h"
#include "../util/readguard.h"

namespace fetch {

// After a 200: reads the body (its Content-Length) into buf. *whole: read to its very end, so a
// kept-alive connection can carry the next request.
static bool readBody(HTTPClient& https, uint8_t* buf, size_t cap, size_t* outLen, int* httpCode,
                     bool allowPartial, bool* whole) {
    *whole = false;
    const int size = https.getSize();
    int len = size;
    if (len > 0 && (size_t)len + 1 > cap && allowPartial) {
        len = (int)cap - 1;              // caller only needs the beginning (e.g. AnimData.xml)
    }
    if (len <= 0 || (size_t)len + 1 > cap) {
        Serial.printf("[fetch] bad length %d (cap %u)\n", len, (unsigned)cap);
        if (httpCode && len > 0) *httpCode = 413;   // too big for the buffer: retrying won't help
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
    if (got != len) return false;
    buf[len] = 0;
    *outLen = (size_t)len;
    *whole = len == size;
    return true;
}

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
    bool whole;
    const bool ok = readBody(https, buf, cap, outLen, httpCode, allowPartial, &whole);
    https.end();
    return ok;
}

Session::Session() {
    netcfg::secure(client_);
    http_.setReuse(true);
    http_.setTimeout(netcfg::HTTP_TIMEOUT_MS);
}

Session::~Session() {
    client_.stop();
}

bool Session::getInto(const char* url, uint8_t* buf, size_t cap, size_t* outLen, int* httpCode,
                      bool allowPartial) {
    *outLen = 0;
    if (httpCode) *httpCode = 0;
    if (!url || !url[0] || !buf || cap < 2) return false;
    if (!netcfg::linkUp()) return false;
    bool retried = false;
    for (;;) {
        const bool reused = client_.connected();
        if (!http_.begin(client_, url)) return false;
        const int code = http_.GET();
        if (code < 0 && reused && !retried) {   // the kept connection had gone stale: once more
            retried = true;
            http_.end();
            client_.stop();
            continue;
        }
        if (httpCode) *httpCode = code;
        if (code != 200) {
            Serial.printf("[fetch] http %d\n", code);
            http_.end();
            client_.stop();                      // an unread error body would poison the next answer
            return false;
        }
        if (!http10_ && http_.getSize() <= 0) {   // chunked: ask again the way httpsGetInto does
            http10_ = true;
            http_.end();
            client_.stop();
            netcfg::streamed(http_);             // HTTP/1.0, no reuse from here on
            continue;
        }
        bool whole;
        const bool ok = readBody(http_, buf, cap, outLen, httpCode, allowPartial, &whole);
        http_.end();                             // keeps the socket open when the server allows it
        if (!ok || !whole) client_.stop();
        return ok;
    }
}

}
