#pragma once
#include <cstdint>
// The night drive's car: the committed Honda (ui/car_sprite) or, on the S3, the git-ignored
// McQueen (data/mcqueen.bin, util/carbin). Size, wheel centres and light anchors come with it.
namespace carlook {
struct Look {
    int w, h, rearX, frontX, wheelY;   // wheel centres (wheels are drawn in code)
    int headX, headY, tailY;           // headlight beam origin; taillight glow top (sprite coords)
    bool (*pixel)(int x, int y, uint16_t* rgb565, uint8_t* kind);
};
Look honda();
bool mcqueen(Look* out);   // false without a valid data/mcqueen.bin (always on the CYD)
}
