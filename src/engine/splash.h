#ifndef SPLASH_H
#define SPLASH_H

/* NOTE SPLASH -- o brilho que nasce na nota quando o acerto e' SICK.
 *
 * A REGRA VEM DA FONTE (Funkin v0.8.6), nao de imitacao:
 *
 *   PlayState.goodNoteHit        -> playNoteSplash(dir) so' com doesNotesplash,
 *                                   que o evento so' leva em SICK.
 *   Strumline.playNoteSplash     -> exige showNotesplash (opcao, padrao true)
 *                                   e noteStyle.isNoteSplashEnabled();
 *                                   x = strum.x + getXPos(dir) + INITIAL_OFFSET
 *                                       + splashOffsets[0] * scale
 *                                   y = strum.y - INITIAL_OFFSET
 *                                       + splashOffsets[1] * scale
 *   NoteSplash.play              -> sorteia a variante, frameRate = 24 +- 2,
 *                                   offset.set(w * 0.3, h * 0.3) e kill() no
 *                                   fim da animacao (4 quadros).
 *
 * Este modulo guarda SO' o estado e a geometria; ele nao conhece o GS, o
 * ps2sdk nem o main.c -- por isso roda inteiro no PC (test/test_splash.c).
 * A tabela de celulas e' gerada de assets-data/splash-plan.json por
 * tools/gen_splash_dados.py.
 *
 * Sem dependencia de PS2: tipos de <stdint.h> apenas.
 */

#include <stdint.h>

#include "splash_dados.h"

/* Quantas splashes podem existir ao mesmo tempo. No original nao ha' limite:
 * cada acerto cria um FlxSprite. Aqui a duracao e' de ~167 ms (4 quadros a
 * 24 fps), entao 24 slots cobrem 144 notas por segundo -- mais do que qualquer
 * chart do jogo. Se mesmo assim estourar, o slot MAIS ANTIGO e' reaproveitado
 * e `reciclados` conta: nenhum evento novo e' descartado em silencio. */
#define SP_MAX 24

typedef struct {
    uint8_t lane;       /* 0..3, na ordem do note style: left/down/up/right */
    uint8_t variante;   /* 0 ou 1 -- qual `splash<DIR><n>` o original sorteia */
    uint8_t ativo;
    uint16_t fps;       /* 24 +- 2, sorteado por splash como no original */
    uint32_t t0;        /* ms do acerto */
} nspl_item_t;

typedef struct {
    nspl_item_t it[SP_MAX];
    uint32_t rng;
    int criados;
    int vencidos;
    int reciclados;
} nspl_pool_t;

/* Zera o pool. `seed` fixa a sequencia de sorteios (determinismo no teste). */
void nspl_init(nspl_pool_t *p, uint32_t seed);

/* Cria uma splash na lane. Devolve o indice do slot ou -1 para lane invalida. */
int nspl_novo(nspl_pool_t *p, int lane, uint32_t ms);

/* Mata o que terminou a animacao. Devolve quantas continuam ativas. */
int nspl_passo(nspl_pool_t *p, uint32_t ms);

/* Quadro 0..3 da splash `i` no instante `ms`, ou -1 se ela nao esta' ativa. */
int nspl_quadro(const nspl_pool_t *p, int i, uint32_t ms);

/* Quantas splashes estao ativas. */
int nspl_ativos(const nspl_pool_t *p);

/* Celula de (lane, variante) no atlas do HUD, ou ponteiro nulo se fora. */
const nspl_cel_t *nspl_celula(int lane, int variante);

/* Canto superior esquerdo do desenho, em pixels de 640x360 (sem o zoom do HUD).
 * `receptor_x`/`receptor_y` sao o canto do receptor dessa lane, que e' o ponto
 * onde o original ancora o splash. */
void nspl_posicao(float receptor_x, float receptor_y, const nspl_cel_t *c,
                float *x, float *y);

#endif /* SPLASH_H */
