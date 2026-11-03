# FNF para PlayStation 2 — o port, o que está pronto e o que falta

Port completo do **Friday Night Funkin'** (v0.8.6) para o PS2, em C com o
`ps2sdk` oficial: **27 músicas, 81 charts (3 dificuldades), 46 personagens,
21 palcos, 35 note kinds**.

Tudo o que este documento afirma foi **medido**, e o comando que mede está
escrito junto. O que não roda neste ambiente (o console, o cartão de memória, o
clone de arte de 2,5 GB) está na última secção, com nome e motivo — nunca
escondido atrás de "quase pronto".

---

## O jogo é jogável de ponta a ponta

O fluxo do original está inteiro: **título → menu principal → STORY MODE
(semanas, músicas emendando) ou FREEPLAY (cápsulas, dificuldade, preview de
20 %, favorito, cinemática de rank) → música (contagem, notas, HUD, vida,
placar, combo, splashes, note kinds) → pausa → game over (com retry) →
Resultado (rank, selo, arte) → volta ao menu.** O save de 1 992 B grava, lê e
recusa arquivo estragado (CRC).

E isso vale para **todas** as músicas, até o fim:

```
$ python3 tools/maratona_harness.py --todas
81 rodada(s) em 234,7 s
MARATONA: PASSOU -- as 81 rodada(s) chegaram ao Resultado pelo fim do chart,
com a faixa visivel intacta
```

Cada rodada vai do primeiro ao último evento do chart (mais a cutscene de 30,4 s
e a sequência do fim, quando é a `spaghetti`), com o autoplay apertando as notas
do próprio chart, e cobra `state,9` (o Resultado), `play_result`, **0 retângulo
e 0 linha** fora da faixa visível, e a ausência de erro de VRAM/retry.

## O que está no jogo, com o número de cada parte

| Área | Estado | Medida |
|---|---|---|
| Músicas / charts / dificuldades | **27 / 81 / 3**, todas jogáveis até o Resultado | maratona acima |
| Personagens | **46/46** empacotados do clone de arte | `assets-data/characters` + `build/packs/chars` |
| Palcos | **21/21** (84 props) + piso em perspectiva, poeira, céu que rola | `src/stage_dados.h` |
| Scripts de palco | **18/18** portados (eventos, timers, tweens, `FlxEase`, 5 sprites criados por script, trem em laço, luzes, carros em Bézier) | `src/engine/stage_script.c` |
| Note kinds | **35** no dado, 34 usados nos charts | `assets-data/notekinds.json` |
| Cutscene do `sserafim` | 631 quadros de geometria assada, 30,4 s, skip em ENTER/A, fades, câmera | `src/engine/ss_cutscene.c` |
| Fim da música do `sserafim` | `endStuff` → SEEYOU1 → SEEYOU2 → `endSong(true)` em 9,000 s; os dois `.cfz4` do fim vão para a RAM **no começo da cena** (não no último segundo) | `cut_fim_pre,0,39951` · `cut_fim_desenho,237,240` · `cut_endstuff_fim,432000` |
| Áudio | anel de 8 B/amostra (Inst L/R + Vox L/R), 48 kHz; mixer soma as duas faixas | `src/audio_ring.c`, `src/engine/audio_mix.c` |
| VRAM | orçamento por palco/personagem medido, LRU com despejo, folga mínima registrada | `tools/vram_budget.py`, `cut_pool_min` |
| Otimização (Fase 5) | custo por quadro medido nas 81 combinações | `build/sweep/custo.txt`, média **121/100** uploads de personagem por quadro |

## Como construir e testar

```sh
sh test/run_tests.sh          # TUDO: 31 suites de PC + harness do jogo real + o montador do pendrive (~10 min)
make                          # o ELF do PS2 (precisa do ps2sdk)
sh tools/build_pendrive.sh    # monta a pasta do pendrive (recusa se faltar algo)
```

O `test/run_tests.sh` roda, entre outras coisas:

* as suites de PC de cada motor (chart, julgamento, VRAM, palco, cutscene…);
* o **harness do jogo real**: o `main.c` de verdade, ligado contra o código de
  `draw`/`packet` do ps2sdk, com zona de guarda e limite de pacote;
* ASan + UBSan no jogo inteiro (1 401 quadros, zero erro e zero vazamento);
* o **sweep** das 81 combinações e o sweep v5 dos note kinds;
* a **maratona** das 27 músicas até o Resultado;
* o **fim do `sserafim`** (a cadeia SEEYOU inteira, 14 000 quadros);
* Story, Freeplay, Game Over, Resultado, pausa, save e o áudio (anel, mixer,
  autoteste do empacotador de stems);
* o **montador do pendrive** (`tools/build_pendrive.sh`), montado duas vezes:
  com tudo (tem de sair com os 8 arquivos da cutscene, 157 props, 27 músicas,
  324 charts) e **sem o `end2.cfz4`** (tem de recusar, dizer `7 de 8` e não
  copiar nada). Foi esse teste que achou dois defeitos que só apareceriam no
  console: o `<name>.cfz4` de exemplo do `stage_dados.h` abortava a cópia dos 21
  palcos **sempre**, e o `end1`/`end2` do `endStuff` nunca eram copiados.

## Os documentos

| arquivo | conteúdo |
|---|---|
| `docs/PLANO_COMPLETO.md` | **o plano e o diário do port**, item por item, com o número medido de cada um (~5 500 linhas) |
| `docs/TESTE_NO_CONSOLE.md` | o roteiro da rodada no PS2 e o que cada linha do log responde |
| `HARNESS.md` / `HARNESS_TOOLCHAIN.md` | o harness do PC: o que ele prova e o que **não** prova |
| `google_drive/handoff/REGRAS.md` | as 10 regras de trabalho que este projeto segue |

## O que falta — e por quê (nada é mistério)

| Falta | Motivo | O que resolve |
|---|---|---|
| os 27 `stems.pcm` no pendrive | a ferramenta está pronta e testada (`tools/empacotar_stems.py`), mas o áudio de origem está no clone de arte de 2,5 GB, que **não está neste ambiente**. Hoje só o Tutorial tem arquivo | `python3 tools/empacotar_stems.py --assets <clone>` (≈1,08 GB, ~46,8 min de música) |
| as telas de OPTIONS / MERCH / CREDITS | o `.hx` e a arte do menu de opções não estão neste ambiente; implementar sem eles seria inventar layout (regra 4) | o fonte/arte do `OffsetMenu`/`OptionsMenu`, ou uma captura do original |
| 175 trechos de shader nos scripts de palco | o GS de pipeline fixa **não tem** estágio de fragmento (`wiggle.frag`, chuva, blur, color grade). O resto dos 666 trechos que mudam estado está portado | decisão de arte: degradar a imagem ou desenhar o efeito com sprites |
| `HapticUtil.vibrate` | o pad do PS2 **tem** rumble (`padSetActDirect`); o que falta é o padrão (intensidade/duração) do `HapticUtil` de PC | o `HapticUtil.hx` ou uma captura do original |
| `MULTIPLY`/`SCREEN` | o GS só faz `NORMAL` e `ADD`; 3 de 16 props pedem as outras duas. O port **conta** cada caso (`palco_blend`) em vez de fingir | um segundo passe com o frame buffer como textura |
| o cartão de memória real (`mc0:`) | formato, gravação, CRC e leitura rodam no PC; o `mcInit()` e os IRX do cartão são do console | um PS2 com cartão |
| **a rodada no hardware** | o harness prova link, desenho e conta — **não** prova I/O do `usbhdfsd`, tempo de leitura nem o `audsrv` no console (regra 3) | ligar o pendrive e devolver o `FNF_COLHEITA.txt`, como em `docs/TESTE_NO_CONSOLE.md` |

## O que este port **não** faz (de propósito)

Nenhum evento do original foi removido, encurtado, adiantado ou simplificado:
mesmos zooms, mesmas trocas de personagem, mesmos bops de palco, mesmos easings,
mesmas animações, mesmos efeitos, na mesma ordem e no mesmo tempo. Onde a arte
não cabia, a **imagem** cedeu (escala, recorte, degradação) — o jogo, não.
