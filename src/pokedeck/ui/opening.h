#pragma once
// The Pokédeck's boot opening: one of the two intro videos from the SD card (tools/gen_intro.py:
// MTG1 frames and 48 kHz mono sound), full screen from intro_wide / max_wide (.bin + .pcm, 480x272,
// --wide), else the black deck's 320x240 intro / max centred, played by the clock of its sound
// (util/black/introclock).
// Never last boot's while the card has both (NVS "boot"/"video"). Without a video, the greeting
// (greeting.pcm, 16 kHz) alone, if it's there. Blocks until it's over, feeding the caller's
// watchdog.
namespace opening {
void play(bool screen);   // screen false: the panel didn't start, the sound alone
}
