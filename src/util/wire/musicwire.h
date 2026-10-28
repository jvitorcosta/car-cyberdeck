#pragma once
#include <cstddef>
#include <cstdint>
#include "songpalette.h"
// Music mode's UDP packet, black deck -> yellow deck (one packet per datagram, latest wins). PURE,
// host-tested. Byte by byte, little-endian, never a memcpy of a struct:
//   PalPkt, 34 bytes:  'M' 'P' 2 | boot u32 | counter u32 | songId u32 | c[0] r g b | c[1] r g b |
//                      c[2] r g b | cap | depth | tag[8]
//   tag: the first 8 bytes of HMAC-SHA256(REMOTE_TOKEN, the 26 bytes before it)
// Signed, because a UDP source address is easy to spoof on the hotspot (anyone there could otherwise
// push colours to the car's outside light). boot: a random id per black-deck boot; counter: from a
// random start, +1 per packet. The yellow deck drops a bad tag and, within one boot, a counter that
// doesn't rise (Fresh), so a recorded packet can't be played again in that boot.
namespace musicwire {
constexpr uint16_t PORT = 4210;
constexpr uint8_t VERSION = 2;
constexpr size_t PAL_LEN = 34;
constexpr size_t TAG_LEN = 8;
struct PalPkt  { uint32_t songId; songpalette::Palette p; };
struct Seq { uint32_t boot; uint32_t counter; };
// The packet's length, or 0 when out has fewer than its bytes or there's no key.
size_t encode(const PalPkt& p, Seq s, const char* key, uint8_t* out, size_t n);
// False on a wrong magic or version, a length other than the packet's, no key or a bad tag.
bool decode(const uint8_t* in, size_t n, const char* key, PalPkt* p, Seq* s);

// The yellow deck's replay guard for its one sender: a packet is fresh when its counter is above
// the last fresh one's from the same boot; a new boot id starts over. reset() when the sender changes.
class Fresh {
public:
    bool accept(Seq s) {
        if (have_ && s.boot == last_.boot && s.counter <= last_.counter) return false;
        have_ = true;
        last_ = s;
        return true;
    }
    void reset() { have_ = false; }

private:
    bool have_ = false;
    Seq last_{};
};
}
