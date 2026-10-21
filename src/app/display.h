#pragma once
#include <TFT_eSPI.h>
// The deck's one display, shared by boot (app/boot) and the screens (ui/deckview, ui/statusview).
// UI loop (core 1): draws only. All network work runs in core/nettask on core 0 and arrives
// through core/shared (README "Design & performance history").
namespace app {
TFT_eSPI& tft();
}
