# open-radars-manaus Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A public open-data repo with the Manaus speed cameras, electronic bumps and red-light cameras, built and checked by its own tools. car-cyberdeck is trimmed so it only pushes released data to the deck.

**Architecture:** A new local repo at `D:\dev\projects\open-radars-manaus` holds two things: `data/radars.csv` (hand-reviewed, confirmed points only) and a generated `data/radars.geojson`. Its Python tools (standard library only) come from car-cyberdeck's `tools/cameras.py`:
- they are split into focused modules;
- auto-placing is new (Nominatim, then a snap to the OSM carriageway);
- the review map can edit points that already exist;
- id-retirement checks and a release helper are added.

GitHub Actions runs `validate`. car-cyberdeck's `cameras.py` keeps only the device commands, and `push` downloads a GitHub Release asset.

**Tech Stack:** Python 3.12, standard library only; pytest; Leaflet 1.9.4 page with OSM tiles; GitHub Actions; `gh` CLI for the publish task.

**Spec:** `docs/superpowers/specs/2026-11-04-open-radars-manaus-design.md` (car-cyberdeck worktree `D:\dev\projects\spotify-pokemon-deck-camalerts`). Task 1 copies it into the new repo as `docs/design.md`.

## Global Constraints

- **CSV header,** exactly: `id,kind,lat,lon,heading,limit,label,source,checked`. Format: UTF-8, LF line endings, no quoting, no commas or `"` in fields, rows sorted by id.
- **Column values:**
  - `kind`: `speed`, `bump` or `redlight`.
  - `source`: `immu`, `osm`, `survey`, `car` or `community`.
  - `id`: `mao-\d{4}`.
  - `heading`: 0..359, required.
  - `limit`: empty or 1..199; always empty for `redlight`.
  - `checked`: a required YYYY-MM-DD date, not in the future.
- **Manaus box:** lat −3.25..−2.90, lon −60.15..−59.80 (`BBOX = "-3.25,-60.15,-2.90,-59.80"`, south,west,north,east).
- **Labels:** the style is `Street · District`. A label over 27 UTF-8 bytes is a warning, not an error.
- **No device limits** (96 rows, 6144 bytes) in the data repo. They stay in car-cyberdeck's `push`.
- **Guesses** never enter `radars.csv`. Only items with `confirmed: true` in `placed.json` are applied. `placed.json` and `build/` are git-ignored.
- **Licences:** data ODbL 1.0, credited "© OpenStreetMap contributors". Code MIT, "Copyright (c) 2026 João Vitor Pinheiro da Costa".
- **Release versions:** `YYYY.MM.N`. Release assets: `radars.csv` and `radars.geojson`.
- **Network etiquette:** the User-Agent is `open-radars-manaus/1.0 (+https://github.com/jvitorcosta/open-radars-manaus)` with no personal address. Nominatim gets at most 1 request/s.
- **Copy:** Portuguese first, then English. GitHub description: "Radares, lombadas eletrônicas e câmeras de avanço de sinal em Manaus: dados abertos". Topics: `radares`, `manaus`, `dados-abertos`, `openstreetmap`.
- **Ground rules in the docs and forms:** fixed public points only; no police checkpoints, blitz or mobile radars; no plates or faces in photos; never raw car traces.
- **Git, new repo:**
  - author `JoaoVitorPinheiro <joaovpro17@gmail.com>`, set in the repo's local config;
  - every commit ends with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`;
  - never write vault paths or personal notes into either repo.
- **Outward-facing steps** (Task 10, and any push in Task 11): each needs the owner's explicit yes in chat at that moment.

## Review Focus

1. **CRLF from a contributor's editor.** `validate` reports "line endings must be LF" (and `.gitattributes` forces LF). It never gives 9 "bad column" errors. Test: `test_validate_rejects_crlf` (Task 1).
2. **Longitude typed without the minus sign** (`59.97`). The error names the Manaus box, not just "out of range". Test: `test_validate_rejects_point_outside_manaus` (Task 1).
3. **Accented labels** (`Tarumã`). The GeoJSON keeps the real characters (not `\u00e3`), and the byte-length warning counts UTF-8 bytes. Tests: `test_geojson_keeps_accents` (Task 3), `test_validate_warns_on_long_label` (Task 1).
4. **A stale `placed.json`** whose `id` no longer exists in the CSV (the row was retired meanwhile). Apply refuses it and the file is untouched. Test: `test_apply_refuses_unknown_id` (Task 2).
5. **Nominatim or Overpass down during `place`.** Each point falls back to the city centre with no heading, and the page is still written. Test: `test_autoplace_survives_network_failure` (Task 5).

---

## File map (new repo)

```
open-radars-manaus/
  data/radars.csv            header only at first
  data/radars.geojson        generated
  data/retired.txt           "# id,date,reason" header line, then one id per line
  data/LICENSE               ODbL 1.0 full text
  LICENSE                    MIT
  README.md  CONTRIBUTING.md  CHANGELOG.md
  docs/design.md             the spec, copied
  tools/radarlist.py         schema, validate, read/write rows, ids, apply_placed
  tools/geojson.py           build + compare
  tools/sources.py           IMMU, survey, Overpass nodes, matching, diff
  tools/autoplace.py         geocode, snap to carriageway, heading
  tools/radars.py            CLI: fetch, check, place, validate, geojson, release
  tools/review_map.html      the review page template
  tests/test_*.py, tests/testdata/
  .github/workflows/ci.yml, .github/ISSUE_TEMPLATE/*.yml, .github/pull_request_template.md
  .gitignore  .gitattributes
```

Tests import the tool modules by putting `tools/` on `sys.path` in `tests/conftest.py`. Run them with `python -m pytest -q` from the repo root.

---

### Task 1: Repo scaffold and the CSV rules

**Files:**
- Create the repo: `D:\dev\projects\open-radars-manaus` (`git init -b main`, then the local `user.name` and `user.email` from the Global Constraints).
- Create: `LICENSE` (MIT), `data/LICENSE` (ODbL 1.0 text from opendatacommons.org), `.gitignore` (`build/`, `dist/`, `placed.json`, `__pycache__/`, `.pytest_cache/`), `.gitattributes` (`* text=auto eol=lf`), `data/radars.csv` (header only), `data/retired.txt` (`# id,date,reason` only), `docs/design.md` (a copy of the spec).
- Create: `tools/radarlist.py`, `tests/conftest.py`, `tests/test_radarlist.py`, `tests/testdata/good.csv` (3 rows, one of each kind, inside Manaus, `source` from the allowed set).

**Interfaces:**
- Produces, in `tools/radarlist.py`:
  - constants `HEADER`, `KINDS`, `SOURCES`, `BBOX_S=-3.25, BBOX_W=-60.15, BBOX_N=-2.90, BBOX_E=-59.80`, `MAX_LABEL=27`, `CSV_PATH`, `RETIRED_PATH`, `GEOJSON_PATH` (paths relative to the repo root).
  - `validate(text: str, today: str | None = None) -> tuple[list[str], list[str]]` returns `(errors, warnings)`; each message starts with `line N:` when it is about a row.
  - `read_rows(path) -> list[dict]` (row dict as in today's `cameras.read_rows`: lat and lon float, heading int, limit int or None, the rest str).
  - `rows_text(rows) -> str`, sorted by id, LF, trailing newline.

- [ ] **Step 1: Write the failing tests** in `tests/test_radarlist.py`. Port the validation tests from car-cyberdeck `tools/test_cameras_tool.py`:
  - `test_validate_accepts_a_good_list`
  - `test_validate_rejects_missing_heading`
  - `test_validate_rejects_redlight_limit`
  - `test_validate_warns_on_long_label`
  - `test_validate_rejects_unicode_digits`
  - `test_validate_rejects_quote_in_label`
  - `test_validate_rejects_bad_header_columns` (drop the "too big" part).

  Then add:
  - `test_validate_has_no_device_limits`: 200 valid rows, so `errors == []`.
  - `test_validate_rejects_point_outside_manaus`: a row with lon `59.97`, so an error contains `"outside Manaus"`.
  - `test_validate_rejects_crlf`: text with `\r\n`, so `errors == ["line endings must be LF"]`.
  - `test_validate_requires_checked`: empty `checked`, so an error contains `"checked"`; `checked="2026-13-01"` errors too; with `today="2026-11-04"`, `checked="2026-11-05"` gives an error containing `"future"`.
  - `test_validate_rejects_bad_id_and_duplicates`: `mao-12`, `abc-0001` and a repeated `mao-0001` each error.
  - `test_validate_rejects_unsorted_rows`: `mao-0002` before `mao-0001`, so an error contains `"sorted"`.
  - `test_validate_rejects_unknown_source`: `immu-survey`, so an error contains `"source"`.
  - `test_rows_text_round_trips`: `rows_text(read_rows(good.csv)) == good.csv text`.

- [ ] **Step 2: Run** `python -m pytest -q tests/test_radarlist.py`. Expected: FAIL, because `radarlist` doesn't exist.
- [ ] **Step 3: Implement `tools/radarlist.py`.** Start from `cameras.validate`, `_degrees_ok`, `_uint_ok`, `read_rows` and `rows_text`.
  - Drop `MAX_ROWS`, `MAX_BYTES` and `MAX_ID`.
  - The CRLF check runs first and returns at once.
  - The bbox error reads `line N: (lat, lon) is outside Manaus (-3.25..-2.90, -60.15..-59.80)`.
- [ ] **Step 4: Run** `python -m pytest -q`. Expected: all pass.
- [ ] **Step 5: Commit.** `git add -A`, then `git commit -m "feat: data layout, licences and the CSV rules"`. The message body says the tools start from car-cyberdeck's `tools/cameras.py` (MIT, same author) and ends with the trailer.

### Task 2: Ids, retirement and applying the review map

**Files:**
- Modify: `tools/radarlist.py`
- Test: `tests/test_ids.py`

**Interfaces:**
- Consumes: `read_rows`, `rows_text`, `validate` (Task 1).
- Produces:
  - `read_retired(path) -> dict[str, str]`: id to `"date,reason"`. It skips `#` lines and blanks.
  - `check_ids(rows, retired: dict, base_ids: set[str]) -> list[str]`. It returns errors:
    - an id from `base_ids` that is neither in rows nor in retired: `"mao-0003 was removed: add it to data/retired.txt"`;
    - a retired id that is in rows: `"mao-0003 is retired and can't come back"`.
  - `next_id(rows, retired) -> str`: `mao-%04d` of 1 + the highest number among rows and retired (`mao-0001` when both are empty).
  - `apply_placed(csv_path, retired_path, placed: list[dict], today: str | None = None) -> tuple[int, int]` returns `(added, updated)`.
    - It raises ValueError and leaves the file untouched in four cases: any item is not `confirmed: true`, an `id` is unknown, an `id` is retired, or the result fails `validate`.
    - An item with `id` replaces that row's kind, lat, lon, heading, limit and label, and sets `checked = today`; the row keeps its `source`.
    - An item without `id` is appended with `next_id`, its given `source`, and `checked = today`.
    - Labels lose `,` and `"` (as today) and are trimmed to one space.

- [ ] **Step 1: Write the failing tests:**
  - `test_next_id_skips_retired`: rows up to `mao-0002`, retired `mao-0005`, so the result is `mao-0006`.
  - `test_next_id_empty`: the result is `mao-0001`.
  - `test_check_ids_flags_silent_removal`
  - `test_check_ids_flags_resurrection`
  - `test_check_ids_ok_when_retired_properly`
  - `test_apply_appends_with_next_id`
  - `test_apply_updates_by_id_and_keeps_source`: the row was `immu`, the item moves 20 m and changes its limit. The result: same id, new lat and limit, `checked == today`, `source == "immu"`.
  - `test_apply_refuses_unconfirmed`
  - `test_apply_refuses_unknown_id`: the file bytes are unchanged afterwards.
  - `test_apply_refuses_retired_id`
  - `test_apply_refuses_an_invalid_result`: an item with no heading, so the file is unchanged.
  - `test_apply_strips_quotes_from_labels`
- [ ] **Step 2: Run** `python -m pytest -q tests/test_ids.py`. Expected: FAIL.
- [ ] **Step 3: Implement** the four functions in `tools/radarlist.py`. Write the file only after `validate` and `check_ids(rows, retired, old_ids)` both pass.
- [ ] **Step 4: Run** `python -m pytest -q`. Expected: all pass.
- [ ] **Step 5: Commit:** `feat: retired ids and applying the review map by id`.

### Task 3: GeoJSON

**Files:**
- Create: `tools/geojson.py`, `tests/test_geojson.py`, `data/radars.geojson` (built from the header-only CSV).

**Interfaces:**
- Consumes: `read_rows`.
- Produces:
  - `build(rows) -> str`. It returns a `FeatureCollection` with one `Point` per row; coordinates are `[lon, lat]`, rounded as in the CSV (6 decimals).
    - `properties` holds every column except lat and lon; `limit` is an int or null, `heading` an int.
    - The output is `json.dumps(..., ensure_ascii=False, indent=1)` plus `"\n"`, and features are in id order. The same rows always give the same bytes.
  - `matches(csv_path, geojson_path) -> bool`: the committed file equals `build(read_rows(csv))`.

- [ ] **Step 1: Write the failing tests:**
  - `test_geojson_point_is_lon_lat`
  - `test_geojson_limit_null_for_redlight`
  - `test_geojson_keeps_accents`: `"Tarumã"` appears literally in the output.
  - `test_geojson_is_stable`: two builds give equal bytes.
  - `test_geojson_empty_list`: `features == []`.
- [ ] **Step 2: Run** them. Expected: FAIL.
- [ ] **Step 3: Implement** `tools/geojson.py`, then write `data/radars.geojson` from the empty CSV.
- [ ] **Step 4: Run** `python -m pytest -q`. Expected: all pass.
- [ ] **Step 5: Commit:** `feat: generated GeoJSON`.

### Task 4: Sources and matching

**Files:**
- Create: `tools/sources.py`, `tests/test_sources.py`, `tests/testdata/immu_inforadar.html` (copied from car-cyberdeck `tools/testdata/`).

**Interfaces:**
- Produces (moved from `cameras.py` with no behaviour change except where noted):
  - `parse_immu(page) -> list[dict]`
  - `survey_items()`: now `source = "survey"`.
  - `parse_overpass(data)`
  - `fetch_immu()`
  - `fetch_osm()`
  - `http_req(url, data=None, headers=None, timeout=30) -> tuple[int, bytes]`, with the new User-Agent.
  - `matches(item, row)`, `diff(items, rows)`, `haversine_m`, `plain(s)` (was `_plain`).
  - Constants `IMMU_URL`, `OVERPASS_URLS`, `BBOX`, `USER_AGENT`.
- An item dict is `{kind, address, sentido, heading, label, limit, source, lat, lon}`, as today.
- `diff` still never reports `car` rows as gone; `community` rows aren't reported as gone either.

- [ ] **Step 1: Port the tests** from `test_cameras_tool.py`:
  - `test_parse_immu_lists`
  - `test_survey_rows_are_speed_60`: also assert `source == "survey"`.
  - `test_parse_overpass`
  - `test_match_by_distance`
  - `test_match_by_label_words`
  - `test_diff_new_and_gone`
  - `test_same_street_other_district_is_another_camera`
  - `test_heading_tolerance`
  - `test_user_agent_has_no_personal_address`: also assert `"open-radars-manaus"` is in it.

  Add `test_diff_never_drops_community_rows`.
- [ ] **Step 2: Run** them. Expected: FAIL.
- [ ] **Step 3: Move the code** into `tools/sources.py`.
- [ ] **Step 4: Run** `python -m pytest -q`. Expected: all pass.
- [ ] **Step 5: Commit:** `feat: IMMU, survey and OSM sources with matching`.

### Task 5: Auto-placing

**Files:**
- Create: `tools/autoplace.py`, `tests/test_autoplace.py`, `tests/testdata/ways_split.json` (a small Overpass `out geom` answer with two one-way ways of one avenue, opposite directions, about 15 m apart) and `tests/testdata/ways_twoway.json` (one two-way way).

**Interfaces:**
- Consumes: `sources.http_req`, `sources.plain`, `sources.haversine_m`, `NOMINATIM_URL`.
- Produces:
  - `geocode(item) -> tuple[float, float] | None`: moved from `cameras.geocode`, keeping the 1.1 s spacing; it adds `viewbox=-60.15,-2.90,-59.80,-3.25&bounded=1`.
  - `street_of(address) -> str`: the street part, as `geocode` splits it today.
  - `fetch_ways(lat, lon, street, radius_m=300) -> list[dict]`. The Overpass query is `[out:json][timeout:25];way(around:R,LAT,LON)["highway"]["name"];out tags geom;`. It keeps the ways whose `plain(name)` shares at least 2 non-stop words with `plain(street)` (reuse the `_STOP` set from `sources`).
  - `snap(lat, lon, ways, want_heading: int | None) -> tuple[float, float, int | None] | None`. The algorithm is below.
  - `autoplace(item) -> dict`: returns the item with `lat`, `lon`, `heading` and `guess=True` filled. It never raises on network errors; a failed step gives the geocoded spot or the centre `(-3.1019, -60.0250)` with `heading=None`.

**snap algorithm:**

```
project each way's segments into local metres around (lat, lon):
  x = (lon - lon0) * 111320 * cos(lat0), y = (lat - lat0) * 110574
for every segment a->b: foot = closest point on the segment, dist = |p - foot|
  bearing = degrees from north of a->b (atan2(dx, dy) % 360)
  oneway = tags.oneway: "yes"/"true"/"1" -> [bearing]; "-1" -> [bearing+180]; else -> [bearing, bearing+180]
  a candidate is (dist, foot, h) for each h in that list
when want_heading is not None: keep candidates with angle(h, want_heading) <= 90; pick the smallest dist
when want_heading is None: pick the smallest dist; heading = h only when the way is one-way, else None
return foot back in degrees (6 decimals), round(h) % 360; None when there are no candidates
```

- [ ] **Step 1: Write the failing tests:**
  - `test_snap_split_carriageway_picks_the_side_of_sentido`: a point between the two ways with want 270 snaps onto the westbound way, heading within 10° of its bearing; with want 90 it snaps onto the other way.
  - `test_snap_twoway_takes_the_direction_nearest_sentido`
  - `test_snap_unknown_sentido_on_twoway_has_no_heading`
  - `test_snap_no_ways_returns_none`
  - `test_fetch_ways_keeps_matching_names` (http_req monkeypatched with the testdata JSON plus one unrelated street).
  - `test_autoplace_survives_network_failure`: `http_req` raises `URLError`, so the result is the centre, `heading is None` and `guess is True`.
  - `test_autoplace_uses_snap_when_found` (both calls monkeypatched).
- [ ] **Step 2: Run** them. Expected: FAIL.
- [ ] **Step 3: Implement** `tools/autoplace.py`.
- [ ] **Step 4: Run** `python -m pytest -q`. Expected: all pass.
- [ ] **Step 5: Commit:** `feat: auto-place IMMU addresses on their carriageway`.

### Task 6: Review map and the CLI

**Files:**
- Create: `tools/review_map.html` (from car-cyberdeck `tools/camera_place.html`), `tools/radars.py`, `tests/test_cli.py`.

**Interfaces:**
- Consumes: Tasks 1–5.
- Produces, in `tools/radars.py` (with `argparse`, `main(argv=None) -> int`):
  - `fetch`, `check`: as `cmd_fetch` and `cmd_check` today; the messages name `data/radars.csv`.
  - `place [--from immu|osm|file] [--file items.json] [--no-serve]`. It loads the CSV rows as items with `id`, `confirmed: true` and `existing: true`, adds the auto-placed new items (`guess: true`), writes `build/review_map.html` and serves it as `serve()` does today. `--from file` reads a JSON list of items (car-cyberdeck's `import-car` writes one).
  - `place --apply placed.json`: calls `apply_placed`, then rewrites `data/radars.geojson`, and prints `added N, updated M`.
  - `validate [--base REF]`: `validate`, `check_ids` (base ids from `git show REF:data/radars.csv`; an empty set when the ref or file doesn't exist) and `geojson.matches`. It prints `error:` and `warning:` lines; exit 1 on any error.
  - `geojson`: rewrites `data/radars.geojson`.
  - `release VERSION`. The version must match `^\d{4}\.(0[1-9]|1[0-2])\.\d+$`. The tree must be clean (`git status --porcelain` empty), there must be no tag of that name already, and `validate` must pass. Then it copies both data files into `dist/`, prints the changelog lines to fill in, and prints the commands to run next: `git tag VERSION`, `git push origin VERSION`, `gh release create VERSION dist/radars.csv dist/radars.geojson --notes-file …`. It runs none of them.
  - Changelog lines: `Adicionados / Added`, `Alterados / Changed`, `Removidos / Removed`, by id, against the previous tag (`git describe --tags --abbrev=0`; everything is "added" when there is no tag).
- Page changes (`review_map.html`):
  - Existing points use a grey pin and are checked "position is right" from the start. Any edit sets `confirmed` (and so `checked`).
  - Guesses get the label "(palpite / guess: drag it!)" and an orange pin.
  - Unconfirmed items stay out of `placed.json`, as today.
  - An existing point that wasn't touched isn't written to `placed.json`. Write it only when it was edited, or when the reviewer ticks "re-confirm today".
  - The "Then:" hint says `python tools/radars.py place --apply placed.json`.
  - The Leaflet version stays the one the page pins today, and the OSM attribution stays.

- [ ] **Step 1: Write the failing tests:**
  - `test_place_page_embeds_items`
  - `test_place_page_marks_existing_and_guesses`: the page holds `"existing": true` for a CSV row and `"guess": true` for a new item.
  - `test_apply_rewrites_geojson`
  - `test_validate_cli_exit_codes` (good list 0, bad list 1).
  - `test_validate_base_catches_silent_removal`: a tmp git repo with two commits, so exit 1.
  - `test_release_refuses_bad_version_and_dirty_tree`
  - `test_release_changelog_lists_added_changed_removed` (a tmp git repo with a tag).
- [ ] **Step 2: Run** them. Expected: FAIL.
- [ ] **Step 3: Implement** `tools/radars.py` and the page changes.
- [ ] **Step 4: Run** `python -m pytest -q`. Expected: all pass. Then run `python tools/radars.py place --from immu --no-serve` once for real. Expected: about 27 items written to `build/review_map.html`, most with a heading. Open the page in a normal browser: grey and orange pins show, and saving gives a `placed.json`. Don't apply it, and leave the CSV as it is.
- [ ] **Step 5: Commit:** `feat: review map for new and existing points, radars.py CLI`.

### Task 7: Contributor-facing docs and CI

**Files:**
- Create: `README.md`, `CONTRIBUTING.md`, `CHANGELOG.md` (`# Changelog`, then "Nenhuma versão ainda / No release yet").
- Create: `.github/ISSUE_TEMPLATE/informar-radar.yml`, `.github/ISSUE_TEMPLATE/algo-errado.yml`, `.github/ISSUE_TEMPLATE/config.yml` (`blank_issues_enabled: false`), `.github/pull_request_template.md`, `.github/workflows/ci.yml`.

**Content decisions:**
- **README, Portuguese first:**
  1. what the data is (three kinds, fixed points only);
  2. a "Contribuições são bem-vindas" section with the two ways to help (issue form, PR);
  3. how to download the latest release, via the `releases/latest/download/radars.csv` link;
  4. the columns table (from the spec);
  5. the ground rules;
  6. licences and the OSM credit;
  7. the legal note: a list of public infrastructure positions, not a detector.

  Then `## English` with the same sections, shorter.
- **CONTRIBUTING:**
  - how to add or fix a row by hand (next id, `checked` date, `retired.txt` on removal);
  - running `python tools/radars.py validate` and `geojson`;
  - the review map for people with the repo;
  - that the owner reviews and merges;
  - the ground rules again.
- **Issue form `Informar um radar [Report a camera]`:**
  - labels `novo-ponto`, `precisa-verificar`;
  - fields: tipo (dropdown of the three kinds), endereço ou link do mapa (required), sentido (required), limite (optional), quando viu (date text), foto (textarea "cole o link ou arraste a imagem", with the text "sem placas nem rostos / no plates or faces");
  - a required checkbox "É um ponto fixo, não blitz nem radar móvel / A fixed point, not a checkpoint or mobile radar".
- **Issue form `Algo errado [Something's wrong]`:**
  - labels `correção`;
  - fields: id (required, placeholder `mao-0001`), o que está errado (dropdown: mudou de lugar, não existe mais, limite errado, sentido errado, outro), detalhes.
- **PR template checklist:**
  - ponto fixo;
  - sem dados pessoais;
  - `validate` passa;
  - GeoJSON regenerado.
- **CI (`ci.yml`):**
  - Triggers: `pull_request` and `push` to `main`.
  - Steps: `actions/checkout@v4` with `fetch-depth: 0`, `actions/setup-python@v5` with 3.12, `pip install pytest`, `python -m pytest -q`.
  - Then `python tools/radars.py validate --base origin/${{ github.base_ref || 'main' }}`. On a push to main, the base is `HEAD~1` (`github.event.before` when it is not all zeros).

- [ ] **Step 1: Write a test** `tests/test_repo_files.py`:
  - `test_issue_forms_parse`: load each YAML with a minimal line check, since there is no PyYAML. Each form has `name:`, `body:` and the expected labels.
  - `test_readme_is_portuguese_first`: "Contribuições são bem-vindas" comes before "## English".
  - `test_no_personal_paths`: no file contains `Obsidian`, `D:\\`, `C:\\Users` or `joaovpro17`.
- [ ] **Step 2: Run** the test. Expected: FAIL.
- [ ] **Step 3: Write the files.**
- [ ] **Step 4: Run** `python -m pytest -q`. Expected: all pass. Then run `python tools/radars.py validate --base HEAD`. Expected: exit 0.
- [ ] **Step 5: Commit:** `docs: README, contributing guide, issue forms and CI`.

### Task 8: Publish on GitHub (owner gate)

Outward-facing. Before each numbered action, show the owner exactly what it does and wait for a yes in chat.

- [ ] **Step 1:** Show `git log --format='%an <%ae> %s'` and the file list, so the owner can see the author email and that there are no personal paths.
- [ ] **Step 2 (yes needed):** Run:
  ```bash
  gh repo create jvitorcosta/open-radars-manaus --public --description "Radares, lombadas eletrônicas e câmeras de avanço de sinal em Manaus: dados abertos" --source . --push
  ```
- [ ] **Step 3 (yes needed):** Add the topics with `gh repo edit --add-topic radares,manaus,dados-abertos,openstreetmap`. Create the labels `novo-ponto`, `correção` and `precisa-verificar` with `gh label create`.
- [ ] **Step 4 (yes needed):** Protect `main`: CI must pass, no force push, and only the owner merges. This is a public repo with the owner as the only admin, so "require a PR" plus "required status check" is enough. Use `gh api -X PUT repos/jvitorcosta/open-radars-manaus/branches/main/protection` with the `ci` check.
- [ ] **Step 5:** Read back the result: `gh run list -L 1` shows CI green on main, and `gh repo view` shows the description and topics. No release is cut here; `2026.11.0` waits for confirmed points.

### Task 9: car-cyberdeck becomes a consumer

On branch `feat/camera-alerts`, in the worktree `D:\dev\projects\spotify-pokemon-deck-camalerts`. Commits use this repo's existing author config.

**Files:**
- Modify: `tools/cameras.py`, `tools/test_cameras_tool.py`
- Delete: `tools/camera_place.html`, `data/cameras.csv`, `tools/testdata/immu_inforadar.html`
- Modify: `README.md` (camera section), `docs/superpowers/specs/2026-11-03-camera-alerts-design.md` (the "list" parts point to open-radars-manaus)
- Replace: `docs/superpowers/specs/2026-11-04-open-radars-manaus-design.md`, with a 3-line pointer to `https://github.com/jvitorcosta/open-radars-manaus/blob/main/docs/design.md`

**Interfaces (the new `cameras.py`):**
- `validate(text)`: the full deck rules exactly as today, including `MAX_ROWS=96`, `MAX_BYTES=6144` and `MAX_ID=11`. It also accepts `source` values from the data repo.
- `release_url(version: str | None) -> str`:
  - `https://github.com/jvitorcosta/open-radars-manaus/releases/latest/download/radars.csv` when `None`;
  - `.../releases/download/{version}/radars.csv` otherwise.
- `push [--release VERSION | --file PATH] [--force]`: it downloads (or reads) the list, then validates, does the empty-list check and posts `POST /cam/list`, as today. When the download fails it says the release or the network is missing and exits 2.
- `import-car [--out build/car-marks.json] [--clear]`:
  - Without `--clear`: `GET /cam/marks`, then `parse_marks`, then it writes the items JSON (git-ignored `build/`). It prints the next step: `python tools/radars.py place --from file --file <path>` in an open-radars-manaus checkout.
  - With `--clear`: `POST /cam/marks/clear`.
  - The marks file never leaves the PC; its docstring says so.
- Removed: `fetch`, `check`, `place`, `apply_placed`, matching, sources, geocode, `write_place_page`, `serve`.

- [ ] **Step 1: Update the tests.**
  - Keep the validate tests (they still cover the device limits), `test_parse_marks` and `test_push_refuses_an_empty_list_without_force`. Remove the tests for the code that moved.
  - Add `test_release_url_latest_and_pinned`.
  - Add `test_push_from_release_downloads_and_posts`: monkeypatch `urllib.request.urlopen` and `deck_call`, and assert the posted body equals the downloaded text.
  - Add `test_push_download_failure_exits_2`.
  - Add `test_import_car_writes_marks_file` (`deck_call` monkeypatched; the file holds 2 items with `source == "car"`).
- [ ] **Step 2: Run** `python -m pytest -q tools/test_cameras_tool.py`. Expected: FAIL.
- [ ] **Step 3: Trim** `cameras.py` and update the docs listed above.
- [ ] **Step 4: Run** `python -m pytest -q tools` (the whole tools suite; it was 93 before this task, minus the moved tests, plus the new ones). Also run `pwsh tools/run_tests.ps1`. Expected: everything passes; the firmware is untouched.
- [ ] **Step 5: Commit:** `refactor(tools): cameras.py pushes releases of open-radars-manaus`. Nothing is pushed; the branch is finished later with superpowers:finishing-a-development-branch.
