#include "httpget.h"
#include <WiFi.h>
#include <cstdio>
#include "../util/yellow/httphead.h"
#include "../util/common/readguard.h"

namespace net {

static constexpr int ERR_CONNECT = -1, ERR_SEND = -2, ERR_LOST = -5, ERR_NO_HTTP = -7, ERR_TIMEOUT = -11;

int httpGet(const char* url, uint32_t timeoutMs, char* body, size_t cap, uint32_t deadlineMs) {
    if (body && cap) body[0] = 0;
    httphead::Url u;
    if (!httphead::split(url, &u)) return ERR_CONNECT;
    char req[384];
    char port[8] = "";
    if (u.port != 80) snprintf(port, sizeof(port), ":%u", static_cast<unsigned>(u.port));
    const int n = snprintf(req, sizeof(req),
                           "GET %s HTTP/1.0\r\nHost: %s%s\r\nUser-Agent: ESP32HTTPClient\r\nConnection: close\r\n\r\n",
                           u.path, u.host, port);
    if (n <= 0 || static_cast<size_t>(n) >= sizeof(req)) return ERR_SEND;

    WiFiClient c;
    if (!c.connect(u.host, u.port, static_cast<int32_t>(timeoutMs))) return ERR_CONNECT;
    c.setTimeout((timeoutMs + 500) / 1000);  // seconds: the socket's send / receive waits
    if (c.write(reinterpret_cast<const uint8_t*>(req), static_cast<size_t>(n)) != static_cast<size_t>(n)) {
        c.stop();
        return ERR_SEND;
    }

    // The head, a byte at a time (WiFiClient buffers the socket), each wait at most timeoutMs.
    httphead::Head head;
    uint32_t lastByte = millis();
    while (!head.done()) {
        if (c.available() > 0) {
            const int ch = c.read();
            if (ch < 0) continue;
            head.feed(static_cast<char>(ch));
            lastByte = millis();
        } else if (!c.connected()) {
            c.stop();
            return ERR_LOST;
        } else if (millis() - lastByte > timeoutMs) {
            c.stop();
            return ERR_TIMEOUT;
        } else {
            vTaskDelay(pdMS_TO_TICKS(2));
        }
    }
    const int rc = head.status() > 0 ? head.status() : ERR_NO_HTTP;

    // The body until the server closes, the buffer is full, it stalls or the deadline passes.
    if (rc == 200 && body && cap) {
        size_t len = 0;
        readguard::Guard g(millis(), timeoutMs, deadlineMs);
        while (len < cap - 1 && (c.connected() || c.available())) {
            const int a = c.available();
            if (a <= 0) {
                if (g.check(millis()) != readguard::State::Ok) break;
                vTaskDelay(pdMS_TO_TICKS(2));
                continue;
            }
            size_t want = cap - 1 - len;
            if (static_cast<size_t>(a) < want) want = static_cast<size_t>(a);
            len += c.readBytes(body + len, want);
            g.onData(millis());
        }
        body[len] = 0;
    }
    c.stop();
    return rc;
}

}  // namespace net
