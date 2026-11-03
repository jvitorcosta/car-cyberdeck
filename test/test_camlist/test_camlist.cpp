#include <unity.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include "../../src/util/car/camlist.h"

void setUp() {}
void tearDown() {}

static const char* HDR = "id,kind,lat,lon,heading,limit,label,source,checked\n";

static int run(const char* csv, camlist::Point* out, int max, int* bad) {
    return camlist::parse(csv, strlen(csv), out, max, bad);
}

void test_parses_three_kinds() {
    const char* csv =
        "id,kind,lat,lon,heading,limit,label,source,checked\n"
        "mao-0001,speed,-2.9656927,-60.021184,270,60,Av. do Turismo,immu,2026-11-03\n"
        "mao-0002,bump,-2.9656927,-60.021184,270,40,Rua A,osm,2026-11-03\n"
        "mao-0003,redlight,-2.9656927,-60.021184,270,,Cruzamento B,immu,2026-11-03\n";
    camlist::Point p[8];
    int bad = 0;
    TEST_ASSERT_EQUAL_INT(3, run(csv, p, 8, &bad));
    TEST_ASSERT_EQUAL_INT32(-2965693, p[0].latE6);
    TEST_ASSERT_EQUAL_INT32(-60021184, p[0].lonE6);
    TEST_ASSERT_EQUAL_UINT16(270, p[0].heading);
    TEST_ASSERT_EQUAL_UINT8(60, p[0].limit);
    TEST_ASSERT_TRUE(p[0].kind == camlist::Kind::Speed);
    TEST_ASSERT_EQUAL_STRING("mao-0001", p[0].id);
    TEST_ASSERT_EQUAL_STRING("Av. do Turismo", p[0].label);
    TEST_ASSERT_TRUE(p[1].kind == camlist::Kind::Bump);
    TEST_ASSERT_EQUAL_UINT8(40, p[1].limit);
    TEST_ASSERT_TRUE(p[2].kind == camlist::Kind::RedLight);
    TEST_ASSERT_EQUAL_UINT8(camlist::NO_LIMIT, p[2].limit);
}

void test_crlf_and_trailing_newline() {
    const char* csv =
        "id,kind,lat,lon,heading,limit,label,source,checked\r\n"
        "a,speed,-3.1,-60.0,90,50,X,osm,2026-11-03\r\n"
        "b,speed,-3.2,-60.1,0,80,Y,osm,2026-11-03\r\n\r\n\n";
    camlist::Point p[4];
    int bad = 0;
    TEST_ASSERT_EQUAL_INT(2, run(csv, p, 4, &bad));
    TEST_ASSERT_EQUAL_STRING("X", p[0].label);
    TEST_ASSERT_EQUAL_INT32(-3100000, p[0].latE6);
    TEST_ASSERT_EQUAL_UINT16(0, p[1].heading);
    // No trailing newline at all is fine too.
    const char* nonl = "id,kind,lat,lon,heading,limit,label,source,checked\na,bump,1,2,359,,X,osm,d";
    TEST_ASSERT_EQUAL_INT(1, run(nonl, p, 4, &bad));
    TEST_ASSERT_EQUAL_UINT16(359, p[0].heading);
    // Header only: zero rows.
    TEST_ASSERT_EQUAL_INT(0, run(HDR, p, 4, &bad));
}

void test_missing_heading_rejects_list() {
    const char* csv =
        "id,kind,lat,lon,heading,limit,label,source,checked\n"
        "a,speed,-3.1,-60.0,90,50,X,osm,2026-11-03\n"
        "b,speed,-3.2,-60.1,,80,Y,osm,2026-11-03\n";
    camlist::Point p[4];
    int bad = 0;
    TEST_ASSERT_EQUAL_INT(-1, run(csv, p, 4, &bad));
    TEST_ASSERT_EQUAL_INT(3, bad);
}

void test_redlight_with_limit_is_bad() {
    const char* csv =
        "id,kind,lat,lon,heading,limit,label,source,checked\n"
        "a,redlight,-3.1,-60.0,90,50,X,osm,2026-11-03\n";
    camlist::Point p[4];
    int bad = 0;
    TEST_ASSERT_EQUAL_INT(-1, run(csv, p, 4, &bad));
    TEST_ASSERT_EQUAL_INT(2, bad);
}

void test_unknown_limit_is_no_limit() {
    const char* csv =
        "id,kind,lat,lon,heading,limit,label,source,checked\n"
        "a,speed,-3.1,-60.0,90,,X,osm,2026-11-03\n";
    camlist::Point p[4];
    int bad = 0;
    TEST_ASSERT_EQUAL_INT(1, run(csv, p, 4, &bad));
    TEST_ASSERT_EQUAL_UINT8(camlist::NO_LIMIT, p[0].limit);
}

void test_too_many_rows() {
    const char* csv =
        "id,kind,lat,lon,heading,limit,label,source,checked\n"
        "a,speed,-3.1,-60.0,90,50,X,osm,d\n"
        "b,speed,-3.1,-60.0,90,50,X,osm,d\n"
        "c,speed,-3.1,-60.0,90,50,X,osm,d\n";
    camlist::Point p[2];
    int bad = 0;
    TEST_ASSERT_EQUAL_INT(-1, run(csv, p, 2, &bad));
    TEST_ASSERT_EQUAL_INT(4, bad);
}

void test_label_cut_keeps_utf8() {
    // 26 ASCII bytes, then "ç" (2 bytes: 26 and 27), then 12 more: 40 bytes. Byte 27 is the cut.
    char csv[256];
    snprintf(csv, sizeof csv, "%sa,speed,-3.1,-60.0,90,50,%s\xC3\xA7%s,osm,d\n", HDR,
             "aaaaaaaaaaaaaaaaaaaaaaaaaa", "bbbbbbbbbbbb");
    camlist::Point p[2];
    int bad = 0;
    TEST_ASSERT_EQUAL_INT(1, run(csv, p, 2, &bad));
    size_t n = strlen(p[0].label);
    TEST_ASSERT_EQUAL_UINT(26, n);  // the ç is dropped whole, not cut in half
    TEST_ASSERT_EQUAL_CHAR(0, p[0].label[n]);
    // A cut that lands between characters keeps the whole ç.
    snprintf(csv, sizeof csv, "%sa,speed,-3.1,-60.0,90,50,%s\xC3\xA7%s,osm,d\n", HDR,
             "aaaaaaaaaaaaaaaaaaaaaaaaa", "bbbbbbbbbbbb");
    TEST_ASSERT_EQUAL_INT(1, run(csv, p, 2, &bad));
    TEST_ASSERT_EQUAL_UINT(27, strlen(p[0].label));
}

void test_bad_rows() {
    camlist::Point p[4];
    int bad = 0;
    const char* cases[] = {
        "a,laser,-3.1,-60.0,90,50,X,osm,d\n",       // unknown kind
        "a,speed,north,-60.0,90,50,X,osm,d\n",      // lat not a number
        "a,speed,-3.1,,90,50,X,osm,d\n",            // lon empty
        "a,speed,-3.1,-60.0,360,50,X,osm,d\n",      // heading out of range
        "a,speed,-3.1,-60.0,9x,50,X,osm,d\n",       // heading not a number
        "a,speed,-3.1,-60.0,90,0,X,osm,d\n",        // limit 0
        "a,speed,-3.1,-60.0,90,200,X,osm,d\n",      // limit 200
        "a,speed,-3.1,-60.0,90,5O,X,osm,d\n",       // limit not a number
        "a,speed,-3.1,-60.0,90,50,X,osm\n",         // too few columns
        "a,speed,-3.1,-60.0,90,50,X,osm,d,extra\n", // too many columns
        "a,speed,91,-60.0,90,50,X,osm,d\n",         // lat out of range
    };
    for (const char* row : cases) {
        char csv[256];
        snprintf(csv, sizeof csv, "%s%s", HDR, row);
        bad = 0;
        TEST_ASSERT_EQUAL_INT_MESSAGE(-1, run(csv, p, 4, &bad), row);
        TEST_ASSERT_EQUAL_INT_MESSAGE(2, bad, row);
    }
    // A blank line in the middle is a bad line; a wrong header is line 1.
    const char* mid = "id,kind,lat,lon,heading,limit,label,source,checked\n\na,speed,1,2,90,50,X,osm,d\n";
    TEST_ASSERT_EQUAL_INT(-1, run(mid, p, 4, &bad));
    TEST_ASSERT_EQUAL_INT(2, bad);
    TEST_ASSERT_EQUAL_INT(-1, run("lat,lon\n1,2\n", p, 4, &bad));
    TEST_ASSERT_EQUAL_INT(1, bad);
}

void test_degrees_round_half_away_from_zero() {
    camlist::Point p[2];
    int bad = 0;
    const char* csv =
        "id,kind,lat,lon,heading,limit,label,source,checked\n"
        "a,bump,0.0000005,-0.0000005,0,,X,osm,d\n"
        "b,bump,12.3456784,+7.5,0,,X,osm,d\n";
    TEST_ASSERT_EQUAL_INT(2, run(csv, p, 2, &bad));
    TEST_ASSERT_EQUAL_INT32(1, p[0].latE6);
    TEST_ASSERT_EQUAL_INT32(-1, p[0].lonE6);
    TEST_ASSERT_EQUAL_INT32(12345678, p[1].latE6);
    TEST_ASSERT_EQUAL_INT32(7500000, p[1].lonE6);
}

void test_kind_names() {
    TEST_ASSERT_EQUAL_STRING("speed", camlist::kindName(camlist::Kind::Speed));
    TEST_ASSERT_EQUAL_STRING("bump", camlist::kindName(camlist::Kind::Bump));
    TEST_ASSERT_EQUAL_STRING("redlight", camlist::kindName(camlist::Kind::RedLight));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_parses_three_kinds);
    RUN_TEST(test_crlf_and_trailing_newline);
    RUN_TEST(test_missing_heading_rejects_list);
    RUN_TEST(test_redlight_with_limit_is_bad);
    RUN_TEST(test_unknown_limit_is_no_limit);
    RUN_TEST(test_too_many_rows);
    RUN_TEST(test_label_cut_keeps_utf8);
    RUN_TEST(test_bad_rows);
    RUN_TEST(test_degrees_round_half_away_from_zero);
    RUN_TEST(test_kind_names);
    return UNITY_END();
}
