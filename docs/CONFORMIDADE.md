# Conformidade com o original — o que está provado, o que foi só lido, o que falta

O objetivo declarado desta rodada: **certeza máxima de que cada linha do port
corresponde ao código do jogo original, e de que o código do port está estável e
funcional**. Este documento é a prestação de contas disso, por subsistema.

A fonte é sempre `FunkinCrew/Funkin`, tag `v0.8.6` (arquivo e linha citados).
"Citação" aqui quer dizer que o número do port foi lido no Haxe e conferido
contra a linha — não que alguém achou parecido.

Três níveis, e a diferença entre eles importa:

| Nível | Significa |
|---|---|
| **A — provado por teste** | uma suíte executa o código do port e compara com o número do original; se alguém mudar, a build falha |
| **B — lido e conferido** | o valor foi conferido contra a linha do Haxe, mas nenhum teste trava isso |
| **C — não auditado** | vale o que está escrito; pode ter divergência não descoberta |

## A — provado por teste (`test/test_gameplay.c`, `tools/run_gameplay_harness.sh`)

| Item do original | Valor na fonte | Onde no port | Como é provado |
|---|---|---|---|
| `Constants.HIT_WINDOW_MS` | 160,0 (`Constants.hx:361`) | `FNF_HIT_WINDOW_MS` | teste: 160,0 → 7.680 amostras a 48 kHz |
| janela simétrica | `getHitWindow()` start −160 / end +160 (`GRhythmUtil.hx:54-61`) | `fnf_play_is_pressable` | teste nos dois sinais e 1 amostra fora |
| nota perdida no fim da janela | `processWindow()`: `songPosition > end → hasMissed` (`GRhythmUtil.hx:93-101`) | `TUTORIAL_NOTE_LATE` | teste: 159 ms acerta, 161 ms é MISS |
| `PBOT1_MAX_SCORE/OFFSET/SLOPE/MIN` | 500 / 54,99 / 0,080 / 9,0 (`Scoring.hx:86-104`) | `gameplay.c` | tabela medida: 0→500, 10→495, 20→480, 30→449, 45→353, 60→209, 90→37, 120→11, 160→9 |
| `PBOT1_PERFECT_THRESHOLD` | 5,0 ms (`Scoring.hx:106`) | idem | teste: 4,9→500, e 0→500 |
| `PBOT1_MISS_SCORE` | −100 (`Scoring.hx:100`) | idem | teste: 161 ms → −100 |
| `PBOT1_*_THRESHOLD` | 45 / 90 / 135 / 160 (`Scoring.hx:115-128`) | `fnf_judge_ms` | teste em cada limite, nos dois lados |
| `LEGACY_*` (janela, 20/75/90/100 %, pontos) | `Scoring.hx:207-256` | idem | testes de 10 a 200 ms |
| `WEEK7_*` (20/55/80 %) | `Scoring.hx:262-267` | idem | testes de 30 a 170 ms |
| `HEALTH_MAX/STARTING/MIN` | 2,0 / 1,0 / 0,0 (`Constants.hx:455-466`) | `gameplay.h` | teste |
| `HEALTH_SICK/GOOD/BAD/SHIT_BONUS` | +0,03 / +0,015 / 0 / −0,02 (`Constants.hx:471-492`) | idem | teste, valor exato |
| `HEALTH_MISS_PENALTY`, `GHOST` | −0,08 (`Constants.hx:502-507`) | idem | teste |
| `HEALTH_HOLD_BONUS_PER_SECOND` | +0,12/s (`Constants.hx:497`) | `fnf_play_hold` | teste: 0,5 s → +0,06 e +125 de placar |
| `SCORE_GHOST_MISS_PENALTY` | −10 (`Constants.hx:532`) | `fnf_play_ghost` | teste |
| `SCORE_HOLD_BONUS_PER_SECOND` | +250/s (`Constants.hx:538`) | `fnf_play_hold` | teste |
| `JUDGEMENT_*_COMBO_BREAK` | BAD e SHIT quebram (`Constants.hx:553-573`) | `fnf_judge_breaks_combo` | teste nos cinco julgamentos |
| `applyScore()` — ordem | tally → vida → combo → placar (`PlayState.hx:3330-3360`) | `play_apply` | teste: BAD mantém a vida e zera o combo; SICK soma e incrementa |
| vida limitada a [MIN,MAX] | `PlayState.hx:1214-1215` | `play_apply` | teste: 100 SICK saturam em 2,0; 13 MISS matam |
| `songScore` **sem** limite em zero | `PlayState.hx:3359` | `play_apply` | teste: 5 misses = −500, +3 ghosts = −530 |
| `tallyCompletion()` e ranks | `Scoring.hx:430-435`, `:580-595` | `fnf_play_completion`, `fnf_play_rank` | teste: 9 SICK + 1 MISS em 10 = 0,80 = GREAT; clamp em 1,0; PERFECT GOLD só com 100 % SICK |
| `ResultState` clear percent | `floor(tallyCompletion*100)` (`ResultState.hx:578-583`) | `fnf_play_clear_percent` | teste |
| `PIXELS_PER_MS` + nota→y | 0,45 (`Constants.hx:356`), `getNoteY()` (`GRhythmUtil.hx:141-145`) | `tutorial_note_y` | conferido por cálculo: 0,45/2 (tela 640×360 é metade de 1280×720) = delta_amostras·0,45/96 |

E no jogo real (não no módulo isolado), `tools/run_gameplay_harness.sh` exige,
a cada execução: relógio da música avançando, ≥2 acertos julgados, ≥2 saltos
positivos de placar, nenhum estouro de pacote, nenhuma escrita fora do limite.
Última medição: **9 acertos, 5 saltos de placar, 1.939.200 amostras de relógio**.

## Divergências encontradas nesta auditoria (e corrigidas)

1. **Janela de acerto 150 ms e janela de perda 250 ms.** O original usa **160 ms
   para as duas** (`Constants.HIT_WINDOW_MS`, aplicado simetricamente). O port
   tinha `TUTORIAL_NOTE_JUDGE_WINDOW 7200` (150 ms) e `TUTORIAL_NOTE_LATE 12000`
   (250 ms): dois números que não existem na fonte. Agora as duas derivam de
   `FNF_HIT_WINDOW_MS` por macro, e a suíte falha se voltarem a ser digitadas.
2. **Placar limitado em zero.** `applyScore()` faz `songScore += score` e nada
   mais; o original mostra placar negativo numa passada ruim. O port limitava em
   zero — invenção minha. Medido no harness: o placar chegava a 0 e ficava lá,
   escondendo centenas de penalidades reais. Removido, e o contador de
   telemetria passou de `u32` para `s32` para não virar 4 bilhões no log.
3. **Total de notas zerado no resultado.** `tutorial_notes_reset()` era chamado
   antes do `chart_load`, então `fnf_play_reset()` recebia `total_notes = 0`;
   `fnf_play_completion()` devolve 0 com total zero e o rank saía `NONE`
   (`-1`) em qualquer música. Encontrado no harness (`play_result,-1,0`), não
   por leitura. Ordem corrigida e `play_total_notes` posta no log.
4. **O fim da música dependia do tamanho do arquivo.** O resto do áudio menor
   que o bloco de leitura ficava preso no anel; `audio_ring_eof()` era
   verdadeiro, o anel nunca esvaziava e a música não terminava. Medido: 1.280
   bytes presos depois de retomar da pausa. Corrigido com
   `audio_ring_take_available()`.
5. **Um bloco curto de áudio matava a música** (`tutorial_audio_mix_submit`).
   Corrigido na mesma rodada; ver `docs/PLANO_COMPLETO.md`, Fase 2.

## B — lido e conferido, sem teste que trave

| Item | Fonte | Port |
|---|---|---|
| passo do chart por seção de BPM | `Conductor.stepLengthMs` sai do crochet da seção (`Conductor.hx`) | `chart_samples_per_step()` | teste: Tutorial 100 BPM → 7.200; 2hot 182 BPM → 3.956 |
| espaçamento entre setas | `Strumline.NOTE_SPACING = STRUMLINE_SIZE+8 = 112` (`Strumline.hx:37-42`) | 56,0 px entre receptores (metade) |
| barra de vida | `FlxBar(RIGHT_TO_LEFT, width 601−8=593, valor 0..2)` (`PlayState.hx:810-811`) | `468,25 − 296,5·(vida/2)`; confere com o 320,0 que estava fixo |
| texto do placar | `updateScoreText()` + `formatMoney(score,false,true)` (`PlayState.hx:2748-2760`) | `fmt_score_line`, vírgula de milhar |
| ícones de vida | `HealthIcon.updatePosition()` (`HealthIcon.hx:269-293`) | desenhados nas duas pontas da barra; posição fixa ainda, sem o remap da fórmula |
| easings | 13 nomes, sem cubic/sine | `chart_ease_names` |
| tela de resultado: conteúdo | `ResultState.hx` mostra rank, placar, tallies, maior combo e `%` (`:578-583`) | `ui_state 9` desenha os mesmos campos, dos mesmos `fnf_play_*` que julgaram | teste: rank != NONE e >= 1 quad na tela 9 |
| freeplay | `FreeplayState.hx`: lista de capsules do registro, dificuldade entre as que a música tem, CROSS toca | `ui_state 11`, lista de 27 músicas, `SR_DIFF_*` | teste: 27 na lista, HARD → chart de 48 notas, volta para a Freeplay |
| pausa | `PauseSubState.hx`: substate sobre o PlayState congelado, fundo preto alpha 0,6 em 0,8 s `quartOut`, `y = 70·i + 30`, `justOpened` | `g_paused` mantendo `ui_state 8`, `153·quartOut(t)`, `15 + 35·i` | teste: 1 valor de relógio em toda a pausa, Resume no mesmo ponto, música fecha em 3.225.600 |
| game over | `PlayState.hx:1255` dispara em `health <= HEALTH_MIN`; `moveToGameOver()` zera placar e vida; `GameOverSubState` abre sem transição e bloqueia entrada por 1 s | `ui_state 10`, `fnf_play_move_to_game_over`, 60 quadros de bloqueio | teste: morte com 13 misses, tela desenhada, reinício tocando de novo |
| contagem regressiva | `Countdown.hx`: Conductor em -5 batidas, `FlxTimer` de uma batida, 5 voltas, alpha 1→0 em `FlxEase.cubeInOut` | `countdown_*` em `main.c` com batida = 4 passos do chart | teste: 5 passos, e música só começa depois do AFTER |
| total de notas do resultado | `tallies.totalNotes` = notas do jogador | `fnf_play_reset` com `must_press` contados **após** o chart carregar | teste: `play_total_notes` = 32 no Tutorial normal |

## C — não auditado (o que continua sendo fé)

- geometria de **todas** as telas de menu/story (só o HUD do jogo foi auditado aqui);
- os 445 eventos de chart: o módulo `chart_events.c` tem testes próprios, mas a
  conferência evento a evento contra `source/funkin/play/event/` não foi feita;
- palcos, personagens e props (46/21): os packs foram medidos por IoU, não por
  comparação de transformação com o `flxanimate`;
- **o que desenha** com cada endereço de VRAM: o mapa (`engine/vram_layout.c`)
  é tabela conferida contra o alocador real do ps2sdk, e o boot registra
  calculado × alocado por slot (`vram_mapa 0` no log). Mas `vram_cache_init()`
  continua sem uso, e nenhum teste confere se o conteúdo certo foi para o slot
  certo;
- geometria da tela de resultado (só o conteúdo foi ligado; posições e arte não
  foram conferidas contra `ResultState.hx`);
- a ARTE e o SOM da contagem regressiva (o tempo e a curva são do original; o
  desenho é texto e os sons do `noteStyle` não estão no pacote);
- a ARTE de morte (animação `firstDeath` do personagem) e a música de game over;
- a arte do menu de pausa (ícone, círculo, AtlasFont) e a música de pausa;
- "Enable Practice Mode": depende do Practice Mode, que ainda não existe;
- a arte das telas de Freeplay (capsules) e de pausa (ícone, círculo,
  `AtlasFont`) -- a informação e o comportamento estão no port, a arte não.

## Regra para as próximas fases

Um item só sai do nível B ou C para o A com um teste que falhe se o número
mudar. Enquanto isso não acontecer, este documento diz B ou C — não A.
