#pragma once
#include <cstddef>
#include "../util/car/triplog.h"
// The trip log's storage on the OBD deck: LittleFS on min_spiffs.csv's data partition ("spiffs",
// ~190 KB; triplog keeps its files under CAP_BYTES = 96 KB). Only the loop task touches it.
namespace carfs {
// Mounts LittleFS, formatting a partition that has never held one. False: no trip log this boot.
bool begin();
size_t usedBytes();
size_t totalBytes();

class Store : public triplog::Store {
public:
    // A short write is cut back to the size before it, so a retry never leaves half a row mid-file.
    bool append(const char* name, const char* data, size_t n) override;
    long size(const char* name) override;
    size_t read(const char* name, long at, char* out, size_t n) override;
    bool truncate(const char* name, long len) override;
    bool remove(const char* name) override;
    // The trip files ("/t<seq>.jsonl", nothing else) by sequence number, which is name order for
    // Log's zero-padded names: oldest first, whatever order the directory walk gives.
    int list(char (*names)[24], int max, int skip) override;
};
}
