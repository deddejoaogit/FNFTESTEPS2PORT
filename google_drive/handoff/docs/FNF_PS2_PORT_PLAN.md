# FNF v0.8.6 → PS2 Port — Master Execution Plan

**Project:** native port of Friday Night Funkin' v0.8.6 to PlayStation 2 (SCPH-90006, uLaunchELF/Matrix)
**Baseline:** 0.2.105-aceite-exato (builds clean: 0 errors / 0 warnings, gcc 15.2.0, ELF 5,089,600 B)
**Plan date:** 2026-10-15
**Companion documents:**
- `docs/ps2sdk-compendium/` — complete PS2SDK API reference (all 5,038 pages of ps2dev.github.io/ps2sdk)
- `/home/user/Funkin-0.8.6/` — original FNF v0.8.6 source (fidelity reference, tag `v0.8.6`)

---

## 1. Governing rules (carried over from the project's non-negotiables)

1. Never guess a root cause. Every change is justified by source, log, telemetry, or hardware evidence.
2. Distinguish **Fact** (demonstrated), **Inference** (follows from facts), **Hypothesis** (needs a discriminating test), **Unknown** (insufficient evidence).
3. Every delivered build carries: exact identity, source delta, causal rationale, native compilation result, ELF size/hash, ZIP size/hash, deployment workflow, hardware test matrix.
4. Preserve the proven architecture: 640×360 logical composition centered in 640×448 with 44-pixel bars; menu music continuity through Title/Main/Story; silent black ownership boundary before heavy loads; no invented sounds/positions/compensations.
5. Delete superseded code physically — never behind a flag.

---

## 2. Verified current state (what exists and is hardware-proven)

| Subsystem | State | Evidence |
|---|---|---|
| Boot / init / GS setup | Done | 0.1.x traces, hardware accepted |
| Title screen | Done | hardware-proven |
| Main Menu (exact flicker, bg geometry, atlas 1:1 resample, 5 items + magenta) | Done | 0.2.105 BUILD_INFO C4–C8, accepted |
| Story Menu (9-level carousel, difficulty arrows/tween, display names, GF/BF props) | Done | 0.1.94-m6.8 provenance, M68 ledger |
| Transitions Title↔Menu↔Story (midpoint 15, switch 16, end 30) | Done | RESPOSTAS_IA1 §4 |
| Menu music continuity (audsrv ring geometry) | Done | audsrv facts §4 of knowledge base |
| Framebuffer capture system (VIF1 DIRECT, PATH3 mask, GS FINISH, FIFO drain, BUSDIR reversal) | Done | FIX3–FIX17 |
| VRAM allocator + texture ownership model | Done | `graph_vram_allocate` slots, M68 ledger |
| Telemetry / diagnostics | Done | telemetry_event, console counters |
| **Gameplay (PlayState)** | **Deleted in 0.2.13** | `MENU_ONLY_GAMEPLAY_DELETION_AUDIT_0.2.13.md`: chart/BPM/section/event arrays and parser, note timeline/judgment/sustain, health/death/Game Over, gameplay transport — physically removed. Rebuild from the proven 0.1.99 reference below. |
| Freeplay / CharSelect / Options / Credits | Not started | backlog P2 |
| Results / saves / progression | Not started | backlog P2–P3 |
| Cutscenes / conversations / subtitles | Not started | backlog P3 |
| Temporary Tutorial foundation | **Must be deleted** | backlog P0.1 |

**Known defects carried in the backlog (P0 correctness):** upper-right level name renders white (official: white α 0.7 — verify GS TEX0 TFX/TCC, vertex RGBA, ALPHA equation); score label must become runtime `LEVEL SCORE: …` with saved score, comma preference, `formatMoney`, smoothLerpPrecision 0.307; compiled display-name arrays must become Level→SongRegistry→metadata data flow.

---

## 3. Target specification (from original v0.8.6, `Funkin-0.8.6/`)

### 3.1 State map
```
TitleState → MainMenuState (6 items: storymode, freeplay, merch, upgrade, options, credits)
MainMenuState → StoryMenuState | FreeplayState | OptionsState | CreditsState
StoryMenuState → LoadingState → PlayState (per song) → … → ResultState
PlayState ⇄ PauseSubState / GameOverSubState / Countdown / conversations
```

### 3.2 Gameplay constants (extracted from original)
- **Hit window:** `LEGACY_HIT_WINDOW = 166.67 ms` (10 frames @60fps); judgement thresholds SICK/GOOD/BAD/SHIT are fractions of it (`Scoring.hx`).
- **Health:** `HEALTH_STARTING`/`MAX`/`MIN` in `Constants`; hit `+0.25 × HEALTH_MAX`, miss `−0.25 × HEALTH_MAX`; death at `HEALTH_MIN` (except practice).
- **Notes:** `NOTE_SPACING = STRUMLINE_SIZE + 8`; scroll speed default 1.0; downscroll support; sustain notes with full length; note kinds + splashes.
- **Strumlines:** player + opponent, `KEY_COUNT` directions, background width `KEY_COUNT × NOTE_SPACING + 2×BACKGROUND_PAD`.
- **Countdown:** BEFORE → THREE → TWO → ONE → GO → AFTER, one chart beat per step, 5 ticks, cubeInOut alpha tween (stepped 8 for pixel style), note-style sound at `COUNTDOWN_VOLUME` (PHASE12).
- **Cameras:** 7 logical layers (game, HUD, cutscene, cutouts, subtitles, pause, transition) — on PS2 these are composited deterministically into the single GS draw pass with scissor/z-order.
- **Song start:** instrumental starts only at `startSong()` at `startTimestamp + combinedOffset`; vocals seek by `startTimestamp − instrumentalOffset`; drift correction via exponential lerp ratio `42 × playbackRate`.
- **End-of-song:** Results target GF → opponent → BF; follow rate 0.05; camera offset +20 X / −350 Y; HUD fade 0.6 s; transition fade 0.6 s; `hey`/`cheer` at 0.8 s.

### 3.3 Data model to port (data-driven, not hardcoded)
Song/chart, note style, stage, character, event registry, results/player data — all loaded from generated C arrays produced by PC-side tools (the project already uses this pipeline: `tools/patch_*.py`, generated headers, embedded IRX arrays).

---

## 4. Architecture (PS2SDK, building on the proven base)

### 4.1 Keep unchanged
- Packet-based GS rendering (`packet_t`/`qword_t`, `draw_setup_environment`, `draw_rect_textured`, `draw_texture_transfer`, `dma_channel_send_*`, `draw_wait_finish`).
- `texbuffer_t` + `graph_vram_allocate` slots + `texture_extent_set/get` ownership tracking.
- `sprite_dims` / `sprite_region` / `sprite_region_alpha` with the validated texel-center UV convention (`0.5 … w−0.5`).
- audsrv audio architecture (RPC, ring geometry: 22,050 Hz → 940 B feed / 9,400 B ring; 48,000 Hz → 2,048 B / 20,480 B; lossless aligned retry wrapper from FIX27).
- Pad input via `libpad` with the port's existing edge handling; memory card via `libmc`.
- File I/O via fileio RPC (`host:`/`mass:`), silent-black heavy-load boundary.

### 4.2 New: gameplay core (P0 — this is the foundation of everything else)
| Module | Responsibility | Key PS2SDK APIs |
|---|---|---|
| `conductor.c/h` | Audio-clock time base: songPosition from audsrv playback position; BPM/beat/time conversion; time changes; drift lerp | `audsrv_available/queued`, RPC |
| `input_queue.c/h` | Timestamped pad-edge queue (press/release with frame-exact stamps), PBOT1 boundaries | `padRead`, VSync counter |
| `chart.c/h` | Pooled chart notes/sustains (fixed pools, no malloc per note), sections, scroll speed, note kinds | — |
| `strumline.c/h` | Player/opponent strumlines, note Y from `getNoteY(strumTime, scrollSpeed, downscroll)`, receptor hit testing | draw_* |
| `scoring.c/h` | Judgement windows (SICK/GOOD/BAD/SHIT), score/combo/accuracy, health deltas | — |
| `playstate.c/h` | Lifecycle: create → countdown → startSong → update → pause/death/retry/end → cleanup (PHASE12 contract) | all of the above |
| `hud.c/h` | Health bar, score (VCR OSD Mono), icons, judgement/combo popups, countdown sprites, splashes | draw_* |
| `stage.c/h` | StageData + characters (BF/GF/Dad origin/focus/animation semantics) | draw_* |
| `events.c/h` | Song-event registry, seeking, cancellation, note kinds | — |
| `dataload.c/h` | Generic loaders for song/chart/notestyle/stage/character/event/results data (embedded arrays) | fileio |

Design constraints: fixed-size pools (no per-frame malloc), deterministic update order, everything measurable via existing telemetry, and a **PC-side test harness** for the pure logic (timing, scoring, chart seeking) so the math is verified without hardware.

### 4.3 Rendering approach for gameplay
- Notes/strumlines as textured quads through the existing sprite path; sustains as clipped strips (scissor + repeated quads).
- 7 camera layers → deterministic single-pass compositing with explicit z-order and scissor rectangles (PS2 has no camera stack; the port's existing approach of computing screen coordinates directly is preserved).
- Downscroll = sign flip in the Y computation only.

### 4.3 Proven reference implementation (recovered from v0.1.99-fix27 `main.c` — the rebuild baseline)

The deleted gameplay code is the project's own hardware-tested implementation. The rebuild starts from these exact, proven definitions (not guesses):

**Chart container `FCH3` (binary, little-endian, FNV-1a validated):**
```
header 40 B: "FCH3" | u16 version=3 | u16 note_stride=12 | u32 note_count
           | u16 bpm_count | u16 section_count | u16 event_count | u16 rsv
           | u16 bpm_stride=16 | u16 section_stride=8 | u16 event_stride=16 | u16 rsv
           | u32 fnv1a_checksum(payload) | u32 song_end_sample | rsv to 40
note   12 B: u32 sample | u32 duration | u8 lane(0..3) | u8 must_press | u16 order
bpm    16 B: u32 start_step | u32 start_sample | u32 samples_per_step | u32 bpm
section 8 B: u32 start_step | u8 must_hit | u8 length_steps | u16 order
event  16 B: u32 sample | u32 type | u32 value | u32 order
```
Validation invariants proven in the loader: lanes ≤3, must_press ≤1, order == index, notes sorted by sample, durations within song end, BPM/section steps strictly increasing from 0, checksum match.

**Judging (sample domain, 48 kHz):** windows SICK ≤2160 (45 ms), GOOD ≤4320 (90 ms), BAD ≤6480 (135 ms), MISS >7200 (150 ms); ghost taps penalized. Points 350/200/100; accuracy weights 10000/7500/5000. Health clamp [0, 20000]: hit +400/+200/0, miss −1000, ghost −1000; sustain tick +50 per 6000-sample tick (125 ms) with +50 score; death at 0.

**Rendering:** spawn ahead 96,000 samples (2 s), despawn 12,000 (250 ms); `note_y_q8(delta) = 59·256 + delta·48·scroll_milli/(125·1000)` (Q8.8 fixed point); event easing: linear, quadInOut (`(x·x·(196608−2x))>>32`), quadOut (`(t·t)>>16`).

**Audio:** producer/consumer ring (256 KB ring, 32 KB reads, semaphore-guarded thread at priority 40) with switch/reset/loop and underrun/error counters; audsrv at 48 kHz → 2,048 B feed / 20,480 B internal ring; FIX27 lossless aligned retry on partial submission. Menu music continuity and the black/silent heavy-load boundary are preserved unchanged.

**Character animation metadata:** `bf_anim_t {name[17], start, count, loop, flags, origin_x, origin_y}` + `bf_meta_t {width, height, anim_count, frame_count, fps, anim[8]}` — loaded per character (BF/GF/death), reusable for StageData characters.

### 4.4 Data pipeline (PC-side, no PS2 dependency)
Extend the existing `tools/` Python pipeline: FNF v0.8.6 song/chart JSON (SongData v2 + legacy sections) → `FCH3`; Sparrow/XML atlases + frame trims → the port's animation metadata + RGBA/LZ4 frames; StageData/CharacterData/NoteStyle/Event/Results registries → embedded C arrays (`*_generated.h`) consumed by the generic loaders.

---

## 5. Implementation phases (work packages)

### Phase P0 — foundation and deletion gate (do first)
1. **Delete the temporary Tutorial foundation** (note-test/stage/HUD paths) physically; acceptance: release binary/strings/asset scan finds none of the superseded implementation.
2. **Gameplay core modules** (§4.2) with PC-side unit tests for pure logic.
3. **Generic data-driven loaders**; convert chart/song/stage/character/notestyle/event/results data to embedded arrays via PC-side Python tools (extend the existing `tools/` pipeline).
4. **Preserve** menu ring continuity and the black/silent heavy-load ownership boundary (do not regress 0.1.81 traces).
5. Fix the three carried P0 correctness defects (level-name alpha, runtime score label, SongRegistry data flow).

### Phase P1 — gameplay (the heart of the port)
1. PlayState lifecycle per PHASE12: constructor contract, creation order, countdown state machine, cutscene gate, song start & A/V sync, pause, death/Game Over, retry, end-song, Results transition, cleanup.
2. Input: exact timestamps, PBOT1 boundaries, held/simultaneous handling.
3. Rendering: strumlines, notes, sustains (clipped strips), note kinds, splashes, covers.
4. HUD: health bar, score text (VCR OSD Mono, exact metrics), icons with bops, judgement/combo popups, countdown.
5. Stage + characters: StageData, exact mainStage, BF/GF/Dad origin/focus/animation semantics.
6. Events: song-event registry, seeking, cancellation, note kinds, script dispatch.
7. Pause and Game Over replacing temporary overlays.
8. Results + save/progression routes.

### Phase P2 — front-end completion
Story typography/layout + animated monochrome props; Freeplay (PHASE3–4); Character Select (PHASE6); Options/Preferences/Controls/Offset/Save (PHASE8–9); Credits (PHASE7); Merch/Upgrade policy resolution; official Loading + sticker transitions; pinned Flixel transition dependency.

### Phase P3 — remaining official surfaces
Character Unlock chain; conversation/speaker/dialogue-box registries + typing flow; video playback or approved platform fallback per cutscene; SRT subtitles; in-engine critical error screen; boot/init + attract closure.

### Phase P4 — release hardening
Asset manifest (original dims, displayed bounds, format, alpha/filtering, VRAM residency); texture optimization to exact displayed dimensions only; removal of every satisfied diagnostic; per-matrix hardware tests on SCPH-90006; clean one-ZIP PowerShell deployment + build identity.

---

## 6. Technical risks and open questions

| Risk | Impact | Mitigation |
|---|---|---|
| Current source (`main.c` ~155 KB, v0.2.105) not yet in my workspace | Cannot integrate/compile | **Needed from user** (see §7) |
| No PS2 toolchain in this sandbox yet | No local compile validation | Build ps2toolchain-ee/iop from GitHub (sources are on github.com/ps2dev) — started; user's PC remains the reference build |
| Assets (the repo carries no assets; Drive rar was truncated) | Gameplay needs charts/textures | Re-harvest from original via PC-side pipeline; Drive has measurement CSVs and reference PNGs |
| Video cutscenes | PS2 homebrew has no practical decoder | Approved platform fallback (still-image + audio) unless a decoder is proven |
| 32 MB RAM / 4 MB VRAM | Chart pools, stage textures | Fixed pools, streaming per-screen, texture optimization only to displayed size (P4) |
| Level-name alpha defect | Visible fidelity bug | Verify GS TEX0 TFX/TCC + vertex RGBA + ALPHA equation against `libgs` (compendium part04) — evidence first, no guessing |

---

## 7. Immediate next steps

1. **Get the current source into the workspace** (blocking for integration/compilation): the user attaches `src/` (or the whole project folder) to the chat, or pushes it to this repo. Everything in §4.2–5 is then implemented against the real tree and compile-validated.
2. **Toolchain:** finish the ps2toolchain build in the sandbox for local compile checks (reference build stays the user's PC).
3. **Start P0** in this order: delete Tutorial paths → `conductor` + `input_queue` (with PC-side tests) → `chart` pools → `strumline`/`scoring` → data loaders → first playable song on hardware via the user's build.
4. Every step lands as: identity, delta, rationale, compile result, test matrix row — per §1.

---

## 8. Definition of done

The port is complete when every state in §3.1 runs on SCPH-90006 with source-verified fidelity (all visible outputs have full value provenance, all branches have a test or an unreachable proof), the temporary implementation is physically gone, the asset manifest and texture residency rules hold, and the one-ZIP deployment reproduces bit-comparable functional builds.
