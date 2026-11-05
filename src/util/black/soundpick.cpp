#include "soundpick.h"
#include <cstring>

namespace soundpick {

int pick(uint32_t rnd, int last, int count, const bool* ok) {
    if (count > MAX) count = MAX;
    int list[MAX];
    int n = 0;
    for (int i = 0; i < count; ++i)
        if (!ok || ok[i]) list[n++] = i;
    if (n == 0) return -1;
    if (n > 1) {                       // drop last boot's, when there's another
        int k = 0;
        for (int i = 0; i < n; ++i)
            if (list[i] != last) list[k++] = list[i];
        n = k;
    }
    return list[rnd % (uint32_t)n];
}

int forced(const char* spec, const char* slot, int count) {
    if (!spec || !slot) return -1;
    const size_t len = strlen(slot);
    for (const char* p = spec; (p = strstr(p, slot)) != nullptr; p += len) {
        if ((p != spec && p[-1] != ',') || p[len] != ':') continue;
        const char* d = p + len + 1;
        if (*d < '0' || *d > '9') return -1;
        int v = 0;
        for (; *d >= '0' && *d <= '9' && v < 1000; ++d) v = v * 10 + (*d - '0');
        return v < count ? v : -1;
    }
    return -1;
}

}
