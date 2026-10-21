#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>
// What the companion shows while the car is there: the gauge cluster (with a lyric strip) when
// moving, her lyrics screen (with a gauge strip) when stopped. The owner pins a view from the
// phone: Drive (always the cluster), Lyrics (always the lyrics) or Auto (by speed). Without the
// car it's her menu screens. PURE, host-tested.
namespace driveview {
enum class Pin : uint8_t { Auto, Drive, Lyrics };
enum class Show : uint8_t { Menu, Cluster, Stopped };
constexpr uint32_t STOP_MS = 3000;     // at 0 km/h this long: stopped (not a stop-and-go blip)
constexpr float MOVING_KMH = 5.0f;     // from this speed: moving (creeping keeps the lyrics)

class Mode {
public:
    // carShown: the cluster's hold says the car is there (obdplan::Hold). kmh: a fresh speed, NAN
    // when unknown or stale (the view stays as it is).
    Show update(Pin pin, bool carShown, float kmh, uint32_t nowMs) {
        if (!carShown) {
            stopped_ = zero_ = false;
            return Show::Menu;
        }
        if (!std::isnan(kmh)) {
            if (kmh >= MOVING_KMH) {
                stopped_ = zero_ = false;
            } else if (kmh <= 0.0f) {
                if (!zero_) { zero_ = true; zeroSince_ = nowMs; }
                if (nowMs - zeroSince_ >= STOP_MS) stopped_ = true;
            } else {
                zero_ = false;                    // creeping: no new stop starts, none ends
            }
        }
        if (pin == Pin::Drive) return Show::Cluster;
        if (pin == Pin::Lyrics) return Show::Stopped;
        return stopped_ ? Show::Stopped : Show::Cluster;
    }
private:
    bool stopped_ = false, zero_ = false;
    uint32_t zeroSince_ = 0;
};

inline const char* name(Pin p) { return p == Pin::Drive ? "drive" : p == Pin::Lyrics ? "lyrics" : "auto"; }
inline const char* name(Show s) { return s == Show::Cluster ? "drive" : s == Show::Stopped ? "lyrics" : "menu"; }
// "auto" "drive" "lyrics" (exact, lower case); false for anything else (*out untouched).
inline bool parse(const char* s, Pin* out) {
    if (!s) return false;
    for (int i = 0; i < 3; ++i)
        if (strcmp(s, name((Pin)i)) == 0) { *out = (Pin)i; return true; }
    return false;
}
}
