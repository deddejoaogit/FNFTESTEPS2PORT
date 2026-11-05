# PLANO MESTRE — TUDO O QUE FALTA IMPLEMENTAR (por prioridade)

Data: 2026-11-04 · build de referencia: **0.2.165** · atualizado a cada entrega
Regras do projeto: `google_drive/handoff/REGRAS.md` (10 regras, vinculativas).

Este documento responde ao pedido de 2026-11-04: *"crie um documento massivo
com uma lista de tudo que deverá implementar; a prioridade é o sistema de log,
pois ele ajudará em todo o resto; então corrigir absolutamente tudo que
encontrar e o que falei"*. Cada item tem: **sintoma**, **estado** (feito /
em curso / por fazer), **causa** (provada ou hipótese), **o que medir** e
**critério de aceitação**.

---

## ESTADO GERAL

* **~97 % do código portado.** 27 músicas, 46 personagens, 21 palcos,
  3 dificuldades, 81 charts — eventos, ordem e tempo idênticos ao original
  v0.8.6 (`/tmp/funk86`), constantes tiradas do Haxe/flixel, nunca adivinhadas.
* **Joga-se hoje (no console):** boot → título → menu → story → freeplay →
  gameplay de notas (setas, placar, combo) com música e vocais.
* **Ainda não funciona no console:** sprites de palco/personagem nas semanas
  (aparece só um pedaço do cenário), menu de pause (sem texto), MERCH/OPTIONS/
  CREDITS (telas não portadas), traços do título, 3ª entrada em fase trava.
* **Medido nesta build:** harness de PC 37 `ok` + 1 falha pré-existente
  (orçamento de VRAM de 5 palcos) + harness ASan/UBSan 1401 quadros sem erro
  de memória; ELF 6 593 220 B.

---

## PRIORIDADE 1 — SISTEMA DE LOG MASSIVO ✅ FEITO (0.2.165)

**Porquê primeiro:** todos os pontos abaixo são diagnosticáveis a partir dos
ficheiros que esta build gera. Sem eles, cada correção seria um palpite — e
palpite é proibido pela regra 4.

### 1.1 O que a 0.2.165 grava (novo)

| Ficheiro | Conteúdo | Quando |
|---|---|---|
| `FNF_LOG00.txt` … `FNF_LOG31.txt` | Série `Q` por quadro (10 Hz + todos os quadros lentos), eventos `E`, snapshots. **Cada despejo = 1 ficheiro inteiro** (o antigo gastava 15 ficheiros por despejo). | 1 ficheiro a cada 300 quadros (5 s); 32 ficheiros ≈ **8 min de história** |
| `FNF_SESSAO.txt` | Ambiente do boot: build, caminho do ELF, init, VRAM, heap, áudio, pad, dimensões da fonte | Fim do boot e a cada troca de estado |
| `FNF_ANOM_XX.txt` | **Anel inteiro de 512 quadros** (cada fase cronometrada) despejado quando um quadro passa de 250 ms ou o cão de guarda vê o jogo parado | Máx. 12 por sessão |
| `FNF_TRAVA.txt` | (existente) marcas finas do quadro onde o jogo parou | Cão de guarda (3 s parado) |
| `FNF_ESTADO.txt` | (existente, SELECT+R1) despejo completo: VRAM mapa vs alocado, heap, GS regs, áudio, contadores | Sob comando |
| `FNF_SHOT_XX_sU_fF.rgba.txt` | **Print da tela** 320×224 RGBA | SELECT+R3 (rápido) + 1 automático por troca de tela (máx. 12) |
| `FNF_AV.txt` / `FNF_CONGELADO.txt` | (existentes) traço A/V e congelamento deliberado | SELECT+L1 / SELECT+L2 |

### 1.2 A linha `Q` — o tempo de cada frame, e PORQUÊ

Cada quadro cronometrado fase a fase (microssegundos):

```
Q,f=<quadro>,t=<total>,in=<pad+timer>,upd=<logica+lista>,dma=<envio GIF>,
  wait=<GS>,cap=<captura>,vs=<vsync>,post=<fim>,fl=<custo do ultimo despejo>,
  qw=<qwords>,qd=<sprites>,bx=<binds de textura>,upn=<uploads>,upk=<KB p/ VRAM>,
  ovf=<estouro da lista>,cl=<recortes>,cu=<culas>,
  st=<estado>,tr=<transicao>,sel=<cursor>,pad=<botoes>,prs=<carregados>,
  clk=<relogio musica>,q=<fila audio>,r=<anel>,un=<secas audio>,eof,pl=<pausa>,
  vr=<VRAM livre>,rb=<RAM buffers>
```

Se um quadro demorar, o campo que crescer é o culpado — é assim que o lag vai
ser atacado na prioridade 2, **com número, não com suposição**.

### 1.3 Comandos do teclado (diagnóstico)

| Gesto | O que faz |
|---|---|
| SELECT+R3 | Print rápido da tela (não trava: pausa cooperativa do anel, 286 KB) |
| SELECT+R1 | `FNF_ESTADO.txt` completo |
| SELECT+L1 | Despejo da lista de desenho (reexecução no PC) |
| SELECT+L2 | Lista + congelamento (180 quadros) |
| SELECT+Triângulo | Alterna 4:3 / 16:9 |
| SELECT+Quadrado | Padrão de calibração |
| SELECT+Start | Sai do jogo (fecha o log) |

### 1.4 Correções feitas junto com o log

* **SELECT+R3 já não trava o jogo.** O caminho antigo parava o áudio, fechava
  o `.pcm`, esperava 500 ms e gravava 1,1 MB no mesmo pendrive da música.
  Agora: pausa cooperativa + fatias de 32 linhas + redução 2×2 = 286 KB.
* **O pause já não vaza entre músicas.** Sair de uma fase pelo Circle deixava
  `g_paused` ligado e a próxima música começava "em pausa" (foi o que a
  Week 3 mostrou). Corrigido na troca de estado.
* **Três escritores deixaram de partilhar `FNF_COLHEITA.txt`** (que podiam
  corromper-se): agora `FNF_LOG*`, `FNF_AV.txt`, `FNF_ESTADO.txt`,
  `FNF_CONGELADO.txt`.

### 1.5 O que ainda falta no log (fase 2 do log)

* [ ] Linha `Q` com contagem de E/S de ficheiro (chamadas e bytes) por quadro.
* [ ] Histograma de tempos no fim de cada despejo (já acumulado em RAM; falta
      formatar no `#` final de cada ficheiro).
* [ ] Top-5 dos piores quadros da sessão em cada despejo.
* [ ] Log de cada abertura de ficheiro de asset (caminho, rc, bytes, ms).
* [ ] Marcador de sincronismo áudio/vídeo: diferença entre `music_clock` e o
      tempo de parede, por quadro (é assim que se mede o "a música toca mas a
      imagem atrasa").

---

## PRIORIDADE 2 — LAG / FPS (o que mais incomoda)

**Sintoma:** o jogo abre com FPS muito baixo, melhora, mas continua a travar —
no menu e nas fases.
**Estado:** por fazer. **Começa pela medição com o log da 0.2.165.**

Suspeitas (a decidir pelos campos `Q`, nunca por palpite):

1. `upk`/`upn` altos no mesmo quadro → uploads de textura a mais (cenário e
   personagens a subir para a VRAM dentro do quadro em vez de no carregamento).
2. `wait` alto → lista de desenho grande demais (`qw`) ou `draw_wait_finish`
   a apanhar o quadro anterior.
3. `fl` alto → o próprio log a escrever (custo conhecido: ~22 ms por despejo
   a cada 5 s; se aparecer noutro campo, está mal colocado).
4. `in`/`upd` alto → lógica a mais por quadro (reconstruções de sprites).
5. `vs` baixo + `t` instável → o jogo não está a atingir 60 Hz e o vsync está
   a ser adiado; medir com `t` por quadro em `FNF_LOG`.

**Critério de aceitação:** ≥ 55 fps sustentados em story + freeplay + fase,
medidos pelo histograma do log (não pelo olho), sem regressão de conteúdo.

**Regra:** uma causa por build, medida antes e depois.

---

## PRIORIDADE 3 — FASES: QUASE SEM SPRITES (Week 2/3)

**Sintoma:** a música toca, as notas andam, mas "quase nenhum sprite aparece;
só do lado direito da tela uma parte da textura do cenário num fundo preto".
**Estado:** por fazer. Hipóteses (o log decide):

1. `palco_falha` com rc −3/−4/−5 → os pacotes do cenário não abrem ou não
   casam de tamanho (o evento `palco_falha` já sai no log).
2. Recorte por software (`cl`/`cu` altos, sprites todos a serem cullados) → a
   faixa visível está a cortar tudo o que não está à direita.
3. VRAM (`vr` no fim / `palco_sem_vram`) → sem espaço, os props não sobem.
4. UV/`texbuffer_t.width` errado para texturas não potência-de-2 (384 px da
   fonte é o caso conhecido; cenários podem ter o mesmo bug).

**Como decidir:** jogar uma vez na Week 2 com a 0.2.165 e enviar
`FNF_LOG*.txt` + `FNF_ANOM*.txt` + `FNF_SESSAO.txt`. Os campos `bx`, `upk`,
`vr` e os eventos `palco_*` dizem qual das quatro é.

**Critério de aceitação:** cenário + BF + GF + ícones visíveis em todas as
semanas, com as cores certas (degradadas se preciso — imagem é negociável,
jogo não).

---

## PRIORIDADE 4 — 3ª ENTRADA EM FASE TRAVA (tela preta tipo screenholder)

**Sintoma:** ao entrar pela terceira vez em qualquer fase, tela preta que só
sai com Circle + fade out.
**Estado:** por fazer. Hipótese principal: **esgotamento de VRAM/descritores
entre sessões de fase** (o pool não devolve tudo ao sair — ver `pool_min` e
`vr` no log). A 3.ª entrada é o momento em que a soma das duas anteriores
ainda está alocada.

**Critério de aceitação:** entrar e sair de fases 10 vezes seguidas sem
travar e sem degradar.

---

## PRIORIDADE 5 — PAUSE MENU BUGADO

**Sintoma:** "menu de pause completamente bugado, sem texto e uma tela preta,
sprites bugados dos lados".
**Feito:** o estado de pause já não vaza entre músicas (0.2.165).
**Por fazer:**

1. Texto do menu de pause (usa a fonte sintética — cai na prioridade 6).
2. Arte dos lados (sprite sheet do pause — verificar `pause_*` no log de E/S).
3. Música do pause (`pause_musica` / `pause_sem_musica` já saem como eventos).

**Critério de aceitação:** pause igual ao original (continuar / reiniciar /
sair), com texto legível e sem tela preta.

---

## PRIORIDADE 6 — FONTE, TÍTULO E BOTÕES BUGADOS

**Sintoma:** "botões bugados" no menu; rótulos como pedaços de duas letras;
traços bugados no título.
**Estado:** HIPÓTESE — a grade da fonte não é 16×6 de 24×32. A **PROVA 4**
secção 1 mede exatamente isto (perfil de tinta por coluna/linha + corredores
vazios de cada grade candidata). **Falta o `PROVA4.txt` do console.**

1. [ ] Correr `PROVA4_PS2.ELF` e enviar `PROVA4.txt` (fica gravado mesmo que
      se desligue a meio; esperar pelas 16 barras).
2. [ ] Com a grade real, corrigir `FONT_CELL`/`FONT_DRAW`/`FONT_W`/`FONT_H`.
3. [ ] Se a fonte real não couber (1024 px máx. de textura), degradar:
      glifos menores ou atlas dividido. Imagem é negociável; texto tem de ler.

**Critério de aceitação:** todo o texto de menus, pause, placar e resultado
legível e alinhado, sem traços partidos.

---

## PRIORIDADE 7 — MERCH / OPTIONS / CREDITS

**Sintoma:** mostram `TELA NAO PORTADA`. O utilizador **não aceita
placeholders.**
**Estado:** por fazer. Cada tela precisa de: arte (do original v0.8.6),
layout exato (Haxe), sons de navegação e ações reais (OPTIONS: as opções do
original; CREDITS: rolagem + música; MERCH: tela estática + som).

**Critério de aceitação:** as três telas navegáveis, com arte e sons, sem
`TELA NAO PORTADA` em lado nenhum do jogo.

---

## PRIORIDADE 8 — 5 PALCOS ACIMA DO ORÇAMENTO DE VRAM

`phillyBlazin`, `limoRide`, `mainStage`, `phillyStreets`, `sserafim` — o
único teste que falha no harness. VRAM total: 4 194 304 B; FB 2×640×448×2.

1. [ ] Degradar texturas destes 5 (resolução/PSM) até caberem — regra 2:
      imagem é negociável.
2. [ ] Medir com `tools/cartao_de_prova.py` e `tools/check_stage_budget.py`.

**Critério de aceitação:** `test/run_tests.sh` verde nos 38 pontos.

---

## PRIORIDADE 9 — VOCAIS (quase fechado)

**Feito (0.2.164):** o chord SELECT+R3 já não desliga as vozes (era a causa
das "Week 2/3 sem vocais"); aviso em tela quando desligadas.
**Falta:** confirmar com a PROVA 4 secção 2 se os `stems.pcm` têm sinal nos
canais `Vox` (`vox=0%` ⇒ falta dado na geração, não é bug do port).

---

## PRIORIDADE 10 — TRANSIÇÕES E TRAVAS AO CLICAR X

**Sintoma:** trava ao confirmar em certos pontos; fade que não termina.
**Estado:** por fazer. Com o log v2, cada travamento deixa `FNF_ANOM_*` +
`FNF_TRAVA.txt` com o quadro exato e a fase em que parou.

**Critério de aceitação:** 20 navegações rápidas entre telas sem travar.

---

## PRIORIDADE 11 — ÁUDIO / DESEMPENHO DO ANEL

* `un` (secas) vira 1 em jogo na colheita 3 → medir se ainda acontece.
* Confirmar sincronismo: `clk` vs tempo de parede (item 1.5).

---

## O QUE NÃO FAZER (regras que o utilizador fixou)

1. **Nenhum evento removido, encurtado, atrasado ou simplificado.**
2. Imagem degrada até caber; **jogo nunca**.
3. Otimizações **medidas**, nunca supostas.
4. Nada de "zero erros / 100 %"; toda a afirmação vem de teste corrido.
5. Constantes do original Haxe/flixel v0.8.6, nunca aproximações.
6. Campo sem script = **dado em falta**, nunca inventado.
7. Diagnóstico nunca pode travar o jogo (regra 7).
8. **Uma causa por build** (regra 8).
9. Terminar sempre com **provado / hipótese / falta** (regra 9).
10. Links públicos; entrega em **UM link**; sem esperas bloqueantes.

---

## FLUXO DE TRABALHO (o que o utilizador envia de volta)

Depois de jogar com a build nova:

1. `FNF_LOG00.txt` … os 32 (pelo menos os de número alto = mais recentes);
2. `FNF_SESSAO.txt`;
3. `FNF_ANOM_*.txt` (se existirem — é onde estão os travamentos);
4. `FNF_SHOT_*.rgba.txt` (as capturas de tela que quiser mostrar);
5. `PROVA4.txt` (da `PROVA4_PS2.ELF`).

Com isto, cada item de cima passa de hipótese a provado ou refutado — e a
build seguinte corrige **a** causa medida.
