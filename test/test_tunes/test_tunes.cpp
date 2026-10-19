#include <unity.h>
#include <cstring>
#include <cstdio>
#include <string>
#include "../../src/util/tunes.h"

void setUp() {}
void tearDown() {}

static const char SET[] =
    "# two tunes\n"
    "tune First one\n"
    "C4 250 R 100 A#4 50\n"
    "Bb3 75   # a comment\n"
    "\n"
    "tune Second\r\n"
    "A4 1000\r\n";

static size_t len(const char* s) { return strlen(s); }

void test_counts_tunes() {
    TEST_ASSERT_EQUAL_INT(2, tunes::count(SET, len(SET)));
    TEST_ASSERT_EQUAL_INT(0, tunes::count("", 0));
    TEST_ASSERT_EQUAL_INT(0, tunes::count("C4 100\n", 7));   // notes before any tune line
    TEST_ASSERT_EQUAL_INT(0, tunes::count("tune Empty\n", 11));   // a tune needs a note
}

void test_reads_notes_rests_sharps_and_flats() {
    tunes::Reader r(SET, len(SET), 0);
    tunes::Note n;
    TEST_ASSERT_TRUE(r.next(&n)); TEST_ASSERT_EQUAL_INT(60, n.midi); TEST_ASSERT_EQUAL_UINT16(250, n.ms);
    TEST_ASSERT_TRUE(r.next(&n)); TEST_ASSERT_EQUAL_INT(tunes::REST, n.midi); TEST_ASSERT_EQUAL_UINT16(100, n.ms);
    TEST_ASSERT_TRUE(r.next(&n)); TEST_ASSERT_EQUAL_INT(70, n.midi); TEST_ASSERT_EQUAL_UINT16(50, n.ms);
    TEST_ASSERT_TRUE(r.next(&n)); TEST_ASSERT_EQUAL_INT(58, n.midi); TEST_ASSERT_EQUAL_UINT16(75, n.ms);
    TEST_ASSERT_FALSE(r.next(&n));   // stops at the next tune
}

void test_second_tune_and_out_of_range() {
    tunes::Reader r(SET, len(SET), 1);
    tunes::Note n;
    TEST_ASSERT_TRUE(r.next(&n));
    TEST_ASSERT_EQUAL_INT(69, n.midi);
    TEST_ASSERT_EQUAL_UINT16(1000, n.ms);
    TEST_ASSERT_FALSE(r.next(&n));
    tunes::Reader none(SET, len(SET), 2);
    TEST_ASSERT_FALSE(none.next(&n));
}

void test_names() {
    char name[16];
    TEST_ASSERT_TRUE(tunes::name(SET, len(SET), 0, name, sizeof name));
    TEST_ASSERT_EQUAL_STRING("First one", name);
    TEST_ASSERT_TRUE(tunes::name(SET, len(SET), 1, name, sizeof name));
    TEST_ASSERT_EQUAL_STRING("Second", name);   // no trailing \r
    TEST_ASSERT_FALSE(tunes::name(SET, len(SET), 2, name, sizeof name));
}

// Bad pairs are skipped, not fatal; lengths are clamped; the text needn't end in a newline.
void test_skips_bad_pairs_and_clamps() {
    static const char T[] = "tune X\nH4 100 C4 x C#9 100 D4 0 E4 99999 G4 120";
    tunes::Reader r(T, len(T), 0);
    tunes::Note n;
    TEST_ASSERT_TRUE(r.next(&n)); TEST_ASSERT_EQUAL_INT(62, n.midi); TEST_ASSERT_EQUAL_UINT16(1, n.ms);
    TEST_ASSERT_TRUE(r.next(&n)); TEST_ASSERT_EQUAL_INT(64, n.midi); TEST_ASSERT_EQUAL_UINT16(tunes::MAX_MS, n.ms);
    TEST_ASSERT_TRUE(r.next(&n)); TEST_ASSERT_EQUAL_INT(67, n.midi); TEST_ASSERT_EQUAL_UINT16(120, n.ms);
    TEST_ASSERT_FALSE(r.next(&n));
}

// The committed fallback set (tests run from the repo root): both tunes parse, long enough.
void test_fallback_tunes_parse() {
    FILE* f = fopen("data/tunes_default.txt", "rb");
    TEST_ASSERT_NOT_NULL(f);
    std::string t;
    char b[512];
    for (size_t n; (n = fread(b, 1, sizeof b, f)) > 0;) t.append(b, n);
    fclose(f);
    TEST_ASSERT_EQUAL_INT(2, tunes::count(t.data(), t.size()));
    for (int i = 0; i < 2; ++i) {
        tunes::Reader r(t.data(), t.size(), i);
        tunes::Note n;
        int notes = 0;
        uint32_t ms = 0;
        while (r.next(&n)) { ++notes; ms += n.ms; TEST_ASSERT_TRUE(n.midi >= 60 && n.midi <= 96); }
        TEST_ASSERT_TRUE(notes > 30 && ms > 15000);
    }
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_counts_tunes);
    RUN_TEST(test_reads_notes_rests_sharps_and_flats);
    RUN_TEST(test_second_tune_and_out_of_range);
    RUN_TEST(test_names);
    RUN_TEST(test_skips_bad_pairs_and_clamps);
    RUN_TEST(test_fallback_tunes_parse);
    return UNITY_END();
}
