#include "carlook.h"
#include "car_sprite.h"
#include "../../util/black/carbin.h"
#include "../../util/common/rgb565.h"
#ifdef BOARD_S3
#include "../core/media.h"
MEDIA_DECL(data_mcqueen_bin);
#endif

namespace carlook {

using rgb565::hex;

const Rims SILVER = {{0, hex(0x141416), hex(0xC9CED4), hex(0x3A3E46), hex(0xC8CCD2), hex(0x2B2F36), hex(0x2B2F36)},
                     {hex(0x2B2F36), hex(0xDFE3E8), hex(0x6A7078)}};
// Red lips and spokes on a dark hub, so the spin still reads; the black tyre is the Honda's.
const Rims RED = {{0, hex(0x141416), hex(0xD8261C), hex(0x5C0F0B), hex(0xC4221A), hex(0x2B2F36), hex(0x2B2F36)},
                  {hex(0x2B2F36), hex(0xE53A2E), hex(0x8A1A14)}};

// The Honda's beam starts at its nose (x 200, y 31) and its taillight glow at y 21, as the scene
// always drew them.
Look honda() {
    return {car_sprite::width(), car_sprite::height(), car_sprite::REAR_WHEEL_X,
            car_sprite::FRONT_WHEEL_X, car_sprite::WHEEL_Y, 200, 31, 21, car_sprite::pixel, &SILVER};
}

#ifdef BOARD_S3
static carbin::Car s_mc;
static bool mcPixel(int x, int y, uint16_t* c, uint8_t* k) { return carbin::pixel(s_mc, x, y, c, k); }
#endif

bool mcqueen(Look* out) {
#ifdef BOARD_S3
    const media::Blob b = MEDIA(data_mcqueen_bin, "mcqueen.bin");
    if (!carbin::parse(b.data, b.size, &s_mc)) return false;
    *out = {s_mc.w, s_mc.h, s_mc.rearX, s_mc.frontX, s_mc.wheelY, s_mc.headX, s_mc.headY, s_mc.tailY, mcPixel, &RED};
    return true;
#else
    (void)out;
    return false;
#endif
}

}
