#pragma once
#include <cstddef>
#include <cstdint>
#include "../../util/car/cameras.h"
#include "../../util/car/camlist.h"
// Speed-camera alerts on the black deck: the list (NVS "cam"/"list", the CSV as pushed), the matcher
// fed by the phone's GPS (car/gps, on its task), the mute switch and the marks taken in the car.
// net/remote serves it as /cam; the buzzer (car/camsound) listens through onStage. The list is in
// NVS, not the media partition: that pack is read-only and replaced only by `uploadmedia`, while a
// push from the PC must take effect at once, without a reboot.
namespace camsvc {
// NVS -> list, then starts the GPS task (TCP client to GPS_HOST:GPS_PORT). Call once, from setup().
// Without the memory for the lists it never starts, and the status stays NoFix (GPS --): the road
// isn't being watched, and the decks must say so.
void begin();
// Thread-safe copy of the current status, the point at status.index (zeroed when none) and mute.
void snapshot(cameras::Status* s, camlist::Point* active, bool* mute);
// Parses into a scratch list and stores the CSV in NVS ("cam"/"list"); only then does it replace
// the live list. False keeps the old list in RAM and NVS: *badLine = the CSV line to fix (camlist),
// or 0 when the CSV was fine but NVS (or memory) refused it. One caller at a time (net/remote's task).
bool setList(const char* csv, size_t len, int* badLine);
void setMute(bool on);  // RAM only: every start is unmuted
// Appends the last valid fix (under cameras::NOFIX_MS old) to NVS ("cam"/"marks"), keeping the newest
// MAX_MARKS. NoFix without a fix; Storage when NVS (or memory) fails, the marks read back short
// included: those are kept as they are rather than overwritten.
constexpr int MAX_MARKS = 32;
enum class MarkResult : uint8_t { Ok, NoFix, Storage };
MarkResult mark();
// The marks as "ms,lat,lon,course,kmh\n" rows (ms = the deck's millis() at the fix, lat/lon in degrees
// with 6 decimals), oldest first. Returns the length written; stops at whole rows that fit in n.
size_t marksCsv(char* out, size_t n);
void clearMarks();
// For the buzzer: called on every stage change, in the order they happen: from the GPS task, and from
// setList's caller (net/remote's task) when a new list ends the current stage (-> Off). `kind` is the
// active point's (the one just left when `to` has none). One slot: a second call replaces the first.
// It runs on the caller's 4 KB stack (either task's), outside camsvc's lock but holding the next
// change back: start the sound and return.
void onStage(void (*cb)(cameras::Stage from, cameras::Stage to, camlist::Kind kind));
}  // namespace camsvc
