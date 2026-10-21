#include "deckview.h"
#include <Arduino.h>
#include <cstring>
#include "screen_now.h"
#include "pokeball.h"
#include "lyricmsg.h"
#include "../app/display.h"
#include "../core/shared.h"
#include "../core/mem.h"
#include "../net/remote.h"
#include "../images/art.h"
#include "../util/genre.h"

namespace deckview {

constexpr uint32_t CD_FRAME_MS = 160;     // spinning CD + Poke Ball, ~6 fps
constexpr uint32_t PROGRESS_MS = 250;     // time, HP/EXP bars, dialogue box
constexpr uint32_t NOTES_MS = 330;        // note icons bob ~3 steps/s
constexpr uint32_t WALK_FRAME_MS = 120;   // walker ~8 fps

// The art box and the decoded bitmap are the same 92x92 area (pushArtIfValid pushes it whole).
static_assert(ui::ART_W == art::W && ui::ART_H == art::H, "art box and bitmap sizes differ");

static char s_topSig[96] = "";                 // last drawn top-strip state

// Pushes the album-art bitmap if it is valid for the track on screen. Check and push happen
// under one lock so the network task cannot start overwriting the bitmap in between.
static void pushArtIfValid(uint32_t gen) {
    shared::Guard g;
    if (shared::artValidLocked(gen))
        app::tft().pushImage(ui::ART_X, ui::ART_Y, art::W, art::H, art::bitmap());
}

static bool s_notesDance = false;   // a timed lyric line is on screen (set by drawDialogue)

// Dialogue box for the track on screen: the lyric line, or a Pokemon-style status message
// (searching / retrying / intro / no lyrics). The status age drives the FAIL -> IDLE switch.
static void drawDialogue(const AppState& view, uint32_t gen) {
    static lyricstatus::Status lastStatus = lyricstatus::Status::Searching;
    static uint32_t lastGen = 0, since = 0;
    static shared::LyricView lv;      // static: keep the loop stack small
    static lyricmsg::Out out;
    shared::lyricView(gen, view.progressMs, lv);
    uint32_t now = millis();
    if (gen != lastGen || lv.status != lastStatus) {
        lastGen = gen;
        lastStatus = lv.status;
        since = now;
    }
    lyricmsg::In in{lv.status, now - since, view.progressMs, lv.firstLineMs, lv.line,
                    view.pokeName, gen};
    lyricmsg::compose(in, out);
    ui::drawLyricArea(app::tft(), out.text, out.notes);
    s_notesDance = out.dance;
}

void draw(const AppState& st, const AppState& view, bool fresh) {
    TFT_eSPI& tft = app::tft();
    static uint32_t shownGen = 0;
    static bool walkerOn = false;
    static uint8_t shownGenre = genre::NONE;   // badge for the track on screen
    if (fresh || st.trackGen != shownGen) {
        // Full deck redraw: immediately on track change (text + Pokemon name; art, lyric
        // and walker fill in as the network task delivers them), or back from a status screen.
        bool newTrack = st.trackGen != shownGen;
        if (fresh) {   // the phone page: no status, the song
            remote::setStatus("", "");
            remote::setTunes("");
        }
        if (newTrack) {
            shownGenre = genre::NONE;
            shownGen = st.trackGen;
            walkerOn = false;
        }
        uint8_t cached;   // a cached badge is posted with the track change: draw it with the title
        if (shared::takeGenre(shownGen, &cached)) shownGenre = cached;
        ui::drawNow(tft, view, shownGenre);
        s_topSig[0] = '\0';
        pushArtIfValid(shownGen);   // back from a status screen: same track's art is still valid
        drawDialogue(view, shownGen);
        mem::log("track");
        return;
    }
    // Media arriving from the network task for the track on screen.
    if (shared::takeArt(shownGen)) pushArtIfValid(shownGen);
    if (shared::takeWalker(shownGen)) walkerOn = true;
    uint8_t g;
    if (shared::takeGenre(shownGen, &g)) {   // genre arrived (or was cached): artist row only
        shownGenre = g;
        ui::drawArtistRow(tft, view, g);
    }

    static uint32_t lastDraw = 0, lastWalk = 0, lastCd = 0, lastTick = 0, animMs = 0;
    static int walkStep = 0, cdFrame = 0, ballFrame = 0;
    uint32_t now = millis();
    uint32_t dt = now - lastTick;
    lastTick = now;
    if (st.isPlaying) animMs += dt;   // walk cycle runs only while playing

    // top strip: redraw only when device / shuffle / repeat change
    char sig[96];
    snprintf(sig, sizeof(sig), "%s|%s|%d|%d|%d", st.deviceName, st.deviceType,
             (int)st.shuffle, st.repeat, (int)st.isPlaying);
    if (strcmp(sig, s_topSig) != 0) {
        strcpy(s_topSig, sig);
        ui::drawTopStrip(tft, st);
    }
    if (st.isPlaying && now - lastCd >= CD_FRAME_MS) {   // spinning CD ~6 fps
        lastCd = now;
        ui::drawCdFrame(tft, cdFrame = (cdFrame + 1) & 3);
        ui::drawPokeballFrame(tft, ballFrame = (ballFrame + 1) % pokeball::FRAMES);
    }
    if (now - lastDraw >= PROGRESS_MS) {
        lastDraw = now;
        ui::drawProgressRegion(tft, view);
        drawDialogue(view, shownGen);
    }
    // Note icons bob while timed lyrics are shown and the music plays; at rest otherwise.
    static uint32_t lastNotes = 0;
    static int noteFrame = 0;
    static bool notesMoved = false;
    if (s_notesDance && st.isPlaying) {
        if (now - lastNotes >= NOTES_MS) {            // ~3 steps/s: alive, not distracting
            lastNotes = now;
            ui::drawNoteFrame(tft, ++noteFrame & 0x7FFF);
            notesMoved = true;
        }
    } else if (notesMoved) {
        ui::drawNoteFrame(tft, -1);
        notesMoved = false;
    }
    if (walkerOn && now - lastWalk >= WALK_FRAME_MS) {     // ~8 fps walker
        lastWalk = now;
        if (st.isPlaying) walkStep++;
        ui::drawWalker(tft, view, animMs, walkStep);
    }
}

}
