#include <unity.h>
#include <cmath>
#include <cstdint>
#include "../../src/util/car/fueleco.h"

void setUp() {}
void tearDown() {}

static const fueleco::Car CITY{1.497f, 0.85f, 44.0f, 30.0f};

void test_blend_for_e30() {
    TEST_ASSERT_FLOAT_WITHIN(0.02f, 12.92f, fueleco::afr(30));   // linear by mass (final review)
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 14.12f, fueleco::afr(10));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.7582f, fueleco::densityKgL(30));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 14.7f, fueleco::afr(0));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 9.0f, fueleco::afr(100));
}

void test_speed_density_and_maf() {
    TEST_ASSERT_FLOAT_WITHIN(0.2f, 12.3f, fueleco::airGs(NAN, 50, 2000, 26.85f, CITY));   // 300 K
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 12.34f, fueleco::airGs(12.34f, 50, 2000, 27, CITY));  // MAF wins
}

// Review focus 1: a car with neither MAF nor MAP (or no intake temperature).
void test_no_air_reading_gives_nothing() {
    TEST_ASSERT_TRUE(std::isnan(fueleco::airGs(NAN, NAN, 2000, 27, CITY)));
    TEST_ASSERT_TRUE(std::isnan(fueleco::airGs(NAN, 50, 2000, NAN, CITY)));
    fueleco::Trip t;
    for (uint32_t ms = 0; ms < 10000; ms += 250) t.step(ms, 80, NAN, false, 30);
    TEST_ASSERT_TRUE(std::isnan(t.nowKmL()));
    TEST_ASSERT_TRUE(std::isnan(t.avgKmL()));
    TEST_ASSERT_TRUE(std::isnan(t.kmLeft(50, CITY)));
}

// Km driven before the first air reading (the City has no MAF: speed density waits for the
// intake temperature) burn no counted fuel, so they must not count as distance either.
void test_distance_without_air_is_not_counted() {
    fueleco::Trip t;
    for (uint32_t ms = 0; ms <= 60000; ms += 250) t.step(ms, 80, NAN, false, 30);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, t.km());
    for (uint32_t ms = 60250; ms <= 260000; ms += 250) t.step(ms, 80, 12.3f, false, 30);
    TEST_ASSERT_FLOAT_WITHIN(0.3f, 17.7f, t.avgKmL());
}

void test_steady_cruise() {          // 12.3 g/s at 80 km/h, E30: ~17.7 km/l
    fueleco::Trip t;
    for (uint32_t ms = 0; ms <= 200000; ms += 250) t.step(ms, 80, 12.3f, false, 30);
    TEST_ASSERT_FLOAT_WITHIN(0.3f, 17.7f, t.nowKmL());
    TEST_ASSERT_FLOAT_WITHIN(0.3f, 17.7f, t.avgKmL());
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 4.44f, t.km());
    TEST_ASSERT_FLOAT_WITHIN(5.0f, 0.5f * 44 * 17.7f, t.kmLeft(50, CITY));
}

void test_fuel_cut_is_free() {
    fueleco::Trip t;
    for (uint32_t ms = 0; ms <= 60000; ms += 250) t.step(ms, 80, 12.3f, false, 30);
    const float before = t.avgKmL();
    for (uint32_t ms = 60250; ms <= 80000; ms += 250) t.step(ms, 70, 9.0f, true, 30);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 99.9f, t.nowKmL());
    TEST_ASSERT_GREATER_THAN_FLOAT(before, t.avgKmL());
}

// Review focus 3: long idle at 0 km/h.
void test_idle_counts_fuel_not_distance() {
    fueleco::Trip t;
    for (uint32_t ms = 0; ms <= 60000; ms += 250) t.step(ms, 0, 2.5f, false, 30);
    TEST_ASSERT_TRUE(std::isnan(t.nowKmL()));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, t.km());
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, t.avgKmL());      // fuel burnt, no distance: 0 km/l
    TEST_ASSERT_TRUE(std::isnan(t.kmLeft(50, CITY)));
}

// Review focus 4: a long Bluetooth stall, across the millis() wrap.
void test_gap_is_capped_across_the_wrap() {
    fueleco::Trip t;
    t.step(0xFFFFF000u, 100, 15, false, 30);
    t.step(0x00002000u, 100, 15, false, 30);                 // ~12 s later, across the wrap
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 100.0f * 2 / 3600, t.km());   // counted as 2 s
}

// Final review: PID 03 value 4 means "open loop: load OR decel fuel cut", and it's read only every
// ~14 s. Fuel cut needs the pedal released at speed; a fresh fuel-system reading only confirms it.
void test_fuel_cut_needs_the_pedal_released() {
    TEST_ASSERT_FALSE(fueleco::fuelCut(90, 85, 4500, 100, 4, true));   // full throttle, "4": not a cut
    TEST_ASSERT_TRUE(fueleco::fuelCut(0, 1, 1800, 60, NAN, false));     // coasting, no fuel system
    TEST_ASSERT_TRUE(fueleco::fuelCut(0, 1, 1800, 60, 4, true));        // coasting, confirmed
    TEST_ASSERT_FALSE(fueleco::fuelCut(0, 1, 1800, 60, 2, true));       // closed loop: still fuelling
    TEST_ASSERT_TRUE(fueleco::fuelCut(0, 1, 1800, 60, 2, false));       // stale reading: ignored
    TEST_ASSERT_FALSE(fueleco::fuelCut(0, 1, 900, 60, NAN, false));     // low revs: idle fuelling
    TEST_ASSERT_FALSE(fueleco::fuelCut(0, 1, 1800, 15, NAN, false));    // nearly stopped
    TEST_ASSERT_TRUE(fueleco::fuelCut(NAN, 1, 1800, 60, NAN, false));   // no pedal: the throttle decides
    TEST_ASSERT_FALSE(fueleco::fuelCut(NAN, NAN, 1800, 60, 4, true));   // neither: can't tell, no cut
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_blend_for_e30);
    RUN_TEST(test_speed_density_and_maf);
    RUN_TEST(test_no_air_reading_gives_nothing);
    RUN_TEST(test_distance_without_air_is_not_counted);
    RUN_TEST(test_steady_cruise);
    RUN_TEST(test_fuel_cut_is_free);
    RUN_TEST(test_idle_counts_fuel_not_distance);
    RUN_TEST(test_gap_is_capped_across_the_wrap);
    RUN_TEST(test_fuel_cut_needs_the_pedal_released);
    return UNITY_END();
}
