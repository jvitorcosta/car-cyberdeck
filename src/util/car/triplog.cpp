#include "triplog.h"
#include <cstdio>
#include <cstring>

namespace triplog {

void Log::open(uint32_t seq) {
    // Padded to 6 digits: a deck boots far fewer than a million times, so names sort as numbers do.
    // At most "/t" + 10 digits + ".jsonl" = 18 chars.
    snprintf(name_, sizeof name_, "/t%06lu.jsonl", static_cast<unsigned long>(seq));
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
    if (flushQueue()) return true;
    // Most likely a full partition: drop the oldest trips and try once more. Nothing dropped, nothing
    // gained: the rows stay queued for the next checkpoint.
    return enforceCap() > 0 && flushQueue();
}

bool Log::flushQueue() {
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
    if (sz < 0) return false;
    if (sz == 0) {  // a power cut before the first row: no data, but a directory entry all the same
        if (open_ && strcmp(name, name_) == 0) return false;
        *removed = s_.remove(name);
        return *removed;
    }
    char last = 0;
    if (s_.read(name, sz - 1, &last, 1) != 1 || last == '\n') return false;
    // Walk back in chunks to the last '\n'; keep everything up to and including it.
    long cut = 0;
    long end = sz;
    char buf[64];
    while (end > 0) {
        long start = end > (long)sizeof(buf) ? end - (long)sizeof(buf) : 0;
        const size_t want = (size_t)(end - start);
        size_t got = s_.read(name, start, buf, want);
        // A short read is a flash error, not the start of the file: guessing would cut good rows or
        // remove the whole trip. Leave it for the next boot.
        if (got != want) return false;
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
    // "Changed" only when the store says it happened.
    if (cut == 0) {
        *removed = s_.remove(name);
        return *removed;
    }
    return s_.truncate(name, cut);
}

int Log::enforceCap() {
    char names[PAGE][24];
    long bytes = 0;
    int files = 0;
    for (int skip = 0;;) {
        int n = s_.list(names, PAGE, skip);
        if (n <= 0) break;
        for (int i = 0; i < n; i++) {
            long sz = s_.size(names[i]);
            if (sz > 0) bytes += sz;
        }
        files += n;
        skip += n;
    }
    const long total = s_.totalBytes();
    auto over = [&]() {
        if (bytes > CAP_BYTES || files > MAX_FILES) return true;
        return total > 0 && s_.usedBytes() * 100 > total * USED_PCT;
    };
    // Oldest first; a file we keep (the open one, or one that won't go) stays in the listing, so skip it.
    int skip = 0, removed = 0;
    while (over()) {
        if (s_.list(names, 1, skip) < 1) break;
        if (open_ && strcmp(names[0], name_) == 0) {
            skip++;
            continue;
        }
        long sz = s_.size(names[0]);
        if (s_.remove(names[0])) {
            if (sz > 0) bytes -= sz;
            files--;
            removed++;
        } else {
            skip++;
        }
    }
    capRemoved_ += removed;
    return removed;
}

}  // namespace triplog
