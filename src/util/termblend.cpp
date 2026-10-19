#include "termblend.h"
#include "rgb565.h"
#include "bootanim.h"

namespace termblend {

void composite(uint16_t* block, int bx, int by, int bw, int bh,
               const uint16_t* overlay, int ox, int oy, int ow, int oh) {
    const int x0 = bx > ox ? bx : ox, x1 = (bx + bw) < (ox + ow) ? bx + bw : ox + ow;
    const int y0 = by > oy ? by : oy, y1 = (by + bh) < (oy + oh) ? by + bh : oy + oh;
    for (int y = y0; y < y1; ++y)
        for (int x = x0; x < x1; ++x) {
            uint16_t& p = block[(y - by) * bw + (x - bx)];
            const uint16_t g = overlay[(y - oy) * ow + (x - ox)];
            p = g != KEY ? g : rgb565::be(bootanim::scale565(rgb565::be(p), SHADE));
        }
}

}
