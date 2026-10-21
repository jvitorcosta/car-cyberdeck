#pragma once
#include <TFT_eSPI.h>
#include "link.h"
// The Lyrics screen: the current line big (2x when it fits in two rows, else 1x), the next line
// dim under it. With the avatar it's a column right of her portrait (x 104..316, above the
// message bar); without one, the whole screen plus a status line. Repainted only when what it
// shows changes (no flicker, little SPI time).
namespace dashboard {
void draw(TFT_eSPI& t, const decklink::View& v, uint32_t nowMs);
void invalidate();   // the next draw repaints everything (night mode switch)
void setTop(int y);  // the lyric area starts at y (0; the stopped view's gauge strip: gauges::TOP_H)
}
