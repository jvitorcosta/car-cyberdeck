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
[TJpg_Decoder](https://github.com/Bodmer/TJpg_Decoder) (FreeBSD-style, tjpgd),
[PNGdec](https://github.com/bitbank2/PNGdec) (Apache-2.0),
[JPEGDEC](https://github.com/bitbank2/JPEGDEC) 1.8.4 (Apache-2.0, S3 intro video),
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
| `src/black/ui/status_sprite.inc` | Pikachu, Zubat, Psyduck, Slowpoke and Porygon sprites from PokeAPI/sprites (`tools/gen_status_sprite.py`) | © Nintendo / Creatures Inc. / GAME FREAK inc.; fan use |
| `data/cjk16.bin` | [GNU Unifont](https://unifoundry.com/unifont/) 16.0.04 glyphs (`tools/gen_cjk_font.py`) | GPLv2+ with the GNU font embedding exception, or SIL OFL 1.1 |
| `src/yellow/fonts/gojira16.h`, `garmin/carlights/resources/drawables/gojira_*.png`, `data/jp16.bin` | Unifont glyphs taken from `data/cjk16.bin`: the three of ゴジラ (`tools/gen_gojira_glyphs.py`), GOJIRA's name on the yellow deck and the watch; and the Japanese subset for the yellow deck's lyrics: kana, CJK punctuation, half/fullwidth forms and the JIS X 0208 level 1 kanji (`tools/gen_jp_font.py`) | GPLv2+ with the GNU font embedding exception, or SIL OFL 1.1 |
| `data/emoji16.bin` | [Twemoji](https://github.com/jdecked/twemoji) 15.1.0 graphics, © Twitter, Inc and other contributors, shrunk to 16 px (`tools/gen_emoji.py`) | CC-BY 4.0 |
| `tools/fonts/VT323-Regular.ttf` | [VT323](https://fonts.google.com/specimen/VT323), © 2011 The VT323 Project Authors; licence text in `tools/fonts/VT323-OFL.txt` | SIL OFL 1.1 |
| `src/yellow/fonts/vt323_*.h` | VT323 glyph bitmaps at 17, 19, 24 and 36 px, derived from the font above (`tools/gfxfont.py`) | SIL OFL 1.1 (see `tools/fonts/VT323-OFL.txt`) |
| `tools/emoji_freq.txt` | Unicode's [emoji frequency](https://home.unicode.org/emoji/emoji-frequency/) tiers | Unicode terms of use |
| `tools/emoji_zwj.txt` | Unicode 15.1 [emoji ZWJ sequences](https://unicode.org/Public/emoji/15.1/) without skin tones | Unicode terms of use |
| `src/black/pokemon/dex_data.inc` | Pokémon names and types from PokéAPI's CSV data (`tools/gen_dex.py`) | PokéAPI data; names © Nintendo |
| `src/util/black/genre_data.inc` | Apple's genre ids and names (`tools/gen_genres.py`) | Apple iTunes Search API terms |

The boot chime (`data/greeting_default.pcm`) and the car sprite (`src/black/ui/car_sprite.inc`) are
original and MIT-licensed.

## Personal media

Your own media are built locally, git-ignored and never distributed with this repository: the
avatar art (`tools/gen_avatar.py`), the intro clips (`tools/gen_intro.py`), the greeting clips
(`tools/gen_greeting.py`), the photo montage (`tools/gen_montage.py`) and the wolf howl
(`tools/gen_howl.py`). Use media you have the rights to. For the howl, the suggested source is
the public-domain ["Wolf howls.ogg"](https://commons.wikimedia.org/wiki/File:Wolf_howls.ogg) on
Wikimedia Commons.

The pixel Lightning McQueen in the S3's boot opening is a fan sprite of a Disney/Pixar
character (*Cars*), drawn with PixelLab and packed locally by `tools/gen_mcqueen_sprite.py`. It
lives in the git-ignored `montage-photos/mcqueen.png` and `data/mcqueen.bin`, never distributed here.

The Max Verstappen opening is a clip of Formula 1's broadcast graphics (Formula One Management),
cut locally by `tools/gen_intro.py` from a copy you download yourself. It lives in the git-ignored
`max-clip/` and `data/max.bin` / `max.pcm`, never distributed here.

Sonic waiting on the S3's "Nothing playing" screen is rebuilt locally by `tools/gen_idle_sprite.py`
from Sonic the Hedgehog (Mega Drive, 1991) art in the
[s1disasm](https://github.com/sonicretro/s1disasm) disassembly. The art is © SEGA; it is fetched
into the git-ignored `sonic-sprites/` and `data/idle.bin`, never distributed here. The Green Hill
Zone behind the firmware-update screen is rebuilt the same way by `tools/gen_backdrop.py`
(into `sonic-sprites/ghz/` and `data/backdrop.bin`). Shadow skating on its rail is cut by
`tools/gen_shadow_rider.py` from his Sonic Battle (GBA, 2003) sprite sheet: original sprites by
SEGA and Sonic Team, ripped by QuadFactor, fetched from
[Sonic Battle Unlimited Edition](https://github.com/SingularMJ/Sonic-Battle-Unlimited-Edition)
into `sonic-sprites/shadow/` and `data/shadow.bin`, never distributed here.

## Inspiration

[intellij-pokemon-progress](https://github.com/kagof/intellij-pokemon-progress).

## Trademarks

CarDeck is a personal, non-commercial fan project, not affiliated with or endorsed by
Spotify, Nintendo, The Pokémon Company, Creatures Inc., GAME FREAK inc., SEGA, Toho or Honda.
Pokémon is a trademark of Nintendo; Sonic the Hedgehog is a trademark of SEGA; Godzilla (ゴジラ,
the GOJIRA light preset's namesake) is a trademark of Toho Co., Ltd.; Spotify is a trademark of
Spotify AB.
