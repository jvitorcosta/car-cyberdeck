#include "carstatus.h"
#include <cstdio>
#include <cstring>

namespace carstatus {
namespace {
struct Out {
    char* p;
    size_t n, len = 0;
    bool full = false;
    void put(char c) {
        if (len + 1 < n) p[len++] = c;
        else full = true;
    }
    void raw(const char* s) { while (*s) put(*s++); }
    // A JSON string of at most max bytes of s (whole UTF-8 characters only).
    void str(const char* s, size_t max) {
        put('"');
        size_t used = 0;
        for (; s && *s;) {
            const unsigned char c = (unsigned char)*s;
            const size_t cl = c < 0x80 ? 1 : (c >> 5) == 6 ? 2 : (c >> 4) == 14 ? 3 : (c >> 3) == 30 ? 4 : 1;
            size_t k = 1;
            while (k < cl && s[k]) ++k;
            if (used + k > max) break;
            if (c == '"' || c == '\\') { put('\\'); put((char)c); }
            else if (c == '\n') raw("\\n");
            else if (c < 0x20) {
                char u[8];
                snprintf(u, sizeof u, "\\u%04x", c);
                raw(u);
            } else {
                for (size_t i = 0; i < k; ++i) put(s[i]);
            }
            used += k;
            s += k;
        }
        put('"');
    }
};
}

bool write(const Status& s, char* out, size_t n) {
    if (!out || n == 0) return false;
    Out o{out, n};
    o.raw("{\"car\":");
    o.str(s.car, 16);
    char k[24];
    snprintf(k, sizeof k, ",\"kmh\":%d", s.kmh < 0 ? -1 : s.kmh);
    o.raw(k);
    o.raw(",\"pin\":");
    o.str(s.pin, 16);
    o.raw(",\"show\":");
    o.str(s.show, 16);
    o.raw(",\"line\":");
    o.str(s.line, MAX_LINE);
    o.put('}');
    out[o.len] = '\0';
    return !o.full;
}
}
