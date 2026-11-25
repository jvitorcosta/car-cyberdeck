"""Try a BLE OBD adapter from the PC, before the firmware meets it:
    python tools/obd_probe.py                 scan 10 s, list what could be an adapter
    python tools/obd_probe.py <name|address>  connect, print its layout, run the read-only checks
    python tools/obd_probe.py <name|address> --fuel
                                              the same, then ask the meter ECU for the fuel level

Prints every service and characteristic (the Vgate's layout is still unknown) and each command's raw
reply, and sets the adapter's ATRV beside the car's own PID 42 (a clone's ATRV can be far off; the
battery warnings trust it). With the adapter in the car, ignition on, and the phone's Bluetooth off:
the adapter takes one connection at a time. Read-only like the firmware: only the commands in
READ_ONLY (plus FUEL_PROBE under --fuel) are ever written, never mode 04. Needs bleak
(tools/requirements.txt).

--fuel is a research probe, off by default and NOT in the decks' gate: it tries the one Honda read OBDb lists
for the City's tank, Mode 22 DID 7029 on the meter ECU ("ATCP 18", "ATSH DA60F1", "227029"; litres = two
bytes / 100). It only runs when ATDPN says the bus is 29-bit CAN, prints the raw reply and both ways to
read the bytes (OBDb's bit offset is ambiguous about the 3-byte header), and ends with ATZ so the adapter
forgets the header. Compare the litres with the car's fuel bars before anything goes into firmware
(.claude/skills/obd2: hard rule 4).
"""
import asyncio
import re
import sys

READ_ONLY = ("ATZ", "ATE0", "ATL0", "ATS0", "ATH0", "ATSP0", "ATI", "ATDPN", "ATRV",
             "0100", "0120", "0140", "010C", "010D", "0142", "03", "07")
NAME_HINTS = ("obd", "elm", "vlink", "v-link", "vgate", "icar", "konnwei")
# The fuel-level research probe (--fuel): exact requests, nothing else. The meter ECU's physical address on a
# 29-bit bus is DA60F1; ATCP 18 is the 29-bit priority byte; ATZ ends it so the adapter drops the header.
FUEL_PROBE = ("ATCP 18", "ATSH DA60F1", "227029", "ATZ")
FUEL_DID = (0x62, 0x70, 0x29)
TANK_L = 44.0
REPLY_S = 6.0


def looks_like_adapter(name):
    n = (name or "").lower()
    return any(h in n for h in NAME_HINTS)


def pick_chars(chars):
    """(notify uuid, write uuid) from [(uuid, properties), ...] of one service; None if it lacks one."""
    notify = next((u for u, p in chars if "notify" in p), None)
    write = next((u for u, p in chars if "write" in p or "write-without-response" in p), None)
    return (notify, write) if notify and write else None


def pick_service(services):
    """[(service uuid, chars), ...] -> the serial service's (notify, write): FFF0 / FFE0 first."""
    def rank(item):
        short = item[0].lower()[4:8]
        return 0 if short in ("fff0", "ffe0") else 1

    for _uuid, chars in sorted(services, key=rank):
        picked = pick_chars(chars)
        if picked:
            return picked
    return None


def atrv_volts(raw):
    m = re.search(r"(\d{1,2}(?:\.\d+)?)\s*V", raw)
    return float(m.group(1)) if m else None


def pid42_volts(raw):
    """Control module voltage from a 0142 reply ("41 42 37 FF" -> 14.335); None without one."""
    m = re.search(r"41\s*42\s*([0-9A-Fa-f]{2})\s*([0-9A-Fa-f]{2})", raw)
    return (int(m.group(1), 16) * 256 + int(m.group(2), 16)) / 1000.0 if m else None


def reply_bytes(raw):
    """Hex bytes of an ELM reply, multi-frame lines ('0: 62 70 ..') and the 3-digit length line dropped."""
    out = []
    for line in re.split(r"[\r\n]+", raw):
        line = re.sub(r"^\s*[0-9A-Fa-f]{1,2}:\s*", "", line).strip()
        if not line or re.fullmatch(r"[0-9A-Fa-f]{3}", line):
            continue
        for tok in line.split():
            if re.fullmatch(r"[0-9A-Fa-f]{2}", tok):
                out.append(int(tok, 16))
    return out


def decode_fuel(raw):
    """A 227029 reply -> {'bytes', 'with_header', 'after_header'}: litres each way (OBDb's bix 96 is bytes 12-13
    of the data after '62 70 29', or of the whole reply; the probe shows both), None when the reply is short.
    {} when there is no 62 70 29 in it (NO DATA, a negative answer, junk)."""
    data = reply_bytes(raw)
    for i in range(len(data) - 2):
        if tuple(data[i:i + 3]) == FUEL_DID:
            frame = data[i:]
            body = frame[3:]

            def litres(buf, at):
                return None if len(buf) < at + 2 else (buf[at] * 256 + buf[at + 1]) / 100.0

            return {"bytes": frame, "with_header": litres(frame, 12), "after_header": litres(body, 12)}
    return {}


def plausible_litres(v):
    return v is not None and 0.0 <= v <= TANK_L + 6


def is_29bit(atdpn_raw):
    """ATDPN answers 'A7' (auto, protocol 7) or '7': ISO 15765-4 CAN 29-bit 500 kbit/s. 'A0' means the
    adapter has not found a bus yet (ask again after a mode 01 request); 6 is the 11-bit 500 kbit/s bus."""
    m = re.search(r"(?:^|[^0-9A-Za-z])A?([0-9A-C])(?:$|[^0-9A-Za-z])", " " + atdpn_raw.strip() + " ")
    return bool(m) and m.group(1) == "7"


def check_allowed(cmd, fuel):
    """The probe's one gate: a command may be written only if it is in READ_ONLY, or in FUEL_PROBE under --fuel."""
    if cmd not in READ_ONLY + (FUEL_PROBE if fuel else ()):
        raise SystemExit("refusing %r: not in the read-only list" % cmd)


async def scan():
    from bleak import BleakScanner

    found = await BleakScanner.discover(timeout=10, return_adv=True)
    for dev, adv in sorted(found.values(), key=lambda da: -da[1].rssi):
        name = adv.local_name or dev.name
        print("%s %s  %4d dBm  %s" % ("*" if looks_like_adapter(name) else " ", dev.address, adv.rssi, name or "?"))
        if adv.service_uuids:
            print("      services:", ", ".join(adv.service_uuids))


async def probe(target, fuel=False):
    from bleak import BleakClient, BleakScanner

    dev = await BleakScanner.find_device_by_filter(
        lambda d, a: target.lower() in ((a.local_name or d.name or "").lower(), d.address.lower()) or
        target.lower() in (a.local_name or d.name or "").lower(), timeout=15)
    if not dev:
        raise SystemExit("%s not found: adapter powered? phone's Bluetooth off?" % target)
    async with BleakClient(dev, timeout=20) as client:
        services = []
        for service in client.services:
            print("[service] %s" % service.uuid)
            chars = [(c.uuid, list(c.properties)) for c in service.characteristics]
            for uuid, props in chars:
                print("  [char] %s  (%s)" % (uuid, ",".join(props)))
            services.append((service.uuid, chars))
        picked = pick_service(services)
        if not picked:
            raise SystemExit("no service with both a notify and a write characteristic")
        print("using notify %s, write %s\n" % picked)
        buf, prompt = [], asyncio.Event()

        def on_data(_sender, data):
            text = bytes(data).decode("ascii", errors="replace")
            buf.append(text)
            if ">" in text:
                prompt.set()

        await client.start_notify(picked[0], on_data)

        async def ask(cmd):
            check_allowed(cmd, fuel)   # the one gate: nothing else is ever written
            buf.clear()
            prompt.clear()
            await client.write_gatt_char(picked[1], (cmd + "\r").encode("ascii"), response=False)
            try:
                await asyncio.wait_for(prompt.wait(), REPLY_S)
            except asyncio.TimeoutError:
                print("%-12s (no prompt in %.0f s)" % (cmd, REPLY_S))
            raw = "".join(buf).replace(">", "").strip()
            print("%-12s %s" % (cmd, raw.replace("\r", " ")))
            return raw

        volts = {}
        for cmd in READ_ONLY:
            volts[cmd] = await ask(cmd)
        print("\nbattery: ATRV %s V, PID 42 %s V (check both against a multimeter)" %
              (atrv_volts(volts["ATRV"]), pid42_volts(volts["0142"])))
        if fuel:
            # the first ATDPN came before any bus traffic (A0 = still searching): ask again now that 0100 ran
            proto = await ask("ATDPN")
            if not is_29bit(proto):
                print("\nfuel probe skipped: ATDPN %r is not 29-bit CAN 500 kbit/s (expected A7)" % proto)
            else:
                print("\n-- fuel level probe (Mode 22 DID 7029, meter ECU DA60) --")
                raw = ""
                try:
                    for cmd in FUEL_PROBE[:-1]:
                        out = await ask(cmd)
                        if cmd == "227029":
                            raw = out
                finally:
                    await ask("ATZ")   # always: the adapter must not keep the ATSH / ATCP header
                found = decode_fuel(raw)
                if not found:
                    print("no 62 70 29 in the reply: the meter ECU did not answer this request (that is the result)")
                else:
                    print("reply bytes: %s" % " ".join("%02X" % b for b in found["bytes"]))
                    for label, key in (("bytes 12-13 after the 3-byte header", "after_header"),
                                       ("bytes 12-13 of the whole reply", "with_header")):
                        v = found[key]
                        note = "(plausible for a %.0f L tank)" % TANK_L if plausible_litres(v) else ""
                        print("  %-38s %s L %s" % (label, "n/a" if v is None else "%.2f" % v, note))
                    print("compare with the cluster's fuel bars, then send this output back")
        await client.stop_notify(picked[0])


def main():
    args = [a for a in sys.argv[1:] if a != "--fuel"]
    fuel = "--fuel" in sys.argv[1:]
    if fuel and not args:
        raise SystemExit("--fuel needs the adapter's name or address")
    asyncio.run(probe(args[0], fuel) if args else scan())


if __name__ == "__main__":
    main()
