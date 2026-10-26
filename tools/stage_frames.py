#!/usr/bin/env python3
"""One rule for "which frames does a stage prop have", shared by the generator
and the packer.

WHY THIS MODULE EXISTS
----------------------
A stage prop's frame list can be derived in two places -- when the C header is
generated and when the pack is written. If those two derivations ever disagree,
the header tells the game there are N frames and the pack holds M, and the
failure shows up as a prop that freezes or reads past its frames on the console.
So there is exactly one implementation, and both callers use it.

It also explains what each renderType means, because they are not the same:

  sparrow   PNG + XML; an animation picks frames by prefix, and the JSON's
            frameIndices are indices into the atlas order.
  packer    PNG + .txt ("name = x y w h"); same index rule as sparrow.
  (none)    a plain image prop with no animation: exactly one frame.
  animateatlas
            a composite: the animation's prefix names a SYMBOL inside the
            Animate timeline, and the frames are that symbol's own timeline,
            composited by tools/achatar_animate.py. The PS2 cannot run this,
            so it is flattened here, offline, exactly like the characters.
"""

import glob
import json
import os
import sys

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import pacote_personagem as pp
import achatar_animate as aa


class Frame:
    """One rendered frame: an RGBA image plus where it sits inside the prop."""

    __slots__ = ("img", "ox", "oy")

    def __init__(self, img, ox=0, oy=0):
        self.img = img
        self.ox = ox
        self.oy = oy


class Anim:
    __slots__ = ("name", "fps", "looped", "frames")

    def __init__(self, name, fps, looped, frames):
        self.name = name
        self.fps = fps
        self.looped = looped
        self.frames = frames


def _sparrow_frames(assets, asset_path, prefix, indices):
    path, why = pp.resolve(assets, asset_path)
    if path is None:
        raise FileNotFoundError(why)
    frames, _ = pp.load_atlas(assets, asset_path)
    if frames is None:
        raise FileNotFoundError(why)
    # pp.load_atlas returns [(img, ox, oy)] in atlas order, the same order the
    # XML lists SubTextures and the same order frameIndices indexes.
    picked = []
    if indices:
        for i in indices:
            if 0 <= i < len(frames):
                picked.append(frames[i])
    else:
        picked = frames
    return [Frame(img, ox, oy) for img, ox, oy in picked]


def _symbol_canvas(atlas, symbol_name):
    """Render every frame of one symbol of an Animate timeline."""
    sym = atlas.symbols.get(symbol_name)
    if sym is None:
        return []
    layers = (sym.get("TL") or {}).get("L") or []
    n = 0
    for lay in layers:
        for fr in lay.get("FR") or []:
            n = max(n, int(fr.get("I", 0)) + max(1, int(fr.get("DU", 1))))
    out = []
    for k in range(n):
        ops = []
        atlas._emit(layers, aa.IDENT, k, "first", ops, 0)
        img, _box = atlas._raster(ops, 1)
        if img is not None:
            out.append(img)
    return out


def _animate_frames(assets, asset_path, prefix):
    atlas = aa.Atlas(assets, asset_path)
    # For an animateatlas PROP the prefix names an animation on the atlas's top
    # timeline, not a symbol. Verified on halloween_bg: its timeline carries
    # exactly the two names the JSON asks for ('static' and 'thunder'), while
    # its symbol table holds 'Symbol 1' and friends, which match nothing.
    anims = atlas.animations()
    start = None
    if prefix in anims:
        start, length = anims[prefix]
    else:
        for name, (s0, n0) in anims.items():
            if name.startswith(prefix):
                start, length = s0, n0
                break
    if start is not None:
        imgs = []
        for k in range(length):
            img, _box = atlas.render(start + k)
            if img is not None:
                imgs.append(img)
        if imgs:
            return [Frame(im) for im in imgs]
        raise FileNotFoundError("animation %r rendered no frames" % prefix)

    # Fall back to a symbol with that name, for atlases that expose one.
    names = [n for n in atlas.symbols if n == prefix] or \
            [n for n in atlas.symbols if n.startswith(prefix)]
    if not names:
        # Last case, measured on two real props (mallXmasErect/bottomBoppers and
        # tankmanBattlefieldErect/guy): the atlas labels nothing at all -- no
        # timeline animations and no symbol matching the prefix -- because the
        # whole timeline is one static composite. Compositing the top timeline
        # is then the only reading that draws the art the original draws, so it
        # is done explicitly and reported, never silently substituted.
        n = 0
        for lay in atlas.top.get("L") or []:
            for fr in lay.get("FR") or []:
                n = max(n, int(fr.get("I", 0)) + max(1, int(fr.get("DU", 1))))
        if n:
            imgs = []
            for k in range(n):
                img, _box = atlas.render(k)
                if img is not None:
                    imgs.append(img)
            if imgs:
                return [Frame(im) for im in imgs]
        raise FileNotFoundError(
            "prefix %r matches neither an animation (%s) nor a symbol in %s"
            % (prefix, ", ".join(sorted(atlas.animations())) or "none", asset_path))
    imgs = _symbol_canvas(atlas, names[0])
    if not imgs:
        raise FileNotFoundError("symbol %r rendered no frames" % names[0])
    # Cropping to a common canvas happens later; here the frames are kept at
    # their natural size with the origin at (0, 0), which is what the symbol
    # timeline's own coordinates already are.
    return [Frame(im) for im in imgs]


def plan_prop(assets, prop):
    """-> [Anim] for one stage prop, in the JSON's animation order.

    A prop with no animations gets a single unnamed animation holding its one
    static frame, so the C side never needs a special case.
    """
    asset = prop.get("assetPath")
    kind = prop.get("animType")
    anims = prop.get("animations") or []

    if kind == "animateatlas":
        out = []
        for a in anims:
            prefix = a.get("prefix") or ""
            frames = _animate_frames(assets, asset, prefix)
            out.append(Anim(a.get("name") or "idle",
                            float(a.get("frameRate") or 24.0),
                            0 if a.get("looped") is False else 1, frames))
        if not out:
            raise FileNotFoundError("animateatlas prop %r has no animations"
                                    % prop.get("name"))
        return out

    if kind in ("sparrow", "packer") or anims:
        out = []
        for a in anims:
            indices = a.get("frameIndices") or []
            frames = _sparrow_frames(assets, asset, a.get("prefix"), indices)
            if not frames:
                raise FileNotFoundError("prop %r animation %r selected no frames"
                                        % (prop.get("name"), a.get("name")))
            out.append(Anim(a.get("name") or "idle",
                            float(a.get("frameRate") or 24.0),
                            0 if a.get("looped") is False else 1, frames))
        if out:
            return out

    # No animation block: a single static image.
    if kind in (None, "", "sparrow", "packer"):
        frames = _sparrow_frames(assets, asset, None, None)
        if not frames:
            raise FileNotFoundError("prop %r has no frames" % prop.get("name"))
        # A static prop only ever draws its first frame.
        return [Anim("idle", 24.0, 1, frames[:1])]

    raise ValueError("prop %r: unhandled animType %r" % (prop.get("name"), kind))


def layout(anims):
    """-> (frames_in_pack_order, canvas_w, canvas_h).

    Every frame of every animation goes in one pack, so the C can play any of
    them. The canvas is the union of all frames' placements, which keeps the
    prop's origin fixed across animations -- exactly the property the original
    relies on when it swaps one animation for another.
    """
    all_frames = []
    x0 = y0 = 1 << 30
    x1 = y1 = -(1 << 30)
    for a in anims:
        for f in a.frames:
            # ox/oy come from Sparrow frameX/frameY, which are floats.
            x0 = min(x0, int(round(f.ox)))
            y0 = min(y0, int(round(f.oy)))
            x1 = max(x1, int(round(f.ox)) + f.img.width)
            y1 = max(y1, int(round(f.oy)) + f.img.height)
            all_frames.append(f)
    if not all_frames:
        return [], 0, 0
    w, h = max(1, x1 - x0), max(1, y1 - y0)
    out = []
    for f in all_frames:
        canvas = Image.new("RGBA", (w, h), (0, 0, 0, 0))
        canvas.paste(f.img, (int(round(f.ox)) - x0, int(round(f.oy)) - y0))
        out.append(canvas)
    return out, w, h
