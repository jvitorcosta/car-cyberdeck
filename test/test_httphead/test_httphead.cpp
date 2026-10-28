#include <unity.h>
#include <cstring>
#include "../../src/util/yellow/httphead.h"

void setUp() {}
void tearDown() {}

// Feeds s byte by byte; returns how many bytes it took to finish the head (-1: never finished).
static int feedAll(httphead::Head& h, const char* s) {
    for (int i = 0; s[i]; i++) {
        if (h.feed(s[i])) return i + 1;
    }
    return -1;
}

void test_split_the_black_decks_state_url() {
    httphead::Url u;
    TEST_ASSERT_TRUE(httphead::split("http://172.20.10.10/state?k=abc", &u));
    TEST_ASSERT_EQUAL_STRING("172.20.10.10", u.host);
    TEST_ASSERT_EQUAL_UINT16(80, u.port);
    TEST_ASSERT_EQUAL_STRING("/state?k=abc", u.path);
}

void test_split_a_named_host_with_a_query() {
    httphead::Url u;
    TEST_ASSERT_TRUE(httphead::split("http://ip-api.com/json/?fields=status,lat,lon,city", &u));
    TEST_ASSERT_EQUAL_STRING("ip-api.com", u.host);
    TEST_ASSERT_EQUAL_UINT16(80, u.port);
    TEST_ASSERT_EQUAL_STRING("/json/?fields=status,lat,lon,city", u.path);
}

void test_split_a_port_and_no_path() {
    httphead::Url u;
    TEST_ASSERT_TRUE(httphead::split("http://deck.local:8080/music?on=1", &u));
    TEST_ASSERT_EQUAL_STRING("deck.local", u.host);
    TEST_ASSERT_EQUAL_UINT16(8080, u.port);
    TEST_ASSERT_EQUAL_STRING("/music?on=1", u.path);
    TEST_ASSERT_TRUE(httphead::split("http://example.com", &u));
    TEST_ASSERT_EQUAL_STRING("example.com", u.host);
    TEST_ASSERT_EQUAL_STRING("/", u.path);
    TEST_ASSERT_TRUE(httphead::split("http://example.com:81", &u));
    TEST_ASSERT_EQUAL_UINT16(81, u.port);
    TEST_ASSERT_EQUAL_STRING("/", u.path);
}

void test_split_refuses_what_it_cant_ask() {
    httphead::Url u;
    TEST_ASSERT_FALSE(httphead::split("https://api.open-meteo.com/v1/forecast", &u));
    TEST_ASSERT_FALSE(httphead::split("ftp://x/", &u));
    TEST_ASSERT_FALSE(httphead::split("http:///state", &u));
    TEST_ASSERT_FALSE(httphead::split("http://", &u));
    TEST_ASSERT_FALSE(httphead::split("http://h:/x", &u));
    TEST_ASSERT_FALSE(httphead::split("http://h:0/x", &u));
    TEST_ASSERT_FALSE(httphead::split("http://h:65536/x", &u));
    TEST_ASSERT_FALSE(httphead::split("http://h:80x/x", &u));
    TEST_ASSERT_FALSE(httphead::split("http://h?x=1", &u));
    TEST_ASSERT_FALSE(httphead::split(nullptr, &u));
    char longHost[100] = "http://";
    memset(longHost + 7, 'a', 64);  // one byte more than the field holds with its terminator
    strcat(longHost, "/x");
    TEST_ASSERT_FALSE(httphead::split(longHost, &u));
    longHost[7 + 63] = 0;  // 63 fit
    TEST_ASSERT_TRUE(httphead::split(longHost, &u));
    TEST_ASSERT_EQUAL_size_t(63, strlen(u.host));
}

void test_head_of_a_webserver_answer_ends_at_the_blank_line() {
    const char* r = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: 2\r\n"
                    "Connection: close\r\n\r\n{}";
    httphead::Head h;
    const int n = feedAll(h, r);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(strlen(r)) - 2, n);  // the body is left
    TEST_ASSERT_TRUE(h.done());
    TEST_ASSERT_EQUAL_INT(200, h.status());
}

void test_head_not_done_until_the_blank_line() {
    httphead::Head h;
    TEST_ASSERT_EQUAL_INT(-1, feedAll(h, "HTTP/1.0 403 Forbidden\r\nContent-Length: 0\r\n"));
    TEST_ASSERT_FALSE(h.done());
    TEST_ASSERT_EQUAL_INT(403, h.status());  // known from the status line on
    TEST_ASSERT_EQUAL_INT(-1, feedAll(h, "\r"));
    TEST_ASSERT_FALSE(h.done());
    TEST_ASSERT_EQUAL_INT(1, feedAll(h, "\n"));
    TEST_ASSERT_TRUE(h.done());
}

void test_head_with_bare_newlines_and_no_reason() {
    httphead::Head h;
    TEST_ASSERT_EQUAL_INT(26, feedAll(h, "HTTP/1.0 503\nX-Why: heap\n\nbody"));
    TEST_ASSERT_EQUAL_INT(503, h.status());
    httphead::Head g;
    TEST_ASSERT_TRUE(feedAll(g, "HTTP/1.1 409 Conflict\r\n\r\n") > 0);
    TEST_ASSERT_EQUAL_INT(409, g.status());
}

void test_long_header_lines_are_skipped() {
    char r[600] = "HTTP/1.1 200 OK\r\nX-Long: ";
    memset(r + strlen(r), 'x', 400);
    strcat(r, "\r\nServer: y\r\n\r\n");
    httphead::Head h;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(strlen(r)), feedAll(h, r));
    TEST_ASSERT_EQUAL_INT(200, h.status());
}

void test_not_an_http_server_has_no_status() {
    httphead::Head h;
    TEST_ASSERT_TRUE(feedAll(h, "hello there\r\n\r\n") > 0);
    TEST_ASSERT_EQUAL_INT(0, h.status());
    httphead::Head e;  // an empty first line ends the head at once, as HTTPClient took it
    TEST_ASSERT_EQUAL_INT(2, feedAll(e, "\r\nHTTP/1.1 200 OK\r\n\r\n"));
    TEST_ASSERT_EQUAL_INT(0, e.status());
    httphead::Head s;  // a status line longer than the part kept still reads its code
    TEST_ASSERT_TRUE(feedAll(s, "HTTP/1.1 200 A very long reason phrase indeed, longer than kept\r\n\r\n") > 0);
    TEST_ASSERT_EQUAL_INT(200, s.status());
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_split_the_black_decks_state_url);
    RUN_TEST(test_split_a_named_host_with_a_query);
    RUN_TEST(test_split_a_port_and_no_path);
    RUN_TEST(test_split_refuses_what_it_cant_ask);
    RUN_TEST(test_head_of_a_webserver_answer_ends_at_the_blank_line);
    RUN_TEST(test_head_not_done_until_the_blank_line);
    RUN_TEST(test_head_with_bare_newlines_and_no_reason);
    RUN_TEST(test_long_header_lines_are_skipped);
    RUN_TEST(test_not_an_http_server_has_no_status);
    return UNITY_END();
}
