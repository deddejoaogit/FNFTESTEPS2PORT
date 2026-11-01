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
