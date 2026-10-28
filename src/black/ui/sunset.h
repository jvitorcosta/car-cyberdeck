#pragma once
#include <cstdint>
// The "Sunset drive" backdrop behind Sonic on the "Waiting for a song..." screen: a banded sky with a
// striped low sun, a city skyline with lit windows, two street lamps, the sidewalk he waits on
// and the road. At night: navy sky, stars, a crescent moon, more windows lit. PURE, host-tested:
// pixel() gives any screen pixel below the top strip (0..319 x 20..239); nothing moves.
namespace sunset {
constexpr int SKY_Y = 20;        // under the top strip
constexpr int GROUND_Y = 118;    // the skyline's base, the sidewalk's top (= ui::background horizon)
constexpr int CURB_Y = 136;      // sidewalk down to here (Sonic's feet at statusscene::FEET_Y)
constexpr int ROAD_Y = 138;      // asphalt from here down
constexpr int SUN_X = 238, SUN_Y = 100, SUN_R = 26;

struct Palette {
    uint16_t sky[7];             // top band first
    uint16_t sun, cloud, far, near, window;
    uint16_t lampPost, lampLit, sidewalk, seam, curb, road, dash, star;
};
extern const Palette DAY;
extern const Palette NIGHT;

uint16_t pixel(int x, int y, bool night);
}
