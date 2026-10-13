#include <unity.h>
#include <cstdint>
#include <cstring>
#include "../../src/ui/status_sprite.h"

void setUp() {}
void tearDown() {}

static bool rowHasOpaque(int id, int y) {
    for (int x = 0; x < status_sprite::width(id); ++x)
        if (status_sprite::pixel(id, x, y, nullptr)) return true;
    return false;
}
static bool colHasOpaque(int id, int x) {
    for (int y = 0; y < status_sprite::height(id); ++y)
        if (status_sprite::pixel(id, x, y, nullptr)) return true;
    return false;
}
void test_every_sprite_cropped_to_opaque_bounds() {   // every edge of the crop touches the creature
    for (int id = 0; id < status_sprite::COUNT; ++id) {
        TEST_ASSERT_TRUE(status_sprite::width(id) > 0);
        TEST_ASSERT_TRUE(rowHasOpaque(id, 0));
        TEST_ASSERT_TRUE(rowHasOpaque(id, status_sprite::height(id) - 1));
        TEST_ASSERT_TRUE(colHasOpaque(id, 0));
        TEST_ASSERT_TRUE(colHasOpaque(id, status_sprite::width(id) - 1));
    }
}
void test_sprites_fit_the_scene_at_2x() {   // statusscene draws them 2x above the platform
    for (int id = 0; id < status_sprite::COUNT; ++id) {
        TEST_ASSERT_TRUE(status_sprite::width(id) <= 64);
        TEST_ASSERT_TRUE(status_sprite::height(id) <= 54);
    }
}
void test_out_of_range_is_transparent() {
    uint16_t c = 0x1234;
    TEST_ASSERT_FALSE(status_sprite::pixel(status_sprite::PIKACHU, -1, 0, &c));
    TEST_ASSERT_FALSE(status_sprite::pixel(status_sprite::PIKACHU, 0,
                                           status_sprite::height(status_sprite::PIKACHU), &c));
    TEST_ASSERT_EQUAL_HEX16(0x1234, c);   // untouched
}
void test_unknown_id_is_empty() {
    TEST_ASSERT_EQUAL_INT(0, status_sprite::width(-1));
    TEST_ASSERT_EQUAL_INT(0, status_sprite::height(status_sprite::COUNT));
    TEST_ASSERT_FALSE(status_sprite::pixel(status_sprite::COUNT, 0, 0, nullptr));
}
void test_names_and_dex_numbers_for_the_info_box() {
    TEST_ASSERT_EQUAL_STRING("PIKACHU", status_sprite::name(status_sprite::PIKACHU));
    TEST_ASSERT_EQUAL_INT(25, status_sprite::dex(status_sprite::PIKACHU));
    TEST_ASSERT_EQUAL_STRING("SLOWPOKE", status_sprite::name(status_sprite::SLOWPOKE));
    TEST_ASSERT_EQUAL_INT(79, status_sprite::dex(status_sprite::SLOWPOKE));
    for (int id = 0; id < status_sprite::COUNT; ++id)
        TEST_ASSERT_TRUE(strlen(status_sprite::name(id)) <= 10);   // fits the info box at font 2
    TEST_ASSERT_EQUAL_STRING("", status_sprite::name(status_sprite::COUNT));
    TEST_ASSERT_EQUAL_INT(0, status_sprite::dex(-1));
}
int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_every_sprite_cropped_to_opaque_bounds);
    RUN_TEST(test_sprites_fit_the_scene_at_2x);
    RUN_TEST(test_out_of_range_is_transparent);
    RUN_TEST(test_unknown_id_is_empty);
    RUN_TEST(test_names_and_dex_numbers_for_the_info_box);
    return UNITY_END();
}
