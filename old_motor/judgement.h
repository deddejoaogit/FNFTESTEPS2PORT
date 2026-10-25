/* judgement.h - pontuacao, vida, combo, julgamento e resultado
 *
 * Extraido de play/scoring/Scoring.hx e util/Constants.hx do fonte oficial
 * v0.8.6. O sistema PADRAO do jogo e PBOT1 (Scoring.scoreNote default),
 * nao o LEGACY do FNF antigo: um erro comum de port e usar 350/200/100/50
 * quando o certo e a curva sigmoide de 500 pontos.
 *
 * Valores copiados, com a linha de origem:
 *   Scoring.hx:100  PBOT1_MAX_SCORE        500
 *   Scoring.hx:105  PBOT1_SCORING_OFFSET   54.99
 *   Scoring.hx:110  PBOT1_SCORING_SLOPE    0.080
 *   Scoring.hx:115  PBOT1_MIN_SCORE        9.0
 *   Scoring.hx:120  PBOT1_MISS_SCORE      -100
 *   Scoring.hx:125  PBOT1_PERFECT_THRESHOLD 5.0 ms
 *   Scoring.hx:130  PBOT1_MISS_THRESHOLD   160 ms
 *   Scoring.hx:147  PBOT1_SICK_THRESHOLD   45 ms
 *   Scoring.hx:154  PBOT1_GOOD_THRESHOLD   90 ms
 *   Scoring.hx:159  PBOT1_BAD_THRESHOLD   135 ms
 *   Scoring.hx:164  PBOT1_SHIT_THRESHOLD  160 ms
 *   Scoring.hx:209  LEGACY_HIT_WINDOW     (10/60)*1000 = 166.66 ms
 *   Scoring.hx:214..231 LEGACY_SICK 0.2, GOOD 0.75, BAD 0.9, SHIT 1.0
 *   Scoring.hx:236..256 LEGACY_SICK_SCORE 350 / GOOD 200 / BAD 100 / SHIT 50 / MISS -10
 *   Scoring.hx:305..307 WEEK7 BAD 0.8, GOOD 0.55, SICK 0.2 do hit window
 *   Scoring.hx:430  tallyCompletion = (sick+good-missed)/totalNotes, clamp 0..1
 *   Constants.hx:455 HEALTH_MAX 2.0, :460 HEALTH_STARTING 1.0, :466 HEALTH_MIN 0.0
 *   Constants.hx:471 HEALTH_KILLER_BONUS  +2.0% de HEALTH_MAX
 *   Constants.hx:476 HEALTH_SICK_BONUS    +1.5%
 *   Constants.hx:481 HEALTH_GOOD_BONUS    +0.75%
 *   Constants.hx:486 HEALTH_BAD_BONUS      0.0%
 *   Constants.hx:492 HEALTH_SHIT_BONUS    -1.0%
 *   Constants.hx:497 HEALTH_HOLD_BONUS_PER_SECOND +6.0% / s
 *   Constants.hx:502 HEALTH_MISS_PENALTY  -4.0%
 *   Constants.hx:580 RANK_PERFECT 1.00, :585 EXCELLENT 0.90, :590 GREAT 0.80, :595 GOOD 0.60
 *
 * Nota de fidelidade: no 0.8.6 o bonus de vida por acerto depende do
 * julgamento (1,5% no sick), e o combo NAO multiplica pontos -- quem
 * multiplicava era o FNF antigo. applyScore (PlayState.hx:3330) so soma
 * score, mexe no combo e no health.
 */
#ifndef JUDGEMENT_H
#define JUDGEMENT_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    J_PERFECT_GOLD = 0,
    J_PERFECT,
    J_EXCELLENT,
    J_GREAT,
    J_GOOD,
    J_SHIT_RANK
} fnf_rank_t;

typedef enum {
    JUDGE_MISS = 0,
    JUDGE_SHIT,
    JUDGE_BAD,
    JUDGE_GOOD,
    JUDGE_SICK,
    JUDGE_KILLER   /* existe no PBOT1, so como limiar de vida/efeito */
} fnf_judge_t;

typedef enum {
    SCORE_PBOT1 = 0, /* padrao do jogo */
    SCORE_LEGACY,
    SCORE_WEEK7
} fnf_score_system_t;

typedef struct {
    /* contadores de julgamento (Save.hx SaveScoreTallyData) */
    unsigned int sick, good, bad, shit, missed;
    unsigned int combo, max_combo;
    unsigned int total_notes;      /* notas jogaveis do chart */
    unsigned int total_notes_hit;  /* notas acertadas (PlayState.hx:3161) */
    int          score;            /* songScore, inteiro como no original */
    /* vida */
    float        health;
    float        health_lerp;
    /* estado */
    fnf_score_system_t sistema;
    unsigned int notas_processadas;
    unsigned int holds_segurados;
} judgement_t;

void  jdg_reset(judgement_t *j, unsigned int total_notes, fnf_score_system_t sistema);

/* Julgamento e pontos de um acerto com desvio de tempo em ms.
   Devolve o julgamento; *pontos recebe o valor (PBOT1 usa a sigmoide). */
fnf_judge_t jdg_hit(judgement_t *j, float desvio_ms, int *pontos);

/* Nota perdida (passou da janela). */
void  jdg_miss(judgement_t *j);

/* Nota longa segurada: 'segundos' de hold acumulado. */
void  jdg_hold(judgement_t *j, float segundos);

/* Soltou a nota longa antes do fim. */
void  jdg_hold_drop(judgement_t *j, float segundos_segurados);

/* Pontos que uma nota receberia, sem alterar estado (para o teste). */
int   jdg_pontos(const judgement_t *j, float desvio_ms);
fnf_judge_t jdg_classificar(const judgement_t *j, float desvio_ms);

/* (sick+good-missed)/total, clamp 0..1 -- Scoring.tallyCompletion */
double jdg_completude(const judgement_t *j);
fnf_rank_t jdg_rank(const judgement_t *j);
const char *jdg_rank_nome(fnf_rank_t r);
const char *jdg_nome(fnf_judge_t j);
/* Acertos processados / total: usado pela tela de resultado. */
float jdg_acerto(const judgement_t *j);

#ifdef __cplusplus
}
#endif
#endif
