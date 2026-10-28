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
  effect: on the black deck it guards `GET /state` (now playing, lyrics, the deck's status,
  network name and trip), `POST /play` (sounds), `POST /trip/reset`, `GET /music` (music mode)
  and `GET /music/debug`; on the yellow deck `/say`, `/state`, `/presets` and `/status` (the
  car's lights and music mode). The page itself and the Home Screen
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
  with `REMOTE_TOKEN`, a random boot id and a counter that must rise. The yellow deck takes only the
  boot id the black deck proves in its `/music` reply (an HMAC over the id and the nonce of the
  yellow deck's own signed request), so a spoofed source address, a recorded packet, or one from an
  earlier boot can't set the car's lights.
- **The HMAC fails closed.** It is our own SHA-256 (no heap: mbedTLS's HMAC allocates, and the black
  deck's heap runs short), checked against RFC 4231 on the host and by a self-test at each boot; if
  it can't compute a tag the request or packet is refused and nothing is signed or sent.
- **The clock comes from SNTP, which isn't authenticated.** Someone on the hotspot who can answer a
  deck's NTP queries can set its clock. A signature's 2-minute window and the "nothing from before
  this boot" floor are measured on that clock, so a deck given a past time accepts requests recorded
  back then whose nonces it no longer remembers (after a reboot, or once they left its 32-entry ring);
  one given a far-off time refuses the real devices ("clock off"). The routes involved only read
  state or repeat a recorded light command; the bearer key is never exposed by it.

- Requests to Spotify (`api.spotify.com`, `accounts.spotify.com`) verify the server against
  7 bundled root certificates (`src/common/net/ca_bundle.inc`, built by `tools/gen_ca_bundle.py`).
  If Spotify moves to a CA outside the bundle, the log shows
  `[tls] certificate rejected: <host>`: regenerate the bundle and flash again. Certificate
  expiry is not checked (the board has no trusted clock during the handshake).
- **Lyrics, genres and sprites are fetched without certificate checks** (`setInsecure()`):
  they are public data and carry no credentials, and a full check would cost RAM.
- **The yellow deck's weather is plain HTTP**: ip-api.com (where the car is, from the hotspot's
  public IP) and Open-Meteo. No credentials go out, but ip-api.com and anyone on the path see the
  approximate location and the lookups.
- **The LED controllers have no pairing or password**: they take commands from any Bluetooth LE
  device that connects. While the yellow deck holds their link nobody else can connect (each takes
  one link at a time); while it doesn't, the controllers' own phone app or anyone nearby can.
- Album art is downloaded over plain HTTP from Spotify's image CDN (public images, chosen to
  save RAM).
