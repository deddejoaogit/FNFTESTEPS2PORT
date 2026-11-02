/* A CINEMATICA DE RANK da Freeplay: a linha do tempo contra a fonte.
 *
 * O QUE PROVA (`FreeplayState.hx` v0.8.6 L1069-1344, com as linhas nos
 * comentarios de src/engine/freeplay_rank.c)
 *  - os quatro instantes dos timers (0,5 s -> display; +0,1 -> as faiscas;
 *    +0,4 -> a queda; +0,6 -> o slam; slam+0,5 -> o pouso) e o fim em 2,2 s;
 *  - os ZOOMS das duas cameras em cada instante, com o easing de cada tween
 *    (sineIn, backInOut, backIn, elasticOut);
 *  - o `rankBg` (1 -> 0 com expoIn a partir de 1,1 s) e o `fade` linear de
 *    0,5 s (o preto que sai no comeco);
 *  - a capsula no CENTRO da tela, o `x -= 10 / y -= 20`, a queda de 1,3 s com
 *    quartIn e o pouso em `originalPos` = `fp_x(1)`,`fp_y(1)`;
 *  - o selo novo crescendo de 20 para 0,9 em 0,1 s e o antigo sumindo em 0,6 s;
 *  - as faiscas: 5 quadros em 208 ms, uma vez, e nada depois;
 *  - a vinheta 1 -> 0 em 0,6 s expoOut;
 *  - o tremor do `IntervalShake`: 9 tiques de 1/30 s e 14 de 1/24 s, dentro da
 *    faixa `intensidade * largura` e de volta a zero no ultimo tique;
 *  - os vizinhos: atraso de `dist/20` s, faixa do angulo `10 - 2*dist`.
 *
 * O que NAO prova: que a tela parece a do original na TV. Isso e' a tela.
 */
#include <stdio.h>
#include <math.h>

#include "freeplay_rank.h"

static int falhas;
#define CHECK(c) do { if (!(c)) { printf("  FALHA: %s (linha %d)\n", #c, __LINE__); falhas++; } } while (0)
#define CHECK_EQ(a, b) do { long _a = (long)(a), _b = (long)(b); \
    if (_a != _b) { printf("  FALHA: %s == %s: %ld != %ld (linha %d)\n", #a, #b, _a, _b, __LINE__); falhas++; } } while (0)
#define CHECK_PX(a, b) do { double _a = (double)(a), _b = (double)(b); \
    if (fabs(_a - _b) > 0.02) { printf("  FALHA: %s == %s: %.4f != %.4f (linha %d)\n", #a, #b, _a, _b, __LINE__); falhas++; } } while (0)

/* Os easings da fonte, escritos AQUI de novo, de proposito: se o modulo e o
 * teste usassem a mesma funcao, o teste nao provaria nada. */
static double sine_in(double t)  { return -cos(M_PI / 2.0 * t) + 1.0; }
static double expo_out(double t) { return -pow(2.0, -10.0 * t) + 1.0; }
static double expo_in(double t)  { return pow(2.0, 10.0 * (t - 1.0)); }
static double back_in(double t)  { return t * t * (2.70158 * t - 1.70158); }
static double back_out(double t) { double u = t - 1.0; return 1.0 - u * u * (-2.70158 * u - 1.70158); }
static double quad_out(double t) { return -t * (t - 2.0); }
static double quart_in(double t) { return t * t * t * t; }
static double elastic_out(double t)
{
    const double A = 1.0, P = 0.4;
    double fase = (P / (2.0 * M_PI)) * asin(1.0 / A);
    return A * pow(2.0, -10.0 * t) * sin((t - fase) * (2.0 * M_PI) / P) + 1.0;
}

int main(void)
{
    fp_rk_estado_t e;

    printf("== cinematica de rank: os instantes (FreeplayState.hx L1069-1344)\n");
    CHECK_EQ(FP_RK_T_DISPLAY, 500);
    CHECK_EQ(FP_RK_T_FAKE_OFF, 600);
    CHECK_EQ(FP_RK_T_QUEDA, 900);
    CHECK_EQ(FP_RK_T_SLAM, 1100);
    CHECK_EQ(FP_RK_T_POUSO, 1600);
    CHECK_EQ(FP_RK_FIM, 2200);

    /* ---- as posicoes de referencia ---- */
    printf("== cinematica de rank: o centro da tela e o lugar da lista\n");
    /* `setPosition((640-244,8)/2, (360-52,8)/2)` = (197,6 , 153,6). */
    CHECK_PX(fp_rk_centro_x(), (640.0 - 244.8) / 2.0);
    CHECK_PX(fp_rk_centro_y(), (360.0 - 52.8) / 2.0);
    /* `originalPos` = intendedX(1), intendedY(1) -> (160,244 , 117,8). */
    CHECK_PX(fp_rk_origem_x(), 135.0 + 30.0 * sin(1.0));
    CHECK_PX(fp_rk_origem_y(), 117.8);

    /* ---- a capsula ---- */
    printf("== cinematica de rank: a capsula no centro, o tranco e o pouso\n");
    fp_rk_avaliar(0, 0, &e);
    CHECK_PX(e.cap_x, 197.6);
    CHECK_PX(e.cap_y, 153.6);
    fp_rk_avaliar(599, 0, &e);
    CHECK_PX(e.cap_x, 197.6);
    /* L1203: no mesmo instante do timer de 0,1 s, `capsuleToRank.x -= 10;
     * y -= 20`. A tween da queda so' comeca em 0,9 s (L1219). */
    fp_rk_avaliar(600, 0, &e);
    CHECK_PX(e.cap_x, 187.6);
    CHECK_PX(e.cap_y, 133.6);
    fp_rk_avaliar(800, 0, &e);
    CHECK_PX(e.cap_x, 187.6);
    /* L1217: tween para (originalPos - 7, - 80) em 1,3 s com quartIn. */
    {
        double t = 0.5;   /* 900 + 650 = 1550 */
        double esperado = 187.6 + (fp_rk_origem_x() - 7.0 - 187.6) * quart_in(t);
        fp_rk_avaliar(1550, 0, &e);
        CHECK_PX(e.cap_x, esperado);
    }
    {
        double t = 0.5;
        double esperado = 133.6 + (fp_rk_origem_y() - 80.0 - 133.6) * quart_in(t);
        fp_rk_avaliar(1550, 0, &e);
        CHECK_PX(e.cap_y, esperado);
    }
    /* A queda termina em 2200 ms, mas L1282 CANCELA as tweens no pouso e
     * escreve `originalPos` -- de 1600 em diante a posicao e' essa. */
    fp_rk_avaliar(1600, 0, &e);
    CHECK_PX(e.cap_x, fp_rk_origem_x());
    CHECK_PX(e.cap_y, fp_rk_origem_y());
    fp_rk_avaliar(2199, 0, &e);
    CHECK_PX(e.cap_x, fp_rk_origem_x());
    fp_rk_avaliar(20000, 0, &e);
    CHECK_PX(e.cap_x, fp_rk_origem_x());
    CHECK_PX(e.cap_y, fp_rk_origem_y());
    CHECK_PX(e.cap_dx, 0.0);
    CHECK_PX(e.cap_dy, 0.0);

    /* ---- as cameras ---- */
    printf("== cinematica de rank: os zooms das duas cameras\n");
    fp_rk_avaliar(0, 0, &e);
    CHECK_PX(e.rc_zoom, 1.85);
    CHECK_PX(e.fc_zoom, 1.15);
    fp_rk_avaliar(600, 0, &e);
    CHECK_PX(e.rc_zoom, 1.3);      /* L1198 manda 1,3 por cima da tween     */
    CHECK_PX(e.fc_zoom, 1.1);      /* a tween de 0,6 s terminou em 1,1      */
    /* Em 300 ms a tween 1,85 -> 1,8 andou sineIn(0,5) = 0,2929 do caminho. */
    fp_rk_avaliar(300, 0, &e);
    CHECK_PX(e.rc_zoom, 1.85 + (1.8 - 1.85) * sine_in(0.5));
    CHECK_PX(e.fc_zoom, 1.15 + (1.1 - 1.15) * sine_in(0.5));
    /* 600 -> 900: backInOut de 1,3 para 1,5. Em 750 ms, t = 0,5 -> 1,4. */
    fp_rk_avaliar(750, 0, &e);
    CHECK_PX(e.rc_zoom, 1.4);
    fp_rk_avaliar(900, 0, &e);
    CHECK_PX(e.rc_zoom, 1.5);
    CHECK_PX(e.fc_zoom, 1.05);
    /* 900 -> 1700: backIn de 1,5 para 1,2. */
    {
        double t = 0.5;
        fp_rk_avaliar(1300, 0, &e);
        CHECK_PX(e.rc_zoom, 1.5 + (1.2 - 1.5) * back_in(t));
    }
    /* No pouso os dois zoons caem para 0,8 e voltam para 1 com elasticOut. */
    fp_rk_avaliar(1600, 0, &e);
    CHECK_PX(e.rc_zoom, 0.8);
    CHECK_PX(e.fc_zoom, 0.8);
    fp_rk_avaliar(2100, 0, &e);
    CHECK_PX(e.rc_zoom, 0.8 + 0.2 * elastic_out(0.5));
    CHECK_PX(e.fc_zoom, 0.8 + 0.2 * elastic_out((2100.0 - 1600.0) / 800.0));
    fp_rk_avaliar(3000, 0, &e);
    CHECK_PX(e.rc_zoom, 1.0);
    CHECK_PX(e.fc_zoom, 1.0);

    /* ---- as camadas de tela cheia ---- */
    printf("== cinematica de rank: o preto do fade e o rankBg\n");
    fp_rk_avaliar(0, 0, &e);
    CHECK(e.fade_alpha > 0.99);
    CHECK_PX(e.bg_alpha, 1.0);
    fp_rk_avaliar(250, 0, &e);
    CHECK_PX(e.fade_alpha, 0.999999 * 0.5);
    CHECK_PX(e.bg_alpha, 1.0);
    fp_rk_avaliar(500, 0, &e);
    CHECK_PX(e.fade_alpha, 0.0);
    /* L1230: no slam, `FlxTween.tween(rankBg, {alpha: 0}, 0.5, expoIn)`. */
    fp_rk_avaliar(1100, 0, &e);
    CHECK_PX(e.bg_alpha, 1.0);
    fp_rk_avaliar(1350, 0, &e);
    CHECK_PX(e.bg_alpha, 1.0 - expo_in(0.5));
    fp_rk_avaliar(1600, 0, &e);
    CHECK_PX(e.bg_alpha, 0.0);

    /* ---- o selo novo e o antigo ---- */
    printf("== cinematica de rank: o selo novo (20 -> 0,9) e o antigo\n");
    fp_rk_avaliar(499, 1, &e);
    CHECK_EQ(e.selo_visivel, 0);
    CHECK_EQ(e.fake_visivel, 1);
    fp_rk_avaliar(500, 1, &e);
    CHECK_EQ(e.selo_visivel, 1);
    CHECK_PX(e.selo_scale, 20.0);
    fp_rk_avaliar(550, 1, &e);
    CHECK_PX(e.selo_scale, 20.0 + (0.9 - 20.0) * 0.5);   /* linear */
    fp_rk_avaliar(600, 1, &e);
    CHECK_PX(e.selo_scale, 0.9);
    CHECK_EQ(e.fake_visivel, 0);       /* L1180-1181 */
    /* Sem rank antigo o `fakeRanking` fica em alpha 0 (L1082: `alpha = 0`), o
     * que para nos e' o mesmo que nao desenhar. */
    fp_rk_avaliar(0, 0, &e);
    CHECK_EQ(e.fake_visivel, 0);

    /* ---- as faiscas ---- */
    printf("== cinematica de rank: as faiscas (5 quadros, 24 fps, uma vez)\n");
    fp_rk_avaliar(599, 0, &e);
    CHECK_EQ(e.sparks_q, -1);
    CHECK_EQ(e.sparksadd_q, -1);
    CHECK_EQ(FP_RK_SPARKS_QUADROS, 5);
    CHECK_EQ(FP_RK_SPARKSADD_QUADROS, 5);
    fp_rk_avaliar(600, 0, &e);
    CHECK_EQ(e.sparks_q, 0);
    fp_rk_avaliar(600 + 41, 0, &e);     /* 1/24 s = 41,67 ms -> quadro 0 ainda */
    CHECK_EQ(e.sparks_q, 0);
    fp_rk_avaliar(600 + 42, 0, &e);
    CHECK_EQ(e.sparks_q, 1);
    fp_rk_avaliar(600 + 4 * 42, 0, &e);
    CHECK_EQ(e.sparks_q, 4);
    /* 5 quadros a 24 fps = 208,3 ms: depois disso o `onFinish` as esconde. */
    fp_rk_avaliar(600 + 209, 0, &e);
    CHECK_EQ(e.sparks_q, -1);
    CHECK_EQ(e.sparksadd_q, -1);

    /* ---- a vinheta ---- */
    printf("== cinematica de rank: a vinheta\n");
    fp_rk_avaliar(1599, 0, &e);
    CHECK_PX(e.vinheta_alpha, 0.0);
    fp_rk_avaliar(1600, 0, &e);
    CHECK_PX(e.vinheta_alpha, 1.0);
    fp_rk_avaliar(1900, 0, &e);
    CHECK_PX(e.vinheta_alpha, 1.0 - expo_out(0.5));
    fp_rk_avaliar(2200, 0, &e);
    CHECK_PX(e.vinheta_alpha, 0.0);

    /* ---- o angulo da capsula ---- */
    printf("== cinematica de rank: o angulo da capsula\n");
    fp_rk_avaliar(599, 0, &e);
    CHECK_PX(e.cap_ang, 0.0);
    fp_rk_avaliar(600, 0, &e);
    CHECK_PX(e.cap_ang, -3.0);      /* L1207 */
    fp_rk_avaliar(850, 0, &e);
    CHECK_PX(e.cap_ang, -3.0 * (1.0 - back_out(0.5)));
    fp_rk_avaliar(1100, 0, &e);
    CHECK_PX(e.cap_ang, 0.0);

    /* ---- o tremor ---- */
    printf("== cinematica de rank: o IntervalShake\n");
    /* Antes de 600 ms nao ha' tremor nenhum. */
    fp_rk_avaliar(599, 0, &e);
    CHECK_PX(e.cap_dx, 0.0);
    /* Primeiro tique em 600+33 ms, com intensidade 0,1 (L1212) sobre uma
     * largura de 244,8 px: o deslocamento fica dentro de +-24,48. */
    {
        int dentro = 1, k;
        for (k = 0; k < 9; k++) {
            unsigned long t = 600 + (unsigned long)(k + 1) * 33 - 1;
            fp_rk_avaliar(t, 0, &e);
            if (fabs(e.cap_dx) > 24.48 + 0.01 || fabs(e.cap_dy) > 24.48 + 0.01)
                dentro = 0;
        }
        CHECK(dentro);
    }
    /* O ultimo tique devolve o objeto ao lugar e o tremor acaba em 900. */
    fp_rk_avaliar(900, 0, &e);
    CHECK_PX(e.cap_dx, 0.0);
    CHECK_PX(e.cap_dy, 0.0);
    /* O segundo tremor: 0,6 s a 1/24, intensidade 0,12 (L1284) -> +-29,376. */
    {
        int dentro = 1, mudou = 0, k;
        float ant = 0.0f;
        for (k = 0; k < 14; k++) {
            unsigned long t = 1600 + (unsigned long)(k + 1) * 41 - 1;
            fp_rk_avaliar(t, 0, &e);
            if (fabs(e.cap_dx) > 29.38 || fabs(e.cap_dy) > 29.38) dentro = 0;
            if (e.cap_dx != ant) mudou = 1;
            ant = e.cap_dx;
        }
        CHECK(dentro);
        CHECK(mudou);      /* e' tremor, nao um valor parado */
    }
    /* E depois de 2,2 s ele para em zero. */
    fp_rk_avaliar(2200, 0, &e);
    CHECK_PX(e.cap_dx, 0.0);
    CHECK_PX(e.cap_dy, 0.0);

    /* ---- os vizinhos ---- */
    printf("== cinematica de rank: os vizinhos\n");
    {
        fp_rk_viz_t v;
        /* dist 0: comeca no pouso, angulo em [-10, 10]. */
        fp_rk_vizinho(1599, 0, 3u, &v);
        CHECK_EQ(v.ativo, 0);
        fp_rk_vizinho(1600, 0, 3u, &v);
        CHECK_EQ(v.ativo, 1);
        CHECK(fabs(v.ang) <= 10.0001);
        /* dist 1: atrasa 1/20 s = 50 ms e o angulo cabe em [-8, 8]. */
        fp_rk_vizinho(1649, 1, 4u, &v);
        CHECK_EQ(v.ativo, 0);
        fp_rk_vizinho(1650, 1, 4u, &v);
        CHECK_EQ(v.ativo, 1);
        CHECK(fabs(v.ang) <= 8.0001);
        /* dist 4 (o ultimo que entra, `distFromSelected < 5`): [-2, 2]. */
        fp_rk_vizinho(1800, 4, 5u, &v);
        CHECK_EQ(v.ativo, 1);
        CHECK(fabs(v.ang) <= 2.0001);
        /* dist 5 nao entra. */
        fp_rk_vizinho(2200, 5, 5u, &v);
        CHECK_EQ(v.ativo, 0);
        /* A semente muda o sorteio, mas nao a faixa. */
        {
            fp_rk_viz_t a, b;
            fp_rk_vizinho(1700, 0, 1u, &a);
            fp_rk_vizinho(1700, 0, 2u, &b);
            CHECK(a.ang != b.ang || a.dx != b.dx);
        }
        /* O tremor do vizinho cabe em 0,12*largura/(dist+1). */
        {
            int k, dentro = 1;
            float lim = 0.12f * 244.8f / 2.0f;   /* dist 1 */
            for (k = 0; k < 14; k++) {
                fp_rk_vizinho(1650 + (unsigned long)(k + 1) * 41 - 1, 1, 9u, &v);
                if (fabs(v.dx) > lim || fabs(v.dy) > lim) dentro = 0;
            }
            CHECK(dentro);
        }
    }

    /* ---- o rastro do fadeAnim ---- */
    printf("== cinematica de rank: o rastro do fadeAnim (FlxTrail)\n");
    {
        float esc = 0.0f;
        /* alpha 0,01 na amostra 0, zero nas outras (as outras nascem com
         * alpha negativo: `_transp -= _difference` no `FlxTrail` L297). */
        CHECK_PX(fp_rk_rastro(0, 0, &esc), 0.01);
        CHECK_PX(esc, 1.0);
        CHECK_PX(fp_rk_rastro(0, 1, &esc), 0.0);
        /* A escala cresce de 1 a 2,5 em 0,5 s (linear). */
        fp_rk_rastro(250, 0, &esc);
        CHECK_PX(esc, 1.75);
        fp_rk_rastro(500, 0, &esc);
        CHECK_PX(esc, 2.5);
        /* E o grupo inteiro some em 0,6 s com quadOut. */
        CHECK_PX(fp_rk_rastro(300, 0, &esc), 0.01 * (1.0 - quad_out(0.5)));
        CHECK_PX(fp_rk_rastro(600, 0, &esc), 0.0);
    }

    if (falhas)
    {
        printf("CINEMATICA DE RANK: %d FALHA(S)\n", falhas);
        return 1;
    }
    printf("CINEMATICA DE RANK OK\n");
    return 0;
}
