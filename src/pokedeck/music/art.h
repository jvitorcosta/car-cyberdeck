#pragma once
#include <stdint.h>
// Copied from black/images/art.h: the Pokedeck's cover is 98x98.
// Album art as a fixed RGB565 bitmap, allocated once. The network task streams the
// cover JPEG over plain HTTP straight into the decoder and writes the bitmap; the UI pushes it.
// Replaced holding the whole JPEG (20-60 KB) plus a 21.6 KB UI decode buffer, which exhausted
// byte-addressable RAM (README "Design & performance history").
namespace art {
constexpr int W = 98, H = 98;
bool begin();                 // bitmap + tjpgd workspace; call in setup() before WiFi
bool fetch(const char* url);  // network task only; caller invalidated the art first
const uint16_t* bitmap();     // big-endian RGB565, W*H (pushImage with swap off)
}
