#include <unity.h>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <string>
#include <vector>
#include "../../src/util/car/triplog.h"

void setUp() {}
void tearDown() {}

// Lists by name, like LittleFS: the names themselves must sort oldest first.
struct RamStore : triplog::Store {
    std::vector<std::pair<std::string, std::string>> files;
    int failAppends = 0;  // the next N appends fail
    int readsLeft = -1;   // >= 0: after that many good reads, every read comes back short
    bool failTruncate = false, failRemove = false;
    long total = 1024 * 1024;  // the partition; the cap tests use the deck's 128 KB
    int failAppendsWhileOver = 0;  // appends fail while usedBytes() > this (0: never)
    std::string* find(const char* name) {
        for (auto& f : files) if (f.first == name) return &f.second;
        return nullptr;
    }
    void put(const char* name, const std::string& data) {
        if (std::string* s = find(name)) *s = data;
        else files.push_back({name, data});
    }
    bool append(const char* name, const char* data, size_t n) override {
        if (failAppends > 0) { failAppends--; return false; }
        if (failAppendsWhileOver > 0 && usedBytes() > failAppendsWhileOver) return false;
        std::string* s = find(name);
        if (!s) { files.push_back({name, ""}); s = &files.back().second; }
        s->append(data, n);
        return true;
    }
    long size(const char* name) override {
        std::string* s = find(name);
        return s ? (long)s->size() : -1;
    }
    size_t read(const char* name, long at, char* out, size_t n) override {
        std::string* s = find(name);
        if (!s || at < 0 || (size_t)at >= s->size()) return 0;
        size_t got = s->size() - (size_t)at;
        if (got > n) got = n;
        if (readsLeft == 0) got /= 2;  // a flash read error: fewer bytes than asked
        else if (readsLeft > 0) readsLeft--;
        memcpy(out, s->data() + at, got);
        return got;
    }
    bool truncate(const char* name, long len) override {
        std::string* s = find(name);
        if (!s || failTruncate) return false;
        s->resize((size_t)len);
        return true;
    }
    bool remove(const char* name) override {
        if (failRemove) return false;
        for (size_t i = 0; i < files.size(); i++)
            if (files[i].first == name) { files.erase(files.begin() + (long)i); return true; }
        return false;
    }
    // LittleFS: a file of 3+ rows takes whole 4 KB blocks; the file system itself keeps 2.
    long usedBytes() override {
        long blocks = 2;
        for (auto& f : files) blocks += ((long)f.second.size() + 4095) / 4096;
        return blocks * 4096;
    }
    long totalBytes() override { return total; }
    int list(char (*names)[24], int max, int skip) override {
        std::vector<std::string> sorted;
        for (auto& f : files) sorted.push_back(f.first);
        std::sort(sorted.begin(), sorted.end());
        int n = 0;
        for (size_t i = (size_t)skip; i < sorted.size(); i++) {
            const std::string& f = sorted[i];
            if (n >= max) break;
            strncpy(names[n], f.c_str(), 23);
            names[n][23] = '\0';
            n++;
        }
        return n;
    }
};

void test_flush_appends_queued_lines() {
    RamStore st;
    triplog::Log log(st);
    log.open(42);
    log.queue("{\"t\":1}\n");
    log.queue("{\"t\":2}\n");
    TEST_ASSERT_TRUE(log.flush());
    TEST_ASSERT_EQUAL_STRING("{\"t\":1}\n{\"t\":2}\n", st.find("/t000042.jsonl")->c_str());
    TEST_ASSERT_EQUAL_INT(0, log.queued());
}

void test_flush_before_open_does_nothing() {
    RamStore st;
    triplog::Log log(st);
    log.queue("{}\n");
    TEST_ASSERT_FALSE(log.flush());
    TEST_ASSERT_EQUAL_INT(1, log.queued());
    TEST_ASSERT_EQUAL_INT(0, (int)st.files.size());
}

void test_cut_row_is_dropped() {
    RamStore st;
    st.put("/t000001.jsonl", "{\"t\":\"a\"}\n{\"t\":\"x\",\"m");
    st.put("/t000002.jsonl", "{\"t\":\"b\"}\n");
    triplog::Log log(st);
    TEST_ASSERT_TRUE(log.recover());
    TEST_ASSERT_EQUAL_STRING("{\"t\":\"a\"}\n", st.find("/t000001.jsonl")->c_str());
    TEST_ASSERT_EQUAL_STRING("{\"t\":\"b\"}\n", st.find("/t000002.jsonl")->c_str());
    TEST_ASSERT_FALSE(log.recover());
}

void test_cut_row_longer_than_a_chunk_and_no_newline_at_all() {
    RamStore st;
    st.put("/t000001.jsonl", "{\"t\":1}\n" + std::string(200, 'x'));
    st.put("/t000002.jsonl", std::string(100, 'y'));
    triplog::Log log(st);
    TEST_ASSERT_TRUE(log.recover());
    TEST_ASSERT_EQUAL_STRING("{\"t\":1}\n", st.find("/t000001.jsonl")->c_str());
    TEST_ASSERT_NULL(st.find("/t000002.jsonl"));
}

void test_cap_drops_oldest_trip_first() {
    RamStore st;
    st.put("/t000001.jsonl", std::string(40 * 1024, 'a'));
    st.put("/t000002.jsonl", std::string(40 * 1024, 'b'));
    st.put("/t000003.jsonl", std::string(40 * 1024, 'c'));
    triplog::Log log(st);
    log.open(3);
    log.enforceCap();
    TEST_ASSERT_NULL(st.find("/t000001.jsonl"));
    TEST_ASSERT_NOT_NULL(st.find("/t000002.jsonl"));
    TEST_ASSERT_NOT_NULL(st.find("/t000003.jsonl"));
}

void test_cap_never_removes_the_open_file() {
    RamStore st;
    st.put("/t000001.jsonl", std::string(100 * 1024, 'a'));
    st.put("/t000002.jsonl", std::string(10 * 1024, 'b'));
    triplog::Log log(st);
    log.open(1);
    log.enforceCap();
    TEST_ASSERT_NOT_NULL(st.find("/t000001.jsonl"));
    TEST_ASSERT_NULL(st.find("/t000002.jsonl"));
}

void test_flush_failure_keeps_queue() {
    RamStore st;
    triplog::Log log(st);
    log.open(7);
    log.queue("a\n");
    log.queue("b\n");
    log.queue("c\n");
    st.failAppends = 1;
    TEST_ASSERT_FALSE(log.flush());
    TEST_ASSERT_EQUAL_INT(3, log.queued());
    TEST_ASSERT_TRUE(log.flush());
    TEST_ASSERT_EQUAL_STRING("a\nb\nc\n", st.find("/t000007.jsonl")->c_str());
}

void test_flush_stops_midway_and_keeps_the_rest() {
    RamStore st;
    triplog::Log log(st);
    log.open(7);
    log.queue("a\n");
    log.queue("b\n");
    TEST_ASSERT_TRUE(log.flush());
    log.queue("c\n");
    log.queue("d\n");
    st.failAppends = 1;
    TEST_ASSERT_FALSE(log.flush());
    TEST_ASSERT_EQUAL_INT(2, log.queued());
    TEST_ASSERT_TRUE(log.flush());
    TEST_ASSERT_EQUAL_STRING("a\nb\nc\nd\n", st.find("/t000007.jsonl")->c_str());
}

void test_full_queue_drops_oldest_and_counts() {
    RamStore st;
    triplog::Log log(st);
    log.open(1);
    for (int i = 0; i < 10; i++) {
        char l[8];
        snprintf(l, sizeof(l), "%d\n", i);
        log.queue(l);
    }
    TEST_ASSERT_EQUAL_INT(8, log.queued());
    TEST_ASSERT_EQUAL_INT(2, log.dropped());
    TEST_ASSERT_TRUE(log.flush());
    TEST_ASSERT_EQUAL_STRING("2\n3\n4\n5\n6\n7\n8\n9\n", st.find("/t000001.jsonl")->c_str());
}

void test_oversize_line_is_dropped_and_counted() {
    RamStore st;
    triplog::Log log(st);
    log.open(1);
    std::string big(256, 'x');
    log.queue(big.c_str());
    TEST_ASSERT_EQUAL_INT(0, log.queued());
    TEST_ASSERT_EQUAL_INT(1, log.dropped());
    std::string ok(255, 'y');
    log.queue(ok.c_str());
    TEST_ASSERT_EQUAL_INT(1, log.queued());
}

void test_file_name_is_the_zero_padded_boot_sequence() {
    RamStore st;
    triplog::Log log(st);
    log.open(4294967295u);  // the largest sequence still fits the 24-byte name
    log.queue("a\n");
    TEST_ASSERT_TRUE(log.flush());
    TEST_ASSERT_NOT_NULL(st.find("/t4294967295.jsonl"));
}

// Trips 9 and 10: unpadded, "/t10" lists before "/t9" and the cap would drop the newer trip.
void test_names_sort_by_age_so_the_cap_drops_the_oldest() {
    RamStore st;
    triplog::Log log(st);
    log.open(9);
    log.queue("a\n");
    TEST_ASSERT_TRUE(log.flush());
    log.open(10);
    log.queue("b\n");
    TEST_ASSERT_TRUE(log.flush());
    st.put("/t000009.jsonl", std::string(60 * 1024, 'a') + "\n");
    st.put("/t000010.jsonl", std::string(60 * 1024, 'b') + "\n");
    log.open(11);
    log.enforceCap();
    TEST_ASSERT_NULL(st.find("/t000009.jsonl"));
    TEST_ASSERT_NOT_NULL(st.find("/t000010.jsonl"));
}

// A flash read error mid-walk must not look like "no newline at all" (which removes the file).
void test_read_error_leaves_the_file_alone() {
    RamStore st;
    const std::string data = "{\"t\":1}\n" + std::string(200, 'x');
    st.put("/t000001.jsonl", data);
    st.readsLeft = 1;  // the last-byte check reads fine, the walk back does not
    triplog::Log log(st);
    TEST_ASSERT_FALSE(log.recover());
    TEST_ASSERT_NOT_NULL(st.find("/t000001.jsonl"));
    TEST_ASSERT_EQUAL_STRING(data.c_str(), st.find("/t000001.jsonl")->c_str());
}

void test_failed_truncate_or_remove_is_not_a_change() {
    RamStore st;
    st.put("/t000001.jsonl", "{\"t\":1}\n{\"cut");
    st.put("/t000002.jsonl", "no newline");
    st.failTruncate = true;
    st.failRemove = true;
    triplog::Log log(st);
    TEST_ASSERT_FALSE(log.recover());
    TEST_ASSERT_EQUAL_STRING("{\"t\":1}\n{\"cut", st.find("/t000001.jsonl")->c_str());
    TEST_ASSERT_EQUAL_STRING("no newline", st.find("/t000002.jsonl")->c_str());
    st.failTruncate = false;
    st.failRemove = false;
    TEST_ASSERT_TRUE(log.recover());
    TEST_ASSERT_EQUAL_STRING("{\"t\":1}\n", st.find("/t000001.jsonl")->c_str());
    TEST_ASSERT_NULL(st.find("/t000002.jsonl"));
}

void test_many_files_are_all_recovered_and_capped() {
    RamStore st;
    char nm[24];
    for (int i = 1; i <= 24; i++) {
        snprintf(nm, sizeof(nm), "/t%06d.jsonl", i);
        st.put(nm, std::string(5 * 1024 - 1, 'a') + "\n");  // 120 KB in all, over the cap
    }
    st.put("/t000024.jsonl", std::string(5 * 1024 - 1, 'a') + "\n" + "{\"cut");
    triplog::Log log(st);
    log.open(24);
    TEST_ASSERT_TRUE(log.recover());
    TEST_ASSERT_EQUAL_INT(24, (int)st.files.size());
    TEST_ASSERT_EQUAL_INT(5 * 1024, (int)st.find("/t000024.jsonl")->size());
    TEST_ASSERT_EQUAL('\n', st.find("/t000024.jsonl")->back());
    log.enforceCap();
    long total = 0;
    for (auto& f : st.files) total += (long)f.second.size();
    TEST_ASSERT_TRUE(total <= triplog::CAP_BYTES);
    TEST_ASSERT_NULL(st.find("/t000001.jsonl"));
    TEST_ASSERT_NOT_NULL(st.find("/t000024.jsonl"));
}

void test_cap_sums_every_page_and_drops_oldest_first() {
    RamStore st;
    char nm[24];
    for (int i = 1; i <= 24; i++) {
        snprintf(nm, sizeof(nm), "/t%06d.jsonl", i);
        st.put(nm, std::string(5 * 1024 - 1, 'a') + "\n");
    }
    triplog::Log log(st);
    log.open(24);
    log.enforceCap();
    long total = 0;
    for (auto& f : st.files) total += (long)f.second.size();
    TEST_ASSERT_TRUE(total <= triplog::CAP_BYTES);
    TEST_ASSERT_TRUE(total > triplog::CAP_BYTES - 5 * 1024);
    TEST_ASSERT_NULL(st.find("/t000001.jsonl"));
    TEST_ASSERT_NOT_NULL(st.find("/t000024.jsonl"));
    TEST_ASSERT_EQUAL_STRING("/t000006.jsonl", st.files[0].first.c_str());
}

// The deck's 128 KB partition: a short trip is a few hundred bytes of rows but a whole 4 KB block.
void test_cap_keeps_real_usage_under_70_percent() {
    RamStore st;
    st.total = 128 * 1024;
    char nm[24];
    for (int i = 1; i <= 18; i++) {
        snprintf(nm, sizeof(nm), "/t%06d.jsonl", i);
        st.put(nm, std::string(499, 'a') + "\n");
    }
    triplog::Log log(st);
    log.open(18);
    TEST_ASSERT_EQUAL_INT(0, log.enforceCap());   // 20 blocks of 32: 62 %
    snprintf(nm, sizeof(nm), "/t%06d.jsonl", 19);
    st.put(nm, std::string(5000, 'b'));          // 2 blocks: 22 of 32 is 69 %, still under
    TEST_ASSERT_EQUAL_INT(0, log.enforceCap());
    snprintf(nm, sizeof(nm), "/t%06d.jsonl", 20);
    st.put(nm, std::string(5000, 'c'));          // 24 of 32: 75 %
    TEST_ASSERT_EQUAL_INT(2, log.enforceCap());   // two one-block trips go
    TEST_ASSERT_TRUE(st.usedBytes() * 10 <= st.totalBytes() * 7);
    TEST_ASSERT_NULL(st.find("/t000001.jsonl"));
    TEST_ASSERT_NULL(st.find("/t000002.jsonl"));
    TEST_ASSERT_NOT_NULL(st.find("/t000003.jsonl"));
    TEST_ASSERT_NOT_NULL(st.find("/t000018.jsonl"));
}

void test_cap_keeps_at_most_20_files() {
    RamStore st;
    char nm[24];
    for (int i = 1; i <= 25; i++) {
        snprintf(nm, sizeof(nm), "/t%06d.jsonl", i);
        st.put(nm, "{}\n");
    }
    triplog::Log log(st);
    log.open(25);
    TEST_ASSERT_EQUAL_INT(5, log.enforceCap());
    TEST_ASSERT_EQUAL_INT(triplog::MAX_FILES, (int)st.files.size());
    TEST_ASSERT_NULL(st.find("/t000005.jsonl"));
    TEST_ASSERT_NOT_NULL(st.find("/t000006.jsonl"));
}

// The open file is never removed, even when it alone keeps usage high: the loop must still end.
void test_usage_cap_stops_at_the_open_file() {
    RamStore st;
    st.total = 128 * 1024;
    st.put("/t000001.jsonl", std::string(400, 'a') + "\n");
    st.put("/t000002.jsonl", std::string(110 * 1024, 'b') + "\n");
    triplog::Log log(st);
    log.open(2);
    TEST_ASSERT_EQUAL_INT(1, log.enforceCap());
    TEST_ASSERT_NULL(st.find("/t000001.jsonl"));
    TEST_ASSERT_NOT_NULL(st.find("/t000002.jsonl"));
}

// A full partition: the failed flush makes room (oldest trips) and tries once more.
void test_failed_flush_makes_room_and_retries() {
    RamStore st;
    st.total = 128 * 1024;
    char nm[24];
    for (int i = 1; i <= 26; i++) {
        snprintf(nm, sizeof(nm), "/t%06d.jsonl", i);
        st.put(nm, std::string(99, 'a') + "\n");     // boot-time cap not run: 28 of 32 blocks
    }
    st.failAppendsWhileOver = 28 * 4096 - 1;
    triplog::Log log(st);
    log.open(27);
    log.queue("x\n");
    TEST_ASSERT_TRUE(log.flush());
    TEST_ASSERT_EQUAL_INT(0, log.queued());
    TEST_ASSERT_EQUAL_STRING("x\n", st.find("/t000027.jsonl")->c_str());
    TEST_ASSERT_NULL(st.find("/t000001.jsonl"));
}

// Nothing to drop: the failure stands and the rows stay queued.
void test_failed_flush_with_nothing_to_drop_keeps_the_queue() {
    RamStore st;
    triplog::Log log(st);
    log.open(1);
    log.queue("x\n");
    st.failAppends = 5;
    TEST_ASSERT_FALSE(log.flush());
    TEST_ASSERT_EQUAL_INT(1, log.queued());
}

// A file the power cut left empty still holds a directory entry: recover() removes it.
void test_recover_reaps_empty_files() {
    RamStore st;
    st.put("/t000001.jsonl", "");
    st.put("/t000002.jsonl", "{}\n");
    triplog::Log log(st);
    TEST_ASSERT_TRUE(log.recover());
    TEST_ASSERT_NULL(st.find("/t000001.jsonl"));
    TEST_ASSERT_NOT_NULL(st.find("/t000002.jsonl"));
    TEST_ASSERT_FALSE(log.recover());
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_flush_appends_queued_lines);
    RUN_TEST(test_flush_before_open_does_nothing);
    RUN_TEST(test_cut_row_is_dropped);
    RUN_TEST(test_cut_row_longer_than_a_chunk_and_no_newline_at_all);
    RUN_TEST(test_cap_drops_oldest_trip_first);
    RUN_TEST(test_cap_never_removes_the_open_file);
    RUN_TEST(test_flush_failure_keeps_queue);
    RUN_TEST(test_flush_stops_midway_and_keeps_the_rest);
    RUN_TEST(test_full_queue_drops_oldest_and_counts);
    RUN_TEST(test_oversize_line_is_dropped_and_counted);
    RUN_TEST(test_file_name_is_the_zero_padded_boot_sequence);
    RUN_TEST(test_names_sort_by_age_so_the_cap_drops_the_oldest);
    RUN_TEST(test_read_error_leaves_the_file_alone);
    RUN_TEST(test_failed_truncate_or_remove_is_not_a_change);
    RUN_TEST(test_many_files_are_all_recovered_and_capped);
    RUN_TEST(test_cap_sums_every_page_and_drops_oldest_first);
    RUN_TEST(test_cap_keeps_real_usage_under_70_percent);
    RUN_TEST(test_cap_keeps_at_most_20_files);
    RUN_TEST(test_usage_cap_stops_at_the_open_file);
    RUN_TEST(test_failed_flush_makes_room_and_retries);
    RUN_TEST(test_failed_flush_with_nothing_to_drop_keeps_the_queue);
    RUN_TEST(test_recover_reaps_empty_files);
    return UNITY_END();
}
