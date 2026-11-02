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
`%` das mesmas funções que julgaram. O **mixer de duas faixas** está fechado por
medição: o `stems.pcm` NÃO é uma mixagem pronta — ele carrega o instrumental e
as vozes em canais separados (8 bytes por amostra: `instL, instR, voicesL,
voicesR`, 16 bits cada) e o mixer do port soma as duas com saturação em
`tutorial_audio_mix_submit()`, exatamente o que o original faz com os dois
`FlxSound`. Ver "Fase 2 — o mixer de duas faixas", abaixo.

### Fase 3 — Telas: Freeplay, seleção de dificuldade, pausa, Game Over, Resultado: FECHADA

**Fechada** — detalhes na seção "Fase 3 — Telas: FECHADA", abaixo. As quatro
telas existem e cada uma tem harness próprio: `ui_state 9` (Resultado), `10`
(Game Over, com reinício por CROSS), `11` (Freeplay: 27 músicas, 153 quads,
dificuldade HARD, 48 notas, 506 quadros de volta) e a pausa (congela o relógio,
73 quads de sobreposição, Resume/Restart/Change Difficulty). A **arte** dessas
telas também entrou: cápsulas da Freeplay + fundo + títulos + DJ (`.chp4` de 9
animações), placar/tallies/percentual/barra do topo/nome da música/arte do rank/
selo de recorde, ícones de vida, splashes e a música do resultado nas 12 faixas
dos dois donos. Duas coisas que a lista antiga pedia **não existem na v0.8.6** e
por isso não entraram: o ícone de pausa (o `PauseSubState` monta só um
retângulo preto com alpha animado, e `pauseAlt/` não é referenciado por fonte
nenhum) e o `AtlasFont` da pausa (as entradas são `AtlasText` desenhadas com a
fonte do jogo).

O **som** da Freeplay, que era a última peça aberta desta fase, entrou: o
preview de 20 % do instrumental em laço com `fadeIn(2)`/`fadeOut(2)` a 0,7, os
dois jingles do rank por cima (oito slots de overlay no anel, porque os dois se
sobrepõem de verdade) e os três `playOnce` de menu com o volume de cada um
(`scrollMenu` 0,4, `confirmMenu`/`cancelMenu` 1,0). Os números, os dois defeitos
que o teste do anel pegou e a lista do que ainda falta estão em *"Fechado: o
ÁUDIO DA FREEPLAY"* e *"Fechado: os `playOnce` de menu"*, no fim do documento.

### Fase 4 — VRAM: FECHADA EM PARTE (1 de 4 passos medidos)

**Existia:** 8 reservas de VRAM no init, zero liberações; 8 buffers = 99,4 % da
VRAM; 2.929.792 B de imagens embutidas para 2 de 4 telas.
`src/engine/vram_cache.c` (LRU) existe, testado, e `vram_cache_init()` é
chamado **zero** vezes em `main.c`.

**Estado agora:** o cache está ligado (gftex e stagetex moram no pool, com
chave de tela), os frame buffers são 640×448 PSMCT16, os 21 palcos foram
reempacotados da arte real e os 46 personagens também. O que **não** é
necessário, medido: nenhum pacote dos 67 tem largura acima de 1024 (o maior é
`tankman-bloody`, 612×516, e o limite de 1024×1024 do PSMCT16 não é tocado), então
não há imagem para fatiar. Os **ícones de vida** do HUD entraram sem VRAM nova
(eles moram na faixa vazia do atlas do HUD, como o splash — seção dos NOTE
KINDS), então o que sobra desta fase é só o que depende do console: ver como a
captura PSMCT16 e o dither saem na TV. O `tools/orcamento_vram.py` fecha 27/27
combinações dentro do orçamento.

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

**Estado medido (atualizado):** este parágrafo descrevia o port antes da Fase 6.
Hoje ele desenha os **21 palcos** (`bs_props`, 157 props) e os **46
personagens** (`build/packs/chars`, 10.170 quadros), e as 81 combinações rodam com
a arte real (seção "O jogo inteiro com os pacotes DE VERDADE"). O texto abaixo
fica como registro do ponto de partida:

**Estado medido (histórico):** o port desenha **um** palco (`mainStage`, embutido no código
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
| `limoRide` | **1.870.848 B** | 5,3 MB |
| `tankmanBattlefield` | 1.869.568 B | 14,4 MB |
| `phillyStreetsErect` | 1.832.960 B | 5,3 MB |
| `sserafim` | 1.811.712 B | 5,0 MB |
| `mallXmasErect` | 1.801.216 B | 5,1 MB |
| `phillyStreets` | 1.790.976 B | 5,25 MB |
| `limoRideErect` | 1.762.816 B | **48,7 MB** |
| `spookyMansion` | 1.751.040 B | **59,4 MB** |
| `school` | 1.575.424 B | 21,6 MB |
| `schoolEvilErect` (melhor) | 529.152 B | 0,5 MB |

Os números desta tabela são os de **depois** da correção do passo da textura
(TBW de 64 px, ver "Fechado: o passo da textura no orçamento de VRAM"); os
antigos eram ~8 % menores porque contavam `sw*sh*2` sem a volta da largura.

Duas conclusões que **mudam o plano**, as duas tiradas dos números e não de
suposição:

1. **VRAM:** o pior palco pede 1,87 MB e o jogo tem 1,27 MB livres com as
   texturas de menu/título residentes. **15 dos 21 palcos não cabem** nesses
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

Hoje o jogo tem 1.269.760 B livres de VRAM e o pior palco precisa de
**1.870.848 B**, então não cabe sem devolver algum slot. Os slots que o **estado 8 realmente
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
pior palco (1.870.848 B) **com a conta correta** — sobram 185.344 B.

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

#### Fechado: o orçamento POR MÚSICA — as três músicas que não cabiam

`tools/orcamento_vram.py` é um teste (sai com 1 quando alguma música não cabe) e
ele **reprovava três**: `stress` (−186.368 B), `cocoa` e `eggnog` (−185.856 B).
A causa não era o tamanho da arte: eram **três contas diferentes** para o mesmo
número, e nenhuma delas era a do runtime.

| # | Erro | Efeito |
|---|---|---|
| 1 | `vram_budget.stage_limit()` (o limite que o **empacotador** obedece) reservava **um quadro** de cada personagem | os personagens residentes são o **canvas inteiro** do CHP4 (bf 435×461, gf 513×369, dad 368×418 a 0,5 ≈ **1 MB** pelos três): o palco era autorizado com ~1 MB a mais |
| 2 | o mesmo limite não reservava os **dois ícones** de vida | 49.152 B medidos nos pacotes `ICP1` |
| 3 | `orcamento_vram.py` media o palco com `cp_tex_bytes` (passo de **64 px** do TBW) | o pool aloca `sp_prop_bytes` = `sw*sh*2` arredondado a **256 B** (`src/engine/stage_vram_data.h`): a ferramenta cobrava 33 a 173 KB a mais por palco do que o jogo pede |

Correção, uma por linha:

- `vram_budget.fixos_bytes()` = **1.810.432 B**, a mesma soma do `orcamento_vram`
  e do `main.c` (2 framebuffers PSMCT16 de 640×448 + `tex2` 320×256×4 + fonte
  384×192×4 + `gftex16` 320×32×4).
- `vram_budget.reserva_por_palco()` lê os **pacotes reais**
  (`build/packs/chars/*.chp4`, `build/packs/icons/*.icp`) e devolve, para cada
  palco, o pior caso entre as músicas daquele palco — reservar o pior caso
  global encolheria palco que não precisava.
- `pacotar_palcos.stage_limit(bpp, palco)` usa `stage_limit_exato()` e a escala
  é procurada com `prop_vram_bytes` (o mesmo arredondamento do pool), não com
  `sw*sh*2` puro.
- `orcamento_vram.py` passou a usar `prop_vram_bytes` para palco e **mantém**
  `cp_tex_bytes` para personagem (que é uma textura só, com passo de 64 px).

Só dois palcos passavam do limite novo, e foram reempacotados da arte real
(`tools/pacotar_palcos.py --assets <clone> --stages <palco>`), com o plano
**atualizado em vez de reescrito** (`--stages` agora faz merge no
`stage-props.json`; antes ele apagava os outros 20 palcos):

| palco | escala antes | escala depois | VRAM antes | VRAM depois | limite |
|---|---|---|---|---|---|
| `mallXmas` (cocoa, eggnog) | 0,200845 | **0,194** | 1.337.856 B | **1.249.536 B** | 1.250.048 B |
| `tankmanBattlefield` (stress) | 0,295745 | **0,294** | 1.339.648 B | **1.326.848 B** | 1.327.104 B |

Medido depois (`tools/orcamento_vram.py --detalhe`): **PASSOU: 27 músicas, 0 não
cabem**. As três apertadas ficam com 256–512 B de folga (é o limite funcionando:
o palco encolheu só até caber), e a próxima mais apertada
(`winter-horrorland`) fica com **142.848 B**. No runtime
(`tools/run_realpacks_harness.sh 600`, 81 combinações): `palco_sem_vram` = 0 em
todas, e o pool livre vai de **135.936 B** (cocoa/eggnog) a 384.000 B (thorns).

Consequência para o que falta: continua **não** havendo espaço para uma textura
nova em cocoa/eggnog/stress — qualquer textura nova precisa entrar pela
`vram_cache` (evictando prop) ou vir de mais reempacotamento. O note splash
escapou dessa regra porque não é textura nova: ele é uma **região vazia de uma
textura que já existe** (a 2ª lição da barra de vida). Para a arte de tela, que
é grande e só aparece fora da música, o caminho é outro: ela pode ocupar o pool
no menu (fase 3) — o pool é liberado quando a música começa.

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

O Tutorial pulava o oponente genérico (`char_dad_pulado`). **Corrigido depois**
— ver a seção "o Tutorial sai do caminho antigo", abaixo, onde o motivo aparece:
o pulo era por *nome* ("gf") e deixava a única música sem `girlfriend` sem
oponente na tela.

**O que falta nesta frente:** o BF e a GF ainda vêm do caminho antigo (pacotes do
tutorial, faixas de quadro fixas). Trocá-los é o próximo passo — e depois as
animações de canto do oponente por evento de chart (`PlayAnimation`), que hoje
só cobrem as notas.

#### Fechado: FocusCamera com modo, ease, duração e posição — e o fixo de 1.000 que estava errado

`FocusCamera` é o evento de câmera mais numeroso do jogo: **664 eventos nos 81
charts**. O port guardava três coisas dele (alvo, x, y) e usava uma quarta
implícita: "a câmera sempre persegue o ponto novo". A fonte diz outra coisa
(`FocusCameraSongEvent.hx` L138-160):

| `ease` do evento | o que o original faz | quantos |
|---|---|---|
| ausente (padrão `CLASSIC`) | `resetCamera(false,false,false)` + `cameraFollowPoint.setPosition` — move o **ponto de foco**, sem tween | **520** |
| `CLASSIC` | idem | 72 |
| `INSTANT` | `tweenCameraToPosition(x,y,0)`, ou seja, a câmera **chega no mesmo quadro** | 61 |
| qualquer curva | `FlxTween` na **posição** da câmera por `stepLengthMs*duration`, com `ease+easeDir` | 11 |

Três defeitos concretos, todos medidos no módulo e no harness:

1. **`x`/`y` eram ignorados.** 123 dos 664 eventos trazem deslocamento e 86 focam
   uma **posição fixa** (`char: -1`, que não soma personagem nenhum). O port
   olhava só o alvo, então `{"char":-1,"x":-440,"y":150}` do `spaghetti` era
   aplicado como "foca quem estava focado".
2. **O modo não existia.** Todo evento virava perseguição: os 61 `INSTANT` e os
   11 com curva chegavam ao destino no ritmo do lerp, não no do evento.
3. **O fixo de 1.000 estava errado para valor negativo.**
   `fx()` fazia `(float)v / 1000.0f` sem passar por `s32`, então `-440` (gravado
   como complemento de dois pelo conversor) lia **4.294.527**. Zoom, scroll e
   intensidade são positivos e nunca acusaram; `x`/`y` de câmera não são. O teste
   novo do módulo pegou o valor na primeira execução.

E uma armadilha de formato, que também valia para o zoom e o scroll: o campo de
ease em `b` é um **índice na tabela de strings do arquivo**, e o índice 0 é um
nome de verdade. Um evento **sem** `ease` lia, portanto, "a primeira string que o
conversor internou" — uma curva que ninguém pediu. O conversor passou a marcar
essas ocorrências (`B_EASE_ABSENT`, bit 23), e o port resolve o padrão da classe
(`CLASSIC` para câmera; `linear`/`In` para zoom e scroll, `SongEvent.DEFAULT_EASE`).

**Medido depois:**

| medida | resultado |
|---|---|
| `test_chart_events` | 114 checks, 0 falhas — inclui os três modos, `char -1`, x negativo e duração padrão de 4 passos |
| harness, `tutorial` e `bopeebo` | `cam_foco_classic` (alvo, `linear`) |
| harness, `spaghetti` | `cam_foco_tween` com `char -1` e a curva `expo` |
| sweep FCH5 | **81/81**, e o passe de eventos segue 9/9 |
| `test_fch4.py` | todos os parâmetros sobrevivem ao round trip |

**O que falta nesta frente, e está declarado:** o foco **inicial**. O original
começa seguindo o oponente (`PlayState.hx` L2225-2227) e o port começa no BF, que
é o que a captura do console do Tutorial conferiu. E a duração do `ZoomCamera`
sem `duration` ainda é tratada como 0 (o padrão da classe é 4 passos) — outro
build, outra causa.

#### Fechado: o Tutorial sai do caminho antigo, e o uploader antigo para de escrever por cima dos personagens

Duas coisas erradas, uma causa: o caminho antigo do Tutorial continuou vivo
depois que o renderizador dele foi removido ("Clean Tutorial renderer
foundation"). O que sobrou não desenhava ninguém, mas continuava **escrevendo**.

**O que estava na tela.** O Tutorial era a única música que ainda passava por
`char_tutorial_legado`, e aquele ramo bloqueava a cena genérica: sobravam palco,
notas e HUD. Medido no harness, uma passada inteira por música:

| música | `char_bf_primeiro` | `char_gf_primeiro` | `char_dad_primeiro` |
|---|---|---|---|
| bopeebo | 1 | 1 | 1 |
| senpai | 1 | 1 | 1 |
| **tutorial** | **0** | **0** | **0** |

Ou seja: a música que abre o jogo era a única sem personagem nenhum.

**O que estava por baixo.** O bloco que subia os quadros de `bf_frames.lz4f` e
`gf_frames.lz4f` rodava em **toda** música, no laço de quadro, e escrevia em
`tex` e `gftex16` — que são exatamente os endereços de VRAM que o caminho
genérico usa para o BF e para a GF. Medido antes de remover: **282 subidas numa
passada do `bopeebo` e 193 numa do `senpai`**, ou seja, a arte do Tutorial era
escrita por cima do personagem daquela música a cada troca de quadro de canto.
Não era código morto: era código ativo com efeito errado (o `senpai` desenhava
`bf-pixel`, e a textura do BF recebia o BF do Tutorial).

**A correção, em três partes.**

1. `cena_carregar` não trata mais o Tutorial como caso especial: as 27 músicas
   abrem pacote CHP4, montam `char_cena` e posicionam por `ch_anchor`. As
   posições e a câmera que a captura de console fixou continuam valendo — é a
   mesma conta do `Stage.addCharacter` + `BaseCharacter.setScale`, e o caminho
   genérico já registra esses números dígito a dígito nos comentários.
2. O uploader antigo saiu do laço de quadro (`BF_SING`, `GF_SING` e `tut_anim_idx`
   foram com ele). Os pacotes do Tutorial continuam sendo **abertos** uma vez no
   `tutorial_scene_load` — eles provam que os assets existem e fixam os
   endereços de VRAM que o layout usa — mas ninguém mais escreve neles a cada
   quadro.
3. A regra "o mesmo personagem não entra duas vezes na cena" passou a ser do
   **dado**, não do nome: um papel é pulado quando o mesmo personagem já ocupa
   um papel anterior (`char_papel_repetido`). A versão antiga pulava o oponente
   só porque o id era `"gf"`.

**Por que o Tutorial precisa disso.** O playData dele
(`assets-data/songs/tutorial-metadata.json`) tem `player: bf`, `opponent: gf` e
**nenhum `girlfriend`**. Então: slot `gf` vazio (`char_vazio`), BF no slot `bf`
e a GF no slot **`dad`**, como oponente — exatamente o que o dado diz. A versão
anterior, por pular o oponente pelo nome, deixava o Tutorial sem BF *e* sem
oponente.

**Câmera por papel, e não por booleano.** O foco da câmera era um booleano
`gf/dad` — tratava "GF" e "DAD" como o mesmo ponto, o que só dá certo no Tutorial
(onde o oponente *é* a GF). Dos **664 eventos FocusCamera** dos 81 charts,
**265 focam o oponente** (char 1): metade das músicas mandava a câmera para o
ponto da GF. Agora o papel sai do valor do evento e o ponto sai do `ch_anchor`
daquele papel; e o alvo só troca se o personagem **existe** na cena, porque
`FocusCameraSongEvent.hx` L104-133 faz `return` quando `getDad()`/`getGirlfriend()`
devolve `null`.

**Medido depois** (harness, jogo real, mesmo roteiro):

| medida | antes | depois |
|---|---|---|
| tutorial: BF / GF / oponente desenhados | 0 / 0 / 0 | **1 / vazio (dado) / 1** |
| subidas do uploader antigo por música | 282 (`bopeebo`), 193 (`senpai`) | **0** |
| sweep 81 combinações | 81/81 | **81/81**, agora cobrando papel vazio só onde o dado manda |
| `char_sem_vram` / `char_pacote_ruim` | 0 | **0** |

E o sweep passou a cobrar o **contrário** do que cobrava: nenhuma música pode
emitir `char_tutorial_legado`, o BF e o oponente têm de aparecer em todas, e a
GF só é exigida nas 26 que têm `girlfriend` no dado — no Tutorial o que se cobra
é o `char_vazio` do slot `gf`.

**O que continua em aberto nesta frente:** o foco inicial da câmera. O original
começa seguindo o **oponente** (`PlayState.hx` L2225-2227) e o port começa no BF,
que é o que a captura do Tutorial conferiu. Mudar o inicial é outra causa (regra
8) e vai num build próprio, com o evento que prova qual ponto a câmera seguiu.

#### Fechado: BF e GF também pelo caminho genérico (27 de 27 personagens em cena)

Com o oponente já na tela, faltava o resto: o BF e a GF continuavam vindo dos
pacotes do tutorial (`bf_frames.lz4f`/`gf_frames.lz4f`) e de faixas de quadro
fixas, em **todas** as músicas. Agora os três papéis usam o mesmo caminho —
pacote CHP4, `char_cena`, `ch_anchor` — e o que sobra do caminho antigo é uma
exceção declarada.

**A exceção, e por que ela existia:** o **Tutorial** continuava no caminho antigo
(`char_tutorial_legado`). É a única música cujas posições e câmera foram
conferidas numa captura do console; trocá-las por dedução de código seria jogar
fora uma medida. O sweep exigia que o Tutorial *não* emitisse os eventos
genéricos — sem isso a exceção passaria despercebida. **A exceção caiu na
seção seguinte**, quando ficou medido que o caminho antigo não desenhava mais
ninguém.

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

#### Fechado: o sufixo `-hold`, e a dança que tinha de acontecer na batida

`BaseCharacter.hx` L408-425: quando a animação atual **termina** e existe
`<nome>-hold`, ela entra e fica (é em loop no dado). É assim que os olhos do pai
piscam e a chama do isqueiro do Darnell se mexe. E L454-456: ao procurar o
`-end`, o sufixo `-hold` sai **antes** — quem está cantando e já entrou no
`singLEFT-hold` precisa achar `singLEFT-end`, não `singLEFT-hold-end`.

Entrou em `char_cena.c`. O teste de "terminou" é `tempo decorrido ≥ count/fps`
(`isAnimationFinished()` só é verdadeiro fora de loop, e nenhum dos 44 pares
`-hold` do pacote declara `looped: false`).

**O erro que isso desenterrou era meu, e era visível:** `dance()` rodava em todo
quadro. O original chama `dance()` no `onBeatHit`. Rodando por quadro, o
personagem terminava o `idle`, entrava no `idle-hold` e **no quadro seguinte**
voltava ao `idle` — o `-hold` nunca aparecia. Agora `cc_papel_t` guarda
`ultima_batida` e a dança acontece uma vez por batida. Ciclo medido: `idle` →
(acaba) → `idle-hold` → (batida) → `idle`.

Medido no harness: `dadbattle` registra `char_hold,2,1` aos 309 ms (papel 2 =
dad, animação 1 = `idle-hold`). O sweep passou a cobrar isso: se o personagem
tem `-hold` de `idle`/`sing*`/`dance*` e a música roda, alguma tem de aparecer.
Resultado: **81/81 combinações começaram, 12/12 palcos desenharam**, e o
`phillyBlazin` — que antes nem era desenhado porque a única música dele falhava
— agora entra.

**O limite do que o sweep pode cobrar, e por quê.** O evento `char_hold_tem`
conta só os `-hold` que o port tem caminho para tocar. Os outros — `taunt-hold`,
`uppercut-hold`, `shoot1..4-hold` — são pedidos por **note kinds**, e é o achado
seguinte.

#### Fechado (em parte): **note kinds** (`k` nas notas) — formato FCH5 e os dois kinds de código

As notas do JSON trazem `"k"`, o *note kind*. O conversor jogava o campo fora e
o `chart_note_t` não tinha onde guardá-lo, então o port tratava toda nota igual.
Nove das 81 combinações usam kinds, 34 kinds distintos:

| música | kinds | notas (easy / normal / hard) |
|---|---|---|
| `blazin` | `weekend-1-*` (21 tipos: blockhigh, punchhighblocked, taunt, picouppercut…) | 422 / 426 / 440 |
| `2hot` | `weekend-1-lightcan/kickcan/cockgun/kneecan/firegun` | 30 / 30 / 30 |
| `spaghetti` | `sakura-joint`, `sakura-bf1/2`, `non_scoreable` | 274 / 354 / 472 |
| `cocoa`, `eggnog` | `mom` | 80 / 96 / 137 (cocoa) |
| `spookeez` | `noanim` | 4 / 4 / 4 |
| `stress` | `hehPrettyGood` | 1 / 1 / 1 |
| `ugh` | `ugh` | 4 / 4 / 4 |

**O formato.** FCH5 é o FCH4 com a nota de 12 → 16 B:

```
nota FCH5 (16 B): u32 sample | u32 duration | u8 lane | u8 must_press |
                  u16 kind | u32 order
```

`kind` é índice na **mesma** tabela de strings dos eventos (0xFFFF = "sem
kind"). A tabela já existia no FCH4; usar a mesma poupa um leitor e um
verificador de tamanho. O conversor passou a **emitir v5 por padrão**
(`--format v3|v4|v5` continua aceitando os antigos), e o loader lê os três. O
v3 e o v4 devolvem `CHART_KIND_NONE` em toda nota, que é a verdade: aqueles
containers não têm onde guardar o campo.

**Os dois kinds que são código, não script** — comportamento provado pela fonte,
sem invenção:

- `noanim` (`NoAnimNoteKind.hx`, `noanim = true`): o canto não toca no acerto —
  `BaseCharacter.hx` L561/L577 — e a nota pontua normalmente;
- `non_scoreable` (`NonScoreableNoteKind.hx`, `scoreable = false`): não entra em
  `tallies.totalNotes` (`PlayState.hx` L2538-2554), o acerto não soma pontos,
  vida nem combo (L3159 põe todo o `applyScore()` dentro de `if
  (note.scoreable)`, e L3143 não incrementa o combo), e no miss o kind esconde a
  nota e **cancela** o evento (`onNoteMiss`: `note.visible = false;
  event.cancel()`), então não há dano nem miss contado.

**O que continua aberto, e por falta de dado.** Os outros 32 kinds
(`weekend-1-*`, `mom`, `ugh`, `hehPrettyGood`) são scripts de mod que **não
estão neste repositório**: sem eles não há como saber o que cada um toca, e
inventar seria divergência. O formato agora **carrega o nome** até o runtime, o
que era o pré-requisito: quando os scripts chegarem, não é preciso mexer no
formato outra vez. As animações `taunt`, `uppercut` e `shoot1..4` do
`pico-blazin`, `pico-speaker` e `darnell-blazin` continuam sem tocar, e é por
isso: quem as pede é o kind, não o chart. Pedido ao usuário: mandar a pasta de
note kinds do jogo original (`mods/.../noteKinds/` ou
`assets/shared/data/noteKinds/`).

**Medido.** `test/test_note_kinds.c` (novo) confere nota por nota nos charts
reais: `spookeez` 4 `noanim`, `spaghetti` 1 `non_scoreable`, `cocoa` 80 `mom`,
`blazin` 4 `weekend-1-taunt`, `2hot` 30 `weekend-1-firegun`, `tutorial` e os
charts FCH4 sem kind nenhum. A suíte converte os 81 charts nos três formatos e
carrega os três no loader real. O sweep ganhou um passe **v5** das sete músicas
com kinds, que cobra do log do jogo o **número exato de notas com kind do
JSON** (`chart_notas_com_kind`): 21/21 combinações. Sem esse passe, um formato
que voltasse a perder o campo passaria despercebido — foi exatamente o que
aconteceu até esta build.

Três armadilhas que o trabalho desenterrou, e as três eram do teste:

1. `preparar_sweep.py` **reaproveitava** o chart já instalado, então trocar
   `FNF_SWEEP_FMT` não mudava nada e o "passe v3" podia rodar v5. Agora o
   fourcc do próprio arquivo é conferido (`FCH3`/`FCH4`/`FCH5`), além de um
   marcador de formato no diretório de assets;
2. `harness_presses.py` recusava `FCH5` — o roteiro de teclas do gameplay
   parava de ser gerado, e o teste falhava por isso, não por um bug do jogo;
3. `test_chart_limits.c` só conhecia `FCH3`/`FCH4`, e chamava os 81 charts de
   "ilegíveis" quando o default virou v5.

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

#### Fechado: `danceEvery` e `startingAnimation` dos props (Bopper.hx)

O `stage_layout` escolhia `danceLeft`/`danceRight` pela **paridade da batida**
para todo prop, e calculava o quadro pelo tempo global. O `Bopper.hx` faz
outra coisa, e o dado dos 157 props separa os casos com precisão:

| `danceEvery` | props | o que o original faz | o que o port fazia |
|---|---|---|---|
| **1** | **25** | `dance()` a cada batida, com `forceRestart` (`shouldBop = true` nos props) e alternando `danceLeft`/`danceRight`; sem as duas, toca `idle` | alternava certo, mas **nunca reiniciava**: o quadro vinha do tempo global, então as 25 animações andavam fora de fase com a música |
| **2** | **2** | idem a cada **duas** batidas | alternava/reiniciava a cada batida |
| **0** | **130** | `dance()` **nunca** é chamado; toca só o `startingAnimation` do JSON (Stage.hx L308-311), em loop desde o início — **e nada sem ele** | animava a primeira animação de todo prop |

Detalhes que o código agora segue, todos de `Bopper.hx`:

- `onStepHit`: `danceEvery > 0 && step % (danceEvery * STEPS_PER_BEAT) == 0`,
  com `STEPS_PER_BEAT = 4`; o passo 0 é a **primeira** chamada, e
  `hasDanced` começa `false`, então o passo 0 toca `danceLeft`;
- a fase do quadro é o tempo **desde a última chamada** (passos desde ela ×
  `beat_s/4`), não o tempo global — é o que faz a animação reiniciar na batida;
- antes do passo 0 (contagem regressiva) vale o `startingAnimation`, que é o
  que o palco tocou ao ser construído.

**Medido, e o que ficou faltando por dado:** dos 7 props que têm mais de um
quadro e **nenhum** `startingAnimation` com `danceEvery == 0`, todos são
animados por **script de palco** do original (`phillyCars`, `phillyCars2`,
`phillyTraffic`, `lightning`) — scripts que este repositório não tem. Eles
ficam no primeiro quadro, que é o que o dado manda, e o jogo grava
`prop_sem_inicio` **por prop** e `palco_props_sem_inicio` **por palco** no log:
medido no harness, `darnell`/`phillyStreets` reporta 3 (props 60, 61 e 62;
`phillyTraffic` com 154 quadros). Movimento inventado seria pior que ausência
declarada.

O teste (`test/test_stage_layout.c`) agora prova a regra, e não a aparência:
`limoDancer1` toca `danceLeft` no quadro 0 no passo 0, **`danceRight` no quadro
0** no passo 4 (reiniciou) e `danceLeft` no passo 8; `bgLimo` (dance 0 com
`startingAnimation`) anda com o tempo; `phillyTraffic` fica no quadro 0 aos
10 s; `sniper` (`danceEvery 2`) está no quadro 0 no passo 8, no 9 no passo 11 e
para no 14 (o `idle` dele tem 15 quadros e não é em loop).

#### Fechado: props e personagens na MESMA lista, por `zIndex`

O desenho fazia `palco_draw(...)` uma vez, **antes** do oponente, da GF e do BF.
Isso põe todo prop atrás de todo personagem, e o original não faz assim:
`Stage.hx` L264 põe cada prop no grupo do palco com `propSprite.zIndex =
dataProp.zIndex`, L449 põe cada personagem no **mesmo** grupo com
`character.zIndex = stageCharData.zIndex`, e `this.add(character)` (L489) usa o
mesmo `add` dos props. O `FunkinGroup.refresh()` reordena os filhos por
`SortUtil.byZIndex`, ou seja: **uma lista só, crescente por z**, com os três
personagens dentro dela. O z de cada personagem não é constante — vem do slot
do palco (`bs_slots[]`) e muda de palco para palco: no `tankmanBattlefield` o BF
e o DAD estão em **102** e a GF em **100**; no `mainStage` são 100/200/300.

Medido nos dados (157 props, 21 palcos), **6 palcos** têm prop com z acima do z
do BF no enquadramento do BF — e é justamente onde o desenho antigo errava:

| palco | props na frente de quem canta |
|---|---|
| `tankmanBattlefield` | `tankmanAudience0..5` (z 1000–1100) e `tankBricks` (101, na frente da GF) |
| `phillyStreets` / `phillyStreetsErect` | `spraycanPile` 690 / `paper` 800 |
| `spookyMansionErect` | `stairsDark` 400, `stairsLight` 401 |
| `mainStageErect` | `lights` 4000, `lightAbove` 4500 |
| `sserafim` | `frontStool` 1500 |

O que o port faz agora (`src/main.c`): `sl_collect` roda **uma vez** por quadro
e o desenho sai em fatias de z — `[z < gf]` gf `[z < bf]` bf `[z < dad]` dad
`[z >= dad]` —, com os personagens ordenados por z e desempate pela ordem de
adição do `PlayState` (GF L2207, BF L2216, DAD L2225). A ordem **dentro** de
cada fatia continua a do `sl_collect` (z crescente, inserção estável). O
`sl_item_t` ganhou o campo `z`, para o corte usar o mesmo número que ordenou.

O caminho legado do Tutorial (o que a captura de console conferiu) não muda:
no `mainStage` os três props têm z 10, 20 e 30, todos abaixo do menor z dos
personagens (100), então "props e depois as três pessoas" é a mesma lista.

A reserva da textura composta (`stagetex`) passou a ser decidida **antes** de
desenhar (`palco_reserva`): ela entra quando o palco não tem nada a desenhar —
nenhum prop na tela ou o primeiro prop sem pacote no pendrive. Antes ela era
decidida pelo resultado do desenho, o que, agora que o palco é fatiado, daria
"0 props" na primeira fatia de todo palco e trocaria o palco inteiro pela arte
composta.

O teste (`test/test_stage_layout.c`) prova quatro coisas, e não a aparência:
o `z` do item é o `z` do prop; a lista sai em z não-decrescente nos 21 palcos;
as fatias por z são **contíguas** (o corte nunca cai no meio de um empate); e o
`tankmanBattlefield` tem prop dos dois lados do BF (medido: 4 na frente, 11
atrás), que é o motivo do corte existir.

#### Fechado: os 12 props de COR (`#RRGGBB`) e o `alpha` de cada prop

Duas coisas do `Stage.hx` que o dado do jogo tem e o port não usava.

**1) Os props de cor.** Doze dos 169 props não têm arte: o `assetPath` deles é
uma cor (`"#222026"`, `"#F3F4F5"`…). O `Stage.hx` L174 marca
`isSolidColor = assetPath.startsWith('#')`, L203-216 cria o retângulo com
`makeSolidColor(scale[0], scale[1], FlxColor.fromString(assetPath))` e — detalhe
que muda a conta — L232-243 **pula** o `propSprite.scale.set(...)` para esses
props: o `scale` do JSON **é** o tamanho em pixels, não um multiplicador. Eles
entram no mesmo grupo por `zIndex` dos props texturizados, então têm de entrar
na mesma lista de desenho, e não antes/depois de todos.

No port, `tools/gen_stage_dados.py` agora lê esses props direto do JSON do palco
e escreve `bs_cores[]` (`x`, `y`, `w`, `h`, `scroll`, `z`, `r/g/b`, `alpha`,
`blend`, `ord`) com faixas por palco em `bs_stage_cor_first[]`/
`bs_stage_cor_count[]`. `sl_collect` faz neles a **mesma** conta de tela dos
props (`SL_VIEW`/`câmera`/`scroll`), e `palco_fatia` os desenha como retângulo
chapado (`rect_alpha`, 3 qwords, 0 byte de VRAM, sem pacote). O `ord` novo em
`bs_prop_t` e `bs_cor_t` é a posição na lista `props` do JSON: o sort do flixel é
estável, então para `zIndex` igual vale a ordem do arquivo — e agora as cores
respeitam isso.

**2) O `alpha`.** `Stage.hx` L253 faz `propSprite.alpha = dataProp.alpha`, com
`@:default(1.0)` (`StageData.hx` L142). 41 dos 169 props declaram `alpha`, e
**9 valem 0** — são invisíveis no instante zero: `schoolErect/freaks`,
`tankmanBattlefield/clouds2`, `phillyStreetsErect/paper`, `sserafim/fucker`,
`sserafim/truckLight1`, `sserafim/truckLight2`, `sserafim/backLightColor`,
`sserafim/backLightWhite` e o `sserafim/solidCover`. O port desenhava todos
opacos. Agora `bs_prop_t.alpha` vem do JSON, `sl_collect` **não** põe na lista
quem tem `alpha <= 0` (nem pacote, nem VRAM, nem desenho) e quem tem `0 < alpha
< 1` chega ao `sprite_dims_alpha` com `alpha*128` no vértice — no GS a mistura é
`Cs*As + Cd*(1-As)`, então `0,3` vira `0x26`. Medido: `phillyStreetsErect/grey1`
0,3 e `grey2` 0,8 são os únicos entre 0 e 1.

**O `blend` continua pendente, e agora se sabe de onde ele vem.** Não há campo
`blend` em prop nenhum dos 169 (o `StageData` tem `@:default('')`); quem põe
blend no original é o **script do palco**, com o número do
`openfl.display.BlendMode` (0 = ADD, 9 = MULTIPLY, 12 = SCREEN):
`limoRideErect.hxc` (6 misturas ADD), `mainStageErect.hxc` (5),
`phillyBlazin.hxc` (ADD em `skyAdditive` e `additionalLighten`, MULTIPLY em
`foregroundMultiply`), `phillyStreetsErect.hxc` (9), `phillyStreets.hxc` (1) e
`sserafim.hxc` (SCREEN em `truckLight1`/`backLightColor`, ADD em
`backLightWhite`). Como o port ainda não executa HScript, a única cor que o
script mexe no instante zero que **muda o desenho** está tratada por dado:
`phillyBlazin/additionalLighten` nasce com `blend = 0` (ADD) **e**
`visible = false` (L76-77), então entra em `bs_cores[]` com `alpha = 0` — é o
estado do original no instante zero, em vez de um branco somado à tela inteira.
As outras misturas ficam registradas na tabela de pendências.

O teste cobre: `BS_COR_COUNT == 12`; as faixas por palco particionam
`bs_cores[]` sem buraco e cada cor pertence ao palco que diz; no `schoolEvil` o
`solid` preto (z 0, `ord` 0) é o **primeiro** item e cobre a tela; o `snowUnder`
do `mallXmas` é 243/244/245 no z 59; `solidCover` e `additionalLighten` **não**
entram (alpha 0); `clouds2` não entra mas `mountains` do mesmo palco entra;
`grey1` chega com 0,3.

#### Fechado: a MESCLAGEM dos props (ADD) e de onde ela vem

O `StageData` não tem `blend`: medido, **0 dos 169 props** carrega a chave. Quem
põe a mistura é o **script do palco**, com o número do
`openfl.display.BlendMode` (0 = ADD, 9 = MULTIPLY, 12 = SCREEN).
`tools/extrair_blend_palcos.py` lê os 19 `.hxc` e copia para
`assets-data/stage-script.json` o **estado inicial** de cada prop, com arquivo e
linha, reconhecendo só quatro formas de statement (nome literal, variável vinda
de `getNamedProp`, `StringTools.endsWith(name, ...)` e `name == ...` dentro de
`addProp`, e `case 'X':` de um `switch (name)`). O que não casa com essas formas
sai como `NAO RESOLVIDO` — nada é adivinhado.

Medido: **16 props em 5 palcos**, 13 com ADD e 3 com MULTIPLY/SCREEN.

| palco | props | mistura | fonte |
|---|---|---|---|
| `mainStageErect` | `brightLightSmall`, `orangeLight`, `lightgreen`, `lightred`, `lightAbove` | ADD | `mainStageErect.hxc` L49-53 |
| `limoRideErect` | `shootingStar` | ADD | `limoRideErect.hxc` L117 |
| `phillyBlazin` | `skyAdditive`, `additionalLighten` (ADD); `foregroundMultiply` (MULTIPLY) | ADD / MULTIPLY | `phillyBlazin.hxc` L65, L72, L76 |
| `phillyStreets` | os `*_lightmap` | ADD **e alpha 0,6** | `phillyStreets.hxc` L517-524 |
| `phillyStreetsErect` | os `*_lightmap` (ADD, 0,6), `grey1` (ADD), `grey2` (MULTIPLY) | ADD / MULTIPLY | `phillyStreetsErect.hxc` L641-652 |
| `sserafim` | `truckLight1`, `backLightColor` (SCREEN), `backLightWhite` (ADD) | SCREEN / ADD | `sserafim.hxc` L393-405 |

O detalhe que muda a imagem: os `_lightmap` do `phillyStreets` são `alpha = 1,0`
no JSON e o `addProp` do script roda **depois** do `buildStage`, então o 0,6 do
script vence — antes o port desenhava esses dois mapas de luz opacos e sem ADD.

No desenho (`src/main.c`), ADD é uma troca do registrador `ALPHA` do GS em volta
do sprite: `GS_SET_ALPHA(0,2,2,1,0x80)` dá `(Cs - 0)*1 + Cd`, que é `src + dst`;
o normal volta com `GS_SET_ALPHA(0,1,0,1,0x80)`. Custa 4 qwords por sprite e
nenhum byte de VRAM. O GS **não** faz MULTIPLY nem SCREEN na pipeline fixa, e o
port não finge que faz: esses 3 props saem normais e o caso é contado no log
(`palco_blend,aditivos,sem_gs`, uma vez por palco).

Medido com os pacotes reais (`darnell`, phillyStreets): `palco_blend,2,0` e
`palco_add,4,77` — o prop de índice 4 desenhou com ADD e alpha 77 (0,6×128), e
`palco_draw` passou de 9 para 13 props na tela sem nenhum estouro de pacote.
No harness dos 81 pares, `sserafim` reporta `palco_blend,1,2` (1 ADD + 2 SCREEN)
e não desenha nenhum deles, porque os três nascem com `alpha = 0` — que é o
estado do original no instante zero.

#### Fechado: o passo da textura no orçamento de VRAM (TBW de 64 px)

O pool reservava `sw*sh*2` para cada prop (16 bpp, arredondado a 256 bytes) e o
upload escrevia a textura com `t->width = (sw+63)&~63`. As duas contas não se
falavam, e a diferença é o passo do GS: o `BITBLTBUF` guarda a largura do buffer
em **unidades de 64 pixels** (`dest_width>>6`, em `draw_texture_transfer` do
ps2sdk), então um quadro de 802 px ocupa a faixa de **832 px por linha**. Para o
`stageBack` do `mainStage` (802×438) isso é 728.832 B de ocupação real contra
702.976 B reservados: **25.856 B de escrita por cima do prop seguinte**.

Medido no plano: **82 dos 157 props** têm largura que não é múltipla de 64, e
todo palco tem pelo menos um. O harness não pega isso — ele não rasteriza, só
conta primitivas e confere o pacote —, então o sintoma seria arte trocada no
console sem uma linha de log dizendo por quê.

Correção em `tools/gen_stage_vram.py` (`bytes_vram`): `ceil64(sw)*sh*2`,
arredondado a 256 B. A altura não é arredondada de propósito — o upload escreve
exatamente `sh` linhas. O `src/engine/stage_vram_data.h` foi regerado e o teste
(`test/test_stage_layout.c`) passou a recalcular a conta **por conta própria**
para cada um dos 157 props e comparar com o array gerado — divergência de
gerador vira falha de teste, não lixo na tela.

O mesmo defeito existia no **personagem**, e ali o sintoma era outro: o
`cena_carregar` subia o quadro com `width = pk->w` (258 no BF do tutorial), e
como o GS arredonda o passo para 256 px, a imagem saía **enviesada dois pixels
por linha** — além de a VRAM reservada (`258*254*4 = 262.128 B`) ser menor que a
ocupada (`320*254*4 = 325.120 B`), escrevendo por cima do vizinho. Agora
`cp_tex_bytes()` (`engine/char_pack.c`, com teste em `test/test_char_pack.c`)
calcula `ceil64(w)*h*psm/8` alinhado a 256, e o `cena_carregar` usa esse número
para a VRAM **e** para o passo da textura (`g_cena_texde[r].width`), com o
evento `char_stride` gravando as duas larguras. O empacotador
(`tools/pacote_personagem.py`) passou a gerar o canvas já com largura múltipla de
64, então pacote e textura concordam por construção.

Números novos (os antigos da tabela de VRAM eram ~8 % menores):

| medida | antes | agora |
|---|---|---|
| pior palco (soma dos props) | 1.697.760 B (`tankmanBattlefield`) | **1.870.848 B** (`limoRide`) |
| pior **momento** na tela (props visíveis ao mesmo tempo) | 1.697.032 B (`mainStage`) | **1.799.424 B** (`sserafim`) |
| props visíveis ao mesmo tempo (máximo dos 21 palcos) | 14 | 14 (o corte do `sl_collect` é 32) |
| palcos que cabem no pool de 2.056.192 B | 21 de 21 | **21 de 21** |

A varredura do "pior momento" olha a câmera em cada prop de cada palco e no slot
do BF, e soma o que o pool reservaria para os props visíveis naquele quadro. É o
número que o console cobra: o pool é por quadro, com despejo LRU, não a soma do
palco inteiro.

#### Fechado: os eventos que o port não reproduz, declarados no log

Dos 3.330 eventos dos 27 charts, a maioria virou estado no `chart_events.c`
(FocusCamera, ZoomCamera, SetCameraBop, ScrollSpeed, PlayAnimation). Sobram dois
grupos, e os dois **por falta de dado no repositório**, não por esquecimento:

| evento | onde | quantos | o que falta |
|---|---|---|---|
| `SetHealthIcon` | `spaghetti` | 15 | a **arte** dos ícones (`yunjin`, `kazuha`, `sakura`, `chaewon`, `eunchae`); `assets-data` não tem `HealthIcon` — o HUD desenha os dois ícones fixos |
| `sserafim*` | `spaghetti` | 89 | o **script** do palco que toca os props (`sserafimPulseLights`, `sserafimSing`, …) |

Antes, os dois grupos caíam no `default` do `chart_events_apply` e o único
registro era `tutorial_chart_event` com um tipo numérico — indistinguível de um
evento que o port entende. Agora o `main.c` conta cada grupo e grava **uma vez
por música**: `evento_sem_icone` (char, ref), `evento_sem_script` (tipo,
amostra) e `evento_sem_efeito` (ChangeCharacter/AltIdleAnimation, que o v0.8.6
não tem). No fim da música, `eventos_sem_efeito` fecha a conta (contagem de
ícone | script<<8, sem-efeito).

Medido (harness, `spaghetti` FCH5, 1.200 quadros):

```
chart_layout,0,0              chart aberto no layout 0
evento_sem_script,256,51665   primeiro evento de script, tipo 0x0100
evento_sem_icone,1,5          SetHealthIcon char 1, ref 5 ("yunjin")
```

E o sweep passou a **cobrar** os dois avisos nessa música: se a arte dos ícones
ou o script do palco entrarem no repositório um dia, o teste acusa (o aviso
sobra) em vez de deixar a diferença desaparecer em silêncio.

#### Fechado: chart FCH3 com eventos agora avisa (em vez de zerar tudo em silêncio)

O loader aceita FCH3 de propósito — é o formato que o console já carregou. Mas o
registro v3 tem só `sample/type/value/order`: **zoom, duração, alvo de animação
e id de ícone não existem nele**. Um FCH3 com 468 eventos (o `spaghetti`) carrega
sem um erro e chega ao `chart_events_apply` com todos os parâmetros zerados — o
sintoma seria a câmera indo para zoom 0, sem nada no log dizendo por quê.

Medido antes da correção: com o pendrive em FCH3, o `spaghetti` desenhava o palco
e o áudio andava, mas **nenhum** dos 89 eventos de script aparecia e o
`SetHealthIcon` nunca era contado (os contadores agora existem e ficaram em zero).

Agora o `main.c` grava `chart_v3_eventos` com o número de eventos quando o chart
carregou em v3 e tem eventos (`chart_v3_eventos,468,0` no log). Continua
funcionando — a compatibilidade não muda — mas a diferença entre "o chart não tem
eventos" e "o chart tem e o formato não os carrega" fica escrita.

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

### Fase 5 — Otimização medida: FECHADA (com a tabela no documento)

**Existe:** o harness de PC mede quadros, pacotes, qwords e overflows, e a
tabela de custo por quadro está registrada abaixo em duas versões — a linha de
base (`## Fase 5: o custo por quadro, medido`) e a mesma conta com os pacotes
**de verdade** (`## Fase 5 com os pacotes de verdade: o custo por quadro,
medido`). Nenhum ganho é declarado sem antes/depois medido no harness.

**O que ficou de fora, e é limite de hardware, não de fonte:** as vozes do anel
de áudio (uma secundária, então dois `playOnce` simultâneos dividem a voz) e o
que só o console responde (dither, captura, tempo real de DMA).

---

### Fase 3 — Telas: FECHADA (arte incluída)

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
| tela | fundo preto opaco, texto "Game Over", BF em `firstDeath` | fundo preto + texto + **BF em `firstDeath`** (seção nova, abaixo) |
| entrada | `canInput` de 1 s **só para o toque**; ACCEPT_P/BACK_P valem desde o 1º quadro | CROSS/CIRCLE a partir do 2º quadro |
| reinício | `deathConfirm`, `FlxG.camera.fade(BLACK, 1)`, e o `needsReset` **no fim do fade** | 60 quadros de fade e depois transporte, relógio, cursor do chart, tallies, câmera e contagem regressiva voltam ao início |

Medido (`tools/run_gameover_harness.sh`, 5.000 quadros, **três** passadas):
morte no quadro 3.559 com **13 misses** (13 × 0,08 = 1,04 > 1,0 de vida), 180
quadros em `ui_state 10`, 22 quads desenhados na tela 10, reinício no quadro
**3.739** — exatamente **60 quadros** depois do CROSS, que é o fade de 1 s —,
música começando 2 vezes, 2 contagens completas, e na terceira passada o CIRCLE
sai da tela (`gameover_exit` no quadro 3.679) **sem** reiniciar a música. O teste
falha se o jogador não morrer, se a tela não desenhar, se o `firstDeath`/
`deathLoop` não entrarem, se o reinício não esperar o fade, se a câmera não
chegar no foco ou se o zoom não bater com `stage.camZoom * death.cameraZoom`.

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
| menu | Resume / Restart Song / Change Difficulty / Enable Practice Mode / Exit to Menu | as cinco, na mesma ordem (ver abaixo) |
| posição | `y = 70·i + 30`, `x = 0` em 1280×720 | metade disso em 640×360 |
| entrada | primeiro quadro ignora (`justOpened`) | `g_pause_frame >= 2` |
| metadados | nome da música, dificuldade, `N Blue Balls` | iguais, no canto superior direito |
| Change Difficulty | lista as dificuldades que a música tem + Back | `SR_DIFF_*` do registro + Back; escolher recarrega chart e cena |

**"Enable Practice Mode" (fechado).** A entrada ficou de fora por muito tempo
com a justificativa de que "o Practice Mode do original é um modo de jogo
inteiro". Lido o original (v0.8.6), **não é**: no `PlayState` atual o Practice
Mode é um **campo do PlayState** (`isPracticeMode`, L387) que só muda uma
coisa — a morte:

```haxe
if (health <= Constants.HEALTH_MIN && !isPracticeMode && !isPlayerDying)   // L1255
```

A música continua começando do zero (não há seleção de seção nesta versão), a
vida **chega** a `HEALTH_MIN` e fica lá, e `applyScore()` continua pontuando e
contando tallies; o que o Practice invalida é o **highscore/medalha**
(`ResultState.hx` L1132, `isScoreValid`) — e o port não grava highscore.
Ou seja, o que faltava era pequeno e mensurável:

| Item | Original | Port |
|---|---|---|
| ligar/desligar | `PauseSubState.enablePracticeMode` (L1129) | quinta entrada da pausa |
| `filter` da entrada | `() -> !isPracticeMode` (L71) | a lista é montada por quadro: 5 entradas, 4 com o modo ligado |
| menu depois de ligar | `regenerateMenu()` — seleção volta ao primeiro item, a entrada some | idem, e a pausa **não** fecha |
| morte | `health <= HEALTH_MIN && !isPracticeMode` | `fnf_play_t.practice` — `dead` nunca vira 1 |
| vida | chega a 0 e fica | igual (`fnf_play` já saturava no mínimo) |
| metadados | `metadataPractice` = `'PRACTICE MODE'` (L512-516) | linha extra no canto direito da pausa |
| sobrevive ao reset | `resetPlaying()` no MESMO PlayState (needsReset) | `fnf_play_reset` preserva `practice` |
| some com um PlayState novo | `isPracticeMode = params.practiceMode ?? false` (L762) | zerado nas cinco entradas de música |

Medido (`tools/run_practice_harness.sh 5800`, música tocando **sem** roteiro de
notas de propósito — errar tudo é o que leva a vida ao chão): `pause_menu` com
**5** entradas, `pause_practice` com `dead = 0`, `pause_menu` com **4** depois,
`vida_zero` com `dead = 0`, **zero** `play_death`, 508 quadros de resultado e a
música chegando ao fim. E o caminho da morte continua provado no
`run_gameover_harness.sh`.

**Fechado: a música da pausa.** `startPauseMusic()`
(`PauseSubState.hx` L365-383) carrega `breakfast$musicSuffix` em **loop**,
começa num ponto **sorteado** de 0 até metade do arquivo
(`FlxG.random.int(0, length/2)`) e faz `fadeIn(5, 0, 0.75)`
(`MUSIC_FADE_IN_TIME = 5`, `MUSIC_FINAL_VOLUME = 0.75`, L127/L132). O sufixo é
o `musicSuffix` do personagem (coluna `pausa` de `src/gameover_dados.h`): os
`pico*` pedem `-pico`, e o `bf-pixel` pede `-pixel` — que **não existe** na
arte, o que no original cai no `trace('Could not play pause music')` e fica sem
som (o port reproduz isso; não inventa a versão sem sufixo). O port não tinha
som nenhum na pausa. Agora:

| Item | Original | Port |
|---|---|---|
| trilha | `breakfast$musicSuffix/breakfast$musicSuffix` | `assets/pause/breakfast{sufixo}.pcm` |
| loop | `FlxSound.load(..., true)` | `audio_ring_start_loop(fd, 1)` |
| início | `random(0, length/2)` | LCG semeado pelo nº de pausas, passo de 4 B (amostra estéreo) |
| fade | `fadeIn(5, 0, 0.75)` | ganho linear por amostra, 0 → 0,75 em 240.000 amostras |
| ao retomar | o substate fecha e o som morre com ele | `pause_audio_parar()` antes do `PLAY_RETOMAR_AUDIO` |
| sem arte | `trace` + silêncio | `pause_sem_musica` na telemetria |

Medido (`tools/run_pause_harness.sh 5600`): `pause_musica` com **384.000
bytes**, trecho sorteado em **146.184** (dentro da metade de 192.000, múltiplo
de 4), **99.328 amostras** tocadas antes do Resume e `pause_musica_fim` no
fechamento — com o relógio do nível ainda congelado em 194.400 e 29 de 29
acertos depois de voltar.

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

**Fase 3 fechada**. A ressalva de arte que existia aqui ("a Freeplay desenha a
lista e não as capsules") **saiu nesta build**: a lista agora são as cápsulas do
original, com o título renderizado por música — ver *A lista da Freeplay: as
cápsulas do original*. O menu de pausa continua sem ícone/círculo e sem o
`AtlasFont` do original, mas está medido que o `PauseSubState.hx` de v0.8.6 não
usa imagem nenhuma (ver a tabela de pendências).

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

Por que `songs/<id>/stems.pcm` e não o `Inst.pcm` do original: o arquivo do
port tem **as duas faixas em canais separados** — 8 bytes por amostra, na ordem
`instL, instR, voicesL, voicesR` — e o mixer soma as duas com saturação
(`amix_somar`, `src/engine/audio_mix.c`). É o mesmo resultado audível de dois
`FlxSound` somados pelo mixer do flixel: não há, no original, nenhum evento que
mude o volume de UMA faixa (medido: nenhum `SetVolume` nos 81 charts), então
dois arquivos não produziriam som diferente — só custariam mais E/S no USB 1.1.
O que a separação em canais dá é justamente o que o debug precisa: o R3 liga e
desliga as vozes sem tocar no instrumental, provando que as duas faixas estão
lá e separadas. Abrir um `Inst.pcm` de 4 bytes/amostra com este mixer leria 8 e
tocaria na metade da velocidade — por isso o formato é o do `stems.pcm` e a
conversão de áudio (fora deste repositório) tem de gravar 4 canais.

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

**Fase 2 — o mixer de duas faixas: FECHADO, e por medição.**

O que faltava não era somar duas faixas: era notar que o `stems.pcm` do port já
é o par `Inst`+`Voices` em canais separados. O arquivo entrega 8 bytes por
amostra — 4 canais de 16 bits, `instL, instR, voicesL, voicesR` — e o mixer
(`amix_somar`, em `src/engine/audio_mix.c`) soma `inst+voices` por canal com saturação em
±32767, que é o mesmo que o mixer do flixel faz com os dois `FlxSound` do
original. O bloco de saída tem metade do tamanho do bloco lido (`n/2`), porque
4 canais viram 2 — e era exatamente aí que uma conta errada dobrava a
velocidade do áudio (já corrigida e medida: `audio_submitted` chegava a
4.755.264 amostras num arquivo de 3.225.600).

A soma saiu do `main.c` para um módulo próprio (`engine/audio_mix.c`) porque
era ali que o bloco de saída saía com o dobro do tamanho — e um módulo com
teste (`test/test_audio_mix.c`) prova o que nenhum harness de desenho provaria:
soma por canal (não canal trocado), saturação em ±32767 sem inverter o sinal,
instrumental intacto com as vozes desligadas, metade dos bytes na saída
(4 canais → 2) e a mistura **in place**, que é como o `main.c` chama.

O que a divisão em canais acrescenta e o arquivo misturado não daria: o R3
liga/desliga as vozes durante a música (`voices_enabled`), deixando o
instrumental intacto — é o teste que mostra, no console, que as duas faixas
chegaram separadas. É um **controle do port**, não do original, e não muda
nenhum evento do jogo; fica documentado aqui para não ser confundido com bug.

O que **não** está provado: que a conversão de áudio do lado do PC grava os 4
canais na ordem `inst, inst, voices, voices` para todas as 27 músicas. O jogo
lê nessa ordem e a duração do `stems.pcm` conferida é 3.225.600 amostras × 8
bytes; se um arquivo vier em 2 canais, o port toca o dobro da velocidade e o
relógio da música sai junto. É verificável no console em uma música: R3 tem de
tirar as vozes e manter a batida.

## Personagem no tamanho certo: a escala do pacote e o foco da câmera

**O defeito.** O `ch_anchor` recebe o tamanho do quadro em unidades do
original. O desenho passou a ser o caminho único das 27 músicas, mas passava
`canvas_w/canvas_h` **do pacote** — e o pacote dos 46 personagens reais é
empacotado a **0,5** (`tools/achatar_tudo.sh`, `tools/ps2_display.py`: a tela do
port é um 0,5 exato do original, então cada textura só precisa de metade da
resolução para aparecer do tamanho que o original mostra). Duas consequências,
nenhuma delas um erro de compilação:

1. **personagem na metade do tamanho** — o sprite saía com `canvas x sc`
   (0,5 × zoom × bop) em vez de `canvas x 2 x sc`;
2. **foco de câmera meia-personagem fora do lugar** — o foco leva
   `- canvas_h/2`; com o canvas pela metade, o y do foco subia meia altura de
   canvas.

O original não tem escolha a fazer: `Stage.hx` L451-455 faz
`finalScale = getBaseScale() * stageCharData.scale` e o canvas já está na
resolução em que deve ser desenhado. A conta certa é uma só:

    escala_mundo = personagem.scale * slot.scale / escala_do_pacote

Com os 46 personagens reais (pacote 0,5, `scale` 1) isso dá **2**; no `senpai`
(pixel art, `CharacterData.scale` 6) dá **12**; no `bf`/`dad` do `phillyBlazin`
(`stage.characters.*.scale` = 1,75, medido em `assets-data/stages`) dá **3,5**.
Está em `ch_escala_mundo()` (`src/engine/char_layout.c`) com teste em
`test/test_char_layout.c` (2, 12, 3,5, pacote sem escala e zeros que não podem
zerar o desenho).

**Onde a escala mora.** O pacote CHP4 subiu para **v2**: o campo 32, que era
`reservado` e sempre 0, passa a guardar a escala **×10000**. Um pacote v1
continua abrindo (o leitor aceita 1 e 2) e o campo 32 do v1 é ignorado — lá ele
não significa nada. `cp_escala()` devolve 1,0 quando não há escala gravada, e o
jogo escreve `char_escala_v1` no log nesse caso: assumir 1,0 em silêncio é
exatamente o que produzia o personagem na metade.

**Quem grava a escala agora.** Até esta build nenhuma ferramenta do repositório
escrevia `.chp4` de personagem real — só o gerador sintético do harness. Os dois
caminhos passaram a gravar:

- `tools/pacote_personagem.py --chp4` (11 personagens de sparrow/packer):
  `pack_character_chp4()` escolhe os quadros **por animação** (`frameIndices`
  quando existe, senão todo quadro cujo nome começa com o `prefix`, na ordem do
  atlas — a regra do `FlxAnimationUtil.addAtlasAnimations`), monta um canvas
  único para o personagem (senão o `ch_anchor` mudaria de lugar a cada
  animação) e grava `escala`;
- `tools/achatar_animate.py --pack` (34 personagens de atlas Animate): aproveita
  que os quadros já são renderizados animação por animação e escreve o `.chp4`
  ao lado do `.cfz4`, com a tabela que a própria timeline declara.

Um quadro usado por duas animações entra duas vezes no pacote: gasta disco, não
VRAM (o jogo sobe um quadro por vez). `tools/chp4.montar_de_grupos()` é o único
lugar que calcula `primeiro/quantos`, para os dois caminhos não divergirem.

**Medido.** `tools/test_pack_chp4.py` (dentro da suíte, com um clone sintético
montado no próprio teste): as animações saem na ordem da JSON e cada uma
contígua, o quadro de cada animação é o **certo** (um quadro por cor, e a ordem
do atlas é embaralhada de propósito em relação à ordem das animações),
`frameIndices` manda quando existe, o quadro compartilhado entra duas vezes, a
escala 0,5 e 1,0 voltam idênticas do campo 32, o `budget` reduz até caber e o
que ele reporta é o que o pacote tem, e prefixo que não existe no atlas vira
registro em `faltando` (reportado, não tapado). `tools/check_char_packs.py`
passou a **falhar** em pacote sem escala. `tools/achatar_tudo.sh` agora exige
`<id>.cfz4` **e** `<id>.chp4` por personagem e conta os `.chp4` no fim.

**No jogo.** `char_tamanho` (papel, largura na tela) e `char_escala`
(papel, escala_mundo ×100) vão para o log uma vez por papel e por música.
Medido no harness: `tutorial` → papéis 1 e 2 com `char_escala,1,100` (pacote
sintético a 1,0); `blazin` → `char_escala,*,100` na GF e `char_escala,*,175` no
BF e no oponente, que é o `slot.scale` 1,75 do `phillyBlazin`.

**A câmera.** O foco passou a usar o **mesmo** tamanho do desenho
(`cena_canvas_mundo()`), porque o `- canvas_h/2` do `ch_anchor` não pode valer 0.
Medido no harness, Tutorial: o primeiro foco é `cam_foco_alvo,888,803` — com
canvas 0x0 saía 835, ou seja meia altura de canvas abaixo. No original a conta
é a mesma: `BaseCharacter.resetCameraFocusPoint` usa
`originalPosition + cameraOffsets` e o `cameraFocusPoint` do sprite já carrega
a metade da altura.

**E a câmera começa no oponente.** `PlayState.hx` L2224-2227 põe o
`cameraFollowPoint` no `dad` logo depois de o adicionar, e o L936 chama
`resetCamera()` na inicialização, que **snapa** a câmera nesse ponto. O port
começava no BF e só ia para o oponente no primeiro evento. Agora o papel inicial
sai da cena (oponente; sem oponente, GF; sem GF, BF) e o log registra
`cam_foco_inicial,<papel>,<y do foco>` — medido: papel **2** (`CH_PAPEL_DAD`,
o oponente) e o y do foco do oponente em cada música (`tutorial` 765,
`bopeebo` 762, `spaghetti` 738, `blazin` 747 — cada um é `slot.y + oy -
canvas_h/2 + cameraOffsets`). O sweep cobra papel 2 e y > 0 nas 81 combinações.

Uma armadilha medida no caminho: a primeira versão nascia no **primeiro quadro
do gameplay**, quando os pacotes de personagem ainda não estavam carregados —
saía `cam_foco_inicial,1,589`, com 589 sendo o valor fixo antigo do mainStage, e
o papel escolhido BF porque os três papéis ainda estavam inativos. O nascimento
agora espera `g_cena_estado != 0` (a cena carregada); nos quadros anteriores o
ponto calculado continua servindo para desenhar.

**Pendrive.** `tools/build_pendrive.sh` nunca copiava `build/packs/chars` nem
`build/packs/stages`, embora o jogo abra `mass:/FNFPS2/assets/chars/<id>.chp4`
e `mass:/FNFPS2/assets/stages/<palco>/<prop>.cfz4`. O pendrive saía sem
personagem nenhum. Agora copia os dois, confere os **46** ids de
`assets-data/characters` e confere prop por prop contra as strings
`stages/<palco>/<prop>.cfz4` de `src/stage_dados.h` (faltou um, o script para),
e o LEIA-ME lista `assets/chars/` e `assets/stages/` como obrigatórios.

**O que NÃO está provado:** que os 46 pacotes reais caibam em VRAM (isso é
`tools/vram_budget.py` + `test_stage_pool.c`, sobre os pacotes reais), que o
console desenhe o `.chp4` de verdade (`char_tamanho`/`char_escala` são o número
que se confere na captura) e que o `.chp4` do caminho Animate saia com as
animações certas nos 34 personagens — ele foi exercitado só no caminho
sparrow/packer com arte sintética; os 34 dependem do clone da arte.

## Duração ausente em ZoomCamera/ScrollSpeed: 4 passos, não "instantâneo"

**O defeito.** `ZoomCameraSongEvent` e `ScrollSpeedSongEvent` declaram
`var duration:Float = 4.0` e só fazem tween quando `duration > 0`. O conversor
gravava `duration` ausente como 0, e o port lia 0 como "muda no mesmo quadro".
Ou seja: **ausente virava instantâneo**, o oposto do original.

**A correção.** O campo `b` do evento ganhou o bit 24 (`B_DUR_ABSENT` no
`tools/fnf_to_fch3.py`, `CHART_B_DUR_ABSENT` em `src/chart_events.h`). Ele diz
exatamente o que o chart disse — não grava um valor que o chart não tinha — e o
port aplica o padrão da classe (`CHART_EVENT_DEFAULT_DURATION` 4,0 passos), que
passa por `steps_to_seconds()` com o `stepLengthMs` da seção. Um `duration: 0`
explícito continua significando "no mesmo quadro", e um tween de duração zero em
`FocusCamera` agora vira `INSTANT` (um `FlxTween` de 0 leva a câmera ao ponto no
mesmo quadro, não fica pendurado para sempre).

**Medido, e o resultado é desconfortável de propósito:** nos 81 charts isto muda
**zero** eventos hoje. 520 eventos `FocusCamera` não têm `duration` e todos são
CLASSIC (onde a duração não é usada); dos 207 `ZoomCamera`, exatamente **um** não
tem `duration` (`2hot`, e é `INSTANT`); `ScrollSpeed` tem um único evento, e ele
traz `duration`. Então a prova não pode vir do sweep — vem do teste:
`test/test_chart_events.c` monta os dois casos de `ZoomCamera` (sem duração →
`zoom_dur_s` 0,6 s e `zoom_t < 1`, ou seja, interpola; com `duration: 0` →
`zoom_dur_s` 0 e `zoom_t == 1`, ou seja, salta) e o mesmo para `ScrollSpeed`.
São 120 conferências, 0 falhas, no lugar de 114.

**E a armadilha que apareceu no caminho.** `tools/preparar_sweep.py` tinha
invalidação por mtime do conversor, mas ela rodava **depois** de um
`return 0` para "o chart de destino já está pronto" — ou seja, com o destino no
lugar ela nunca rodava. Um chart gerado pelo conversor antigo era reaproveitado
para sempre: o teste do módulo via o bit novo e o harness não. A invalidação
passou para antes do atalho, e o atalho agora exige que o chart em cache exista.

## Pacote de palco lido quadro a quadro: a RAM deixa de ser o teto

**A dívida medida.** O `main.c` lia cada `.cfz4` **inteiro** para a RAM principal
e o mantinha lá, com um teto de 4 MB para todos os props juntos. Com a arte real,
quatro palcos não caberiam nem com o jogo vazio — `spookyMansion` tem **59,4 MB**
de pacote num console de **32 MiB** — e oito ficavam no limite do teto. Isso
estava documentado e era o item 2 da Fase 6.

**O que já existia no formato e ninguém usava.** O CFZ4 guarda uma **tabela**
`(offset, tamanho)` por quadro, e cada quadro é um bloco LZ4 **independente**.
Ou seja: não é preciso o arquivo inteiro para desenhar um quadro — é preciso a
tabela e o bloco daquele quadro.

**O módulo** `src/engine/pack_stream.{h,c}`: `pst_abrir()` lê cabeçalho + tabela
e mantém o **descritor do arquivo** aberto; `pst_quadro()` faz `lseek` +
`read` de um bloco e descomprime no destino; `pst_fechar()` devolve tudo. Todo
erro é negativo e **nomeado** (`pst_erro()`), e a tabela é conferida inteira na
abertura — entrada fora do arquivo é recusada ali, não na tela.

**Medido** (`test/test_pack_stream.c`, dentro da suíte): arquivo de **66.384 B**
aberto custa **2.053 B** de RAM (tabela + o maior bloco + os vetores), — 3 % do
arquivo; cada quadro sai byte a byte igual ao empacotado, fora de ordem e
repetido; e oito arquivos estragados são recusados com erro nomeado (magic,
versão, contagem zero, bytes por quadro absurdo, tabela fora do arquivo, bloco
fora do arquivo, LZ4 que não fecha no tamanho, e leitura de pacote já fechado).
Um bloco ruim **não contamina** o resto do pacote (o quadro seguinte continua
saindo certo).

**No jogo.** `g_palco.pack[]` deixou de existir: o vetor de pacotes virou um
vetor de `pst_pack_t`, e `palco_prop_textura()` chama `pst_quadro()`. O teto
`PALCO_RAM_MAX` (4 MB) continua como rede de segurança, agora contando
**tabelas**. Um detalhe que só aparece no console: o vetor é estático, então na
primeira `palco_soltar()` os `fd` valem **0** — e 0 é a entrada padrão, não
"fechado". O primeiro palco fechava o `stdin` do processo. Agora a primeira
passagem marca `-1` em vez de fechar.

**Efeito no orçamento, medido por `tools/check_stage_budget.py --strict`:**

| pergunta | antes | agora |
|---|---|---|
| RAM (pacote inteiro de cada palco) | 4 palcos estouravam (até 59,4 MB) | **21 de 21** cabem (pior: `spookyMansion` 1.696.842 B, tabela + 1 bloco) |
| VRAM (um quadro por prop) | 15 de 21 "acima" | **21 de 21** cabem (pior: 1.697.934 B de 2.056.192 B) |

A segunda linha mudou por um motivo diferente e vale registrar: o script
comparava contra um **modelo** de VRAM (1.337.520 B) que o jogo não usa. O
orçamento real é o pool que o `main.c` monta — **2.056.192 B**, o mesmo número
que `test/test_stage_pool.c` já provava. Reportar 15 palcos "acima" contra um
número que ninguém usa é medir a coisa errada; agora o script usa o `vram_livre`
do log do próprio jogo (e cai no modelo só quando não há log). Com isso
`test/run_tests.sh` passou a rodar o script em `--strict`: deixou de ser relatório
e virou porta.

**O que NÃO está provado:** (1) que a leitura de blocos do pendrive USB 1.1
(~1,5 MB/s, blocos de 8 KB) chega a tempo — um bloco de prop tem ordem de 5 a
40 KB, ou seja 3 a 27 ms, e isso só o console mede; (2) que abrir 157
descritores (um por prop, 21 palcos) não esbarra em limite do `fileXio` — nos
palcos maiores são 17 props abertos ao mesmo tempo. As duas são medíveis no log:
`palco_aberto` traz o custo em RAM por prop, e uma falha de `open` viraria
`palco_pacote` com `PST_ERR_ABRIR`.

## O zoom e o scroll com que a música começa vêm do dado

**O defeito.** `chart_events_reset()` recebia dois números fixos:
`g_official_mainstage_camera_zoom` (1,10, o zoom do `mainStage`) e **1,0** de
scroll. Os dois são do **Tutorial**, aplicados às 81 combinações música ×
dificuldade.

Isso não é um detalhe de enquadramento: `sc = 0,5 × zoom × bop` é o tamanho de
**tudo** que está em cena — sprites, props, e o próprio mapeamento da câmera
para a tela. E o scroll é a velocidade com que as notas descem.

**De onde os valores deviam vir.**

| valor | fonte | medido |
|---|---|---|
| zoom inicial | `stageData.cameraZoom` do palco da música (`bs_stages[].zoom`, gerado de `assets-data/stages/*.json`) | 8 valores distintos, de **0,50** (`sserafim`) a **1,10** (`mainStage`) |
| scroll | `scrollSpeed` do **chart** daquela dificuldade (`assets-data/charts/<id>-chart.json`) — é de lá que `Song.loadFromJson` lê, não do metadata | **20 valores** distintos, de **1,0** (`tutorial`) a **3,1** |

**A correção.** `sr_song_t` ganhou `float scroll[3]` (easy/normal/hard) gerado por
`tools/gen_song_registry.py` a partir do chart, com `sr_scroll(song, diff)` para
ler (fora de 0..2 cai em *normal*, que é o que o original faz quando a
dificuldade pedida não existe — nunca um 1,0 inventado). Se um chart não trouxer
`scrollSpeed`, o gerador **falha** em vez de emitir 1,0: rolar notas numa
velocidade que o dado não declara é invenção. E `cena_zoom_do_palco()` procura o
palco da música em `bs_stages[]`; palco desconhecido cai no zoom do `mainStage` e
o caso é o que o `sweep` cobra (não acontece nos 21).

**Medido no harness** (evento `cena_inicio`, zoom e scroll em milésimos):

| música | dificuldade | `cena_inicio` | antes |
|---|---|---|---|
| `bopeebo` | hard | `1100,1600` | `1100,1000` |
| `spaghetti` (palco `sserafim`, zoom 0,50) | easy | `500,2200` | `1100,1000` |
| `blazin` | easy | `900,1700` | `1100,1000` |

E no sweep das 81: **7 zooms e 19 scrolls distintos** (o sweep cobra ≥ 5 de cada
— se tudo voltasse ao valor fixo daria 1 e 1, e a checagem falharia).

**O Tutorial não muda:** o palco dele é o `mainStage` (zoom 1,10) e o chart dele
diz `scrollSpeed 1.0` nas três dificuldades. As capturas de console que fixaram a
geometria do Tutorial continuam valendo — e é por isso que a troca pôde ser
medida sem medo de derrubar o que já estava conferido no hardware.

**O que NÃO está provado:** que o zoom do palco seja aplicado em todos os lugares
onde o original o usa. O port usa um `sc` global para o mundo inteiro; o original
tem `defaultCamZoom` (aplicado por `resetCamera`/`resetCameraZoom`) e
`camera.zoom` (que os eventos `ZoomCamera` interpolam). Nos eventos do tipo
*stage* (`ZoomCamera` com `mode:'stage'`, que multiplica o zoom do palco) o port
resolve contra `zoom_from`, que agora começa no zoom certo — mas o `resetCameraZoom`
de fim de evento (voltar ao `defaultCamZoom`) não existe no port. Nenhum dos 207
eventos `ZoomCamera` dos 81 charts pede isso hoje: todos terminam em `bop`/valor
absoluto explícito. Fica registrado como a próxima diferença a medir, não como
pendência de dado.

## O espelho dos personagens: `CharacterData.flipX` era dado ignorado

**O defeito.** `bs_dados`/`char_dados` já geravam `flipx` desde o começo, e
**nenhuma linha do desenho o lia**. Quatorze dos 46 personagens têm
`"flipX": true` — **todos os `bf`** (`bf`, `bf-car`, `bf-christmas`, `bf-dark`,
`bf-holding-gf`, `bf-pixel`), **todos os `pico`** e o `sserafim-sakura`.

O original aplica em `BaseCharacter.hx` L192 (`this.flipX = _data.flipX`), e no
flixel isso espelha a arte **dentro do próprio quadro**. Sem isso, o BF — que
fica no lado direito da tela — aparecia olhando para a direita, de costas para o
oponente, em **todas** as músicas.

**A correção.** `sprite_dims_flip()` no `main.c` é o `sprite_dims` com as duas
coordenadas U trocadas; a posição na tela e o recorte vertical não mudam (é o que
o flixel faz). A conta em si mora em `ch_uv_do_quadro()` (`char_layout.c`), com o
meio texel de recuo em cada ponta — o mesmo que o caminho sem espelho já usava —
porque é uma conta de três números que erra em silêncio: sprite virado para o
lado errado não gera erro nenhum, só um jogo esquisito.

**Medido.** `test/test_char_layout.c`: sem espelho `u0=0,5`, `u1=319,5`; com
espelho `u0=319,5`, `u1=0,5`; largura inválida não vira coordenada negativa.
No harness, evento `char_flip` (papel, flipX) uma vez por papel por música:
`bopeebo` → `char_flip,0,0` (GF), `char_flip,1,1` (BF, espelhado),
`char_flip,2,0` (oponente). No sweep das 81: **162 papéis espelhados e 78 sem**;
o sweep cobra ≥ 1 espelhado por combinação e ≥ metade sem — as duas direções,
porque um espelho aplicado a todos seria tão errado quanto nenhum.

**O que NÃO está provado:** que a arte dos 14 esteja desenhada de modo que o
espelho a ponha virada para o lado certo. Isso depende do atlas real (o clone da
arte não está aqui). O que o port garante é que faz **o que o dado diz** —
`flipX` do `CharacterData` — e que o número aparece no log por papel, para a
captura de console confirmar em uma música.

## O passo do chart segue a seção de BPM, não o BPM do começo

**O defeito.** `g_play_samples_per_step` era calculado **uma vez**, no início da
música, com `chart_samples_per_step(0)`. Tudo que anda por **passo** — dança
ociosa, props do palco, bop da câmera, duração do canto e das animações — usava
esse número para a música inteira. Em 26 das 27 músicas isso é exato (o BPM não
muda). Em **`monster`** são **14 mudanças de tempo**, de **95 a 132 bpm**: depois
da primeira, o passo ficava 10 % a 30 % errado, e o erro se acumula.

**A correção.** Dois leitores, `cena_spv()` e `cena_passo()`, que olham a tabela
de BPM do chart na posição **atual** (`chart_samples_per_step()` e
`chart_step_at()`, que já existiam e já eram testados, mas o `main.c` só os
chamava na amostra 0). Eles passaram a alimentar:

- o passo e a duração da batida usados pelo `sl_collect` dos props (`bt`, `bts`);
- a batida dos personagens (`bt2 = passo/4`, `STEPS_PER_BEAT`);
- a duração do canto: `bpm = 720000 / samples_por_passo` (48000×60/(bpm×4)), na
  seção vigente em vez do `song->bpm` do começo;
- `chart_events_init()`, que converte **duração de evento em passos para
  segundos** — era inicializado uma vez com o passo do começo.

**Medido.** `test/test_real_chart.c` roda agora em **todos** os 81 charts (a
suíte já fazia isso) e, para os que têm mais de uma seção, exige: o passo do
primeiro e do último trecho são diferentes (95 bpm → 7578 amostras; 120 bpm →
6000), o passo 0 está na amostra 0, e no meio da música `chart_step_at()` **não**
é igual a `amostra / passo_do_começo` — ou seja, o número é integrado sobre a
tabela, não derivado de um passo fixo. Só `monster` entra nesse bloco (é a única
com mais de uma seção), e ele passa nas três dificuldades.

**O que NÃO está provado:** que 14 mudanças de BPM em `monster` cheguem ao fim da
música sem desvio no console. O relógio de áudio é contado em amostras
submetidas (`g_title_audio_submitted − audsrv_queued()`), então a posição é a do
áudio, não um acumulador de passos — mas isso só uma passada completa do
`monster` no hardware mostra. No harness o sweep toca 20 s de cada uma das 81
combinações, o que cobre a primeira mudança de `monster` (aos ~28 s fica fora do
trecho).

## A dança ociosa alterna por danças jogadas, não pela paridade do passo

**O defeito.** O lado do `danceLeft`/`danceRight` saía da paridade da batida
(`batida & 1`). No original o que conta é `hasDanced`, o contador de **danças
jogadas** (`Bopper.hx` L185-200), e `BaseCharacter.dance()` (L479-490) sai fora
**antes** de mexer nele quando o personagem está cantando:

```haxe
if (!force) {
  if (isSinging()) return;                 // nao toca dance nem hasDanced
  ...
}
if (shouldAlternate) { playAnimation(hasDanced ? 'danceRight' : 'danceLeft'); hasDanced = !hasDanced; }
```

Ou seja: uma batida cantada **não** vira o lado. O port virava — e como o passo
de dança é consumido de qualquer jeito (o `onStepHit` não volta), a partir da
primeira nota cantada o personagem dançava no pé trocado até o fim da música.

**Medido:** no sweep das 81 combinações, **135 de 243 papéis** (três por música,
menos os vazios) perdem pelo menos um passo de dança por estar cantando — o caso
não é teórico, é a maioria. O evento `char_danca_pulada` dá esse número no log do
console, e o `run_sweep_harness.sh` passa a exigir ≥ 1.

**O segundo defeito, no mesmo lugar: `danceEvery`.** O `dance()` só é chamado nos
passos que satisfazem `event.step % (danceEvery * STEPS_PER_BEAT) == 0`
(`Bopper.hx` L166). O port dançava em **toda** batida. `danceEvery` **não está em
nenhum dos 46 JSON** de personagem; o `CharacterData.hx` (L453, L588-590)
preenche `DEFAULT_DANCEEVERY = 1.0`, que dá exatamente uma dança por batida —
logo o comportamento de hoje coincide, mas por coincidência, e não por dado.
`ch_dados_t` ganhou `dance` (gerado por `tools/gen_char_dados.py`, default 1.0) e
`cc_quadro` passou a receber o **passo** do chart em vez da batida.

**O que mudou no código.**
- `ca_passo_de_danca(dance_every, passo)` — a condição do `Bopper.hx` L166, em
  float, aceitando `danceEvery` fracionário (0,5 = duas vezes por batida) e
  desligando quando `danceEvery == 0` (é o caso de 132 dos 157 props; entre os
  personagens todos são 1,0).
- `ca_danca()` / `anim_ocioso(p, lado)` — o lado é `dancas & 1`, e `dancas` só
  avança quando a dança é **jogada** (inclusive a dança **forçada** do fim do
  canto, `dance(true)`, e a volta de uma animação nomeada).
- `cc_quadro(..., passo)` e `cc_danca_cada()`; `main.c` passa `cena_passo()`.

**Provas.** `test/test_char_cena.c` cobre: `danceEvery` 2,0 (dança nos passos 0, 8,
16 e **não** no 4), `danceEvery` 0 desligando a dança, e a sequência completa
L → (dois passos perdidos cantando) → R forçado no fim do canto → L no passo
seguinte, que é o oposto do que a paridade daria. `test/test_char_anim.c` cobre a
condição de passo (incluindo passo negativo, que na contagem regressiva dança nos
múltiplos de 4, como no `%` do Haxe) e confere que `ca_reset` copia o `dance` dos
46. `tools/gen_char_dados.py --check` continua verde.

## O combo e o drop da namorada (`combo50`, `combo200`, `drop70`)

**O defeito.** Oito dos 46 personagens têm animações de combo e de queda nos
dados — e o port não tocava nenhuma delas, nunca. Eram eventos do original
simplesmente ausentes:

| personagem | combo | drop |
|---|---|---|
| `gf`, `gf-christmas`, `gf-dark` | `combo50` | `drop70` |
| `nene`, `nene-christmas`, `nene-dark`, `nene-tankmen` | `combo50`, `combo200` | `drop70` |
| `gf-tankmen` | — | `drop70` |

**A regra, lida da fonte.** `BaseCharacter.hx` L586-600 (`onNoteHit`, só para
`characterType == GF` e só em nota do jogador):

```haxe
switch (event.judgement) {
  case 'sick' | 'good': playComboAnimation(event.comboCount);
  default:              playComboDropAnimation(event.comboCount);
}
```

e `L614-626` (`onNoteMiss`, GF, nota do jogador) → `playComboDropAnimation`.
O `comboCount` do evento é `note.scoreable ? tallies.combo + 1 : tallies.combo`
(`PlayState.hx` L3142-3143), ou seja **o combo já contando esta nota**; no miss é
`Highscore.tallies.combo` **antes** de zerar (L2903).

`playComboAnimation` (L651-659) só toca `combo${comboCount}` **exato** — o
`combo50` não vale no 51. `playComboDropAnimation` (L661-681) percorre os
`drop<N>` do personagem e usa o **maior** `N` com `comboCount >= N`; abaixo de
todos, não toca nada.

**O que foi feito.** `cc_combo()`/`cc_drop()` em `char_cena.c` —
`cc_combo` monta `combo<combo>` e só toca se o pacote tiver essa animação;
`cc_drop` varre os nomes `drop<N>` do próprio pacote (nada de tabela no código) e
escolhe o maior degrau que o combo alcança. As duas usam `cc_toca`, que já
implementa o "não é interrompida e ao terminar volta a dançar"
(`onAnimationFinished` → `dance(true)`). `main.c` chama:

- acerto `sick`/`good` → `cc_combo(g_play.combo)`, evento `char_combo`;
- acerto `bad`/`shit` → `cc_drop(g_play.combo)`, evento `char_combo_drop`;
- miss → `cc_drop(combo_antes)`, com o combo lido antes de `fnf_play_miss()`.

**Medido no jogo de verdade** (`tools/run_combo_harness.sh`, novo): `2hot` hard,
stem de 70 s, pressões vindas do chart com 8 notas de fora para o combo quebrar.
A execução julga 96 acertos e o log traz, nesta ordem:

```
char_combo_anims,2,1      (a GF: combo50, combo200, drop70 -- vem do dado)
char_combo,50,4           (combo 50, julgamento 4 = sick)
char_combo_drop,73,0      (combo 73 quebrado, julgamento 0 = miss -> drop70)
```

O script também exige o contrário: **nenhum** `char_combo` num número que não
seja degrau. O sweep das 81 combinações continua com 0 combos: 20 s de música não
alcançam 50 acertos seguidos, e é por isso que o `combo` precisa de passada
própria.

**Limite do que está provado.** `combo200` não foi observado tocando (precisa de
200 acertos seguidos; a passada chega a ~96). A regra é a mesma função que tocou
o `combo50`, e `cc_combo` é testado no 200 em `test/test_char_cena.c`, mas no
console o `combo200` só aparecerá numa passada longa de uma música densa.

## Fase 5: o custo por quadro, medido (linha de base)

**O que faltava.** A Fase 5 diz "medir antes e depois de cada mudança". Não havia
*denominador*: o harness conta uploads, mas "1.149 uploads numa música" não compara
com nada. Agora o jogo emite, **por música**, três números acumulados:

| evento | campos |
|---|---|
| `custo_char` | uploads de quadro de personagem, e o mesmo × 100 / quadros de música |
| `custo_palco` | uploads de quadro de prop do palco, e × 100 / quadros |
| `custo_quadros` | quadros desenhados **com música tocando**, e ms de música |

A amostra sai a cada 240 quadros de música **e** no fim (ou na saída da cena), com
os números acumulados: a última amostra é o total. A amostra periódica existe
porque só 43 das 81 combinações chegavam ao fim do áudio dentro da passada — as
outras 38 ficavam **sem medida nenhuma**, e um teste que mede metade não mede.

**Medido nas 81 combinações** (`build/sweep/custo.txt`, resumo no fim do sweep):

| | valor |
|---|---|
| combinações medidas | **78 / 81** |
| uploads de personagem por quadro (média) | **1,19** |
| mínimo | **0,79** (`tutorial`: 2 personagens em cena: não tem namorada) |
| máximo | **1,23** (`eggnog`) |
| uploads de prop do palco | 18.571 no total (≈ 238 por música de 20 s) |
| quadros de música somados | 44.323 |

**O que este número significa.** Três personagens a 24 fps são 72 quadros/s; a 60
quadros/s de tela isso dá **1,2 upload por quadro**. O medido é 1,19 — ou seja o
caminho já está no piso: um quadro só sobe quando o **índice do quadro mudou**
(`cc_quadro` devolve -1 quando não mudou), e não há reenvio de quadro repetido. O
`tutorial`, com dois personagens, mede 0,79 ≈ 2/3 de 1,19, como esperado.

**O que sobra como alavanca** (e por que não foi feito): reduzir **bytes** por
upload, não o número deles — o pacote guarda o canvas inteiro do personagem
(≈ 320×256×2 B = 160 KB por quadro nos maiores), e a arte ocupa uma fração dele.
Isso exige recortar o quadro no **empacotador** (formato de pacote novo), ou seja é
mudança de dado, não de jogo: o port já desenha o quadro recortado, mas sobe o
canvas todo. Fica registrado como a próxima alavanca medida possível, com o
número de partida acima.

**O que NÃO está provado:** o custo em tempo de CPU/GS no console. O harness mede
*quantas* vezes cada caminho roda, não *quanto* ele custa em ciclos.

## Os 34 NOTE KINDS: o dado que faltava estava no clone da arte

**O erro que esteve escrito no documento por várias builds.** "32 dos 34 kinds
são scripts que não estão no pacote de dados; não há o que portar sem eles."
Falso: os scripts estão em `preload/scripts/` do clone da arte, e o mesmo vale
para os 46 `.hxc` de personagem. O port implementava só os dois kinds de
**código** (`noanim`, `non_scoreable`) e tratava o resto como dado ausente.

**Agora o dado é lido, não pedido** (`tools/gen_notekinds.py`):

| peça | o que é |
|---|---|
| `preload/scripts/notekinds/*.hxc` | **35** classes, cada uma `super('id', 'Nome de exibição')` — a lista fechada dos kinds de script |
| `preload/scripts/characters/*.hxc` | o que cada kind FAZ, no `switch (event.note.kind)` do `onNoteHit`: **68 associações em 10 personagens** |
| `assets-data/notekinds.json` | o dado, para diff e conferência |
| `src/engine/notekind_dados.h` | o mesmo dado em C, gerado (não editado à mão) |
| `src/engine/notekind.{h,c}` | a busca: `nk_acao(personagem, kind)`, `nk_nome(kind)` |

**As duas formas de ação, lidas do script — nenhuma inventada:**

- **canto com sufixo**: `this.playSingAnimation(dir, false, 'alt')` →
  `sing<DIR>-alt`. É o `mom` das pais de Natal (43 notas em `cocoa` hard) e o
  `sakura-joint`/`bf1`/`bf2` da Sakura (92 notas em `spaghetti` hard).
- **animação nomeada**: o `case` chama uma função que faz `playAnimation('X')` →
  o port toca `X` pelo `cc_toca` (especial: segura o personagem até acabar e
  depois ele volta a dançar). É o `weekend-1-*` dos Blazin' (214 notas em
  `blazin` hard) e o `ugh`/`hehPrettyGood` do Tankman.

**O detalhe que custou 110 animações "que a arte não tem".** Os kinds
`weekend-1-punchhigh`/`punchlow` não pedem um nome escrito: o script faz
`'punchHigh' + postfix`, e `postfix` vem de `doAlternate()`, que alterna
`'1'`/`'2'` a cada chamada. Os dois nomes **existem** nos dados
(`punchHigh1`/`punchHigh2`). A primeira leitura do gerador pegava só o prefixo
`punchHigh`, o jogo não achava a animação e registrava `nota_kind_sem_anim`.

Medido, uma passada de 8.000 quadros por música no harness (FCH5, que é o
contêiner que carrega o `k` por nota):

| música | notas com kind | antes do conserto da alternância | depois |
|---|---|---|---|
| `blazin` hard | 440 | 104 animações + **110 sem animação** | **214 animações, 0 sem** |
| `cocoa` hard | 137 | 43 cantos com sufixo | 43 (igual) |
| `spaghetti` hard | 472 | 92 cantos com sufixo | 92 (igual) |
| `ugh` hard | 4 | 3 animações | 3 (igual) |

`stress` hard tem **1** nota com kind (`hehPrettyGood`, aos 62 s) e ela não foi
alcançada nessa passada: o relógio da música pula para o fim por volta dos 51 s
nesse asset sintético (os stems do harness são sintéticos). Fica registrado como
"não observado", não como "não funciona" — o caminho geral está provado nas
outras quatro.

**O que o dado marca e o jogo NÃO aplica (reportado, não adivinhado):**

| campo do script | no dado | no port |
|---|---|---|
| `condicional` (vida baixa, sorteio de 30%, `cantUppercut`) | marcado | **não** escolhe sozinho qual caminho |
| `ordem` (`moveToBack`/`moveToFront`) | marcado | **não** aplicado (a ordem de desenho dos 3 papéis é fixa) |
| `shake` (`camGame.shake(0.0025, 0.15)`) | marcado (força ×1000, ms) | **não** aplicado: o port não tem tremor de câmera |

Os três são eventos do original; ficam no dado para não serem reinventados
quando o tremor de câmera entrar.

**Teste:** `test/test_notekind.c` (dentro de `test/run_tests.sh`) prende os casos
reais (`mom`→`alt`, `ugh`→`ugh`, `sakura-joint` sem classe NoteKind), o que NÃO
existe (personagem que não implementa, kind desconhecido, ponteiro nulo), o canto
com sufixo recusando quando a animação não existe (sem cair no canto normal), e
a animação nomeada segurando o personagem até acabar. E
`gen_notekinds.py --check` falha se um kind usado nos 81 charts não tiver nem
registro nem ação — foi assim que `sakura-*` (3 kinds sem classe, resolvidos só
pela ação do personagem) apareceu em vez de passar calado.

## Empacotador de personagem: os 46 de verdade (e os 10 com mais de um atlas)

O `tools/achatar_tudo.sh` reportava **12 pacotes e 34 falhas**. As 34 eram o
mesmo `NameError`, num nome que não existia no escopo na hora de montar a tabela
de animações do CHP4 — e os 12 que passavam são os de PNG plano, que nem entram
nesse caminho. Por isso nenhum teste pegava: o teste do formato usa quadros
crus, não o empacotador.

Consertado o erro, apareceu o segundo defeito, mais grave: **o pacote sairia
inútil**. Os grupos levavam os nomes da timeline do Atlas (`Idle`, `Up`,
`Cheer`); o jogo pede os nomes do `character.json` (`danceLeft`, `singUP`,
`drop70`). O `prefix` de cada animação do JSON **é** o nome na timeline e os
`frameIndices` são índices *dentro* dela — é assim que `danceLeft`/`danceRight`
partem o mesmo `Idle` em duas metades. Agora o pacote é montado a partir do
JSON, na ordem do JSON.

E o terceiro, que só apareceu com os pacotes na mão (`check_char_packs.py`):
**10 dos 46 personagens declaram, por animação, um `assetPath` próprio.**
`bf` tem `bf-death`/`bfFakeOut`, `tankman` tem `extra-animations`
(`ugh`, `pretty good`, `laugh`, `beat it`, `argh`). Lendo só o atlas do corpo,
esses pacotes saíam sem as animações — inclusive sem `ugh`/`hehPrettyGood`, que
são justamente os NOTE KINDS do Tankman. Agora o empacotador abre cada atlas
declarado, sob demanda.

| personagem | antes | depois |
|---|---|---|
| `bf-car` | 316 quadros, 15 animações | **442 quadros, 18 animações** |
| `tankman` | 5 animações (sem `ugh`) | **10 animações, com `ugh`/`pretty good`** |

O que fica reportado e **não** aplicado: essas animações vêm de outro atlas e o
JSON dá `offsets` próprios para elas (`ugh`: `[5, -40]`). O pacote guarda os
quadros, mas o desenho ainda usa os offsets do *personagem* — aplicar os do
JSON por animação é o passo seguinte, e o empacotador imprime a lista
(`de outro atlas (N): ...`) em vez de deixar passar.

Custo medido do empacotamento: ~12 min por personagem (decodificar o atlas
inteiro, compor cada quadro e comprimir em LZ4 bloco a bloco), 46 personagens,
~250 MB de pacotes. Por isso `FORCAR=1` reempacota o que já existe: sem isso uma
correção no empacotador nunca chegava aos pacotes antigos.

### O sweep passou a cobrar o kind APLICADO, não só carregado

O sweep já conferia que o `k` do JSON chegava ao runtime (`chart_notas_com_kind`
= número do JSON, no contêiner v5). Faltava a segunda metade: **o kind
acontecer**. Um kind que chegasse ao jogo e não fizesse nada passaria com o
teste verde — que é exatamente o estado em que ele ficou por 32 builds.

Agora `build/sweep/kinds.txt` ganhou a coluna `KIND_JOGAVEL`: quantas notas com
kind caem nos primeiros **6 s** e têm ação no personagem que está em cena
(`assets-data/notekinds.json` × `playData` da música). No passe v5, se
`KIND_JOGAVEL ≥ 1` e o log não tem nenhum `nota_kind_sing`/`nota_kind_anim`/
`nota_kind_sem_anim`, a combinação **falha**.

O 6 s não é escolha: é a medição da própria passada. 1.200 quadros chegam a
~10 s de música (o resto é boot, título e contagem), então uma janela de 15 s
fazia o `2hot` falhar por uma nota que a passada nem alcança — a primeira nota
de kind dele está em 8,9 s. Com 6 s sobram `blazin` (24 notas jogáveis) e `ugh`
(1), e o `2hot` fica de fora por não ser alcançável, não por estar errado.

Medido no passe v5 completo: **81/81 combinações, 42 aplicações de kind**, e o
resumo do sweep passou a imprimir esse número (`note kinds aplicados pelo jogo:
42 nota(s) em toda a passada`).

## Os 21 palcos reempacotados da arte de verdade (o passo que faltava)

O documento dizia "falta reempacotar, que precisa do clone de 2,5 GB da arte e
por isso não roda aqui". O clone existe e agora foi usado: **os 21 palcos, 157
props, foram empacotados da arte real**, não do plano.

O que mudou de fato é a **escala de cada palco**, e ela caiu de ~0,31 para ~0,28
nos palcos que batem no teto de VRAM. O motivo é medido, não gosto: o
empacotador roda **os dois orçamentos** (VRAM e RAM) e fica com o menor deles.
Com o orçamento de VRAM de 1.337.520 B (`4 MB − 2 framebuffers − fonte/HUD/notas
− três quadros de personagem no tamanho nativo`), o `limoRide` que estava em
**1.870.848 B** no plano antigo ficou em **1.336.816 B** — dentro.

| | plano antigo | reempacotado |
|---|---|---|
| palcos com pacote | 1 (`mainStage`) | **21** |
| props com pacote | 3 | **157** |
| pior palco em VRAM | 1.870.848 B (**estouro** do teto de 1.337.520) | **1.336.816 B** (`limoRide`), 100 % do teto |
| palcos que passam do teto | 15 de 21 | **0 de 21** |
| bytes no pendrive | — | **10.423.199 B** em 21 palcos |
| `mainStage/stageBack` | 802×438 | **712×389** (escala 0,313 → 0,278) |

E o pior caso de RAM por palco (`check_stage_budget.py`, que soma o pacote
inteiro e a tabela do leitor por quadro) continua dentro do limite de
16.182.527 B: o maior é o `limoRideErect` com 16.138.576 B — 99,7 % do teto, que
é o que se quer de um orçamento que está sendo usado.

**O que isso custa em imagem:** os palcos ficam ~11 % menores que o plano antigo
(que não cabia). A regra do trabalho é explícita — *a fidelidade da imagem é
negociável, a do jogo não* — e o plano antigo comprava nitidez com VRAM que o
console não tem.

**O que NÃO está provado:** que os 21 palcos *pareçam* certos na tela. O harness
sintetiza os props (prova o encanamento: pacote → pool → desenho → geometria),
mas não tem rasterizador. O teste de aceitação é a captura do console.

Os três headers saíram do mesmo plano, e o `--check` de cada um roda na suíte:
`stage-props.json` (plano), `src/stage_dados.h` (157 props, 199 animações) e
`src/engine/stage_vram_data.h` (tabela de VRAM). O teste de layout prende
712×389 escrito à mão de propósito: se o empacotador mudar a escala, o número
tem de ser reconferido em vez de o teste acompanhar sozinho.

## Offsets por animacao: 155 das 523 animacoes estavam desenhadas no lugar errado

O personagem nao fica no mesmo lugar em todas as animacoes -- cada animacao do
`character.json` pode trazer `offsets`, e o original aplica o da animacao
corrente quando ela comeca. O port somava so' o `offsets` do PERSONAGEM (o
`globalOffsets`) e ignorava o da animacao: **155 das 523 animacoes dos 46
personagens** tem offset proprio, e todas as 155 apareciam deslocadas. Nao e'
um caso de canto raro: `bf-christmas/idle` e' [-5,0], `bf-christmas/singDOWN`
e' [-10,-50], e o `ugh` do tankman -- que e' justamente um NOTE KIND -- e'
[5,-40].

A regra foi lida, nao deduzida:

- `Bopper.hx` L321-325 (`applyAnimationOffsets`): toda troca de animacao copia
  `animationOffsets.get(name)` para `animOffsets`;
- `Bopper.hx` L336-341 (`getScreenPosition`): `output.x -= (animOffsets[0] -
  globalOffsets[0]) * scale.x`, idem y;
- animacao sem `offsets` no JSON recebe `[0,0]` (`CharacterData.hx` L637
  `DEFAULT_OFFSETS`) -- que e' o valor declarado do padrao, nao dado faltando.

O `* scale` e' a parte que se erra com facilidade: o scale do sprite e'
`CharacterData.scale` vezes o `scale` do slot do palco (`Stage.hx` L451-455,
`finalScale = getBaseScale() * stageCharData.scale`). Com os personagens de
pixel (scale 6) uma animacao de offset [12,-6] anda **72** px na horizontal,
nao 12. `ch_offset_anim()` recebe as duas escalas e devolve o delta ja' no
espaco do `ch_anchor()`.

O que muda no desenho: `cena_desenha()` aplica o delta DEPOIS do
`ch_anchor()`. O foco da CAMERA nao muda -- o `resetCameraFocusPoint`
(`BaseCharacter.hx` L350-355) usa `originalPosition + cameraOffsets`, que nao
levam `animOffsets`. Aplicar o offset na camera moveria a camera a cada
animacao, que e' um defeito que nao existe no original.

Prova no log do console, uma vez por papel: `char_anim_offset` e
`char_anim_offset_y`, em centesimos de pixel (para sair o mesmo numero do
dado). No `ugh` do tankman: `-500` e `4000`, que e' [5,-40] com o sinal do
original.

O gerador mudou junto: `tools/gen_char_dados.py` passa a emitir `dx,dy` em
`ch_anims[]` e `CH_ANIM_COM_OFFSET` (155), e o teste `test_char_layout.c`
cobra o contador, o sinal, o fator de escala do slot (1,75 no phillyBlazin), o
fator 6 dos personagens de pixel e o caso sem offset (devolve 0 e zera a
saida, para nao vazar o valor da animacao anterior).

## Tremor de camera e ordem de desenho: os dois ultimos pedacos do NOTE KIND

O dado dos kinds ja' estava lido e o canto/animacao ja' tocavam. Faltavam as
duas outras chamadas que os scripts fazem no MESMO trecho, e as duas eram
"lidas e nao aplicadas":

| pedaco | no dado | onde | antes | agora |
|---|---|---|---|---|
| `shake` | 14 pares | `camGame.shake(0.0025, 0.15)` | nada tremia | `engine/shake.c` |
| `ordem` | 49 pares | `this.zIndex = 2000/3000; currentStage.refresh()` | sempre o z do slot | `g_z_extra[]` |

As duas sao chamadas INCONDICIONAIS dentro da funcao do script
(`darnell-blazin.hxc` L306-311: `playAnimation`, `shake`, `moveToBack`), entao
entram antes de saber se a animacao existe -- igual ao original, onde
`playAnimation` de um nome inexistente so' escreve no log.

**O tremor.** `FlxCamera.shake(I, D)`: a intensidade e' uma FRACAO do tamanho da
camera, nao um numero de pixels -- 0,0025 numa camera de 1280x720 da' **+-3,2 px**
na horizontal e **+-1,8** na vertical, sorteados a cada quadro ate' o tempo
acabar. A conta esta' em `src/engine/shake.c` com teste proprio
(`test/test_shake.c`): duracao, amplitude, volta a zero no ultimo quadro,
sequencia repetivel com a mesma semente e tremor novo substituindo o velho em
vez de somar. O que **nao** e' igual, de proposito, e' a sequencia de numeros
aleatorios: o original sorteia pelo `FlxG.random` (semente do jogo) e o port usa
um LCG proprio, porque o harness precisa do mesmo resultado duas vezes. A
distribuicao e a amplitude sao as do original; a sequencia alheia nao daria para
reproduzir de qualquer jeito.

O deslocamento entra na CAMERA (dividido pela escala com que o mundo e'
desenhado), nao no mundo: e' o que o FlxCamera faz, e por isso o HUD nao treme --
no original ele esta' em outra camera tambem.

**A ordem.** `moveToBack()` poe `zIndex = 2000` e `moveToFront()` poe 3000, e o
`currentStage.refresh()` reordena o grupo -- a troca VALE dali em diante, nao
volta sozinha. E' o Blazin' inteiro: o Darnell soca, vai para tras, e as vezes
para a frente. Agora o vetor `g_z_extra[]` (um por papel) entra no lugar do z do
slot e a ordenacao por zIndex que ja' existia faz o resto.

### O defeito que apareceu ao mexer nisso

O vetor de z da ordenacao estava trocado em relacao ao vetor de papeis:

```c
int zz[3], ord[3] = {CH_PAPEL_GF, CH_PAPEL_BF, CH_PAPEL_DAD};
zz[0] = z("gf");  zz[1] = z("dad");  zz[2] = z("bf");   /* ERRADO */
```

A ordenacao troca `zz[i]` e `ord[i]` de lugar JUNTOS, entao o z do DAD era
aplicado ao BF e vice-versa. Medido no dado: **17 dos 21 palcos** tem
`z(bf) != z(dad)` (no mainStage 300 contra 200), ou seja, em 17 palcos os dois
personagens eram desenhados na ordem contraria a' do original -- o pai na frente
do BF. Era erro de LEITURA de dado: o numero certo estava la', preso ao papel
errado.

Prova no log (`blazin` hard, 1200 quadros): 13 eventos `nota_kind_fundo`/
`nota_kind_frente` e 5 `nota_kind_shake`. Suite inteira: `ALL SUITES PASSED`.

**Nao provado:** que tremer +-3,2 px e trocar a ordem *parecam* certos. Nenhum
dos dois tem como ser julgado sem tela.

## O empacotador de personagem: tres leitores que faltavam (46 pacotes, 0 falhas)

Com o `NameError` e os grupos corrigidos, `sh tools/achatar_tudo.sh /tmp/fassets
build/packs/chars` fechou **46 pacotes e 0 falhas** em ~42 minutos. Aí o
`tools/check_char_packs.py` -- que confere o pacote contra o `character.json`,
nao contra o proprio empacotador -- acusou **6 pacotes sem animacao**. Nenhuma
era dado ausente: eram tres formatos que o leitor nao entendia.

| o que | personagens | o que era |
|---|---|---|
| export FULL do Animate | `bf`, `bf-car` (`fakeoutDeath`) | 4 dos 68 `Animation.json` do clone usam chaves longas (`ANIMATION`/`TIMELINE`/`Frames`/`SYMBOL_DICTIONARY`) e o leitor so' entendia as compactas (`AN`/`TL`/`FR`). O rotulo da animacao no FULL nao esta' no `FR`: e' o `SYMBOL_name` do documento |
| rotulo com espaco | `pico-holding-nene` (`singUP`/`singDOWN`) | a timeline rotula `"up "` e `"down "`, o JSON pede `"up"`/`"down"`; casar pelo nome aparado (e reportar) e' o que faz a animacao existir |
| indice fora do atlas | `gf-pixel` (`danceLeft`) | o dado pede 16 quadros e um deles e' o indice 30 de um atlas de 0..29. O empacotador jogava fora a animacao INTEIRA por causa de um indice -- a GF ficava sem a metade esquerda da danca. Agora o invalido e' descartado e CONTADO |

Depois dos tres consertos, reempacotados so' os afetados:

| personagem | antes | depois |
|---|---|---|
| `bf` | 349 quadros, 15 anim | **500, 16** |
| `bf-car` | 442, 18 | **593, 19** |
| `pico-holding-nene` | 264, 14 | **286, 16** |
| `gf-pixel` | 1 anim | **2** |
| **os 46** | 9.468 quadros, 513 anim | **9.807 quadros, 518 anim**, 169.710.819 B em `.chp4` |

`check_char_packs.py`: de 6 falhas para **2**, e as duas sao dado que o leitor
ainda nao alcanca -- ficam reportadas, nao tapadas:

- `pico-playable` `sing*-censor`: o `assetPath` da animacao aponta para um atlas
  **Sparrow** (`characters/Pico_Censored`, PNG+XML) dentro de um personagem
  Animate. O empacotador Animate so' abre Animate;
- `tankman-bloody` `stressPicoEnding`: o atlas
  (`week7:erect/cutscene/tankmanEnding`) e' um **embrulho**: a timeline de cima
  tem UM quadro que instancia um simbolo de 224 quadros, e o rotulo esta' em
  `AN.SN`, nao num `FR`. Ler isso pede o comprimento do simbolo aninhado e o
  modo `inner="sync"` no renderizador; sem isso o pacote teria 1 quadro e a
  animacao sairia congelada -- pior que faltar, porque parece funcionar.

**E o jogo abre os pacotes de verdade.** Os 46 `.chp4` reais foram para os
assets do harness no lugar dos sinteticos de `bf`/`gf`/`dad` e o tutorial rodou
**1402 quadros** com eles: `char_carregado` 5 com 3.753.133 B (bf.chp4), 14 com
1.766.022 B (gf), `char_bf_primeiro`/`char_dad_primeiro` desenhados, nenhum
estouro, nenhuma escrita fora do limite. Ou seja: o formato que o console vai ler
nao e' so' o que o empacotador escreve -- e' o que o jogo consome.

## Note kinds medidos com os pacotes de verdade (e o que faltava era o pacote)

O `check_char_packs.py` fecha **46 pacotes, 10.170 quadros, 523 animacoes,
174.086.278 B, zero falhas**. Com os pacotes REAIS no lugar dos sinteticos do
harness, a conta dos NOTE KINDS muda de valor pela primeira vez:

| `blazin` hard, 1200 quadros | pacotes sinteticos | pacotes reais |
|---|---|---|
| `nota_kind_anim` (animacao tocada) | 104 | **104** |
| `nota_kind_sem_anim` (o dado pede, o pacote nao tem) | **110** | **0** |
| `nota_kind_fundo`/`nota_kind_frente` | 0 | **13** |
| `nota_kind_shake` | 0 | **5** |

Os 110 eram `punchHigh*`/`punchLow*`: o script monta o nome com
`'punchHigh' + postfix` e o `postfix` vem de `doAlternate()`, que alterna
`'1'`/`'2'`. O gerador ja' lia isso (o `anim2` do dado), o que faltava era o
pacote ter as duas metades -- os 46 reais tem.

Outras musicas, mesma passada: `cocoa` 43 cantos com sufixo (`mom` -> `alt`),
`spaghetti` 92 (`sakura-joint`), `ugh` 3 animacoes (`ugh`/`hehPrettyGood`).
Nenhum `sem_anim` quando o pacote do personagem em cena e' o real.

**O que continua declarado, e nao tapado:** `stressPicoEnding` do
`tankman-bloody` esta' no pacote (o atlas-embrulho foi lido: 224 quadros de
simbolo), mas o `char_kinds` desse personagem ainda mostra os kinds dele porque
a musica `stress` nao chega nessa animacao na janela varrida -- 1 nota de kind
aos 62 s. Nao e' falha, e' "nao observado".

**Suite completa com os pacotes reais no harness:** `ALL SUITES PASSED`
(346 s), incluindo o sweep ASan/UBSan das 81 combinacoes e o Story Mode.

## Tela de GAME OVER: a animação de morte e a câmera do original

Até aqui a tela de game over existia, mas era um retângulo preto com texto: dos
eventos que o original mostra nela, **nenhum** acontecia — o BF não morria, a
câmera não ia para ele, o zoom não mudava e o reinício era instantâneo em vez
de acontecer no fim de um fade de 1 s. O dado para tudo isso já estava no
repositório (`assets-data/characters`, `stage_dados.h`); o que faltava era o
`death` do `CharacterData` chegar ao C.

**O dado que faltava.** `tools/gen_char_dados.py` passou a emitir `dcx, dcy,
dzoom` (`death.cameraOffsets` e `death.cameraZoom`) em `ch_dados_t`, e
`src/engine/char_dados.h` foi regerado: 46 personagens, 523 animações. Medido
nos 46 JSONs: **35 não têm `death`** (valem o padrão do original, `[0,0]` e
zoom 1,0), **11 têm**, e dois têm zoom diferente de 1: `pico-blazin` 0,8 e
`sakura` 1,5. As animações de morte existem em **13** personagens — os quatro
`bf*`, os cinco `pico*`, `pico-playable` (com as variantes `-explosion`),
`bf-car`, `bf-christmas`, `bf-dark`, `bf-holding-gf`, `pico-holding-nene` e
`sserafim-sakura`. Nenhum dos 46 tem `death.quote` no pacote: o caminho da
"frase de morte" (`playDeathQuote`) fica sem dado, **declarado**, e não
inventado.

**O que o original faz, linha por linha** (`GameOverSubState.hx`):

| Evento | Linha | Port |
|---|---|---|
| fundo preto opaco cobrindo `width*2 x height*2` | L139-145 | já tinha |
| BF sai do palco e entra no substate **na mesma posição**, `isDead = true` | L155-179 | `cc_especial_fixo` (novo): o BF **não volta a dançar** quando a morte acaba |
| `fakeoutDeath` com 1 em 4096, senão `firstDeath` | L250-259 | sorteio determinístico (LCG semeado pelo quadro), telemetria `gameover_fakeout`; o fim do fakeout entra no `firstDeath` e chama `playBlueBalledSFX()` (bf.hxc `doFakeoutDeath`), como o original |
| acabou o `firstDeath` -> `deathLoop` + música em loop | L305-318 | `cc_especial_terminou` decide a troca e `startDeathMusic(1.0,false)` toca `gameOver` + sufixo do personagem (ver a seção do áudio) |
| câmera segue `cameraFollowPoint`, `DEFAULT_CAMERA_FOLLOW_RATE/2` | L220 | `GO_CAM_LERP = 0,020019798` (= `1 - 0,98^1,001` do `FlxCamera.updateFollow`) |
| zoom -> `stage.camZoom * death.cameraZoom`, `smoothLerpPrecision(..., 0,5)` | L100, L265 | `GO_ZOOM_K = 0,85756425` (= `0,01^(1/59,94/0,5)`) |
| CROSS -> `deathConfirm`, `gameOverEnd`, espera `FADE_TIMER`, `fade(BLACK, 2)`, `needsReset` + `fade(BLACK, 1, true)` | L271-278, L330-438 | `deathConfirm` + `FADE_TIMER = comprimento do End / 7000` + 120 quadros de fade para preto + reinício + 60 quadros de fade-in (`g_fade_in`) |
| CIRCLE -> `goBack()`, `deathCounter = 0` | L544-570 | sai para Freeplay/Story e zera "BLUE BALLS" |
| as DUAS saídas exigem `blueballed`, marcado por `playBlueBalledSFX()` | L271-278, L546, L600-623 | estado `g_go_blueballed`, armado junto com o `firstDeath`; o caminho do `fakeoutDeath` **não** arma, e aí a tela não sai (1 em 4096, como no original) — o harness avisa em vez de acusar defeito |

**O foco da câmera** é uma conta do original com três parcelas que se cancelam
em parte (L196-222):

```
foco = originalPosition + canvas/2 + data.cameraOffsets      (cameraFocusPoint)
       - data.cameraOffsets                                  (some)
       - stage.characters.bf.cameraOffsets
       + death.cameraOffsets
```

Ela **não** é a fórmula do gameplay (`ch_anchor` soma os dois offsets). Por isso
virou `ch_foco_morte()` em `src/engine/char_layout.c`, com as duas truncagens
(`Std.int`) do original, e teste próprio: com o canvas do pacote real do BF
(365×278, escala 0,5 → 730×556 no espaço do original) o foco no `mainStage` dá
**(1033,5 ; 763,0)** — e o gameplay, no mesmo palco, dá (888,5 ; 589,0). Os dois
números estão presos em `test/test_char_layout.c`.

**Medido no harness** (`tools/run_gameover_harness.sh`, três passadas):
`gameover_primeira_morte` = personagem 5 (o `bf`), `deathLoop` no quadro 3566
(o `firstDeath` do BF tem 2 quadros a 24 fps), `gameover_confirmar` = 1, câmera
final (1030 ; 1007) contra o foco publicado pelo jogo (1033 ; 1009) — o que resta
é a exponencial do lerp —, zoom final **1100** = alvo **1100** (milesimos),
reinício **128 quadros** depois do CROSS (FADE_TIMER de 142 ms do End sintético +
120 quadros de fade) e CIRCLE saindo sem reiniciar.

**O que continua não provado:** como isso *aparece*. O harness não tem tela; ele
prova o estado, os quadros pedidos e os números.

## O jogo inteiro com os pacotes DE VERDADE: 81/81

Tudo o que o harness rodava até aqui usava os assets **sintéticos** de
`build/harness_assets` — texturas de 64×64, um prop por palco, três animações
por personagem. Isso prova a lógica, mas **não prova que o empacotador e o
leitor do jogo combinam**: um pacote com quadro fora do canvas, `stride` errado,
PSM trocado ou bloco LZ4 truncado passa nos testes sintéticos e só aparece com
o pacote real.

`tools/run_realpacks_harness.sh` roda o jogo com os pacotes de verdade, sem
copiar 330 MB: a raiz de assets é a do harness por **symlink**, menos duas
pastas — `assets/chars` → `build/packs/chars` (46 personagens) e `assets/stages`
→ `build/packs/stages` (21 palcos). Ele cobra, por combinação: o harness termina,
nenhum `palco_sem_vram`, pelo menos 2 personagens carregados (`char_carregado`) e
pelo menos uma animação de personagem executada (`char_*_animando` — a prova de
que o pacote real **decodificou pixel**, e não só abriu).

**Medido: 81 de 81 combinações passaram** (27 músicas × 3 dificuldades, 600
quadros cada, ~48 s).

Sobra do pool de VRAM (2.056.192 B) por música, com a arte real carregada —
igual nas três dificuldades, porque a dificuldade não muda o elenco:

| música | pool livre (B) | props | | música | pool livre (B) | props |
|---|---|---|---|---|---|---|
| tutorial | 355.072 | 3 | | winter-horrorland | 495.360 | 3 |
| bopeebo / fresh / dadbattle | 338.688 | 3 | | senpai | 36.352 | 8 |
| spookeez / south | 430.336 | 1 | | roses | 36.096 | 8 |
| monster | 585.728 | 1 | | thorns | 440.064 | 4 |
| **pico / philly-nice / blammed** | **1.792** | 5 | | ugh / guns / stress | 478.208 | 14 |
| satin-panties / high / milf | 532.992 | 8 | | darnell / lit-up / 2hot | 512.256 | 8 |
| cocoa / eggnog | 258.048 | 6 | | blazin | 358.400 | 4 |
| | | | | spaghetti | 415.232 | 12 |

Três consequências, todas medidas:

1. **Cabe.** Com a arte real, nenhuma das 81 combinações estoura o pool — o pior
   caso (`pico`, `philly-nice`, `blammed`, semana 3) deixa **1.792 B** livres.
   É aperto, não folga: qualquer textura nova (os **ícones de vida** do HUD, por
   exemplo, que hoje são dado ausente e aparecem como `evento_sem_icone`) não
   entra sem liberar espaço antes. Esse é o próximo trabalho de VRAM.
2. **Os pacotes reais funcionam no leitor do jogo.** 46 pacotes, 10.170 quadros,
   523 animações, 174 MB de arte, decodificados quadro a quadro pelo mesmo
   código que roda no console.
3. **O elenco real tem menos de 3 personagens em uma música:** `tutorial` tem 2
   (o oponente é a própria GF, e o dado não tem `girlfriend`), e o teste cobra
   ≥ 2 em vez de exigir 3 — o número vem do dado, não de uma suposição.

**Um defeito que apareceu ao escrever isto** — e que teria escondido o teste: a
lista de músicas era tirada do nome da variável C (`s_philly_nice`) em vez do id
do dado (`philly-nice`). Oito das 27 músicas nunca começavam, o harness
"passava" sem entrar no gameplay e as três verificações por combinação davam
zero. Agora o id vem do primeiro campo da struct, que é o que vira caminho de
arquivo.

## O áudio da tela de GAME OVER: os três sons eram dado ausente

A tela de game over estava completa em animação e câmera, e **muda**. Não por
esquecimento: nem `gameplay/gameover/gameOver*`, nem `fnf_loss_sfx*` existiam no
pacote de dados deste repositório, e os sufixos que escolhem a variante
(`-pico`, `-pixel`, `-pixel-pico`, `-pico-gutpunch`) também não — eles moram nos
**scripts de personagem**, não no `CharacterData`.

**Onde os dois dados estavam.** No repositório oficial de arte:

- `shared/music/gameplay/gameover/gameOver{,-pico,-pixel,-pixel-pico}.ogg` e os
  `gameOverEnd*`; `shared/sounds/gameplay/gameover/fnf_loss_sfx{,...}.ogg` e
  `fakeout_death.ogg`. `tools/empacotar_audio.py` decodifica, reamostra para
  48 kHz e escreve PCM s16le estéreo — o formato que o `audsrv` toca. Vão para
  `mass:/FNFPS2/assets/gameover/`: **13 arquivos, 45.721.916 B**, com manifesto
  (`AUDIO.txt`) e sha256 curto de cada um.
- `shared/music/breakfast.ogg` (44,1 kHz) e
  `shared/music/breakfast-pico/breakfast-pico.ogg` (48 kHz) são a **música da
  pausa**; o mesmo empacotador as converte e o pendrive as põe em
  `mass:/FNFPS2/assets/pause/`. São **11.520.000 B** (o `-pico`, 60 s exatos) e 11.520.628 B (o sem sufixo, que
vem de 44,1 kHz e ganha algumas amostras no reamostrador),
  porque a trilha da pausa toca inteira em loop — cortá-la mudaria a duração do
  loop e, com ela, o instante em que a música volta ao começo.
- `preload/scripts/characters/*.hxc` definem `GameOverSubState.musicSuffix`,
  `blueBallSuffix` e `animationSuffix`. `tools/gen_gameover_sfx.py` lê as
  atribuições **literais** e gera `src/gameover_dados.h` (5 personagens):

| personagem | música | blue balls | pausa |
|---|---|---|---|
| `bf-pixel` | `-pixel` | `-pixel` | `-pixel` |
| `pico-blazin` | `-pico` | `-pico-gutpunch` | `-pico` |
| `pico-holding-nene` | `-pico` | `-pico-and-nene` | `-pico` |
| `pico-pixel` | `-pixel-pico` | `-pixel-pico` | `-pixel` |
| `pico-playable` | `-pico` | `-pico` | `-pico` |

Os outros 41 personagens não atribuem nada, e o padrão do original é vazio
(`GameOverSubState.reset()`, chamado por `PlayState.reset()`). **Nenhuma linha
do port chuta um sufixo.**

**Duas ausências que ficam declaradas, não "consertadas":**

1. `-pico-gutpunch` e `-pico-and-nene` são procurados em `fnf_loss_sfx` e **não
   existem nem na arte oficial**. O original faz `Assets.exists` e loga erro;
   o port abre o arquivo, falha, registra `gameover_sfx_ausente` e segue sem o
   som. O harness conhece esses dois caminhos (`h_ausencia_declarada`) para não
   acusar a raiz de assets por um arquivo que nunca existiu.
2. `songs/2hot.hxc` troca os sufixos para `-pico-explode` **em tempo de
   execução**, quando a lata explode e o jogador morre (`takeCanDamage`). Isso é
   lógica de script de música, não configuração de personagem: não há linha em
   lugar nenhum do dado dizendo "2hot usa -pico-explode". O resolvedor do port
   sabe reduzir o sufixo (é o mesmo `resolveMusicPath`), mas nada o define hoje.

**Como dois sons tocam ao mesmo tempo.** A música do game over e o sfx do blue
balls são dois `FlxSound` no original; o port tem **um** anel de áudio e **um**
`audsrv`. A soma é feita na thread do anel (`audio_ring`, fonte secundária
"overlay"), e não num segundo anel, porque o `usbhdfsd` não é reentrante: duas
leituras simultâneas do pendrive corrompem o driver — foi medido antes, com a
captura de tela. A ordem do original é reproduzida:

1. morte -> o anel entra em **silêncio + `fnf_loss_sfx`** (a música ainda não
   existe nesse instante no original);
2. fim do `firstDeath` -> `audio_ring_trocar_fonte`: a música entra **por baixo**
   do sfx que ainda está tocando, em loop;
3. CROSS -> `gameOverEnd` sem loop; o comprimento dele é o `FADE_TIMER`.

`playBlueBalledSFX()` marca `blueballed` **e** toca o som; o `fakeoutDeath`
toca `fakeout_death` e só chama o sfx quando termina, então durante o fakeout
(beco sem saída de 1 em 4096) a tela continua sem saída **e** com o som certo.

**A conta do fade, corrigida.** O port esperava 60 quadros fixos depois do
CROSS. O original (`GameOverSubState.hx` L381-438, idêntico na v0.8.6):

```
FADE_TIMER = gameOverMusic.length / 7000            (End real: 7,21 s -> 1,03 s)
fade(BLACK, 2, false, ...)                          (2 s indo para preto)
   -> resetPlaying(): fade(BLACK, 1, true) + needsReset + close()
```

Ou seja: **FADE_TIMER + 2 s** até o reinício, e **1 s** de volta do preto já no
PlayState reiniciado (`g_fade_in`). O `FADE_TIMER` é derivado do arquivo que
tocou (`amostras / 336` ms), não de uma constante.

**Medido no harness** (`tools/run_gameover_harness.sh`, End sintético de 1 s):
`gameover_audio_inicio` = 1 (o sfx abriu), `gameover_musica` = 96.000 amostras
(2 s, em loop), `gameover_end` = 48.000 amostras, `gameover_fade_espera` = 142 ms,
**128 quadros** entre o CROSS e o reinício (esperado 129 = 142 ms + 120),
**199.680 amostras** entregues ao audsrv nessa tela e `gameover_audio_fim` sem
erro. O evento `gameover_sem_audio` — que declarava "esta tela não tem som" —
**não existe mais no log**, e o teste reprova se voltar.

**O que isso mudou no harness** (e que não era bug do jogo):

- o shim tratava `DelayThread` como **no-op** na thread de render. Como as
  funções do anel passaram a publicar pedidos e esperar a thread consumir, a
  espera de 500 ms virava 500 voltas instantâneas, a produtora nunca era
  escalonada e o pedido "falhava" no PC — enquanto no console funcionava. Agora
  `DelayThread` **cede a CPU** (`sched_yield`) nos dois casos e só a thread de
  render move o relógio virtual;
- o sweep rodava **sem apertar nada**: as notas viravam MISS, a vida caía e o
  jogador morria (visível no `blazin`, que tem nota cedo). Ele passou a usar o
  roteiro de autoplay do próprio chart (`tools/harness_presses.py`), o mesmo do
  `run_gameplay_harness.sh`;
- `icone_desenho` só era publicado na entrada do resultado, então uma música que
  não terminasse dentro dos quadros do harness ficava sem prova de que o ícone
  foi desenhado (e o sweep reprovava por isso). Agora ele sai também a cada 300
  quadros de gameplay, e os harnesses leem o **último** valor.

## Fechado: a CINEMÁTICA DE RANK da Freeplay (arte empacotada, tela ligada, harness)

**FECHADO nesta build.** Quando o jogador termina uma música e volta para a
Freeplay com um rank **pessoal**, o port agora roda `rankAnimStart`
(`FreeplayState.hx` L1069-1145), `rankDisplayNew` (L1147-1225) e
`rankAnimSlam` (L1230-1292), com o `fadeAnim` da cápsula (`SongMenuItem.hx`
L400-437) — as duas câmeras, os zooms com os easings do `FlxEase`, o `rankBg`,
o selo crescendo de 20 para 0,9, as duas faíscas de 5 quadros em ADD, a vinheta,
o tremor do `IntervalShake` (nove tiques de 1/30 s na cápsula e quatorze de
1/24 s no pouso), o ângulo sorteado das vizinhas e a lista travada
(`Interacting`) nos 2,2 s.

Medido no harness (`tools/run_rank_anim_harness.sh`, 7002 quadros):

| grandeza | valor medido | o que a fonte manda |
|---|---|---|
| texturas | `freeplay_cine,7,186566` | as três abriram, 186.566 B |
| fases | `0 1 2 3 4` | 500 / 600 / 900 / 1100 / 1600 ms |
| zoom da `rankCamera` | `1850 1812 1500 1519 800` | 1,85; sineIn em 300 ms; **1,5** exato em 900 ms; backIn em 1100 ms; **0,8** no pouso |
| cápsula (décimos de px) | `1670:1470` → `1570:1270` → `1569:1269` → `1602:1178` | centro; tranco de −10/−20; queda `quartIn`; `originalPos` = (160,244 , 117,8) |
| fim | `freeplay_rank_fim,132,0` | termina em 2,2 s, 132 elementos desenhados, 0 ms de atraso |

O portão também está provado pelo outro lado: o harness da Freeplay (que chega
ao resultado pelo Practice) cobra `freeplay_rank_pedido,0` e **zero** inícios —
`isScoreValid = !isPracticeMode && !isBotPlayMode` (`ResultState.hx` L938), e o
`fromResults` com `playRankAnim` só vai quando o rank sobe.

**Dois defeitos apareceram no caminho, os dois medidos e corrigidos:**

* o **x da lista** usava `intendedX(i - sel)` e a fonte usa
  `intendedX(index - curSelected)` com `index` **1-based** (`changeSelection`,
  L2943) — a lista inteira estava **25,2 px à esquerda** e o `originalPos` da
  cinemática não casaria com o lugar de onde a cápsula saiu;
* o **alpha do vértice** no GS vale 0..128 (0x80 = 1,0) e o código multiplicava
  por 255: a meia-transparência da entrada (`alpha = 0,5`) saía **opaca**.

**Divergências que sobraram, com o número:**

* o `FlxTrail` do `fadeAnim` é 1 amostra visível (alpha 0,01; as outras nascem
  com alpha negativo e o `redrawTrailSprites` as revive sem desenho) e o `delay`
  do campo é `Int`, então vale 0 e todo quadro entra. O port desenha essa
  amostra e some, como a fonte;
* `FlxSpriteGroup.angle` gira cada filho em volta do **próprio** `origin`
  (`angleTransform`, `FlxSpriteGroup.hx` L1017) e o port faz o mesmo, com o eixo
  no centro do quadro (`centerOrigin` do `resetHelpers`);
* o selo (`FreeplayRank`) tem `updateHitbox()` depois do `scale.set(0,9)`, o
  que desloca o desenho em `0,05 × frameWidth` = **1,3 px** em 640x360. O port
  empacota a arte já a 0,9 e desenha no canto; a diferença é 1,3 px e está
  registrada aqui, não escondida.

O que a cinemática **usa** de dado já levantado antes desta build:

| elemento | fonte | número medido |
|---|---|---|
| `rankBg` | L480-482 | cor sólida `0xD3000000`, tela inteira (sem textura), alpha 1 → 0 em 0,5 s `expoIn` (L1230) |
| `rankCamera` | L324, L1131-1132 | zoom 1,85 → 1,8 em 0,6 s `sineIn`; **+0,1 s**: 1,3 → 1,5 em 0,3 s `backInOut`; **+0,4 s**: → 1,2 em 0,8 s `backIn`; **slam+0,5**: 0,8 → 1 em 1 s `elasticOut` |
| `funnyCam` | L325, L1134-1135 | 1,15 → 1,1 em 0,6 s `sineIn`; +0,1 s: → 1,05 em 0,3 s `elasticOut`; +0,4 s: → 1 em 0,8 s `sineIn`; slam+0,5: `shake(0,0045; 0,35)` e 0,8 → 1 em 0,8 s `elasticOut` |
| `fakeRanking`/`fakeBlurredRanking` | L1081-1082, L1091-1093, L1180-1181 | mostram o rank ANTIGO (alpha 1 só quando `oldRank != null`) e somem em +0,1 s |
| `sparks` | L1096-1104 | Sparrow `freeplay/sparks`, **5 quadros** (o maior 522×361 na folha 2048×1024), escala 0,5, `BlendMode.ADD`, pos (517, 134), toca uma vez |
| `sparksADD` | L1106-1115 | Sparrow `freeplay/sparksadd`, **5 quadros** (maior 556×395), escala 0,5, ADD, pos (498, 116), cor = `oldRank.getRankingFreeplayColor()` |
| `rankVignette` | L332, L809-814, L1280 | imagem 640×360 escalada 2×, ADD, alpha 0 → 1 → 0 em 0,6 s `expoOut`, cor = a do trail |
| `fadeAnim` | `SongMenuItem.hx` L400-437 | `impactThing` = o quadro da cápsula, escala → 2,5 em 0,5 s; `FlxTrail` (15 amostras, 0,03/0,01/0,069) ADD, alpha → 0 em 0,6 s `quadOut` |
| cápsula | L1137-1144, L1183-1215, L1262-1264 | `ranking.scale` 20 → 0,9 em 0,1 s; x−10, y−20; `angle` −3 → 0 em 0,5 s `backOut`; `IntervalShake` 0,3 s a 1/30; tween para (orig−7, orig−80) em 1,3 s `quartIn`; e `targetPos` de volta em 0,5 s `expoOut` |
| sons | L1190-1197, L1232-1246 | `ranks/rankinbad` \| `rankinperfect` \| `rankinnormal` em +0,1 s (pelo rank novo); `ranks/loss|good|great|excellent|perfect` no slam |
| DJ | L1084, L1251 | `fistPumpLossIntro`/`fistPumpIntro` no começo; `fistPump`/`fistPumpLoss` no slam |
| ordem dos timers | L1146, L1176, L1219, L1223 | 0,5 s → `rankDisplayNew`; dentro dele +0,1 s e +0,4 s; +0,6 s → `rankAnimSlam`; dentro dele +0,5 s |

Dado que falta **não é arte nem som**: os dois Sparrows, a vinheta, o
`sparkle` (256×256, 10 quadros) e os 5 `.ogg` de `preload/sounds/ranks/` estão
todos no clone. O encanamento das três coisas abaixo **entrou** nesta build; o
que ficou de fora são os SONS de `ranks/` (o áudio da tela é o próximo passo,
e o `.ogg`→PCM é o mesmo caminho de `empacotar_audio.py`) e o `sparkle`/
`favHeart`, que são da cápsula na lista e não da cinemática:

1. os parâmetros que o Resultado passa para a Freeplay —
   `fromResults {songId, difficultyId, oldRank, newRank, playRankAnim}`
   (`ResultState.hx` → `FreeplayState` L287-288, L367, L780-782). O port já lê o
   save depois de gravar; o `oldRank` tem de ser capturado **antes** da gravação;
2. a pilha de easeings que o port não tem (`backIn`, `backOut`, `backInOut`,
   `elasticOut`, `expoIn/Out`, `quartIn`, `sineIn`) — todas do `FlxEase`, com
   fórmula fechada, sem estado;
3. as duas câmeras (`rankCamera`/`funnyCam`) desenhando a MESMA cena com zooms
   diferentes em volta da cápsula — no port o desenho é um passe só, então isto é
   uma mudança no caminho de desenho da Freeplay, não um sprite novo.

VRAM da cinemática: **186.566 B** (33.280 + 38.086 + 115.200), com as faíscas a
um quadro por vez na VRAM (5 subidas de 33 KB em 208 ms). Somando a cápsula
(161.568), o fundo (413.280), os 27 títulos (122.740), o selo (6.624) e o DJ
(277.500), a tela de Freeplay usa **1.168.278 B** dos 2.383.872 do pool — os
21 palcos (pior 1.736.192 B) não convivem com ela, e não precisam: a Freeplay
roda com o palco solto.

## O ÁUDIO DA FREEPLAY (o preview e os jingles do rank) — planejado aqui, fechado no fim do documento

Medido antes de escrever código (regra 2), porque o estado 11 hoje é **mudo**
debaixo da música do menu: o laço do consumidor (`src/main.c` L6029) inclui
`ui_state==11`, mas quem toca ali é o `freakyMenu_48000.pcm` que veio do menu.

**1. O preview da música** (`playCurSongPreview`, `FreeplayState.hx` L2967-3046),
chamado em `changeSelection` e no fim do `rankAnimSlam` (L1289):

| peça | número medido | linha |
|---|---|---|
| volume do preview | `0,7 * dj.getMusicPreviewMult()` | L2971-2972 |
| instrumento | `pathsFunction: INST` — só o inst, sem as vozes | L3015 |
| trecho | `start = previewStartTime`, `end = previewEndTime`, carregados como **fração** do arquivo (`loadPartial`) | L3017-3022 |
| frações padrão | `DEFAULT_PREVIEW_START_TIME = 0` e `DEFAULT_PREVIEW_END_TIME = 0,2` (`util/Constants.hx` L306/L311) | — |
| correção das frações | se `start >= end` ou o valor passa de 1, volta ao padrão | L3414-3418, L3430-3434 |
| fade | `fadeIn(2, 0, previewVolume)` na carga e `fadeOut(2, 0)` nos últimos 2 s | L3025-3033 |
| repetição | um `FlxTimer` do tamanho do trecho chama `playCurSongPreview()` de novo | L3035-3038 |
| `curSelected == 0` | toca `freeplayRandom` em vez do instrumento | L2978-2986 |

Então o preview é **o primeiro 20 % do instrumental**, em laço infinito, com 2 s
de entrada e 2 s de saída. O port tem o `stems.pcm` por música (4 canais:
instL, instR, vozesL, vozesR) e `amix_somar(..., vozes=0)` já entrega só o
instrumento — o caminho de leitura existe; falta a janela e o fade.

**2. Os jingles do rank** (`FreeplayState.hx` L1190-1197 e L1232-1246), 8 sons de
`preload/sounds/ranks/` (`rankinbad`, `rankinnormal`, `rankinperfect`,
`loss`, `good`, `great`, `excellent`, `perfect`): o primeiro par toca em +0,1 s
pelo rank NOVO e o segundo no slam, pelo mesmo rank. São one-shots curtos: o
caminho mais barato é o que a tela de game over já usa (`audio_ring`
overlay + `amix_somar_estereo`), sem RAM residente.

**3. O `getMusicPreviewMult()`** (`BaseFreeplayDJ.hx` L84-127): o DJ baixa o
volume do preview conforme o tempo parado (`timeIdling`) — mais um motivo para
o estado do DJ já estar no port antes do áudio.

## O que ainda falta, com o motivo medido (estado nesta build)

Tudo o que está abaixo é **dado ausente ou espaço de VRAM**, não lógica que
faltou escrever. A lista existe para não deixar nada implícito.

Uma linha **saiu** desta tabela por correção de fato: *"arte da tela de GAME
OVER"*. Essa arte **não existe no original** — `GameOverSubState.create()`
(v0.8.6, L139-172) monta só o retângulo preto, o BF pego do palco (na mesma
posição, animação `firstDeath`) e o ponto de câmera. O port chegou a desenhar
"GAME OVER", "MISSES n" e "CROSS RETRY / CIRCLE EXIT" nessa tela: texto que o
original nunca mostra, e que foi **removido** (o harness agora cobra zero quads
de texto ali). O que faltava não era arte — era o port ter inventado texto.

Uma segunda linha **saiu** pelo mesmo motivo, mas por outro caminho: *"note
splashes"*. O original cria o brilho a cada nota SICK
(`PlayState.goodNoteHit` → `Strumline.playNoteSplash`), e a arte existe no clone
(`shared/images/noteSplashes.png`, 2048×2048, 8 animações × 4 quadros de
260×298 / 274×267). O que faltava era espaço — e ele **não** foi preciso: o
atlas do HUD (`tex2`, 320×256) só é usado até y=112 (notas em `(lane*56, 0)` e
barra de vida em `(0,112) 301×10`), então a faixa `y=128..255` está vazia e a
splash coube inteira ali em **224×124** (divisor 4,75 — a maior resolução que
cabe, escolhida por medida em `tools/empacotar_splash.py`). VRAM nova: **zero**.
O arquivo `splash.rgba` é copiado para dentro de `g_hud_pixels` antes do upload
único do HUD, exatamente como a barra de vida. Provado no harness: 29 SICKs →
29 splashes, pico de 1 na tela, `splash_arte=1` (`tools/run_gameplay_harness.sh`).

Uma terceira linha **saiu** por correção de fato: *"ícone da pausa"*. O
`PauseSubState.hx` de v0.8.6 **não usa imagem nenhuma**: `buildBackground()`
(L404-420) monta só um retângulo preto com alpha 0 animado até **0,6** em 0,8 s
(`FlxEase.quartOut`, L612) e `clearAndAddMenuEntries()` (L923-950) desenha as
entradas como `AtlasText` `BOLD` em `y = 70*i + 30`, `x = 0`, com as letras
dobradas de tamanho. `pauseAlt/` existe no clone (`bfLol.png`, `pauseBG.png`,
`pauseUI.png`) e **não é referenciado por nenhum fonte da v0.8.6** — é arte de
outra tela/versão. O port já desenha exatamente esse fundo e essas posições
(`src/main.c`, bloco "SOBREPOSIÇÃO DA PAUSA"), então o que faltava não era arte:
era a arte não existir nessa tela.

A mesma verificação vale para as *cápsulas da Freeplay*: `shared/images/` do
clone esparso tem 27 entradas e **nenhuma** pasta `freeplay`. Não é "arte que o
empacotador não pegou": o clone precisa de `git sparse-checkout add`.

A pasta da Freeplay **nao** falta no clone: ela esta' em `preload/images/freeplay`
(53 entradas), e o clone esparso ja' a tem desde `preload/images`. A linha
antiga desta tabela mandava procurar em `shared/images/freeplay`, que nao existe
no repositorio da arte -- o caminho estava errado, nao o dado. Medido:
`git ls-tree -r HEAD | grep -i freeplay` -> 46 arquivos, entre eles
`freeplay/freeplayCapsule/capsule/freeplayCapsule{,_pico}.{png,xml}`.

| Falta | Por que | O que precisa |
|---|---|---|
| *nenhum item de LÓGICA* | -- | -- |
| o CARTAO do PS2 (`mc0:` de verdade) | o formato, a gravação, a leitura e a recusa de arquivo estragado já rodam no PC (`FNF_CARD=1` + `tools/run_save_harness.sh`, com o CRC conferido pelo zlib do Python). O que **não** roda no PC é o `mcInit()`/os IRX do cartão e o sistema de arquivos dele | no console, com um cartão: o log tem de mostrar `save_gravado,1960,0` no fim da primeira música e `save_estado,1,1960` no boot seguinte |
| `MULTIPLY` e `SCREEN` (as outras duas misturas) | o GS só faz **NORMAL** e **ADD** na pipeline fixa. Dos 16 props de palco que o script do palco mistura, 13 pedem ADD (feito) e 3 pedem MULTIPLY/SCREEN -- `phillyStreetsErect/grey2` (MULTIPLY), `sserafim/truckLight1` e `sserafim/backLightColor` (SCREEN); os dois últimos nascem com `alpha = 0` e nem aparecem | não é dado ausente. Ou um segundo passe com o frame buffer como textura, ou aceitar a diferença e registrá-la -- o port **conta** cada caso no log (`palco_blend,aditivos,sem_gs`) em vez de fingir |
| os SONS da cinemática de rank | a cinemática em si está **fechada** (arte, câmeras, zooms, posições, selo, faíscas, vinheta e o fim em 2,2 s, tudo medido no harness). O que ficou de fora são os cinco `.ogg` de `preload/sounds/ranks/`: `rankinbad`/`rankinnormal`/`rankinperfect` em +0,1 s e `loss`/`good`/`great`/`excellent`/`perfect` no slam (`FreeplayState.hx` L1190-1197 e L1232-1246) | empacotar em PCM 48 kHz como `tools/empacotar_audio.py` já faz com o resto, e tocar pelo mesmo `audio_mix` (é o próximo passo, e não bloqueia a tela) |
| o `sparkle` da capsula e o coração de favorito | `SongMenuItem.hx` L156-166 (`sparkle`, 10 quadros, escala 0,8, `BlendMode.ADD`, alpha 0,7, invisível ate' um `FlxTimer` de 1,2-4,5 s o sortear em volta do selo) e L208/L218 (`favIcon`/`favIconBlurred`) | arte empacotada (10 quadros de 31 a 59 px) + o sorteio do `sparkleEffect`. E' efeito decorativo; o selo em si ja' esta' com ADD, alpha e cinza do original |
| o COMPORTAMENTO dos scripts de palco | os `.hxc` do clone (`preload/scripts/stages/`, 19 arquivos) criam coisas que **nao estao no JSON** -- os `mist1..5` do `limoRideErect` sao `FlxBackdrop` criados por script -- e mexem em alpha/blend por evento (relampago do `phillyBlazin`, luzes do `sserafim`, `spookyMansionErect` trocando quem aparece) | um interpretador de HScript, ou a conversao manual de cada evento. Hoje o port usa dos scripts so' o **estado inicial** de blend/alpha dos props do JSON, extraido por `tools/extrair_blend_palcos.py` com a linha da fonte em cada ficha. O resto esta' contado: 77 linhas de `alpha`/`blend` fora do estado inicial nos 19 arquivos |

#### A tela de Resultado: a arte entrou

`ResultState.hx` (v0.8.6) monta a cena com folhas Sparrow animadas. Tres delas
sao as folhas do `resultsAnim`/`ratingsPopin`/`scorePopin` e estao empacotadas e
desenhadas (ver abaixo); o resto da cena -- placar, percentual, contadores, barra
do topo, nome da musica, a arte do rank e as animacoes do personagem por rank --
entrou depois, e cada um tem a sua secao com o custo e o instante medidos.

O que roda hoje (`tools/run_gameplay_harness.sh`, 5600 quadros):
`resultado_arte` = 3 folhas abertas, `resultado_fundo` = 1, e as folhas entram
nos **timers exatos da fonte** — 250 ms (6/24 s), 883 ms (21/24 s + 1 quadro) e
1500 ms (36/24 s) —, com o ultimo quadro congelado na tela (`looped = false` no
flixel para, nao some). O caminho e' o mesmo dos personagens: CFZ4 lido quadro a
quadro por `engine/pack_stream.c`, um quadro por vez na VRAM, e a VRAM vem do
pool — o palco ja' foi solto quando a tela abre (1.269.760 B livres medidos,
contra os 480.968 B que a tela pede).

O fundo e' o `FlxGradient` de 90 graus da fonte (0xFFFECC5C -> 0xFFFDC05C) numa
textura de **16x360** esticada na horizontal: 23.040 B e um quad, em vez de 360
retangulos de cor por quadro.

#### A tela de Resultado: o RANK entrou (arte do original)

O rank nao e' mais texto do port: os seis pares que o `ResultState.hx` usa
(`displayRankText`, L660-700) estao empacotados e desenhados — `LOSS`, `GOOD`,
`GREAT`, `EXCELLENT`, `PERFECT` e `PERFECT_GOLD` (que reusa a arte do
PERFECT, como no original). Cada rank e' um par de imagens: a **vertical** do
`rankText` a esquerda e a **horizontal** por cima, ambas em `Paths.image`
(estaticas, um quadro so') e por isso guardadas em RGBA cru, na metade:

| rank | vertical (0,5) | horizontal (0,5) |
|---|---|---|
| LOSS | 20x121 | 245x26 |
| GOOD | 20x152 | 244x26 |
| GREAT | 20x216 | 297x26 |
| EXCELLENT | 20x278 | 524x26 |
| PERFECT / PERFECT_GOLD | 20x216 | 408x26 |

O pior par custa **76.736 B** de VRAM — e so' um par fica residente por
partida (o rank so' e' conhecido no fim), entao o custo real e' esse, contra
os 1.269.760 B livres da tela. Medido no harness: `resultado_rank_arte,1,43048`
(o par do GREAT que a musica do teste alcanca).

A ENTRADA dele e' a da fonte, ao milesimo: `Scoring.hx` L568-586 da' o
`getFlashDelay()` por rank — 4.542 ms no GREAT, 4.458 no GOOD, 5.083 no
EXCELLENT, 5.375 no PERFECT e 7.750 no SHIT. O harness confere o relogio:
`resultado_rank,2,4550` — 8 ms de diferenca, que e' a amostragem de quadro
(o valor e' medido no primeiro quadro depois do timer, nao dentro dele).
O flash da tela (14/24 s de alpha 1 -> 0 em `(255,241,166)`), o piscar do
vertical (250 ms alternando a cada 83 ms) e as 12 linhas horizontais em
`y = 60 + 67,5*i` com `vel.x = -/+3,5` e espaco de 5 px tambem sao os da
fonte, com as formulas isoladas em `engine/resultado.c` (funcoes puras, com a
linha do original no comentario) e testadas em `test/test_resultado.c`.

#### O SAVE: o recorde por musica e dificuldade, e o SELO DE RECORDE

`ResultState.hx` L415-426 so' desenha o `highscoreNew` quando
`params.isNewHighscore`. Esse valor vem de
`Save.instance.isSongHighScore(...)` (`PlayState.hx` L3503), isto e', do SAVE do
jogo. Ate' esta build o port nao tinha save nenhum, entao o campo era dado
ausente e o selo nunca aparecia. Agora:

* **`engine/save.c`** guarda `score` + `tallies` (sick/good/bad/shit/missed/
  combo/maxCombo/totalNotesHit/totalNotes) por musica x dificuldade (27 x 3),
  com a regra da fonte: `isSongHighScore` (Save.hx L568-582: **sem registro ->
  `true`**, senao `score >` o guardado) e `applySongRank` (L531-556: maior
  placar + tallies do melhor rank; no empate, os do maior `tallyCompletion`).
  O rank NAO tem formula paralela: o registro e' montado num `fnf_play_t` e
  quem responde e' `fnf_play_rank`, a mesma funcao que julgou a musica.
* **O formato** e' de tamanho fixo (1.960 B: `FNFS` + versao + contagem + CRC32
  + 81 registros de 24 B). Arquivo curto, de outra versao ou com CRC errado e'
  recusado INTEIRO -- o jogo comeca como quem nunca jogou, nunca com numero
  inventado (`test/test_save.c` prova as cinco recusas).
* **O arquivo** vai para o memory card: `mc0:/FNFPS2/save.dat`. Sem cartao o
  `open()` falha na hora (nada trava) e o log diz `save_estado,0,-1` e
  `save_gravado,0,-1`; o save fica na RAM e a tela e' a mesma.
* **O selo** (`highscoreNew`) entrou como arte: Sparrow de **29 quadros** a
  24 fps, canvas de 844x99 no original -> **422x50 a 16 bpp = 42.200 B** de
  VRAM (cabe nos 175.616 B que sobram no pior rank). Ele entra no
  `rank.getHighscoreDelay()` (8625 ms no SHIT, 5292 no GOOD, 5375 no GREAT,
  5833 no EXCELLENT/PERFECT -- `Scoring.hx` L587-604: 207, 127, 129 e 140 /24) e,
  depois da primeira passada, repete do quadro 16. O porque do 16..28, conferido
  no flixel do projeto (`FunkinCrew/flixel` 141f23c4): `addByPrefix(..., 24,
  false)` deixa `looped = false` e `FlxAnimation.play(Force, Reversed, Frame)`
  nao mexe em `looped` nem em `loopPoint` -- o terceiro argumento e' so' o quadro
  inicial. Entao a passada 0..28 termina, dispara `onFinish`, o `onFinish` faz
  `play(..., 16)`, essa roda 16..28 e termina de novo, disparando o mesmo
  `onFinish`. O ciclo e' essa corrente, nao um `looped` do flixel (com `looped`
  de verdade o `loopPoint` 0 faria voltar ao quadro 0). zIndex 1200: e' o
  ultimo grupo da tela.

  Obtido por medicao, nao por leitura do XML: o canvas de cada quadro do
  `highscoreNew` **nao** e' uniforme -- o primeiro tem `frameY = -25`, e o
  empacotador une os 29 num canvas unico de 844x99 e reescreve os offsets. Sem
  isso o selo "pula" ao longo da animacao.

Medido no harness (sem cartao, entao save vazio): `save_estado,0,-1`,
`save_resultado,1,1` (musica 0/dificuldade 1, e' recorde), `resultado_highscore,1,5375`
e `resultado_selo_desenho,0,5834` -- o selo desenhou o quadro 0 no instante do
timer.

#### O SAVE no PC: `FNF_CARD=1` e `tools/run_save_harness.sh`

O save mora no cartao, e no PC nao ha' cartao -- entao tudo o que dependia dele
(save, recorde, selo de recorde e o selo de rank da Freeplay) rodava sempre no
ramo "sem cartao". O harness agora mapeia `mc0:/` para `<raiz>/FNFPS2/` quando
`FNF_CARD=1` (`ps2h_map_path`); sem a variavel o caminho vai para um diretorio
que nunca existe, que e' exatamente o comportamento de um console sem cartao.

`tools/run_save_harness.sh` (entra no `test/run_tests.sh`) faz a volta inteira:

| passo | o que cobra |
|---|---|
| 1. partida com cartao vazio | `save_estado,0,-1` e `save_gravado,1960,0` |
| 2. o arquivo em disco | 1.960 B, `FNFS`, versao 1, 81 registros, **CRC32 conferido pelo zlib do Python** (nao pelo codigo que escreveu), exatamente 1 registro valido, na musica 0/dificuldade 1 |
| 3. a partida seguinte | `save_estado,1,1960`, `freeplay_selo,1,1104`, `freeplay_selo_ranks,1,1` e o selo de rank desenhado |
| 4. troca de dificuldade | o selo SOME (0 ranks na dificuldade sem registro) |

O que isso **nao** prova: o cartao do PS2 (`mcInit`, os IRX, o sistema de
arquivos do cartao). Prova o formato, a gravacao, a leitura, a recusa de arquivo
estragado e o caminho de desenho -- que sao a parte que o port escreveu.

Dois defeitos que o teste expos e que foram consertados:

* `save_estado,0,-1` significava **duas coisas** ("sem cartao" e "arquivo
  curto"), porque `SAVE_ERR_CURTO` era -1. Os erros passaram a -2..-6 e -1 e'
  so' `SAVE_SEM_ARQUIVO`;
* o log do harness e' aberto **em anexo**: sem apagar, o `awk` lia as linhas da
  rodada anterior e o teste dizia "o segundo boot nao leu o arquivo" com o
  arquivo la'.

#### Corrigido: a capsula era desenhada no tamanho do SELO (18,4x19,2 px)

Achado ao refazer os pacotes com o clone de arte completo. `empacotar_freeplay.py`
gravava `FP_DESENHO_W/H`, `FP_Y_PASSO` e `FP_TEXTO_X` a partir de `cw`, `ch`,
`w`, `h` — variáveis que sobravam do laço do **selo de rank** (52x54) e não
foram reatribuídas antes do bloco de dados. Efeito, medido no cabeçalho
anterior: `FP_DESENHO_W = 18,400` e `FP_DESENHO_H = 19,200` (o selo a 0,8), o
passo vertical `26,600` em vez de `57,800`, e o título em `x = 5,408` em vez de
`63,648`.

A fonte diz outra coisa, e toda ela já estava citada no empacotador:

| grandeza | fonte | valor certo |
|---|---|---|
| `intendedY(index)` | `SongMenuItem.hx` L766-769: `index*((capsule.height*realScaled)+10)+120`, `capsule.height = 132` | `(132*0,8+10)*0,5 = **57,8**` |
| desenho da capsula | L669-670: `capsule.scale *= realScaled` (`0,8`) sobre 612x132 | `**244,8 x 52,8**` |
| `songText` | L191: `CapsuleText(capsule.width*0,26, 45, ...)` | `x = 612*0,26*0,8*0,5 = **63,648**` |

O erro passou por 4 harnesses porque nenhum deles mede tamanho de cápsula: o da
Freeplay conta quads (9), o selo, o DJ e a volta ao menu. `test/test_freeplay.c`
já cobrava `57,8` (número escrito à mão a partir da fórmula da fonte) e era o
único que pegava — ele estava **reprovando** desde `dbe6288`, e a suíte inteira
não tinha sido rodada depois daquele commit. Agora passa, e o gerador lê o
tamanho de `dado_capsula["src_w"/"src_h"]` (nada de variável solta).

#### O SELO DE RANK na capsula da Freeplay

Cada capsula mostra o rank que o jogador tirou naquela musica **naquela
dificuldade** (`FreeplayRank`, `SongMenuItem.hx` L821-901). E' a primeira coisa
do jogo que o save alimenta de volta na interface.

| | |
|---|---|
| arte | `freeplay/rankbadges`, 6 animacoes, 85 quadros no total |
| quadro | canvas comum 52x54 -> **23x24 a 16 bpp = 1.104 B**, um por quadro |
| VRAM | um slot **por rank** (`FP_CHAVE_RB + 0..5`): a animacao so' difere entre ranks |
| animacao | 13 a 17 quadros a 24 fps, **uma vez** (`looped = false`), para no ultimo |
| posicao | `FreeplayRank(420, 41)` no espaco do item -> (210, 20,5) em 640x360 |
| alpha | `ranking.alpha = isSelected ? 1 : 0,7` (`updateSelected`) |
| cor | `ranking.color = isSelected ? 0xFFFFFFFF : 0xFFAAAAAA` (`updateSelected`) |
| blend | `ranking.blend = BlendMode.ADD` (`set_rank`, L830) |

Quem escolhe a animacao e' `ScoringRank.getFreeplayRankIconAsset()`
(Scoring.hx L606-627): PERFECT_GOLD usa a arte dourada (`PERFECTSICK`), SHIT usa
`LOSS`, o resto pelo proprio nome. O unico deslocamento que sobrevive ao
`switch` do `set_rank` e' `offset.y -= 8` no GOOD e no GREAT: no Haxe **nao ha'
fallthrough** ("Case body expressions never fall through", manual 5.5) e o corpo
de um `case` e' opcional, entao os `case` com so' comentario dentro sao no-op.

Trocar de dificuldade refaz a conta para todas as musicas
(`freeplay_selo_ranks`, telemetria com quantas tem rank e em que dificuldade).

**O `blend = BlendMode.ADD` do selo entrou nesta build**, com o cinza do
nao-selecionado junto. O GS faz ADD mexendo no registrador `ALPHA` no meio do
pacote (`GS_SET_ALPHA(0,2,2,1,0x80)`) e restaurando o normal
(`GS_SET_ALPHA(0,1,0,1,0x80)`) logo depois do sprite: 4 qwords por selo, zero
VRAM. Os valores do port: selecionado `alpha = 0x80` e cor `0x80` (branco);
nao-selecionado `alpha = 0x5A` (0,7) e cor `0x55` (o `0xAA` do flixel dividido
por 2, que e' a convencao do port para o `color` do sprite). O harness do save
continua vendo o selo desenhar (`selo desenhado: rank 2 em 1 quadro(s)`).

Fica de fora, e esta' na tabela de pendencias: o `sparkle` (10 quadros, escala
0,8, ADD, alpha 0,7, sorteado por `FlxTimer` em volta do selo).

#### A tela de Resultado: o PERSONAGEM DO RANK se move

O que faltava depois do rank em si: as animacoes do proprio personagem por rank
(`ResultState.hx` L201-330 monta, L704-740 dispara). Cada rank tem uma ou duas
entradas em `players/<dono>.json` -> `results[rank]`, com `zIndex` 499..501,
`delay`, `startFrameLabel`, `loopFrameLabel` ou `loopFrame`, e uma escala e um
posicao que vem do JSON. O empacotador (`tools/empacotar_resultado_rank.py`)
resolve o `scriptClass` lendo o `.hxc` de `preload/scripts/players/results/bf/`,
descarta o filtro `safe` (o jogo usa `naughtyness = true`) e escreve um CFZ4 por
entrada. Numeros MEDIDOS do empacotamento (16 bpp):

| rank | VRAM | entradas |
|---|---|---|
| PERFECT_GOLD / PERFECT | 194.744 B | 300x319/149q (`bed`) + 44x38/116q (`hearts`) |
| EXCELLENT | 308.320 B | 376x410/260q |
| GREAT | 122.240 B | 144x170/44q (`gf`) + 229x160/39q (`bf`) |
| GOOD | 224.540 B | 328x262/74q (`bf`) + 126x209/52q (`gf`) |
| SHIT | 271.440 B | 360x377/319q (`resultsSHIT`) |

O `PERFECT` e o `PERFECT_GOLD` usam a MESMA arte, e o empacotador reaproveita o
pacote (nao ha' dois arquivos iguais no pendrive). O `SHIT` cru dava **374.778
B** e NAO cabia nos 350.208 B livres (depois de o atlas do placar ir para 16
bpp): o empacotador degrada em passos de 0,85 **so' o pacote**, nunca o tamanho
na tela -- `escala_na_tela = w * escala_JSON * 0,5/s_pack` -- e a escala que
sobrou (1,177) mantem os 423 px de largura do original.

No binario, `engine/resultado_rank.c` guarda a regra do tempo como funcao pura
(`res_rr_quadro`): invisivel antes de `getBFDelay() + delay`, comeca no rotulo
inicial, recomeca no rotulo/quadro do loop com periodo `quadros - loop_ini` e,
sem loop, PARA no ultimo quadro. O pacote e' lido pelo `pack_multi` (tabela na
RAM, um bloco do pendrive por quadro) e so' sobe para a VRAM quando o quadro
MUDA.

Provado no harness com os seis ranks forcados (`FNF_RANK`):

```
rank 0 SHIT          entradas=1 VRAM=271440 livre=217856  entra em 3967 ms
rank 1 GOOD          entradas=2 VRAM=224540 livre=264704  3967 / 4884 ms
rank 2 GREAT         entradas=2 VRAM=122240 livre=358912  4217 / 3967 ms
rank 3 EXCELLENT     entradas=1 VRAM=308320 livre=139520  4051 ms
rank 4 PERFECT       entradas=2 VRAM=194744 livre=269824  3967 / 8384 ms
rank 5 PERFECT_GOLD  entradas=2 VRAM=194744 livre=269824  3967 / 8384 ms
```

Os instantes batem com o plano (3958 / 4208 / 4868 / 8368 ms) dentro de dois
quadros de video -- e' o `getBFDelay()` do rank mais o `delay` da entrada, que
e' como o `hearts` do PERFECT entra 4,41 s depois do `bed`. O harness cobra
entrada a entrada (`resultado_rrf_entra`) e cobra que o pior rank caiba nos
350.208 B; `test/test_resultado_rank.c` prova as quatro regras de tempo no PC,
sem console.

#### A tela de Resultado: o PLACAR entrou (os digitos do original)

O numero do placar nao e' mais texto: sao os dez `ScoreNum` do `ResultScore.hx`
(L108-180 e `ResultState.hx` L151), cada um com as 12 animacoes de
`resultScreen/score-digital-numbers` — `ZERO`..`NINE` com **5 quadros** cada
mais `DISABLED` e `GONE` com um, **52 quadros** ao todo, num atlas RGBA de
**560x227** na metade da resolucao.

Por que um ATLAS e nao CFZ4 (o caminho das folhas): na tela os dez slots
mostram digitos diferentes AO MESMO TEMPO e, durante o shuffle, qualquer valor
pode cair em qualquer posicao. Nao existe "um quadro por vez": o conjunto
inteiro precisa estar residente. Custo medido: **508.480 B** de VRAM, o maior
item da tela — e ele cabe porque o palco ja' foi solto (1.261.568 B livres
medidos em `pool_cena,9`; a tela ocupa 1.032.496 B com folhas + rank + placar).

O TEMPO e' o do original, em 24 avos de segundo (`ResultScore.hx`):

| quando | o que acontece |
|---|---|
| `37/24 s` | `score.visible = true` e `animateNumbers()` |
| `(i-1)/24 s` | o slot i comeca a embaralhar |
| `41 x 1/24 s` | `shuffle()`: o digito sobe de 1 em 1, 10 -> 0 -> 1 ... |
| `23/24 s` | `FlxTween.num(0 -> finalDigit, quadOut)` |
| `finalDelay/24 s` | a animacao do digito final recomeca no quadro 0 (o piscar) |

Duas coisas que a leitura da fonte obrigou a corrigir no port:

- **Os timers dos slots partem de `(i-1)/24 s`, nao de 37/24 s** — ou seja o
  shuffle ja' esta' correndo quando o placar aparece. A primeira troca de cada
  slot (o `glow`, que toca no quadro 0) acontece FORA da tela; o port nao
  "conserta" isso puxando o timer, porque isso seria mudar o tempo do original.
- **`quadro == 4` nao quer dizer "parado"**: durante o shuffle o slot tambem
  desenha o ultimo quadro. Por isso o "assentou" e' por TEMPO
  (`res_score_assentado`), e e' ele que libera a telemetria do numero final.

Medido no harness (`tools/run_gameplay_harness.sh 5600`):
`resultado_placar_arte,1,508480`, `resultado_placar,10,1550` (dez slots
desenhados no primeiro quadro depois de 1.542 ms) e
`resultado_placar_final,13794,3216` — o numero montado dos DIGITOS QUE ESTAO NA
TELA igual ao `play_result_score,13794` da musica, 8 ms depois do previsto pelos
timers (77/24 s = 3.208 ms; a diferenca e' a amostragem de quadro). O harness
cobra os tres.

#### A tela de Resultado: os CONTADORES DE JULGAMENTO

Os sete `TallyCounter` do `ResultState` (`TallyCounter.hx`) tambem sairam do
texto: o atlas `resultScreen/tallieNumber` (dez glifos, um quadro cada) entra
empacotado por recorte num **96x44 = 16.896 B**, e cada contador desenha os
seus digitos com **21,5 px** de espacamento (43 px em 1280), na cor do
`flavour` do original:

| contador | posicao (640x360) | cor |
|---|---|---|
| notas acertadas | (187,5, 75) | branco |
| maior combo | (187,5, 100) | branco |
| SICK | (115, 138,5) | `0x89E59E` |
| GOOD | (105, 165,5) | `0x89C9E5` |
| BAD | (95, 192,5) | `0xE6CF8A` |
| SHIT | (110, 219,5) | `0xE68C8A` |
| erradas | (130, 246,5) | `0xC68AE6` |

Cada um aparece em `(0,3*i) + 1,20` s e sobe de 0 ate' o alvo num tween de
0,5 s com `FlxEase.quartOut` (`1 - (1-t)^3`), arredondado como o `Math.round`
do original — o desenho e' por TEMPO, nao por estado acumulado, entao nao ha'
contador paralelo para divergir. Acima de 1000 notas acertadas, os dois
primeiros andam 30 px para a esquerda, como em `ResultState.hx` L446-450
(senao o numero de quatro digitos sairia da tela).

Duas coisas de fidelidade que apareceram aqui:

- **O number exibido vem do lugar certo**: no Story Mode o `ResultState` usa
  `Highscore.talliesLevel` (a SEMANA), nao os tallies da musica
  (`PlayState.hx` L3873). O port somava so' o placar da campanha; agora soma
  julgamentos, erradas e maior combo tambem (e zera junto, nos tres pontos em
  que a campanha zera).
- **`totalNotesHit` nao e' "notas da musica"**: e' quantas notas pontuaveis
  foram ACERTADAS (`PlayState.hx` L3161 conta uma vez por acerto), ou seja
  `sick + good + bad + shit`. As erradas nao entram.

Medido no harness: `resultado_tallies_arte,1,16896` e os sete
`resultado_tally,<campo>,<numero>` — 29 acertadas, 10 de combo, SICK 29, GOOD 0,
BAD 0, SHIT 0, 3 erradas, iguais ao `play_result_tallies`/`play_result_score` da
musica. A cor vai no vertice (o GS multiplica a textura) com `flixel / 2`,
porque no GS `0x80` e' 1,0.

#### A tela de Resultado: o PERCENTUAL DE CONCLUSAO

O `ClearPercentCounter` entrou com os cinco pedacos do original: os tres atlas de
digitos (`Left` e `Right`, as duas inclinacoes do glifo grande, e o `Small` do
`%` da barra) e os dois textos (`CLEAR` com o `%` embaixo, e o `%` pequeno).

**A primeira degradacao de imagem que o orcamento exigiu**, e ela esta' medida:
os cinco atlas na metade custariam **190.348 B**, e depois do placar (508.480 B)
e dos contadores (16.896 B) nao havia isso no pool. Os glifos grandes foram
entao guardados a **0,4** do original (desenhados a 1,25x, `fator` no plano):
**129.452 B**, e nenhum evento mudou — so' a nitidez. Com isso a tela inteira
fica em 1.178.844 B dos 1.261.568 B livres (`pool_cena,9`), e **nenhuma** das
treze alocacoes falhou (`resultado_sem_vram` nao aparece no log).

O contador do meio da tela:

| quando | o que acontece |
|---|---|
| `37/24 s` | nasce em (415, 145) com `alvo - 36` (`clearPercentLerp`) |
| `58/24 s` | sobe ate' o alvo: `FlxTween` quartOut, arredondado no desenho |
| `+0,40 s` | `flash(true)` (o `PureColor` branco), 0,4 s |
| `+0,25 s` | alpha 1 -> 0 em 0,5 s, com 0,5 s de atraso |
| `+1,00 s` | `remove()`, o contador sai da cena |

A posicao de cada digito e' a da fonte: `(ind + digitOffset) * 72` px
(`digitOffset` = 1 com um digito, -1 com tres, 0 no resto), `y = 72` abaixo do
texto, com o glifo `Left` so' no primeiro de tres digitos (`res_cp_variante`).
No Story Mode o alvo e' o percentual da SEMANA, como os tallies.

**Falta uma coisa medida nesta peca**: o `flash(true)` do contador e' um shader
`PureColor` que troca a COR do sprite por branco e mantem o alpha. No GS isso e'
`TFX = DECAL` (a cor vem do vertice, o alpha da textura) — o port desenha tudo
em `MODULATE`, entao o flash ainda **nao** esta' desenhado: nao e' um evento que
se possa fingir com tint, e trocar o TFX por desenho nao se prova no harness
(ele conta quads; quem prova cor e' a tela do console). Fica registrado como
pendencia com o caminho tecnico pronto (o `res_cp_flash` ja' diz a janela).

A **barra do topo** e a **imagem da dificuldade** sairam desta lista na build
seguinte, e o **nome da musica** na seguinte — ver as secoes abaixo. Continuam
sem arte o `highscoreNew` e o `results-bf`/`results-pico`, com o custo ja'
medido na tabela abaixo.

#### A barra do topo e a imagem da dificuldade (medido)

`ResultState.hx` L332-334 e L342-360, `BitmapUtil.createResultsBar`. Duas pecas
com naturezas opostas:

**A barra nao tem PNG — e nunca teve.** `BitmapUtil.createResultsBar()` monta um
`BitmapData` de **1295x149** cheio de preto, aplica `matrix.rotate(-3,8°)`,
`matrix.translate(-15, 0)` e recorta em `(0, 0, 1280, 149)`. O que sobra na tela
e' "tudo acima de uma reta inclinada". Reproduzir isso com 1295x149 de textura
(191.512 B a 0,5) seria empacotar um retangulo preto — e o recorte do GS ainda
cortaria em 1024 px de largura. Entao o port desenha a **geometria**: calcula a
reta da borda de baixo com a mesma matriz (0,5 de escala: **y = 74,2 em x = 0** e
**y = 31,7 em x = 640**) e emite **um trapezio** (`barra_trapezio`, PRIM
`TRIANGLE_FAN` de quatro vertices, o mesmo formato de qwords que o `draw_rect_filled`
do ps2sdk usa). Custo: **0 byte de VRAM e 3 qwords por quadro** — medido no
harness (`resultado_barra,742,317`, ou seja 74,2 e 31,7 em decimos).

O movimento e' o do original, sem atalho: `y = -height` ate' **3/24 s** (125 ms) e
depois `FlxTween` de **7/24 s** com `quartOut` ate' 0; o `onComplete` do tween e'
o que torna o nome da musica visivel (`res_barra_assentada` marca esse instante).

**A imagem da dificuldade e' um arquivo por partida.** `diff_${difficultyId}`
com 555 (x, em 1280x720) e `y = -height`, um tween de **0,5 s** com `expoOut` e
**0,8 s** de atraso ate' `122 + (blackTopBar.height - 148)` = 123 na fonte (61,5
na metade). Cinco arquivos existem no clone, mas o port tem tres dificuldades —
carrega **um**:

| dificuldade | 0,5 | VRAM |
|---|---|---|
| `diff_easy` | 78x26 | 8.112 B |
| `diff_normal` | 114x29 | 13.224 B |
| `diff_hard` | 80x26 | 8.320 B |
| `diff_erect` (empacotado, inalcancavel hoje) | 101x28 | 11.312 B |
| `diff_nightmare` (idem) | 164x32 | 20.992 B |

O `LeftMaskShader` do original **nao recorta nada**: `swagMaskX = difficulty.x - 30`
(L357) e `swagSprX = difficulty.x` (L811, dentro do `update`), entao
`cutOff / textureWidth` e' negativo e `uv.x < negativo` nunca vale. A condicao
esta' registrada no codigo para ninguem "consertar" um shader que nao recorta.

**Orcamento depois destas duas pecas** (medido, `pool_cena,9 = 1.261.568` B
livres ao entrar na tela):

| peca | bytes |
|---|---|
| tres folhas animadas (`results`, `ratingsPopin`, `scorePopin`) | 457.928 |
| gradiente do fundo (16x360) | 23.040 |
| placar (52 quadros de digitos num atlas) | 508.480 |
| percentual (3 atlas + 2 textos) | 129.452 |
| rank (o par maior, PERFECT) | 76.736 |
| contadores de julgamento | 16.896 |
| imagem da dificuldade (normal) | 13.224 |
| nome da musica (o titulo mais largo, `winter-horrorland`) | 28.836 |
| **total** | **1.254.592** |
| **sobra** | **6.976** |

#### O que ainda falta na tela de Resultado, medido nesta build

| peca | estado | numero medido |
|---|---|---|
| `bgFlash` (o flash amarelo, zIndex 20) | **feito** | oito faixas de cor por `res_flash_cor`, alpha 1->0 em 5/24 s e em 14/24 s |
| `soundSystem` (a maquina que cai atras do placar) | **feito** | 65 quadros a 0,235 do original (canvas 206x226 = **186.224 B**), um quadro por vez na VRAM |
| animacao do personagem por rank (`results-bf`/`results-pico`) | **formato** | Animate Atlas multi-simbolo (`AN`/`SD`/`TL` com `M3D` por instancia): nao e' sequencia de quadros, e' timeline com hierarquia |
| `highscoreNew` | **dado ausente** | `params.isNewHighscore` vem do save; o port nao tem save, entao o selo nao e' desenhado (e' o `else` da fonte) |
| `results-bf`/`results-pico` como PASTA de arte | **nao e' do jogo** | nenhum `.hx` da v0.8.6 referencia a pasta: so' os `Animation.json`/`sparrow` de dentro dela |

O que **fechou nesta build**: a `bgFlash` (que ja' estava no codigo, e aqui
esta' registrada como feita) e a **maquina do `soundSystem`**, a unica peca da
tela que toca em TODA partida. Ela entrou trocando fidelidade de imagem por
espaco, como a regra do projeto manda -- e a troca esta' medida:

| peca | antes | agora | VRAM |
|---|---|---|---|
| tres folhas (`results`, `ratingsPopin`, `scorePopin`) | 0,5 do original | **0,4** (desenhadas a 1,25x) | 462.452 -> **295.560** |
| fundo | textura 16x360 esticada | **oito faixas** (`res_bg_cor`, 0 textura) | 23.040 -> **0** |
| maquina (`soundSystem`) | fora da tela | canvas 206x226 (0,235 do original) | **186.224** |

Medido no harness (`tools/run_gameplay_harness.sh 7600`):
`resultado_soundsystem,1,186224` (abriu), `resultado_maquina,334,438` (o
primeiro quadro desenha em 334 ms, largura 438,3 px -- o `FlxTimer` de 8/24 s
e' 333 ms) e `resultado_vram,96000` livre **depois** de toda a arte da tela.
Nenhum `resultado_sem_vram`.

A escala nao foi escolhida a mao: `tools/empacotar_soundsystem.py` desce de 0,5
em passos de 0,005 ate' o canvas caber no `--limite` (190.000 B, o que as outras
duas trocas liberam), e o desenho multiplica por `fator = 0,5 / escala`, entao a
maquina sai no MESMO lugar e no MESMO tamanho da fonte. O que muda e' a nitidez,
nunca o evento: os 65 quadros, os 24 fps, o congelamento no ultimo quadro
(`looped = false`) e o zIndex 1100 continuam os da fonte.

O que **sobrou** na lista, com o motivo medido:

- **`highscoreNew`** nao e' "arte que nao cabe": e' **dado ausente**. A fonte o
  desenha so' quando `params.isNewHighscore` (L417), que vem do save; o port nao
  tem save (nem memory card), entao o caminho de desenho e' o `else` da fonte
  (`highscoreNew.visible = false`). Guardar 47.424 B de arte para um selo que
  nunca pode acender seria pior que nao te-lo.
- **a animacao do personagem por rank** (`results-bf`/`results-pico`) e' um
  problema de **formato**, nao de espaco: os `Animation.json` dessas pastas sao
  Animate Atlas **multi-simbolo** (`AN`/`SD`/`TL`, com `SI`/`TRP`/`M3D` por
  instancia e simbolos aninhados), isto e', uma timeline hierarquica do Flash --
  nao uma sequencia de quadros como as folhas do `resultScreen`. Portar isso e'
  escrever um *flattener* de timeline no empacotador (resolver o grafo de
  simbolos e emitir, por quadro, a lista de retangulos com matriz), e o plano e'
  esse: **flatten no PC, desenho de quadrilateros no console**, sem rasterizar
  nada em tempo de execucao. Fica registrado com o formato lido e o numero de
  cada pasta (`results-bf` 5,1 MB, `results-pico` 6,4 MB; canvas de 1003x862 a
  4094x2485).

#### O nome da musica (medido)

`ResultState.hx` L119-122 (`songName = new FunkinSprite()`), L736-765
(`timerThenSongName`) e L792-836 (`update`/`draw`). No original o titulo e' um
`FlxBitmapText` com a fonte `resultScreen/tardlingSpritesheet` e o bitmap que o
texto monta vai por cima da barra preta, girado de **-4,4°**.

**Nao ha' texto a compor no console: o titulo vai pronto.** O
`FlxBitmapText` gera um `BitmapData` unico por musica, entao o empacotador
(`tools/empacotar_nome.py`) recorta as celulas de **49x61** da folha, aplica o
`letterSpacing = -15` (avanco **34 px** em vez de 49) e salva **um** RGBA por
musica, ja' com o giro fora (o giro e' do desenho, um quadrilatero de quatro
cantos quaisquer — `sprite_quad_uv`). Largura do bitmap: `34n + 15`.

| titulo | escala | bitmap | VRAM |
|---|---|---|---|
| `satin-panties` (7 letras, 0,5) | 0,50 | 228x30 | 27.360 B |
| `winter-horrorland` | 0,45 | 267x27 | 28.836 B |
| `spaghetti` | 0,28 | 423x17 | 28.764 B |

A escala e' **por titulo** (`tools/empacotar_nome.py` desce a tabela
0,5 -> 0,45 -> ... -> 0,16 ate' o bitmap caber em 33.000 B, o mesmo teto do
pacote), nunca um numero escolhido a mao: o texto tem que caber na VRAM sem
sair da tela (423 px numa tela de 640).

O loop e' o do original, com as tres fases medidas:

- **entrada**: nasce em `x = clearPercentSmall.x + 47`, `y = -altura`, espera
  **900 ms** e sobe com `expoOut` de **500 ms** ate'
  `y = (122 - 25 - fucked) * 0,5`, onde `fucked = -(larg * 0,5) * sin(-4,4°) - 10`
  — o `songName.y` do original e' corrigido pela largura girada, e e' por isso
  que cada titulo tem o seu `y_alvo` na tabela de dados;
- **movimento**: so' comeca depois de `showSmallClearPercent()` (**+2,5 s**),
  com `vel_x = -59,82` e `vel_y = 4,60` (px/s na metade, `Math.cos/sin(rad) *
  660` no original); a velocidade usa a **integral analitica** do `quadIn`
  (`u<=0: 0; u<T: u³/(3T²); senao T/3 + (u - T)`), nao uma aproximacao — e'
  `speedOfTween = t/0,7` entrando em `songName.x += Math.cos(rad) * 660 * t`
  a cada quadro;
- **recirculacao**: quando `x + largura < 50` o texto volta para a entrada
  (espera de **1000 ms** no primeiro ciclo, **3000 ms** nos seguintes), igual ao
  `timerThenSongName(1.0, false)` que o original chama de novo.

O texto so' aparece quando a barra assenta (`onComplete` do tween da barra:
`songName.visible = true`) e e' recortado em `clipRect = max(0, 520 - x)`
(**260 px** na metade) para nao passar por cima do placar.

**Os tres sprites do topo rolam JUNTOS.** `update()` (L806-826) soma o mesmo
`speedOfTween * 60 * elapsed` em x e y do nome, da imagem da dificuldade e do
percentual pequeno, e quando `songName.x + songName.width < 100` chama
`timerThenSongName()` -- que poe os tres de volta em `y = -height`, repete o
tween de entrada de cada um (0,8 s / 0,85 s / 0,9 s de atraso, 0,5 s de
`expoOut`) e espera 3,0 s para voltar a andar. Ou seja: a cada **~12,9 s** a
dificuldade e o percentual pequeno SOBEM de novo, do mesmo jeito que o titulo.
Por isso `res_nome_t` guarda `dx`/`dy` (so' o que o scroll somou, sem a entrada)
e as entradas da dificuldade e do percentual pequeno sao medidas **desde o
inicio do ciclo** (`res_dif_y_desde`, `res_cp_small_y_desde`), nao desde a
abertura da tela.

Medido: os recomecos saem em 15.883 / 28.750 / 41.616 ms de tela (passo de
**12,87 s**), e o `tools/run_gameplay_harness.sh` cobra dois deles.

#### A musica e os dois `playOnce` do resultado (medido)

`ResultState.hx` L502-550 e L592-609. Ate' esta build a tela de resultado era
MUDA: entrava em `ui_state 9` com `audsrv_stop_audio()` e nada mais.

**A musica vem do DONO do personagem jogavel, nao da musica.**
`getMusicPath` = `playerCharacter?.getResultsMusicPath(rank) ?? 'resultsNORMAL'`
e `getResultsMusicPath` (PlayableCharacter.hx L126-145) le
`results.music.<RANK>` do `players/<dono>.json`. Neste projeto os donos sao dois:
`bf` (que possui `bf`, `bf-car`, `bf-christmas`, `bf-holding-gf`, `bf-pixel` e,
pelo `?? 'bf'`, o `sserafim-sakura` do spaghetti) e `pico` (`pico-playable`,
`pico-blazin`). Cada dono tem o SEU conjunto de arquivos:

| rank | bf | pico | `getMusicDelay()` |
|---|---|---|---|
| LOSS (SHIT) | `resultsSHIT` **+ intro** | `resultsSHIT-pico` | 2/24 s = 84 ms |
| GOOD | `resultsNORMAL` | `resultsNORMAL-pico` | 3/24 s = 125 ms |
| GREAT | `resultsNORMAL` | `resultsNORMAL-pico` | 5/24 s = 209 ms |
| EXCELLENT | `resultsEXCELLENT` **+ intro** | `resultsEXCELLENT-pico` **+ intro** | 0 |
| PERFECT / PERFECT_GOLD | `resultsPERFECT` | `resultsPERFECT-pico` **+ intro** | 95/24 s = 3959 ms |

O `-intro` toca **uma vez** e o principal entra em loop no fim dele (o
`onComplete` de L506-524); onde nao existe intro (medido no clone de arte: o
`resultsNORMAL` dos dois e o `resultsSHIT-pico`/`resultsPERFECT` do bf), o
principal toca direto (L536-541). No port:

- `tools/empacotar_audio.py` converte os 12 OGG em PCM 48 kHz (135,3 MB) e os
  tres sons; `tools/empacotar_resultado_musica.py` gera o bloco `musica` do
  plano (dono x rank, intro, atraso) **conferindo que cada PCM existe**;
- no jogo, `resmus_iniciar` abre o intro com `audio_ring_start_loop(fd, 0)` e, no
  `audio_ring_eof()`, troca para o principal com `audio_ring_trocar_fonte(fd, 1)`
  -- e' o mesmo `onComplete` do original;
- **o rank PERFECT pede musica em 3959 ms, mas os ticks do percentual comecam em
  1542 ms**: o anel ja' esta' rodando em SILENCIO+OVERLAY quando a musica chega,
  e nesse caso ela entra por `audio_ring_trocar_fonte` (o caminho que o game over
  ja' usava para a musica entrar por baixo do `loss_sfx`). Isto foi um BUG
  medido: a primeira versao so' iniciava a musica se o anel nao existisse, e os
  ranks PERFECT/PERFECT_GOLD ficavam sem musica para sempre.

Os dois `playOnce`: `scrollMenu` a cada AUMENTO do percentual (L592-604, um por
quadro em que o numero muda -- 35 a 36 no harness) e `confirmMenu` no
`onComplete` do tween de 58/24 s (L609, 3959 ms).

**Limite conhecido, medido:** o anel tem UMA voz secundaria (o overlay). Cada
`playOnce` do original e' um `FlxSound` independente e eles podem soar juntos;
aqui um tick novo RECOMECA o overlay em vez de somar com o anterior. Os eventos
sao os mesmos e emitem no mesmo instante (comprovado nos 35 ticks), o que se
perde e' a cauda de um tick quando outro chega em menos de 0,44 s, e o
`confirmMenu` cortando o ultimo tick. E' limite de vozes do anel, nao evento
removido -- esta' na lista de pendencias do console.

**Como cada rank foi provado.** O roteiro do harness chega sempre no GREAT, e
metade da tela depende do rank. Por isso o binario do harness ganhou
`FNF_RANK=<0..5>` (`test/ps2harness/ps2h.c`, so' no harness):
`tools/run_resultado_rank_harness.sh` roda os seis e cobra, em cada um, a arte
do `rankText` daquele rank, o `getFlashDelay`, o `getMusicDelay`, o par
intro+loop (nos dois que tem intro) e o loop direto (nos quatro que nao tem). A
suite roda os dois extremos (LOSS e PERFECT).

Medido no harness (`tools/run_gameplay_harness.sh 5600`):
`resultado_nome_arte,1,17280` (o bitmap do Tutorial), `resultado_nome,4685,-300`
(entra em 468,5 / -30,0), `resultado_nome_move,4634,6566` (aos 6566 ms o texto
ja' tinha saido do x de entrada) e `resultado_nome_ciclo` tres vezes em 127 s de
harness. O roteiro padrao (`tools/run_gameplay_harness.sh`) passou de 5600 para
**7600 quadros** para que os dois recomecos caibam no teste.

#### A tela de Resultado: o que falta, medido

`ResultState.hx` L119-160 e L359-386. Tudo é desenhado em 1280x720; o port
desenha em 640x360, então a arte entra pela metade. O pool fica com
**1.269.760 B livres** nessa tela (o palco é solto em `palco_soltar()` ao entrar
em `ui_state 9`; medido no log do harness: `pool_cena,9,1261568`), então o
orçamento aqui é de ~1,2 MB — folgado em comparação com a música.

| peça | fonte | quadros | tamanho original | a 0,5 | VRAM de 1 quadro |
|---|---|---|---|---|---|
| `results` (banner animado) | `resultScreen/results.png+xml` | 36 | 1130x149 | 565x75 | 169.500 B |
| `ratingsPopin` | `resultScreen/ratingsPopin` | 16 | 449x419 | 225x210 | 189.000 B |
| `scorePopin` | `resultScreen/scorePopin` | 30 | 712x145 | 356x73 | 103.952 B |
| `soundSystem` | `resultScreen/soundSystem` | — | 3432x3069 | — | (só 1 quadro por vez) |
| `topBarBlack` (procedural: 0 B) + `difficulty` | `diff_*.png` | 1 | 1294x148 + 228x58 | 647x74 + 114x29 | **0 B** + 13.224 B |
| `rankText*` / `clearPercent*` | `resultScreen/rankText`, `clearPercent` | 1 por arquivo | vários | ~1/2 | ~50 KB cada |
| `results-bf` / `results-pico` | `resultScreen/results-bf`, `results-pico` | por rank | — | — | ~150 KB |

Nada disso precisa ficar residente ao mesmo tempo: a tela é uma SEQUÊNCIA
(`FlxTimer` de 8/24 s para o `soundSystem`, `rankBg`, `resultsAnim` aos 0,05 s,
`ratingsPopin`/`scorePopin` encadeados por `onFinish`), então o caminho é o
mesmo dos personagens — quadros LZ4 na RAM, um quadro por vez na VRAM, com
`pack_stream` — e não um atlas fixo. Isso é trabalho de uma build própria; o
que a build atual entrega é o número de cada peça e o fato de o espaço existir.

E o que **não** falta mais, medido nesta build: os 46 personagens e os 21 palcos
reais rodam nas 81 combinações; a barra de vida desenha a **moldura do
`healthBar.png`** de verdade (`health_bar.rgba`, 301×10, sobreposto à região
(0,112) da `tex2` — nenhuma textura nova, o pool tem 1.792 B livres); a pausa
toca a **música `breakfast`** do original em loop com o fade-in de 5 s até 0,75
(`tools/run_pause_harness.sh` prova 384.000 bytes abertos, trecho sorteado em
146.184 e 99.328 amostras tocadas antes do Resume); o game over tem a animação de morte, a câmera, o
zoom **e o áudio** do original (música em loop, `fnf_loss_sfx`, `gameOverEnd` e
o `fakeout_death`); os ícones de vida do HUD carregam com a arte real dos 15
pacotes ICP1; os note kinds (canto com sufixo, animação nomeada,
alternância, `moveToBack`/`moveToFront` e tremor de câmera) são aplicados; os
offsets por animação (155 de 523) entram no desenho; e a ordem de desenho do
palco é por `zIndex` com os props na mesma lista dos personagens.

## A lista da Freeplay: as capsulas do original (medido)

A tela de Freeplay desenhava uma **lista de texto** num painel. Agora ela desenha
o carrossel de cápsulas do original, com a arte de verdade, e o título de cada
música é o `FlxText` renderizado do `5by7` (não há bitmap de letra para compor no
console).

**A geometria é do `SongMenuItem.hx`/`FreeplayState.hx`, linha por linha:**

    intendedX(i) = 270 + 60*sin(i)                                (L761-765)
    intendedY(i) = i*((capsule.height*0.8) + 10) + 120             (L766-770)
    y = intendedY(index - sel); if (rel < 0) y -= 50;
    else if (rel > 4) y -= -10; if (index < sel) y -= 100;        (L2941-2949)
    x persegue o alvo com smoothLerpPrecision: 0,256 s e 0,192 s   (L735-736)
    entrada: 7 passos de 1/24 s com xFrames[] em x e 1/xFrames em y (L683-700)
    alpha 0,5 desde a criação e 1 no timer de (7/24)*2,5 = 729 ms  (L988, L639)
    textAppear: (1,7;0,2) -> (0,4;1,4) -> (1;1) em 3/24 s          (L518-532)
    texto não selecionado: alpha 0,6 e `capsule.offset.x = -5`     (L796-799)

O `index` de `changeSelection` é **1-based** e o `curSelected` é 0-based, então o
item selecionado tem `rel = 1` e fica em `intendedY(1)`, não em `intendedY(0)` —
foi um erro de leitura meu (o teste `test/test_freeplay.c` cobra o valor certo).

**O `CUTOUT_WIDTH` é zero nesta plataforma, e isso é decisão, não esquecimento.**
`FreeplayState.CUTOUT_WIDTH = FullScreenScaleMode.gameCutoutSize.x / 1.5`
(L121), e `gameCutoutSize` só é preenchido quando existe **recorte de tela de
aparelho** (`FullScreenScaleMode.updateDeviceCutout`, L299-313, e
`adjustGameSize`, L420+: os dois exigem `enabled`, que só liga em Android com
cutout). Num aparelho sem cutout — o caso do PS2 — `gameCutoutSize` fica `(0,0)`
e o deslocamento `CUTOUT_WIDTH * SONGS_POS_MULTI` das cápsulas é **0**. Por isso
`FP_X_BASE = 135` e não `135 + 320`.

**A arte, com o número de cada peça:**

| peça | origem | tamanho na tela (640x360) | VRAM |
|---|---|---|---|
| cápsula, 16 quadros (8 `selected`, 8 `unselected`) | `freeplayCapsule/capsule/freeplayCapsule` 2048x1024 (quadro 612x132) | 306x66, desenhada a 244,8x52,8 | 80.784 B por quadro, **2 quadros residentes** = 161.568 B |
| título, um RGBA por música | `CapsuleText` = `FlxText` `fonts/5by7.ttf`, 40*0,8*0,5 = **12,8 px** | 1:1, recorte de 102 px (`clipWidth` 255 * 0,4) | **27 títulos = 122.740 B** (o maior, `spaghetti`, 20.672) |

Os 27 títulos juntos custam **menos que os dois quadros de cápsula**, então eles
sobem **todos** na abertura da tela: o cache de 8 slots da primeira versão relia
**um arquivo por quadro** quando a janela de 9 cápsulas não cabia no cache (o
harness mostrou a leitura acontecendo quadro a quadro). Sem cache, sem I/O e sem
trava ao rolar.

**Medido no harness** (`tools/run_freeplay_harness.sh 3000`):

    freeplay_capsula,1,80784        a cápsula abriu, 80.784 B por quadro
    freeplay_titulos,122740,0       os 27 títulos na VRAM, zero falhas
    freeplay_capsulas,9,5091        9 cápsulas desenhadas (a janela)
    freeplay_draw,16,27             16 quads no quadro da tela

**O FUNDO e o DJ: feitos depois desta medição.**

- **Fundo** (`backingImage`, `FreeplayState.hx` L340/L456/L529-531): arte
  821×516 reduzida a **574×360 em 16 bpp = 413.280 B**, entra de `x = -width`
  até 0 em 700 ms com `quintOut` (`fp_fundo_x`). Medido:
  `freeplay_fundo,1,413280` e `freeplay_fundo_desenho,-5740,0` (o primeiro
  desenho já fora da tela, em -574,0 px).
- **DJ** (`AnimateAtlasFreeplayDJ`, `preload/images/freeplay/freeplay-boyfriend`):
  o atlas achatado pelo `tools/achatar_animate.py --pack` com
  `--scale 0.5 --bpp 16` dá **370×375, 678 quadros, 9 animações**; o pacote tem
  **12.651.668 B** e vem em **duas partes** (`.chp4` + `.chp4.part1`, o corte de
  8 MB do `write_packs`), lidas pelo `src/engine/pack_multi.c`. Na VRAM ele
  custa **um quadro = 277.500 B**. Posição **(0,0)**: `resetPosition()` com
  `useAnimatePosition: true` e `CUTOUT_WIDTH = 0`.
- **A máquina de estados** (`AnimateAtlasFreeplayDJ.hx` L62-140 e
  `BaseFreeplayDJ.hx` L74-221): `Intro` (17 quadros, sem loop) → `Idle`
  (14, loop, e é no FIM DE CADA VOLTA que o `timeIdling` decide: 60 s →
  `idleEasterEgg` 139 quadros, 120 s → `cartoon` 166); `onPlayerAction()` (todo
  movimento da lista: L1764/1821/1844/1850/2548/2884) só zera `timeIdling`;
  `onConfirm()` → `confirm` (28); e, na **volta do resultado**
  (`FreeplayState.hx` L1084 e L1259), `fistPumpIntro()`/`fistPumpLossIntro()`
  (estado 5/6, repetindo a janela `[introStart..introEnd-1]` do `fistPump` do
  JSON) e 0,5 s depois `fistPump()`/`fistPumpLoss()` (estado 7/8; com
  `loopEndFrame: -1` toca UMA vez, do `loopStart` ao fim, e o `onFinishAnim`
  L210-214 devolve o `Idle`).
- **Medido no harness** (`tools/run_freeplay_harness.sh 7000`, que agora liga o
  Practice Mode para a música CHEGAR ao resultado em vez de morrer):
  `freeplay_dj,1,277500` e, sem rank (`FNF_RANK` ausente ⇒ SHIT),
  `freeplay_dj_volta,0,6` com estados `1 2 6 8 1` (Intro→Idle→Confirm→loss→
  lossLoop→Idle); com `FNF_RANK=3`, `freeplay_dj_volta,3,5` e estados
  `1 2 5 7 1` (fist pump → fist pump loop → Idle). Nenhum quadro recusado.
- **VRAM da tela, medida**: disponível ao entrar `pool_cena,11,1269760`; usado
  413.280 (fundo) + 2×80.784 (cápsulas) + 122.740 (27 títulos) + 277.500 (DJ) =
  **975.088 B**, sobram **294.672 B**.

**O que ainda falta nesta tela, medido** (não é lógica, é arte/dado):
- o **cinza 0,8** das cápsulas não selecionadas (`Grayscale(1)` com
  `setAmount(0.8)`, L197/L792): decide-se no empacotador (pixels), não no GS;
- o título **sem o brilho** quando não selecionado (`songText.blurredText` só é
  visível na selecionada, L798) — exige uma segunda variante de cada título;
- os enfeites da cápsula: `weekText`, `ranking`/`blurredRanking`, `sparkle`
  (PERFECT_GOLD), `newText` (o selo `NEW`), `favIcon`/`favIconBlurred` (o
  coração), `pixelIcon` e as **estrelas de dificuldade**
  (`updateDifficultyRating`);
- a cápsula **Random** (`initRandom`), que o original põe no topo da lista.

## Fase 5 com os pacotes de verdade: o custo por quadro, medido

A linha de base que existia (`build/sweep/custo.txt`) foi medida com os assets
**sintéticos** de 64×64: ela conta *quantos* uploads acontecem, mas não *quanto*
sobe para a VRAM. Com os pacotes reais o número de uploads cai e o de bytes por
upload sobe 30×, e é o segundo que decide o orçamento de quadro no console.

Medido hoje (`tools/run_realpacks_harness.sh` para o carregamento e uma música
inteira por vez com `FNF_START_SONG`; eventos `custo_quadros`/`custo_char` a
cada 240 quadros):

| música | uploads de personagem / 100 quadros | canvas dos 3 (16 bpp) | bytes/frame | taxa a 60 Hz |
|---|---|---|---|---|
| tutorial (2 personagens) | **79–80** | 202.940 B (BF) + 213.504 B (GF/oponente) | ≈ 170 KB | ≈ 10 MB/s |
| bopeebo (3 personagens) | **120–121** | 213.504 + 258.816 + 275.200 B | ≈ 300 KB | ≈ 18 MB/s |
| pior canvas do jogo | — | `tankman-bloody` 612×516 = **631.584 B** | ≈ 430 KB (se cantar todo quadro) | ≈ 26 MB/s |

O que isso quer dizer, sem otimismo:

- **O teto não é a banda do GS.** 18–26 MB/s de DMA para a VRAM está dentro do
  que o canal de imagem do ps2sdk entrega. O custo real é **decodificar LZ4 no
  EE**: 300 KB por quadro a 60 Hz são ~1 ms/quadro de decodificação (mais o
  `memcpy` do pacote para o buffer de 128 B alinhado), dentro de 16,7 ms, mas
  é o primeiro número que uma build de console precisa ver no `frame_mark`.
- **Não há frame para cortar.** 80–120 uploads por 100 quadros é exatamente a
  taxa de animação do original (24 fps por personagem, ~1,3 personagens
  animando ao mesmo tempo, mais os props). Reduzir isso seria remover quadro de
  animação — proibido pela regra do projeto.
- **A economia que sobrou é de VRAM, não de tempo:** a pior combinação deixa
  1.792 B livres no pool. Textura NOVA exige liberar espaço antes. Duas coisas
  já saíram dessa fila sem gastar um byte: os ícones de vida (pacote ICP1 por
  música, com despejo) e o note splash (região vazia do `tex2`, ver a seção de
  lacunas). O que ainda precisa de espaço é a arte das telas (cápsulas da
  Freeplay, `AtlasFont` do Resultado, ícone da pausa) — e ela só é desenhada
  FORA da música, quando o pool está livre. O candidato medido continua sendo a
  `tex` (320×256 PSMCT32 = 327.680 B), que o gameplay com pacotes **não usa**
  (os personagens vêm do pool) e que o menu já re-sobe ao voltar
  (`menu_restore`).

## Fechado: o ÁUDIO DA FREEPLAY — o preview de 20 % e os 8 jingles, medidos

O estado 11 deixou de ser mudo. A cadeia inteira, do arquivo ao `audsrv`:

| peça | onde | número medido |
|---|---|---|
| atraso do preview | `freeplay_audio_entrar` → `FPA_ATRASO` | 0,25 s (`FADE_IN_DELAY`, L141) |
| janela | `fp_aud_janela(total, 0, 0,2)` | `stems.pcm` inteiro → **20 %** dele, em bytes alinhados a uma amostra de 4 canais |
| janela medida no harness | `freeplay_audio_janela` | **645 120 amostras** = 5 160 960 B = 5 040 KB |
| canais | `audio_ring` no modo preview | 4 → 2 **sem as vozes** (`amix_somar`, `vozes = 0`) |
| ganho | `fp_aud_ganho` + `amix_somar_ganho` | `min(t/2 s, (len−t)/2 s) × 0,7`; pico **700** de 1000 no dente de serra do teste |
| laço | fim da janela → `prev_ini` | segunda volta idêntica à primeira em k = 500 / 5 000 / 95 000 |
| jingles | `audio_ring_overlay` (4 slots) | 8 sons, 3 720 852 B no total, **dois tocando juntos** (o de 1,004 s do `rankinperfect` ainda está no ar quando o do slam entra 0,5 s depois) |
| agenda | `fp_aud_eventos` | jingle do rank **600 ms**, jingle do slam **1100 ms**, preview de volta **2200 ms** — sem pular instante num quadro longo |
| troca de música | `freeplay_audio_reagendar` | só quando o índice muda; trocar de dificuldade NÃO reinicia o preview (a fonte só reinicia em troca de variação) |
| cinemática | `freeplay_audio_entrar(cinematica=1)` | primária em silêncio (`music.volume = 0`, L1088) + os dois jingles; o preview volta no fim |

**Os dois erros que o teste do anel pegou** (e que o harness do PC nunca pegaria,
porque com música de verdade a janela de 5 MB não termina no roteiro):

1. **O ganho era interpolado de ponta a ponta do bloco de 32 KB.** O `fp_aud_ganho`
   é o MENOR de duas rampas lineares, então ele tem um bico em 2 s (fim do
   `fadeIn` = começo do `fadeOut`). Uma reta entre as duas pontas de 4 096
   amostras corta esse bico: medido, o pico do fade saía **686** em vez de
   **700** — 2 % de amplitude em toda a janela, e o mesmo erro de volta. Agora o
   ganho é aplicado em trechos de 256 amostras dentro do bloco: pior erro
   0,7 × 128/96 000 = **0,09 %**, 16 chamadas por bloco de 85 ms.
2. **O marcador de posição da janela misturava amostras com bytes.**
   `prev_pos` conta amostras (é o que alimenta o ganho) e `prev_ini`/`prev_fim`
   são bytes; a conta da sobra era `prev_fim − prev_pos`, ou seja, bytes menos
   amostras. O limite nunca disparava: o produtor lia **até o fim do ARQUIVO**
   (5× a janela) e só então voltava. No teste, isso apareceu como o conteúdo
   deslocado no tempo — a primeira subida de zero do dente de serra saía no
   quadro **926** em vez do começo da janela, e os períodos medidos eram
   2 052/2 011/2 005/2 003 em vez de 2 001. Corrigido: a sobra é
   `prev_fim − prev_pos × 8` e o marcador volta a **zero** no laço.

O defeito 2 era audível e não sutil: a janela de 4 s tocava e depois vinham
**12 s de silêncio** (o ganho já estava em 0) antes de o trecho recomeçar.

**Como está provado**

- `test/test_audio_ring.c` (novo, no `test/run_tests.sh`): kernel falso em
  pthread (`test/ps2sdk-shim/kernel.h` e `delaythread.h`) e um `stems.pcm`
  sintético de 200 000 amostras com as vozes em ±0x6000. Uma janela de 4 s, duas
  voltas. Cobra: pico ≤ 1001 (**as vozes não vazam**), pico ≥ 690 (o
  instrumental chega ao volume 0,7), o meio da janela em 667, L = −R em todas as
  amostras, 8 zeros no recomeço e a repetição idêntica em três posições.
- `test/test_freeplay_audio.c` (já existia): janela em bytes, ganho, os 8
  jingles por rank, a agenda.
- `tools/run_freeplay_harness.sh`: `previews iniciados 2`, `janela 0,5040 KB`
  (20 % do `stems.pcm` real), `reagendamentos 2`, `falhas nenhuma`.
- `tools/run_rank_anim_harness.sh`: `jingles 1 6` (normal + excellent para o
  rank 3), `preview antes 645120,0`, `preview de volta 645120,5040`.

**O que o PC não prova**: que sai som na TV. Isto é `audsrv`, `libsd` e o
`usbhdfsd` lendo 5 MB de um pendrive enquanto o GS desenha — só o console.

Os três `playOnce` de menu da tela (`scrollMenu` a 0,4, `confirmMenu` e
`cancelMenu` a 1,0) entraram na mesma rodada — ver *"Fechado: os `playOnce` de
menu e o volume por fonte"*, logo abaixo.

**O que continua faltando nesta tela, e por quê**: só a cápsula aleatória
(`curSelected == 0` → `freeplayRandom`). Duas medidas, não um chute: a lista do
port são as 27 músicas sem a cápsula "?", e `shared/music/freeplayRandom.ogg`
não existe no repositório de arte (`ls shared/music | grep -i random` → 0). É
dado ausente, não decisão de design.

## Fechado: os `playOnce` de menu e o volume por fonte (Freeplay, menu e título)

A tela de Freeplay estava **muda fora do preview**: trocar de música, trocar de
dificuldade, confirmar e voltar não tocavam nada. As quatro chamadas estão na
fonte e agora no port, com o volume de cada linha:

| som | onde na fonte | volume | no port | medido no log |
|---|---|---|---|---|
| `scrollMenu` | `FreeplayState.hx` L2918 (`changeSelection`) e L2388 (`changeDiff`) | **0,4** | UP/DOWN na lista e LEFT/RIGHT na dificuldade | `ui_som 0 40` |
| `confirmMenu` | L2764 (`startSong`) | 1,0 | CROSS na lista | `ui_som 1 100` |
| `cancelMenu` | L2240 (`goBack`) e `MainMenuState.hx` L629 | 1,0 | CIRCLE na Freeplay e CIRCLE no menu | `ui_som 2 100` |
| `confirmMenu` | `TitleState.hx` L288 | **0,7** | CROSS no título com o intro já pulado | `ui_som 1 70` |

**Como toca.** Não é um mixer novo: é o mesmo anel de áudio, pelos slots de
fonte secundária (`audio_ring_overlay_slot`). Três coisas mudaram nele:

1. **Volume por slot** (`amix_volume`): o `scrollMenu` é o único a 0,4. O ajuste
   é na amostra lida, **antes** de somar, porque cada fonte tem o seu — a mesma
   regra que já valia para o ganho do preview.
2. **Slot fixo por som** (0, 1, 2): o arquivo é sempre o mesmo, então o fd é
   aberto uma vez e reaproveitado. Abrir um arquivo no meio de uma música é o
   que trava o quadro no console (o `usbhdfsd` não é reentrante). O alocador
   automático (jingles do rank, sfx de game over/resultado) agora começa no
   **último** slot e desce até o 3: sem essa separação um jingle poderia pegar o
   slot de um sfx de menu e o pedido seguinte fecharia o fd do jingle no meio.
   Slots: **8** (3 fixos de menu + 5 automáticos).
3. **Gerador de geração** (`audio_ring_geracao`): os fds guardados para reusar
   valem enquanto o anel é o mesmo. Toda parada/início sobe a geração — quando
   ela muda, o anel já fechou os fds e o port reabre no toque seguinte. Sem isso
   haveria `read` de fd fechado depois de cada troca de tela.

**O `cancelMenu` não existia como amostra.** O `menu_sfx.pcm` do port tem só
`scrollMenu` [0, 39168) e `confirmMenu` [39168, 241920); a faixa seguinte é a
fonte VCR do tutorial. Era uma pendência declarada desde a 0.2.104 ("não vou
inventar offset"). Agora ele é um arquivo próprio, empacotado de
`preload/sounds/cancelMenu.ogg` como os outros dois: **85 264 B, 0,44 s** em
`assets/result/cancel_menu.pcm` (a mesma pasta, para o pendrive não ganhar mais
uma divisão; o gate do `tools/build_pendrive.sh` subiu de 15 para 16 PCM).

**A saída da tela não para mais o som na hora.** Antes, `freeplay_audio_parar()`
era chamado no mesmo quadro do CIRCLE/CROSS e cortava o `cancelMenu`/`confirmMenu`
recém-começado. Agora quem para é `freeplay_audio_sair()`, na **troca de
estado**, e ele só toca no anel se a geração ainda for a desta tela:

- **CIRCLE → menu**: a geração é a mesma, então o anel é parado ali — antes do
  `ui_state==6` abrir o `freakyMenu` (parar depois mataria a música nova, que foi
  um defeito já medido);
- **CROSS → palco**: o preparo da cena (`transition_frame==15`) já parou este
  anel e abriu o dele (`audio_ring_start`, L6788) no mesmo quadro. A geração
  mudou, então aqui só se devolve o fd do preview — mexer no anel mataria a
  música recém-começada (`TUT_RING_BEGIN` + stop no mesmo quadro);
- o bombeamento (`freeplay_audio_bombear`) passou a rodar **também** no fade de
  saída, senão o sfx morria no primeiro quadro. Na **entrada** ele continua de
  fora: medido, bombear durante a transição de entrada empurrava o primeiro
  quadro da cinemática de 0 para 1 ms e o zoom de **1850 para 1849**.

**Limite medido, e é do console**: o `confirmMenu` tem **2,30 s** e o
`cancelMenu` **0,44 s**, mas o fade da transição leva 0,25 s e o estado seguinte
chama `audsrv_stop_audio()` — a cauda é cortada a ~11 % do `confirmMenu`. No
original o `FlxSound` global sobrevive à troca de estado e toca por cima da
contagem. Levar o sfx de uma tela para outra exigiria um transporte contínuo
entre telas, que o port não tem: está na lista de pendências do console, com
estes números.

**O que continua faltando, e por quê (dado, não lógica):**

- `fav` / `unfav` (L3092 e L3115, volume 1): o **favorito** não existe no port —
  não há campo no save, nem o ícone na cápsula, nem o tween de 0,1 s do
  `favoriteSong()`. Os dois sons vêm junto com a função, não antes dela.
- os sons do menu de **opções** (`OffsetMenu.hx` L418/L422): a tela de ajustes do
  port é um stub sem lista de itens, então não há evento para somar.
