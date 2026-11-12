# PS3 controller redesign (yellow deck)

## Why

The DualShock 3 on the yellow deck acts on input nobody meant. Setting it down on a table or
picking it up changes the preset, the brightness, even turns karaoke on or off, and it buzzes while
doing it. The owner also wants the three things he reaches for while driving (the lights, music mode
and karaoke) to take one press each, and to see both light levels on the BOTH page.

## What the pad does wrong today (measured 2026-11-12)

Logged on the desk with the normal firmware and with a raw-data diagnostic build:

- **A set-down presses many buttons lightly at once.** Lying still on the table, X, O, Square,
  Triangle, R1 and R2 all reported their on/off bit for ~1.7 s at pressures of only 13-30 (of 255).
- **Resting thumbs register.** Held still in the hand, the D-pad reported down/left/right at
  pressures 9-166.
- **Real presses are firm.** Deliberate L2/R2 presses read 244-255 (one lighter pair, 133 and
  176, may have been a bump).
- **A bit can stay on for minutes.** In one run the D-pad's "up" stayed on for ~100 s with the
  pad lying face up and untouched.
- **Motion is clean.** The accelerometer moves 1-3 units (raw /8) at rest and 10-20 per 100 ms
  while the pad is picked up or put down; its Z axis tells hand from table.
- Today the deck acts on a button's bit (the DS3 sets it at the lightest touch), on L2/R2 past 120,
  and the shortcuts buzz on every press they act on.

## Goals

1. A press counts only when it was meant. Set-downs, bumps and resting thumbs do nothing and never
   buzz.
2. Lights, music mode and karaoke each one press from any screen.
3. Colours without opening the menus (about 40 % of light changes; presets are the other 60 %).
4. Feel what happened without looking: each kind of action buzzes its own way.
5. The BOTH page shows both levels when the lights differ.

## Non-goals

- No lock mode, no long-press or chord features, no motion gestures beyond the guard.
- No change to pairing, the battery icon or the pad's LEDs.
- The black deck and the Xbox code are out of scope.

## Anti-goals (a failure even if it works)

- Presses that feel slow or need a harder push than a normal game does.
- A guard so eager that a quick deliberate press after picking the pad up is lost more than rarely.

## The guard (agreed)

Applied to every button before the mapping. Starting values below are the builder's to tune on the
desk; the rules are agreed.

1. **A minimum push.** A pressure-sensitive button (D-pad, X, O, Square, Triangle, L1, R1) counts
   once its pressure passes about a quarter of travel (start: 64); L2/R2 need about 200 (today
   120). Down past the line, up again only well below it (hysteresis, like `padtrigger`).
   START, SELECT, PS, L3 and R3 have no pressure; they stay on their bit, but rules 2-4 apply.
2. **Several at once count as nothing.** While 3 or more buttons have their on/off bit set in the
   same report (however light), no new press acts; presses act again once fewer than 3 bits are
   on. No press is held back waiting for others: the measured set-down sets all its bits in one
   report, so the check is per report and costs no delay. (Builder's call, from the review: it
   counts bits, not presses past rule 1, or rule 2 would never see a set-down; "fewer than 3"
   rather than "all released", so one stuck bit can't lock the pad.)
3. **A pause after handling.** The pad counts as *resting* once the accelerometer has stayed
   within a few units for about a second, which happens on a table, not in a hand. When it leaves
   rest (picked up), presses are ignored for about half a second; the same on arriving at rest
   (put down). Plain hand or road motion never triggers it. (Builder's call; to be checked in the
   car, since all recordings so far are from the desk.)
4. **Stuck buttons.** A button held for several seconds (start: 5 s) is ignored until released.
   The D-pad's hold-to-repeat keeps working within that limit.
5. **Buzz only for actions that went through.**

Presses still act the moment they pass the line (not on release), so the pad feels instant.

## The mapping (agreed)

| Button | Does | Change from today |
|---|---|---|
| L2 / R2 | previous / next preset, any screen | guard only |
| L1 / R1 | both lights dimmer / brighter, any screen | now the same on the light pages too |
| START | lights on / off | none |
| SELECT | music mode on / off | none |
| Triangle | karaoke on / off, any screen | new; it no longer toggles one light page |
| Right stick flick left / right | previous / next colour on both lights, any screen | new |
| PS | back to the Menu | none |
| D-pad, X, O | move, press, back | none |
| Square | next tab on the light pages | none |

- **The flick** counts only near the edge of the stick's travel (start: 70 % of full scale), and
  the stick must come back near centre before the next step, so drift or a light touch does
  nothing. It steps through the colour swatches in their screen order and wraps around.
- **The flick changes only the colour** (owner's choice): each light keeps its own level and
  on/off, so after a split preset (say IN 60 %, OUT 40 %) both lights take the new colour and keep
  60 % and 40 %; a light that was off stays off. That needs a colour-only change next to
  `bothlook`'s whole-look copy. When the two colours differ, it steps from the inside light's
  colour. From a preset-only colour (cyan), it steps to the first grid colour (right) or the last
  (left). (Builder's call on the last two.)
- **Colours, brightness and presets end music mode,** like any manual change today.

## The feel (agreed)

The DS3 has a small motor that is only on or off and a big one with a strength. Today both get the
same value. Proposed, each with a softer version for turning something off:

| Action | Feel |
|---|---|
| Preset step, brightness step, colour step | one short tick |
| Music mode | two ticks |
| Karaoke | one long buzz |
| Lights on / off | one firm buzz |

Karaoke buzzes on the press, like every other shortcut; the screen already says when the black
deck doesn't answer, so there is no extra "not answering" buzz. (Builder's call.)

The existing rule stays: an action that went to a light that isn't linked buzzes twice, so the
builder must keep the two meanings apart (for example the partial-link double uses the big motor
and the music double the small one). This is the builder's call; it must be told apart by hand.

## The BOTH page (agreed)

While the inside and outside lights differ, the level line shows `IN 60%  OUT 40%` (or
`IN OFF  OUT 80%`) in place of `MIX`. The bar shows the inside level. When they match it shows one
`60%` as today. It must fit the level rect between the - and + buttons; a smaller face is fine.

## Constraints

- The yellow deck runs Bluedroid in dual mode with ~30 KB heap low water; no new tasks or large
  buffers. Rumble and LEDs go through `padhid` as today.
- Pure logic (guard, flick, feel patterns) goes in `src/util/yellow/` headers with host tests, like
  `padtrigger`, `padfeel` and `padbattery`.
- Flashing the yellow deck is over USB only.

## Left to the builder

The exact thresholds and windows, how the guard reads the peak pressure between loop passes, the
rumble durations and strengths, and the layout of the BOTH level line. Each threshold should be
checked against a desk recording like the one above.
