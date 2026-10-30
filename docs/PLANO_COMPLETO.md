# Plano para terminar o port — tudo, de uma vez, em etapas

Este documento existe porque a regra 2 do `REGRAS.md` manda escrever o plano
antes do código. Ele é a lista de trabalho, na ordem em que o trabalho será
feito, com o **estado real medido** de cada item e com os **números tirados do
original** (`FunkinCrew/Funkin`, tag `v0.8.6`) que cada item precisa.

Nada aqui é estimativa de "quanto falta": cada item diz o que já existe no
código do port, com arquivo e função. Onde o port já faz a coisa certa, o item
está marcado como feito — não será refeito.

---

## O que o usuário pediu

Port completo: **27 músicas, 46 personagens, 21 palcos, 3 dificuldades, 81
charts**, otimizado ao máximo, rodando no console. E, nesta rodada: *terminar
tudo*, sem parar para mais uma rodada de diagnóstico, aceitando que leve
várias sessões, com revisão do código final.

Isso reordena as prioridades anteriores: o diagnóstico de travamento deixa de
ser o item 1 e passa a ser um **acessório** que continua ligado (as marcas já
escritas ficam), mas o trabalho passa a ser construir o jogo que falta.
A justificativa não é preferência: sem gameplay completo não existe jogo para
otimizar, e a otimização sem o jogo inteiro é otimização de um alvo que muda.

---

## Fatos do original que o port tem de reproduzir (lidos da fonte, não inferidos)

`source/funkin/play/scoring/Scoring.hx`:

| Sistema | Janela total | SICK | GOOD | BAD | SHIT | Pontos SICK/GOOD/BAD/SHIT/MISS |
|---|---|---|---|---|---|---|
| PBOT1 (**o que o PlayState usa**) | 160 ms | ≤45 ms | ≤90 ms | ≤135 ms | ≤160 ms | 500 (sigmóide) / −100 |
| LEGACY | 166,67 ms | 20 % | 75 % | 90 % | 100 % | 350/200/100/50 / −10 |
| WEEK7 | 166,67 ms | 20 % | 55 % | 80 % | 100 % | 350/200/100/50 / −10 |

PBOT1: `MAX=500`, `OFFSET=54.99`, `SLOPE=0.080`, `MIN=9.0`, perfeito < 5 ms →
500; `score = int(500 * (1 - 1/(1+exp(-0.080*(|ms|-54.99)))) + 9)`.

`source/funkin/util/Constants.hx`:

| Constante | Valor |
|---|---|
| `HEALTH_MAX` / `HEALTH_STARTING` / `HEALTH_MIN` | 2,0 / 1,0 / 0,0 |
| `HEALTH_SICK_BONUS` | +1,5 % de 2 = **+0,03** |
| `HEALTH_GOOD_BONUS` | +0,75 % = **+0,015** |
| `HEALTH_BAD_BONUS` | **0,0** |
| `HEALTH_SHIT_BONUS` | −1,0 % = **−0,02** |
| `HEALTH_MISS_PENALTY` | −4,0 % = **−0,08** |
| `HEALTH_GHOST_MISS_PENALTY` | **−0,08** (aperto sem nota) |
| `HEALTH_HOLD_BONUS_PER_SECOND` | +6,0 %/s = **+0,12/s** |
| `SCORE_GHOST_MISS_PENALTY` | **−10** |
| `SCORE_HOLD_BONUS_PER_SECOND` | **+250/s** |
| `JUDGEMENT_*_COMBO_BREAK` | BAD e SHIT quebram; SICK, GOOD e KILLER não |
| `RANK_PERFECT/EXCELLENT/GREAT/GOOD_THRESHOLD` | 1,00 / 0,90 / 0,80 / 0,60 |

`PlayState.applyScore()` — a ordem importa: soma o tally do julgamento, aplica
`health += healthChange`, quebra ou incrementa o combo, e só então
`songScore += score`.

`ResultState` — "clear percent" = `floor(tallyCompletion*100)`, onde
`tallyCompletion = clamp((sick+good-missed)/totalNotes, 0, 1)`. Em 0.8.6 **não
existe** métrica separada chamada "accuracy": esse número é o do resultado, e
é ele que o port vai mostrar. Rank: `sick == totalNotes` → PERFECT_GOLD;
senão, completion 1,00 → PERFECT, ≥0,90 EXCELLENT, ≥0,80 GREAT, ≥0,60 GOOD,
abaixo disso SHIT.

---

## Fases, com o estado medido do port

### Fase 1 — Núcleo de gameplay (pontuação, vida, combo, julgamento, resultado)

**Existe hoje:** `src/main.c` tem um caminho de tutorial próprio
(`tutorial_notes_update`, linhas ~696) que soma pontos com três faixas fixas,
conta acertos e erros, e **não tem vida, não tem combo com quebra, não tem
resultado, não tem rank**. `src/chart_events.c` cuida dos eventos de palco.
`src/chart.c` entrega as notas.

**Falta:** um módulo portátil e testável que seja a tradução literal de
`Scoring.hx` + `Constants.hx` + `applyScore()`. É o que esta fase entrega:
`src/engine/gameplay.c` / `.h`, sem dependência de console, com suíte própria.

### Fase 2 — PlayState genérico (qualquer música, qualquer dificuldade): FECHADA

**Fechada** — detalhes e medições na seção "Fase 2 — PlayState genérico:
FECHADA", abaixo. Em uma linha: música e dificuldade saem do
`song_registry`/`story_level`, o chart e o áudio são por música
(`songs/<id>/<dif>.chart`, `songs/<id>/stems.pcm`), a contagem regressiva tem os
5 passos de `Countdown.hx` e o fim da música leva à tela de Resultado com rank e
`%` das mesmas funções que julgaram. O que continua pendente é o **mixer de
duas faixas** (`Inst` + `Voices`), que hoje é um `stems.pcm` já misturado.

### Fase 3 — Telas: Freeplay, seleção de dificuldade, pausa, Game Over, Resultado: FECHADA

**Fechada** — detalhes na seção "Fase 3 — Telas: FECHADA", abaixo. As quatro
telas existem e cada uma tem harness próprio: `ui_state 9` (Resultado), `10`
(Game Over, com reinício por CROSS), `11` (Freeplay: 27 músicas, 153 quads,
dificuldade HARD, 48 notas, 506 quadros de volta) e a pausa (congela o relógio,
73 quads de sobreposição, Resume/Restart/Change Difficulty). Falta só a **arte**
dessas telas (cápsulas da Freeplay, ícone de pausa, `AtlasFont` e `firstDeath`).

### Fase 4 — VRAM: FECHADA EM PARTE (1 de 4 passos medidos)

**Existia:** 8 reservas de VRAM no init, zero liberações; 8 buffers = 99,4 % da
VRAM; 2.929.792 B de imagens embutidas para 2 de 4 telas.
`src/engine/vram_cache.c` (LRU) existe, testado, e `vram_cache_init()` é
chamado **zero** vezes em `main.c`.

**Falta:** ligar o cache, fatiar as imagens grandes, 16 bpp nas folhas pequenas,
frame buffers 512×384 PSMCT16, repack por palco.

#### Fechado: o mapa de VRAM agora é uma tabela conferida contra o alocador

Antes o mapa existia só na ordem de oito `graph_vram_allocate()` em `main.c` e
num texto no documento ("99,4 % da VRAM") que nada verificava. Uma reserva que
estourasse devolvia `-1` e o jogo desenhava num endereço inválido, em silêncio.

Agora `src/engine/vram_layout.{h,c}` é o mapa, em tabela, e
`test/test_vram_layout.c` (dentro de `test/run_tests.sh`) confere:

| Conferência | Valor |
|---|---|
| conta de tamanho igual a `graph_vram_size()` do ps2sdk | 640×448 PSMCT32 = 286.720 palavras; 320×64 PSMCT16 = 10.240; largura arredondada em 64 |
| endereços dos slots | `fb1` 286.720, `tex` 573.440, `tex2` 655.360, `gftex` 737.280, `gftex16` 802.816, `stagetex` 813.056, `fonttex` 944.128 |
| total | 1.017.856 palavras = 4.071.424 B — **cabe** em 4.194.304 B |
| livre | 30.720 palavras = 122.880 B |

(Este quadro é o estado logo depois de a fonte encolher. O framebuffer de 16
bits, na seção seguinte, muda todos esses endereços para baixo — os números
finais estão lá.)

E o harness passou a rodar a **conta de verdade**: `graph_vram_allocate()` no
`test/ps2harness/ps2h.c` devolvia `0` sempre, então no PC todas as texturas
ficavam no mesmo endereço e nenhum teste notava. O corpo agora é o de
`ee/graph/src/graph_vram.c` e, no boot, o jogo registra calculado × alocado por
slot:

```
VRAM fb0       640x448  psm  0 calculado        0 alocado        0 igual
VRAM fb1       640x448  psm  0 calculado   286720 alocado   286720 igual
VRAM tex       320x256  psm  0 calculado   573440 alocado   573440 igual
VRAM tex2      320x256  psm  0 calculado   655360 alocado   655360 igual
VRAM gftex     256x256  psm  0 calculado   737280 alocado   737280 igual
VRAM gftex16   320x64   psm  2 calculado   802816 alocado   802816 igual
VRAM stagetex  512x256  psm  0 calculado   813056 alocado   813056 igual
VRAM fonttex   384x192  psm  0 calculado   944128 alocado   944128 igual
```

E a primeira economia saiu de graça: `fonttex` era reservado 384×256 mas o atlas
é `FONT_W × FONT_H` = 384×192 (células 24×32). **−24.576 palavras = 98.304 B**,
antes só havia 24.576 B livres. Evento `vram_mapa` no log: `0` divergências.

Medido: `tools/run_gameplay_harness.sh 5600` → PASSOU; `test/run_tests.sh`
completo → `ALL SUITES PASSED`.

#### Fechado: framebuffer 640×448 em 16 bits (PSMCT16)

O par de buffers de quadro era PSMCT32: 286.720 palavras cada, **573.440 no
par**. Em PSMCT16 cada um cai para 143.360 e o par para 286.720 —
**1.146.880 bytes de VRAM de volta**, com a resolução *igual*. Nenhuma
coordenada de nada mudou: o que muda é a profundidade de cor da tela.

| | antes (PSMCT32) | agora (PSMCT16) |
|---|---|---|
| framebuffer | 1.146.880 B | 573.440 B |
| par de framebuffers | 2.293.760 B | 1.146.880 B |
| VRAM usada no total | 4.071.424 B | **2.924.544 B** |
| VRAM livre | 122.880 B | **1.269.760 B** |
| endereço da `tex` | 573.440 | 286.720 |
| endereço da `fonttex` | 944.128 | 657.408 |

O que vem junto e é obrigatório:

- `draw_enable_dither16()`: `draw_setup_environment()` do ps2sdk **desliga** o
  dither a cada quadro (o padrão dele assume framebuffer de 32 bits). Sem
  dither, 5 bits por canal mostram escada em qualquer degradê. São 2 quadwords
  por quadro, nas três montagens de pacote (menu, jogo e títulos do story).
- O `FBA` o ps2sdk já escolhe sozinho: ele lê `frame->psm` e põe
  `ALPHA_CORRECT_RGBA16` quando o framebuffer é de 16 bits.
- A captura de tela (`safe_readback_rect` + `capture_framebuffer_raw`) agora lê
  2 bytes por pixel (`QWC = largura*altura/8`, `BITBLTBUF`/`TRXREG` em PSMCT16)
  e **expande de volta para 32 bits no próprio buffer, de trás para frente**.
  O arquivo `.rgba.txt` continua idêntico ao de antes, byte por byte na
  ordem R,G,B,A com A=255 — as ferramentas que leem a captura não mudam.

Provado no PC: mapa de VRAM com `vram_mapa 0` (calculado × alocado, oito slots
iguais), `tools/run_gameplay_harness.sh 5600` PASSOU, suíte inteira
`ALL SUITES PASSED`.

**O que NÃO está provado:** como isso *aparece*. O harness não tem tela — ele
prova que o GS recebeu os registradores certos, não que o dither ficou bom nem
que a captura PSMCT16 sai legível. Isso só o console responde, e é a primeira
coisa a olhar na próxima build (o log já traz `capture_status` e o
`screen_capture_raw`).

### Fase 6 — Palcos e personagens genéricos (o que falta para 21 e 46)

**Estado medido:** o port desenha **um** palco (`mainStage`, embutido no código
como `mainstage_tutorial_parallax.rgba`) e **dois** personagens (`bf`, `gf`, em
pacotes `bf_frames.lz4f` / `gf_frames.lz4f`). Os dados dos outros 20 palcos e 44
personagens **já existem e estão commitados**, só não há quem os consuma:

| arquivo | o que tem | cabe? |
|---|---|---|
| `src/bs_dados.h` | 46 personagens (offsets, cameraOffsets, singTime) e 21 palcos (`cameraZoom` + posição dos três slots bf/gf/dad) | — |
| `src/stage_dados.h` | 157 props: `tex` = `stages/<palco>/<nome>.cfz4`, `x/y`, `sx/sy`, `scrollx/y`, `z` (zIndex), `danceEvery`, `frames`, `sw/sh` (um quadro já na escala do palco) e a lista de animações com fps e loop | — |
| `assets-data/stages/*.json` | a fonte de tudo acima (posições, escalas, scroll) | — |
| `assets-data/characters/*.json` | 46 personagens: `renderType`, `offsets`, `cameraOffsets`, `singTime`, `death`, `animations` | — |
| `assets-data/charts/` | 27 JSON de música (os 81 charts de 3 dificuldades saem do conversor `tools/fnf_to_fch3.py`) | — |

**Os dois orçamentos, medidos** (`tools/vram_budget.py` + os dados acima):

| palco (pior caso de cada) | VRAM só com o canvas de cada prop | RAM com o pacote inteiro |
|---|---|---|
| `phillyStreets` | 1.697.934 B | 5,25 MB |
| `tankmanBattlefield` | 1.697.760 B | 14,4 MB |
| `limoRideErect` | 1.697.420 B | **48,7 MB** |
| `spookyMansion` | 1.696.282 B | **59,4 MB** |
| `school` | 1.497.040 B | 21,6 MB |
| `schoolEvilErect` (melhor) | 504.534 B | 0,5 MB |

Duas conclusões que **mudam o plano**, as duas tiradas dos números e não de
suposição:

1. **VRAM:** o pior palco pede 1,70 MB e o jogo tem 1,27 MB livres com as
   texturas de menu/título residentes. **18 dos 21 palcos não cabem** nesses
   1,27 MB; cabe só **evictando** o que não é do jogo durante a música. É exatamente para isso que existem `vram_cache.c`
   (LRU, testado, `vram_cache_init()` ainda com **zero** chamadas) e
   `old_motor/asset_bank.c` (classes `AB_PALCO`/`AB_PERSONAGEM`, folha
   compartilhada). Sem eles, 18 dos 21 palcos não cabem.
2. **RAM:** o pacote inteiro do `spookyMansion` tem **59,4 MB** e o Emotion
   Engine tem 32 MB. O empacotador dimensionou tudo por VRAM e **nunca conferiu
   a RAM**. Quatro palcos (`limoRideErect` 46,5 MB, `spookyMansion` 56,6 MB,
   `school` e `schoolErect` 20,6 MB) passam de 16 MB, ou seja, não caberiam
   nem com o jogo vazio. Antes de escrever o
   renderizador, o empacotador precisa de um orçamento de RAM por palco (baixar
   a escala dos quadros grandes — a fidelidade da imagem é negociável, a RAM
   não) ou de um decodificador que leia do pendrive por quadro sem manter o
   pacote todo.

**Ordem de trabalho proposta (um motivo por build):**

1. ~~`stage_layout.c`: função pura palco × câmera → props visíveis, ordenados
   por z, com retângulo de tela~~ — **FEITO** (ver abaixo).
2. ~~Orçamento de RAM no empacotador + conferir o plano~~ — **FEITO o
   orçamento e a conferência** (abaixo); falta **reempacotar**, que precisa do
   clone de 2,5 GB da arte e por isso não roda aqui.
3. ~~`vram_cache_init()` de verdade~~ → **o pool existe e está medido**
   (`src/engine/stage_pool.{h,c}`, abaixo). Falta pendurá-lo no `main.c` e tirar
   `gftex`/`stagetex` de fora dele, que é o que libera o orçamento inteiro.
4. Estado 8 desenha o palco da música (não o `mainStage` fixo) e os
   personagens do registro (`sr_level(n)->songs[0]->stage/player/opponent/
   girlfriend`), com `danceEvery`, animações de sing/hit e a câmera do palco.

**Decisão tomada para os passos 3 e 4 (medida, não escolhida no gosto):**

Hoje o jogo tem 1.269.760 B livres de VRAM e o pior palco precisa de 1.697.934 B,
então não cabe sem devolver algum slot. Os slots que o **estado 8 realmente
liga** foram levantados lendo o próprio bloco de desenho do jogo:

| textura | quem usa no estado 8 | tamanho |
|---|---|---|
| `fonttex` | HUD (texto) | 294.912 B |
| `g_gftex16` | GF | 40.960 B |
| `tex` | BF | 327.680 B |
| `tex2` | notas/receptores | 327.680 B |
| `stagetex` | a janela do palco do tutorial | 524.288 B |
| `gftex` | **ninguém** no gameplay (é do título) | 262.144 B |

Então, durante a música, `gftex` (262.144) e `stagetex` (524.288) podem ser
devolvidos — `stagetex` justamente porque o palco genérico o substitui. Com
isso o pool de props fica em **1.269.760 + 786.432 = 2.056.192 B**, maior que o
pior palco (1.697.934 B), sobrando ~358 KB para os personagens.

O `vc_*` (`src/engine/vram_cache.h`, LRU, já testado) passa a ser esse pool:
chave = índice do prop em `bs_props[]`, tamanho = `sw*sh*2`, despejo por LRU
quando não couber. Prop que não cabe de jeito nenhum vira evento
`stage_over` no log — nunca é omitido em silêncio.

#### Fechado: o pool de texturas do palco (`stage_pool.c`)

`src/engine/stage_pool.{h,c}` + `test/test_stage_pool.c` (dentro de
`test/run_tests.sh`). Quem desenha pede a textura **por chave** (o índice do prop
em `bs_props[]`); residente custa zero, não coube despeja a menos usada
recentemente, e não caber de jeito nenhum é falha contada — nunca um desenho com
endereço inválido. O trabalho de alocar/despejar é do `vc_*`
(`vram_cache.c`, que já tinha teste); aqui fica a chave derivada do prop, o
tamanho vindo de **tabela gerada** e a contabilidade.

A tabela é `src/engine/stage_vram_data.h`, gerada por
`tools/gen_stage_vram.py` do mesmo `assets-data/stage-props.json` que gera o
`stage_dados.h` — e o `run_tests.sh` roda `gen_stage_vram.py --check`, então ela
não pode envelhecer sem o teste falhar.

Medido, com os 157 props reais:

| | |
|---|---|
| orçamento do pool (`SP_POOL_BYTES`) | 2.056.192 B = 4.194.304 − 1.146.880 (2 framebuffers) − 991.232 (fonttex + gftex16 + tex + tex2) |
| pior palco (`tankmanBattlefield`) | **1.700.352 B** |
| sobra no pior caso | **355.840 B** |
| palcos que não cabem | **0 de 21** |

O teste também prende o que já errou antes: unidade de endereço vs de tamanho
(o `vc_*` trabalha em **bytes** e o GS em **palavras** — misturar os dois passava
com o pool pela metade; agora a conversão mora dentro do `stage_pool` e o teste
compara em bytes), palco que não cabe falhando limpo (`-1`, sem endereço), e
troca de palco devolvendo o pool inteiro.

#### Fechado: o pool no jogo, e o orçamento inteiro de 2.056.192 B

O `main.c` mudou a **ordem** de alocação para o mapa ficar com os slots que o
gameplay liga sempre no começo e todo o resto como pool:

```
0        fb0 (640x448 PSMCT16)      143.360 palavras
143.360  fb1                        143.360
286.720  tex  (BF)                   81.920
368.640  tex2 (notas)                81.920
450.560  fonttex (HUD)               73.728
524.288  gftex16 (GF no jogo)        10.240
534.528  <-- pool: 514.048 palavras = 2.056.192 B
           gftex    534.528 (fixa nesta build)
           stagetex 600.064 (fixa nesta build)
           livres   731.136 .. 1.048.575  -> 1.269.760 B para props
```

`gftex` e `stagetex` deixaram de ser reservas do `graph_vram_allocate` e passaram
a ser **entradas do pool** (chaves `SP_CHAVE_TELA_BASE+0/1`), fixadas para não
serem despejadas nesta build — os endereços mudaram de lugar, mas continuam
imutáveis durante a sessão, então nada do que desenha hoje precisou mudar.

Medido no harness (`tools/run_harness.sh 300`), no boot:

```
VRAM fb0/fb1/tex/tex2/fonttex/gftex16 ... calculado == alocado  (6 slots)
E,...,pool,534528,2056192          pool comeca em 534.528, tem 2.056.192 B
E,...,pool_tela,534528,600064      gftex em 534.528, stagetex em 600.064
E,...,pool_livre,1269760,0         1.269.760 B livres, 0 despejos
E,...,vram_mapa,0,514048           0 divergencias no mapa
```

E `test/test_vram_layout.c` passou a prender os dois números que unem os dois
módulos: `vl_pool_base() == 534.528` e `vl_pool_size_bytes() == 2.056.192`,
que é exatamente o `SP_POOL_BYTES` do `stage_pool`. Se um slot fixo crescer, o
teste do pool acusa "palco sem lugar" em vez de o jogo desenhar por cima.

**O que falta para o pool valer os 2 MB inteiros:** soltar a fixação de `gftex`
e `stagetex` no estado 8 (elas são de título/menu/story, e a reentrada nesses
estados já reenvia as duas) e desenhar os props do palco com `sp_prop()`. É o
passo 4, com o tutorial como primeiro caso de teste.

**Primeiro teste do renderizador novo é o próprio tutorial**: o `mainStage` tem
3 props e 1.697.032 B, cabe no pool, e o resultado tem de ser o mesmo que a
textura composta que está lá hoje. É o caminho mais barato para achar erro de
posição, porque existe captura de tela do palco atual no console
(`evidencia-console/`) para comparar.
5. Os 46 personagens: `bf_meta_t` (`MAX_ANIMS 8`) vira tabela gerada por
   personagem, com `renderType` (`multianimateatlas` para boa parte deles) — é
   o item mais caro e o único que ainda não tem formato de pacote definido.

#### Fechado: `stage_layout.c`, a conta de "onde cada prop cai"

`src/engine/stage_layout.{h,c}` + `test/test_stage_layout.c` (dentro de
`test/run_tests.sh`). É C puro: nada de GS, nada de arquivo, nada de VRAM —
por isso roda no PC, contra os mesmos dados que o console vai usar.

| O que o teste prova | Número |
|---|---|
| os 21 palcos existem, e o id é o id inteiro (`mainStageXX` não acha nada) | 21 |
| as faixas `first/count` de cada palco particionam `bs_props[]` sem buraco e cada prop pertence ao palco que diz | 157 props |
| a conta de tela é a que o port já usava no tutorial | `stageBack` do `mainStage`, câmera no foco do BF (888,5 ; 589) e zoom 1,1: `x0 = -485,0075`, `y0 = -241,3550`, `w = 1408`, `h = 770` |
| o fundo cobre a tela inteira | `x0 ≤ 0`, `y0 ≤ 0`, `x0+w ≥ 640`, `y0+h ≥ 360` |
| ordem de desenho é zIndex crescente | 21 palcos |
| prop fora da tela não entra no pacote | `fastCar` (x = −12600) nunca aparece; o fundo do mesmo palco sempre |
| animação em loop volta ao zero; sem loop para no último quadro | 24 fps, 4 quadros |

Um erro foi encontrado **ao escrever o teste**: o cabeçalho gerado
(`stage_dados.h`) trazia `sw/sh`, o tamanho do quadro **já reduzido** para caber
na VRAM (escala 0,31 no `mainStage`), e nenhum campo com o tamanho do quadro no
espaço do original. Desenhar com `sw/sh` encolheria o palco inteiro em três
vezes — e ninguém veria isso no PC, porque o teste não tinha como perguntar. O
gerador (`tools/gen_stage_dados.py`) passou a emitir `src_w/src_h` e o
`stage_layout` calcula a tela com eles, deixando `tex_w/tex_h` para a UV.

**O que ele não prova:** que o desenho acontece. Este módulo decide *o quê*,
*onde* e *em que ordem*; quem desenha é o passo 4.

#### Fechado: orçamento de RAM do empacotador e conferência dos 21 palcos

O empacotador só tinha orçamento de **VRAM**. O pacote CFZ4 de um palco, porém,
fica **inteiro** na RAM do Emotion Engine enquanto a música toca, e a RAM nunca
foi conferida — `spookyMansion` saiu com **59 MB** de pacote num console de
**32 MiB**. Nada no repositório reclamava.

Agora há dois orçamentos, e a escala de cada palco é a menor das duas:

| Recurso | Paga por | Orçamento (medido) |
|---|---|---|
| VRAM | **um** quadro de cada prop | 1.337.520 B pelo modelo; **1.269.760 B** livres no log da 0.2.117 |
| RAM | **todos** os quadros de **todos** os props | **16.182.527 B** = 32 MiB − ELF 5.278.492 − buffers 7.899.109 − staging 4 MiB |

As três parcelas da RAM têm fonte, nenhuma é estimativa de planilha: o tamanho do
ELF está no `docs/DIAGNOSTICO_TRAVA_CONSOLE.md`, a soma dos buffers sai do
**próprio jogo** (evento `ram_buffers`, medido no boot: 7.899.109 B) e o staging
é a janela de trabalho (um quadro descomprimido + bloco LZ4 + anel de áudio).

Além disso o `tools/vram_budget.py` modelava framebuffers de **512×384**, que o
port nunca usou: o console roda 640×448 PSMCT16 (573.440 B cada). O modelo
passou a ser o do jogo — um orçamento calculado para uma tela que o jogo não tem
é orçamento para outro jogo.

`tools/check_stage_budget.py` (novo, offline, dentro de `test/run_tests.sh`)
confere o **plano commitado** contra os dois orçamentos e diz a escala que
fecharia cada um. Medido agora:

```
VRAM: 15 de 21 palcos acima do orçamento
RAM:   4 de 21 palcos acima do orçamento -- limoRideErect, school, schoolErect, spookyMansion
  spookyMansion   agora 0.367 -> VRAM 0.326 / RAM 0.192  => usar 0.192
  limoRideErect   agora 0.335 -> VRAM 0.298 / RAM 0.193  => usar 0.193
  school          agora 1.000 -> VRAM 0.945 / RAM 0.866  => usar 0.866
```

Ou seja: **o plano atual não cabe** e isso agora está medido e no relatório de
todo teste, em vez de aparecer como "o jogo trava/tem buraco" no console. A
dívida fecha com `python3 tools/pacotar_palcos.py --assets <clone>` (o
empacotador já aplica a menor escala) e depois
`python3 tools/check_stage_budget.py --strict`, que é o modo que falha.

**O tutorial de hoje não é afetado**: ele desenha uma única textura de janela
(`stagetex`, 512×256), não os três props — a troca para o renderizador genérico
é o passo 4, e é ele que passa a depender deste orçamento.

### Fase 5 — Otimização medida

**Existe hoje:** harness de PC que mede quadros, pacotes, qwords, overflows.

**Falta:** usar o mesmo harness para medir antes/depois de cada mudança e
registrar a tabela neste documento. Sem tabela, nada é declarado ganho.

---

### Fase 3 — Telas: FECHADA (arte das telas ainda pendente)

**Fechado: Game Over (`ui_state == 10`, `GameOverSubState.hx`).**

Antes: a vida chegava a zero, a barra esvaziava e **o jogo continuava**. Dava
para terminar a música com saúde zero, e o resultado saía como se a passagem
tivesse sido normal. Além disso o harness apertava no aleatório e produzia 23
ghost misses — ou seja, ele vinha testando justamente uma situação em que o
jogador já estaria morto no original.

Agora:

| Item | Original | Port |
|---|---|---|
| gatilho | `health <= HEALTH_MIN` (`PlayState.hx:1255`) | mesmo teste em `g_play.dead` |
| ao morrer | pausa música/vozes, `songScore = 0`, `health = STARTING` | igual (`fnf_play_move_to_game_over`) |
| transição | **sem fade** (`skipNextTransIn/Out = true`) | troca imediata de `ui_state`, sem fade |
| tela | fundo preto opaco, texto "Game Over", BF em `firstDeath` | fundo preto + texto (a arte de morte não está no pacote) |
| entrada | bloqueada 1 s (`canInput`), depois CROSS reinicia e CIRCLE sai | 60 quadros, depois CROSS reinicia e CIRCLE vai ao Story |
| reinício | fecha o substate, `needsReset` | transporte, relógio, cursor do chart, tallies, câmera e contagem regressiva voltam ao início |

Medido (`tools/run_gameover_harness.sh`, 5.000 quadros, duas passadas): morte
no quadro 3.559 com **13 misses** (13 × 0,08 = 1,04 > 1,0 de vida), 120 quadros
em `ui_state 10`, 21 quads desenhados na tela 10, reinício no quadro **3.679**,
música começando **2 vezes** e **2 contagens regressivas** completas. O teste
falha se o jogador não morrer, se a tela não desenhar, ou se a música não voltar
a tocar depois do reinício.

**Fechado também: o roteiro de teste deixou de ser aleatório.**

`tools/harness_presses.py` lê o **próprio chart** e gera uma pressão por nota do
jogador, no quadro em que ela chega (`quadro = quadro_zero + amostra/800`), com
o `quadro_zero` tirado do log da primeira passada. Medido: 29 pressões, **29
acertos**, 3 notas deixadas para trás de propósito (os 3 misses), placar
**14.200**, conclusão **81 %** e rank **GREAT (2)** — que é exatamente
`(29 − 3) / 32 = 81,25 %`, o número que o próprio `fnf_play_completion`
devolve. Ou seja: o resultado da tela agora bate com a conta do original, e o
teste compara os dois.

**Fechado: pausa (`PauseSubState.hx`).**

No original a pausa é um **substate**: o PlayState continua atrás, congelado
(`shouldSubstatePause`), com um retângulo preto a 60 % por cima
(`bgColor 0x99000000`) e o menu. O port mantém `ui_state == 8` e desenha a
sobreposição por cima — mesma estrutura, sem duplicar a cena.

| Item | Original | Port |
|---|---|---|
| abrir | `controls.PAUSE_P` (START) | START em `ui_state 8` |
| congela | `shouldSubstatePause` + `persistentUpdate = false` | relógio da música, contagem, eventos e áudio param |
| fundo | alpha 0 → 0,6 em 0,8 s, `FlxEase.quartOut` | idem, `153 · quartOut(t)` |
| menu | Resume / Restart Song / Change Difficulty / Enable Practice Mode / Exit to Menu | as 4 primeiras menos **Practice Mode** (ver abaixo) |
| posição | `y = 70·i + 30`, `x = 0` em 1280×720 | metade disso em 640×360 |
| entrada | primeiro quadro ignora (`justOpened`) | `g_pause_frame >= 2` |
| metadados | nome da música, dificuldade, `N Blue Balls` | iguais, no canto superior direito |
| Change Difficulty | lista as dificuldades que a música tem + Back | `SR_DIFF_*` do registro + Back; escolher recarrega chart e cena |

**Por que "Enable Practice Mode" não está lá:** o Practice Mode do original é
um modo de jogo (sem morte, começando numa seção escolhida), não um item de
menu. Uma entrada que não faz o que promete é pior que a ausência dela; ela
entra quando o modo existir.

Medido (`tools/run_pause_harness.sh`): pausa aberta em 194.400 amostras,
**um único valor de relógio em todos os quadros pausados** (congelou de
verdade), 73 quads na sobreposição, Resume voltando em **194.400** (o mesmo
ponto) e a música terminando em **3.225.600** — exatamente o tamanho do arquivo
de áudio. Troca de dificuldade: 2 charts carregados e 3 contagens regressivas
completas.

**Três defeitos reais que a pausa desenterrou** (todos medidos, nenhum por
leitura):

1. **O fim da música dependia de o áudio ter tamanho múltiplo do bloco lido.**
   O produtor lê de 32 KB e o consumidor só aceitava blocos inteiros de 8 KB: o
   resto do arquivo ficava preso no anel, `audio_ring_eof()` era verdadeiro,
   o anel nunca esvaziava e a música **nunca terminava**. Depois de retomar da
   pausa sobravam **1.280 bytes** presos. Corrigido com
   `audio_ring_take_available()` (entrega o que houver) mais o caminho de cauda
   no mixer. Agora a música fecha em 3.225.600 de 3.225.600 amostras.
2. **O harness segurava SELECT o tempo todo** para ter o combo de saída
   (START+SELECT) disponível — o que fazia **qualquer START** ser um pedido de
   saída. O primeiro teste de pausa não pausou: fechou o jogo (morreu em
   `SleepThread` no quadro 1201). Agora o SELECT só entra nos dois quadros da
   sequência de saída.
3. **O roteiro por quadro não servia para apertar nota.** O relógio da música
   anda por tempo, e no harness o tempo virtual também anda dentro dos
   `DelayThread` do áudio: depois de mexer no envio de áudio a razão
   amostras/quadro mudou e o roteiro (calculado em quadros) errou **tudo** —
   0 acertos. O roteiro agora é em **amostras** (`PS2H_INPUT_AMOSTRAS`), com o
   jogo informando o relógio ao harness por um gancho fraco
   (`ps2h_song_clock`, nulo no console). Resultado: 29 de 29 acertos, e a pausa
   não precisa de compensação nenhuma — o que já é o teste do congelamento.

E um erro meu, encontrado pelo próprio teste no mesmo build: ao passar o bloco
variável ao `audio_play_complete` eu enviei `n` bytes de **entrada** (8 bytes
por amostra) em vez de `n/2` bytes de **saída** (4 bytes por amostra). O áudio
entregue ao audsrv dobrou e o relógio correu até 4.755.264 amostras num arquivo
de 3.225.600. Corrigido com o fator explícito e comentado no lugar.

**Fechado: Freeplay (`ui_state 11`).**

Até aqui 26 das 27 músicas eram **inalcançáveis**: o menu tinha a entrada
FREEPLAY, mas apertá-la mostrava "COMING IN NEXT STAGE". Agora:

| Item | Original | Port |
|---|---|---|
| entrada | item FREEPLAY do menu (`MainMenuState.hx`) | mesmo item, abre `ui_state 11` |
| lista | capsules numa grade, vindas do registro | 27 músicas do registro (`sr_levels()`), sem duplicar as que aparecem em mais de uma semana |
| navegação | quatro direções na grade | UP/DOWN na lista (sem a arte das capsules), 9 linhas visíveis de 27 |
| dificuldade | "bubble" da esquerda/direita | LEFT/RIGHT, **só entre as `SR_DIFF_*` que a música tem** |
| tocar | CROSS na capsule | CROSS fixa a música e a dificuldade e entra no PlayState com ela |
| voltar | CIRCLE sai para o menu | CIRCLE sai para o menu; e ao sair do **resultado/game over/pausa** o destino é a Freeplay, não o Story |
| log | — | `freeplay_open`, `freeplay_enter`, `freeplay_sel`, `freeplay_diff`, `freeplay_play`, `freeplay_draw`, `freeplay_back` |

Medido (`tools/run_freeplay_harness.sh`, 6.000 quadros): **27 músicas** na
lista, tela desenhada com **153 quads**, RIGHT levando a dificuldade para
**HARD**, o chart carregado com **48 notas** (o normal tem 32) e checksum
diferente, a música tocando e **506 quadros de volta na Freeplay** depois de
sair do resultado.

Também fechado de quebra: **um defeito de escalonamento no reinício**.
`audio_ring_reset()` depende de a thread produtora ver o pedido dentro de
500 ms; quando ela não é escalonada a tempo o reset devolve −1 e o reinício
ficava com o anel cheio e a música parada. Aconteceu no harness (game over com
"1 vez que a música começou" em vez de 2) e não acontecia sempre — é corrida.
Agora, se o reset falha, o transporte para e recomeça do zero
(`audio_ring_stop` + `audio_ring_start`), que é o que o Resume da pausa já
fazia, e o resultado vai para o log (`rebobinar`).

**Fase 3 fechada**, com uma ressalva de arte: a Freeplay desenha a lista e não
as capsules, e o menu de pausa não tem o ícone/círculo nem o `AtlasFont` do
original. A informação e o comportamento estão lá; a arte entra com o pacote.

## Regra de trabalho desta rodada

1. Uma fase por vez, terminada e verificada, na ordem acima.
2. Verificação mínima **antes** de publicar qualquer binário: `verificar_sdk.sh`
   (compila todo o `src/` contra os headers reais do ps2sdk), a suíte offline
   (`test/run_tests.sh`) e o harness de 1400 quadros. Isso não é "mais uma
   rodada de diagnóstico": é o portão que impede publicar uma build pior que a
   anterior (regra 5). Roda em minutos, aqui, sem o console.
3. Cada fase termina com um parágrafo no fim deste arquivo: o que ficou
   provado, o que é hipótese, o que continua faltando.
4. Nenhum evento de jogo removido, encurtado ou simplificado.

## Diário de fases

### Fase 1 — núcleo de gameplay: FECHADA

**O que entrou**

| Arquivo | O que é |
|---|---|
| `src/engine/gameplay.h` | contrato + todos os números do original, um por constante |
| `src/engine/gameplay.c` | julgamento, pontuação, vida, combo, conclusão, rank |
| `test/test_gameplay.c` | 60 asserções, cada uma citando o arquivo do original |
| `src/main.c` | o HUD e o caminho do tutorial passaram a usar o módulo |
| `test/ps2harness/ps2h.c` | relógio virtual do harness (ver abaixo) |
| `tools/gen_packs_vazios.py` | sintetiza os 4 pacotes `.lz4f` para compilar sem a arte |

**Ligação no jogo**

- `tutorial_notes_reset()` conta as notas `must_press` do chart e chama
  `fnf_play_reset()` com esse total — é o `tallies.totalNotes` do original.
- Acerto: `fnf_play_hit(erro_em_ms)`; o erro vem de
  `(amostra_da_nota - amostra_da_música)/48.0`, porque a 48 kHz uma amostra é
  1/48 ms, igual ao `noteDiff` do `goodNoteHit()`.
- Nota perdida: `fnf_play_miss()`. Aperto sem nota: `fnf_play_ghost()`
  (−0,08 de vida, −10 de placar, sem mexer no combo).
- HUD: a barra de vida era **um retângulo fixo até 320,0** (meio) e o placar era
  **o texto literal "Score: 0"**. Agora a divisão é `468,25 − 296,5·(vida/2)`
  (a conta do `FlxBar` RIGHT_TO_LEFT com largura 593/2) e o placar é
  `"Score: " + formatMoney(songScore,false,true)`, com vírgula de milhar.

**Provas medidas**

- suíte offline: `ALL SUITES PASSED`, incluindo a nova
  `== gameplay (julgamento, vida, combo, resultado) ==`;
- `verificar_sdk.sh`: 14 fontes contra os headers reais do ps2sdk;
- harness: 1401 quadros, 5097 pacotes, 0 estouros, 0 escritas fora do limite,
  todos os assets abriram;
- caminho de ghost exercitado de ponta a ponta no harness
  (`tutorial_note_ghost` no log).

**Defeito de teste encontrado e corrigido: o relógio do harness**

O shim devolvia `GetTimerSystemTime()` em microssegundos de relógio de parede.
O jogo converte com `dt*AUDIO_RATE/kBUSCLK`; com µs, isso dá 0,0003 amostra por
µs, e o relógio da música ficou **travado em 1 durante 800 quadros** (medido no
log). Consequência: nenhuma nota chegava perto do receptor no PC, e o caminho de
julgamento **nunca era executado pelo harness** — ele só cobria desenho.
Agora o harness tem relógio virtual em ticks de BUSCLK, avançando 1/60 s por
`graph_wait_vsync()`: 800 amostras por quadro, determinístico.

**O que continua faltando nesta fase**

O caminho de **acerto** ainda não foi exercitado de ponta a ponta no PC, porque
o áudio do tutorial *morre no harness* poucos quadros depois de começar
(`audio_playing` cai a 0 e o relógio da música congela). Isso é o primeiro item
da Fase 2.

### Fase 2 — PlayState genérico: FECHADA (falta só o mixer de duas faixas, que é Fase 4)

**Fechado: o harness agora executa o julgamento.** Medido antes e depois:

| | antes | depois |
|---|---|---|
| relógio da música no quadro 2200 | **1** (travado) | **1.939.200** amostras |
| acertos julgados numa execução | **0** | **9** |
| placar | 0 | 793 |
| código de gameplay executado no PC | só desenho | acerto, ghost, miss, combo, vida |

Foram três defeitos de harness, cada um medido:

1. `GetTimerSystemTime()` em µs (já corrigido na Fase 1, relógio virtual).
2. `audsrv_queued()` devolvia 0 e `audsrv_available()` 1 MB. Com `queued()==0`
   o jogo concluía "música acabou" 49 quadros depois de começar. Agora a fila
   é modelada como no console: 20.480 B, consumo de 192.000 B/s, `available()`
   = espaço livre. `DelayThread` também passou a avançar o tempo (só na thread
   que desenha), porque o `audio_play_complete()` espera com ele.
3. O `audio_stems.pcm` sintético tinha 1,36 s e o chart tem 67 s — o jogo
   *corretamente* encerrava a música. Agora o arquivo sintético dura o chart
   inteiro (25,8 MB, gerado, não versionado).

E dois defeitos do próprio roteiro de teste: o limite de 64 eventos de botão
descartava o excedente **em silêncio**, e a cadência periódica criava aliasing
com o espaçamento das notas (passo 12 → erro preso em 12 quadros; passo 7 →
erro preso em 10 quadros, 2 fora da janela). A cadência agora é
pseudoaleatória determinística.

**Fechado: um bloco curto de áudio não mata mais a música.**
`tutorial_audio_mix_submit()` devolvendo algo diferente de `AUDIO_READ_CHUNK`
marcava `g_tutorial_audio_status = -6`, parava o `audsrv` e congelava o relógio
**para o resto da tentativa, sem retentativa**. Um bloco curto é o que acontece
quando a fila do audsrv enche no meio de um envio — normal no console, onde o
pendrive entrega a ~1,5 MB/s e o anel tem 20 KB. Agora: bloco curto conta
(`tutorial_audio_curto` no log, limitado a 8 eventos para não inundar), o
transporte segue, e ele só para depois de **120 quadros seguidos (2 s)** sem
conseguir enviar nada.

Isso é o que a regra 8 pede: **uma** causa, medida, com o número na mão. Não
afirmo que era a causa do travamento do console — afirmo que era um defeito
real, que o sintoma descrito cabe nele, e que agora ele não pode mais acontecer.

**Fechado parcialmente: `tutorial_audio_end` com o transporte vivo**

Antes a música do tutorial terminava no quadro 817 de 1500. Agora ela avança até
o fim da janela de teste; o fim de música continua sendo reconhecido quando o
arquivo realmente acaba.

**Fechado: o PlayState deixou de ser "o Tutorial".**

Antes: o estado de jogo tinha `sr_song_by_id("tutorial")` escrito no código, um
único arquivo de áudio (`assets/audio_stems.pcm`) e o passo do chart fixo em
`TUT_SAMPLES_PER_STEP = 7200` — que é o valor do Tutorial a **100 BPM**.
Medido: a 182 BPM (2hot) o passo é **3.956 amostras**; com 7.200 o bop e os
eventos de palco sairiam do tempo.

Agora:

| Item | Antes | Agora |
|---|---|---|
| música | `"tutorial"` no código | `sr_level(story_level)->songs[0]`, com queda para o Tutorial se a semana não existir |
| passo do chart | 7.200 fixo | `chart_samples_per_step(amostra)`, a seção de BPM vigente do próprio chart (`chart.c`) |
| áudio | `assets/audio_stems.pcm` fixo | `songs/<id>/stems.pcm` via `sr_stems_path()`; o arquivo plano fica só como reserva **do Tutorial** |
| log | nada dizia qual música tentou | `play_song` (hash do id), `play_step`, `play_audio_path`, `play_audio_ausente` |

A reserva vale só para o Tutorial de propósito: usar o stems do Tutorial para
tocar Bopeebo seria **tocar a música errada com o chart certo**, que é pior que
admitir que falta o áudio. Uma música sem áudio marca `play_audio_ausente` e o
HUD mostra o erro de asset.

Por que `songs/<id>/stems.pcm` e não o `Inst.pcm` do original: no original são
**duas** faixas (instrumental + vozes) com volume independente; o port ainda tem
um fluxo único com as duas já misturadas (8 bytes por amostra). Abrir `Inst.pcm`
com o mixer atual leria 8 bytes de um arquivo de 4 e tocaria na metade da
velocidade. O caminho fica no registro (`sr_stems_path`), documentado, e
desaparece quando o mixer de duas faixas existir.

Provado no harness: `play_audio_path` abriu o arquivo por música (fd 6),
`play_step` = 7.200 com `initial_bpm` 100, 9 acertos e 5 saltos de placar.

**Fechado: fim de música → tela de Resultado (`ui_state == 9`).**

Antes o PlayState não tinha fim: a música acabava e o jogo ficava no quadro
parado. Agora:

| Item | Antes | Agora |
|---|---|---|
| fim da música | nada acontecia | `audio_ring_eof()` sem fila pendente → 90 quadros de espera → transição para `ui_state 9` |
| relógio final | constante `TUTORIAL_AUDIO_SAMPLES` | `g_chart_info.song_end_sample`, com a constante só como reserva do Tutorial |
| resultado | não existia tela | rank (`fnf_play_rank`), placar, cinco tallies, maior combo e `%` de conclusão (`fnf_play_clear_percent`), das **mesmas** funções que julgaram as notas |
| log | nada | `play_result` (rank, %), `play_result_score` (placar, combo), `play_result_tallies`, `result_enter`, `result_draw` |
| saída | — | CROSS volta ao Story Menu |

Um defeito **encontrado pelo teste**, não por leitura: `tutorial_notes_reset()`
era chamado **antes** do `chart_load`, então `fnf_play_reset()` recebia
`total_notes = 0`. Com total zero, `fnf_play_completion()` devolve 0 e
`fnf_play_rank()` devolve `FNF_RANK_NONE`. O resultado sairia sempre com rank
**NONE** e **0 %** em qualquer música, por melhor que fosse a execução — e no
console isso apareceria como "o jogo não conta as notas". A ordem foi corrigida
(reset depois do `chart_events_bind`) e o log ganhou `play_total_notes`.

| Medição (harness, 5.600 quadros, Tutorial normal) | Antes | Depois |
|---|---|---|
| `play_total_notes` | 0 | **32** |
| rank no resultado | `-1` (NONE) | **0** (SHIT, coerente: 23 misses) |
| quadros em `ui_state 9` | 0 | **689** |
| quads desenhados na tela 9 | 0 | **95** |

`PLAY_RESULT` no harness agora falha se o rank vier `-1` (total de notas zero) e
se a tela não desenhar nenhum quad — "o estado existe" não é prova de que
apareceu alguma coisa.

**Fechado: contagem regressiva (5 passos, `Countdown.hx`).**

O original não deixa a música começar no quadro em que o PlayState aparece:
`Countdown.performCountdown()` põe o `Conductor` em **-5 batidas**, cria um
`FlxTimer` de uma batida com 5 voltas e só quando o `songPosition` chega em 0 é
que `PlayState.startSong()` toca o instrumental (`PlayState.hx:1144-1151`). Nos
cinco passos aparece 3, 2, 1, GO! com um tween de alpha 1→0 em
`FlxEase.cubeInOut` durante uma batida (`Countdown.showCountdownGraphic`).

O port agora faz o mesmo:

| Item | Antes | Agora |
|---|---|---|
| início da música | no primeiro quadro do PlayState | depois de 5 batidas (`countdown_begin`) |
| batida | — | 4 passos do chart (28.800 amostras a 100 BPM) |
| passos | — | 0 BEFORE, 1 "3", 2 "2", 3 "1", 4 "GO!", 5 AFTER |
| gráfico | — | texto no centro, alpha 1→0 em `cubeInOut` por uma batida |
| relógio do chart | só ≥ 0 | **negativo** durante a contagem (é o que o original faz), então as notas do começo já descem |
| log | — | `countdown_begin`, `countdown_step`, `countdown_sound_ausente` |

Duas coisas ditas sem enfeite: (a) a **arte** do countdown vem do `noteStyle`
do original e ainda não está no pacote, então o desenho é texto — o tempo e a
curva são os do original; (b) o **som** de cada passo também é do `noteStyle` e
também não está no pacote, e é por isso que existe `countdown_sound_ausente` no
log: o passo que ficou sem som fica registrado em vez de ser omitido em
silêncio.

Medido no harness (5.800 quadros): `countdown_begin` com batida 28.800,
cinco `countdown_step`, `tutorial_audio_start` **no quadro 957**, depois do
passo AFTER no quadro 956. O teste falha se a música começar antes do fim da
contagem, se algum passo faltar ou se a batida sair zero.

**O que falta na Fase 2**

**Primeiro item, e ele é uma hipótese forte para o travamento do console**

`tutorial_audio_mix_submit()` devolve menos que `AUDIO_READ_CHUNK` → o jogo
marca `g_tutorial_audio_status = -6` **para sempre**, faz `audsrv_stop_audio()`,
põe `audio_playing = 0` e o relógio da música congela. Um único bloco curto
mata o áudio e a música da tentativa inteira, sem retentativa.

Por que isso casa com o sintoma do console ("começa a música, fica muito lento
e engasgado, e trava"): o pendrive do PS2 é USB 1.1, a ~1,5 MB/s, com blocos de
8 KB; um único produtor atrasado deixa o anel vazio. Não é prova — é
**hipótese**, e o teste que a distingue é barato: gravar quantas vezes
`tutorial_audio_mix_submit` devolve bloco curto e em que quadro, com o jogo
continuando em vez de parar. Entra na próxima build, sozinho (regra 8).

A Fase 2 fecha com este item. Escolher música e dificuldade, o áudio por
música, a contagem regressiva e o fim de música → resultado estão fechados
acima. O que continua pendente na camada de áudio, e é Fase 4/5, é o **mixer de
duas faixas** (instrumental + vozes separados, como o `Inst`/`Voices` do
original): hoje o port toca um único `stems.pcm` já misturado.
