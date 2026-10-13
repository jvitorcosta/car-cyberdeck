#include <unity.h>
#include <cstdint>
#include "../../src/ui/statusscene.h"
#include "../../src/ui/status_sprite.h"

using linkstatus::Kind;
using statusscene::Bubble;

void setUp() {}
void tearDown() {}

static const statusscene::Colors C = {0x0001, 0x0002, 0x0003, 0x0004, 0x0005,
                                      0x0006, 0x0007, 0x0008, 0x0009, 0x000A};
//                                    sky     horizon grass   platform edge
//                                    bubble  ink     ballRed ballWhite ballDark

void test_each_cause_gets_its_own_pokemon_and_bubble() {
    struct { Kind k; int sprite; Bubble b; bool ball; } rows[] = {
        {Kind::Ok,           status_sprite::PIKACHU,  Bubble::None,     false},   // nothing playing
        {Kind::NoWifi,       status_sprite::ZUBAT,    Bubble::Question, true},
        {Kind::NoInternet,   status_sprite::PSYDUCK,  Bubble::Dots,     false},
        {Kind::SpotifyError, status_sprite::SLOWPOKE, Bubble::Zzz,      false},
        {Kind::CertRejected, status_sprite::PORYGON,  Bubble::Bang,     false},
    };
    for (auto& r : rows) {
        statusscene::Look l = statusscene::lookFor(r.k);
        TEST_ASSERT_EQUAL_INT(r.sprite, l.sprite);
        TEST_ASSERT_EQUAL_INT((int)r.b, (int)l.bubble);
        TEST_ASSERT_EQUAL(r.ball, l.ball);
    }
}

void test_bob_cycles_gently_and_survives_millis_wrap() {
    statusscene::Look l = statusscene::lookFor(Kind::Ok);
    const int want[] = {0, 1, 2, 1, 0, 1};
    for (int i = 0; i < 6; ++i)
        TEST_ASSERT_EQUAL_INT(want[i], statusscene::poseAt(i * statusscene::FRAME_MS, l).bob);
    statusscene::Pose p = statusscene::poseAt(0xFFFFFFFFu, l);
    TEST_ASSERT_TRUE(p.bob >= 0 && p.bob <= 2);
}

void test_ball_rolls_back_and_forth_on_the_platform() {
    statusscene::Look l = statusscene::lookFor(Kind::NoWifi);
    int lo = 1000, hi = -1000;
    for (uint32_t ms = 0; ms < 20000; ms += statusscene::FRAME_MS) {
        statusscene::Pose p = statusscene::poseAt(ms, l);
        if (p.ballX < lo) lo = p.ballX;
        if (p.ballX > hi) hi = p.ballX;
        TEST_ASSERT_TRUE(p.ballFrame >= 0 && p.ballFrame < 8);
    }
    TEST_ASSERT_TRUE(lo < -30 && hi > 30);   // it really travels
    TEST_ASSERT_TRUE(lo >= -48 && hi <= 48); // and stays on the platform
}

void test_background_bands_and_platform() {
    statusscene::Look l = statusscene::lookFor(Kind::Ok);
    statusscene::Pose p = statusscene::poseAt(0, l);
    TEST_ASSERT_EQUAL_HEX16(C.sky, statusscene::pixel(45, 30, l, p, C));
    TEST_ASSERT_EQUAL_HEX16(C.horizon, statusscene::pixel(45, 119, l, p, C));
    TEST_ASSERT_EQUAL_HEX16(C.grass, statusscene::pixel(45, 150, l, p, C));
    TEST_ASSERT_EQUAL_HEX16(C.platform, statusscene::pixel(160, 140, l, p, C));   // below the feet
    TEST_ASSERT_EQUAL_HEX16(C.edge, statusscene::pixel(100, 136, l, p, C));       // left tip
    TEST_ASSERT_EQUAL_HEX16(C.grass, statusscene::pixel(98, 136, l, p, C));       // just outside
}

void test_sprite_drawn_2x_standing_on_the_platform_and_bobs_up() {
    statusscene::Look l = statusscene::lookFor(Kind::Ok);
    const int id = l.sprite, w = status_sprite::width(id), h = status_sprite::height(id);
    int sx = -1, sy = -1;
    uint16_t c = 0;
    for (int y = h - 1; y >= 0 && sx < 0; --y)          // lowest opaque pixel: a foot
        for (int x = 0; x < w; ++x)
            if (status_sprite::pixel(id, x, y, &c)) { sx = x; sy = y; break; }
    const int left = 160 - w, top = statusscene::FEET_Y - 2 * h;
    statusscene::Pose rest = statusscene::poseAt(0, l);
    TEST_ASSERT_EQUAL_HEX16(c, statusscene::pixel(left + 2 * sx + 1, top + 2 * sy + 1, l, rest, C));
    statusscene::Pose up = statusscene::poseAt(2 * statusscene::FRAME_MS, l);   // bob = 2
    TEST_ASSERT_EQUAL_HEX16(c, statusscene::pixel(left + 2 * sx + 1, top + 2 * sy + 1 - up.bob, l, up, C));
}

void test_bubble_only_when_the_look_has_one() {
    statusscene::Look none = statusscene::lookFor(Kind::Ok);
    statusscene::Look zzz = none;
    zzz.bubble = Bubble::Zzz;
    statusscene::Pose p = statusscene::poseAt(2 * statusscene::FRAME_MS, zzz);   // "zZz"
    statusscene::Rect r = statusscene::bubbleRect(zzz, p);
    int ink = 0, white = 0, inkWithout = 0;
    for (int y = r.y; y < r.y + r.h; ++y)
        for (int x = r.x; x < r.x + r.w; ++x) {
            uint16_t v = statusscene::pixel(x, y, zzz, p, C);
            ink += v == C.ink;
            white += v == C.bubble;
            inkWithout += statusscene::pixel(x, y, none, p, C) == C.ink;
        }
    TEST_ASSERT_TRUE(ink > 20 && white > 100);
    TEST_ASSERT_EQUAL_INT(0, inkWithout);
}

void test_everything_stays_inside_the_redrawn_region() {
    const Kind kinds[] = {Kind::Ok, Kind::NoWifi, Kind::NoInternet, Kind::SpotifyError, Kind::CertRejected};
    for (Kind k : kinds) {
        statusscene::Look l = statusscene::lookFor(k);
        for (uint32_t ms = 0; ms < 8000; ms += statusscene::FRAME_MS) {
            statusscene::Pose p = statusscene::poseAt(ms, l);
            const int w = status_sprite::width(l.sprite), h = status_sprite::height(l.sprite);
            TEST_ASSERT_TRUE(160 - w >= statusscene::X0 && 160 + w <= statusscene::X0 + statusscene::W);
            TEST_ASSERT_TRUE(statusscene::FEET_Y - 2 * h - p.bob >= statusscene::Y0);
            if (l.bubble != Bubble::None) {
                statusscene::Rect r = statusscene::bubbleRect(l, p);
                TEST_ASSERT_TRUE(r.x >= statusscene::X0 && r.y >= statusscene::Y0);
                TEST_ASSERT_TRUE(r.x + r.w <= statusscene::X0 + statusscene::W);
            }
        }
    }
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_each_cause_gets_its_own_pokemon_and_bubble);
    RUN_TEST(test_bob_cycles_gently_and_survives_millis_wrap);
    RUN_TEST(test_ball_rolls_back_and_forth_on_the_platform);
    RUN_TEST(test_background_bands_and_platform);
    RUN_TEST(test_sprite_drawn_2x_standing_on_the_platform_and_bobs_up);
    RUN_TEST(test_bubble_only_when_the_look_has_one);
    RUN_TEST(test_everything_stays_inside_the_redrawn_region);
    return UNITY_END();
}
