#pragma once
#include <cstddef>
// Finds a value in nested JSON by a dotted path: "item.album.images.1.url" walks object keys and
// array indexes, so the many "name" keys of a Spotify answer (song, album, artist, device) are
// told apart. Read the value with jsonflat::strAt / numAt / boolAt. No allocation. PURE,
// host-tested.
namespace jsonpath {
// Points at the value's first character, or nullptr when a step is missing, of the wrong kind,
// or the body is cut off before it. Path steps are keys or, inside an array, a decimal index.
const char* find(const char* body, const char* path);
bool str(const char* body, const char* path, char* out, size_t n);   // "" when missing
bool num(const char* body, const char* path, long* out);
bool boolean(const char* body, const char* path, bool* out);
}
