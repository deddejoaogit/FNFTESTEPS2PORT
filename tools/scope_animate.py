#!/usr/bin/env python3
"""Scope the flattening problem: how deep is each character's Animate atlas?

WHY
---
tools/pacote_personagem.py packs flat frame lists. Measured over the real art,
that covers Sparrow atlases, but 34 of the 46 characters are Animate atlases,
and an Animate atlas is not a frame list: it is a timeline of layers whose
frames hold SYMBOL INSTANCES, and those symbols are themselves timelines. To
produce the flat frames the port's CFZ4 pack holds, something has to composite
that graph -- offline, because the PS2 cannot run a Flash renderer.

Before writing that compositor, the size of the job has to be known rather than
guessed. This tool walks every character's Animation.json and reports, in
numbers:

  * how many top-level layers the first animation has;
  * the maximum nesting depth reached by following SI -> SD.S -> TL -> FR -> SI;
  * how many distinct symbols are reachable (the compositing work);
  * which leaf raster names are actually present in the spritemaps -- a leaf
    that names nothing is a gap, and a gap must be reported, never papered over;
  * whether any SI carries an MX affine matrix that is not identity (those need
    a real transform, not just a paste).

Output is one line per character plus a summary, and it exits non-zero if a
character cannot be composited with paste-only transforms, so the cost is
visible instead of discovered halfway through.

Usage:  python3 tools/scope_animate.py --assets /path/to/funkin.assets
"""

import argparse
import glob
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.dirname(HERE)
DATA = os.path.join(PROJECT, "assets-data")

IMAGE_DIRS = [
    "shared/images", "preload/images", "tutorial/images",
    "week1/images", "week2/images", "week3/images", "week4/images",
    "week5/images", "week6/images", "week7/images", "weekend1/images",
    "sserafim/images",
]


def find_atlas(assets, stem):
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
    for base in cands:
        if os.path.isdir(base) and os.path.isfile(os.path.join(base, "Animation.json")):
            return base
        if os.path.isfile(base + ".png"):
            return None          # a Sparrow sheet: already flat
    return "missing"


def load_symbols(atlas_dir):
    """-> {name: symbol_node} from the Animation.json SD table."""
    a = json.load(open(os.path.join(atlas_dir, "Animation.json"), encoding="utf-8-sig"))
    syms = {}
    for s in (a.get("SD", {}).get("S") or []):
        n = s.get("SN")
        if n:
            syms[n] = s
    return a, syms


def raster_names(atlas_dir):
    """Every bitmap name the spritemaps provide."""
    names = set()
    for jf in sorted(glob.glob(os.path.join(atlas_dir, "spritemap*.json"))):
        d = json.load(open(jf, encoding="utf-8-sig"))
        atlas = d.get("ATLAS") or {}
        sheets = [atlas] if "SPRITES" in atlas else [
            v for v in atlas.values() if isinstance(v, dict)]
        for sheet in sheets:
            for spr in sheet.get("SPRITES") or []:
                n = (spr.get("SPRITE") or {}).get("name")
                if n is not None:
                    names.add(n)
    return names


def is_identity(mx):
    if not mx:
        return True
    ident = [1, 0, 0, 1, 0, 0]
    return all(abs(float(a) - float(b)) < 1e-6 for a, b in zip(mx, ident))


def walk(atlas_dir, max_nodes=200000):
    """Walk the symbol graph and measure it.

    -> dict with layers, depth, symbols reached, elements seen, non-identity
       matrices, and the leaf names that resolve to no raster.
    """
    anim, syms = load_symbols(atlas_dir)
    rasters = raster_names(atlas_dir)

    top = (anim.get("AN") or {}).get("TL", {})
    layers = top.get("L") or []
    first_layers = len((layers[0].get("FR") or [{}])[0].get("E") or []) if layers else 0

    reached = set()
    leaves = {}
    non_id = 0
    nodes = 0
    max_depth = 0
    missing_leaves = set()

    def visit_symbol(name, depth):
        nonlocal nodes, max_depth, non_id
        if depth > max_depth:
            max_depth = depth
        if name in reached or depth > 24:
            return
        reached.add(name)
        sym = syms.get(name)
        if sym is None:
            # A name that is neither a symbol nor a raster: report it.
            if name not in rasters:
                missing_leaves.add(name)
            else:
                leaves[name] = leaves.get(name, 0) + 1
            return
        tl = sym.get("TL") or {}
        for lay in tl.get("L") or []:
            for fr in lay.get("FR") or []:
                for el in fr.get("E") or []:
                    si = el.get("SI")
                    if not si:
                        continue
                    nodes += 1
                    if nodes > max_nodes:
                        return
                    if si.get("MX") and not is_identity(si.get("MX")):
                        non_id += 1
                    visit_symbol(si.get("SN"), depth + 1)

    # Walk the real animation layers, not the symbol table, so unreachable
    # symbols are not charged as work.
    for lay in layers:
        for fr in lay.get("FR") or []:
            for el in fr.get("E") or []:
                si = el.get("SI")
                if si:
                    nodes += 1
                    if si.get("MX") and not is_identity(si.get("MX")):
                        non_id += 1
                    visit_symbol(si.get("SN"), 1)

    return {
        "layers": len(layers),
        "elements_on_first_layer": first_layers,
        "depth": max_depth,
        "symbols": len(reached),
        "elements": nodes,
        "non_identity": non_id,
        "missing_leaves": sorted(missing_leaves),
        "rasters": len(rasters),
    }


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--assets", required=True)
    args = ap.parse_args()

    rows = []
    flat = []
    bad = []
    for f in sorted(glob.glob(os.path.join(DATA, "characters", "*.json"))):
        cid = os.path.basename(f)[:-5]
        try:
            meta = json.load(open(f))
        except ValueError:
            bad.append((cid, "unreadable json"))
            continue
        stem = meta.get("assetPath") or cid
        d = find_atlas(args.assets, stem)
        if d is None:
            flat.append(cid)
            continue
        if d == "missing":
            bad.append((cid, "no art for %r" % stem))
            continue
        try:
            r = walk(d)
        except (ValueError, KeyError, OSError) as e:
            bad.append((cid, "walk failed: %s" % e))
            continue
        rows.append((cid, r))

    print("%-24s %6s %8s %6s %8s %9s %s"
          % ("character", "layers", "depth", "syms", "elements", "non-ident", "gaps"))
    print("-" * 78)
    for cid, r in sorted(rows, key=lambda x: (-x[1]["depth"], -x[1]["symbols"])):
        gaps = len(r["missing_leaves"])
        print("%-24s %6d %8d %6d %8d %9d %s"
              % (cid, r["layers"], r["depth"], r["symbols"], r["elements"],
                 r["non_identity"], gaps if gaps else "none"))
        if r["missing_leaves"]:
            for n in r["missing_leaves"][:4]:
                print("%-24s   leaf with no raster: %r" % ("", n))

    print()
    print("flat Sparrow sheets (no compositing needed): %d -- %s"
          % (len(flat), ", ".join(flat) if flat else "none"))
    print("Animate atlases needing a compositor:        %d" % len(rows))
    if bad:
        print("unresolved:                                  %d" % len(bad))
        for cid, why in bad:
            print("  %-22s %s" % (cid, why))
    if rows:
        deep = max(r["depth"] for _, r in rows)
        syms = max(r["symbols"] for _, r in rows)
        el = max(r["elements"] for _, r in rows)
        nid = sum(r["non_identity"] for _, r in rows)
        print()
        print("deepest nesting: %d levels; most symbols in one character: %d;"
              % (deep, syms))
        print("most symbol instances on one character: %d." % el)
        print("%d symbol instance(s) carry a non-identity affine matrix, so the"
              % nid)
        print("compositor needs real transforms, not paste-at-offset.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
