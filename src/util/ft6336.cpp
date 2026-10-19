#include "ft6336.h"

namespace ft6336 {

Raw parse(const uint8_t* regs) {
    const int touches = regs[0] & 0x0F;
    Raw r{touches == 1 || touches == 2, 0, 0};
    r.x = (regs[1] & 0x0F) << 8 | regs[2];
    r.y = (regs[3] & 0x0F) << 8 | regs[4];
    return r;
}

static int clamp(int v, int hi) { return v < 0 ? 0 : v > hi ? hi : v; }

void toScreen(const Raw& r, const Orient& o, int* x, int* y) {
    int sx = o.swap ? r.y : r.x, sy = o.swap ? r.x : r.y;
    sx = clamp(sx, 319);
    sy = clamp(sy, 239);
    *x = o.flipX ? 319 - sx : sx;
    *y = o.flipY ? 239 - sy : sy;
}

}
