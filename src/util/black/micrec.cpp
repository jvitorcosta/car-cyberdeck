#include "micrec.h"
#include <cstdio>
#include <cstring>

namespace micrec {

static const char* const PHRASE[PHRASES] = {
    "liga as luzes",
    "deixa azul fraquinho dentro",
    "apaga a luz de fora",
    "roxo cinquenta por cento",
    "toca Djavan",
    "pula essa música",
    "volta a música anterior",
    "toca Tyler, the Creator",
    "toca Billie Eilish",
    "aumenta o volume",
};

static const char* const CONDITION[SLOTS / PHRASES] = {
    "Spotify pausado, vidros fechados",
    "Spotify a 20%, vidros fechados",
    "Spotify pausado, vidros abertos",
    "Spotify a 20%, vidros abertos",
};

static bool inRange(int slot) { return slot >= 0 && slot < SLOTS; }

const char* phrase(int p) { return p >= 0 && p < PHRASES ? PHRASE[p] : ""; }

const char* conditionText(int slot) { return inRange(slot) ? CONDITION[slot / PHRASES] : ""; }

char condition(int slot) { return inRange(slot) ? (char)('A' + slot / PHRASES) : '?'; }

int phraseOf(int slot) { return inRange(slot) ? slot % PHRASES : -1; }

static void put32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}
static void put16(uint8_t* p, uint16_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

void wavHeader(uint8_t out[44], uint32_t dataBytes, uint32_t rate) {
    memcpy(out, "RIFF", 4);
    put32(out + 4, 36 + dataBytes);
    memcpy(out + 8, "WAVEfmt ", 8);
    put32(out + 16, 16);         // fmt chunk size
    put16(out + 20, 1);          // PCM
    put16(out + 22, 1);          // mono
    put32(out + 24, rate);
    put32(out + 28, rate * 2);   // byte rate
    put16(out + 32, 2);          // block align
    put16(out + 34, 16);         // bits per sample
    memcpy(out + 36, "data", 4);
    put32(out + 40, dataBytes);
}

int samplesToKeep(uint32_t heldMs) {
    if (heldMs < (uint32_t)MIN_MS) return 0;
    if (heldMs > (uint32_t)MAX_MS) heldMs = MAX_MS;
    return (int)(heldMs * RATE / 1000);
}

void clipName(int slot, char* out, size_t n) {
    if (!inRange(slot)) {
        snprintf(out, n, "?");
        return;
    }
    snprintf(out, n, "%c%02d", condition(slot), slot % PHRASES + 1);
}

}  // namespace micrec
