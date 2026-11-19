# The OBD deck's drive log

The OBD deck (`env:obddeck`) can keep a per-second log of each drive and send it over the phone's
hotspot to a **Google Sheet in your own account**, through a small Apps Script web app that you
deploy. It is for checking the car's readings offline: OBD speed (01 0D) against the phone's GPS at
60 / 80 / 100 km/h, what PID 01 A4 answers, and the CVT's N/V ratio.

It is **off** until `DLOG_URL` and `DLOG_KEY` are set in `src/config.h` (git-ignored). The bench
build (`obddeck_bench`) never logs. The trip log on the deck's own flash (`/car`, `/fill`, the fuel
book) is separate and does not change.

## What lands in the Sheet

- **One tab per power-on of the deck**, named by the UTC time of its first record (`20261119-225001`).
  One row a second while the car answers, a minute of rows per request. Columns:
  `time` (a date in the spreadsheet's time zone), `t_utc` (unix seconds, one decimal), `ms` (the
  deck's `millis()`), `kmh` (01 0D), `rpm` (01 0C), `coolant_c`, `iat_c`, `throttle_pct` (01 11),
  `pedal_pct` (01 49, else 4A), `load_pct` (01 04), `volt` (the adapter's ATRV), `nv` (rpm per km/h:
  the CVT has no gears, so this ratio is the "estimated gear"; blank below 5 km/h).
  A blank cell is a missing or stale value, never a zero. Slow values (coolant, intake, load, volts)
  repeat their last reading until the next one (up to 30 s).
- **The `linkup` tab**: at each link-up, the raw reply to 0100, 0120, 0140, 0160, 0180, 01A0 (the
  supported-PID bitmaps) and 01A4, one row each (`request`, `reply` kept as text).
- No position, no VIN: nothing in the log says where the car is.

## How it behaves on the deck

- Its own task (`dlog`) on core 1 at the lowest priority; the OBD tasks have core 0, so the poll
  never waits for an upload. The task watchdog watches it (90 s; one request is ~40 s at worst).
- A part goes only when the hotspot is up, SNTP has set the clock, no OTA update is running and the
  heap has a 40 KB block for TLS (50 KB free) with the part's body already built. Otherwise nothing
  is tried (no DNS, no connect) and the rows wait: 5 minutes in RAM, then about 13 more on flash
  (`/dlog.bin`, 16 KB at most), sent oldest first when the link is back. Past that the newest are
  dropped (counted in the `[dlog]` line). Rows still waiting when the deck powers off are lost
  (their `millis()` can't be placed in time after a reboot), so the drive's last minute goes only if
  the deck stays on about a minute after the engine stops.
- Each request: a POST to the `/exec` URL, then a GET of the page Google redirects to, which holds
  the script's answer (`ok 60`, `ok dup` for a retry it already has, `bad key`). A wrong key or URL
  waits 10 minutes before the next try; no answer waits 30 s.
- Serial: `[dlog] part 3 sent: 60 rows (HTTP 200, 2900 ms, 0 dropped, stack left ...)`, or why it waits.

## Limits (from Google's pages, 2026-11)

- A spreadsheet holds up to 20 million cells. 12 columns, 3,600 rows an hour: about 460 hours of
  driving per spreadsheet. Start a new spreadsheet (and deployment) long before that.
- Apps Script, consumer account: 6 minutes per execution (each request takes about a second),
  30 at once. Google states no daily count for web app requests; the deck makes about 60 an hour.
- Each trip's part number is kept as a script property (a few bytes each).

## Setting it up (João)

### 1. The Sheet and the script

1. Open <https://sheets.new> signed in to your own Google account. Name it `CarDeck drive log`.
2. **Extensions → Apps Script**. Replace everything in `Code.gs` with the contents of
   `tools/drivelog_sheet.gs` from this repo. Click **Save** (the disk icon).
3. Make the shared key on the PC:

   ```bash
   python -c "import secrets; print(secrets.token_urlsafe(24))"
   ```
4. In the Apps Script editor: **Project Settings** (the gear) → **Script properties** →
   **Add script property**: Property `KEY`, Value: the string from step 3. **Save script properties**.

### 2. The web app

1. **Deploy → New deployment**. Next to *Select type*, the gear → **Web app**.
2. Description `cardeck`. **Execute as: Me**. **Who has access: Anyone** (the deck can't sign in to
   Google; the key is what keeps others out). **Deploy**.
3. **Authorize access** → your account → *Google hasn't verified this app* → **Advanced** →
   **Go to … (unsafe)** → **Allow**. It is your own script asking for your own spreadsheet.
4. Copy the **Web app URL** (it ends in `/exec`).
5. Check it: open that URL in a browser. It shows `cardeck drive log: up`.

If you change the script later: **Deploy → Manage deployments** → the pencil → Version: **New
version** → **Deploy**. The URL stays the same.

### 3. On the deck

1. In your main checkout's `src/config.h` (never committed), add:

   ```cpp
   #define DLOG_URL           "<the /exec URL>"
   #define DLOG_KEY           "<the key from step 1.3>"
   ```
2. Build and flash the OBD deck (USB the first time, or OTA on the hotspot):

   ```bash
   pio run -e obddeck -t upload --upload-port COMx
   ```
3. With the deck on the hotspot, the serial log shows `[dlog] on, one part per 60 rows`. After a
   link-up the `linkup` tab fills; after a minute of driving, a new tab with the rows appears.

The URL and the key live in the deck's firmware: anyone holding the deck could read them out and add
rows to the Sheet. To shut that door, change `KEY` in the script properties (the deck then gets
`bad key` until you flash the new one), or archive the deployment in **Manage deployments**.
