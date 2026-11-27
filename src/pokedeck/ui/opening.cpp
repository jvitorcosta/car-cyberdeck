#include "opening.h"
#include <Arduino.h>
#include <Preferences.h>
#include <new>
#include <esp_heap_caps.h>
#include <FS.h>
using fs::File;   // JPEGDEC.h names the global File, which this core's FS.h may not export
#include <JPEGDEC.h>
#include "../audio/speaker.h"
#include "../board/panel.h"
#include "../media/sdcard.h"
#include "../../common/core/watchdog.h"
#include "../../black/ui/montage.h"
#include "../../util/black/introclock.h"
#include "../../util/black/intropick.h"

namespace opening {
namespace {
int s_ox = 0, s_oy = 0;   // where the running video's frames go: centred on the panel
// Percent of the clips' level, as the black deck plays them (its INTRO_VOLUME for the videos).
constexpr int VIDEO_VOLUME = 50, GREETING_VOLUME = 100;

int draw(JPEGDRAW* d) {
    panel::gfx().draw16bitBeRGBBitmap(static_cast<int16_t>(s_ox + d->x), static_cast<int16_t>(s_oy + d->y),
                                      d->pPixels, static_cast<int16_t>(d->iWidth),
                                      static_cast<int16_t>(d->iHeight));
    return 1;
}

// The video's frames by the clock of its sound (none: from now): late frames are skipped.
void frames(JPEGDEC& jpeg, bool hasSound) {
    const uint32_t t0 = millis();
    int shown = -1, drawn = 0;
    for (;;) {
        watchdog::feed();
        const uint32_t ms = introclock::elapsed(millis(), t0, speaker::startedAt(), hasSound);
        const int i = introclock::frameAt(ms, montage::fps(), montage::count());
        if (i >= montage::count()) break;
        const uint8_t* jpg = nullptr;
        size_t len = 0;
        if (i != shown && montage::frame(i, &jpg, &len)) {
            if (jpeg.openRAM(const_cast<uint8_t*>(jpg), static_cast<int>(len), draw)) {
                jpeg.setPixelType(RGB565_BIG_ENDIAN);
                const int w = jpeg.getWidth(), h = jpeg.getHeight();
                if (w > 0 && h > 0 && w <= panel::W && h <= panel::H) {
                    s_ox = (panel::W - w) / 2;
                    s_oy = (panel::H - h) / 2;
                    if (jpeg.decode(0, 0, 0)) ++drawn;
                }
                jpeg.close();
            }
            shown = i;
        }
        delay(2);
    }
    Serial.printf("[intro] %d of %d frames in %lu ms\n", drawn, montage::count(),
                  static_cast<unsigned long>(millis() - t0));
}

// A video's files: the full-screen pair when the card has its frames, else the black deck's.
struct Files {
    const char* video;
    const char* sound;
};
Files filesFor(bool max) {
    if (sdcard::has(max ? "max_wide.bin" : "intro_wide.bin"))
        return max ? Files{"max_wide.bin", "max_wide.pcm"} : Files{"intro_wide.bin", "intro_wide.pcm"};
    return max ? Files{"max.bin", "max.pcm"} : Files{"intro.bin", "intro.pcm"};
}

// Picks a video the card has (not last boot's) and remembers it; -1 for none.
int pick() {
    const bool kiss = sdcard::has(filesFor(false).video), max = sdcard::has(filesFor(true).video);
    Preferences p;
    p.begin("boot", false);
    const int last = p.getInt("video", -1);
    const int o = intropick::video(esp_random(), last, kiss, max);
    if (o >= 0 && o != last) p.putInt("video", o);
    p.end();
    return o;
}

void greeting() {
    sdcard::Blob pcm = sdcard::load("greeting.pcm");
    if (pcm.data && speaker::play(reinterpret_cast<const int16_t*>(pcm.data), pcm.size / 2, 16000, GREETING_VOLUME))
        speaker::wait();
    sdcard::release(pcm);
}
}  // namespace

void play(bool screen) {
    const int o = screen ? pick() : -1;
    if (o < 0) {
        greeting();
        return;
    }
    const Files files = filesFor(o == intropick::MAX);
    sdcard::Blob video = sdcard::load(files.video);
    sdcard::Blob sound = sdcard::load(files.sound);
    bool shown = false;
    // ~18 KB of decoder state, for the opening only.
    void* mem = heap_caps_malloc(sizeof(JPEGDEC), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    JPEGDEC* jpeg = mem ? new (mem) JPEGDEC() : nullptr;
    if (video.data && jpeg && montage::parse(video.data, video.size) && montage::count() > 0) {
        Serial.printf("[intro] %s: %d frames at %d fps\n", intropick::name(static_cast<intropick::Opening>(o)),
                      montage::count(), montage::fps());
        const bool hasSound = sound.data &&
                              speaker::play(reinterpret_cast<const int16_t*>(sound.data), sound.size / 2, 48000,
                                            VIDEO_VOLUME);
        frames(*jpeg, hasSound);
        speaker::wait();
        shown = true;
    } else {
        Serial.println("[intro] the video didn't load");
    }
    montage::parse(nullptr, 0);   // it pointed into the file just freed
    if (jpeg) jpeg->~JPEGDEC();
    if (mem) heap_caps_free(mem);
    sdcard::release(video);
    sdcard::release(sound);
    if (!shown) greeting();
}
}  // namespace opening
