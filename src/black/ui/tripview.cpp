#include "tripview.h"
#include <Arduino.h>
#include <cstring>
#include "../../util/black/lyricface.h"
#include "../../util/car/tripticker.h"
#include "../../util/common/rgb565.h"

namespace ui {
namespace {
using rgb565::hex;
using tripticker::Cell;

// the mocks' terminal palette (native RGB565: sprite draws)
constexpr uint16_t BG = hex(0x020a04), TX = hex(0x9dffb0), TXM = hex(0x4fb870), TXD = hex(0x1f4a2c),
                   VAL = hex(0x7fe89a), AL = hex(0xff2020), WHITE = hex(0xffffff);
constexpr int CELL_W = 100, CELL_H = 92, HEAD_LINE = 22, FOOT_LINE = 219;
constexpr int N = (int)Cell::COUNT;

TFT_eSprite* s_frame = nullptr;   // the screen as drawn (PSRAM)
bool s_batch = false;             // a whole redraw: parts are drawn, the frame pushed once at the end
int s_state = 0;                  // 0 not made yet, 1 ready, -1 no memory
struct Shown {
    char head[24], status[8], val[N][tripticker::TEXT];
    bool alert[N];
} s_shown;

void face(lyricface::Face f) {
    s_frame->setFreeFont(reinterpret_cast<const GFXfont*>(&lyricface::font(f)));
}
int cellX(int i) { return 6 + i % 3 * 104; }
int cellY(int i) { return 26 + i / 3 * 96; }

void drawHeader(const char* head, const char* status) {
    TFT_eSprite& s = *s_frame;
    s.fillRect(0, 0, 320, HEAD_LINE, BG);
    face(lyricface::Face::LINE);
    s.setTextColor(TX);
    s.setTextDatum(TL_DATUM);
    s.drawString(head, 6, 2);
    s.setTextDatum(TR_DATUM);
    s.drawString(status, 314, 2);
    s.setFreeFont(nullptr);
    if (!s_batch) s.pushSprite(0, 0, 0, 0, 320, HEAD_LINE);
}

// A cell: its frame, label and value; the value at 36 px, or 24 when that is too wide ("COAST").
void drawCell(int i, const char* val, bool alert) {
    TFT_eSprite& s = *s_frame;
    const int x = cellX(i), y = cellY(i);
    s.fillRect(x, y, CELL_W, CELL_H, alert ? AL : BG);
    s.drawRect(x, y, CELL_W, CELL_H, alert ? AL : TXD);
    s.setTextDatum(TL_DATUM);
    face(lyricface::Face::SMALL);
    s.setTextColor(alert ? WHITE : TXM);
    s.drawString(tripticker::label((Cell)i), x + 5, y + 3);
    face(lyricface::Face::SUNG);
    if (s.textWidth(val) > CELL_W - 10) face(lyricface::Face::SUNG_SMALL);
    s.setTextColor(alert ? WHITE : i == (int)Cell::KmlNow ? TX : VAL);
    s.drawString(val, x + 5, y + 30);
    s.setFreeFont(nullptr);
    if (!s_batch) s.pushSprite(x, y, x, y, CELL_W, CELL_H);
}

void drawFrame() {   // the lines and the prompt, which never change
    TFT_eSprite& s = *s_frame;
    s.fillSprite(BG);
    s.drawFastHLine(0, HEAD_LINE, 320, TXD);
    s.drawFastHLine(0, FOOT_LINE, 320, TXD);
    face(lyricface::Face::LINE);
    s.setTextColor(TXM);
    s.setTextDatum(TL_DATUM);
    s.drawString("> _ SCREENS: PHONE PAGE, PARKED", 6, FOOT_LINE + 2);
    s.setFreeFont(nullptr);
}
}  // namespace

bool tripReady(TFT_eSPI& tft) {
    if (s_state == 0) {
        s_state = -1;
        s_frame = new TFT_eSprite(&tft);
        s_frame->setColorDepth(16);
        if (s_frame->createSprite(320, 240)) {   // PSRAM: TFT_eSprite takes it when there is some
            s_state = 1;
        } else {   // not tried again
            delete s_frame;
            s_frame = nullptr;
        }
        Serial.printf("[screen] trip ticker %s\n", s_state == 1 ? "ready" : "out of memory");
    }
    return s_state == 1;
}

void drawTrip(TFT_eSPI& tft, const obdwire::Live& l, bool whole) {
    if (!tripReady(tft)) return;
    s_batch = whole;
    if (whole) {
        drawFrame();
        memset(&s_shown, 0, sizeof s_shown);
    }
    char head[sizeof s_shown.head];
    tripticker::header(l, head, sizeof head);
    const char* status = tripticker::status(l.link);
    if (whole || strcmp(head, s_shown.head) != 0 || strcmp(status, s_shown.status) != 0) {
        drawHeader(head, status);
        strlcpy(s_shown.head, head, sizeof s_shown.head);
        strlcpy(s_shown.status, status, sizeof s_shown.status);
    }
    for (int i = 0; i < N; ++i) {
        char val[tripticker::TEXT];
        tripticker::text((Cell)i, l, val);
        const bool alert = tripticker::alert((Cell)i, l);
        if (!whole && alert == s_shown.alert[i] && strcmp(val, s_shown.val[i]) == 0) continue;
        drawCell(i, val, alert);
        strlcpy(s_shown.val[i], val, sizeof s_shown.val[i]);
        s_shown.alert[i] = alert;
    }
    if (s_batch) s_frame->pushSprite(0, 0);
    s_batch = false;
}
}  // namespace ui
