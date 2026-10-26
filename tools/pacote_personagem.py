#!/usr/bin/env python3
"""Pack a real FNF character into the port's CFZ4 frame format.

WHY THIS EXISTS
---------------
Measured: the game's stage and character images total 2,008,108,660 B at 32 bpp
(tools/measure_assets.py over the real asset tree). VRAM is 4,194,304 B. The
gap is 480x, so no arrangement of "load the atlas" can work.

The port already solved this for the tutorial: a CFZ4 pack holds every frame of
an animation as its own LZ4 block, LZ4-compressed, in main RAM; the game
decompresses ONE frame into a staging buffer and uploads that one frame to VRAM
(`tutorial_pack_load` / `tutorial_pack_frame` in src/main.c). VRAM therefore
pays for a single frame per character, not for an atlas.

This tool produces exactly that file from the original art, so every one of the
46 characters and 21 stages becomes packable the same way.

FORMAT (read from the decoder, not invented)
    "CFZ4", u32 version=1, u32 count, u32 frame_bytes
    count * (u32 offset, u32 size)
    blocks: `size` bytes each, one LZ4 block decompressing to exactly
            frame_bytes

frame_bytes is w*h*4 for PSMCT32 or w*h*2 for PSMCT16, and every frame in a
pack shares it -- so the canvas is the animation's largest frame, and the frame
is drawn centred on that canvas at its original offsets.

DEFAULTS THE ORIGINAL USES (Character.hx / Animation.hx v0.8.6)
    animFramerate is per animation; a looped animation restarts at the end,
    a "hold" animation holds its last frame. The pack stores frames only -- the
    timing lives in the animation table, not in the pack.

USAGE
    python3 tools/pacote_personagem.py --assets /path/to/funkin.assets \
        --characters bf,gf,dad --out build/packs
"""

import argparse
import glob
import json
import os
import struct
import sys

import lz4.block
import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.dirname(HERE)
DATA = os.path.join(PROJECT, "assets-data")

IMAGE_DIRS = [
    "shared/images", "preload/images", "tutorial/images",
    "week1/images", "week2/images", "week3/images", "week4/images",
    "week5/images", "week6/images", "week7/images", "weekend1/images",
    "sserafim/images",
]

# The port's hard limit on a texture, and on one pack in RAM
# (tutorial_pack_load rejects anything over 8 MB).
TEXTURE_MAX_W = 1024
TEXTURE_MAX_H = 1024
PACK_MAX_BYTES = 8 * 1024 * 1024


# --------------------------------------------------------------- resolution --

def resolve(assets, stem):
    """-> ('animate', dir) | ('sparrow', (png, xml)) | (None, reason)."""
    stem_orig = stem
    stem = stem.replace("\\", "/")
    cands = []
    if ":" in stem:
        pkg, rest = stem.split(":", 1)
        cands += [os.path.join(assets, pkg, rest),
                  os.path.join(assets, pkg, "images", rest)]
        stem = rest
    for d in IMAGE_DIRS:
        for c in (stem, os.path.basename(stem)):
            cands.append(os.path.join(assets, d, c))
    if ":" in stem_orig:
        pkg, rest = stem_orig.split(":", 1)
        for sub in ("images", "characters", "characters/" + rest.split("/")[0]):
            for cand in (rest, os.path.basename(rest)):
                b = os.path.join(assets, pkg, sub, cand)
                cands.append(b)
    for base in cands:
        if os.path.isdir(base) and glob.glob(os.path.join(base, "spritemap*.png")):
            return "animate", base
        if os.path.isfile(base + ".png"):
            # Third layout: renderType "packer" ships a .txt frame list next to
            # the image ("name = x y w h"), with no XML at all.
            if os.path.isfile(base + ".txt"):
                return "packer", (base + ".png", base + ".txt")
            return "sparrow", (base + ".png", base + ".xml")
    return None, "no art for %r" % stem


def packer_frames(txt_path):
    """-> [(name, x, y, w, h)] from a TexturePacker-style .txt, in file order.

    The format is one line per frame:
        idle spirit_0 = 0 1028 128 128
    Anything that does not parse as four integers is a comment or a trailer and
    is skipped, which is how the real files are written.
    """
    out = []
    with open(txt_path, encoding="utf-8-sig", errors="replace") as f:
        for line in f:
            line = line.strip()
            if "=" not in line:
                continue
            name, rest = line.rsplit("=", 1)
            parts = rest.split()
            if len(parts) < 4:
                continue
            try:
                x, y, w, h = (int(float(v)) for v in parts[:4])
            except ValueError:
                continue
            if w > 0 and h > 0:
                out.append((name.strip(), x, y, w, h))
    return out


def sparrow_frames(xml_path):
    """-> {name: (x, y, w, h, fx, fy, fw, fh)} in atlas order."""
    import xml.etree.ElementTree as ET
    out = {}
    order = []
    try:
        root = ET.parse(xml_path).getroot()
    except (ET.ParseError, OSError):
        return out, order
    for el in root.iter():
        if not el.tag.endswith("SubTexture"):
            continue
        name = el.get("name")

        def g(k, dflt=0.0):
            try:
                return float(el.get(k))
            except (TypeError, ValueError):
                return dflt
        w, h = g("width"), g("height")
        fw = g("frameWidth", w) or w
        fh = g("frameHeight", h) or h
        fx = g("frameX")
        fy = g("frameY")
        out[name] = (g("x"), g("y"), w, h, fx, fy, fw, fh)
        order.append(name)
    return out, order


def load_atlas(assets, stem):
    """-> list of frames [(pil RGBA image, offset_x, offset_y)], in atlas order.

    Both layouts return the same shape so the packer below is layout-agnostic.
    """
    kind, where = resolve(assets, stem)
    if kind is None:
        return None, where

    frames = []
    if kind == "packer":
        png, txt = where
        sheet = Image.open(png).convert("RGBA")
        for name, x, y, w, h in packer_frames(txt):
            frames.append((sheet.crop((x, y, x + w, y + h)), 0, 0))
        return frames, None
    if kind == "sparrow":
        png, xml = where
        rects, order = sparrow_frames(xml)
        if not order:
            # No XML at all: a static prop that ships as a plain image. The
            # whole PNG is the single frame. (philly/sky and friends are like
            # this; treating them as an empty atlas dropped 6 props.)
            return [(Image.open(png).convert("RGBA"), 0, 0)], None
        sheet = Image.open(png).convert("RGBA")
        for name in order:
            x, y, w, h, fx, fy, fw, fh = rects[name]
            if w <= 0 or h <= 0:
                continue
            img = sheet.crop((int(x), int(y), int(x + w), int(y + h)))
            # frameX/frameY are negative when the art sits inside a larger
            # logical frame; the sprite is placed at -frameX/-frameY.
            frames.append((img, -fx, -fy))
        return frames, None

    # Animate: several spritemaps, each with its own image and the same
    # SPRITES list shape. Frames are keyed by "name"; the multi-sheet case
    # maps names to the sheet that holds them.
    names = {}
    for jf in sorted(glob.glob(os.path.join(where, "spritemap*.json"))):
        d = json.load(open(jf, encoding="utf-8-sig"))
        atlas = d.get("ATLAS") or {}
        sheets = [atlas] if "SPRITES" in atlas else [
            v for v in atlas.values() if isinstance(v, dict)]
        png = jf[:-5] + ".png"
        if not os.path.isfile(png):
            continue
        img_sheet = Image.open(png).convert("RGBA")
        for sheet in sheets:
            for spr in sheet.get("SPRITES") or []:
                sp = spr.get("SPRITE") or {}
                try:
                    x, y = int(sp["x"]), int(sp["y"])
                    w, h = int(sp["w"]), int(sp["h"])
                except (KeyError, TypeError, ValueError):
                    continue
                if w <= 0 or h <= 0:
                    continue
                img = img_sheet.crop((x, y, x + w, y + h))
                names[sp.get("name")] = img
    # Animation.json gives the frame order per animation and the per-frame
    # pivot. That file is what the game reads at runtime, so the packer reads
    # the same file rather than guessing an order from the atlas.
    anim_json = os.path.join(where, "Animation.json")
    order = []
    if os.path.isfile(anim_json):
        a = json.load(open(anim_json, encoding="utf-8-sig"))
        for an in a.get("ANIMATION", {}).get("ANIMATIONS", []):
            for fname in an.get("FRAMES", []):
                n = fname.get("name")
                if n in names and n not in order:
                    order.append(n)
    if not order:
        order = list(names)
    for n in order:
        frames.append((names[n], 0, 0))
    return frames, None


# ------------------------------------------------------------------ the pack --

def render_frames(frames, scale, bpp, pad=0):
    """-> (canvas_frames, w, h, total_bytes_per_frame).

    Every frame is drawn at the same scale onto one canvas big enough for the
    largest scaled frame, so one canvas size serves the whole animation -- the
    format has a single frame_bytes per pack.
    """
    scaled = []
    for img, ox, oy in frames:
        if scale != 1.0:
            nw = max(1, int(round(img.width * scale)))
            nh = max(1, int(round(img.height * scale)))
            # NEAREST, not LANCZOS: the downscale is large (0.28 measured for
            # a 42-frame sheet), and a smooth kernel on 16 bpp input spreads
            # alpha into transparent pixels, which shows as a halo against the
            # stage. Point sampling keeps the silhouette exact.
            img = img.resize((nw, nh), Image.NEAREST)
        scaled.append((img, int(round(ox * scale)), int(round(oy * scale))))

    # Canvas: the largest frame, with room for the offset so no frame clips.
    cw = max(img.width + max(0, ox) for img, ox, _ in scaled)
    ch = max(img.height + max(0, oy) for img, _, oy in scaled)
    # Frames whose offset is negative still need room on the left/top.
    left = max(0, -min(ox for _, ox, _ in scaled))
    top = max(0, -min(oy for _, _, oy in scaled))
    cw += left + pad * 2
    ch += top + pad * 2

    if cw > TEXTURE_MAX_W or ch > TEXTURE_MAX_H:
        raise ValueError("canvas %dx%d exceeds the %dx%d texture limit"
                         % (cw, ch, TEXTURE_MAX_W, TEXTURE_MAX_H))

    out = []
    for img, ox, oy in scaled:
        canvas = Image.new("RGBA", (cw, ch), (0, 0, 0, 0))
        canvas.paste(img, (ox + left + pad, oy + top + pad))
        out.append(canvas)
    frame_bytes = cw * ch * (bpp // 8)
    return out, cw, ch, frame_bytes


def to_ps2_bytes(img, bpp):
    """RGBA image -> the bytes the game's upload_raw() expects.

    32 bpp (PSMCT32) is RGBA in memory order, which is what
    `upload_raw` takes. 16 bpp (PSMCT16) is RGBA5551 little-endian, which is
    what `upload_raw_psm(..., GS_PSM_16)` takes -- the game already uploads the
    girlfriend's atlas that way, so both paths are real.
    """
    a = np.array(img, dtype=np.uint8)
    if bpp == 32:
        return a.tobytes()
    r = (a[:, :, 0].astype(np.uint16) >> 3) & 0x1F
    g = (a[:, :, 1].astype(np.uint16) >> 3) & 0x1F
    b = (a[:, :, 2].astype(np.uint16) >> 3) & 0x1F
    al = (a[:, :, 3].astype(np.uint16) >> 7) & 0x01
    v = (r | (g << 5) | (b << 10) | (al << 15)).astype("<u2")
    return v.tobytes()


def write_pack(blocks, frame_bytes, magic=b"CFZ4"):
    header = magic + struct.pack("<III", 1, len(blocks), frame_bytes)
    table_size = 16 + len(blocks) * 8
    entries = bytearray()
    blob = bytearray()
    off = table_size
    for b in blocks:
        entries += struct.pack("<II", off, len(b))
        blob += b
        off += len(b)
    if off > PACK_MAX_BYTES:
        raise ValueError("pack is %d B, over the %d B limit the loader accepts"
                         % (off, PACK_MAX_BYTES))
    return header + bytes(entries) + bytes(blob), off


def write_packs(blocks, frame_bytes, base_path):
    """Write as many CFZ4 files as it takes to stay under the loader's 8 MB.

    `tutorial_pack_load` refuses a pack over 8 MB, and a character whose whole
    frame set lands above that cannot be one file. Splitting is not a
    workaround for a limit -- it is what the runtime wants anyway: the port
    uploads one frame at a time and never needs a whole character resident, so
    a part is a natural unit.

    Returns [(path, bytes, first_frame, count)].
    """
    out = []
    part = 0
    i = 0
    while i < len(blocks):
        size = 16 + 8          # header + one entry, the smallest possible part
        take = []
        while i < len(blocks):
            nxt = size + 8 + len(blocks[i])
            if nxt > PACK_MAX_BYTES and take:
                break
            take.append(blocks[i])
            size = nxt
            i += 1
        path = base_path if part == 0 else "%s.part%d" % (base_path, part)
        data, total = write_pack(take, frame_bytes)
        # Writing happens HERE and only here: the previous shape returned
        # descriptors and left writing to the caller, and one caller forgot,
        # reporting a successful pack that was never on disk.
        with open(path, "wb") as f:
            f.write(data)
        out.append((path, total, i - len(take), len(take)))
        part += 1
    return out


def pack_character(assets, cid, out_dir, scale=None, bpp=32, budget=None):
    """Pack one character. Returns a dict of measured facts.

    scale=None asks for the largest scale whose ONE frame fits `budget` bytes;
    that is the honest meaning of "degrade as much as needed, no more".
    """
    cj = os.path.join(DATA, "characters", cid + ".json")
    if not os.path.isfile(cj):
        return {"id": cid, "error": "no %s" % cj}
    meta = json.load(open(cj))
    stem = meta.get("assetPath") or cid
    frames, why = load_atlas(assets, stem)
    if frames is None:
        return {"id": cid, "error": why}
    if not frames:
        return {"id": cid, "error": "atlas had no frames"}

    native_w = max(img.width for img, _, _ in frames)
    native_h = max(img.height for img, _, _ in frames)

    import math
    if scale is None:
        if budget is None:
            raise ValueError("pack_character needs scale or budget")
        # The canvas grows roughly with scale, so the bytes do too; solving
        # for an exact fit would be circular. One iteration from the linear
        # estimate is enough, and the result is measured, not assumed.
        base_w, base_h = native_w, native_h
        scale = min(1.0, math.sqrt(float(budget) / (base_w * base_h * (bpp // 8))))
        for _ in range(6):
            _, cw, ch, fb = render_frames(frames, scale, bpp)
            if fb <= budget:
                break
            scale *= math.sqrt(float(budget) / fb) * 0.98

    canvases, cw, ch, frame_bytes = render_frames(frames, scale, bpp)
    if budget is not None and frame_bytes > budget:
        return {"id": cid, "error": "cannot fit: %.3f scale is still %d B > %d B"
                % (scale, frame_bytes, budget)}

    blocks = []
    for c in canvases:
        raw = to_ps2_bytes(c, bpp)
        if len(raw) != frame_bytes:
            return {"id": cid, "error": "frame is %d B, header says %d"
                    % (len(raw), frame_bytes)}
        blocks.append(lz4.block.compress(raw, store_size=False))

    os.makedirs(out_dir, exist_ok=True)
    base = os.path.join(out_dir, cid + ".cfz4")
    parts = write_packs(blocks, frame_bytes, base)

    return {
        "id": cid,
        "files": [p for p, _, _, _ in parts],
        "parts": len(parts),
        "frames": len(blocks),
        "canvas": (cw, ch),
        "scale": round(scale, 4),
        "bpp": bpp,
        "vram_bytes": frame_bytes,
        "disk_bytes": sum(t for _, t, _, _ in parts),
        "source_atlas": stem,
        "native": (native_w, native_h),
    }


def verify(path, bpp):
    """Decode a pack back and check every frame decodes to the stated size."""
    data = open(path, "rb").read()
    if data[:4] != b"CFZ4":
        return "bad magic %r" % data[:4]
    _m, ver, count, frame_bytes = (data[:4], ) + struct.unpack("<III", data[4:16])
    if ver != 1:
        return "bad version %d" % ver
    expect = 4 if bpp == 32 else 2
    if frame_bytes % expect:
        return "frame_bytes %d not a multiple of %d" % (frame_bytes, expect)
    px = frame_bytes // expect
    for i in range(count):
        off, size = struct.unpack("<II", data[16 + i * 8: 24 + i * 8])
        blk = data[off:off + size]
        raw = lz4.block.decompress(blk, uncompressed_size=frame_bytes)
        if len(raw) != frame_bytes:
            return "frame %d decoded to %d B, expected %d" % (i, len(raw), frame_bytes)
    return None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--assets", required=True)
    ap.add_argument("--characters", default="bf,gf,dad")
    ap.add_argument("--out", default=os.path.join(PROJECT, "build", "packs"))
    ap.add_argument("--scale", type=float, default=None)
    ap.add_argument("--bpp", type=int, default=32, choices=(16, 32))
    ap.add_argument("--budget", type=int, default=147456,
                    help="VRAM bytes allowed for ONE frame (default 144 KiB)")
    ap.add_argument("--verify", action="store_true", default=True)
    args = ap.parse_args()

    total_vram = 0
    total_disk = 0
    fails = 0
    for cid in [c.strip() for c in args.characters.split(",") if c.strip()]:
        try:
            r = pack_character(args.assets, cid, args.out,
                               scale=args.scale, bpp=args.bpp, budget=args.budget)
        except ValueError as e:
            print("%-22s FAIL  %s" % (cid, e))
            fails += 1
            continue
        if "error" in r:
            print("%-22s FAIL  %s" % (cid, r["error"]))
            fails += 1
            continue
        err = None
        for fpath in r["files"]:
            err = verify(fpath, args.bpp) if args.verify else None
            if err:
                break
        if err:
            print("%-22s FAIL  verify: %s" % (cid, err))
            fails += 1
            continue
        total_vram += r["vram_bytes"]
        total_disk += r["disk_bytes"]
        print("%-22s %3d fr  %4dx%-4d  scale %.3f  %d bpp  "
              "VRAM/frame %7d B  pack %8d B  %d file(s)"
              % (cid, r["frames"], r["canvas"][0], r["canvas"][1], r["scale"],
                 r["bpp"], r["vram_bytes"], r["disk_bytes"], r["parts"]))
    print()
    print("total VRAM for one frame of each: %d B" % total_vram)
    print("total on the pendrive:            %d B" % total_disk)
    if fails:
        print("%d failure(s)" % fails)
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
