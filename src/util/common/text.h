#pragma once
#include <cstddef>
#include <cstdint>
namespace txt {
// Copy src -> dst (at most n bytes incl. NUL), converting UTF-8 Latin-1 accented
// letters to their ASCII base (á->a, ç->c, ã->a, É->E, ...). Other non-ASCII
// multibyte sequences are dropped. The display now draws through foldMarks + glyph runs
// (accent marks, CJK); this plain fold remains for tests and simple ASCII-only uses. Pure.
void asciiFold(const char* src, char* dst, size_t n);
// Accent carried by a folded character, so the UI can draw it over the ASCII glyph.
enum class Mark : uint8_t { None, Acute, Grave, Circumflex, Tilde, Diaeresis, Cedilla, Ring };
// asciiFold that also records each output character's accent. `marks` (room for n entries)
// may be nullptr. Letters that fold to two characters (Æ, ß) carry no mark. Returns the
// folded length.
size_t foldMarks(const char* src, char* dst, Mark* marks, size_t n);
// UTF-8 -> Latin-1 bytes, the yellow deck's lettering (its fonts cover 0x20-0xFF): ASCII and
// U+00A1..00FF (accented letters, the degree sign) become one byte each, the no-break space and the
// other Unicode spaces a plain ' ' (lines wrap at it); above that, curly quotes and ʼ fold to ' and ",
// ‹ › to < >, • to *, dashes to -, the ellipsis to ..., Œ œ to OE oe, Ÿ to Y; anything else (CJK, emoji),
// the C1 controls and the soft hyphen are dropped. A character goes in whole or not at all, and text
// cut mid-character stops at its NUL. Writes at most n bytes incl. NUL; returns the length. Pure.
size_t latin1Fold(const char* utf8, char* dst, size_t n);
// Upper-cases Latin-1 text in place: a-z, à..þ -> À..Þ (not ÷), ÿ -> Y (Latin-1 has no capital
// Ÿ); ß and the rest stay as they are.
void latin1Upper(char* s);
// Copy an identifier (e.g. a Spotify URI) into n bytes. If it does not fit, keep the start
// and end with "#" + 8 hex digits of a hash of the whole id, so two long ids sharing a
// prefix (local files from one album) stay distinct. nullptr -> "".
void copyId(const char* src, char* dst, size_t n);
// Percent-encodes s for a URL query value: RFC 3986 unreserved characters (A-Z a-z 0-9 - _ . ~)
// are kept, every other byte (UTF-8 included) becomes %XX. Writes at most n bytes incl. NUL,
// never a partial %XX; returns the encoded length. nullptr -> "".
size_t urlEncode(const char* s, char* out, size_t n);
// Copies src into dst (n bytes incl. NUL), truncating; always NUL-terminated. nullptr -> "".
void copy(char* dst, const char* src, size_t n);
// dst = a + b, all or nothing: if it does not fit in n bytes (or either is nullptr) dst becomes
// "" and the result is false. For values that must never be truncated (an auth header).
bool concat(char* dst, size_t n, const char* a, const char* b);
// Strips HTML from text in place (a Spotify playlist's description): tags go (an unclosed '<', text
// cut inside a tag, drops the rest), &amp; &lt; &gt; &quot; &apos; and numeric entities (&#39;
// &#x27;) become their character (UTF-8); anything else stays as it is. Returns the new length;
// nullptr -> 0.
size_t stripHtml(char* s);
// ASCII case-insensitive equality; false if either side is nullptr.
bool equalsIgnoreCase(const char* a, const char* b);
}
