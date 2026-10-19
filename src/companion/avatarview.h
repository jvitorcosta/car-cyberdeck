#pragma once
#include <TFT_eSPI.h>
#include "link.h"
#include "../util/avatarlines.h"
// The deck avatar on the companion (data/avatar.bin, embedded in this build only): her portrait
// at the spot and size the current screen gives her, singing along with synced lyrics, and her
// lines for the message bar. Spec: docs/superpowers/specs/2026-10-14-avatar-home-design.md
namespace avatarview {
// Parses the embedded file; false (no avatar: the companion's lyrics-only screen) when there is
// none. Logs "[avatar] ...".
bool begin();
bool on();
void invalidate();                         // repaint her (and her name tag) on the next update
// Where the current screen shows her: top-left corner and scale (1 = 92x92, 2 = 184x184,
// HALF = 46x46, still: her first frame, no name tag (the gauge cluster); 0 = not on this
// screen). A change repaints her on the next update.
constexpr int HALF = -1;
void place(int x, int y, int scale);
// She says the line for this event now (reactions always; Boot / Track by avatarlines' rules).
void say(avatarlines::Event ev, const decklink::View& v, uint32_t nowMs);
// Per frame: the boot greeting and song-change remarks from the feed, her portrait (her line's
// mood, else singing along, else the lollipop), and her line typed so far for the message bar
// (nullptr when she is quiet).
const char* update(TFT_eSPI& t, const decklink::View& v, uint32_t nowMs);
}
