#include "tunes.h"

namespace tunes {

static bool space(char c) { return c == ' ' || c == '\t' || c == '\r'; }

// [p, end): the line from p (without its '\n'); returns where the next line starts.
static const char* lineEnd(const char* p, const char* end) {
    while (p < end && *p != '\n') ++p;
    return p;
}

// A "tune <name>" line: true, and the name's bounds (trimmed, comment dropped).
static bool header(const char* p, const char* e, const char** n0, const char** n1) {
    while (p < e && space(*p)) ++p;
    if (e - p < 4 || p[0] != 't' || p[1] != 'u' || p[2] != 'n' || p[3] != 'e') return false;
    p += 4;
    if (p < e && !space(*p) && *p != '#') return false;   // "tunes ...": not a header
    while (p < e && space(*p)) ++p;
    const char* q = p;
    while (q < e && *q != '#') ++q;
    while (q > p && space(q[-1])) --q;
    *n0 = p;
    *n1 = q;
    return true;
}

// The next whitespace-separated token in [*p, end), skipping comments; false at the end.
static bool token(const char** p, const char* end, const char** t0, const char** t1) {
    const char* s = *p;
    for (;;) {
        while (s < end && (space(*s) || *s == '\n')) ++s;
        if (s < end && *s == '#') { s = lineEnd(s, end); continue; }
        break;
    }
    if (s >= end) { *p = s; return false; }
    const char* e = s;
    while (e < end && !space(*e) && *e != '\n') ++e;   // '#' mid-token is a sharp
    *t0 = s;
    *t1 = e;
    *p = e;
    return true;
}

// "C4", "F#5", "Bb3" -> MIDI number; "R" -> REST; else false.
static bool pitch(const char* s, const char* e, int* midi) {
    if (e - s == 1 && *s == 'R') { *midi = REST; return true; }
    static const int SEMI[] = {9, 11, 0, 2, 4, 5, 7};   // A B C D E F G
    if (s >= e || *s < 'A' || *s > 'G') return false;
    int semi = SEMI[*s - 'A'];
    ++s;
    if (s < e && (*s == '#' || *s == 'b')) semi += *s++ == '#' ? 1 : -1;
    if (e - s != 1 || *s < '0' || *s > '8') return false;
    *midi = 12 * (*s - '0' + 1) + semi;
    return true;
}

static bool number(const char* s, const char* e, uint32_t* v) {
    if (s >= e) return false;
    uint32_t n = 0;
    for (; s < e; ++s) {
        if (*s < '0' || *s > '9') return false;
        if (n < 1000000) n = n * 10 + (uint32_t)(*s - '0');
    }
    *v = n;
    return true;
}

// The next valid pair in [*p, end); false at the end.
static bool pair(const char** p, const char* end, Note* n) {
    const char *a0, *a1, *b0, *b1;
    while (token(p, end, &a0, &a1)) {
        if (!token(p, end, &b0, &b1)) return false;
        int midi;
        uint32_t ms;
        if (!pitch(a0, a1, &midi) || !number(b0, b1, &ms)) continue;   // a bad pair: skipped
        n->midi = midi;
        n->ms = (uint16_t)(ms < 1 ? 1 : ms > MAX_MS ? MAX_MS : ms);
        return true;
    }
    return false;
}

// Tune i (counting only tunes with a note): its body and name. False when there's none.
static bool find(const char* text, size_t len, int i, const char** b0, const char** b1,
                 const char** n0, const char** n1) {
    const char* end = text + len;
    const char* p = text;
    int k = 0;
    while (p < end) {
        const char* e = lineEnd(p, end);
        const char *a, *z;
        if (header(p, e, &a, &z)) {
            const char* body = e < end ? e + 1 : e;
            const char* q = body;   // the body runs to the next header
            while (q < end) {
                const char* qe = lineEnd(q, end);
                const char *x, *y;
                if (header(q, qe, &x, &y)) break;
                q = qe < end ? qe + 1 : qe;
            }
            const char* scan = body;
            Note n;
            if (pair(&scan, q, &n)) {
                if (k == i) { *b0 = body; *b1 = q; *n0 = a; *n1 = z; return true; }
                ++k;
            }
            p = q;
            continue;
        }
        p = e < end ? e + 1 : e;
    }
    return false;
}

int count(const char* text, size_t len) {
    int n = 0;
    const char *a, *b, *c, *d;
    while (find(text, len, n, &a, &b, &c, &d)) ++n;
    return n;
}

bool name(const char* text, size_t len, int i, char* out, size_t n) {
    const char *a, *b, *n0, *n1;
    if (n == 0 || !find(text, len, i, &a, &b, &n0, &n1)) return false;
    size_t k = 0;
    for (; n0 < n1 && k + 1 < n; ++n0) out[k++] = *n0;
    out[k] = '\0';
    return true;
}

Reader::Reader(const char* text, size_t len, int tune) : p_(text), end_(text) {
    const char *n0, *n1;
    if (tune < 0 || !find(text, len, tune, &p_, &end_, &n0, &n1)) p_ = end_ = text;
}

bool Reader::next(Note* n) { return pair(&p_, end_, n); }

}
