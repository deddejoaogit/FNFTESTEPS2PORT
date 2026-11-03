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

## `assar_cutscene_sserafim.py` — a cutscene de 30 s, assada do FlxAnimate

Lê os `Animation.json` + spritesheets do `sserafim` no clone de arte e escreve,
em `build/packs/stages/sserafim/cut/`:
`main`, `gf`, `bf` (`.quads` de geometria assada + `.cfz4` de textura, 24 fps) e
as duas imagens de UM quadro do `endStuff` (`end1`, `end2`).

```sh
python3 tools/assar_cutscene_sserafim.py --assets <clone funkin.assets> \
        --saida build/packs/stages/sserafim/cut
```

**`--largura-max` (padrão 512)** é a escala das texturas da cena. Medido nesta
árvore, com a arte real, no `manifest.json` que o assador escreve:

| largura | `main` | `getup gf` | `getup bf` | VRAM da cena |
|---|---|---|---|---|
| **512 (padrão, o que está no pendrive)** | 512×249 | 512×304 | 512×202 | **773 120 B** |
| 448 | 448×218 | 448×266 | 448×176 | **591 360 B** |

Rodando o jogo no harness com as texturas de 512, a cena inteira cabe no pool do
palco e o `pool_min` (menor folga em toda a música, medido pelo próprio
alocador) fecha em **33 792 B livres**, com o palco, as três dançarinas e o piso
já carregados. É pouco: se o console acusar `cut_sem_vram` ou troca de prop
durante a cena, `--largura-max 448` economiza 181 760 B e reduz 12,5 % da
imagem. O tamanho por quadro (`pack.frame_bytes`) é conferido contra a textura
antes de desenhar: se a geometria e a textura discordarem, o log diz
`cut_tamanho` e o sprite não entra.

A posição de cada sprite na tela (`bounds`/`offset` do FlxAnimate) foi o defeito
mais caro deste port: o `origin` do sprite é `bounds.x`/`bounds.y` (o recorte do
quadro na spritesheet), não a âncora do quadro. Usar o `origin` jogava os três
sprites fora da tela. O teste `test/test_ss_cutscene.c` cobra a matriz de um quad
e os 6 marcos da linha do tempo.

## `build_pendrive.sh` — o cartão do console, medido antes de copiar

Monta a pasta do pendrive (o layout que `src/main.c` abre em `mass:/FNFPS2/`).
Antes de copiar **qualquer** arquivo ele mede os sete grupos: 16 assets de
runtime, 46 `.chp4`, 21 palcos (157 props), os 8 arquivos da cutscene do
`sserafim`, os 81 charts, os 27 stems, o áudio das telas (31 PCMs) e o ELF. Se
faltar algo, imprime o que falta com o comando que resolve cada item e não copia
nada (uma pasta pela metade não chega a existir).

```sh
sh tools/build_pendrive.sh                       # monta em build/pendrive
ASSETS=<pasta> ELF=<elf> STAGES_SRC=... \
    CHARS_SRC=... CHARTS_SRC=... STEMS_SRC=... GO_SRC=... \
    sh tools/build_pendrive.sh build/pendrive     # tudo por variável (testes)
```

Dois defeitos que só apareciam aqui — e que a suíte agora cobra:

* o `grep` da lista de props pegava o **exemplo** `"stages/<stage>/<name>.cfz4"`
  do comentário do `stage_dados.h` e abortava a cópia dos 21 palcos **sempre**
  (correção: `grep -v '<'`; 158 strings → 157 props);
* o `end1.cfz4`/`end2.cfz4` do `endStuff` não eram copiados (só `main`/`gf`/`bf`),
  e no console a sequência do fim rodaria sem imagem (`cut_fim_falta`).

O teste monta o pendrive duas vezes — com tudo e **sem o `end2.cfz4`** — e cobra
o resultado e a recusa (`cutscene do sserafim: 7 de 8`, nada copiado).

## `maratona_harness.py` — cada música ATÉ O FIM

O sweep dá 1 200 quadros por música; o fim nunca era exercitado. Este roda cada
música até o `song_end_sample` do chart (mais a cutscene e a sequência do fim,
quando é o `sserafim`) e cobra o **Resultado** (`state,9`), o `play_result`, a
auditoria da faixa visível e a ausência de `palco_script_retry`,
`char_sem_vram`, `palco_sem_vram` e `cut_fim_*`.

```
python3 tools/maratona_harness.py            # 27 músicas, normal  (~40 s)
python3 tools/maratona_harness.py --todas    # 81 rodadas          (~120 s)
python3 tools/maratona_harness.py --musicas monster,guns
python3 tools/maratona_harness.py --bin build/fnf_harness_asan   # com ASan+UBSan
```

Medido: `--todas` → 81/81 até o Resultado, 0 retângulo / 0 linha fora.
A passada de 27 roda dentro de `test/run_tests.sh`, que também roda as duas
músicas mais longas com o binário de sanitizadores (`--bin`; o timeout do
subprocesso sobe de 300 s para 900 s, porque o ASan é ~3× mais lento).

O orçamento de quadros é **o do áudio**, não o do chart:
`max(song_end_sample, duração do stems.pcm)`. O `stems` do Tutorial tem 67,2 s
para um chart de 60,0 s, e é o fim do arquivo que termina a música — com o
orçamento do chart a rodada acabava 41 quadros antes do `state,9` e reprovava
uma música que estava certa (medido: `tutorial/1` passou a 4 633 quadros).
