#include "conductor.h"
#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ------------------------------------------------------------------ */
/* Curvas: formulas do flixel, uma a uma                              */
/* ------------------------------------------------------------------ */

static float clamp01(float t)
{
    if (!(t > 0.0f)) return 0.0f; /* pega NaN e negativos */
    if (t > 1.0f) return 1.0f;
    return t;
}

float fnf_ease(int indice, float t)
{
    float u, n;
    switch (indice) {
    case EASE_CLASSIC: /* FlxEase.smoothStep */
        u = clamp01(t);
        return u * u * (3.0f - 2.0f * u);
    case EASE_INSTANT: /* FlxEase.instant: identidade */
        return clamp01(t);
    case EASE_CIRC: {  /* FlxEase.circOut: sqrt(1-(t-1)^2) */
        u = clamp01(t);
        n = u - 1.0f;
        return sqrtf(1.0f - n * n);
    }
    case EASE_ELASTIC: { /* FlxEase.elasticOut:
                            if t<=0 ->0; if t>=1 ->1;
                            2^(-10t) * sin((t-0.075)*2pi/0.3) + 1 */
        u = clamp01(t);
        if (u <= 0.0f) return 0.0f;
        if (u >= 1.0f) return 1.0f;
        return powf(2.0f, -10.0f * u) * sinf((u - 0.075f) * (2.0f * (float)M_PI) / 0.3f) + 1.0f;
    }
    case EASE_ELASTIC_INOUT: { /* FlxEase.elasticInOut, 2pi/0.45 e 0.1125 */
        const float TAU_045 = 13.962634016f;
        u = clamp01(t);
        if (u <= 0.0f) return 0.0f;
        if (u >= 1.0f) return 1.0f;
        if (u < 0.5f) {
            float v = u * 2.0f - 1.0f;
            return -0.5f * (powf(2.0f, 10.0f * v) * sinf((v - 0.1125f) * TAU_045));
        }
        {
            float v = u * 2.0f - 1.0f;
            return powf(2.0f, -10.0f * v) * sinf((v - 0.1125f) * TAU_045) * 0.5f + 1.0f;
        }
    }
    case EASE_EXPO:     /* cai no expoOut */
    case EASE_EXPO_OUT: /* FlxEase.expoOut: 1-2^(-10t) */
        u = clamp01(t);
        if (u <= 0.0f) return 0.0f;
        if (u >= 1.0f) return 1.0f;
        return 1.0f - powf(2.0f, -10.0f * u);
    case EASE_EXPO_IN:  /* FlxEase.expoIn: 2^(10(t-1)) */
        u = clamp01(t);
        if (u <= 0.0f) return 0.0f;
        if (u >= 1.0f) return 1.0f;
        return powf(2.0f, 10.0f * (u - 1.0f));
    case EASE_LINEAR:
        return clamp01(t);
    case EASE_QUAD:     /* FlxEase.quadOut: -(t)*(t-2) */
        u = clamp01(t);
        return -(u) * (u - 2.0f);
    case EASE_QUAD_INOUT: /* FlxEase.quadInOut */
        u = clamp01(t);
        if (u < 0.5f) return 2.0f * u * u;
        return (-2.0f * u * u) + (4.0f * u) - 1.0f;
    case EASE_QUART: {  /* FlxEase.quartOut: -((t-1)^4)+1 */
        u = clamp01(t);
        n = u - 1.0f;
        return -(n * n * n * n) + 1.0f;
    }
    case EASE_QUINT_INOUT: { /* FlxEase.quintInOut */
        u = clamp01(t) * 2.0f;
        if (u < 1.0f) return 0.5f * u * u * u * u * u;
        u -= 2.0f;
        return 0.5f * (u * u * u * u * u + 2.0f);
    }
    default:
        return clamp01(t);
    }
}

float fnf_ease_apply(int indice, float de, float para, float t)
{
    if (indice == EASE_INSTANT) return para; /* o jogo pula direto ao alvo */
    return de + (para - de) * fnf_ease(indice, t);
}

/* Tabela de nomes. Os indices 0..12 sao os do port; os nomes do FlxEase
   aparecem porque os charts originais gravam o nome, nao o numero. */
static const char *const EASE_NOMES[EASE_COUNT] = {
    "CLASSIC", "INSTANT", "circ", "elastic", "elasticInOut", "expo",
    "expoIn", "expoOut", "linear", "quad", "quadInOut", "quart", "quintInOut"
};

static const struct { const char *nome; int idx; } EASE_ALIAS[] = {
    { "classic",      EASE_CLASSIC },
    { "smoothstep",   EASE_CLASSIC },
    { "instant",      EASE_INSTANT },
    { "circ",         EASE_CIRC },
    { "circout",      EASE_CIRC },
    { "circin",       EASE_CIRC },
    { "elastic",      EASE_ELASTIC },
    { "elasticout",   EASE_ELASTIC },
    { "elasticinout", EASE_ELASTIC_INOUT },
    { "expo",         EASE_EXPO },
    { "expoout",      EASE_EXPO_OUT },
    { "expoin",       EASE_EXPO_IN },
    { "linear",       EASE_LINEAR },
    { "quad",         EASE_QUAD },
    { "quadout",      EASE_QUAD },
    { "quadinout",    EASE_QUAD_INOUT },
    { "quart",        EASE_QUART },
    { "quartout",     EASE_QUART },
    { "quintinout",   EASE_QUINT_INOUT }
};

static void minusculo(char *d, const char *s, int max)
{
    int i = 0;
    for (; s[i] && i < max - 1; i++) {
        char c = s[i];
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
        d[i] = c;
    }
    d[i] = 0;
}

int fnf_ease_from_name(const char *nome)
{
    char b[32];
    int i;
    if (!nome) return EASE_LINEAR;
    minusculo(b, nome, (int)sizeof(b));
    for (i = 0; i < (int)(sizeof(EASE_ALIAS) / sizeof(EASE_ALIAS[0])); i++)
        if (!strcmp(b, EASE_ALIAS[i].nome)) return EASE_ALIAS[i].idx;
    return -1;
}

const char *fnf_ease_name(int indice)
{
    if (indice < 0 || indice >= EASE_COUNT) return "linear";
    return EASE_NOMES[indice];
}

/* ------------------------------------------------------------------ */
/* Conductor                                                          */
/* ------------------------------------------------------------------ */

static void recalcular_tempos(conductor_t *c)
{
    /* Conductor.hx:178-181 */
    c->beat_length_ms = (60000.0 / c->bpm) * (4.0 / (double)c->time_sig_den);
    /* Conductor.hx:189-191 */
    c->step_length_ms = c->beat_length_ms / (double)COND_STEPS_PER_BEAT;
}

void cond_init(conductor_t *c, double bpm, int time_sig_num, int time_sig_den)
{
    memset(c, 0, sizeof(*c));
    c->bpm = (bpm > 0.0) ? bpm : 100.0;
    c->time_sig_num = (time_sig_num > 0) ? time_sig_num : 4;
    c->time_sig_den = (time_sig_den > 0) ? time_sig_den : 4;
    c->n_sec = 1;
    c->sec[0].time_ms = 0.0;
    c->sec[0].bpm = c->bpm;
    c->sec[0].beat_time = 0.0;
    c->sec_index = 0;
    c->bop_mult = 1.0f;
    c->bop_intensity = 1.015f; /* Constants.DEFAULT_BOP_INTENSITY */
    c->bop_rate = 4;           /* Constants.DEFAULT_ZOOM_RATE      */
    c->bop_offset = 0;         /* Constants.DEFAULT_ZOOM_OFFSET    */
    c->last_bop_step = -1;
    c->current_step = -1;
    c->current_beat = -1;
    recalcular_tempos(c);
}

int cond_add_section(conductor_t *c, double time_ms, double bpm)
{
    int i, pos;
    double acumulado;
    if (c->n_sec >= COND_MAX_SECTIONS) return -1;
    if (!(bpm > 0.0)) return -1;
    /* insercao ordenada por tempo */
    pos = c->n_sec;
    for (i = 0; i < c->n_sec; i++) {
        if (time_ms < c->sec[i].time_ms) { pos = i; break; }
        if (time_ms == c->sec[i].time_ms) { c->sec[i].bpm = bpm; return 0; }
    }
    for (i = c->n_sec; i > pos; i--) c->sec[i] = c->sec[i - 1];
    c->sec[pos].time_ms = time_ms;
    c->sec[pos].bpm = bpm;
    c->sec[pos].beat_time = 0.0;
    c->n_sec++;
    /* Recalcula beatTime acumulado de cada secao, do inicio:
       beatTime(n) = beatTime(n-1) + (t(n)-t(n-1)) / beatLengthMs(n-1) */
    acumulado = 0.0;
    for (i = 0; i < c->n_sec; i++) {
        double bpm_ant;
        if (i > 0) {
            bpm_ant = c->sec[i - 1].bpm;
            {
                double beat_len_ant = (60000.0 / bpm_ant) * (4.0 / (double)c->time_sig_den);
                acumulado += (c->sec[i].time_ms - c->sec[i - 1].time_ms) / beat_len_ant;
            }
        }
        c->sec[i].beat_time = acumulado;
    }
    return 0;
}

static void aplicar_secao(conductor_t *c)
{
    int i, idx = 0;
    for (i = 0; i < c->n_sec; i++) {
        if (c->song_ms >= c->sec[i].time_ms) idx = i;
        else break;
    }
    c->sec_index = idx;
    c->bpm = c->sec[idx].bpm;
    c->section_beat_time = c->sec[idx].beat_time;
    recalcular_tempos(c);
}

void cond_update_ms(conductor_t *c, double song_ms)
{
    c->song_ms = song_ms;
    aplicar_secao(c);
    /* Conductor.hx:470-472 */
    c->current_step_time = c->section_beat_time * (double)COND_STEPS_PER_BEAT
                         + (c->song_ms - c->sec[c->sec_index].time_ms) / c->step_length_ms;
    c->current_beat_time = c->current_step_time / (double)COND_STEPS_PER_BEAT;
    c->current_step = (int)floor(c->current_step_time + 1e-9);
    c->current_beat = (int)floor(c->current_beat_time + 1e-9);

    /* PlayState.hx:1846-1859: a cada rate*4 passos o bop recarrega. */
    if (c->current_step != c->last_bop_step) {
        int periodo = c->bop_rate * COND_STEPS_PER_BEAT;
        if (periodo > 0) {
            int desloc = c->current_step + c->bop_offset * COND_STEPS_PER_BEAT;
            int mod = desloc % periodo;
            if (mod < 0) mod += periodo;
            if (mod == 0) c->bop_mult = c->bop_intensity;
        }
        c->last_bop_step = c->current_step;
    }
}

void cond_update_samples(conductor_t *c, unsigned int heard_samples)
{
    cond_update_ms(c, (double)heard_samples * 1000.0 / (double)COND_SAMPLE_RATE);
}

void cond_frame(conductor_t *c, float dt_s)
{
    /* PlayState.hx:1217-1227 / FlxMath.lerp(1, bop, 0.95^dt) */
    double dt = (double)dt_s * 60.0;
    float decay;
    if (dt < 0.0) dt = 0.0;
    decay = (float)pow(0.95, dt); /* CHART_BOP_DECAY 0.95 */
    c->bop_mult = 1.0f + (c->bop_mult - 1.0f) * decay;
    c->frames++;
}

float cond_zoom(const conductor_t *c, float current_camera_zoom)
{
    return current_camera_zoom * c->bop_mult;
}

float cond_follow_lerp(float base_lerp, float dt_s)
{
    /* FlxCamera.updateFollow: adjustedLerp = 1-(1-lerp)^(elapsed*60) */
    if (dt_s <= 0.0f) return 0.0f;
    return 1.0f - powf(1.0f - base_lerp, dt_s * 60.0f);
}
