#pragma once
#include <cstdint>
#include "cameras.h"
#include "camlist.h"
#include "camtone.h"
// When the black deck's camera beeps play: camtone's patterns plus the timing around them. The
// buzzer task (black/car/camsound) feeds it every stage change and asks it again whenever waitMs()
// runs out. Three rules on top of camtone:
// - an alert chime for the point that chimed less than CHIME_GUARD_MS ago stays quiet (around
//   10 km/h near a red light the matcher flickers Alert / Off with every fix);
// - the alarm's triple beep plays at most every camtone::ALARM_REPEAT_MS, also across a flicker
//   out of Alarm and back, and only while the stage really is Alarm: tick() is given the live stage,
//   so a lost "-> off" can't keep it going;
// - muted, nothing plays, but the timing runs on (unmuting mid-alarm beeps at the next slot).
// Times are millis(), wrap-safe. PURE, host-tested.
namespace camcue {
constexpr uint32_t CHIME_GUARD_MS = 3000;

class Cue {
public:
    // A stage change (camsvc::onStage). `id` is the active point's (camlist::Point::id; nullptr or ""
    // when unknown, then the kind tells points apart). Returns the notes to play now (0: none).
    int onChange(cameras::Stage from, cameras::Stage to, camlist::Kind kind, const char* id, bool mute,
                 uint32_t now, camtone::Note* out);
    // No change came: the alarm's repeat when its slot is due. `live` is the status's stage now.
    int tick(cameras::Stage live, bool mute, uint32_t now, camtone::Note* out);
    // How long until tick() has something to do: UINT32_MAX when nothing is pending.
    uint32_t waitMs(uint32_t now) const;
private:
    int alarm(bool mute, uint32_t now, camtone::Note* out);
    cameras::Stage stage_ = cameras::Stage::Off;
    bool chimed_ = false;           // an alert chime has played (lastId_, lastKind_, chimeMs_ valid)
    char lastId_[sizeof camlist::Point::id] = "";
    camlist::Kind lastKind_ = camlist::Kind::Speed;
    uint32_t chimeMs_ = 0;
    bool alarmed_ = false;          // a triple beep has played (alarmMs_ valid)
    uint32_t alarmMs_ = 0;          // when the last one played
};
}  // namespace camcue
