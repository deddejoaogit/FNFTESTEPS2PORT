/* Teste da pontuacao. Os valores esperados sao calculados aqui com a
   MESMA formula do Scoring.hx, escritos a mao, para que um erro de
   transcricao no C apareca como falha. */
#include "../src/engine/judgement.h"
#include <stdio.h>
#include <math.h>

static int falhas = 0, checks = 0;
static void ok(int cond, const char *txt)
{
    checks++;
    if (!cond) { falhas++; printf("FALHOU: %s\n", txt); }
}
#define OK(c) ok((c), #c)
static int quase(double a, double b, double eps) { return fabs(a - b) <= eps; }

/* sigmoide oficial, reescrita aqui de proposito */
static double sigmoide(double a)
{
    return 500.0 * (1.0 - 1.0 / (1.0 + exp(-0.080 * (a - 54.99)))) + 9.0;
}

int main(void)
{
    judgement_t j;
    int p;

    printf("== PBOT1 (padrao do jogo) ==\n");
    jdg_reset(&j, 100, SCORE_PBOT1);

    OK(jdg_classificar(&j, 0.0f) == JUDGE_SICK);
    OK(jdg_classificar(&j, 45.0f) == JUDGE_SICK);
    OK(jdg_classificar(&j, 45.1f) == JUDGE_GOOD);
    OK(jdg_classificar(&j, 90.0f) == JUDGE_GOOD);
    OK(jdg_classificar(&j, 90.1f) == JUDGE_BAD);
    OK(jdg_classificar(&j, 135.0f) == JUDGE_BAD);
    OK(jdg_classificar(&j, 135.1f) == JUDGE_SHIT);
    OK(jdg_classificar(&j, 160.0f) == JUDGE_SHIT);
    OK(jdg_classificar(&j, 160.1f) == JUDGE_MISS);
    OK(jdg_classificar(&j, -45.0f) == JUDGE_SICK); /* atrasado = adiantado */

    /* menos de 5 ms e 500 pontos */
    ok(jdg_pontos(&j, 0.0f) == 500, "0 ms = 500 pontos");
    ok(jdg_pontos(&j, 4.9f) == 500, "4.9 ms = 500 pontos");
    ok(jdg_pontos(&j, 5.0f) == (int)sigmoide(5.0), "5 ms entra na sigmoide");
    ok(jdg_pontos(&j, 45.0f) == (int)sigmoide(45.0), "45 ms na sigmoide");
    ok(jdg_pontos(&j, 100.0f) == (int)sigmoide(100.0), "100 ms na sigmoide");
    ok(jdg_pontos(&j, 160.0f) == (int)sigmoide(160.0), "160 ms na sigmoide");
    ok(jdg_pontos(&j, 161.0f) == -100, "acima de 160 ms = -100");
    /* Std.int trunca (nao arredonda) */
    printf("   sigmoide: 5ms=%d 45ms=%d 90ms=%d 135ms=%d 160ms=%d\n",
           jdg_pontos(&j, 5.0f), jdg_pontos(&j, 45.0f), jdg_pontos(&j, 90.0f),
           jdg_pontos(&j, 135.0f), jdg_pontos(&j, 160.0f));
    OK(jdg_pontos(&j, 5.0f) > jdg_pontos(&j, 45.0f));
    OK(jdg_pontos(&j, 45.0f) > jdg_pontos(&j, 160.0f));

    printf("== pontos e vida ==\n");
    jdg_reset(&j, 4, SCORE_PBOT1);
    OK(quase(j.health, 1.0, 1e-6));         /* HEALTH_STARTING = MAX/2 = 1.0 */
    jdg_hit(&j, 0.0f, &p);
    OK(p == 500);
    OK(j.sick == 1 && j.combo == 1 && j.score == 500);
    OK(quase(j.health, 1.0 + 0.03, 1e-6));  /* SICK_BONUS = 1.5% de 2.0 = 0.03 */
    jdg_hit(&j, 60.0f, &p);
    OK(j.good == 1 && j.combo == 2);
    OK(quase(j.health, 1.03 + 0.015, 1e-6)); /* GOOD_BONUS = 0.75% de 2.0 */
    jdg_hit(&j, 120.0f, &p);
    OK(j.bad == 1);
    OK(quase(j.health, 1.045, 1e-6));        /* BAD_BONUS = 0 */
    jdg_hit(&j, 150.0f, &p);
    OK(j.shit == 1);
    OK(quase(j.health, 1.045 - 0.02, 1e-6)); /* SHIT_BONUS = -1% de 2.0 */

    printf("== erro ==\n");
    jdg_reset(&j, 4, SCORE_PBOT1);
    jdg_hit(&j, 0.0f, &p);
    jdg_hit(&j, 0.0f, &p);
    OK(j.combo == 2);
    jdg_miss(&j);
    OK(j.missed == 1);
    OK(j.combo == 0);                        /* miss quebra o combo */
    OK(j.max_combo == 2);                    /* mas o maximo fica */
    OK(quase(j.health, 1.0 + 0.06 - 0.08, 1e-6)); /* MISS_PENALTY = -4% de 2.0 */
    /* vida grampeia em 0 e em 2 */
    { int i; for (i = 0; i < 40; i++) jdg_miss(&j); }
    OK(j.health == 0.0f);
    { int i; for (i = 0; i < 200; i++) jdg_hit(&j, 0.0f, &p); }
    OK(j.health == 2.0f);

    printf("== nota longa ==\n");
    jdg_reset(&j, 4, SCORE_PBOT1);
    j.health = 1.0f;
    jdg_hold(&j, 1.0f);
    OK(quase(j.health, 1.0 + 0.12, 1e-6));   /* HOLD += 6% de 2.0 por segundo */

    printf("== completude e rank ==\n");
    jdg_reset(&j, 10, SCORE_PBOT1);
    j.sick = 10;
    OK(jdg_completude(&j) == 1.0);
    OK(jdg_rank(&j) == J_PERFECT_GOLD);      /* sick == total -> gold */
    jdg_reset(&j, 10, SCORE_PBOT1);
    j.sick = 9; j.missed = 1;
    OK(quase(jdg_completude(&j), 0.8, 1e-9)); /* (9+0-1)/10 */
    OK(jdg_rank(&j) == J_GREAT);              /* 0.8 >= GREAT */
    jdg_reset(&j, 10, SCORE_PBOT1);
    j.sick = 6; j.good = 3; j.missed = 1;
    OK(quase(jdg_completude(&j), 0.8, 1e-9));
    jdg_reset(&j, 10, SCORE_PBOT1);
    j.sick = 5; j.good = 4; j.missed = 0;
    OK(quase(jdg_completude(&j), 0.9, 1e-9));
    OK(jdg_rank(&j) == J_EXCELLENT);
    jdg_reset(&j, 10, SCORE_PBOT1);
    j.sick = 4; j.missed = 10; /* (4-10)/10 = -0.6 -> clamp em 0 */
    OK(jdg_completude(&j) == 0.0);
    OK(jdg_rank(&j) == J_SHIT_RANK);
    /* total 0 nao divide por zero */
    jdg_reset(&j, 0, SCORE_PBOT1);
    OK(jdg_completude(&j) == 0.0);

    printf("== LEGACY e WEEK7 ==\n");
    jdg_reset(&j, 10, SCORE_LEGACY);
    OK(jdg_pontos(&j, 0.0f) == 350);
    OK(jdg_pontos(&j, 40.0f) == 200);   /* janela 166.66 * 0.75 = 125 */
    OK(jdg_pontos(&j, 130.0f) == 100);  /* * 0.9 = 150 */
    OK(jdg_pontos(&j, 160.0f) == 50);
    OK(jdg_pontos(&j, 200.0f) == 0);
    jdg_reset(&j, 10, SCORE_WEEK7);
    OK(jdg_pontos(&j, 0.0f) == 350);
    OK(jdg_pontos(&j, 50.0f) == 200);   /* * 0.55 = 91.6 */

    printf("\n%d verificacoes, %d falhas\n", checks, falhas);
    return falhas ? 1 : 0;
}
