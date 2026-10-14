#include <unity.h>
#include <cstring>
#include <vector>
#include "../../src/util/avatarbin.h"

void setUp() {}
void tearDown() {}

// "AVT1", n, 0, name[12], n x (id, frames), frames (each FRAME_BYTES, filled with its index)
static std::vector<uint8_t> blob(const char* name, std::vector<std::pair<uint8_t, uint8_t>> moods) {
    std::vector<uint8_t> b = {'A', 'V', 'T', '1', (uint8_t)moods.size(), 0};
    char nm[avatarbin::NAME_LEN] = {};
    strncpy(nm, name, sizeof nm);
    b.insert(b.end(), nm, nm + sizeof nm);
    for (auto& m : moods) { b.push_back(m.first); b.push_back(m.second); }
    int k = 0;
    for (auto& m : moods)
        for (int f = 0; f < m.second; ++f, ++k) b.insert(b.end(), avatarbin::FRAME_BYTES, (uint8_t)k);
    return b;
}

void test_parses_moods_and_offsets() {
    auto b = blob("ZERO TWO", {{0, 1}, {1, 2}, {2, 1}});
    avatarbin::Info in;
    const char* why = "x";
    TEST_ASSERT_TRUE(avatarbin::parse(b.data(), b.size(), in, &why));
    TEST_ASSERT_EQUAL_STRING("", why);
    TEST_ASSERT_EQUAL_INT(3, in.moods);
    TEST_ASSERT_EQUAL_STRING("ZERO TWO", in.name);
    TEST_ASSERT_EQUAL_UINT8(2, in.frames[1]);
    const uint32_t head = 18 + 2 * 3;
    TEST_ASSERT_EQUAL_UINT32(head, avatarbin::frameOffset(in, 0, 0));
    TEST_ASSERT_EQUAL_UINT32(head + 2 * avatarbin::FRAME_BYTES, avatarbin::frameOffset(in, 1, 1));
    TEST_ASSERT_EQUAL_UINT8(2, b[avatarbin::frameOffset(in, 1, 1)]);   // third frame overall
    TEST_ASSERT_EQUAL_UINT8(3, b[avatarbin::frameOffset(in, 2, 0)]);
}
void test_twelve_char_name_is_terminated() {
    auto b = blob("ABCDEFGHIJKL", {{0, 1}});
    avatarbin::Info in;
    TEST_ASSERT_TRUE(avatarbin::parse(b.data(), b.size(), in, nullptr));
    TEST_ASSERT_EQUAL_STRING("ABCDEFGHIJKL", in.name);
}
void test_find_falls_back_to_first_mood() {
    auto b = blob("Z", {{2, 1}, {1, 2}});
    avatarbin::Info in;
    TEST_ASSERT_TRUE(avatarbin::parse(b.data(), b.size(), in, nullptr));
    TEST_ASSERT_EQUAL_INT(1, avatarbin::find(in, avatarbin::Talk));
    TEST_ASSERT_EQUAL_INT(0, avatarbin::find(in, avatarbin::Smug));   // not in the file
}
void test_frame_offset_clamps() {
    auto b = blob("Z", {{0, 2}});
    avatarbin::Info in;
    TEST_ASSERT_TRUE(avatarbin::parse(b.data(), b.size(), in, nullptr));
    TEST_ASSERT_EQUAL_UINT32(avatarbin::frameOffset(in, 0, 1), avatarbin::frameOffset(in, 0, 5));
    TEST_ASSERT_EQUAL_UINT32(avatarbin::frameOffset(in, 0, 0), avatarbin::frameOffset(in, 0, -1));
    TEST_ASSERT_EQUAL_UINT32(avatarbin::frameOffset(in, 0, 0), avatarbin::frameOffset(in, 7, 0));
}
static void rejects(const std::vector<uint8_t>& b, const char* reason) {
    avatarbin::Info in;
    in.moods = 5;
    const char* why = nullptr;
    TEST_ASSERT_FALSE(avatarbin::parse(b.data(), b.size(), in, &why));
    TEST_ASSERT_EQUAL_INT(0, in.moods);
    TEST_ASSERT_EQUAL_STRING(reason, why);
}
// Review Focus 1: the build fallback (an empty file) and broken files turn the feature off.
void test_empty_file_from_the_build_fallback() {
    std::vector<uint8_t> b = {'A', 'V', 'T', '1'};
    b.resize(18, 0);
    rejects(b, "no moods");
}
void test_bad_files() {
    rejects({}, "no file");
    auto b = blob("Z", {{0, 1}});
    b[0] = 'X';
    rejects(b, "bad magic");
    b = blob("Z", {{0, 1}});
    b.pop_back();
    rejects(b, "size mismatch");          // a gen_avatar run cut off half way
    b = blob("Z", {{0, 1}});
    b.push_back(0);
    rejects(b, "size mismatch");
    b = blob("Z", {{0, 1}});
    b[19] = 0;                            // frame count 0
    rejects(b, "bad frame count");
    b[19] = 9;
    rejects(b, "bad frame count");
    b = blob("Z", {{0, 1}});
    b[4] = 9;                             // 9 moods
    rejects(b, "too many moods");
    b = blob("Z", {{0, 1}});
    b[4] = 3;                             // claims 3 moods, header too short for them
    b.resize(20);
    rejects(b, "short header");
}
void test_null_blob() {
    avatarbin::Info in;
    TEST_ASSERT_FALSE(avatarbin::parse(nullptr, 100, in, nullptr));
    TEST_ASSERT_EQUAL_INT(0, in.moods);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_parses_moods_and_offsets);
    RUN_TEST(test_twelve_char_name_is_terminated);
    RUN_TEST(test_find_falls_back_to_first_mood);
    RUN_TEST(test_frame_offset_clamps);
    RUN_TEST(test_empty_file_from_the_build_fallback);
    RUN_TEST(test_bad_files);
    RUN_TEST(test_null_blob);
    return UNITY_END();
}
