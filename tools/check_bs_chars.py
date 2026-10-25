#!/usr/bin/env python3
"""Verify bs_dados.h's character table against the game's own character JSON.

`bs_char_t` holds `offsets`, `cameraOffsets` and `singTime` for 46 characters.
Those three fields are what BaseCharacter uses to place the sprite and aim the
camera, so if the recovered table is wrong the characters will be drawn in the
wrong place with the camera following the wrong point -- which is exactly the
kind of error that survives a "looks about right" check on hardware.

Each character's JSON is in assets/data/characters/<id>.json. Fields the JSON
omits fall back to the defaults in CharacterData.hx:

    DEFAULT_OFFSETS    = [0, 0]      (used for both offsets and cameraOffsets)
    DEFAULT_SINGTIME   = 8.0         (in steps, not seconds)
    DEFAULT_DANCEEVERY = 1.0

Run from the project root:  python3 tools/check_bs_chars.py
"""
import glob
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.dirname(HERE)
CHARS = os.path.join(PROJECT, "assets-data", "characters")
BS = os.path.join(PROJECT, "src", "bs_dados.h")

# CharacterData.hx, verbatim.
DEFAULT_OFFSETS = [0.0, 0.0]
DEFAULT_SINGTIME = 8.0


def parse_bs_chars():
    src = open(BS).read()
    m = re.search(r"static const bs_char_t bs_chars\[\]=\{(.*?)\};", src, re.S)
    if not m:
        sys.exit("could not find bs_chars[] in bs_dados.h")
    out = {}
    pat = (r'\{\s*"([^"]+)"\s*,\s*'
           r"(-?[0-9.]+)f\s*,\s*(-?[0-9.]+)f\s*,\s*"
           r"(-?[0-9.]+)f\s*,\s*(-?[0-9.]+)f\s*,\s*"
           r"(-?[0-9.]+)f\s*\}")
    for cid, ox, oy, cx, cy, sing in re.findall(pat, m.group(1)):
        out[cid] = (float(ox), float(oy), float(cx), float(cy), float(sing))
    return out


def want_from_json(d):
    def vec(key, default):
        v = d.get(key)
        if v is None:
            return default
        if len(v) != 2:
            return default
        return [float(v[0]), float(v[1])]
    offsets = vec("offsets", DEFAULT_OFFSETS)
    cam = vec("cameraOffsets", DEFAULT_OFFSETS)
    sing = d.get("singTime")
    sing = float(sing) if sing is not None else DEFAULT_SINGTIME
    return offsets[0], offsets[1], cam[0], cam[1], sing


def main():
    bs = parse_bs_chars()
    files = {}
    for f in sorted(glob.glob(os.path.join(CHARS, "*.json"))):
        cid = os.path.basename(f)[:-5]
        try:
            files[cid] = json.load(open(f))
        except Exception as e:
            print("  could not parse %s: %s" % (f, e))
    print("bs_dados.h: %d characters, JSON: %d files" % (len(bs), len(files)))
    print()

    bad = 0
    checked = 0
    for cid in sorted(files):
        want = want_from_json(files[cid])
        if cid not in bs:
            print("%-22s MISSING from bs_dados.h" % cid)
            bad += 1
            continue
        got = bs[cid]
        checked += 1
        if any(abs(a - b) > 1e-6 for a, b in zip(got, want)):
            bad += 1
            print("%-22s" % cid)
            for i, name in enumerate(("offsets.x", "offsets.y",
                                      "cameraOffsets.x", "cameraOffsets.y",
                                      "singTime")):
                if abs(got[i] - want[i]) > 1e-6:
                    print("    %-18s bs=%-10g json=%-10g" % (name, got[i], want[i]))
    print()
    print("in bs_dados.h but not downloaded: %d"
          % len([k for k in bs if k not in files]))
    if len(files) != len(bs):
        missing = sorted(set(bs) - set(files))
        extra = sorted(set(files) - set(bs))
        if missing:
            print("  not downloaded: %s" % ", ".join(missing))
        if extra:
            print("  JSON with no bs_dados.h row: %s" % ", ".join(extra))
    print("checked %d, mismatches: %d" % (checked, bad))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
