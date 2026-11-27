// The black deck's media:: interface (black/core/media.h) over the SD card, for the screen code
// that asks for its fonts by name: a file is read into PSRAM the first time and kept. The card must
// be mounted at that first ask (the boot asks for what the screens need while it is).
#include <cstring>
#include "sdcard.h"
#include "../../black/core/media.h"

namespace media {
namespace {
constexpr int MAX_KEPT = 8;
struct Kept {
    char name[24];
    sdcard::Blob blob;
};
Kept s_kept[MAX_KEPT];
int s_count = 0;
}  // namespace

void begin() {}

Blob get(const char* name) {
    for (int i = 0; i < s_count; ++i)
        if (strcmp(s_kept[i].name, name) == 0) return {s_kept[i].blob.data, s_kept[i].blob.size};
    if (s_count >= MAX_KEPT) return {nullptr, 0};
    const sdcard::Blob b = sdcard::load(name);
    if (!b.data) return {nullptr, 0};
    strlcpy(s_kept[s_count].name, name, sizeof s_kept[s_count].name);
    s_kept[s_count].blob = b;
    ++s_count;
    return {b.data, b.size};
}
}  // namespace media
