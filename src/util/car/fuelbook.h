#pragma once
#include <cmath>
#include <cstddef>
#include <cstdint>
// The fuel book: has the tank been filled to the top since the car was last forgotten, how many litres
// went since, the last price, and the range that follows. There is no fuel-level PID on this car, so the
// range is "tank minus what the trip logs burned since the fill-up, times the recent km/L". Also the
// spoken fill-up phrase and the NVS record. PURE, host-tested.
namespace fuelbook {
constexpr float TANK_L = 44.0f, PRICE_MIN = 3.0f, PRICE_MAX = 15.0f;
struct Book {
    bool hasFull = false;           // false until a fill-up was said: no fill-up, no range
    float litresSinceFull = 0;
    float pricePerL = NAN;          // R$/L, NAN until one was said
    float recentKmL = NAN;          // kept by the caller from the trip logs
};

// A full tank: the burn starts again from 0. A price inside PRICE_MIN..PRICE_MAX replaces the old one;
// NAN (none said) or anything outside the range leaves it unchanged.
void fillUp(Book* b, float pricePerL);
// Litres used. NAN and negative amounts are ignored.
void burn(Book* b, float litres);
// Km left: (TANK_L - litresSinceFull) * recentKmL, never below 0. NAN unless hasFull and recentKmL is > 0.
float rangeKm(const Book& b);
// What `litres` cost at the book's price. NAN without a price or with NAN litres.
float costRs(const Book& b, float litres);

// NVS "fuel": flags u8 (bit 0 hasFull), then litresSinceFull, pricePerL, recentKmL and one reserved
// float (written as 0, ignored on read), all float32 little-endian. The spare lets the record grow
// without a format change.
constexpr size_t RECORD_BYTES = 17;
void encode(const Book& b, uint8_t out[RECORD_BYTES]);
// False on a wrong length, unknown flag bits, or negative / non-finite litres; *b is then untouched.
bool decode(const uint8_t* in, size_t len, Book* b);

// Siri-dictated text, any case and punctuation, unknown words skipped. A fill-up phrase has "filled",
// "fill", "abasteci" or "abastecido", or the pair "full"+"tank" / "tanque"+"cheio". The price is the first
// number in the text ('.' or ',' decimal, up to two decimals); *price = NAN when none was said, and
// *priceOk = false when one was said outside PRICE_MIN..PRICE_MAX ("649"). Returns false for any other text.
bool parsePhrase(const char* text, float* price, bool* priceOk);
}
