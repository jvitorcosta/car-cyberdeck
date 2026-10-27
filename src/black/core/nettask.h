#pragma once
// The network task (core 0): Spotify polls, per-track downloads, walker loading.
// It never draws; results reach the UI through core/shared.
namespace nettask {
// Starts the task. It waits for WiFi (connecting itself when the hotspot comes up after the
// deck), logs into Spotify (spclient::begin) and starts the clock, then polls.
void start();
}
