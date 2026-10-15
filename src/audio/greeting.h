#pragma once
#include <cstdint>
// The deck's sound clips (boot greeting, montage opener and finale, the S3's intro sound, howl):
// embedded 16-bit mono PCM played on the board's audio output (audio/sink.h: the CYD's DAC on
// its SPEAK connector, the S3's ES8311 codec). Each plays in its own short-lived task, one at a
// time, so boot carries on while it plays; the task frees the audio driver and itself when done.
namespace greeting {
void play();                    // S3: the original or data/greeting_alt.pcm, at random (util/greetpick)
uint32_t durationMs();          // this boot's greeting's length
uint8_t level(uint32_t ms);     // clip loudness 0-255 over 20 ms at `ms`; 0 past the end. Ignores VOLUME.
// Finale (data/finale.pcm, git-ignored, may be empty): played by the boot scene so it ends with
// the montage's fade to black. Never overlaps the greeting (the scene only ends after it).
void playFinale();
uint32_t finaleMs();                // 0 when there's no finale clip
// Opener (data/opener.pcm, git-ignored, may be empty): played on the montage's first frame. The
// scene skips it when it would still be playing as the finale starts (one I2S port).
void playOpener();
uint32_t openerMs();                // 0 when there's no opener clip
// The S3's boot intro video's sound (data/intro.pcm, 48 kHz, git-ignored, may be empty), started
// by ui/intro with its first frame. No-op on the CYD.
void playIntro();
// Wolf howl for the phone remote (net/remote): on the S3 a real recording (data/howl.pcm from
// tools/gen_howl.py, git-ignored) when the build has one, else synthesised on the fly (util/howl).
void playHowl();
bool playing();                     // a clip is still playing (its task and I2S driver are alive)
}
