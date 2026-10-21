#pragma once
#include <Arduino.h>
#include "app_state.h"
namespace spclient {
bool hasToken();                       // src/config.h has a refresh token (tools/spotify_auth.py)
void begin();                          // first access token; then every request refreshes it as needed
// One GET /v1/me/player: track, progress, status, context name, device, volume, shuffle, repeat.
void poll(AppState& st);
}
