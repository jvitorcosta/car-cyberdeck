#include "avatarbin.h"
#include <cstring>

namespace avatarbin {
static bool fail(Info& out, const char** why, const char* reason) {
    out.moods = 0;
    out.name[0] = '\0';
    if (why) *why = reason;
    return false;
}

bool parse(const uint8_t* b, size_t len, Info& out, const char** why) {
    if (!b || len < 18) return fail(out, why, "no file");
    if (memcmp(b, "AVT1", 4) != 0) return fail(out, why, "bad magic");
    const int n = b[4];
    if (n == 0) return fail(out, why, "no moods");
    if (n > MAX_MOODS) return fail(out, why, "too many moods");
    const size_t head = 18 + 2 * (size_t)n;
    if (len < head) return fail(out, why, "short header");
    size_t off = head;
    for (int e = 0; e < n; ++e) {
        const uint8_t k = b[18 + 2 * e + 1];
        if (k == 0 || k > MAX_FRAMES) return fail(out, why, "bad frame count");
        out.id[e] = b[18 + 2 * e];
        out.frames[e] = k;
        out.first[e] = (uint32_t)off;
        off += k * FRAME_BYTES;
    }
    if (len != off) return fail(out, why, "size mismatch");
    memcpy(out.name, b + 6, NAME_LEN);
    out.name[NAME_LEN] = '\0';
    out.moods = n;
    if (why) *why = "";
    return true;
}

int find(const Info& in, uint8_t id) {
    for (int e = 0; e < in.moods; ++e)
        if (in.id[e] == id) return e;
    return 0;
}

uint32_t frameOffset(const Info& in, int e, int f) {
    if (e < 0 || e >= in.moods) e = 0;
    if (f >= in.frames[e]) f = in.frames[e] - 1;
    if (f < 0) f = 0;
    return in.first[e] + (uint32_t)f * (uint32_t)FRAME_BYTES;
}
}
