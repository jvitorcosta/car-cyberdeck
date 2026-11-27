"""Copy the media from data/ to the Pokédeck's microSD card, into its "cardeck" folder.

  python tools/sd_media.py E:\\                 (the card's drive, in the PC's reader)
  python tools/sd_media.py E:\\ --data <folder>  (the media from another checkout)

Copies the files the black deck packs into its media partition (pack_media.FILES), skipping those
already on the card unchanged. Writes nothing else: no other file on the card is touched, and the
card is never formatted (it must be FAT32). Refuses the whole copy when any file is a stand-in
(tools/media_sizes.py placeholders) or lacks its header.
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pack_media as pm  # noqa: E402

FOLDER = "cardeck"   # src/pokedeck/media/sdcard.cpp reads from here


class Refused(Exception):
    pass


def read(path):
    with open(path, "rb") as f:
        return f.read()


def media(data):
    """[(name, bytes)] of the real media in `data`; Refused when one is a stand-in or none is there."""
    files, bad = [], []
    for name in pm.FILES:
        path = os.path.join(data, name)
        if not os.path.isfile(path) or os.path.getsize(path) <= pm.STAND_IN:
            continue
        blob = read(path)
        problem = pm.stand_in_problem(name, blob)
        if problem:
            bad.append(problem)
        files.append((name, blob))
    if bad:
        raise Refused("stand-in media, nothing copied: " + "; ".join(bad))
    if not files:
        raise Refused("no media in " + data)
    return files


def missing(data):
    """The media the black deck packs that `data` doesn't have (stand-ins of a few bytes included)."""
    paths = ((n, os.path.join(data, n)) for n in pm.FILES)
    return [n for n, p in paths if not os.path.isfile(p) or os.path.getsize(p) <= pm.STAND_IN]


def root(card):
    """"E:" is the current folder on E:, not its root."""
    return card + os.sep if card.endswith(":") else card


def write(path, blob):
    """Through a .tmp beside it, flushed to the card: a pulled card leaves the old file whole."""
    tmp = path + ".tmp"
    try:
        with open(tmp, "wb") as f:
            f.write(blob)
            f.flush()
            os.fsync(f.fileno())
        os.replace(tmp, path)
    finally:
        if os.path.exists(tmp):
            os.remove(tmp)


def copy(data, card):
    """Copies data's media to card/FOLDER. Returns (copied names, names already there)."""
    if not os.path.isdir(card):
        raise Refused("no card at " + card)
    files = media(data)
    folder = os.path.join(card, FOLDER)
    copied, kept = [], []
    try:
        os.makedirs(folder, exist_ok=True)
        for name, blob in files:
            path = os.path.join(folder, name)
            if os.path.isfile(path) and os.path.getsize(path) == len(blob) and read(path) == blob:
                kept.append(name)
                continue
            write(path, blob)
            copied.append(name)
    except OSError as e:
        raise Refused("the card stopped taking files (%s): run it again" % e)
    return copied, kept


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("card", help="the card's drive or folder")
    ap.add_argument("--data", default=pm.DATA, help="where the media are (default: data/)")
    args = ap.parse_args(argv)
    card = root(args.card)
    try:
        copied, kept = copy(args.data, card)
    except Refused as e:
        sys.exit("sd_media: " + str(e))
    for name in missing(args.data):
        print("sd_media: no %s in %s, left out" % (name, args.data))
    for name in copied:
        print("sd_media: copied " + name)
    print("sd_media: %d copied, %d already there, in %s" % (len(copied), len(kept), os.path.join(card, FOLDER)))


if __name__ == "__main__":
    main()
