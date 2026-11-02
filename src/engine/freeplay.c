/* A GEOMETRIA E O TEMPO DA TELA DE FREEPLAY -- ver freeplay.h para a fonte.
 *
 * Nada aqui toca GS, VRAM ou arquivo: sao as contas do `SongMenuItem.hx` e da
 * `MathUtil` do original, para poderem ser cobradas no PC.
 */

#include <math.h>
#include <string.h>

#include "freeplay.h"

/* `FlxMath.fastSin` e' uma aproximacao de seno; a diferenca para `sinf` e' de
 * 0,1 % na faixa que a tela usa (|idx| <= ~6 rad), e o alvo aqui e' a posicao
 * de 1280x720 reduzida a metade -- o erro fica abaixo de 0,01 px. Usar `sinf`
 * e' a escolha registrada, nao um esquecimento. */
float fp_x(int idx)
{
    return FP_X_BASE + FP_X_AMP * sinf((float)idx);
}

float fp_y(int idx)
{
    return FP_Y_BASE + FP_Y_PASSO * (float)idx;
}

/* `MathUtil.smoothLerpPrecision(base, alvo, dt, tempo)` (L153-158):
 *   lerp(alvo, base, pow(0,01, dt / tempo))
 * = alvo + (base - alvo) * pow(0,01, dt / tempo).
 * `dt == 0` devolve o proprio base e base == alvo devolve o alvo, como na fonte. */
static float fp_suave(float atual, float alvo, float dt, float tempo)
{
    if (dt <= 0.0f || tempo <= 0.0f) return atual;
    if (atual == alvo) return alvo;
    return alvo + (atual - alvo) * powf(0.01f, dt / tempo);
}

float fp_suave_x(float atual, float alvo, float dt)
{
    return fp_suave(atual, alvo, dt, FP_LERP_X);
}

float fp_suave_y(float atual, float alvo, float dt)
{
    return fp_suave(atual, alvo, dt, FP_LERP_Y);
}

int fp_quadro(int sel, unsigned long desde_ms)
{
    int n = sel ? FP_CAP_SEL_N : FP_CAP_NAO_N;
    unsigned long passo;
    if (n <= 0 || FP_CAP_FPS <= 0) return 0;
    /* 24 fps: o quadro troca a cada 1000/24 ms. O flixel conta os quadros pelo
     * acumulador de `frameTimer` e para no ultimo quando nao e' `looped`; a
     * animacao da capsula E' loopada, entao o resto do ciclo manda. */
    passo = (desde_ms * (unsigned long)FP_CAP_FPS) / 1000ul;
    return (int)(passo % (unsigned long)n);
}

float fp_escala_x(int passo)
{
    if (passo < 0) passo = 0;
    if (passo >= FP_XFRAMES_N) return 1.0f;
    return fp_xframes[passo];
}

float fp_escala_y(int passo)
{
    if (passo < 0) passo = 0;
    if (passo >= FP_XFRAMES_N) return 1.0f;
    /* L677: `capsule.scale.y = 1 / xFrames[frameInTypeBeat]`. */
    return 1.0f / fp_xframes[passo];
}

float fp_x_do_salto(int passo)
{
    if (passo < 0) passo = 0;
    if (passo >= FP_XPOS_LERP_N) passo = FP_XPOS_LERP_N - 1;
    /* L688-690: `targetPos.x = FlxG.width * xPosLerpLol[passo]`, em 640x360. */
    return 640.0f * fp_xpos_lerp[passo];
}

float fp_y_item(int indice, int sel)
{
    int rel = indice - sel;
    float y = fp_y(rel);
    /* L2941-2943: os desvios das pontas (em 1280x720 -> metade). */
    if (rel < 0) y -= 25.0f;
    else if (rel > 4) y -= -5.0f;
    /* L2949: `if (index < curSelected) targetPos.y -= 100`. Compara o indice
     * 1-based com o `curSelected` 0-based, como o original -- por isso o item
     * logo acima do selecionado NAO leva o castigo. */
    if (indice < sel) y -= 50.0f;
    return y;
}

int fp_passo_salto(unsigned long desde_ms)
{
    return (int)((desde_ms * 24ul) / 1000ul);
}

void fp_texto_escala(unsigned long desde_ms, float *sx, float *sy)
{
    float x = 1.0f, y = 1.0f;
    /* 1/24 s = 41,67 ms e 3/24 s = 125 ms (L520-530). */
    if (desde_ms < 42ul) { x = 1.70f; y = 0.20f; }
    else if (desde_ms < 125ul) { x = 0.40f; y = 1.40f; }
    if (sx) *sx = x;
    if (sy) *sy = y;
}

const fp_titulo_t *fp_titulo(const char *id)
{
    int i;
    if (!id) return 0;
    for (i = 0; i < FP_TITULOS_N; i++)
        if (strcmp(fp_titulos[i].id, id) == 0) return &fp_titulos[i];
    return 0;
}

float fp_fundo_x(unsigned long desde_ms)
{
    /* `FreeplayState.hx` L529-531:
     *
     *     backingImage.x = -backingImage.width;
     *     FlxTween.tween(backingImage, {x: 0}, 0.7, {ease: FlxEase.quintOut});
     *
     * `FlxEase.quintOut(t) = 1 - (1 - t)^5`, e o tween e' LINEAR no tempo (sem
     * `startDelay`): em 700 ms o fundo esta' parado em x = 0. O valor devolvido
     * e' o x em 640x360 (negativo enquanto entra). */
    unsigned long dur = (unsigned long)FP_FUNDO_ENTRADA_MS;
    float t, e;
    if (!dur || desde_ms >= dur) return FP_FUNDO_X;
    t = (float)desde_ms / (float)dur;
    e = 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t) * (1.0f - t) * (1.0f - t);
    return FP_FUNDO_X - (float)FP_FUNDO_W * (1.0f - e);
}
