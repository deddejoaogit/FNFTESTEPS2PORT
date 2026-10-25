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
    /* GUARDA DE LIMITE. Esta e a rede de seguranca contra a classe de
       defeito que trava o console e nao aparece no PC: um upload que
       passa do fim da VRAM (ou que invade o framebuffer) trava o
       caminho GIF/DMA, e o graph_wait_vsync nunca volta. O PC nao
       modela VRAM, entao 1400 quadros passam la e o console morre no
       primeiro. Aqui a conta e conferida antes de qualquer envio. */
    if (c->b[i].endereco < c->base
        || c->b[i].endereco + b > c->base + c->tamanho) {
        c->falhas++;
        c->b[i].usado = 0;
        return -1;
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

/* =====================================================================
 * Extensoes do port completo
 * =====================================================================
 * As tres primeiras funcoes existem por causa de medidas feitas nos 21
 * palcos:
 *
 *  - limoRideErect tem 5 sprites apontando para a MESMA folha
 *    (limo/henchmen, 2048x2048). Com vc_pedir() puro isso viraria 5
 *    copias na VRAM (5 x 16 MB, impossivel). vc_apelido() faz as 5
 *    chaves dividirem um bloco so.
 *
 *  - tankmanBattlefield pede 13 sprites vivos ao mesmo tempo e uma
 *    reducao a 16bpp deixa a regiao com ~3,4 MB. Um palco pode ter mais
 *    conteudo do que cabe inteiro; nesse caso o desenho despeja durante
 *    o quadro. vc_despejar_ate() permite perguntar "abre espaco para
 *    tantos bytes sem eu perder o que estou usando agora".
 *
 * Contagem de fixacao: vc_fixar() antigo era booleano. HUD e fonte sao
 * fixados por varias telas ao mesmo tempo e a tela que sai nao pode
 * soltar o que a outra ainda usa. Por isso o contador.
 */

static int achar_q(const vc_t *c, unsigned int chave)
{
    int i;
    for (i = 0; i < c->n; i++)
        if (c->b[i].usado && c->b[i].chave == chave) return i;
    return -1;
}

int vc_apelido(vc_t *c, unsigned int chave_original, unsigned int chave_nova)
{
    int i = achar_q(c, chave_original);
    int k;
    if (i < 0) return -1;
    if (achar_q(c, chave_nova) >= 0) return 0; /* ja existe, nada a fazer */
    if (c->n >= VC_MAX_BLOCOS) return -1;
    /* Insere um descritor de tamanho zero imediatamente depois do
       original. bytes=0 e proposital: ele nao ocupa VRAM, so herda o
       endereco. fundir() nunca o junta porque ele esta 'usado'. */
    for (k = c->n; k > i + 1; k--) c->b[k] = c->b[k - 1];
    c->n++;
    c->b[i + 1].chave = chave_nova;
    c->b[i + 1].endereco = c->b[i].endereco;
    c->b[i + 1].bytes = 0;
    c->b[i + 1].selo = c->b[i].selo;
    c->b[i + 1].usado = 1;
    c->b[i + 1].fixo = 0;
    return 0;
}

void vc_prender(vc_t *c, unsigned int chave)
{
    int i = achar_q(c, chave);
    if (i >= 0) c->b[i].fixo = (unsigned char)(c->b[i].fixo + 1);
}

void vc_soltar(vc_t *c, unsigned int chave)
{
    int i = achar_q(c, chave);
    if (i >= 0 && c->b[i].fixo) c->b[i].fixo--;
}

int vc_despejar_ate(vc_t *c, unsigned int bytes)
{
    unsigned int b = alinhar(bytes);
    if (b > c->tamanho) return -1;
    while (vc_maior_buraco(c) < b) {
        if (!despejar_um(c)) return -1;
    }
    return 0;
}

/* Relatorio sem printf: o ps2sdk tem sprintf mas o teste no PC tambem
   usa esta funcao, e ela precisa ser identica nos dois lados. */
void vc_relatar(const vc_t *c, char *destino, int tam)
{
    int p = 0;
    unsigned int vals[5];
    const char *rot[5] = { "livre=", "maior=", "faltas=", "acertos=", "despejos=" };
    int i, k;
    if (tam <= 0) return;
    vals[0] = vc_livre(c); vals[1] = vc_maior_buraco(c);
    vals[2] = c->faltas; vals[3] = c->acertos; vals[4] = c->despejos;
    destino[0] = 0;
    for (i = 0; i < 5; i++) {
        unsigned int v = vals[i];
        char tmp[12];
        int t = 0;
        for (k = 0; rot[i][k] && p < tam - 1; k++) destino[p++] = rot[i][k];
        if (v == 0) tmp[t++] = '0';
        while (v && t < 11) { tmp[t++] = (char)('0' + (v % 10u)); v /= 10u; }
        while (t > 0 && p < tam - 1) destino[p++] = tmp[--t];
        if (i + 1 < 5 && p < tam - 1) destino[p++] = ' ';
    }
    destino[p] = 0;
}
