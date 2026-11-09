#include <unity.h>
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

void test_line_heights() {
    TEST_ASSERT_TRUE(lyricface::lineHeight(Face::SUNG) >= 22 && lyricface::lineHeight(Face::SUNG) <= 30);
    TEST_ASSERT_TRUE(lyricface::lineHeight(Face::SUNG_SMALL) < lyricface::lineHeight(Face::SUNG));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_faces_shrink);
    RUN_TEST(test_latin1_covered);
    RUN_TEST(test_line_heights);
    return UNITY_END();
}
