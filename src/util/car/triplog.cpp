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
    char names[MAX_FILES][24];
    int n = s_.list(names, MAX_FILES);
    bool changed = false;
    for (int i = 0; i < n; i++) {
        long sz = s_.size(names[i]);
        if (sz <= 0) continue;
        char last = 0;
        if (s_.read(names[i], sz - 1, &last, 1) != 1) continue;
        if (last == '\n') continue;
        // Walk back in chunks to the last '\n'; keep everything up to and including it.
        long cut = 0;
        long end = sz;
        char buf[64];
        while (end > 0) {
            long start = end > (long)sizeof(buf) ? end - (long)sizeof(buf) : 0;
            size_t got = s_.read(names[i], start, buf, (size_t)(end - start));
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
        if (cut == 0) s_.remove(names[i]);
        else s_.truncate(names[i], cut);
        changed = true;
    }
    return changed;
}

void Log::enforceCap() {
    char names[MAX_FILES][24];
    int n = s_.list(names, MAX_FILES);
    long sizes[MAX_FILES];
    long total = 0;
    for (int i = 0; i < n; i++) {
        long sz = s_.size(names[i]);
        sizes[i] = sz > 0 ? sz : 0;
        total += sizes[i];
    }
    for (int i = 0; i < n && total > CAP_BYTES; i++) {
        if (open_ && strcmp(names[i], name_) == 0) continue;
        if (s_.remove(names[i])) total -= sizes[i];
    }
}

}  // namespace triplog
