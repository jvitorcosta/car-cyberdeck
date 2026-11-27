#pragma once
// The Pokédeck's boot opening: one of the black deck's intro videos (intro.bin + intro.pcm, or
// max.bin + max.pcm: tools/gen_intro.py, 320x240 MTG1 frames and 48 kHz mono sound) from the SD
// card, centred on the 480x272 panel and played by the clock of its sound (util/black/introclock).
// Never last boot's while the card has both (NVS "boot"/"video"). Without a video, the greeting
// (greeting.pcm, 16 kHz) alone, if it's there. Blocks until it's over, feeding the caller's
// watchdog.
namespace opening {
void play(bool screen);   // screen false: the panel didn't start, the sound alone
}
