#include "sunset.h"
#include "../util/common/rgb565.h"

namespace sunset {
using rgb565::hex;

// Field order: sky[7], sun (the moon at night), cloud, far, near, window, lampPost, lampLit,
// sidewalk, seam, curb, road, dash, star.
const Palette DAY = {
    {hex(0x382858), hex(0x502C68), hex(0x703474), hex(0x943C78), hex(0xB84C74), hex(0xD8646C), hex(0xF08460)},
    hex(0xF8D070), hex(0xF0A090), hex(0x6C3C70), hex(0x382440), hex(0xF8C868),
    hex(0x241830), hex(0xF8E8A8), hex(0x8C7088), hex(0x6C5470), hex(0x4C3850), hex(0x3C3448),
    hex(0xF8D878), hex(0xF8F8F8)};
const Palette NIGHT = {
    {hex(0x0C1028), hex(0x101834), hex(0x142040), hex(0x18284C), hex(0x203054), hex(0x28385C), hex(0x344064)},
    hex(0xE8E0C0), hex(0x1C2448), hex(0x1C2040), hex(0x0C0C1C), hex(0xF8C060),
    hex(0x080810), hex(0xF8E8A8), hex(0x3C3850), hex(0x2C2840), hex(0x1C1828), hex(0x181824),
    hex(0xB09848), hex(0xF8F8F8)};

static constexpr int BAND_H = (GROUND_Y - SKY_Y) / 7;   // 14
static constexpr int DASH_Y = 152, DASH_H = 2, DASH_PITCH = 32, DASH_LEN = 16;
static constexpr int SEAM_PITCH = 24;                    // sidewalk slabs
static constexpr int MOON_X = 250, MOON_Y = 44, MOON_R = 9;
static constexpr int LAMP_TOP = 78, LAMP_BASE = GROUND_Y + 8;

struct Building { int16_t x, w, h; };
// Front row, with windows; low around the sun so it shows. 2 px alleys between.
static const Building NEAR[] = {
    {0, 22, 40},   {24, 16, 28},  {42, 26, 50},  {70, 18, 34},  {90, 28, 44},
    {120, 16, 26}, {138, 24, 56}, {164, 20, 36}, {186, 26, 46}, {214, 14, 14},
    {230, 22, 20}, {254, 16, 30}, {272, 22, 46}, {296, 24, 34}};
// Back row: taller, narrower, hazy, clear of the sun.
static const Building FAR[] = {
    {8, 14, 62}, {52, 12, 70}, {102, 16, 66}, {150, 12, 76}, {196, 14, 58}, {282, 12, 68}, {304, 14, 56}};
struct Lamp { int16_t x, dir; };   // dir: the arm points this way
static const Lamp LAMPS[] = {{64, 1}, {256, -1}};
struct Streak { int16_t x0, x1, y; };
static const Streak CLOUDS[] = {
    {36, 96, 44}, {48, 110, 46}, {150, 186, 62}, {142, 176, 64}, {262, 310, 36}, {270, 300, 38}};

static uint32_t mix(uint32_t a) {
    a ^= a >> 16; a *= 0x7FEB352Du; a ^= a >> 15; a *= 0x846CA68Bu; a ^= a >> 16;
    return a;
}

static bool inside(const Building& b, int x, int y) {
    return x >= b.x && x < b.x + b.w && y >= GROUND_Y - b.h;
}

// 2x2 windows on a 5 x 6 grid, a margin all round; some lit, more at night.
static bool litWindow(int i, const Building& b, int x, int y, bool night) {
    const int lx = x - b.x - 3, ly = y - (GROUND_Y - b.h) - 4;
    if (lx < 0 || ly < 0 || lx % 5 >= 2 || ly % 6 >= 2) return false;
    if (lx / 5 * 5 + 2 > b.w - 6 || y >= GROUND_Y - 3) return false;
    return mix((uint32_t)(i * 977 + lx / 5 * 31 + ly / 6)) % 100 < (night ? 60u : 30u);
}

static bool lampPixel(int x, int y, const Palette& p, uint16_t* out) {
    for (const Lamp& l : LAMPS) {
        if (y >= LAMP_TOP && y < LAMP_BASE && (x == l.x || x == l.x + 1)) { *out = p.lampPost; return true; }
        const int ax = (x - l.x) * l.dir;   // along the arm
        if ((y == LAMP_TOP || y == LAMP_TOP + 1) && ax >= 0 && ax <= 8) { *out = p.lampPost; return true; }
        if (y >= LAMP_TOP + 2 && y < LAMP_TOP + 5 && ax >= 4 && ax <= 9) { *out = p.lampLit; return true; }
    }
    return false;
}

static bool inCircle(int dx, int dy, int r) { return dx * dx + dy * dy <= r * r; }

static uint16_t sky(int x, int y, const Palette& p, bool night) {
    if (night) {
        if (inCircle(x - MOON_X, y - MOON_Y, MOON_R) && !inCircle(x - MOON_X - 4, y - MOON_Y + 3, MOON_R - 1))
            return p.sun;   // a crescent
        if (y < 90 && mix((uint32_t)(y * 320 + x)) % 500 == 0) return p.star;
    } else if (inCircle(x - SUN_X, y - SUN_Y, SUN_R)) {
        const int d = y - (SUN_Y - 6);   // stripes, widening downwards
        if (d < 0 || d % 6 >= 1 + d / 10) return p.sun;
    }
    for (const Streak& s : CLOUDS)
        if (y == s.y && x >= s.x0 && x <= s.x1) return p.cloud;
    int band = (y - SKY_Y) / BAND_H;
    if (band > 6) band = 6;
    if (band > 0 && (y - SKY_Y) % BAND_H == 0 && ((x + y) & 1)) --band;   // dithered edge
    return p.sky[band];
}

uint16_t pixel(int x, int y, bool night) {
    const Palette& p = night ? NIGHT : DAY;
    if (y >= ROAD_Y) {
        const bool dash = y >= DASH_Y && y < DASH_Y + DASH_H && x % DASH_PITCH < DASH_LEN;
        return dash ? p.dash : p.road;
    }
    if (y >= CURB_Y) return p.curb;
    uint16_t v;
    if (lampPixel(x, y, p, &v)) return v;
    if (y >= GROUND_Y) return (y == GROUND_Y || x % SEAM_PITCH == 0) ? p.seam : p.sidewalk;
    for (int i = 0; i < (int)(sizeof NEAR / sizeof NEAR[0]); ++i)
        if (inside(NEAR[i], x, y)) return litWindow(i, NEAR[i], x, y, night) ? p.window : p.near;
    for (const Building& b : FAR)
        if (inside(b, x, y)) return p.far;
    return sky(x, y, p, night);
}
}
