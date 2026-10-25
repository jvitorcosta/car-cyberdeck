#include "musicsvc.h"
#include <Arduino.h>
#include <cmath>
#include <cstring>
#include <lwip/sockets.h>
#include "greeting.h"
#include "../board/s3/mic_es8311.h"
#include "../core/mem.h"
#include "../core/shared.h"
#include "../images/art.h"
#include "../net/clock.h"
#include "../util/musicfeat.h"
#include "../util/musicwire.h"
#include "../util/netplan.h"
#include "../util/songpalette.h"

namespace musicsvc {

namespace {
constexpr uint32_t FEAT_MS = 30;            // every second 16 ms read: ~31 FeatPkts a second
constexpr uint32_t PAL_CHECK_MS = 1000;     // the palette is rebuilt (genre, time of day) this often
constexpr uint32_t PAL_RESEND_MS = 10000;   // and sent this often even when unchanged
constexpr uint32_t LOG_MS = 2000;
constexpr uint32_t RETRY_MS = 2000;         // mic::begin() refused (a 48 kHz clip holds the port), or no socket
// Memory: the mic gives back its I2S port (~8 KB of DMA) below KEEP_NEED in the largest block (the
// music box's shed, main.cpp) and takes it again only from START_NEED (keepAlive's and a howl's margin).
constexpr unsigned KEEP_NEED = netplan::TLS_NEED + 4000, START_NEED = netplan::TLS_NEED + 12000;
constexpr int BPM_SECONDS = 15;             // beats/min = the beats of the last 15 s x 4
constexpr uint32_t STACK = 4096;

// Control, shared with the routes (remote task) under s_mux.
portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
bool s_running = false;                     // from keepAlive()'s start until the task deletes itself
bool s_stopReq = false;
bool s_closing = false;                     // the task decided to stop: no more keep-alives for it
uint32_t s_keepMs = 0;
uint32_t s_peer = 0;                        // IPv4, network order (IPAddress's dword)

// The song's cover colours, from the network task.
uint32_t s_gen = 0;
songpalette::Rgb s_cover[3];
int s_coverCount = 0;

// The readout, written by the capture task.
struct Debug {
    uint16_t floor[3];                      // band floors as RMS
    uint8_t energy, bright;
    bool present, mic, udp;
    bool fed;                               // featMs is set: features since the last reset
    uint32_t featMs;                        // millis() of the last feature update
    uint16_t beatsPerMin;
    songpalette::Palette pal;
};
Debug s_dbg{};

// Clears the readout except the palette (still the song's): a run's end or a fresh mic start. Caller holds s_mux.
void resetDebugLocked() {
    const songpalette::Palette p = s_dbg.pal;
    s_dbg = Debug{};
    s_dbg.pal = p;
}

// Capture task only.
int16_t s_buf[256];
musicfeat::Analyzer s_an;

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

songpalette::Palette buildPalette(uint32_t* gen) {
    songpalette::Rgb cover[3];
    int count;
    portENTER_CRITICAL(&s_mux);
    *gen = s_gen;
    count = s_coverCount;
    memcpy(cover, s_cover, sizeof cover);
    portEXIT_CRITICAL(&s_mux);
    int minute = -1;
    if (!netclock::minuteOfDay(&minute)) minute = -1;
    return songpalette::build(cover, count, shared::genreNow(*gen), minute);
}

bool samePalette(const songpalette::Palette& a, const songpalette::Palette& b) {
    for (int i = 0; i < 3; ++i)
        if (a.c[i].r != b.c[i].r || a.c[i].g != b.c[i].g || a.c[i].b != b.c[i].b) return false;
    return a.cap == b.cap && a.depth == b.depth;
}

void task(void*) {
    int sock = -1;
    bool micOn = false;
    uint32_t micRetryAt = 0, sockRetryAt = 0, lastFeat = 0, lastPalCheck = 0, lastPalSend = 0, lastLog = 0;
    uint64_t samples = 0;
    uint16_t seq = 0;
    uint32_t sentGen = 0;
    songpalette::Palette sentPal{};
    bool palSent = false;
    uint8_t beats[BPM_SECONDS] = {};        // beats per second, a ring over the last BPM_SECONDS
    uint32_t beatSecond = millis() / 1000;
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

        const unsigned largest = (unsigned)mem::byteLargest();
        if (micOn && largest < KEEP_NEED) {   // Spotify's TLS first: give the port back, retry later
            mic::end();
            micOn = false;
            micRetryAt = now + RETRY_MS;
            Serial.printf("[music] low memory (largest %u): mic off until %u\n", largest, START_NEED);
            portENTER_CRITICAL(&s_mux);
            s_dbg.mic = false;
            portEXIT_CRITICAL(&s_mux);
        }
        if (!micOn) {
            if ((int32_t)(now - micRetryAt) >= 0) {
                micRetryAt = now + RETRY_MS;
                if (largest >= START_NEED) {
                    micOn = mic::begin();
                    if (!micOn) Serial.println("[music] mic busy: retry in 2 s");
                }
                if (micOn) {
                    s_an = musicfeat::Analyzer();
                    samples = 0;
                }
                portENTER_CRITICAL(&s_mux);
                if (micOn) resetDebugLocked();   // no stale features from before the gap
                s_dbg.mic = micOn;
                s_dbg.udp = sock >= 0;
                portEXIT_CRITICAL(&s_mux);
            }
            if (!micOn) { vTaskDelay(pdMS_TO_TICKS(100)); continue; }
        }

        const size_t n = mic::read(s_buf, sizeof s_buf / sizeof s_buf[0]);   // ~16 ms
        samples += n;
        s_an.feed(s_buf, n, (uint32_t)(samples * 1000 / musicfeat::RATE));

        if (now - lastFeat >= FEAT_MS) {
            lastFeat = now;
            const musicfeat::Features f = s_an.take();
            uint8_t pkt[musicwire::FEAT_LEN];
            sendTo(sock, peer, pkt, musicwire::encode(musicwire::FeatPkt{seq++, f}, pkt, sizeof pkt));
            const uint32_t sec = now / 1000;
            if (sec - beatSecond >= (uint32_t)BPM_SECONDS) {   // the mic was out for a while
                memset(beats, 0, sizeof beats);
                beatSecond = sec;
            }
            while (beatSecond != sec) beats[++beatSecond % BPM_SECONDS] = 0;   // the seconds since the last
            if (f.beat && beats[sec % BPM_SECONDS] < 255) ++beats[sec % BPM_SECONDS];
            unsigned total = 0;
            for (int i = 0; i < BPM_SECONDS; ++i) total += beats[i];
            portENTER_CRITICAL(&s_mux);
            for (int k = 0; k < 3; ++k) s_dbg.floor[k] = (uint16_t)sqrtf(s_an.floorPower(k));
            s_dbg.energy = f.energy;
            s_dbg.bright = f.brightness;
            s_dbg.present = f.present;
            s_dbg.beatsPerMin = (uint16_t)(total * 60 / BPM_SECONDS);
            s_dbg.fed = true;
            s_dbg.featMs = now;
            portEXIT_CRITICAL(&s_mux);
        }

        if (now - lastPalCheck >= PAL_CHECK_MS || !palSent) {
            lastPalCheck = now;
            uint32_t gen;
            const songpalette::Palette p = buildPalette(&gen);
            if (!palSent || gen != sentGen || !samePalette(p, sentPal) || now - lastPalSend >= PAL_RESEND_MS) {
                uint8_t pkt[musicwire::PAL_LEN];
                sendTo(sock, peer, pkt, musicwire::encode(musicwire::PalPkt{gen, p}, pkt, sizeof pkt));
                palSent = true;
                sentGen = gen;
                sentPal = p;
                lastPalSend = now;
                portENTER_CRITICAL(&s_mux);
                s_dbg.pal = p;
                portEXIT_CRITICAL(&s_mux);
            }
        }

        if (now - lastLog >= LOG_MS) {
            lastLog = now;
            Debug d;
            portENTER_CRITICAL(&s_mux);
            d = s_dbg;
            portEXIT_CRITICAL(&s_mux);
            Serial.printf("[music] floor %u/%u/%u energy %u beats/min %u present %d largest %u\n", d.floor[0],
                          d.floor[1], d.floor[2], d.energy, d.beatsPerMin, d.present ? 1 : 0, largest);
        }
    }
    mic::end();
    if (sock >= 0) lwip_close(sock);
    Serial.printf("[music] off (stack free %u)\n", (unsigned)uxTaskGetStackHighWaterMark(nullptr));
    portENTER_CRITICAL(&s_mux);
    s_running = false;
    s_closing = false;
    resetDebugLocked();
    portEXIT_CRITICAL(&s_mux);
    vTaskDelete(nullptr);
}
}  // namespace

Start keepAlive(const IPAddress& peer) {
    const uint32_t ip = (uint32_t)peer;
    portENTER_CRITICAL(&s_mux);
    if (s_running) {
        const bool closing = s_closing;
        if (!closing) {
            s_keepMs = millis();
            s_peer = ip;
            s_stopReq = false;                 // an on=1 before the task saw an on=0 cancels it: latest wins
        }
        portEXIT_CRITICAL(&s_mux);
        return closing ? Start::Stopping : Start::On;
    }
    portEXIT_CRITICAL(&s_mux);
    if (mem::byteLargest() < START_NEED) return Start::LowMemory;   // a howl's margin
    greeting::stopTunes();                     // the mic would hear it; main.cpp keeps it off while on
    portENTER_CRITICAL(&s_mux);                // claimed before the task exists: it clears s_running as it ends
    if (s_running) {                           // another caller won the race (the routes are one task today)
        portEXIT_CRITICAL(&s_mux);
        return Start::On;
    }
    s_running = true;
    s_keepMs = millis();
    s_peer = ip;
    s_stopReq = false;
    s_closing = false;
    portEXIT_CRITICAL(&s_mux);
    // Core 1 beside the UI, priority 1: it blocks in mic::read() most of the time.
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
    portENTER_CRITICAL(&s_mux);
    s_gen = gen;
    s_coverCount = 0;
    portEXIT_CRITICAL(&s_mux);
}

void onCover(uint32_t gen) {
    songpalette::Rgb c[3];
    const int count = songpalette::fromCover(art::bitmap(), art::W, art::H, c);   // ~8.5k pixels, this task
    portENTER_CRITICAL(&s_mux);
    if (gen == s_gen) {
        memcpy(s_cover, c, sizeof c);
        s_coverCount = count;
    }
    portEXIT_CRITICAL(&s_mux);
}

bool debugJson(char* out, size_t n) {
    Debug d;
    portENTER_CRITICAL(&s_mux);
    const bool running = s_running && !s_closing;
    d = s_dbg;
    portEXIT_CRITICAL(&s_mux);
    const long ageMs = d.fed ? (long)(millis() - d.featMs) : -1L;
    const songpalette::Rgb* c = d.pal.c;
    const int w = snprintf(out, n,
        "{\"on\":%d,\"floor\":[%u,%u,%u],\"energy\":%u,\"bright\":%u,\"present\":%d,\"beatsPerMin\":%u,"
        "\"palette\":[[%u,%u,%u],[%u,%u,%u],[%u,%u,%u]],\"cap\":%u,\"mic\":%d,\"udp\":%d,\"ageMs\":%ld,"
        "\"largest\":%u}",
        running ? 1 : 0, d.floor[0], d.floor[1], d.floor[2], d.energy, d.bright, d.present ? 1 : 0, d.beatsPerMin,
        c[0].r, c[0].g, c[0].b, c[1].r, c[1].g, c[1].b, c[2].r, c[2].g, c[2].b, d.pal.cap, d.mic ? 1 : 0,
        d.udp ? 1 : 0, ageMs, (unsigned)mem::byteLargest());
    return w > 0 && (size_t)w < n;
}

}
