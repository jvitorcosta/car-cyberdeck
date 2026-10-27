#include "musicsvc.h"
#include <Arduino.h>
#include <cstring>
#include <lwip/sockets.h>
#include "../core/mem.h"
#include "../net/clock.h"
#include "../util/musicwire.h"
#include "../util/netplan.h"
#include "../util/songmood.h"

namespace musicsvc {

namespace {
constexpr uint32_t TICK_MS = 100;
constexpr uint32_t PAL_CHECK_MS = 1000;     // the palette is rebuilt (genre, time of day) this often
constexpr uint32_t PAL_RESEND_MS = 10000;   // and sent this often even when unchanged
constexpr uint32_t RETRY_MS = 2000;         // no socket
constexpr uint32_t STACK = 4096;
// The task's stack and a socket, with room: Spotify's TLS keeps its margin.
constexpr unsigned START_NEED = netplan::TLS_NEED + 6000;

// Control, shared with the routes (remote task) under s_mux.
portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
bool s_running = false;                     // from keepAlive()'s start until the task deletes itself
bool s_stopReq = false;
bool s_closing = false;                     // the task decided to stop: no more keep-alives for it
uint32_t s_keepMs = 0;
uint32_t s_peer = 0;                        // IPv4, network order (IPAddress's dword); 0 between runs

// The song's colour, decided by the network task (util/songmood).
uint32_t s_gen = 0;
uint32_t s_trackMs = 0;                     // millis() of the track change
bool s_coverIn = false;                     // the choice saw its cover (whatever colours it gave)
songmood::Choice s_choice{};

// The readout, written by the task.
struct Debug {
    bool udp;
    songpalette::Palette pal;               // the last one sent
};
Debug s_dbg{};

int openSocket() {
    const int s = lwip_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s < 0) Serial.println("[music] no UDP socket");
    return s;
}

void sendTo(int sock, uint32_t peer, const uint8_t* p, size_t n) {
    if (sock < 0 || !peer || !n) return;
    sockaddr_in to{};
    to.sin_family = AF_INET;
    to.sin_port = htons(musicwire::PORT);
    to.sin_addr.s_addr = peer;
    lwip_sendto(sock, p, n, MSG_DONTWAIT, (const sockaddr*)&to, sizeof to);   // a lost packet is fine
}

// The song's palette now; *settled false while a new song still waits for its cover.
songpalette::Palette buildPalette(uint32_t now, uint32_t* gen, bool* settled) {
    songmood::Choice choice;
    portENTER_CRITICAL(&s_mux);
    *gen = s_gen;
    *settled = s_coverIn || now - s_trackMs >= COVER_WAIT_MS;
    choice = s_choice;
    portEXIT_CRITICAL(&s_mux);
    int minute = -1;
    if (!netclock::minuteOfDay(&minute)) minute = -1;
    return songmood::lights(choice, minute);   // the song's swatch, the hour's cap
}

bool samePalette(const songpalette::Palette& a, const songpalette::Palette& b) {
    for (int i = 0; i < 3; ++i)
        if (a.c[i].r != b.c[i].r || a.c[i].g != b.c[i].g || a.c[i].b != b.c[i].b) return false;
    return a.cap == b.cap && a.depth == b.depth;
}

void task(void*) {
    int sock = -1;
    uint32_t sockRetryAt = 0, lastPalCheck = 0, lastPalSend = 0;
    uint32_t sentGen = 0;
    songpalette::Palette sentPal{};
    bool palSent = false;
    Serial.println("[music] on");
    for (;;) {
        const uint32_t now = millis();
        uint32_t peer;
        portENTER_CRITICAL(&s_mux);
        const bool quit = s_stopReq || (int32_t)(now - s_keepMs) > (int32_t)TIMEOUT_MS;
        if (quit) s_closing = true;
        peer = s_peer;
        portEXIT_CRITICAL(&s_mux);
        if (quit) break;

        if (sock < 0 && (int32_t)(now - sockRetryAt) >= 0) {
            sock = openSocket();
            if (sock < 0) sockRetryAt = now + RETRY_MS;
            portENTER_CRITICAL(&s_mux);
            s_dbg.udp = sock >= 0;
            portEXIT_CRITICAL(&s_mux);
        }

        if (now - lastPalCheck >= PAL_CHECK_MS) {
            lastPalCheck = now;
            uint32_t gen;
            bool settled;
            songpalette::Palette p = buildPalette(now, &gen, &settled);
            if (!settled && palSent) {   // a new song's cover is on its way: the last colours meanwhile
                gen = sentGen;
                p = sentPal;
            }
            const bool changed = !palSent || gen != sentGen || !samePalette(p, sentPal);
            // Nothing before the first settled palette: the yellow deck's lights change once per song.
            if ((settled || palSent) && (changed || now - lastPalSend >= PAL_RESEND_MS)) {
                uint8_t pkt[musicwire::PAL_LEN];
                sendTo(sock, peer, pkt, musicwire::encode(musicwire::PalPkt{gen, p}, pkt, sizeof pkt));
                if (changed)
                    Serial.printf("[music] song %u: %u,%u,%u cap %u\n", (unsigned)gen, p.c[0].r, p.c[0].g,
                                  p.c[0].b, p.cap);
                palSent = true;
                sentGen = gen;
                sentPal = p;
                lastPalSend = now;
                portENTER_CRITICAL(&s_mux);
                s_dbg.pal = p;
                portEXIT_CRITICAL(&s_mux);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(TICK_MS));
    }
    if (sock >= 0) lwip_close(sock);
    Serial.printf("[music] off (stack free %u)\n", (unsigned)uxTaskGetStackHighWaterMark(nullptr));
    portENTER_CRITICAL(&s_mux);
    s_running = false;
    s_closing = false;
    s_peer = 0;                                // the next run streams to whoever starts it
    s_dbg.udp = false;                         // the palette stays: still the song's
    portEXIT_CRITICAL(&s_mux);
    vTaskDelete(nullptr);
}
}  // namespace

Start keepAlive(const IPAddress& peer, bool takePeer) {
    const uint32_t ip = (uint32_t)peer;
    portENTER_CRITICAL(&s_mux);
    if (s_running) {
        const bool closing = s_closing;
        if (!closing) {
            s_keepMs = millis();
            if (takePeer || !s_peer) s_peer = ip;
            s_stopReq = false;                 // an on=1 before the task saw an on=0 cancels it: latest wins
        }
        portEXIT_CRITICAL(&s_mux);
        return closing ? Start::Stopping : Start::On;
    }
    portEXIT_CRITICAL(&s_mux);
    if (mem::byteLargest() < START_NEED) return Start::LowMemory;
    portENTER_CRITICAL(&s_mux);                // claimed before the task exists: it clears s_running as it ends
    if (s_running) {                           // another caller won the race (the routes are one task today)
        portEXIT_CRITICAL(&s_mux);
        return Start::On;
    }
    s_running = true;
    s_keepMs = millis();
    if (takePeer || !s_peer) s_peer = ip;
    s_stopReq = false;
    s_closing = false;
    portEXIT_CRITICAL(&s_mux);
    // Core 1 beside the UI, priority 1: it sleeps TICK_MS between looks.
    if (xTaskCreatePinnedToCore(task, "music", STACK, nullptr, 1, nullptr, 1) != pdPASS) {
        portENTER_CRITICAL(&s_mux);
        s_running = false;
        portEXIT_CRITICAL(&s_mux);
        Serial.println("[music] no memory for its task");
        return Start::NoTask;
    }
    return Start::On;
}

void stop() {
    portENTER_CRITICAL(&s_mux);
    s_stopReq = true;
    portEXIT_CRITICAL(&s_mux);
}

bool on() {
    portENTER_CRITICAL(&s_mux);
    const bool r = s_running;
    portEXIT_CRITICAL(&s_mux);
    return r;
}

void onTrack(uint32_t gen) {
    const uint32_t now = millis();
    portENTER_CRITICAL(&s_mux);
    s_gen = gen;
    s_trackMs = now;
    s_coverIn = false;
    portEXIT_CRITICAL(&s_mux);
}

void onChoice(uint32_t gen, const songmood::Choice& c, bool coverIn) {
    portENTER_CRITICAL(&s_mux);
    if (gen == s_gen) {
        s_choice = c;
        s_coverIn = s_coverIn || coverIn;
    }
    portEXIT_CRITICAL(&s_mux);
}

bool debugJson(char* out, size_t n) {
    Debug d;
    portENTER_CRITICAL(&s_mux);
    const bool running = s_running && !s_closing;
    d = s_dbg;
    portEXIT_CRITICAL(&s_mux);
    const songpalette::Rgb* c = d.pal.c;
    const int w = snprintf(out, n,
        "{\"on\":%d,\"palette\":[[%u,%u,%u],[%u,%u,%u],[%u,%u,%u]],\"cap\":%u,\"udp\":%d,\"largest\":%u}",
        running ? 1 : 0, c[0].r, c[0].g, c[0].b, c[1].r, c[1].g, c[1].b, c[2].r, c[2].g, c[2].b, d.pal.cap,
        d.udp ? 1 : 0, (unsigned)mem::byteLargest());
    return w > 0 && (size_t)w < n;
}

}
