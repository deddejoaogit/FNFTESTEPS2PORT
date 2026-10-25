#!/usr/bin/env python3
"""VRAM budget for every stage, computed from MEASURED image dimensions.

WHAT CHANGED AND WHY
--------------------
The first version of this tool assumed the port uploads a whole atlas per prop,
at 32 bpp, padded to a power of two. Every one of those three assumptions turned
out to be wrong for the port that actually runs:

  * the atlas is never resident -- src/main.c keeps the frames LZ4-compressed in
    main RAM and uploads ONE decoded frame (`tutorial_pack_load` /
    `tutorial_pack_frame`), so VRAM pays for one frame, not for the sheet;
  * bpp is 16 where it matters -- the girlfriend's atlas is already uploaded as
    GS_PSM_16, and tools/pacote_personagem.py shows 16 bpp is what lets the
    characters stay at their native size;
  * the GS needs no power-of-two extent for these textures -- the port uploads
    320x307 and 384x192 today.

A budget built on the wrong model is worse than no budget: it reported numbers
that looked settled. This one is computed from assets-data/image-sizes.json,
which tools/measure_assets.py fills from the real pixels, and it reports its
inputs rather than hiding them.

WHAT IS CHARGED
    2 framebuffers          2 x 512 x 384 x 2   (PSMCT16, measured to fit)
    font atlas              384 x 192 x 4
    HUD + note atlas        2 x 320 x 256 x 2
    three characters        one frame each, at the largest frame of the atlas
    the stage               one frame per unique prop, at scale f

The stage scale f is the answer the tool prints: the layout and the scroll
factors are preserved, only the pixels shrink.

Run from the project root: python3 tools/vram_budget.py [--bpp 16]
"""
import argparse
import glob
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.dirname(HERE)
STAGES = os.path.join(PROJECT, "assets-data", "stages")
SIZES = os.path.join(PROJECT, "assets-data", "image-sizes.json")

# SCPH-90006, 4 MB of GS VRAM.
VRAM_TOTAL = 4194304

# Two 512x384 PSMCT16 framebuffers. The port draws 640x360 logical inside
# 640x448 today at PSMCT32, which costs 2,293,760 B and leaves 1,900,544 -- the
# reason the original budget was so tight. PSMCT16 framebuffers are the change
# that buys the room for native-resolution characters, so they are the model
# used here (and are on the standing work list).
FB_W, FB_H = 512, 384
FB_COUNT = 2

FONT_W, FONT_H = 384, 192
HUD_W, HUD_H = 320, 256
NOTE_W, NOTE_H = 320, 256

# The three character slots the port always fills (bf, gf, dad).
CHAR_SLOTS = 3


def load_sizes():
    if not os.path.isfile(SIZES):
        sys.exit("missing %s -- run tools/measure_assets.py first" % SIZES)
    return json.load(open(SIZES))["images"]


def frame_dims(entry):
    """The largest single frame of an atlas -- what one resident frame costs."""
    if not entry:
        return None
    w = entry.get("frame_w") or entry.get("w")
    h = entry.get("frame_h") or entry.get("h")
    if not w or not h:
        return None
    return (w, h)


def stage_props(sid):
    """-> [(assetPath, (w, h))] unique props, in zIndex order, gaps dropped."""
    d = json.load(open(os.path.join(STAGES, sid + ".json")))
    out = []
    seen = set()
    for p in d.get("props") or []:
        ap = p.get("assetPath")
        if not ap or ap.startswith("#") or ap in seen:
            continue
        seen.add(ap)
        e = frame_dims(SIZES_CACHE.get(ap))
        if e:
            out.append((ap, e))
    return out


def stage_bytes(sid, f, bpp):
    return sum(max(1, int(w * f)) * max(1, int(h * f)) * (bpp // 8)
               for _, (w, h) in stage_props(sid))


def max_scale(sid, budget, bpp):
    """Largest uniform scale whose total stays inside `budget`."""
    if stage_bytes(sid, 1.0, bpp) <= budget:
        return 1.0
    lo, hi = 0.0, 1.0
    for _ in range(50):
        mid = (lo + hi) / 2.0
        if stage_bytes(sid, mid, bpp) <= budget:
            lo = mid
        else:
            hi = mid
    return lo


def char_frame_bytes(bpp):
    """Sum of one frame for each character slot, at native size."""
    # bf, gf and dad are the three the port always loads; measured native
    # frames are 435x461, 513x369 and 368x418 (see pacote_personagem.py).
    dims = [(435, 461), (513, 369), (368, 418)]
    return sum(w * h * (bpp // 8) for w, h in dims)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--bpp", type=int, default=16, choices=(16, 32))
    ap.add_argument("--quiet", action="store_true")
    args = ap.parse_args()
    bpp = args.bpp

    global SIZES_CACHE
    SIZES_CACHE = load_sizes()

    fb_bytes = FB_W * FB_H * (bpp // 8)
    textures = VRAM_TOTAL - FB_COUNT * fb_bytes
    fixed = (FONT_W * FONT_H * 4 + HUD_W * HUD_H * (bpp // 8)
             + NOTE_W * NOTE_H * (bpp // 8))
    chars = char_frame_bytes(bpp)
    limit = textures - fixed - chars

    if limit <= 0:
        print("FAIL: %d bpp does not even fit the framebuffers and the fixed "
              "textures (%d B over)" % (bpp, -limit))
        return 1

    rows = []
    for f in sorted(glob.glob(os.path.join(STAGES, "*.json"))):
        sid = os.path.basename(f)[:-5]
        props = stage_props(sid)
        if not props:
            rows.append((sid, None, 0, 0, None))
            continue
        sc = max_scale(sid, limit, bpp)
        rows.append((sid, sc, stage_bytes(sid, sc, bpp), len(props),
                     max(w * h for _, (w, h) in props)))
    rows.sort(key=lambda r: -(r[2] or 0))

    if not args.quiet:
        print("SCPH-90006: %d B VRAM; %d x %dx%d PSMCT%d framebuffers = %d B"
              % (VRAM_TOTAL, FB_COUNT, FB_W, FB_H, bpp, FB_COUNT * fb_bytes))
        print("texture region: %d B" % textures)
        print("  fixed textures (font + HUD + notes): %d B" % fixed)
        print("  3 characters at native size:         %d B" % chars)
        print("  left for the stage:                  %d B" % limit)
        print()
        print("%-24s %7s %12s %6s %10s  %s"
              % ("stage", "scale", "stage VRAM", "props", "biggest", "verdict"))
        print("-" * 78)
        for sid, sc, cost, nprop, big in rows:
            if sc is None:
                print("%-24s %7s %12s %6d %10s  %s"
                      % (sid, "n/a", "-", 0, "-", "no measured props"))
                continue
            v = "fits at native size" if sc >= 0.999 else \
                "reduced to %.0f%%" % (sc * 100.0)
            if cost > limit:
                v = "OVER by %d B" % (cost - limit)
            print("%-24s %7.3f %12d %6d %10d  %s"
                  % (sid, sc, cost, nprop, big, v))
        worst = max((r for r in rows if r[1] is not None),
                    key=lambda r: r[2], default=None)
        if worst:
            print()
            print("worst stage: %s needs %d B of its %d B allowance, so it is "
                  "drawn at scale %.3f." % (worst[0], worst[2], limit, worst[1]))
    return 0


SIZES_CACHE = {}

if __name__ == "__main__":
    sys.exit(main())
