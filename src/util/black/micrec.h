#pragma once
#include <cstddef>
#include <cstdint>
// The car mic test's recorder (micrec_s3.cpp): ten Portuguese phrases recorded in four
// conditions, 40 clip slots in PSRAM, served as WAV (docs/superpowers/plans/
// 2026-10-23-voice-agent-1-mic-test.md). Slot i = condition i / 10, phrase i % 10.
namespace micrec {
constexpr int RATE = 16000;    // the ES8311 mic, 16-bit mono
constexpr int MAX_MS = 4000;   // a longer hold is cut here
constexpr int MIN_MS = 300;    // a shorter press is a tap: no clip
constexpr int SLOTS = 40;
constexpr int PHRASES = 10;
constexpr int MAX_SAMPLES = MAX_MS * RATE / 1000;

const char* phrase(int p);     // P1-P10 for p 0-9, "" out of range
const char* conditionText(int slot);  // "Spotify pausado, vidros fechados", "" out of range
char condition(int slot);      // 'A'-'D', '?' out of range
int phraseOf(int slot);        // slot % 10, -1 out of range
// Canonical 44-byte RIFF/WAVE header for 16-bit mono PCM.
void wavHeader(uint8_t out[44], uint32_t dataBytes, uint32_t rate);
// Samples a hold of heldMs keeps: 0 below MIN_MS, else capped at MAX_MS.
int samplesToKeep(uint32_t heldMs);
void clipName(int slot, char* out, size_t n);  // "A01".."D10"

// Hands-free run from boot: phrase by phrase, a 3-2-1 countdown, a beep, MAX_MS of recording, a
// short rest, the next phrase; BREAK_MS between conditions to change the music and windows.
constexpr int COUNT_MS = 3000;
constexpr int GAP_MS = 700;
constexpr int START_MS = 5000;    // after boot, before the first phrase
constexpr int BREAK_MS = 20000;   // before conditions B, C and D
bool breakBefore(int slot);       // true at the first phrase of B, C and D
// A screen tap while paused, by thirds of the 320-px width; any tap while running pauses.
enum class Tap { Back, Resume, Skip };
Tap tapAt(int x);
enum class Phase { Countdown, Record, Gap, Done };
struct Step {
    Phase phase;
    int count;   // 3, 2, 1 during the countdown, else 0
};
Step stepAt(uint32_t msIntoPhrase);  // where a phrase is, ms after it started
int runEnd(int slot);                // the slot a run started at `slot` stops before
}
