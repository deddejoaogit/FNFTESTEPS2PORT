/*
 * Testes do núcleo de gameplay.
 *
 * Cada asserção aqui é um número lido de FunkinCrew/Funkin v0.8.6, com o
 * arquivo citado ao lado. Se alguém mudar uma constante em gameplay.h "para
 * ficar mais fácil", é aqui que a build para -- e é de propósito: a regra do
 * projeto é que constante vem da fonte, não de aproximação.
 *
 *   gcc -O2 -Wall -Wextra -Werror -std=gnu11 -Isrc -Isrc/engine \
 *       -Itest/ps2sdk-shim test/test_gameplay.c src/engine/gameplay.c -lm
 */

#include <stdio.h>
#include <math.h>
#include "gameplay.h"

static int failures;

#define CHECK(cond) do { \
    if (!(cond)) { \
        printf("  FALHA %s:%d  %s\n", __FILE__, __LINE__, #cond); \
        failures++; \
    } \
} while (0)

#define CHECK_NEAR(a, b, eps) do { \
    double _a = (double)(a), _b = (double)(b); \
    if (fabs(_a - _b) > (eps)) { \
        printf("  FALHA %s:%d  %s = %.6f, esperado %.6f\n", \
               __FILE__, __LINE__, #a, _a, _b); \
        failures++; \
    } \
} while (0)

/* O projeto proibe descartar valor de retorno com (void) -- a suite rejeita
   isso por grep, e por bom motivo. Estes dois lacoes fazem o acerto e conferem
   o julgamento, que e' o que o teste quer de qualquer forma. */
static void hit_many(fnf_play_t *p, int n, float ms, fnf_judge_t want)
{
    int i;
    for (i = 0; i < n; i++) {
        if (fnf_play_hit(p, ms) != want) {
            printf("  FALHA: acerto %d de %d com %.1f ms nao deu %s\n",
                   i, n, (double)ms, fnf_judge_name(want));
            failures++;
            return;
        }
    }
}

/* A janela do JOGO (Constants.HIT_WINDOW_MS) e a conversao para amostras que
   main.c usa. 160 ms a 48 kHz = 7.680 amostras; o port usava 7.200 para acertar
   e 12.000 para perder, nenhum dos dois existindo na fonte. */
static void test_hit_window_matches_game(void)
{
    const int amostras = (int)(FNF_HIT_WINDOW_MS * 48000.0f / 1000.0f);
    CHECK_NEAR(FNF_HIT_WINDOW_MS, 160.0, 1e-6);
    CHECK(amostras == 7680);
    /* A janela de acerto e a de nota perdida sao o mesmo numero no original. */
    CHECK(fnf_play_is_pressable(FNF_HIT_WINDOW_MS, FNF_SCORING_PBOT1) == 1);
    CHECK(fnf_play_is_pressable(-FNF_HIT_WINDOW_MS, FNF_SCORING_PBOT1) == 1);
    CHECK(fnf_play_is_pressable(FNF_HIT_WINDOW_MS + 0.5f, FNF_SCORING_PBOT1) == 0);
    /* E o julgamento concorda com a janela: dentro nunca da' MISS. */
    CHECK(fnf_judge_ms(159.0f, FNF_SCORING_PBOT1) != FNF_JUDGE_MISS);
    CHECK(fnf_judge_ms(161.0f, FNF_SCORING_PBOT1) == FNF_JUDGE_MISS);
}

static void test_windows(void)
{
    /* Scoring.PBOT1_SHIT_THRESHOLD / LEGACY_HIT_WINDOW. */
    CHECK_NEAR(fnf_hit_window_ms(FNF_SCORING_PBOT1), 160.0, 0.001);
    CHECK_NEAR(fnf_hit_window_ms(FNF_SCORING_LEGACY), 166.66666, 0.001);
    CHECK_NEAR(fnf_hit_window_ms(FNF_SCORING_WEEK7), 166.66666, 0.001);
}

static void test_pbot1_judge(void)
{
    /* Limites de Scoring.judgeNotePBOT1, todos "menor ou igual". */
    CHECK(fnf_judge_ms(0.0f, FNF_SCORING_PBOT1) == FNF_JUDGE_SICK);
    CHECK(fnf_judge_ms(45.0f, FNF_SCORING_PBOT1) == FNF_JUDGE_SICK);
    CHECK(fnf_judge_ms(45.1f, FNF_SCORING_PBOT1) == FNF_JUDGE_GOOD);
    CHECK(fnf_judge_ms(90.0f, FNF_SCORING_PBOT1) == FNF_JUDGE_GOOD);
    CHECK(fnf_judge_ms(90.1f, FNF_SCORING_PBOT1) == FNF_JUDGE_BAD);
    CHECK(fnf_judge_ms(135.0f, FNF_SCORING_PBOT1) == FNF_JUDGE_BAD);
    CHECK(fnf_judge_ms(135.1f, FNF_SCORING_PBOT1) == FNF_JUDGE_SHIT);
    CHECK(fnf_judge_ms(160.0f, FNF_SCORING_PBOT1) == FNF_JUDGE_SHIT);
    CHECK(fnf_judge_ms(160.1f, FNF_SCORING_PBOT1) == FNF_JUDGE_MISS);
    /* O sinal não importa: o original usa Math.abs. */
    CHECK(fnf_judge_ms(-44.0f, FNF_SCORING_PBOT1) == FNF_JUDGE_SICK);
    CHECK(fnf_judge_ms(-200.0f, FNF_SCORING_PBOT1) == FNF_JUDGE_MISS);
    /* O rating 'killer' está comentado no original: nunca sai. */
    CHECK(fnf_judge_ms(1.0f, FNF_SCORING_PBOT1) != FNF_JUDGE_KILLER);
}

static void test_pbot1_score(void)
{
    /* scoreNotePBOT1: <5 ms é o máximo; >160 ms é o valor de erro. */
    CHECK(fnf_score_ms(0.0f, FNF_SCORING_PBOT1) == 500);
    CHECK(fnf_score_ms(4.9f, FNF_SCORING_PBOT1) == 500);
    CHECK(fnf_score_ms(161.0f, FNF_SCORING_PBOT1) == -100);
    /* No limite de 45 ms o Haxe calcula 353 (344,9 + 9). */
    CHECK(fnf_score_ms(45.0f, FNF_SCORING_PBOT1) == 353);
    /* 160 ms ainda é acerto: 9 pontos, o piso da sigmóide. */
    CHECK(fnf_score_ms(160.0f, FNF_SCORING_PBOT1) == 9);
    /* A curva é monótona: erro maior nunca dá mais pontos. */
    CHECK(fnf_score_ms(20.0f, FNF_SCORING_PBOT1) > fnf_score_ms(60.0f, FNF_SCORING_PBOT1));
    CHECK(fnf_score_ms(60.0f, FNF_SCORING_PBOT1) > fnf_score_ms(140.0f, FNF_SCORING_PBOT1));
}

static void test_legacy_and_week7(void)
{
    /* Scoring.LEGACY_*: janela 166,666 ms; 20/75/90/100 %. */
    CHECK(fnf_judge_ms(33.0f, FNF_SCORING_LEGACY) == FNF_JUDGE_SICK);
    CHECK(fnf_judge_ms(34.0f, FNF_SCORING_LEGACY) == FNF_JUDGE_GOOD);
    CHECK(fnf_judge_ms(120.0f, FNF_SCORING_LEGACY) == FNF_JUDGE_GOOD);
    CHECK(fnf_judge_ms(126.0f, FNF_SCORING_LEGACY) == FNF_JUDGE_BAD);
    CHECK(fnf_judge_ms(150.0f, FNF_SCORING_LEGACY) == FNF_JUDGE_BAD);
    CHECK(fnf_judge_ms(155.0f, FNF_SCORING_LEGACY) == FNF_JUDGE_SHIT);
    CHECK(fnf_judge_ms(170.0f, FNF_SCORING_LEGACY) == FNF_JUDGE_MISS);
    CHECK(fnf_score_ms(10.0f, FNF_SCORING_LEGACY) == 350);
    CHECK(fnf_score_ms(100.0f, FNF_SCORING_LEGACY) == 200);
    CHECK(fnf_score_ms(140.0f, FNF_SCORING_LEGACY) == 100);
    CHECK(fnf_score_ms(160.0f, FNF_SCORING_LEGACY) == 50);
    CHECK(fnf_score_ms(200.0f, FNF_SCORING_LEGACY) == 0);
    CHECK(fnf_miss_score(FNF_SCORING_LEGACY) == -10);

    /* WEEK7: 20/55/80 %. */
    CHECK(fnf_judge_ms(30.0f, FNF_SCORING_WEEK7) == FNF_JUDGE_SICK);
    CHECK(fnf_judge_ms(80.0f, FNF_SCORING_WEEK7) == FNF_JUDGE_GOOD);
    CHECK(fnf_judge_ms(120.0f, FNF_SCORING_WEEK7) == FNF_JUDGE_BAD);
    CHECK(fnf_judge_ms(160.0f, FNF_SCORING_WEEK7) == FNF_JUDGE_SHIT);
    CHECK(fnf_judge_ms(170.0f, FNF_SCORING_WEEK7) == FNF_JUDGE_MISS);
}

static void test_health_and_combo(void)
{
    /* Constants.HEALTH_*, todos frações de HEALTH_MAX = 2,0. */
    CHECK_NEAR(fnf_judge_health(FNF_JUDGE_SICK), 0.03, 1e-6);
    CHECK_NEAR(fnf_judge_health(FNF_JUDGE_GOOD), 0.015, 1e-6);
    CHECK_NEAR(fnf_judge_health(FNF_JUDGE_BAD), 0.0, 1e-6);
    CHECK_NEAR(fnf_judge_health(FNF_JUDGE_SHIT), -0.02, 1e-6);
    CHECK_NEAR(fnf_judge_health(FNF_JUDGE_MISS), -0.08, 1e-6);
    CHECK_NEAR(fnf_judge_health(FNF_JUDGE_KILLER), 0.04, 1e-6);
    CHECK_NEAR(FNF_HEALTH_HOLD_BONUS_PER_SECOND, 0.12, 1e-6);
    CHECK_NEAR(FNF_HEALTH_STARTING, 1.0, 1e-6);
    CHECK_NEAR(FNF_HEALTH_MAX, 2.0, 1e-6);

    /* Constants.JUDGEMENT_*_COMBO_BREAK. */
    CHECK(fnf_judge_breaks_combo(FNF_JUDGE_SICK) == 0);
    CHECK(fnf_judge_breaks_combo(FNF_JUDGE_GOOD) == 0);
    CHECK(fnf_judge_breaks_combo(FNF_JUDGE_BAD) == 1);
    CHECK(fnf_judge_breaks_combo(FNF_JUDGE_SHIT) == 1);
    CHECK(fnf_judge_breaks_combo(FNF_JUDGE_MISS) == 1);
}

static void test_play_sequence(void)
{
    fnf_play_t p;

    fnf_play_reset(&p, FNF_SCORING_PBOT1, 100);
    CHECK_NEAR(p.health, 1.0, 1e-6);
    CHECK(p.score == 0 && p.combo == 0 && p.max_combo == 0 && !p.dead);

    /* Tres SICK seguidos: combo 3, vida 1,0 + 3x0,03, placar 500+495+495.
       A tabela da sigmoide, para referencia (medida, nao estimada):
           0 ms -> 500     10 ms -> 495     20 ms -> 480     30 ms -> 449
          45 ms -> 353     60 ms -> 209     90 ms ->  37    120 ms ->  11
         160 ms ->   9
       So o primeiro e' 500 porque o perfeito e' |ms| < 5 no original. */
    CHECK(fnf_score_ms(10.0f, FNF_SCORING_PBOT1) == 495);
    CHECK(fnf_score_ms(20.0f, FNF_SCORING_PBOT1) == 480);
    CHECK(fnf_score_ms(30.0f, FNF_SCORING_PBOT1) == 449);
    CHECK(fnf_score_ms(60.0f, FNF_SCORING_PBOT1) == 209);
    CHECK(fnf_score_ms(90.0f, FNF_SCORING_PBOT1) == 37);
    CHECK(fnf_score_ms(120.0f, FNF_SCORING_PBOT1) == 11);
    CHECK(fnf_play_hit(&p, 0.0f) == FNF_JUDGE_SICK);
    CHECK(fnf_play_hit(&p, 10.0f) == FNF_JUDGE_SICK);
    CHECK(fnf_play_hit(&p, -10.0f) == FNF_JUDGE_SICK);
    CHECK(p.combo == 3 && p.max_combo == 3 && p.sick == 3);
    CHECK_NEAR(p.health, 1.09, 1e-5);
    CHECK(p.score == 1490);

    /* Um BAD mantém a vida (bônus 0) e QUEBRA o combo. */
    CHECK(fnf_play_hit(&p, 100.0f) == FNF_JUDGE_BAD);
    CHECK(p.combo == 0 && p.bad == 1 && p.max_combo == 3);
    CHECK_NEAR(p.health, 1.09, 1e-5);

    /* O placar NAO e' limitado em zero: o original soma e mostra o negativo
       (PBOT1_MISS_SCORE = -100, SCORE_GHOST_MISS_PENALTY = -10). */
    {
        fnf_play_t q;
        int i;
        fnf_play_reset(&q, FNF_SCORING_PBOT1, 100);
        for (i = 0; i < 5; i++) fnf_play_miss(&q);
        CHECK(q.score == -500);
        for (i = 0; i < 3; i++) fnf_play_ghost(&q);
        CHECK(q.score == -530);
    }

    /* Um MISS tira 0,08 e 100 pontos. */
    fnf_play_miss(&p);
    CHECK(p.missed == 1);
    CHECK_NEAR(p.health, 1.01, 1e-5);

    /* Aperto longe (164 ms) não conta como acerto: devolve MISS e não mexe em
       nada. O chamador decide se é ghost. */
    {
        int before_score = p.score;
        int before_sick = p.sick;
        CHECK(fnf_play_hit(&p, 164.0f) == FNF_JUDGE_MISS);
        CHECK(p.score == before_score && p.sick == before_sick);
    }

    /* Ghost: -0,08 de vida e -10 de placar, sem tally nem combo. */
    {
        int before_combo = p.combo;
        fnf_play_ghost(&p);
        CHECK_NEAR(p.health, 0.93, 1e-5);
        CHECK(p.combo == before_combo);
        CHECK(p.missed == 1);
    }

    /* Hold por meio segundo: +0,06 de vida, +125 de placar. */
    {
        float before_health = p.health;
        int before_score = p.score;
        fnf_play_hold(&p, 0.5f);
        CHECK_NEAR(p.health - before_health, 0.06, 1e-5);
        CHECK(p.score - before_score == 125);
    }

    /* A vida satura em HEALTH_MAX, não passa. */
    fnf_play_reset(&p, FNF_SCORING_PBOT1, 100);
    hit_many(&p, 100, 0.0f, FNF_JUDGE_SICK);
    CHECK_NEAR(p.health, FNF_HEALTH_MAX, 1e-6);
    CHECK_NEAR(fnf_play_health_ratio(&p), 1.0, 1e-6);

    /* E a morte acontece em HEALTH_MIN. */
    fnf_play_reset(&p, FNF_SCORING_PBOT1, 100);
    { int i; for (i = 0; i < 13; i++) fnf_play_miss(&p); }
    CHECK(p.dead == 1);
    CHECK_NEAR(p.health, FNF_HEALTH_MIN, 1e-6);
    CHECK_NEAR(fnf_play_health_ratio(&p), 0.0, 1e-6);
}

static void test_result(void)
{
    fnf_play_t p;

    /* Sem nota nenhuma, o ResultState não tem o que mostrar. */
    fnf_play_reset(&p, FNF_SCORING_PBOT1, 0);
    CHECK(fnf_play_rank(&p) == FNF_RANK_NONE);
    CHECK_NEAR(fnf_play_completion(&p), 0.0, 1e-6);

    /* 10 notas, 10 SICK: PERFECT GOLD. */
    fnf_play_reset(&p, FNF_SCORING_PBOT1, 10);
    hit_many(&p, 10, 0.0f, FNF_JUDGE_SICK);
    CHECK(fnf_play_rank(&p) == FNF_RANK_PERFECT_GOLD);
    CHECK(fnf_play_clear_percent(&p) == 100);

    /* 10 notas: 9 SICK + 1 MISS -> (9+0-1)/10 = 0,8 -> GREAT (e não PERFECT). */
    fnf_play_reset(&p, FNF_SCORING_PBOT1, 10);
    hit_many(&p, 9, 0.0f, FNF_JUDGE_SICK);
    fnf_play_miss(&p);
    CHECK_NEAR(fnf_play_completion(&p), 0.8, 1e-6);
    CHECK(fnf_play_rank(&p) == FNF_RANK_GREAT);
    CHECK(fnf_play_clear_percent(&p) == 80);

    /* 10 SICK + 1 MISS em 10 notas daria 1,1 -> clampado em 1,0 -> PERFECT,
       que é exatamente o motivo de o original clampar. */
    fnf_play_reset(&p, FNF_SCORING_PBOT1, 5);
    hit_many(&p, 6, 0.0f, FNF_JUDGE_SICK);
    fnf_play_miss(&p);
    CHECK_NEAR(fnf_play_completion(&p), 1.0, 1e-6);
    CHECK(fnf_play_rank(&p) == FNF_RANK_PERFECT);   /* não GOLD: houve miss */

    /* Metade GOOD, metade nada: 0,5 -> SHIT. */
    fnf_play_reset(&p, FNF_SCORING_PBOT1, 4);
    hit_many(&p, 2, 60.0f, FNF_JUDGE_GOOD);
    fnf_play_miss(&p);
    fnf_play_miss(&p);
    CHECK(fnf_play_rank(&p) == FNF_RANK_SHIT);

    /* Nomes, para o ResultState não imprimir número. */
    CHECK(fnf_rank_name(FNF_RANK_PERFECT_GOLD)[0] == 'P');
    CHECK(fnf_judge_name(FNF_JUDGE_SICK)[0] == 's');
}

int main(void)
{
    printf("== gameplay: janelas e julgamento ==\n");
    test_windows();
    test_hit_window_matches_game();
    test_pbot1_judge();
    printf("== gameplay: pontuacao ==\n");
    test_pbot1_score();
    test_legacy_and_week7();
    printf("== gameplay: vida e combo ==\n");
    test_health_and_combo();
    test_play_sequence();
    printf("== gameplay: resultado ==\n");
    test_result();

    if (failures) {
        printf("\n  %d falha(s)\n", failures);
        return 1;
    }
    printf("\n  gameplay: tudo exato\n");
    return 0;
}
