// The Suki drive screen on the PC: renders util/car/sukiride's compose() for a few drive states and
// every termpal theme, day and night, from the real data/sukiride.bin, into PPM files (320 x 240, as
// the deck draws them). tools/suki_ride_preview.py builds and runs it and makes one PNG sheet.
//   g++ -std=gnu++17 -O2 tools/suki_ride_preview.cpp src/util/car/sukiride.cpp src/util/car/drivedash.cpp
//       src/util/common/termpal.cpp -o preview && preview data/sukiride.bin outdir
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include "../src/util/car/sukiride.h"

using namespace sukiride;

static obdwire::Live drive(float kmh, float rpm, float cool, float volts, float pedal, bool live) {
    obdwire::Live l;
    l.link = live ? obdplan::Link::Live : obdplan::Link::Connecting;
    l.kmh = kmh;
    l.rpm = rpm;
    l.coolC = cool;
    l.volts = volts;
    l.throttle = pedal;
    l.load = 40;
    l.intakeC = 31;
    l.kml = kmh > 3 ? 15.8f : NAN;
    l.tripKm = 12.4f;
    return l;
}

static void write(const std::string& path, const std::vector<uint16_t>& px) {
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) return;
    fprintf(f, "P6\n%d %d\n255\n", W, H);
    for (uint16_t be : px) {
        const uint16_t c = (uint16_t)((be >> 8) | (be << 8));
        const uint8_t rgb[3] = {(uint8_t)(((c >> 11) & 31) * 255 / 31), (uint8_t)(((c >> 5) & 63) * 255 / 63),
                                (uint8_t)((c & 31) * 255 / 31)};
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
}

int main(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: preview sukiride.bin outdir\n");
        return 2;
    }
    FILE* f = fopen(argv[1], "rb");
    if (!f) return 2;
    std::vector<uint8_t> blob;
    uint8_t b[65536];
    size_t n;
    while ((n = fread(b, 1, sizeof b, f)) > 0) blob.insert(blob.end(), b, b + n);
    fclose(f);
    Art art;
    if (!parse(blob.data(), blob.size(), &art)) {
        fprintf(stderr, "not an SKR1 file\n");
        return 1;
    }
    struct State { const char* name; obdwire::Live l; bool coolBad, battBad; uint32_t ms; };
    const State states[] = {
        {"cruise", drive(62, 2100, 88, 14.1f, 24, true), false, false, 20000},
        {"spirited", drive(124, 6800, 92, 14.2f, 91, true), false, false, 21130},
        {"overheat", drive(48, 1900, 107, 14.0f, 18, true), true, false, 22400},
        {"lowbatt", drive(0, 820, 86, 11.7f, 0, true), false, true, 23000},
        {"linklost", drive(62, 2100, 88, 14.1f, 24, false), false, false, 24000},
    };
    static const char* THEMES[9] = {"red", "orange", "pink", "purple", "blue", "green", "white", "yellow", "cyan"};
    std::vector<uint16_t> px(W * H), buf(MAX_PIXELS);
    Look look;
    auto render = [&](const State& st, int theme, bool night, const std::string& name) {
        makeLook(art, theme, night, &look);
        Trace t;
        for (int i = 0; i < TRACE_N; ++i)   // a trace that rose to the state's rpm
            t.update(st.l.link == obdplan::Link::Live ? st.l.rpm * (0.55f + 0.45f * i / (TRACE_N - 1)) +
                                                            300.0f * sinf(i * 1.7f)
                                                      : NAN,
                     st.ms - (TRACE_N - 1 - i) * Trace::SAMPLE_MS);
        Scene s;
        scene(st.l, t, st.coolBad, st.battBad, st.ms, &s);
        s.demo = name.find("demo") != std::string::npos;
        for (int y = 0; y < H; y += STRIP_H) {   // as the view's whole redraw does
            const int h = y + STRIP_H > H ? H - y : STRIP_H;
            compose(art, look, s, Rect{0, (int16_t)y, W, (int16_t)h}, buf.data());
            for (int r = 0; r < h; ++r) memcpy(&px[(y + r) * W], &buf[r * W], W * 2);
        }
        // a region redraw must match the whole one: check every region on top
        for (int r = 0; r < REGIONS; ++r) {
            const Rect g = region((Region)r);
            compose(art, look, s, g, buf.data());
            for (int yy = 0; yy < g.h; ++yy)
                if (memcmp(&px[(g.y + yy) * W + g.x], &buf[yy * g.w], g.w * 2) != 0) {
                    fprintf(stderr, "region %d differs from the whole screen at row %d\n", r, g.y + yy);
                    return false;
                }
        }
        write(std::string(argv[2]) + "/" + name + ".ppm", px);
        return true;
    };
    for (const State& st : states)
        if (!render(st, 5, false, std::string("green-day-") + st.name)) return 1;
    for (int th = 0; th < 9; ++th)
        for (int night = 0; night < 2; ++night)
            if (!render(states[0], th, night, std::string(THEMES[th]) + (night ? "-night" : "-day") + "-cruise"))
                return 1;
    if (!render(states[2], 0, false, "red-day-overheat")) return 1;
    if (!render(states[2], 5, true, "green-night-overheat")) return 1;
    if (!render(states[1], 5, false, "green-day-demo")) return 1;
    printf("preview: written to %s\n", argv[2]);
    return 0;
}
