#include "jsonpath.h"
#include <cstring>
#include "jsonflat.h"

namespace jsonpath {

static const char* skipSpace(const char* p) {
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') ++p;
    return p;
}

// Past the string whose opening quote is at p, or nullptr when it is cut off.
static const char* skipString(const char* p) {
    for (++p; *p; ++p) {
        if (*p == '\\') { if (!*++p) return nullptr; continue; }
        if (*p == '"') return p + 1;
    }
    return nullptr;
}

// Past the value at p (any kind, nested ones whole), or nullptr when it is cut off.
static const char* skipValue(const char* p) {
    if (*p == '"') return skipString(p);
    if (*p == '{' || *p == '[') {
        int depth = 0;
        while (*p) {
            if (*p == '"') {
                p = skipString(p);
                if (!p) return nullptr;
                continue;
            }
            if (*p == '{' || *p == '[') ++depth;
            else if ((*p == '}' || *p == ']') && --depth == 0) return p + 1;
            ++p;
        }
        return nullptr;
    }
    while (*p && *p != ',' && *p != '}' && *p != ']') ++p;   // number, true, false, null
    return *p ? p : nullptr;
}

// The value of `key` (len bytes, no escapes) in the object at p ('{'), or nullptr.
static const char* member(const char* p, const char* key, size_t len) {
    for (p = skipSpace(p + 1); *p == '"';) {
        const char* end = skipString(p);
        if (!end) return nullptr;
        const bool hit = (size_t)(end - p - 2) == len && strncmp(p + 1, key, len) == 0;
        p = skipSpace(end);
        if (*p != ':') return nullptr;
        p = skipSpace(p + 1);
        if (hit) return p;
        p = skipValue(p);
        if (!p) return nullptr;
        p = skipSpace(p);
        if (*p != ',') return nullptr;
        p = skipSpace(p + 1);
    }
    return nullptr;
}

// Element `index` of the array at p ('['), or nullptr.
static const char* element(const char* p, long index) {
    p = skipSpace(p + 1);
    if (*p == ']') return nullptr;
    for (long i = 0; *p; ++i) {
        if (i == index) return p;
        p = skipValue(p);
        if (!p) return nullptr;
        p = skipSpace(p);
        if (*p != ',') return nullptr;
        p = skipSpace(p + 1);
    }
    return nullptr;
}

const char* find(const char* body, const char* path) {
    if (!body || !path) return nullptr;
    const char* p = skipSpace(body);
    while (*path) {
        const char* dot = strchr(path, '.');
        const size_t len = dot ? (size_t)(dot - path) : strlen(path);
        if (len == 0) return nullptr;
        if (*p == '{') {
            p = member(p, path, len);
        } else if (*p == '[') {
            long index = 0;
            for (size_t i = 0; i < len; ++i) {
                if (path[i] < '0' || path[i] > '9' || index > 100000) return nullptr;
                index = index * 10 + (path[i] - '0');
            }
            p = element(p, index);
        } else {
            return nullptr;
        }
        if (!p) return nullptr;
        path += dot ? len + 1 : len;
    }
    return p;
}

bool str(const char* body, const char* path, char* out, size_t n) {
    if (out && n) out[0] = '\0';
    return jsonflat::strAt(find(body, path), out, n);
}

bool num(const char* body, const char* path, long* out) { return jsonflat::numAt(find(body, path), out); }

bool boolean(const char* body, const char* path, bool* out) {
    return jsonflat::boolAt(find(body, path), out);
}

}
