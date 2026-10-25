#!/usr/bin/env python3
"""Per-stage VRAM budget from the real image dimensions.

The port draws one stage at a time, so the question is not how big the whole
game is but how much texture memory one scene needs: its background props, the
three characters, the notes, the HUD. Everything is uploaded as 32-bit
PSMCT32, so a texture costs w*h*4 bytes whatever the PNG compressed to.

assets-data/stage-image-sizes.json holds the measured dimensions, taken from
the Drive mirror's imageMediaMetadata. An entry that is null is a gap, and it
is reported as one -- an invented size would make the budget look settled when
it is not.

Run from the project root: python3 tools/vram_budget.py
"""
import glob
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.dirname(HERE)
STAGES = os.path.join(PROJECT, "assets-data", "stages")
SIZES = os.path.join(PROJECT, "assets-data", "stage-image-sizes.json")

# SCPH-90006, measured earlier in the port.
VRAM_TOTAL = 4194304
FRAMEBUFFERS = 2293760          # 2 x 640x448 x 4 B
TEXTURE_BUDGET = VRAM_TOTAL - FRAMEBUFFERS

# What the port needs alongside the stage, taken from src/main.c as it stands.
# These are not estimates: each one is a size the port actually uploads.
CHARACTER_VRAM = 164 * 165 * 4 + 170 * 157 * 4   # BF and GF frame buffers
HUD_VRAM = 320 * 307 * 2                          # g_gftex16, GS_PSM_16
TITLE_VRAM = (320 * 240 + 256 * 235 + 320 * 192) * 4
FONT_VRAM = 384 * 192 * 4                         # the font atlas
OTHER = 256 * 128 * 4                             # note splashes, cursors

NON_STAGE = CHARACTER_VRAM + HUD_VRAM + TITLE_VRAM + FONT_VRAM + OTHER

# src/main.c line 1153 reports the stage texture as 512x256 -- the port draws
# every stage into that one buffer, whatever the original's pixel size.
STAGE_TEXTURE_W, STAGE_TEXTURE_H = 512, 256
STAGE_TEXTURE_VRAM = STAGE_TEXTURE_W * STAGE_TEXTURE_H * 4


def stage_cost(stage, sizes):
    """-> (total_bytes, known_props, unknown_props)"""
    total = 0
    known = 0
    unknown = 0
    seen = set()
    for p in stage.get("props") or []:
        path = p.get("assetPath")
        if not path or path.startswith("#"):
            continue          # a flat colour, not a texture
        if path in seen:
            continue          # already charged: one upload serves every reference
        seen.add(path)
        e = sizes.get(path)
        if not e or e.get("w") is None or e.get("h") is None:
            unknown += 1
            continue
        w, h = e["w"], e["h"]
        # The PS2 samples the texture once per uploaded pixel, so a sprite's
        # display scale does not change the cost: a 3591x1538 backdrop drawn at
        # scale 6 still occupies 3591x1538 in VRAM, it is merely stretched over
        # a bigger quad. Round each axis up to a power of two, which is what the
        # GS wants, and charge 4 bytes per pixel.
        tw = next_pow2(w)
        th = next_pow2(h)
        total += tw * th * 4
        known += 1
    return total, known, unknown


def next_pow2(n):
    p = 1
    while p < n:
        p <<= 1
    return p


def main():
    sizes = json.load(open(SIZES))["images"]
    files = sorted(glob.glob(os.path.join(STAGES, "*.json")))

    rows = []
    for f in files:
        d = json.load(open(f))
        sid = os.path.basename(f)[:-5]
        cost, known, unknown = stage_cost(d, sizes)
        rows.append((sid, cost, known, unknown, d.get("cameraZoom")))

    rows.sort(key=lambda r: -r[1])

    print("SCPH-90006: %d B VRAM, %d B for two framebuffers, %d B for textures"
          % (VRAM_TOTAL, FRAMEBUFFERS, TEXTURE_BUDGET))
    print("the port already uploads %d B of textures outside the stage" % NON_STAGE)
    print("so a stage may cost at most %d B" % (TEXTURE_BUDGET - NON_STAGE))
    print()
    print("%-24s %14s %9s %6s %6s  %s"
          % ("stage", "stage VRAM", "share", "props", "gaps", "against the budget"))
    print("-" * 84)
    limit = TEXTURE_BUDGET - NON_STAGE
    for sid, cost, known, unknown, zoom in rows:
        pct = 100.0 * cost / limit
        verdict = ("OK   %.1f%%" % pct) if cost <= limit else \
                  ("OVER by %d B (%.1f%%)" % (cost - limit, pct))
        print("%-24s %14d %8.1f%% %6d %6d  %s"
              % (sid, cost, 100.0 * cost / TEXTURE_BUDGET, known, unknown, verdict))

    worst = rows[0]
    limit = TEXTURE_BUDGET - NON_STAGE
    print()
    print("worst stage: %s at %d B for its props alone, against a %d B budget"
          % (worst[0], worst[1], limit))
    print("that is %.0fx too big to upload at its native resolution."
          % (worst[1] / float(limit)))
    gaps = sum(r[3] for r in rows)
    print("props with no measured size: %d across all stages" % gaps)
    if gaps:
        print("those are counted as zero above, so the figures are LOWER BOUNDS.")

    print()
    print("The original has no such limit; the PS2 does. The next table asks the")
    print("useful question instead: how far must each stage be reduced to fit,")
    print("assuming every prop is scaled by the same factor (so the composition")
    print("and the scroll factors survive) and rounded up to a power of two?")
    print()
    print("%-24s %10s %14s %14s  %s" % ("stage", "factor", "biggest prop", "props total", "note"))
    print("-" * 84)
    for sid, cost, known, unknown, zoom in rows:
        if unknown:
            note = "%d prop(s) unmeasured, factor is optimistic" % unknown
        elif cost <= limit:
            note = "fits at native resolution"
        else:
            note = ""
        f = max_scale(os.path.join(STAGES, sid + ".json"), sizes, limit)
        if f is None:
            print("%-24s %10s %14s %14s  %s" % (sid, "n/a", "-", "-", "no props"))
            continue
        biggest = biggest_prop(os.path.join(STAGES, sid + ".json"), sizes, f)
        print("%-24s %10s %14s %14s  %s"
              % (sid, "1/%d" % round(1 / f) if f < 1 else "1",
                 biggest, "%d B" % prop_total(os.path.join(STAGES, sid + ".json"), sizes, f), note))
    return 0


def prop_total(path, sizes, f):
    d = json.load(open(path))
    t = 0
    for p in d.get("props") or []:
        ap = p.get("assetPath")
        if not ap or ap.startswith("#"):
            continue
        e = sizes.get(ap)
        if not e or e["w"] is None:
            continue
        t += next_pow2(max(1, int(e["w"] * f))) * next_pow2(max(1, int(e["h"] * f))) * 4
    return t


def biggest_prop(path, sizes, f):
    d = json.load(open(path))
    best = "-"
    best_area = (0,)
    seen = set()
    for p in d.get("props") or []:
        ap = p.get("assetPath")
        if not ap or ap.startswith("#") or ap in seen:
            continue
        seen.add(ap)
        e = sizes.get(ap)
        if not e or e["w"] is None or e["h"] is None:
            continue
        w = next_pow2(max(1, int(e["w"] * f)))
        h = next_pow2(max(1, int(e["h"] * f)))
        if w * h > best_area[0]:
            best_area = (w * h,)
            best = "%dx%d" % (w, h)
    return best


def max_scale(path, sizes, budget):
    """Largest uniform downscale factor whose pow2 total fits the budget."""
    d = json.load(open(path))
    props = []
    for p in d.get("props") or []:
        ap = p.get("assetPath")
        if not ap or ap.startswith("#"):
            continue
        e = sizes.get(ap)
        if e and e["w"] is not None and e["h"] is not None:
            props.append((e["w"], e["h"]))
    if not props:
        return None
    if sum(next_pow2(w) * next_pow2(h) * 4 for w, h in props) <= budget:
        return 1.0
    lo, hi = 0.0, 1.0
    for _ in range(60):
        mid = (lo + hi) / 2
        t = sum(next_pow2(max(1, int(w * mid))) * next_pow2(max(1, int(h * mid))) * 4
                 for w, h in props)
        if t <= budget:
            lo = mid
        else:
            hi = mid
    return lo


if __name__ == "__main__":
    sys.exit(main())
