#pragma once
#include <cstddef>
#include <cstdint>
// HMAC-SHA256 (RFC 2104), one call. On the decks mbedTLS (mbedtls_md_hmac); on the host a portable
// SHA-256, so the suites can check it against RFC 4231's test vectors (test_hmac256). selfTest()
// runs RFC 4231 test case 2 through whichever build this is: the decks call it once at boot and
// sign nothing (and accept nothing signed) if it fails.
namespace hmac256 {
constexpr size_t LEN = 32;
void mac(const uint8_t* key, size_t keyLen, const uint8_t* msg, size_t msgLen, uint8_t out[LEN]);
bool selfTest();
// Equal bytes, in time that depends only on n (never on where they differ).
bool same(const uint8_t* a, const uint8_t* b, size_t n);
}
