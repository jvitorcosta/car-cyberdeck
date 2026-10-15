#pragma once
#include <cstdint>
// Boot greeting: plays the embedded clip (data/greeting.pcm, 16-bit mono 16 kHz) once on the
// CYD SPEAK connector (GPIO 26, built-in DAC through I2S0). Runs in its own short-lived task,
// so boot carries on while it plays; the task frees the I2S driver and itself when done.
namespace greeting {
void play();
uint32_t durationMs();          // clip length (16 kHz)
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
// Wolf howl synthesised on the fly (util/howl), for the phone remote (net/remote).
void playHowl();
bool playing();                     // a clip is still playing (its task and I2S driver are alive)
}
