/* Tela de FREEPLAY: a geometria e o tempo das capsulas, contra a fonte.
 *
 * O QUE PROVA (SongMenuItem.hx / FreeplayState.hx v0.8.6, com as linhas nos
 * comentarios de src/engine/freeplay.h)
 *  - `intendedX(i) = 270 + 60*sin(i)` e `intendedY(i) = i*((h*0,8)+10) + 120`,
 *    reduzidos a metade (a tela do port e' 640x360, o original 1280x720);
 *  - os desvios das pontas de `changeSelection` (L2941-2949): -50 e +10 nos
 *    itens de fora e -100 nos que estao ACIMA do selecionado;
 *  - o `smoothLerpPrecision` (0,256 s em x e 0,192 s em y): com dt igual a
 *    constante o passo fica exatamente a 1 % do caminho;
 *  - a entrada: os 7 passos de 1/24 s da escala (`xFrames`) e o ciclo de 24 fps
 *    da animacao da capsula (selecionada e nao selecionada);
 *  - o `textAppear`: (1,7 ; 0,2) -> (0,4 ; 1,4) -> (1 ; 1) em 3/24 s.
 *
 * O que NAO prova: que a tela parece a do original na TV. Isso e' a tela.
 */
#include <stdio.h>
#include <math.h>

#include "freeplay.h"

static int falhas;
#define CHECK(c) do { if (!(c)) { printf("  FALHA: %s (linha %d)\n", #c, __LINE__); falhas++; } } while (0)
#define CHECK_EQ(a, b) do { long _a = (long)(a), _b = (long)(b); \
    if (_a != _b) { printf("  FALHA: %s == %s: %ld != %ld (linha %d)\n", #a, #b, _a, _b, __LINE__); falhas++; } } while (0)
/* Comparacao de ponto flutuante com a folga de 0,01 px: a unica coisa que o
 * arredondamento de 32 bits pode mexer nas contas da tela. */
#define CHECK_PX(a, b) do { double _a = (double)(a), _b = (double)(b); \
    if (fabs(_a - _b) > 0.01) { printf("  FALHA: %s == %s: %.4f != %.4f (linha %d)\n", #a, #b, _a, _b, __LINE__); falhas++; } } while (0)

int main(void)
{
    printf("== freeplay: a geometria da lista (SongMenuItem.hx)\n");

    /* intendedX = 270 + 60*sin(i), em 640x360 -> 135 + 30*sin(i). */
    CHECK_PX(fp_x(0), 135.0f);
    CHECK_PX(fp_x(1), 135.0 + 30.0 * sin(1.0));
    CHECK_PX(fp_x(-1), 135.0 - 30.0 * sin(1.0));
    CHECK_PX(fp_x(2), 135.0 + 30.0 * sin(2.0));

    /* intendedY = i*((131*0,8)+10) + 120 em 1280x720 -> i*57,8 + 60. */
    CHECK_PX(fp_y(0), 60.0f);
    CHECK_PX(fp_y(1), 117.8f);
    CHECK_PX(fp_y(4), 291.2f);
    CHECK_PX(fp_y(-1), 2.2f);

    /* O item 1-based: `capsuleIndex = index - curSelected` (L2944-2949), entao o
     * SELECIONADO tem rel = 1 e fica em intendedY(1) = 117,8. */
    CHECK_PX(fp_y_item(1, 0), 117.8f);
    CHECK_PX(fp_y_item(2, 1), 117.8f);
    CHECK_PX(fp_y_item(3, 1), 175.6f);
    /* rel == 0 so' acontece em `updateSongsScroll` (indice == curSelected), e o
     * teste cobra a formula pura: intendedY(0) = 60, sem desvio. */
    CHECK_PX(fp_y_item(1, 1), 60.0f);
    CHECK_PX(fp_y_item(2, 2), 60.0f);
    /* rel > 4: yOffset -= 10 (L2943) -> +5 na metade. */
    CHECK_PX(fp_y_item(5, 0), 60.0 + 5.0 * 57.8 + 5.0);
    CHECK_PX(fp_y_item(6, 0), 60.0 + 6.0 * 57.8 + 5.0);
    /* Acima do selecionado: -25 (rel < 0) e -50 (index < curSelected). */
    CHECK_PX(fp_y_item(1, 2), 2.2 - 25.0 - 50.0);
    CHECK_PX(fp_y_item(2, 3), 2.2 - 25.0 - 50.0);

    printf("== freeplay: o lerp da posicao (MathUtil.smoothLerpPrecision)\n");
    /* dt == tempo -> pow(0,01, 1) = 0,01, ou seja 1 % do caminho. */
    CHECK_PX(fp_suave_x(100.0f, 200.0f, 0.256f), 199.0f);
    CHECK_PX(fp_suave_y(100.0f, 200.0f, 0.192f), 199.0f);
    /* dt == 0 devolve o atual (a fonte nao chama pow com 0). */
    CHECK_PX(fp_suave_x(100.0f, 200.0f, 0.0f), 100.0f);
    /* Ja' no alvo: fica no alvo, sem sobra de arredondamento. */
    CHECK_PX(fp_suave_x(100.0f, 100.0f, 0.05f), 100.0f);
    /* Um passo de 1/60 s anda mais devagar que um de 0,05 s. */
    CHECK(fp_suave_x(0.0f, 100.0f, 1.0f / 60.0f) < fp_suave_x(0.0f, 100.0f, 0.05f));

    printf("== freeplay: a entrada das capsulas (initJumpIn)\n");
    /* A escala do salto: xFrames[passo] em x e o inverso em y (L687-688). */
    CHECK_PX(fp_escala_x(0), 1.70f);
    CHECK_PX(fp_escala_y(0), 1.0 / 1.70);
    CHECK_PX(fp_escala_x(2), 0.85f);
    CHECK_PX(fp_escala_x(FP_XFRAMES_N - 1), 1.00f);
    /* Depois do ultimo passo a escala fica em 1 (o `doJumpIn` desarma). */
    CHECK_PX(fp_escala_x(FP_XFRAMES_N), 1.0f);
    CHECK_PX(fp_escala_y(99), 1.0f);

    /* O passo do `frameInTicker` e' de 1/24 s (41,67 ms), nao de 42 ms. */
    CHECK_EQ(fp_passo_salto(0), 0);
    CHECK_EQ(fp_passo_salto(41), 0);
    CHECK_EQ(fp_passo_salto(42), 1);
    CHECK_EQ(fp_passo_salto(1000), 24);

    /* `targetPos.x = FlxG.width * xPosLerpLol[passo]` -- 640 * 0,245 = 156,8. */
    CHECK_PX(fp_x_do_salto(0), 0.0f);
    CHECK_PX(fp_x_do_salto(6), 156.8f);
    CHECK_PX(fp_x_do_salto(99), 156.8f);

    /* A capsula fica em alpha 0,5 ate' (7/24)*2,5 s = 729 ms e depois em 1. */
    CHECK_EQ(FP_ENTRADA_ESCONDE_MS, 729);

    printf("== freeplay: a animacao da capsula (24 fps, loop)\n");
    /* A cada 1/24 s troca de quadro; 8 quadros por animacao, entao volta ao 0
     * em 1/3 s. O `addByPrefix(..., 24)` do original cicla. */
    CHECK_EQ(fp_quadro(0, 0), 0);
    CHECK_EQ(fp_quadro(0, 41), 0);
    CHECK_EQ(fp_quadro(0, 42), 1);
    CHECK_EQ(fp_quadro(0, 500), 12 % 8);
    CHECK_EQ(fp_quadro(0, 1000), 0);
    /* A selecionada e a nao selecionada tem o mesmo tamanho de ciclo. */
    CHECK_EQ(FP_CAP_SEL_N, 8);
    CHECK_EQ(FP_CAP_NAO_N, 8);
    CHECK_EQ(FP_CAP_FPS, 24);

    printf("== freeplay: o texto da capsula (textAppear)\n");
    {
        float sx = 0.0f, sy = 0.0f;
        fp_texto_escala(0, &sx, &sy);
        CHECK_PX(sx, 1.70f); CHECK_PX(sy, 0.20f);
        fp_texto_escala(41, &sx, &sy);
        CHECK_PX(sx, 1.70f); CHECK_PX(sy, 0.20f);
        fp_texto_escala(42, &sx, &sy);
        CHECK_PX(sx, 0.40f); CHECK_PX(sy, 1.40f);
        fp_texto_escala(124, &sx, &sy);
        CHECK_PX(sx, 0.40f); CHECK_PX(sy, 1.40f);
        fp_texto_escala(125, &sx, &sy);
        CHECK_PX(sx, 1.0f); CHECK_PX(sy, 1.0f);
        fp_texto_escala(100000, 0, 0); /* nao pode estourar com NULL */
    }

    printf("== freeplay: os titulos do pacote\n");
    CHECK(fp_titulo("bopeebo") != 0);
    CHECK(fp_titulo("nao-existe") == 0);
    CHECK(fp_titulo(0) == 0);
    if (fp_titulo("bopeebo"))
    {
        const fp_titulo_t *t = fp_titulo("bopeebo");
        /* O arquivo e' o que o empacotador escreveu em assets/freeplay/. */
        CHECK(t->arquivo != 0 && t->arquivo[0] != 0);
        CHECK(t->w > 0 && t->h > 0);
        /* O maior titulo tem que caber no buffer de leitura do main.c. */
        CHECK(t->vram_bytes <= 24576u);
    }

    printf("== freeplay: o fundo (backingImage) e a entrada de 0,7 s\n");
    {
        /* `FreeplayState.hx` L529-531: x nasce em `-backingImage.width` (574 em
           640x360) e vai para 0 em 700 ms com `FlxEase.quintOut`.
           quintOut(t) = 1 - (1-t)^5. */
        float x0 = fp_fundo_x(0), xf = fp_fundo_x(700), xn = fp_fundo_x(100000);
        CHECK_PX(x0, -574.0f);
        CHECK_PX(xf, 0.0f);
        CHECK_PX(xn, 0.0f);
        /* Em metade do tempo o quintOut ja' passou de 96% do caminho: o valor
           em 350 ms tem de estar entre -574*0,03125 e 0, e mais perto de 0. */
        {
            float xm = fp_fundo_x(350);
            CHECK(xm > -574.0f * 0.04f);
            CHECK(xm < -0.5f);
        }
        /* Monotonico: nunca volta para tras. */
        CHECK(fp_fundo_x(100) < fp_fundo_x(200));
        CHECK(fp_fundo_x(200) < fp_fundo_x(400));
        CHECK(fp_fundo_x(400) < fp_fundo_x(699));
        /* O dado do pacote tem de bater com o que o empacotador escreveu. */
        CHECK(FP_FUNDO_BPP == 16);
        CHECK(FP_FUNDO_W > 0 && FP_FUNDO_H == 360);
        CHECK(FP_FUNDO_VRAM == (unsigned)FP_FUNDO_W * 360u * 2u);
    }

    if (falhas)
    {
        printf("FREEPLAY: %d FALHA(S)\n", falhas);
        return 1;
    }
    printf("FREEPLAY OK\n");
    return 0;
}
