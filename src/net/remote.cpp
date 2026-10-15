#include "remote.h"
#include <Arduino.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include "../config.h"
#include "wifi.h"
#include "../audio/greeting.h"
#include "../util/remotegate.h"
#include "../util/netplan.h"
#include "../core/mem.h"
#include "../core/shared.h"
#include "../util/statejson.h"
#include "../util/tripclock.h"
#include "../util/interp.h"
#include <WiFi.h>

#ifndef REMOTE_TOKEN
#define REMOTE_TOKEN ""   // older config.h: remote off
#endif
// Name on the local network: http://<REMOTE_NAME>.local/ (mDNS / Bonjour). Different per
// board so both decks can be on at once.
#ifndef REMOTE_NAME
#ifdef BOARD_S3
#define REMOTE_NAME "cardeck"
#else
#define REMOTE_NAME "cardeck-cyd"
#endif
#endif

namespace remote {
static WebServer* s_server = nullptr;   // created only when the remote is on
static bool (*s_hold)() = nullptr;      // true while the boot scene runs (its clips own the audio)
static remotegate::Gate s_gate(REMOTE_TOKEN);

// The phone page. Add it to the Home Screen from Safari (Share -> Add to Home Screen) with the
// ?k= link: it opens full-screen with the icon, and the key stays in the icon's link (Home
// Screen apps get their own storage, so remembering it in the page would be unreliable). It
// reads GET /state every 5 s (now playing, the deck's status, network, time on) and builds a
// button per sound the deck offers.
static const char PAGE[] = R"HTML(<!doctype html><html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="apple-mobile-web-app-capable" content="yes"><meta name="mobile-web-app-capable" content="yes">
<meta name="apple-mobile-web-app-status-bar-style" content="black-translucent">
<meta name="apple-mobile-web-app-title" content="CarDeck"><meta name="theme-color" content="#101820">
<link rel="apple-touch-icon" href="/icon.png"><title>CarDeck</title>
<style>*{box-sizing:border-box}body{margin:0;min-height:100vh;display:flex;flex-direction:column;
gap:18px;padding:max(24px,env(safe-area-inset-top)) 20px max(24px,env(safe-area-inset-bottom));
background:#101820;color:#f8f8d8;font:17px system-ui,sans-serif}
.now{background:#1c2834;border-radius:18px;padding:18px}.now small{opacity:.6;letter-spacing:.08em}
#t{font-size:22px;font-weight:700;margin-top:6px;overflow-wrap:anywhere}#a{opacity:.8;margin-top:2px}
.st{background:#3a2a10;color:#f8d890;border-radius:14px;padding:12px 16px}
#btns{display:grid;gap:14px}button{font:700 26px system-ui,sans-serif;padding:24px;border:0;
border-radius:18px;background:#e8a020;color:#101820}button:active{transform:scale(.97)}
#s{min-height:1.4em;text-align:center;margin:0}footer{margin-top:auto;opacity:.5;font-size:14px;text-align:center}
</style></head><body>
<div class="now"><small>NOW PLAYING</small><div id="t">...</div><div id="a"></div></div>
<div class="st" id="st" hidden></div><div id="btns"></div><p id="s"></p><footer id="f"></footer>
<script>
const k=new URLSearchParams(location.search).get('k')||'',$=i=>document.getElementById(i);
const NAMES={howl:'\u{1F43A} Howl'};
const say={202:'Playing!',403:'Wrong key',409:'Busy: a sound is playing',429:'Wait a moment',503:'Deck is low on memory, try again'};
let shown='';
async function play(c){$('s').textContent='...';try{
const r=await fetch('/play?clip='+c+'&k='+encodeURIComponent(k),{method:'POST'});
$('s').textContent=say[r.status]||('Error '+r.status)}catch(e){$('s').textContent='Deck not reachable'}}
function dur(s){const h=Math.floor(s/3600),m=Math.floor(s%3600/60);return h?h+'h '+String(m).padStart(2,'0')+'m':m+' min'}
async function poll(){try{const r=await fetch('/state?k='+encodeURIComponent(k),{cache:'no-store'});
if(r.status==403){$('st').hidden=false;$('st').textContent='Wrong key: open the link that has ?k=';return}
const d=await r.json();$('t').textContent=d.track||'Nothing playing';$('a').textContent=d.artist||'';
$('st').hidden=!d.status;$('st').textContent=d.status+(d.detail?' — '+d.detail:'');
$('f').textContent=d.wifi+' · '+d.rssi+' dBm · on for '+dur(d.uptime);
const c=d.clips.join();if(c!==shown){shown=c;$('btns').innerHTML='';for(const n of d.clips){
const b=document.createElement('button');b.textContent=NAMES[n]||n;b.onclick=()=>play(n);$('btns').appendChild(b)}}
}catch(e){$('st').hidden=false;$('st').textContent='Deck not reachable'}}
poll();setInterval(poll,5000);
</script></body></html>)HTML";

#include "icon.inc"   // ICON_PNG: tools/gen_icon.py

// Status screen text, set by the UI loop (main.cpp), read by this task for /state.
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
static tripclock::Clock s_trip(0);   // power-on = the car started; reset by POST /trip/reset (this task only)
static char s_title[24] = "", s_detail[48] = "";
static const char* const CLIPS[] = {"howl"};

void setStatus(const char* title, const char* detail) {
    portENTER_CRITICAL(&s_mux);
    strlcpy(s_title, title ? title : "", sizeof s_title);
    strlcpy(s_detail, detail ? detail : "", sizeof s_detail);
    portEXIT_CRITICAL(&s_mux);
}

bool link(char* out, size_t n) {
    if (!s_gate.enabled() || !net::isOnline()) return false;
    snprintf(out, n, "http://%s/?k=%s", net::deviceIp().c_str(), REMOTE_TOKEN);
    return true;
}

static void handleState() {
    if (!s_gate.keyOk(s_server->arg("k").c_str())) { s_server->send(403, "text/plain", "wrong key"); return; }
    static AppState st;        // static: ~1 KB, off this task's 4 KB stack (one request at a time)
    static char out[statejson::MAX_JSON];   // 768 B overflowed with a long title and lyric lines
    static shared::LyricPair lp;   // static: two 160-byte lines, off this task's stack
    shared::snapshot(st);
    char title[sizeof s_title], detail[sizeof s_detail];
    portENTER_CRITICAL(&s_mux);
    memcpy(title, s_title, sizeof title);
    memcpy(detail, s_detail, sizeof detail);
    portEXIT_CRITICAL(&s_mux);
    const String ssid = net::ssid();
    const uint32_t now = millis();
    const uint32_t pos = interp::currentProgressMs(st.progressMs, st.durationMs, st.isPlaying,
                                                   now - st.lastPollMs);
    shared::lyricPair(st.trackGen, pos, lp);
    statejson::State s{};
    s.track = st.trackName; s.artist = st.artist; s.title = title; s.detail = detail;
    s.ssid = ssid.c_str(); s.rssi = (int)WiFi.RSSI(); s.uptimeS = now / 1000;
    s.clips = CLIPS; s.clipCount = (int)(sizeof CLIPS / sizeof CLIPS[0]);
    s.trackGen = st.trackGen; s.progressMs = pos; s.durationMs = st.durationMs;
    s.playing = st.isPlaying; s.tripS = s_trip.seconds(now);
    s.lyricKind = statejson::lyricKindName(lp.status);
    s.line = lp.line; s.next = lp.next; s.lineStartMs = lp.lineStartMs; s.nextStartMs = lp.nextStartMs;
    if (!statejson::write(s, out, sizeof out)) { s_server->send(500, "text/plain", "state too big"); return; }
    s_server->sendHeader("Cache-Control", "no-store");
    s_server->send(200, "application/json", out);
}

static void handlePlay() {
    if (s_server->arg("clip") != "howl") { s_server->send(404, "text/plain", "unknown clip"); return; }
    const String key = s_server->arg("k");
    // A howl starts the audio driver and a 3 KB task: never at the cost of Spotify's TLS.
    if (mem::byteLargest() < netplan::TLS_NEED + 12000) {
        s_server->send(503, "text/plain", "low memory");
        return;
    }
    switch (s_gate.request(key.c_str(), greeting::playing(), millis())) {
    case remotegate::Result::Play:
        greeting::playHowl();
        Serial.println("[remote] howl");
        s_server->send(202, "text/plain", "playing");
        break;
    case remotegate::Result::BadKey:  s_server->send(403, "text/plain", "wrong key"); break;
    case remotegate::Result::Busy:    s_server->send(409, "text/plain", "busy"); break;
    case remotegate::Result::TooSoon: s_server->send(429, "text/plain", "too soon"); break;
    }
}

// Own task at idle priority on the UI core: WebServer reads a request with blocking reads (a
// slow or half-sent request holds it for seconds), so it must never run on the UI loop. At
// priority 0 it only gets the CPU the display leaves free.
static void task(void*) {
    while ((s_hold && s_hold()) || !net::isOnline()) vTaskDelay(pdMS_TO_TICKS(200));
    s_server->begin();
    // mDNS follows the WiFi events itself, so the name keeps working after a reconnect or a
    // new hotspot address.
    const bool named = MDNS.begin(REMOTE_NAME);
    if (named) MDNS.addService("http", "tcp", 80);
    Serial.printf("[remote] on: http://%s.local/?k=<REMOTE_TOKEN> (or http://%s/)\n",
                  named ? REMOTE_NAME : "(mDNS failed)", net::deviceIp().c_str());
    for (;;) {
        s_server->handleClient();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void begin(bool (*hold)()) {
    if (!s_gate.enabled()) { Serial.println("[remote] off (REMOTE_TOKEN empty)"); return; }
    s_hold = hold;
    s_server = new WebServer(80);
    s_server->on("/", HTTP_GET, [] {
        Serial.printf("[remote] page for %s\n", s_server->client().remoteIP().toString().c_str());
        s_server->send_P(200, "text/html", PAGE);
    });
    s_server->on("/play", HTTP_POST, handlePlay);
    s_server->on("/state", HTTP_GET, handleState);
    s_server->on("/trip/reset", HTTP_POST, [] {
        if (!s_gate.keyOk(s_server->arg("k").c_str())) { s_server->send(403, "text/plain", "wrong key"); return; }
        s_trip.reset(millis());
        Serial.println("[remote] trip reset");
        s_server->send(204);
    });
    s_server->on("/icon.png", HTTP_GET, [] {   // Home Screen icon: public, like any favicon
        s_server->sendHeader("Cache-Control", "max-age=86400");
        s_server->send_P(200, "image/png", (const char*)ICON_PNG, sizeof ICON_PNG);
    });
    s_server->onNotFound([] { s_server->send(404, "text/plain", "not found"); });
    xTaskCreatePinnedToCore(task, "remote", 4096, nullptr, 0, nullptr, 1);
}
}
