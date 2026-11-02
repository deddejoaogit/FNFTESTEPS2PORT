/* resultado_rank - ver o cabecalho. Funcoes puras do tempo. */
#include "resultado_rank.h"

/* Os nomes da tabela, na ordem em que o empacotador os escreveu. A ordem vem
 * de `PlayableCharacter.getResultsAnimationDatas` (L102-126) casada com o rank
 * do jogo (`Scoring.hx`: SHIT, GOOD, GREAT, EXCELLENT, PERFECT,
 * PERFECT_GOLD) -- NAO e' a ordem alfabetica do plano. */
static const char *const nomes_rank[RES_RR_RANKS] = {
    "SHIT", "GOOD", "GREAT", "EXCELLENT", "PERFECT", "PERFECT_GOLD"
};

void res_rr_init(res_rr_t *r)
{
    int i;
    if (!r) return;
    for (i = 0; i < RES_RR_MAX_ENTRADAS; i++) {
        r->p[i].aberto = 0;
        r->p[i].erro = 0;
        r->p[i].pm.n = 0;
    }
    r->idx = -1;
}

void res_rr_liberar(res_rr_t *r)
{
    int i;
    if (!r) return;
    for (i = 0; i < RES_RR_MAX_ENTRADAS; i++) {
        if (r->p[i].aberto) pm_fechar(&r->p[i].pm);
        r->p[i].aberto = 0;
        r->p[i].erro = 0;
        r->p[i].pm.n = 0;
    }
    r->idx = -1;
}

int res_rr_carregar(res_rr_t *r, const char *dir, int idx)
{
    int i, ok = 0;
    const res_rr_rank_t *rk;
    if (!r) return 0;
    res_rr_liberar(r);
    rk = res_rr_rank(idx);
    if (!rk) return 0;
    r->idx = idx;
    for (i = 0; i < (int)rk->n && i < RES_RR_MAX_ENTRADAS; i++) {
        const res_rr_entrada_t *e = &rk->entradas[i];
        int er;
        er = pm_abrir(dir, e->partes, (int)e->n_partes, &r->p[i].pm);
        if (er == 0) {
            /* O plano manda quantos quadros: um pacote que abre com outra
             * contagem e' dado errado e NAO entra (o desenho pediria um quadro
             * que nao existe). */
            if (r->p[i].pm.total != e->quadros) {
                r->p[i].erro = PM_ERR_QUADRO;
                pm_fechar(&r->p[i].pm);
            } else if (r->p[i].pm.frame_bytes !=
                       (unsigned int)e->w * e->h * (unsigned int)(e->psm / 8)) {
                r->p[i].erro = PM_ERR_QUADRO;
                pm_fechar(&r->p[i].pm);
            } else {
                r->p[i].aberto = 1;
                ok++;
            }
        } else {
            r->p[i].erro = er;
        }
    }
    return ok;
}

int res_rr_pixels(res_rr_t *r, int i, int q, unsigned char *dst)
{
    if (!r || !dst || i < 0 || i >= RES_RR_MAX_ENTRADAS) return PM_ERR_QUADRO;
    if (!r->p[i].aberto) return PM_ERR_VAZIO;
    if (q < 0) return PM_ERR_QUADRO;
    return pm_quadro(&r->p[i].pm, (unsigned int)q, dst);
}

unsigned int res_rr_ram(const res_rr_t *r)
{
    int i;
    unsigned int s = 0;
    if (!r) return 0;
    for (i = 0; i < RES_RR_MAX_ENTRADAS; i++)
        if (r->p[i].aberto) s += pm_ram(&r->p[i].pm);
    return s;
}

static int mosquitinho(const char *a, const char *b)
{
    while (*a && *b && *a == *b) { a++; b++; }
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

const res_rr_rank_t *res_rr_rank(int idx)
{
    if (idx < 0 || idx >= RES_RR_RANKS) return 0;
    return &g_res_rr[idx];
}

int res_rr_indice(int rank)
{
    int i;
    if (rank < 0 || rank > 5) return -1;
    for (i = 0; i < RES_RR_RANKS; i++)
        if (mosquitinho(g_res_rr[i].rank, nomes_rank[rank]) == 0) return i;
    return -1;
}

static unsigned short fps_da(const res_rr_entrada_t *e)
{
    /* `animation.addByPrefix(..., 24)` -- mas o dado manda: todas as entradas
     * saem do CHP4 com o fps do atlas (24). */
    if (e->n_anims > 0 && e->anims[0].fps100 > 0)
        return (unsigned short)(e->anims[0].fps100 / 100u);
    (void)e;
    return 24;
}

unsigned long res_rr_dura_ms(const res_rr_entrada_t *e)
{
    unsigned short fps;
    unsigned int n, ini;
    if (!e || e->quadros == 0) return 0ul;
    fps = fps_da(e);
    if (!fps) fps = 24;
    ini = (e->inicio > 0 && e->inicio < e->quadros) ? (unsigned int)e->inicio : 0u;
    n = e->quadros - ini;
    if (n == 0) return 0ul;
    /* A PRIMEIRA passada: de `inicio` ao fim do pacote, com ou sem loop. E' o
     * que `play(startLabel)` faz. O periodo do loop, quando ha' um, e'
     * (quadros - loop_ini) e comeca depois desta passada. */
    return ((unsigned long)n * 1000ul + fps - 1ul) / fps;
}

int res_rr_quadro(const res_rr_entrada_t *e, unsigned long ms)
{
    unsigned short fps;
    unsigned long t;
    unsigned int n, ini, loop, k;
    if (!e || e->quadros == 0) return -1;
    if (ms < (unsigned long)e->inicio_ms) return -1;
    fps = fps_da(e);
    if (!fps) fps = 24;
    t = ms - (unsigned long)e->inicio_ms;
    k = (unsigned int)((t * (unsigned long)fps) / 1000ul);
    ini = (e->inicio > 0 && e->inicio < e->quadros) ? (unsigned int)e->inicio : 0u;
    n = e->quadros - ini;               /* quadros da primeira passada */
    if (n == 0) return (int)(e->quadros - 1);
    if (k < n) return (int)(ini + k);
    if (e->loop_ini < 0) return (int)(e->quadros - 1);   /* pausa no ultimo */
    loop = (unsigned int)e->loop_ini;
    {
        unsigned int periodo = e->quadros - loop;
        unsigned int k2;
        if (periodo == 0) return (int)(e->quadros - 1);
        k2 = (k - n) % periodo;
        return (int)(loop + k2);
    }
}
