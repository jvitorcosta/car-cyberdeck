# Car lights by voice (Siri Shortcuts)

The LED panel on the old deck (`pio run -e ledpanel -t upload`) takes spoken commands from an
iPhone Shortcut over the hotspot. Siri turns your voice into text, the Shortcut sends it to the
deck, the deck changes both lights and answers with one line that Siri speaks back.

- Address: on the iPhone hotspot the deck is always `172.20.10.11`.
- `GET /say?text=<phrase>&k=<REMOTE_TOKEN>` changes the lights; the reply is e.g. `Purple, 70 percent`.
- `GET /state?k=<REMOTE_TOKEN>` answers the same line without changing anything.
- A wrong key gets 403; a phrase with nothing to do gets "Sorry, I didn't get a colour or a command".
- Voice always changes both lights; the zone picked on the screen is left as it is.

## What it understands

English or Portuguese, any order, other words ignored (`src/util/ledvoice.h`):

| Kind | Words |
|---|---|
| Colours | red/vermelho, orange/laranja, pink/rosa, purple/roxo/lilás, blue/azul, green/verde, white/branco, yellow/amarelo (also Siri's "read" = red, "blew" = blue) |
| Power | on, off / liga, desliga, acende, apaga |
| Brighter / dimmer (10 % steps) | brighter, up, more / mais, aumenta, forte; dimmer, down, less / menos, diminui, fraco |
| Level | a number (`50`, `50%`, rounded to 10), max/máximo, min/mínimo |

A colour also turns the lights on.

## Shortcut 1: "Lights" (say anything)

In the Shortcuts app, new shortcut:

1. **Dictate Text**: language English (or Portuguese), stop listening *After Pause*.
2. **URL Encode** the Dictated Text.
3. **Text**: `http://172.20.10.11/say?k=YOUR_REMOTE_TOKEN&text=` followed by the URL Encoded Text
   variable.
4. **Get Contents of URL** of that Text (method GET).
5. **Speak Text**: Contents of URL (or *Show Result*, for a silent version).
6. Name it **Lights**.

"Hey Siri, lights" → Siri listens → "purple" → the lights turn purple and Siri says "Purple, 70
percent". The first run asks to allow connecting to 172.20.10.11: allow.

## Shortcut 2: one phrase, no question

For the commands used most, one shortcut each skips the "what?" step. Same as above without
steps 1-2, with the phrase in the URL, e.g. `...&text=purple`, named **Purple lights**: "Hey Siri,
purple lights". Likewise "Lights off", "Lights dimmer".

## Which language, and the name

Tested on 2026-10-21 with the shortcut named **Cyberpunk** ("Lights" was taken by HomeKit, and
English Siri misheard "Luzes"):

- **"Hey Siri, run Cyberpunk"**: Siri itself asks "What's the text?" and transcribes in *Siri's*
  language, ignoring Dictate Text's setting. With Siri in English, "roxo" arrived as "Who";
  "Purple" and "Red 50%" worked. So with Siri, say the English words.
- **Started without Siri** (tap, Back Tap, Action Button): Dictate Text uses its own language,
  so Portuguese works ("Roxo" arrived as "Roxo").
- "run" before the name helps Siri treat it as a shortcut.

## Notes

- The iPhone's Bluetooth must be off for the deck to hold the LED controllers, but the hotspot is
  WiFi, so the Shortcuts still reach the deck.
- At home on the router the PC couldn't reach the deck at all (it never showed in the PC's ARP
  table), so test over the hotspot.
- Next idea: when the deck answers "Sorry", the Shortcut asks Claude to turn the phrase into a
  command ("something cozy" → "purple 30") and sends that instead.
