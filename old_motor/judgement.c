#include "judgement.h"
#include <math.h>
#include <string.h>

/* --- constantes do PBOT1 (padrao do jogo) -------------------------- */
#define PBOT1_MAX_SCORE          500
#define PBOT1_SCORING_OFFSET     54.99
#define PBOT1_SCORING_SLOPE      0.080
#define PBOT1_MIN_SCORE          9.0
#define PBOT1_MISS_SCORE        (-100)
#define PBOT1_PERFECT_THRESHOLD  5.0
#define PBOT1_MISS_THRESHOLD     160.0
#define PBOT1_KILLER_THRESHOLD   12.5
#define PBOT1_SICK_THRESHOLD     45.0
#define PBOT1_GOOD_THRESHOLD     90.0
#define PBOT1_BAD_THRESHOLD      135.0
#define PBOT1_SHIT_THRESHOLD     160.0

/* --- LEGACY ------------------------------------------------------- */
#define LEGACY_HIT_WINDOW  (10.0 / 60.0 * 1000.0)
#define LEGACY_SICK_THRESHOLD  0.2
#define LEGACY_GOOD_THRESHOLD  0.75
#define LEGACY_BAD_THRESHOLD   0.9
#define LEGACY_SHIT_THRESHOLD  1.0
#define LEGACY_SICK_SCORE  350
#define LEGACY_GOOD_SCORE  200
#define LEGACY_BAD_SCORE   100
#define LEGACY_SHIT_SCORE   50
#define LEGACY_MISS_SCORE  (-10)

/* --- WEEK7 -------------------------------------------------------- */
#define WEEK7_HIT_WINDOW        LEGACY_HIT_WINDOW
#define WEEK7_SICK_THRESHOLD    0.2
#define WEEK7_GOOD_THRESHOLD    0.55
#define WEEK7_BAD_THRESHOLD     0.8

/* --- vida (Constants.hx) ------------------------------------------ */
#define HEALTH_MAX 2.0f
#define HEALTH_MIN 0.0f
#define HEALTH_STARTING (HEALTH_MAX / 2.0f)
#define HEALTH_KILLER_BONUS (2.0f / 100.0f * HEALTH_MAX)
#define HEALTH_SICK_BONUS   (1.5f / 100.0f * HEALTH_MAX)
#define HEALTH_GOOD_BONUS   (0.75f / 100.0f * HEALTH_MAX)
#define HEALTH_BAD_BONUS    (0.0f / 100.0f * HEALTH_MAX)
#define HEALTH_SHIT_BONUS   (-1.0f / 100.0f * HEALTH_MAX)
#define HEALTH_HOLD_BONUS_PER_SECOND (6.0f / 100.0f * HEALTH_MAX)
#define HEALTH_MISS_PENALTY (-4.0f / 100.0f * HEALTH_MAX)

/* --- ranks (Constants.hx:580..595) -------------------------------- */
#define RANK_PERFECT_THRESHOLD   1.00
#define RANK_EXCELLENT_THRESHOLD 0.90
#define RANK_GREAT_THRESHOLD     0.80
#define RANK_GOOD_THRESHOLD      0.60

static float mod(float v) { return v < 0.0f ? -v : v; }

static void clamp_health(judgement_t *j)
{
    if (j->health > HEALTH_MAX) j->health = HEALTH_MAX;
    if (j->health < HEALTH_MIN) j->health = HEALTH_MIN;
}

void jdg_reset(judgement_t *j, unsigned int total_notes, fnf_score_system_t sistema)
{
    memset(j, 0, sizeof(*j));
    j->total_notes = total_notes;
    j->sistema = sistema;
    j->health = HEALTH_STARTING;
    j->health_lerp = HEALTH_STARTING;
}

fnf_judge_t jdg_classificar(const judgement_t *j, float desvio_ms)
{
    float a = mod(desvio_ms);
    switch (j->sistema) {
    case SCORE_LEGACY:
        if (a <= LEGACY_HIT_WINDOW * LEGACY_SICK_THRESHOLD) return JUDGE_SICK;
        if (a <= LEGACY_HIT_WINDOW * LEGACY_GOOD_THRESHOLD) return JUDGE_GOOD;
        if (a <= LEGACY_HIT_WINDOW * LEGACY_BAD_THRESHOLD) return JUDGE_BAD;
        if (a <= LEGACY_HIT_WINDOW * LEGACY_SHIT_THRESHOLD) return JUDGE_SHIT;
        return JUDGE_MISS;
    case SCORE_WEEK7:
        if (a <= WEEK7_HIT_WINDOW * WEEK7_SICK_THRESHOLD) return JUDGE_SICK;
        if (a <= WEEK7_HIT_WINDOW * WEEK7_GOOD_THRESHOLD) return JUDGE_GOOD;
        if (a <= WEEK7_HIT_WINDOW * WEEK7_BAD_THRESHOLD) return JUDGE_BAD;
        if (a <= WEEK7_HIT_WINDOW) return JUDGE_SHIT;
        return JUDGE_MISS;
    case SCORE_PBOT1:
    default:
        if (a <= PBOT1_SICK_THRESHOLD) return JUDGE_SICK;
        if (a <= PBOT1_GOOD_THRESHOLD) return JUDGE_GOOD;
        if (a <= PBOT1_BAD_THRESHOLD) return JUDGE_BAD;
        if (a <= PBOT1_SHIT_THRESHOLD) return JUDGE_SHIT;
        return JUDGE_MISS;
    }
}

int jdg_pontos(const judgement_t *j, float desvio_ms)
{
    float a = mod(desvio_ms);
    switch (j->sistema) {
    case SCORE_LEGACY:
        if (a < LEGACY_HIT_WINDOW * LEGACY_SICK_THRESHOLD) return LEGACY_SICK_SCORE;
        if (a < LEGACY_HIT_WINDOW * LEGACY_GOOD_THRESHOLD) return LEGACY_GOOD_SCORE;
        if (a < LEGACY_HIT_WINDOW * LEGACY_BAD_THRESHOLD) return LEGACY_BAD_SCORE;
        if (a < LEGACY_HIT_WINDOW * LEGACY_SHIT_THRESHOLD) return LEGACY_SHIT_SCORE;
        return 0;
    case SCORE_WEEK7:
        if (a < WEEK7_HIT_WINDOW * WEEK7_SICK_THRESHOLD) return LEGACY_SICK_SCORE;
        if (a < WEEK7_HIT_WINDOW * WEEK7_GOOD_THRESHOLD) return LEGACY_GOOD_SCORE;
        if (a < WEEK7_HIT_WINDOW * WEEK7_BAD_THRESHOLD) return LEGACY_BAD_SCORE;
        if (a < WEEK7_HIT_WINDOW) return LEGACY_SHIT_SCORE;
        return 0;
    case SCORE_PBOT1:
    default: {
        double factor, score;
        if (a > PBOT1_MISS_THRESHOLD) return PBOT1_MISS_SCORE;
        if (a < PBOT1_PERFECT_THRESHOLD) return PBOT1_MAX_SCORE;
        /* sigmoide oficial: 1 - 1/(1+exp(-slope*(a-offset))) */
        factor = 1.0 - (1.0 / (1.0 + exp(-PBOT1_SCORING_SLOPE * (a - PBOT1_SCORING_OFFSET))));
        score = PBOT1_MAX_SCORE * factor + PBOT1_MIN_SCORE;
        return (int)score; /* Std.int trunca, nao arredonda */
    }
    }
}

fnf_judge_t jdg_hit(judgement_t *j, float desvio_ms, int *pontos)
{
    fnf_judge_t julg = jdg_classificar(j, desvio_ms);
    int p = jdg_pontos(j, desvio_ms);
    if (pontos) *pontos = p;
    if (julg == JUDGE_MISS) { jdg_miss(j); return JUDGE_MISS; }

    switch (julg) {
    case JUDGE_SICK: j->sick++;  j->health += HEALTH_SICK_BONUS;  break;
    case JUDGE_GOOD: j->good++;  j->health += HEALTH_GOOD_BONUS;  break;
    case JUDGE_BAD:  j->bad++;   j->health += HEALTH_BAD_BONUS;   break;
    case JUDGE_SHIT: j->shit++;  j->health += HEALTH_SHIT_BONUS;  break;
    default: break;
    }
    (void)HEALTH_KILLER_BONUS; /* limiar de killer existe mas o jogo so
                                  usa o bonus de sick no 0.8.6 */
    j->combo++;
    if (j->combo > j->max_combo) j->max_combo = j->combo;
    j->total_notes_hit++;
    j->score += p;
    j->notas_processadas++;
    clamp_health(j);
    return julg;
}

void jdg_miss(judgement_t *j)
{
    j->missed++;
    j->combo = 0; /* PlayState.hx:3352 */
    j->health += HEALTH_MISS_PENALTY;
    j->notas_processadas++;
    clamp_health(j);
}

void jdg_hold(judgement_t *j, float segundos)
{
    if (segundos <= 0.0f) return;
    j->health += HEALTH_HOLD_BONUS_PER_SECOND * segundos;
    clamp_health(j);
    j->holds_segurados++;
}

void jdg_hold_drop(judgement_t *j, float segundos_segurados)
{
    /* Constants.hx:512/517: as duas taxas de penalidade do drop sao 0.0
       nesta versao. O que quebra o combo e o proprio miss da nota. */
    (void)j;
    (void)segundos_segurados;
}

/* Haxe Float e double: manter double aqui nao e preciosismo. Com float,
   (5+4-0)/10 vira 0.89999998 e o rank EXCELLENT (limiar 0.90) cai para
   GREAT no ultimo digito. Mesma armadilha para o limiar PERFECT. */
double jdg_completude(const judgement_t *j)
{
    double v;
    if (!j->total_notes) return 0.0;
    v = ((double)j->sick + (double)j->good - (double)j->missed) / (double)j->total_notes;
    if (v < 0.0) v = 0.0;
    if (v > 1.0) v = 1.0;
    return v;
}

fnf_rank_t jdg_rank(const judgement_t *j)
{
    double c;
    if (!j->total_notes) return J_SHIT_RANK;
    if (j->sick == j->total_notes) return J_PERFECT_GOLD;
    c = jdg_completude(j);
    if (c >= RANK_PERFECT_THRESHOLD) return J_PERFECT;
    if (c >= RANK_EXCELLENT_THRESHOLD) return J_EXCELLENT;
    if (c >= RANK_GREAT_THRESHOLD) return J_GREAT;
    if (c >= RANK_GOOD_THRESHOLD) return J_GOOD;
    return J_SHIT_RANK;
}

const char *jdg_rank_nome(fnf_rank_t r)
{
    switch (r) {
    case J_PERFECT_GOLD: return "PERFECT_GOLD";
    case J_PERFECT:      return "PERFECT";
    case J_EXCELLENT:    return "EXCELLENT";
    case J_GREAT:        return "GREAT";
    case J_GOOD:         return "GOOD";
    default:             return "SHIT";
    }
}

const char *jdg_nome(fnf_judge_t j)
{
    switch (j) {
    case JUDGE_SICK:   return "sick";
    case JUDGE_GOOD:   return "good";
    case JUDGE_BAD:    return "bad";
    case JUDGE_SHIT:   return "shit";
    case JUDGE_KILLER: return "killer";
    default:           return "miss";
    }
}

float jdg_acerto(const judgement_t *j)
{
    if (!j->total_notes) return 0.0f;
    return (float)j->total_notes_hit / (float)j->total_notes;
}
