#include "ledpresets.h"

#include <cstdio>

namespace ledpresets {
const Preset PRESETS[N] = {
    {"ROSE", "rose", {2, 40, true}, {2, 100, true}},
    {"FIRE", "fire", {0, 50, true}, {0, 100, true}},
    {"CALM", "calm", {6, 40, true}, {6, 40, true}},
    {"PURPURA", "purpura", {3, 40, true}, {3, 100, true}},
};

namespace {
const Preset& at(int slot) { return PRESETS[slot < 1 ? 0 : slot > N ? N - 1 : slot - 1]; }
}  // namespace

void describe(int slot, char* out, size_t n) {
    const Preset& p = at(slot);
    // title case: first letter as is, the rest lowered
    char title[16];
    size_t i = 0;
    for (; p.name[i] && i < sizeof title - 1; i++) {
        const char c = p.name[i];
        title[i] = (i > 0 && c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
    }
    title[i] = 0;
    snprintf(out, n, "%s: inside %s %u percent, outside %s %u percent", title, ledlook::SWATCHES[p.in.swatch].name,
             static_cast<unsigned>(p.in.pct), ledlook::SWATCHES[p.out.swatch].name, static_cast<unsigned>(p.out.pct));
}

void wireLine(int slot, char* out, size_t n) {
    const Preset& p = at(slot);
    snprintf(out, n, "%d %s %s %u %s %u", slot, p.name, ledlook::SWATCHES[p.in.swatch].word, static_cast<unsigned>(p.in.pct),
             ledlook::SWATCHES[p.out.swatch].word, static_cast<unsigned>(p.out.pct));
}
}  // namespace ledpresets
