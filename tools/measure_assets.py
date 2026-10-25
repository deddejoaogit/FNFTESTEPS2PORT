#!/usr/bin/env python3
"""Measure the real image assets and emit the VRAM figures the port budgets on.

Everything the port knows about image sizes came, until now, from a Drive
`imageMediaMetadata` dump that only covered the 42 files somebody happened to
upload. That left gaps in the budget for most stages, and a gap in a budget is
worse than a bad number: it makes the total look settled when it is not.

This tool reads the pixels instead of a metadata mirror. For a Sparrow/Animate
atlas it reports the ATLAS dimensions (`png`), because that is what the port
uploads -- a 4096x4096 sheet costs 64 MB at 32 bpp no matter how small the
individual frames are -- plus the largest single frame, which is the floor for
any per-frame scheme that slices the sheet.

Sources, in the game's own asset tree (a clone of FunkinCrew/funkin.assets):
  shared/images, preload/images, <week>/images, <song>/images

Usage:
  python3 tools/measure_assets.py --assets /path/to/funkin.assets
  python3 tools/measure_assets.py --assets ... --stages-only
"""

import argparse
import glob
import json
import os
import re
import struct
import sys
import xml.etree.ElementTree as ET

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.dirname(HERE)
DATA = os.path.join(PROJECT, "assets-data")

# Folders the asset clone keeps images in, in FNF's own priority order. The
# engine searches the song folder first, then the week, then shared.
IMAGE_DIRS = [
    "shared/images", "preload/images", "tutorial/images",
    "week1/images", "week2/images", "week3/images", "week4/images",
    "week5/images", "week6/images", "week7/images", "weekend1/images",
    "sserafim/images", "fonts", "preload/videos",
]


def png_size(path):
    """-> (w, h) from the IHDR chunk, or None. No decoder needed: IHDR is
    always the first chunk, at a fixed offset."""
    try:
        with open(path, "rb") as f:
            head = f.read(26)
        if len(head) < 26 or head[:8] != b"\x89PNG\r\n\x1a\n":
            return None
        if head[12:16] != b"IHDR":
            return None
        w, h = struct.unpack(">II", head[16:24])
        return (w, h)
    except OSError:
        return None


def find_image(assets, stem):
    """Resolve an assetPath to its pixels, searching like the engine does.

    Two layouts exist in the game and both are common:

      Sparrow  -- <stem>.png + <stem>.xml, one sheet
      Animate  -- <stem>/Animation.json + <stem>/spritemapN.json + <stem>/spritemapN.png
                  (renderType "multianimateatlas": a character may ship SEVERAL
                   spritemaps, and VRAM pays for every one of them)

    An assetPath may also carry a package prefix -- "shared:characters/bf-car" --
    which the engine expands to an asset folder. Both are handled here.

    Returns (png_paths, dir_path) where png_paths is a list of every PNG the
    asset occupies, and dir_path is set for the Animate layout.
    """
    stem = stem.replace("\\", "/")
    tries = []
    if ":" in stem:
        pkg, rest = stem.split(":", 1)
        tries.append(os.path.join(assets, pkg, rest))
        tries.append(os.path.join(assets, pkg, "images", rest))
        stem = rest

    def direct(base):
        if os.path.isfile(base + ".png"):
            return ([base + ".png"], None)
        if os.path.isdir(base):
            maps = sorted(glob.glob(os.path.join(base, "spritemap*.png")))
            if maps:
                return (maps, base)
        return None

    for d in IMAGE_DIRS:
        for cand in (stem, os.path.basename(stem)):
            hit = direct(os.path.join(assets, d, cand))
            if hit:
                return hit
    for t in tries:
        hit = direct(t)
        if hit:
            return hit

    base = os.path.basename(stem)
    for hit in glob.glob(os.path.join(assets, "**", base + ".png"), recursive=True):
        if os.path.isfile(hit):
            return ([hit], None)
    for hit in glob.glob(os.path.join(assets, "**", base), recursive=True):
        if os.path.isdir(hit):
            maps = sorted(glob.glob(os.path.join(hit, "spritemap*.png")))
            if maps:
                return (maps, hit)
    return (None, None)


def animate_frames(dir_path):
    """-> (max_frame_w, max_frame_h, n_frames) from an Animate spritemap .json.

    Frame rects live in "ATLAS"."SPRITES"[i].SPRITE, each with w/h. Several
    spritemap json files may exist, one per sheet, and the largest frame of any
    of them is the floor for a per-frame scheme.
    """
    mw = mh = n = 0
    for jf in sorted(glob.glob(os.path.join(dir_path, "spritemap*.json"))):
        try:
            d = json.load(open(jf))
        except (ValueError, OSError):
            continue
        atlas = d.get("ATLAS") or {}
        # Two shapes occur: ATLAS = {"SPRITES": [...]} directly, or
        # ATLAS = {"<sheetname>": {"SPRITES": [...]}}. Accept both.
        sheets = [atlas] if "SPRITES" in atlas else [
            v for v in atlas.values() if isinstance(v, dict)]
        for sheet in sheets:
            for spr in sheet.get("SPRITES") or []:
                sp = spr.get("SPRITE") or {}
                try:
                    w = int(sp.get("w", 0))
                    h = int(sp.get("h", 0))
                except (TypeError, ValueError):
                    continue
                n += 1
                mw, mh = max(mw, w), max(mh, h)
    if not n:
        return None
    return (mw, mh, n)


def sparrow_frames(xml_path):
    """-> (max_frame_w, max_frame_h, n_frames) from a Sparrow/Animate atlas.

    Both atlas formats keep the frame rect in the frame element:
      Sparrow:  <SubTexture name="x" x=".." y=".." width=".." height=".."/>
      Animate:  <SubTexture name="x" frameX=".." frameY=".." frameWidth=".." frameHeight=".."/>
    """
    try:
        root = ET.parse(xml_path).getroot()
    except (ET.ParseError, OSError):
        return None
    mw = mh = 0
    n = 0
    for el in root.iter():
        if not el.tag.endswith("SubTexture"):
            continue
        n += 1
        for wk, hk in (("frameWidth", "frameHeight"), ("width", "height")):
            try:
                w = int(float(el.get(wk)))
                h = int(float(el.get(hk)))
            except (TypeError, ValueError):
                continue
            mw, mh = max(mw, w), max(mh, h)
            break
    if not n:
        return None
    return (mw, mh, n)


def nearest_pow2(v):
    p = 1
    while p < v:
        p <<= 1
    return p


def collect_stems():
    """Every image assetPath the stage and character JSONs mention."""
    stems = {}
    for f in sorted(glob.glob(os.path.join(DATA, "stages", "*.json"))):
        d = json.load(open(f))
        for p in d.get("props") or []:
            ap = p.get("assetPath")
            if ap and not ap.startswith("#"):
                stems.setdefault(ap, []).append(os.path.basename(f)[:-5])
    return stems


def char_assets():
    """-> [(id, assetPath)] for every character JSON.

    The id is NOT the file stem: bf-car's images come from
    "shared:characters/bf-car", and older entries name a different stem than
    their id. The JSON's own assetPath is the only right answer.
    """
    out = []
    for f in sorted(glob.glob(os.path.join(DATA, "characters", "*.json"))):
        try:
            d = json.load(open(f))
        except ValueError:
            continue
        cid = os.path.basename(f).rsplit(".json", 1)[0]
        out.append((cid, d.get("assetPath") or cid))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--assets", required=True,
                    help="root of a funkin.assets clone")
    ap.add_argument("--out", default=os.path.join(DATA, "image-sizes.json"))
    ap.add_argument("--stages-only", action="store_true")
    args = ap.parse_args()

    assets = os.path.abspath(args.assets)
    if not os.path.isdir(assets):
        sys.exit("not a directory: %s" % assets)

    images = {}
    missing = []

    def measure(stem, where):
        if stem in images:
            return
        pngs, d = find_image(assets, stem)
        if not pngs:
            images[stem] = None
            missing.append((stem, where, "no png"))
            return
        sizes = []
        for pn in pngs:
            sz = png_size(pn)
            if sz is None:
                images[stem] = None
                missing.append((stem, where, "unreadable png"))
                return
            sizes.append((pn, sz))
        entry = {
            "w": sum(s[0] for _, s in sizes),
            "h": max(s[1] for _, s in sizes),
            "sheets": len(sizes),
            "png": [os.path.relpath(pn, assets) for pn, _ in sizes],
        }
        if len(sizes) == 1:
            entry["w"], entry["h"] = sizes[0][1]
        fr = None
        if d is not None:
            fr = animate_frames(d)
        else:
            xml = sizes[0][0][:-4] + ".xml"
            if os.path.isfile(xml):
                fr = sparrow_frames(xml)
        if fr:
            entry["frame_w"], entry["frame_h"], entry["frames"] = fr
        images[stem] = entry

    stems = collect_stems()
    for stem in sorted(stems):
        measure(stem, "stage")

    chars = []
    if not args.stages_only:
        chars = char_assets()
        for cid, apath in chars:
            measure(apath, "character:" + cid)

    out = {"source": os.path.relpath(assets, PROJECT) if assets.startswith(PROJECT)
           else assets,
           "images": images}
    with open(args.out, "w") as f:
        json.dump(out, f, indent=1, sort_keys=True)
        f.write("\n")

    known = [e for e in images.values() if e]
    total32 = sum(e["w"] * e["h"] * 4 for e in known)
    total16 = sum(e["w"] * e["h"] * 2 for e in known)
    print("measured %d image stems (%d stage props, %d characters), %d missing"
          % (len(images), len(stems), len(chars), len(missing)))
    print("at 32 bpp: %d B   at 16 bpp: %d B   (VRAM total 4194304)"
          % (total32, total16))
    big = sorted(known, key=lambda e: -e["w"] * e["h"])[:5]
    for e in big:
        name = e["png"][0] if isinstance(e["png"], list) else e["png"]
        name = name.split("/")[-1]
        if e.get("sheets", 1) > 1:
            name = "%s (+%d sheets)" % (name, e["sheets"] - 1)
        print("  %-40s %5dx%-5d = %9d B @32bpp"
              % (name, e["w"], e["h"], e["w"] * e["h"] * 4 * e.get("sheets", 1)))
    if missing:
        for stem, where, why in missing[:20]:
            print("  GAP: %-34s (%s) %s" % (stem, where, why))
        if len(missing) > 20:
            print("  ... and %d more" % (len(missing) - 20))
    print("wrote %s" % os.path.relpath(args.out, PROJECT))
    return 1 if missing else 0


if __name__ == "__main__":
    sys.exit(main())
