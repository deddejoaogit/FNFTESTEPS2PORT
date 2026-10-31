#!/usr/bin/env python3
"""
fnf_to_fch3.py - Friday Night Funkin' v0.8.6 chart -> PS2 FCH3 converter.

Part of the FNF v0.8.6 -> PS2 port data pipeline (plan section 4.4).
Converts the original game's song/chart JSON into the port's proven binary
chart container "FCH3" (recovered from the v0.1.99-fix27 implementation).

FCH3 container layout (little-endian, FNV-1a validated):
  header  40 B: b"FCH3" | u16 version=3 | u16 note_stride=12 | u32 note_count
               | u16 bpm_count | u16 section_count | u16 event_count | u16 rsv
               | u16 bpm_stride=16 | u16 section_stride=8 | u16 event_stride=16
               | u16 rsv | u32 fnv1a(payload) | u32 song_end_sample | u32 rsv
  note    12 B: u32 sample | u32 duration | u8 lane(0..3) | u8 must_press | u16 order
  bpm     16 B: u32 start_step | u32 start_sample | u32 samples_per_step | u32 bpm
  section  8 B: u32 start_step | u8 must_hit | u8 length_steps | u16 order
  event   16 B: u32 sample | u32 type | u32 value | u32 order
             value packs up to two numeric event values: value1 in the low
             16 bits, value2 in the high 16 bits (FocusCamera carries char).

Input formats supported (both produced by FNF v0.8.6):
  * legacy : {"song": {"bpm":..,"speed":..,"notes":[{mustHitSection,sectionNotes:
              [[t,d,len],..]}], "events":[[name,t,v1,v2],..]}}
  * v2     : metadata.json (SongMetadata) + charts/<difficulty>.json (SongChartData)

Semantics taken from the original source:
  * Note times are milliseconds (SongMetadata.timeFormat = 'ms').
  * SongTimeChange.timeStamp is a timestamp in the song's timeFormat, i.e.
    MILLISECONDS (not steps) - funkin.data.song.SongTimeChange.
  * Legacy BPM changes are applied at the first note's timestamp of the section
    that carries changeBPM (FNFLegacyImporter.rebuildTimeChanges).
  * Legacy note data: 0..3 player strumline (left,down,up,right), 4..7 opponent
    (floor(data/4) = strumline index). Legacy charts flip 0..3 <-> 4..7 when
    mustHitSection is false. data < 0 are Psych event notes (skipped).
  * Legacy sections carry mustHitSection and lengthInSteps (default 16).

Usage:
  python3 fnf_to_fch3.py --legacy song.json --difficulty hard -o hard.chart
  python3 fnf_to_fch3.py --metadata metadata.json --chart chart.json -o chart.bin
  python3 fnf_to_fch3.py --selftest
"""
import argparse
import json
import struct
import sys

SAMPLE_RATE = 48000
STRUMLINE_SIZE = 4          # lanes per strumline
STEPS_PER_BEAT = 4
DEFAULT_BPM = 100.0         # funkin.data.song.SongTimeChange.DEFAULT
SECTION_STEPS = 16          # FNF chart editor section length
DEFAULT_DIFFICULTY = "normal"

# FNF song event -> port FCH3 event type id.
#
# These ids are NOT free to choose: they are the ones the PS2 loader already
# switches on. funkin/src/main.c, in the ui_state==8 event pump:
#   type 1 -> g_tut_cam_focus = (value != 0)
#   type 2 -> g_tut_zoom_to  = (value & 0xFFFF) / 1000
#   type 4 -> g_tut_scroll_to = (value & 0xFFFF) / 1000
# The earlier table had ChangeCharacter on 2 and AltIdleAnimation on 4, and no
# ZoomCamera or ScrollSpeed at all -- so converting a real v2 chart produced an
# UNKNOWN event where the port expected the zoom, and the camera never moved.
#
# The ids from 7 up come from the event inventory of all 27 shipped charts (see
# docs/SOURCE_ANALYSIS_0.2.105.md section 8). They are assigned here so the
# port can tell them apart; which of them it actually implements is a separate
# question, and one this file does not answer.
EVENT_TYPES = {
    "FocusCamera": 1,
    "ZoomCamera": 2,
    "PlayAnimation": 3,
    "ScrollSpeed": 4,
    "ChangeCharacter": 5,
    "AltIdleAnimation": 6,
    "SetCameraBop": 7,
    "SetHealthIcon": 8,
}

# The sserafim week's songs are scripted: their .hxc files emit their own event
# names. They get ids in their own block so they can never collide with a
# base-game id, and so a future interpreter can dispatch on the range.
EVENT_TYPE_SCRIPTED = 0x0100
SCRIPTED_EVENT_IDS = {}

EVENT_TYPE_UNKNOWN = 0xFFFF

# ---------------------------------------------------------------------------
# FCH4 field tables.
#
# These are built from tools/survey_events.py's output over the 27 real
# charts, not from the v2 spec. Every value below is one that actually occurs;
# a value that does not occur is not given an id here, it is appended on first
# sight and reported, so no parameter is silently dropped.
# ---------------------------------------------------------------------------

# FlxEase names seen in the 27 charts. Order is alphabetical so a given chart
# produces the same indices on every run.
EASE_NAMES_SEED = [
    "CLASSIC", "INSTANT", "circ", "elastic", "elasticInOut", "expo", "expoIn",
    "expoOut", "linear", "quad", "quadInOut", "quart", "quintInOut",
]
EASE_IDS = {n: i for i, n in enumerate(EASE_NAMES_SEED)}
EASE_IDS_SEEN_EXTRA = []

EASEDIR_IDS = {"In": 0, "Out": 1, "InOut": 2}
MODE_IDS = {"direct": 0, "stage": 1}
STRUMLINE_IDS = {"both": 0, "opponent": 1, "player": 2}
# PlayAnimation.target names the character; "boyfriend" is an alias of "bf".
TARGET_IDS = {"bf": 0, "boyfriend": 0, "dad": 2, "gf": 3}

# flag bits in the event record's `b` word
B_EASE_SHIFT = 0
B_EASE_MASK = 0xFFFF
B_EASEDIR_SHIFT = 16
B_FORCE = 1 << 18
B_FLIPX = 1 << 19
B_ABSOLUTE = 1 << 20
B_REF_IS_STRING = 1 << 21
B_REF_IS_BLOCK = 1 << 22
# Set when the chart carried NO `ease` field. The ease field in `b` is an index
# into this file's string table, and index 0 is a real name, so "absent" cannot
# be told from "CLASSIC" by the value -- and absent means the CLASS default
# (FocusCamera 'CLASSIC', ZoomCamera/ScrollSpeed 'linear'), not whichever string
# the table happened to intern first.
B_EASE_ABSENT = 1 << 23

# value kinds in a parameter block
BLOCK_FLOAT, BLOCK_INT, BLOCK_RGB, BLOCK_BOOL = 0, 1, 2, 3


def small_id(table, value, what):
    """Index into a small enum table, or 0 with a note if the value is new."""
    if value is None:
        return 0
    key = str(value)
    if key in table:
        return table[key]
    print("  note: %s value %r is not in the known set; stored as 0"
          % (what, key), file=sys.stderr)
    return 0


def fixed(x):
    """float -> u32 fixed-point x1000."""
    return int(round(float(x) * FIXED_SCALE)) & 0xFFFFFFFF


def raw_int(x):
    return int(x) & 0xFFFFFFFF


def as_bool(x):
    n = _as_number(x)
    return 1 if n else 0

# FCH3 packs an event's parameter in one u32: the primary value in the low 16
# bits and the duration (in chart steps) in the high 16. The loader reads only
# the low half today, so the high half is spare room that costs nothing and
# lets the port honour per-song durations later.
FIXED_SCALE = 1000          # zoom/scroll/intensity are stored x1000

# Which field of each event's dict is the primary one, and how to read it.
# "fixed" means x1000 as a float; "int" means a plain integer; "bool" 0/1.
# Anything not listed packs 0 and is reported, rather than guessed at.
EVENT_PRIMARY = {
    "FocusCamera": ("char", "int"),
    "ZoomCamera": ("zoom", "fixed"),
    "ScrollSpeed": ("scroll", "fixed"),
    "SetCameraBop": ("intensity", "fixed"),
    "SetHealthIcon": ("char", "int"),
    "PlayAnimation": (None, "none"),
    "ChangeCharacter": ("char", "int"),
    "AltIdleAnimation": (None, "none"),
    "sserafimSing": ("singing", "bool"),
    "sserafimShow": ("visible", "bool"),
    "sserafimCover": ("visible", "bool"),
    "sserafimDark": ("amount", "fixed"),
    "sserafimLights": ("amount", "fixed"),
    "sserafimFlash": ("duration", "int"),
    "sserafimGuitarVibration": ("duration", "int"),
    "sserafimKick": ("final", "bool"),
    "sserafimBeautiful": ("beautiful", "bool"),
}


def event_type_id(name):
    """Stable id for an event name, assigning scripted names on first sight."""
    if name in EVENT_TYPES:
        return EVENT_TYPES[name]
    if name in SCRIPTED_EVENT_IDS:
        return SCRIPTED_EVENT_IDS[name]
    if name.startswith("sserafim") or name[0].islower() or name[0] == "_":
        # A script-authored event. Give it a deterministic id in the scripted
        # block, ordered by first appearance, so a re-run is reproducible.
        nid = EVENT_TYPE_SCRIPTED + len(SCRIPTED_EVENT_IDS)
        SCRIPTED_EVENT_IDS[name] = nid
        return nid
    return EVENT_TYPE_UNKNOWN


def pack_event_value(fixed_value, duration_steps=0):
    """(primary value, duration in steps) -> the u32 the loader decodes."""
    v = int(round(fixed_value)) & 0xFFFF
    d = int(round(duration_steps)) & 0xFFFF
    return (d << 16) | v


def _as_number(x):
    """Accept the shapes the charts actually use for a scalar field."""
    if isinstance(x, bool):
        return 1 if x else 0
    if isinstance(x, (int, float)):
        return x
    if isinstance(x, str):
        # "true"/"false" and plain numbers both turn up in the wild
        low = x.strip().lower()
        if low in ("true", "yes", "on"):
            return 1
        if low in ("false", "no", "off", ""):
            return 0
        try:
            return float(x)
        except ValueError:
            return None
    return None


def _event_value(name, v):
    """Encode one event's parameters into the u32 value field.

    The charts use three shapes for `v`:
      * a dict of named parameters (the documented v2 form)
      * a bare int, which older charts use for FocusCamera to mean the char
      * a list, the legacy positional form
    Only the primary parameter survives; the rest is dropped, because FCH3 v3
    gives an event a single u32. That is a real limitation, recorded in
    section 8 of the analysis rather than papered over.
    """
    if isinstance(v, dict):
        key, kind = EVENT_PRIMARY.get(name, (None, "none"))
        dur = _as_number(v.get("duration", 0)) or 0
        if key is None:
            return 0
        raw = _as_number(v.get(key))
        if raw is None:
            return 0
        if kind == "fixed":
            return pack_event_value(float(raw) * FIXED_SCALE, dur)
        return pack_event_value(raw, dur)
    if isinstance(v, bool):
        return pack_event_value(1 if v else 0)
    if isinstance(v, (int, float)):
        # bare int: FocusCamera's char, or a plain flag
        return pack_event_value(v)
    if isinstance(v, (list, tuple)):
        packed = 0
        for i, x in enumerate(list(v)[:2]):
            n = _as_number(x)
            if n is None:
                continue
            packed |= (int(n) & 0xFFFF) << (16 * i)
        return packed
    return 0


# ---------------------------------------------------------------------------
# FCH4 event record layout.
#
#   u32 sample | u32 type | u32 p0 | u32 p1 | u32 p2 | u32 a | u32 b | u32 ref
#
# `a` is the event's primary small integer (char, mode, target, strumline).
# `b` packs ease, easeDir and the booleans. `ref` indexes the string table or
# the block table. p0..p2 are fixed-point x1000.
#
# This table is the whole mapping: every parameter survey_events.py reports
# has a home, and nothing is dropped. An event with no entry is encoded with
# zeroes and reported, never guessed at.
#
# kinds: "fixed" = x1000 float, "int" = raw integer, "bool" = 0/1,
#        "mask"  = a boolean list packed into `a`'s low bits,
#        "str"   = goes in the string table, referenced by `ref`
EVENT_V4 = {
    "FocusCamera": {"p": [("duration", "fixed"), ("x", "fixed"), ("y", "fixed")],
                    "a": ("char", "int"), "ease": True, "easeDir": True},
    "ZoomCamera": {"p": [("duration", "fixed"), ("zoom", "fixed")],
                   "a": ("mode", "str"), "ease": True, "easeDir": True},
    "SetCameraBop": {"p": [("intensity", "fixed"), ("offset", "fixed"),
                           ("rate", "fixed")]},
    "PlayAnimation": {"a": ("target", "str"), "force": True,
                      "ref": ("anim", "str")},
    "SetHealthIcon": {"a": ("char", "int"), "flipX": True,
                      "ref": ("id", "str")},
    "ScrollSpeed": {"p": [("duration", "fixed"), ("scroll", "fixed")],
                    "a": ("strumline", "str"), "ease": True,
                    "absolute": True},
    "ChangeCharacter": {"a": ("char", "int")},
    "AltIdleAnimation": {"ref": ("anim", "str")},
    # sserafimEnd carries no parameters at all; it is a marker, and it needs a
    # mapping so the round-trip test can tell "deliberately empty" from
    # "not handled".
    "sserafimEnd": {},
    # scripted events: the ones whose parameters are scalars map directly;
    # the ones with lists (sserafimPulseLights) go to the block table.
    "sserafimDark": {"p": [("amount", "fixed"), ("duration", "fixed")]},
    "sserafimLights": {"p": [("amount", "fixed"), ("duration", "fixed")]},
    "sserafimGuitarVibration": {"p": [("duration", "fixed")]},
    "sserafimFlash": {"p": [("duration", "fixed")]},
    "sserafimCover": {"p": [("visible", "bool")]},
    "sserafimKick": {"p": [("final", "bool")]},
    "sserafimBeautiful": {"p": [("beautiful", "bool")]},
    # boolean lists are masks, not blocks: 6 and 5 booleans fit in `a`
    "sserafimSing": {"a": ("singing", "mask")},
    "sserafimShow": {"a": ("visible", "mask")},
}

# the only event whose parameters are variable-length lists of numbers
EVENT_V4_BLOCKED = {"sserafimPulseLights"}


class V4Tables:
    """Per-file string and block tables, so ids are stable within a chart."""

    def __init__(self):
        self.strings = []
        self._index = {}
        self.blocks = []

    def string(self, text):
        if text in self._index:
            return self._index[text]
        idx = len(self.strings)
        self.strings.append(str(text))
        self._index[text] = idx
        return idx

    def block(self, blob):
        self.blocks.append(blob)
        return len(self.blocks) - 1


def _hex_rgb(text):
    """'0xFF017690' / '#017690' -> 0x00RRGGBB, or None if unparseable."""
    t = str(text).strip().lower()
    if t.startswith("0x"):
        t = t[2:]
    elif t.startswith("#"):
        t = t[1:]
    if len(t) == 8:          # AARRGGBB
        t = t[2:]
    if len(t) != 6:
        return None
    try:
        return int(t, 16)
    except ValueError:
        return None


def encode_block(params, tables):
    """A self-describing parameter block: named, typed, variable-length."""
    out = bytearray()
    names = [k for k in params.keys()]
    out += struct.pack("<I", len(names))
    for name in names:
        vals = params[name]
        if not isinstance(vals, list):
            vals = [vals]
        out += struct.pack("<II", tables.string(name), len(vals))
        for v in vals:
            n = _as_number(v)
            if isinstance(v, bool) or n is None and not isinstance(v, str):
                out += struct.pack("<II", BLOCK_BOOL, 1 if v else 0)
            elif isinstance(v, str):
                rgb = _hex_rgb(v)
                if rgb is None:
                    out += struct.pack("<II", BLOCK_INT, raw_int(n or 0))
                else:
                    out += struct.pack("<II", BLOCK_RGB, rgb)
            else:
                out += struct.pack("<II", BLOCK_FLOAT, fixed(float(n)))
    return bytes(out)


def encode_event_v4(name, v, tables):
    """-> (p0, p1, p2, a, b, ref) or None if the event is not representable.

    Events whose parameters are variable-length lists go to the block table
    instead of the fixed record; `ref` then indexes the blocks and
    B_REF_IS_BLOCK is set. Everything else fits the record.
    """
    if name in EVENT_V4_BLOCKED:
        if not isinstance(v, dict):
            return None
        blob = encode_block(v, tables)
        enabled = 1 if _as_number(v.get("enabled", False)) else 0
        return (enabled, 0, 0, 0, B_REF_IS_BLOCK, tables.block(blob))

    spec = EVENT_V4.get(name)
    if spec is None:
        return None

    p = [0, 0, 0]
    a = 0
    b = 0
    ref = 0xFFFFFFFF
    d = v if isinstance(v, dict) else {}
    if not isinstance(v, dict):
        if "a" in spec and spec["a"][0] == "char":
        # FocusCameraSongEvent.handleEvent:
        #   var char = data.getInt('char');
        #   if (char == null) char = cast data.value;   // the bare int IS the char
        #   if (char == null) char = DEFAULT_TARGET;    // 0 = Boyfriend
        # Older charts write {"e":"FocusCamera","v":1} instead of the dict, and
        # 408 of the 3330 events in the 27 charts use that form.
            n = _as_number(v)
            if n is None:
                return None
            a = raw_int(n)
        else:
            # a bare scalar on an event that does not take a char: there is no
            # documented meaning for it, so it is reported rather than guessed
            return None

    for i, (key, kind) in enumerate(spec.get("p", [])):
        if key not in d:
            continue
        raw = d[key]
        if kind == "fixed":
            n = _as_number(raw)
            p[i] = fixed(float(n)) if n is not None else 0
        elif kind == "bool":
            p[i] = as_bool(raw)
        else:
            n = _as_number(raw)
            p[i] = raw_int(n) if n is not None else 0

    if "a" in spec:
        key, kind = spec["a"]
        if key in d:
            raw = d[key]
            if kind == "mask":
                # a boolean list -> bitmask in `a`
                vals = raw if isinstance(raw, list) else [raw]
                mask = 0
                for i, x in enumerate(vals[:32]):
                    if _as_number(x):
                        mask |= 1 << i
                a = mask
            elif kind == "str":
                a = small_id({"direct": 0, "stage": 1} if key == "mode"
                             else STRUMLINE_IDS if key == "strumline"
                             else TARGET_IDS, raw, key)
            else:
                n = _as_number(raw)
                a = raw_int(n) if n is not None else 0

    if spec.get("ease"):
        # `b`'s ease field is an index into THIS FILE's string table, not into
        # a fixed list. A fixed list would make an ease name the port has never
        # seen unrepresentable without rebuilding the port; a string index
        # costs one lookup at load time and never loses a name.
        ease_name = d.get("ease")
        if ease_name is not None:
            b |= (tables.string(ease_name) & B_EASE_MASK) << B_EASE_SHIFT
        else:
            b |= B_EASE_ABSENT
    if spec.get("easeDir"):
        b |= small_id(EASEDIR_IDS, d.get("easeDir"), "easeDir") << B_EASEDIR_SHIFT
    if spec.get("force"):
        b |= B_FORCE if as_bool(d.get("force", False)) else 0
    if spec.get("flipX"):
        b |= B_FLIPX if as_bool(d.get("flipX", False)) else 0
    if spec.get("absolute"):
        b |= B_ABSOLUTE if as_bool(d.get("absolute", False)) else 0
    if spec.get("ref"):
        key = spec["ref"][0]
        if key in d:
            b |= B_REF_IS_STRING
            ref = tables.string(d[key])

    return p[0], p[1], p[2], a, b, ref


MAGIC_V3 = b"FCH3"
VERSION_V3 = 3
MAGIC_V4 = b"FCH4"
VERSION_V4 = 4
# v5 keeps every byte of v4 and gives each note a note-kind string index: the
# `k` field of the chart JSON, which v3 and v4 threw away. 12 B -> 16 B.
MAGIC_V5 = b"FCH5"
VERSION_V5 = 5
NOTE_STRIDE = 12
NOTE_STRIDE_V5 = 16
# 0xFFFF means "no kind": the note is an ordinary note and the port must take
# the ordinary path. Using 0 would alias the first string in the table.
NOTE_KIND_NONE = 0xFFFF
BPM_STRIDE = 16
SECTION_STRIDE = 8
EVENT_STRIDE = 16
EVENT_STRIDE_V4 = 32


def fnv1a(data: bytes) -> int:
    h = 2166136261
    for b in data:
        h ^= b
        h = (h * 16777619) & 0xFFFFFFFF
    return h


# ----------------------------------------------------------------------------
# time model (timestamps in milliseconds, per SongTimeChange.timeStamp)
# ----------------------------------------------------------------------------
class Timeline:
    """ms/step/sample conversion driven by BPM time changes."""

    def __init__(self, time_changes):
        ch = sorted(((float(t), float(b)) for t, b in time_changes), key=lambda c: c[0])
        if not ch or ch[0][0] != 0.0:
            ch.insert(0, (0.0, DEFAULT_BPM))
        self.changes = ch

    @staticmethod
    def _ms_per_beat(bpm):
        return 60000.0 / bpm

    def sample_at_ms(self, ms):
        return int(round(float(ms) * SAMPLE_RATE / 1000.0))

    def step_at_ms(self, ms):
        """Steps elapsed at the given millisecond timestamp."""
        step = 0.0
        for i, (t, bpm) in enumerate(self.changes):
            end_t = self.changes[i + 1][0] if i + 1 < len(self.changes) else None
            if ms <= t:
                break
            seg_end = end_t if (end_t is not None and ms > end_t) else ms
            span = seg_end - t
            if span > 0:
                step += span / self._ms_per_beat(bpm) * STEPS_PER_BEAT
            if end_t is None or ms <= end_t:
                break
        return step

    def ms_at_step(self, step):
        """Millisecond timestamp of the given step position."""
        ms = 0.0
        cur = 0.0
        for i, (t, bpm) in enumerate(self.changes):
            end_t = self.changes[i + 1][0] if i + 1 < len(self.changes) else None
            mpb = self._ms_per_beat(bpm)
            if end_t is None:
                seg = step - cur
                if seg > 0:
                    ms += seg * mpb / STEPS_PER_BEAT
                cur = step
                break
            seg_steps = (end_t - t) / mpb * STEPS_PER_BEAT
            if cur + seg_steps >= step:
                ms += (step - cur) * mpb / STEPS_PER_BEAT
                cur = step
                break
            ms += seg_steps * mpb / STEPS_PER_BEAT
            cur += seg_steps
        return ms

    def samples_per_step(self, bpm):
        return int(round(self._ms_per_beat(bpm) / STEPS_PER_BEAT * SAMPLE_RATE / 1000.0))


# ----------------------------------------------------------------------------
# chart extraction
# ----------------------------------------------------------------------------
def _legacy_notes(song):
    """Yield (time_ms, data, sustain_ms, kind) from a legacy FNF chart.

    Legacy charts have no note kinds at all: `None` is the honest value, and
    v5 will write it as NOTE_KIND_NONE.
    """
    for section in song.get("notes", []):
        must_hit = bool(section.get("mustHitSection", False))
        for entry in section.get("sectionNotes", []):
            if len(entry) < 2:
                continue
            t = float(entry[0])
            data = int(entry[1])
            length = float(entry[2]) if len(entry) > 2 and entry[2] is not None else 0.0
            if data < 0:
                continue  # Psych event note
            if data > STRUMLINE_SIZE * 2:
                data = data % (2 * STRUMLINE_SIZE)
            if not must_hit:  # flip: player notes are 4..7 in non-must-hit sections
                data = data - STRUMLINE_SIZE if data >= STRUMLINE_SIZE else data + STRUMLINE_SIZE
            yield t, data, length, None


def _legacy_time_changes(song):
    """(time_ms, bpm) pairs; changes land on the first note of their section."""
    out = [(0.0, float(song.get("bpm", DEFAULT_BPM)))]
    for section in song.get("notes", []):
        if section.get("changeBPM", False) and section.get("sectionNotes"):
            t = float(section["sectionNotes"][0][0])
            out.append((t, float(section.get("bpm", DEFAULT_BPM))))
    return out


def _legacy_sections(song):
    """(start_step, must_hit, length_steps) straight from the chart sections."""
    return [(float(s.get("sectionNotes", [[0]])[0][0]),
             1 if s.get("mustHitSection", False) else 0,
             int(s.get("lengthInSteps", SECTION_STEPS) or SECTION_STEPS))
            for s in song.get("notes", [])]


def _legacy_events(song):
    """Legacy event rows are [name, strumTime, value1, value2].

    Returns (time_ms, name, v) with `v` kept as the raw positional list, not
    packed. FNF's own legacy importer never emits event rows -- it inserts
    FocusCamera events itself -- so a legacy chart carrying its own rows is
    hand-authored and has no documented parameter order. The v3 encoder packs
    the two values as it always did; the v4 encoder carries them positionally
    and says so, rather than inventing parameter names for them.
    """
    out = []
    for e in song.get("events", []) or []:
        if len(e) < 2:
            continue
        out.append((float(e[1]), str(e[0]), list(e[2:])))
    return out


def _v2_notes(chart, difficulty):
    notes = []
    per_diff = chart.get("notes", {}) or {}
    arr = per_diff.get(difficulty)
    if arr is None:
        if not per_diff:
            return notes
        arr = next(iter(per_diff.values()))  # fall back to first difficulty
    for n in arr:
        t = float(n.get("t", n.get("time", 0.0)))
        data = int(n.get("d", n.get("data", 0)))
        length = float(n.get("l", n.get("length", 0.0)) or 0.0)
        # `k` is the note kind (NoteKindManager). Absent means "no kind", which
        # is a real state and not the same as an empty kind name.
        kind = n.get("k")
        if kind is not None:
            kind = str(kind)
            if not kind:
                kind = None
        notes.append((t, data, length, kind))
    return notes


def _v2_time_changes(metadata):
    changes = []
    for tc in metadata.get("timeChanges", []) or []:
        changes.append((float(tc.get("t", tc.get("time", 0.0))),
                        float(tc.get("bpm", tc.get("value", DEFAULT_BPM)))))
    return changes or [(0.0, DEFAULT_BPM)]


def _v2_events(chart, scroll_speed=1.0):
    """v2 events carry a dict of named parameters, not a positional list:
       {"t":9600, "e":"ZoomCamera", "v":{"duration":4,"ease":"elasticInOut",
        "zoom":1.3}}
    Returns [(time_ms, name, packed_u32)] so build_fch3 stays format-agnostic.

    scroll_speed is the difficulty's chart scrollSpeed, needed because
    ScrollSpeedEvent honours "absolute": when false the target is
    scroll * chart.scrollSpeed, not scroll.
    """
    out = []
    for e in chart.get("events", []) or []:
        t = float(e.get("t", e.get("time", 0.0)))
        name = e.get("e", e.get("event", ""))
        v = e.get("v", e.get("values", {}))
        if v is None:
            v = {}

        if name == "ScrollSpeed" and isinstance(v, dict):
            # ScrollSpeedEvent.hx: absolute false -> scroll * chart.scrollSpeed.
            # Resolved here because it needs the difficulty's scrollSpeed,
            # which only the caller knows. The resolved value is written back
            # into the dict so both encoders see the same number.
            scroll = _as_number(v.get("scroll", 1.0))
            scroll = 1.0 if scroll is None else float(scroll)
            if not v.get("absolute", False):
                scroll = scroll * scroll_speed
            v = dict(v)
            v["scroll"] = scroll
        out.append((t, name, v))
    return out
    return out


def _sections_derived(notes, timeline):
    """Derive 16-step sections for v2 charts (which carry no section list)."""
    if not notes:
        return [(0, 1, SECTION_STEPS)]
    last_sample = max(s for s, _, _, _, _ in notes)
    last_step = int(timeline.step_at_ms(last_sample * 1000.0 / SAMPLE_RATE) // SECTION_STEPS)
    out = []
    for idx in range(last_step + 1):
        lo = timeline.sample_at_ms(timeline.ms_at_step(idx * SECTION_STEPS))
        hi = timeline.sample_at_ms(timeline.ms_at_step((idx + 1) * SECTION_STEPS))
        has_player = any(mp == 1 and lo <= s < hi for _, s, _, mp, _k in notes)
        out.append((idx * SECTION_STEPS, 1 if has_player else 0, SECTION_STEPS))
    return out


# ----------------------------------------------------------------------------
# conversion
# ----------------------------------------------------------------------------
def _build_notes(notes_ms, timeline):
    """[(time_ms, data, sustain_ms, kind)] -> [(sample, sustain, lane, press, kind)].

    `data` is the v2 strumline index: 0..3 the player's, 4..7 the opponent's
    (SongNoteDataRaw: floor(d / strumlineSize) picks the strumline).
    """
    notes = []
    for t, data, length, kind in notes_ms:
        notes.append((timeline.sample_at_ms(t),
                      timeline.sample_at_ms(length) if length > 0 else 0,
                      data % STRUMLINE_SIZE,
                      1 if data < STRUMLINE_SIZE else 0,
                      kind))
    notes.sort(key=lambda n: (n[0], n[2]))
    for s, d, _, _, _ in notes:
        assert 0 <= s <= 0xFFFFFFFF and 0 <= d <= 0xFFFFFFFF
    return notes


def _build_bpms(time_changes, timeline):
    """-> [(start_step, start_sample, samples_per_step, bpm)] with duplicates merged.

    The loader requires strictly increasing start_step, a first change at
    (0, 0), and samples_per_step > 0, so a repeated timestamp keeps the last.
    """
    bpms = []
    for t_ms, bpm in timeline.changes:
        bpms.append((int(round(timeline.step_at_ms(t_ms))),
                     timeline.sample_at_ms(t_ms),
                     timeline.samples_per_step(bpm),
                     int(round(bpm))))
    dedup = []
    for b in bpms:
        if dedup and dedup[-1][0] == b[0]:
            dedup[-1] = b
        else:
            dedup.append(b)
    return dedup


def _build_sections(notes, timeline, sections_ms):
    if sections_ms:
        sections = [(int(round(timeline.step_at_ms(t))), mh, ln)
                    for t, mh, ln in sections_ms]
        merged = []
        for s in sorted(sections):
            if merged and merged[-1][0] == s[0]:
                merged[-1] = s
            else:
                merged.append(s)
        return merged
    return _sections_derived(notes, timeline)


def _positional_v4(v):
    """Encode an event whose parameters are not named (legacy rows).

    Two values in p0/p1, everything else zero. Reported by the caller so the
    loss is visible rather than silent.
    """
    p0 = p1 = 0
    if isinstance(v, (list, tuple)):
        vals = list(v)[:2]
        for i, x in enumerate(vals):
            n = _as_number(x)
            if n is None:
                continue
            if i == 0:
                p0 = fixed(float(n))
            else:
                p1 = fixed(float(n))
    elif isinstance(v, (int, float)) and not isinstance(v, bool):
        p0 = fixed(float(v))
    elif isinstance(v, bool):
        p0 = 1 if v else 0
    return p0, p1, 0, 0, 0, 0xFFFFFFFF


def build_chart(notes_ms, time_changes, events, sections_ms=None, fmt="v4"):
    """Serialise one chart. fmt is "v3" (the proven 16-byte event record),
    "v4" (32-byte record plus string and block tables) or "v5" (v4 plus a
    note-kind string index in every note record).

    `events` is [(time_ms, name, v)] with `v` the raw parameter value: a dict
    for v2 charts, a positional list for legacy ones.
    """
    if fmt not in ("v3", "v4", "v5"):
        raise ValueError("unknown format %r" % fmt)
    v4 = fmt in ("v4", "v5")
    v5 = fmt == "v5"
    timeline = Timeline(time_changes)

    # ---- notes, bpm changes, sections (identical in both formats) --------
    notes = _build_notes(notes_ms, timeline)
    bpms = _build_bpms(time_changes, timeline)
    sections = _build_sections(notes, timeline, sections_ms)

    # ---- events ----------------------------------------------------------
    evs = []
    tables = V4Tables() if v4 else None
    # The note kinds go into the same string table as the event names: both are
    # small, both are per-file, and one table keeps the loader to one reader.
    kind_index = {}
    if v5:
        for _s, _d, _l, _mp, kind in notes:
            if kind is not None and kind not in kind_index:
                kind_index[kind] = tables.string(kind)
    unrepresentable = 0
    for t, name, v in events:
        etype = event_type_id(name)
        if v4:
            enc = encode_event_v4(name, v, tables)
            if enc is None:
                # either a legacy positional row, or a v2 event with no v4
                # mapping. Carry what there is and count it, so a chart that
                # silently loses an event shows up in the conversion report.
                enc = _positional_v4(v)
                unrepresentable += 1
            evs.append((timeline.sample_at_ms(t), etype) + tuple(enc))
        else:
            evs.append((timeline.sample_at_ms(t), etype,
                        int(_event_value(name, v)) & 0xFFFFFFFF))
    for _s, _t, *_rest in evs:
        assert _t != 0, "the loader rejects an event whose type is 0"
    evs.sort(key=lambda e: e[0])

    # ---- song end --------------------------------------------------------
    end_sample = 0
    for s, d, _, _, _ in notes:
        end_sample = max(end_sample, s + d)
    for e in evs:
        end_sample = max(end_sample, e[0])
    if end_sample == 0:
        end_sample = SAMPLE_RATE  # 1 s floor keeps the loader's invariant

    # ---- serialise -------------------------------------------------------
    payload = bytearray()
    ev_index = 0
    # Every record's trailing `order` field must equal its index. The loader
    # enforces this as a corruption check (tutorial_chart.c returns -9 for
    # notes, -12 for sections, -14 for events), so writing 0 here produced
    # files the PS2 rejected. Only the real loader catches this.
    for i, (s, d, lane, mp, kind) in enumerate(notes):
        if v5:
            payload += struct.pack("<IIBBHI", s, d, lane, mp,
                                   NOTE_KIND_NONE if kind is None else kind_index[kind], i)
        else:
            payload += struct.pack("<IIBBH", s, d, lane, mp, i)
    for st, ss, sps, bpm in bpms:
        payload += struct.pack("<IIII", st, ss, sps, bpm)
    for i, (st, mh, ln) in enumerate(sections):
        payload += struct.pack("<IBBH", st, mh, ln, i)
    for e in evs:
        if v4:
            payload += struct.pack("<IIIIIIII", *e)
        else:
            payload += struct.pack("<IIII", e[0], e[1], e[2], ev_index)
            ev_index += 1

    string_blob = b""
    block_blob = b""
    string_count = block_count = 0
    flags = 0
    if v4:
        # string table: u32 offsets then the NUL-terminated strings
        offsets = []
        blob = bytearray()
        for text in tables.strings:
            offsets.append(len(blob))
            blob += text.encode("utf-8") + b"\x00"
        string_blob = b"".join(struct.pack("<I", o) for o in offsets) + bytes(blob)
        string_count = len(tables.strings)
        # Each block is length-prefixed so the reader can walk the table
        # without knowing any block's size in advance.
        block_blob = struct.pack("<I", len(tables.blocks))
        for blob in tables.blocks:
            block_blob += struct.pack("<I", len(blob)) + blob
        block_count = len(tables.blocks)
        if string_count:
            flags |= 1
        if block_count:
            flags |= 2

    # v3's header is 40 B and ends with a reserved u32. v4's is 48 B and spends
    # that reserved word on string_count, so the common part is 4 B shorter.
    if v4:
        header = (MAGIC_V5 if v5 else MAGIC_V4) + struct.pack(
            "<HHIHHHHHHHHII",
            VERSION_V5 if v5 else VERSION_V4,
            NOTE_STRIDE_V5 if v5 else NOTE_STRIDE, len(notes),
            len(bpms), len(sections), len(evs), flags,
            BPM_STRIDE, SECTION_STRIDE, EVENT_STRIDE_V4, 0,
            fnv1a(bytes(payload) + string_blob + block_blob), end_sample)
        header += struct.pack("<III", string_count, len(string_blob), block_count)
        assert len(header) == 48, len(header)
    else:
        header = MAGIC_V3 + struct.pack(
            "<HHIHHHHHHHHIII",
            VERSION_V3, NOTE_STRIDE, len(notes),
            len(bpms), len(sections), len(evs), flags,
            BPM_STRIDE, SECTION_STRIDE, EVENT_STRIDE, 0,
            fnv1a(bytes(payload)), end_sample, 0)
        assert len(header) == 40, len(header)

    return (bytes(header) + bytes(payload) + string_blob + block_blob), {
        "notes": len(notes), "bpms": len(bpms), "sections": len(sections),
        "events": len(evs), "song_end_sample": end_sample,
        "strings": string_count, "blocks": block_count,
        "unrepresentable": unrepresentable,
    }


def build_fch3(*args, **kwargs):
    """v3 output, kept because the port already loads FCH3 on hardware."""
    kwargs["fmt"] = "v3"
    return build_chart(*args, **kwargs)


def convert_legacy(path, difficulty, fmt="v5"):
    with open(path, encoding="utf-8") as f:
        doc = json.load(f)
    song = doc.get("song", doc)
    return build_chart(
        list(_legacy_notes(song)),
        _legacy_time_changes(song),
        _legacy_events(song),
        sections_ms=_legacy_sections(song), fmt=fmt)


def _v2_scroll_speed(chart, difficulty):
    """chart["scrollSpeed"] is a per-difficulty map; 1.0 when absent."""
    ss = chart.get("scrollSpeed") or {}
    if not isinstance(ss, dict):
        return 1.0
    v = ss.get(difficulty)
    if v is None:
        # fall back to the first entry rather than to a guess
        v = next(iter(ss.values()), 1.0)
    try:
        return float(v)
    except (TypeError, ValueError):
        return 1.0


def convert_v2(metadata_path, chart_path, difficulty, fmt="v5"):
    with open(metadata_path, encoding="utf-8") as f:
        meta = json.load(f)
    with open(chart_path, encoding="utf-8") as f:
        chart = json.load(f)
    return build_chart(
        _v2_notes(chart, difficulty),
        _v2_time_changes(meta),
        _v2_events(chart, _v2_scroll_speed(chart, difficulty)),
        fmt=fmt)


# ----------------------------------------------------------------------------
# independent parser (mirrors the port loader's validation invariants)
# ----------------------------------------------------------------------------
def _parse_chart(data):
    """Read either container. Returns (version, notes, bpms, secs, evs, end,
    strings, blocks).

    The first 28 bytes of the header mean the same thing in both versions, so
    the magic picks the version and the event stride follows from it. The
    string and block tables sit after the payload, in that order.
    """
    magic = data[:4]
    if magic == MAGIC_V3:
        ver_expected, ev_stride = VERSION_V3, EVENT_STRIDE
        header_len = 40
    elif magic == MAGIC_V4:
        ver_expected, ev_stride = VERSION_V4, EVENT_STRIDE_V4
        header_len = 48
    elif magic == MAGIC_V5:
        ver_expected, ev_stride = VERSION_V5, EVENT_STRIDE_V4
        header_len = 48
    else:
        raise AssertionError("bad magic %r" % magic)
    # v3: ... | fnv1a | end_sample | rsv          (40 B total)
    # v4: ... | fnv1a | end_sample | str_cnt | str_bytes | blk_cnt  (48 B total)
    if magic in (MAGIC_V4, MAGIC_V5):
        (ver, stride, count, bc, sc, ec, flags,
         bs, ss, es, _r2, checksum, end_sample) = struct.unpack_from(
            "<HHIHHHHHHHHII", data, 4)
    else:
        (ver, stride, count, bc, sc, ec, flags,
         bs, ss, es, _r2, checksum, end_sample, _r3) = struct.unpack_from(
            "<HHIHHHHHHHHIII", data, 4)
    note_stride = NOTE_STRIDE_V5 if magic == MAGIC_V5 else NOTE_STRIDE
    assert ver == ver_expected and stride == note_stride
    assert bs == BPM_STRIDE and ss == SECTION_STRIDE and es == ev_stride
    string_count = block_count = string_bytes = 0
    if magic in (MAGIC_V4, MAGIC_V5):
        string_count, string_bytes, block_count = struct.unpack_from("<III", data, 36)

    payload = data[header_len:]
    assert fnv1a(payload) == checksum, "checksum mismatch"
    if magic == MAGIC_V5:
        notes = [struct.unpack_from("<IIBBHI", payload, i * NOTE_STRIDE_V5)
                 for i in range(count)]
    else:
        notes = [struct.unpack_from("<IIBBH", payload, i * NOTE_STRIDE) for i in range(count)]
    off = count * note_stride
    bpms = [struct.unpack_from("<IIII", payload, off + i * 16) for i in range(bc)]
    off += bc * 16
    secs = [struct.unpack_from("<IBBH", payload, off + i * 8) for i in range(sc)]
    off += sc * 8
    # v3 events are 4 words, v4 events are 8. Both are `ev_stride` bytes.
    ev_words = ev_stride // 4
    evs = [struct.unpack_from("<" + "I" * ev_words, payload,
                              off + i * ev_stride) for i in range(ec)]
    off += ec * ev_stride

    strings, blocks = [], []
    if magic in (MAGIC_V4, MAGIC_V5):
        # `off` is relative to payload; the tables live in the file after it.
        base = header_len + off
        for i in range(string_count):
            (o,) = struct.unpack_from("<I", data, base + i * 4)
            start = base + string_count * 4 + o
            end = data.index(b"\x00", start)
            strings.append(data[start:end].decode("utf-8"))
        # the block table is a u32 count followed by length-prefixed blobs
        (block_total,) = struct.unpack_from("<I", data, base + string_bytes)
        cursor = base + string_bytes + 4
        for _ in range(block_total):
            (blen,) = struct.unpack_from("<I", data, cursor)
            cursor += 4
            blocks.append(data[cursor:cursor + blen])
            cursor += blen
    return ver, notes, bpms, secs, evs, end_sample, strings, blocks


_parse_fch3 = _parse_chart   # old name kept for the call sites below


def selftest():
    # 120 BPM: 1 step = 60000/120/4 = 125 ms = 6000 samples
    legacy = {
        "song": {
            "bpm": 120,
            "speed": 2.0,
            "notes": [
                {"mustHitSection": False, "lengthInSteps": 16,
                 "sectionNotes": [[0, 0, 0], [125, 1, 0], [2000, 4, 500]]},
                {"mustHitSection": True, "lengthInSteps": 16, "changeBPM": True, "bpm": 240,
                 "sectionNotes": [[4000, 0, 0], [4125, 5, 250]]},
            ],
            "events": [["FocusCamera", 0, 0, 1]],
        }
    }
    data, info = build_fch3(
        list(_legacy_notes(legacy["song"])),
        _legacy_time_changes(legacy["song"]),
        _legacy_events(legacy["song"]),
        sections_ms=_legacy_sections(legacy["song"]))
    ver, notes, bpms, secs, evs, end, strings, blocks = _parse_chart(data)
    assert ver == VERSION_V3 and strings == [] and blocks == []

    # container invariants from the port loader
    assert len(notes) == 5, info
    for i, n in enumerate(notes):
        assert n[2] <= 3 and n[3] <= 1
        # The loader rejects the whole file unless each record's order field
        # equals its index (tutorial_chart.c: -9 for notes, -12 sections,
        # -14 events). Writing 0 here is what made the PS2 refuse every
        # converted chart, and only the real loader catches it.
        assert n[4] == i, (i, n)
        if i:
            assert n[0] >= notes[i - 1][0]      # sorted by sample
        assert n[0] <= end and n[0] + n[1] <= end
    for i, sec in enumerate(secs):
        assert sec[3] == i, (i, sec)
    for i, ev in enumerate(evs):
        # v4 drops the order field (it was required to equal the index, so it
        # carried nothing) and grows to 8 words; v3 keeps 4 words with order.
        if ver == VERSION_V3:
            assert ev[3] == i, (i, ev)
        else:
            assert len(ev) == 8, ev
    assert bpms[0][0] == 0 and bpms[0][1] == 0 and bpms[0][3] == 120
    assert bpms[0][2] == 6000                   # samples per step at 120 BPM
    # BPM change at 4000 ms: step 4000/125 = 32, sample 4000*48 = 192000,
    # samples per step at 240 BPM = 60000/240/4 ms = 62.5 ms = 3000
    assert bpms[1][0] == 32 and bpms[1][1] == 192000, bpms[1]
    assert bpms[1][2] == 3000 and bpms[1][3] == 240, bpms[1]
    assert secs[0][0] == 0 and secs[0][1] == 0 and secs[0][2] == 16   # mustHit False
    assert secs[1][0] == 32 and secs[1][1] == 1                        # mustHit True
    assert evs[0][0] == 0 and evs[0][1] == 1      # FocusCamera -> type 1
    assert end >= max(n[0] + n[1] for n in notes)

    # must-hit flip: section 0 is mustHitSection=False so data 0,1 -> opponent
    # (4,5) and data 4 -> player (0); section 1 is mustHitSection=True so data
    # 0 -> player and data 5 -> opponent. Notes are sorted by sample, so the
    # first three come from section 0 and the last two from section 1.
    lanes = [(n[2], n[3]) for n in notes]
    assert lanes == [(0, 0), (1, 0), (0, 1), (0, 1), (1, 0)], lanes

    # time math: note at 125 ms -> 6000 samples; sustain 500 ms -> 24000
    assert notes[1][0] == 6000, notes[1]
    assert notes[2][0] == 2000 * 48 and notes[2][1] == 500 * 48, notes[2]

    # v2 path: sections derived from player-note presence
    v2chart = {"scrollSpeed": {"hard": 2.4}, "events": [], "notes": {"hard": [
        {"t": 0, "d": 0, "l": 0}, {"t": 1000, "d": 4, "l": 0}]}}
    v2meta = {"timeChanges": [{"t": 0, "bpm": 150}]}
    d2, i2 = build_fch3(_v2_notes(v2chart, "hard"), _v2_time_changes(v2meta),
                        _v2_events(v2chart))
    v2, n2, b2, s2, e2, end2, st2, bl2 = _parse_chart(d2)
    assert v2 == VERSION_V3 and st2 == [] and bl2 == []
    assert b2[0][3] == 150 and b2[0][2] == 4800      # 150 BPM -> 4800 samples/step
    assert any(sec[1] == 1 for sec in s2), s2        # section with the player note
    print("selftest OK:", info, "| v2:", i2)
    return 0


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--legacy", help="legacy FNF chart JSON (contains {\"song\": ...})")
    p.add_argument("--metadata", help="v2 SongMetadata JSON")
    p.add_argument("--chart", help="v2 SongChartData JSON")
    p.add_argument("--difficulty", default=DEFAULT_DIFFICULTY)
    p.add_argument("-o", "--output", help="output .chart path")
    p.add_argument("--selftest", action="store_true")
    p.add_argument("--format", choices=("v3", "v4", "v5"), default="v5",
                   help="v3 is the 16-byte event record the PS2 already loads; "
                        "v4 adds named parameters; v5 adds the per-note note "
                        "kind (the chart JSON's `k`) (default)")
    p.add_argument("--v3", action="store_true",
                   help="shorthand for --format v3")
    a = p.parse_args(argv)
    fmt = "v3" if a.v3 else a.format

    if a.selftest:
        return selftest()
    if a.legacy:
        data, info = convert_legacy(a.legacy, a.difficulty, fmt=fmt)
    elif a.metadata and a.chart:
        data, info = convert_v2(a.metadata, a.chart, a.difficulty, fmt=fmt)
    else:
        p.error("provide --legacy, or --metadata with --chart, or --selftest")
    if a.output:
        with open(a.output, "wb") as f:
            f.write(data)
        print("wrote", a.output, len(data), "bytes:", info)
    else:
        sys.stdout.buffer.write(data)
    return 0


if __name__ == "__main__":
    sys.exit(main())
