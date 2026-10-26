#pragma once
#include <cstddef>
#include <cstdint>
#include "songpalette.h"
// Music mode's UDP packet, black deck -> yellow deck (one packet per datagram, latest wins). PURE,
// host-tested. Byte by byte, little-endian, never a memcpy of a struct:
//   PalPkt, 18 bytes:  'M' 'P' 1 | songId u32 | c[0] r g b | c[1] r g b | c[2] r g b | cap | depth
namespace musicwire {
constexpr uint16_t PORT = 4210;
constexpr uint8_t VERSION = 1;
constexpr size_t PAL_LEN = 18;
struct PalPkt  { uint32_t songId; songpalette::Palette p; };
// The packet's length, or 0 when out has fewer than its bytes.
size_t encode(const PalPkt& p, uint8_t* out, size_t n);
// False on a wrong magic or version, or a length other than the packet's.
bool decode(const uint8_t* in, size_t n, PalPkt* p);
}
