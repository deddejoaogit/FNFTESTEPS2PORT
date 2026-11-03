/* pack_stream: ler os quadros de um pacote CFZ4 direto do arquivo.
 *
 * O QUE PROVA
 * O jogo lia cada pacote de palco INTEIRO para a RAM e o mantinha la'. Com o
 * teto de 4 MB para todos os props (`PALCO_RAM_MAX`) e a arte real, oito dos 21
 * palcos nao abrem (o pior pacote tem 59,4 MB e o PS2 tem 32 MB). O CFZ4 ja'
 * guarda uma tabela (offset, tamanho) por quadro, e cada quadro e' um bloco LZ4
 * independente -- entao da' para ler um quadro por vez do pendrive.
 *
 * Aqui um pacote de 64 quadros e' montado em disco e aberto pelo leitor novo:
 *  - o quadro sai byte a byte igual ao empacotado, em qualquer ordem e repetido;
 *  - a RAM que ele ocupa e' a TABELA mais UM bloco, nao o arquivo (medido);
 *  - todo arquivo estragado e' recusado com erro NOMEADO: magic, versao, contagem
 *    zero, bytes por quadro absurdo, tabela fora do arquivo, bloco fora do
 *    arquivo, bloco LZ4 corrompido, quadro fora de faixa e leitura de pacote
 *    fechado. Um quadro meio decodificado nao pode chegar ao GS.
 *
 * O que NAO prova: que a leitura do pendrive USB 1.1 e' rapida o bastante (so'
 * o console diz -- 5 a 40 KB por bloco, 3 a 27 ms na taxa medida de 1,5 MB/s).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pack_stream.h"

static int falhas;
#define CHECK(c) do { if (!(c)) { printf("  FALHA: %s (linha %d)\n", #c, __LINE__); falhas++; } } while (0)
#define CHECK_EQ(a, b) do { long _a = (long)(a), _b = (long)(b); \
    if (_a != _b) { printf("  FALHA: %s == %s: %ld != %ld (linha %d)\n", #a, #b, _a, _b, __LINE__); falhas++; } } while (0)

#define NQ 64
#define QB 1024u          /* bytes por quadro */
#define CAMINHO "/tmp/fnf_pack_stream_teste.cfz4"

static void put32(unsigned char *p, unsigned int v)
{
    p[0] = (unsigned char)(v & 0xffu);
    p[1] = (unsigned char)((v >> 8) & 0xffu);
    p[2] = (unsigned char)((v >> 16) & 0xffu);
    p[3] = (unsigned char)((v >> 24) & 0xffu);
}

/* Bloco LZ4 de literais puros (token 0xF0 + extensoes), como o empacotador
   escreve para dado incompressivel. Devolve quantos bytes escreveu. */
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

/* Monta o pacote com NQ quadros previsiveis. Devolve o tamanho do arquivo. */
static unsigned int montar(unsigned char *out, unsigned int *tabela_off, unsigned int *tabela_tam)
{
    static unsigned char cru[QB], comp[QB * 2];
    unsigned int i, o, q;
    o = 16u + NQ * 8u;
    memcpy(out, "CFZ4", 4);
    put32(out + 4, 1u);
    put32(out + 8, NQ);
    put32(out + 12, QB);
    for (q = 0; q < NQ; q++) {
        unsigned int n;
        for (i = 0; i < QB; i++) cru[i] = (unsigned char)((q * 31u + i * 7u) & 0xffu);
        n = lz4_literais(cru, QB, comp);
        memcpy(out + o, comp, n);
        tabela_off[q] = o;
        tabela_tam[q] = n;
        put32(out + 16u + q * 8u, o);
        put32(out + 20u + q * 8u, n);
        o += n;
    }
    return o;
}

int main(void)
{
    static unsigned char pacote[NQ * (QB * 2 + 8) + 64];
    static unsigned int tab_off[NQ], tab_tam[NQ];
    static unsigned char esperado[QB], saida[QB];
    unsigned int n, q, i;
    pst_pack_t p;
    FILE *f;

    printf("== leitor de pacote por quadro (pack_stream) ==\n");

    n = montar(pacote, tab_off, tab_tam);
    f = fopen(CAMINHO, "wb");
    CHECK(f != NULL);
    if (!f) return 1;
    CHECK_EQ(fwrite(pacote, 1, n, f), n);
    fclose(f);

    /* ---- o pacote bom abre, e cada quadro sai como entrou (fora de ordem) */
    CHECK_EQ(pst_abrir(CAMINHO, &p), PST_OK);
    CHECK_EQ(p.count, NQ);
    CHECK_EQ(p.frame_bytes, QB);
    /* A RAM e' a tabela + o maior bloco + os dois vetores; o arquivo e' maior.
       Sem isso, "streaming" seria so' um nome. */
    CHECK(p.ram < n / 2u);
    printf("  arquivo %u B, RAM do pacote aberto %u B (tabela + 1 bloco)\n", n, p.ram);
    for (q = 0; q < NQ; q += 7u) {
        for (i = 0; i < QB; i++) esperado[i] = (unsigned char)((q * 31u + i * 7u) & 0xffu);
        CHECK_EQ(pst_quadro(&p, q, saida), PST_OK);
        CHECK(memcmp(esperado, saida, QB) == 0);
    }
    /* O mesmo quadro duas vezes seguidas: o buffer de leitura e' reaproveitado. */
    CHECK_EQ(pst_quadro(&p, 5, saida), PST_OK);
    for (i = 0; i < QB; i++) esperado[i] = (unsigned char)((5u * 31u + i * 7u) & 0xffu);
    CHECK(memcmp(esperado, saida, QB) == 0);
    CHECK_EQ(pst_quadro(&p, NQ, saida), PST_ERR_QUADRO);
    pst_fechar(&p);
    CHECK_EQ(pst_quadro(&p, 0, saida), PST_ERR_ABRIR);   /* fechado: nao le' */

    /* ---- o MESMO pacote lido da RAM (`pst_abrir_mem`) --------------------
       E' o caminho do `endStuff` do sserafim: os dois `.cfz4` do fim da musica
       vao para a RAM COMPRIMIDOS no comeco da cutscene (39 951 + 10 102 B na
       arte real) e descomprimem no instante do evento, para nao ler 283 504 B
       do pendrive nos ultimos 9 s da musica. Aqui se cobra que ele de' o mesmo
       byte e a mesma recusa que o caminho do arquivo -- nao e' um atalho sem
       validacao. */
    {
        pst_pack_t pf, pm;
        unsigned int ram_ram;
        CHECK_EQ(pst_abrir(CAMINHO, &pf), PST_OK);
        CHECK_EQ(pst_abrir_mem(pacote, n, &pm), PST_OK);
        CHECK_EQ(pm.count, pf.count);
        CHECK_EQ(pm.frame_bytes, pf.frame_bytes);
        CHECK_EQ(pm.bytes, pf.bytes);
        CHECK_EQ(pm.ram, pf.ram);        /* mesma tabela, mesmo bloco */
        CHECK(pm.fd < 0);                /* nenhum arquivo aberto       */
        for (q = 0; q < NQ; q += 11u) {
            for (i = 0; i < QB; i++) esperado[i] = (unsigned char)((q * 31u + i * 7u) & 0xffu);
            CHECK_EQ(pst_quadro(&pf, q, saida), PST_OK);
            CHECK_EQ(pst_quadro(&pm, q, saida), PST_OK);
            CHECK(memcmp(esperado, saida, QB) == 0);
        }
        CHECK_EQ(pst_quadro(&pm, NQ, saida), PST_ERR_QUADRO);
        ram_ram = pm.ram;
        pst_fechar(&pm);
        CHECK_EQ(pst_quadro(&pm, 0, saida), PST_ERR_ABRIR);  /* fechado: nao le' */
        /* O buffer e' do CHAMADOR: fechar NAO o invalida, entao reabrir do
           mesmo buffer tem de funcionar (e' assim que o `endStuff` usa). */
        CHECK_EQ(pst_abrir_mem(pacote, n, &pm), PST_OK);
        CHECK_EQ(pst_quadro(&pm, 1, saida), PST_OK);
        for (i = 0; i < QB; i++) esperado[i] = (unsigned char)((1u * 31u + i * 7u) & 0xffu);
        CHECK(memcmp(esperado, saida, QB) == 0);
        pst_fechar(&pm);

        /* Recusas do caminho da RAM, com erro nomeado. */
        CHECK_EQ(pst_abrir_mem(0, n, &pm), PST_ERR_CABECALHO);
        CHECK_EQ(pst_abrir_mem(pacote, 0u, &pm), PST_ERR_CABECALHO);
        CHECK_EQ(pst_abrir_mem(pacote, 16u, &pm), PST_ERR_CABECALHO);
        /* Tabela cortada no meio: 32 quadros de tabela para 64 anunciados. */
        CHECK_EQ(pst_abrir_mem(pacote, 16u + 8u * (NQ / 2u), &pm), PST_ERR_TABELA);
        pacote[0] = 'X';
        CHECK_EQ(pst_abrir_mem(pacote, n, &pm), PST_ERR_CABECALHO);
        pacote[0] = 'C';
        /* Bloco LZ4 corrompido dentro do buffer: erro, nunca meio quadro. */
        {
            unsigned char guardado = pacote[tab_off[3] + 4u];
            pacote[tab_off[3] + 4u] = (unsigned char)(guardado ^ 0xFFu);
            if (pst_abrir_mem(pacote, n, &pm) == PST_OK) {
                CHECK_EQ(pst_quadro(&pm, 3, saida), PST_ERR_LZ4);
                pst_fechar(&pm);
            }
            pacote[tab_off[3] + 4u] = guardado;
        }
        pst_fechar(&pf);
        printf("  RAM: mesmos quadros, mesma RAM (%u B) e as mesmas recusas\n", ram_ram);
    }

    /* ---- todo arquivo estragado e' recusado, com erro nomeado ------------ */
    {
        static unsigned char ruim[sizeof pacote];

        memcpy(ruim, pacote, n); ruim[0] = 'X';
        f = fopen(CAMINHO, "wb"); fwrite(ruim, 1, n, f); fclose(f);
        CHECK_EQ(pst_abrir(CAMINHO, &p), PST_ERR_CABECALHO);

        memcpy(ruim, pacote, n); put32(ruim + 4, 2u);
        f = fopen(CAMINHO, "wb"); fwrite(ruim, 1, n, f); fclose(f);
        CHECK_EQ(pst_abrir(CAMINHO, &p), PST_ERR_CABECALHO);

        memcpy(ruim, pacote, n); put32(ruim + 8, 0u);
        f = fopen(CAMINHO, "wb"); fwrite(ruim, 1, n, f); fclose(f);
        CHECK_EQ(pst_abrir(CAMINHO, &p), PST_ERR_QUADRO);

        memcpy(ruim, pacote, n); put32(ruim + 12, 0u);
        f = fopen(CAMINHO, "wb"); fwrite(ruim, 1, n, f); fclose(f);
        CHECK_EQ(pst_abrir(CAMINHO, &p), PST_ERR_QUADRO);

        /* Tabela fora do arquivo: cabecalho diz 64 quadros, arquivo tem 4. */
        memcpy(ruim, pacote, n); put32(ruim + 8, 64u);
        f = fopen(CAMINHO, "wb"); fwrite(ruim, 1, 20u, f); fclose(f);
        CHECK_EQ(pst_abrir(CAMINHO, &p), PST_ERR_TABELA);

        /* Bloco fora do arquivo: o offset do quadro 0 passa do fim. */
        memcpy(ruim, pacote, n); put32(ruim + 16, n + 100u);
        f = fopen(CAMINHO, "wb"); fwrite(ruim, 1, n, f); fclose(f);
        CHECK_EQ(pst_abrir(CAMINHO, &p), PST_ERR_BLOCO);

        /* Bloco LZ4 que nao fecha no tamanho: a tabela diz que o quadro 3 tem
           100 bytes (esta' dentro do arquivo, entao o pacote ABRE), e o
           decodificador nao consegue produzir os 1024 do quadro. */
        memcpy(ruim, pacote, n);
        put32(ruim + 20u + 3u * 8u, 100u);
        f = fopen(CAMINHO, "wb"); fwrite(ruim, 1, n, f); fclose(f);
        CHECK_EQ(pst_abrir(CAMINHO, &p), PST_OK);
        CHECK_EQ(pst_quadro(&p, 3, saida), PST_ERR_LZ4);
        /* E o resto do pacote continua bom: um quadro ruim nao contamina. */
        CHECK_EQ(pst_quadro(&p, 4, saida), PST_OK);
        pst_fechar(&p);

        /* Arquivo que nao existe. */
        CHECK_EQ(pst_abrir("/tmp/fnf_pack_stream_nao_existe.cfz4", &p), PST_ERR_ABRIR);
    }

    /* ---- o erro sempre tem nome */
    CHECK(strcmp(pst_erro(PST_ERR_LZ4), "ok") != 0);
    CHECK(strstr(pst_erro(PST_ERR_BLOCO), "bloco") != NULL);
    CHECK(strstr(pst_erro(PST_ERR_TABELA), "tabela") != NULL);

    remove(CAMINHO);
    if (falhas) { printf("  %d falha(s)\n", falhas); return 1; }
    printf("  todas as conferencias passaram\n");
    return 0;
}
