#include "qrcard.h"

namespace qrcard {

int offset(int size) {
    if (size <= 0 || size > qrfit::modules(qrfit::MAX_VERSION)) return -1;
    return (PANEL - size * qrfit::MODULE_PX) / 2;
}

bool dark(const Qr& qr, int mx, int my) {
    if (!qr.bits || mx < 0 || my < 0 || mx >= qr.size || my >= qr.size) return false;
    const int i = my * qr.size + mx;
    return (qr.bits[i >> 3] >> (7 - (i & 7))) & 1;
}

Rect beside(const Rect& r) {
    const Rect& f = FOOTPRINT;
    const int right = f.x + f.w;
    const bool rows = r.y < f.y + f.h && f.y < r.y + r.h;
    if (!rows || r.x >= right || r.x + r.w <= f.x) return r;
    if (r.x + r.w <= right) return {right, r.y, 0, r.h};   // all of it under the card
    return {right, r.y, r.x + r.w - right, r.h};
}

}
