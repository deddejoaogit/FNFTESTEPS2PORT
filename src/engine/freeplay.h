#ifndef FREEPLAY_H
#define FREEPLAY_H

/* A GEOMETRIA E O TEMPO DA TELA DE FREEPLAY (as capsulas).
 *
 * O QUE O ORIGINAL FAZ (`SongMenuItem.hx`, v0.8.6)
 *   Cada capsula de musica e' um `FlxSpriteGroup` cujo filho `capsule` e' um
 *   Sparrow de 16 quadros (8 da animacao `selected`, 8 da `unselected`) a 24
 *   fps, desenhado a `realScaled = 0.8` (L619). A posicao na lista vem de duas
 *   funcoes puras do deslocamento `i - curSelected`:
 *
 *       intendedX(i) = 270 + 60 * sin(i)                              (L762-765)
 *       intendedY(i) = i * ((capsule.height * realScaled) + 10) + 120  (L767-770)
 *
 *   e o sprite persegue esse alvo com `MathUtil.smoothLerpPrecision` -- x com
 *   0,256 s e y com 0,192 s de constante (L735-736), que e' um decaimento
 *   exponencial: `alvo + (atual - alvo) * 0,01^(dt / tempo)`.
 *
 *   Na entrada (`initJumpIn`, L624-647 + L670-700) cada capsula fica em
 *   `x = FlxG.width` (fora da tela), some com `alpha = 0,5` e reaparece com
 *   alpha 1 depois de `(7/24)*2,5` s; nesse meio-tempo o salto anda UMA casa
 *   por 1/24 s e escala a capsula com `xFrames[k]` em x e o INVERSO em y.
 *
 * TUDO AQUI E' FUNCAO PURA -- NAO CONHECE GS, VRAM NEM ARQUIVO
 *   Assim a conta roda no PC (test/test_freeplay.c) contra os numeros da fonte,
 *   e o `main.c` so' desenha o que este modulo responde.
 *
 * AS CONSTANTES VEM DE `src/engine/freeplay_dados.h`, que e' RENDERIZACAO de
 * `assets-data/freeplay-plan.json` (gerado de `SongMenuItem.hx`).
 */

#include "freeplay_dados.h"

/* A metade da tela do port em relacao ao original. */
#define FP_MEIA 0.5f

/* Deslocamento (em capsulas) do alvo de x (`intendedX`). */
float fp_x(int idx);

/* Deslocamento (em capsulas) do alvo de y (`intendedY`). */
float fp_y(int idx);

/* Um passo de `smoothLerpPrecision` (L735-736), com `dt` em segundos.
 * `fp_suave_x` usa a constante de 0,256 s e `fp_suave_y` a de 0,192 s. */
float fp_suave_x(float atual, float alvo, float dt);
float fp_suave_y(float atual, float alvo, float dt);

/* Quadro da animacao `sel` (1 = `selected`, 0 = `unselected`) que esta' na tela
 * `desde_ms` milissegundos depois de a animacao comecar. A animacao do original
 * e' `addByPrefix(..., 24)` sem `false` em `looped`, ou seja: cicla. */
int fp_quadro(int sel, unsigned long desde_ms);

/* A escala do salto de entrada no passo `passo` (0..FP_XFRAMES_N-1): x vale
 * `xFrames[passo]` e y vale o inverso (L676-677). Depois do ultimo passo a
 * escala fica em 1 (L692-698: `frameInTypeBeat == xFrames.length` desarma). */
float fp_escala_x(int passo);
float fp_escala_y(int passo);

/* Quantos passos o salto tem (7: `xFrames.length`). */
#define FP_SALTO_PASSOS FP_XFRAMES_N

/* O x do alvo DURANTE o salto: `FlxG.width * xPosLerpLol[passo]` (L688-690),
 * em 640x360. `passo` fora da tabela cai na ultima entrada (o original usa
 * `Math.min(passo, xPosLerpLol.length - 1)`). */
float fp_x_do_salto(int passo);

/* Quanto tempo (ms) a capsula fica invisivel na entrada:
 * `new FlxTimer().start((xFrames.length / 24) * 2.5)` (L637). */
#define FP_ENTRADA_ESCONDE_MS 729

/* O ALVO DE Y DE UM ITEM DA LISTA, do jeito que a `changeSelection` o escreve
 * (`FreeplayState.hx` L2930-2950). `indice` e' o indice 1-based do item (o
 * original guarda `index = i + 1` e cria a musica i com `index = i + 1`) e
 * `sel` e' o `curSelected` 0-based:
 *
 *     y = intendedY(index - sel)
 *     if (index - sel < 0) y -= 50;            // L2941-2942: yOffset += 50
 *     else if (index - sel > 4) y -= -10;      // L2943: yOffset -= 10
 *     if (index < sel) y -= 100;               // L2949: "another 100 for good measure"
 *
 * O comentario do original explica o porque: e' para as capsulas de fora da tela
 * sairem de verdade e nao precisarem ser desenhadas. */
float fp_y_item(int indice, int sel);

/* A ESCALA DO TEXTO na entrada (`SongMenuItem.textAppear`, L518-532):
 *   t = 0        -> (1,7 , 0,2)
 *   t = 1/24 s   -> (0,4 , 1,4)
 *   t = 3/24 s   -> (1   , 1  )
 * Ela e' chamada em `setVisibleGrp(true)` (729 ms) e de novo em toda troca de
 * selecao (`updateSelected`). */
void fp_texto_escala(unsigned long desde_ms, float *sx, float *sy);

/* Quantos passos de 1/24 s ja' passaram desde `desde_ms` (o `frameInTicker` do
 * `initJumpIn`, L676-683). */
int fp_passo_salto(unsigned long desde_ms);

/* O titulo de uma musica pelo id (NULL quando o id nao esta' no pacote). */
const fp_titulo_t *fp_titulo(const char *id);

#endif /* FREEPLAY_H */
