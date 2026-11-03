#include "camcue.h"
#include <cstring>

namespace camcue {

using cameras::Stage;

int Cue::alarm(bool mute, uint32_t now, camtone::Note* out) {
    alarmed_ = true;
    alarmMs_ = now;   // muted too: the slot passes silently, so unmuting keeps the cadence
    return mute ? 0 : camtone::alarmRepeat(out);
}

int Cue::onChange(Stage from, Stage to, camlist::Kind kind, const char* id, bool mute, uint32_t now,
                  camtone::Note* out) {
    stage_ = to;
    if (to == Stage::Alarm) {
        // Back into Alarm right after a triple beep: tick() plays it when its slot comes.
        if (alarmed_ && now - alarmMs_ < camtone::ALARM_REPEAT_MS) return 0;
        return alarm(mute, now, out);
    }
    const int n = camtone::onChange(from, to, kind, out);
    if (n == 0) return 0;
    // An alert chime: the same point again inside the guard is the matcher flickering, not news.
    if (!id) id = "";
    if (chimed_ && kind == lastKind_ && strcmp(id, lastId_) == 0 && now - chimeMs_ < CHIME_GUARD_MS)
        return 0;
    chimed_ = true;
    strncpy(lastId_, id, sizeof lastId_ - 1);
    lastId_[sizeof lastId_ - 1] = '\0';
    lastKind_ = kind;
    chimeMs_ = now;
    return mute ? 0 : n;
}

int Cue::tick(Stage live, bool mute, uint32_t now, camtone::Note* out) {
    if (stage_ != Stage::Alarm) return 0;
    if (live != Stage::Alarm) {   // the change that ended it never came: the live stage wins
        stage_ = live;
        return 0;
    }
    if (alarmed_ && now - alarmMs_ < camtone::ALARM_REPEAT_MS) return 0;
    return alarm(mute, now, out);
}

uint32_t Cue::waitMs(uint32_t now) const {
    if (stage_ != Stage::Alarm) return UINT32_MAX;
    if (!alarmed_) return 0;
    const uint32_t since = now - alarmMs_;
    return since >= camtone::ALARM_REPEAT_MS ? 0 : camtone::ALARM_REPEAT_MS - since;
}

}  // namespace camcue
