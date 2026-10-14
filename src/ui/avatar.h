#pragma once
#include <TFT_eSPI.h>
#include <stdint.h>
// The deck avatar on screen (S3): her frames from the embedded data/avatar.bin in the album-art
// box, her name tag and her line in the dialogue box. Without a valid file (or on the CYD)
// everything here is a no-op. Spec: docs/superpowers/specs/2026-10-14-avatar-design.md
namespace ui {
enum class AvatarTick : uint8_t { Idle, Running, Ended };
// Parses the embedded file once and logs "[avatar] ..."; false when there is none.
bool avatarBegin();
bool avatarOn();
bool avatarActive();
// Starts her scene now, replacing any running one. mood: avatarbin::Mood.
void avatarStart(uint8_t mood, const char* text, uint32_t nowMs);
// Draws what changed since the last call. Ended comes once, when the scene is over: the caller
// then puts the album art back, calls clearNameTag and redraws the lyric.
AvatarTick avatarTick(TFT_eSPI& t, uint32_t nowMs);
// Drops the scene without drawing (the caller is about to repaint the screen).
void avatarCancel();
}
