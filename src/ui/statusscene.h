#pragma once
#include <cstdint>
#include "../util/linkstatus.h"
// The animated picture on the status screens: a bundled Pokemon per cause (status_sprite)
// standing 2x on the battle platform, bobbing, with a speech bubble; a Poke Ball rolls along
// the platform while the deck looks for WiFi. PURE, host-tested: pixel() gives each screen
// pixel's colour, ui::drawOfflineScene pushes the region in small strips (no big buffer).
namespace statusscene {
enum class Bubble : uint8_t { None, Question, Dots, Zzz, Bang };
// idle: Sonic waiting (util/idlesprite) instead of the sprite: "Nothing playing" with his file,
// in front of the Sunset drive (ui/sunset); night: its night palette (the caller sets it).
struct Look { int sprite; Bubble bubble; bool ball; bool idle = false; bool night = false; };
// Ok = "Nothing playing" (Pikachu, no bubble).
Look lookFor(linkstatus::Kind k);

// Layout: the Pokemon on its platform in the middle, its bubble to the upper right, the
// dialogue box across the bottom. Only the scene region is redrawn every frame.
constexpr int DLG_Y = 168, DLG_H = 68;
constexpr int X0 = 40, Y0 = 20, W = 240, H = 148;
constexpr int CX = 160;              // sprite and platform centre
constexpr int FEET_Y = 132;          // sprite's bottom edge at rest
constexpr uint32_t FRAME_MS = 250;   // 4 fps: gentle, and cheap on the UI loop

struct Pose { int bob; int step; int ballX; int ballFrame; int idleFrame; };   // bob: px up; ballX: from centre
// ms: time since the screen appeared (Sonic's waiting animation starts over with it).
Pose poseAt(uint32_t ms, const Look& look);
// The scene needs redrawing when this changes: each FRAME_MS for the Pokemon; for Sonic only
// when his frame (every 400 ms) or the dialogue box's blinking arrow (every 500 ms) can change.
uint32_t frameKey(uint32_t ms, const Look& look);

struct Colors {
    uint16_t sky, horizon, grass, platform, edge;
    uint16_t bubble, ink, ballRed, ballWhite, ballDark;
};
uint16_t pixel(int x, int y, const Look& look, const Pose& pose, const Colors& c);

struct Rect { int x, y, w, h; };
Rect bubbleRect(const Look& look, const Pose& pose);   // the box, without tail and shadow
}
