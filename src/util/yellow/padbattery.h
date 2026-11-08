#pragma once
// The PS3 controller's battery on the yellow deck's Menu header: the DS3 reports a level, not a
// percentage (its status byte: 1 shutdown, 2 dying, 3 low, 4 high, 5 full, 0xEE charging, 0xEF
// charged on USB), shown as 0-4 cells of a battery icon. PURE, host-tested.
#include <cstdint>
#include <cstring>

namespace padbattery {
constexpr int CELLS = 4;

struct Look {
    int cells = -1;         // 0..CELLS; -1 before the first report (the outline alone)
    bool alert = false;     // dying or shutting down: drawn in ALERT
    bool charging = false;  // on USB, charging: the cells fill one by one, round
    bool operator==(const Look& o) const { return cells == o.cells && alert == o.alert && charging == o.charging; }
    bool operator!=(const Look& o) const { return !(*this == o); }
};

inline Look of(uint8_t raw) {
    Look l;
    switch (raw) {
    case 0x05: case 0xEF: l.cells = 4; break;
    case 0x04: l.cells = 3; break;
    case 0x03: l.cells = 2; break;
    case 0x02: l.cells = 1; l.alert = true; break;
    case 0x01: l.cells = 0; l.alert = true; break;
    case 0xEE: l.cells = 0; l.charging = true; break;
    default: break;  // no report yet (0), or a byte this table doesn't know
    }
    return l;
}

// The look after a report `raw`, from the one shown (`prev`): a byte the table doesn't know (the
// DS3 sends the odd one, 0x0F) keeps the last known level instead of blanking the icon.
inline Look next(const Look& prev, uint8_t raw) {
    const Look l = of(raw);
    return l.cells < 0 && prev.cells >= 0 ? prev : l;
}

// The cells drawn at `ms` (millis): the level, or while charging one more every 500 ms, 0..CELLS.
inline int shownCells(const Look& l, uint32_t ms) {
    return l.charging ? static_cast<int>((ms / 500) % (CELLS + 1)) : l.cells;
}

// The terminal message when the battery runs down, drawn in ALERT like the lost links.
constexpr const char* LOW_MESSAGE = ">PAD: BATTERY LOW";
inline bool isAlert(const char* msg) { return msg && strcmp(msg, LOW_MESSAGE) == 0; }
}  // namespace padbattery
