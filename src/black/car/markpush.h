#pragma once
#include <cstdint>
// The camera marks taken in the car, sent online as they come: each mark camsvc stores becomes an
// issue in MARKS_REPO (a private GitHub repo) with its time, position, course, speed and a map link,
// posted with MARKS_TOKEN over the hotspot. Oldest first, one per tick; a mark is counted as sent
// only on GitHub's 201, so one taken offline (a dead zone) goes out when the link is back. Off
// (nothing sent, the marks wait for import-car) when MARKS_REPO or MARKS_TOKEN is empty.
// Network task only: tick() runs between the Spotify polls, after the steps, and needs the TLS heap
// (netplan::TLS_NEED) like them.
namespace markpush {
// Sends the oldest unsent mark when due. now: millis().
void tick(uint32_t now);
}  // namespace markpush
