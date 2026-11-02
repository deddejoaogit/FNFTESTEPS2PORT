/* freeplay_rank -- a linha do tempo da cinematica de rank da Freeplay.
 *
 * FONTE (`Funkin` v0.8.6, `/tmp/funk86/source`)
 *   funkin/ui/freeplay/FreeplayState.hx  L1069-1344  (a cinematica inteira)
 *   funkin/ui/freeplay/SongMenuItem.hx   L400-437    (o `fadeAnim`/`FlxTrail`)
 *   funkin/effects/IntervalShake.hx      L160-220    (o tremor por intervalo)
 *   flixel/tweens/FlxEase.hx                         (as formulas dos easings)
 *   flixel/tweens/FlxTween.hx                        (o update de cada tween)
 *   flixel/FlxCamera.hx                  L1310-1330  (o `fade` linear)
 *
 * O QUE ELE NAO FAZ: nada de GS, VRAM, arquivo ou audio. So' aritmetica em
 * ponto flutuante, para o mesmo resultado poder ser cobrado no PC.
 */
#include <math.h>
#include <string.h>

#include "freeplay_rank.h"
#include "freeplay.h"      /* fp_x, fp_y, FP_DESENHO_W/H */

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ---- os easings, transcritos de `flixel/tweens/FlxEase.hx` -------------- */
enum {
    EZ_LINEAR = 0,
    EZ_SINE_IN,
    EZ_EXPO_IN,
    EZ_EXPO_OUT,
    EZ_BACK_IN,
    EZ_BACK_OUT,
    EZ_BACK_IN_OUT,
    EZ_ELASTIC_OUT,
    EZ_QUART_IN,
    EZ_QUAD_OUT
};

static float ez_sine_in(float t)  { return -cosf((float)(M_PI / 2.0) * t) + 1.0f; }
static float ez_expo_in(float t)  { return powf(2.0f, 10.0f * (t - 1.0f)); }
static float ez_expo_out(float t) { return -powf(2.0f, -10.0f * t) + 1.0f; }
static float ez_quad_out(float t) { return -t * (t - 2.0f); }
static float ez_quart_in(float t) { return t * t * t * t; }

/* `backIn`:  t*t*(2.70158*t - 1.70158)                       (FlxEase L200)
 * `backOut`: 1 - (t-1)^2 * (-2.70158*(t-1) - 1.70158)        (FlxEase L204-208)
 *   Na fonte e' `1 - (--t) * (t) * (-2.70158 * t - 1.70158)` -- o `--t`
 *   acontece ANTES do uso, entao o fator e' (t-1) nos tres lugares.
 * `backInOut` (L212-220): t*=2; t<1 -> backIn(t)/2; senao t--, e
 *   (1 - (--t)*(t)*(-2.70158*t - 1.70158))/2 + 0.5.                    */
static float ez_back_in(float t)  { return t * t * (2.70158f * t - 1.70158f); }
static float ez_back_out(float t)
{
    float u = t - 1.0f;
    return 1.0f - u * u * (-2.70158f * u - 1.70158f);
}
static float ez_back_in_out(float t)
{
    float u;
    t *= 2.0f;
    if (t < 1.0f) return ez_back_in(t) / 2.0f;
    t -= 1.0f;
    u = t - 1.0f;
    return (1.0f - u * u * (-2.70158f * u - 1.70158f)) / 2.0f + 0.5f;
}

/* `elasticOut` (FlxEase L232-238), com ELASTIC_AMPLITUDE = 1 e
 * ELASTIC_PERIOD = 0,4 (L26-27):
 *   A*2^(-10t)*sin((t - (P/(2pi))*asin(1/A)) * (2pi)/P) + 1                    */
static float ez_elastic_out(float t)
{
    const float A = 1.0f, P = 0.4f;
    float fase = (P / (float)(2.0 * M_PI)) * asinf(1.0f / A);
    return A * powf(2.0f, -10.0f * t) *
           sinf((t - fase) * (float)(2.0 * M_PI) / P) + 1.0f;
}

static float fp_ease(int e, float t)
{
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    switch (e) {
    case EZ_SINE_IN:     return ez_sine_in(t);
    case EZ_EXPO_IN:     return ez_expo_in(t);
    case EZ_EXPO_OUT:    return ez_expo_out(t);
    case EZ_BACK_IN:     return ez_back_in(t);
    case EZ_BACK_OUT:    return ez_back_out(t);
    case EZ_BACK_IN_OUT: return ez_back_in_out(t);
    case EZ_ELASTIC_OUT: return ez_elastic_out(t);
    case EZ_QUART_IN:    return ez_quart_in(t);
    case EZ_QUAD_OUT:    return ez_quad_out(t);
    default:             return t;          /* `linear` (FlxTween sem ease) */
    }
}

/* ---- a tween: um segmento como o `FlxTween` o guarda -------------------- */
typedef struct {
    unsigned long t0;    /* quando comeca, em ms                  */
    unsigned long dur;   /* 0 = "set" (o sprite recebeu o valor)  */
    float de, para;      /* `start` e `finish`                    */
    int ease;
} tw_t;

/* Aplica os segmentos NA ORDEM DA TABELA. Uma tween criada depois escreve
 * depois em cada quadro, entao ela vence a anterior -- e' o que o
 * `FlxTween.globalManager` faz: percorre a lista na ordem de criacao. */
static float tw_aval(const tw_t *a, int n, unsigned long t, float padrao)
{
    float v = padrao;
    int i;
    for (i = 0; i < n; i++) {
        if (t < a[i].t0) continue;
        if (a[i].dur == 0 || t >= a[i].t0 + a[i].dur) {
            if (a[i].dur == 0) { v = a[i].para; continue; }
            v = a[i].para;
            continue;
        }
        v = a[i].de + (a[i].para - a[i].de) *
            fp_ease(a[i].ease, (float)(t - a[i].t0) / (float)a[i].dur);
    }
    return v;
}

/* ---- o gerador de numeros do `FlxG.random` ------------------------------
 * O original usa `FlxG.random.float(a, b)`, semeado em tempo de execucao. O
 * port precisa que o MESMO instante de para o MESMO desenho (o harness do PC
 * compara quadros), entao a semente e' fixa por capsula. xorshift32 de
 * Marsaglia: 2^32-1 de periodo, sem estado escondido. */
static unsigned int rk_sorteio(unsigned int *s)
{
    unsigned int x = *s;
    x ^= x << 13; x &= 0xFFFFFFFFu;
    x ^= x >> 17;
    x ^= x << 5;  x &= 0xFFFFFFFFu;
    *s = x;
    return x;
}

/* `FlxG.random.float(a, b)` em [-1, 1] -- o sinal entra na multiplicacao. */
static float rk_rand_pm1(unsigned int *s)
{
    return (float)(rk_sorteio(s) & 0xFFFFFFu) / 8388607.5f - 1.0f;
}

/* ---- o `IntervalShake` (`IntervalShake.hx` L160-220) --------------------
 * Cada tique (`60 ticks/s` nao: o intervalo dado) faz
 *     _secondsSinceStart += interval
 *     scale = _secondsSinceStart / duration
 *     scale = 1 - ease(scale)
 *     I = lerp(endIntensity, startIntensity, scale)     // end = 0 sempre aqui
 *     x = initialOffset.x + random(-I*w, I*w)
 *     y = initialOffset.y + random(-I*w, I*w)           // sim: `width` nos DOIS
 * No ultimo tique o objeto volta ao `initialOffset`.
 * Devolve 1 quando o tremor ja' comecou. */
static int rk_shake(unsigned long t, unsigned long ini, unsigned long dur_ms,
                    unsigned long intervalo_ms, float intensidade,
                    float largura, unsigned int semente,
                    float *dx, float *dy)
{
    unsigned int loops, k, s;
    float dur_s, ix, iy;
    if (t < ini) return 0;
    loops = (unsigned int)(dur_ms / intervalo_ms);   /* `Std.int(duration / interval)` */
    s = semente * 2654435761u + 1u;
    ix = iy = 0.0f;
    for (k = 0; k < loops; k++) {
        unsigned long tk = ini + (unsigned long)(k + 1) * intervalo_ms;
        float escala, I;
        if (t < tk) break;
        dur_s = (float)dur_ms / 1000.0f;
        escala = ((float)(k + 1) * (float)intervalo_ms / 1000.0f) / dur_s;
        escala = 1.0f - fp_ease(EZ_QUAD_OUT, escala);
        I = intensidade * escala;
        ix = rk_rand_pm1(&s) * I * largura;
        iy = rk_rand_pm1(&s) * I * largura;
    }
    /* O `if (timer.loopsLeft == 0)` do fim devolve o objeto ao lugar. */
    if (t >= ini + (unsigned long)loops * intervalo_ms) { ix = iy = 0.0f; }
    *dx = ix; *dy = iy;
    return 1;
}

/* ---- as posicoes de referencia ---------------------------------------- */

/* `capsuleToRank.setPosition((FlxG.width / 2) - (capsule.width / 2),
 *                             (FlxG.height / 2) - (capsule.height / 2))`
 * L1140-1142.
 *
 * `capsule.width` NAO e' a largura desenhada: em flixel, `width` so' muda com
 * `updateHitbox()`, e o `SongMenuItem` nunca chama `updateHitbox()` na capsula
 * -- ele mexe em `capsule.scale` direto (L669-670, L692-693, L722-723). Logo
 * `width` continua sendo o `frameWidth` do quadro (612) e `height` o 132. Em
 * 640x360 isso e' `FP_CAP_W` e `FP_CAP_H`, nao `FP_DESENHO_W/H`.
 *
 * Consequencia VISIVEL, e ela e' da fonte: como `origin` e `offset` do sprite
 * ficam em (0,0) (sem `updateHitbox`), o desenho sai escalado a partir do
 * canto, entao a capsula de 244,8 px fica com o canto esquerdo em 167 e o
 * centro em 289,4 -- 30,6 px a' esquerda do centro da tela. E' assim no
 * original; o port reproduz. */
float fp_rk_centro_x(void) { return (FP_RK_TELA_W - (float)FP_CAP_W) * 0.5f; }
float fp_rk_centro_y(void) { return (FP_RK_TELA_H - (float)FP_CAP_H) * 0.5f; }

/* `originalPos.x = (CUTOUT_WIDTH * SONGS_POS_MULTI) + 320.488; .y = 235.6`
 * (L1120-1121). `CUTOUT_WIDTH` e' 0 no PS2 (nao ha' recorte de tela de
 * aparelho) -- e 320.488 e' `intendedX(1)` = 270 + 60*sin(1); 235,6 e'
 * `intendedY(1)`. Em 640x360: `fp_x(1)`, `fp_y(1)`. */
float fp_rk_origem_x(void) { return fp_x(1); }
float fp_rk_origem_y(void) { return fp_y(1); }

/* ---- a avaliacao ------------------------------------------------------ */

void fp_rk_avaliar(unsigned long ms, int tem_rank_antigo, fp_rk_estado_t *e)
{
    static const tw_t zoom_rc[] = {
        {0,    0,    0.0f, 1.85f, EZ_LINEAR},      /* rankCamera.zoom = 1.85  L1130 */
        {0,    600,  1.85f, 1.80f, EZ_SINE_IN},    /* -> 1,8 em 0,6 s        L1131 */
        {600,  0,    0.0f, 1.30f, EZ_LINEAR},      /* rankCamera.zoom = 1.3  L1198 */
        {600,  300,  1.30f, 1.50f, EZ_BACK_IN_OUT},/* -> 1,5 em 0,3 s        L1200 */
        {900,  800,  1.50f, 1.20f, EZ_BACK_IN},    /* -> 1,2 em 0,8 s        L1216 */
        {1600, 0,    0.0f, 0.80f, EZ_LINEAR},      /* rankCamera.zoom = 0.8  L1261 */
        {1600, 1000, 0.80f, 1.00f, EZ_ELASTIC_OUT} /* -> 1 em 1 s            L1263 */
    };
    static const tw_t zoom_fc[] = {
        {0,    0,    0.0f, 1.15f, EZ_LINEAR},      /* funnyCam.zoom = 1.15   L1133 */
        {0,    600,  1.15f, 1.10f, EZ_SINE_IN},    /* -> 1,1 em 0,6 s        L1134 */
        {600,  300,  1.10f, 1.05f, EZ_ELASTIC_OUT},/* -> 1,05 em 0,3 s       L1205 */
        {900,  800,  1.05f, 1.00f, EZ_SINE_IN},    /* -> 1 em 0,8 s          L1215 */
        {1600, 0,    0.0f, 0.80f, EZ_LINEAR},      /* funnyCam.zoom = 0.8    L1262 */
        {1600, 800,  0.80f, 1.00f, EZ_ELASTIC_OUT} /* -> 1 em 0,8 s          L1264 */
    };
    /* `rankAnimSlam` L1230: `FlxTween.tween(rankBg, {alpha: 0}, 0.5, expoIn)`
     * -- comeca no SLAM (1100), e o `rankBg.alpha = 1` do `rankAnimStart`
     * (L1089) vale de 0 a 1100. */
    static const tw_t bg[] = {
        {1100, 500, 1.0f, 0.0f, EZ_EXPO_IN}
    };
    /* `rankCamera.fade(0xFF000000, 0.5, true, null, true)` (L1087): o flixel
     * guarda `_fxFadeAlpha = 0,999999` e desconta `elapsed/duration` por
     * quadro (`FlxCamera.hx` L1314) -- linear, sem easing. */
    static const tw_t fade[] = {
        {0, 500, 0.999999f, 0.0f, EZ_LINEAR}
    };
    /* A capsula. Os quatro instantes, na ordem:
     *   L1140  setPosition(centro) = (197,6 , 153,6)
     *   L1203  `x -= 10; y -= 20` (no +0,1 s)
     *   L1217  tween para (originalPos - 7, originalPos.y - 80) em 1,3 s quartIn
     *   L1282  `cancelTweensOf` + `setPosition(originalPos)` no pouso
     * O CENTRO da tela em 640x360 e' (197,6 , 153,6) e `originalPos` e'
     * (160,244 , 117,8): os dois saem de `fp_rk_centro_*`/`fp_rk_origem_*`. */
    static tw_t cx[4], cy[4];
    static int montado = 0;
    static const tw_t ang[] = {
        {600,  0,   0.0f, -3.0f, EZ_LINEAR},     /* `capsuleToRank.angle = -3` L1207 */
        {600,  500, -3.0f, 0.0f, EZ_BACK_OUT},   /* -> 0 em 0,5 s              L1208 */
        {1600, 500, 0.0f, 0.0f, EZ_BACK_OUT}     /* -> 0 em 0,5 s              L1286 */
    };
    static const tw_t selo[] = {
        {500,  0,   0.0f, 20.0f, EZ_LINEAR},     /* ranking.scale.set(20,20)   L1152 */
        {500,  100, 20.0f, 0.9f, EZ_LINEAR}      /* -> 0,9 em 0,1 s            L1160 */
    };
    static const tw_t vinh[] = {
        {1600, 600, 1.0f, 0.0f, EZ_EXPO_OUT}     /* rankVignette 1 -> 0        L1280-1281 */
    };
    float dx1, dy1, dx2, dy2;

    memset(e, 0, sizeof(*e));
    if (!montado) {
        float cx0 = fp_rk_centro_x(), cy0 = fp_rk_centro_y();
        float ox = fp_rk_origem_x(),  oy = fp_rk_origem_y();
        montado = 1;
        cx[0].t0 = 0;    cx[0].dur = 0;    cx[0].de = 0.0f;      cx[0].para = cx0;      cx[0].ease = EZ_LINEAR;
        cx[1].t0 = 600;  cx[1].dur = 0;    cx[1].de = 0.0f;      cx[1].para = cx0 - 10.f; cx[1].ease = EZ_LINEAR;
        cx[2].t0 = 900;  cx[2].dur = 1300; cx[2].de = cx0 - 10.f; cx[2].para = ox - 7.f; cx[2].ease = EZ_QUART_IN;
        cx[3].t0 = 1600; cx[3].dur = 0;    cx[3].de = 0.0f;      cx[3].para = ox;       cx[3].ease = EZ_LINEAR;
        cy[0].t0 = 0;    cy[0].dur = 0;    cy[0].de = 0.0f;      cy[0].para = cy0;      cy[0].ease = EZ_LINEAR;
        cy[1].t0 = 600;  cy[1].dur = 0;    cy[1].de = 0.0f;      cy[1].para = cy0 - 20.f; cy[1].ease = EZ_LINEAR;
        cy[2].t0 = 900;  cy[2].dur = 1300; cy[2].de = cy0 - 20.f; cy[2].para = oy - 80.f; cy[2].ease = EZ_QUART_IN;
        cy[3].t0 = 1600; cy[3].dur = 0;    cy[3].de = 0.0f;      cy[3].para = oy;       cy[3].ease = EZ_LINEAR;
    }

    e->rc_zoom = tw_aval(zoom_rc, 7, ms, 1.0f);
    e->fc_zoom = tw_aval(zoom_fc, 6, ms, 1.0f);
    e->fade_alpha = tw_aval(fade, 1, ms, 0.0f);
    e->bg_alpha = tw_aval(bg, 1, ms, 1.0f);
    e->cap_x = tw_aval(cx, 4, ms, fp_rk_centro_x());
    e->cap_y = tw_aval(cy, 4, ms, fp_rk_centro_y());
    e->selo_scale = tw_aval(selo, 2, ms, 0.9f);
    e->selo_visivel = (ms >= FP_RK_T_DISPLAY);
    e->fake_visivel = (tem_rank_antigo && ms < FP_RK_T_FAKE_OFF);
    e->vinheta_alpha = tw_aval(vinh, 1, ms, 0.0f);
    e->cap_ang = tw_aval(ang, 3, ms, 0.0f);

    /* Os DOIS tremores da capsula alvo, em sequencia:
     *   L1212 `IntervalShake.shake(capsuleToRank, 0.3, 1/30, 0.1, 0, quadOut)`
     *   L1284 `IntervalShake.shake(capsule,        0.6, 1/24, 0.12, 0, quadOut)`
     * O `object.width` deles e' a largura do GRUPO: `FlxSpriteGroup.get_width`
     * (`FlxSpriteGroup.hx` L839-846) devolve `maxX - minX` dos filhos, e o
     * maior filho e' a capsula, com 612 de `frameWidth` (o `scale` nao entra em
     * `width`). Em 640x360: `FP_CAP_W` = 306. */
    dx1 = dy1 = dx2 = dy2 = 0.0f;
    rk_shake(ms, 600, 300, 33, 0.1f, (float)FP_CAP_W, 1u, &dx1, &dy1);
    rk_shake(ms, 1600, 600, 41, 0.12f, (float)FP_CAP_W, 2u, &dx2, &dy2);
    e->cap_dx = dx1 + dx2;
    e->cap_dy = dy1 + dy2;

    /* As faiscas: `animation.play('sparks', true)` (L1183) toca UMA vez com
     * 5 quadros a 24 fps = 208,3 ms e o `onFinish` as esconde (L1185-1188). */
    if (ms >= FP_RK_T_FAKE_OFF) {
        unsigned long k = (ms - FP_RK_T_FAKE_OFF) * 24ul / 1000ul;
        if (k < (unsigned long)FP_RK_SPARKS_QUADROS) {
            e->sparks_q = (int)k;
            e->sparksadd_q = (int)k;
        } else {
            e->sparks_q = -1;
            e->sparksadd_q = -1;
        }
    } else {
        e->sparks_q = -1;
        e->sparksadd_q = -1;
    }

    e->fase = ms < FP_RK_T_DISPLAY ? 0 :
              ms < FP_RK_T_QUEDA   ? 1 :
              ms < FP_RK_T_SLAM    ? 2 :
              ms < FP_RK_T_POUSO   ? 3 :
              ms < FP_RK_FIM       ? 4 : 5;
}

void fp_rk_vizinho(unsigned long ms, int dist, unsigned int semente, fp_rk_viz_t *v)
{
    unsigned long ini;
    unsigned int s;
    float faixa;
    v->ang = 0.0f; v->dx = 0.0f; v->dy = 0.0f; v->ativo = 0;
    if (dist < 0 || dist >= 5) return;
    /* `new FlxTimer().start(distFromSelected / 20, ...)` (L1296, L1322). */
    ini = FP_RK_T_POUSO + (unsigned long)dist * 50ul;
    if (ms < ini) return;
    v->ativo = 1;
    s = semente * 2246822519u + 7u;
    /* `capsule.angle = FlxG.random.float(-10 + (distFromSelected * 2),
     *                                   10 - (distFromSelected * 2))` (L1300) */
    faixa = 10.0f - 2.0f * (float)dist;
    v->ang = rk_rand_pm1(&s) * faixa;
    /* `FlxTween.tween(capsule, {angle: 0}, 0.5, {ease: FlxEase.backOut})`. */
    if (ms < ini + 500ul)
        v->ang = v->ang * (1.0f - fp_ease(EZ_BACK_OUT, (float)(ms - ini) / 500.0f));
    else
        v->ang = 0.0f;
    /* `IntervalShake.shake(capsule, 0.6, 1/24, 0.12 / (dist + 1), 0, quadOut)`. */
    rk_shake(ms, ini, 600, 41, 0.12f / (float)(dist + 1), (float)FP_CAP_W,
             semente + 13u, &v->dx, &v->dy);
}

float fp_rk_rastro(unsigned long ms, int k, float *escala)
{
    float esc_t;
    /* `FlxTween.tween(impactThing.scale, {x: 2.5, y: 2.5}, 0.5)` (L408),
     * linear, sem easing. A copia de indice `k` tem a escala de `k` quadros
     * antes; o passo de atualizacao e' zero (o `delay` do `FlxTrail` e'
     * 0,03 s e o tipo do campo e' `Int`, entao vale 0 e todo quadro entra). */
    esc_t = 1.0f + 1.5f * ((float)ms / 500.0f);
    if (esc_t > 2.5f) esc_t = 2.5f;
    if (escala) *escala = esc_t;
    if (k != 0) return 0.0f;
    if (ms >= 600ul) return 0.0f;
    /* alpha 0,01 da amostra 0, vezes o alfa do grupo (1 -> 0 em 0,6 s
     * quadOut, L410-418). */
    return 0.01f * (1.0f - fp_ease(EZ_QUAD_OUT, (float)ms / 600.0f));
}
