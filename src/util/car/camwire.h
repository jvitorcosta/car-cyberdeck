#pragma once
#include <cstddef>
#include <cstdint>
#include "cameras.h"
#include "camlist.h"
// The /cam JSON the black deck serves and the yellow deck draws its band from. One small flat object:
//   {"st":"off|nofix|alert|alarm","kind":"speed","limit":60,"dist":320,"kmh":58,"label":"...","id":"mao-0007","mute":0}
// `st`, `kmh` and `mute` are always there (the band shows the speed in every stage); kind, limit, dist,
// label and id only when st is alert or alarm. limit 0 = unknown, or a red light. Labels are UTF-8.
// PURE, host-tested.
namespace camwire {
constexpr size_t MAX_JSON = 192;

// `active` is the point at status.index (nullptr when none). False when the buffer is too small, or
// when the stage is alert/alarm and there is no point to describe.
bool write(const cameras::Status& s, const camlist::Point* active, bool mute, char* out, size_t n);

struct View {
    cameras::Stage stage;
    camlist::Kind kind;  // Speed when the stage has no point
    uint8_t limit;
    uint16_t distM, kmh;
    bool mute;
    char label[28];
};
// False on anything it can't read: no/unknown `st`, or an alert/alarm without `kind` or `dist`.
// For off/nofix the point fields are zeroed (kind Speed, limit 0, distM 0, empty label).
bool parse(const char* json, View* out);
}  // namespace camwire
