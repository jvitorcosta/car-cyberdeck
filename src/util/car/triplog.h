#pragma once
#include <cstddef>
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
    virtual int list(char (*names)[24], int max, int skip) = 0; // "/t<id>.jsonl" files, oldest first: up to max names after the first skip; returns how many
    virtual ~Store() = default;
};

constexpr long CAP_BYTES = 96 * 1024;
constexpr int QUEUE_LINES = 8;
constexpr size_t LINE_BYTES = 256;  // a line must be shorter than this (its NUL included)
constexpr int PAGE = 8;  // names fetched per list() call; any number of files is walked page by page

class Log {
public:
    explicit Log(Store& s) : s_(s) {}
    // Names the file "/t<id>.jsonl"; the id is cut so the whole name fits 24 bytes with its NUL.
    void open(const char* tripId);
    // Copies the line into RAM until flush(). A line of LINE_BYTES or more is dropped; when the queue is
    // full the oldest queued line is dropped. Both are counted in dropped().
    void queue(const char* line);
    // Appends the queued lines in order, one append each. Stops at the first failure and keeps that line
    // and the rest queued. True when the queue is empty afterwards; false (and nothing done) before open().
    bool flush();
    // At boot: in every file whose last byte is not '\n', cut back to just after the last '\n' (a file
    // left empty is removed). True if any file changed.
    bool recover();
    // Removes the oldest files while the total is above CAP_BYTES; never the open file.
    void enforceCap();
    int queued() const { return count_; }
    int dropped() const { return dropped_; }
private:
    bool cutPartialRow(const char* name, bool* removed);
    Store& s_;
    char name_[24] = {0};
    bool open_ = false;
    char lines_[QUEUE_LINES][LINE_BYTES];
    size_t lens_[QUEUE_LINES] = {0};
    int head_ = 0;   // oldest queued line
    int count_ = 0;
    int dropped_ = 0;
};
}
