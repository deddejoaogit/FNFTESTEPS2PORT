/* pack_multi: ler um pacote CFZ4 dividido em PARTES.
 *
 * O QUE PROVA
 * O DJ da tela de Freeplay tem 678 quadros de 370x375 a 16 bpp: 12,6 MB de
 * pacote. O leitor de um arquivo so' (`pack_stream`) nao abre isso -- o proprio
 * empacotador corta em partes de no maximo 8 MB (`write_packs`). Aqui duas
 * partes de tamanhos DIFERENTES sao montadas em disco e lidas como um pacote
 * so':
 *  - o total e' a soma das partes, e cada quadro sai byte a byte igual ao
 *    empacotado, inclusive na TRAVESSIA de uma parte para a outra;
 *  - a ordem do plano e' a ordem dos quadros: nao ha' tabela global para
 *    inventar, o indice global e' `primeiro + indice local`;
 *  - parte faltando, parte com outro tamanho de quadro, quadro fora do total e
 *    pacote fechado sao ERRO NOMEADO, nao silencio (um quadro a menos no meio
 *    do DJ e' um personagem que pula animacao sem avisar).
 *
 * O que NAO prova: a taxa de leitura do pendrive (so' o console diz) e nem que
 * o CHP4 escrito pelo empacotador tem as animacoes certas -- isso e' o
 * `test_char_pack`/o empacotador.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pack_multi.h"

static int falhas;
#define CHECK(c) do { if (!(c)) { printf("  FALHA: %s (linha %d)\n", #c, __LINE__); falhas++; } } while (0)
#define CHECK_EQ(a, b) do { long _a = (long)(a), _b = (long)(b); \
    if (_a != _b) { printf("  FALHA: %s == %s: %ld != %ld (linha %d)\n", #a, #b, _a, _b, __LINE__); falhas++; } } while (0)

#define QB 512u                 /* bytes por quadro */
#define NQ0 5u                  /* quadros da primeira parte */
#define NQ1 7u                  /* quadros da segunda parte  */
#define DIR "/tmp/fnf_pack_multi_teste"
#define P0 "dj_teste.cfz4"
#define P1 "dj_teste.cfz4.part1"

static void put32(unsigned char *p, unsigned int v)
{
    p[0] = (unsigned char)(v & 0xffu);
    p[1] = (unsigned char)((v >> 8) & 0xffu);
    p[2] = (unsigned char)((v >> 16) & 0xffu);
    p[3] = (unsigned char)((v >> 24) & 0xffu);
}

/* Bloco LZ4 de literais puros, como o empacotador escreve. */
static unsigned int lz4_literais(const unsigned char *src, unsigned int n, unsigned char *dst)
{
    unsigned int o = 0, resta;
    if (n < 15u) {
        dst[o++] = (unsigned char)(n << 4);
    } else {
        dst[o++] = 0xF0u;
        resta = n - 15u;
        while (resta >= 255u) { dst[o++] = 255u; resta -= 255u; }
        dst[o++] = (unsigned char)resta;
    }
    memcpy(dst + o, src, n);
    return o + n;
}

/* O byte do quadro `q` (indice GLOBAL) na posicao `i`. */
static unsigned char byte_do_quadro(unsigned int q, unsigned int i)
{
    return (unsigned char)((q * 37u + i * 11u + 3u) & 0xffu);
}

/* Escreve uma parte com `q0..q0+nq-1` (indices GLOBAIS) e devolve o tamanho. */
static unsigned int escrever_parte(const char *caminho, unsigned int q0,
                                   unsigned int nq, unsigned int frame_bytes)
{
    static unsigned char cru[QB], comp[QB * 2];
    static unsigned char out[NQ0 * (QB * 2 + 8) + 64];
    unsigned int q, i, o;
    FILE *f;
    memset(out, 0, sizeof out);
    o = 16u + nq * 8u;
    memcpy(out, "CFZ4", 4);
    put32(out + 4, 1u);
    put32(out + 8, nq);
    put32(out + 12, frame_bytes);
    for (q = 0; q < nq; q++) {
        unsigned int n;
        for (i = 0; i < QB; i++)
            cru[i] = (i < frame_bytes) ? byte_do_quadro(q0 + q, i) : 0u;
        n = lz4_literais(cru, frame_bytes, comp);
        memcpy(out + o, comp, n);
        put32(out + 16u + q * 8u, o);
        put32(out + 20u + q * 8u, n);
        o += n;
        if (o > sizeof out) { printf("  FALHA: pacote de teste estourou\n"); exit(1); }
    }
    f = fopen(caminho, "wb");
    if (!f) { printf("  FALHA: nao abri %s\n", caminho); exit(1); }
    fwrite(out, 1, o, f);
    fclose(f);
    return o;
}

int main(void)
{
    static unsigned char saida[QB], esperado[QB];
    static const char *const duas[2] = { P0, P1 };
    static const char *const uma[1] = { P0 };
    static const char *const falta[2] = { P0, "dj_teste.cfz4.part9" };
    pm_pack_t m;
    unsigned int q, i;
    char buf[256];

    printf("== pacote em PARTES (pack_multi) ==\n");

    snprintf(buf, sizeof buf, "mkdir -p %s", DIR);
    if (system(buf) != 0) { printf("  FALHA: nao criei %s\n", DIR); return 1; }

    snprintf(buf, sizeof buf, "%s/%s", DIR, P0);
    escrever_parte(buf, 0, NQ0, QB);
    snprintf(buf, sizeof buf, "%s/%s", DIR, P1);
    escrever_parte(buf, NQ0, NQ1, QB);

    /* ---- as duas partes viram UM pacote, na ordem do plano --------------- */
    CHECK_EQ(pm_abrir(DIR, duas, 2, &m), 0);
    CHECK_EQ(m.n, 2);
    CHECK_EQ(m.total, NQ0 + NQ1);
    CHECK_EQ(m.frame_bytes, QB);
    CHECK_EQ(m.parte[0].primeiro, 0u);
    CHECK_EQ(m.parte[1].primeiro, NQ0);
    CHECK_EQ(m.parte[1].quantos, NQ1);
    printf("  %u quadros em 2 partes (a 1a com %u, a 2a com %u), %u B por quadro\n",
           m.total, m.parte[0].quantos, m.parte[1].quantos, m.frame_bytes);
    /* A RAM e' a tabela das partes + um bloco de cada uma, nunca o pacote. */
    printf("  RAM das tabelas + blocos: %u B\n", pm_ram(&m));

    for (q = 0; q < m.total; q++) {
        for (i = 0; i < QB; i++) esperado[i] = byte_do_quadro(q, i);
        CHECK_EQ(pm_quadro(&m, q, saida), 0);
        if (memcmp(esperado, saida, QB) != 0) {
            printf("  FALHA: quadro %u saiu diferente do empacotado\n", q);
            falhas++;
        }
    }
    /* A travessia e' o ponto do teste: o ultimo da 1a e o primeiro da 2a. */
    for (i = 0; i < QB; i++) esperado[i] = byte_do_quadro(NQ0 - 1u, i);
    CHECK_EQ(pm_quadro(&m, NQ0 - 1u, saida), 0);
    CHECK(memcmp(esperado, saida, QB) == 0);
    for (i = 0; i < QB; i++) esperado[i] = byte_do_quadro(NQ0, i);
    CHECK_EQ(pm_quadro(&m, NQ0, saida), 0);
    CHECK(memcmp(esperado, saida, QB) == 0);
    /* Fora do total: erro nomeado. */
    CHECK_EQ(pm_quadro(&m, m.total, saida), PM_ERR_QUADRO);
    CHECK_EQ(pm_quadro(&m, 99999u, saida), PM_ERR_QUADRO);
    pm_fechar(&m);
    CHECK_EQ(m.n, 0);
    CHECK_EQ(pm_quadro(&m, 0, saida), PM_ERR_VAZIO);   /* fechado nao le' */

    /* ---- erro nomeado em cada estrago ----------------------------------- */
    CHECK_EQ(pm_abrir(DIR, falta, 2, &m), PM_ERR_PARTE);   /* parte ausente */
    CHECK_EQ(pm_abrir(DIR, 0, 0, &m), PM_ERR_VAZIO);       /* sem plano      */
    CHECK_EQ(pm_abrir(DIR, 0, 1, &m), PM_ERR_VAZIO);
    CHECK_EQ(pm_abrir(DIR, uma, 1, &m), 0);                /* uma parte so'  */
    CHECK_EQ(m.total, NQ0);
    pm_fechar(&m);

    /* Parte com OUTRO tamanho de quadro: sao pacotes diferentes, nao um. */
    snprintf(buf, sizeof buf, "%s/%s", DIR, P1);
    escrever_parte(buf, NQ0, NQ1, QB / 2u);
    CHECK_EQ(pm_abrir(DIR, duas, 2, &m), PM_ERR_PARTE);
    CHECK_EQ(m.n, 0);

    printf(falhas ? "\nPACK_MULTI FALHOU (%d)\n" : "\nPACK_MULTI OK\n", falhas);
    return falhas ? 1 : 0;
}
