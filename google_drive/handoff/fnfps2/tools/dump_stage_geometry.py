#!/usr/bin/env python3
"""Dump the real stage geometry from assets/data/stages/*.json.

bs_dados.h carries only each stage's cameraZoom and the three character slots.
The stages also carry `props`: the background layers with their own position,
scale, parallax scroll factor and zIndex. main.c hardcodes one stage window
(STAGE_WIN_X/Y/W/H) and one scroll factor (STAGE_SCROLL), which is a composite
of these layers, so the real values have to be visible somewhere or that
composite cannot be checked.

This prints, per stage, the bounding box of the union of its props in world
units and the scroll factors it uses. Where the prop's image is available
under tools/known-sizes.json its true pixel size is used; otherwise the prop
is reported with an unknown extent, which is honest -- an invented size would
be worse than a gap.

The three shared stage images are already known (see below) because they are
the same files for every week that reuses them.
"""
import glob
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.dirname(HERE)
STAGES = os.path.join(PROJECT, "assets-data", "stages")
KNOWN = os.path.join(HERE, "known-sizes.json")

# Measured from the real PNGs in assets/shared/images/ (see the docs). These
# three are reused by every stage that references them, so they are constant.
SHARED_IMAGE_SIZES = {
    "stageback": (2560, 1400, 1),
    "stagefront": (2560, 581, 4),
    "stagecurtains": (2560, 1400, 4),
}


def load_known():
    if os.path.exists(KNOWN):
        return json.load(open(KNOWN))
    return {}


def bbox(stage, known):
    """Union of the props' world-space rectangles. None if any extent is unknown."""
    x0 = y0 = None
    x1 = y1 = None
    missing = []
    for p in stage.get("props") or []:
        path = p.get("assetPath")
        size = known.get(path) or SHARED_IMAGE_SIZES.get(path)
        if not size:
            missing.append(path)
            continue
        w, h, _ = size
        px, py = p.get("position") or [0, 0]
        sx, sy = p.get("scale") or [1, 1]
        ax0, ay0 = px, py
        ax1, ay1 = px + w * sx, py + h * sy
        x0 = ax0 if x0 is None else min(x0, ax0)
        y0 = ay0 if y0 is None else min(y0, ay0)
        x1 = ax1 if x1 is None else max(x1, ax1)
        y1 = ay1 if y1 is None else max(y1, ay1)
    if x0 is None:
        return None, missing
    return (x0, y0, x1, y1), missing


def main():
    known = load_known()
    print("%-24s %-6s %-5s %-34s %-22s" % ("stage", "zoom", "prop", "union bbox (x0,y0,x1,y1)", "scroll factors"))
    print("-" * 100)
    gaps = 0
    for f in sorted(glob.glob(os.path.join(STAGES, "*.json"))):
        d = json.load(open(f))
        sid = os.path.basename(f)[:-5]
        box, missing = bbox(d, known)
        scrolls = sorted({tuple(p.get("scroll") or [1, 1]) for p in (d.get("props") or [])})
        if box is None:
            gaps += 1
            print("%-24s %-6s %-5d %-34s %-22s"
                  % (sid, d.get("cameraZoom"), len(d.get("props") or []),
                     "unknown (no image sizes)", str([s[0] for s in scrolls])))
        else:
            x0, y0, x1, y1 = box
            print("%-24s %-6s %-5d %-34s %-22s"
                  % (sid, d.get("cameraZoom"), len(d.get("props") or []),
                     "(%g,%g,%g,%g) %gx%g" % (x0, y0, x1, y1, x1 - x0, y1 - y0),
                     str([s[0] for s in scrolls])))
        if missing:
            print("      images with no known size: %s"
                  % ", ".join(sorted(set(m for m in missing if m))))
    print()
    print("%d stages have no complete bounding box because their images are not"
          " downloaded yet. That is a gap, not a guess." % gaps)
    return 0


if __name__ == "__main__":
    sys.exit(main())
