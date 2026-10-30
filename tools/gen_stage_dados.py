#!/usr/bin/env python3
"""Generate src/stage_dados.h: every stage prop the game actually draws.

WHY
---
bs_dados.h carries the 21 stages, but only their camera zoom and the three
character slot positions. The 157 texture-bearing props -- position, scale,
scrollFactor, zIndex, animation, and which pack file holds each one -- existed
only inside the JSON. tools/check_bs_dados.py has been printing that gap for a
while ("stage props (position/scale/scroll/zIndex): 169 across 21 stages").
Without them the port can draw exactly one stage (the tutorial's mainStage) and
no other, which is why only the tutorial plays.

This generator closes that gap the same way tools/gen_song_registry.py closed
the song table: the C is a rendering of the game's own JSON, so it cannot drift.

THE SCALE IS MEASURED, NOT CHOSEN
---------------------------------
19 of the 21 stages do not fit VRAM at native size (docs/ORCAMENTO_IMAGENS.md
section 4). Each stage therefore gets a uniform scale `f`, and the choice of
packing model was measured too: merging props by scrollFactor was tried and is
WORSE (mean scale 0.290 against 0.388 for one texture per prop), because it
merges props that are far apart and wastes the gaps. So one texture per prop.
The scale comes from tools/vram_budget.py -- the single place the VRAM model
lives -- and is imported, not re-derived here.

POSITIONS ARE THE ORIGINAL'S
----------------------------
x, y, scale, scroll and zIndex are copied verbatim from the stage JSON. The
port draws the prop at (x, y) in the original's coordinate space; `f` only
shrinks the texture. No position is adjusted to compensate for the scale --
that would move the scene, which the project's rules forbid.

Usage:
  python3 tools/gen_stage_dados.py            # write src/stage_dados.h
  python3 tools/gen_stage_dados.py --check    # verify the committed file
"""

import argparse
import glob
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.dirname(HERE)
STAGES = os.path.join(PROJECT, "assets-data", "stages")
OUT = os.path.join(PROJECT, "src", "stage_dados.h")

sys.path.insert(0, HERE)
import vram_budget as vb


def c_float(v):
    return "%.6ff" % float(v)


def c_str(s):
    return '"%s"' % s.replace("\\", "\\\\").replace('"', '\\"')


def image_dims(entry):
    """Largest single frame -- what one resident texture costs."""
    if not entry:
        return None
    w = entry.get("frame_w") or entry.get("w")
    h = entry.get("frame_h") or entry.get("h")
    if not w or not h:
        return None
    return int(w), int(h)


def collect(sizes):
    """-> [(stage_id, zoom, [prop dict]), ...] in a stable order."""
    vb.SIZES_CACHE = sizes
    out = []
    for f in sorted(glob.glob(os.path.join(STAGES, "*.json"))):
        sid = os.path.basename(f)[:-5]
        d = json.load(open(f))
        props = []
        for p in d.get("props") or []:
            ap = p.get("assetPath")
            if not ap or ap.startswith("#"):
                # A flat colour rectangle, not a texture. The original draws it
                # as a rectangle; it costs no VRAM and needs no pack file.
                continue
            dims = image_dims(sizes.get(ap))
            if not dims:
                # A prop with art that was never measured would silently vanish
                # from the port. Refuse instead.
                sys.exit("stage %s prop %r: no measured size for %r"
                         % (sid, p.get("name"), ap))
            anims = p.get("animations") or []
            frames = 1
            fps = 24.0
            anim_name = None
            if anims:
                a0 = anims[0]
                anim_name = a0.get("name")
                # The JSON's animation entry may carry its own frame list; the
                # atlas itself decides how many frames exist, and the packer
                # writes exactly that many. Store the declared fps so the C can
                # pace it, defaulting to the game's 24.
                fps = float(a0.get("fps") or 24.0)
                fps_list = a0.get("frameIndices") or []
                frames = len(fps_list) if fps_list else 1
            pos = p.get("position") or [0, 0]
            scale = p.get("scale") or [1, 1]
            scroll = p.get("scroll") or [1, 1]
            props.append({
                "name": p.get("name") or "prop",
                "asset": ap,
                "x": float(pos[0]), "y": float(pos[1]),
                "sx": float(scale[0]), "sy": float(scale[1]),
                "scrollx": float(scroll[0]), "scrolly": float(scroll[1]),
                "z": int(p.get("zIndex") or 0),
                "dance": int(p.get("danceEvery") or 0),
                "anim": anim_name or "",
                "fps": fps,
                "frames_declared": frames,
                "sw": dims[0], "sh": dims[1],
            })
        zoom = float(d.get("cameraZoom") or 1.0)
        out.append((sid, zoom, props))
    return out


def stage_scale(sid, sizes, bpp=16):
    """The scale this stage is drawn at, from the shared VRAM model."""
    vb.SIZES_CACHE = sizes
    fb = vb.FB_COUNT * vb.FB_W * vb.FB_H * (bpp // 8)
    fixed = (vb.FONT_W * vb.FONT_H * 4 + vb.HUD_W * vb.HUD_H * (bpp // 8)
             + vb.NOTE_W * vb.NOTE_H * (bpp // 8))
    limit = vb.VRAM_TOTAL - fb - fixed - vb.char_frame_bytes(bpp)
    return vb.max_scale(sid, limit, bpp)


PLAN = os.path.join(PROJECT, "assets-data", "stage-props.json")


def load_plan():
    if not os.path.isfile(PLAN):
        sys.exit("missing %s -- run tools/pacotar_palcos.py --assets <clone>"
                 % os.path.relpath(PLAN, PROJECT))
    with open(PLAN) as f:
        return json.load(f)


def render(plan):
    """Render the C header from the committed plan.

    The plan is produced by tools/pacotar_palcos.py, which is the only thing
    that has seen the real pixels: it knows how many frames each animation has
    and what the prop's canvas came out as. Rendering the header from it means
    the C can never disagree with the pack it will load, and it means this
    generator runs offline, without the 2.5 GB asset clone.
    """
    w = []
    w.append("/* GERADO POR tools/gen_stage_dados.py - NAO EDITAR A MAO.")
    w.append("   Fonte: assets-data/stage-props.json, escrito por")
    w.append("   tools/pacotar_palcos.py a partir dos pixels reais, e")
    w.append("   assets-data/stages (um JSON por palco) para posicoes e zIndex.")
    w.append("")
    w.append("   Uma textura por prop, a %d bpp. A escala de cada palco foi resolvida" % plan["bpp"])
    w.append("   sobre a geometria REALMENTE empacotada (o canvas uniao das animacoes),")
    w.append("   nao sobre o maior quadro: a primeira tentativa usou o maior quadro e 17")
    w.append("   dos 21 palcos estouraram a VRAM. Agrupar props por scrollFactor tambem")
    w.append("   foi medido e e pior (escala media 0.290 contra 0.388).")
    w.append("   Posicoes, scale, scroll e zIndex sao os do JSON, sem compensacao. */")
    w.append("#ifndef STAGE_DADOS_H")
    w.append("#define STAGE_DADOS_H")
    w.append("")
    w.append("/* Uma animacao de um prop: os quadros first..first+count-1 do pacote. */")
    w.append("typedef struct{")
    w.append("  const char*name;")
    w.append("  int first,count;")
    w.append("  float fps;")
    w.append("  int looped;")
    w.append("}bs_prop_anim_t;")
    w.append("")
    w.append("typedef struct{")
    w.append("  const char*stage;   /* palco a que pertence */")
    w.append("  const char*name;    /* nome do prop no JSON */")
    w.append('  const char*tex;     /* "stages/<stage>/<name>.cfz4" sob a raiz */')
    w.append("  float x,y;          /* position, do JSON */")
    w.append("  float sx,sy;        /* scale, do JSON */")
    w.append("  float scrollx,scrolly;")
    w.append("  int z;              /* zIndex */")
    w.append("  int dance;          /* danceEvery, 0 = nunca */")
    w.append("  int frames;         /* quadros no pacote CFZ4 */")
    w.append("  int sw,sh;          /* tamanho de UM quadro, ja na escala do palco */")
    w.append("  int src_w,src_h;    /* tamanho do quadro no espaco do ORIGINAL (1280x720) */")
    w.append("  int anim_first,anim_count;  /* faixa em bs_prop_anims[] */")
    w.append("}bs_prop_t;")
    w.append("")

    anims = []
    rows = []
    per_stage = []
    for st in plan["stages"]:
        sid = st["id"]
        first = len(rows)
        for p in st["props"]:
            afirst = len(anims)
            for a in p["anims"]:
                anims.append(a)
            rows.append({
                "stage": sid, "name": p["name"], "tex": p["tex"],
                "x": p["x"], "y": p["y"], "sx": p["sx"], "sy": p["sy"],
                "scrollx": p["scrollx"], "scrolly": p["scrolly"],
                "z": p["z"], "dance": p["dance"], "frames": p["frames"],
                "sw": p["sw"], "sh": p["sh"],
                "src_w": p["src_w"], "src_h": p["src_h"],
                "af": afirst, "ac": len(p["anims"]),
            })
        per_stage.append((sid, st["zoom"], st["scale"], first, len(rows) - first))

    w.append("/* Animacoes de todos os props. */")
    w.append("static const bs_prop_anim_t bs_prop_anims[]={")
    for a in anims:
        w.append('  {%s,%d,%d,%s,%d},'
                 % (c_str(a["name"]), a["first"], a["count"],
                    c_float(a["fps"]), a["looped"]))
    w.append("};")
    w.append("#define BS_PROP_ANIM_COUNT %d" % len(anims))
    w.append("")
    w.append("/* Todos os props, agrupados por palco. */")
    w.append("static const bs_prop_t bs_props[]={")
    for r in rows:
        w.append('  {%s,%s,%s,%s,%s,%s,%s,%s,%s,%d,%d,%d,%d,%d,%d,%d,%d,%d},'
                 % (c_str(r["stage"]), c_str(r["name"]), c_str(r["tex"]),
                    c_float(r["x"]), c_float(r["y"]),
                    c_float(r["sx"]), c_float(r["sy"]),
                    c_float(r["scrollx"]), c_float(r["scrolly"]),
                    r["z"], r["dance"], r["frames"], r["sw"], r["sh"],
                    r["src_w"], r["src_h"],
                    r["af"], r["ac"]))
    w.append("};")
    w.append("#define BS_PROP_COUNT %d" % len(rows))
    w.append("")
    w.append("/* Um palco: zoom da camera, escala aplicada aos props, e a faixa em")
    w.append("   bs_props[]. A escala fica aqui para o loader converter uma posicao do")
    w.append("   espaco original. */")
    w.append("typedef struct{const char*id;float zoom;float scale;int first;int count;}bs_stage_props_t;")
    w.append("static const bs_stage_props_t bs_stage_props[]={")
    for sid, zoom, f, first, count in per_stage:
        w.append('  {%s,%s,%s,%d,%d},' % (c_str(sid), c_float(zoom), c_float(f),
                                          first, count))
    w.append("};")
    w.append("#define BS_STAGE_PROPS_COUNT %d" % len(per_stage))
    w.append("")
    w.append("#endif /* STAGE_DADOS_H */")
    return "\n".join(w) + "\n", len(rows), len(per_stage), len(anims)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true")
    args = ap.parse_args()

    plan = load_plan()
    text, nprops, nstages, nanims = render(plan)

    if args.check:
        if not os.path.isfile(OUT):
            print("missing %s -- run without --check" % os.path.relpath(OUT, PROJECT))
            return 1
        have = open(OUT).read()
        if have != text:
            print("src/stage_dados.h does NOT match assets-data/")
            import difflib
            for line in list(difflib.unified_diff(
                    have.splitlines(), text.splitlines(),
                    "committed", "regenerated", lineterm=""))[:40]:
                print(line)
            return 1
        print("stage_dados.h matches assets-data/ (%d stages, %d props, "
              "%d animations)" % (nstages, nprops, nanims))
        return 0

    with open(OUT, "w") as f:
        f.write(text)
    print("wrote %s: %d stages, %d texture props, %d animations"
          % (os.path.relpath(OUT, PROJECT), nstages, nprops, nanims))
    return 0


if __name__ == "__main__":
    sys.exit(main())
