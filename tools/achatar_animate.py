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
        # Dois formatos de export na mesma arte (ver converte_full).
        if "ANIMATION" in anim and "AN" not in anim:
            anim = converte_full(anim)
        self.doc = anim
        self.an = anim.get("AN") or {}
        self.top = self.an.get("TL", {}) or {}
        self.symbols = {}
        for s in (anim.get("SD", {}).get("S") or []):
            n = s.get("SN")
            if n:
                self.symbols[n] = s
        self.meta = anim.get("MD") or {}
        # `graphic`: timeline em que CADA quadro instancia o mesmo simbolo com
        # FF=0 e DU=1. E' o symbol tipo "graphic" do Flash, que anda um quadro
        # por quadro do pai -- e o export as vezes repete a instancia em vez de
        # esticar a duracao. Sem isso os N quadros desenham o PRIMEIRO quadro do
        # simbolo e a animacao sai congelada (`tankmanEnding`: 304 quadros,
        # todos iguais).
        self.graphic = self._e_grafico()

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
        if not out:
            # ATLAS-EMBRULHO: a timeline de cima nao tem rotulo nenhum; o
            # nome da animacao esta' no `SN` do documento e o conteudo e' UM
            # quadro que instancia um simbolo com a animacao inteira
            # (`week7/erect/cutscene/tankmanEnding`: 1 quadro -> simbolo de 224
            #  quadros, rotulo "tankman stress ending"). Ler isso como "1
            # quadro" geraria uma animacao congelada -- que e' pior do que
            # faltar, porque parece que funcionou.
            sn = self.an.get("SN")
            if sn:
                out[sn] = (0, self.comprimento(self.top))
        return out

    def _e_grafico(self):
        for lay in self.top.get("L") or []:
            frs = lay.get("FR") or []
            if len(frs) < 2:
                continue
            ok = True
            for fr in frs:
                els = fr.get("E") or []
                if len(els) != 1 or int(fr.get("DU") or 1) != 1:
                    ok = False
                    break
                si = els[0].get("SI")
                if not si or int(si.get("FF") or 0) != 0:
                    ok = False
                    break
            if ok:
                return True
        return False

    def comprimento(self, tl, prof=0):
        """Quadros de uma timeline: soma de DU da camada mais longa, descendo
        para o simbolo instanciado quando a timeline e' so' um embrulho."""
        melhor = 0
        for lay in tl.get("L") or []:
            melhor = max(melhor, sum(int(f.get("DU") or 1) for f in lay.get("FR") or []))
        if melhor <= 1 and prof < 8:
            for lay in tl.get("L") or []:
                for fr in lay.get("FR") or []:
                    for el in fr.get("E") or []:
                        si = el.get("SI")
                        if si and si.get("SN") in self.symbols:
                            melhor = max(melhor, self.comprimento(
                                self.symbols[si["SN"]].get("TL") or {}, prof + 1))
        return melhor

    def e_embrulho(self):
        """1 quando a timeline de cima e' um quadro que instancia um simbolo:
        ai' o quadro interno tem de ANDAR junto com o de fora (inner=sync), e
        nao ficar no primeiro quadro do simbolo."""
        n = sum(len(lay.get("FR") or []) for lay in self.top.get("L") or [])
        if n != 1:
            return False
        for lay in self.top.get("L") or []:
            for fr in lay.get("FR") or []:
                for el in fr.get("E") or []:
                    if (el.get("SI") or {}).get("SN") in self.symbols:
                        return True
        return False

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
    def render(self, frame, layer_order="json", inner=None, ss=1):
        if inner is None:
            inner = "graphic" if self.graphic else "first"
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
                    inner_frame = inner_frame_for(si, offset, inner, frame)
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



def _mx_full(m):
    """Matrix3D (4x4) do export FULL -> MX[6] do formato compacto.

    2D, sem ambiguidade: com a forma que `mat_apply` usa
    (x' = a*x + c*y + tx, y' = b*x + d*y + ty), o 4x4 do Animate da'
    a=m00, b=m01, c=m10, d=m11, tx=m30, ty=m31.
    """
    if not m:
        return None
    return [float(m.get("m00", 1.0)), float(m.get("m01", 0.0)),
            float(m.get("m10", 0.0)), float(m.get("m11", 1.0)),
            float(m.get("m30", 0.0)), float(m.get("m31", 0.0))]


def converte_full(doc):
    """Export FULL do Adobe Animate (chaves longas: ANIMATION/TIMELINE/LAYERS/
    Frames/SYMBOL_DICTIONARY) -> a forma COMPACTA que o resto deste arquivo le'
    (AN/TL/L/FR/I/DU/N/E, SI/ASI/MX/SN/FF).

    Existem dois exports na arte do jogo e so' um era lido. Medido: 64 dos 68
    Animation.json do clone sao compactos; 4 sao FULL -- e entre eles esta' o
    `bfFakeOut`, de onde vem o `fakeoutDeath` do BF e do BF do carro. O
    empacotador dizia "prefixo 'fake out death BF' fora da timeline" e o pacote
    ficava sem a animacao. O dado estava la'; o leitor e' que nao entendia o
    formato.
    """
    def tl(t):
        layers = []
        for lay in (t or {}).get("LAYERS") or []:
            frs = []
            for fr in lay.get("Frames") or []:
                els = []
                for el in fr.get("elements") or []:
                    si = el.get("SYMBOL_Instance")
                    if si:
                        els.append({"SI": {"SN": si.get("SYMBOL_name"),
                                           "MX": _mx_full(si.get("Matrix3D")),
                                           "FF": si.get("firstFrame") or 0}})
                        continue
                    asi = el.get("ATLAS_SPRITE_instance")
                    if asi:
                        els.append({"ASI": {"N": asi.get("name"),
                                            "MX": _mx_full(asi.get("Matrix3D"))}})
                frs.append({"I": fr.get("index") or 0,
                            "DU": fr.get("duration") or 1,
                            "N": fr.get("name"),
                            "E": els})
            layers.append({"FR": frs})
        return {"L": layers}

    top = doc.get("ANIMATION") or {}
    tld = tl(top.get("TIMELINE"))
    # O rotulo da animacao nao esta' no FR: no formato FULL ele e' o
    # `SYMBOL_name` do proprio documento. Sem isso `animations()` ficaria vazio
    # e todo prefixo seria "fora da timeline".
    rotulo = top.get("SYMBOL_name")
    if rotulo:
        for lay in tld["L"]:
            for fr in lay["FR"]:
                if fr.get("I") == 0 and not fr.get("N"):
                    fr["N"] = rotulo
                    break
            break
    sd = []
    for sym in (doc.get("SYMBOL_DICTIONARY") or {}).get("Symbols") or []:
        n = sym.get("SYMBOL_name")
        if n:
            sd.append({"SN": n, "TL": tl(sym.get("TIMELINE"))})
    md = doc.get("metadata") or {}
    return {"AN": {"TL": tld}, "SD": {"S": sd},
            "MD": {"W": md.get("width") or 0, "H": md.get("height") or 0}}

def mx_matrix(mx):
    """MX[6] -> (a, b, c, d, tx, ty), exactly as FlxElement.fromJSONEx does.

    SI and ASI both carry MX in this form; the source decodes both identically.
    """
    if mx and len(mx) >= 6:
        return (float(mx[0]), float(mx[1]), float(mx[2]),
                float(mx[3]), float(mx[4]), float(mx[5]))
    return IDENT


def inner_frame_for(si, offset, mode, frame=0):
    ff = int(si.get("FF") or 0)
    if mode == "first":
        return ff
    if mode == "graphic":
        # O symbol "graphic" do Flash anda junto com o pai, um quadro por
        # quadro -- nao reinicia a cada FR.
        return ff + frame
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


def pack_from_render(rendered, boxes, scale, bpp, out_path, origem=None):
    """Compose the union canvas, scale, and write CFZ4 parts.

    `origem`, quando passado, recebe `(x0, y0)` -- o canto do canvas no espaco
    do DOCUMENTO. O canvas e' a uniao das caixas de todos os quadros, entao
    quem desenha precisa saber onde ele comeca: no personagem isso vem dos
    offsets por animacao do JSON; nas animacoes da tela de Resultado (que nao
    tem offsets no JSON) e' este numero que posiciona o sprite.
    """
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
    if origem is not None:
        origem.extend((x0, y0))
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
    ap.add_argument("--char-dir", default=None,
                    help="onde esta' o <character>.json (padrao: "
                         "assets-data/characters; os empacotadores de TELA "
                         "escrevem o JSON deles em build/ e apontam aqui, "
                         "porque aquele diretorio e' a lista de personagens "
                         "do JOGO -- ver tools/gen_char_dados.py)")
    ap.add_argument("--pack", default=None,
                    help="flatten EVERY animation and write CFZ4 parts here")
    ap.add_argument("--scale", type=float, default=0.5,
                    help="output scale (0.5 is the game's own display scale)")
    ap.add_argument("--bpp", type=int, default=16, choices=(16, 32))
    args = ap.parse_args()

    cj = os.path.join(args.char_dir or os.path.join(DATA, "characters"),
                      args.character + ".json")
    with open(cj) as f:
        meta = json.load(f)
    import pacote_personagem as _pp0
    stem0 = meta.get("assetPath") or args.character
    sparrow0 = None
    try:
        atlas = Atlas(args.assets, stem0)
    except FileNotFoundError as e:
        # O asset do char JSON pode ser um atlas SPARROW (PNG+XML, sem
        # Animation.json). O laco de animacoes ja' sabia empacotar isso quando
        # o sparrow era um asset SECUNDARIO; o que faltava era o primeiro.
        # (Quem usa isto: as animacoes da tela de Resultado, onde o GOOD do bf
        # tem um `resultsGOOD/resultGirlfriendGOOD` Sparrow ao lado do BF.)
        sp, erro = _pp0.atlas_nomeado(args.assets, stem0)
        if not sp:
            raise
        atlas = None
        sparrow0 = ("sparrow", sp)
        print("atlas: %s (SPARROW -- %s)" % (stem0, erro or "PNG+XML"))
        print("sparrow: %d quadros" % len(sp))

    if atlas is None:
        anims = {}
        meta_atlas = {}
    else:
        anims = atlas.animations()
        meta_atlas = atlas.meta or {}
        print("atlas: %s" % os.path.relpath(atlas.dir, args.assets))
        print("document %sx%s, %d symbols, %d sprites"
              % (meta_atlas.get("W"), meta_atlas.get("H"),
                 len(atlas.symbols), len(atlas.sprites)))
    print("animations in the timeline: %d" % len(anims))
    for n, (i, du) in list(anims.items())[:12]:
        print("  %-24s start %3d  length %3d" % (n, i, du))

    if args.pack:
        import pacote_personagem as pp
        # `anims` ja' foi calculado acima (e pode ser vazio quando o atlas de
        # entrada e' um SPARROW -- nesse caso o laco de animacoes usa o
        # sparrow). Recalcular aqui com `atlas.animations()` estourava com
        # `atlas = None` no caso Sparrow.

        # O NOME QUE O JOGO PEDE NAO E' O NOME DO ATLAS.
        # A timeline rotula as animacoes ("Idle", "Up", "Cheer", "Fear"); o jogo
        # pede `danceLeft`, `singUP`, `drop70` -- os nomes do `character.json`.
        # O que liga os dois e' o `prefix` de cada animacao do JSON (e' o `N` da
        # timeline, ver Atlas.animations) e os `frameIndices`, que sao indices
        # DENTRO daquela animacao: e' assim que `danceLeft`/`danceRight` partem
        # o mesmo "Idle" em duas metades. Empacotar com os nomes do atlas (como
        # esta versao fazia) gera um pacote que o jogo abre e nao acha nada.
        # MAIS DE UM ATLAS POR PERSONAGEM. Dez personagens declaram, por
        # animacao, um `assetPath` proprio: o BF tem bf-death/bfFakeOut alem do
        # corpo, e o Tankman tem extra-animations (ugh, pretty good, laugh,
        # beat it, argh). Ler so' o atlas do personagem deixava 10 dos 46
        # pacotes sem essas animacoes -- e' o que o check_char_packs chamava de
        # "faltam animacoes". Os atlases sao abertos sob demanda e guardados.
        atlases = {stem0: (atlas if atlas is not None else sparrow0)}
        planos, faltando, de_outro = [], [], []
        for an in (meta.get("animations") or []):
            nome = an.get("name")
            if not nome:
                continue
            stem = an.get("assetPath") or meta.get("assetPath") or args.character
            if stem not in atlases:
                try:
                    atlases[stem] = Atlas(args.assets, stem)
                except FileNotFoundError as e:
                    # O `assetPath` de UMA animacao pode ser um atlas SPARROW
                    # (PNG+XML) dentro de um personagem Animate: os
                    # `sing*-censor` do pico-playable vem de
                    # `characters/Pico_Censored`. Nao e' dado ausente -- e'
                    # outro formato de atlas, e o leitor de plano
                    # (`pacote_personagem._atlas`) ja' sabe le-lo. Devolve a
                    # lista de quadros (nome, img, ox, oy).
                    sp, erro = pp.atlas_nomeado(args.assets, stem)
                    if sp:
                        atlases[stem] = ("sparrow", sp)
                    else:
                        faltando.append((nome, "%s (sparrow: %s)" % (e, erro)))
                        continue
            at = atlases[stem]
            if isinstance(at, tuple):
                # SPARROW: sem timeline. O prefixo do JSON casa com o comeco do
                # NOME de cada quadro do XML, e `frameIndices` indexa essa lista
                # (mesma regra do pacote_personagem).
                pre = an.get("prefix") or nome
                nomeados = [f for f in at[1] if f[0].startswith(pre)]
                if not nomeados:
                    faltando.append((nome, "prefixo %r nao existe no sparrow"
                                     % pre))
                    continue
                idx = an.get("frameIndices")
                if idx:
                    use = [i for i in idx if isinstance(i, int)
                           and 0 <= i < len(nomeados)]
                    if len(use) != len(idx):
                        faltando.append((nome, "%d indices fora do sparrow"
                                         % (len(idx) - len(use))))
                    if not use:
                        continue
                    nomeados = [nomeados[i] for i in use]
                planos.append({"nome": nome, "looped": an.get("looped", True),
                               "sparrow": nomeados, "atlas": None})
                continue
            labs = anims if (atlas is not None and at is atlas) else at.animations()
            pre = an.get("prefix") or nome
            # O export as vezes deixa ESPACO no rotulo da timeline ("up ",
            # "down " no pico-holding-nene) e o character.json pede o nome sem
            # ele. Casar pelo nome aparado e' o que faz a animacao existir; o
            # casamento e' REPORTADO e o rotulo usado e' o do atlas, nunca um
            # nome inventado.
            if pre not in labs:
                aparado = {k.strip(): k for k in labs}
                if pre.strip() in aparado:
                    pre = aparado[pre.strip()]
                    print("  %s: prefixo casado com espaco (%r)" % (nome, pre))
            if pre not in labs:
                faltando.append((nome, "prefixo %r fora da timeline de %s"
                                 % (pre, os.path.basename(at.dir))))
                continue
            if at is not atlas:
                de_outro.append(nome)
            ini, tam = labs[pre]
            idx = an.get("frameIndices")
            if not idx:
                idx = list(range(tam))
            use = [i for i in idx if isinstance(i, int) and 0 <= i < tam]
            if len(use) != len(idx):
                faltando.append((nome, "%d indices fora de %r"
                                 % (len(idx) - len(use), pre)))
            if not use:
                continue
            planos.append({"nome": nome, "looped": an.get("looped", True),
                           "idx": [ini + i for i in use], "atlas": at,
                           "inner": "graphic" if at.graphic else "first"})
        if not planos:
            raise SystemExit("%s: nenhuma animacao do character.json existe no "
                             "atlas (%r)" % (args.character, faltando))
        if faltando:
            print("faltando no atlas: %s"
                  % ", ".join("%s (%s)" % f for f in faltando))
        if de_outro:
            # Estes quadros vem de OUTRO atlas, e o JSON da' offsets proprios
            # para eles. O pacote guarda as animacoes (sem elas o kind nao toca
            # nada), mas o desenho ainda usa os offsets do PERSONAGEM: aplicar
            # os do JSON por animacao e' o passo seguinte, e fica registrado
            # aqui em vez de passar calado.
            print("de outro atlas (%d): %s -- as animacoes entram; os "
                  "offsets por animacao vem do assets-data/characters" %
                  (len(de_outro), ", ".join(de_outro)))

        rendered, boxes, dono = [], [], []
        for plano in planos:
            if plano.get("sparrow") is not None:
                for (_n, img, ox, oy) in plano["sparrow"]:
                    if img is None:
                        continue
                    rendered.append((plano["nome"], len(dono), img))
                    boxes.append((-ox, -oy, -ox + img.width, -oy + img.height))
                    dono.append(plano["nome"])
                continue
            for i in plano["idx"]:
                try:
                    img, box = plano["atlas"].render(i, inner=plano.get("inner"))
                except ValueError as e:
                    print("  %s quadro %d: %s" % (plano["nome"], i, e))
                    continue
                if img is None:
                    continue
                rendered.append((plano["nome"], len(dono), img))
                boxes.append(box)
                dono.append(plano["nome"])
        print("rendered %d frames for %d animations at native size"
              % (len(rendered), len(planos)))
        origem = []
        frames, w, h = pack_from_render(rendered, boxes, args.scale, args.bpp,
                                        None, origem)

        print("out canvas: %dx%d at scale %.3f (%d B/frame at %d bpp)"
              % (w, h, args.scale, w * h * (args.bpp // 8), args.bpp))
        print("origem do canvas: %.1f,%.1f (canto no espaco do documento)"
              % (origem[0], origem[1]))
        blocks = []
        import lz4.block
        for fr in frames:
            raw = pp.to_ps2_bytes(fr, args.bpp)
            blocks.append(lz4.block.compress(raw, store_size=False))
        os.makedirs(os.path.dirname(args.pack) or ".", exist_ok=True)
        parts = pp.write_packs(blocks, w * h * (args.bpp // 8), args.pack)

        # CHP4: o MESMO pacote, mais a tabela de animacoes que o jogo le' em
        # `assets/chars/<id>.chp4`. A tabela sai da ORDEM em que os quadros foram
        # pintados (um grupo por animacao do JSON), sem adivinhar nada.
        grupos = []
        for plano in planos:
            grupo = [b for b, d in zip(blocks, dono) if d == plano["nome"]]
            if grupo:
                grupos.append((plano["nome"], grupo, plano["looped"]))
        import chp4 as _chp4
        # A taxa: o atlas Animate declara o frameRate do spritemap; quando nao
        # declara, 24 e' o valor do original (CharacterData DEFAULT).
        fps = int(round(float(meta_atlas.get("FRT") or meta_atlas.get("frameRate") or 24)))
        # Os grupos levam os LZ4 ja' prontos, na ordem em que os quadros foram
        # comprimidos para o CFZ4: o CHP4 nao recomprime nada (mesmo resultado,
        # metade do tempo -- cada personagem leva minutos).
        pacote, nq, tabela = _chp4.montar_de_grupos(grupos, w, h, args.bpp,
                                                   args.scale, fps100=fps * 100,
                                                   ja_comprimido=True)
        cab = _chp4.ler(pacote)
        _chp4.conferir(cab)
        destino_chp4 = os.path.splitext(args.pack)[0] + ".chp4"
        with open(destino_chp4, "wb") as f:
            f.write(pacote)
        print("chp4: %s  %d quadros, %d animacoes, canvas %dx%d, escala %.3f, "
              "%d B" % (os.path.basename(destino_chp4), nq, len(tabela), w, h,
                        args.scale, len(pacote)))
        total = sum(t for _p, t, _f, _n in parts)
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
