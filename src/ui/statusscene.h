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

// Region redrawn every frame: between the top strip and the dialogue box.
constexpr int X0 = 40, Y0 = 20, W = 240, H = 140;
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
Rect bubbleRect(const Look& look, const Pose& pose);   // the box, without its tail
}
