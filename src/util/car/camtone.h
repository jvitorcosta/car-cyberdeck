#pragma once
#include <cstdint>
#include "cameras.h"
#include "camlist.h"
// The beep patterns of the camera alerts, as plain note lists the black deck feeds to its buzzer.
// An alert sound plays once, on the step INTO Alert from Off or NoFix (not when an alarm eases back
// to alert: the driver just braked, no need to chime again); the triple beep plays on the step into
// Alarm and then repeats every ALARM_REPEAT_MS while the alarm lasts. Everything else is silent.
// Since the voice lines (util/camseq) these are the alarm's beeps and the fallback for a missing clip.
// PURE, host-tested.
namespace camtone {
struct Note { uint16_t hz; uint16_t ms; };  // hz 0 = rest
constexpr int MAX_NOTES = 6;
constexpr uint32_t ALARM_REPEAT_MS = 1500;

// The notes to play when the stage changes from `from` to `to` for a point of `kind`
// (alert: per kind; alarm: the triple beep; anything else: none). `out` holds MAX_NOTES.
// Returns the count.
int onChange(cameras::Stage from, cameras::Stage to, camlist::Kind kind, Note* out);
int alarmRepeat(Note* out);  // the triple beep again
// A mark from the car (the yellow deck's right trigger): saved, a short rising chirp unlike any
// alert's beeps; not saved (no GPS fix, storage), one long low beep.
int markSaved(Note* out);
int markFailed(Note* out);
}  // namespace camtone
