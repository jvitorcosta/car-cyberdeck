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

void test_base64_matches_rfc_4648() {
    const char* cases[][2] = {{"", ""}, {"f", "Zg=="}, {"fo", "Zm8="}, {"foo", "Zm9v"},
                              {"foob", "Zm9vYg=="}, {"fooba", "Zm9vYmE="}, {"foobar", "Zm9vYmFy"}};
    for (auto& c : cases) {
        char out[16];
        const size_t n = b64(reinterpret_cast<const uint8_t*>(c[0]), strlen(c[0]), out, sizeof out);
        TEST_ASSERT_EQUAL(strlen(c[1]), n);
        TEST_ASSERT_EQUAL(b64Len(strlen(c[0])), n);
        TEST_ASSERT_EQUAL_STRING(c[1], out);
    }
    const uint8_t bin[] = {0xFF, 0xFE, 0x00};
    char out[8];
    b64(bin, 3, out, sizeof out);
    TEST_ASSERT_EQUAL_STRING("//4A", out);
    TEST_ASSERT_EQUAL(0, b64(bin, 3, out, 4));   // no room for the terminator
}

void test_trip_dir_is_utc_and_needs_a_clock() {
    char dir[32];
    TEST_ASSERT_TRUE(tripDir(1763592601, dir, sizeof dir));   // 2025-11-19 22:50:01 UTC
    TEST_ASSERT_EQUAL_STRING("drive/20251119-225001", dir);
    TEST_ASSERT_TRUE(tripDir(1709251199, dir, sizeof dir));   // 2024-02-29 23:59:59, a leap day
    TEST_ASSERT_EQUAL_STRING("drive/20240229-235959", dir);
    TEST_ASSERT_TRUE(tripDir(1798761600, dir, sizeof dir));   // 2027-01-01 00:00:00
    TEST_ASSERT_EQUAL_STRING("drive/20270101-000000", dir);
    TEST_ASSERT_FALSE(tripDir(12345, dir, sizeof dir));       // 1970: SNTP hasn't answered
    TEST_ASSERT_FALSE(tripDir(1763592601, dir, 10));
}

void test_part_and_linkup_paths() {
    char p[64];
    TEST_ASSERT_TRUE(partPath("drive/20251119-225001", 7, p, sizeof p));
    TEST_ASSERT_EQUAL_STRING("drive/20251119-225001/p0007.csv", p);
    TEST_ASSERT_TRUE(linkupPath("drive/20251119-225001", 2, p, sizeof p));
    TEST_ASSERT_EQUAL_STRING("drive/20251119-225001/linkup2.txt", p);
}

void test_put_body_is_the_contents_api_json() {
    const char* data = "t_utc,ms\n1.0,2\n";
    const size_t need = bodyLen("drive p0001", strlen(data));
    char out[128];
    const size_t n = putBody("drive p0001", reinterpret_cast<const uint8_t*>(data), strlen(data), out, sizeof out);
    TEST_ASSERT_EQUAL(need, n);
    TEST_ASSERT_EQUAL_STRING("{\"message\":\"drive p0001\",\"content\":\"dF91dGMsbXMKMS4wLDIK\"}", out);
    TEST_ASSERT_EQUAL(0, putBody("drive p0001", reinterpret_cast<const uint8_t*>(data), strlen(data), out, need));
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

void test_outcomes() {
    TEST_ASSERT_EQUAL((int)Outcome::Done, (int)outcome(201));
    TEST_ASSERT_EQUAL((int)Outcome::Done, (int)outcome(422));   // a retry of a part already there
    TEST_ASSERT_EQUAL((int)Outcome::Refused, (int)outcome(401));
    TEST_ASSERT_EQUAL((int)Outcome::Refused, (int)outcome(404));
    TEST_ASSERT_EQUAL((int)Outcome::Retry, (int)outcome(-1));
    TEST_ASSERT_EQUAL((int)Outcome::Retry, (int)outcome(429));
    TEST_ASSERT_EQUAL((int)Outcome::Retry, (int)outcome(502));
}

void test_a_full_part_fits_the_upload_buffer() {
    // the worst row: every cell at its widest
    Sample s{255.0f, 16383.0f, -40.0f, -40.0f, 100.0f, 100.0f, 100.0f, 25.5f};
    char line[128];
    const size_t one = csvLine(pack(4294967295u, s), 4102444800.0, line, sizeof line);
    TEST_ASSERT_TRUE(one > 0);
    const size_t csv = strlen(HEADER) + BATCH * one;
    TEST_ASSERT_TRUE(csv <= 6 * 1024);                                  // dlogup's CSV buffer
    TEST_ASSERT_TRUE(bodyLen("cardeck drive/20261119-225001/p0001.csv", csv) <= 8400);   // and body
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
    RUN_TEST(test_base64_matches_rfc_4648);
    RUN_TEST(test_trip_dir_is_utc_and_needs_a_clock);
    RUN_TEST(test_part_and_linkup_paths);
    RUN_TEST(test_put_body_is_the_contents_api_json);
    RUN_TEST(test_raw_replies_stay_on_their_own_line);
    RUN_TEST(test_outcomes);
    RUN_TEST(test_a_full_part_fits_the_upload_buffer);
    return UNITY_END();
}
