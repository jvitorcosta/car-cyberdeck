# Security

## Reporting a problem

Please don't open a public issue for a security problem. Use GitHub's
[private vulnerability reporting](https://github.com/jvitorcosta/spotify-car-deck/security/advisories/new)
instead, with steps to reproduce and the board and version (first serial line,
`[boot] cardeck v…`). You'll get an answer within a week.

## How credentials are handled

- WiFi passwords and the Spotify client ID, client secret and refresh token live only in
  `src/config.h`, which is git-ignored, and are compiled into the firmware. Anyone with
  physical access to the board can read them from flash: treat a lost deck like a lost key and
  revoke the token in your Spotify account (Apps → Remove access) and change the WiFi
  passwords.
- The refresh token is obtained on a PC with `tools/spotify_auth.py`, because Spotify only
  allows plain-`http` redirects to the loopback address.
- The Spotify client (`src/black/spotify/client.cpp`) never logs the token request, the access token
  or the secret.

## Known limitations

- **The phone remote is plain HTTP on the local network.** Its token travels in the URL and
  the request, readable by anyone on the same WiFi or hotspot. It gates more than a sound
  effect: on the decks' web servers it guards `GET /state` (now playing, lyrics, the deck's
  status, network name and trip), `POST /play` (sounds), `POST /trip/reset`, and on the
  yellow deck `/say`, `/state`, `/presets` and `/status` (the car's lights). The page itself and the Home Screen
  icon are public; they carry no data of their own. The remote is off unless
  `REMOTE_TOKEN` is set; use a long random token and don't reuse it, and only on networks you
  trust (your own hotspot): anyone on the network can reach the web server, and a deliberately
  slow request can hold it, though not the display.
- **Firmware updates over WiFi** (ArduinoOTA on port 3232, both decks) take their own password,
  `OTA_PASSWORD`, not `REMOTE_TOKEN`: the remote's token travels in plain-HTTP URLs, the update
  password never does (espota answers a challenge with it). Updates are off while `OTA_PASSWORD`
  is empty. Anyone with it, on the deck's network, can flash firmware that reads `src/config.h`
  back out, so make it long and random and don't reuse it. Updates aren't signed.
- **Devices sign; only the phone sends the token.** The yellow deck's requests to the black deck
  (`/state`, `/music`) and the Garmin app's to the yellow deck (`/say`, `/state`, `/presets`,
  `/status`) carry `t=<unix s>&n=<nonce>&sig=<HMAC-SHA256(REMOTE_TOKEN, method|path|params|t|n),
  16 bytes>` instead of the token (`src/util/wire/sigreq.h`, checked against RFC 4231 in
  `test/test_hmac256`). A deck accepts a signature within 2 minutes of its own clock, once (a ring of
  the last 32 nonces; older times are refused outright), compares it in constant time and never logs
  it. Someone who takes a deck's hotspot address, or sniffs the hotspot, learns nothing reusable from
  them. Before a deck's first SNTP answer it can't check a signature (503), and the yellow deck sends
  nothing to the black deck until its own clock is set: it never falls back to the token. The
  bearer `?k=` stays accepted on those routes because **Siri Shortcuts and the Home Screen page can't
  compute an HMAC**; those still put the token in plain-HTTP URLs, and anyone who sniffs one of them
  on the hotspot has it. The responses aren't signed (they carry nothing secret). On the hotspot the
  yellow deck looks the black deck up by mDNS when its fixed address stops answering as the deck.
- **Music mode's UDP palettes are signed too** (`src/util/wire/musicwire.h`): an 8-byte HMAC tag
  with `REMOTE_TOKEN`, a random boot id and a counter that must rise, so a spoofed source address
  or a recorded packet can't set the car's lights. A packet from an *earlier* boot of the black deck
  is still accepted when it arrives (a new boot id starts the counter over); it can only replay a
  real song's colours, at the usual levels (never under 40 %).

- Requests to Spotify (`api.spotify.com`, `accounts.spotify.com`) verify the server against
  7 bundled root certificates (`src/common/net/ca_bundle.inc`, built by `tools/gen_ca_bundle.py`).
  If Spotify moves to a CA outside the bundle, the log shows
  `[tls] certificate rejected: <host>`: regenerate the bundle and flash again. Certificate
  expiry is not checked (the board has no trusted clock during the handshake).
- **Lyrics, genres and sprites are fetched without certificate checks** (`setInsecure()`):
  they are public data and carry no credentials, and a full check costs RAM on the CYD.
- Album art is downloaded over plain HTTP from Spotify's image CDN (public images, chosen to
  save RAM).
