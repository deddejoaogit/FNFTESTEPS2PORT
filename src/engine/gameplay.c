/*
 * Núcleo de gameplay -- ver gameplay.h para a origem de cada número.
 *
 * Uma nota sobre o arredondamento: o original é Haxe/OpenFL e roda em double;
 * aqui tudo é float de 32 bits, que é o que o EE faz bem. A diferença só
 * aparece no último dígito dos pontos de PBOT1 (a sigmóide), nunca no
 * julgamento: os limites comparam contra constantes redondas e um float de
 * 32 bits representa 160,0 e 45,0 exatamente.
 */

#include <math.h>
#include "gameplay.h"

float fnf_hit_window_ms(fnf_scoring_t system)
{
    switch (system) {
        case FNF_SCORING_LEGACY: return FNF_LEGACY_HIT_WINDOW;
        case FNF_SCORING_WEEK7:  return FNF_LEGACY_HIT_WINDOW; /* WEEK7_HIT_WINDOW = LEGACY */
        case FNF_SCORING_PBOT1:
        default:                 return FNF_PBOT1_SHIT_THRESHOLD;
    }
}

int fnf_play_is_pressable(float ms, fnf_scoring_t system)
{
    float a = ms < 0.0f ? -ms : ms;
    return a <= fnf_hit_window_ms(system);
}

fnf_judge_t fnf_judge_ms(float ms, fnf_scoring_t system)
{
    float a = ms < 0.0f ? -ms : ms;

    switch (system) {
        case FNF_SCORING_LEGACY: {
            float w = FNF_LEGACY_HIT_WINDOW;
            if (a <= w * FNF_LEGACY_SICK_THRESHOLD) return FNF_JUDGE_SICK;
            if (a <= w * FNF_LEGACY_GOOD_THRESHOLD) return FNF_JUDGE_GOOD;
            if (a <= w * FNF_LEGACY_BAD_THRESHOLD)  return FNF_JUDGE_BAD;
            if (a <= w * FNF_LEGACY_SHIT_THRESHOLD) return FNF_JUDGE_SHIT;
            return FNF_JUDGE_MISS;
        }
        case FNF_SCORING_WEEK7: {
            float w = FNF_LEGACY_HIT_WINDOW;
            if (a <= w * FNF_WEEK7_SICK_THRESHOLD) return FNF_JUDGE_SICK;
            if (a <= w * FNF_WEEK7_GOOD_THRESHOLD) return FNF_JUDGE_GOOD;
            if (a <= w * FNF_WEEK7_BAD_THRESHOLD)  return FNF_JUDGE_BAD;
            if (a <= w)                            return FNF_JUDGE_SHIT;
            return FNF_JUDGE_MISS;
        }
        case FNF_SCORING_PBOT1:
        default:
            /* O limiar KILLER existe em Scoring.hx mas está comentado: o
               julgamento devolvido nunca é 'killer'. Fica de fora de
               propósito (a epígrafe de FNF_PBOT1_KILLER_THRESHOLD é só
               informativa). */
            if (a <= FNF_PBOT1_SICK_THRESHOLD) return FNF_JUDGE_SICK;
            if (a <= FNF_PBOT1_GOOD_THRESHOLD) return FNF_JUDGE_GOOD;
            if (a <= FNF_PBOT1_BAD_THRESHOLD)  return FNF_JUDGE_BAD;
            if (a <= FNF_PBOT1_SHIT_THRESHOLD) return FNF_JUDGE_SHIT;
            return FNF_JUDGE_MISS;
    }
}

int fnf_score_ms(float ms, fnf_scoring_t system)
{
    float a = ms < 0.0f ? -ms : ms;

    switch (system) {
        case FNF_SCORING_LEGACY: {
            float w = FNF_LEGACY_HIT_WINDOW;
            if (a < w * FNF_LEGACY_SICK_THRESHOLD) return FNF_LEGACY_SICK_SCORE;
            if (a < w * FNF_LEGACY_GOOD_THRESHOLD) return FNF_LEGACY_GOOD_SCORE;
            if (a < w * FNF_LEGACY_BAD_THRESHOLD)  return FNF_LEGACY_BAD_SCORE;
            if (a < w * FNF_LEGACY_SHIT_THRESHOLD) return FNF_LEGACY_SHIT_SCORE;
            return 0;
        }
        case FNF_SCORING_WEEK7: {
            float w = FNF_LEGACY_HIT_WINDOW;
            if (a < w * FNF_WEEK7_SICK_THRESHOLD) return FNF_LEGACY_SICK_SCORE;
            if (a < w * FNF_WEEK7_GOOD_THRESHOLD) return FNF_LEGACY_GOOD_SCORE;
            if (a < w * FNF_WEEK7_BAD_THRESHOLD)  return FNF_LEGACY_BAD_SCORE;
            if (a < w)                            return FNF_LEGACY_SHIT_SCORE;
            return 0;
        }
        case FNF_SCORING_PBOT1:
        default: {
            float factor;
            int score;
            if (a > FNF_PBOT1_MISS_THRESHOLD) return FNF_PBOT1_MISS_SCORE;
            if (a < FNF_PBOT1_PERFECT_THRESHOLD) return FNF_PBOT1_MAX_SCORE;
            factor = 1.0f - (1.0f / (1.0f + (float)exp(-FNF_PBOT1_SCORING_SLOPE *
                                                        (a - FNF_PBOT1_SCORING_OFFSET))));
            score = (int)((float)FNF_PBOT1_MAX_SCORE * factor + FNF_PBOT1_MIN_SCORE);
            return score;
        }
    }
}

int fnf_miss_score(fnf_scoring_t system)
{
    switch (system) {
        case FNF_SCORING_LEGACY: return FNF_LEGACY_MISS_SCORE;
        case FNF_SCORING_WEEK7:  return FNF_LEGACY_MISS_SCORE; /* WEEK7_MISS_SCORE = -10 */
        case FNF_SCORING_PBOT1:
        default:                 return FNF_PBOT1_MISS_SCORE;
    }
}

float fnf_judge_health(fnf_judge_t judge)
{
    switch (judge) {
        case FNF_JUDGE_KILLER: return FNF_HEALTH_KILLER_BONUS;
        case FNF_JUDGE_SICK:   return FNF_HEALTH_SICK_BONUS;
        case FNF_JUDGE_GOOD:   return FNF_HEALTH_GOOD_BONUS;
        case FNF_JUDGE_BAD:    return FNF_HEALTH_BAD_BONUS;
        case FNF_JUDGE_SHIT:   return FNF_HEALTH_SHIT_BONUS;
        case FNF_JUDGE_MISS:
        default:               return FNF_HEALTH_MISS_PENALTY;
    }
}

int fnf_judge_breaks_combo(fnf_judge_t judge)
{
    /* Constants.JUDGEMENT_*_COMBO_BREAK: SICK e GOOD não quebram, BAD e SHIT
       quebram. MISS quebra por definição (o original chama applyScore com
       isComboBreak=true em onNoteMiss). */
    return judge == FNF_JUDGE_BAD || judge == FNF_JUDGE_SHIT || judge == FNF_JUDGE_MISS;
}

const char *fnf_judge_name(fnf_judge_t judge)
{
    switch (judge) {
        case FNF_JUDGE_KILLER: return "killer";
        case FNF_JUDGE_SICK:   return "sick";
        case FNF_JUDGE_GOOD:   return "good";
        case FNF_JUDGE_BAD:    return "bad";
        case FNF_JUDGE_SHIT:   return "shit";
        case FNF_JUDGE_MISS:
        default:               return "miss";
    }
}

const char *fnf_rank_name(fnf_rank_t rank)
{
    switch (rank) {
        case FNF_RANK_PERFECT_GOLD: return "PERFECT GOLD";
        case FNF_RANK_PERFECT:      return "PERFECT";
        case FNF_RANK_EXCELLENT:    return "EXCELLENT";
        case FNF_RANK_GREAT:        return "GREAT";
        case FNF_RANK_GOOD:         return "GOOD";
        case FNF_RANK_SHIT:         return "SHIT";
        case FNF_RANK_NONE:
        default:                    return "NONE";
    }
}

void fnf_play_reset(fnf_play_t *play, fnf_scoring_t system, int total_notes)
{
    if (!play) return;
    play->system = system;
    play->health = FNF_HEALTH_STARTING;
    play->score = 0;
    play->combo = 0;
    play->max_combo = 0;
    play->sick = play->good = play->bad = play->shit = play->missed = 0;
    play->total_notes_hit = 0;
    play->total_notes = total_notes < 0 ? 0 : total_notes;
    play->dead = 0;
}

static void play_apply(fnf_play_t *play, fnf_judge_t judge, int score)
{
    /* PlayState.applyScore(), na mesma ordem. */
    switch (judge) {
        case FNF_JUDGE_SICK:   play->sick++;   break;
        case FNF_JUDGE_GOOD:   play->good++;   break;
        case FNF_JUDGE_BAD:    play->bad++;    break;
        case FNF_JUDGE_SHIT:   play->shit++;   break;
        case FNF_JUDGE_MISS:   play->missed++; break;
        default: break;
    }

    play->health += fnf_judge_health(judge);
    if (play->health > FNF_HEALTH_MAX) play->health = FNF_HEALTH_MAX;
    if (play->health < FNF_HEALTH_MIN) play->health = FNF_HEALTH_MIN;
    if (play->health <= FNF_HEALTH_MIN) play->dead = 1;

    if (fnf_judge_breaks_combo(judge)) {
        play->combo = 0;
    } else {
        play->combo++;
        if (play->combo > play->max_combo) play->max_combo = play->combo;
    }

    /* songScore NAO e' limitado no original: PlayState.applyScore() faz
       `songScore += score` e nada mais, e updateScoreText() mostra
       formatMoney(songScore). Com PBOT1_MISS_SCORE = -100 e
       SCORE_GHOST_MISS_PENALTY = -10, o placar passa de zero para baixo numa
       passagem ruim -- e o jogo original mostra isso. Limitar em zero aqui era
       invencao minha, e invencao e' divergencia. Medido no harness: o placar
       chegou a zero e ficou la', escondendo centenas de penalidades reais. */
    play->score += score;
}

fnf_judge_t fnf_play_hit(fnf_play_t *play, float ms)
{
    fnf_judge_t judge;
    int score;

    if (!play) return FNF_JUDGE_MISS;
    judge = fnf_judge_ms(ms, play->system);
    if (judge == FNF_JUDGE_MISS) return judge;   /* aperto longe: ghost miss */

    score = fnf_score_ms(ms, play->system);
    play->total_notes_hit++;
    play_apply(play, judge, score);
    return judge;
}

void fnf_play_miss(fnf_play_t *play)
{
    if (!play) return;
    play_apply(play, FNF_JUDGE_MISS, fnf_miss_score(play->system));
}

void fnf_play_ghost(fnf_play_t *play)
{
    if (!play) return;
    /* ghostNoteMiss(): health += healthChange; songScore += scoreChange; e
       nada mais -- sem tally, sem combo. Os dois valores vem de
       Constants.HEALTH_GHOST_MISS_PENALTY e SCORE_GHOST_MISS_PENALTY. */
    play->health += FNF_HEALTH_GHOST_MISS_PENALTY;
    if (play->health < FNF_HEALTH_MIN) play->health = FNF_HEALTH_MIN;
    if (play->health <= FNF_HEALTH_MIN) play->dead = 1;
    play->score += FNF_SCORE_GHOST_MISS_PENALTY;
}

void fnf_play_hold(fnf_play_t *play, float seconds)
{
    if (!play || seconds <= 0.0f) return;
    play->health += FNF_HEALTH_HOLD_BONUS_PER_SECOND * seconds;
    if (play->health > FNF_HEALTH_MAX) play->health = FNF_HEALTH_MAX;
    play->score += (int)(FNF_SCORE_HOLD_BONUS_PER_SECOND * seconds);
}

float fnf_play_completion(const fnf_play_t *play)
{
    float v;
    if (!play || play->total_notes <= 0) return 0.0f;
    v = (float)(play->sick + play->good - play->missed) / (float)play->total_notes;
    if (v < 0.0f) v = 0.0f;
    if (v > 1.0f) v = 1.0f;
    return v;
}

int fnf_play_clear_percent(const fnf_play_t *play)
{
    /* ResultState: floor(tallyCompletion * 100). */
    return (int)(fnf_play_completion(play) * 100.0f);
}

fnf_rank_t fnf_play_rank(const fnf_play_t *play)
{
    float completion;

    if (!play || play->total_notes <= 0) return FNF_RANK_NONE;
    if (play->sick == play->total_notes) return FNF_RANK_PERFECT_GOLD;

    completion = fnf_play_completion(play);
    if (completion == FNF_RANK_PERFECT_THRESHOLD) return FNF_RANK_PERFECT;
    if (completion >= FNF_RANK_EXCELLENT_THRESHOLD) return FNF_RANK_EXCELLENT;
    if (completion >= FNF_RANK_GREAT_THRESHOLD) return FNF_RANK_GREAT;
    if (completion >= FNF_RANK_GOOD_THRESHOLD) return FNF_RANK_GOOD;
    return FNF_RANK_SHIT;
}

float fnf_play_health_ratio(const fnf_play_t *play)
{
    if (!play) return 0.0f;
    if (play->health <= FNF_HEALTH_MIN) return 0.0f;
    if (play->health >= FNF_HEALTH_MAX) return 1.0f;
    return play->health / FNF_HEALTH_MAX;
}
