#include "char_cena.h"

#include "char_anim.h"

#include <limits.h>
#include <stdio.h>
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
    p->especial_fixo = 0;
    p->ultimo_passo = INT_MIN;    /* o primeiro passo que valer danca */
    p->danca_cada = 1.0f;         /* CharacterData DEFAULT_DANCEEVERY */
    p->dancas = 0;                /* hasDanced = false -> a primeira e' danceLeft */
    p->pulos = 0;
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

/* A animacao do dance(): `idle` fixo para quem tem, senao danceLeft/danceRight
   pelo LADO. O lado e' `dancas & 1`: o `hasDanced` do original, que so' vira
   quando uma danca e' DE FATO jogada. Antes esta funcao recebia a batida e
   usava `batida & 1` -- e entao, quando o personagem passava uma batida
   cantando (o dance() do original sai fora e nao mexe em `hasDanced`), o port
   trocava o lado mesmo assim: a partir da primeira nota cantada, o BF e o
   oponente dançavam no pe' trocado para o resto da musica. */
static int anim_ocioso(const cc_papel_t *p, int lado)
{
    int a = cp_anim_i(&p->pacote, "idle");
    if (a >= 0) return a;
    a = cp_anim_i(&p->pacote, (lado & 1) ? "danceRight" : "danceLeft");
    if (a >= 0) return a;
    return cp_anim_i(&p->pacote, (lado & 1) ? "danceLeft" : "danceRight");
}

void cc_especial_fixo(cc_cena_t *c, int papel, int fixo)
{
    if (!c || papel < 0 || papel >= CH_CENA_PAPEIS) return;
    c->p[papel].especial_fixo = fixo ? 1 : 0;
}

/* Duracao de uma animacao em ms, pela taxa da sequencia (centesimos de quadro
   por segundo). Taxa ausente nao vira "instantaneo": sem fps nao ha' como saber
   que acabou, entao devolve -1 e quem chama decide. */
static long duracao_ms(const ch_seq_t *e)
{
    if (!e || e->fps100 <= 0 || e->count <= 0) return -1;
    return (long)((long)e->count * 100000L / (long)e->fps100);
}

int cc_especial_terminou(const cc_cena_t *c, int papel, long agora)
{
    const cc_papel_t *p;
    ch_seq_t e;
    long dur;
    if (!c || papel < 0 || papel >= CH_CENA_PAPEIS) return 0;
    p = &c->p[papel];
    if (!p->ativo || !p->especial || p->anim < 0) return 0;
    if (cp_seq(&p->pacote, p->anim, &e) != 0) return 0;
    dur = duracao_ms(&e);
    if (dur < 0) return 0;
    return (agora - p->t0) >= dur ? 1 : 0;
}

void cc_danca_cada(cc_cena_t *c, int papel, float cada)
{
    if (!c || papel < 0 || papel >= CH_CENA_PAPEIS) return;
    if (cada < 0.0f) cada = 0.0f;
    c->p[papel].danca_cada = cada;
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

int cc_canto_sufixo(cc_cena_t *c, int papel, int dir, int missing,
                    const char *sufixo, long agora, long sing_ms)
{
    static const char *base[4] = { "singLEFT", "singDOWN", "singUP", "singRIGHT" };
    cc_papel_t *p;
    char nome[CHP_NOME];
    size_t n, m, extra;
    int a;
    if (!c || papel < 0 || papel >= CH_CENA_PAPEIS || !sufixo || !*sufixo) return 0;
    if (dir < 0 || dir > 3) return 0;
    p = &c->p[papel];
    if (!p->ativo) return 0;
    /* "sing<DIR>" + ("miss") + "-" + sufixo. Sem snprintf: o nome tem de caber
       em CHP_NOME (24), e um sufixo maior que isso nao existe no original. */
    n = strlen(base[dir]);
    m = strlen(sufixo);
    extra = (missing ? 4u : 0u) + 1u + m;
    if (n + extra + 1u > sizeof nome) return 0;
    memcpy(nome, base[dir], n);
    if (missing) { memcpy(nome + n, "miss", 4); n += 4; }
    nome[n++] = '-';
    memcpy(nome + n, sufixo, m + 1u);
    a = cp_anim_i(&p->pacote, nome);
    if (a < 0) return 0;                 /* o personagem nao tem esse canto */
    if (a != p->anim) {
        p->anim = a;
        p->t0 = agora;
        p->quadro = -1;
    }
    p->especial = 0;
    if (sing_ms > 0 && agora + sing_ms > p->canto_ate) p->canto_ate = agora + sing_ms;
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

/* "<prefixo><numero>" -> o numero, ou -1 se o nome nao for desse formato. E' a
   leitura do `findCountAnimations` do BaseCharacter.hx L232-251: prefixo e
   `Std.parseInt` do resto; o original DESCARTA o nome que nao for numero, e o
   port tambem. */
static int numero_da_anim(const char *nome, const char *prefixo)
{
    size_t n = strlen(prefixo);
    long v = 0;
    const char *d;
    if (!nome || strncmp(nome, prefixo, n) != 0) return -1;
    d = nome + n;
    if (!*d) return -1;
    for (; *d; d++) {
        if (*d < '0' || *d > '9') return -1;
        v = v * 10 + (*d - '0');
        if (v > 1000000) return -1;
    }
    return (int)v;
}

int cc_combo(cc_cena_t *c, int papel, int combo, long agora)
{
    char alvo[CHP_NOME];
    if (!c || papel < 0 || papel >= CH_CENA_PAPEIS) return 0;
    if (!c->p[papel].ativo || combo <= 0) return 0;
    /* BaseCharacter L653: `comboAnim = 'combo${comboCount}'`, e so' toca se
       `hasAnimation(comboAnim)`. Nada de arredondar para o degrau de baixo: o
       combo200 nao substitui o combo50. */
    if (snprintf(alvo, sizeof alvo, "combo%d", combo) <= 0) return 0;
    if (cp_anim_i(&c->p[papel].pacote, alvo) < 0) return 0;
    return cc_toca(c, papel, alvo, agora);
}

int cc_drop(cc_cena_t *c, int papel, int combo, long agora)
{
    cc_papel_t *p;
    const char *melhor = 0;
    int i, melhor_n = -1;
    if (!c || papel < 0 || papel >= CH_CENA_PAPEIS) return 0;
    p = &c->p[papel];
    if (!p->ativo) return 0;
    /* BaseCharacter L661-681: percorre os `drop<N>` do personagem (ordenados) e
       fica com o MAIOR que `comboCount >= N`; se nenhum serve, nao toca nada. */
    for (i = 0; i < p->pacote.n_anims; i++) {
        const char *nome = p->pacote.anim[i].nome;
        int n = numero_da_anim(nome, "drop");
        if (n < 0 || combo < n) continue;
        if (n > melhor_n) { melhor_n = n; melhor = nome; }
    }
    if (!melhor) return 0;
    return cc_toca(c, papel, melhor, agora);
}

int cc_quadro(cc_cena_t *c, int papel, long agora, int passo)
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
            /* Fim do canto sem `-end`: BaseCharacter.dance(true) -- danca
               FORCADA, que conta como danca jogada (inverte o lado). */
            a = anim_ocioso(p, p->dancas & 1);
            p->dancas++;
            if (a >= 0 && a != p->anim) {
                p->anim = a;
                p->t0 = agora;
                p->quadro = -1;
            }
        }
    }
    /* Animacao nomeada ou "-end": fica ate' ACABAR. So' sai quando o ultimo
       quadro dela ja' passou (a animacao terminou). */
    if (p->especial && p->anim >= 0 && !p->especial_fixo) {
        ch_seq_t e;
        if (cp_seq(&p->pacote, p->anim, &e) == 0) {
            long passado = agora - p->t0;
            /* Uma animacao nomeada em loop (o dado nao diz que 'hey' seria)
               conta como terminada depois de UMA volta: senao o personagem
               ficaria preso nela para o resto da musica, sem nunca voltar a
               dancar. E' a unica leitura que mantem a musica tocando. */
            if (e.fps100 > 0 && passado >= (long)((long)e.count * 100000L / (long)e.fps100)) {
                p->especial = 0;
                /* Volta a dancar: tambem conta como danca jogada. */
                a = anim_ocioso(p, p->dancas & 1);
                p->dancas++;
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
       mostraria o "-hold" -- os olhos do pai nunca piscariam. No PASSO, o
       ciclo e' o do original: idle -> (acaba) -> idle-hold -> (passo) -> idle.
       `ultimo_passo` guarda em que passo o dance() rodou, e `ca_passo_de_danca`
       e' a condicao do Bopper.hx L166 -- com o `danceEvery` do personagem em
       BATIDAS (1.0 nos 46: o campo esta' ausente em todos, e o CharacterData
       preenche o default). */
    if (passo != p->ultimo_passo && ca_passo_de_danca(p->danca_cada, passo)) {
        p->ultimo_passo = passo;
        if (p->canto_ate != 0) {
            /* Passo de danca perdido porque o personagem esta' cantando: o
               onStepHit ja' passou e nao volta, entao o passo fica consumido --
               mas o LADO nao gira (BaseCharacter.dance() sai fora antes de
               `hasDanced`). E' a contagem de pulos que o log mostra. */
            p->pulos++;
        } else {
            a = anim_ocioso(p, p->dancas & 1);
            p->dancas++;
            if (a >= 0 && a != p->anim) {
                p->anim = a;
                p->t0 = agora;
                p->quadro = -1;
            }
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
