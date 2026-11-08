#include <unity.h>
#include <cstring>
#include "../../src/util/car/camwire.h"

void setUp() {}
void tearDown() {}

static camlist::Point pt(camlist::Kind k, uint8_t limit, const char* id, const char* label) {
    camlist::Point p{};
    p.kind = k;
    p.limit = limit;
    strcpy(p.id, id);
    strcpy(p.label, label);
    return p;
}

static cameras::Status st(cameras::Stage s, int idx, uint16_t d, uint16_t kmh) {
    cameras::Status x;
    x.stage = s;
    x.index = idx;
    x.distM = d;
    x.kmh = kmh;
    return x;
}

void test_write_alert_round_trips() {
    camlist::Point p = pt(camlist::Kind::Speed, 60, "mao-0007", "Av. Brasil");
    char out[camwire::MAX_JSON];
    TEST_ASSERT_TRUE(camwire::write(st(cameras::Stage::Alert, 3, 320, 58), &p, true, out, sizeof out));
    camwire::View v;
    TEST_ASSERT_TRUE(camwire::parse(out, &v));
    TEST_ASSERT_TRUE(v.stage == cameras::Stage::Alert);
    TEST_ASSERT_TRUE(v.kind == camlist::Kind::Speed);
    TEST_ASSERT_EQUAL_UINT8(60, v.limit);
    TEST_ASSERT_EQUAL_UINT16(320, v.distM);
    TEST_ASSERT_EQUAL_UINT16(58, v.kmh);
    TEST_ASSERT_TRUE(v.mute);
    TEST_ASSERT_EQUAL_STRING("Av. Brasil", v.label);
    TEST_ASSERT_NOT_NULL(strstr(out, "\"id\":\"mao-0007\""));
}

void test_alarm_and_bump_round_trip() {
    camlist::Point p = pt(camlist::Kind::Bump, camlist::NO_LIMIT, "b1", "Lombada");
    char out[camwire::MAX_JSON];
    TEST_ASSERT_TRUE(camwire::write(st(cameras::Stage::Alarm, 0, 90, 71), &p, false, out, sizeof out));
    camwire::View v;
    TEST_ASSERT_TRUE(camwire::parse(out, &v));
    TEST_ASSERT_TRUE(v.stage == cameras::Stage::Alarm);
    TEST_ASSERT_TRUE(v.kind == camlist::Kind::Bump);
    TEST_ASSERT_EQUAL_UINT8(0, v.limit);
    TEST_ASSERT_FALSE(v.mute);
}

void test_off_has_no_point_fields() {
    char out[camwire::MAX_JSON];
    TEST_ASSERT_TRUE(camwire::write(st(cameras::Stage::Off, -1, 0, 42), nullptr, false, out, sizeof out));
    TEST_ASSERT_NULL(strstr(out, "kind"));
    TEST_ASSERT_NULL(strstr(out, "label"));
    TEST_ASSERT_NOT_NULL(strstr(out, "\"st\":\"off\""));
    TEST_ASSERT_NOT_NULL(strstr(out, "\"kmh\":42"));
    TEST_ASSERT_NOT_NULL(strstr(out, "\"mute\":0"));
    camwire::View v;
    TEST_ASSERT_TRUE(camwire::parse(out, &v));
    TEST_ASSERT_TRUE(v.stage == cameras::Stage::Off);
    TEST_ASSERT_TRUE(v.kind == camlist::Kind::Speed);
    TEST_ASSERT_EQUAL_UINT16(42, v.kmh);
    TEST_ASSERT_EQUAL_STRING("", v.label);
}

void test_nofix_round_trips() {
    char out[camwire::MAX_JSON];
    TEST_ASSERT_TRUE(camwire::write(st(cameras::Stage::NoFix, -1, 0, 0), nullptr, true, out, sizeof out));
    camwire::View v;
    TEST_ASSERT_TRUE(camwire::parse(out, &v));
    TEST_ASSERT_TRUE(v.stage == cameras::Stage::NoFix);
    TEST_ASSERT_TRUE(v.mute);
}

void test_alert_without_point_fails() {
    char out[camwire::MAX_JSON];
    TEST_ASSERT_FALSE(camwire::write(st(cameras::Stage::Alert, 1, 100, 50), nullptr, false, out, sizeof out));
    TEST_ASSERT_FALSE(camwire::write(st(cameras::Stage::Alarm, 1, 100, 50), nullptr, false, out, sizeof out));
}

void test_buffer_too_small_fails() {
    camlist::Point p = pt(camlist::Kind::Speed, 60, "mao-0007", "Av. Brasil");
    char out[20];
    TEST_ASSERT_FALSE(camwire::write(st(cameras::Stage::Alert, 3, 320, 58), &p, false, out, sizeof out));
}

void test_label_with_quote_escaped() {
    camlist::Point p = pt(camlist::Kind::Speed, 60, "a\"b", "Rua \"X\" \\ \x01");
    char out[camwire::MAX_JSON];
    TEST_ASSERT_TRUE(camwire::write(st(cameras::Stage::Alert, 0, 100, 50), &p, false, out, sizeof out));
    TEST_ASSERT_NOT_NULL(strstr(out, "Rua \\\"X\\\" \\\\ \\u0001"));
    TEST_ASSERT_NOT_NULL(strstr(out, "a\\\"b"));
    camwire::View v;
    TEST_ASSERT_TRUE(camwire::parse(out, &v));
    TEST_ASSERT_EQUAL_STRING("Rua \"X\" \\ \x01", v.label);
}

void test_utf8_label_passes_through() {
    camlist::Point p = pt(camlist::Kind::RedLight, 0, "r1", "Avenida S\xC3\xA3o Jo\xC3\xA3o");
    char out[camwire::MAX_JSON];
    TEST_ASSERT_TRUE(camwire::write(st(cameras::Stage::Alert, 0, 100, 30), &p, false, out, sizeof out));
    TEST_ASSERT_NOT_NULL(strstr(out, "S\xC3\xA3o"));
    camwire::View v;
    TEST_ASSERT_TRUE(camwire::parse(out, &v));
    TEST_ASSERT_TRUE(v.kind == camlist::Kind::RedLight);
    TEST_ASSERT_EQUAL_STRING("Avenida S\xC3\xA3o Jo\xC3\xA3o", v.label);
}

void test_longest_point_fits_max_json() {
    camlist::Point p = pt(camlist::Kind::Speed, 199, "12345678901", "");
    memset(p.label, 'x', 27);
    p.label[27] = 0;
    char out[camwire::MAX_JSON];
    TEST_ASSERT_TRUE(camwire::write(st(cameras::Stage::Alarm, 0, 65535, 65535), &p, true, out, sizeof out));
}

void test_parse_rejects_garbage() {
    camwire::View v;
    TEST_ASSERT_FALSE(camwire::parse(nullptr, &v));
    TEST_ASSERT_FALSE(camwire::parse("", &v));
    TEST_ASSERT_FALSE(camwire::parse("not json", &v));
    TEST_ASSERT_FALSE(camwire::parse("{}", &v));
    TEST_ASSERT_FALSE(camwire::parse("{\"st\":\"bogus\"}", &v));
    TEST_ASSERT_FALSE(camwire::parse("{\"st\":\"alert\",\"dist\":5}", &v));
    TEST_ASSERT_FALSE(camwire::parse("{\"st\":\"alarm\",\"kind\":\"speed\"}", &v));
    TEST_ASSERT_FALSE(camwire::parse("{\"st\":\"alert\",\"kind\":\"nope\",\"dist\":5}", &v));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_write_alert_round_trips);
    RUN_TEST(test_alarm_and_bump_round_trip);
    RUN_TEST(test_off_has_no_point_fields);
    RUN_TEST(test_nofix_round_trips);
    RUN_TEST(test_alert_without_point_fails);
    RUN_TEST(test_buffer_too_small_fails);
    RUN_TEST(test_label_with_quote_escaped);
    RUN_TEST(test_utf8_label_passes_through);
    RUN_TEST(test_longest_point_fits_max_json);
    RUN_TEST(test_parse_rejects_garbage);
    return UNITY_END();
}
