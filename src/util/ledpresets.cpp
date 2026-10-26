#include "ledpresets.h"

#include <cstdio>

namespace ledpresets {
static constexpr char GOJIRA_SHOWN[] = {GOJIRA_MARK, 0};
const Preset PRESETS[N] = {
    {"ROSE", "rose", {2, 40, true}, {2, 100, true}},
    {"FIRE", "fire", {0, 40, true}, {0, 100, true}},
    {"GOJIRA", "gojira", {ledlook::LIGHT_BLUE, 40, true}, {ledlook::LIGHT_BLUE, 100, true}, GOJIRA_SHOWN},
    {"PURPURA", "purpura", {3, 40, true}, {3, 100, true}},
    {"CIVIC", "civic", {5, 40, true}, {5, 100, true}},
    {"OCEAN", "ocean", {4, 40, true}, {4, 100, true}},
};

namespace {
// An out-of-range slot clamps to 1..N, and callers print the clamped number so a line never pairs one
// slot's number with another's data.
int clampSlot(int slot) { return slot < 1 ? 1 : slot > N ? N : slot; }
const Preset& at(int slot) { return PRESETS[clampSlot(slot) - 1]; }
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
    snprintf(out, n, "%d %s %s %u %s %u", clampSlot(slot), p.name, ledlook::SWATCHES[p.in.swatch].word, static_cast<unsigned>(p.in.pct),
             ledlook::SWATCHES[p.out.swatch].word, static_cast<unsigned>(p.out.pct));
}
}  // namespace ledpresets
