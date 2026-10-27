#include "httphead.h"
#include <cstring>

namespace httphead {

bool split(const char* url, Url* out) {
    static const char SCHEME[] = "http://";
    if (!url || strncmp(url, SCHEME, sizeof(SCHEME) - 1) != 0) return false;
    const char* p = url + sizeof(SCHEME) - 1;
    size_t n = 0;
    while (p[n] && p[n] != ':' && p[n] != '/' && p[n] != '?') n++;
    if (n == 0 || n >= sizeof(out->host)) return false;
    memcpy(out->host, p, n);
    out->host[n] = 0;
    p += n;
    out->port = 80;
    if (*p == ':') {
        p++;
        long port = 0;
        int digits = 0;
        for (; *p >= '0' && *p <= '9' && digits < 6; p++, digits++) port = port * 10 + (*p - '0');
        if (digits == 0 || port < 1 || port > 65535) return false;
        out->port = static_cast<uint16_t>(port);
    }
    if (*p == 0) out->path = "/";
    else if (*p == '/') out->path = p;
    else return false;
    return true;
}

bool Head::feed(char c) {
    if (done_) return true;
    if (c == '\r') return false;
    if (c != '\n') {
        if (first_ && lineLen_ < static_cast<int>(sizeof(line_)) - 1) line_[lineLen_] = c;
        lineLen_++;
        return false;
    }
    // the end of a line: the status line's code, or the empty line that ends the head
    if (first_) {
        first_ = false;
        const char* sp = strchr(line_, ' ');
        if (sp) {
            int code = 0;
            for (const char* d = sp + 1; *d >= '0' && *d <= '9' && code < 1000; d++) code = code * 10 + (*d - '0');
            status_ = code < 1000 ? code : 0;
        }
    }
    if (lineLen_ == 0) done_ = true;
    lineLen_ = 0;
    return done_;
}

}  // namespace httphead
