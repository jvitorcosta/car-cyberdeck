#include "hue.h"

namespace hue {
void hueSat(int r, int g, int b, int turn, int full, int* hue, int* sat) {
    int mx = r, mn = r;
    if (g > mx) mx = g;
    if (b > mx) mx = b;
    if (g < mn) mn = g;
    if (b < mn) mn = b;
    const int d = mx - mn;
    *sat = mx ? d * full / mx : 0;
    if (!d) {
        *hue = 0;
        return;
    }
    const int sixth = turn / 6;
    int h;
    if (mx == r) h = sixth * (g - b) / d;
    else if (mx == g) h = 2 * sixth + sixth * (b - r) / d;
    else h = 4 * sixth + sixth * (r - g) / d;
    *hue = (h + turn) % turn;
}

int dist(int a, int b, int turn) {
    const int d = ((a - b) % turn + turn) % turn;
    return d > turn / 2 ? turn - d : d;
}
}  // namespace hue
