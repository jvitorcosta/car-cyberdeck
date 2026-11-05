#pragma once
#include <cstdint>
// One clip of a slot's roster (util/sndroster) each boot: the drive greeting, the montage start
// or the Honda logo. Random, never last boot's (kept in NVS by main.cpp), only from the clips `ok`
// allows (nullptr = all: the montage start must end before the logo's clip). PURE, host-tested.
namespace soundpick {
constexpr int MAX = 32;   // == sndroster::MAX; larger counts are clamped
// last: the previous boot's (anything out of range = none). -1 when count <= 0 or none is ok.
int pick(uint32_t rnd, int last, int count, const bool* ok = nullptr);
// The test hook -D SOUND_FORCE=\"drive:3,opener:0\": slot's index when spec names it and it is
// below count, else -1.
int forced(const char* spec, const char* slot, int count);
}
