#pragma once
#include <cstddef>
#include <cstdint>
// The deck avatar's scene timeline: she slides into the art box, types her line while her mouth
// flaps, holds it, then leaves. PURE, host-tested.
// Spec: docs/superpowers/specs/2026-10-14-avatar-design.md ("Her scene").
namespace avatarscene {
constexpr int SIDE = 92;
constexpr uint32_t SLIDE_MS = 240;   // STEPS steps of 40 ms
constexpr int STEPS = 6;
constexpr uint32_t CHAR_MS = 25;     // ~40 characters per second
constexpr uint32_t MOUTH_MS = 100;   // the mouth opens / closes at most this often
constexpr uint32_t HOLD_MS = 3000;   // the whole line stays up this long
enum class Phase : uint8_t { Slide, Type, Hold, Done };
struct State {
    Phase phase;
    int shown;      // portrait columns visible at the left of the art box (0..SIDE)
    size_t bytes;   // UTF-8 prefix of the line typed so far (whole characters)
    int frame;      // the mood's frame to show (0 = rest, mouth closed)
};
// ms: since the scene started; line: her text (nullptr = ""); frames: the mood's frame count.
State at(uint32_t ms, const char* line, int frames);
}
