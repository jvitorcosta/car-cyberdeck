#pragma once
#include <cstddef>
#include <cstdint>
// Sonic waiting on the black deck's "Waiting for a song..." screen (data/idle.bin, from
// tools/gen_idle_sprite.py; git-ignored, S3 only): his four Sonic 1 frames and the game's own
// waiting timeline. PURE, host-tested. A missing, empty or bad file reads as none (count 0):
// the screen keeps its Pokemon.
namespace idlesprite {
enum Frame : int { STAND, WAIT1, WAIT2, WAIT3, FRAMES };   // the file's frame order
constexpr uint32_t FRAME_MS = 400;   // 24 game ticks at 60 Hz
bool parse(const uint8_t* blob, size_t len);   // false (and count 0) unless it is a valid file
int count();                                   // FRAMES, or 0 without a file
int width();
int height();
// True when (x, y) of frame f is opaque; then *rgb565 is its colour.
bool pixel(int f, int x, int y, uint16_t* rgb565);
// The game's waiting animation, ms after he first stood still: 12 x STAND, WAIT2, WAIT1 x 3,
// then WAIT2 / WAIT3 for ever (the foot tap).
int frameAt(uint32_t ms);
uint32_t tickAt(uint32_t ms);   // which 400 ms step: the frame can only change when this does
}
