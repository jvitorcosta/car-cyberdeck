#pragma once
#include <cstdint>
#include "../util/linkstatus.h"
// The animated picture on the status screens: a bundled Pokemon per cause (status_sprite)
// standing 2x on the battle platform, bobbing, with a speech bubble; a Poke Ball rolls along
// the platform while the deck looks for WiFi. PURE, host-tested: pixel() gives each screen
// pixel's colour, ui::drawOfflineScene pushes the region in small strips (no big buffer).
namespace statusscene {
enum class Bubble : uint8_t { None, Question, Dots, Zzz, Bang };
struct Look { int sprite; Bubble bubble; bool ball; };
// Ok = "Nothing playing" (Pikachu, no bubble).
Look lookFor(linkstatus::Kind k);

// Battle layout: info box top-left (name, Lv, HP), the Pokemon on its platform to the right,
// the dialogue box across the bottom. Only the scene region is redrawn every frame.
constexpr int INFO_X = 8, INFO_Y = 28, INFO_W = 120, INFO_H = 44;
constexpr int DLG_Y = 168, DLG_H = 68;
constexpr int X0 = 132, Y0 = 20, W = 184, H = 148;
constexpr int CX = 196;              // sprite and platform centre
constexpr int FEET_Y = 132;          // sprite's bottom edge at rest
constexpr uint32_t FRAME_MS = 250;   // 4 fps: gentle, and cheap on the UI loop

struct Pose { int bob; int step; int ballX; int ballFrame; };   // bob: px up; ballX: from centre
Pose poseAt(uint32_t ms, const Look& look);

struct Colors {
    uint16_t sky, horizon, grass, platform, edge;
    uint16_t bubble, ink, ballRed, ballWhite, ballDark;
};
uint16_t pixel(int x, int y, const Look& look, const Pose& pose, const Colors& c);

struct Rect { int x, y, w, h; };
Rect bubbleRect(const Look& look, const Pose& pose);   // the box, without tail and shadow

// Info-box HP bar, 0..1000: full for "Nothing playing", low for no WiFi / certificate, half
// for no internet. While Spotify rate-limits (429) it charges up as the retry nears.
int hpPermille(linkstatus::Kind k, int lastCode, int retryS, int intervalS);
}
