#!/usr/bin/env python3
"""Survey every event in every real chart: which parameters, which values.

The FCH3 v3 event record is 16 bytes with a single u32 value, which cannot
carry the named parameters the v2 charts actually use. Designing v4 means
knowing exactly what has to fit -- not what the docs say fits -- so this
enumerates every event type across all 27 charts and reports the parameter
names, their types, and the distinct values each takes.

Run from the project root: python3 tools/survey_events.py
"""
import glob
import json
import os
import sys
from collections import defaultdict

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.dirname(HERE)
CHARTS = os.path.join(PROJECT, "assets-data", "charts")


def kind(v):
    if isinstance(v, bool):
        return "bool"
    if isinstance(v, int):
        return "int"
    if isinstance(v, float):
        return "float"
    if isinstance(v, str):
        return "str"
    if isinstance(v, list):
        return "list[%d]" % len(v)
    if isinstance(v, dict):
        return "dict"
    return type(v).__name__


def main():
    # type -> param -> set of (kind, sample values)
    seen = defaultdict(lambda: defaultdict(lambda: {"kinds": set(), "vals": set()}))
    counts = defaultdict(int)
    total = 0
    shapes = defaultdict(int)

    for f in sorted(glob.glob(os.path.join(CHARTS, "*-chart.json"))):
        d = json.load(open(f))
        # events are a TOP-LEVEL list shared by all difficulties; notes is a
        # map keyed by difficulty whose values are the notes themselves.
        for ev in (d.get("events") or []):
                total += 1
                name = ev.get("e")
                counts[name] += 1
                v = ev.get("v")
                shapes[kind(v)] += 1
                if isinstance(v, dict):
                    for k, val in v.items():
                        e = seen[name][k]
                        e["kinds"].add(kind(val))
                        if len(e["vals"]) < 12:
                            e["vals"].add(repr(val))

    print("events surveyed: %d across %d charts" % (total, len(glob.glob(os.path.join(CHARTS, "*-chart.json")))))
    print("value shapes: %s" % dict(shapes))
    print()
    print("%-24s %5s  %s" % ("event", "count", "parameters (kind -> distinct values)"))
    print("-" * 100)
    for name in sorted(counts, key=lambda n: -counts[n]):
        params = seen.get(name) or {}
        if not params:
            print("%-24s %5d  (no parameters)" % (name, counts[name]))
            continue
        parts = []
        for k in sorted(params):
            e = params[k]
            vals = ", ".join(sorted(e["vals"])[:6])
            if len(e["vals"]) > 6:
                vals += ", ..."
            parts.append("%s[%s]{%s}" % (k, "/".join(sorted(e["kinds"])), vals))
        print("%-24s %5d  %s" % (name, counts[name], "; ".join(parts)))

    print()
    print("types with no dict parameters (need a bare value or a string):")
    for name in sorted(counts):
        if not seen.get(name):
            print("  %-24s count=%d" % (name, counts[name]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
