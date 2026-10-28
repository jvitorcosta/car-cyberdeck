#pragma once
// How the yellow deck's terminal buttons show a press, in the theme's own colours so it shows on
// every theme, day and night. Idle: a box (MID border). Selected (the lit colour, the current tab,
// the Xbox focus on the Menu): the solid TEXT bar. Pressed (under the finger, or a controller's
// flash), selected or not: the TEXT bar with a BG ring RING px thick cut in INSET px from the edge,
// so it differs from both (a tap acts while the finger is down, so the tile it lights is already
// selected when it is drawn pressed). PURE, host-tested (test_presslook).
namespace presslook {
constexpr int INSET = 3, RING = 2;
struct Box {
    bool bar;   // the solid TEXT bar (else the bordered box)
    bool ring;  // the BG ring inside the bar
};
constexpr Box box(bool pressed, bool selected) { return {pressed || selected, pressed}; }
}  // namespace presslook
