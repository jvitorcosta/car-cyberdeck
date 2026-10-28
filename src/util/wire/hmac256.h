#pragma once
#include <cstddef>
#include <cstdint>
// HMAC-SHA256 (RFC 2104), one call. A portable SHA-256 (FIPS 180-4) on the decks and the host alike,
// so the code the suites check against RFC 4231's vectors (test_hmac256) is the code that runs; it
// uses no heap (mbedtls_md_hmac allocates its context, and the black deck's heap runs short).
// selfTest() runs RFC 4231 test case 2: the decks call it once at boot and sign nothing (and accept
// nothing signed) if it fails.
// mac() returns false when it couldn't compute the tag; out is then unusable, and every caller
// treats that as a refusal (never as a tag to compare against).
namespace hmac256 {
constexpr size_t LEN = 32;
bool mac(const uint8_t* key, size_t keyLen, const uint8_t* msg, size_t msgLen, uint8_t out[LEN]);
bool selfTest();
// Equal bytes, in time that depends only on n (never on where they differ).
bool same(const uint8_t* a, const uint8_t* b, size_t n);
#ifndef ARDUINO
// Host tests only: while on, mac() fails (the fail-closed paths of sigreq and musicwire).
void failForTest(bool on);
#endif
}
