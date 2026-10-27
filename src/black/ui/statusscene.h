#pragma once
#include <cstdint>
#include "../../util/black/linkstatus.h"
// The animated picture on the status screens: a bundled Pokemon per cause (status_sprite)
// standing 2x on the battle platform, bobbing, with a speech bubble; a Poke Ball rolls along
// the platform while the deck looks for WiFi. PURE, host-tested: pixel() gives each screen
// pixel's colour, ui::drawOfflineScene pushes the region in small strips (no big buffer).
namespace statusscene {
enum class Bubble : uint8_t { None, Question, Dots, Zzz, Bang };
// idle: Sonic waiting (util/idlesprite) instead of the sprite: "Waiting for a song..." with his
// file, in front of the Sunset drive (ui/sunset); night: its night palette (the caller sets it).
struct Look { int sprite; Bubble bubble; bool ball; bool idle = false; bool night = false; };
// Ok = "Waiting for a song..." (Pikachu, no bubble).
Look lookFor(linkstatus::Kind k);

// Layout: the Pokemon on its platform in the middle, its bubble to the upper right, the
// dialogue box across the bottom. Only the scene region is redrawn every frame.
constexpr int DLG_Y = 168, DLG_H = 68;
constexpr int X0 = 40, Y0 = 20, W = 240, H = 148;
constexpr int CX = 160;              // sprite and platform centre
constexpr int IDLE_CX = 216;         // Sonic's centre: right of the phone link's QR card (x 4..127)
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
// The part of the scene that changes from frame to frame, all ui::drawOfflineScene repaints
// after the screen's first paint: the whole region with a bubble or a rolling ball, else only
// the waiting figure's box (Sonic, or Pikachu with room for his bob), so the phone link's QR
// card beside him (ui/qrcard) is painted once.
Rect frameRect(const Look& look);
}
