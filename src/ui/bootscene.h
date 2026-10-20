#pragma once
#include <TFT_eSPI.h>
#include "carlook.h"
// Boot scene: a car (the Honda, or on the S3 the pixel McQueen) drives along a night road while the
// greeting plays and WiFi connects, then the photo montage; or, for the kiss opening, the montage
// alone (docs/superpowers/specs/2026-10-09-boot-scene-design.md, 2026-10-19-random-intro-design.md). Runs in its own task; while
// active() it is the only code drawing on the display.
namespace bootscene {
// Starts the scene task. `online` is polled each frame: the car leaves once it has been shown
// for 5 s, the greeting has ended and online() is true.
// car: which car drives (ui/carlook); drive false: no drive, the montage starts once online()
// (the kiss opening: the video already ran).
struct Opts { carlook::Look car; bool drive; };
void start(TFT_eSPI& tft, bool (*online)(), const Opts& o);
void start(TFT_eSPI& tft, bool (*online)());   // the Honda's drive
// Static string. A BIOS-style log types into an on-board terminal in the sky while the car
// drives; a caption containing "retry" turns its WI-FI line to RETRYING... (the caption text
// itself only shows when the scene couldn't get its strip buffer).
void setCaption(const char* text);
bool active();                       // true until the car has left and the task has ended
void waitDone();                     // blocks until !active()
}
