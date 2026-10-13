#pragma once
#include <Arduino.h>
#include "app_state.h"
namespace spclient {
bool hasToken();                       // src/config.h has a refresh token (tools/spotify_auth.py)
void begin();
void poll(AppState& st);               // now playing: track, progress, status, context name
void pollPlayerDetails(AppState& st); // fills deviceName/volume/shuffle/repeat
}
