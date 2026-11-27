#pragma once
// The Pokédeck's web server (port 80), copied from black/net/remote and cut to what the other
// devices ask the music deck: the yellow deck's link check and music mode, and the watch's playlist
// picker, signed (util/wire/sigreq) or with ?k=. The phone page, karaoke, the gauges and the
// cameras aren't here. Off when REMOTE_TOKEN is empty. Its own idle-priority task.
namespace remote {
void begin();   // serves once WiFi is up
}
