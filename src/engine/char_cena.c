#include "char_cena.h"

#include "char_anim.h"

#include <string.h>

void cc_reset(cc_cena_t *c)
{
    if (!c) return;
    memset(c, 0, sizeof(*c));
    c->p[CH_PAPEL_GF].papel = CH_PAPEL_GF;
    c->p[CH_PAPEL_BF].papel = CH_PAPEL_BF;
    c->p[CH_PAPEL_DAD].papel = CH_PAPEL_DAD;
    c->p[CH_PAPEL_GF].anim = c->p[CH_PAPEL_BF].anim = c->p[CH_PAPEL_DAD].anim = -1;
    c->p[CH_PAPEL_GF].quadro = c->p[CH_PAPEL_BF].quadro = c->p[CH_PAPEL_DAD].quadro = -1;
}

int cc_anexar(cc_cena_t *c, int papel, const unsigned char *pacote,
              unsigned int bytes, long agora)
{
    int r;
    cc_papel_t *p;
    if (!c || papel < 0 || papel >= CH_CENA_PAPEIS) return -1;
    p = &c->p[papel];
    p->ativo = 0;
    p->anim = -1;
    p->quadro = -1;
    p->t0 = agora;
    p->canto_ate = 0;
    r = cp_abrir(pacote, bytes, &p->pacote);
    if (r != 0) return r;
    r = cp_conferir(&p->pacote);
    if (r != 0) return r;
    p->ativo = 1;
    return 0;
}

/* Animacao de canto: "sing<DIR>" e, com erro, "sing<DIR>miss"; se o nome com
   "miss" nao existir, cai no normal -- a troca de sufixo do BaseCharacter.
   Devolve -1 se o personagem nao canta em nenhuma das duas. */
static int anim_canto(const cc_papel_t *p, int dir, int missing)
{
    static const char *base[4] = { "singLEFT", "singDOWN", "singUP", "singRIGHT" };
    char buf[CHP_NOME];
    size_t n;
    if (dir < 0 || dir > 3) return -1;
    if (missing) {
        n = strlen(base[dir]);
        if (n + 4u < sizeof buf) {
            memcpy(buf, base[dir], n);
            memcpy(buf + n, "miss", 5);          /* inclui o NUL */
            {
                int a = cp_anim_i(&p->pacote, buf);
                if (a >= 0) return a;
            }
        }
    }
    return cp_anim_i(&p->pacote, base[dir]);
}

static int anim_ocioso(const cc_papel_t *p, int batida)
{
    int a = cp_anim_i(&p->pacote, "idle");
    if (a >= 0) return a;
    a = cp_anim_i(&p->pacote, (batida & 1) ? "danceRight" : "danceLeft");
    if (a >= 0) return a;
    return cp_anim_i(&p->pacote, (batida & 1) ? "danceLeft" : "danceRight");
}

void cc_canto(cc_cena_t *c, int papel, int dir, int missing, long agora, long sing_ms)
{
    cc_papel_t *p;
    int a;
    if (!c || papel < 0 || papel >= CH_CENA_PAPEIS) return;
    p = &c->p[papel];
    if (!p->ativo) return;
    a = anim_canto(p, dir, missing);
    if (a < 0) return;                 /* personagem de fundo: nao canta */
    if (a != p->anim) {
        p->anim = a;
        p->t0 = agora;
        p->quadro = -1;
    }
    /* O mesmo canto pedido de novo NAO reinicia (senao a perna da GF
       reiniciaria a cada nota escrita no mesmo instante), mas estende o tempo:
       duas notas seguidas mantem o personagem cantando ate' a segunda acabar. */
    if (sing_ms > 0 && agora + sing_ms > p->canto_ate) p->canto_ate = agora + sing_ms;
}

int cc_quadro(cc_cena_t *c, int papel, long agora, int batida)
{
    cc_papel_t *p;
    ch_seq_t q;
    int a, quadro;
    if (!c || papel < 0 || papel >= CH_CENA_PAPEIS) return -1;
    p = &c->p[papel];
    if (!p->ativo) return -1;
    if (p->canto_ate > 0 && agora >= p->canto_ate) {
        p->canto_ate = 0;
        a = anim_ocioso(p, batida);
        if (a >= 0 && a != p->anim) {
            p->anim = a;
            p->t0 = agora;
            p->quadro = -1;
        }
    }
    /* Quem nao tem idle alterna danceLeft/danceRight a CADA batida, e nao so'
       quando o canto acaba: sem esta troca o personagem ficaria congelado no
       primeiro lado para o resto da musica. Para quem tem `idle` isto nao muda
       nada (anim_ocioso devolve sempre o idle). */
    if (p->canto_ate == 0) {
        a = anim_ocioso(p, batida);
        if (a >= 0 && a != p->anim) {
            p->anim = a;
            p->t0 = agora;
            p->quadro = -1;
        }
    }
    if (p->anim < 0) return -1;
    if (cp_seq(&p->pacote, p->anim, &q) != 0) return -1;
    quadro = ca_frame(&q, agora - p->t0);
    if (quadro < 0 || quadro == p->quadro) return -1;
    p->quadro = quadro;
    return quadro;
}

int cc_anim(const cc_cena_t *c, int papel)
{
    if (!c || papel < 0 || papel >= CH_CENA_PAPEIS) return -1;
    return c->p[papel].ativo ? c->p[papel].anim : -1;
}
