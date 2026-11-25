#include <unity.h>
#include <cmath>
#include "../../src/util/car/tripbook.h"

using tripbook::Meter;
using tripbook::Totals;

void setUp() {}
void tearDown() {}

void test_seconds_count_only_while_live() {
    Meter m;
    fuelbook::Book b;
    const Totals t{0, 0, 0};
    m.tick(1000, true, t, &b);              // the first call only sets the clock
    for (uint32_t ms = 1500; ms <= 11000; ms += 500) m.tick(ms, true, t, &b);
    TEST_ASSERT_EQUAL_UINT32(10, m.seconds());
    for (uint32_t ms = 11500; ms <= 21000; ms += 500) m.tick(ms, false, t, &b);   // engine off
    TEST_ASSERT_EQUAL_UINT32(10, m.seconds());
    m.tick(21500, true, t, &b);
    m.tick(81500, true, t, &b);             // a 60 s stall counts as MAX_GAP_MS
    TEST_ASSERT_EQUAL_UINT32(13, m.seconds());
}

void test_litres_are_charged_every_30_s() {
    Meter m;
    fuelbook::Book b;
    fuelbook::fillUp(&b, 6.49f);
    TEST_ASSERT_FALSE(m.tick(0, true, Totals{0, 0, 0}, &b));
    TEST_ASSERT_FALSE(m.tick(29000, true, Totals{0.3f, 0.04f, 30}, &b));   // not yet
    TEST_ASSERT_TRUE(m.tick(30000, true, Totals{0.3f, 0.05f, 31}, &b));
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.05f, b.litresSinceFull);
    TEST_ASSERT_TRUE(m.tick(60000, true, Totals{0.6f, 0.08f, 60}, &b));    // only the delta
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.08f, b.litresSinceFull);
}

void test_nothing_counted_charges_nothing() {
    Meter m;
    fuelbook::Book b;
    fuelbook::fillUp(&b, NAN);
    m.tick(0, true, Totals{0, 0, 7}, &b);
    TEST_ASSERT_FALSE(m.tick(30000, false, Totals{0, 0, 7}, &b));   // no air step since
    TEST_ASSERT_EQUAL_FLOAT(0.0f, b.litresSinceFull);
}

void test_km_l_is_learnt_after_a_kilometre() {
    Meter m;
    fuelbook::Book b;
    m.tick(0, true, Totals{0, 0, 0}, &b);
    m.tick(30000, true, Totals{0.4f, 0.05f, 30}, &b);   // a crawl: charged, not learnt
    TEST_ASSERT_TRUE(std::isnan(b.recentKmL));
    m.tick(60000, true, Totals{1.2f, 0.10f, 60}, &b);   // 1.2 km over 0.10 L since the start
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 12.0f, b.recentKmL);
    m.tick(90000, true, Totals{1.5f, 0.12f, 90}, &b);   // 0.3 km more: waits again
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 12.0f, b.recentKmL);
}

void test_range_follows_once_full_and_learnt() {
    Meter m;
    fuelbook::Book b;
    fuelbook::fillUp(&b, 6.0f);
    TEST_ASSERT_TRUE(std::isnan(fuelbook::rangeKm(b)));
    m.tick(0, true, Totals{0, 0, 0}, &b);
    m.tick(30000, true, Totals{2.0f, 0.2f, 30}, &b);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, (44.0f - 0.2f) * 10.0f, fuelbook::rangeKm(b));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.2f, fuelbook::costRs(b, 0.2f));
}

void test_a_clock_wrap_still_charges() {
    Meter m;
    fuelbook::Book b;
    m.tick(0xFFFFF000u, true, Totals{0, 0, 0}, &b);
    for (uint32_t ms = 0xFFFFF000u + 1000; ms != 0xFFFFF000u + 30000; ms += 1000)   // across 0
        TEST_ASSERT_FALSE(m.tick(ms, true, Totals{0, 0.01f, 1}, &b));
    TEST_ASSERT_TRUE(m.tick(0xFFFFF000u + 30000, true, Totals{0, 0.02f, 2}, &b));
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.02f, b.litresSinceFull);
    TEST_ASSERT_EQUAL_UINT32(30, m.seconds());
}

void test_a_fill_up_charges_the_old_tank_first() {
    Meter m;
    fuelbook::Book b;
    fuelbook::fillUp(&b, 6.0f);
    m.tick(0, true, Totals{0, 0, 0}, &b);
    m.tick(30000, true, Totals{2.0f, 0.2f, 30}, &b);
    m.tick(45000, true, Totals{3.0f, 0.3f, 45}, &b);    // 0.1 L not charged yet
    TEST_ASSERT_TRUE(m.charge(45000, Totals{3.0f, 0.3f, 45}, &b));
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.3f, b.litresSinceFull);
    fuelbook::fillUp(&b, 6.2f);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, b.litresSinceFull);
    TEST_ASSERT_FALSE(m.tick(74000, true, Totals{3.5f, 0.35f, 74}, &b));   // the next 30 s from the fill
    TEST_ASSERT_TRUE(m.tick(75000, true, Totals{3.5f, 0.35f, 75}, &b));
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.05f, b.litresSinceFull);   // only what came after it
}

void test_bad_totals_charge_nothing() {
    Meter m;
    fuelbook::Book b;
    m.tick(0, true, Totals{0, 0, 0}, &b);
    m.tick(30000, true, Totals{1.5f, 0.1f, 30}, &b);
    TEST_ASSERT_FALSE(m.tick(60000, true, Totals{NAN, 0.2f, 60}, &b));
    TEST_ASSERT_FALSE(m.tick(90000, true, Totals{1.6f, INFINITY, 90}, &b));
    TEST_ASSERT_FALSE(m.tick(120000, true, Totals{1.6f, 0.05f, 120}, &b));   // a total that went down
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.1f, b.litresSinceFull);
}

void test_idle_fuel_counts_in_the_learnt_km_l() {
    Meter m;
    fuelbook::Book b;
    m.tick(0, true, Totals{0, 0, 0}, &b);
    m.tick(30000, true, Totals{0, 0.1f, 30}, &b);       // idling: charged, waits for km
    TEST_ASSERT_TRUE(std::isnan(b.recentKmL));
    m.tick(60000, true, Totals{1.5f, 0.2f, 60}, &b);    // 1.5 km over the 0.2 L of both chunks
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 7.5f, b.recentKmL);
}

void test_parse_price() {
    float p = -1.0f;
    TEST_ASSERT_TRUE(tripbook::parsePrice("6.49", &p));
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 6.49f, p);
    TEST_ASSERT_TRUE(tripbook::parsePrice(" 6,5 ", &p));
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 6.5f, p);
    TEST_ASSERT_TRUE(tripbook::parsePrice("7", &p));
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 7.0f, p);
    TEST_ASSERT_TRUE(tripbook::parsePrice("15.00", &p));
    const char* bad[] = {"", " ", "649", "2.99", "15.01", "6.499", "6.", ".5", "6.4x", "-6", "6..4", "6,4.9",
                         "abc", "1e1"};
    for (const char* s : bad) {
        float q = 1.0f;
        TEST_ASSERT_FALSE_MESSAGE(tripbook::parsePrice(s, &q), s);
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE(1.0f, q, s);
    }
    TEST_ASSERT_FALSE(tripbook::parsePrice(nullptr, &p));
}

// /fill on the black deck takes its price through parsePrice (remote.cpp handleFill); the OBD deck's own
// copy of the handler (src/obd/main.cpp) uses strtof and is not host-testable (F-09). What parsePrice
// refuses is exactly what strtof lets through, so the contract is pinned here: a comma works, and
// nothing strtof reads that is not "R$ 3.00 to 15.00, at most two decimals" is a price.
void test_parse_price_refuses_what_strtof_accepts() {
    float p = 1.0f;
    TEST_ASSERT_TRUE(tripbook::parsePrice("6,49", &p));
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 6.49f, p);
    const char* strtofOnly[] = {"inf", "-inf", "nan", "NAN", "infinity", "6.5e0", "0x7", "0x1.cp2", "+6.5",
                                "6.49abc", "6.490", "6.49 7"};
    for (const char* s : strtofOnly) {
        float q = 1.0f;
        TEST_ASSERT_FALSE_MESSAGE(tripbook::parsePrice(s, &q), s);
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE(1.0f, q, s);
    }
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_seconds_count_only_while_live);
    RUN_TEST(test_litres_are_charged_every_30_s);
    RUN_TEST(test_nothing_counted_charges_nothing);
    RUN_TEST(test_km_l_is_learnt_after_a_kilometre);
    RUN_TEST(test_range_follows_once_full_and_learnt);
    RUN_TEST(test_a_clock_wrap_still_charges);
    RUN_TEST(test_a_fill_up_charges_the_old_tank_first);
    RUN_TEST(test_bad_totals_charge_nothing);
    RUN_TEST(test_idle_fuel_counts_in_the_learnt_km_l);
    RUN_TEST(test_parse_price);
    RUN_TEST(test_parse_price_refuses_what_strtof_accepts);
    return UNITY_END();
}
