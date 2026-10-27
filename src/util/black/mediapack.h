#pragma once
#include <cstddef>
#include <cstdint>
// The S3's media pack (media.bin, from tools/pack_media.py; flashed to the "media" partition):
// a 16-byte header ("CDMEDIA1", u16 version, u16 entry count, u32 pack size), then one 32-byte
// entry per file (name NUL-padded to 20, u32 offset from the pack's start, u32 size, u32 CRC-32),
// then the files, 4-byte aligned. Little-endian. PURE, host-tested.
namespace mediapack {
constexpr size_t HEAD = 16, ENTRY = 32, NAME = 20;
constexpr uint16_t VERSION = 1;
// The pack's size from its first HEAD bytes, or 0 when they are not a pack this firmware reads.
uint32_t total(const uint8_t* head, size_t len);
int count(const uint8_t* pack, size_t len);   // entries, or 0 when the header is bad
struct File { const uint8_t* data; size_t size; uint32_t crc; };
// Entry i of a whole pack; false when it points outside the pack.
bool at(const uint8_t* pack, size_t len, int i, File* out);
// The entry named name; -1 when the pack has none (or is bad).
int find(const uint8_t* pack, size_t len, const char* name);
uint32_t crc32(const uint8_t* p, size_t n);   // zlib's CRC-32 (Python's zlib.crc32)
}
