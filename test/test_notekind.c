/* O que um NOTE KIND faz, e o que o port faz com ele.
 *
 * Por que este teste existe: o port passou varias builds reportando 32 dos 34
 * kinds dos charts como "script nao esta' no pacote de dados". Estava errado --
 * os scripts estao no clone da arte e `tools/gen_notekinds.py` os le'. O que
 * este teste prende:
 *
 *   1. os casos que o DADO diz, um por tipo: canto com sufixo (`mom` -> `alt`)
 *      e animacao nomeada (`ugh` -> `ugh`), com os dois personagens reais;
 *   2. o que o dado NAO diz: personagem que nao implementa o kind, kind que o
 *      dado nao conhece, e nulo -- nos tres casos a resposta e' "nada", nunca
 *      um palpite;
 *   3. `cc_canto_sufixo` toca `sing<DIR>-<sufixo>` e NAO cai no canto normal
 *      quando o personagem nao tem a animacao com sufixo (e' o que o original
 *      faz: um playAnimation de nome inexistente so' avisa);
 *   4. a animacao nomeada do kind segura o personagem ate' acabar, e depois ele
 *      volta a dancar -- sem isso o personagem ficaria preso no `block`.
 */
#include <stdio.h>
#include <string.h>

#include "char_cena.h"
#include "notekind.h"

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

#define W 4
#define H 2
#define QB (W * H * 4)
#define BUF (64u * 1024u)
static unsigned char g_pac[BUF];

typedef struct { const char *nome; int primeiro, quantos, fps100, looped; } anim_t;

/* Pacote CHP4 minimo, com blocos LZ4 de literais puros (igual ao
   test_char_pack): o formato tem de abrir pelo leitor de verdade. */
static unsigned int montar(int nq, int na, const anim_t *anims)
{
    unsigned int tabela = CHP_CABECALHO + (unsigned int)nq * 8u;
    unsigned int o = tabela + (unsigned int)na * CHP_ANIM_ENTRADA;
    int i, q;
    memset(g_pac, 0, BUF);
    memcpy(g_pac, CHP_MAGIC, 4);
    put32(g_pac + 4, CHP_VERSAO);
    put32(g_pac + 8, (unsigned int)nq);
    put32(g_pac + 12, QB);
    put32(g_pac + 16, W);
    put32(g_pac + 20, H);
    put32(g_pac + 24, (unsigned int)na);
    put32(g_pac + 28, 32);
    for (q = 0; q < nq; q++) {
        unsigned char *d = g_pac + o;
        unsigned int k = 0, resta = QB;
        if (QB < 15u) {
            d[k++] = (unsigned char)(QB << 4);
        } else {
            d[k++] = 0xF0u;
            resta = QB - 15u;
            while (resta >= 255u) { d[k++] = 255u; resta -= 255u; }
            d[k++] = (unsigned char)resta;
        }
        put32(g_pac + CHP_CABECALHO + (unsigned int)q * 8u, o);
        put32(g_pac + CHP_CABECALHO + (unsigned int)q * 8u + 4u, k + QB);
        memset(d + k, q & 0xff, QB);
        o += k + QB;
    }
    for (i = 0; i < na; i++) {
        unsigned char *e = g_pac + tabela + (unsigned int)i * CHP_ANIM_ENTRADA;
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
    printf("== o dado: os casos que o script do original escreve ==\n");
    {
        const nk_acao_t *a;
        /* pais de Natal: `case "mom": playSingAnimation(dir, false, 'alt')` */
        a = nk_acao("parents-christmas", "mom");
        CHECK(a != 0);
        if (a) {
            CHECK_EQ(a->tipo, 1);
            CHECK(strcmp(a->valor, "alt") == 0);
        }
        /* tankman: `case "ugh": this.playAnimation('ugh', true, true)` */
        a = nk_acao("tankman", "ugh");
        CHECK(a != 0);
        if (a) {
            CHECK_EQ(a->tipo, 2);
            CHECK(strcmp(a->valor, "ugh") == 0);
        }
        /* Sakura: canto com sufixo, e o kind NAO tem classe NoteKind -- o
           comportamento vem do script do personagem, e o teste exige que ele
           exista mesmo sem registro. */
        a = nk_acao("sserafim-sakura", "sakura-joint");
        CHECK(a != 0);
        if (a) {
            CHECK_EQ(a->tipo, 1);
            CHECK(strcmp(a->valor, "joint") == 0);
        }
        /* e o nome de exibicao? existe para os registrados, e nao para o kind
           que ninguem registrou -- NULL e' a resposta certa, nao um nome
           inventado. */
        CHECK(nk_nome("mom") != 0);
        CHECK(nk_nome("sakura-joint") == 0);
        CHECK(nk_nome("noanim") == 0);      /* kind de codigo, nao de script */
    }

    printf("== o que o dado nao diz: nada e' inventado ==\n");
    {
        CHECK(nk_acao("bf", "mom") == 0);            /* bf nao implementa `mom` */
        CHECK(nk_acao("parents-christmas", "ugh") == 0);
        CHECK(nk_acao("", "mom") == 0);
        CHECK(nk_acao("parents-christmas", "") == 0);
        CHECK(nk_acao(0, "mom") == 0);
        CHECK(nk_acao("parents-christmas", 0) == 0);
        CHECK(nk_nome("weekend-1-naoexiste") == 0);
        CHECK(nk_kind_count() >= 31);
        CHECK(nk_acao_count() >= 60);
    }

    printf("== cc_canto_sufixo: sing<DIR>-<sufixo>, e nada quando nao existe ==\n");
    {
        const anim_t alt_anims[3] = {
            { "danceLeft", 0, 1, 2400, 1 },
            { "singUP", 1, 1, 2400, 0 },
            { "singUP-alt", 2, 1, 2400, 0 },
        };
        const anim_t sem_alt[2] = {
            { "danceLeft", 0, 1, 2400, 1 },
            { "singUP", 1, 1, 2400, 0 },
        };
        cc_cena_t c;
        unsigned int n = montar(4, 3, alt_anims);
        cc_reset(&c);
        CHECK_EQ(cc_anexar(&c, CH_PAPEL_DAD, g_pac, n, 0), 0);
        CHECK_EQ(cc_canto_sufixo(&c, CH_PAPEL_DAD, 2, 0, "alt", 100, 500), 1);
        CHECK_EQ(cc_anim(&c, CH_PAPEL_DAD), 2);      /* a animacao COM sufixo */
        /* direcao sem essa animacao: nao toca nada e o anim atual NAO muda */
        CHECK_EQ(cc_canto_sufixo(&c, CH_PAPEL_DAD, 0, 0, "alt", 200, 500), 0);
        CHECK_EQ(cc_anim(&c, CH_PAPEL_DAD), 2);
        /* sufixo vazio e sufixo absurdo: recusa, sem estourar o nome */
        CHECK_EQ(cc_canto_sufixo(&c, CH_PAPEL_DAD, 2, 0, "", 300, 500), 0);
        CHECK_EQ(cc_canto_sufixo(&c, CH_PAPEL_DAD, 2, 0,
                                 "sufixo-bem-maior-que-o-nome", 300, 500), 0);
        /* personagem sem a animacao com sufixo: devolve 0 e nao toca o normal */
        n = montar(2, 2, sem_alt);
        cc_reset(&c);
        CHECK_EQ(cc_anexar(&c, CH_PAPEL_DAD, g_pac, n, 0), 0);
        cc_canto(&c, CH_PAPEL_DAD, 2, 0, 0, 500);   /* toca o canto normal */
        CHECK_EQ(cc_anim(&c, CH_PAPEL_DAD), 1);     /* "singUP", o sem sufixo */
        CHECK_EQ(cc_canto_sufixo(&c, CH_PAPEL_DAD, 2, 0, "alt", 0, 500), 0);
        CHECK_EQ(cc_anim(&c, CH_PAPEL_DAD), 1);      /* nada mudou: nao caiu no normal */
    }

    printf("== animacao nomeada do kind: segura ate' acabar, depois danca ==\n");
    {
        /* 2 quadros a 24 fps = 83 ms: tempo de sobra para ver a diferenca. */
        const anim_t blk[4] = {
            { "danceLeft", 0, 1, 2400, 1 },
            { "danceRight", 1, 1, 2400, 1 },
            { "block", 2, 2, 2400, 0 },
            { "singLEFT", 0, 1, 2400, 0 },
        };
        cc_cena_t c;
        unsigned int n = montar(4, 4, blk);
        cc_reset(&c);
        CHECK_EQ(cc_anexar(&c, CH_PAPEL_DAD, g_pac, n, 0), 0);
        CHECK_EQ(cc_toca(&c, CH_PAPEL_DAD, "block", 1000), 1);
        CHECK_EQ(cc_anim(&c, CH_PAPEL_DAD), 2);
        /* passo de danca durante o block: o block NAO e' interrompido */
        CHECK_EQ(cc_anim(&c, CH_PAPEL_DAD), 2);
        CHECK(cc_quadro(&c, CH_PAPEL_DAD, 1040, 4) >= 0);
        CHECK_EQ(cc_anim(&c, CH_PAPEL_DAD), 2);
        /* depois de 83 ms a animacao acabou: volta a dancar (danceLeft) */
        CHECK(cc_quadro(&c, CH_PAPEL_DAD, 1090, 5) >= 0);
        CHECK_EQ(cc_anim(&c, CH_PAPEL_DAD), 0);
    }

    if (falhas) { printf("\n  %d falha(s)\n", falhas); return 1; }
    printf("\n  notekind: o dado dos 35 kinds de script, o canto com sufixo e o que nao existe\n");
    return 0;
}
