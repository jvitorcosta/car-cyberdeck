#include <unity.h>
#include "../../src/util/black/quietboot.h"

void setUp() {}
void tearDown() {}

using quietboot::MAGIC;

// The S3's self-heal restart leaves a note in RTC memory; the boot after it skips the opening.
void test_quiet_only_after_the_restart_that_left_the_note() {
    TEST_ASSERT_TRUE(quietboot::quiet(MAGIC, true));
    TEST_ASSERT_FALSE(quietboot::quiet(MAGIC, false));        // a crash or power-on: the full opening
    TEST_ASSERT_FALSE(quietboot::quiet(0x12345678u, true));   // an update's or a rollback's restart
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_quiet_only_after_the_restart_that_left_the_note);
    return UNITY_END();
}
