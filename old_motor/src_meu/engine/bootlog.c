#include "bootlog.h"
#include <string.h>

static const char *const FASE_NOMES[BF_FASE_COUNT] = {
    "INICIO", "PAD", "AUDIO_CAUDAL", "AUDIO_ENVIO", "AUDIO_LEITURA",
    "CHART", "ESTADO", "DESENHO", "CAPTURA", "VSYNC", "TROCA_FB",
    "TELEMETRIA", "FIM"
};

const char *bootlog_fase_nome(int fase)
{
    if (fase < 0 || fase >= BF_FASE_COUNT) return "?";
    return FASE_NOMES[fase];
}

/* Monta "PASSO=66 MARCO=FRAME VALOR=0 QUADRO=12 FASE=VSYNC" sem printf. */
static int add_str(char *d, int p, int tam, const char *s)
{
    int i;
    for (i = 0; s[i] && p < tam - 1; i++) d[p++] = s[i];
    d[p] = 0;
    return p;
}

static int add_num(char *d, int p, int tam, int v)
{
    char tmp[12];
    int t = 0;
    unsigned int u;
    if (v < 0) { if (p < tam - 1) d[p++] = '-'; u = (unsigned int)(-v); }
    else u = (unsigned int)v;
    if (!u) tmp[t++] = '0';
    while (u && t < 11) { tmp[t++] = (char)('0' + (u % 10u)); u /= 10u; }
    while (t > 0 && p < tam - 1) d[p++] = tmp[--t];
    d[p] = 0;
    return p;
}

static void emite(bootlog_t *b, int passo, const char *nome, int valor)
{
    char linha[128];
    int p = 0;
    if (!b->escritor) return;
    p = add_str(linha, p, (int)sizeof(linha), "PASSO=");
    p = add_num(linha, p, (int)sizeof(linha), passo);
    p = add_str(linha, p, (int)sizeof(linha), " MARCO=");
    p = add_str(linha, p, (int)sizeof(linha), nome);
    p = add_str(linha, p, (int)sizeof(linha), " VALOR=");
    p = add_num(linha, p, (int)sizeof(linha), valor);
    p = add_str(linha, p, (int)sizeof(linha), " QUADRO=");
    p = add_num(linha, p, (int)sizeof(linha), (int)b->quadro);
    p = add_str(linha, p, (int)sizeof(linha), " FASE=");
    p = add_str(linha, p, (int)sizeof(linha), bootlog_fase_nome((int)b->fase));
    b->escritor(b->ctx, linha);
}

void bootlog_init(bootlog_t *b, bootlog_escritor_fn escritor, void *ctx, int verbose)
{
    memset(b, 0, sizeof(*b));
    b->escritor = escritor;
    b->ctx = ctx;
    b->verbose = verbose;
    b->fase = BF_INICIO;
    b->fase_anterior = -1;
    b->limite_ms = 100; /* 100 ms num quadro e travamento, nao lentidao */
    b->passo = 0;
    /* o arquivo de log termina com o ultimo SHUTDOWN; quem chama
       bootlog_analisa_anterior() pode descobrir se a execucao anterior
       morreu. */
    emite(b, 0, "BOOT", 0);
}

void bootlog_analisa_anterior(bootlog_t *b, const char *conteudo)
{
    const char *p;
    if (!conteudo) return;
    b->morto_sem_fim = 1; /* assume morte ate achar prova de vida */
    for (p = conteudo; *p; p++) {
        if (!strncmp(p, "MARCO=SHUTDOWN", 14)) { b->morto_sem_fim = 0; break; }
    }
}

int bootlog_marco(bootlog_t *b, const char *nome, int valor)
{
    int i, passo = ++b->passo;
    if (b->n_marcos < BOOTLOG_MARCOS) i = b->n_marcos++;
    else { /* anel cheio: descarta o mais antigo */
        int k;
        for (k = 0; k + 1 < BOOTLOG_MARCOS; k++) b->marcos[k] = b->marcos[k + 1];
        i = BOOTLOG_MARCOS - 1;
    }
    b->marcos[i].passo = passo;
    b->marcos[i].valor = valor;
    {
        int k = 0;
        for (; nome && nome[k] && k < 39; k++) b->marcos[i].nome[k] = nome[k];
        b->marcos[i].nome[k] = 0;
    }
    if (b->escritor && b->verbose) emite(b, passo, nome ? nome : "?", valor);
    return passo;
}

void bootlog_nota(bootlog_t *b, const char *nome, int valor)
{
    if (b->escritor && b->verbose) emite(b, b->passo, nome ? nome : "?", valor);
}

void bootlog_quadro_inicio(bootlog_t *b, unsigned int quadro)
{
    b->quadro = quadro;
    b->fase = BF_INICIO;
    b->fase_anterior = BF_INICIO;
    b->ms_inicio_quadro = 0;
}

void bootlog_fase(bootlog_t *b, bootlog_fase_t fase)
{
    b->fase = fase;
    /* grava a fase so quando muda: 4 a 8 bytes por troca, e o unico
       rastro que sobrevive a um travamento duro no meio do quadro. */
    if (b->fase_anterior != (int)fase) {
        b->fase_anterior = (int)fase;
        if (b->escritor && b->verbose) emite(b, b->passo, "FASE", (int)fase);
    }
}

void bootlog_quadro_fim(bootlog_t *b, unsigned int ms_gasto)
{
    if (b->limite_ms && ms_gasto > b->limite_ms) bootlog_alerta(b, "QUADRO_LENTO", (int)ms_gasto);
    b->fase = BF_FIM;
}

void bootlog_alerta(bootlog_t *b, const char *motivo, int valor)
{
    b->alertas++;
    /* alerta vai SEMPRE, mesmo com verbose desligado: e a informacao
       que interessa. */
    if (b->escritor) {
        char linha[128];
        int p = 0;
        p = add_str(linha, p, (int)sizeof(linha), "ALERTA=");
        p = add_str(linha, p, (int)sizeof(linha), motivo ? motivo : "?");
        p = add_str(linha, p, (int)sizeof(linha), " VALOR=");
        p = add_num(linha, p, (int)sizeof(linha), valor);
        p = add_str(linha, p, (int)sizeof(linha), " QUADRO=");
        p = add_num(linha, p, (int)sizeof(linha), (int)b->quadro);
        p = add_str(linha, p, (int)sizeof(linha), " FASE=");
        p = add_str(linha, p, (int)sizeof(linha), bootlog_fase_nome((int)b->fase));
        b->escritor(b->ctx, linha);
    }
}

const bootlog_marco_t *bootlog_ultimo(const bootlog_t *b)
{
    if (b->n_marcos <= 0) return (const bootlog_marco_t *)0;
    return &b->marcos[b->n_marcos - 1];
}
