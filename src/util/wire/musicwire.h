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
// random start, +1 per packet. The yellow deck learns the boot id from the black deck's /music
// reply, proven with an HMAC over the boot id and the nonce of the yellow deck's own signed request
// (bootProof / readBoot), and takes packets only from that boot, with a counter that rises (Fresh):
// a recorded packet can't be played again, from this boot or an earlier one.
namespace musicwire {
constexpr uint16_t PORT = 4210;
constexpr uint8_t VERSION = 2;
constexpr size_t PAL_LEN = 34;
constexpr size_t TAG_LEN = 8;
struct PalPkt  { uint32_t songId; songpalette::Palette p; };
struct Seq { uint32_t boot; uint32_t counter; };
// The packet's length, or 0 when out has fewer than its bytes, there's no key or the HMAC failed
// (nothing to send then).
size_t encode(const PalPkt& p, Seq s, const char* key, uint8_t* out, size_t n);
// False on a wrong magic or version, a length other than the packet's, no key, a bad tag, or an
// HMAC that couldn't be computed.
bool decode(const uint8_t* in, size_t n, const char* key, PalPkt* p, Seq* s);

// The black deck's proof of its boot id for the /music reply: the first 8 bytes of
// HMAC-SHA256(key, "music-boot|<boot>|<nonce>"), as 16 hex digits, nonce the request's n (16 hex).
// False (out "") without a key or nonce, or when the HMAC failed.
constexpr size_t PROOF_HEX = 16;
bool bootProof(const char* key, uint32_t boot, const char* nonce, char out[PROOF_HEX + 1]);
// From a /music reply body ({"on":1,"boot":<u32>,"proof":"<16 hex>"}): the boot id, when its proof
// matches the nonce of the request it answers (compared in constant time). False otherwise.
bool readBoot(const char* body, const char* key, const char* nonce, uint32_t* boot);

// The yellow deck's replay guard for its one sender: a packet is fresh when it comes from the boot
// the black deck proved last (known; 0 = none yet: nothing is) and its counter is above the last
// fresh one's. reset() when the sender changes.
class Fresh {
public:
    bool accept(Seq s, uint32_t known) {
        if (known == 0 || s.boot != known) return false;
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
