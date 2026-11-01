#include "resultado.h"

#include <math.h>
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

/* ---- OS CONTADORES DE JULGAMENTO -----------------------------------------
 *
 * `TallyCounter.hx` L42-77 monta o numero a partir de `Math.round(curNumber)`
 * e posiciona os digitos a cada `43 * scale.x` px, do MAIS significativo para
 * o menos. `update()` (L36-40) so' redesenha enquanto `curNumber < needed`.
 * O tween e' `FlxEase.quartOut`: 1 - (1 - t)^3.
 */

int res_tally_glifo(int d)
{
    if (d < 0) d = 0;
    if (d > 9) d = 9;
    return d;
}

int res_tally_digitos(int n)
{
    int k = 1;
    if (n < 0) n = 0;
    while (n >= 10) { n /= 10; k++; }
    return k;
}

int res_tally_visivel(int i, unsigned long ms)
{
    unsigned long ini;
    if (i < 0 || i >= (int)RES_TALLIES) return 0;
    /* `new FlxTimer().start((0.3 * ind) + 1.20, ...)` (ResultState L481-488). */
    ini = (unsigned long)g_res_tallies.atraso_base_ms
          + (unsigned long)g_res_tallies.atraso_passo_ms * (unsigned long)i;
    return (ms >= ini) ? 1 : 0;
}

int res_tally_valor(int alvo, int i, unsigned long ms)
{
    unsigned long ini, dt;
    float t, e;
    if (alvo < 0) alvo = 0;
    if (i < 0 || i >= (int)RES_TALLIES) return alvo;
    ini = (unsigned long)g_res_tallies.atraso_base_ms
          + (unsigned long)g_res_tallies.atraso_passo_ms * (unsigned long)i;
    if (ms <= ini) return 0;
    dt = ms - ini;
    if (dt >= (unsigned long)g_res_tallies.tween_ms) return alvo;
    t = (float)dt / (float)g_res_tallies.tween_ms;
    e = 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);   /* quartOut */
    return (int)((float)alvo * e + 0.5f);               /* Math.round */
}

float res_tally_ajuste_x(int i, int total_hit)
{
    if (i != 0 && i != 1) return 0.0f;      /* so' totalNotesHit e maxCombo */
    if (total_hit < 1000) return 0.0f;
    return -(float)g_res_tallies.ajuste_1000;
}

/* ---- O PERCENTUAL DE CONCLUSAO -------------------------------------------
 *
 * O RELOGIO (`ResultState.hx` L577-646):
 *
 *   37/24 s           o contador grande nasce em (830, 290) com
 *                     `curNumber = max(0, alvo - 36)`
 *   58/24 s           `FlxTween.tween(..., {curNumber: alvo}, 58/24, quartOut)`
 *   +0,40 s           `flash(true)` -> 0,4 s -> `flash(false)`
 *   +0,25 s           `FlxTween.tween(..., {alpha: 0}, 0,5, {startDelay: 0,5})`
 *   +1,00 s           `remove(clearPercentCounter)` -- nao desenha mais
 *
 * `Math.round` no desenho (`drawNumbers`), e o valor do tween acompanha
 * `quartOut`: 1 - (1 - t)^3.
 */

/* 24 avos -> ms, arredondando para CIMA: o FlxTimer so' dispara quando o
 * tempo passa (1542 ms para 37/24 s, e nao 1541). O placar usa a mesma regra. */
static unsigned long res_cp_q_ms(unsigned long q)
{
    return (q * 1000ul + (unsigned long)g_res_score.fps - 1ul)
           / (unsigned long)g_res_score.fps;
}

static unsigned long res_cp_inicio_ms(void)
{
    return res_cp_q_ms((unsigned long)g_res_cp_geo.inicio_q);
}

static unsigned long res_cp_tween_ms(void)
{
    return res_cp_q_ms((unsigned long)g_res_cp_geo.tween_q);
}

int res_cp_digitos(int alvo)
{
    int k = 1;
    if (alvo < 0) alvo = 0;
    while (alvo >= 10) { alvo /= 10; k++; }
    return k;
}

int res_cp_deslocamento(int digitos)
{
    if (digitos == 1) return 1;
    if (digitos == 3) return -1;
    return 0;
}

int res_cp_variante(int digitos, int ind)
{
    /* `var variant = (seperatedScore.length == 3) ? (digitIndex >= 2)
                                                   : (digitIndex >= 1);`
       com `digitIndex = ind + 1`. */
    if (digitos == 3) return (ind + 1 >= 2) ? 1 : 0;
    return (ind + 1 >= 1) ? 1 : 0;
}

int res_cp_valor(int alvo, unsigned long ms)
{
    unsigned long ini = res_cp_inicio_ms(), tw = res_cp_tween_ms(), dt;
    int inicio_valor, i;
    float t, e;
    if (alvo < 0) alvo = 0;
    if (ms <= ini) {
        /* Antes de o tween comecar o contador mostra o valor de partida. */
        int v = alvo - (int)g_res_cp_geo.abaixo;
        return (v > 0) ? v : 0;
    }
    dt = ms - ini;
    if (dt >= tw) return alvo;
    inicio_valor = alvo - (int)g_res_cp_geo.abaixo;
    if (inicio_valor < 0) inicio_valor = 0;
    t = (float)dt / (float)tw;
    e = 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);
    i = inicio_valor + (int)(((float)alvo - (float)inicio_valor) * e + 0.5f);
    if (i > alvo) i = alvo;
    return i;
}

/* Quando o fade do alpha termina: inicio + tween + flash + espera + atraso +
 * a propria duracao do fade. */
static unsigned long res_cp_fim_ms(void)
{
    return res_cp_inicio_ms() + res_cp_tween_ms()
           + (unsigned long)g_res_cp_geo.flash_ms
           + (unsigned long)g_res_cp_geo.fade_espera_ms
           + (unsigned long)g_res_cp_geo.fade_atraso_ms
           + (unsigned long)g_res_cp_geo.fade_ms;
}

int res_cp_visivel(unsigned long ms)
{
    return (ms >= res_cp_inicio_ms() && ms < res_cp_fim_ms()) ? 1 : 0;
}

int res_cp_alpha(unsigned long ms)
{
    unsigned long ini;
    if (!res_cp_visivel(ms)) return 0;
    ini = res_cp_inicio_ms() + res_cp_tween_ms()
          + (unsigned long)g_res_cp_geo.flash_ms
          + (unsigned long)g_res_cp_geo.fade_espera_ms
          + (unsigned long)g_res_cp_geo.fade_atraso_ms;
    if (ms <= ini) return 255;
    if (!g_res_cp_geo.fade_ms) return 0;
    {
        unsigned long dt = ms - ini;
        unsigned long d = (unsigned long)g_res_cp_geo.fade_ms;
        if (dt >= d) return 0;
        return (int)(255ul - (255ul * dt) / d);
    }
}

int res_cp_flash(unsigned long ms)
{
    unsigned long ini = res_cp_inicio_ms() + res_cp_tween_ms();
    return (ms >= ini && ms < ini + (unsigned long)g_res_cp_geo.flash_ms) ? 1 : 0;
}

int res_cp_small_visivel(int rank, unsigned long ms)
{
    const res_rank_t *r = res_rank(rank);
    if (!r) return 0;
    return ms >= (unsigned long)r->bf_ms ? 1 : 0;
}

int res_cp_small_flash(int rank, unsigned long ms)
{
    const res_rank_t *r = res_rank(rank);
    if (!r || !res_cp_small_visivel(rank, ms)) return 0;
    /* `clearPercentSmall.flash(true)` seguido de `new FlxTimer().start(0.4,
       flash(false))`. O flash do original e' `PureColor` branco (o mesmo
       caso pendente do contador grande): a JANELA esta' aqui, a cor depende de
       TFX = DECAL no GS. */
    return (ms - (unsigned long)r->bf_ms) < 400ul ? 1 : 0;
}

float res_cp_small_x(float dif_w)
{
    /* `(difficulty.x + difficulty.width) + 60` em 1280x720, pela metade:
       (555 + largura + 60) / 2 = small_fixo_x + largura/2. `dif_w` ja' vem em
       640x360 (a largura que o port carregou), e small_fixo_x e' (555+60)/2. */
    return g_res_cp_geo.small_fixo_x + dif_w;
}

/* ---- A BARRA DO TOPO E A DIFICULDADE (ResultState.hx L342-360, L736-750) --

   Dois tweens, ambos com o MESMO arredondamento para CIMA que o percentual usa
   (`res_cp_q_ms`): o instante de q/24 s e' o teto da divisao, e a duracao e' a
   diferenca entre os dois extremos (10/24 - 3/24 = 417 - 125 = 292 ms, e nao
   291,6 truncado). E' o mesmo criterio que fez o teste do
   percentual passar: a comparacao e' "ja' chegou?", e uma janela de 1 ms a
   menos faz o quadro de chegada ser o anterior. */

static unsigned long res_barra_q_ms(unsigned long q)
{
    return (q * 1000ul + 23ul) / 24ul;
}

float res_barra_y(unsigned long ms)
{
    const unsigned long ini = res_barra_q_ms((unsigned long)g_res_barra.atraso_q);
    const unsigned long fim = res_barra_q_ms((unsigned long)g_res_barra.atraso_q
                                             + (unsigned long)g_res_barra.tween_q);
    float t, e;
    if (ms <= ini) return -g_res_barra.altura;
    if (ms >= fim) return 0.0f;
    t = (float)(ms - ini) / (float)(fim - ini);
    e = 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);   /* FlxEase.quartOut */
    return -g_res_barra.altura * (1.0f - e);
}

int res_barra_assentada(unsigned long ms)
{
    return ms >= res_barra_q_ms((unsigned long)g_res_barra.atraso_q
                                + (unsigned long)g_res_barra.tween_q) ? 1 : 0;
}

float res_dif_y_desde(float altura, unsigned long desde)
{
    unsigned long ini = (unsigned long)g_res_dif_geo.atraso_ms;
    unsigned long fim = ini + (unsigned long)g_res_dif_geo.tween_ms;
    float t, e;
    if (desde <= ini) return -altura;
    if (desde >= fim) return g_res_dif_geo.y_alvo;
    t = (float)(desde - ini) / (float)(fim - ini);
    /* FlxEase.expoOut: 1 - 2^(-10t). Em t = 1 vale 1 (2^-10 nao e' zero). */
    e = 1.0f - powf(2.0f, -10.0f * t);
    return -altura + (g_res_dif_geo.y_alvo + altura) * e;
}

float res_dif_y(float altura, unsigned long ms)
{
    return res_dif_y_desde(altura, ms);
}

float res_cp_small_y_desde(float altura, unsigned long desde)
{
    unsigned long ini = (unsigned long)g_res_cp_geo.small_atraso_ms;
    unsigned long fim = ini + (unsigned long)g_res_cp_geo.small_tween_ms;
    float t, e;
    if (desde <= ini) return -altura;
    if (desde >= fim) return g_res_cp_geo.small_y_alvo;
    t = (float)(desde - ini) / (float)(fim - ini);
    e = 1.0f - powf(2.0f, -10.0f * t);   /* FlxEase.expoOut, igual ao original */
    return -altura + (g_res_cp_geo.small_y_alvo + altura) * e;
}

int res_dif_indice(int dificuldade)
{
    /* `diff_${params.difficultyId}`: o port so' tem tres dificuldades
       (0 = easy, 1 = normal, 2 = hard). Os arquivos `diff_erect` e
       `diff_nightmare` ficam empacotados para o dia em que o port tiver essas
       dificuldades -- nao ha' como chegar neles hoje. */
    if (dificuldade < 0 || dificuldade > 2) return -1;
    return dificuldade;
}

/* ---- O NOME DA MUSICA (ResultState.hx L736-836) --------------------------

   O `update()` do original faz, enquanto `movingSongStuff`:

       songName.x += speedOfTween.x * 60 * elapsed;
       songName.y += speedOfTween.y * 60 * elapsed;

   e `speedOfTween` nao nasce pronto: `timerThenSongName()` guarda o valor
   atual, zera e tweena de volta em 0,7 s com `FlxEase.quadIn`, e um
   `FlxTimer` de `timerLength` (1,0 s no create, 3,0 s nos ciclos seguintes)
   liga/desliga o movimento. Ou seja: a velocidade SOBE com o quadrado do
   tempo, e o deslocamento e' a integral disso -- nao uma reta.

   Integral de `quadIn` no tempo (u em segundos desde o inicio do tween,
   T = duracao em segundos, `full` = velocidade final):
       u <= 0      : 0
       0 < u < T   : full * u^3 / (3 T^2)
       u >= T      : full * (T/3 + (u - T))
   Esta funcao devolve o FATOR (sem o `full`), e as duas posicoes usam o mesmo
   fator -- x para a esquerda, y para baixo. */

static float res_nome_fator(float u, float T)
{
    if (u <= 0.0f) return 0.0f;
    if (u < T) return (u * u * u) / (3.0f * T * T);
    return T / 3.0f + (u - T);
}

static unsigned long res_nome_ms_ramp(const res_nome_t *n)
{
    return n->ramp;
}

static float res_nome_y_entrada(const res_nome_t *n, unsigned long ms)
{
    const unsigned long ini = n->t0 + (unsigned long)g_res_nome_geo.entrada_atraso_ms;
    const unsigned long fim = ini + (unsigned long)g_res_nome_geo.entrada_ms;
    float t, e;
    if (ms <= ini) return -n->alt;
    if (ms >= fim) return n->y_alvo;
    t = (float)(ms - ini) / (float)(fim - ini);
    e = (t >= 1.0f) ? 1.0f : 1.0f - powf(2.0f, -10.0f * t);   /* FlxEase.expoOut */
    return -n->alt + (n->y_alvo + n->alt) * e;
}

const res_nome_arte_t *res_nome_do_id(const char *id)
{
    unsigned int i;
    if (!id) return 0;
    for (i = 0; i < (unsigned int)RES_NOMES; i++)
        if (strcmp(g_res_nome[i].id, id) == 0) return &g_res_nome[i];
    return 0;
}

void res_nome_iniciar(res_nome_t *n, unsigned long ms, float x0, float y_alvo,
                      float larg, float alt)
{
    if (!n) return;
    memset(n, 0, sizeof(*n));
    n->ativo = 1;
    n->ciclo = 0;
    n->t0 = ms;
    n->ramp = ms + (unsigned long)g_res_nome_geo.ciclo_espera_primeiro_ms;
    n->x0 = x0;
    n->y_alvo = y_alvo;
    n->larg = larg;
    n->alt = alt;
    n->x = x0;
    n->y = -alt;
}

void res_nome_soltar(res_nome_t *n, unsigned long ms)
{
    if (!n || !n->ativo) return;
    if (!n->move) n->move = ms;
}

static void res_nome_reiniciar(res_nome_t *n, unsigned long ms)
{
    /* `timerThenSongName()` sem argumentos: espera 3,0 s e `autoScroll` = true,
       ou seja, o movimento volta junto com o tween da velocidade. */
    n->ciclo++;
    n->t0 = ms;
    n->ramp = ms + (unsigned long)g_res_nome_geo.ciclo_espera_ms;
    n->move = n->ramp;
    n->x = n->x0;
    n->y = -n->alt;
    n->dx = 0.0f;
    n->dy = 0.0f;
}

void res_nome_passo(res_nome_t *n, unsigned long ms)
{
    const float T = (float)g_res_nome_geo.velocidade_ms / 1000.0f;
    float desloc;
    if (!n || !n->ativo) return;
    /* A partir do 2o ciclo o movimento volta sozinho, junto com a velocidade. */
    if (!n->move && n->ciclo > 0 && ms >= n->ramp) n->move = n->ramp;
    desloc = 0.0f;
    if (n->move && ms > n->move) {
        float u = (float)(ms - res_nome_ms_ramp(n)) / 1000.0f;
        float um = (float)((long)n->move - (long)res_nome_ms_ramp(n)) / 1000.0f;
        desloc = res_nome_fator(u, T) - res_nome_fator(um, T);
    }
    n->dx = g_res_nome_geo.vel_x * desloc;
    n->dy = g_res_nome_geo.vel_y * desloc;
    n->x = n->x0 + n->dx;
    n->y = res_nome_y_entrada(n, ms) + n->dy;
    if (n->x + n->larg < g_res_nome_geo.ciclo_fim_x) res_nome_reiniciar(n, ms);
}

float res_nome_clip_local(const res_nome_t *n)
{
    float c;
    if (!n) return 0.0f;
    c = g_res_nome_geo.clip_x - n->x;
    return c > 0.0f ? c : 0.0f;
}
