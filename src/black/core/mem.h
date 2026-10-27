#pragma once
#include <stddef.h>
#include <stdint.h>
// Byte-addressable internal RAM — what byte buffers, mbedTLS and the WiFi driver can use.
// ESP.getFreeHeap()/getMaxAllocHeap() also count 32-bit-only IRAM and read ~60 KB too high;
// that hid an exhausted heap (README "Design & performance history").
namespace mem {
size_t byteFree();
size_t byteLargest();
void log(const char* where);   // "[mem] <where> free=... largest=... [allocfail=N (last ...)]"
uint32_t failTotal();          // failed allocations since boot (monotonic)
void installFailHook();        // counts failed allocations; log() reports and resets the count
// A big buffer that the CPU alone touches (bitmaps, download and decode buffers): in PSRAM when
// the board has it (the S3's 8 MB), else internal RAM. Keeps internal RAM, the only kind
// mbedTLS and the WiFi driver can use, for them. free() releases it.
void* bigAlloc(size_t n);
}
