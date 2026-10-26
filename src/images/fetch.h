#pragma once
#include <stddef.h>
#include <stdint.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
namespace fetch {
// HTTPS GET of `url` into the caller's buffer (no per-download malloc while TLS is open).
// Fails on non-200, unknown length, len + 1 > cap, or a stalled/short read (8 s handshake
// and read timeouts). On success buf holds *outLen bytes plus a trailing NUL.
// allowPartial: a body larger than the buffer is read up to cap - 1 bytes instead of failing.
// *httpCode is the server's status, or 413 when a 200 body is too big for the buffer.
bool httpsGetInto(const char* url, uint8_t* buf, size_t cap, size_t* outLen,
                  int* httpCode = nullptr, bool allowPartial = false);

// Several GETs to one host on one TLS connection (HTTP/1.1 keep-alive): one handshake instead
// of one per file (the walker's AnimData.xml + Walk-Anim.png). getInto() works like
// httpsGetInto(). The connection closes when the session ends, after an answer whose body
// wasn't read to its end (it would poison the next one), or when the server says close. A
// kept connection the server dropped is reconnected once. A body without a length (chunked)
// is asked again over HTTP/1.0, as httpsGetInto() does, for the rest of the session.
// Scope it to the downloads: the TLS buffers stay allocated while it lives.
class Session {
public:
    Session();
    ~Session();
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;
    bool getInto(const char* url, uint8_t* buf, size_t cap, size_t* outLen,
                 int* httpCode = nullptr, bool allowPartial = false);
private:
    WiFiClientSecure client_;   // before http_: http_'s destructor still uses it
    HTTPClient http_;
    bool http10_ = false;
};
}
