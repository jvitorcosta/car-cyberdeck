#include "carfs.h"
#include <Arduino.h>
#include <LittleFS.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <algorithm>
#include <cstdio>
#include <cstring>

// POSIX calls on LittleFS's mount point rather than fs::File: they don't log a missing file at error
// level (the core's open() does), and esp_littlefs implements truncate().
namespace carfs {
namespace {
constexpr const char* MOUNT = "/littlefs";
// More trip files than this can't fit under triplog::CAP_BYTES (a file holds at least one ~200-byte
// row); past it list() keeps the oldest, which enforceCap() removes first anyway.
constexpr int MAX_FILES = 512;
uint32_t s_seq[MAX_FILES];   // list()'s scratch: the loop task's only

// "/t000042.jsonl" -> "/littlefs/t000042.jsonl"
bool full(const char* name, char* out, size_t n) {
    const int k = snprintf(out, n, "%s%s", MOUNT, name);
    return k > 0 && static_cast<size_t>(k) < n;
}

// A trip file's sequence number: exactly the name triplog::Log::open() gives it.
bool tripSeq(const char* base, uint32_t* seq) {
    unsigned long v = 0;
    if (sscanf(base, "t%lu.jsonl", &v) != 1) return false;
    char again[24];
    snprintf(again, sizeof again, "t%06lu.jsonl", v);
    if (strcmp(again, base) != 0) return false;
    *seq = static_cast<uint32_t>(v);
    return true;
}
}  // namespace

bool begin() {
    if (!LittleFS.begin(true, MOUNT, 4, "spiffs")) {
        Serial.println("[car] LittleFS mount failed: no trip log this boot");
        return false;
    }
    Serial.printf("[car] LittleFS %u of %u bytes used\n", static_cast<unsigned>(usedBytes()),
                  static_cast<unsigned>(totalBytes()));
    return true;
}

size_t usedBytes() { return LittleFS.usedBytes(); }
size_t totalBytes() { return LittleFS.totalBytes(); }

bool Store::append(const char* name, const char* data, size_t n) {
    char path[48];
    if (!full(name, path, sizeof path)) return false;
    const long before = size(name);
    FILE* f = fopen(path, "a");
    if (!f) return false;
    const size_t wrote = fwrite(data, 1, n, f);
    const bool closed = fclose(f) == 0;            // the flush: a full partition shows here too
    if (wrote == n && closed) return true;
    if (before > 0) ::truncate(path, before);      // back to the last whole row
    else unlink(path);                             // a new file with half a row: gone
    return false;
}

long Store::size(const char* name) {
    char path[48];
    struct stat st;
    if (!full(name, path, sizeof path) || stat(path, &st) != 0) return -1;
    return static_cast<long>(st.st_size);
}

size_t Store::read(const char* name, long at, char* out, size_t n) {
    char path[48];
    if (!full(name, path, sizeof path)) return 0;
    FILE* f = fopen(path, "r");
    if (!f) return 0;
    size_t got = 0;
    if (fseek(f, at, SEEK_SET) == 0) got = fread(out, 1, n, f);
    fclose(f);
    return got;
}

bool Store::truncate(const char* name, long len) {
    char path[48];
    return full(name, path, sizeof path) && ::truncate(path, len) == 0;
}

bool Store::remove(const char* name) {
    char path[48];
    return full(name, path, sizeof path) && unlink(path) == 0;
}

int Store::list(char (*names)[24], int max, int skip) {
    DIR* d = opendir(MOUNT);
    if (!d) return 0;
    int count = 0;
    bool over = false;
    for (struct dirent* e; (e = readdir(d)) != nullptr;) {
        uint32_t seq;
        if (!tripSeq(e->d_name, &seq)) continue;
        if (count < MAX_FILES) {
            s_seq[count++] = seq;
            continue;
        }
        over = true;                               // keep the oldest MAX_FILES
        uint32_t* newest = std::max_element(s_seq, s_seq + count);
        if (seq < *newest) *newest = seq;
    }
    closedir(d);
    if (over) Serial.printf("[car] over %d trip files: listing the oldest\n", MAX_FILES);
    std::sort(s_seq, s_seq + count);
    int n = 0;
    for (int i = skip; i < count && n < max; ++i, ++n)
        snprintf(names[n], sizeof names[n], "/t%06lu.jsonl", static_cast<unsigned long>(s_seq[i]));
    return n;
}
}  // namespace carfs
