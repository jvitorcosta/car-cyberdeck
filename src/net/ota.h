#pragma once
// Firmware updates over WiFi, pushed from the PC:
//   pio run -e s3_ota -t upload
// ArduinoOTA on port 3232, password REMOTE_TOKEN; off when REMOTE_TOKEN is empty, like the phone
// remote. Never pulled from the internet: the firmware carries the Spotify secrets (config.h).
// The update writes the other app slot and switches only once its SHA-256 checks out; the
// boot-loop guard (util/bootguard) goes back to the previous slot when new firmware keeps crashing.
namespace ota {
// Early in setup(): counts a crash reset and, after bootguard::LIMIT of them on firmware not yet
// proven, boots the other slot ("[ota] rolled back").
void checkBoot();
// Starts the listener task; it listens once `hold` is false (the boot scene) and WiFi is up.
void begin(bool (*hold)());
bool active();     // an update is being received: the network task pauses, the UI shows progress
int progress();    // 0-100 while active, else -1
// The network task, after each good Spotify poll: once the deck has run bootguard::HEALTHY_MS
// the running firmware counts as good and the crash count clears.
void onGoodPoll();
}
