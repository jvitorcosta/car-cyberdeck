#pragma once
#include <TFT_eSPI.h>
#include "link.h"
// The companion's screen (v1: lyrics only): the current line big over up to two rows, the next
// line small and dim under it, and a status line at the bottom when the deck is out of reach.
// Repainted only when what it shows changes (no flicker, little SPI time).
namespace dashboard {
void draw(TFT_eSPI& t, const decklink::View& v, uint32_t nowMs);
void invalidate();   // the next draw repaints everything (night mode switch)
}
