#pragma once
#include <TFT_eSPI.h>
#include "link.h"
// The gauge cluster's bottom strip (y gauges::STRIP_Y..240): her small still portrait on the left
// (avatarview::HALF, placed by the app) and, beside it, her line while she talks (inverted for a
// warning), else the current lyric and the next one. Spec 2026-10-21-companion-drive-design.md.
namespace carstrip {
constexpr int PORTRAIT_X = 2;   // where the app places her (y = gauges::STRIP_Y + 1)
// herLine: her line typed so far (nullptr when quiet); warning: it's a warning.
void draw(TFT_eSPI& t, const decklink::View& v, const char* herLine, bool warning, uint32_t nowMs);
void invalidate();   // repaint on the next draw
}
