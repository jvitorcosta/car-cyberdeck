#pragma once
// RGB -> hue and saturation, and how far apart two hues are: the one measure behind a cover's colours
// (songpalette), the nearest swatch (musiclights) and a mood's nearest colour (songmood). PURE,
// host-tested.
namespace hue {
// The hue of r,g,b (0..255 each) on a circle of `turn` steps (360: degrees, 3600: tenths; a multiple
// of 6), 0 for a grey; its saturation, the spread over the brightest channel, out of `full` (100: %;
// 255), 0 for black. Integer maths, each sector's step truncated toward zero.
void hueSat(int r, int g, int b, int turn, int full, int* hue, int* sat);
// The circular distance between hues a and b on a circle of `turn` steps: 0..turn/2.
int dist(int a, int b, int turn);
}  // namespace hue
