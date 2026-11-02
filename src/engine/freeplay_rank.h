#ifndef FREEPLAY_RANK_H
#define FREEPLAY_RANK_H

/* A CINEMATICA DE RANK DA FREEPLAY -- o que acontece quando o jogador volta
 * do resultado com um rank novo.
 *
 * O QUE O ORIGINAL FAZ (`FreeplayState.hx` v0.8.6, L1069-1344)
 *   `rankAnimStart` (L1069)   -> camaras, `rankBg`, a capsula no centro, 0,5 s
 *   `rankDisplayNew` (L1147)  -> o selo NOVO cresce de 20 para 0,9, as faiscas
 *   `rankAnimSlam` (L1227)    -> o fundo some, a capsula volta ao lugar com um
 *                                tremor, a vinheta pisca e o DJ faz o fist pump
 *   `SongMenuItem.fadeAnim`   -> o rastro aditivo (`FlxTrail`) do selo
 *
 * Este modulo e' a LINHA DO TEMPO: recebe o tempo desde o `rankAnimStart` e diz
 * onde cada coisa esta'. Nao conhece GS, VRAM nem arquivo -- roda no PC
 * (test/test_freeplay_rank.c) contra os numeros da fonte, e o `main.c` so'
 * desenha o que ele responde.
 *
 * AS CONTAS SAO AS DO FlxTween E DO FlxEase (`/tmp/flixelf`), com a formula
 * fechada de cada easing: `sineIn`, `backIn`, `backOut`, `backInOut`,
 * `elasticOut`, `expoIn`, `expoOut`, `quartIn`, `quadOut`. Quando a fonte cria
 * DUAS tweens da mesma propriedade, o port aplica as duas em ORDEM de criacao e
 * a ultima que ja' comecou vence -- que e' o que o `FlxTween` faz, porque o
 * `globalManager` atualiza na ordem em que as tweens foram criadas.
 *
 * O QUE NAO ESTA AQUI: som (`ranks/rankinbad|rankinnormal|rankinperfect` e
 * `ranks/loss|good|great|excellent|perfect`) e o `FlxTrail` do `fadeAnim`.
 * O primeiro e' do `audio`; o segundo e' um rastro de 15 amostras que o port
 * desenha como um leque de quads aditivos (ver `fp_rk_rastro`).
 */

#include "freeplay_dados.h"

/* A tela do original (1280x720) na metade, como todo o resto do port. */
#define FP_RK_TELA_W   640.0f
#define FP_RK_TELA_H   360.0f

/* OS INSTANTES, em ms desde o `rankAnimStart`. Cada um tem linha na fonte. */
#define FP_RK_T_DISPLAY   500   /* L1146: `new FlxTimer().start(0.5, rankDisplayNew)` */
#define FP_RK_T_FAKE_OFF  600   /* L1176: `new FlxTimer().start(0.1, ...)`          */
#define FP_RK_T_QUEDA     900   /* L1219: `new FlxTimer().start(0.4, ...)`          */
#define FP_RK_T_SLAM     1100   /* L1223: `new FlxTimer().start(0.6, rankAnimSlam)` */
#define FP_RK_T_POUSO    1600   /* L1253: `new FlxTimer().start(0.5, ...)`          */
#define FP_RK_FIM        2200   /* pouso + o `IntervalShake` de 0,6 s (L1284)       */

/* `rankBg.makeSolidColor(FlxG.width, FlxG.height, 0xD3000000)` (L480) -- o
 * alfa 0xD3 e' o que escurece a tela por cima da lista inteira. */
#define FP_RK_BG_ALFA  211

typedef struct {
    /* As duas cameras do estado (`FunkinCamera`, L324-325). So' o `rankCamera`
     * desenha coisa visivel: no `funnyCam` moram o `capsuleHitbox` (invisivel)
     * e os `bs` do fundo. O zoom dele entra aqui porque e' o dado da fonte. */
    float rc_zoom, fc_zoom;

    /* As duas camadas de tela cheia do `rankCamera`, de tras para frente. */
    float fade_alpha;    /* `rankCamera.fade(0xFF000000, 0.5, true)`: 1 -> 0 */
    float bg_alpha;      /* `rankBg`, 1 -> 0 a partir de 1,1 s (`expoIn`)   */

    /* A capsula alvo -- o grupo INTEIRO (capsula + titulo + selo) -- no
     * espaco da tela, sem o tremor. Em 640x360. */
    float cap_x, cap_y;
    /* O tremor do `IntervalShake` sobre ela, em px, e o angulo em GRAUS. */
    float cap_dx, cap_dy, cap_ang;

    /* O selo NOVO (`ranking` + `blurredRanking`, L1150-1168). */
    int   selo_visivel;
    float selo_scale;    /* 20 -> 0,9 em 0,1 s (linear, L1160)              */

    /* O selo ANTIGO (`fakeRanking`/`fakeBlurredRanking`), so' quando ha'
     * `fromResults.oldRank` (L1091-1093): some em +0,1 s.                 */
    int   fake_visivel;

    /* As faiscas: quadro do Sparrow (0..4) ou -1 quando nao desenha.       */
    int   sparks_q, sparksadd_q;

    /* A vinheta (`rankVignette`): 1 -> 0 em 0,6 s `expoOut` (L1279-1281).  */
    float vinheta_alpha;

    /* Fase para a telemetria: 0 antes do display, 1 no display, 2 na queda,
     * 3 no slam, 4 no pouso, 5 depois do fim.                             */
    int   fase;
} fp_rk_estado_t;

/* O estado da cinematica no instante `ms`. `tem_rank_antigo` liga o
 * `fakeRanking` (o rank que o save tinha antes desta musica). */
void fp_rk_avaliar(unsigned long ms, int tem_rank_antigo, fp_rk_estado_t *e);

/* O QUE ACONTECE COM AS CAPSULAS VIZINHAS (L1294-1340): cada uma ganha um
 * angulo sorteado e um tremor, com atraso de `dist/20` s e intensidade
 * `0,12/(dist+1)`. `dist` = |indice - selecionado| - 1; so' `dist < 5` entra.
 * `semente` deixa o resultado reproduzivel (o original usa `FlxG.random`). */
typedef struct { float ang, dx, dy; int ativo; } fp_rk_viz_t;
void fp_rk_vizinho(unsigned long ms, int dist, unsigned int semente, fp_rk_viz_t *v);

/* A posicao da capsula alvo no CENTRO da tela (L1140-1142:
 * `setPosition((FlxG.width/2) - (capsule.width/2), ...)`) e a posicao da lista
 * para onde ela volta (`originalPos`, L1120-1121). Em 640x360. */
float fp_rk_centro_x(void);
float fp_rk_centro_y(void);
float fp_rk_origem_x(void);
float fp_rk_origem_y(void);

/* O rastro do `fadeAnim` (`SongMenuItem.hx` L400-437): um `FlxTrail` de 15
 * amostras, alpha 0,01 na primeira e `-0,069` em cada seguinte, ADD, que some
 * em 0,6 s `quadOut`. Medido na fonte do `flixel-addons` (`FlxTrail.hx`
 * L280-305): so' a amostra 0 sobrevive (`alpha <= 0` mata as outras) e o
 * `redrawTrailSprites` (L204) revive todas, mas com alfa negativo o desenho e'
 * nulo. Aqui: `fp_rk_rastro` devolve o alpha da amostra `k` (0 para k > 0) e
 * escreve em `escala` a escala da copia naquele instante, que cresce de 1 a
 * 2,5 em 0,5 s (o tween linear do `impactThing`, L408). `ms` conta do POUSO. */
float fp_rk_rastro(unsigned long ms, int k, float *escala);

#endif /* FREEPLAY_RANK_H */
