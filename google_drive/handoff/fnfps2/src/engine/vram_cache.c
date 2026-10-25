#include "vram_cache.h"

static unsigned int alinhar(unsigned int v)
{
    return (v + (VC_ALINHAMENTO - 1u)) & ~(VC_ALINHAMENTO - 1u);
}

void vc_init(vc_t *c, unsigned int base, unsigned int tamanho)
{
    int i;
    for (i = 0; i < VC_MAX_BLOCOS; i++) {
        c->b[i].chave = 0; c->b[i].endereco = 0; c->b[i].bytes = 0;
        c->b[i].selo = 0; c->b[i].usado = 0; c->b[i].fixo = 0;
    }
    c->base = base;
    c->tamanho = tamanho & ~(VC_ALINHAMENTO - 1u);
    c->relogio = 1;
    c->n = 1;
    c->b[0].endereco = base;
    c->b[0].bytes = c->tamanho;
    c->b[0].usado = 0;
    c->acertos = c->faltas = c->despejos = c->falhas = 0;
    c->bytes_enviados = 0;
}

static int achar(const vc_t *c, unsigned int chave)
{
    int i;
    for (i = 0; i < c->n; i++)
        if (c->b[i].usado && c->b[i].chave == chave) return i;
    return -1;
}

/* Junta buracos vizinhos. Sem isto a VRAM vira queijo suico depois de
   algumas trocas de cena e o cache passa a falhar com espaco de sobra. */
static void fundir(vc_t *c)
{
    int i = 0;
    while (i + 1 < c->n) {
        if (!c->b[i].usado && !c->b[i + 1].usado) {
            int k;
            c->b[i].bytes += c->b[i + 1].bytes;
            for (k = i + 1; k + 1 < c->n; k++) c->b[k] = c->b[k + 1];
            c->n--;
        } else {
            i++;
        }
    }
}

static int inserir(vc_t *c, int i, unsigned int bytes)
{
    /* Parte o buraco i, devolvendo a parte da frente com 'bytes'. */
    if (c->b[i].bytes == bytes) {
        c->b[i].usado = 1;
        return i;
    }
    if (c->n >= VC_MAX_BLOCOS) return -1;
    {
        int k;
        for (k = c->n; k > i + 1; k--) c->b[k] = c->b[k - 1];
        c->n++;
        c->b[i + 1].endereco = c->b[i].endereco + bytes;
        c->b[i + 1].bytes = c->b[i].bytes - bytes;
        c->b[i + 1].usado = 0;
        c->b[i + 1].fixo = 0;
        c->b[i + 1].chave = 0;
        c->b[i].bytes = bytes;
        c->b[i].usado = 1;
    }
    return i;
}

static int primeiro_que_cabe(vc_t *c, unsigned int bytes)
{
    int i;
    for (i = 0; i < c->n; i++)
        if (!c->b[i].usado && c->b[i].bytes >= bytes) return i;
    return -1;
}

/* Despeja o bloco usado, nao fixo, com o menor selo. Devolve 0 se nao
   houver nada despejavel. */
static int despejar_um(vc_t *c)
{
    int i, alvo = -1;
    unsigned int menor = 0xFFFFFFFFu;
    for (i = 0; i < c->n; i++) {
        if (c->b[i].usado && !c->b[i].fixo && c->b[i].selo < menor) {
            menor = c->b[i].selo;
            alvo = i;
        }
    }
    if (alvo < 0) return 0;
    c->b[alvo].usado = 0;
    c->b[alvo].chave = 0;
    c->despejos++;
    fundir(c);
    return 1;
}

int vc_pedir(vc_t *c, unsigned int chave, unsigned int bytes, unsigned int *saida)
{
    int i;
    unsigned int b = alinhar(bytes);
    if (!b || b > c->tamanho) { c->falhas++; return -1; }

    i = achar(c, chave);
    if (i >= 0) {
        c->b[i].selo = c->relogio++;
        if (saida) *saida = c->b[i].endereco;
        c->acertos++;
        return 1;
    }

    i = primeiro_que_cabe(c, b);
    while (i < 0) {
        if (!despejar_um(c)) { c->falhas++; return -1; }
        i = primeiro_que_cabe(c, b);
    }
    i = inserir(c, i, b);
    if (i < 0) {
        /* Sem descritor livre: despeja para reduzir a fragmentacao e
           tenta de novo uma unica vez. */
        if (!despejar_um(c)) { c->falhas++; return -1; }
        i = primeiro_que_cabe(c, b);
        if (i < 0) { c->falhas++; return -1; }
        i = inserir(c, i, b);
        if (i < 0) { c->falhas++; return -1; }
    }
    c->b[i].chave = chave;
    c->b[i].fixo = 0;
    c->b[i].selo = c->relogio++;
    if (saida) *saida = c->b[i].endereco;
    c->faltas++;
    c->bytes_enviados += b;
    return 0;
}

void vc_fixar(vc_t *c, unsigned int chave)
{
    int i = achar(c, chave);
    if (i >= 0) c->b[i].fixo = 1;
}

void vc_limpar(vc_t *c)
{
    int i;
    for (i = 0; i < c->n; i++) {
        if (c->b[i].usado && !c->b[i].fixo) {
            c->b[i].usado = 0;
            c->b[i].chave = 0;
        }
    }
    fundir(c);
}

unsigned int vc_livre(const vc_t *c)
{
    unsigned int s = 0;
    int i;
    for (i = 0; i < c->n; i++) if (!c->b[i].usado) s += c->b[i].bytes;
    return s;
}

unsigned int vc_maior_buraco(const vc_t *c)
{
    unsigned int m = 0;
    int i;
    for (i = 0; i < c->n; i++)
        if (!c->b[i].usado && c->b[i].bytes > m) m = c->b[i].bytes;
    return m;
}
