#pragma once
#include <cstddef>
#include <cstdint>
// Trip files that survive a power cut. The car can lose power at any moment, so rows are queued in RAM,
// appended to a flash file every ~30 s, and at boot any half-written last row is cut away. Storage sits
// behind a small Store interface (LittleFS on the deck, a RAM store in the tests); no heap is used, so
// the queue is fixed-size. PURE, host-tested.
namespace triplog {
struct Store {
    virtual bool append(const char* name, const char* data, size_t n) = 0;
    virtual long size(const char* name) = 0;                    // -1 if missing
    virtual size_t read(const char* name, long at, char* out, size_t n) = 0;
    virtual bool truncate(const char* name, long len) = 0;
    virtual bool remove(const char* name) = 0;
    // The "/t<seq>.jsonl" files in ascending name order (LittleFS lists by name), which is oldest first
    // only because Log names them by a zero-padded boot sequence: up to max names after the first skip;
    // returns how many.
    virtual int list(char (*names)[24], int max, int skip) = 0;
    // The partition as the file system counts it (whole blocks, its own included); totalBytes() <= 0
    // means unknown, and then only the byte and file caps apply.
    virtual long usedBytes() = 0;
    virtual long totalBytes() = 0;
    virtual ~Store() = default;
};

constexpr long CAP_BYTES = 96 * 1024;
// LittleFS gives each file of a few rows whole 4 KB blocks and every power-on is a file, so on the
// deck's 128 KB partition the bytes in the files say little: short boots fill it long before
// CAP_BYTES. These two caps follow what the flash really holds.
constexpr int MAX_FILES = 20;
constexpr int USED_PCT = 70;
constexpr int QUEUE_LINES = 8;
constexpr size_t LINE_BYTES = 256;  // a line must be shorter than this (its NUL included)
constexpr int PAGE = 8;  // names fetched per list() call; any number of files is walked page by page

class Log {
public:
    explicit Log(Store& s) : s_(s) {}
    // Names the file "/t%06lu.jsonl" from the boot sequence, zero-padded so name order is age order
    // (the readable trip id stays in each row's "t" key). Any uint32_t fits the 24-byte name.
    void open(uint32_t seq);
    // Copies the line into RAM until flush(). A line of LINE_BYTES or more is dropped; when the queue is
    // full the oldest queued line is dropped. Both are counted in dropped().
    void queue(const char* line);
    // Appends the queued lines in order, one append each. Stops at the first failure and keeps that line
    // and the rest queued; a failure that enforceCap() can make room for (it removed a file) is tried
    // once more. True when the queue is empty afterwards; false (and nothing done) before open().
    bool flush();
    // At boot: in every file whose last byte is not '\n', cut back to just after the last '\n' (a file
    // left empty is removed, and so is one found empty). A file that can't be read in full is left
    // alone. True if any file changed (a truncate or remove the store reported as done).
    bool recover();
    // Removes the oldest files while the files hold more than CAP_BYTES, there are more than MAX_FILES
    // of them, or the store is more than USED_PCT % full; never the open file. Returns how many went.
    int enforceCap();
    int queued() const { return count_; }
    int dropped() const { return dropped_; }
    int capRemoved() const { return capRemoved_; }  // files enforceCap() removed since boot
private:
    bool cutPartialRow(const char* name, bool* removed);
    bool flushQueue();
    Store& s_;
    char name_[24] = {0};
    bool open_ = false;
    char lines_[QUEUE_LINES][LINE_BYTES];
    size_t lens_[QUEUE_LINES] = {0};
    int head_ = 0;   // oldest queued line
    int count_ = 0;
    int dropped_ = 0;
    int capRemoved_ = 0;
};
}
