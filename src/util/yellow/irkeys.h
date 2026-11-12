#pragma once
// The light kit's 20-button IR remote on the yellow deck (a KY-022 on GPIO 27, lib/irrx): which
// code is which button, what each one does to both lights, and how a held button repeats. The
// codes were captured on 2026-11-12 (src/diag/irtest.cpp): NEC, one address, commands 0x00..0x13
// in the buttons' row order. Only that address counts, so another remote (a TV's) does nothing.
// PURE, host-tested.
#include <cstddef>
#include <cstdint>
#include "../lights/ledlook.h"

namespace irkeys {
enum class Key : uint8_t {
    BrightUp, BrightDown, On, Off,
    Red, Green, Blue, White,
    Orange, Yellow, Cyan, Purple,
    Jump3, Jump7, Fade3, Fade7,
    Music1, Music2, Music3, Music4,
    N
};

constexpr uint16_t ADDRESS = 0xEF00;

// The button's command is its row-order index (0x00 = brightness up .. 0x13 = the fourth music key).
inline bool lookup(uint16_t address, uint8_t command, Key* out) {
    if (address != ADDRESS || command >= static_cast<uint8_t>(Key::N)) return false;
    *out = static_cast<Key>(command);
    return true;
}

// What a button does, on both lights: a level step (+1 / -1), power (1 on, 0 off), a colour (its
// ledlook swatch), one of the controllers' four effects (0..3: JUMP3, JUMP7, FADE3, FADE7), or one
// of the four playlists (0..3).
enum class Kind : uint8_t { Level, Power, Colour, Effect, Playlist };
struct Action {
    Kind kind;
    int value;
};

inline Action action(Key k) {
    static constexpr uint8_t COLOURS[8] = {ledlook::RED,    ledlook::GREEN,  ledlook::BLUE, ledlook::WHITE,
                                           ledlook::ORANGE, ledlook::YELLOW, ledlook::CYAN, ledlook::PURPLE};
    const int i = static_cast<int>(k);
    switch (k) {
        case Key::BrightUp: return {Kind::Level, 1};
        case Key::BrightDown: return {Kind::Level, -1};
        case Key::On: return {Kind::Power, 1};
        case Key::Off: return {Kind::Power, 0};
        default: break;
    }
    if (k >= Key::Red && k <= Key::Purple) return {Kind::Colour, COLOURS[i - static_cast<int>(Key::Red)]};
    if (k >= Key::Jump3 && k <= Key::Fade7) return {Kind::Effect, i - static_cast<int>(Key::Jump3)};
    return {Kind::Playlist, i - static_cast<int>(Key::Music1)};
}

// The controllers' built-in effect (elkcmd::effect, 0..212) for JUMP3, JUMP7, FADE3 and FADE7, at
// EFFECT_SPEED (0..100). TUNE: starting values, to be found in the car with the IR_EFFECT_PROBE
// build (docs/BACKLOG.md).
constexpr uint8_t EFFECT_MODE[4] = {0, 1, 2, 3};
constexpr uint8_t EFFECT_SPEED = 50;

// A held brightness key steps once every STEP_MS (about four a second); every other key acts once
// however long it is held. NEC sends a repeat frame every ~110 ms while held; some remotes send the
// whole frame again instead, so a frame of the same key within SAME_FRAME_MS of its last one is a
// repeat too (or ON, a colour or a playlist would act twice).
constexpr uint32_t STEP_MS = 250, SAME_FRAME_MS = 200, STALE_MS = 250;

class Repeat {
public:
    bool accept(Key k, bool repeat, uint32_t now) {
        if (k != last_) {
            last_ = k;
            frameAt_ = actAt_ = now;
            return true;
        }
        // A repeat frame long after this key's last frame isn't this key held: IRremote reports a
        // repeat as the last command it decoded, so another key whose first frame was lost would
        // step the lights. Its whole train is ignored (frameAt_ stays), until a full frame comes.
        if (repeat && since(frameAt_, now) > STALE_MS) return false;
        const bool again = repeat || since(frameAt_, now) < SAME_FRAME_MS;
        frameAt_ = now;
        if (!again || ((k == Key::BrightUp || k == Key::BrightDown) && since(actAt_, now) >= STEP_MS)) {
            actAt_ = now;
            return true;
        }
        return false;
    }

private:
    static uint32_t since(uint32_t at, uint32_t now) {
        const int32_t d = static_cast<int32_t>(now - at);
        return d < 0 ? 0 : static_cast<uint32_t>(d);
    }

    Key last_ = Key::N;
    uint32_t frameAt_ = 0, actAt_ = 0;
};
}  // namespace irkeys
