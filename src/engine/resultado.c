#include "resultado.h"

#include <stdio.h>
#include <string.h>

void res_init(res_t *r)
{
    int i;
    if (!r) return;
    memset(r, 0, sizeof(*r));
    for (i = 0; i < (int)RES_FOLHAS; i++) r->f[i].pst.fd = -1;
}

int res_carregar(res_t *r, const char *dir)
{
    int i, ok = 0;
    char path[160];

    if (!r || !dir) return 0;
    res_init(r);
    for (i = 0; i < (int)RES_FOLHAS; i++) {
        res_folha_t *f = &r->f[i];
        /* `dir` termina com "/" como as outras pastas de asset do port. */
        snprintf(path, sizeof(path), "%s%s", dir, g_res_folha[i].tex);
        f->tentou = 1;
        if (pst_abrir(path, &f->pst) == PST_OK) {
            f->aberto = 1;
            ok++;
        } else {
            f->pst.fd = -1;
            f->aberto = 0;
        }
    }
    return ok;
}

void res_liberar(res_t *r)
{
    int i;
    if (!r) return;
    for (i = 0; i < (int)RES_FOLHAS; i++) {
        if (r->f[i].aberto) pst_fechar(&r->f[i].pst);
        r->f[i].aberto = 0;
        r->f[i].tentou = 0;
        r->f[i].vistos = 0;
    }
}

int res_quadro(res_t *r, int i, unsigned long ms)
{
    const res_folha_dado_t *d;
    unsigned long dt;
    unsigned long q;

    if (!r || i < 0 || i >= (int)RES_FOLHAS) return -1;
    d = &g_res_folha[i];
    if (!r->f[i].aberto) return -1;
    /* Antes do timer a folha esta' escondida (`visible = false`). */
    if (ms < (unsigned long)d->inicio_ms) return -1;
    dt = ms - (unsigned long)d->inicio_ms;
    q = (dt * (unsigned long)d->fps) / 1000ul;
    /* `looped = false`: para no ultimo quadro e fica na tela. */
    if (q >= (unsigned long)d->quadros) {
        if (!d->looped) return (int)d->quadros - 1;
        q %= (unsigned long)d->quadros;
    }
    return (int)q;
}

int res_pixels(res_t *r, int i, int q, unsigned char *dst)
{
    if (!r || i < 0 || i >= (int)RES_FOLHAS || q < 0 || !dst) return PST_ERR_QUADRO;
    if (!r->f[i].aberto) return PST_ERR_ABRIR;
    if (q >= (int)g_res_folha[i].quadros) return PST_ERR_QUADRO;
    r->f[i].vistos++;
    return pst_quadro(&r->f[i].pst, (unsigned int)q, dst);
}

/* ---- A ARTE DO RANK -------------------------------------------------------
 *
 * O TEMPO VEM DA FONTE (Scoring.hx L568-586 e ResultState.hx L667-700):
 *
 *   flash_ms   `rank.getFlashDelay()` -- 129/24 s no PERFECT, 122/24 no
 *              EXCELLENT, 109/24 no GREAT, 107/24 no GOOD e 186/24 no SHIT.
 *              Conta do instante em que a tela abre (todos os FlxTimer do
 *              create() partem juntos).
 *   piscar     FlxFlicker.flicker(rankTextVert, 2/24*3, 2/24, true): 0,25 s
 *              alternando a cada 2 quadros.
 *   flash      14/24 s de alpha 1 -> 0 (bgFlash).
 *   vertical   para de rolar ate' 30/24 s depois; entao velocity.y = -80 px/s
 *              (em 1280x720, que aqui viram -40).
 *   horizontal 12 linhas em y = 60 + 67,5*i, velocity.x = -7 nas pares e +7 nas
 *              impares (velocidades em 1280x720; aqui a metade).
 *
 * Nada e' escolhido: cada numero tem a linha acima.
 */

int res_rank_indice(int rank)
{
    if (rank < 0 || rank >= (int)RES_RANKS) return 0;   /* cai em SHIT */
    return rank;
}

const res_rank_t *res_rank(int rank)
{
    return &g_res_rank[res_rank_indice(rank)];
}

int res_flash_alpha(const res_rank_t *r, unsigned long ms)
{
    unsigned long dt;
    if (!r || ms < (unsigned long)r->flash_ms || !r->flash_dura_ms) return 0;
    dt = ms - (unsigned long)r->flash_ms;
    if (dt >= (unsigned long)r->flash_dura_ms) return 0;
    return (int)(255ul - (255ul * dt) / (unsigned long)r->flash_dura_ms);
}

int res_rank_pisca(const res_rank_t *r, unsigned long ms)
{
    unsigned long dt, passo;
    if (!r || ms < (unsigned long)r->flash_ms) return 1;
    dt = ms - (unsigned long)r->flash_ms;
    if (dt >= (unsigned long)r->flicker_total_ms) return 1;
    if (!r->flicker_ms) return 1;
    passo = dt / (unsigned long)r->flicker_ms;
    /* `autoToggle` do FlxFlicker: comeca VISIVEL e alterna a cada intervalo. */
    return (int)(passo % 2ul) == 0;
}

float res_rank_scroll_y(const res_rank_t *r, unsigned long ms)
{
    unsigned long ini, dt;
    if (!r) return 0.0f;
    ini = (unsigned long)r->flash_ms + (unsigned long)r->vert_espera_ms;
    if (ms <= ini) return 0.0f;
    dt = ms - ini;
    return r->vert_vel * ((float)dt / 1000.0f);
}

float res_rank_scroll_x(const res_rank_t *r, int linha, unsigned long ms)
{
    unsigned long dt;
    float v;
    if (!r || ms < (unsigned long)r->flash_ms) return 0.0f;
    dt = ms - (unsigned long)r->flash_ms;
    v = (linha % 2 == 0) ? -r->hor_vel : r->hor_vel;
    return v * ((float)dt / 1000.0f);
}

/* ---- O PLACAR -------------------------------------------------------------
 *
 * O RELOGIO E' O DA FONTE. Todos os timers e todas as animacoes andam em 24
 * avos de segundo (24 fps), entao o estado do placar e' calculado em `q`, o
 * numero de 24 avos desde a abertura da tela -- a mesma unidade dos
 * `FlxTimer(1/24)`. Cada numero abaixo tem a linha do `ResultScore.hx`.
 *
 *   37/24 s         score.visible = true; animateNumbers()          (L391-396)
 *   (i-1)/24 s      o slot i comeca a embaralhar                    (L32-40)
 *   41 x 1/24 s     shuffle(): o digito sobe de 1 em 1, 10 -> 0      (L172-180)
 *   23/24 s         FlxTween.num(0 -> finalDigit, quadOut)           (L146-166)
 *   finalDelay/24   e entao a animacao do digito recomeca no quadro 0 (L160-165)
 *
 * `glow`: a PRIMEIRA troca toca a animacao no quadro 0 e apaga o glow; todas
 * as seguintes tocam no quadro 4 -- o ultimo, que `looped = false` segura
 * (L57-76). Por isso o quadro 4 e' o que aparece em quase todo o tempo.
 */

#define RES_SCORE_NOMES_MAX 10

int res_score_digitos(long score)
{
    long lim = 10000000000L;  /* Math.pow(10, group.members.length), 10 slots */
    int n = 0;
    if (score < 0) score = 0;
    if (score > lim - 1) score = lim - 1;
    while (score > 0) { n++; score /= 10; }
    return n;
}

int res_score_final(int slot, long score)
{
    long lim = 10000000000L;
    int k;
    if (slot < 0 || slot >= (int)g_res_score.slots) return g_res_score.digito_disabled;
    if (score < 0) score = 0;
    if (score > lim - 1) score = lim - 1;
    /* Os digitos entram da DIREITA: o slot 9 e' a unidade. */
    for (k = (int)g_res_score.slots - 1; k > slot; k--) score /= 10;
    return (int)(score % 10);
}

int res_score_indice(int digito, int quadro)
{
    int nq = (int)g_res_score.quadros_por_digito;
    if (digito < 0) digito = 0;
    if (quadro < 0) quadro = 0;
    if (digito < (int)g_res_score.digito_disabled) {
        if (quadro > nq - 1) quadro = nq - 1;
        return digito * nq + quadro;
    }
    /* DISABLED e GONE tem UM quadro cada e ficam no fim da tabela. */
    digito = (digito == (int)g_res_score.digito_gone)
             ? (int)g_res_score.digito_gone : (int)g_res_score.digito_disabled;
    return (int)g_res_score.digito_disabled * nq
           + (digito - (int)g_res_score.digito_disabled);
}

/* QuadOut do flixel: `FlxEase.quadOut(t) = -t * (t - 2)` = 2t - t^2. */
static float res_score_quad_out(float t)
{
    if (t <= 0.0f) return 0.0f;
    if (t >= 1.0f) return 1.0f;
    return t * (2.0f - t);
}

int res_score_estado(int slot, long score, unsigned long ms,
                     int *digito, int *quadro)
{
    unsigned long q, ini, k, fim_tween;
    int digitos = res_score_digitos(score);
    int final, d;
    /* 37/24 s em ms: 1541,67 -> 1542. Arredonda para CIMA porque o FlxTimer so'
       dispara quando o tempo passa; o harness cobra o primeiro quadro em 1542. */
    long ini_ms = ((long)g_res_score.inicio_q * 1000L
                   + (long)g_res_score.fps - 1L) / (long)g_res_score.fps;

    if (digito) *digito = (int)g_res_score.digito_disabled;
    if (quadro) *quadro = 0;
    if (slot < 0 || slot >= (int)g_res_score.slots) return 0;
    /* `score.visible = false` ate' o timer de 37/24 s. O ESTADO continua sendo
       calculado abaixo mesmo antes disso: os timers de cada slot partem de
       (i-1)/24 s, ou seja, o shuffle ja' esta' correndo quando a tela abre. O
       valor devolvido diz so' se o placar APARECE. */

    /* 24 avos desde a abertura da tela. */
    q = (ms * (unsigned long)g_res_score.fps) / 1000ul;
    /* Slots a esquerda do primeiro digito ficam em DISABLED (o `while
       (loopNum > 0)` do setter nunca chega neles). */
    if (slot < (int)g_res_score.slots - digitos)
        return ((long)ms >= ini_ms) ? 1 : 0;

    final = res_score_final(slot, score);
    /* (i-1)/24 s; no slot 0 o timer do original nasce negativo e dispara
       imediatamente -- aqui isso vira 0. */
    ini = (slot > 0) ? (unsigned long)(slot - 1) : 0ul;
    if (q < ini) {                             /* ainda em DISABLED */
        return ((long)ms >= ini_ms) ? 1 : 0;
    }
    k = q - ini;
    if (k == 0) {
        /* playAnim() toca DISABLED antes de o shuffle comecar. */
        if (digito) *digito = (int)g_res_score.digito_disabled;
        if (quadro) *quadro = 0;
        return ((long)ms >= ini_ms) ? 1 : 0;
    }
    if (k < (unsigned long)g_res_score.shuffle_q) {
        /* shuffleProgress(): o digito sobe de 1 em 1, 10 -> 0 -> 1 ... */
        d = (int)(k % 10ul);
        if (digito) *digito = d;
        if (quadro) *quadro = (k == 1ul) ? 0 : (int)g_res_score.quadros_por_digito - 1;
        return ((long)ms >= ini_ms) ? 1 : 0;
    }
    fim_tween = (unsigned long)g_res_score.shuffle_q
                + (unsigned long)g_res_score.tween_q;
    if (k < fim_tween) {
        /* O tween cai de 0 ate' `final` com quadOut, e cada passo escolhe o
           digito do chao do valor (L146-152). */
        float p = (float)(k - (unsigned long)g_res_score.shuffle_q)
                  / (float)g_res_score.tween_q;
        float v = (float)final * res_score_quad_out(p);
        if (digito) *digito = (int)v;
        if (quadro) *quadro = (int)g_res_score.quadros_por_digito - 1;
        return ((long)ms >= ini_ms) ? 1 : 0;
    }
    /* Depois do tween: `FlxTimer(finalDelay/24)` e o piscar final no quadro 0
       (L157-165). `finalDelay = scoreStart - (i - 1)`; negativo o timer dispara
       no mesmo quadro, entao aqui vira 0. */
    d = digitos - (slot - 1);
    if (d < 0) d = 0;
    if (digito) *digito = final;
    if (k >= fim_tween + (unsigned long)d) {
        unsigned long f = k - fim_tween - (unsigned long)d;
        int nq = (int)g_res_score.quadros_por_digito;
        if (f > (unsigned long)(nq - 1)) f = (unsigned long)(nq - 1);
        if (quadro) *quadro = (int)f;
    } else {
        if (quadro) *quadro = (int)g_res_score.quadros_por_digito - 1;
    }
    return ((long)ms >= ini_ms) ? 1 : 0;
}

int res_score_assentado(int slot, long score, unsigned long ms)
{
    unsigned long q, ini, fim, d;
    int digitos = res_score_digitos(score);
    if (slot < 0 || slot >= (int)g_res_score.slots) return 0;
    if (slot < (int)g_res_score.slots - digitos) return 0;   /* DISABLED */
    q = (ms * (unsigned long)g_res_score.fps) / 1000ul;
    ini = (slot > 0) ? (unsigned long)(slot - 1) : 0ul;
    fim = ini + (unsigned long)g_res_score.shuffle_q
          + (unsigned long)g_res_score.tween_q;
    /* `finalDelay`, em 24 avos; negativo dispara na hora. */
    {
        long dd = (long)digitos - (long)(slot - 1);
        d = (dd > 0) ? (unsigned long)dd : 0ul;
    }
    /* ... e o ultimo piscar leva mais `quadros_por_digito` 24 avos. */
    fim += d + (unsigned long)g_res_score.quadros_por_digito;
    return (q >= fim) ? 1 : 0;
}
