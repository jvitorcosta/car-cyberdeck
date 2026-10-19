#include "termblend.h"
#include "rgb565.h"

namespace termblend {

void composite(uint16_t* block, int bx, int by, int bw, int bh,
               const uint16_t* overlay, int ox, int oy, int ow, int oh) {
    const int x0 = bx > ox ? bx : ox, x1 = (bx + bw) < (ox + ow) ? bx + bw : ox + ow;
    const int y0 = by > oy ? by : oy, y1 = (by + bh) < (oy + oh) ? by + bh : oy + oh;
    for (int y = y0; y < y1; ++y) {
        uint16_t* p = block + (y - by) * bw + (x0 - bx);
        const uint16_t* g = overlay + (y - oy) * ow + (x0 - ox);
        for (int x = x0; x < x1; ++x, ++p, ++g) *p = *g != KEY ? *g : rgb565::be(shade(rgb565::be(*p)));
    }
}

}
