#include <unity.h>
#include <initializer_list>
#include "../../src/util/black/lyricface.h"

using lyricface::Face;

void setUp() {}
void tearDown() {}

void test_faces_shrink() {
    TEST_ASSERT_TRUE(lyricface::advance(Face::SUNG, 'M') > lyricface::advance(Face::SUNG_SMALL, 'M'));
    TEST_ASSERT_TRUE(lyricface::advance(Face::SUNG_SMALL, 'M') > lyricface::advance(Face::LINE, 'M'));
    TEST_ASSERT_TRUE(lyricface::advance(Face::LINE, 'M') > lyricface::advance(Face::SMALL, 'M'));
    TEST_ASSERT_TRUE(lyricface::advance(Face::SMALL, 'M') > 0);
}

void test_latin1_covered() {
    TEST_ASSERT_TRUE(lyricface::advance(Face::SUNG, 0xE7) > 0);   // ç
    TEST_ASSERT_TRUE(lyricface::advance(Face::SUNG, 0xE3) > 0);   // ã
    TEST_ASSERT_EQUAL_INT(0, lyricface::advance(Face::SUNG, 0x10));   // below the font
}

// The yellow deck's VT323 sizes: 36, 24, 19 and 17 px.
void test_line_heights() {
    TEST_ASSERT_TRUE(lyricface::lineHeight(Face::SUNG) >= 37);
    TEST_ASSERT_TRUE(lyricface::lineHeight(Face::SUNG_SMALL) < lyricface::lineHeight(Face::SUNG));
    TEST_ASSERT_TRUE(lyricface::lineHeight(Face::SMALL) < lyricface::lineHeight(Face::LINE));
}

// Falling back to SUNG_SMALL never needs more room than SUNG took.
void test_sung_faces_share_their_room() {
    TEST_ASSERT_EQUAL_INT(3, lyricface::rows(Face::SUNG));
    TEST_ASSERT_EQUAL_INT(3, lyricface::rows(Face::SUNG_SMALL));
    TEST_ASSERT_TRUE(lyricface::rows(Face::SUNG_SMALL) * lyricface::lineHeight(Face::SUNG_SMALL) <=
                     lyricface::rows(Face::SUNG) * lyricface::lineHeight(Face::SUNG));
}


// Every glyph's ink, accents and descenders included, inside its row: from the row's top (ascent
// rows above the baseline) to the line height. The sweep clips to the row and a new line clears it.
void test_every_glyph_inside_its_row() {
    for (Face f : {Face::SUNG, Face::SUNG_SMALL, Face::LINE, Face::SMALL}) {
        const lyricface::Font& ft = lyricface::font(f);
        for (int c = ft.first; c <= ft.last; ++c) {
            const lyricface::Glyph& g = ft.glyph[c - ft.first];
            if (!g.height) continue;
            const int top = lyricface::ascent(f) + g.yOffset;   // rows under the row's top
            TEST_ASSERT_TRUE_MESSAGE(top >= 0, "ink above the row");
            TEST_ASSERT_TRUE_MESSAGE(top + g.height <= lyricface::lineHeight(f), "ink below the row");
        }
    }
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_faces_shrink);
    RUN_TEST(test_latin1_covered);
    RUN_TEST(test_line_heights);
    RUN_TEST(test_sung_faces_share_their_room);
    RUN_TEST(test_every_glyph_inside_its_row);
    return UNITY_END();
}
