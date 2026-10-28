#pragma once
#include <cstddef>
#include "../../util/common/remotegate.h"
#include "../../util/wire/sigreq.h"
class WebServer;
// Who may call a deck's route, and how the decks sign what they send each other (util/wire/sigreq,
// keyed by REMOTE_TOKEN). The phone's Siri Shortcuts and Home Screen page can't compute an HMAC, so
// the bearer ?k= stays accepted where they call; devices (the yellow deck, the watch) sign instead
// and never send the key. begin() runs HMAC's self-test once: if it fails nothing is signed and
// nothing signed is accepted (the bearer key still works).
namespace reqauth {
void begin();
// HMAC's self-test passed (begin): music mode's signed packets (util/wire/musicwire) are off without it.
bool hmacOk();
// True when the request carries the bearer key (gate) or a valid signature for `path`. Else it has
// answered: 403 "clock off" for a signature outside the 2-minute window (or from before this boot),
// 503 "clock not set" for a signed request before SNTP (a signature can't be checked yet), else 403
// with wrongKeyText. Logs a refused signature's reason, never the key or the tag.
bool pass(WebServer& s, const remotegate::Gate& gate, const char* path, const char* wrongKeyText);
// "t=..&n=..&sig=.." (sigreq::AUTH_LEN) for a GET of `path` with these params. False while the clock
// isn't set or after a failed self-test: the caller sends nothing then.
bool signGet(const char* path, const sigreq::Param* params, int count, char* out, size_t n);
}
