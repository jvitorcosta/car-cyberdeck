#!/usr/bin/env python3
"""Check that every src/util/**/*.h basename is named in docs/ARCHITECTURE.md.

Usage: python tools/check_architecture.py [--list]
Exit 0 when all are named, 1 with the missing names otherwise. --list prints the
modules per group in the Layout section's format (to regenerate its lists).
Not wired into CI: propose it first (see the PR).
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
UTIL = ROOT / "src" / "util"
DOC = ROOT / "docs" / "ARCHITECTURE.md"


def modules():
    groups = {}
    for h in sorted(UTIL.glob("*/*.h")):
        groups.setdefault(h.parent.name, []).append(h.stem)
    return groups


def main(argv):
    groups = modules()
    if "--list" in argv:
        for g, names in groups.items():
            print(f"{g}/: " + ", ".join(names))
        return 0
    text = DOC.read_text(encoding="utf-8")
    missing = [f"{g}/{n}" for g, names in groups.items() for n in names
               if not re.search(r"(?<![A-Za-z0-9_])" + re.escape(n) + r"(?![A-Za-z0-9_])", text)]
    for m in missing:
        print(f"not in docs/ARCHITECTURE.md: src/util/{m}.h")
    total = sum(len(v) for v in groups.values())
    print(f"{total - len(missing)}/{total} util modules named")
    return 1 if missing else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
