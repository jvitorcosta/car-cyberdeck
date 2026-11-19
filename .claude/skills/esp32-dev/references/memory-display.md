# Memory and display performance

## The heap number that lies

`ESP.getFreeHeap()` and `getMaxAllocHeap()` also count 32-bit-only IRAM, which byte buffers,
mbedTLS and the WiFi driver cannot use. They read ~60 KB too high and hid an exhausted heap for
days (`docs/HISTORY.md` items 8-9). Measure **byte-addressable internal RAM**:

```cpp
heap_caps_get_free_size(MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
heap_caps_get_largest_free_block(MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
```

That is what `mem::byteFree()` / `mem::byteLargest()` (`src/black/core/mem.h`) report. The
**largest block** matters more than the total: fragmentation is what kills TLS.

## Budgets that were measured

| Need | Size | Source |
|---|---|---|
| mbedTLS record buffers | 16,717 B contiguous each, in and out; plan ~40 KB in one block for a session | HISTORY item 8; CONTRIBUTING "On the device"; `SSL_MAX_CONTENT_LEN 16384` |
| WiFi RX buffer | 1,512 B each | HISTORY items 8, 11 |
| Black deck, OBD link on | ~107-121 KB free, ~98 KB largest (172 KB before NimBLE) | ARCHITECTURE "Live gauges"; memory `black-obd-gauges` |
| Black deck, camera alerts | keep the largest block >= 102 KB | camalerts branch notes |
| Yellow deck, desk, WiFi up | ~37-42 KB free, block ~22 KB; low ~30 KB | memory `yellow-heap-guard`, `yellow-deck-scope` |
| Yellow deck, panic zone | 15-19 KB free, block 124 B-4 KB | #66 (81f01da) |
| Yellow `heapguard` thresholds | wait under 18 KB free or 6 KB block, resume at 22 KB | `src/util/yellow/heapguard` |

## Rules

1. **Big CPU-only buffers go to PSRAM** on the black deck: `mem::bigAlloc(n)` (PSRAM if present,
   else internal). Bitmaps, download scratch, decoders, arenas, JSON bodies. Static DRAM `.bss`
   went 79 KB -> 41 KB this way (#22, e5e1b49). `EXT_RAM_BSS_ATTR` is not available (the
   precompiled sdkconfig lacks `SPIRAM_ALLOW_BSS_SEG_EXTERNAL_MEMORY`).
2. **Never put a DMA or ISR buffer in PSRAM** without `MALLOC_CAP_DMA`. The panel is driven without
   DMA today, which is why the PSRAM sprites work.
3. **Internal RAM is for mbedTLS, lwIP, WiFi and Bluetooth.** NimBLE's host memory is in PSRAM on
   the black deck (`CONFIG_BT_NIMBLE_MEM_ALLOC_MODE_EXTERNAL=1`).
4. **Allocate once at boot, reuse.** Fixed buffers (a 92x92 art bitmap, a 12-64 KB walk scratch,
   a ~5.6 KB lyrics arena) instead of malloc/free per song (HISTORY item 9).
5. **Stream, don't buffer.** `HTTPClient::getString()` on a large JSON failed on a fragmented heap;
   HTTP/1.0 plus a filtered stream parse fixed it (HISTORY item 3).
6. **Gate TLS on the largest block.** The black deck's network steps wait until the largest block
   fits TLS (`TLS_NEED`), and 2 failed polls pause optional downloads (HISTORY item 9).
7. **Yellow deck: no TLS at all.** No PSRAM and Bluedroid dual mode leave no room. Plain HTTP and
   fixed buffers only (ARCHITECTURE "Yellow deck" Memory).
8. **Size task stacks from measurement**, not guesses: `uxTaskGetStackHighWaterMark()` is in every
   `[mem]` line. Leave ~2 KB spare. Shrinking the yellow deck's stacks bought ~6 KB of heap (#66).
9. **Count failed allocations**: `heap_caps_register_failed_alloc_callback` (`common/core/crashlog`).
   Inside that callback: no allocation and no `Serial` (its lock); `esp_rom_printf` only.
10. **Churn is fragmentation.** Installing/removing the I2S driver per beep moved ~8 KB every 1.5 s;
    install once per alarm instead (camalerts notes).

## Reading the logs

- Black: `[mem] <where> free=<B> largest=<B> stackfree=<B> [allocfail=N (last S B caps=0x..)]`.
- Yellow: `[mem] heap <B> (min <B>, block <B>, fails N)  stack left: link .., voice .., deck .., music .., loop ..` every 10 s.
- OBD: `[mem] heap <B> (min <B>)  stack left: obd-link .., obd-ask .., net .., ota .., loop ..` every minute.
- Every deck at boot: `[boot] reset reason: ...` and the crash ring `[crash] N: <reason> after <s>s:
  heap .. (min .., block ..), N failed allocs (last S B), at <phase>` (kept in NVS + RTC memory).
- `python tools/serial_health.py <log>` summarizes all of these (lowest heap, block, stacks,
  crashes) for any deck; `tools/analyze_session.py` adds the black deck's per-step timings.

## Display (TFT_eSPI, not LVGL)

The decks draw with **TFT_eSPI 2.5.43** directly (sprites, `pushImage`, GFX free fonts), plus
TJpg_Decoder / JPEGDEC / PNGdec. There is no LVGL in these repos. If someone proposes LVGL, its
draw buffers and object heap would compete with TLS on the S3's internal RAM and do not fit the
yellow deck at all.

- **Only redraw what changed.** Full-screen redraws per value change flashed black at 1 Hz; each
  value redraws its own region (gauges, `healthview`, trip band).
- **Sprites in PSRAM** (S3): the gauge panel (140x144), frame sprites, a region buffer; push the
  sprite, never draw piecemeal to the panel during animation.
- **SPI clock**: S3 40 MHz (80 MHz corrupted the picture, 2026-11-03); CYD asks 55 MHz, gets 40.
  The S3's TFT is on SPI3 via the GPIO matrix; GPIO 10-13 are SPI2's IOMUX pins, so SPI2 at 80 MHz
  is an untested option (music-branch research note).
- **No `initDMA()`**: after TFT_eSPI's `initDMA()` every later draw was black, with or without
  `deInitDMA()` (dfbca0e, 6ac2565; HISTORY item 16). The intro pushes without DMA at ~50 ms a
  frame and `util/introclock` drops late frames to stay in sync with the sound.
- **Fonts cost flash, not RAM**: CJK Unifont (686 KB) read from flash on demand; `SMOOTH_FONT` is
  unflagged on the S3 (-16.8 KB), fonts 2/4 unflagged on the yellow deck.
- **Draw from one task.** On the black deck only the UI loop (core 1) touches the display; the
  network task never does. Two tasks drawing on one SPI bus corrupt the frame.
- Frame budget references: night drive 24 ms a frame at 40 MHz; gauges at most 10 fps; album art
  decode ~118 ms (HISTORY item 5).
