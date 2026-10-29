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
    float recentKmL = NAN;          // km/L over roughly the last 50 km: see learn()
};

// A full tank: the burn starts again from 0. A price inside PRICE_MIN..PRICE_MAX replaces the old one;
// NAN (none said) or anything outside the range leaves it unchanged.
void fillUp(Book* b, float pricePerL);
// Litres used. NAN and negative amounts are ignored.
void burn(Book* b, float litres);
// A driven chunk (a trip, or part of one): km and the litres it burnt. recentKmL follows a km-weighted
// moving average that approximates "the last LEARN_KM": the chunk weighs km / LEARN_KM, capped at 1,
// so a 50 km trip replaces the old value and a 5 km hop moves it 10 %. The first chunk sets it.
// Zero, negative or non-finite km or litres are ignored.
constexpr float LEARN_KM = 50.0f;
void learn(Book* b, float km, float litres);
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
// A price outside PRICE_MIN..PRICE_MAX or non-finite reads as NAN (none), and so does a non-finite or
// non-positive recentKmL: a bad old record must not show a wrong cost or range.
bool decode(const uint8_t* in, size_t len, Book* b);

// Siri-dictated text, any case and punctuation, unknown words skipped. A fill-up phrase has "filled",
// "fill", "abasteci" or "abastecido", or the pair "full"+"tank" / "tanque"+"cheio". The price is the first
// number in the text ('.' or ',' decimal, up to two decimals); *price = NAN when none was said, and
// *priceOk = false when one was said outside PRICE_MIN..PRICE_MAX ("649"). Returns false for any other text.
bool parsePhrase(const char* text, float* price, bool* priceOk);

// The yellow deck forwards a fill-up phrase as GET /fill: p is the price as "6.49", sent only when one
// was heard inside the range (priceOk and not NAN). False (out "") when there is no p to send.
bool priceParam(float price, bool priceOk, char* out, size_t n);
// The spoken reply to the phrase, from the OBD deck's /fill answer (nullptr: no answer):
//   "ok 6.49"     -> "Full tank noted, 6.49 per litre."
//   "ok none"     -> "Full tank noted.", or, when a price was heard outside the range (`heard` not NAN,
//                    !priceOk), "Full tank noted. I heard 649, that's not a price." (up to two
//                    decimals, no trailing zeros)
//   "refused 649" -> "Full tank noted. I heard 649, that's not a price." (the number it sent back)
//   anything else -> "I can't reach the car module."
void fillReply(const char* answer, float heard, bool priceOk, char* out, size_t n);
}
