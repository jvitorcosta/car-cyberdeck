#pragma once
#include <TFT_eSPI.h>
#include "link.h"
// The deck avatar beside the lyrics on the companion screen (data/avatar.bin, embedded in this
// build only): her portrait at the left, singing along with synced lyrics, and now and then a
// line of her own. Spec: docs/superpowers/specs/2026-10-14-avatar-design.md (Revision).
namespace avatarview {
constexpr int X = 10, Y = 40;    // portrait; the name tag sits under it
constexpr int COL_X = 112;       // the lyric column starts here while she is on
// Parses the embedded file; false (lyrics full width) when there is none. Logs "[avatar] ...".
bool begin();
bool on();
void invalidate();               // repaint her on the next update (night mode switch)
// Draws her for this frame. Returns her own line, typed so far, to show instead of the lyric,
// or nullptr when the lyrics should show.
const char* update(TFT_eSPI& t, const decklink::View& v, uint32_t nowMs);
}
