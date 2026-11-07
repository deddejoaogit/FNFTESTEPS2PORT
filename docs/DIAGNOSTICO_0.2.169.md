# DIAGNOSTICO 0.2.169 — o verdadeiro escritor que matava o jogo

## O que os logs da 0.2.168 disseram
- Q-lines (FNF_LOG02-4): 234 linhas, frames 0..599. in mediano 34 us (o cache
  dos RPCs funcionou), MAS 27 quadros com in=53-83 ms. **A sessao morreu
  outra vez por volta do quadro 600** (o despejo do f=600 foi escrito por
  completo e depois nada -- nao ha FNF_LOG03-4).
- er=0 ck=1: sem erros registados, canario vivo. O vigia NAO escreveu de novo
  (o caminho USB estava morto quando ele acordou, ou desligamento antes).
- DIAGNOSTICO-7/SESSAO-3: build 0.2.168 confirmada a correr.

## A CAUSA (finalmente medida, nao suposta)
TRES escritores de diagnostico dentro do laco do jogo, todos em mass::
1. **boot_mark("FRAME")** -- via capture_status a cada 200 ms -- abria BOOT.txt
   (open+write+fsync+close) durante os primeiros ~60 marcas (~12 s): e' o que
   media os picos de 53-83 ms em "in" (cadencia de 5/s = 200 ms). E este
   escritor NAO estava no portao de I/O: corria em paralelo com as leituras do
   produtor de audio -- a reentrancia que o usbhdfsd nao aceita.
2. **transbordo do buffer do log**: com 64 KB o buffer enchia a meio da sessao
   e o write_text ABRIA um despejo e escrevia DENTRO do quadro.
3. **quadro 600**: despejo grande (118 KB) + escrita BOOT.txt do frame_mark
   (porta de 600 quadros) = DUAS escritas seguidas -- o momento exacto da
   morte nas sessoes 0.2.167 e 0.2.168.

## CORRECAO (0.2.169) -- uma causa: escritas de diagnostico dentro do laco
1. boot_mark: o marco FRAME periodico fica SÓ em RAM (telemetry_phase); as
   escritas de boot que sobram passam pelo portao; erros registados.
2. frame_mark: nunca mais escreve em USB (anel RAM; o vigia despeja).
3. Buffer do log 64 KB -> 192 KB; transbordo = DESCARTE contado (DROPS no
   cabecalho), nunca escrita a meio do quadro.
4. Despejo periodico: 700 quadros (dessincronizado), F-records fora (~60 KB).
5. Vigia: MARCADOR MINIMO (96 B, open+write+close) ANTES do despejo grande --
   se o grande derrubar o driver, a linha de morrencia ja' existe.

## ESTADO
- provado: picos de 53-83 ms = escritas do boot_mark (cadencia 200 ms medida);
  morte das sessoes 0.2.167/0.2.168 junto ao duplo escrita do quadro 600;
  transbordo do buffer a meio do quadro (codigo).
- hipotese: a remocao destes tres escritores elimina o travamento; o marcador
  minimo do vigia regista a causa se ainda morrer.
- falta: medir no console; os 2 ficheiros das setas cortadas.

Harness ASan: PASSOU 1402 quadros, 2272 pacotes, 0 erros.
run_tests.sh: unica FALHA = orcamento dos palcos (pre-existente).
ELF md5 0f5ec28fd63ed4626548401a7e1e28c7.
