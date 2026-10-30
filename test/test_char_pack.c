/* O pacote de UM personagem (CHP4): leitura do cabecalho, da tabela de
 * animacoes, e um quadro descomprimido.
 *
 * O que prova: o leitor recusa com erro NOMEADO todo pacote que nao fecha
 * (magic, versao, canvas que nao bate com bytes por quadro, tabela fora do
 * arquivo, animacao apontando para quadro inexistente, bloco LZ4 corrompido) e
 * aceita e entrega o byte certo no pacote bom. O que NAO prova: que a arte real
 * cabe em VRAM nem que o empacotador le' o atlas certo -- isso e' do
 * empacotador, e depende do clone da arte.
 *
 * Os pacotes usados aqui sao montados NO TESTE, com blocos LZ4 "so' literais"
 * (token 0xF0 + extensao), entao o teste nao depende de arquivo nenhum.
 */
#include <stdio.h>
#include <string.h>

#include "char_pack.h"
#include "lz4_bloco.h"

static int falhas;
#define CHECK(c) do { if (!(c)) { printf("  FALHA: %s (linha %d)\n", #c, __LINE__); falhas++; } } while (0)
#define CHECK_EQ(a, b) do { long _a = (long)(a), _b = (long)(b); \
    if (_a != _b) { printf("  FALHA: %s == %s: %ld != %ld (linha %d)\n", #a, #b, _a, _b, __LINE__); falhas++; } } while (0)

static void put32(unsigned char *p, unsigned int v)
{
    p[0] = (unsigned char)(v & 0xffu);
    p[1] = (unsigned char)((v >> 8) & 0xffu);
    p[2] = (unsigned char)((v >> 16) & 0xffu);
    p[3] = (unsigned char)((v >> 24) & 0xffu);
}

static unsigned int get32(const unsigned char *p)
{
    return (unsigned int)p[0] | ((unsigned int)p[1] << 8) |
           ((unsigned int)p[2] << 16) | ((unsigned int)p[3] << 24);
}

/* Bloco LZ4 de literais puros. Devolve quantos bytes escreveu. */
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

#define MAX_Q 8
#define CANVAS_W 8
#define CANVAS_H 4
#define QBYTES (CANVAS_W * CANVAS_H * 4)
#define BUF (256u * 1024u)

static unsigned char g_pacote[BUF];
static unsigned char g_quadro[QBYTES];
static unsigned char g_saida[QBYTES];

typedef struct {
    const char *nome;
    int primeiro, quantos, fps100, looped;
} anim_t;

/* Monta um pacote CHP4 com `nq` quadros de conteudo previsivel e `na` animacoes.
   Devolve o tamanho total em bytes. */
static unsigned int montar(unsigned char *out, int nq, int na, const anim_t *anims,
                           int psm, int w, int h)
{
    unsigned int qbytes = (unsigned int)w * (unsigned int)h * ((unsigned int)psm / 8u);
    unsigned int tabela = CHP_CABECALHO + (unsigned int)nq * 8u;
    unsigned int anims_off = tabela;
    unsigned int blocos = anims_off + (unsigned int)na * (unsigned int)CHP_ANIM_ENTRADA;
    unsigned int o = blocos;
    int i;
    int q;

    memset(out, 0, BUF);
    memcpy(out, CHP_MAGIC, 4);
    put32(out + 4, CHP_VERSAO);
    put32(out + 8, (unsigned int)nq);
    put32(out + 12, qbytes);
    put32(out + 16, (unsigned int)w);
    put32(out + 20, (unsigned int)h);
    put32(out + 24, (unsigned int)na);
    put32(out + 28, (unsigned int)psm);

    for (q = 0; q < nq; q++) {
        unsigned int n;
        memset(g_quadro, 0, sizeof(g_quadro));
        for (i = 0; i < (int)qbytes; i++) g_quadro[i] = (unsigned char)((q * 37 + i * 11) & 0xff);
        n = lz4_literais(g_quadro, qbytes, g_saida);
        memcpy(out + o, g_saida, n);
        put32(out + CHP_CABECALHO + (unsigned int)q * 8u, o);
        put32(out + CHP_CABECALHO + (unsigned int)q * 8u + 4u, n);
        o += n;
    }
    for (i = 0; i < na; i++) {
        unsigned char *e = out + anims_off + (unsigned int)i * CHP_ANIM_ENTRADA;
        memset(e, 0, CHP_ANIM_ENTRADA);
        strncpy((char *)e, anims[i].nome, CHP_NOME - 1);
        put32(e + CHP_NOME, (unsigned int)anims[i].primeiro);
        put32(e + CHP_NOME + 4u, (unsigned int)anims[i].quantos);
        put32(e + CHP_NOME + 8u, (unsigned int)anims[i].fps100);
        put32(e + CHP_NOME + 12u, (unsigned int)anims[i].looped);
    }
    return o;
}

int main(void)
{
    printf("== o pacote bom abre, e cada quadro sai como entrou ==\n");
    {
        anim_t anims[3] = {
            { "idle", 0, 2, 2400, 1 },
            { "singLEFTmiss", 2, 1, 2400, 0 },
            { "danceLeft", 3, 1, 2000, 1 },
        };
        cp_pack_t p;
        unsigned int n = montar(g_pacote, 4, 3, anims, 32, CANVAS_W, CANVAS_H);
        int r = cp_abrir(g_pacote, n, &p);
        int q;
        CHECK_EQ(r, 0);
        CHECK_EQ(p.w, CANVAS_W);
        CHECK_EQ(p.h, CANVAS_H);
        CHECK_EQ(p.psm, 32);
        CHECK_EQ(p.n_quadros, 4);
        CHECK_EQ(p.n_anims, 3);
        CHECK_EQ(p.bytes_por_quadro, QBYTES);
        CHECK_EQ(cp_conferir(&p), 0);

        /* Nome de 12 letras ("singLEFTmiss") tem de sobreviver: e' o mais longo
           que o original usa. */
        CHECK_EQ(cp_anim_i(&p, "singLEFTmiss"), 1);
        CHECK_EQ(cp_anim_i(&p, "idle"), 0);
        CHECK_EQ(cp_anim_i(&p, "singRIGHT"), -1);

        /* O conteudo tem de ser EXATAMENTE o que o empacotador poe: o teste
           monta o quadro com (q*37+i*11) e confere byte a byte. */
        for (q = 0; q < 4; q++) {
            int i, ok = 1;
            CHECK_EQ(cp_quadro(&p, (unsigned int)q, g_quadro), 0);
            for (i = 0; i < (int)QBYTES; i++)
                if (g_quadro[i] != (unsigned char)((q * 37 + i * 11) & 0xff)) { ok = 0; break; }
            CHECK(ok);
        }
        CHECK_EQ(cp_quadro(&p, 4, g_quadro), CHP_ERR_QUADRO);   /* fora do pacote */

        /* A animacao vira ch_seq_t, que e' o que o char_anim.c consome. */
        {
            ch_seq_t s;
            CHECK_EQ(cp_seq(&p, 1, &s), 0);
            CHECK_EQ(s.first, 2);
            CHECK_EQ(s.count, 1);
            CHECK_EQ(s.fps100, 2400);
            CHECK_EQ(s.looped, 0);
            CHECK_EQ(cp_seq(&p, 3, &s), -1);
        }
    }

    printf("== nome repetido: vale a ULTIMA, como o Map do original ==\n");
    {
        /* Caso real da arte: "Boyfriend (Holding GF)" tem dois `singUP`. */
        anim_t anims[3] = {
            { "singUP", 0, 1, 2400, 0 },
            { "idle", 1, 1, 2400, 1 },
            { "singUP", 2, 2, 2400, 0 },
        };
        cp_pack_t p;
        unsigned int n = montar(g_pacote, 4, 3, anims, 32, CANVAS_W, CANVAS_H);
        ch_seq_t s;
        CHECK_EQ(cp_abrir(g_pacote, n, &p), 0);
        CHECK_EQ(cp_anim_i(&p, "singUP"), 2);
        CHECK_EQ(cp_seq(&p, cp_anim_i(&p, "singUP"), &s), 0);
        CHECK_EQ(s.first, 2);
        CHECK_EQ(s.count, 2);
    }

    printf("== PSMCT16: 2 bytes por pixel, e o canvas continua valendo ==\n");
    {
        anim_t anims[1] = { { "idle", 0, 2, 2400, 1 } };
        cp_pack_t p;
        unsigned int n = montar(g_pacote, 2, 1, anims, 16, CANVAS_W, CANVAS_H);
        CHECK_EQ(cp_abrir(g_pacote, n, &p), 0);
        CHECK_EQ(p.bytes_por_quadro, (unsigned int)(CANVAS_W * CANVAS_H * 2));
        CHECK_EQ(p.psm, 16);
        CHECK_EQ(cp_quadro(&p, 1, g_quadro), 0);
    }

    printf("== todo pacote que nao fecha e' recusado, com erro nomeado ==\n");
    {
        anim_t anims[2] = { { "idle", 0, 2, 2400, 1 }, { "singLEFT", 2, 2, 2400, 0 } };
        cp_pack_t p;
        unsigned int n = montar(g_pacote, 4, 2, anims, 32, CANVAS_W, CANVAS_H);
        /* Guarda o pacote bom numa copia, porque cada caso abaixo o estraga. */
        static unsigned char bom[BUF];
        memcpy(bom, g_pacote, n);

        memcpy(g_pacote, bom, n);
        g_pacote[0] = 'X';
        CHECK_EQ(cp_abrir(g_pacote, n, &p), CHP_ERR_MAGIC);

        memcpy(g_pacote, bom, n);
        put32(g_pacote + 4, 2);
        CHECK_EQ(cp_abrir(g_pacote, n, &p), CHP_ERR_VERSAO);

        memcpy(g_pacote, bom, n);
        put32(g_pacote + 12, 999);                 /* bytes por quadro != w*h*4 */
        CHECK_EQ(cp_abrir(g_pacote, n, &p), CHP_ERR_TAMANHO);

        memcpy(g_pacote, bom, n);
        put32(g_pacote + 28, 24);                  /* psm inexistente */
        CHECK_EQ(cp_abrir(g_pacote, n, &p), CHP_ERR_PSM);

        memcpy(g_pacote, bom, n);
        put32(g_pacote + 16, 4096);                /* canvas absurdo */
        CHECK_EQ(cp_abrir(g_pacote, n, &p), CHP_ERR_QUADRO);

        memcpy(g_pacote, bom, n);
        put32(g_pacote + 8, 0);                    /* nenhum quadro */
        CHECK_EQ(cp_abrir(g_pacote, n, &p), CHP_ERR_QUADRO);

        memcpy(g_pacote, bom, n);
        put32(g_pacote + 24, 0);                   /* nenhuma animacao */
        CHECK_EQ(cp_abrir(g_pacote, n, &p), CHP_ERR_ANIMS);
        put32(g_pacote + 24, CHP_MAX_ANIMS + 1);   /* demais */
        CHECK_EQ(cp_abrir(g_pacote, n, &p), CHP_ERR_ANIMS);

        memcpy(g_pacote, bom, n);
        put32(g_pacote + CHP_CABECALHO + 4, n);    /* bloco 0: tamanho alem do fim */
        CHECK_EQ(cp_abrir(g_pacote, n, &p), CHP_ERR_BLOCO);

        /* Cabecalho truncado de verdade. */
        CHECK_EQ(cp_abrir(bom, 20, &p), CHP_ERR_CABECALHO);
    }

    printf("== a tabela de animacoes e' conferida contra os quadros do pacote ==\n");
    {
        anim_t bom_anims[2] = { { "idle", 0, 2, 2400, 1 }, { "singLEFT", 2, 2, 2400, 0 } };
        unsigned int n = montar(g_pacote, 4, 2, bom_anims, 32, CANVAS_W, CANVAS_H);
        static unsigned char copia[BUF];
        memcpy(copia, g_pacote, n);

        /* animacao apontando para quadro que nao existe: o cabecalho abre (a
           conta e' da conferencia), mas cena nao entra.
           anim 1 comeca em CHP_CABECALHO + 4*8 (fim da tabela de quadros) mais
           uma entrada de 40 bytes, e "quantos" fica logo depois do nome. */
        put32(copia + CHP_CABECALHO + 4u * 8u + CHP_ANIM_ENTRADA + CHP_NOME + 4u, 3);
        {
            cp_pack_t p;
            CHECK_EQ(cp_abrir(copia, n, &p), 0);
            CHECK_EQ(cp_conferir(&p), CHP_ERR_TABELA);
        }
        memcpy(copia, g_pacote, n);
        put32(copia + CHP_CABECALHO + 4u * 8u + CHP_ANIM_ENTRADA + CHP_NOME + 4u, 0);
        {
            cp_pack_t p;
            CHECK_EQ(cp_abrir(copia, n, &p), 0);
            CHECK_EQ(cp_conferir(&p), CHP_ERR_TABELA);
        }
    }

    printf("== bloco LZ4 corrompido nao vira quadro meio desenhado ==\n");
    {
        anim_t anims[1] = { { "idle", 0, 2, 2400, 1 } };
        unsigned int n = montar(g_pacote, 2, 1, anims, 32, CANVAS_W, CANVAS_H);
        cp_pack_t p;
        unsigned int off = get32(g_pacote + CHP_CABECALHO);
        CHECK_EQ(cp_abrir(g_pacote, n, &p), 0);
        /* Estraga o token do primeiro bloco: literal maior do que o bloco. */
        g_pacote[off] = 0xF0;
        g_pacote[off + 1] = 200;
        CHECK_EQ(cp_quadro(&p, 0, g_quadro), CHP_ERR_LZ4);
        /* E o segundo quadro continua bom: um quadro ruim nao contamina o resto. */
        CHECK_EQ(cp_quadro(&p, 1, g_quadro), 0);
    }

    printf("== o erro sempre tem nome ==\n");
    {
        int rc;
        for (rc = 0; rc >= -12; rc--) CHECK(cp_erro(rc) != 0);
        CHECK(strcmp(cp_erro(0), "ok") == 0);
        CHECK(cp_erro(-12345)[0] != 0);
    }

    if (falhas) { printf("\n  %d falha(s)\n", falhas); return 1; }
    printf("\n  pacote de personagem: cabecalho, tabela, quadro, e 12 erros nomeados\n");
    return 0;
}
