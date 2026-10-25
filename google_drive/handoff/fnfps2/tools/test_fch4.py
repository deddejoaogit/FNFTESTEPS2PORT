#!/usr/bin/env python3
"""Round-trip the real charts through FCH4 and check nothing was dropped.

The v3 converter could only carry one numeric parameter per event, so
FocusCamera's x/y/duration/ease, SetCameraBop's offset/rate and everything
with a string parameter were silently discarded. FCH4 exists to stop that.

This test is deliberately spec-driven: it reads the converter's own EVENT_V4
table and checks each parameter against it, instead of restating the mapping
by hand. A mapping that is wrong in the converter is then wrong here too, so
the two cannot drift apart silently -- but a parameter that the mapping omits
still shows up, because the check walks the JSON's keys and not the table's.

Run from the project root: python3 tools/test_fch4.py
"""
import glob
import json
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import fnf_to_fch3 as C  # noqa: E402

PROJECT = os.path.dirname(HERE)
CHARTS = os.path.join(PROJECT, "assets-data", "charts")
SONGS = os.path.join(PROJECT, "assets-data", "songs")
FIXED = C.FIXED_SCALE


def s32(u):
    """u32 -> signed, for `a` (which holds signed chars and unsigned masks)."""
    return u - 0x100000000 if u >= 0x80000000 else u


def fixed(u):
    """u32 fixed-point -> signed float."""
    return s32(u) / FIXED


def num(x):
    n = C._as_number(x)
    return None if n is None else float(n)


def near(a, b):
    return abs(a - b) <= 0.001


def check_event(rec, ev, strings, blocks, problems):
    """Check one JSON event against the converter's own mapping."""
    name = ev.get("e")
    v = ev.get("v")
    if not isinstance(v, dict):
        return
    sample, etype, p0, p1, p2, a, b, ref = rec
    spec = C.EVENT_V4.get(name)

    # ---- parameters the mapping claims to carry -------------------------
    if spec is not None:
        for i, (key, kind) in enumerate(spec.get("p", [])):
            if key not in v:
                # optional parameter: the original treats an absent duration,
                # x or y as "not specified", so its absence is not a loss
                continue
            got = (p0, p1, p2)[i]
            n = num(v[key])
            if n is None:
                continue
            if kind == "fixed" and not near(fixed(got), n):
                problems.append("%s: %s %g -> %g" % (name, key, n, fixed(got)))
            elif kind == "bool" and bool(got) != bool(n):
                problems.append("%s: %s %r -> %r" % (name, key, v[key], got))
            elif kind == "int" and s32(got) != int(n):
                problems.append("%s: %s %g -> %g" % (name, key, n, s32(got)))

        if "a" in spec:
            key, kind = spec["a"]
            if key in v:
                if kind == "mask":
                    vals = v[key] if isinstance(v[key], list) else [v[key]]
                    want_mask = 0
                    for i, x in enumerate(vals[:32]):
                        if num(x):
                            want_mask |= 1 << i
                    if a != want_mask:
                        problems.append("%s: %s mask %d -> %d"
                                        % (name, key, want_mask, a))
                elif kind == "str":
                    table = (C.MODE_IDS if key == "mode" else
                             C.STRUMLINE_IDS if key == "strumline" else
                             C.TARGET_IDS)
                    if a != table.get(str(v[key])):
                        problems.append("%s: %s %r -> %r" % (name, key, v[key], a))
                else:
                    n = num(v[key])
                    if n is not None and s32(a) != int(n):
                        problems.append("%s: %s %g -> %g" % (name, key, n, s32(a)))

        if spec.get("ease") and "ease" in v:
            got = (b >> C.B_EASE_SHIFT) & C.B_EASE_MASK
            if got >= len(strings) or strings[got] != str(v["ease"]):
                problems.append("%s: ease %r -> string %r"
                                % (name, v["ease"],
                                   strings[got] if got < len(strings) else None))
        if spec.get("easeDir") and "easeDir" in v:
            got = (b >> C.B_EASEDIR_SHIFT) & 0x3
            if got != C.EASEDIR_IDS.get(str(v["easeDir"])):
                problems.append("%s: easeDir %r -> %r"
                                % (name, v["easeDir"], got))
        for bit, key in ((C.B_FORCE, "force"), (C.B_FLIPX, "flipX"),
                         (C.B_ABSOLUTE, "absolute")):
            if spec.get(key) and key in v:
                if bool(v[key]) != bool(b & bit):
                    problems.append("%s: %s %r -> %r"
                                    % (name, key, v[key], bool(b & bit)))
        if spec.get("ref") and spec["ref"][0] in v:
            want = str(v[spec["ref"][0]])
            if not (b & C.B_REF_IS_STRING) or ref >= len(strings):
                problems.append("%s: %s %r has no string ref"
                                % (name, spec["ref"][0], want))
            elif strings[ref] != want:
                problems.append("%s: %s %r -> %r"
                                % (name, spec["ref"][0], want, strings[ref]))

    # ---- parameters the mapping does NOT claim --------------------------
    # These are the ones that must not be quietly lost. An event with no
    # mapping at all is an error unless it is one of the block events.
    claimed = set()
    if spec is not None:
        claimed |= {k for k, _ in spec.get("p", [])}
        if "a" in spec:
            claimed.add(spec["a"][0])
        if spec.get("ref"):
            claimed.add(spec["ref"][0])
        if spec.get("ease"):
            claimed.add("ease")
        if spec.get("easeDir"):
            claimed.add("easeDir")
        for key in ("force", "flipX", "absolute"):
            if spec.get(key):
                claimed.add(key)
    if name not in C.EVENT_V4 and name not in C.EVENT_V4_BLOCKED:
        problems.append("%s: no FCH4 mapping (params: %s)"
                        % (name, sorted(v.keys())))
    for key in v:
        if key not in claimed and name not in C.EVENT_V4_BLOCKED:
            problems.append("%s: parameter %r is dropped by the mapping"
                            % (name, key))


def check_block(rec, ev, strings, blocks, problems):
    """The block events' lists must round-trip through a self-describing block."""
    sample, etype, p0, p1, p2, a, b, ref = rec
    v = ev.get("v") or {}
    if not (b & C.B_REF_IS_BLOCK):
        problems.append("%s: no block reference" % ev.get("e"))
        return
    if ref >= len(blocks):
        problems.append("%s: block index %d out of range" % (ev.get("e"), ref))
        return
    blob = blocks[ref]
    (nparams,) = struct.unpack_from("<I", blob, 0)
    off = 4
    seen = {}
    for _ in range(nparams):
        name_ref, count = struct.unpack_from("<II", blob, off)
        off += 8
        name = strings[name_ref] if name_ref < len(strings) else "?"
        vals = []
        for _ in range(count):
            kind, value = struct.unpack_from("<II", blob, off)
            off += 8
            vals.append((kind, value))
        seen[name] = vals
    if off != len(blob):
        problems.append("%s: block has %d trailing bytes" % (ev.get("e"), len(blob) - off))
    for key, raw in v.items():
        # encode_block wraps scalars in a one-element list, so compare like
        # with like
        want_list = raw if isinstance(raw, list) else [raw]
        if key not in seen:
            problems.append("%s: block is missing %r" % (ev.get("e"), key))
            continue
        if len(seen[key]) != len(want_list):
            problems.append("%s: %s has %d values, JSON has %d"
                            % (ev.get("e"), key, len(seen[key]), len(want_list)))
            continue
        for (kind, got), want in zip(seen[key], want_list):
            n = num(want)
            if n is None:
                continue
            if kind == C.BLOCK_FLOAT and not near(fixed(got), n):
                problems.append("%s: %s %g -> %g"
                                % (ev.get("e"), key, n, fixed(got)))
            elif kind == C.BLOCK_INT and s32(got) != int(n):
                problems.append("%s: %s %g -> %g"
                                % (ev.get("e"), key, n, s32(got)))
            elif kind == C.BLOCK_RGB:
                hexs = str(want).strip().lower().lstrip("#")
                if hexs.startswith("0x"):
                    hexs = hexs[2:]
                if len(hexs) == 8:
                    hexs = hexs[2:]
                if int(hexs, 16) != got:
                    problems.append("%s: %s %r -> 0x%06X"
                                    % (ev.get("e"), key, want, got))
    # booleans in a block must come back as booleans
    for key, vals in seen.items():
        for kind, got in vals:
            if kind == C.BLOCK_BOOL and got not in (0, 1):
                problems.append("%s: %s bool %d is not 0 or 1"
                                % (ev.get("e"), key, got))


def main():
    files = sorted(glob.glob(os.path.join(CHARTS, "*-chart.json")))
    problems = []
    total_events = total_strings = total_blocks = unrepresentable = 0
    ease_names = set()

    for path in files:
        sid = os.path.basename(path)[:-len("-chart.json")]
        meta_path = os.path.join(SONGS, "%s-metadata.json" % sid)
        if not os.path.exists(meta_path):
            problems.append("%s: no metadata file" % sid)
            continue
        chart = json.load(open(path))

        for diff in ("easy", "normal", "hard"):
            data, info = C.convert_v2(meta_path, path, diff, fmt="v4")
            unrepresentable += info["unrepresentable"]
            total_blocks += info["blocks"]
            ver, notes, bpms, secs, evs, end, strings, blocks = C._parse_chart(data)
            if ver != C.VERSION_V4:
                problems.append("%s/%s: expected v4, got %d" % (sid, diff, ver))
                continue
            total_strings += len(strings)
            for s in strings:
                if s in C.EASE_NAMES_SEED:
                    ease_names.add(s)

            json_events = sorted(chart.get("events") or [],
                                 key=lambda e: float(e.get("t", 0.0)))
            if len(json_events) != len(evs):
                problems.append("%s/%s: %d events in JSON, %d in the file"
                                % (sid, diff, len(json_events), len(evs)))
                continue
            total_events += len(evs)

            for rec, ev in zip(evs, json_events):
                if rec[1] == 0:
                    problems.append("%s/%s: event type 0 for %r"
                                    % (sid, diff, ev.get("e")))
                if ev.get("e") in C.EVENT_V4_BLOCKED:
                    check_block(rec, ev, strings, blocks, problems)
                else:
                    check_event(rec, ev, strings, blocks, problems)

    print("charts converted:      %d x 3 difficulties" % len(files))
    print("events checked:        %d" % total_events)
    print("strings interned:      %d (across all files)" % total_strings)
    print("parameter blocks:      %d" % total_blocks)
    print("unrepresentable:       %d" % unrepresentable)
    print("ease names seen:       %d of the 13 expected"
          % len(ease_names & set(C.EASE_NAMES_SEED)))
    if problems:
        print()
        print("FAILURES (%d):" % len(problems))
        for p in problems[:40]:
            print("  " + p)
        if len(problems) > 40:
            print("  ... and %d more" % (len(problems) - 40))
        return 1
    print()
    print("every parameter of every event survived the round trip")
    return 0


if __name__ == "__main__":
    sys.exit(main())
