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
// The WiFi driver's ~1.5 KB receive buffers fail during almost every TLS request and recover by
// themselves (BACKLOG "Dropped WiFi receive buffers"); only failures this big mean starvation.
constexpr size_t BIG_FAIL = 4096;
uint32_t bigFailTotal();       // failed allocations of BIG_FAIL bytes or more since boot (monotonic)
void installFailHook();        // counts failed allocations; log() reports and resets the count
}
