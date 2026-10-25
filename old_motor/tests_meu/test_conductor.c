/* Teste do conductor e das 13 curvas. Roda no PC.
   Verifica valores contra as formulas do flixel, nao contra a minha
   memoria: cada assercao tem a formula ao lado. */
#include "../src/engine/conductor.h"
#include <stdio.h>
#include <math.h>
#include <string.h>

static int falhas = 0, checks = 0;

static void ok(int cond, const char *fmt, ...)
{
    (void)fmt;
    checks++;
    if (!cond) { falhas++; printf("FALHOU: %s\n", fmt); }
}
#define OK(c) ok((c), #c)

static int quase(float a, float b, float eps) { return fabsf(a - b) <= eps; }

int main(void)
{
    conductor_t c;
    int i, ok_e;

    printf("== curvas ==\n");
    /* extremos: toda curva tem de dar 0 em 0 e 1 em 1 (INSTANT tambem,
       porque e a identidade). */
    for (i = 0; i < EASE_COUNT; i++) {
        float a = fnf_ease(i, 0.0f), b = fnf_ease(i, 1.0f);
        if (!(quase(a, 0.0f, 1e-6f) && quase(b, 1.0f, 1e-6f)))
            printf("  curva %2d %-14s f(0)=%.6f f(1)=%.6f\n", i, fnf_ease_name(i), a, b);
        OK(quase(a, 0.0f, 1e-6f));
        OK(quase(b, 1.0f, 1e-6f));
    }
    /* valores conferidos a mao contra a formula do flixel */
    OK(quase(fnf_ease(EASE_LINEAR, 0.25f), 0.25f, 1e-6f));
    OK(quase(fnf_ease(EASE_CLASSIC, 0.5f), 0.5f, 1e-6f));          /* t*t*(3-2t) em .5 = .5 */
    OK(quase(fnf_ease(EASE_CLASSIC, 0.25f), 0.15625f, 1e-6f));     /* .0625*2.5 */
    OK(quase(fnf_ease(EASE_QUAD, 0.5f), 0.75f, 1e-6f));            /* 2t-t^2 = .75 */
    OK(quase(fnf_ease(EASE_QUAD_INOUT, 0.25f), 0.125f, 1e-6f));    /* 2*.0625 */
    OK(quase(fnf_ease(EASE_QUART, 0.5f), 0.9375f, 1e-6f));         /* 1-(.5)^4 */
    OK(quase(fnf_ease(EASE_CIRC, 0.5f), 0.8660254f, 1e-6f));       /* sqrt(1-.25) */
    OK(quase(fnf_ease(EASE_EXPO_IN, 0.5f), 0.03125f, 1e-6f));      /* 2^(10*(-.5)) */
    OK(quase(fnf_ease(EASE_EXPO_OUT, 0.5f), 0.96875f, 1e-6f));     /* 1-2^-5 */
    OK(quase(fnf_ease(EASE_ELASTIC, 0.5f),
             2.0f * 0.0f + powf(2.0f, -5.0f) * sinf((0.5f - 0.075f) * 2.0f * (float)M_PI / 0.3f) + 1.0f, 1e-6f) ||
       quase(fnf_ease(EASE_ELASTIC, 0.5f), powf(2.0f, -5.0f) * sinf((0.425f) * 20.943951f) + 1.0f, 1e-5f));
    OK(quase(fnf_ease(EASE_QUINT_INOUT, 0.5f), 0.5f, 1e-6f));
    /* instante pula direto */
    OK(quase(fnf_ease_apply(EASE_INSTANT, 3.0f, 7.0f, 0.1f), 7.0f, 1e-6f));
    OK(quase(fnf_ease_apply(EASE_LINEAR, 3.0f, 7.0f, 0.5f), 5.0f, 1e-6f));
    /* grampeamento e NaN */
    OK(quase(fnf_ease(EASE_LINEAR, 2.0f), 1.0f, 1e-6f));
    OK(quase(fnf_ease(EASE_LINEAR, -2.0f), 0.0f, 1e-6f));
    OK(quase(fnf_ease(EASE_LINEAR, 0.0f / 0.0f), 0.0f, 1e-6f));

    printf("  nomes dos eventos -> indice\n");
    OK(fnf_ease_from_name("linear") == EASE_LINEAR);
    OK(fnf_ease_from_name("CLASSIC") == EASE_CLASSIC);
    OK(fnf_ease_from_name("elasticInOut") == EASE_ELASTIC_INOUT);
    OK(fnf_ease_from_name("expoIn") == EASE_EXPO_IN);
    OK(fnf_ease_from_name("quintInOut") == EASE_QUINT_INOUT);
    OK(fnf_ease_from_name("circOut") == EASE_CIRC);
    OK(fnf_ease_from_name("smoothStep") == EASE_CLASSIC);
    OK(fnf_ease_from_name("inexistente") == -1);

    printf("== conductor ==\n");
    /* 100 BPM, 4/4: beatLengthMs = 600, stepLengthMs = 150
       (o tutorial e 100 BPM e o port ja usava 7200 amostras por passo) */
    cond_init(&c, 100.0, 4, 4);
    OK(quase((float)c.beat_length_ms, 600.0f, 1e-6f));
    OK(quase((float)c.step_length_ms, 150.0f, 1e-6f));
    OK(quase((float)c.step_length_ms * 48.0f, 7200.0f, 1e-3f)); /* amostras por passo */

    /* BPM 150: beat 400 ms, passo 100 ms */
    cond_init(&c, 150.0, 4, 4);
    OK(quase((float)c.beat_length_ms, 400.0f, 1e-6f));
    OK(quase((float)c.step_length_ms, 100.0f, 1e-6f));

    /* posicao: em 1500 ms a 100 BPM sao 10 passos */
    cond_init(&c, 100.0, 4, 4);
    cond_update_ms(&c, 1500.0);
    OK(c.current_step == 10);
    OK(c.current_beat == 2);
    /* 7200 amostras = 150 ms = 1 passo */
    cond_update_samples(&c, 7200u);
    OK(c.current_step == 1);
    cond_update_samples(&c, 14399u);
    OK(c.current_step == 1);
    cond_update_samples(&c, 14400u);
    OK(c.current_step == 2);

    printf("== troca de BPM ==\n");
    cond_init(&c, 100.0, 4, 4);
    cond_add_section(&c, 3000.0, 200.0); /* dobra a velocidade aos 3 s */
    cond_update_ms(&c, 3000.0);
    OK(c.sec_index == 1);
    OK(quase((float)c.step_length_ms, 75.0f, 1e-6f));
    /* 3 s a 100 BPM = 20 passos; depois disso cada 75 ms e um passo */
    OK(c.current_step == 20);
    cond_update_ms(&c, 3075.0);
    OK(c.current_step == 21);
    cond_update_ms(&c, 3150.0);
    OK(c.current_step == 22);
    /* volta para tras (o jogo permite restart) */
    cond_update_ms(&c, 1500.0);
    OK(c.sec_index == 0);
    OK(c.current_step == 10);

    /* secoes fora de ordem no arquivo: o vetor tem de ficar ordenado */
    cond_init(&c, 100.0, 4, 4);
    cond_add_section(&c, 6000.0, 120.0);
    cond_add_section(&c, 3000.0, 150.0);
    cond_update_ms(&c, 6500.0);
    OK(quase((float)c.bpm, 120.0f, 1e-6f));
    cond_update_ms(&c, 3500.0);
    OK(quase((float)c.bpm, 150.0f, 1e-6f));

    printf("== bop de camera ==\n");
    /* PlayState.hx:1846: bop recarrega quando (step+offset*4) % (rate*4)==0.
       Com rate 4, a cada 16 passos. Intensidade 1.015. */
    cond_init(&c, 100.0, 4, 4);
    cond_update_ms(&c, 0.0);
    OK(quase(c.bop_mult, 1.015f, 1e-6f));      /* passo 0 dispara */
    /* decaimento: 0.95^dt com dt = elapsed*60. 16,68 ms -> dt 1,0010 */
    {
        float antes = c.bop_mult;
        cond_frame(&c, 0.01668333f);
        OK(c.bop_mult < antes);
        OK(quase(c.bop_mult, 1.0f + 0.015f * powf(0.95f, 1.001f), 2e-5f));
    }
    /* 16 passos = 2400 ms, recarrega */
    {
        float sujo;
        for (i = 0; i < 60; i++) cond_frame(&c, 0.01668333f);
        sujo = c.bop_mult;
        OK(sujo < 1.015f);
        cond_update_ms(&c, 2400.0);
        OK(quase(c.bop_mult, 1.015f, 1e-6f)); /* recarregou no passo 16 */
    }
    /* passo 8 nao recarrega */
    cond_init(&c, 100.0, 4, 4);
    cond_update_ms(&c, 1200.0); /* passo 8 */
    OK(quase(c.bop_mult, 1.0f, 1e-6f));

    printf("== seguir de camera ==\n");
    /* FlxCamera.updateFollow com dt de um quadro NTSC.
       O port do tutorial ja usava 0.040042306 para lerp 0.04. */
    OK(quase(cond_follow_lerp(0.04f, 0.01668333f), 0.040042306f, 1e-5f));
    OK(quase(cond_follow_lerp(0.04f, 0.0f), 0.0f, 1e-6f));

    ok_e = (falhas == 0);
    printf("\n%d verificacoes, %d falhas\n", checks, falhas);
    return ok_e ? 0 : 1;
}
