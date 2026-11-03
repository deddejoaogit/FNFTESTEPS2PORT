# FNF v0.8.6 → PS2 port — data pipeline tools

PC-side tooling for the port (plan section 4.4). Everything here runs on the
PC (Python 3, stdlib only) and feeds the PS2 build; nothing here runs on console.

## `fnf_to_fch3.py` — chart converter

Converts the original game's song/chart JSON into the port's proven binary
chart container **FCH3** (recovered from the v0.1.99-fix27 implementation),
so the PS2 loader can keep its exact format and validation invariants
(magic/version/strides, FNV-1a checksum, sorted notes, order fields, bounds).

```bash
# legacy chart (single JSON with {"song": {...}})
python3 fnf_to_fch3.py --legacy tutorial-hard.json --difficulty hard -o tutorial-hard.chart

# v2 chart pair (SongMetadata + SongChartData)
python3 fnf_to_fch3.py --metadata metadata.json --chart hard.json -o hard.chart

# self test: container invariants, time math, must-hit flip, v2 derivation
python3 fnf_to_fch3.py --selftest
```

### Format mapping (original → FCH3)

| Original (FNF v0.8.6) | FCH3 | Notes |
|---|---|---|
| note `t` (ms) | `sample` | `ms × 48` at 48 kHz |
| note `l` (sustain, ms) | `duration` | samples |
| note `d` (0-3 player, 4-7 opponent) | `lane` + `must_press` | legacy charts flip 0-3 ↔ 4-7 when `mustHitSection` is false; `data < 0` skipped |
| `SongTimeChange{t,bpm}` (timestamp in ms) | `bpm` records | `start_step`/`start_sample`/`samples_per_step` derived; legacy `changeBPM` lands on the section's first note timestamp |
| legacy sections | `section` records | `mustHitSection` and `lengthInSteps` preserved (v2 charts derive 16-step sections) |
| `events` | `event` records | name → type id (FocusCamera=1, ChangeCharacter=2, PlayAnimation=3, AltIdleAnimation=4); `value` = value1 \| value2<<16 |
| song length | `song_end_sample` | max(note end, event, override) |

### Status
- Self test passing (invariants, time math, must-hit flip, v2 derivation).
- Needs one integration decision before first hardware use: the port's event
  type ids above are this tool's proposal — confirm against the rebuilt event
  registry (plan P1.6) and adjust `EVENT_TYPES` if the port assigns other ids.

## Planned (next in pipeline)
- `fnf_assets.py` — Sparrow/XML atlases + frame trims → the port's animation
  metadata (`bf_anim_t`/`bf_meta_t`) + RGBA/LZ4 frames.
- `fnf_registry.py` — SongRegistry / StageData / CharacterData / NoteStyle /
  Results data → embedded `*_generated.h` C arrays.

## `empacotar_stems.py` — the audio of the 27 songs (the pendrive had only one)

The game opens `assets/songs/<id>/stems.pcm` per song and falls back to the flat
`assets/audio_stems.pcm` (the Tutorial's) when the per-song file is missing —
so the console played music for exactly 1 of 27 songs. Nothing generated those
files; this tool does.

The format is not a choice, it is what the mixer reads: **8 bytes per sample**,
`[InstL, InstR, VoxL, VoxR]` as int16 little-endian at 48 kHz (the ring gives
the instrumental and the voices interleaved and `amix_somar` sums them into the
4-byte stereo frame the audsrv stream wants). The 27 songs fit the budget the
plan has been quoting: 1 078 655 000 B ≈ 1,08 GB.

```bash
# prove the interleaving/channel-order math without the 2.5 GB art clone
python3 empacotar_stems.py --autoteste

# real run: songs/<id>/Inst.ogg + Voices.ogg from the art clone
python3 empacotar_stems.py --assets /path/to/funkin.assets

# measure what already exists (no clone needed) and compare with each chart's
# `song_end_sample`, which is the number the game itself uses for the end
python3 empacotar_stems.py --conferir
```

A song without `Voices.ogg` gets silent voices **and says so** in the manifest;
an audio shorter than the chart is reported as such instead of being padded to
hide it. `tools/build_pendrive.sh` refuses to build a pendrive without the 27
stems (and copies `STEMS.txt` next to them).
