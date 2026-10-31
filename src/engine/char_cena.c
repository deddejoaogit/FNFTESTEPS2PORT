#include "char_cena.h"

#include "char_anim.h"

#include <limits.h>
#include <string.h>

#define CH_ANIM_END_SUF "-end"
#define CH_ANIM_HOLD_SUF "-hold"

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
    p->especial = 0;
    p->ultima_batida = INT_MIN;   /* a primeira batida sempre danca */
    r = cp_abrir(pacote, bytes, &p->pacote);
    if (r != 0) return r;
    r = cp_conferir(&p->pacote);
    if (r != 0) return r;
    p->ativo = 1;
    return 0;
}


/* "<nome><sufixo>" em buf. Devolve 1 se coube. */
static int com_sufixo(char *buf, size_t tam, const char *nome, const char *sufixo)
{
    size_t n = strlen(nome), m = strlen(sufixo);
    if (n + m + 1u > tam) return 0;
    memcpy(buf, nome, n);
    memcpy(buf + n, sufixo, m + 1u);
    return 1;
}

static int termina_com(const char *nome, const char *sufixo)
{
    size_t n = strlen(nome), m = strlen(sufixo);
    return n >= m && memcmp(nome + n - m, sufixo, m) == 0;
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

int cc_toca(cc_cena_t *c, int papel, const char *nome, long agora)
{
    cc_papel_t *p;
    int a;
    if (!c || papel < 0 || papel >= CH_CENA_PAPEIS || !nome) return 0;
    p = &c->p[papel];
    if (!p->ativo) return 0;
    a = cp_anim_i(&p->pacote, nome);
    if (a < 0) return 0;
    if (a != p->anim) {
        p->anim = a;
        p->t0 = agora;
        p->quadro = -1;
    }
    p->canto_ate = 0;       /* animacao nomeada nao e' canto */
    p->especial = 1;
    return 1;
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
    p->especial = 0;
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
        /* BaseCharacter.hx L443-460: quando o canto acaba, se existe
           "<animacao>-end" ela toca; senao, volta a dancar. */
        char fim[CHP_NOME];
        int achou = -1;
        p->canto_ate = 0;
        if (p->anim >= 0) {
            /* BaseCharacter.hx L454-456: tira o "-hold" ANTES de procurar o
               "-end" -- quem esta' cantando e ja' entrou no "-hold" precisa
               achar "singLEFT-end", e nao "singLEFT-hold-end". */
            char base[CHP_NOME];
            const char *nome = p->pacote.anim[p->anim].nome;
            size_t n = strlen(nome);
            if (termina_com(nome, CH_ANIM_HOLD_SUF)) n -= strlen(CH_ANIM_HOLD_SUF);
            if (n + 1u > sizeof base) n = 0;
            memcpy(base, nome, n);
            base[n] = 0;
            if (com_sufixo(fim, sizeof fim, base, CH_ANIM_END_SUF))
                achou = cp_anim_i(&p->pacote, fim);
        }
        if (achou >= 0) {
            p->anim = achou;
            p->t0 = agora;
            p->quadro = -1;
            p->especial = 1;          /* "-end" tambem nao e' interrompida */
        } else {
            a = anim_ocioso(p, batida);
            if (a >= 0 && a != p->anim) {
                p->anim = a;
                p->t0 = agora;
                p->quadro = -1;
            }
        }
    }
    /* Animacao nomeada ou "-end": fica ate' ACABAR. So' sai quando o ultimo
       quadro dela ja' passou (a animacao terminou). */
    if (p->especial && p->anim >= 0) {
        ch_seq_t e;
        if (cp_seq(&p->pacote, p->anim, &e) == 0) {
            long passado = agora - p->t0;
            /* Uma animacao nomeada em loop (o dado nao diz que 'hey' seria)
               conta como terminada depois de UMA volta: senao o personagem
               ficaria preso nela para o resto da musica, sem nunca voltar a
               dancar. E' a unica leitura que mantem a musica tocando. */
            if (e.fps100 > 0 && passado >= (long)((long)e.count * 100000L / (long)e.fps100)) {
                p->especial = 0;
                a = anim_ocioso(p, batida);
                if (a >= 0 && a != p->anim) {
                    p->anim = a;
                    p->t0 = agora;
                    p->quadro = -1;
                }
            }
        }
    }
    if (p->especial) {
        /* nada: quem esta' numa animacao nomeada nao danca */
    } else
    /* O DANCE ACONTECE NA BATIDA, nao em todo quadro.
       Bopper.dance() e' chamado no beat hit (onBeatHit), e a diferenca nao e'
       cosmetica: rodando a cada quadro, um personagem que termina o `idle` e
       entra no `idle-hold` voltaria ao `idle` no quadro seguinte e nunca
       mostraria o "-hold" -- os olhos do pai nunca piscariam. Na batida, o
       ciclo e' o do original: idle -> (acaba) -> idle-hold -> (batida) -> idle.
       `ultima_batida` guarda em que batida o dance() rodou. */
    if (p->canto_ate == 0 && batida != p->ultima_batida) {
        p->ultima_batida = batida;
        a = anim_ocioso(p, batida);
        if (a >= 0 && a != p->anim) {
            p->anim = a;
            p->t0 = agora;
            p->quadro = -1;
        }
    }
    if (p->anim < 0) return -1;
    if (cp_seq(&p->pacote, p->anim, &q) != 0) return -1;
    /* "-hold": quando a animacao atual TERMINA e existe "<nome>-hold", ela toca
       e fica (e' em loop no dado). BaseCharacter.hx L408-416: e' assim que os
       olhos do pai piscam e a chama do isqueiro do Darnell se mexe -- sem isso
       os dois ficam parados no ultimo quadro do canto, e a diferenca aparece na
       tela. Se a animacao atual e' em loop, ela nunca "termina", entao o
       "-hold" nao entra: e' o mesmo comportamento do original
       (isAnimationFinished() so' e' verdadeiro fora de loop). */
    {
        const char *nome = p->pacote.anim[p->anim].nome;
        if (!termina_com(nome, CH_ANIM_HOLD_SUF) &&
            q.fps100 > 0 &&
            (agora - p->t0) >= (long)((long)q.count * 100000L / (long)q.fps100)) {
            char alvo[CHP_NOME];
            if (com_sufixo(alvo, sizeof alvo, nome, CH_ANIM_HOLD_SUF)) {
                int h = cp_anim_i(&p->pacote, alvo);
                if (h >= 0) {
                    p->anim = h;
                    p->t0 = agora;
                    p->quadro = -1;
                    if (cp_seq(&p->pacote, p->anim, &q) != 0) return -1;
                }
            }
        }
    }
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
