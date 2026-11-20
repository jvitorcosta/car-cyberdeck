#include <unity.h>
#include <cmath>
#include <cstring>
#include <string>
#include "../../src/util/car/drivelog.h"

using namespace drivelog;

void setUp() {}
void tearDown() {}

static Sample full() { return Sample{80.0f, 2100.0f, 90.0f, 34.0f, 18.4f, 22.0f, 41.2f, 14.18f}; }

void test_a_full_row_as_csv() {
    char line[128];
    const Row r = pack(123456, full());
    const size_t n = csvLine(r, 1763592601.25, line, sizeof line);
    TEST_ASSERT_EQUAL(strlen(line), n);
    TEST_ASSERT_EQUAL_STRING("1763592601.2,123456,80.0,2100,90.0,34.0,18.4,22.0,41.2,14.18,26.2\n", line);
}

void test_the_header_matches_the_columns() {
    char line[128];
    csvLine(pack(1, full()), 2e9, line, sizeof line);
    int commas = 0, headerCommas = 0;
    for (const char* p = line; *p; ++p) commas += *p == ',';
    for (const char* p = HEADER; *p; ++p) headerCommas += *p == ',';
    TEST_ASSERT_EQUAL(headerCommas, commas);
    TEST_ASSERT_EQUAL('\n', HEADER[strlen(HEADER) - 1]);
}

void test_missing_values_are_blank_cells_never_zero() {
    Sample s = full();
    s.coolantC = NAN;
    s.pedalPct = NAN;
    s.volts = NAN;
    char line[128];
    csvLine(pack(5, s), 0, line, sizeof line);   // no clock either
    TEST_ASSERT_EQUAL_STRING(",5,80.0,2100,,34.0,18.4,,41.2,,26.2\n", line);
}

void test_an_absurd_value_is_missing_not_clipped() {
    Sample s = full();
    s.kmh = 99999.0f;   // tenths would overflow int16
    const Row r = pack(0, s);
    TEST_ASSERT_EQUAL(NONE, r.kmh10);
    char line[128];
    csvLine(r, 0, line, sizeof line);
    TEST_ASSERT_EQUAL_STRING(",0,,2100,90.0,34.0,18.4,22.0,41.2,14.18,\n", line);   // no N/V without speed
}

void test_negative_temperatures_keep_their_sign() {
    Sample s = full();
    s.iatC = -4.0f;
    char line[128];
    csvLine(pack(0, s), 0, line, sizeof line);
    TEST_ASSERT_NOT_NULL(strstr(line, ",90.0,-4.0,"));
}

void test_nv_is_blank_when_crawling_or_stopped() {
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 25.0f, nv(2500.0f, 100.0f));
    TEST_ASSERT_TRUE(std::isnan(nv(800.0f, 0.0f)));
    TEST_ASSERT_TRUE(std::isnan(nv(800.0f, 4.9f)));
    TEST_ASSERT_TRUE(std::isnan(nv(0.0f, 60.0f)));
    TEST_ASSERT_TRUE(std::isnan(nv(NAN, 60.0f)));
}

void test_a_line_that_does_not_fit_is_empty() {
    char line[20];
    TEST_ASSERT_EQUAL(0, csvLine(pack(1, full()), 1763592601.0, line, sizeof line));
    TEST_ASSERT_EQUAL_STRING("", line);
}

void test_row_unix_counts_back_from_now() {
    TEST_ASSERT_EQUAL_FLOAT(1000.0, rowUnix(1010, 20000, 10000));
    // millis() wrapped between the row and now
    TEST_ASSERT_EQUAL_FLOAT(1009.0, rowUnix(1010, 500, 0xFFFFFFFFu - 499));
}

void test_the_ring_keeps_order_and_numbers() {
    static Ring ring;
    ring = Ring();
    for (uint32_t i = 0; i < 5; ++i) ring.push(pack(i * 1000, full()));
    TEST_ASSERT_EQUAL(5, ring.size());
    TEST_ASSERT_EQUAL(0, ring.firstSeq());
    TEST_ASSERT_EQUAL(0, ring.at(0).ms);
    TEST_ASSERT_EQUAL(4000, ring.at(4).ms);
    ring.release(3);
    TEST_ASSERT_EQUAL(2, ring.size());
    TEST_ASSERT_EQUAL(3, ring.firstSeq());
    TEST_ASSERT_EQUAL(3000, ring.at(0).ms);
}

void test_a_full_ring_drops_the_oldest_and_counts_them() {
    static Ring ring;
    ring = Ring();
    for (uint32_t i = 0; i < CAP + 10; ++i) ring.push(pack(i, full()));
    TEST_ASSERT_EQUAL(CAP, ring.size());
    TEST_ASSERT_EQUAL(10, ring.dropped());
    TEST_ASSERT_EQUAL(10, ring.firstSeq());
    TEST_ASSERT_EQUAL(10, ring.at(0).ms);
    TEST_ASSERT_EQUAL(CAP + 9, ring.at(CAP - 1).ms);
}

void test_releasing_rows_already_dropped_is_harmless() {
    static Ring ring;
    ring = Ring();
    for (uint32_t i = 0; i < 10; ++i) ring.push(pack(i, full()));
    // a part of rows 0..59 was being sent while the ring dropped past them
    for (uint32_t i = 10; i < CAP + 70; ++i) ring.push(pack(i, full()));
    const uint32_t first = ring.firstSeq();
    ring.release(60);
    TEST_ASSERT_EQUAL(first, ring.firstSeq());
    TEST_ASSERT_EQUAL(CAP, ring.size());
    ring.release(ring.nextSeq());
    TEST_ASSERT_EQUAL(0, ring.size());
}

void test_a_part_is_due_at_a_batch_or_after_the_wait() {
    TEST_ASSERT_FALSE(partDue(0, 0, 999999, 60000));
    TEST_ASSERT_TRUE(partDue(BATCH, 1000, 1000, 60000));
    TEST_ASSERT_FALSE(partDue(10, 1000, 30000, 60000));
    TEST_ASSERT_TRUE(partDue(10, 1000, 61000, 60000));
}

void test_rows_go_to_the_ring_then_flash_in_order() {
    TEST_ASSERT_EQUAL((int)Route::Ring, (int)route(0, 0));
    TEST_ASSERT_EQUAL((int)Route::Ring, (int)route(SPOOL_AT - 1, 0));
    TEST_ASSERT_EQUAL((int)Route::Spool, (int)route(SPOOL_AT, 0));     // offline: the ring is full enough
    TEST_ASSERT_EQUAL((int)Route::Spool, (int)route(10, 5));           // flash still holds older rows
    TEST_ASSERT_EQUAL((int)Route::Drop, (int)route(SPOOL_AT, SPOOL_MAX));
    TEST_ASSERT_TRUE(SPOOL_AT + BATCH <= CAP);                          // the ring never drops while spooling
}

void test_refill_moves_a_batch_back_once_the_ring_drains() {
    TEST_ASSERT_EQUAL(0, refill(0, 0));
    TEST_ASSERT_EQUAL(0, refill(BATCH, 500));          // still a part's worth in RAM
    TEST_ASSERT_EQUAL(BATCH, refill(0, 500));
    TEST_ASSERT_EQUAL(7, refill(20, 7));
    TEST_ASSERT_TRUE(refill(BATCH - 1, 500) + BATCH - 1 <= SPOOL_AT);
}

void test_spool_round_trip_keeps_every_row_once_in_order() {
    // offline for a while, then the hotspot comes back: a fake of dlogup's loop and task
    static Ring ring;
    ring = Ring();
    Row spool[SPOOL_MAX];
    uint32_t spoolHead = 0, spoolRows = 0, sent = 0, made = 0;
    bool online = false;
    for (int t = 0; t < 2000; ++t) {
        if (t == 600) online = true;   // 300 rows in RAM, 300 on flash by then
        const Row r = pack(made++, full());
        switch (route(ring.size(), spoolRows)) {
            case Route::Ring: ring.push(r); break;
            case Route::Spool: spool[(spoolHead + spoolRows++) % SPOOL_MAX] = r; break;
            case Route::Drop: break;
        }
        for (uint32_t k = refill(ring.size(), spoolRows); k; --k, --spoolRows) {
            ring.push(spool[spoolHead]);
            spoolHead = (spoolHead + 1) % SPOOL_MAX;
        }
        if (online && t % 5 == 0 && ring.size()) {        // a part every 5 s while catching up
            const uint32_t n = ring.size() < BATCH ? ring.size() : BATCH;
            for (uint32_t i = 0; i < n; ++i, ++sent) TEST_ASSERT_EQUAL(sent, ring.at(i).ms);
            ring.release(ring.firstSeq() + n);
        }
    }
    TEST_ASSERT_EQUAL(0, ring.dropped());
    TEST_ASSERT_TRUE(sent > SPOOL_AT);
}

void test_trip_name_is_utc_and_needs_a_clock() {
    char name[32];
    TEST_ASSERT_TRUE(tripName(1763592601, name, sizeof name));   // 2025-11-19 22:50:01 UTC
    TEST_ASSERT_EQUAL_STRING("20251119-225001", name);
    TEST_ASSERT_TRUE(tripName(1709251199, name, sizeof name));   // 2024-02-29 23:59:59, a leap day
    TEST_ASSERT_EQUAL_STRING("20240229-235959", name);
    TEST_ASSERT_TRUE(tripName(1798761600, name, sizeof name));   // 2027-01-01 00:00:00
    TEST_ASSERT_EQUAL_STRING("20270101-000000", name);
    TEST_ASSERT_FALSE(tripName(12345, name, sizeof name));       // 1970: SNTP hasn't answered
    TEST_ASSERT_FALSE(tripName(1763592601, name, 10));
}

void test_raw_replies_stay_on_their_own_line() {
    char out[96] = "";
    TEST_ASSERT_TRUE(appendRaw(out, sizeof out, "01A4\r", "7E8 41 A4 01 02\r7E9 NO DATA\x01"));
    TEST_ASSERT_TRUE(appendRaw(out, sizeof out, "0100\r", nullptr));
    TEST_ASSERT_EQUAL_STRING("01A4 -> 7E8 41 A4 01 02|7E9 NO DATA?\n0100 -> \n", out);
    char small[8] = "";
    TEST_ASSERT_FALSE(appendRaw(small, sizeof small, "01A4\r", "41 A4 00"));
    TEST_ASSERT_TRUE(strlen(small) < sizeof small);
}

void test_a_full_part_fits_line_max() {
    // the worst row: every cell at its widest
    Sample s{255.0f, 16383.0f, -40.0f, -40.0f, 100.0f, 100.0f, 100.0f, 25.5f};
    char line[128];
    const size_t one = csvLine(pack(4294967295u, s), 4102444800.0, line, sizeof line);
    TEST_ASSERT_TRUE(one > 0);
    TEST_ASSERT_TRUE(one + 1 <= LINE_CAP);
    TEST_ASSERT_EQUAL(one, csvLine(pack(4294967295u, s), 4102444800.0, line, LINE_CAP));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_a_full_row_as_csv);
    RUN_TEST(test_the_header_matches_the_columns);
    RUN_TEST(test_missing_values_are_blank_cells_never_zero);
    RUN_TEST(test_an_absurd_value_is_missing_not_clipped);
    RUN_TEST(test_negative_temperatures_keep_their_sign);
    RUN_TEST(test_nv_is_blank_when_crawling_or_stopped);
    RUN_TEST(test_a_line_that_does_not_fit_is_empty);
    RUN_TEST(test_row_unix_counts_back_from_now);
    RUN_TEST(test_the_ring_keeps_order_and_numbers);
    RUN_TEST(test_a_full_ring_drops_the_oldest_and_counts_them);
    RUN_TEST(test_releasing_rows_already_dropped_is_harmless);
    RUN_TEST(test_a_part_is_due_at_a_batch_or_after_the_wait);
    RUN_TEST(test_rows_go_to_the_ring_then_flash_in_order);
    RUN_TEST(test_refill_moves_a_batch_back_once_the_ring_drains);
    RUN_TEST(test_spool_round_trip_keeps_every_row_once_in_order);
    RUN_TEST(test_trip_name_is_utc_and_needs_a_clock);
    RUN_TEST(test_raw_replies_stay_on_their_own_line);
    RUN_TEST(test_a_full_part_fits_line_max);
    return UNITY_END();
}
