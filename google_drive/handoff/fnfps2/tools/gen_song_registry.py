#!/usr/bin/env python3
"""Generate src/engine/song_registry.c from the game's own JSON.

The song/level data used to be typed into song_registry.c by hand, and two
entries were wrong (the Tutorial's opponent, week4's order) because they were
guessed from reading funkin/InitState.hx instead of from the data. This script
removes that class of mistake: the C file is now a rendering of the JSON, so
re-running it after dropping a new <id>-metadata.json in assets-data/songs/ is
all it takes to add a song.

Sources, all under assets-data/:
  levels/<id>.json            -> week order, display name, title asset,
                                 background colour, capsule
  songs/<id>-metadata.json    -> playData.characters, playData.stage,
                                 playData.noteStyle, playData.difficulties,
                                 playData.songVariations, timeChanges[0].bpm

Usage:  python3 tools/gen_song_registry.py [--check]

--check regenerates into a string and exits 1 if it differs from the file on
disk, so the test suite can prove the committed C matches the committed JSON.
"""

import argparse
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.dirname(HERE)
DATA = os.path.join(PROJECT, "assets-data")
OUT = os.path.join(PROJECT, "src", "engine", "song_registry_data.inc")

# funkin.data.story.level.LevelRegistry.listBaseGameEntryIds(). This order is
# the story-menu order and the play order; the JSON only says which songs are
# in a week, never in which order the weeks appear.
LEVEL_ORDER = [
    "tutorial", "week1", "week2", "week3", "week4", "week5",
    "week6", "week7", "weekend1", "sserafim",
]

DIFFS = ["easy", "normal", "hard"]
DIFF_FLAG = {"easy": "SR_DIFF_EASY", "normal": "SR_DIFF_NORMAL", "hard": "SR_DIFF_HARD"}
VAR_FLAG = {"erect": "SR_VAR_ERECT", "pico": "SR_VAR_PICO", "bf": "SR_VAR_BF"}
NOTE_STYLE = {"funkin": "SR_NOTE_FUNKIN", "pixel": "SR_NOTE_PIXEL"}

# Everything the original puts in playData.characters that we do not model yet.
# Kept explicit so the generator notices instead of silently dropping it.
# altInstrumentals: some songs swap the Inst for a variant (bopeebo has
# "erect"); the port loads the default only, so it is recorded, not emitted.
KNOWN_CHAR_KEYS = {
    "player", "girlfriend", "opponent", "opponentVocals", "playerVocals",
    "altInstrumentals",
}


def die(msg):
    sys.stderr.write("gen_song_registry: %s\n" % msg)
    raise SystemExit(1)


def load_levels():
    levels = []
    for lid in LEVEL_ORDER:
        path = os.path.join(DATA, "levels", lid + ".json")
        if not os.path.exists(path):
            die("missing level file %s" % path)
        d = json.load(open(path))
        levels.append({
            "id": lid,
            "name": d.get("name", ""),
            "title_asset": d.get("titleAsset", ""),
            "background": d.get("background", "#000000"),
            "capsule": (d.get("capsule") or {}).get("name"),
            "songs": d.get("songs", []),
        })
    return levels


def load_song(sid):
    path = os.path.join(DATA, "songs", sid + "-metadata.json")
    if not os.path.exists(path):
        die("missing song metadata %s" % path)
    d = json.load(open(path))
    play = d.get("playData") or {}
    chars = play.get("characters") or {}

    unknown = set(chars) - KNOWN_CHAR_KEYS
    if unknown:
        die("%s: unmapped character keys %s" % (sid, sorted(unknown)))

    style = play.get("noteStyle", "funkin")
    if style not in NOTE_STYLE:
        die("%s: unknown noteStyle %r" % (sid, style))

    diffs = play.get("difficulties") or ["easy", "normal", "hard"]
    for x in diffs:
        if x not in DIFF_FLAG:
            die("%s: unknown difficulty %r" % (sid, x))

    variations = play.get("songVariations") or []
    for x in variations:
        if x not in VAR_FLAG:
            die("%s: unknown variation %r" % (sid, x))

    bpm = (d.get("timeChanges") or [{}])[0].get("bpm")
    if not isinstance(bpm, (int, float)) or bpm <= 0:
        die("%s: no usable bpm in timeChanges" % sid)

    return {
        "id": sid,
        "name": d.get("songName", sid),
        "artist": d.get("artist", ""),
        "charter": d.get("charter", ""),
        "player": chars.get("player"),
        # girlfriend is absent on purpose for the Tutorial.
        "girlfriend": chars.get("girlfriend"),
        "opponent": chars.get("opponent"),
        "stage": play.get("stage"),
        "note_style": style,
        "difficulties": diffs,
        "variations": variations,
        "bpm": float(bpm),
    }


def cstr(s):
    if s is None:
        return "NULL"
    if s == "":
        return '""'
    return '"%s"' % s.replace("\\", "\\\\").replace('"', '\\"')


def mask(flags, table):
    return " | ".join(table[x] for x in flags) if flags else "0"


def hexcolor(s):
    s = s.lstrip("#")
    if len(s) != 6:
        die("bad background colour %r" % s)
    return "0x%s" % s.upper()


def fmt_bpm(bpm):
    # 111.5 must not become 111.49999.
    return ("%.4f" % bpm).rstrip("0").rstrip(".") or "0"


def generate():
    levels = load_levels()
    songs = {}
    for lv in levels:
        for sid in lv["songs"]:
            if sid not in songs:
                songs[sid] = load_song(sid)

    L = []
    w = L.append
    w("/* GENERATED by tools/gen_song_registry.py -- do not edit by hand.")
    w(" * Included by song_registry.c, which owns the logic and the checks.")
    w(" *")
    w(" * Every value comes from the game's own JSON under assets-data/:")
    w(" *   levels/<id>.json          -> weeks, order, names, backgrounds, capsules")
    w(" *   songs/<id>-metadata.json  -> characters, stage, note style, bpm,")
    w(" *                               difficulties, variations")
    w(" *")
    w(" * Two entries used to be guessed wrong -- the Tutorial's opponent (it is")
    w(" * the girlfriend) and week4's order (satin-panties, high, milf). Both")
    w(" * mistakes came from inferring from funkin/InitState.hx instead of")
    w(" * reading the shipped JSON. Now the C is a rendering of the JSON, so it")
    w(" * cannot drift. Re-run the generator after adding a song.")
    w(" */")
    w("")
    w("#include <string.h>")
    w("")

    # ---- songs, grouped by level so the file reads like the story menu ----
    emitted = set()
    for lv in levels:
        w("/* ---- %s: %s ---- */" % (lv["id"], lv["name"]))
        for sid in lv["songs"]:
            s = songs[sid]
            if sid in emitted:
                continue
            emitted.add(sid)
            note = []
            note.append("stage %s" % s["stage"])
            if s["note_style"] == "pixel":
                note.append("pixel notes")
            if s["variations"]:
                note.append("variations: %s" % ", ".join(s["variations"]))
            if s["girlfriend"] is None:
                note.append("no girlfriend slot")
            w("/* %s (%s) -- %s. */" % (s["name"], sid, "; ".join(note)))
            w("static const sr_song_t s_%s = {" % sid.replace("-", "_"))
            w("    %s, %s, %s, %s," % (cstr(sid), cstr(s["name"]), cstr(s["artist"]), cstr(s["charter"])))
            w("    %s, %s, %s," % (cstr(s["player"]), cstr(s["girlfriend"]), cstr(s["opponent"])))
            w("    %s, %s, %s," % (cstr(s["stage"]), NOTE_STYLE[s["note_style"]], mask(s["difficulties"], DIFF_FLAG)))
            w("    %s, %s" % (mask(s["variations"], VAR_FLAG), fmt_bpm(s["bpm"])))
            w("};")
            w("")
    missing = set(songs) - emitted
    if missing:
        die("songs not placed in any level: %s" % sorted(missing))

    # ---- levels ----
    for lv in levels:
        w("static const sr_song_t *const lv_%s_songs[] = {" % lv["id"].replace("-", "_"))
        for sid in lv["songs"]:
            w("    &s_%s," % sid.replace("-", "_"))
        w("};")
        w("")

    for lv in levels:
        w("static const sr_level_t lv_%s = {" % lv["id"].replace("-", "_"))
        w("    %s, %s, %s, %s," % (cstr(lv["id"]), cstr(lv["name"]),
                                   cstr(lv["title_asset"]), hexcolor(lv["background"])))
        w("    %d, %s," % (1 if lv["capsule"] else 0, cstr(lv["capsule"])))
        w("    lv_%s_songs, %d" % (lv["id"].replace("-", "_"), len(lv["songs"])))
        w("};")
        w("")

    # An array of pointers, which is what sr_levels() hands out: a week is
    # then a stable address the caller can hold onto while iterating.
    w("static const sr_level_t *const g_levels[] = {")
    for lv in levels:
        w("    &lv_%s," % lv["id"].replace("-", "_"))
    w("};")
    w("")
    return "\n".join(L) + "\n"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true",
                    help="fail if the generated file differs from the one on disk")
    args = ap.parse_args()

    text = generate()
    if args.check:
        try:
            current = open(OUT).read()
        except OSError:
            die("%s is missing; run without --check" % OUT)
        if current != text:
            die("src/engine/song_registry.c is stale -- run "
                "python3 tools/gen_song_registry.py")
        print("song_registry_data.inc matches assets-data/")
        return

    with open(OUT, "w") as f:
        f.write(text)
    print("wrote %s (%d bytes)" % (OUT, len(text)))


if __name__ == "__main__":
    main()
