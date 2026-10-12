# Security

## Reporting a problem

Please don't open a public issue for a security problem. Use GitHub's
[private vulnerability reporting](https://github.com/jvitorcosta/car-deck/security/advisories/new)
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
- The Spotify library would print the token request (secret included) on every refresh;
  `tools/patch_spotify_lib.py` turns that off before every build.

## Known limitations

- **TLS certificates are not verified** (`setInsecure()`), including for the Spotify token
  endpoint. Someone who controls the network between the deck and Spotify could read the
  client secret and refresh token. Checking the Spotify certificates is planned.
- The on-device setup portal (`src/spotify/auth.cpp`), used only when no refresh token is
  configured, serves plain HTTP on the local network and is planned for removal.
- Album art is downloaded over plain HTTP from Spotify's image CDN (public images, chosen to
  save RAM).
