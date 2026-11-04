#include "camseq.h"
#include <cstring>

namespace camseq {

using cameras::Stage;
using camclip::Slot;

static bool active(Stage s) { return s == Stage::Alert || s == Stage::Alarm; }

static void copyId(char* to, const char* from) {
    strncpy(to, from ? from : "", sizeof camlist::Point::id - 1);
    to[sizeof camlist::Point::id - 1] = '\0';
}

void Seq::onEvent(const Event& e, uint32_t now) {
    const bool was = active(stage_), is = active(e.to);
    const bool newPoint = is && (!was || strcmp(id_, e.id) != 0);
    if (was && (!is || newPoint)) {   // a point leaves Alert/Alarm: the short call counts from here
        left_ = true;
        leftMs_ = now;
        leftLimit_ = limit_;
    }

    // GPS lines. A loss drops a "back" not yet said; a fix back drops a "lost" not yet said, and
    // says "back" only after "lost" was heard.
    if (e.to == Stage::NoFix && stage_ != Stage::NoFix) {
        ngSaid_ = false;
        if (gps_ == Slot::GpsOk) gps_ = Slot::COUNT;
        if (e.lastKmh >= cameras::MIN_KMH && has(Slot::GpsNg)) gps_ = Slot::GpsNg;
    } else if (stage_ == Stage::NoFix && e.to != Stage::NoFix) {
        if (gps_ == Slot::GpsNg) gps_ = Slot::COUNT;
        if (ngSaid_ && has(Slot::GpsOk)) gps_ = Slot::GpsOk;
        ngSaid_ = false;
    }
    if (e.firstFix && !firstDone_) {
        firstDone_ = true;
        if (has(Slot::GpsFirst)) gps_ = Slot::GpsFirst;
    }

    // An alert's lines last while its point stays in Alert. The alarm needs nothing here: its slot
    // is due once stage_ is Alarm and the last one is ALARM_REPEAT_MS old (also across a flicker).
    if (e.to != Stage::Alert || newPoint) nLines_ = line_ = 0;
    if (e.to == Stage::Alert && newPoint) call(e, now);

    stage_ = e.to;
    alarmOff_ = false;
    copyId(id_, is ? e.id : "");
    limit_ = is ? e.limit : camlist::NO_LIMIT;
}

void Seq::call(const Event& e, uint32_t now) {
    // The same point again inside the guard is the matcher flickering, not news.
    if (chimed_ && e.kind == chimeKind_ && strcmp(e.id, chimeId_) == 0 && now - chimeMs_ < CHIME_GUARD_MS)
        return;
    chimed_ = true;
    copyId(chimeId_, e.id);
    chimeKind_ = e.kind;
    chimeMs_ = now;

    const bool red = e.kind == camlist::Kind::RedLight;
    const bool brief = !red && left_ && now - leftMs_ < SHORT_CALL_MS;
    int n = 0;
    lines_[n++] = has(Slot::Pon) ? Slot::Pon : Slot::COUNT;   // no chime clip: the old beep
    if (!brief) {
        const Slot what = e.kind == camlist::Kind::Speed ? Slot::Torishimari : Slot::Gochui;
        if (has(what)) lines_[n++] = what;
    } else if (leftLimit_ != camlist::NO_LIMIT && e.limit != camlist::NO_LIMIT && leftLimit_ != e.limit &&
               has(Slot::SpdChange)) {
        lines_[n++] = Slot::SpdChange;
    }
    const Slot limit = red ? Slot::COUNT : camclip::limitSlot(e.limit);
    if (has(limit)) lines_[n++] = limit;
    nLines_ = n;
    line_ = 0;
    callKind_ = e.kind;
    copyId(callId_, e.id);
}

bool Seq::alarmDue(uint32_t now) const {
    return alarmOn() && (!alarmed_ || now - alarmMs_ >= camtone::ALARM_REPEAT_MS);
}

static bool clip(Step* out, uint8_t prio, bool muteable, Slot s) {
    out->prio = prio;
    out->muteable = muteable;
    out->clip = true;
    out->slot = s;
    out->n = 0;
    return true;
}
static void tones(Step* out, uint8_t prio, bool muteable) {
    out->prio = prio;
    out->muteable = muteable;
    out->clip = false;
    out->slot = Slot::COUNT;
}

bool Seq::next(Stage live, const char* liveId, bool mute, uint32_t now, Step* out) {
    if (!liveId) liveId = "";
    // The change that ends an alarm is queued or was lost (a full queue): the live stage wins, and
    // only another change into Alarm starts it again.
    if (stage_ == Stage::Alarm && live != Stage::Alarm) alarmOff_ = true;
    if (alarmDue(now)) {
        alarmed_ = true;
        alarmMs_ = now;   // a late slot: the next counts from now
        if (!mute && has(Slot::Speedni) && (!voiced_ || now - voiceMs_ >= ALARM_VOICE_MS)) {
            voiced_ = true;
            voiceMs_ = now;
            return clip(out, ALARM, false, Slot::Speedni);
        }
        tones(out, ALARM, false);
        out->n = camtone::alarmRepeat(out->notes);
        return true;
    }
    if (alarmOn()) return false;                // between two slots: the rest waits
    if (line_ < nLines_) {
        if (mute || live != Stage::Alert || strcmp(liveId, callId_) != 0) {
            nLines_ = line_ = 0;                 // stale or muted: the rest of the call goes
        } else {
            const Slot s = lines_[line_++];
            if (s != Slot::COUNT) return clip(out, ALERT, true, s);
            tones(out, ALERT, true);
            out->n = camtone::onChange(Stage::Off, Stage::Alert, callKind_, out->notes);
            return true;
        }
    }
    if (gps_ != Slot::COUNT) {
        const Slot s = gps_;
        gps_ = Slot::COUNT;
        if (mute) return false;                  // not heard: no "back" for it either
        if (s == Slot::GpsNg) ngSaid_ = true;
        return clip(out, GPS, true, s);
    }
    return false;
}

uint32_t Seq::waitMs(uint32_t now) const {
    if (alarmOn()) {
        if (!alarmed_) return 0;
        const uint32_t since = now - alarmMs_;
        return since >= camtone::ALARM_REPEAT_MS ? 0 : camtone::ALARM_REPEAT_MS - since;
    }
    return line_ < nLines_ || gps_ != Slot::COUNT ? 0 : UINT32_MAX;
}

uint8_t Seq::top(uint32_t now) const {
    if (alarmDue(now)) return ALARM;
    if (alarmOn()) return NONE;
    if (line_ < nLines_) return ALERT;
    if (gps_ != Slot::COUNT) return GPS;
    return NONE;
}

bool Seq::cuts(uint8_t prio, uint32_t now) const { return top(now) > prio; }

}  // namespace camseq
