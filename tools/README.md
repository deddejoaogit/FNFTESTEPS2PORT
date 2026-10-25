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
