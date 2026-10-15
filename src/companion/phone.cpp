#include "phone.h"
#include <Arduino.h>
#include <WebServer.h>
#include "../config.h"
#include "../net/wifi.h"
#include "../util/phonepress.h"
#include "../util/remotegate.h"

#ifndef REMOTE_TOKEN
#define REMOTE_TOKEN ""
#endif

namespace phone {
static WebServer* s_server = nullptr;
static remotegate::Gate s_gate(REMOTE_TOKEN);
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
static int s_pending = menunav::None;

static const char PAGE[] PROGMEM = R"HTML(<!doctype html><html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,user-scalable=no">
<meta name="apple-mobile-web-app-capable" content="yes"><title>Zero Two</title>
<style>body{margin:0;background:#1c1424;color:#fff;font:600 20px system-ui;padding:16px;
-webkit-user-select:none;user-select:none}h1{font-size:18px;color:#f497b8;margin:0 0 12px}
.g{display:grid;grid-template-columns:1fr 1fr;gap:12px;margin-bottom:16px}
button{font:inherit;padding:22px 0;border:0;border-radius:14px;background:#f8f0e8;color:#2a1a2a}
button:active{background:#f497b8;color:#fff}#reset{background:#ffd9a8;position:relative;overflow:hidden}
#fill{position:absolute;left:0;top:0;bottom:0;width:0;background:#f497b8;opacity:.6}
#msg{min-height:24px;font-size:16px;color:#ccc}</style></head><body>
<h1>Darling's menu</h1>
<div class="g"><button data-b="lyrics">LYRICS</button><button data-b="sounds">SOUNDS</button>
<button data-b="trip">TRIP</button><button data-b="back">BACK</button></div>
<div class="g"><button data-b="howl">HOWL</button><button data-b="vroom">VROOM</button></div>
<div class="g"><button id="reset">HOLD TO RESET TRIP<span id="fill"></span></button></div>
<div id="msg"></div>
<script>const k=new URLSearchParams(location.search).get('k')||'';
const msg=t=>{document.getElementById('msg').textContent=t};
async function press(b){try{const r=await fetch('/press?b='+b+'&k='+encodeURIComponent(k),{method:'POST'});
msg(r.status==204?'':r.status==403?'Wrong key':'Error '+r.status)}catch(e){msg('Old deck not reachable')}}
document.querySelectorAll('button[data-b]').forEach(e=>e.onclick=()=>press(e.dataset.b));
const R=document.getElementById('reset'),F=document.getElementById('fill');let t=null,s=0;
function stop(){clearInterval(t);t=null;F.style.width='0'}
R.onpointerdown=e=>{e.preventDefault();s=Date.now();t=setInterval(()=>{const p=(Date.now()-s)/1000;
F.style.width=Math.min(100,p*100)+'%';if(p>=1){stop();press('reset')}},30)};
R.onpointerup=R.onpointerleave=R.onpointercancel=stop;</script></body></html>)HTML";

menunav::Button take() {
    portENTER_CRITICAL(&s_mux);
    const int b = s_pending;
    s_pending = menunav::None;
    portEXIT_CRITICAL(&s_mux);
    return (menunav::Button)b;
}

static void handlePress() {
    if (!s_gate.keyOk(s_server->arg("k").c_str())) { s_server->send(403, "text/plain", "wrong key"); return; }
    const menunav::Button b = phonepress::parse(s_server->arg("b").c_str());
    if (b == menunav::None) { s_server->send(400, "text/plain", "unknown button"); return; }
    portENTER_CRITICAL(&s_mux);
    s_pending = b;
    portEXIT_CRITICAL(&s_mux);
    s_server->send(204);
}

static void task(void*) {
    while (!net::isOnline()) vTaskDelay(pdMS_TO_TICKS(200));
    s_server->begin();
    Serial.printf("[phone] on: http://%s/?k=<REMOTE_TOKEN>\n", net::deviceIp().c_str());
    for (;;) {
        s_server->handleClient();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void begin() {
    if (!s_gate.enabled()) { Serial.println("[phone] off (REMOTE_TOKEN empty)"); return; }
    s_server = new WebServer(80);
    s_server->on("/", HTTP_GET, [] { s_server->send_P(200, "text/html", PAGE); });
    s_server->on("/press", HTTP_POST, handlePress);
    xTaskCreatePinnedToCore(task, "phone", 4096, nullptr, 0, nullptr, 1);
}
}
