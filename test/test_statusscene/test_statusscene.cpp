#include <unity.h>
#include <cstdint>
#include "../../src/ui/statusscene.h"
#include "../../src/ui/status_sprite.h"
#include "../../src/util/idlesprite.h"
#include <cstring>

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
    const int x = statusscene::X0 + 1;
    TEST_ASSERT_EQUAL_HEX16(C.sky, statusscene::pixel(x, 30, l, p, C));
    TEST_ASSERT_EQUAL_HEX16(C.horizon, statusscene::pixel(x, 119, l, p, C));
    TEST_ASSERT_EQUAL_HEX16(C.grass, statusscene::pixel(x, 160, l, p, C));
    TEST_ASSERT_EQUAL_HEX16(C.platform, statusscene::pixel(statusscene::CX, 140, l, p, C));   // below the feet
    TEST_ASSERT_EQUAL_HEX16(C.edge, statusscene::pixel(statusscene::CX - 60, 136, l, p, C));   // left tip
    TEST_ASSERT_EQUAL_HEX16(C.grass, statusscene::pixel(statusscene::CX - 62, 136, l, p, C));   // just outside
}

void test_sprite_drawn_2x_standing_on_the_platform_and_bobs_up() {
    statusscene::Look l = statusscene::lookFor(Kind::Ok);
    const int id = l.sprite, w = status_sprite::width(id), h = status_sprite::height(id);
    int sx = -1, sy = -1;
    uint16_t c = 0;
    for (int y = h - 1; y >= 0 && sx < 0; --y)          // lowest opaque pixel: a foot
        for (int x = 0; x < w; ++x)
            if (status_sprite::pixel(id, x, y, &c)) { sx = x; sy = y; break; }
    const int left = statusscene::CX - w, top = statusscene::FEET_Y - 2 * h;
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
            TEST_ASSERT_TRUE(statusscene::CX - w >= statusscene::X0);
            TEST_ASSERT_TRUE(statusscene::CX + w <= statusscene::X0 + statusscene::W);
            TEST_ASSERT_TRUE(statusscene::CX + p.ballX - 7 >= statusscene::X0);
            TEST_ASSERT_TRUE(statusscene::FEET_Y - 2 * h - p.bob >= statusscene::Y0);
            if (l.bubble != Bubble::None) {
                statusscene::Rect r = statusscene::bubbleRect(l, p);
                TEST_ASSERT_TRUE(r.x >= statusscene::X0 && r.y >= statusscene::Y0);
                TEST_ASSERT_TRUE(r.x + r.w + 2 <= statusscene::X0 + statusscene::W);   // + shadow
            }
        }
    }
}

void test_region_leaves_room_for_the_info_box_and_dialogue() {
    TEST_ASSERT_TRUE(statusscene::Y0 + statusscene::H <= statusscene::DLG_Y);
    TEST_ASSERT_EQUAL_INT(0, statusscene::H % 4);   // pushed in 4-row strips
}

void test_bubble_has_room_around_the_text() {
    statusscene::Look l = statusscene::lookFor(Kind::SpotifyError);
    statusscene::Rect r = statusscene::bubbleRect(l, statusscene::poseAt(0, l));
    TEST_ASSERT_TRUE(r.w >= 3 * 12 - 2 + 2 * 8);   // "zZz" + 8 px each side
    TEST_ASSERT_TRUE(r.h >= 14 + 2 * 6);
}

// A 4-frame, 2x2 idle sprite: frame f's top-left pixel is opaque (palette f+1), rest clear.
static uint8_t s_idle[8 + 32 + 4 * 2];
static void loadIdle() {
    memcpy(s_idle, "IDL1", 4);
    s_idle[4] = 4; s_idle[5] = 2; s_idle[6] = 2; s_idle[7] = 0;
    for (int i = 0; i < 16; ++i) { s_idle[8 + 2 * i] = (uint8_t)(0x40 + i); s_idle[9 + 2 * i] = 0; }
    memset(s_idle + 40, 0, 8);
    for (int f = 0; f < 4; ++f) s_idle[40 + f * 2] = (uint8_t)((f + 1) << 4);
    TEST_ASSERT_TRUE(idlesprite::parse(s_idle, sizeof s_idle));
}

// "Nothing playing" shows Sonic waiting when the deck has his file, Pikachu otherwise; every
// other cause keeps its Pokemon.
void test_nothing_playing_is_sonic_with_the_file() {
    idlesprite::parse(nullptr, 0);
    TEST_ASSERT_FALSE(statusscene::lookFor(Kind::Ok).idle);
    loadIdle();
    TEST_ASSERT_TRUE(statusscene::lookFor(Kind::Ok).idle);
    TEST_ASSERT_FALSE(statusscene::lookFor(Kind::NoWifi).idle);
}

// He stands at 2x with his feet on the platform, doesn't bob, and his frame follows the game's
// waiting timeline (frame f's top-left pixel is palette entry f+1).
void test_sonic_drawn_2x_on_the_platform_following_the_timeline() {
    loadIdle();
    const statusscene::Look l = statusscene::lookFor(Kind::Ok);
    const int left = statusscene::CX - idlesprite::width();
    const int top = statusscene::FEET_Y - 2 * idlesprite::height();
    struct { uint32_t ms; int frame; } rows[] = {{0, idlesprite::STAND}, {4800, idlesprite::WAIT2},
                                                 {5200, idlesprite::WAIT1}, {6800, idlesprite::WAIT3}};
    for (auto& r : rows) {
        const statusscene::Pose p = statusscene::poseAt(r.ms, l);
        TEST_ASSERT_EQUAL_INT(0, p.bob);
        TEST_ASSERT_EQUAL_HEX16(0x40 + r.frame + 1, statusscene::pixel(left + 1, top + 1, l, p, C));
        TEST_ASSERT_NOT_EQUAL(0x40 + r.frame + 1, statusscene::pixel(left + 2, top, l, p, C));
    }
}

// The scene redraws when its key changes: every FRAME_MS for the Pokemon; for Sonic only when
// his frame (400 ms) or the dialogue's blinking arrow (500 ms) can change.
void test_redraw_key() {
    loadIdle();
    const statusscene::Look poke = statusscene::lookFor(Kind::NoWifi);
    TEST_ASSERT_EQUAL_UINT32(3, statusscene::frameKey(3 * statusscene::FRAME_MS, poke));
    const statusscene::Look sonic = statusscene::lookFor(Kind::Ok);
    TEST_ASSERT_EQUAL_UINT32(statusscene::frameKey(0, sonic), statusscene::frameKey(399, sonic));
    TEST_ASSERT_NOT_EQUAL(statusscene::frameKey(399, sonic), statusscene::frameKey(400, sonic));
    TEST_ASSERT_NOT_EQUAL(statusscene::frameKey(499, sonic), statusscene::frameKey(500, sonic));
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
    RUN_TEST(test_region_leaves_room_for_the_info_box_and_dialogue);
    RUN_TEST(test_bubble_has_room_around_the_text);
    RUN_TEST(test_nothing_playing_is_sonic_with_the_file);
    RUN_TEST(test_sonic_drawn_2x_on_the_platform_following_the_timeline);
    RUN_TEST(test_redraw_key);
    return UNITY_END();
}
