#pragma once
#include <cstddef>
#include <cstdint>
// The music-box tune set (data/tunes.txt from tools/tune_clip.py): plain text, read in place
// from flash, nothing copied. A "tune <name>" line starts a tune; then pairs of a note and its
// length in ms, as many per line as you like ("E5 250 R 125 F#5 500"); # starts a comment.
// Notes: C..B, an optional # or b, octave 0-8 (A4 = 440 Hz); R is a rest. Bad pairs are
// skipped. PURE, host-tested.
namespace tunes {
constexpr int REST = -1;
constexpr uint16_t MAX_MS = 10000;
struct Note { int midi; uint16_t ms; };   // midi: MIDI note number (A4 = 69), or REST

int count(const char* text, size_t len);   // tunes with at least one note
// The name of tune i into out (truncated); false when there's no tune i.
bool name(const char* text, size_t len, int i, char* out, size_t n);

class Reader {
public:
    Reader(const char* text, size_t len, int tune);   // tune: 0..count()-1
    bool next(Note* n);                                // false at the tune's end
private:
    const char* p_;
    const char* end_;
};
}
