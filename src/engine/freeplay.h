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

/* O x do FUNDO da tela (`backingImage`) em 640x360, `desde_ms` milissegundos
 * depois de a tela abrir. Ele nasce em `-width` e vai para 0 em 700 ms com
 * `FlxEase.quintOut` (`FreeplayState.hx` L529-531); o y e' sempre 0 e a altura
 * e' a da tela -- `setGraphicSize(0, FlxG.height + 1)` (L456). */
float fp_fundo_x(unsigned long desde_ms);

/* ---- O BRILHO DO SELO DOURADO (PERFECT_GOLD) --------------------------
 *
 * `SongMenuItem.hx` L156-166 cria o sprite em `(ranking.x, ranking.y)` =
 * (420, 41), com `scale 0,8`, `BlendMode.ADD`, `alpha = 0,7`, `visible =
 * false` e a animacao `sparkle` tocando UMA vez (10 quadros a 24 fps; o
 * decimo e' 1x1, o "vazio" em que ela para).
 * `updateScoringRank` (L493-504) so' e' dourado quando o rank e'
 * PERFECT_GOLD: ai' poe `visible = true` e arma o PRIMEIRO disparo em 1 s.
 * `sparkleEffect` (L228-233) sorteia a posicao em `x - 20..x + 3`,
 * `y - 29..y + 4`, da' `play('sparkle', true)` e re-arma em 1,2..4,5 s.
 *
 * O QUE E' FIEL E O QUE NAO E'
 * Posicao, intervalos, quadro e a ORDEM dos sorteios (x, y, intervalo) sao os
 * da fonte. A sequencia de numeros em si nao: o original sorteia pelo
 * `FlxG.random`, o port por um LCG deterministico com semente por musica --
 * a mesma escolha registrada em `shake.h`, porque o harness tem de dar o
 * mesmo resultado duas vezes. O que se preserva e' a distribuicao uniforme
 * nas faixas e o instante de cada disparo.
 *
 * C puro: sem GS, sem VRAM, sem arquivo. Quem chama passa o dt.
 */

typedef struct {
    float x, y;       /* canto do sprite no espaco do item, em 640x360 */
    float t_anim;     /* segundos desde o disparo que esta' desenhando */
    float t_espera;   /* segundos que faltam para o proximo disparo    */
    unsigned int semente;
    int armado;       /* 0 = o rank nao e' PERFECT_GOLD: nada a desenhar */
} fp_spk_t;

/* Quadros da animacao (`addByPrefix('sparkle', 'sparkle Export0', 24, false)`).
 * Igual a `FP_SP_QUADROS` do pacote; o `main.c` cobra isso num assert. */
#define FP_SPK_QUADROS FP_SP_QUADROS
#define FP_SPK_FPS     FP_SP_FPS

/* Desarma: o rank deixou de ser dourado (ou a tela esta' sendo montada).
 * `semente` fica guardada para a proxima vez que armar. */
void fp_spk_desarma(fp_spk_t *s, unsigned int semente);

/* `updateScoringRank(PERFECT_GOLD)`: armado, no meio do item (`base_x`,
 * `base_y` ja' na metade) e parado no ULTIMO quadro -- e' onde a animacao que
 * tocou na criacao parou. O primeiro disparo vem em 1 s. */
void fp_spk_arma(fp_spk_t *s, unsigned int semente, float base_x, float base_y);

/* Um quadro do estado. `base_x`/`base_y` sao o (ranking.x, ranking.y) do item,
 * o ponto de onde o sorteio de `sparkleEffect` parte. */
void fp_spk_passo(fp_spk_t *s, float dt, float base_x, float base_y);

/* Quadro do atlas a desenhar, ou -1 quando nao ha' nada (desarmado). Depois
 * de 10/24 s a animacao PARA no ultimo quadro, como o `looped = false` do
 * original -- nao reinicia sozinha. */
int fp_spk_quadro(const fp_spk_t *s);

/* ---- O CORACAO DE FAVORITO -------------------------------------------------
 *
 * `SongMenuItem.hx` L202-219 cria DUAS copias do mesmo Sparrow
 * (`freeplay/favHeart`) em `(380, 40)` no espaco do item, ambas `BlendMode.ADD`
 * e invisiveis; `checkClip` (L306-318) poe o x em 370 quando o selo de rank
 * esta' visivel e em 405 quando nao esta'; `updateSelected` (L801-802) da'
 * `alpha 1` na nitida e `0,6` na do item nao-selecionado, e `0`/`1` na
 * `Blurred` (que so' aparece no selecionado).
 *
 * A ANIMACAO e' de 13 quadros a 24 fps, UMA vez, e PARA no ultimo
 * (`initData` L568-569 poe `curFrame = numFrames - 1` de proposito): o repouso
 * e' o quadro 12. Quem toca e' `favoriteSong` (`FreeplayState.hx` L3079-3130):
 *   - favoritar: `visible = true` nos dois, `play('fav')` do comeco;
 *   - desfavoritar: `play('fav', true, true, 9)` -- REVERSO, do quadro 9 para
 *     tras -- e um `FlxTimer` de 0,2 s esconde os dois.
 * O `y` do ITEM leva um empurrao de 5 px (metade aqui: 2,5): favoritar desce,
 * desfavoritar sobe, 0,1 s `expoOut`, 0,1 s parado e 0,1 s `expoIn` de volta.
 *
 * C puro: sem GS, sem arquivo, sem tempo real.
 */
typedef struct {
    float t;        /* segundos desde o ultimo toque                      */
    int   modo;     /* FP_FV_PARADO / FP_FV_ENTRANDO / FP_FV_SAINDO       */
    int   visivel;  /* 0 = nada a desenhar (musica nao favorita)          */
} fp_fav_t;

#define FP_FV_PARADO   0
#define FP_FV_ENTRANDO 1
#define FP_FV_SAINDO   2

/* Zera: invisivel e parado. */
void fp_fav_reset(fp_fav_t *f);

/* A musica ja' era favorita quando a tela abriu (`refreshDisplay` ->
 * `favIcon.visible = freeplayData.isFav`): aparece "em repouso", sem animar. */
void fp_fav_mostra(fp_fav_t *f);

/* `favoriteSong()`: `favoritou` != 0 toca do comeco; 0 toca o reverso e some
 * em 0,2 s. */
void fp_fav_toggle(fp_fav_t *f, int favoritou);

/* Um quadro do estado (`favoritou` guarda o que o toque pediu). */
void fp_fav_passo(fp_fav_t *f, float dt);

/* Quadro do atlas a desenhar, ou -1 quando o coracao nao aparece. */
int fp_fav_quadro(const fp_fav_t *f);

/* O empurrao do `y` do ITEM, em px de 640x360 (0 fora da janela de 0,3 s). */
float fp_fav_dy(const fp_fav_t *f);

/* Se o coracao esta' visivel (para o `main.c` nao desenhar duas texturas). */
int fp_fav_visivel(const fp_fav_t *f);

#endif /* FREEPLAY_H */
