#pragma once
#include <cstddef>
#include <cstdint>
// The Pokédeck's media: the files tools/sd_media.py puts in the card's /cardeck folder, the same
// ones the black deck packs into its media partition. A file is read whole into PSRAM when asked
// for and given back when done (4 MB of flash can't hold them). Nothing is written to the card,
// and it is never formatted. One task uses it at a time.
namespace sdcard {
struct Blob {
    uint8_t* data;   // PSRAM; nullptr when the file isn't there or didn't read whole
    size_t size;
};
bool begin();                  // mounts the card; logs "[sd] ..."
void end();                    // unmounts it: the mount holds internal heap Spotify will want
bool has(const char* name);    // /cardeck/<name> is there and not empty
Blob load(const char* name);   // {nullptr, 0} (logged) when it can't be read
void release(Blob& b);         // frees it; safe on an empty one
}
