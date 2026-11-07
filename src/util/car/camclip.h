#pragma once
#include <cstddef>
#include <cstdint>
// The camera alerts' voice clips: data/camvoice.bin (tools/gen_camvoice.py, in the media pack) is
// a slot table, then 16 kHz mono s16le PCM. Header: "CAMV", u16 version, u16 slot count; then per
// slot u32 offset from the file's start and u32 size in bytes (0 = an empty slot). Little-endian.
// The clips stay where they are (the memory-mapped media pack): the table only points into it.
// Reader plays one out at the output's rate (linear interpolation when that isn't 16 kHz).
// PURE, host-tested.
namespace camclip {
// The slots, in the file's order (tools/gen_camvoice.py SLOTS).
enum class Slot : uint8_t {
    Pon,          // the chime
    GpsFirst,     // the greeting set's GPS clip: the first fix after power-on
    Torishimari,  // 取り締まりポイントです (speed camera)
    Gochui,       // ご注意ください (bump, red light)
    SpdChange,    // 制限速度が変わったよ
    Speedni,      // スピードに気をつけてね (the alarm)
    GpsNg,        // GPSがみつかりません
    GpsOk,        // GPSが測位できました
    L20, L30, L40, L50, L60, L80,   // 制限速度XXkm以下です
    COUNT
};
constexpr int COUNT = (int)Slot::COUNT;
constexpr int RATE = 16000;
constexpr size_t HEAD = 8, ENTRY = 8;
constexpr uint16_t VERSION = 1;

struct Clip { const uint8_t* pcm; size_t samples; };   // pcm: s16le, no alignment guarantee

// The limit line's slot for a limit (km/h), or Slot::COUNT when that limit has none.
Slot limitSlot(uint8_t limit);
uint32_t lengthMs(const Clip& c);

class Table {
public:
    // False (and every slot empty) when the blob is no table this firmware reads. A slot whose
    // bytes fall outside the blob, inside the header or are an odd count reads as empty.
    bool load(const uint8_t* blob, size_t len);
    Clip get(Slot s) const;
    uint16_t present() const;   // bit i: slot i has sound
private:
    Clip clips_[COUNT] = {};
};

// One clip out at `rate` Hz, `volume` percent.
class Reader {
public:
    Reader(const Clip& c, int rate, int volume);
    size_t fill(int16_t* out, size_t n);   // samples written; 0 once done
    bool done() const { return i_ >= c_.samples; }
private:
    int16_t at(size_t i) const;
    Clip c_;
    uint32_t rate_;
    int volume_;
    size_t i_ = 0;      // the source sample
    uint32_t frac_ = 0; // how far past it, in 1/rate_ steps of a source sample
};
}  // namespace camclip
