#include <unity.h>
#include <cmath>
#include <cstdint>
#include <cstring>
#include "../../src/util/car/fuelbook.h"

void setUp() {}
void tearDown() {}

void test_no_fill_up_means_no_range() {
    fuelbook::Book b;
    b.recentKmL = 12.0f;
    TEST_ASSERT_TRUE(std::isnan(fuelbook::rangeKm(b)));
    fuelbook::burn(&b, 5.0f);
    TEST_ASSERT_TRUE(std::isnan(fuelbook::rangeKm(b)));
}

void test_range_needs_a_known_economy() {
    fuelbook::Book b;
    fuelbook::fillUp(&b, 6.49f);
    TEST_ASSERT_TRUE(std::isnan(fuelbook::rangeKm(b)));
    b.recentKmL = 0.0f;
    TEST_ASSERT_TRUE(std::isnan(fuelbook::rangeKm(b)));
}

void test_range_after_fill_and_burn() {
    fuelbook::Book b;
    fuelbook::fillUp(&b, 6.49f);
    b.recentKmL = 12.0f;
    fuelbook::burn(&b, 10.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 408.0f, fuelbook::rangeKm(b));
}

void test_range_never_negative() {
    fuelbook::Book b;
    fuelbook::fillUp(&b, 6.49f);
    b.recentKmL = 12.0f;
    fuelbook::burn(&b, 50.0f);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, fuelbook::rangeKm(b));
}

void test_burn_ignores_bad_litres() {
    fuelbook::Book b;
    fuelbook::fillUp(&b, 6.49f);
    fuelbook::burn(&b, NAN);
    fuelbook::burn(&b, -3.0f);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, b.litresSinceFull);
}

void test_fill_up_resets_the_burn_and_keeps_price_rules() {
    fuelbook::Book b;
    fuelbook::fillUp(&b, 6.49f);
    fuelbook::burn(&b, 20.0f);
    fuelbook::fillUp(&b, NAN);  // full tank, no price said: price unchanged
    TEST_ASSERT_TRUE(b.hasFull);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, b.litresSinceFull);
    TEST_ASSERT_EQUAL_FLOAT(6.49f, b.pricePerL);
    fuelbook::fillUp(&b, 649.0f);  // out of range: ignored too
    TEST_ASSERT_EQUAL_FLOAT(6.49f, b.pricePerL);
    fuelbook::fillUp(&b, 5.99f);
    TEST_ASSERT_EQUAL_FLOAT(5.99f, b.pricePerL);
}

void test_cost_uses_price() {
    fuelbook::Book b;
    TEST_ASSERT_TRUE(std::isnan(fuelbook::costRs(b, 2.0f)));
    fuelbook::fillUp(&b, 6.49f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 12.98f, fuelbook::costRs(b, 2.0f));
    TEST_ASSERT_TRUE(std::isnan(fuelbook::costRs(b, NAN)));
}

static void phrase(const char* t, bool expectFill, float expectPrice, bool expectOk) {
    float p = 123.0f;
    bool ok = !expectOk;
    TEST_ASSERT_EQUAL_MESSAGE(expectFill, fuelbook::parsePhrase(t, &p, &ok), t);
    if (!expectFill) return;
    TEST_ASSERT_EQUAL_MESSAGE(expectOk, ok, t);
    if (std::isnan(expectPrice)) TEST_ASSERT_TRUE_MESSAGE(std::isnan(p), t);
    else TEST_ASSERT_FLOAT_WITHIN(0.001f, expectPrice, p);
}

void test_phrase_english_and_portuguese() {
    phrase("Filled up at 6.49.", true, 6.49f, true);
    phrase("fill up 6,49", true, 6.49f, true);
    phrase("tanque cheio a 6,49", true, 6.49f, true);
    phrase("Abasteci a 5,9 reais", true, 5.9f, true);
    phrase("filled up at 6", true, 6.0f, true);
}

void test_phrase_full_tank_without_price() {
    phrase("full tank", true, NAN, true);
    phrase("Tanque cheio.", true, NAN, true);
    phrase("filled up", true, NAN, true);
}

void test_price_out_of_range_refused() {
    phrase("filled up at 649", true, 649.0f, false);
    phrase("filled up at 2.50", true, 2.5f, false);
    phrase("filled up at 15,01", true, 15.01f, false);
    phrase("filled up at 3.00", true, 3.0f, true);
    phrase("filled up at 15", true, 15.0f, true);
}

void test_not_a_fill_phrase() {
    phrase("purple inside", false, NAN, true);
    phrase("", false, NAN, true);
    phrase("full volume", false, NAN, true);
    phrase("empty tank 6.49", false, NAN, true);
    TEST_ASSERT_FALSE(fuelbook::parsePhrase(nullptr, nullptr, nullptr));
}

void test_record_roundtrip_and_bad_length() {
    fuelbook::Book b;
    fuelbook::fillUp(&b, 6.49f);
    b.litresSinceFull = 12.5f;
    b.recentKmL = 11.2f;
    uint8_t buf[fuelbook::RECORD_BYTES];
    fuelbook::encode(b, buf);
    TEST_ASSERT_EQUAL_UINT8(1, buf[0]);
    fuelbook::Book r;
    TEST_ASSERT_TRUE(fuelbook::decode(buf, sizeof buf, &r));
    TEST_ASSERT_TRUE(r.hasFull);
    TEST_ASSERT_EQUAL_FLOAT(12.5f, r.litresSinceFull);
    TEST_ASSERT_EQUAL_FLOAT(6.49f, r.pricePerL);
    TEST_ASSERT_EQUAL_FLOAT(11.2f, r.recentKmL);

    // unknowns (NAN) survive the round trip
    fuelbook::Book e;
    fuelbook::encode(e, buf);
    TEST_ASSERT_TRUE(fuelbook::decode(buf, sizeof buf, &r));
    TEST_ASSERT_FALSE(r.hasFull);
    TEST_ASSERT_TRUE(std::isnan(r.pricePerL));
    TEST_ASSERT_TRUE(std::isnan(r.recentKmL));

    TEST_ASSERT_FALSE(fuelbook::decode(buf, fuelbook::RECORD_BYTES - 1, &r));
    TEST_ASSERT_FALSE(fuelbook::decode(buf, fuelbook::RECORD_BYTES + 1, &r));
    TEST_ASSERT_FALSE(fuelbook::decode(nullptr, fuelbook::RECORD_BYTES, &r));
    buf[0] = 2;
    TEST_ASSERT_FALSE(fuelbook::decode(buf, sizeof buf, &r));
}

void test_decode_rejects_corrupt_litres() {
    fuelbook::Book b;
    b.litresSinceFull = -1.0f;
    uint8_t buf[fuelbook::RECORD_BYTES];
    fuelbook::encode(b, buf);
    fuelbook::Book r;
    TEST_ASSERT_FALSE(fuelbook::decode(buf, sizeof buf, &r));
}

// A record from an older build or a bad flash read: a price outside the accepted range or a non-finite
// one reads as "no price"; a non-finite or non-positive economy reads as "unknown".
void test_decode_sanitizes_price_and_economy() {
    fuelbook::Book b;
    fuelbook::fillUp(&b, 6.49f);
    uint8_t buf[fuelbook::RECORD_BYTES];
    fuelbook::Book r;
    const float badPrices[] = {2.99f, 15.01f, INFINITY, -6.0f};
    for (float p : badPrices) {
        b.pricePerL = p;
        b.recentKmL = 11.0f;
        fuelbook::encode(b, buf);
        TEST_ASSERT_TRUE(fuelbook::decode(buf, sizeof buf, &r));
        TEST_ASSERT_TRUE(std::isnan(r.pricePerL));
        TEST_ASSERT_EQUAL_FLOAT(11.0f, r.recentKmL);
    }
    b.pricePerL = 3.0f;  // the ends are in range
    fuelbook::encode(b, buf);
    TEST_ASSERT_TRUE(fuelbook::decode(buf, sizeof buf, &r));
    TEST_ASSERT_EQUAL_FLOAT(3.0f, r.pricePerL);
    const float badKmL[] = {0.0f, -4.0f, INFINITY};
    for (float k : badKmL) {
        b.pricePerL = 6.49f;
        b.recentKmL = k;
        fuelbook::encode(b, buf);
        TEST_ASSERT_TRUE(fuelbook::decode(buf, sizeof buf, &r));
        TEST_ASSERT_TRUE(std::isnan(r.recentKmL));
        TEST_ASSERT_EQUAL_FLOAT(6.49f, r.pricePerL);
    }
}

void test_learn_first_chunk_sets_economy() {
    fuelbook::Book b;
    fuelbook::learn(&b, 2.0f, 0.2f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 10.0f, b.recentKmL);
}

void test_learn_fifty_km_replaces_it() {
    fuelbook::Book b;
    b.recentKmL = 10.0f;
    fuelbook::learn(&b, 50.0f, 2.5f);  // 20 km/L
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 20.0f, b.recentKmL);
    fuelbook::learn(&b, 120.0f, 10.0f);  // longer than 50 km still weighs 1
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 12.0f, b.recentKmL);
}

void test_learn_five_km_moves_it_ten_percent() {
    fuelbook::Book b;
    b.recentKmL = 10.0f;
    fuelbook::learn(&b, 5.0f, 0.25f);  // 20 km/L, weight 0.1
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 11.0f, b.recentKmL);
}

void test_learn_ignores_bad_input() {
    fuelbook::Book b;
    b.recentKmL = 10.0f;
    fuelbook::learn(&b, 0.0f, 1.0f);
    fuelbook::learn(&b, -5.0f, 1.0f);
    fuelbook::learn(&b, 5.0f, 0.0f);
    fuelbook::learn(&b, 5.0f, -1.0f);
    fuelbook::learn(&b, NAN, 1.0f);
    fuelbook::learn(&b, 5.0f, NAN);
    fuelbook::learn(&b, INFINITY, 1.0f);
    fuelbook::learn(&b, 5.0f, INFINITY);
    fuelbook::learn(nullptr, 5.0f, 0.5f);
    TEST_ASSERT_EQUAL_FLOAT(10.0f, b.recentKmL);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_no_fill_up_means_no_range);
    RUN_TEST(test_range_needs_a_known_economy);
    RUN_TEST(test_range_after_fill_and_burn);
    RUN_TEST(test_range_never_negative);
    RUN_TEST(test_burn_ignores_bad_litres);
    RUN_TEST(test_fill_up_resets_the_burn_and_keeps_price_rules);
    RUN_TEST(test_cost_uses_price);
    RUN_TEST(test_phrase_english_and_portuguese);
    RUN_TEST(test_phrase_full_tank_without_price);
    RUN_TEST(test_price_out_of_range_refused);
    RUN_TEST(test_not_a_fill_phrase);
    RUN_TEST(test_record_roundtrip_and_bad_length);
    RUN_TEST(test_decode_rejects_corrupt_litres);
    RUN_TEST(test_decode_sanitizes_price_and_economy);
    RUN_TEST(test_learn_first_chunk_sets_economy);
    RUN_TEST(test_learn_fifty_km_replaces_it);
    RUN_TEST(test_learn_five_km_moves_it_ten_percent);
    RUN_TEST(test_learn_ignores_bad_input);
    return UNITY_END();
}
