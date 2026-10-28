#pragma once
#include <stdint.h>

// The walker's frames. Two slots so the network task can load the next walker while
// the UI keeps drawing the current one:
//  - loaders (loadPmd / loadFallback) always write the STAGED slot (network task only);
//  - info / pixels / mask / durationMs read the ACTIVE slot (UI, under shared::lock);
//  - promote() swaps them (network task, under shared::lock).
// Sources: PMD SpriteCollab walk cycle (Right row, several frames) or, as a fallback,
// the PokeAPI sprite cropped to one frame (the UI adds bob + mirror).
namespace walk {
constexpr int BAND_H     = 32;      // walk band height in the status box
constexpr int MAX_W      = 64;      // widest frame kept
constexpr int MAX_FRAMES = 16;
// Per slot: RGB565 + 1-byte mask per pixel, room for every frame at the largest size (PSRAM, so
// no frame is dropped to fit; it was 10 KB in internal RAM).
constexpr int CAP_BYTES  = MAX_FRAMES * MAX_W * BAND_H * 3;
// One download buffer for XML / sheet / sprite (begin(), PSRAM); larger sheets fall back. It was
// 12 KB in internal RAM and sent 17 KB sheets to the fallback sprite (2026-10-21).
constexpr int SCRATCH    = 64 * 1024;

struct Info { bool ready; bool pmd; int w, h, frames; };

// Allocates both slots and the download scratch once (setup(), before WiFi).
bool begin();

bool loadPmd(int dex);                            // -> staged
bool loadFallback(const char* spriteUrl, int dex); // -> staged
int stagedDex();                                  // dex in the staged slot, 0 if none
bool outOfMemory();                               // the last load failed only for lack of heap (retry)
void promote();                                   // staged <-> active

const Info& info();                  // active slot
const uint16_t* pixels(int frame);   // big-endian RGB565, w*h
const uint8_t*  mask(int frame);     // 1 = opaque, w*h
uint16_t durationMs(int frame);      // 0 for the fallback frame
}
