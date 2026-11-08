"""Build data/camvoice.bin: the camera alerts' chime and voice lines (src/util/car/camclip.h).
Run: python tools/gen_camvoice.py <dir with the 16 kHz WAVs> [out, default data/camvoice.bin]

The pon chime and the GPS clip of the greeting set, plus amitaro's radar-detector lines
(voice: あみたろの声素材工房), each a 16 kHz mono 16-bit WAV named as in SLOTS. A missing WAV is an
empty slot (the deck beeps for it, or says nothing). data/camvoice.bin is git-ignored; it goes to
the deck in the media pack (tools/pack_media.py).

Format: "CAMV", u16 version, u16 slot count; per slot u32 offset from the file's start and u32
size in bytes (0 = empty); then the clips, s16le PCM. Little-endian.
"""
import os
import struct
import sys
import wave

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
OUT = os.path.normpath(os.path.join(ROOT, "data", "camvoice.bin"))
RATE = 16000
MAGIC, VERSION = b"CAMV", 1
HEAD, ENTRY = 8, 8
# In camclip::Slot's order: (slot, source WAV).
SLOTS = [
    ("PON", "chimevoice_chime_pon_16k.wav"),
    ("GPS_FIRST", "chimevoice_gps_16k.wav"),
    ("TORISHIMARI", "cam_torishimari_02_16k.wav"),
    ("GOCHUI", "cam_gochuuikudasai_01_16k.wav"),
    ("SPD_CHANGE", "cam_spd-change_01_16k.wav"),
    ("SPEEDNI", "cam_speednikiwo_01_16k.wav"),
    ("GPS_NG", "cam_gps-ng_01_16k.wav"),
    ("GPS_OK", "cam_gps-ok_02_16k.wav"),
] + [("L%d" % n, "cam_speed%dika_01_16k.wav" % n) for n in (20, 30, 40, 50, 60, 80)]


def read_wav(path):
    """The WAV's PCM bytes; exits unless it is 16 kHz mono 16-bit."""
    try:
        with wave.open(path, "rb") as w:
            fmt = (w.getframerate(), w.getnchannels(), w.getsampwidth())
            if fmt != (RATE, 1, 2):
                sys.exit("gen_camvoice: %s is %d Hz, %d channel(s), %d-bit: want 16 kHz mono 16-bit"
                         % (os.path.basename(path), fmt[0], fmt[1], fmt[2] * 8))
            return w.readframes(w.getnframes())
    except (wave.Error, EOFError) as e:
        sys.exit("gen_camvoice: %s is not a WAV this reads (%s)" % (os.path.basename(path), e))


def build(clips):
    """clips: one PCM bytes (b"" = empty) per slot, in SLOTS' order -> the file's bytes."""
    off = HEAD + ENTRY * len(clips)
    table, body = b"", b""
    for pcm in clips:
        table += struct.pack("<II", off + len(body) if pcm else 0, len(pcm))
        body += pcm
    return MAGIC + struct.pack("<HH", VERSION, len(clips)) + table + body


def main():
    if len(sys.argv) < 2 or len(sys.argv) > 3:
        sys.exit(__doc__.splitlines()[1])
    src = sys.argv[1]
    out = sys.argv[2] if len(sys.argv) > 2 else OUT
    if not os.path.isdir(src):
        sys.exit("gen_camvoice: no such directory " + src)
    clips = []
    for slot, name in SLOTS:
        path = os.path.join(src, name)
        if os.path.isfile(path):
            clips.append(read_wav(path))
        else:
            print("gen_camvoice: no %s, %s left empty" % (name, slot))
            clips.append(b"")
    blob = build(clips)
    os.makedirs(os.path.dirname(os.path.abspath(out)), exist_ok=True)
    with open(out, "wb") as f:
        f.write(blob)
    full = sum(1 for c in clips if c)
    secs = sum(len(c) for c in clips) / 2 / RATE
    print("gen_camvoice: %d of %d slots, %.1f s of sound, %d B -> %s"
          % (full, len(SLOTS), secs, len(blob), out))


if __name__ == "__main__":
    main()
