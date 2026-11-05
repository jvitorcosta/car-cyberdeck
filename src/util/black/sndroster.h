#pragma once
#include <cstddef>
#include <cstdint>
// A slot's boot clips (tools/gen_rosters.py): "SNDR", u16 version, u16 count, then count x
// {u32 offset from the file's start, u32 size in bytes, char name[24] NUL-padded}, then the clips
// (16-bit mono 16 kHz PCM, 2-byte aligned). Little-endian. PURE, host-tested.
namespace sndroster {
constexpr size_t HEAD = 8, ENTRY = 32, NAME = 24;
constexpr uint16_t VERSION = 1;
constexpr int MAX = 32;
struct Clip { const uint8_t* data; size_t bytes; const char* name; };
// Clips in the file; 0 when it isn't one (bad magic or version, more than MAX, table past len).
int count(const uint8_t* blob, size_t len);
// Clip i; false when i is out of range, its bytes run past len or its name has no NUL.
bool at(const uint8_t* blob, size_t len, int i, Clip* out);
}
