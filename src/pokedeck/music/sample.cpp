#include "sample.h"
#include <Arduino.h>
#include "app_state.h"
#include "nowscreen.h"
#include "../board/canvas.h"
#include "../../black/ui/theme.h"
#include "../../common/net/clock.h"
#include "../../util/black/genrebadge.h"
#include "../../util/common/daynight.h"

namespace sample {
namespace {
constexpr uint32_t DURATION_MS = 200000, STEP_MS = 250;
constexpr uint32_t APPLE_RNB = 15;   // Apple's genre id for R&B/Soul
AppState s_song{};
uint32_t s_startedAt = 0, s_steppedAt = 0;
int s_frame = 0;
bool s_ready = false;

// A stand-in for the cover: no art is downloaded yet.
void drawArt(TFT_eSPI& t) {
    for (int y = 0; y < pokeui::ART_H; ++y)
        t.drawFastHLine(pokeui::ART_X, pokeui::ART_Y + y, pokeui::ART_W,
                        t.color565(static_cast<uint8_t>(150 - y * 90 / pokeui::ART_H),
                                   static_cast<uint8_t>(40 + y * 30 / pokeui::ART_H),
                                   static_cast<uint8_t>(60 + y * 80 / pokeui::ART_H)));
    t.setTextDatum(MC_DATUM);
    t.setTextColor(TFT_WHITE);
    t.drawString("SAMPLE", pokeui::ART_X + pokeui::ART_W / 2, pokeui::ART_Y + pokeui::ART_H / 2, 4);
    t.setTextDatum(TL_DATUM);
}

// The black deck's schedule (18:00-06:00); day until the clock is synced.
bool night() {
    int m = 0;
    return netclock::minuteOfDay(&m) && daynight::isNight(m);
}
}  // namespace

void redraw() {
    if (!s_ready) return;
    theme::setNight(night());
    TFT_eSPI& t = canvas::screen();
    pokeui::drawNow(t, s_song, genrebadge::forGenreId(APPLE_RNB));
    drawArt(t);
    pokeui::drawLyricArea(t, "the lyric line shows here", true);
    canvas::present();
}

bool begin() {
    s_ready = canvas::begin();
    if (!s_ready) return false;
    strlcpy(s_song.trackName, "A sample song with a long title", sizeof s_song.trackName);
    strlcpy(s_song.artist, "Sample Artist", sizeof s_song.artist);
    strlcpy(s_song.context, "Night drive", sizeof s_song.context);
    strlcpy(s_song.deviceName, "iPhone", sizeof s_song.deviceName);
    strlcpy(s_song.deviceType, "Smartphone", sizeof s_song.deviceType);
    strlcpy(s_song.pokeName, "Minccino", sizeof s_song.pokeName);
    strlcpy(s_song.pokeType, "normal", sizeof s_song.pokeType);
    s_song.pokedexNum = 572;
    s_song.durationMs = DURATION_MS;
    s_song.isPlaying = true;
    s_song.shuffle = true;
    s_song.volume = 70;
    s_startedAt = millis();
    redraw();
    return true;
}

void tick(uint32_t now) {
    if (!s_ready || now - s_steppedAt < STEP_MS) return;
    s_steppedAt = now;
    if (night() != theme::isNightActive()) {
        redraw();
        return;
    }
    s_song.progressMs = (now - s_startedAt) % DURATION_MS;
    TFT_eSPI& t = canvas::screen();
    ++s_frame;
    pokeui::drawProgressRegion(t, s_song);
    pokeui::drawPokeballFrame(t, s_frame & 7);
    pokeui::drawCdFrame(t, s_frame & 3);
    canvas::present();
}
}  // namespace sample
