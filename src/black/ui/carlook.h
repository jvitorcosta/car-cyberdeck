#pragma once
#include <cstdint>
// The night drive's car: the committed Honda (ui/car_sprite) or, on the S3, the git-ignored
// McQueen (data/mcqueen.bin, util/carbin). Size, wheel centres and light anchors come with it.
namespace carlook {
// Wheel colours, RGB565: ring[1..6] tyre to hub as ui/bootscene's wheel rings them,
// spoke[0..2] between spokes, spoke face, spoke edge.
struct Rims { uint16_t ring[7]; uint16_t spoke[3]; };
extern const Rims SILVER;   // the Honda's machined alloys
extern const Rims RED;      // McQueen's red rims
struct Look {
    int w, h, rearX, frontX, wheelY;   // wheel centres (wheels are drawn in code)
    int headX, headY, tailY;           // headlight beam origin; taillight glow top (sprite coords)
    bool (*pixel)(int x, int y, uint16_t* rgb565, uint8_t* kind);
    const Rims* rims;
};
Look honda();
bool mcqueen(Look* out);   // false without a valid data/mcqueen.bin (always in host tests)
}
