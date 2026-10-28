#pragma once
#include <cstdint>
// The text side of the yellow deck's plain-http GETs (net/httpget): splitting a URL into where to
// connect and what to ask, and reading the response's head a byte at a time. The servers it talks
// to (the black deck's WebServer, ip-api.com, Open-Meteo) close the connection after an HTTP/1.0
// answer and never chunk it, so the body is simply what follows the head. PURE, host-tested.
namespace httphead {

// "http://host[:port][/path]": host (false if it doesn't fit), port (80 without one) and the path
// from its '/', pointing into url ("/" when there is none). False for anything else (https, an empty
// host, a bad port, a query with no '/').
struct Url { char host[64]; uint16_t port; const char* path; };
bool split(const char* url, Url* out);

// A response's head: the status line, then header lines up to the empty line that ends them
// ("\r\n" or a bare "\n"). Headers are skipped: nothing the deck asks needs them.
class Head {
public:
    // Takes the response's next byte; true once the head is over (the next byte is the body's).
    bool feed(char c);
    bool done() const { return done_; }
    // The status line's code (200, 503 ...): the number after its first space, as HTTPClient read
    // it; 0 when there is none (not an HTTP server).
    int status() const { return status_; }
private:
    bool done_ = false, first_ = true;
    int status_ = 0;
    int lineLen_ = 0;       // characters on the current line, '\r' not counted
    char line_[32] = {};    // the status line's start
};

}  // namespace httphead
