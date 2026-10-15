#include "jsonflat.h"
#include <cstdlib>
#include <cstring>

namespace jsonflat {

// Skips a JSON string starting at p (p points at the opening quote); returns the char after the
// closing quote, or nullptr if unterminated.
static const char* skipString(const char* p) {
    for (++p; *p; ++p) {
        if (*p == '\\') { if (!*++p) return nullptr; continue; }
        if (*p == '"') return p + 1;
    }
    return nullptr;
}

// True when the string token at p (opening quote) equals key exactly (no escapes in keys).
static bool keyIs(const char* p, const char* key) {
    size_t k = strlen(key);
    return strncmp(p + 1, key, k) == 0 && p[1 + k] == '"';
}

static const char* skipSpace(const char* p) {
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') ++p;
    return p;
}

// Start of the value for `key`, or nullptr.
static const char* find(const char* body, const char* key) {
    if (!body || !key) return nullptr;
    for (const char* p = body; *p;) {
        if (*p == '"') {
            const char* end = skipString(p);
            if (!end) return nullptr;
            const char* after = skipSpace(end);
            if (*after == ':' && keyIs(p, key)) return skipSpace(after + 1);
            p = end;
        } else {
            ++p;
        }
    }
    return nullptr;
}

static int hexval(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

// Appends UTF-8 for cp if all of it fits (keeps whole characters).
static void putUtf8(char* out, size_t n, size_t* len, unsigned cp) {
    char b[3];
    size_t k;
    if (cp < 0x80) { b[0] = (char)cp; k = 1; }
    else if (cp < 0x800) { b[0] = (char)(0xC0 | (cp >> 6)); b[1] = (char)(0x80 | (cp & 0x3F)); k = 2; }
    else { b[0] = (char)(0xE0 | (cp >> 12)); b[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
           b[2] = (char)(0x80 | (cp & 0x3F)); k = 3; }
    if (*len + k >= n) return;
    memcpy(out + *len, b, k);
    *len += k;
}

bool str(const char* body, const char* key, char* out, size_t n) {
    if (!out || n == 0) return false;
    out[0] = '\0';
    const char* p = find(body, key);
    if (!p || *p != '"') return false;
    size_t len = 0;
    bool full = false;
    for (++p; *p && *p != '"'; ++p) {
        unsigned cp;
        if (*p == '\\') {
            ++p;
            switch (*p) {
            case 'n': cp = '\n'; break;
            case 't': cp = '\t'; break;
            case 'r': cp = '\r'; break;
            case 'b': cp = '\b'; break;
            case 'f': cp = '\f'; break;
            case 'u': {
                int v = 0;
                for (int i = 1; i <= 4; ++i) {
                    int h = hexval(p[i]);
                    if (h < 0) return false;
                    v = v * 16 + h;
                }
                p += 4;
                cp = (unsigned)v;
                break;
            }
            case '\0': return false;
            default: cp = (unsigned char)*p; break;   // \" \\ \/
            }
            if (!full) putUtf8(out, n, &len, cp);
        } else {
            // raw UTF-8 byte: keep whole characters only
            if (!full) {
                if (len + 1 >= n) full = true;
                else out[len++] = *p;
            }
        }
        if (len + 1 >= n) full = true;
    }
    if (*p != '"') { out[0] = '\0'; return false; }   // unterminated: truncated body
    while (len > 0 && ((unsigned char)out[len - 1] & 0xC0) == 0x80) {
        // drop a cut multi-byte tail
        size_t s = len - 1;
        while (s > 0 && ((unsigned char)out[s] & 0xC0) == 0x80) --s;
        unsigned char lead = (unsigned char)out[s];
        size_t need = lead >= 0xF0 ? 4 : lead >= 0xE0 ? 3 : lead >= 0xC0 ? 2 : 1;
        if (len - s == need) break;
        len = s;
    }
    out[len] = '\0';
    return true;
}

bool num(const char* body, const char* key, long* out) {
    const char* p = find(body, key);
    if (!p || !((*p >= '0' && *p <= '9') || *p == '-')) return false;
    char* end = nullptr;
    long v = strtol(p, &end, 10);
    if (end == p || *end == '.' || *end == 'e' || *end == 'E') return false;
    *out = v;
    return true;
}

bool real(const char* body, const char* key, double* out) {
    const char* p = find(body, key);
    if (!p || !((*p >= '0' && *p <= '9') || *p == '-')) return false;
    char* end = nullptr;
    double v = strtod(p, &end);
    if (end == p) return false;
    *out = v;
    return true;
}

bool boolean(const char* body, const char* key, bool* out) {
    const char* p = find(body, key);
    if (!p) return false;
    if (strncmp(p, "true", 4) == 0) { *out = true; return true; }
    if (strncmp(p, "false", 5) == 0) { *out = false; return true; }
    return false;
}

bool arrayHas(const char* body, const char* key, const char* item) {
    const char* p = find(body, key);
    if (!p || *p != '[' || !item) return false;
    for (p = skipSpace(p + 1); *p && *p != ']';) {
        if (*p == '"') {
            const char* end = skipString(p);
            if (!end) return false;
            if (keyIs(p, item)) return true;
            p = skipSpace(end);
        } else {
            ++p;
        }
    }
    return false;
}

}
