#include "phone.h"
#include <Arduino.h>
#include <WebServer.h>
#include "../net/remote_config.h"
#include "../net/wifi.h"
#include "../util/carstatus.h"
#include "../util/phonepress.h"
#include "../util/remotegate.h"

namespace phone {
static WebServer* s_server = nullptr;
static remotegate::Gate s_gate(REMOTE_TOKEN);
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
static int s_pending = menunav::None;
static int s_pin = -1;                                  // a driveview::Pin, -1 = none waiting
static SemaphoreHandle_t s_statusLock = nullptr;
static char s_status[carstatus::MAX_JSON] = "{}";

static const char PAGE[] PROGMEM = R"HTML(<!doctype html><html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,user-scalable=no">
<meta name="apple-mobile-web-app-capable" content="yes"><title>Zero Two</title>
<style>body{margin:0;background:#1c1424;color:#fff;font:600 20px system-ui;padding:16px;
-webkit-user-select:none;user-select:none}h1{font-size:18px;color:#f497b8;margin:0 0 12px}
.g{display:grid;grid-template-columns:1fr 1fr;gap:12px;margin-bottom:16px}.v{grid-template-columns:1fr 1fr 1fr}
button{font:inherit;padding:22px 0;border:0;border-radius:14px;background:#f8f0e8;color:#2a1a2a}
button:active,button.on{background:#f497b8;color:#fff}#reset{background:#ffd9a8;position:relative;overflow:hidden}
#fill{position:absolute;left:0;top:0;bottom:0;width:0;background:#f497b8;opacity:.6}
#st{background:#2a1f33;border-radius:14px;padding:12px;font-size:16px;line-height:1.5;margin-bottom:16px}
#line{color:#ccc;font-weight:400}#msg{min-height:24px;font-size:16px;color:#ccc}</style></head><body>
<h1>Darling's menu</h1>
<div id="st"><div id="car">...</div><div id="scr"></div><div id="line"></div></div>
<div class="g v"><button data-v="drive">DRIVE</button><button data-v="lyrics">LYRICS</button>
<button data-v="auto">AUTO</button></div>
<div class="g"><button data-b="lyrics">LYRICS</button><button data-b="sounds">SOUNDS</button>
<button data-b="trip">TRIP</button><button data-b="back">BACK</button></div>
<div class="g"><button data-b="howl">HOWL</button><button data-b="vroom">VROOM</button></div>
<div class="g"><button id="reset">HOLD TO RESET TRIP<span id="fill"></span></button></div>
<div id="msg"></div>
<script>const k=new URLSearchParams(location.search).get('k')||'';const $=i=>document.getElementById(i);
const msg=t=>{$('msg').textContent=t};const q='&k='+encodeURIComponent(k);
async function post(u){try{const r=await fetch(u+q,{method:'POST'});
msg(r.status==204?'':r.status==403?'Wrong key':'Error '+r.status);status()}catch(e){msg('Old deck not reachable')}}
const CAR={live:'Car: live',noengine:'Car: engine off',connecting:'Car: connecting...',off:'Car: not connected'};
const SHOW={drive:'Screen: gauges',lyrics:'Screen: lyrics',menu:'Screen: her menu'};
async function status(){try{const r=await fetch('/status?'+q.slice(1));if(!r.ok)return;const s=await r.json();
$('car').textContent=(CAR[s.car]||s.car)+(s.car=='live'&&s.kmh>=0?', '+s.kmh+' km/h':'');
$('scr').textContent=(SHOW[s.show]||s.show)+' ('+s.pin+')';$('line').textContent=s.line;
document.querySelectorAll('button[data-v]').forEach(e=>e.classList.toggle('on',e.dataset.v==s.pin))}catch(e){}}
document.querySelectorAll('button[data-b]').forEach(e=>e.onclick=()=>post('/press?b='+e.dataset.b));
document.querySelectorAll('button[data-v]').forEach(e=>e.onclick=()=>post('/view?v='+e.dataset.v));
const R=$('reset'),F=$('fill');let t=null,s=0;
function stop(){clearInterval(t);t=null;F.style.width='0'}
R.onpointerdown=e=>{e.preventDefault();s=Date.now();t=setInterval(()=>{const p=(Date.now()-s)/1000;
F.style.width=Math.min(100,p*100)+'%';if(p>=1){stop();post('/press?b=reset')}},30)};
R.onpointerup=R.onpointerleave=R.onpointercancel=stop;status();setInterval(status,2000);</script></body></html>)HTML";

menunav::Button take() {
    portENTER_CRITICAL(&s_mux);
    const int b = s_pending;
    s_pending = menunav::None;
    portEXIT_CRITICAL(&s_mux);
    return (menunav::Button)b;
}

bool takePin(driveview::Pin* out) {
    portENTER_CRITICAL(&s_mux);
    const int p = s_pin;
    s_pin = -1;
    portEXIT_CRITICAL(&s_mux);
    if (p < 0) return false;
    *out = (driveview::Pin)p;
    return true;
}

void setStatus(const char* json) {
    if (!s_statusLock) return;
    xSemaphoreTake(s_statusLock, portMAX_DELAY);
    strlcpy(s_status, json, sizeof s_status);
    xSemaphoreGive(s_statusLock);
}

static bool keyOk() {
    if (s_gate.keyOk(s_server->arg("k").c_str())) return true;
    s_server->send(403, "text/plain", "wrong key");
    return false;
}

static void handlePress() {
    if (!keyOk()) return;
    const menunav::Button b = phonepress::parse(s_server->arg("b").c_str());
    if (b == menunav::None) { s_server->send(400, "text/plain", "unknown button"); return; }
    portENTER_CRITICAL(&s_mux);
    s_pending = b;
    portEXIT_CRITICAL(&s_mux);
    s_server->send(204);
}

static void handleView() {
    if (!keyOk()) return;
    driveview::Pin p;
    if (!driveview::parse(s_server->arg("v").c_str(), &p)) { s_server->send(400, "text/plain", "unknown view"); return; }
    portENTER_CRITICAL(&s_mux);
    s_pin = (int)p;
    portEXIT_CRITICAL(&s_mux);
    s_server->send(204);
}

static void handleStatus() {
    if (!keyOk()) return;
    static char copy[carstatus::MAX_JSON];   // static: off the server task's small stack
    xSemaphoreTake(s_statusLock, portMAX_DELAY);
    strlcpy(copy, s_status, sizeof copy);
    xSemaphoreGive(s_statusLock);
    s_server->send(200, "application/json", copy);
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
    s_statusLock = xSemaphoreCreateMutex();
    s_server = new WebServer(80);
    s_server->on("/", HTTP_GET, [] { s_server->send_P(200, "text/html", PAGE); });
    s_server->on("/press", HTTP_POST, handlePress);
    s_server->on("/view", HTTP_POST, handleView);
    s_server->on("/status", HTTP_GET, handleStatus);
    xTaskCreatePinnedToCore(task, "phone", 4096, nullptr, 0, nullptr, 1);
}
}
