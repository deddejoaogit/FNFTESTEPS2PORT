#!/usr/bin/env python3
"""Pack every stage's props into CFZ4 and record the resolved plan.

Two outputs, and the split matters:

  build/packs/stages/<stage>/<prop>.cfz4   the pixels the PS2 uploads
  assets-data/stage-props.json             the PLAN: how many frames each
                                           animation has, its fps, whether it
                                           loops, and each prop's canvas size

The plan is committed, the packs are not, because the plan is what the C header
is rendered from and the header has to be checkable offline (without the 2.5 GB
asset clone). This is the same arrangement tools/gen_song_registry.py uses, and
for the same reason: one rendering step, no hand-typed tables, no drift.

The frame rule itself lives in tools/stage_frames.py, which both this packer
and the header generator read, so "how many frames does this animation have"
has exactly one answer.

Sizing is measured, not chosen (docs/ORCAMENTO_IMAGENS.md):
  * one texture per prop -- merging props by scrollFactor was tried and is
    worse (mean scale 0.290 against 0.388);
  * each stage's scale comes from tools/vram_budget.py, the single place the
    VRAM model lives;
  * 16 bpp, which is what lets the characters keep their native size.

Usage:
  python3 tools/pacotar_palcos.py --assets <clone> [--stages bopeebo,philly-nice]
"""

import argparse
import glob
import json
import os
import sys

import lz4.block

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.dirname(HERE)
sys.path.insert(0, HERE)

import pacote_personagem as pp
import stage_frames as sf
import vram_budget as vb
import gen_stage_dados as gsd

DATA = os.path.join(PROJECT, "assets-data")
PLAN = os.path.join(DATA, "stage-props.json")


def stage_limit(bpp):
    fb = vb.FB_COUNT * vb.FB_W * vb.FB_H * (bpp // 8)
    fixed = (vb.FONT_W * vb.FONT_H * 4 + vb.HUD_W * vb.HUD_H * (bpp // 8)
             + vb.NOTE_W * vb.NOTE_H * (bpp // 8))
    return vb.VRAM_TOTAL - fb - fixed - vb.char_frame_bytes(bpp)


def _stage_scale(native, limit):
    """Largest uniform scale whose rounded texture sizes fit `limit`.

    The scale MUST be solved from the geometry that actually gets packed -- the
    union canvas over a prop's animations -- not from the largest single frame.
    The first version of this tool used the frame size, and 17 of the 21 stages
    then overflowed VRAM by 3 KB to 1 MB once the real canvases were built.
    Measuring before writing would have caught it; measuring after did.
    """
    if not native:
        return 1.0
    # The GS cannot sample a texture wider than 1024 px, so the scale is capped
    # by the WIDEST prop in the stage. Without this the search happily picked a
    # scale that fit the byte budget while pushing four props past 1024 px wide,
    # and those props were then dropped -- art silently missing from the scene.
    hi = 1.0
    for w, h in native:
        hi = min(hi, 1024.0 / max(1, w), 1024.0 / max(1, h))
    lo = min(0.0001, hi)
    for _ in range(60):
        mid = (lo + hi) / 2.0
        total = sum(max(1, int(round(w * mid))) * max(1, int(round(h * mid))) * 2
                    for w, h in native)
        if total <= limit:
            lo = mid
        else:
            hi = mid
    return lo


def pack_stage(assets, sid, props, limit, out_root, bpp):
    """Pack one stage. -> (entries, bytes_written, problems, scale, vram)."""
    problems = []
    stage_dir = os.path.join(out_root, sid)
    os.makedirs(stage_dir, exist_ok=True)

    # Pass 1: resolve every prop's frames at native size and learn its canvas.
    # The frames are kept so pass 2 does not have to re-render them, which for
    # an animateatlas prop costs minutes.
    prepared = []
    for p in props:
        name = p.get("name") or "prop"
        try:
            anims = sf.plan_prop(assets, p)
            frames, cw, ch = sf.layout(anims)
        except (FileNotFoundError, ValueError) as e:
            problems.append("%s/%s: %s" % (sid, name, e))
            continue
        if not frames:
            problems.append("%s/%s: no frames" % (sid, name))
            continue
        prepared.append((p, name, anims, frames, cw, ch))

    # Pass 2: a escala, dos canvas reais.
    #
    # DUAS contas, e a menor vence, porque sao dois recursos diferentes:
    #   VRAM -> paga UM quadro por prop (o pacote mora na RAM);
    #   RAM  -> paga TODOS os quadros de TODOS os props (o pacote inteiro fica
    #           na memoria do EE enquanto a musica toca).
    # O empacotador so' fazia a primeira. Com a segunda, os quatro palcos que
    # saiam com 20 a 59 MB de pacote (school, schoolErect, limoRideErect,
    # spookyMansion) num console de 32 MiB entram no orcamento.
    canvas = [(cw, ch) for _p, _n, _a, _f, cw, ch in prepared]
    nframes = [len(fr) for _p, _n, _a, fr, cw, ch in prepared]
    f = min(_stage_scale(canvas, limit),
            vb.ram_scale_limit(canvas, nframes, vb.ram_limit_per_stage()))
    if f * max(cw for cw, _ in canvas) > 1024 or f * max(ch for _, ch in canvas) > 1024:
        problems.append("%s: a escala %.3f passa do limite de 1024 px do GS"
                        % (sid, f))

    entries = []
    written = 0
    vram = 0
    keep = set()
    for p, name, anims, frames, cw, ch in prepared:
        keep.add(name + ".cfz4")
        sw = max(1, int(round(cw * f)))
        sh = max(1, int(round(ch * f)))
        if sw > 1024 or sh > 1024:
            # The scale search caps itself at 1024/widest, so reaching here
            # means the cap logic is broken -- say so loudly instead of
            # dropping the prop.
            problems.append("%s/%s: %dx%d passa do limite de textura 1024 "
                            "(o teto da busca de escala falhou)" % (sid, name, sw, sh))
            continue
        blocks = []
        for fr in frames:
            scaled = fr if (sw, sh) == fr.size else fr.resize((sw, sh), pp.Image.NEAREST)
            blocks.append(lz4.block.compress(pp.to_ps2_bytes(scaled, bpp),
                                             store_size=False))
        path = os.path.join(stage_dir, name + ".cfz4")
        parts = pp.write_packs(blocks, sw * sh * (bpp // 8), path)
        for extra in glob.glob(path + ".part*"):
            keep.add(os.path.basename(extra))
        written += sum(t for _p, t, _f, _n in parts)
        vram += sw * sh * (bpp // 8)
        plan_anims = []
        first = 0
        for a in anims:
            plan_anims.append({"name": a.name, "fps": round(a.fps, 4),
                               "looped": a.looped, "first": first,
                               "count": len(a.frames)})
            first += len(a.frames)
        entries.append({
            "name": name, "x": p["x"], "y": p["y"],
            "sx": p["sx"], "sy": p["sy"],
            "scrollx": p["scrollx"], "scrolly": p["scrolly"],
            "z": p["z"], "dance": p["dance"],
            "tex": "stages/%s/%s.cfz4" % (sid, name),
            "frames": len(frames), "sw": sw, "sh": sh,
            "src_w": cw, "src_h": ch,
            "anims": plan_anims,
        })

    if vram > limit:
        problems.append("%s: %d B de textura contra %d B de orcamento"
                        % (sid, vram, limit))

    for fn in os.listdir(stage_dir):
        if fn.endswith(".cfz4") and fn not in keep:
            os.remove(os.path.join(stage_dir, fn))
            problems.append("removed stale %s/%s" % (sid, fn))
    return entries, written, problems, f, vram


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--assets", required=True)
    ap.add_argument("--out", default=os.path.join(PROJECT, "build", "packs"))
    ap.add_argument("--stages", default=None,
                    help="comma separated stage ids; default is all 21")
    ap.add_argument("--bpp", type=int, default=16, choices=(16, 32))
    args = ap.parse_args()

    vb.SIZES_CACHE = vb.load_sizes()
    sizes = vb.SIZES_CACHE
    stages = gsd.collect(sizes)
    if args.stages:
        want = set(s.strip() for s in args.stages.split(",") if s.strip())
        stages = [s for s in stages if s[0] in want]
        if not stages:
            sys.exit("no stage matched %r" % args.stages)

    out_root = os.path.join(args.out, "stages")
    os.makedirs(out_root, exist_ok=True)

    plan = {"bpp": args.bpp,
            "note": "Gerado por tools/pacotar_palcos.py a partir dos pixels "
                    "reais. O header em C e uma renderizacao deste arquivo.",
            "stages": []}
    total_bytes = 0
    problems = []
    limit = stage_limit(args.bpp)
    for sid, zoom, raw_props in stages:
        props = []
        for p in raw_props:
            # The generator's dict carries what the packer needs, already
            # normalised (position/scale/scroll/z), so both sides read one
            # shape instead of re-deriving it from the JSON twice.
            props.append({
                "name": p["name"], "assetPath": p["asset"], "animType": None,
                "animations": None, "x": p["x"], "y": p["y"],
                "sx": p["sx"], "sy": p["sy"], "scrollx": p["scrollx"],
                "scrolly": p["scrolly"], "z": p["z"], "dance": p["dance"],
            })
        # gsd.collect dropped the art metadata, so re-read it from the stage
        # JSON. Keeping the two in step by index would be fragile; matching by
        # name is exact because a prop name is unique inside a stage.
        sj = json.load(open(os.path.join(DATA, "stages", sid + ".json")))
        by_name = {}
        for p in sj.get("props") or []:
            by_name.setdefault(p.get("name"), []).append(p)
        merged = []
        for p in props:
            src = (by_name.get(p["name"]) or [None])[0]
            if src is None:
                problems.append("%s/%s: prop vanished from the JSON"
                                % (sid, p["name"]))
                continue
            q = dict(p)
            q["assetPath"] = src.get("assetPath")
            q["animType"] = src.get("animType")
            q["animations"] = src.get("animations")
            merged.append(q)
        entries, written, probs, f, vram = pack_stage(args.assets, sid, merged,
                                                      limit, out_root, args.bpp)
        total_bytes += written
        problems.extend(probs)
        plan["stages"].append({"id": sid, "zoom": round(zoom, 6),
                               "scale": round(f, 6), "props": entries})
        print("%-22s escala %.3f  %2d props  %8d B VRAM (%.0f%% do orcamento)"
              "  %9d B no disco"
              % (sid, f, len(entries), vram, 100.0 * vram / limit, written))

    with open(PLAN, "w") as fh:
        json.dump(plan, fh, indent=1, sort_keys=True)
        fh.write("\n")

    print()
    print("total no pendrive: %d B em %d palcos" % (total_bytes, len(stages)))
    print("plano escrito em %s" % os.path.relpath(PLAN, PROJECT))
    if problems:
        print()
        print("%d problema(s):" % len(problems))
        for p in problems[:20]:
            print("  " + p)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
