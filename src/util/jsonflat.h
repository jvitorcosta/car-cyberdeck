#pragma once
#include <cstddef>
// Reads values by key from a small JSON object whose values are strings, numbers, booleans or
// arrays of strings (the deck's GET /state). Keys are found by scanning tokens, so a key's
// name appearing inside a string value never matches. No allocation. PURE, host-tested.
namespace jsonflat {
// Decodes \" \\ \/ \b \f \n \r \t and \uXXXX (BMP, to UTF-8). Cut to n-1 bytes (whole UTF-8
// characters), always terminated. False when the key is missing or not a string.
bool str(const char* body, const char* key, char* out, size_t n);
bool num(const char* body, const char* key, long* out);      // integer; false if not a number
bool real(const char* body, const char* key, double* out);
bool boolean(const char* body, const char* key, bool* out);
bool arrayHas(const char* body, const char* key, const char* item);   // array of strings
// The same reads on a value already located (util/jsonpath::find): `value` points at its first
// character. False for nullptr or a value of another type.
bool strAt(const char* value, char* out, size_t n);
bool numAt(const char* value, long* out);
bool boolAt(const char* value, bool* out);
}
