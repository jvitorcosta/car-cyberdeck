"""Try a BLE OBD adapter from the PC, before the firmware meets it:
    python tools/obd_probe.py                 scan 10 s, list what could be an adapter
    python tools/obd_probe.py <name|address>  connect, print its layout, run the read-only checks

Prints every service and characteristic (the Vgate's layout is still unknown) and each command's raw
reply, and sets the adapter's ATRV beside the car's own PID 42 (a clone's ATRV can be far off; the
battery warnings trust it). With the adapter in the car, ignition on, and the phone's Bluetooth off:
the adapter takes one connection at a time. Read-only like the firmware: only the commands in
READ_ONLY are ever written, never mode 04. Needs bleak (tools/requirements.txt).
"""
import asyncio
import re
import sys

READ_ONLY = ("ATZ", "ATE0", "ATL0", "ATS0", "ATH0", "ATSP0", "ATI", "ATDPN", "ATRV",
             "0100", "0120", "0140", "010C", "010D", "0142", "03", "07")
NAME_HINTS = ("obd", "elm", "vlink", "v-link", "vgate", "icar", "konnwei")
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


async def scan():
    from bleak import BleakScanner

    found = await BleakScanner.discover(timeout=10, return_adv=True)
    for dev, adv in sorted(found.values(), key=lambda da: -da[1].rssi):
        name = adv.local_name or dev.name
        print("%s %s  %4d dBm  %s" % ("*" if looks_like_adapter(name) else " ", dev.address, adv.rssi, name or "?"))
        if adv.service_uuids:
            print("      services:", ", ".join(adv.service_uuids))


async def probe(target):
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
        volts = {}
        for cmd in READ_ONLY:
            buf.clear()
            prompt.clear()
            await client.write_gatt_char(picked[1], (cmd + "\r").encode("ascii"), response=False)
            try:
                await asyncio.wait_for(prompt.wait(), REPLY_S)
            except asyncio.TimeoutError:
                print("%-6s (no prompt in %.0f s)" % (cmd, REPLY_S))
            raw = "".join(buf).replace("\r", " ").replace(">", "").strip()
            print("%-6s %s" % (cmd, raw))
            volts[cmd] = raw
        print("\nbattery: ATRV %s V, PID 42 %s V (check both against a multimeter)" %
              (atrv_volts(volts["ATRV"]), pid42_volts(volts["0142"])))
        await client.stop_notify(picked[0])


def main():
    asyncio.run(probe(sys.argv[1]) if len(sys.argv) > 1 else scan())


if __name__ == "__main__":
    main()
