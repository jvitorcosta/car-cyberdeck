#include <unity.h>
#include <cstdint>
#include <cstring>
#include "../../src/ui/qrcard.h"
#include "../../src/ui/statusscene.h"
#include "../../src/ui/status_sprite.h"
#include "../../src/util/black/idlesprite.h"

using qrcard::Rect;

void setUp() {}
void tearDown() {}

template <class A, class B> static bool overlap(const A& a, const B& b) {
    return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h;
}

// The card: a battle box from x 4, y 22, its shadow included in what erasing repaints, under the
// top strip and clear of the dialogue box.
void test_card_sits_top_left_between_the_strip_and_the_dialogue() {
    const Rect f = qrcard::FOOTPRINT;
    TEST_ASSERT_EQUAL_INT(4, f.x);
    TEST_ASSERT_EQUAL_INT(22, f.y);
    TEST_ASSERT_EQUAL_INT(127, f.x + f.w - 1);   // x 4..127
    TEST_ASSERT_EQUAL_INT(149, f.y + f.h - 1);   // y 22..149
    TEST_ASSERT_TRUE(f.y >= 20);                                   // under the top strip
    TEST_ASSERT_TRUE(f.y + f.h <= statusscene::DLG_Y);             // above the dialogue box
    TEST_ASSERT_EQUAL_INT(qrcard::BOX.w + qrcard::SHADOW, f.w);
    TEST_ASSERT_EQUAL_INT(qrcard::BOX.h + qrcard::SHADOW, f.h);
}

// The panel holds a version 3 code at 3 px a module with a 2-module quiet zone, inside the
// box's fill: clear of the border and of the slanted right edge at its narrowest (the bottom).
void test_panel_fits_inside_the_box_fill() {
    const Rect b = qrcard::BOX, p = qrcard::PANEL_AT;
    TEST_ASSERT_EQUAL_INT((29 + 2 * 2) * 3, p.w);
    TEST_ASSERT_EQUAL_INT(p.w, p.h);
    TEST_ASSERT_TRUE(p.x >= b.x + qrcard::BORDER + 2);
    TEST_ASSERT_TRUE(p.y >= b.y + qrcard::BORDER + 16);            // under the name line
    TEST_ASSERT_TRUE(p.y + p.h <= b.y + b.h - qrcard::BORDER - 2);
    TEST_ASSERT_TRUE(p.x + p.w <= b.x + b.w - qrcard::TAB - qrcard::BORDER - 2);
}

// The name line: the phone icon (12 px) then "REMOTE" in font 2 (16 px tall, 50 px wide), above
// the panel and left of the slant.
void test_name_line_above_the_panel() {
    const Rect b = qrcard::BOX, p = qrcard::PANEL_AT;
    TEST_ASSERT_TRUE(qrcard::ICON_X >= b.x + qrcard::BORDER && qrcard::ICON_Y >= b.y + qrcard::BORDER);
    TEST_ASSERT_TRUE(qrcard::ICON_Y + 12 <= p.y);
    TEST_ASSERT_TRUE(qrcard::LABEL_X >= qrcard::ICON_X + 12 + 2);
    TEST_ASSERT_TRUE(qrcard::LABEL_Y >= b.y + qrcard::BORDER && qrcard::LABEL_Y + 16 <= p.y);
    TEST_ASSERT_TRUE(qrcard::LABEL_X + 50 <= b.x + b.w - qrcard::TAB - qrcard::BORDER);
}

// The code is centred on the panel: a version 3 code gets exactly the 2-module quiet zone, a
// smaller one a wider zone; one the panel can't hold has no place.
void test_code_centred_on_the_panel() {
    TEST_ASSERT_EQUAL_INT(2 * 3, qrcard::offset(29));
    TEST_ASSERT_EQUAL_INT(12, qrcard::offset(25));
    TEST_ASSERT_EQUAL_INT(18, qrcard::offset(21));
    TEST_ASSERT_EQUAL_INT(-1, qrcard::offset(33));
    TEST_ASSERT_EQUAL_INT(-1, qrcard::offset(0));
}

// Modules row-major, MSB first, 1 = dark; outside the code reads light.
void test_dark_reads_the_bits() {
    static uint8_t bits[(21 * 21 + 7) / 8];
    memset(bits, 0, sizeof bits);
    bits[0] = 0x80;                                     // (0,0)
    const int i = 20 * 21 + 20;                         // (20,20)
    bits[i >> 3] |= (uint8_t)(0x80 >> (i & 7));
    const qrcard::Qr qr{bits, 21};
    TEST_ASSERT_TRUE(qrcard::dark(qr, 0, 0));
    TEST_ASSERT_FALSE(qrcard::dark(qr, 1, 0));
    TEST_ASSERT_TRUE(qrcard::dark(qr, 20, 20));
    TEST_ASSERT_FALSE(qrcard::dark(qr, 21, 0));
    TEST_ASSERT_FALSE(qrcard::dark(qr, -1, 0));
    TEST_ASSERT_FALSE(qrcard::dark(qrcard::Qr{nullptr, 21}, 0, 0));
}

static qrcard::Rect asCard(const statusscene::Rect& r) { return {r.x, r.y, r.w, r.h}; }

// The card is drawn once; the frames repaint only the waiting figure's box beside it. Sonic
// (data/idle.bin: 32 x 39, 2x from x 128) never reaches it: his box stays whole.
static uint8_t s_idle[8 + 32 + 4 * (32 / 2) * 39];
void test_sonics_frames_never_reach_the_card() {
    memset(s_idle, 0, sizeof s_idle);
    memcpy(s_idle, "IDL1", 4);
    s_idle[4] = 4; s_idle[5] = 32; s_idle[6] = 39;
    TEST_ASSERT_TRUE(idlesprite::parse(s_idle, sizeof s_idle));
    const statusscene::Look sonic = statusscene::lookFor(linkstatus::Kind::Ok);
    TEST_ASSERT_TRUE(sonic.idle);
    const qrcard::Rect r = asCard(statusscene::frameRect(sonic));
    TEST_ASSERT_FALSE(overlap(r, qrcard::FOOTPRINT));
    const qrcard::Rect b = qrcard::beside(r);
    TEST_ASSERT_EQUAL_INT(r.x, b.x);
    TEST_ASSERT_EQUAL_INT(r.w, b.w);
    TEST_ASSERT_EQUAL_INT(r.y, b.y);
    TEST_ASSERT_EQUAL_INT(r.h, b.h);
}

// Pikachu (no Sonic file; 39 px wide, 2x from x 121) has his tail under the card's right edge:
// his frames repaint only what lies right of it, and the card stays on top.
void test_pikachus_frames_stop_at_the_card() {
    idlesprite::parse(nullptr, 0);
    const statusscene::Look pika = statusscene::lookFor(linkstatus::Kind::Ok);
    TEST_ASSERT_FALSE(pika.idle);
    const qrcard::Rect r = asCard(statusscene::frameRect(pika));
    TEST_ASSERT_TRUE(overlap(r, qrcard::FOOTPRINT));
    const qrcard::Rect b = qrcard::beside(r);
    TEST_ASSERT_FALSE(overlap(b, qrcard::FOOTPRINT));
    TEST_ASSERT_EQUAL_INT(qrcard::FOOTPRINT.x + qrcard::FOOTPRINT.w, b.x);
    TEST_ASSERT_EQUAL_INT(r.x + r.w, b.x + b.w);   // same right edge, rows untouched
    TEST_ASSERT_EQUAL_INT(r.y, b.y);
    TEST_ASSERT_EQUAL_INT(r.h, b.h);
}

// Below or above the card a box keeps its columns.
void test_beside_leaves_boxes_clear_of_the_card_alone() {
    const qrcard::Rect below = {40, 150, 100, 10};
    TEST_ASSERT_EQUAL_INT(40, qrcard::beside(below).x);
    TEST_ASSERT_EQUAL_INT(100, qrcard::beside(below).w);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_card_sits_top_left_between_the_strip_and_the_dialogue);
    RUN_TEST(test_panel_fits_inside_the_box_fill);
    RUN_TEST(test_name_line_above_the_panel);
    RUN_TEST(test_code_centred_on_the_panel);
    RUN_TEST(test_dark_reads_the_bits);
    RUN_TEST(test_sonics_frames_never_reach_the_card);
    RUN_TEST(test_pikachus_frames_stop_at_the_card);
    RUN_TEST(test_beside_leaves_boxes_clear_of_the_card_alone);
    return UNITY_END();
}
