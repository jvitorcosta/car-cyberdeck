#pragma once
#include <cstddef>
#include <cstdint>
#include "../../util/black/intropick.h"
// The deck's sound clips (boot greeting, montage opener and finale, the intro sound, howl):
// embedded 16-bit mono PCM played on the ES8311 codec (audio/sink.h). Each plays in its own short-lived task, one at a
// time, so boot carries on while it plays; the task frees the audio driver and itself when done.
namespace greeting {
void play();                    // this boot's greeting (choose())
// This boot's greeting, from the opening (util/intropick): MAIN (data/greeting.pcm), ALT
// (data/greeting_alt.pcm) or NONE (play() does nothing, durationMs() is 0). MAIN unless called.
void choose(intropick::Greeting g);
uint32_t durationMs();          // this boot's greeting's length
// The greeting's loudness 0-255 over 20 ms right now, by its own clock (from its first sample
// out of the speaker, ~0.1 s after play()); 0 before that and past its end. Ignores VOLUME.
uint8_t level();
// Finale (data/finale.pcm, git-ignored, may be empty): played by the boot scene so it ends with
// the montage's fade to black. Never overlaps the greeting (the scene only ends after it).
void playFinale();
uint32_t finaleMs();                // 0 when there's no finale clip
// Opener (data/opener.pcm, git-ignored, may be empty): played on the montage's first frame. The
// scene skips it when it would still be playing as the finale starts (one I2S port).
void playOpener();
uint32_t openerMs();                // 0 when there's no opener clip
// A boot intro video's sound (data/intro.pcm for the kiss, max.pcm for Max; 48 kHz, git-ignored,
// may be empty), started by ui/intro with its first frame.
bool playIntro(intropick::Opening o);   // false: no sound for this video
uint32_t introStartedAt();          // millis() of the intro sound's first sample out; 0 until then
// Wolf howl for the phone remote (net/remote): a real recording (data/howl.pcm from
// tools/gen_howl.py, git-ignored) when the build has one, else synthesised on the fly (util/howl);
// the synthesised one while music mode's mic holds the port (the recording is 48 kHz).
void playHowl();
bool playing();                     // a clip is still playing (its task and I2S driver are alive)

// Boot sound rosters (data/drive.snd, opener.snd, finale.snd from tools/gen_rosters.py, git-ignored):
// many clips per slot, one picked each boot by main.cpp (util/soundpick). A slot without a roster
// plays its single file above (greeting.pcm, opener.pcm, finale.pcm).
enum Slot : uint8_t { DRIVE = 0, OPENER = 1, FINALE = 2 };
int rosterCount(Slot s);                 // clips in the slot's roster; 0 = none
uint32_t rosterMs(Slot s, int i);        // clip i's length; 0 when out of range
const char* rosterName(Slot s, int i);   // clip i's name (logs); "?" when out of range
// This boot's clip of the slot (DRIVE stands for MAIN's greeting). i < 0 with a roster: no sound
// for that slot (no montage-start clip fits before the logo's); without a roster: the single file.
void pick(Slot s, int i);
}
