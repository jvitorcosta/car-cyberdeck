# The decks' hardware

João's names: **black deck** = the ESP32-S3 board, **yellow deck** = the Cheap Yellow Display.
The **OBD deck** is a third firmware for a plain ESP32 that has never been flashed.

| | Black deck | Yellow deck | OBD deck |
|---|---|---|---|
| Board | Freenove FNK0104B | ESP32-2432S028R "CYD" | any ESP32 devkit (`esp32dev`) |
| Chip | ESP32-S3R8, 2 cores, 240 MHz | ESP32 (classic), 2 cores, 240 MHz | ESP32 (classic) |
| Flash | 16 MB, QIO 80 MHz | 4 MB, DIO 40 MHz | 4 MB |
| PSRAM | 8 MB octal (`qio_opi`) | **none** | none |
| Display | 2.8" ILI9341 240x320 SPI, BGR | 2.8" ILI9341 240x320 SPI | none |
| Touch | FT6336U (I2C 0x38), only used by diag envs | XPT2046 resistive, its own SPI bus | none |
| Audio | ES8311 codec (I2C 0x18) + amp, speaker, mic | none | none |
| USB | the S3's native USB (HWCDC), no UART bridge | CH340 USB-UART bridge | the devkit's bridge |
| Usual port | COM12 | COM13 to COM18 (the CH340 moves: seen as 13, 16, 17, 18) | `--upload-port COMx` |
| Firmware env | `s3` (`s3_ota` over WiFi) | `ledpanel` (USB only) | `obddeck` (`obddeck_ota`) |
| Radio in use | WiFi + NimBLE central (the Vgate OBD adapter) | WiFi + Bluedroid dual mode (BLE for two LED controllers, Classic for the PS3 pad) | WiFi + NimBLE central |

Check the port before you flash: `pio device list`. A CH340 that changed number is normal.

## Pins

Source of truth: `include/pins.h` and the `-D TFT_*` flags in `platformio.ini`.

**Black deck (S3)**
- TFT: MISO 13, MOSI 11, SCLK 12, CS 10, DC 46, BL 45 (active HIGH), `TFT_RST=-1`.
  SPI at 40 MHz (80 MHz corrupted parts of the picture, 2026-11-03, `platformio.ini` comment).
- I2C (codec + touch): SDA 16, SCL 15.
- I2S: MCLK 4, BCLK 5, WS 7, DOUT 8, DIN 6. Amp enable GPIO 1, **LOW plays**.
- SD (SDMMC 4-bit) exists but is unused: mounting it ate the TLS heap (see `pitfalls.md`).

**Yellow deck (CYD)**
- TFT on HSPI: MISO 12, MOSI 13, SCLK 14, CS 15, DC 2, BL 21. `SPI_FREQUENCY=55000000` asks for
  55 MHz; the divider gives 40 MHz in practice.
- Touch (XPT2046) on its own bus: SCLK 25, MISO 39, MOSI 32, CS 33, IRQ 36. Calibration values
  `TOUCH_RAW_*` in `pins.h`; the deck stores its own in NVS (`touch`).
- IR receiver (KY-022) on CN1, GPIO 27 (`lib/irrx`).
- GPIO 12 is a strapping pin (flash voltage). It is the TFT's MISO here and works because the
  panel does not pull it high at boot. Do not wire anything that pulls it high.

## Peripherals around the decks

- Two ELK-BLEDDM / MELK BLE LED controllers (service FFF0, characteristic FFF3), driven by the
  yellow deck.
- Sony DualShock 3 over Bluetooth Classic (yellow deck), paired with `tools/ds3_pair.py`.
- Vgate iCar 2S OBD adapter over BLE (black deck, and the OBD deck when it exists).
- Garmin FR255 watch app (`garmin/`), which talks to the decks over HTTP on the iPhone hotspot.
- iPhone hotspot: 2.4 GHz only ("Maximize Compatibility"). Pinned addresses: black
  172.20.10.10, yellow 172.20.10.11, OBD 172.20.10.12 (`util/hotspotip`).

## Sibling projects on the same boards

- `car-can-lab` (separate repo): listen-only CAN sniffer on a CYD with a BLE ELM327, env `sniff`.
  It keeps `min_spiffs.csv` on purpose so it and the yellow deck can replace each other.
- `spotify-pokemon-deck-voice` (worktree for the voice agent): same black deck plus mic diag envs
  (`micrec_s3`, `micmeter_s3`). The ES8311 is the I2S slave; the S3 is master in full duplex
  because TX is what clocks the codec (`src/black/board/s3/audioport.h` there).

Sources: `platformio.ini`, `include/pins.h`, `docs/ARCHITECTURE.md`, `README.md` (hardware
table), the music-branch research note on both decks (sdkconfig readings), memory notes
`deck-names`, `terminal-line-link-tags` (CH340 as COM18).
