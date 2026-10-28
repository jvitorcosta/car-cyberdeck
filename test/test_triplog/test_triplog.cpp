#include <unity.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include "../../src/util/car/triplog.h"

void setUp() {}
void tearDown() {}

// Files in insertion order, like LittleFS listing oldest first.
struct RamStore : triplog::Store {
    std::vector<std::pair<std::string, std::string>> files;
    int failAppends = 0;  // the next N appends fail
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
        memcpy(out, s->data() + at, got);
        return got;
    }
    bool truncate(const char* name, long len) override {
        std::string* s = find(name);
        if (!s) return false;
        s->resize((size_t)len);
        return true;
    }
    bool remove(const char* name) override {
        for (size_t i = 0; i < files.size(); i++)
            if (files[i].first == name) { files.erase(files.begin() + (long)i); return true; }
        return false;
    }
    int list(char (*names)[24], int max) override {
        int n = 0;
        for (auto& f : files) {
            if (n >= max) break;
            strncpy(names[n], f.first.c_str(), 23);
            names[n][23] = '\0';
            n++;
        }
        return n;
    }
};

void test_flush_appends_queued_lines() {
    RamStore st;
    triplog::Log log(st);
    log.open("0042");
    log.queue("{\"t\":1}\n");
    log.queue("{\"t\":2}\n");
    TEST_ASSERT_TRUE(log.flush());
    TEST_ASSERT_EQUAL_STRING("{\"t\":1}\n{\"t\":2}\n", st.find("/t0042.jsonl")->c_str());
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
    st.put("/t1.jsonl", "{\"t\":\"a\"}\n{\"t\":\"x\",\"m");
    st.put("/t2.jsonl", "{\"t\":\"b\"}\n");
    triplog::Log log(st);
    TEST_ASSERT_TRUE(log.recover());
    TEST_ASSERT_EQUAL_STRING("{\"t\":\"a\"}\n", st.find("/t1.jsonl")->c_str());
    TEST_ASSERT_EQUAL_STRING("{\"t\":\"b\"}\n", st.find("/t2.jsonl")->c_str());
    TEST_ASSERT_FALSE(log.recover());
}

void test_cut_row_longer_than_a_chunk_and_no_newline_at_all() {
    RamStore st;
    st.put("/t1.jsonl", "{\"t\":1}\n" + std::string(200, 'x'));
    st.put("/t2.jsonl", std::string(100, 'y'));
    triplog::Log log(st);
    TEST_ASSERT_TRUE(log.recover());
    TEST_ASSERT_EQUAL_STRING("{\"t\":1}\n", st.find("/t1.jsonl")->c_str());
    TEST_ASSERT_NULL(st.find("/t2.jsonl"));
}

void test_cap_drops_oldest_trip_first() {
    RamStore st;
    st.put("/t1.jsonl", std::string(40 * 1024, 'a'));
    st.put("/t2.jsonl", std::string(40 * 1024, 'b'));
    st.put("/t3.jsonl", std::string(40 * 1024, 'c'));
    triplog::Log log(st);
    log.open("3");
    log.enforceCap();
    TEST_ASSERT_NULL(st.find("/t1.jsonl"));
    TEST_ASSERT_NOT_NULL(st.find("/t2.jsonl"));
    TEST_ASSERT_NOT_NULL(st.find("/t3.jsonl"));
}

void test_cap_never_removes_the_open_file() {
    RamStore st;
    st.put("/t1.jsonl", std::string(100 * 1024, 'a'));
    st.put("/t2.jsonl", std::string(10 * 1024, 'b'));
    triplog::Log log(st);
    log.open("1");
    log.enforceCap();
    TEST_ASSERT_NOT_NULL(st.find("/t1.jsonl"));
    TEST_ASSERT_NULL(st.find("/t2.jsonl"));
}

void test_flush_failure_keeps_queue() {
    RamStore st;
    triplog::Log log(st);
    log.open("7");
    log.queue("a\n");
    log.queue("b\n");
    log.queue("c\n");
    st.failAppends = 1;
    TEST_ASSERT_FALSE(log.flush());
    TEST_ASSERT_EQUAL_INT(3, log.queued());
    TEST_ASSERT_TRUE(log.flush());
    TEST_ASSERT_EQUAL_STRING("a\nb\nc\n", st.find("/t7.jsonl")->c_str());
}

void test_flush_stops_midway_and_keeps_the_rest() {
    RamStore st;
    triplog::Log log(st);
    log.open("7");
    log.queue("a\n");
    log.queue("b\n");
    TEST_ASSERT_TRUE(log.flush());
    log.queue("c\n");
    log.queue("d\n");
    st.failAppends = 1;
    TEST_ASSERT_FALSE(log.flush());
    TEST_ASSERT_EQUAL_INT(2, log.queued());
    TEST_ASSERT_TRUE(log.flush());
    TEST_ASSERT_EQUAL_STRING("a\nb\nc\nd\n", st.find("/t7.jsonl")->c_str());
}

void test_full_queue_drops_oldest_and_counts() {
    RamStore st;
    triplog::Log log(st);
    log.open("1");
    for (int i = 0; i < 10; i++) {
        char l[8];
        snprintf(l, sizeof(l), "%d\n", i);
        log.queue(l);
    }
    TEST_ASSERT_EQUAL_INT(8, log.queued());
    TEST_ASSERT_EQUAL_INT(2, log.dropped());
    TEST_ASSERT_TRUE(log.flush());
    TEST_ASSERT_EQUAL_STRING("2\n3\n4\n5\n6\n7\n8\n9\n", st.find("/t1.jsonl")->c_str());
}

void test_oversize_line_is_dropped_and_counted() {
    RamStore st;
    triplog::Log log(st);
    log.open("1");
    std::string big(256, 'x');
    log.queue(big.c_str());
    TEST_ASSERT_EQUAL_INT(0, log.queued());
    TEST_ASSERT_EQUAL_INT(1, log.dropped());
    std::string ok(255, 'y');
    log.queue(ok.c_str());
    TEST_ASSERT_EQUAL_INT(1, log.queued());
}

void test_long_trip_id_is_cut_to_fit_the_name() {
    RamStore st;
    triplog::Log log(st);
    log.open("0123456789012345678901234567890123456789");
    log.queue("a\n");
    TEST_ASSERT_TRUE(log.flush());
    TEST_ASSERT_EQUAL_INT(1, (int)st.files.size());
    TEST_ASSERT_EQUAL_INT(23, (int)st.files[0].first.size());
    TEST_ASSERT_EQUAL_STRING(".jsonl", st.files[0].first.c_str() + 17);
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
    RUN_TEST(test_long_trip_id_is_cut_to_fit_the_name);
    return UNITY_END();
}
