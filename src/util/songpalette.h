#pragma once
#include <cstdint>
// Three colours (and a brightness cap / pulse depth) for the car's LED lights, computed once per song
// from the album cover, the genre badge and the time of day. PURE, host-tested.
namespace songpalette {
struct Rgb { uint8_t r, g, b; };
struct Palette { Rgb c[3]; uint8_t cap; uint8_t depth; };   // cap: max brightness 0..100, depth: pulse depth 0..100
// Up to 3 dominant colours of an RGB565 big-endian bitmap (the S3's art buffer), skipping pixels with
// saturation < 25 % or value < 15 % (and washed-out highlights: value > 95 % with saturation < 50 %); hues
// bucketed by 15 deg, a bucket needs 3 % of the pixels, picks are >= 30 deg apart, returned at full S and V.
// Returns the count (0 for a null bitmap or w/h <= 0).
int fromCover(const uint16_t* bmpBe, int w, int h, Rgb out[3]);
// The genre family's 3 colours for a genrebadge index (0..30; genre::NONE = 0xFF or anything else = neutral).
void family(uint8_t badge, Rgb out[3]);
// Cover + genre + time: < 2 cover colours = the family; otherwise each cover hue nudged <= 15 deg toward the
// family's nearest hue (never past it; no nudge for an unknown genre, badges 28-30, NONE or out of range,
// whose neutral family would only pull covers its way), and the third slot filled from the family when the
// cover gave two.
// minuteOfDay < 0 (clock not synced) = day. Day (06:00-18:00): cap 100, depth 60. Evening (18:00-22:00,
// 05:00-06:00): warmer (hues 180-270 deg shifted 15 deg toward 300, washed-out colours = amber), cap 70,
// depth 45. Late (22:00-05:00): same warm shift, cap 45, depth 30.
Palette build(const Rgb* cover, int coverCount, uint8_t badge, int minuteOfDay);
// Music mode's palette for the car's lights: build()'s colours as by day (no evening warm shift, no
// whites turned amber: the yellow deck snaps c[0] to one swatch, which must not change with the
// clock mid-song) with the time of day's cap and depth.
Palette forLights(const Rgb* cover, int coverCount, uint8_t badge, int minuteOfDay);
}
