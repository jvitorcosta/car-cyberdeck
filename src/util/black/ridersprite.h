#pragma once
#include <cstddef>
#include <cstdint>
// Shadow skating on the firmware-update rail (data/shadow.bin, from tools/gen_shadow_rider.py;
// git-ignored, in the media partition): his Sonic Battle running loop and a standing frame (for
// "Update failed"). PURE, host-tested. A missing, stand-in or bad file reads as none
// (runFrames 0): the rail keeps its two jet glows.
namespace ridersprite {
constexpr uint32_t FRAME_MS = 70;   // ~4 game frames at 60 Hz, as he skates in Sonic Battle
constexpr int MAX_SIDE = 64;
bool parse(const uint8_t* blob, size_t len);   // false (and runFrames 0) unless it is a valid file
int runFrames();                               // the run loop's length, 0 without a file
int stand();                                   // the standing frame's index (= runFrames())
int width();
int height();
// True when (x, y) of frame f is opaque; then *rgb565 is its colour.
bool pixel(int f, int x, int y, uint16_t* rgb565);
int frameAt(uint32_t ms);                      // the run loop's frame ms into the animation
}
