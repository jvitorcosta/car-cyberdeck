#include "qrfit.h"
#include <cstdio>
#include <cstring>

namespace qrfit {

int version(size_t bytes) {
    static const size_t CAPACITY[MAX_VERSION + 1] = {0, 17, 32, 53};   // byte mode, ECC L
    if (bytes == 0) return 0;
    for (int v = 1; v <= MAX_VERSION; ++v)
        if (bytes <= CAPACITY[v]) return v;
    return 0;
}

bool link(const char* ip, const char* token, char* out, size_t n) {
    if (!out || n == 0) return false;
    out[0] = '\0';
    if (!ip || !ip[0] || strcmp(ip, "0.0.0.0") == 0 || !token || !token[0]) return false;
    const int len = snprintf(out, n, "http://%s/?k=%s", ip, token);
    if (len < 0 || (size_t)len >= n || !version((size_t)len)) { out[0] = '\0'; return false; }
    return true;
}

}
