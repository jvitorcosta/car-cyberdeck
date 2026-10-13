#include "statejson.h"
#include <cstdio>

namespace statejson {

namespace {
// Appends to a fixed buffer; remembers when anything didn't fit.
struct Out {
    char* p;
    size_t n, len = 0;
    bool full = false;
    void put(char c) {
        if (len + 1 < n) p[len++] = c;
        else full = true;
    }
    void raw(const char* s) { while (*s) put(*s++); }
    void str(const char* s) {
        put('"');
        for (; s && *s; ++s) {
            unsigned char c = (unsigned char)*s;
            if (c == '"' || c == '\\') { put('\\'); put((char)c); }
            else if (c == '\n') raw("\\n");
            else if (c < 0x20) {
                char u[8];
                snprintf(u, sizeof u, "\\u%04x", c);
                raw(u);
            } else put((char)c);
        }
        put('"');
    }
    void key(const char* k, bool first = false) {
        if (!first) put(',');
        str(k);
        put(':');
    }
    void num(long v) {
        char b[16];
        snprintf(b, sizeof b, "%ld", v);
        raw(b);
    }
};
}

size_t write(const State& s, char* out, size_t n) {
    if (n == 0) return 0;
    Out o{out, n};
    o.put('{');
    o.key("track", true); o.str(s.track);
    o.key("artist");      o.str(s.artist);
    o.key("status");      o.str(s.title);
    o.key("detail");      o.str(s.detail);
    o.key("wifi");        o.str(s.ssid);
    o.key("rssi");        o.num(s.rssi);
    o.key("uptime");      o.num((long)s.uptimeS);
    o.key("clips");
    o.put('[');
    for (int i = 0; s.clips && i < s.clipCount; ++i) {
        if (i) o.put(',');
        o.str(s.clips[i]);
    }
    o.put(']');
    o.put('}');
    if (o.full) { out[0] = '\0'; return 0; }
    out[o.len] = '\0';
    return o.len;
}

}
