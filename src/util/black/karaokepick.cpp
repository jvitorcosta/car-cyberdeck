#include "karaokepick.h"

namespace karaokepick {
namespace {
bool blank(const lyricbuf::Lyrics& l, int i) {
    for (const char* s = lyricbuf::lineText(l, i); *s; ++s)
        if (*s != ' ' && *s != '\t') return false;
    return true;
}
}  // namespace

Pick pick(const lyricbuf::Lyrics& l, uint32_t posMs, uint32_t durationMs) {
    Pick p{-1, -1, -1, 0, 0};
    int i = lyricbuf::currentIndex(l, posMs);
    while (i >= 0 && blank(l, i)) --i;
    p.cur = i;
    for (int j = p.cur - 1; j >= 0 && p.prev < 0; --j)
        if (!blank(l, j)) p.prev = j;
    for (int j = p.cur + 1; j < l.n && p.next < 0; ++j)
        if (!blank(l, j)) p.next = j;
    if (p.cur < 0) return p;
    p.startMs = l.lines[p.cur].tMs;
    if (p.cur + 1 < l.n) {
        p.endMs = l.lines[p.cur + 1].tMs;
    } else {
        p.endMs = p.startMs + LAST_LINE_MS;
        if (durationMs && durationMs < p.endMs) p.endMs = durationMs;
    }
    return p;
}

int sweepPx(uint32_t posMs, uint32_t startMs, uint32_t endMs, int widthPx) {
    if (endMs <= startMs || posMs >= endMs) return widthPx;
    if (posMs <= startMs) return 0;
    return static_cast<int>(static_cast<uint64_t>(widthPx) * (posMs - startMs) / (endMs - startMs));
}

Hop hopAt(uint32_t sinceMs, int fromX, int toX) {
    if (sinceMs >= HOP_MS) return {toX, 0, true};
    const float t = static_cast<float>(sinceMs) / static_cast<float>(HOP_MS);
    const int x = fromX + static_cast<int>(static_cast<float>(toX - fromX) * t);
    const int lift = static_cast<int>(4.0f * static_cast<float>(HOP_PX) * t * (1.0f - t) + 0.5f);
    return {x, lift, false};
}
}  // namespace karaokepick
