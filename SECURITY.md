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
- The Spotify client (`src/spotify/client.cpp`) never logs the token request, the access token
  or the secret.

## Known limitations

- **The phone remote is plain HTTP on the local network.** Its token travels in the URL and
  the request, readable by anyone on the same WiFi or hotspot. It gates more than a sound
  effect: on the decks' web servers it guards `GET /state` (now playing, lyrics, the deck's
  status, network name and trip), `POST /play` (sounds), `POST /trip/reset`, and on the
  companion `POST /press` (its menu, Howl and trip reset). The page itself and the Home Screen
  icon are public; they carry no data of their own. The remote is off unless
  `REMOTE_TOKEN` is set; use a long random token and don't reuse it, and only on networks you
  trust (your own hotspot): anyone on the network can reach the web server, and a deliberately
  slow request can hold it, though not the display.
- **Firmware updates over WiFi** (ArduinoOTA on port 3232, both decks) take their own password,
  `OTA_PASSWORD`, not `REMOTE_TOKEN`: the remote's token travels in plain-HTTP URLs, the update
  password never does (espota answers a challenge with it). Updates are off while `OTA_PASSWORD`
  is empty. Anyone with it, on the deck's network, can flash firmware that reads `src/config.h`
  back out, so make it long and random and don't reuse it. Updates aren't signed.
- **The companion screen reads `/state` the same way:** it sends `REMOTE_TOKEN` in plain HTTP
  on the hotspot about once a second, so anyone on that network can read the token and the
  now-playing feed.

- Requests to Spotify (`api.spotify.com`, `accounts.spotify.com`) verify the server against
  7 bundled root certificates (`src/net/ca_bundle.inc`, built by `tools/gen_ca_bundle.py`).
  If Spotify moves to a CA outside the bundle, the log shows
  `[tls] certificate rejected: <host>`: regenerate the bundle and flash again. Certificate
  expiry is not checked (the board has no trusted clock during the handshake).
- **Lyrics, genres and sprites are fetched without certificate checks** (`setInsecure()`):
  they are public data and carry no credentials, and a full check costs RAM on the CYD.
- Album art is downloaded over plain HTTP from Spotify's image CDN (public images, chosen to
  save RAM).
