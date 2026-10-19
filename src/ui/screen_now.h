#pragma once
#include <TFT_eSPI.h>
#include "app_state.h"
#include "statusscene.h"
namespace ui {
// Album art target rect inside the art battle box (main pushes the cover here).
constexpr int ART_X = 11, ART_Y = 27, ART_W = 92, ART_H = 92;

// Full Gen-3 battle deck: background, top strip, art box, info box, status box, dialogue box.
// Called on track change / return from a status screen. genre: genrebadge index for the
// artist row, or genre::NONE (no badge yet / none).
void drawNow(TFT_eSPI& t, const AppState& st, uint8_t genre);
// Repaints only the artist row in the info box (badge + name) when the genre arrives.
void drawArtistRow(TFT_eSPI& t, const AppState& st, uint8_t genre);
// Top strip only (title, CD, shuffle/repeat, device icon + name).
void drawTopStrip(TFT_eSPI& t, const AppState& st);
// Spinning CD after "NOW PLAYING" (frame 0..3).
void drawCdFrame(TFT_eSPI& t, int frame);
// Spinning Poke Ball before the time (frame 0..pokeball::FRAMES-1).
void drawPokeballFrame(TFT_eSPI& t, int frame);
// Poke Ball + time text + HP bar + EXP (volume) bar, repainted in place.
void drawProgressRegion(TFT_eSPI& t, const AppState& st);
// Walker on the HP bar. animMs = play-time animation clock (PMD frame timing);
// step = frame counter (fallback bob/mirror).
void drawWalker(TFT_eSPI& t, const AppState& st, uint32_t animMs, int step);
// Dialogue-box text (a lyric line or a status message), redrawn only when it changes.
// notes: draw the note icons (off for plain lyrics, whose timing is approximate).
void drawLyricArea(TFT_eSPI& t, const char* text, bool notes);
// Moves the dialogue-box note icons (frame >= 0 bobs them, < 0 puts them at rest); no-op
// while the box has no notes (plain lyrics).
void drawNoteFrame(TFT_eSPI& t, int frame);
// Forget the last drawn lyric so the next drawLyricArea() repaints (after drawNow).
void resetLyricArea();
// Full status screen ("No WiFi...", "Spotify busy...", "Spotify is silent"): the cause's Pokemon
// on its platform (ui/statusscene) and a dialogue box with `msg` and an optional smaller
// `detail` line. sound: the music box's speaker button in the box (statusscene::SOUND_ICON):
// -1 none, 0 muted, 1 on. qr: the phone-remote link drawn in the sky ("Spotify is silent" on
// the S3), or nullptr.
void drawOffline(TFT_eSPI& t, const char* msg, const char* detail, const statusscene::Look& look,
                 uint32_t animMs, int sound = -1, const statusscene::Qr* qr = nullptr);
// Repaints only the dialogue box (the text changed, e.g. the retry countdown).
void drawOfflineText(TFT_eSPI& t, const char* msg, const char* detail, int sound = -1);
// Repaints only the detail line (same title, new detail: the countdown ticking, the next tune).
void drawOfflineDetail(TFT_eSPI& t, const char* detail, int sound = -1);
// The music box's key (statusscene::SOUND_ICON), e.g. darker while a finger is on it.
void drawSoundButton(TFT_eSPI& t, int sound, bool pressed);
// The note before the tune's name, bobbing 1 px with frame.
void drawTuneNote(TFT_eSPI& t, int frame);
// Next animation frame of the scene (and the dialogue box's blinking arrow), pushed in small
// strips: no flicker, ~1.9 KB buffer.
void drawOfflineScene(TFT_eSPI& t, const statusscene::Look& look, uint32_t animMs, bool arrow = true,
                      const statusscene::Qr* qr = nullptr);
}
