#pragma once
#include <cstdint>
// The companion avatar singing along with synced lyrics: when her mouth moves for the line on
// screen. PURE, host-tested. Spec: docs/superpowers/specs/2026-10-14-avatar-design.md (Revision).
namespace avatarsing {
constexpr uint32_t FLAP_MS = 150;        // mouth open / closed every 150 ms
constexpr uint32_t LONE_LINE_MS = 4000;  // a line with no next line is sung this long
// True while the line that started at lineStartMs is sung at song position posMs: the first
// 3/4 of the time to the next line (nextStartMs; 0 = none, then LONE_LINE_MS).
bool singing(uint32_t posMs, uint32_t lineStartMs, uint32_t nextStartMs);
// The frame for a mood with `frames` frames: open (1) and rest (0) alternating every FLAP_MS
// while singing; 0 otherwise.
int frame(uint32_t posMs, uint32_t lineStartMs, uint32_t nextStartMs, int frames);
}
