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
// Camera alert sound (beeps and voice lines, car/camsound) cutting in. pauseFor silences the clip playing now for ms (it
// stops between two 128-sample writes, keeps its output open, and carries on from the same sample
// afterwards); it returns once that clip has stopped writing (or none plays). A clip that starts
// meanwhile waits for it too. Its length on screen (durationMs, level) does not stretch.
void pauseFor(uint32_t ms);
// Then the output for the beeps: the rate to write them at with audiosink::write (the paused clip's,
// whose output they borrow, or 16 kHz on an output opened for them when no clip plays), or 0 when it
// can't be had right now (a clip opening or closing it: ask again shortly). endTones() follows on
// the same task, whatever beginTones() said: it ends the pause and gives the output back, unless
// keepOpen asks to hold an output opened for the beeps (an alarm's next beep in 1.5 s: no driver,
// codec and amp cycle per beep) and no clip is waiting for it. Then the next beginTones() reuses
// it, and releaseTones() (same task) closes it: on leaving the alarm, or when
// clipWaiting() says a clip started meanwhile.
int beginTones();
void endTones(bool keepOpen = false);
void releaseTones();
bool clipWaiting();
// The audio output is still on from the last 16 kHz clip (audio/sink.h warm()): a clip cued now is
// heard at once (bootanim::SOUND_LEAD_MS), not after the speaker's wake (COLD_SOUND_LEAD_MS).
bool warm();

// WiFi-connected chime: this boot's CONNECT clip, played by the boot scene once the deck is online
// and the greeting is over, just before the montage. Silent without a connect roster (no single file).
void playConnect();

// Boot sound rosters (data/drive.snd, opener.snd, finale.snd, connect.snd from tools/gen_rosters.py,
// git-ignored): many clips per slot, one picked each boot by main.cpp (util/soundpick). A slot without
// a roster plays its single file above (greeting.pcm, opener.pcm, finale.pcm; CONNECT has none).
enum Slot : uint8_t { DRIVE = 0, OPENER = 1, FINALE = 2, CONNECT = 3 };
int rosterCount(Slot s);                 // clips in the slot's roster; 0 = none
uint32_t rosterMs(Slot s, int i);        // clip i's length; 0 when out of range
const char* rosterName(Slot s, int i);   // clip i's name (logs); "?" when out of range
// This boot's clip of the slot (DRIVE stands for MAIN's greeting). i < 0 with a roster: no sound
// for that slot (no montage-start clip fits before the logo's); without a roster: the single file.
void pick(Slot s, int i);
}
