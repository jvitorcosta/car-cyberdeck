#include <unity.h>
#include <cstring>
#include "../../src/util/black/lrcmatch.h"

using lrcmatch::Answer;
using lrcstream::Kind;
using lyricstatus::Result;
void setUp() {}
void tearDown() {}

static void base(const char* title, const char* want) {
    char out[96];
    lrcmatch::baseTitle(title, out, sizeof out);
    TEST_ASSERT_EQUAL_STRING(want, out);
}

void test_base_title_cuts_spotify_suffixes() {
    base("Bohemian Rhapsody - Remastered 2011", "Bohemian Rhapsody");
    base("Dreams - 2004 Remaster", "Dreams");
    base("Peaches (feat. Daniel Caesar & Giveon)", "Peaches");
    base("Stay (with Justin Bieber)", "Stay");
    base("Me Negaram Amor - Ao Vivo", "Me Negaram Amor");
    base("Hallelujah - Live At Sin-\xC3\xA9", "Hallelujah");
    base("Evid\xC3\xAAncias [Ao Vivo]", "Evid\xC3\xAAncias");
    base("Wish You Were Here \xE2\x80\x93 2011 Remaster", "Wish You Were Here");
    base("Mood feat. iann dior", "Mood");
    base("Mood FT. iann dior", "Mood");
}
void test_base_title_keeps_what_is_part_of_the_name() {
    base("Blinding Lights", "Blinding Lights");
    base("(I Can't Get No) Satisfaction", "(I Can't Get No) Satisfaction");
    base("Sin-\xC3\xA9", "Sin-\xC3\xA9");
    base("Anti-Hero", "Anti-Hero");
    base("\xE5\xA4\x9C\xE3\x81\xAB\xE9\xA7\x86\xE3\x81\x91\xE3\x82\x8B", "\xE5\xA4\x9C\xE3\x81\xAB\xE9\xA7\x86\xE3\x81\x91\xE3\x82\x8B");
}
void test_base_title_is_bounded() {
    char out[6];
    TEST_ASSERT_EQUAL_size_t(5, lrcmatch::baseTitle("Blinding Lights", out, sizeof out));
    TEST_ASSERT_EQUAL_STRING("Blind", out);
    TEST_ASSERT_EQUAL_size_t(0, lrcmatch::baseTitle(nullptr, out, sizeof out));
    TEST_ASSERT_EQUAL_STRING("", out);
}

void test_same_title_ignores_case_accents_and_punctuation() {
    TEST_ASSERT_TRUE(lrcmatch::sameTitle("Bohemian Rhapsody - Remastered 2011", "Bohemian rhapsody"));
    TEST_ASSERT_TRUE(lrcmatch::sameTitle("Meu Cen\xC3\xA1rio", "Meu Cenario"));
    TEST_ASSERT_TRUE(lrcmatch::sameTitle("Sweet Child O' Mine", "Sweet Child O\xE2\x80\x99 Mine"));
    TEST_ASSERT_TRUE(lrcmatch::sameTitle("Don't Stop Believin'", "Dont Stop Believin"));
    TEST_ASSERT_TRUE(lrcmatch::sameTitle("Sultans Of Swing - Remastered 2024", "Sultans of Swing (2010 Remaster)"));
    TEST_ASSERT_TRUE(lrcmatch::sameTitle("Tit\xC3\xAD Me Pregunt\xC3\xB3", "TITI ME PREGUNTO"));
}
void test_same_title_rejects_other_songs() {
    TEST_ASSERT_FALSE(lrcmatch::sameTitle("Love", "Love Story"));
    TEST_ASSERT_FALSE(lrcmatch::sameTitle("Stay", "Stay With Me"));
    TEST_ASSERT_FALSE(lrcmatch::sameTitle("Me Leva", "Leva Eu"));
}
void test_same_title_other_scripts_compare_as_they_are() {
    const char* a = "\xE5\xA4\x9C\xE3\x81\xAB\xE9\xA7\x86\xE3\x81\x91\xE3\x82\x8B";
    const char* b = "\xE7\xBA\xA2\xE8\x8E\xB2";
    TEST_ASSERT_TRUE(lrcmatch::sameTitle(a, a));
    TEST_ASSERT_FALSE(lrcmatch::sameTitle(a, b));   // would be "" = "" with a plain ASCII fold
}
void test_same_title_with_nothing_to_compare_passes() {
    TEST_ASSERT_TRUE(lrcmatch::sameTitle("", "Anything"));
    TEST_ASSERT_TRUE(lrcmatch::sameTitle("!!!", "Anything"));
    TEST_ASSERT_TRUE(lrcmatch::sameTitle(nullptr, "Anything"));
}

static const Answer MISS{404, false, Kind::None};
static const Answer BUSY{503, false, Kind::None};
static const Answer PLAIN{200, false, Kind::Plain};
static const Answer SYNCED{200, false, Kind::Synced};
static const Answer EMPTY{200, false, Kind::None};

void test_search_runs_unless_get_settled_it() {
    TEST_ASSERT_TRUE(lrcmatch::needSearch(MISS));
    TEST_ASSERT_TRUE(lrcmatch::needSearch(PLAIN));   // "I Heard It Through The Grapevine"
    TEST_ASSERT_TRUE(lrcmatch::needSearch(EMPTY));
    TEST_ASSERT_TRUE(lrcmatch::needSearch(BUSY));
    TEST_ASSERT_FALSE(lrcmatch::needSearch(SYNCED));
    TEST_ASSERT_FALSE(lrcmatch::needSearch({200, false, Kind::Instrumental}));
    TEST_ASSERT_FALSE(lrcmatch::needSearch({429, false, Kind::None}));   // rate limit: back off
    TEST_ASSERT_FALSE(lrcmatch::needSearch({-1, false, Kind::None}));    // no network
    TEST_ASSERT_FALSE(lrcmatch::needSearch({200, true, Kind::None}));    // captive portal
}
void test_settle_get_alone() {
    TEST_ASSERT_EQUAL_INT((int)Result::Synced, (int)lrcmatch::settle(SYNCED, nullptr, false));
    TEST_ASSERT_EQUAL_INT((int)Result::Instrumental, (int)lrcmatch::settle({200, false, Kind::Instrumental}, nullptr, false));
    TEST_ASSERT_EQUAL_INT((int)Result::TempError, (int)lrcmatch::settle({429, false, Kind::None}, nullptr, false));
    TEST_ASSERT_EQUAL_INT((int)Result::TempError, (int)lrcmatch::settle({200, true, Kind::None}, nullptr, false));
}
void test_settle_search_upgrades_plain_to_synced() {
    TEST_ASSERT_EQUAL_INT((int)Result::Synced, (int)lrcmatch::settle(PLAIN, &SYNCED, false));
}
void test_settle_keeps_plain_when_the_search_has_nothing_better() {
    TEST_ASSERT_EQUAL_INT((int)Result::Plain, (int)lrcmatch::settle(PLAIN, &PLAIN, false));
    const Answer none{404, false, Kind::Plain};            // kept lines count as the search's
    TEST_ASSERT_EQUAL_INT((int)Result::Plain, (int)lrcmatch::settle(PLAIN, &none, false));
    const Answer instrumental{200, false, Kind::Instrumental};
    TEST_ASSERT_EQUAL_INT((int)Result::Plain, (int)lrcmatch::settle(PLAIN, &instrumental, false));
}
void test_settle_a_busy_get_can_still_find_synced() {
    TEST_ASSERT_EQUAL_INT((int)Result::Synced, (int)lrcmatch::settle(BUSY, &SYNCED, false));
}
void test_settle_a_busy_get_with_no_search_result_retries() {
    TEST_ASSERT_EQUAL_INT((int)Result::TempError, (int)lrcmatch::settle(BUSY, &EMPTY, false));
    TEST_ASSERT_EQUAL_INT((int)Result::TempError, (int)lrcmatch::settle(BUSY, &MISS, true));
    TEST_ASSERT_EQUAL_INT((int)Result::TempError, (int)lrcmatch::settle(BUSY, &PLAIN, false));
    TEST_ASSERT_EQUAL_INT((int)Result::Plain, (int)lrcmatch::settle(BUSY, &PLAIN, true));
}
void test_settle_a_failed_search_retries_then_shows_the_plain_it_has() {
    const Answer failed{503, false, Kind::Plain};
    TEST_ASSERT_EQUAL_INT((int)Result::TempError, (int)lrcmatch::settle(PLAIN, &failed, false));
    TEST_ASSERT_EQUAL_INT((int)Result::Plain, (int)lrcmatch::settle(PLAIN, &failed, true));
}
void test_settle_a_stalled_search_never_shows_a_cut_synced_body() {
    const Answer cut{200, true, Kind::Synced};
    TEST_ASSERT_EQUAL_INT((int)Result::TempError, (int)lrcmatch::settle(PLAIN, &cut, false));
    TEST_ASSERT_EQUAL_INT((int)Result::TempError, (int)lrcmatch::settle(PLAIN, &cut, true));
}
void test_settle_nothing_anywhere_is_not_found() {
    TEST_ASSERT_EQUAL_INT((int)Result::NotFound, (int)lrcmatch::settle(MISS, &EMPTY, false));
    TEST_ASSERT_EQUAL_INT((int)Result::NotFound, (int)lrcmatch::settle(MISS, &MISS, false));
    const Answer instrumental{200, false, Kind::Instrumental};
    TEST_ASSERT_EQUAL_INT((int)Result::Instrumental, (int)lrcmatch::settle(MISS, &instrumental, false));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_base_title_cuts_spotify_suffixes);
    RUN_TEST(test_base_title_keeps_what_is_part_of_the_name);
    RUN_TEST(test_base_title_is_bounded);
    RUN_TEST(test_same_title_ignores_case_accents_and_punctuation);
    RUN_TEST(test_same_title_rejects_other_songs);
    RUN_TEST(test_same_title_other_scripts_compare_as_they_are);
    RUN_TEST(test_same_title_with_nothing_to_compare_passes);
    RUN_TEST(test_search_runs_unless_get_settled_it);
    RUN_TEST(test_settle_get_alone);
    RUN_TEST(test_settle_search_upgrades_plain_to_synced);
    RUN_TEST(test_settle_keeps_plain_when_the_search_has_nothing_better);
    RUN_TEST(test_settle_a_busy_get_can_still_find_synced);
    RUN_TEST(test_settle_a_busy_get_with_no_search_result_retries);
    RUN_TEST(test_settle_a_failed_search_retries_then_shows_the_plain_it_has);
    RUN_TEST(test_settle_a_stalled_search_never_shows_a_cut_synced_body);
    RUN_TEST(test_settle_nothing_anywhere_is_not_found);
    return UNITY_END();
}
