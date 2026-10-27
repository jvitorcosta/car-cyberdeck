#pragma once
#include <cstdint>
// A song's colours for the car's LED lights: the album cover's dominant colours, and the time of
// day's brightness cap. util/songmood decides the colour from these. PURE, host-tested.
namespace songpalette {
struct Rgb { uint8_t r, g, b; };
struct Palette { Rgb c[3]; uint8_t cap; uint8_t depth; };   // cap: max brightness 0..100, depth: pulse depth 0..100
// Up to 3 dominant colours of an RGB565 big-endian bitmap (the S3's art buffer), skipping pixels with
// saturation < 25 % or value < 15 % (and washed-out highlights: value > 95 % with saturation < 50 %); hues
// bucketed by 15 deg, a bucket needs 3 % of the pixels, picks are >= 30 deg apart, returned at full S and V.
// Returns the count (0 for a null bitmap or w/h <= 0).
int fromCover(const uint16_t* bmpBe, int w, int h, Rgb out[3]);
// The time of day's brightness cap and pulse depth for the car's lights (the colours never change
// with the clock: util/songmood picks them). minuteOfDay < 0 (clock not synced) = day. Day
// (06:00-18:00): cap 100, depth 60. Evening (18:00-22:00, 05:00-06:00): cap 70, depth 45. Late
// (22:00-05:00): cap 45, depth 30.
void timeOfDay(int minuteOfDay, uint8_t* cap, uint8_t* depth);
}
