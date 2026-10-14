#pragma once
#include <cstddef>
#include <cstdint>
// data/avatar.bin (tools/gen_avatar.py): the deck avatar's portraits. PURE, host-tested.
// "AVT1", u8 mood count n (1..8), u8 0, name (12 bytes, NUL-padded ASCII), n x (u8 mood id,
// u8 frame count 1..8), then every frame as 92x92 RGB565 high byte first, mood by mood.
// Spec: docs/superpowers/specs/2026-10-14-avatar-design.md
namespace avatarbin {
constexpr int SIDE = 92;
constexpr size_t FRAME_BYTES = (size_t)SIDE * SIDE * 2;
constexpr int MAX_MOODS = 8, MAX_FRAMES = 8, NAME_LEN = 12;
enum Mood : uint8_t { Smug = 0, Talk = 1, Happy = 2 };
struct Info {
    int moods;                        // 0 = no avatar (feature off)
    char name[NAME_LEN + 1];
    uint8_t id[MAX_MOODS], frames[MAX_MOODS];
    uint32_t first[MAX_MOODS];        // byte offset of each entry's frame 0
};
// Parses blob into out. False (out.moods = 0) for a missing, empty or malformed file; `why`
// (may be nullptr) gets a short reason for the log, "" on success.
bool parse(const uint8_t* blob, size_t len, Info& out, const char** why);
// Entry index for mood id; 0 (the first mood) when the file has no such mood.
int find(const Info& in, uint8_t id);
// Byte offset of frame f of entry e; e and f are clamped into range.
uint32_t frameOffset(const Info& in, int e, int f);
}
