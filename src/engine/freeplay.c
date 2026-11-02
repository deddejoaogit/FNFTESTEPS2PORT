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

/* ---- O BRILHO DO SELO DOURADO -----------------------------------------
 *
 * Ver o bloco no `freeplay.h`: `SongMenuItem.hx` L156-166, L228-233 e
 * L493-504. O sorteio e' `FlxG.random.float(a, b)` = uniforme em [a, b), e o
 * port faz o mesmo com um LCG proprio, semente por musica.
 */

/* `FlxG.random.float(a, b)`: uniforme em [a, b). */
static float fp_spk_sorteia(unsigned int *s, float a, float b)
{
    *s = (*s) * 1103515245u + 12345u;
    /* Os 24 bits altos: o LCG do glibc perde os bits baixos primeiro. */
    {
        float u = (float)(((*s) >> 8) & 0xFFFFFFu) / 16777216.0f;
        return a + u * (b - a);
    }
}

/* Duracao de uma passada: 10 quadros a 24 fps = 0,41667 s. */
static float fp_spk_dura(void)
{
    return (float)FP_SPK_QUADROS / (float)FP_SPK_FPS;
}

static void fp_spk_parar(fp_spk_t *s, unsigned int semente)
{
    s->semente = semente ? semente : 1u;
    s->x = FP_SP_X;
    s->y = FP_SP_Y;
    s->t_anim = fp_spk_dura();   /* parado no ultimo quadro (o 1x1 vazio) */
    s->t_espera = 0.0f;
    s->armado = 0;
}

void fp_spk_desarma(fp_spk_t *s, unsigned int semente)
{
    fp_spk_parar(s, semente);
}

void fp_spk_arma(fp_spk_t *s, unsigned int semente, float base_x, float base_y)
{
    fp_spk_parar(s, semente);
    s->armado = 1;
    s->x = base_x;
    s->y = base_y;
    /* `new FlxTimer().start(1, sparkleEffect)` (L502): o primeiro disparo nao
       sorteia nada. */
    s->t_espera = (float)FP_SP_PRIMEIRO_MS / 1000.0f;
}

void fp_spk_passo(fp_spk_t *s, float dt, float base_x, float base_y)
{
    float dx, dy, espera;
    if (!s->armado) return;
    s->t_anim += dt;
    s->t_espera -= dt;
    if (s->t_espera > 0.0f) return;
    /* `sparkleEffect`: primeiro a posicao (x e depois y), depois o intervalo
       seguinte. A ordem importa para a sequencia do LCG ser a mesma toda vez. */
    dx = fp_spk_sorteia(&s->semente, FP_SP_DX_MIN, FP_SP_DX_MAX);
    dy = fp_spk_sorteia(&s->semente, FP_SP_DY_MIN, FP_SP_DY_MAX);
    espera = fp_spk_sorteia(&s->semente,
                            (float)FP_SP_INTERVALO_MIN_MS / 1000.0f,
                            (float)FP_SP_INTERVALO_MAX_MS / 1000.0f);
    s->x = base_x + dx;
    s->y = base_y + dy;
    s->t_anim = 0.0f;                 /* `play('sparkle', true)` */
    /* O FlxTimer leva a sobra negativa: `_timeCounter += time`. */
    s->t_espera += espera;
}

int fp_spk_quadro(const fp_spk_t *s)
{
    int k;
    if (!s->armado) return -1;
    k = (int)(s->t_anim * (float)FP_SPK_FPS);
    if (k < 0) k = 0;
    if (k >= FP_SPK_QUADROS) k = FP_SPK_QUADROS - 1;
    return k;
}

/* ---- O CORACAO DE FAVORITO --------------------------------------------
 *
 * Ver o bloco no `freeplay.h`. As tres coisas que o original faz e que aqui
 * viram conta: a animacao (13 quadros a 24 fps, uma vez, parando no ultimo), o
 * reverso do desfavoritar (`play('fav', true, true, 9)`) e o empurrao de 5 px
 * do item (0,1 s `expoOut`, 0,1 s parado, 0,1 s `expoIn`).
 */

#define FP_FV_DURA    ((float)FP_FV_QUADROS / (float)FP_FV_FPS)
#define FP_FV_ESCONDE 0.2f                 /* `new FlxTimer().start(0.2, ...)` */
#define FP_FV_EMPURRAO (5.0f * FP_MEIA)    /* favoritar: y - 5 na fonte */
#define FP_FV_TRECHO  0.1f                 /* cada tween de 0,1 s      */

void fp_fav_reset(fp_fav_t *f)
{
    if (!f) return;
    f->t = 0.0f;
    f->modo = FP_FV_PARADO;
    f->visivel = 0;
}

void fp_fav_mostra(fp_fav_t *f)
{
    if (!f) return;
    f->t = FP_FV_DURA;      /* parado no ULTIMO quadro */
    f->modo = FP_FV_PARADO;
    f->visivel = 1;
}

void fp_fav_toggle(fp_fav_t *f, int favoritou)
{
    if (!f) return;
    f->t = 0.0f;
    if (favoritou) {
        f->modo = FP_FV_ENTRANDO;
        f->visivel = 1;
    } else {
        f->modo = FP_FV_SAINDO;
        /* Continua visivel: o `FlxTimer` de 0,2 s e' que esconde. */
    }
}

void fp_fav_passo(fp_fav_t *f, float dt)
{
    if (!f || !f->visivel) return;
    f->t += dt;
    if (f->modo == FP_FV_ENTRANDO && f->t >= FP_FV_DURA) {
        f->modo = FP_FV_PARADO;      /* para no ultimo quadro, como o looped=false */
    } else if (f->modo == FP_FV_SAINDO && f->t >= FP_FV_ESCONDE) {
        f->visivel = 0;
        f->modo = FP_FV_PARADO;
    }
}

int fp_fav_quadro(const fp_fav_t *f)
{
    int k;
    if (!f || !f->visivel) return -1;
    if (f->modo == FP_FV_PARADO) return FP_FV_QUADROS - 1;
    k = (int)(f->t * (float)FP_FV_FPS);
    if (k < 0) k = 0;
    if (k >= FP_FV_QUADROS) k = FP_FV_QUADROS - 1;
    /* O reverso do desfavoritar comeca no quadro 9 e DESCE (`play(name, true,
       true, 9)`). */
    return (f->modo == FP_FV_SAINDO) ? (9 - k) : k;
}

float fp_fav_dy(const fp_fav_t *f)
{
    float u, e;
    if (!f || !f->visivel || f->modo == FP_FV_PARADO) return 0.0f;
    /* `FlxTween.tween(y, y -/+ 5, 0.1, expoOut)` e o de volta com
       `startDelay 0.1` e `expoIn`: 0..0,1 desce, 0,1..0,2 parado, 0,2..0,3
       volta. O sinal depende do modo (`favoriteSong` desce, o unfav sobe). */
    if (f->t < FP_FV_TRECHO) {
        u = f->t / FP_FV_TRECHO;
        e = 1.0f - powf(2.0f, -10.0f * u);              /* expoOut */
        return (f->modo == FP_FV_ENTRANDO ? -1.0f : 1.0f) * FP_FV_EMPURRAO * e;
    }
    if (f->t < 2.0f * FP_FV_TRECHO)
        return (f->modo == FP_FV_ENTRANDO ? -1.0f : 1.0f) * FP_FV_EMPURRAO;
    if (f->t < FP_FV_ESCONDE + FP_FV_TRECHO) {
        u = (f->t - 2.0f * FP_FV_TRECHO) / FP_FV_TRECHO;
        e = powf(2.0f, 10.0f * (u - 1.0f));             /* expoIn  */
        return (f->modo == FP_FV_ENTRANDO ? -1.0f : 1.0f) * FP_FV_EMPURRAO * (1.0f - e);
    }
    return 0.0f;
}

int fp_fav_visivel(const fp_fav_t *f)
{
    return (f && f->visivel) ? 1 : 0;
}
