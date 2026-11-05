# DIAGNÓSTICO 0.2.165/166 — o que os ficheiros do console provaram

Data: 2026-11-05 · build analisada: **0.2.164** · correções em: **0.2.166**
Ficheiros recebidos: `PROVA4.txt`, `FNF_LOG00..31`, `FNF_TRAVA(-2)`, `BOOT-4`,
`DIAGNOSTICO-4`, `FNF_SCREEN_0000/0001` (3 capturas completas).

---

## 1. PROVA 4 — as três perguntas fechadas

### 1.1 FONTE: a grade está CERTA (provado) — o culpado era o slot da textura

Perfil de tinta do `font.rgba` (384×192):

* colunas: tinta em blocos de ~24 px com ~2 px de folga por célula;
* linhas: tinta em blocos de 32 px, 6 linhas;
* bbox das 8 primeiras células 24×32: `!` 2 px, `"` 6 px no topo, `#` 15 px,
  `$` 99-108 com descendente a y=22, `%` 17 px, `&` 14 px, `'` 2 px no topo —
  **exactamente o alfabeto ASCII 32..127, 16 col × 6 linhas de 24×32**.

Conclusão: **o asset da fonte e a grelha do `text_draw` estão corretos.** O
texto saía em "pedaços de duas letras" por outra razão, encontrada no código:

> **O slot da fonte (`fonttex`) era partilhado por DUAS fontes.** O tutorial e
> o story faziam upload de uma imagem VCR de 160×120 (grelha 10×20) por cima
> da fonte real (384×192, grelha 24×32). A partir daí, TODAS as chamadas
> `text_draw` amostravam a imagem errada com a grelha errada — menu, HUD,
> pause, tudo em pedaços/espelhado — e a fonte só voltava num caminho
> residual ("placeholder"). As capturas do console mostram exactamente isto:
> primeiro menu bom, e depois de visitar story/gameplay, "botões bugados".

**Correção (0.2.166):** uma grelha, uma fonte. `hud_score_line` e
`story_vcr_line` passaram à grelha 24×32 da fonte real; nada volta a escrever
no slot (`vcr_slot_fechado` no log). Fidelidade de estilo (bold VCR) trocada
por legibilidade — imagem é negociável, texto não.

### 1.2 VOCAIS: os stems do pendrive NÃO têm voz (provado)

`[InstL,InstR,VoxL,VoxR]`, 8 B/amostra, 3 posições (25/50/75 %) × 6 músicas:

```
2hot, blammed, blazin, bopeebo, cocoa, dadbattle
→ inst=100% (não-nulo)  vox=0.0% (tudo zero)  em TODAS as posições
```

Os canais de voz são silêncio digital. O empacotador documenta: *"música sem
`Voices.ogg` recebe vozes em silêncio"* — logo o clone de arte usado na
geração não tinha `Voices.ogg` (ou tinha ficheiros vazios). **Isto é dado em
falta na geração dos dados, não bug do jogo.** O toggle da 0.2.164 (que já
não desliga as vozes acidentalmente) estava certo; faltava era o que tocar.

Receita para repor (próximo passo, requer o clone `funkin.assets`):
`python3 tools/empacotar_stems.py --assets <clone>` com `Voices.ogg` presentes,
e copiar os `stems.pcm` novos para o pendrive. O manifesto `STEMS.txt` diz,
música a música, o que entrou.

### 1.3 PACOTES: cabeçalhos bons (provado) — 1 ficheiro do sserafim não abre

Todos os `stages/*/*.cfz4` com magic `CFZ4 v1` e todos os `chars/*.chp4` com
`CHP4 v2` — **0 cabeçalhos maus**. Excepção: `sserafim/…` tem 1 ficheiro que
não abre (`[cut:nao abre]` — ficheiro de cutscene, não é arte de jogo).

Nota honesta: a lista de personagens da PROVA 4 parava aos 6 por limite do
próprio teste (`vistos >= 6`) — **não** é falta de personagens no pendrive.

---

## 2. LOG 0.2.164 — o lag medido e a patologia do próprio log

### 2.1 O quadro normal é 60 fps; os picos é que doem

119 quadros analisados (F-lines): **mediana 8,11 ms = 60 fps** (draw ≈ 2 ms,
vsync ≈ 6 ms). Padrões:

| Padrão | Medida | Onde |
|---|---|---|
| Picos de 33–50 ms (draw 30–46 ms) a cada ~8 quadros | f=10560…10635, estados 7/10/11 | menu/story — custo periódico no update+draw |
| Pico de **5 112 ms** num quadro | f=10642, entrada na freeplay (st=11) | carregamento da cápsula da freeplay |
| Gameplay (st=8) com vários quadros > 33 ms | f=10659…10676 | fase — ver Q-lines da 0.2.166 |

A 0.2.166 mede cada fase por quadro (linha `Q`: `in/upd/dma/wait/cap/vs/post`
em µs) — o pico seguinte já não é hipótese, é campo.

### 2.2 Porque é que o log 0.2.164 veio quase vazio

32 ficheiros, quase todos com UMA linha: cada `telemetry_phase` chamava
`flush_output()` e o despejo criava um **arquivo novo por linha**. Os F-lines
reais estavam só nos ficheiros 14/15/16 (o buffer de 8 KB). **Corrigido
(0.2.166):** as linhas ficam no buffer; cada despejo (5 s) escreve um arquivo
inteiro; os 32 arquivos ≈ 2,5 min de história + anel de 512 quadros
(`FNF_ANOM_XX.txt`) sempre que um quadro passa de 250 ms.

### 2.3 Travamentos (FNF_TRAVA-2)

* f=571: parou no início do quadro (antes de `F.IN`) — 3 s+;
* f=4804: parou depois de `F.TITLE` no story — 3 s+, duas vezes.
A janela do anel diz exactamente a última fase completa; a 0.2.166 acrescenta
o despejo do anel de 512 quadros com os tempos por fase nesse instante.

---

## 3. As capturas de tela (o que se vê)

* `shot0000` (story): rótulos em pedaços + barras — era o texto com a fonte
  errada (slot VCR); a 0.2.166 tira as capturas automáticas a cada troca de
  ecrã (`FNF_SHOT_XX`), para o próximo relatório já vir com a prova.
* `shot0001/0003` (fase): setas cortadas a meio, texto ilegível, cenário só
  num canto, barra de vida presente — texto resolvido pela fonte; **setas e
  cenário continuam em investigação** (§4).

---

## 4. O que fica aberto (prioridades atualizadas)

1. **Setas cortadas (receptores)**: os receptores saem de
   `tutorial_hud_official_320x256_gsalpha.rgba` com células de 56 px. O atlas
   não está no repositório — **preciso do ficheiro** (327 680 B) para medir o
   passo real das células (56 vs 64) e corrigir os UVs. Enquanto isso é
   hipótese, não afirmação.
2. **Cenário/personagens quase invisíveis nas semanas**: os pacotes estão
   bons (§1.3); a suspeita passa para UV/escala dos props e recorte. Os
   eventos `palco_*` e as capturas da 0.2.166 dizem qual.
3. **Vocais**: repor `Voices.ogg` no empacotamento (§1.2).
4. **Picos de 8 quadros / freeplay 5 s**: medir com as Q-lines e corrigir a
   causa medida (uma por build).
5. **MERCH/OPTIONS/CREDITS**, traços do título, 5 palcos acima do orçamento
   de VRAM (a falha pré-existente do harness), `sserafim` 1 ficheiro mau.

---

## 5. Provado / hipótese / falta

* **Provado**: grade da fonte 16×6 de 24×32 (PROVA 4); stems sem voz
  (vox=0,0 % em 18 medições); pacotes CFZ4/CHP4 válidos; 60 fps de base com
  picos de 33–50 ms a cada ~8 quadros e 5,1 s na freeplay (F-lines); o slot
  da fonte era partilhado e ficava errado após story/tutorial (código +
  capturas); o log perdia a história por um arquivo por linha (ficheiros).
* **Hipótese**: setas cortadas por passo de célula errado no atlas do HUD
  (56 vs 64) — falta o ficheiro; cenário cortado por UV dos props; picos de
  8 quadros ligados a trabalho periódico no update/draw (as Q-lines decidem).
* **Falta**: voz nas músicas (regenerar dados), MERCH/OPTIONS/CREDITS,
  traços do título, orçamento dos 5 palcos, ficheiro `sserafim` que não abre.
