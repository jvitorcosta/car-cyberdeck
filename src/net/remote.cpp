#include "remote.h"
#include <Arduino.h>
#include <WebServer.h>
#include "../config.h"
#include "wifi.h"
#include "../audio/greeting.h"
#include "../util/remotegate.h"

#ifndef REMOTE_TOKEN
#define REMOTE_TOKEN ""   // older config.h: remote off
#endif

namespace remote {
static WebServer* s_server = nullptr;   // created only when the remote is on
static remotegate::Gate s_gate(REMOTE_TOKEN);

// The whole phone page: one button; the key comes from the page's own ?k= and goes back in
// the POST. Answers are shown under the button.
static const char PAGE[] = R"HTML(<!doctype html><html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>CarDeck</title>
<style>body{margin:0;min-height:100vh;display:flex;flex-direction:column;align-items:center;
justify-content:center;background:#101820;color:#f8f8d8;font:18px system-ui,sans-serif}
button{font-size:28px;padding:28px 44px;border:0;border-radius:18px;background:#e8a020;
color:#101820;font-weight:700}button:active{transform:scale(.96)}p{min-height:1.5em}</style>
</head><body><button id="b">&#x1F43A; Howl</button><p id="s"></p><script>
const k=new URLSearchParams(location.search).get('k')||'',s=document.getElementById('s');
const say={202:'Howling!',403:'Wrong key',409:'Busy: a sound is playing',429:'Wait a moment'};
document.getElementById('b').onclick=async()=>{s.textContent='...';try{
const r=await fetch('/play?clip=howl&k='+encodeURIComponent(k),{method:'POST'});
s.textContent=say[r.status]||('Error '+r.status)}catch(e){s.textContent='Deck not reachable'}};
</script></body></html>)HTML";

static void handlePlay() {
    if (s_server->arg("clip") != "howl") { s_server->send(404, "text/plain", "unknown clip"); return; }
    const String key = s_server->arg("k");
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

void begin() {
    if (!s_gate.enabled()) { Serial.println("[remote] off (REMOTE_TOKEN empty)"); return; }
    s_server = new WebServer(80);
    s_server->on("/", HTTP_GET, [] { s_server->send_P(200, "text/html", PAGE); });
    s_server->on("/play", HTTP_POST, handlePlay);
    s_server->onNotFound([] { s_server->send(404, "text/plain", "not found"); });
    s_server->begin();
    Serial.printf("[remote] on: http://%s/?k=<REMOTE_TOKEN>\n", net::deviceIp().c_str());
}

void loop() {
    if (s_server) s_server->handleClient();
}
}
