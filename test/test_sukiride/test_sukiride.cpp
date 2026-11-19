#include <unity.h>
#include <cmath>
#include <cstring>
#include <vector>
#include "../../src/util/car/sukiride.h"
#include "../../src/util/common/termpal.h"

using namespace sukiride;

void setUp() {}
void tearDown() {}

static void le16(std::vector<uint8_t>& b, uint16_t v) { b.push_back(v & 0xFF); b.push_back(v >> 8); }
static void le32(std::vector<uint8_t>& b, uint32_t v) { for (int i = 0; i < 4; ++i) b.push_back((v >> (8 * i)) & 0xFF); }
static uint16_t be(uint32_t rgb) {
    const uint16_t n = (uint16_t)((((rgb >> 16) & 0xF8) << 8) | (((rgb >> 8) & 0xFC) << 3) | ((rgb & 0xFF) >> 3));
    return (uint16_t)((n >> 8) | (n << 8));
}

// Palette: 0 black, 1 the film's pink, 2 skin, 3 grey, 4 white; the rest black.
static const uint32_t PAL[5] = {0x000000, 0xFF2878, 0xFAD7BE, 0x808080, 0xFFFFFF};
// A whole file: cel k is all skin (2) but in cel 7 its column 60 is white (4) and pixel (70, 20)
// grey (3); the logo is solid; every font has '0'..'9', '-', '.', 'C', 'V', '%' 2 px wide, cell 3,
// cap 2, solid.
static std::vector<uint8_t> file(int cels = CELS) {
    std::vector<uint8_t> b = {'S', 'K', 'R', '2'};
    le16(b, ART_X); le16(b, ART_W); le16(b, ART_H); le16(b, (uint16_t)cels); le16(b, FONTS);
    le16(b, LOGO_W); le16(b, LOGO_H); le16(b, 0);
    for (int i = 0; i < COLOURS; ++i) {
        const uint32_t c = i < 5 ? PAL[i] : 0;
        b.push_back(c >> 16); b.push_back((c >> 8) & 0xFF); b.push_back(c & 0xFF);
    }
    for (int k = 0; k < cels; ++k)
        for (int y = 0; y < ART_H; ++y)
            for (int x = 0; x < ART_W; ++x) b.push_back(k == 7 && x == 60 ? 4 : k == 7 && x == 70 && y == 20 ? 3 : 2);
    for (int i = 0; i < LOGO_W * LOGO_H; ++i) b.push_back(255);
    const char chars[] = "0123456789-.CV%";
    const int n = (int)strlen(chars);
    std::vector<size_t> fix;
    for (int f = 0; f < FONTS; ++f) {
        b.push_back(3); b.push_back(1); b.push_back((uint8_t)n); b.push_back(2);
        for (int g = 0; g < n; ++g) {
            b.push_back((uint8_t)chars[g]); b.push_back(2); le16(b, 0);
            fix.push_back(b.size());
            le32(b, 0);
        }
    }
    for (size_t i = 0; i < fix.size(); ++i) {
        const uint32_t off = (uint32_t)b.size();
        for (int k = 0; k < 4; ++k) b[fix[i] + k] = (off >> (8 * k)) & 0xFF;
        for (int p = 0; p < 2 * 3; ++p) b.push_back(255);
    }
    return b;
}

void test_parse_takes_a_whole_file_and_nothing_else() {
    std::vector<uint8_t> f = file();
    Art a;
    TEST_ASSERT_TRUE(parse(f.data(), f.size(), &a));
    TEST_ASSERT_EQUAL_UINT8(3, a.cels[((size_t)7 * ART_H + 20) * ART_W + 70]);
    TEST_ASSERT_EQUAL_UINT8(4, a.cels[(size_t)7 * ART_W * ART_H + 60]);
    TEST_ASSERT_EQUAL_UINT8(255, a.logo[0]);
    TEST_ASSERT_EQUAL_INT(2 * 3 + 2, width(a, F_VALUE, "123"));   // 3 x 2 px + 2 x 1 tracking
    TEST_ASSERT_EQUAL_INT(2, width(a, F_VALUE, "x1"));            // unknown chars skipped
    TEST_ASSERT_FALSE(parse(f.data(), f.size() - 1, &a));         // the last glyph cut off
    TEST_ASSERT_FALSE(parse(f.data(), HEAD + 100, &a));
    TEST_ASSERT_FALSE(parse(nullptr, 0, &a));
    std::vector<uint8_t> bad = f;
    bad[0] = 'X';
    TEST_ASSERT_FALSE(parse(bad.data(), bad.size(), &a));
    std::vector<uint8_t> few = file(CELS - 1);   // the cel table must hold every cel celIndex picks
    TEST_ASSERT_FALSE(parse(few.data(), few.size(), &a));
    std::vector<uint8_t> zero(f.size(), 0);      // CI's stand-in
    TEST_ASSERT_FALSE(parse(zero.data(), zero.size(), &a));
}

void test_recolour_moves_hues_and_keeps_skin_and_greys() {
    const float green = hueOf(0x5ABE28);
    TEST_ASSERT_EQUAL_HEX32(0x808080, recolour(0x808080, green, 120, 1, 1));
    TEST_ASSERT_EQUAL_HEX32(0xFFFFFF, recolour(0xFFFFFF, green, 120, 1, 1));
    TEST_ASSERT_EQUAL_HEX32(0xFAD7BE, recolour(0xFAD7BE, green, 120, 1, 1));   // skin
    const uint32_t pink = recolour(0xFF2878, green, 120, 1, 1);
    const float h = hueOf(pink);
    TEST_ASSERT_TRUE(h > 80 && h < 130);   // pink (hue ~338) lands near the theme's green
    // the white theme: saturation cut, near grey
    const uint32_t grey = recolour(0xFF2878, green, 120, 0.06f, 1);
    const int r = grey >> 16, g = (grey >> 8) & 255, b = grey & 255;
    TEST_ASSERT_TRUE(abs(r - g) < 20 && abs(g - b) < 20);
    // night: x 0.78
    TEST_ASSERT_EQUAL_HEX32(0x646464, recolour(0x808080, green, 120, 1, 0.78f));
}

void test_look_takes_termpal_roles() {
    std::vector<uint8_t> f = file();
    Art a;
    TEST_ASSERT_TRUE(parse(f.data(), f.size(), &a));
    Look k;
    makeLook(a, 5, false, &k);   // green, day
    TEST_ASSERT_EQUAL_HEX16(be(termpal::rgb(termpal::TEXT, 5, false)), k.tx);
    TEST_ASSERT_EQUAL_HEX16(be(termpal::rgb(termpal::ALERT, 5, false)), k.al);
    TEST_ASSERT_EQUAL_HEX16(be(0xFAD7BE), k.art[2]);   // skin stays
    TEST_ASSERT_EQUAL_HEX16(be(0x0D0008), k.onAlert);  // dark text reads better on the green theme's red alert

    makeLook(a, 0, true, &k);   // red, night: the alert is white there
    TEST_ASSERT_EQUAL_HEX16(be(0xFFFFFF), k.al);
    TEST_ASSERT_EQUAL_HEX16(be(0xC3A895), k.art[2]);   // skin at 78 %
}

void test_zones_and_pills() {
    TEST_ASSERT_EQUAL_INT((int)Zone::Cold, (int)coolZone(70));
    TEST_ASSERT_EQUAL_INT((int)Zone::Norm, (int)coolZone(88));
    TEST_ASSERT_EQUAL_INT((int)Zone::Warn, (int)coolZone(95));
    TEST_ASSERT_EQUAL_INT((int)Zone::Alert, (int)coolZone(105));
    TEST_ASSERT_EQUAL_INT((int)Zone::Alert, (int)battZone(11.9f));
    TEST_ASSERT_EQUAL_INT((int)Zone::Norm, (int)battZone(14.7f));
    TEST_ASSERT_EQUAL_INT((int)Zone::Warn, (int)battZone(14.8f));
    TEST_ASSERT_EQUAL_INT((int)Zone::Warn, (int)pedalZone(85));
    TEST_ASSERT_EQUAL_INT(0, pillsLit(NAN));
    TEST_ASSERT_EQUAL_INT(0, pillsLit(-1));
    TEST_ASSERT_EQUAL_INT(PILLS, pillsLit(2));
    TEST_ASSERT_EQUAL_INT(8, pillsLit(coolFrac(95)));   // 60..130: 95 is half
    TEST_ASSERT_EQUAL_INT(12, pillsLit(battFrac(14)));
}

void test_gear_estimate() {
    TEST_ASSERT_EQUAL_INT(0, gearEst(2000, 9.9f));     // too slow to say
    TEST_ASSERT_EQUAL_INT(0, gearEst(NAN, 50));
    TEST_ASSERT_EQUAL_INT(0, gearEst(2000, NAN));
    TEST_ASSERT_EQUAL_INT(1, gearEst(2500, 20));        // 125 rpm per km/h
    TEST_ASSERT_EQUAL_INT(7, gearEst(2000, 100));       // 20
    TEST_ASSERT_EQUAL_INT(4, gearEst(2525, 50));        // 50.5
    TEST_ASSERT_EQUAL_INT(0, gearEst(6000, 20));        // 300: slipping, no band
    TEST_ASSERT_EQUAL_INT(0, gearEst(1000, 120));       // 8.3: coasting, no band
}

void test_the_girl_follows_the_speed() {
    TEST_ASSERT_EQUAL_INT(7, celIndex(60, false, 1234));   // frozen off the link
    TEST_ASSERT_EQUAL_INT(7, celIndex(2, true, 1234));     // parked
    TEST_ASSERT_EQUAL_INT(7, celIndex(NAN, true, 1234));
    TEST_ASSERT_EQUAL_INT(5, celIndex(50, true, 0));       // calm ping-pong, 4 a second
    TEST_ASSERT_EQUAL_INT(6, celIndex(50, true, 250));
    TEST_ASSERT_EQUAL_INT(9, celIndex(50, true, 1000));    // the far end of the ping-pong
    TEST_ASSERT_EQUAL_INT(6, celIndex(50, true, 1750));    // and back
    TEST_ASSERT_EQUAL_INT(10, celIndex(95, true, 250));    // the open mouth
    for (uint32_t t = 0; t < 4000; t += 125) {             // teeth only, never a calm one
        const int c = celIndex(130, true, t);
        TEST_ASSERT_TRUE((c >= 0 && c <= 4) || (c >= 11 && c <= 18));
    }
    TEST_ASSERT_EQUAL_INT(0, shake(100, true, 0));
    TEST_ASSERT_EQUAL_INT(0, shake(130, false, 0));
    TEST_ASSERT_TRUE(shake(130, true, 0) != 0);
}

void test_trace_samples_four_a_second() {
    Trace t;
    t.update(1000, 0);
    t.update(5000, 100);   // too soon: ignored
    t.update(3000, 250);
    t.update(NAN, 500);
    TEST_ASSERT_EQUAL_UINT32(3, t.samples());
    TEST_ASSERT_EQUAL_UINT16(1000, t.at(TRACE_N - 3));
    TEST_ASSERT_EQUAL_UINT16(3000, t.at(TRACE_N - 2));
    TEST_ASSERT_EQUAL_UINT16(0, t.at(TRACE_N - 1));
}

static obdwire::Live live() {
    obdwire::Live l;
    l.link = obdplan::Link::Live;
    l.kmh = 62.4f; l.rpm = 2100; l.coolC = 88; l.volts = 14.06f; l.throttle = 24; l.intakeC = 31; l.kml = 15.84f;
    l.tripKm = 12.4f;
    return l;
}

void test_scene_from_the_values() {
    Trace t;
    Scene s;
    scene(live(), t, false, false, 0, &s);
    TEST_ASSERT_TRUE(s.live);
    TEST_ASSERT_EQUAL_STRING(" 62", s.speed);
    TEST_ASSERT_EQUAL_STRING("2100", s.rpm);
    TEST_ASSERT_EQUAL_STRING("15.8", s.chip[0].val);
    TEST_ASSERT_EQUAL_STRING("31", s.chip[1].val);
    TEST_ASSERT_EQUAL_STRING("C", s.chip[1].unit);
    TEST_ASSERT_EQUAL_STRING("PEDAL", s.rib[0].label);
    TEST_ASSERT_EQUAL_STRING("88", s.rib[1].val);
    TEST_ASSERT_EQUAL_STRING("14.1", s.rib[2].val);
    TEST_ASSERT_EQUAL_STRING("V", s.rib[2].unit);
    TEST_ASSERT_EQUAL_INT(4, s.rib[0].lit);
    TEST_ASSERT_EQUAL_INT(5, s.gear);   // 2100 / 62.4 = 33.7 rpm per km/h: 5th (37.3) more than 6th (27.6)
    TEST_ASSERT_EQUAL_INT((int)Zone::Cold, (int)s.rib[1].pill[0]);   // the coolant scale's first pill is cold
    TEST_ASSERT_FALSE(s.rib[1].bad);
    obdwire::Live hot = live();
    hot.coolC = 107;
    scene(hot, t, true, false, 0, &s);
    TEST_ASSERT_TRUE(s.rib[1].bad);
    TEST_ASSERT_EQUAL_INT((int)Zone::Alert, (int)s.rib[1].zone);
    obdwire::Live off = live();
    off.link = obdplan::Link::Connecting;
    scene(off, t, true, true, 0, &s);
    TEST_ASSERT_FALSE(s.live);
    TEST_ASSERT_EQUAL_STRING("---", s.speed);
    TEST_ASSERT_EQUAL_STRING("--", s.rpm);
    TEST_ASSERT_EQUAL_STRING("--", s.rib[1].val);
    TEST_ASSERT_EQUAL_STRING("", s.rib[1].unit);
    TEST_ASSERT_EQUAL_INT(0, s.rib[1].lit);
    TEST_ASSERT_FALSE(s.rib[1].bad);   // no alert from stale values
    TEST_ASSERT_EQUAL_STRING("--", s.chip[0].val);
    TEST_ASSERT_EQUAL_STRING("12.4", s.chip[2].val);   // the trip stays
    TEST_ASSERT_EQUAL_INT(0, s.gear);
    TEST_ASSERT_EQUAL_INT(7, s.cel);
}

void test_regions_draw_what_the_whole_screen_draws() {
    std::vector<uint8_t> f = file();
    Art a;
    TEST_ASSERT_TRUE(parse(f.data(), f.size(), &a));
    Look k;
    makeLook(a, 5, false, &k);
    Trace t;
    for (uint32_t ms = 0; ms < 4000; ms += 250) t.update(1500 + ms, ms);
    for (int pass = 0; pass < 3; ++pass) {
        obdwire::Live l = live();
        if (pass == 1) { l.kmh = 130; l.coolC = 110; }
        if (pass == 2) l.link = obdplan::Link::Off;
        Scene s;
        scene(l, t, pass == 1, false, 3000, &s);
        std::vector<uint16_t> whole(W * H), buf(MAX_PIXELS);
        for (int y = 0; y < H; y += STRIP_H) {
            const int h = y + STRIP_H > H ? H - y : STRIP_H;
            compose(a, k, s, Rect{0, (int16_t)y, W, (int16_t)h}, buf.data());
            memcpy(&whole[y * W], buf.data(), (size_t)h * W * 2);
        }
        if (pass < 2) TEST_ASSERT_EQUAL_HEX16(k.bg[2], whole[2 * W + 2]);   // the panel's own background
        for (int r = 0; r < REGIONS; ++r) {
            const Rect g = region((Region)r);
            TEST_ASSERT_TRUE(g.w * g.h <= MAX_PIXELS);
            TEST_ASSERT_TRUE(g.x >= 0 && g.y >= 0 && g.x + g.w <= W && g.y + g.h <= H);
            compose(a, k, s, g, buf.data());
            for (int y = 0; y < g.h; ++y)
                TEST_ASSERT_EQUAL_MEMORY(&whole[(g.y + y) * W + g.x], &buf[y * g.w], (size_t)g.w * 2);
        }
    }
}

void test_the_art_cel_and_dimming() {
    std::vector<uint8_t> f = file();
    Art a;
    TEST_ASSERT_TRUE(parse(f.data(), f.size(), &a));
    Look k;
    makeLook(a, 5, false, &k);
    Trace t;
    Scene s;
    obdwire::Live l = live();
    l.kmh = 0;   // parked: cel 7, whose column 0 is white
    scene(l, t, false, false, 0, &s);
    uint16_t px[4];
    compose(a, k, s, Rect{ART_X + 59, 1, 2, 1}, px);
    TEST_ASSERT_EQUAL_HEX16(k.art[2], px[0]);   // the cel, past the fade
    TEST_ASSERT_EQUAL_HEX16(k.art[4], px[1]);   // its column 60
    compose(a, k, s, Rect{ART_X + 70, 20, 1, 1}, px);
    TEST_ASSERT_EQUAL_HEX16(k.art[3], px[0]);
    compose(a, k, s, Rect{PANEL_W + FADE_W / 2, 230, 1, 1}, px);   // the fade: neither the panel nor the art
    TEST_ASSERT_TRUE(px[0] != k.art[2] && px[0] != k.bg[230]);
    l.link = obdplan::Link::Off;
    scene(l, t, false, false, 0, &s);
    compose(a, k, s, Rect{ART_X + 60, 1, 1, 1}, px);
    TEST_ASSERT_TRUE(px[0] != k.art[4]);   // dimmed
}

void test_keys_change_only_with_what_a_region_shows() {
    Trace t;
    Scene s1, s2;
    scene(live(), t, false, false, 0, &s1);
    obdwire::Live l = live();
    l.kmh = 63;
    scene(l, t, false, false, 0, &s2);
    TEST_ASSERT_TRUE(key(s1, R_SPEED) != key(s2, R_SPEED));
    TEST_ASSERT_EQUAL_UINT32(key(s1, R_RIB1), key(s2, R_RIB1));
    TEST_ASSERT_EQUAL_UINT32(key(s1, R_TRACE), key(s2, R_TRACE));
    TEST_ASSERT_EQUAL_UINT32(key(s1, R_GEAR), key(s2, R_GEAR));
    // a pulsing alert redraws with the pulse, a calm ribbon doesn't
    scene(live(), t, true, false, 0, &s1);
    scene(live(), t, true, false, 400, &s2);
    TEST_ASSERT_TRUE(key(s1, R_RIB1) != key(s2, R_RIB1));
    TEST_ASSERT_EQUAL_UINT32(key(s1, R_RIB0), key(s2, R_RIB0));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_parse_takes_a_whole_file_and_nothing_else);
    RUN_TEST(test_recolour_moves_hues_and_keeps_skin_and_greys);
    RUN_TEST(test_look_takes_termpal_roles);
    RUN_TEST(test_zones_and_pills);
    RUN_TEST(test_gear_estimate);
    RUN_TEST(test_the_girl_follows_the_speed);
    RUN_TEST(test_trace_samples_four_a_second);
    RUN_TEST(test_scene_from_the_values);
    RUN_TEST(test_regions_draw_what_the_whole_screen_draws);
    RUN_TEST(test_the_art_cel_and_dimming);
    RUN_TEST(test_keys_change_only_with_what_a_region_shows);
    return UNITY_END();
}
