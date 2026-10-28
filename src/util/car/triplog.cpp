#include "triplog.h"
#include <cstring>

namespace triplog {

void Log::open(const char* tripId) {
    // "/t" + id + ".jsonl" + NUL must fit 24 bytes.
    constexpr size_t fixed = 2 + 6 + 1;
    size_t room = sizeof(name_) - fixed;
    size_t idLen = tripId ? strlen(tripId) : 0;
    if (idLen > room) idLen = room;
    name_[0] = '/';
    name_[1] = 't';
    if (idLen) memcpy(name_ + 2, tripId, idLen);
    memcpy(name_ + 2 + idLen, ".jsonl", 7);  // includes the NUL
    open_ = true;
}

void Log::queue(const char* line) {
    size_t n = line ? strlen(line) : 0;
    if (n >= LINE_BYTES) {
        dropped_++;
        return;
    }
    if (count_ == QUEUE_LINES) {
        head_ = (head_ + 1) % QUEUE_LINES;
        count_--;
        dropped_++;
    }
    int slot = (head_ + count_) % QUEUE_LINES;
    if (n) memcpy(lines_[slot], line, n);
    lines_[slot][n] = '\0';
    lens_[slot] = n;
    count_++;
}

bool Log::flush() {
    if (!open_) return false;
    while (count_ > 0) {
        if (!s_.append(name_, lines_[head_], lens_[head_])) return false;
        head_ = (head_ + 1) % QUEUE_LINES;
        count_--;
    }
    return true;
}

bool Log::recover() {
    char names[PAGE][24];
    bool changed = false;
    int skip = 0;
    for (;;) {
        int n = s_.list(names, PAGE, skip);
        if (n <= 0) break;
        for (int i = 0; i < n; i++) {
            bool removed = false;
            if (cutPartialRow(names[i], &removed)) changed = true;
            if (!removed) skip++;  // a removed file leaves the listing, so the next one slides into its place
        }
    }
    return changed;
}

bool Log::cutPartialRow(const char* name, bool* removed) {
    long sz = s_.size(name);
    if (sz <= 0) return false;
    char last = 0;
    if (s_.read(name, sz - 1, &last, 1) != 1 || last == '\n') return false;
    // Walk back in chunks to the last '\n'; keep everything up to and including it.
    long cut = 0;
    long end = sz;
    char buf[64];
    while (end > 0) {
        long start = end > (long)sizeof(buf) ? end - (long)sizeof(buf) : 0;
        size_t got = s_.read(name, start, buf, (size_t)(end - start));
        long found = -1;
        for (size_t k = got; k > 0; k--) {
            if (buf[k - 1] == '\n') {
                found = start + (long)k;
                break;
            }
        }
        if (found >= 0) {
            cut = found;
            break;
        }
        end = start;
    }
    if (cut == 0) *removed = s_.remove(name);
    else s_.truncate(name, cut);
    return true;
}

void Log::enforceCap() {
    char names[PAGE][24];
    long total = 0;
    for (int skip = 0;;) {
        int n = s_.list(names, PAGE, skip);
        if (n <= 0) break;
        for (int i = 0; i < n; i++) {
            long sz = s_.size(names[i]);
            if (sz > 0) total += sz;
        }
        skip += n;
    }
    // Oldest first; a file we keep (the open one, or one that won't go) stays in the listing, so skip it.
    int skip = 0;
    while (total > CAP_BYTES) {
        if (s_.list(names, 1, skip) < 1) break;
        if (open_ && strcmp(names[0], name_) == 0) {
            skip++;
            continue;
        }
        long sz = s_.size(names[0]);
        if (s_.remove(names[0])) {
            if (sz > 0) total -= sz;
        } else {
            skip++;
        }
    }
}

}  // namespace triplog
