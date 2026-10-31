#!/usr/bin/env python3
"""Flatten a layered Animate atlas into the flat frames the port packs.

WHY
---
34 of the 46 characters are Animate atlases: a timeline whose frames hold
SYMBOL INSTANCES, where each symbol is itself a timeline. The PS2 cannot run a
Flash renderer, so the layering has to be resolved offline into plain frames --
the same frames tools/pacote_personagem.py packs into CFZ4.

THE ALGORITHM IS READ, NOT INVENTED
-----------------------------------
Everything below comes from the renderer the game actually uses,
FunkinCrew/flxanimate:

  flxanimate/animate/FlxElement.hx, fromJSONEx():
      m[0]=mx[0]; m[1]=mx[1]; m[4]=mx[2]; m[5]=mx[3]; m[12]=mx[4]; m[13]=mx[5]
      new FlxMatrix(m[0], m[1], m[4], m[5], m[12], m[13])
    i.e. a symbol instance's MX[6] IS the Flash affine matrix
    [a, b, c, d, tx, ty], so
        x' = a*x + c*y + tx
        y' = b*x + d*y + ty

  flxanimate/animate/FlxTimeline.hx, fromJSONEx():
      for (layer in timeline.L) layers.push(FlxLayer.fromJSONEx(layer))
    i.e. layers keep the JSON order; there is no reversal.

  flxanimate/data/SpriteMapData.hx, AnimateSpriteData:
      a spritemap sprite is only a rect (x, y, w, h, rotated) in the sheet.
      There is no per-sprite registration offset, so a bitmap leaf draws its
      rect with its own local origin at (0, 0).

  flxanimate/animate/FlxElement.hx, fromJSONEx() again:
      params.firstFrame = element.SI.FF ?? 0
    so the inner frame of a graphic instance starts at SI.FF.

WHAT IS DELIBERATELY PARAMETERISED
----------------------------------
Two things the source does not pin down for a static export, and which this
tool therefore treats as settings rather than assumptions:

  --layer-order  json|reverse   which end of TL.L draws last (on top)
  --inner        first|advance  how a graphic instance picks its inner frame:
                                `first` uses SI.FF as-is; `advance` adds the
                                frames elapsed inside the parent keyframe,
                                which is what a looping graphic does in Flash

They are settled by measurement, not by opinion: run `--fit` and the tool
scores every combination against a known-good reference pack and reports which
one reproduces it.

USAGE
    # render one animation to PNGs
    python3 tools/achatar_animate.py --assets <clone> --character bf \
        --animation Idle --out build/flat/bf

    # decide the two settings against the port's own bf_frames.lz4f
    python3 tools/achatar_animate.py --assets <clone> --character bf \
        --reference <bf_frames.lz4f> --fit
"""

import argparse
import glob
import json
import math
import os
import struct
import sys

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


# ------------------------------------------------------------------ the atlas --

class Atlas:
    """One Animate atlas: the timeline, the symbol table and the sheets."""

    def __init__(self, assets, stem):
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
                    cands.append(os.path.join(assets, pkg, sub, cand))
        self.dir = None
        for base in cands:
            if os.path.isdir(base) and os.path.isfile(os.path.join(base, "Animation.json")):
                self.dir = base
                break
        if self.dir is None:
            raise FileNotFoundError("no Animate atlas for %r" % stem)

        with open(os.path.join(self.dir, "Animation.json"), encoding="utf-8-sig") as f:
            anim = json.load(f)
        self.doc = anim
        self.top = (anim.get("AN") or {}).get("TL", {}) or {}
        self.symbols = {}
        for s in (anim.get("SD", {}).get("S") or []):
            n = s.get("SN")
            if n:
                self.symbols[n] = s
        self.meta = anim.get("MD") or {}

        # spritemap: name -> (sheet_image, x, y, w, h)
        self.sprites = {}
        for jf in sorted(glob.glob(os.path.join(self.dir, "spritemap*.json"))):
            png = jf[:-5] + ".png"
            if not os.path.isfile(png):
                continue
            sheet = Image.open(png).convert("RGBA")
            with open(jf, encoding="utf-8-sig") as f:
                d = json.load(f)
            atlas = d.get("ATLAS") or {}
            sheets = [atlas] if "SPRITES" in atlas else [
                v for v in atlas.values() if isinstance(v, dict)]
            for sh in sheets:
                for spr in sh.get("SPRITES") or []:
                    sp = spr.get("SPRITE") or {}
                    n = sp.get("name")
                    try:
                        self.sprites[n] = (sheet, int(sp["x"]), int(sp["y"]),
                                           int(sp["w"]), int(sp["h"]))
                    except (KeyError, TypeError, ValueError):
                        continue

    # ---------------------------------------------------------------- timeline --
    def animations(self):
        """-> {name: (first_frame, length)} from the top timeline's first layer.

        A layer's FR entries carry N (the animation name on the first frame),
        I (start index) and DU (duration). That is how the export labels the
        animations the game plays; reading them beats re-deriving names from the
        character JSON, which uses a different prefix convention.
        """
        out = {}
        for lay in self.top.get("L") or []:
            for fr in lay.get("FR") or []:
                n = fr.get("N")
                if n and n not in out:
                    out[n] = (int(fr.get("I", 0)), int(fr.get("DU", 1)))
        return out

    def layer_frames(self, layers, frame):
        """For each layer, the FR entry covering `frame`, or None."""
        cur = []
        for lay in layers:
            pick = None
            for fr in lay.get("FR") or []:
                i = int(fr.get("I", 0))
                du = int(fr.get("DU", 1)) or 1
                if i <= frame < i + du:
                    pick = (fr, frame - i)
                    break
            cur.append((lay, pick))
        return cur

    # ------------------------------------------------------------------ render --
    def render(self, frame, layer_order="json", inner="first", ss=1):
        """Composite one frame of the top timeline -> (RGBA image, bbox)."""
        layers = list(self.top.get("L") or [])
        if layer_order == "reverse":
            layers = layers[::-1]
        ops = []          # (matrix, sprite tuple), in draw order
        self._emit(layers, IDENT, frame, inner, ops, 0)
        return self._raster(ops, ss)

    def _emit(self, layers, parent, frame, inner, ops, depth):
        if depth > 12:
            return
        for lay, cur in self.layer_frames(layers, frame):
            if cur is None:
                continue
            fr, offset = cur
            for el in fr.get("E") or []:
                # Two element kinds, and the source is explicit about it:
                # FlxElement.fromJSON does `var symbol = element.SI != null`
                # and then reads element.ASI for the other branch. SI is a
                # symbol instance (a nested timeline); ASI is a bitmap, whose
                # ASI.N is the spritemap sprite name.
                si = el.get("SI")
                if si is None:
                    asi = el.get("ASI")
                    if not asi:
                        continue
                    name = asi.get("N")
                    if name in self.sprites:
                        ops.append((mat_mul(parent, mx_matrix(asi.get("MX"))),
                                    self.sprites[name]))
                    continue
                m = mx_matrix(si.get("MX"))
                world = mat_mul(parent, m)
                name = si.get("SN")
                if name in self.symbols:
                    inner_frame = inner_frame_for(si, offset, inner)
                    sub = (self.symbols[name].get("TL") or {}).get("L") or []
                    self._emit(sub, world, inner_frame, inner, ops, depth + 1)
                elif name in self.sprites:
                    ops.append((world, self.sprites[name]))

    def _raster(self, ops, ss):
        if not ops:
            return None, None
        # Union of every transformed quad, in document space.
        xs, ys = [], []
        for m, (sheet, sx, sy, w, h) in ops:
            for px, py in ((0, 0), (w, 0), (0, h), (w, h)):
                x, y = mat_apply(m, px, py)
                xs.append(x)
                ys.append(y)
        x0, x1 = math.floor(min(xs)), math.ceil(max(xs))
        y0, y1 = math.floor(min(ys)), math.ceil(max(ys))
        w, h = max(1, x1 - x0), max(1, y1 - y0)
        if w * h > 4000 * 4000:
            raise ValueError("canvas %dx%d is absurd; transform is wrong" % (w, h))

        canvas = np.zeros((h, w, 4), dtype=np.float64)
        for m, (sheet, sx, sy, sw, sh) in ops:
            patch = sheet.crop((sx, sy, sx + sw, sy + sh))
            # Inverse map: for each destination pixel, where did it come from.
            a, b, c, d, tx, ty = m
            det = a * d - b * c
            if abs(det) < 1e-9:
                continue
            ia, ib, ic, id_ = d / det, -b / det, -c / det, a / det
            itx = -(ia * tx + ic * ty)
            ity = -(ib * tx + id_ * ty)
            # Destination pixel centres.
            gx, gy = np.meshgrid(np.arange(w) + 0.5 + x0,
                                 np.arange(h) + 0.5 + y0)
            u = ia * gx + ic * gy + itx
            v = ib * gx + id_ * gy + ity
            su = np.floor(u).astype(np.int64)
            sv = np.floor(v).astype(np.int64)
            ok = (su >= 0) & (su < sw) & (sv >= 0) & (sv < sh)
            if not ok.any():
                continue
            src = np.array(patch, dtype=np.uint8)
            tex = src[np.clip(sv, 0, sh - 1), np.clip(su, 0, sw - 1)]
            al = (tex[:, :, 3].astype(np.float64) / 255.0) * ok
            # Straight-alpha "over": the export uses normal blending, and the
            # layers are drawn bottom to top in `ops` order.
            dst_a = canvas[:, :, 3] / 255.0
            out_a = al + dst_a * (1.0 - al)
            safe = np.where(out_a > 0, out_a, 1.0)
            for ch in range(3):
                canvas[:, :, ch] = (tex[:, :, ch] * al +
                                    canvas[:, :, ch] * dst_a * (1.0 - al)) / safe
            canvas[:, :, 3] = out_a * 255.0
        img = Image.fromarray(
            np.clip(canvas, 0, 255).astype(np.uint8), "RGBA")
        return img, (x0, y0, x1, y1)


IDENT = (1.0, 0.0, 0.0, 1.0, 0.0, 0.0)


def mx_matrix(mx):
    """MX[6] -> (a, b, c, d, tx, ty), exactly as FlxElement.fromJSONEx does.

    SI and ASI both carry MX in this form; the source decodes both identically.
    """
    if mx and len(mx) >= 6:
        return (float(mx[0]), float(mx[1]), float(mx[2]),
                float(mx[3]), float(mx[4]), float(mx[5]))
    return IDENT


def inner_frame_for(si, offset, mode):
    ff = int(si.get("FF") or 0)
    if mode == "first":
        return ff
    return ff + offset


def mat_apply(m, x, y):
    a, b, c, d, tx, ty = m
    return (a * x + c * y + tx, b * x + d * y + ty)


def mat_mul(p, q):
    """p ∘ q: q is applied first."""
    pa, pb, pc, pd, ptx, pty = p
    qa, qb, qc, qd, qtx, qty = q
    return (pa * qa + pc * qb,
            pb * qa + pd * qb,
            pa * qc + pc * qd,
            pb * qc + pd * qd,
            pa * qtx + pc * qty + ptx,
            pb * qtx + pd * qty + pty)


# ----------------------------------------------------------------- reference --

def read_cfz4(path):
    """-> (count, frame_bytes, [raw frames]) from a real CFZ4 pack."""
    import lz4.block
    with open(path, "rb") as f:
        d = f.read()
    if d[:4] != b"CFZ4":
        raise ValueError("not a CFZ4 pack: %r" % d[:4])
    _v, count, fb = struct.unpack("<III", d[4:16])
    frames = []
    for i in range(count):
        off, size = struct.unpack("<II", d[16 + i * 8:24 + i * 8])
        frames.append(lz4.block.decompress(d[off:off + size],
                                           uncompressed_size=fb))
    return count, fb, frames


def alpha_mask(img):
    a = np.array(img, dtype=np.uint8)[:, :, 3]
    return a > 8


def score_against(ref_img, cand_img):
    """IoU of the alpha silhouettes, after aligning the bounding boxes.

    Geometry is what matters here: if the transforms are right, the silhouette
    lands on the silhouette. Colour comes later.
    """
    ra, ca = alpha_mask(ref_img), alpha_mask(cand_img)
    if not ra.any() or not ca.any():
        return 0.0
    def bbox(m):
        ys, xs = np.nonzero(m)
        return xs.min(), ys.min(), xs.max() + 1, ys.max() + 1
    rx0, ry0, rx1, ry1 = bbox(ra)
    cx0, cy0, cx1, cy1 = bbox(ca)
    rw, rh = rx1 - rx0, ry1 - ry0
    cw, ch = cx1 - cx0, cy1 - cy0
    if rw <= 0 or rh <= 0 or cw <= 0 or ch <= 0:
        return 0.0
    # Resample the candidate's crop onto the reference crop's grid.
    scaled = cand_img.crop((cx0, cy0, cx1, cy1)).resize((rw, rh), Image.NEAREST)
    sm = alpha_mask(scaled)
    sub = ra[ry0:ry1, rx0:rx1]
    inter = np.logical_and(sub, sm).sum()
    union = np.logical_or(sub, sm).sum()
    return float(inter) / float(union) if union else 0.0


def flatten_character(atlas, scale=0.5, animations=None, pad=2):
    """-> (frames, w, h) as flat images for the whole character.

    Every animation is expanded frame by frame. With `inner=first` each
    keyframe names its own inner frame, so the expansion is just the timeline's
    own frame range -- no interpolation is invented.

    The canvas is the union of every frame's bounding box across ALL animations,
    so one pack can hold the character without frames shifting between packs.
    """
    anims = atlas.animations()
    if animations:
        anims = {k: v for k, v in anims.items() if k in animations}
    rendered = []
    for name, (start, length) in anims.items():
        for k in range(length):
            img, _box = atlas.render(start + k)
            if img is None:
                continue
            rendered.append((name, k, img))
    if not rendered:
        return [], 0, 0

    # Union bbox in document space, then scale it.
    gx0 = min(b[0] for _, _, (i, b) in [((n, k, img), None) for n, k, img in []] ) if False else None
    return rendered, 0, 0


def pack_from_render(rendered, boxes, scale, bpp, out_path):
    """Compose the union canvas, scale, and write CFZ4 parts."""
    x0 = min(b[0] for b in boxes)
    y0 = min(b[1] for b in boxes)
    x1 = max(b[2] for b in boxes)
    y1 = max(b[3] for b in boxes)
    w = max(1, int(round((x1 - x0) * scale)))
    h = max(1, int(round((y1 - y0) * scale)))
    out = []
    for (name, k, img), b in zip(rendered, boxes):
        canvas = Image.new("RGBA", (x1 - x0, y1 - y0), (0, 0, 0, 0))
        canvas.paste(img, (b[0] - x0, b[1] - y0))
        if (w, h) != canvas.size:
            canvas = canvas.resize((w, h), Image.NEAREST)
        out.append(canvas)
    return out, w, h


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--assets", required=True)
    ap.add_argument("--character", required=True)
    ap.add_argument("--animation", default=None)
    ap.add_argument("--out", default=None)
    ap.add_argument("--reference", default=None,
                    help="a real CFZ4 pack to score against (--fit)")
    ap.add_argument("--fit", action="store_true",
                    help="try every setting combination and report the score")
    ap.add_argument("--pack", default=None,
                    help="flatten EVERY animation and write CFZ4 parts here")
    ap.add_argument("--scale", type=float, default=0.5,
                    help="output scale (0.5 is the game's own display scale)")
    ap.add_argument("--bpp", type=int, default=16, choices=(16, 32))
    args = ap.parse_args()

    cj = os.path.join(DATA, "characters", args.character + ".json")
    with open(cj) as f:
        meta = json.load(f)
    atlas = Atlas(args.assets, meta.get("assetPath") or args.character)

    anims = atlas.animations()
    print("atlas: %s" % os.path.relpath(atlas.dir, args.assets))
    print("document %sx%s, %d symbols, %d sprites"
          % (atlas.meta.get("W"), atlas.meta.get("H"),
             len(atlas.symbols), len(atlas.sprites)))
    print("animations in the timeline: %d" % len(anims))
    for n, (i, du) in list(anims.items())[:12]:
        print("  %-24s start %3d  length %3d" % (n, i, du))

    if args.pack:
        import pacote_personagem as pp
        anims = atlas.animations()
        rendered, boxes = [], []
        conta = 0
        for name, (start, length) in anims.items():
            for k in range(length):
                try:
                    img, box = atlas.render(start + k)
                except ValueError as e:
                    print("  %s %d: %s" % (name, k, e))
                    continue
                if img is None:
                    continue
                rendered.append((name, k, img))
                boxes.append(box)
        print("rendered %d frames from %d animations at native size"
              % (len(rendered), len(anims)))
        frames, w, h = pack_from_render(rendered, boxes, args.scale, args.bpp, None)

        print("out canvas: %dx%d at scale %.3f (%d B/frame at %d bpp)"
              % (w, h, args.scale, w * h * (args.bpp // 8), args.bpp))
        blocks = []
        for fr in frames:
            raw = pp.to_ps2_bytes(fr, args.bpp)
            import lz4.block
            blocks.append(lz4.block.compress(raw, store_size=False))
        os.makedirs(os.path.dirname(args.pack) or ".", exist_ok=True)
        parts = pp.write_packs(blocks, w * h * (args.bpp // 8), args.pack)

        # CHP4: o MESMO pacote, mais a tabela de animacoes que o jogo le' em
        # `assets/chars/<id>.chp4`. Aqui a tabela sai de graca e sem adivinhar:
        # `rendered` foi montado animacao por animacao, entao os quadros de cada
        # uma sao um intervalo contiguo. O CFZ4 fica como esta' (o palco o
        # consome); este e' o arquivo do personagem.
        grupos, atual, n = [], None, 0
        for nome, _k, _img in rendered:
            if atual is not None and nome == atual:
                n += 1
                continue
            if atual is not None:
                grupos.append((atual, blocos[conta:conta + n], True))
                conta += n
            atual, n = nome, 1
        if atual is not None:
            grupos.append((atual, blocos[conta:conta + n], True))
        import chp4 as _chp4
        # A taxa: o atlas Animate declara o frameRate do spritemap; quando nao
        # declara, 24 e' o valor do original (CharacterData DEFAULT).
        fps = int(round(float(atlas.meta.get("FRT") or atlas.meta.get("frameRate") or 24)))
        pacote, nq, tabela = _chp4.montar_de_grupos(grupos, w, h, args.bpp,
                                                   args.scale, fps100=fps * 100)
        cab = _chp4.ler(pacote)
        _chp4.conferir(cab)
        destino_chp4 = os.path.splitext(args.pack)[0] + ".chp4"
        with open(destino_chp4, "wb") as f:
            f.write(pacote)
        print("chp4: %s  %d quadros, %d animacoes, canvas %dx%d, escala %.3f, "
              "%d B" % (os.path.basename(destino_chp4), nq, len(tabela), w, h,
                        args.scale, len(pacote)))
        total = sum(t for _p, t, _f, _n in parts)
        print("wrote %d part(s), %d B total, %d B/frame in VRAM"
              % (len(parts), total, w * h * (args.bpp // 8)))
        for p, t, f, n in parts:
            print("  %-40s %9d B  frames %d..%d" % (os.path.basename(p), t, f, f + n - 1))
        return 0

    if not args.fit:
        name = args.animation or (list(anims)[0] if anims else None)
        if name is None:
            sys.exit("no animation to render")
        start, length = anims[name]
        out = args.out or os.path.join(PROJECT, "build", "flat", args.character)
        os.makedirs(out, exist_ok=True)
        for k in range(length):
            img, box = atlas.render(start + k)
            if img is None:
                print("  frame %d: nothing drawn" % k)
                continue
            img.save(os.path.join(out, "%s_%03d.png" % (name, k)))
            print("  %s %3d  %3dx%-3d  bbox %s"
                  % (name, k, img.width, img.height, box))
        return 0

    # ---- fit against the reference
    if not args.reference:
        sys.exit("--fit needs --reference")
    count, fb, frames = read_cfz4(args.reference)
    # The reference's canvas: one dimension is known from frame_bytes and the
    # port's own call site (258x254 for bf).
    ref_w, ref_h = 258, 254
    if fb != ref_w * ref_h * 4:
        for w in range(64, 1025):
            if fb % (w * 4) == 0:
                h = fb // (w * 4)
                if 64 <= h <= 1024:
                    ref_w, ref_h = w, h
                    break
    print()
    print("reference: %d frames of %dx%d (%d B each)"
          % (count, ref_w, ref_h, fb))
    refs = [Image.frombytes("RGBA", (ref_w, ref_h), fr) for fr in frames[:6]]

    best = None
    for order in ("json", "reverse"):
        for inner in ("first", "advance"):
            scores = []
            for k in range(min(6, count)):
                try:
                    img, _box = atlas.render(k, layer_order=order, inner=inner)
                except ValueError as e:
                    print("  %-8s %-8s erro: %s" % (order, inner, e))
                    scores = []
                    break
                if img is None:
                    scores.append(0.0)
                    continue
                scores.append(score_against(refs[k], img))
            if not scores:
                continue
            avg = sum(scores) / len(scores)
            print("  layer-order %-8s inner %-8s  IoU media %.3f  (por quadro %s)"
                  % (order, inner, avg,
                     " ".join("%.2f" % s for s in scores)))
            if best is None or avg > best[0]:
                best = (avg, order, inner)
    if best is None:
        print("no combination rendered anything")
        return 1
    print()
    print("melhor: layer-order=%s inner=%s com IoU media %.3f"
          % (best[1], best[2], best[0]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
