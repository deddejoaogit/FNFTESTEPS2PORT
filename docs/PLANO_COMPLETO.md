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
4. ~~Estado 8 desenha o palco da música (não o `mainStage` fixo)~~ — **FEITO
   para o `mainStage`** (abaixo). O palco vem do id da música pelo
   `sr_level(n)->songs[0]->stage` quando os outros palcos tiverem pacote no
   pendrive; hoje só o `mainStage` tem, e o resto cai na reserva. Falta também
   os
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

#### Fechado: o palco genérico desenhando (passo 4, primeiro palco)

O tutorial desenhava o palco como **uma** textura composta (512×256) feita fora
do jogo. Agora o `main.c` desenha os **props** do `mainStage`, um a um:

```
pacote CFZ4 do prop  (assets/stages/mainStage/<prop>.cfz4)
   -> stage_layout: onde cai, em que quadro, em que ordem de z
   -> stage_pool:   endereço no pool (despeja o que não é fixo, se faltar)
   -> upload_raw_psm 16 bpp
   -> sprite_dims
```

E a VRAM passou a ser **do estado**, não do bloco: entrando no jogo o
`palco_ui_entrou()` **solta** `gftex` e `stagetex` (o palco pode despejá-las);
saindo, limpa os props e volta a fixá-las, com endereço novo se foram despejadas.
Sem esse caminho de volta uma textura de menu ficaria apontando para VRAM que
agora é de um prop — e o sintoma seria arte trocada na tela sem log nenhum.

Medido no harness de gameplay (5.600 quadros, Tutorial normal):

```
pool_cena,8,1269760        entrou no jogo: gftex/stagetex deixam de ser fixas
palco_entra,2,3            palco mainStage (índice 2), 3 props
palco_draw,3,0             3 props desenhados, 0 falhas
palco_pool,358656,3        358.656 B livres no pool, 3 uploads
pool_cena,9,1269760        saiu para o resultado: props soltos, telas refixadas
```

O `run_gameplay_harness.sh` passou a **falhar** se nenhum prop for desenhado
(cair na textura composta é reserva aceitável, não resultado de teste) ou se
houver qualquer `palco_falha`/`palco_sem_vram`.

**Reserva, de propósito:** se um pacote não abrir, ou não couber na VRAM, o
estado 8 volta para a textura composta do tutorial — nunca tela vazia. O evento
`palco_falha` diz qual prop e por quê. É por isso que `stagetex` continua
existindo.

**O que ainda não está provado:** que o desenho *pareça* o palco. O harness não
tem rasterizador: ele prova que os 3 props saíram, no quadro certo, com a
geometria do `stage_layout` e sem falha de VRAM. Comparar com a captura do
console (`evidencia-console/`) é o teste de aceitação que falta.

O palco já **não é fixo no código**: o estado 8 pede
`g_play_song->stage`, o id do registro da música. Hoje só o `mainStage` tem
pacote no pendrive, então os outros 20 caem na reserva e o log diz qual palco
foi tentado.

**O que falta aqui:** os outros 20 palcos, que dependem do reempacotamento com a
nova escala (orçamento de VRAM/RAM acima) — e, para o
`tankmanBattlefield` (1.700.352 B), do pool ter os 2 MB inteiros, o que já
acontece a partir desta build porque `gftex`/`stagetex` são soltas durante a
música.

**Primeiro teste do renderizador novo é o próprio tutorial**: o `mainStage` tem
3 props e 1.697.032 B, cabe no pool, e o resultado tem de ser o mesmo que a
textura composta que está lá hoje. É o caminho mais barato para achar erro de
posição, porque existe captura de tela do palco atual no console
(`evidencia-console/`) para comparar.
5. Os 46 personagens: `bf_meta_t` (`MAX_ANIMS 8`) vira tabela gerada por
   personagem, com `renderType` (`multianimateatlas` para boa parte deles) — é
   o item mais caro e o único que ainda não tem formato de pacote definido.

#### Fechado: os 46 personagens (passo 5, os dados e a conta)

Antes de desenhar qualquer personagem, o port precisava saber **qual** personagem,
com **que** animação, **onde**. Isso virou três peças, todas medidas:

| peça | o que é | o que prova |
|---|---|---|
| `tools/gen_char_dados.py` → `src/engine/char_dados.h` | os 46 personagens e **523 animações** lidos de `assets-data/characters` (o mesmo `CharacterData` do v0.8.6): `render` (tipo de desenho), `flipx`, `offsets`, `cameraOffsets`, `singTime` e a faixa de animações de cada um | o C é a renderização do JSON, e `--check` roda na suíte |
| `tools/gen_stage_dados.py` (estendido) → `bs_slots[]` em `src/stage_dados.h` | **63 slots** = 21 palcos × bf/gf/dad, com posição, `cameraOffsets` e `zIndex` | todo palco tem os três papéis (o teste falha se faltar um) |
| `src/engine/char_layout.{h,c}` | `ch_find`, `ch_anim_idle`, `ch_anim_sing`, `ch_anchor`, `ch_slot` — C puro, sem GS, sem arquivo | roda no PC contra os 46 reais |

A conta de posição/câmera **não foi deduzida**; ela é a que o estado 8 do
`main.c` já faz à mão para dois personagens e que a captura do console aprovou.
Uma coisa que ela provou durante o teste: o foco de câmera **soma também o
`cameraOffsets` do personagem**, não só o do slot. Com o `mainStage` e o BF:
`989,5 + 17 − 18 − 100 = 888,50` e `885 + 14 − 246 + 36 − 100 = 589,00` — os dois
números que o console já mostrou. O `BaseCharacter.hx` L350-355
(`resetCameraFocusPoint`) confirma a origem da parcela.

O que o teste (`test/test_char_layout.c`, dentro do `run_tests.sh`) exige:

- **46 contra 46**: para cada personagem, `offsets`, `cameraOffsets` e `singTime`
  do `char_dados.h` têm de ser iguais aos do `bs_dados.h`. São dois geradores
  independentes sobre a mesma fonte, então um gerador velho de um lado é pego;
- **todos têm o que desenhar**: ≥ 1 animação, tipo de desenho conhecido, faixa
  de animações dentro da tabela;
- **idle e canto**: 46 têm idle, 32 cantam, **14 não cantam** (as GFs e as de
  fundo usam `danceLeft`/`danceRight`); quem não canta devolve `-1` e um canto
  de direção inválida também;
- **tipo de desenho contado**: 24 `animateatlas`, 10 `multianimateatlas`,
  3 `multisparrow`, 8 `sparrow`, 1 `packer`. Os 8 `sparrow` e não 1 porque 7
  JSON não têm `renderType` e caem no `DEFAULT_RENDERTYPE` do
  `CharacterData.hx` (L463, L510-512) — o gerador **não inventa**, usa o padrão
  do original;
- **posições finitas**: 46 × 21 × 3 = **2.898** combinações de personagem × slot,
  todas com coordenada finita. Coordenada `NaN` travava o GS, e no PC isso não
  apareceria.

**O que isto ainda NÃO prova:** que o personagem pareça certo na tela. Isso é o
próximo passo (o desenho), e só a tela responde.

#### Fechado: Story Mode joga a SEMANA inteira (a campanha)

O que estava errado, medido no código: o Story Menu só chamava o PlayState com
`story_level==0` — **as semanas 1 a 8 eram inalcançáveis** — e mesmo na semana do
Tutorial o `g_play_song_index` era sempre 0, então **cada semana tocava uma
música só**. Somando: 20 das 27 músicas existiam e nenhum caminho do jogo
chegava nelas (a Freeplay só toca a primeira da lista).

O original (`PlayStatePlaylist.hx`, `StoryMenuState.hx` L571-598, `PlayState.hx`
L3560-3660 e L3885) faz uma **playlist** da semana: toca a primeira, e a cada fim
de música faz `shift()` e carrega a próxima **sem** passar pelo resultado — o
resultado só aparece quando a lista esvazia, mostrando o placar **acumulado da
campanha**. Foi isso que entrou:

| peça | onde |
|---|---|
| playlist (`g_playlist_n`, `g_playlist_i`), campanha (`g_story_mode`, `g_story_week`) e placar acumulado (`g_campaign_score`) | `src/main.c` |
| CROSS no Story Menu monta a playlist de **qualquer** semana | `ui_state==7` |
| fim de música: `story_next` (próxima da semana) ou resultado (fim da lista) | bloco de fim de música |
| resultado mostra o placar da campanha quando é Story Mode | `ui_state==9` |

Um detalhe que só apareceu ao testar: `g_play_level` era derivado de
`story_level` (a semana que o **menu** está mostrando), então a campanha da
semana 1 tocava o Tutorial. Agora a semana vem de `g_story_week`, fixada no
início da campanha.

**O teste** (`tools/run_story_harness.sh`, dentro da suíte, semanas 1, 8 e 9):

- o roteiro de teclas é calculado do chart de **cada** música e leva o índice da
  música (`harness_presses.py --onda K`). Isso foi obrigatório: o relógio da
  música volta a zero em cada uma, e sem o índice o roteiro da segunda música
  aperta nas horas da primeira — vira ghost miss e mata o jogador;
- o harness conta "música nova" pelo relógio voltar ao começo (não pelo jogo
  avisar nada) e rearma o roteiro daquela música;
- cobra: todas as músicas da semana tocadas, `N-1` emendas, **uma** tela de
  resultado (no fim), músicas **distintas** e volta ao Story Menu.

**Medido:** semana 1 (3 músicas) → 3 tocadas, 2 emendas, 1 resultado; semana 8
(`weekend1`, **4** músicas) → 4 tocadas, 3 emendas, 1 resultado; semana 9 (1
música) → 1 tocada, 0 emendas, 1 resultado. Todas voltam ao Story Menu.

Dois bugs de teste que isso desenterrou, e que valem registro porque eram
**silenciosos**: o roteiro de botões do harness era cortado em 512 eventos (uma
semana de 4 músicas tem mais de mil) — o texto era truncado sem aviso e a música
3 em diante ficava sem aperto nenhum; e o `PS2H_INPUT` padrão do harness aperta
CIRCLE/CROSS no meio da música, o que vira ghost miss. Os dois agora **morrem
dizendo o que houve** em vez de truncar.

#### Fechado: as 81 combinacoes (27 musicas x 3 dificuldades) tocam no harness

Medido antes: o harness provava **uma** música (o Tutorial) e a Freeplay do port
toca a primeira da lista. Resultado: **26 das 27 músicas e 54 dos 81 charts
nunca tinham sido carregadas pelo jogo**. Um caminho de chart errado, um BPM
exótico ou um palco que não desenha só apareceria no console, com o jogo parado.

O que entrou para fechar isso:

| peça | o que faz |
|---|---|
| gancho `PS2H` no `main.c` | compilado **só** no harness (`-DPS2H=1`), começa direto numa música. No console o define não existe e nada disso entra no binário |
| `FNF_START_SONG=<id>` / `FNF_START_DIFF=<0..2>` | escolhem música e dificuldade para o teste |
| `tools/preparar_sweep.py` | para cada música: o chart FCH3 (o que o console lê) e um `stems.pcm` de 20 s — o áudio tem de **abrir** para o relógio da música andar. Não finge a duração inteira: custaria 100 MB por música e não provaria nada a mais |
| `tools/run_sweep_harness.sh` | roda as 81 combinações e cobra, em cada uma: `harness_start`, `chart_layout>=0`, `tutorial_chart` status 0 com checksum ≠ 0, `play_total_notes>0`, `countdown_begin` + 4 passos, tela de jogo desenhada, **prop de palco desenhado**, áudio começado e `RESULTADO: PASSOU` |
| `gen_harness_assets.py` | passou a gerar pacote dos **21** palcos (eram 2). Com 2, 60 das 81 combinações morriam em "arquivo não abriu" |
| lista de fontes do harness | agora vem do `EE_OBJS` do Makefile. O console não liga nesta máquina (não há ps2sdk instalado), então **este link é a única prova** de que o `EE_OBJS` está completo: se faltar um objeto lá, falta aqui e o harness não liga |

O sweep roda **duas vezes**: com o binário normal e com o dos sanitizadores
(`PS2H_BIN=build/fnf_harness_asan`), então os 81 charts e os 12 palcos passam
com ASan + UBSan ligados — é onde um estouro de buffer ao ler um chart do week7
aparece como erro, e não como arte estranha na tela. O harness com sanitizadores
estava sendo **pulado em silêncio** desde que a lista de fontes mudou (uma
variável indefinida com `set -u`); agora ele compila e roda: 1 401 quadros do
jogo inteiro, **zero** erro de memória, comportamento indefinido ou vazamento.

**Medido:** `sweep: 81/81 combinações música × dificuldade começaram`,
`12/12 palcos usados pelas músicas desenharam`, cada música com as suas notas
(ex.: `2hot/hard` 585 notas, `bopeebo/easy` 44, `winter-horrorland/hard` 224) e
com os seus eventos de chart. A suíte inteira (`test/run_tests.sh`) leva ~2 min.

**O bug que o sweep encontrou, e ele é de fidelidade:** nos palcos de pixel
(`school`, `schoolEvil`, ...) o JSON do palco traz `"scale": [6, 6]` em cada
prop, e o `Stage.hx` (L234-240) faz `propSprite.scale.set(value)`. O
`sl_collect()` **ignorava** esse campo: o tamanho na tela era
`src_w * 0.5 * zoom`, sem o 6. Resultado medido no sweep: `roses`, `senpai` e
`thorns` entravam no estado 8, carregavam o chart, tocavam — e **nenhum prop de
palco era desenhado**, sem nada no log dizendo por quê. Agora o tamanho é
`src_w * sx * 0,5 * zoom * bop`, e o teste de `stage_layout` exige o `scale`
por prop (6 em `school`, 1,1 e 0,9 no `mainStage`).

Ou seja: o palco do tutorial, que já estava aprovado no hardware, passa a ser
desenhado **10 % maior/11 % menor** nos props cujo JSON diz 1,1/0,9 — é a
correção do original, não um ajuste novo.

#### Fechado: um decodificador de pacote so', conferido contra o empacotador

Antes de escrever a quarta copia do decodificador LZ4 (a dos personagens), as
três cópias que estavam dentro do `main.c` foram para
`src/engine/lz4_bloco.{h,c}` — C puro, com teste. E o teste não compara o
decodificador com ele mesmo: `tools/lz4py.py` é o lado que **comprime** (é o que
o empacotador usa), e `tools/gen_lz4_vetores.py` grava em `test/lz4_vetores.h`
nove fluxos reais comprimidos por ele — vazio, 1 byte, literais longos com
extensão de 255 em 255, **match sobreposto** (offset menor que o match, que é o
que comprime cor chapada), período de 4 KB, aleatório e um quadro de 8 KB.
`test/test_lz4.c` decodifica cada um e exige igualdade byte a byte. É a única
forma de pegar os dois lados concordando sobre o **formato errado**.

A segunda metade do teste é o caminho do erro: saída maior ou menor que o fluxo
produz, extensão truncada, offset 0, offset antes do início, match maior que o
espaço restante, ponteiro nulo. O decodificador tem de **recusar**; um quadro
meio decodificado chega ao GS como arte rasgada, não como erro.

Duas coisas apareceram ao fazer isso, e são o tipo de coisa que só aparece
quando se procura:

1. **`vram_cache.o` não estava no `EE_OBJS`** e `stage_pool.c` chama `vc_*`. O
   link do console teria morrido; a checagem antiga só olhava o que o `main.c`
   chama direto, e o harness liga os `.c` por nome, então passava. A checagem
   agora faz **fechamento transitivo**: quem entra no jogo arrasta quem chama.
2. A lista de fontes de engine dos dois `run_harness*.sh` estava **escrita à
   mão**. Ao entrar `char_layout.c`/`char_anim.c`/`lz4_bloco.c`, o harness parou
   de compilar e o teste que "prova o jogo" nem rodava. Agora ela é derivada do
   `EE_OBJS` do Makefile — uma fonte de verdade só.

#### Fechado: qual quadro cada personagem mostra (`char_anim.c`)

Com os 46 personagens e os 63 slots resolvidos (passo 5), faltava a segunda
metade da conta: **em que quadro** cada animação está. `src/engine/char_anim.{h,c}`
+ `test/test_char_anim.c`. As regras vêm do original, não do gosto:

| regra | de onde vem |
|---|---|
| quadro = `floor(tempo × frameRate)`, com módulo quando é loop e preso no último quando não é | `FlxAnimation`/`FlxAnimationController` |
| quem tem `idle` toca `idle`; quem não tem alterna `danceLeft`/`danceRight` pela batida | `Bopper.dance()` + `IdleCharacter` do v0.8.6 |
| canto dura `singTime` passos e depois volta a dançar | `PlayState` chamando `dance()` no fim do tempo de canto |
| erro de nota usa `singLEFTmiss` e, se não existir, cai no nome-base | troca de sufixo do `BaseCharacter` |
| o mesmo canto pedido de novo **não** reinicia a animação | senão a perna da GF reiniciaria a cada batida |

O teste cobre 24 fps e 29,97 fps (2997 centésimos), loop e não-loop, tempo
negativo (o relógio do chart é negativo durante a contagem), taxa ausente (não
divide por zero: mostra o primeiro quadro), `passos → ms` a 100 e 150 BPM, o
ciclo canto → `singTime` → idle no BF, a alternância `danceLeft`/`danceRight` na
GF (que **canta**, mas não tem `idle`), um personagem que **não canta**
(`darnell-blazin`) não inventando animação, e os **46** atravessando o ciclo com
índice de animação sempre dentro da faixa.

**A peça que falta, e é dado, não código:** a taxa de quadro, o `looped` e a
lista de quadros de cada animação **não estão** no `character.json` — eles vivem
no atlas de cada personagem (`AnimationData`), que só o clone da arte tem. Por
isso `ch_seq_t` é preenchido pelo **pacote**, e o pacote é construído pelo
empacotador lendo o atlas. Sem o clone, este módulo continua correto e testado;
o que falta é o dado, e dado faltando não vira número inventado (regra 4).

#### Fechado: o pacote de UM personagem (`char_pack.c`), passo 6 do palco/personagem

O palco já era genérico: `stage_pool.c` lê CFZ4, `stage_layout.c` decide o que
aparece, e o sweep prova os **12** palcos desenhando. Personagem não era — o
jogo conhece **dois** pacotes escritos no código (`bf_frames.lz4f`,
`gf_frames.lz4f`) e escolhe o quadro por duas tabelas fixas. São 46 personagens.

O que faltava era o **arquivo**: onde guardar, por personagem, os quadros
comprimidos **e** a tabela que diz quantos quadros cada animação tem, a que taxa
e se volta ao começo. Duas medições mandaram no desenho:

1. **A tabela não pode ficar no `char_dados.h`.** `assets-data/characters` tem o
   nome e o prefixo de cada animação e, em algumas, `frameIndices`/`looped`; a
   taxa e a contagem de quadros vivem no atlas (`AnimationData`), que só o clone
   da arte tem. Inventar isso no gerador seria divergência (regra 4).
2. **Não dá para reusar o CFZ4 do palco.** O CFZ4 tem um tamanho de quadro para
   o arquivo inteiro e nenhuma tabela de animação — o palco tira isso do
   `stage_dados.h`, gerado de JSON que **tem** a taxa e o loop de cada prop. Para
   personagem esse dado não existe fora da arte, então ele viaja no pacote.

Daí o **CHP4** (`src/engine/char_pack.h`, 36 bytes de cabeçalho): canvas, psm,
quantos quadros, tabela de quadros, **tabela de animações** (`nome[24]`,
`primeiro`, `quantos`, `fps*100`, `looped`) e os blocos LZ4 — o mesmo
decodificador `lz4_bloco.c` que o resto do jogo usa.

Duas coisas que só apareceram ao gerar os 46 de verdade:

- **`nome[16]` não cabia.** O maior nome do original tem **23** letras
  (`singRIGHTmiss-beautiful`, `singLEFTmiss-beautiful`). O gerador falhou no
  primeiro pacote e o campo virou `nome[24]` (entrada de 40 bytes). Nome cortado
  em silêncio seria animação perdida sem ninguém ver.
- **`cp_conferir()` é obrigatório no carregamento**, não só no teste: nenhuma
  animação pode apontar para um quadro que o pacote não tem. É barato (46
  personagens × ~20 animações) e impede o quadro errado aparecer no meio de uma
  música — arte rasgada é o pior defeito possível de diagnosticar na tela.

**Medido:**
`test/test_char_pack.c` (dentro de `test/run_tests.sh`) monta pacotes no próprio
teste, com blocos LZ4 "só literais", e confere: o pacote bom abre, o quadro sai
**byte a byte** igual ao empacotado nos 4 quadros, PSMCT16 com 2 bytes por
pixel, a animação vira `ch_seq_t`, e **12** arquivos errados são recusados cada
um com o seu erro nomeado (magic, versão, cabeçalho curto, tabela, número de
animações, canvas, psm, tamanho que não bate com o canvas, bloco fora do
arquivo, LZ4 corrompido, `quantos = 0`, animação passando do fim). Um bloco
corrompido **não contamina** o resto do pacote.

`tools/chp4.py` é o escritor e o leitor em Python — o mesmo formato, para o
empacotador e para o gerador de teste não terem duas cópias. Com ele
`tools/gen_harness_assets.py` passou a produzir **46** pacotes
(`assets/chars/<id>.chp4`, canvas 64×64, quadros sintéticos, contagem 3 ou o
`frameIndices` quando a JSON já traz) e faz `chp4.conferir()` antes de escrever:
a conferência do Python e a do C existem para discordar uma da outra se alguém
mudar o formato de um lado só.

**O que falta para os 46 aparecerem na tela:** o estado 8 ainda desenha BF e GF
pelos pacotes do tutorial. O próximo passo é pendurar `cp_pack_t` no lugar do
`tutorial_pack_frame`, com um pool de texturas de personagem (o `vc_*` já
existe) — o caminho é o mesmo que o palco genérico já faz, e o pacote já está
pronto e conferido.

#### Achado: nome de animação repetido — vale a **última**, como no original

Saiu da conferência dos 46 pacotes (`tools/check_char_packs.py`, agora dentro da
suíte): o personagem **"Boyfriend (Holding GF)" tem DUAS animações chamadas
`singUP`** — a primeira é "BF Dead with GF Loop", a segunda é "BF NOTE UP0". É
dado real da arte, não erro de geração.

O que o original faz: `FlxAnimationUtil.addAtlasAnimations` chama
`animation.addByPrefix(anim.name, ...)` para cada animação, na ordem da JSON, e o
`FlxAnimationController` guarda tudo num `Map` por nome — então **a segunda
sobrescreve a primeira** e é ela que toca. O port devolvia a **primeira** (parava
no primeiro nome igual). Era uma divergência silenciosa que poria em cena o
quadro errado **só nesse personagem** — o tipo de defeito que passa por todos os
testes e aparece uma vez na tela.

Corrigido nos dois lugares onde há busca por nome (`cp_anim_i` no pacote e
`ch_anim_i` no `char_dados`), com o motivo escrito ao lado, e com teste que usa o
caso real: pega o índice da primeira e da última ocorrência, exige duas
distintas, e exige que a busca devolva a **última** — e que o prefixo dela seja o
`BF NOTE UP0`.

**Medido nesta rodada (os 46 pacotes gerados):** 46 arquivos, **2.516 quadros**,
**523 animações**, 872.984 B; cada pacote abre, toda animação cabe nos quadros
que ele tem, **todo** quadro descomprime no tamanho exato, e os nomes batem com
`assets-data/characters` (é o item que pega o erro mais provável de todos: um
personagem empacotado com o atlas de outro).

#### Fechado: quem entra em cena (`ch_cena_plano`), nas 27 músicas

O desenho precisa saber três coisas antes de desenhar qualquer personagem: qual
personagem, em qual **slot** do palco, e qual é o **papel** dele (quem canta as
notas do oponente). Parece dedutível do id do personagem — não é.

O original escolhe o slot pelo **papel**, não pelo personagem: `PlayState.hx`
L2200-2225 chama `currentStage.addCharacter(x, GF|BF|DAD)`, e `Stage.hx`
L400-490 posiciona pelo tipo. Daí uma consequência que só aparece olhando o
dado: **no Tutorial o oponente é a própria GF** (`tutorial-metadata.json` →
`characters.opponent = "gf"`, e não existe `girlfriend`). Ou seja, ela entra pelo
slot `dad` — e é ela que canta as notas do oponente. O port já fazia a GF cantar
essas notas (correto), mas por um caminho escrito à mão; agora o papel sai do
dado.

**Medido** (teste dentro da suíte, percorrendo as 27 músicas pelas semanas):
26 têm namorada, **1 não** (o Tutorial — o slot fica vazio, não se inventa
personagem para tampar buraco), todas as 27 têm jogador e oponente, e o oponente
é a GF em **exatamente 1** música. Nesse caso o teste exige que o slot da
namorada fique **vazio**: senão o mesmo personagem seria desenhado duas vezes,
uma delas na posição errada.

**Divergência aberta, e ela precisa do console:** o estado 8 do `main.c` desenha
a GF do Tutorial no slot `gf` (x = 751,5 — número que a captura do console
confirmou). Pela regra do original, sendo ela o **oponente**, o slot seria o
`dad`. As duas coisas não podem estar certas ao mesmo tempo. Não vou trocar o
número aprovado no hardware por dedução de código (regra 4/5): fica registrado
como divergência a medir, e é barato — uma captura do Tutorial no console
mostrando de que lado a GF está resolve. O `ch_cena_plano()` já devolve o papel,
então quando o desenho genérico entrar, a decisão fica em um lugar só.

#### Fechado: um personagem em cena (`char_cena.c`) — a ligação que faltava

`char_pack.c` lê os quadros, `char_anim.c` sabe qual quadro de uma sequência
mostrar. Faltava a máquina de estado entre os dois, e ela tem uma decisão que
vale dinheiro no PS2: **quando reenviar a textura**. O custo do port não é
decidir o quadro, é decodificar LZ4 e subir pixels para a VRAM — então
`cc_quadro()` devolve o quadro novo **só quando ele mudou** (ou -1), em vez de um
número por quadro para o chamador comparar.

As regras, todas do original: dança em `idle`, ou alterna `danceLeft`/`danceRight`
pela batida quando não tem `idle` (`Bopper.dance` + `IdleCharacter`); canta por
`singTime` e volta (o mesmo canto pedido de novo **não reinicia**, mas
**estende** o tempo — senão a perna da GF reiniciaria a cada nota escrita no
mesmo instante); erro de nota usa `...miss` e cai no nome-base quando não existe.

Dois comportamentos que os testes acharam, e que teriam virado defeito na tela:

1. **Quem não tem `idle` congelava no primeiro lado.** A troca
   `danceLeft`/`danceRight` só acontecia quando o canto acabava — então um
   personagem de fundo (que nunca canta) ficaria parado no lado 0 a música
   inteira. Agora a troca é conferida a cada quadro, e para quem tem `idle` a
   conta devolve sempre o mesmo valor.
2. **Papel sem pacote não desenha nada** — e isso é o comportamento certo: arte
   não se inventa (regra 4). Um pacote que não confere deixa o papel inativo, em
   vez de desenhar lixo.

**Medido** (`test/test_char_cena.c`, dentro da suíte): papel sem pacote devolve
-1 sempre; pacote com a tabela estragada ou com magic errado deixa o papel
inativo; o idle troca de quadro a 24 fps (41,7 ms) e volta no loop em 167 ms;
o canto segura o quadro e termina no `singTime` (200 ms) voltando a dançar; o
mesmo canto estende de 300 para 360 ms sem reiniciar; `miss` cai no nome-base na
GF; `danceLeft`/`danceRight` alterna pela batida; personagem sem canto não
inventa animação. Suíte inteira depois: **`ALL SUITES PASSED`**, sweep **81/81**,
**12/12** palcos.

**O que falta para entrar em cena de verdade:** o `main.c` precisa (1) abrir o
pacote `assets/chars/<id>.chp4` do personagem de cada papel
(`ch_cena_plano()` já diz qual é), (2) dar-lhe lugar na VRAM no pool, e (3)
chamar `cc_quadro()` no laço de desenho e subir a textura quando ele devolver um
quadro. As três peças de dado e lógica estão prontas e testadas.

#### Fechado: o OPONENTE aparece na tela (o personagem do slot `dad`)

Era o buraco mais visível do port: em **26 das 27 músicas** a tela mostrava o
palco, o BF, a GF e **mais ninguém** — o oponente não era desenhado. Agora ele é:
quem é sai do registro da música (`sr_song_t.opponent`) pelo papel
(`ch_cena_plano`), os quadros do pacote `assets/chars/<id>.chp4`
(`char_pack.c`), a animação de cada instante do `char_cena.c` e a posição do slot
`dad` do palco (`ch_anchor` + `ch_slot`).

VRAM: a textura vai para o **mesmo pool** do palco (chave `SP_CHAVE_TELA_BASE+2`),
no **formato de pixel do próprio pacote** (PSMCT32 ou PSMCT16). Não se converte
32→16 aqui: essa conta de cor não foi medida, e pixel inventado é pior que pixel
caro. Se não couber, o evento é `char_sem_vram` — o oponente não entra, e o log
diz por quê; nada é omitido em silêncio.

Três defeitos que só apareceram executando:

1. **`corrupted double-linked list`.** A primeira versão reservava `w*h*2` bytes
   do staging (PSMCT16) e mandava o `cp_quadro` descomprimir `w*h*4` (o pacote é
   PSMCT32): 8 KB de estouro no heap. O tamanho do staging **é**
   `bytes_por_quadro`, e é isso que o teste com glibc/gcc pegou em segundos.
2. **O nome do arquivo do pacote não é o `name` da JSON.** O gerador usava o
   campo `name` ("Boyfriend (Holding GF)"), e o jogo procura pelo **id**
   (`bf-holding-gf`): as 46 artes existiam e **nenhuma** abria. Corrigido no
   gerador, na conferência e no comentário — o `name` é o nome de exibição.
3. **A carga não podia morar só na transição de estado.** No caminho do harness
   por música o `g_play_song` chega depois da transição, e o oponente nunca era
   carregado (o sweep pegou: 78/81). A conferência passou para o desenho, onde
   sempre se sabe qual é a música; ela sai na hora quando a música não mudou.

**Medido** (harness, jogo real):

| medida | resultado |
|---|---|
| sweep das 81 combinações | **81/81**, e agora cobrando `char_dad_primeiro` + `char_dad_animando` em cada uma |
| palcos | 12/12 (o sweep continua conferindo o palco também) |
| oponente animando | 10 quadros diferentes subiram para a VRAM; ~204 desenhos numa música |
| VRAM do oponente (pacote sintético 64×64 PSMCT32) | 16.384 B no pool |
| âncora medida (sintética) | (316, 824) — do slot `dad` do palco da música |
| ASan + UBSan | suíte inteira verde, zero erro/UB/vazamento |
| Story Mode (semana 1) | 3 músicas, 2 emendas, 1 resultado — inalterado |
| gameplay (roteiro do chart) | passa — inalterado |

O Tutorial continua **pulando** o oponente genérico (`char_dad_pulado`): ali o
oponente é a própria GF, que já é desenhada pelo caminho antigo, e desenhá-la de
novo seria vê-la duas vezes. É a divergência do slot registrada acima, que só uma
captura do console resolve.

**O que falta nesta frente:** o BF e a GF ainda vêm do caminho antigo (pacotes do
tutorial, faixas de quadro fixas). Trocá-los é o próximo passo — e depois as
animações de canto do oponente por evento de chart (`PlayAnimation`), que hoje
só cobrem as notas.

#### Fechado: BF e GF também pelo caminho genérico (27 de 27 personagens em cena)

Com o oponente já na tela, faltava o resto: o BF e a GF continuavam vindo dos
pacotes do tutorial (`bf_frames.lz4f`/`gf_frames.lz4f`) e de faixas de quadro
fixas, em **todas** as músicas. Agora os três papéis usam o mesmo caminho —
pacote CHP4, `char_cena`, `ch_anchor` — e o que sobra do caminho antigo é uma
exceção declarada.

**A exceção, e por que ela existe:** o **Tutorial** continua no caminho antigo
(`char_tutorial_legado`). É a única música cujas posições e câmera foram
conferidas numa captura do console; trocá-las por dedução de código seria jogar
fora uma medida. O sweep agora **exige** que o Tutorial *não* emita os eventos
genéricos — sem isso a exceção passaria despercebida e alguém acharia que os 81
estão no caminho novo.

**VRAM: nenhum byte novo.** Em vez de abrir slots, os personagens reaproveitam os
endereços que já existiam: o BF usa o slot `tex` (320×256 PSMCT32 = 327.680 B) e
a GF o slot `gftex` do pool (256×256 PSMCT32 = 262.144 B, o mesmo onde o tutorial
já escrevia 196.480 B de PSMCT16). Os pacotes reais cabem exatamente ali — BF
258×254 PSMCT32 = 262.128 B, GF 320×307 PSMCT16 = 196.480 B. Um pacote que não
caiba no seu slot fica de fora com `char_sem_vram` e as medidas no log, em vez de
escrever por cima do vizinho. O orçamento dos 21 palcos, portanto, **não muda**.

Duas coisas que a troca exigiu, e que valem registro:

- **A câmera passa a vir do `ch_anchor`** quando o modo genérico está ligado:
  `position + offsets + cameraOffsets do personagem + do slot`, que é a fórmula
  do `BaseCharacter.resetCameraFocusPoint` e a que já reproduz o BF do mainStage
  dígito a dígito. No Tutorial nada muda.
- **O canto dos três** agora é alimentado nos mesmos pontos onde o jogo já sabia
  quem canta: acerto do jogador → BF, nota do oponente → oponente **e** GF
  (quando ela não é o oponente), erro de nota → BF com `miss`.

E a duplicação saiu: o desenho do oponente virou a mesma função
`cena_desenha()` do BF e da GF, com os contadores por papel no log.

**Medido** (harness, jogo real, `dadbattle/normal`, 3002 quadros):

| evento | valor |
|---|---|
| `char_carregado` (bf, gf, dad) | 3 papéis, 30.598 / 16.900 / 9.860 B |
| `char_vram` ×2 + `char_dad_vram` | 16.384 B cada (pacote de teste 64×64 PSMCT32) |
| `char_bf_primeiro` / `char_gf_primeiro` / `char_dad_primeiro` | todos presentes |
| `char_bf_animando` / `char_gf_animando` / `char_dad_animando` | 126 desenhos, 10+ quadros cada |
| sweep com ASan+UBSan | **81/81**, exigindo os três personagens em cada combinação |
| Story Mode, gameplay, freeplay | passam, inalterados |

**O que continua faltando nesta frente:** as animações por evento de chart
(`PlayAnimation` — `hey`, `cheer`, `scared` etc.), que hoje só reagem a notas; e a
divergência do slot da GF no Tutorial, que espera uma captura do console.

#### Fechado: animação por evento de chart (`PlayAnimation`), e dois erros de tempo que ela revelou

O `main.c` recebia os eventos `PlayAnimation` desde a v0.2.105 e **só escrevia
log** (`tut_play_anim`). O comentário no código dizia que os pacotes do tutorial
não têm `hey` nem `cheer`, o que é verdade — mas isso escondia o resto: nas 27
músicas, 4 têm esses eventos (`bopeebo` 17, `philly-nice` 7, `spookeez` 4,
`tutorial` 4), e **nenhuma** das que têm era tocada.

Agora o evento vira `cc_toca()` no papel certo (`PlayAnimationSongEvent.hx`
L41-69: `bf`/`boyfriend` → 0, `dad` → 2, `gf` → 3), com o nome vindo da tabela
de strings do chart. E a animação nomeada **não é interrompida** pela dança até
terminar, que é a regra do `BaseCharacter.dance()` L479-497 ("quem está numa
animação que não começa com `dance`/`idle` e não terminou não dança").

**Dois erros de tempo, achados ao ligar isso, e os dois eram meus:**

1. **Um passo não é uma batida.** `ca_passos_ms()` fazia
   `passos * 60000/bpm`. O original é `stepLengthMs = beatLengthMs /
   STEPS_PER_BEAT` (`Conductor.hx` L187-192) com `STEPS_PER_BEAT = 4`
   (`Constants.hx` L424). Ou seja: **todo canto do port durava quatro vezes o
   que devia**. Não aparecia no tutorial porque lá o tempo era um número fixo
   escrito à mão (`TUT_SING_MS`) — o erro estava esperando o primeiro
   personagem de verdade.
2. **O erro de nota segura o canto pelo DOBRO**: `BaseCharacter.hx` L436,
   `if (getCurrentAnimation().endsWith('miss')) singTimeSec *= 2;`. Isso agora
   é `ca_canto_ms(passos, bpm, missing)`, e o tempo sai do `singTime` do
   **personagem** (com `8.0` de padrão, `CharacterData.hx` L451) e do **bpm da
   música**, não de uma constante.

Também entrou o `-end`: quando o canto acaba, se existe `<animação>-end` ela
toca em vez de voltar direto ao idle (`BaseCharacter.hx` L443-460).

**O que o sweep passou a cobrar:** um passe separado, com chart **FCH4**, nas
três músicas com `PlayAnimation`. O FCH3 tem um `u32` por evento e **o nome da
animação se perde** — no v3 o port sabe que houve o evento mas não o que tocar;
é o FCH4, com tabela de strings, que carrega o nome. Medido: **9/9** combinações
tocaram animação vinda do chart (bopeebo 3, philly-nice 2, spookeez 1) e **0
ficaram sem quadro**.

Três coisas que o teste desenterrou, e que eram do teste e não do jogo:

- o áudio sintético do sweep tinha 20 s e o evento da `philly-nice` cai em
  **13,7 s** de música — mas o `preparar_sweep` também encurtava o **fim do
  chart**, e quando o arquivo de áudio acaba o port salta o relógio para o fim.
  O passe de animação agora usa 32 s, e só nas três músicas (`--musicas`);
- sem o roteiro de teclas do chart o jogador **morria** em 13,5 s, 200 ms antes
  do evento. O passe agora aperta as notas do próprio chart;
- o passe v4 grava no **mesmo** destino que o v3: sem devolver esses três
  arquivos ao FCH3 no fim, o "passe v3" seguinte rodaria v4 sem ninguém ver.
  O script restaura, e o `--forcar` do preparador evita reaproveitar o destino.

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
