#include "statusview.h"
#include <Arduino.h>
#include <cstring>
#include <qrcode.h>
#include "screen_now.h"
#include "theme.h"
#include "../app/display.h"
#include "../core/mem.h"
#include "../net/wifi.h"
#include "../net/remote.h"
#include "../audio/greeting.h"
#include "../board/s3/touch_ft6336.h"
#include "../util/linkstatus.h"
#include "../util/netplan.h"
#include "../util/press.h"
#include "../util/qrfit.h"

namespace statusview {

// Status screen text when Spotify can't be polled: whose fault it is (util/linkstatus).
static linkstatus::Kind offlineText(const AppState& st, char* title, size_t tn, char* detail, size_t dn) {
    const bool wifiUp = net::isOnline();
    linkstatus::Kind k = linkstatus::classify({wifiUp, true, st.lastPollCode, st.certRejected});
    char names[96];
    if (wifiUp) strlcpy(names, net::ssid().c_str(), sizeof names);
    else net::savedNetworks(names, sizeof names);
    linkstatus::describe(k, st.lastPollCode, linkstatus::retryInS(millis(), st.nextPollMs), names,
                         title, tn, detail, dn);
    return k;
}

// The phone-remote link as a QR code for the "Waiting for a song..." screen (esp_qrcode, built into
// the core), from the deck's current address: 172.20.10.10 on the iPhone hotspot, the DHCP one
// elsewhere. Rebuilt when the link changes (another network, a reconnect, the hotspot address
// pinned after DHCP), checked when the screen appears and then at most every 5 s:
// net::deviceIp() allocates a String.
constexpr int QR_N = qrfit::modules(qrfit::MAX_VERSION);   // modules per side (29)
static uint8_t s_qrBits[(QR_N * QR_N + 7) / 8];
static qrcard::Qr s_qr{s_qrBits, 0};
static uint32_t s_qrGen = 0;   // bumped when the code changes: the screen redraws it

static void copyQr(esp_qrcode_handle_t h) {
    const int n = esp_qrcode_get_size(h);
    s_qr.size = 0;
    if (n > QR_N) return;
    memset(s_qrBits, 0, sizeof s_qrBits);
    for (int y = 0; y < n; ++y)
        for (int x = 0; x < n; ++x)
            if (esp_qrcode_get_module(h, x, y)) {
                const int i = y * n + x;
                s_qrBits[i >> 3] |= (uint8_t)(0x80 >> (i & 7));
            }
    s_qr.size = n;
}

static const qrcard::Qr* phoneQr(bool recheck) {
    static char built[96] = "";
    static uint32_t checkedAt = 0;
    if (recheck || millis() - checkedAt >= 5000) {
        checkedAt = millis();
        char url[96];
        if (!remote::link(url, sizeof url)) {
            if (built[0]) {
                Serial.println("[qr] none (remote off or offline)");
                ++s_qrGen;
            }
            built[0] = '\0';
            s_qr.size = 0;
        } else if (strcmp(url, built) != 0) {
            strlcpy(built, url, sizeof built);
            esp_qrcode_config_t cfg = ESP_QRCODE_CONFIG_DEFAULT();
            cfg.display_func = copyQr;
            cfg.max_qrcode_version = qrfit::MAX_VERSION;
            cfg.qrcode_ecc_level = ESP_QRCODE_ECC_LOW;
            s_qr.size = 0;
            esp_qrcode_generate(&cfg, url);
            ++s_qrGen;
            const char* key = strstr(url, "?k=");                  // the key stays off the log
            Serial.printf("[qr] %.*s?k=<REMOTE_TOKEN> (%d modules)\n",
                          key ? (int)(key - url) : (int)strlen(url), url, s_qr.size);
        }
    }
    return s_qr.size ? &s_qr : nullptr;
}

// It holds the audio driver and a 4 KB task, so only with Spotify's TLS memory to spare: it
// starts with the same margin as a phone howl, stops when memory runs low, and then tries again
// at most once a minute.
void musicBox(bool want) {
    constexpr uint32_t RETRY_MS = 60000;
    static uint32_t gaveUpAt = 0;
    static bool gaveUp = false;
    if (!want) {
        if (greeting::tunesPlaying()) greeting::stopTunes();
        gaveUp = false;
        return;
    }
    if (greeting::tunesPlaying()) {
        if (mem::byteLargest() < netplan::TLS_NEED + 4000) {
            Serial.printf("[tunes] low memory (largest %u): stopping\n", (unsigned)mem::byteLargest());
            greeting::stopTunes();
            gaveUp = true;
            gaveUpAt = millis();
        }
        return;
    }
    if (gaveUp && millis() - gaveUpAt < RETRY_MS) return;
    if (greeting::playing()) return;   // a howl, or the last tune still fading: next pass
    if (mem::byteLargest() < netplan::TLS_NEED + 12000) {
        gaveUp = true;
        gaveUpAt = millis();
        return;
    }
    gaveUp = false;
    greeting::playTunes();
}

// A tap on the music box's speaker button (statusscene::onSoundButton): fires on release, as on
// the companion. Touch is read every 20 ms, only while "Waiting for a song..." is up.
static press::Press s_soundPress;
static bool soundTapped() {
    press::Press& p = s_soundPress;
    static uint32_t at = 0;
    if (millis() - at < 20) return false;
    at = millis();
    int x = 0, y = 0;
    const bool down = touchft::read(&x, &y);
    return p.update(down, down && statusscene::onSoundButton(x, y) ? 1 : 0, false, millis()) == 1;
}

void draw(const AppState& st, Mode mode, bool fresh) {
    TFT_eSPI& tft = app::tft();
    static char shownText[80] = "", shownTitle[24] = "";
    static char title[24], detail[48];
    static uint32_t textAt = 0, shownAt = 0, shownKey = 0;   // shownAt: the screen appeared
    static linkstatus::Kind kind = linkstatus::Kind::Ok, shownKind = linkstatus::Kind::Ok;
    static int shownSound = -2;
    static uint32_t shownQrGen = 0;   // the phone link's QR on screen (s_qrGen)
    char text[80];   // title|detail: 24 + 1 + 48
    if (mode == WAITING && soundTapped()) {   // the speaker button: mute or unmute, shown at once
        greeting::setTunesMuted(!greeting::tunesMuted());
        textAt = 0;
    }
    const int sound = mode == WAITING ? (greeting::tunesMuted() ? 0 : 1) : -1;   // the button
    // Re-read the cause at most once a second: net::ssid() allocates a String.
    if (fresh || millis() - textAt >= 1000) {
        textAt = millis();
        strlcpy(title, "Waiting for a song...", sizeof title);
        detail[0] = '\0';
        kind = mode == OFFLINE ? offlineText(st, title, sizeof title, detail, sizeof detail)
                               : linkstatus::Kind::Ok;
        if (sound == 0) strlcpy(detail, "Tunes muted", sizeof detail);
        else if (sound == 1) {                       // the tune playing, once it has started
            greeting::tuneName(detail, sizeof detail);
        }
    }
    statusscene::Look look = statusscene::lookFor(kind);
    look.night = theme::isNightActive();   // Sonic's sunset turns to night with the palette
    const qrcard::Qr* qr = mode == WAITING ? phoneQr(fresh) : nullptr;   // the card, top left
    snprintf(text, sizeof text, "%s|%s", title, detail);
    if (fresh || kind != shownKind) {   // new screen: everything, once
        shownAt = millis();
        shownKey = statusscene::frameKey(0, look);
        shownQrGen = s_qrGen;
        ui::drawOffline(tft, title, detail, look, 0, sound, qr);
    } else if (strcmp(text, shownText) != 0 || sound != shownSound) {   // new text (countdown, tune)
        const char* shownDetail = strchr(shownText, '|');   // same two-line layout as before?
        if (sound == shownSound && strcmp(title, shownTitle) == 0 && detail[0] && shownDetail &&
            shownDetail[1])
            ui::drawOfflineDetail(tft, detail, sound);
        else ui::drawOfflineText(tft, title, detail, sound);   // the button changed too
    } else if (statusscene::frameKey(millis() - shownAt, look) != shownKey ||
               s_qrGen != shownQrGen) {   // next frame, or the link's QR changed
        const uint32_t t = millis() - shownAt;   // Pokemon bob, bubble, ball; Sonic waiting
        if (s_qrGen != shownQrGen) {   // the card once more, or the scene back where it was
            shownQrGen = s_qrGen;
            ui::drawQrCard(tft, look, t, qr);
        }
        if (statusscene::frameKey(t, look) != shownKey) {   // only what moves (frameRect)
            shownKey = statusscene::frameKey(t, look);
            ui::drawOfflineScene(tft, look, t, sound < 0, false, qr);   // music box: no arrow
            if (sound == 1 && detail[0]) ui::drawTuneNote(tft, (int)(t / 500));   // bobs with the tune
        }
    }
    if (fresh || strcmp(title, shownTitle) != 0)   // log the screen, not each tick
        Serial.printf("[screen] %s / %s\n", title, detail);
    if (fresh || strcmp(text, shownText) != 0 || sound != shownSound) {
        remote::setStatus(title, detail);
        remote::setTunes(sound < 0 ? "" : sound ? "on" : "muted");
    }
    static bool shownPressed = false;   // the key darkens while a finger is on it
    const bool pressed = sound >= 0 && s_soundPress.pressed() == 1;
    if (pressed != shownPressed && sound == shownSound) ui::drawSoundButton(tft, sound, pressed);
    shownPressed = pressed;
    shownSound = sound;
    strlcpy(shownText, text, sizeof shownText);
    strlcpy(shownTitle, title, sizeof shownTitle);
    shownKind = kind;
}

}
