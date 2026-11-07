# DIAGNOSTICO 0.2.167 — o lag dos menus, medido

## O que o utilizador reportou (0.2.166)
Menus com FPS muito mau; a tela de titulo parecia travar depois de alguns
segundos. Enviou os ficheiros novos (FNF_LOG*, FNF_ANOM*, FNF_SESSAO,
FNF_SCREEN).

## PLANO (antes de analisar, como pedido)
1. Protocolo de teste claro para o utilizador (ver ENTREGA).
2. Analisar os Q-lines novos por fase (in/upd/dma/wait/cap/vs/post/fl).
3. Uma causa por build; correcao medida; harness antes de entregar.

## ANALISE (376 linhas Q de 899 quadros, sessao de ~15 s apos boot)
- t_total: mediana 15 920 us (62 fps), **p90 16 237, media 22 342**.
- **27 quadros (7%) com `in`=53 ms mediano, a cada 9 quadros** (cadencia de
  5/s): `frame_mark` -> `fnmark_flush` a escrever BOOT.txt dentro do laco.
  Total perdido so' ali: 1,73 s em 899 quadros. Era o "travado" dos menus
  (o mesmo pico que a 0.2.164 aparecia como draw 33-50 ms a cada 8 quadros).
- `fl` (custo do despejo de telemetria): **58 810 us mediano**, max 220 ms,
  a cada 300 quadros (5 s).
- FNF_SCREEN_0000.rgba.txt **0 bytes** = captura automatica por mudanca de
  estado abriu o ficheiro e foi interrompida (o utilizador desligou durante
  a escrita de 286 KB com o audio pausado).
- FNF_ANOM_00 = 343 B: unico quadro >250 ms foi o f=0 (724 ms, boot normal).
- Fim da sessao (f=899): fps 61-63 no titulo — nao houve congelamento
  permanente no log; a experiencia de "travar" e' a soma dos picos acima.

## CAUSA UNICA
Diagnostico a escrever em USB dentro do laco critico (regra 7 violada):
(a) BOOT.txt 5x/s a 53 ms; (b) captura automatica 286 KB por mudanca de
estado; (c) despejo de telemetria 59 ms a cada 5 s.

## CORRECAO (0.2.167)
1. `FNMARK_MIN_GAP_TICKS` 200 ms -> **10 s** (`FNMARK_FRAME_GATE` 300 -> 600).
   Custo cai de 53 ms/150 ms para 53 ms/10 s (0,5%).
2. **Captura automatica removida** — fica a manual SELECT+R3 (o que foi pedido).
3. Despejo periodico 300 -> **600 quadros** (10 s): 59 ms a cada 10 s (0,6%).

## ESTADO
- provado: causa do lag (Q-lines por fase, cadencia igual a do portao 5/s).
- hipotese: a experiencia de "travar no titulo" incluia a captura interrompida
  (ficheiro 0 B) — a remocao deve elimina-la; a confirmar no console.
- falta: medir 0.2.167 no console (Q-lines de novo); ficheiros das setas
  cortadas (tutorial_hud_official_320x256_gsalpha.rgba, menu_sfx.pcm).
