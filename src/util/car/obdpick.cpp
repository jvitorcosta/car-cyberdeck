#include "obdpick.h"
#include <strings.h>
#include "obd.h"
// See obdpick.h. PURE, host-tested.
namespace obdpick {

void Recent::failed(uint64_t addr, uint32_t nowMs) {
    for (int i = 0; i < RING; ++i) {
        if (used_[i] && addr_[i] == addr) {
            atMs_[i] = nowMs;
            return;
        }
    }
    addr_[next_] = addr;
    atMs_[next_] = nowMs;
    used_[next_] = true;
    next_ = (next_ + 1) % RING;   // the oldest goes first
}

bool Recent::skip(uint64_t addr, uint32_t nowMs) const {
    for (int i = 0; i < RING; ++i)
        if (used_[i] && addr_[i] == addr && nowMs - atMs_[i] < SKIP_MS) return true;   // unsigned: wrap-safe
    return false;
}

bool isLight(const char* name) {
    return name && (strncasecmp(name, "ELK", 3) == 0 || strncasecmp(name, "MELK", 4) == 0);
}

int pick(const Candidate* c, int n, const Recent& recent, uint32_t nowMs) {
    int byName = -1, nameless = -1;
    for (int i = 0; c && i < n; ++i) {
        const Candidate& d = c[i];
        if (d.light || isLight(d.name) || recent.skip(d.addr, nowMs)) continue;
        const bool named = d.name && d.name[0];
        if (named && obd::looksLikeAdapter(d.name)) {
            if (byName < 0 || d.rssi > c[byName].rssi) byName = i;
        } else if (!named && d.serial) {
            if (nameless < 0 || d.rssi > c[nameless].rssi) nameless = i;
        }
    }
    return byName >= 0 ? byName : nameless;
}
}  // namespace obdpick
