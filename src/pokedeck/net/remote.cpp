// Copied from black/net/remote.cpp (remote.h says what's left).
#include "remote.h"
#include <Arduino.h>
#include <WebServer.h>
#include "../../black/audio/musicsvc.h"
#include "../../black/spotify/client.h"
#include "../../common/net/remote_config.h"
#include "../../common/net/reqauth.h"
#include "../../common/net/wifi.h"
#include "../../util/black/playlists.h"
#include "../../util/common/decklink.h"
#include "../../util/common/remotegate.h"
#include "../../util/wire/musicwire.h"

namespace remote {
namespace {
WebServer* s_server = nullptr;   // created only when the remote is on
remotegate::Gate s_gate(REMOTE_TOKEN);

// The yellow deck's link check (util/common/decklink): GET /link every 5 s, signed; the answer says
// whether there is a GPS fix: this deck has no GPS, so "gps=0".
void handleLink() {
    if (!reqauth::pass(*s_server, s_gate, "/link", "wrong key")) return;
    char body[8];
    decklink::body(false, body, sizeof body);
    s_server->sendHeader("Cache-Control", "no-store");
    s_server->send(200, "text/plain", body);
}

// Music mode (black/audio/musicsvc): the yellow deck sends on=1 every 10 s while it wants the
// song's palette over UDP, on=0 to stop at once.
void handleMusic() {
    if (!reqauth::pass(*s_server, s_gate, "/music", "wrong key")) return;
    const String on = s_server->arg("on");
    if (on == "0") {
        musicsvc::stop();
        s_server->send(200, "application/json", "{\"on\":0}");
        return;
    }
    if (on != "1") { s_server->send(400, "text/plain", "on=1 or on=0"); return; }
    switch (musicsvc::keepAlive(s_server->client().remoteIP())) {
    case musicsvc::Start::On: {
        // The boot id the packets carry, proven for this request's nonce (util/wire/musicwire): the
        // yellow deck takes packets only from it. A request with the key (no nonce) gets no proof.
        char proof[musicwire::PROOF_HEX + 1];
        char body[80];
        const uint32_t boot = musicsvc::bootId();
        if (musicwire::bootProof(REMOTE_TOKEN, boot, s_server->arg("n").c_str(), proof))
            snprintf(body, sizeof body, "{\"on\":1,\"boot\":%lu,\"proof\":\"%s\"}", static_cast<unsigned long>(boot), proof);
        else
            snprintf(body, sizeof body, "{\"on\":1}");
        s_server->send(200, "application/json", body);
        break;
    }
    case musicsvc::Start::LowMemory: s_server->send(503, "text/plain", "low memory"); break;
    case musicsvc::Start::Stopping:  s_server->send(409, "text/plain", "stopping, ask again"); break;
    case musicsvc::Start::NoTask:    s_server->send(503, "text/plain", "no memory for the task"); break;
    }
}

// The watch's playlist picker (util/black/playlists). Spotify is asked on the network task
// (spotify/client), so these wait for it: a page of the owner's playlists, or one played.
void handlePlaylists() {
    if (!reqauth::pass(*s_server, s_gate, "/playlists", "wrong key")) return;
    int offset = 0;
    if (s_server->hasArg("offset") && !playlists::parseOffset(s_server->arg("offset").c_str(), &offset)) {
        s_server->send(400, "text/plain", "bad offset");
        return;
    }
    static char out[playlists::REPLY_MAX];   // static: off this task's stack
    const int code = spclient::watchPlaylists(offset, out, sizeof out);
    s_server->sendHeader("Cache-Control", "no-store");
    s_server->send(code, "text/plain; charset=utf-8", out);
}

void handlePlaylistPlay() {
    if (!reqauth::pass(*s_server, s_gate, "/playlists/play", "wrong key")) return;
    const String id = s_server->arg("id");
    if (!playlists::validId(id.c_str())) { s_server->send(400, "text/plain", "bad id"); return; }
    char out[48];
    const int code = spclient::watchPlay(id.c_str(), s_server->arg("shuffle") == "1", out, sizeof out);
    s_server->send(code, "text/plain", out);
}

// A slow or half-sent request holds the server for seconds, so it never runs on the UI loop. At
// priority 0 it only gets the CPU the display leaves free.
void task(void*) {
    while (!net::isOnline()) vTaskDelay(pdMS_TO_TICKS(200));
    s_server->begin();
    Serial.printf("[remote] on: http://%s/ (signed requests or ?k=)\n", net::deviceIp().c_str());
    for (;;) {
        s_server->handleClient();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
}  // namespace

void begin() {
    if (!s_gate.enabled()) { Serial.println("[remote] off (REMOTE_TOKEN empty)"); return; }
    reqauth::begin();
    s_server = new WebServer(80);
    s_server->on("/link", HTTP_GET, handleLink);
    s_server->on("/music", HTTP_GET, handleMusic);
    s_server->on("/playlists", HTTP_GET, handlePlaylists);
    s_server->on("/playlists/play", HTTP_GET, handlePlaylistPlay);
    s_server->onNotFound([] { s_server->send(404, "text/plain", "not found"); });
    xTaskCreatePinnedToCore(task, "remote", 6144, nullptr, 0, nullptr, 1);
}
}  // namespace remote
