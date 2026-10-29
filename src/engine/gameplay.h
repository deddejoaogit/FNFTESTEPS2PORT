#ifndef FNF_GAMEPLAY_H
#define FNF_GAMEPLAY_H

/*
 * Núcleo de gameplay: julgamento, pontuação, vida, combo e resultado.
 *
 * Este módulo é a tradução literal de três lugares do original
 * (FunkinCrew/Funkin, tag v0.8.6):
 *
 *   source/funkin/play/scoring/Scoring.hx     -> nota, julgamento, pontos
 *   source/funkin/util/Constants.hx           -> vida, combo, limites de rank
 *   source/funkin/play/PlayState.hx           -> applyScore(), a ordem das
 *                                                coisas: tally, vida, combo,
 *                                                e só então o placar
 *
 * Por que um módulo separado e não mais código no laço: o laço do main.c já
 * tem dois mil parágrafos de desenho, e o que se testa aqui é aritmética pura.
 * Isolado assim, cada número do original tem um teste que falha se alguém o
 * mexer por acidente, e o mesmo código serve ao HUD, ao resultado e ao harness.
 *
 * Ele não sabe o que é um console, um quadro ou um arquivo: só recebe
 * "quantos milissegundos de erro" numa batida e devolve o julgamento.
 */

#include <tamtypes.h>

/* Qual sistema de pontuação. O PlayState de 0.8.6 usa PBOT1 fixo em
 * goodNoteHit() -- não é configuração do jogador --, e é esse o padrão aqui.
 * LEGACY e WEEK7 existem porque o original os carrega junto e porque o port
 * pode querer conferir um chart contra eles; nenhum caminho os usa hoje. */
typedef enum {
    FNF_SCORING_PBOT1 = 0,
    FNF_SCORING_LEGACY = 1,
    FNF_SCORING_WEEK7 = 2
} fnf_scoring_t;

/* Ordem crescente de qualidade, para comparação e para o placar. MISS é o
 * único que também quebra o combo junto com BAD e SHIT -- ver
 * fnf_judge_breaks_combo(). */
typedef enum {
    FNF_JUDGE_MISS = 0,
    FNF_JUDGE_SHIT = 1,
    FNF_JUDGE_BAD = 2,
    FNF_JUDGE_GOOD = 3,
    FNF_JUDGE_SICK = 4,
    FNF_JUDGE_KILLER = 5
} fnf_judge_t;

/* Ranks, na ordem de Scoring.hx (ScoringRank). */
typedef enum {
    FNF_RANK_NONE = -1,
    FNF_RANK_SHIT = 0,
    FNF_RANK_GOOD = 1,
    FNF_RANK_GREAT = 2,
    FNF_RANK_EXCELLENT = 3,
    FNF_RANK_PERFECT = 4,
    FNF_RANK_PERFECT_GOLD = 5
} fnf_rank_t;

/* ---- números do original, um por um -------------------------------- */

/* Scoring.PBOT1_* */
#define FNF_PBOT1_MAX_SCORE        500
#define FNF_PBOT1_SCORING_OFFSET   54.99f
#define FNF_PBOT1_SCORING_SLOPE    0.080f
#define FNF_PBOT1_MIN_SCORE        9.0f
#define FNF_PBOT1_MISS_SCORE       (-100)
#define FNF_PBOT1_PERFECT_THRESHOLD 5.0f
#define FNF_PBOT1_MISS_THRESHOLD   160.0f
#define FNF_PBOT1_KILLER_THRESHOLD 12.5f
#define FNF_PBOT1_SICK_THRESHOLD   45.0f
#define FNF_PBOT1_GOOD_THRESHOLD   90.0f
#define FNF_PBOT1_BAD_THRESHOLD    135.0f
#define FNF_PBOT1_SHIT_THRESHOLD   160.0f

/*
 * Constants.HIT_WINDOW_MS. E' ESTA a janela do jogo, e ela vale para as duas
 * pontas do mesmo numero:
 *
 *   GRhythmUtil.getHitWindow()  start = strumTime - HIT_WINDOW_MS,
 *                               center = strumTime,
 *                               end   = strumTime + HIT_WINDOW_MS
 *   GRhythmUtil.processWindow() songPosition > end  ->  hasMissed = true
 *
 * Ou seja: um aperto so' acerta dentro de +-160 ms, e a nota so' e' dada como
 * perdida DEPOIS dos mesmos +-160 ms. O port usava 150 ms para acertar
 * (TUTORIAL_NOTE_JUDGE_WINDOW = 7200 amostras) e 250 ms para perder
 * (TUTORIAL_NOTE_LATE = 12000), dois numeros que nao existem no original.
 */
#define FNF_HIT_WINDOW_MS 160.0f

/* Scoring.LEGACY_* e Scoring.WEEK7_* -- a janela de 10 quadros a 60 fps. */
#define FNF_LEGACY_HIT_WINDOW      (10.0f/60.0f*1000.0f)  /* 166,666... ms */
#define FNF_LEGACY_SICK_THRESHOLD  0.2f
#define FNF_LEGACY_GOOD_THRESHOLD  0.75f
#define FNF_LEGACY_BAD_THRESHOLD   0.9f
#define FNF_LEGACY_SHIT_THRESHOLD  1.0f
#define FNF_LEGACY_SICK_SCORE      350
#define FNF_LEGACY_GOOD_SCORE      200
#define FNF_LEGACY_BAD_SCORE       100
#define FNF_LEGACY_SHIT_SCORE      50
#define FNF_LEGACY_MISS_SCORE      (-10)
#define FNF_WEEK7_SICK_THRESHOLD   0.2f
#define FNF_WEEK7_GOOD_THRESHOLD   0.55f
#define FNF_WEEK7_BAD_THRESHOLD    0.8f

/* Constants.HEALTH_* -- todos múltiplos de HEALTH_MAX. */
#define FNF_HEALTH_MAX             2.0f
#define FNF_HEALTH_STARTING        1.0f     /* HEALTH_MAX / 2 */
#define FNF_HEALTH_MIN             0.0f
#define FNF_HEALTH_KILLER_BONUS    (2.0f/100.0f*FNF_HEALTH_MAX)   /* +0,04 */
#define FNF_HEALTH_SICK_BONUS      (1.5f/100.0f*FNF_HEALTH_MAX)   /* +0,03 */
#define FNF_HEALTH_GOOD_BONUS      (0.75f/100.0f*FNF_HEALTH_MAX)  /* +0,015 */
#define FNF_HEALTH_BAD_BONUS       (0.0f/100.0f*FNF_HEALTH_MAX)
#define FNF_HEALTH_SHIT_BONUS      (-1.0f/100.0f*FNF_HEALTH_MAX)  /* -0,02 */
#define FNF_HEALTH_HOLD_BONUS_PER_SECOND (6.0f/100.0f*FNF_HEALTH_MAX) /* +0,12 */
#define FNF_HEALTH_MISS_PENALTY    (-4.0f/100.0f*FNF_HEALTH_MAX)  /* -0,08 */
#define FNF_HEALTH_GHOST_MISS_PENALTY (-4.0f/100.0f*FNF_HEALTH_MAX)

/* Constants.SCORE_* */
#define FNF_SCORE_GHOST_MISS_PENALTY (-10)
#define FNF_SCORE_HOLD_BONUS_PER_SECOND 250.0f

/* Constants.RANK_*_THRESHOLD */
#define FNF_RANK_PERFECT_THRESHOLD   1.00f
#define FNF_RANK_EXCELLENT_THRESHOLD 0.90f
#define FNF_RANK_GREAT_THRESHOLD     0.80f
#define FNF_RANK_GOOD_THRESHOLD      0.60f

/* Constants.STEPS_PER_BEAT, usado pela contagem regressiva e pelo bop. */
#define FNF_STEPS_PER_BEAT 4

/* ---- consultas puras ------------------------------------------------ */

/* Janela de acerto, em ms, do sistema. PBOT1 devolve a sua SHIT (160 ms). */
float fnf_hit_window_ms(fnf_scoring_t system);

/* Julga um erro de tempo (ms; sinal ignorado, como no original, que usa
 * Math.abs). Fora da janela devolve FNF_JUDGE_MISS. */
fnf_judge_t fnf_judge_ms(float ms, fnf_scoring_t system);

/* Pontos de uma nota, na tabela do sistema. Fora da janela, o valor de erro. */
int fnf_score_ms(float ms, fnf_scoring_t system);

/* Pontos de uma nota perdida. */
int fnf_miss_score(fnf_scoring_t system);

/* Varição de vida do julgamento (Constants.HEALTH_*). */
float fnf_judge_health(fnf_judge_t judge);

/* 1 quando o julgamento quebra o combo (BAD, SHIT e MISS). */
int fnf_judge_breaks_combo(fnf_judge_t judge);

const char *fnf_judge_name(fnf_judge_t judge);
const char *fnf_rank_name(fnf_rank_t rank);

/* ---- estado de uma tentativa --------------------------------------- */

/*
 * Espelha SaveScoreTallyData mais a vida e o placar correntes. `total_notes`
 * é quantas notas pontuáveis o chart tem: sem ele não existe percentual de
 * conclusão nem rank, e é exatamente o número que o original guarda em
 * `tallies.totalNotes` ao terminar a música.
 */
typedef struct {
    fnf_scoring_t system;
    float health;
    int score;
    int combo;
    int max_combo;
    int sick, good, bad, shit, missed;
    int total_notes_hit;
    int total_notes;
    /* Quantas notas contam para o rank: as notas "scoreable" do chart. Fica
       separado de total_notes porque o chamador pode querer mostrar o total do
       chart antes da primeira nota e ainda assim jogar. */
    int dead;   /* 1 quando a vida chegou a HEALTH_MIN: Game Over */
} fnf_play_t;

void fnf_play_reset(fnf_play_t *play, fnf_scoring_t system, int total_notes);

/* Acerto. `ms` é o erro de tempo (songPosition - noteTime, como no original).
 * Devolve o julgamento aplicado. Não pontua se o julgamento for MISS -- um
 * aperto fora da janela é ghost miss, e o chamador decide chamando
 * fnf_play_ghost() --, o que mantém a mesma separação do PlayState. */
fnf_judge_t fnf_play_hit(fnf_play_t *play, float ms);

/* Nota perdida por deixar a janela passar. */
void fnf_play_miss(fnf_play_t *play);

/* Aperto sem nota por perto: health -0,08 e score -10, sem mexer no tally de
 * julgamentos nem no combo (é o ghostNoteMiss do original). */
void fnf_play_ghost(fnf_play_t *play);

/* Trecho de hold mantido, em segundos: +0,12 de vida e +250 de placar por
 * segundo. Chamar por quadro com o delta de tempo real. */
void fnf_play_hold(fnf_play_t *play, float seconds);

/* Aperto de nota que existe mas ainda está longe: não conta como acerto nem
 * como nota perdida; só a vida e a nota continuam andando. O original trata
 * isso dentro de processWindow(), e é o chamador que sabe a distância. */
int fnf_play_is_pressable(float ms, fnf_scoring_t system);

/* Percentual de conclusão do ResultState: clamp((sick+good-missed)/total,0,1). */
float fnf_play_completion(const fnf_play_t *play);

/* O número que o ResultState mostra, já em porcentagem inteira. */
int fnf_play_clear_percent(const fnf_play_t *play);

/* Rank do fim da música. FNF_RANK_NONE quando não houve nota nenhuma. */
fnf_rank_t fnf_play_rank(const fnf_play_t *play);

/* Vida como fração 0..1 da barra: health/HEALTH_MAX. */
float fnf_play_health_ratio(const fnf_play_t *play);

#endif
