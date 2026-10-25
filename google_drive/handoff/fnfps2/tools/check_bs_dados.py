#!/usr/bin/env python3
"""Verify bs_dados.h's stage table against the game's own stage JSON.

bs_dados.h was extracted from the official v0.8.6 data, so it should agree
exactly with assets/data/stages/*.json. This proves it does -- or names the
stage that does not, which is the difference between trusting a recovered
table and knowing it is right.

Also reports what bs_dados.h does NOT carry: the stage props (position, scale,
scroll factor, zIndex) and the per-character cameraOffsets. main.c hardcodes
those (STAGE_SCROLL, the -18/+36 stage camera shift), so the gaps are listed
rather than left implicit.
"""
import glob
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.dirname(HERE)
STAGES = os.path.join(PROJECT, "assets-data", "stages")
BS = os.path.join(PROJECT, "src", "bs_dados.h")


def parse_bs_stages():
    src = open(BS).read()
    m = re.search(r"static const bs_stage_t bs_stages\[\]=\{(.*?)\};", src, re.S)
    if not m:
        sys.exit("could not find bs_stages[] in bs_dados.h")
    rows = {}
    pat = (r'\{\s*"([^"]+)"\s*,\s*([0-9.]+)f\s*,'
           r"\s*\{([^}]*)\}\s*,\s*\{([^}]*)\}\s*,\s*\{([^}]*)\}\s*\}")
    for sid, zoom, bf, gf, dad in re.findall(pat, m.group(1)):
        def nums(s):
            return [float(x.replace("f", "")) for x in s.split(",")]
        rows[sid] = (float(zoom), nums(bf), nums(gf), nums(dad))
    return rows


def load_json_stages():
    out = {}
    for f in sorted(glob.glob(os.path.join(STAGES, "*.json"))):
        d = json.load(open(f))
        sid = os.path.basename(f)[:-5]
        ch = d.get("characters") or {}
        out[sid] = {
            "zoom": d.get("cameraZoom"),
            "bf": (ch.get("bf") or {}).get("position"),
            "gf": (ch.get("gf") or {}).get("position"),
            "dad": (ch.get("dad") or {}).get("position"),
            "props": d.get("props") or [],
            "name": d.get("name"),
        }
    return out


def main():
    bs = parse_bs_stages()
    js = load_json_stages()
    print("bs_dados.h: %d stages, JSON: %d stages" % (len(bs), len(js)))
    print()
    print("%-24s %-6s %-5s %-20s %-20s %-20s %s"
          % ("stage", "zoom", "prop", "bf", "gf", "dad", "verdict"))
    print("-" * 112)

    bad = 0
    for sid in sorted(js):
        s = js[sid]
        if sid not in bs:
            print("%-24s MISSING from bs_dados.h" % sid)
            bad += 1
            continue
        z, bf, gf, dad = bs[sid]
        probs = []
        if s["zoom"] is not None and abs(s["zoom"] - z) > 1e-6:
            probs.append("zoom %s vs %s" % (s["zoom"], z))
        for key, want in (("bf", bf), ("gf", gf), ("dad", dad)):
            got = s[key]
            if got is None:
                if want != [0.0, 0.0]:
                    probs.append("%s absent (bs has %s)" % (key, want))
            elif (len(got) != 2 or abs(got[0] - want[0]) > 1e-6
                  or abs(got[1] - want[1]) > 1e-6):
                probs.append("%s %s vs %s" % (key, got, want))
        if probs:
            bad += 1
        print("%-24s %-6s %-5d %-20s %-20s %-20s %s"
              % (sid, s["zoom"], len(s["props"]), str(s["bf"]), str(s["gf"]),
                 str(s["dad"]), "OK" if not probs else "; ".join(probs)))

    print()
    print("in bs_dados.h but not in the JSON: %s" % sorted(set(bs) - set(js)))
    print("mismatches: %d of %d" % (bad, len(js)))

    # what the recovered table does not carry
    print()
    print("what bs_dados.h does not carry:")
    n_props = sum(len(s["props"]) for s in js.values())
    print("  stage props (position/scale/scroll/zIndex): %d across %d stages"
          % (n_props, len(js)))
    # cameraOffsets live on the character entries, which are dicts; guard
    # against a stage that stores a list there instead.
    def has_off(stage, key):
        c = stage.get(key)
        return isinstance(c, dict) and bool(c.get("cameraOffsets"))

    with_off = [k for k, s in js.items()
                if any(has_off(s, k2) for k2 in ("bf", "gf", "dad"))]
    print("  stages whose characters have cameraOffsets: %d" % len(with_off))
    if with_off:
        s = js[with_off[0]]
        for k2 in ("bf", "gf", "dad"):
            c = s.get(k2) or {}
            if isinstance(c, dict) and c.get("cameraOffsets"):
                print("    e.g. %s.%s.cameraOffsets = %s"
                      % (with_off[0], k2, c["cameraOffsets"]))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
