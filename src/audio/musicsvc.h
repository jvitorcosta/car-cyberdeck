#pragma once
#include <cstddef>
#include <cstdint>
#include <IPAddress.h>
// Music mode on the black deck: while the yellow deck keeps it alive (net/remote's GET /music), a
// capture task (core 1, 4 KB stack, static buffers) reads the mic (board/s3/mic_es8311), runs
// util/musicfeat and sends a FeatPkt every second 16 ms read (~32 ms, ~31/s) plus the song's PalPkt
// (util/songpalette) on change and every 10 s, over UDP to the requester's musicwire::PORT
// (util/musicwire). The socket exists only while it runs. No keep-alive for 30 s stops it. While on,
// the music box stays off (main.cpp). Below TLS_NEED + 4 KB in the largest free block the mic lets go
// of the I2S port (its DMA buffers) until TLS_NEED + 12 KB is free again.
namespace musicsvc {
constexpr uint32_t TIMEOUT_MS = 30000;
enum class Start : uint8_t { On, LowMemory, Stopping, NoTask };
// GET /music?on=1: starts music mode (stopping the music box), or keeps it alive; the packets go to
// peer from now on. LowMemory: not started (Spotify's TLS comes first); Stopping: the last run is
// still closing, ask again. Any task.
Start keepAlive(const IPAddress& peer);
void stop();         // GET /music?on=0: the capture task closes at its next read (~16 ms). Any task.
bool on();           // the capture task runs (or is closing). Any task.
// The network task: a new song (no cover colours yet), then its art decoded into art::bitmap().
void onTrack(uint32_t gen);
void onCover(uint32_t gen);
// GET /music/debug's JSON into out (ageMs: since the last feature update, -1 for none since the last
// reset; largest: the largest free block now); false if it didn't fit. Any task.
bool debugJson(char* out, size_t n);
}
