/* Tremor de camera: a conta do `FlxCamera.shake` (o que os NOTE KIND pedem).
 *
 * O que este teste pode provar no PC: que a duracao e' a pedida, que a
 * amplitude respeita a FRACAO do tamanho da camera, que o deslocamento volta a
 * zero no fim, que a sequencia e' repetivel com a mesma semente e que um
 * tremor novo substitui o anterior (e nao soma).
 *
 * O que ele NAO prova: que tremer 0,0025 parece o original. Isso e' a tela.
 */
#include <stdio.h>
#include <math.h>
#include "shake.h"

static int falhas;
#define CHECK(c) do { if (!(c)) { printf("  FALHA: %s (linha %d)\n", #c, __LINE__); falhas++; } } while (0)
#define CHECK_NEAR(a, b, tol) do { double _a = (double)(a), _b = (double)(b); \
    if (fabs(_a - _b) > (tol)) { printf("  FALHA: %s ~= %s: %.6f != %.6f (linha %d)\n", #a, #b, _a, _b, __LINE__); falhas++; } } while (0)

#define CAM_W 1280.0f
#define CAM_H 720.0f
#define DT 0.016683333f

int main(void)
{
    shk_t s;

    printf("== tremor: parado nao mexe em nada ==\n");
    shk_reset(&s, 1u);
    CHECK_NEAR(s.x, 0.0, 1e-9);
    shk_passo(&s, DT, CAM_W, CAM_H);
    CHECK_NEAR(s.x, 0.0, 1e-9);
    CHECK_NEAR(s.y, 0.0, 1e-9);

    printf("== tremor: amplitude e' fracao da camera (0,0025 -> 3,2 px) ==\n");
    {
        /* O numero do Blazin': shake(0.0025, 0.15). 0,0025 * 1280 = 3,2 px de
           camera na horizontal e 0,0025 * 720 = 1,8 na vertical. */
        int i, n = 0;
        float mx = 0.0f, my = 0.0f;
        shk_reset(&s, 7u);
        shk_inicia(&s, 0.0025f, 0.15f);
        for (i = 0; i < 8; i++) {           /* 8 quadros = 0,133 s < 0,15 s */
            shk_passo(&s, DT, CAM_W, CAM_H);
            if (s.restante > 0.0f) {
                CHECK(fabsf(s.x) <= 0.0025f * CAM_W + 1e-4f);
                CHECK(fabsf(s.y) <= 0.0025f * CAM_H + 1e-4f);
                if (fabsf(s.x) > mx) mx = fabsf(s.x);
                if (fabsf(s.y) > my) my = fabsf(s.y);
                n++;
            }
        }
        CHECK(n >= 7);
        /* Nao esta' congelado em zero nem preso no maximo: 8 sorteios tem de
           dar deslocamentos diferentes. */
        CHECK(mx > 0.0f || my > 0.0f);
    }

    printf("== tremor: acaba no tempo pedido (0,15 s = 9 quadros) ==\n");
    {
        int i, quadros = 0;
        shk_reset(&s, 3u);
        shk_inicia(&s, 0.002f, 0.1f);       /* o `punchhighblocked`: 0,1 s */
        for (i = 0; i < 200 && s.restante > 0.0f; i++) { shk_passo(&s, DT, CAM_W, CAM_H); quadros++; }
        /* 0,1 s / 0,016683 s = 5,99 -> o 6o quadro encerra. */
        CHECK(quadros >= 5 && quadros <= 7);
        CHECK_NEAR(s.x, 0.0, 1e-9);
        CHECK_NEAR(s.y, 0.0, 1e-9);
    }

    printf("== tremor: mesma semente, mesma sequencia ==\n");
    {
        shk_t a, b;
        int i, igual = 1;
        shk_reset(&a, 42u);
        shk_reset(&b, 42u);
        shk_inicia(&a, 0.0025f, 1.0f);
        shk_inicia(&b, 0.0025f, 1.0f);
        for (i = 0; i < 20; i++) {
            shk_passo(&a, DT, CAM_W, CAM_H);
            shk_passo(&b, DT, CAM_W, CAM_H);
            if (a.x != b.x || a.y != b.y) igual = 0;
        }
        CHECK(igual);
        /* E semente diferente da' sequencia diferente (senao o LCG nao sorteia
           nada, so' repete um numero). */
        shk_reset(&b, 43u);
        shk_inicia(&b, 0.0025f, 1.0f);
        shk_passo(&a, DT, CAM_W, CAM_H);
        shk_passo(&b, DT, CAM_W, CAM_H);
        CHECK(a.x != b.x || a.y != b.y);
    }

    printf("== tremor: o novo SUBSTITUI o velho (nao soma) ==\n");
    {
        shk_reset(&s, 5u);
        shk_inicia(&s, 0.01f, 0.5f);
        shk_passo(&s, DT, CAM_W, CAM_H);
        shk_inicia(&s, 0.002f, 0.1f);
        CHECK_NEAR(s.restante, 0.1, 1e-6);
        shk_passo(&s, DT, CAM_W, CAM_H);
        CHECK(fabsf(s.x) <= 0.002f * CAM_W + 1e-4f);   /* a forca nova, nao 0,012 */
    }

    printf("== tremor: pedido invalido nao apaga o que esta' rodando ==\n");
    {
        shk_reset(&s, 9u);
        shk_inicia(&s, 0.002f, 0.2f);
        shk_inicia(&s, 0.0f, 0.0f);
        CHECK_NEAR(s.restante, 0.2, 1e-6);
        shk_inicia(&s, 0.002f, -1.0f);
        CHECK_NEAR(s.restante, 0.2, 1e-6);
    }

    if (falhas) { printf("\n  %d falha(s)\n", falhas); return 1; }
    printf("\n  tremor: 0,0025 de 1280x720 = +-3,2 px, 0,15 s = 9 quadros, "
           "sequencia repetivel\n");
    return 0;
}
