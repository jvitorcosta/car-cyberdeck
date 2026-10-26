#pragma once
#include <cstddef>
#include <cstdint>
#include <IPAddress.h>
// Music mode on the black deck ("song colours"): while the yellow deck keeps it alive (net/remote's
// GET /music), a small task (core 1, 4 KB stack) sends the song's PalPkt (util/songpalette: cover
// colours, genre badge, time of day) over UDP to the requester's musicwire::PORT (util/musicwire):
// once a new song's cover is in (or COVER_WAIT_MS after the track change without one), on any later
// change and every 10 s. The socket exists only while it runs. No keep-alive for 30 s stops it. No
// mic: the music box and the real wolf howl play as usual.
namespace musicsvc {
constexpr uint32_t TIMEOUT_MS = 30000;
constexpr uint32_t COVER_WAIT_MS = 8000;
enum class Start : uint8_t { On, LowMemory, Stopping, NoTask };
// GET /music?on=1: starts music mode, or keeps it alive; the packets go to peer from now on.
// takePeer false (GET /music/debug's on=1): only keeps it alive, and takes peer only while none is
// set (a run's peer is cleared as it ends), so a laptop's debug loop never steals the stream from the
// yellow deck. LowMemory: not started (Spotify's TLS comes first); Stopping: the last run is still
// closing, ask again. Any task.
Start keepAlive(const IPAddress& peer, bool takePeer = true);
void stop();         // GET /music?on=0: the task closes within ~100 ms. Any task.
bool on();           // the task runs (or is closing). Any task.
// The network task: a new song (no cover colours yet), then its art decoded into art::bitmap().
void onTrack(uint32_t gen);
void onCover(uint32_t gen);
// GET /music/debug's JSON into out (on, the palette last sent, its cap, the socket, the largest free
// block now); false if it didn't fit. Any task.
bool debugJson(char* out, size_t n);
}
