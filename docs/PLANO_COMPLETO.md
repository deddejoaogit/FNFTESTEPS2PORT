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

### Fase 2 — PlayState genérico (qualquer música, qualquer dificuldade)

**Existe hoje:** o estado `ui_state == 8` só sabe tocar `tutorial/song` com
`assets/audio_stems.pcm` fixo no código e `FNF_ASSET_ROOT` para o chart.

**Falta:** (a) escolher música e dificuldade e carregar
`<root>/songs/<id>/<dif>.chart` (o loader já faz, `chart_load_song_layout`);
(b) áudio por música em vez de um arquivo único — `audio_ring_start(fd)` já
aceita um fd, então é trocar a origem; (c) contagem regressiva (5 ticks,
`Countdown.hx`), (d) fim de música → resultado, sem depender do tutorial.

### Fase 3 — Telas: Freeplay, seleção de dificuldade, pausa, Game Over, Resultado

**Existe hoje:** título (0), menu (6), story (7), jogo (8), **resultado (9)** e
**game over (10)**. Faltam Freeplay, seleção de dificuldade própria da Freeplay
e pausa.

**Falta:** três telas, cada uma com a geometria do original (o menu e o story
já mostram o método: coordenadas de `camHUD` a metade de 1280×720).

### Fase 4 — VRAM: o modelo quebrado e o cache sem uso

**Existe hoje:** 6 reservas de VRAM no init, zero liberações; 8 buffers =
99,4 % da VRAM; 2.929.792 B de imagens embutidas para 2 de 4 telas.
`src/engine/vram_cache.c` (LRU) existe, testado, e `vram_cache_init()` é
chamado **zero** vezes em `main.c`.

**Falta:** ligar o cache, fatiar as imagens grandes, 16 bpp nas folhas pequenas,
frame buffers 512×384 PSMCT16, repack por palco.

### Fase 5 — Otimização medida

**Existe hoje:** harness de PC que mede quadros, pacotes, qwords, overflows.

**Falta:** usar o mesmo harness para medir antes/depois de cada mudança e
registrar a tabela neste documento. Sem tabela, nada é declarado ganho.

---

### Fase 3 — Telas: ABERTA (Resultado e Game Over fechados)

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

**O que falta na Fase 3:** Freeplay (`FreeplayState.hx`) e a seleção de
dificuldade dentro dela.

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
