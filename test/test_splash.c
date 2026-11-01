/* NOTE SPLASH: o brilho do acerto SICK tem de existir, durar o que o original
 * dura e nascer onde o original nasce.
 *
 * O que este teste pode provar no PC: que a tabela de celulas e' a do plano
 * (a splash inteira cabe na faixa livre do HUD, sem invadir nota nem barra de
 * vida, e sem pedir VRAM nova); que o tamanho desenhado e' o canvas do
 * original sobre a metade do HUD; que a animacao tem os 4 quadros na ordem e a
 * duracao de 24 fps +- 2; que a geometria de sp_posicao reproduz
 * Strumline.playNoteSplash; e que o pool nao perde evento em silencio.
 *
 * O que ele NAO prova: que o brilho parece o do original na TV. Isso e' a tela.
 */
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "splash.h"

static int falhas;
#define CHECK(c) do { if (!(c)) { printf("  FALHA: %s (linha %d)\n", #c, __LINE__); falhas++; } } while (0)
#define CHECK_NEAR(a, b, tol) do { double _a = (double)(a), _b = (double)(b); \
    if (fabs(_a - _b) > (tol)) { printf("  FALHA: %s ~= %s: %.6f != %.6f (linha %d)\n", #a, #b, _a, _b, __LINE__); falhas++; } } while (0)

/* A barra de vida mora em (0,112) 301x10 no atlas do HUD, e as notas em
 * (lane*56, 0) 56x56. Nenhuma das duas pode ser atropelada. */
#define HUD_BARRA_Y 112
#define HUD_BARRA_H 10

int main(void)
{
    int lane, var, i;
    const nspl_cel_t *c;
    nspl_pool_t p;
    float x, y;

    printf("== splash: a regiao cabe na faixa livre do atlas do HUD ==\n");
    CHECK(SPLASH_TEX_W == 320 && SPLASH_TEX_H == 256);
    CHECK(SPLASH_REG_X + SPLASH_REG_W <= SPLASH_TEX_W);
    CHECK(SPLASH_REG_Y + SPLASH_REG_H <= SPLASH_TEX_H);
    CHECK(SPLASH_REG_Y >= HUD_BARRA_Y + HUD_BARRA_H);
    /* Quatro quadros empilhados: a faixa de uma animacao e' a altura/4. */
    CHECK(SPLASH_FAIXA * SPLASH_QUADROS == SPLASH_REG_H);
    CHECK(SPLASH_ANIMS == 8);          /* 4 direcoes x 2 variantes */

    printf("== splash: 8 animacoes, 4 quadros, celulas dentro da regiao ==\n");
    for (lane = 0; lane < 4; lane++) {
        for (var = 0; var < 2; var++) {
            c = nspl_celula(lane, var);
            CHECK(c != 0);
            if (!c) continue;
            CHECK(c->cw > 0 && c->ch > 0);
            CHECK(c->cx + c->cw <= SPLASH_REG_W);
            CHECK(c->ch <= SPLASH_FAIXA);
            /* A celula e' uma coluna inteira: os 4 quadros descem na faixa. */
            CHECK(c->cy == 0);
            /* Tamanho desenhado = canvas do original / 2 (HUD em 640x360). */
            CHECK_NEAR(c->dw, c->lw * 0.5, 0.001);
            CHECK_NEAR(c->dh, c->lh * 0.5, 0.001);
        }
    }
    /* As 8 celulas nao podem se sobrepor: duas animacoes no mesmo lugar
       desenhariam o brilho errado. */
    for (lane = 0; lane < 8; lane++) {
        int k;
        for (k = lane + 1; k < 8; k++) {
            const nspl_cel_t *a = &g_splash_cel[lane], *b = &g_splash_cel[k];
            int sobrepoe = !(a->cx + a->cw <= b->cx || b->cx + b->cw <= a->cx);
            CHECK(!sobrepoe);
        }
    }
    CHECK(nspl_celula(4, 0) == 0);
    CHECK(nspl_celula(0, 2) == 0);
    CHECK(nspl_celula(-1, 0) == 0);

    printf("== splash: alpha 0,8 e centro 0,3 vem do note style e da classe ==\n");
    CHECK(SPLASH_ALPHA == 204);        /* round(0.8 * 255) */
    CHECK_NEAR(SPLASH_CENTRO, 0.3, 1e-6);
    CHECK_NEAR(SPLASH_INITIAL_OFFSET, -0.275 * 104.0, 1e-4);
    CHECK_NEAR(SPLASH_OFF_X, 25.0, 1e-6);
    CHECK_NEAR(SPLASH_OFF_Y, -5.0, 1e-6);

    printf("== splash: a posicao e' a de Strumline.playNoteSplash ==\n");
    {
        /* Lane LEFT do port: receptor em (22,65 ; 25,30) em 640x360. O
         * original poe o splash em x = strum.x + getXPos + INITIAL_OFFSET +
         * 25*scale e y = strum.y - INITIAL_OFFSET - 5*scale, com o canto
         * 0,3 do tamanho para cima e para a esquerda. */
        c = nspl_celula(0, 0);
        nspl_posicao(22.65f, 25.30f, c, &x, &y);
        CHECK_NEAR(x, 22.65 + 12.5 - 0.3 * 130.0, 0.01);
        CHECK_NEAR(y, 25.30 + (28.6 - 5.0) * 0.5 - 0.3 * 149.0, 0.01);
        /* E o canto esquerdo fica acima do receptor: o brilho cobre a nota. */
        CHECK(y < 25.30f);
        /* Variante 2 tem outro canvas (274x267), entao o canto muda. */
        c = nspl_celula(0, 1);
        nspl_posicao(22.65f, 25.30f, c, &x, &y);
        CHECK_NEAR(x, 22.65 + 12.5 - 0.3 * 137.0, 0.01);
        CHECK_NEAR(y, 25.30 + (28.6 - 5.0) * 0.5 - 0.3 * 133.5, 0.01);
        nspl_posicao(0.0f, 0.0f, 0, &x, &y);
        CHECK(x == 0.0f && y == 0.0f);
    }

    printf("== splash: 4 quadros a 24 fps +- 2, e o fim mata a splash ==\n");
    nspl_init(&p, 1234u);
    CHECK(nspl_novo(&p, 0, 1000u) >= 0);
    {
        int q0 = nspl_quadro(&p, 0, 1000u);
        CHECK(q0 == 0);
        /* 1000 ms / fps: com 24 fps o quadro 1 comeca em 41,7 ms e o 3o em
           125 ms; o fim cai entre 153 ms (26 fps) e 182 ms (22 fps). */
        CHECK(nspl_quadro(&p, 0, 1040u) == 0 || nspl_quadro(&p, 0, 1040u) == 1);
        CHECK(nspl_quadro(&p, 0, 1125u) >= 2);
        CHECK(nspl_quadro(&p, 0, 1000u + 190u) == -1);
        CHECK(nspl_passo(&p, 1000u + 190u) == 0);
        CHECK(p.vencidos == 1);
    }

    printf("== splash: sorteio deterministico, com as duas variantes ==\n");
    {
        nspl_pool_t a, b;
        int vistos[2] = {0, 0};
        int fps_lo = 1000, fps_hi = 0;
        nspl_init(&a, 99u);
        nspl_init(&b, 99u);
        for (i = 0; i < 200; i++) {
            int ia = nspl_novo(&a, i % 4, 0u);
            int ib = nspl_novo(&b, i % 4, 0u);
            CHECK(ia == ib);
            if (ia < 0) continue;
            CHECK(a.it[ia].variante == b.it[ib].variante);
            CHECK(a.it[ia].fps == b.it[ib].fps);
            vistos[a.it[ia].variante & 1]++;
            if (a.it[ia].fps < fps_lo) fps_lo = a.it[ia].fps;
            if (a.it[ia].fps > fps_hi) fps_hi = a.it[ia].fps;
            /* Reciclagem: o pool tem 24 slots e ja' criamos 200 sem deixar
               nenhum vencer -- nada pode ser descartado sem contador. */
            nspl_passo(&a, 0u);
            nspl_passo(&b, 0u);
        }
        CHECK(vistos[0] > 0 && vistos[1] > 0);
        CHECK(fps_lo == 22 && fps_hi == 26);
    }

    printf("== splash: o pool enche, recicla o mais velho e conta ==\n");
    nspl_init(&p, 5u);
    for (i = 0; i < SP_MAX + 3; i++) CHECK(nspl_novo(&p, i % 4, (uint32_t)i) >= 0);
    CHECK(nspl_ativos(&p) == SP_MAX);
    CHECK(p.criados == SP_MAX + 3);
    CHECK(p.reciclados == 3);
    CHECK(nspl_novo(&p, -1, 0u) < 0);
    CHECK(nspl_novo(&p, 4, 0u) < 0);
    /* Nenhuma splash ativa pode estar "vencida" aos olhos de sp_quadro. */
    for (i = 0; i < SP_MAX; i++) {
        if (p.it[i].ativo) CHECK(nspl_quadro(&p, i, (uint32_t)(SP_MAX + 3)) >= 0);
    }

    printf("== splash: a fila anda e todas voltam a ficar livres ==\n");
    CHECK(nspl_passo(&p, 5000u) == 0);
    CHECK(nspl_ativos(&p) == 0);
    CHECK(p.vencidos == p.criados - p.reciclados);

    if (falhas) {
        printf("\n  %d falha(s) no note splash\n", falhas);
        return 1;
    }
    printf("  ok:   regiao, celulas, alpha, posicao, tempo e pool\n");
    return 0;
}
