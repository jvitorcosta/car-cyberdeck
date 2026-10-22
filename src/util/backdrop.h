#pragma once
#include <cstddef>
#include <cstdint>
// The Star Light Zone night behind the S3's firmware-update screen (data/slzbg.bin, from
// tools/gen_slz_backdrop.py; git-ignored, in the media partition). PURE, host-tested. A missing,
// stand-in or bad file reads as none (width 0): the update screen keeps its battle box.
// Format "BKD1": u16 width, u16 height (LE), then the pixels row by row, each RGB565 as 2 bytes
// big-endian (pushImage's byte order).
namespace backdrop {
bool parse(const uint8_t* blob, size_t len);   // false (and width 0) unless it is a valid file
int width();
int height();
const uint8_t* row(int y);   // width() * 2 bytes; nullptr out of range or without a file
}
