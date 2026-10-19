#include "carlook.h"
#include "car_sprite.h"
#include "../util/carbin.h"
#ifdef BOARD_S3
#include "../core/embed.h"
EMBED(data_mcqueen_bin);
#endif

namespace carlook {

// The Honda's beam starts at its nose (x 200, y 31) and its taillight glow at y 21, as the scene
// always drew them.
Look honda() {
    return {car_sprite::width(), car_sprite::height(), car_sprite::REAR_WHEEL_X,
            car_sprite::FRONT_WHEEL_X, car_sprite::WHEEL_Y, 200, 31, 21, car_sprite::pixel};
}

#ifdef BOARD_S3
static carbin::Car s_mc;
static bool mcPixel(int x, int y, uint16_t* c, uint8_t* k) { return carbin::pixel(s_mc, x, y, c, k); }
#endif

bool mcqueen(Look* out) {
#ifdef BOARD_S3
    if (!carbin::parse(EMBED_DATA(data_mcqueen_bin), EMBED_SIZE(data_mcqueen_bin), &s_mc)) return false;
    *out = {s_mc.w, s_mc.h, s_mc.rearX, s_mc.frontX, s_mc.wheelY, s_mc.headX, s_mc.headY, s_mc.tailY, mcPixel};
    return true;
#else
    (void)out;
    return false;
#endif
}

}
