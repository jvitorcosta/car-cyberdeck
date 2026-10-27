#pragma once
#include <cstddef>
#include <cstdint>
// A plain-http GET on a raw WiFiClient, for the yellow deck's three askers (the weather, the
// black deck's /state and /music): HTTP/1.0, so the answer is never chunked and the server closes
// after it. HTTPClient did the same with ~12 KB of code, its Strings and an IRAM strptime. Blocks
// the calling task; call it from a network task, not loop().
namespace net {

// GET url. timeoutMs bounds the connect and every wait for a byte of the head, and the body's
// stalls; deadlineMs the whole body. With a body buffer, a 200's body goes into it (cut at cap - 1,
// always terminated; "" for anything else). Returns the HTTP status, or HTTPClient's negative codes:
// -1 no connection (or a URL it can't ask), -2 the request didn't go out, -5 the connection closed
// before the head ended, -7 no status in the answer, -11 the head stopped arriving.
int httpGet(const char* url, uint32_t timeoutMs, char* body = nullptr, size_t cap = 0, uint32_t deadlineMs = 0);

}
