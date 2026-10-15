# Credits

CarDeck stands on a lot of other people's work. Thank you.

## Data and services

- **Walking sprites:** [PMDCollab SpriteCollab](https://github.com/PMDCollab/SpriteCollab)
  (Pokémon Mystery Dungeon sprite archive), licensed
  [CC BY-NC 4.0](https://creativecommons.org/licenses/by-nc/4.0/). Per-sprite artist credits
  are in each Pokémon's `credits.txt` in that repository; many sprites are original Spike
  Chunsoft assets. Downloaded by the deck at runtime, not redistributed here.
- **Pokémon data and fallback sprites:** [PokéAPI](https://pokeapi.co/) and
  [PokeAPI/sprites](https://github.com/PokeAPI/sprites).
- **Lyrics:** [LRCLIB](https://lrclib.net/).
- **Artist genres:** Apple's [iTunes Search API](https://performance-partners.apple.com/search-api).
- **Now playing:** the [Spotify Web API](https://developer.spotify.com/documentation/web-api).

## Libraries

[TFT_eSPI](https://github.com/Bodmer/TFT_eSPI) (FreeBSD/MIT mix),
[ArduinoJson](https://arduinojson.org/) (MIT),
[TJpg_Decoder](https://github.com/Bodmer/TJpg_Decoder) (FreeBSD-style, tjpgd),
[PNGdec](https://github.com/bitbank2/PNGdec) (Apache-2.0),
[JPEGDEC](https://github.com/bitbank2/JPEGDEC) 1.8.4 (Apache-2.0, S3 intro video),
[spotify-api-arduino](https://github.com/witnessmenow/spotify-api-arduino) (MIT),
[XPT2046_Touchscreen](https://github.com/PaulStoffregen/XPT2046_Touchscreen) (MIT, hardware
check only), [Unity](https://github.com/ThrowTheSwitch/Unity) (MIT, host tests).

The S3's QR code uses `esp_qrcode`, ESP-IDF's qrcode component shipped in the Arduino-ESP32
core (Apache-2.0), which is built on Project Nayuki's
[QR Code generator library](https://www.nayuki.io/page/qr-code-generator-library) (MIT).

## Files in this repository not covered by the MIT license

The MIT license in [LICENSE](LICENSE) covers the code and the original assets. These
generated files contain third-party material and keep their own terms:

| File | Contains | Terms |
|---|---|---|
| `src/ui/status_sprite.inc` | Pikachu, Zubat, Psyduck, Slowpoke and Porygon sprites from PokeAPI/sprites (`tools/gen_status_sprite.py`) | © Nintendo / Creatures Inc. / GAME FREAK inc.; fan use |
| `data/cjk16.bin` | [GNU Unifont](https://unifoundry.com/unifont/) 16.0.04 glyphs (`tools/gen_cjk_font.py`) | GPLv2+ with the GNU font embedding exception, or SIL OFL 1.1 |
| `src/pokemon/dex_data.inc` | Pokémon names and types from PokéAPI's CSV data (`tools/gen_dex.py`) | PokéAPI data; names © Nintendo |
| `src/util/genre_data.inc` | Apple's genre ids and names (`tools/gen_genres.py`) | Apple iTunes Search API terms |

The boot chime (`data/greeting_default.pcm`) and the car sprite (`src/ui/car_sprite.inc`) are
original and MIT-licensed.

## Personal media

Your own media are built locally, git-ignored and never distributed with this repository: the
avatar art (`tools/gen_avatar.py`), the intro clip (`tools/gen_intro.py`), the greeting clips
(`tools/gen_greeting.py`), the photo montage (`tools/gen_montage.py`) and the wolf howl
(`tools/gen_howl.py`). Use media you have the rights to. For the howl, the suggested source is
the public-domain ["Wolf howls.ogg"](https://commons.wikimedia.org/wiki/File:Wolf_howls.ogg) on
Wikimedia Commons.

## Inspiration

[intellij-pokemon-progress](https://github.com/kagof/intellij-pokemon-progress).

## Trademarks

CarDeck is a personal, non-commercial fan project, not affiliated with or endorsed by
Spotify, Nintendo, The Pokémon Company, Creatures Inc., GAME FREAK inc. or Honda. Pokémon is a
trademark of Nintendo; Spotify is a trademark of Spotify AB.
