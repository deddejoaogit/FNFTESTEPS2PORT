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
