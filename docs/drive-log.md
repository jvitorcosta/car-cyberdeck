# The OBD deck's drive log

The OBD deck (`env:obddeck`) can keep a per-second log of each drive and send it to a **private
GitHub repo** over the phone's hotspot. It is for checking the car's readings offline: OBD speed
(01 0D) against the phone's GPS at 60 / 80 / 100 km/h, what PID 01 A4 answers, and the CVT's N/V ratio.

It is **off** until `DLOG_REPO` and `DLOG_TOKEN` are set in `src/config.h` (git-ignored). The bench
build (`obddeck_bench`) never logs. The trip log on the deck's own flash (`/car`, the fuel book) is
separate and does not change.

## What it writes

One folder per power-on of the deck, named by the UTC time of its first record:

```
drive/20261119-225001/linkup1.txt   the raw replies at the link-up
drive/20261119-225001/p0001.csv     the first minute of rows
drive/20261119-225001/p0002.csv     the next minute, ...
```

- `pNNNN.csv`: one row a second while the car answers, 60 rows a part. Columns:
  `t_utc` (unix seconds, one decimal), `ms` (the deck's `millis()`), `kmh` (01 0D), `rpm` (01 0C),
  `coolant_c`, `iat_c`, `throttle_pct` (01 11), `pedal_pct` (01 49, else 4A), `load_pct` (01 04),
  `volt` (the adapter's ATRV), `nv` (rpm per km/h: the CVT has no gears, so this ratio is the
  "estimated gear"; blank below 5 km/h). A blank cell is a missing or stale value, never a zero.
  Slow values (coolant, intake, load, volts) repeat their last reading until the next one (up to 30 s).
- `linkupN.txt`: at each link-up, the raw reply to 0100, 0120, 0140, 0160, 0180, 01A0 (the
  supported-PID bitmaps) and 01A4, one `<request> -> <reply>` line each.
- No position, no VIN: nothing in the log says where the car is.

A part is created once and never changed. Join a trip with:

```bash
python tools/dlog_join.py <clone>/drive --latest -o trip.csv
```

## How it behaves on the deck

- Its own task (`dlog`) on core 1 at the lowest priority; the OBD tasks have core 0, so the poll
  never waits for an upload. The task watchdog watches it (90 s; one upload is ~40 s at worst).
- A part goes only when the hotspot is up, SNTP has set the clock, no OTA update is running, and the
  heap has a 40 KB block for TLS (60 KB free). Otherwise nothing is tried and the rows wait in RAM:
  6 minutes of them (the oldest dropped past that, counted in the `[dlog]` line).
- The last minute of a drive goes only if the deck stays on a minute after the engine stops.
- Serial: `[dlog] sent drive/.../p0003.csv: 60 rows (HTTP 201, 1840 ms, ...)`, or why it waits.
  A wrong token or repo (401 / 403 / 404) waits 10 minutes before the next try.

## Setting it up (João)

### 1. The repo

1. Open <https://github.com/new>.
2. Owner: your account. Name: `car-drive-log` (any name works; use the same one in step 3).
3. Choose **Private**. Tick **Add a README file**. **Create repository**.

### 2. The token (fine-grained, this repo only)

1. Open <https://github.com/settings/personal-access-tokens/new>
   (Settings → Developer settings → Personal access tokens → Fine-grained tokens → Generate new token).
2. Token name: `cardeck obd drive log`. Expiration: your choice (when it runs out, the deck logs
   `HTTP 401` and keeps its rows in RAM; make a new one and flash again).
3. Resource owner: your account. Repository access: **Only select repositories** → `car-drive-log`.
4. Permissions → Repository permissions → **Contents: Read and write**. Leave everything else at
   *No access* (GitHub adds *Metadata: Read-only* by itself).
5. **Generate token** and copy it (it shows once).

### 3. On the deck

1. In your main checkout's `src/config.h` (never committed), add:

   ```cpp
   #define DLOG_REPO          "<your account>/car-drive-log"
   #define DLOG_TOKEN         "<the token>"
   ```
2. Build and flash the OBD deck (USB the first time, or OTA on the hotspot):

   ```bash
   pio run -e obddeck -t upload --upload-port COMx
   ```
3. With the deck on the hotspot, the serial log shows `[dlog] on: <repo>, one part per 60 rows`.
   After a minute of driving, `drive/<time>/p0001.csv` is in the repo.

The token lives in the deck's firmware: anyone holding the deck could read it out. It can only write
files in that one repo, so the worst case is a spoiled log; revoke it at
<https://github.com/settings/personal-access-tokens> if the deck is lost.
