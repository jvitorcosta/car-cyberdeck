# Pokéface v2: the Emerald battle redesign

Supersedes the Charmander-line partner and layout "A, all at once" of
`2026-10-23-pokeface-watch-face-design.md`. Everything that spec says about the device, power, memory, licensing, data
sources and "--" for missing values still holds unless this one changes it.

## Intent and why

João wore v1 and wanted three things changed:
- **Too much dark background.** v1 drew on black.
- **The Pokémon is too small.** v1 drew it at 19–68 px.
- **The look.** He likes 8-bit-era sprites, and wants the face to look like Pokémon Emerald's battle screen, with its typography.

The Charmander line and the Megas are gone. Each weekday gets its own pair of Pokémon (one for the day, one for the night), and a third legendary appears as the reward for a run or the step goal.

## Goals

- Every Pokémon is a Gen 3 (Emerald-style) sprite, drawn big: the 64×64 sprite at 2×.
- The face reads as Emerald's battle screen: a light Hoenn grass field, a battle platform, game-style info boxes with HP and EXP bars, and a pixel font copied from the game.
- Seven weekday pairs plus seven reward legendaries, 21 Pokémon in all.
- The same data as v1, rearranged.

## Non-goals

- Megas, evolutions, a time-of-day crew, or Pokémon the user can pick.
- Store publishing. The face is still sideloaded onto João's own watch.

## Anti-goals

- A mostly black face.
- A small Pokémon.
- Blurry or smoothed pixels. Scaling is integer and nearest-neighbour only, and the font is a bitmap font at its native pixel size.
- Fan or Nintendo art, or the fan font, committed to this **public** repo.
- Worse battery than v1. Readings stay at once a minute and on wake, and the animation stays capped at about 10 s.

## Who is out

| Weekday | Day (sunrise → sunset) | Night (sunset → sunrise) | Reward |
|---|---|---|---|
| Monday | Blaziken | Sceptile | Latias |
| Tuesday | Ho-Oh | Lugia | Suicune |
| Wednesday | Rayquaza | Deoxys | Latios |
| Thursday | Cresselia | Darkrai | Jirachi |
| Friday | Arceus | Giratina | Dialga |
| Saturday | Solgaleo | Lunala | Necrozma |
| Sunday | Xerneas | Yveltal | Zygarde |

- **Day or night:** day runs from today's sunrise to today's sunset, and night is the rest. *(Claude's call, accepted.)* Without sun times (no weather position), fall back to 06:00–18:00.
- **Weekday:** this is the local calendar weekday. After midnight the pair switches to the new weekday's, so from 00:00 to sunrise it's the new weekday's night Pokémon.
- **Reward:** finishing a run today (v1's `finishedToday` rule, FIT-epoch aware) or steps ≥ step goal (goal > 0) brings out the weekday's reward. It replaces whoever is out, day or night, until midnight. *(Claude's guess, accepted.)* There is no random roll any more.

## Sprites

- **Source:** the rh-hideout/pokeemerald-expansion project on GitHub. For each species, `graphics/pokemon/<name>/anim_front.png` holds 2 frames of 64×64 stacked vertically, and `normal.pal` holds the JASC palette. Folder names are lower case with underscores, for example `ho_oh`. All 21 were checked to exist on 2026-10-23.
- **Art:** the Gen 3 Pokémon are the original Emerald art. Later-generation ones (Darkrai, Arceus, Giratina, Cresselia, Dialga, the Alola and Kalos ones) were drawn by fans in Gen 3 style.
- **Pinning:** pin a commit SHA of the repo in the tool, the way `gen_shadow_rider.py` pins its source.
- **Approval:** João approves the exact URLs (repo + SHA + the 42 files) before anything is downloaded.
- **Storage:** fetched into a git-ignored folder, converted to frames with the palette applied and colours snapped to the 255's 64 colours, and written git-ignored.
- **Size:** shown at 2× (128×128), point-sampled. Whether the 2× happens in the tool or on the watch (`drawBitmap2` with a scale transform and point filtering, available on CIQ 5.2) is left to the builder, with memory in mind.
- **Animation:** on wake, the 2 frames alternate with Emerald's rhythm (about 2–4 Hz, builder's choice). The 10 s cap and the stop on hide and sleep stay. While asleep the face holds frame 0.
- **v1 leftovers:** the Black/White tool and the old frames go away, along with Body Battery-sized tail flames, the "z Z" and the darkened sleep frames. Night is now its own Pokémon.

## Layout "B, your Pokémon" (João's pick)

On a 260 px round screen, top to bottom:
1. **Big time** at the top, over the sky, in the Emerald font (large, at integer scale).
2. **Small lines** (1×): weekday + date, notifications, phone-connected mark and battery; weather icon, today's high/low, next sunrise/sunset. These can be one or two lines.
3. **Middle left:** the Pokémon on its oval battle platform, **mirrored to face its info box** *(Claude's call: `anim_front` faces left, and unmirrored on the left of the screen it would look at the bezel)*.
4. **Middle right:** an Emerald player-side info box, cream with a dark border and the game's corner shape. It holds:
   - the Pokémon's **NAME** in caps, at 2×;
   - **"Lv"** = the day of the month, at 1× *(Claude's guess, accepted)*;
   - an **HP bar** = Body Battery, green above 50 %, yellow 21–50 %, red at 20 % or below, like the game, with "BB nn" at 1× under it;
   - a blue **EXP bar** = steps against the goal, with the step count as a number at 1× under it *(answered: "the same data as v1" includes the step number)*.
5. **Bottom:** a dark-blue Emerald menu box with **four values at 2×**: heart rate, temperature, calories and distance.

**Text sizes (João's call):** the time is huge, the name and the bottom-box values are at 2×, and small labels (Lv, BB, steps, date line, weather line) are at 1×. Readability still matters (the v1 anti-goal stands), so check the 1× lines on the wrist.

The approved mockup, local and git-ignored, is `.superpowers/brainstorm/19703-1792765866/content/emerald-battle-v2.html` (card B). Exact positions are the builder's, within that arrangement, and must fit the round screen with nothing clipped.

## Background

- **Day:** Emerald's grass battle look, with a pale sky and a green field. Use the real battle background from the pokeemerald graphics *(Claude's call)* if it can be assembled into one image without much work. Otherwise draw it to match the game's colours. Either way it uses the 64-colour palette.
- **Night:** a darker version (night sky, dim field) *(Claude's guess, accepted)*, from sunset to sunrise.
- Any real game file used is fetched, approved and git-ignored like the sprites.

## Typography

- A **fan-made recreation of the Emerald font** (João's call). The builder finds a source and João approves it before the download.
- Convert it to a Connect IQ custom bitmap font (BMFont `.fnt` + `.png`) at its native pixel size, with a larger integer-scaled variant for the time if the font has no big digits. Keep it git-ignored, generated by the tool.
- Keep the glyph set small (digits, A–Z, a–z, ° / % : . – and a space) to save memory.

## Data

The data is the same as v1, from `Stats` and `Fmt`, and is still read once a minute and on wake. The only change is where it's shown. Body Battery feeds the HP bar instead of a flame.

## Left to the builder

- Whether to scale in the tool or on the watch, and how frames and fonts are packed to stay inside the 128 KB watch-face memory (measure in the simulator).
- How the background is built (from game files or drawn).
- Pixel positions, box shapes and colours within the mockup's arrangement.
- How to restructure `Partner` (the weekday table, day/night by sun, reward rule) while keeping it pure and unit-tested. v1's Mega/stage functions go.
- The simulator demo: step through the weekdays, day/night and the reward.

## As built (2026-10-23, after wearing it)

João tried each build on the watch and asked for changes. What ships differs from the design above as follows:

- **Font:** the game's own font sheet (pokeemerald-expansion `graphics/fonts/latin_normal.png`), not a fan TTF. FontStruct's "Pokémon Emerald Pro" needs an account to download. Like the sprites, the sheet is git-ignored.
- **Time and small lines:** the time is 3× (not 4×), and the small info runs on two 1× lines.
  - Line 1: phone dot · weekday + date · envelope + notifications · battery icon + %.
  - Line 2: weather · high/low · sunrise/sunset arrow + time.
  - The weekday comes from a fixed English table, because the Emerald font has no accented letters.
- **No calories.** The dialogue box shows heart rate + temperature, then distance, at 2×. A row too wide for the round edge drops to 1× (`Fit`). The info box shows "N steps" under the EXP bar.
- **Colours:** solid and vivid, with no dithering:
  - day: sky 55AAFF, horizon 005500, grass 00AA00;
  - night: 000055 / 005555 / 005500.
  - Text on the sky is black. The info box is cream with black text, the dialogue box is navy with white text, HP is 00FF00 / FFAA00 / FF0000, and EXP is 00AAFF.
  - The deck's pale battle colours, even dithered, washed out on the 255's reflective screen.
- **Boxes:** shaped like the deck's (`src/ui/battle.cpp`): a battle box with a 3 px shadow and a slanted side, and a framed dialogue box.
- **Animation:** the 2-frame idle runs at 2 Hz, 40 frames (about 10 s) at most. Frame 1 is clipped from the whole 2×-drawn sheet, rather than using `drawBitmap2`'s source rectangle, whose offset handling differs between devices.
- **Memory:** about 20 KB of 124 KB in the simulator (one sheet, one 2 bpp field, three fonts).
