#pragma once
#include <cstdint>
#include "cameras.h"
#include "camclip.h"
#include "camlist.h"
#include "camtone.h"
// What the black deck says and beeps for the camera alerts, one step at a time (a voice clip or a
// camtone pattern). The sound task (black/car/camsound) feeds it every event camsvc reports and
// asks next() for the step to play whenever waitMs() runs out. The rules (the spec's "Sound"):
// - entering Alert: the chime, then the kind's line (speed: 取り締まり, bump and red light: ご注意),
//   then the limit line (20/30/40/50/60/80 only). A point entering Alert within SHORT_CALL_MS of
//   the previous one leaving Alert/Alarm gets the chime and the limit line (+ the "limit changed"
//   line between them when both limits are known and differ); a red light keeps its full call;
// - the same point again inside CHIME_GUARD_MS of its chime stays quiet (the matcher flickers
//   Alert / Off around 10 km/h);
// - an alert's lines play only while the live stage is still Alert for that point;
// - Alarm: the alarm line on entry, then camtone's triple beep every camtone::ALARM_REPEAT_MS; the
//   line again in a beep slot once ALARM_VOICE_MS have gone by since it last played (at once for
//   another point's alarm). The slots keep their cadence across a flicker out of Alarm and back,
//   and stop, the one playing included (stops()), when the stage leaves Alarm;
// - GPS: the lost line on NoFix when the last valid fix was at MIN_KMH or more (once per loss), the
//   back line on the next fix only after the lost line played, the GPS clip on the first fix after
//   power-on (once);
// - priority ALARM > ALERT > GPS: cuts() tells the player to stop a clip for a higher step;
// - a mark (Event::mark): camtone's mark chirp or low beep after the alert's lines, before a GPS line,
//   at GPS rank and never muted (the driver pressed for it); an alarm's slots go first;
// - mute: no chime, no voice (a line playing stops); the alarm's slots all beep;
// - a slot the pack lacks: a missing chime plays the kind's old beep (camtone) in its place, a
//   missing alarm line the triple beep; other missing lines are skipped (GPS lines have no beep).
// Times are millis(), wrap-safe. PURE, host-tested.
namespace camseq {
constexpr uint32_t CHIME_GUARD_MS = 3000;
constexpr uint32_t SHORT_CALL_MS = 20000;
constexpr uint32_t ALARM_VOICE_MS = 6000;
// A step's rank: a higher one cuts a lower one playing.
constexpr uint8_t NONE = 0, GPS = 1, ALERT = 2, ALARM = 3;
// Event::mark: no mark, a mark saved, a mark refused (no fix, storage).
constexpr uint8_t MARK_NONE = 0, MARK_SAVED = 1, MARK_FAILED = 2;

// What camsvc reports: a stage change, the active point changing within Alert/Alarm, or the first
// fix (then from == to may hold). kind, id, limit: the point entered, or the one just left when
// `to` has none.
struct Event {
    cameras::Stage from = cameras::Stage::Off, to = cameras::Stage::Off;
    camlist::Kind kind = camlist::Kind::Speed;
    char id[sizeof camlist::Point::id] = "";
    uint8_t limit = camlist::NO_LIMIT;
    uint16_t lastKmh = 0;    // the last valid fix's speed (0: none yet), for the lost line
    bool firstFix = false;   // the first valid fix since power-on
    uint8_t mark = MARK_NONE;   // a mark taken in the car: its answer, and nothing else in this event
};

struct Step {
    uint8_t prio;
    bool muteable;           // mute stops it (every clip and the alert beeps; not the alarm's beeps)
    bool clip;               // a clip (slot) or tones (notes, n)
    camclip::Slot slot;
    int n;
    camtone::Note notes[camtone::MAX_NOTES];
};

class Seq {
public:
    void clips(uint16_t present) { present_ = present; }   // camclip::Table::present()
    void onEvent(const Event& e, uint32_t now);
    // The step to play now, false when none is due. `live`: the status's stage now, `liveId` its
    // active point's id (nullptr or "" when none).
    bool next(cameras::Stage live, const char* liveId, bool mute, uint32_t now, Step* out);
    // How long until next() has something: 0 now, UINT32_MAX when nothing is pending.
    uint32_t waitMs(uint32_t now) const;
    // A step above `prio` is due: the player stops the clip it plays (at its next write).
    bool cuts(uint8_t prio, uint32_t now) const;
    // The live stage, read between two writes: an alarm whose "-> off" is still queued (or lost)
    // ends here as it would at the next slot.
    void sync(cameras::Stage live);
    // An alarm step playing now has to stop: the stage left Alarm (silence at once).
    bool stops(const Step& s) const { return s.prio == ALARM && !alarmOn(); }
    bool alarming() const { return alarmOn(); }
private:
    static constexpr int MAX_LINES = 3;
    bool has(camclip::Slot s) const { return (int)s < camclip::COUNT && (present_ >> (int)s) & 1; }
    void call(const Event& e, uint32_t now);
    bool alarmOn() const { return stage_ == cameras::Stage::Alarm && !alarmOff_; }
    bool alarmDue(uint32_t now) const;
    uint8_t top(uint32_t now) const;
    uint16_t present_ = 0;
    // Where the events left it (the live stage can be ahead: next() checks it).
    cameras::Stage stage_ = cameras::Stage::Off;
    bool alarmOff_ = false;  // stage_ is Alarm but the live stage left it
    char id_[sizeof camlist::Point::id] = "";
    uint8_t limit_ = camlist::NO_LIMIT;
    // The last point to leave Alert/Alarm, for the short call.
    bool left_ = false;
    uint32_t leftMs_ = 0;
    uint8_t leftLimit_ = camlist::NO_LIMIT;
    // The chime guard.
    bool chimed_ = false;
    char chimeId_[sizeof camlist::Point::id] = "";
    camlist::Kind chimeKind_ = camlist::Kind::Speed;
    uint32_t chimeMs_ = 0;
    // The alert's lines still to play (Slot::COUNT = the kind's beep), for point callId_.
    camclip::Slot lines_[MAX_LINES] = {};
    int nLines_ = 0, line_ = 0;
    camlist::Kind callKind_ = camlist::Kind::Speed;
    char callId_[sizeof camlist::Point::id] = "";
    // The alarm's slots.
    bool alarmed_ = false;
    uint32_t alarmMs_ = 0;   // the last slot
    bool voiced_ = false;
    uint32_t voiceMs_ = 0;   // the alarm line's last play...
    char voiceId_[sizeof camlist::Point::id] = "";   // ...and the point it was for
    // The GPS line waiting (Slot::COUNT: none), the lost line said this loss, the first fix seen.
    camclip::Slot gps_ = camclip::Slot::COUNT;
    bool ngSaid_ = false;
    bool firstDone_ = false;
    uint8_t mark_ = MARK_NONE;   // a mark's answer still to beep
};
}  // namespace camseq
