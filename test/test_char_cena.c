/* A maquina de estado de um personagem em cena (char_cena).
 *
 * O que prova: danca pela batida, canta por singTime e volta, erro de nota usa
 * `miss` e cai no nome-base quando nao existe, o MESMO canto nao reinicia mas
 * estende o tempo, papel sem pacote nao desenha nada (arte nao se inventa), e o
 * quadro so' e' pedido quando ele MUDA -- que e' o que decide o custo real
 * (decodificar LZ4 e subir para a VRAM).
 *
 * Os pacotes sao montados aqui, com blocos LZ4 "so' literais", igual ao
 * test_char_pack.
 */
#include <stdio.h>
#include <string.h>

#include "char_cena.h"

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
        /* bloco de literais puros: token + n bytes */
        unsigned char *d = g_pac + o;
        d[0] = (unsigned char)(QB < 15u ? (QB << 4) : 0xF0u);
        if (QB >= 15u) {
            unsigned int resta = QB - 15u, k = 1;
            while (resta >= 255u) { d[k++] = 255u; resta -= 255u; }
            d[k++] = (unsigned char)resta;
            put32(g_pac + CHP_CABECALHO + (unsigned int)q * 8u, o);
            put32(g_pac + CHP_CABECALHO + (unsigned int)q * 8u + 4u, k + QB);
            memset(d + k, q & 0xff, QB);
            o += k + QB;
        }
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
    /* BF-like: idle 4 quadros a 24 fps (166 ms), canta 2 quadros, tem miss. */
    const anim_t bf_anims[4] = {
        { "idle", 0, 4, 2400, 1 },
        { "singLEFT", 4, 2, 2400, 0 },
        { "singLEFTmiss", 6, 1, 2400, 0 },
        { "danceRight", 7, 1, 2400, 1 },
    };
    /* GF-like: so' danceLeft/danceRight (nao tem idle) e canta UP sem miss. */
    const anim_t gf_anims[3] = {
        { "danceLeft", 0, 1, 2400, 1 },
        { "danceRight", 1, 1, 2400, 1 },
        { "singUP", 2, 1, 2400, 0 },
    };

    printf("== papel sem pacote nao desenha nada ==\n");
    {
        cc_cena_t c;
        cc_reset(&c);
        CHECK_EQ(cc_quadro(&c, CH_PAPEL_DAD, 0, 0), -1);
        CHECK_EQ(cc_quadro(&c, CH_PAPEL_DAD, 1000, 1), -1);
        cc_canto(&c, CH_PAPEL_DAD, 0, 0, 0, 500);
        CHECK_EQ(cc_quadro(&c, CH_PAPEL_DAD, 10, 0), -1);
        CHECK_EQ(cc_anim(&c, CH_PAPEL_DAD), -1);
    }

    printf("== pacote que nao confere deixa o papel inativo ==\n");
    {
        cc_cena_t c;
        unsigned int n = montar(8, 4, bf_anims);
        cc_reset(&c);
        put32(g_pac + CHP_CABECALHO + 4u * 8u + 32u + CHP_NOME + 4u, 99);  /* quantos fora da faixa */
        CHECK_EQ(cc_anexar(&c, CH_PAPEL_BF, g_pac, n, 0), CHP_ERR_TABELA);
        CHECK_EQ(cc_quadro(&c, CH_PAPEL_BF, 0, 0), -1);
        /* e um magic errado tambem */
        n = montar(8, 4, bf_anims);
        g_pac[0] = 'Z';
        CHECK_EQ(cc_anexar(&c, CH_PAPEL_BF, g_pac, n, 0), CHP_ERR_MAGIC);
        CHECK_EQ(cc_quadro(&c, CH_PAPEL_BF, 0, 0), -1);
    }

    printf("== parado: danca em idle, e so' pede quadro quando ele muda ==\n");
    {
        cc_cena_t c;
        unsigned int n = montar(8, 4, bf_anims);
        int q0, q1, q2;
        cc_reset(&c);
        CHECK_EQ(cc_anexar(&c, CH_PAPEL_BF, g_pac, n, 0), 0);
        q0 = cc_quadro(&c, CH_PAPEL_BF, 0, 0);
        CHECK_EQ(q0, 0);
        CHECK_EQ(cc_quadro(&c, CH_PAPEL_BF, 10, 0), -1);    /* mesmo quadro: nada a subir */
        q1 = cc_quadro(&c, CH_PAPEL_BF, 50, 0);             /* 24 fps: 41,7 ms por quadro */
        CHECK_EQ(q1, 1);
        q2 = cc_quadro(&c, CH_PAPEL_BF, 125, 0);
        CHECK_EQ(q2, 3);
        /* loop: 166 ms depois volta ao primeiro */
        CHECK_EQ(cc_quadro(&c, CH_PAPEL_BF, 167, 0), 0);
    }

    printf("== canto: dura o singTime, e depois volta a dancar ==\n");
    {
        cc_cena_t c;
        unsigned int n = montar(8, 4, bf_anims);
        cc_reset(&c);
        CHECK_EQ(cc_anexar(&c, CH_PAPEL_BF, g_pac, n, 0), 0);
        cc_quadro(&c, CH_PAPEL_BF, 0, 0);
        cc_canto(&c, CH_PAPEL_BF, 0, 0, 100, 200);          /* singLEFT por 200 ms */
        CHECK_EQ(cc_anim(&c, CH_PAPEL_BF), 1);
        CHECK_EQ(cc_quadro(&c, CH_PAPEL_BF, 100, 0), 4);    /* primeiro quadro do canto */
        CHECK_EQ(cc_quadro(&c, CH_PAPEL_BF, 120, 0), -1);   /* 20 ms: ainda no quadro 4 */
        CHECK_EQ(cc_quadro(&c, CH_PAPEL_BF, 142, 0), 5);    /* 42 ms -> segundo quadro */
        /* o mesmo canto de novo NAO reinicia a animacao (quadro continua), mas
           estende o tempo -- e' o que o original faz com notas no mesmo instante */
        cc_canto(&c, CH_PAPEL_BF, 0, 0, 160, 200);
        CHECK_EQ(cc_anim(&c, CH_PAPEL_BF), 1);
        /* antes do fim estendido (360 ms) ainda canta */
        CHECK_EQ(cc_anim(&c, CH_PAPEL_BF), 1);
        /* depois, volta ao idle */
        cc_quadro(&c, CH_PAPEL_BF, 400, 0);
        CHECK_EQ(cc_anim(&c, CH_PAPEL_BF), 0);
    }

    printf("== erro de nota usa miss, e cai no normal quando nao existe ==\n");
    {
        cc_cena_t c;
        unsigned int n = montar(8, 4, bf_anims);
        cc_reset(&c);
        CHECK_EQ(cc_anexar(&c, CH_PAPEL_BF, g_pac, n, 0), 0);
        cc_canto(&c, CH_PAPEL_BF, 0, 1, 0, 200);
        CHECK_EQ(cc_anim(&c, CH_PAPEL_BF), 2);              /* singLEFTmiss */
        /* GF-like nao tem singUPmiss: cai no singUP */
        n = montar(4, 3, gf_anims);
        CHECK_EQ(cc_anexar(&c, CH_PAPEL_GF, g_pac, n, 0), 0);
        cc_canto(&c, CH_PAPEL_GF, 2, 1, 0, 200);
        CHECK_EQ(cc_anim(&c, CH_PAPEL_GF), 2);              /* singUP */
    }

    printf("== animacao nomeada nao e' interrompida, e o '-end' fecha o canto ==\n");
    {
        cc_cena_t c;
        const anim_t a[5] = {
            { "idle", 0, 2, 2400, 1 },
            { "hey", 2, 2, 1000, 0 },      /* 100 ms por quadro: acaba em 200 ms */
            { "singLEFT", 4, 2, 2400, 0 },
            { "singLEFT-end", 6, 1, 2400, 0 },
            { "danceRight", 7, 1, 2400, 1 },
        };
        unsigned int n = montar(8, 5, a);
        cc_reset(&c);
        CHECK_EQ(cc_anexar(&c, CH_PAPEL_DAD, g_pac, n, 0), 0);
        CHECK_EQ(cc_quadro(&c, CH_PAPEL_DAD, 0, 0), 0);      /* idle */
        /* "hey" toca e NAO volta ao idle na batida seguinte */
        CHECK_EQ(cc_toca(&c, CH_PAPEL_DAD, "hey", 10), 1);
        CHECK_EQ(cc_anim(&c, CH_PAPEL_DAD), 1);
        CHECK_EQ(cc_quadro(&c, CH_PAPEL_DAD, 60, 1), 2);
        CHECK_EQ(cc_anim(&c, CH_PAPEL_DAD), 1);              /* continua */
        /* e volta so' quando ela ACABA (200 ms depois de comecar) */
        /* 211 ms: o "hey" acabou (200 ms) e o idle volta, com quadro novo. */
        CHECK_EQ(cc_quadro(&c, CH_PAPEL_DAD, 211, 1), 0);
        CHECK_EQ(cc_anim(&c, CH_PAPEL_DAD), 0);
        /* animacao que o personagem nao tem: nao inventa */
        CHECK_EQ(cc_toca(&c, CH_PAPEL_DAD, "cheer", 300), 0);
        CHECK_EQ(cc_anim(&c, CH_PAPEL_DAD), 0);
        /* canto que acaba: se existe "singLEFT-end", ela toca no lugar do idle */
        cc_canto(&c, CH_PAPEL_DAD, 0, 0, 400, 200);
        CHECK_EQ(cc_anim(&c, CH_PAPEL_DAD), 2);
        CHECK_EQ(cc_quadro(&c, CH_PAPEL_DAD, 601, 0), 6);    /* 1o quadro do -end */
        CHECK_EQ(cc_anim(&c, CH_PAPEL_DAD), 3);              /* singLEFT-end */
    }

    printf("== \"-hold\": olhos do pai e chama do Darnell ==\n");
    {
        /* Caso real do dad.json: idle (nao-loop, 2 quadros a 24 fps) e
           idle-hold (loop). Quando o idle ACABA, entra o idle-hold e fica --
           e' isso que faz os olhos piscarem. */
        cc_cena_t c;
        const anim_t pai[3] = {
            { "idle", 0, 2, 2400, 0 },        /* 2 quadros = 83 ms */
            { "idle-hold", 2, 2, 2400, 1 },   /* em loop */
            { "singUP", 4, 1, 2400, 0 },
        };
        unsigned int n = montar(5, 3, pai);
        cc_reset(&c);
        CHECK_EQ(cc_anexar(&c, CH_PAPEL_DAD, g_pac, n, 0), 0);
        CHECK_EQ(cc_quadro(&c, CH_PAPEL_DAD, 0, 0), 0);       /* idle, 1o quadro */
        CHECK_EQ(cc_anim(&c, CH_PAPEL_DAD), 0);
        CHECK_EQ(cc_quadro(&c, CH_PAPEL_DAD, 45, 0), 1);      /* 2o quadro */
        /* 83 ms: o idle terminou -> idle-hold, e ela CONTINUA la' depois */
        CHECK_EQ(cc_quadro(&c, CH_PAPEL_DAD, 90, 0), 2);
        CHECK_EQ(cc_anim(&c, CH_PAPEL_DAD), 1);
        CHECK_EQ(cc_quadro(&c, CH_PAPEL_DAD, 500, 0), 3);
        CHECK_EQ(cc_anim(&c, CH_PAPEL_DAD), 1);
        /* 2 quadros a 24 fps: um ciclo a cada 83 ms. 84 ms depois do inicio do
           "-hold" ela ja' voltou ao primeiro quadro. */
        CHECK_EQ(cc_quadro(&c, CH_PAPEL_DAD, 174, 0), 2);     /* loop volta */
        /* e um canto interrompe o -hold, como no original */
        cc_canto(&c, CH_PAPEL_DAD, 2, 0, 6000, 300);
        CHECK_EQ(cc_anim(&c, CH_PAPEL_DAD), 2);
        CHECK_EQ(cc_quadro(&c, CH_PAPEL_DAD, 6000, 0), 4);
    }

    printf("== quem nao tem idle alterna danceLeft/danceRight pela batida ==\n");
    {
        cc_cena_t c;
        unsigned int n = montar(4, 3, gf_anims);
        cc_reset(&c);
        CHECK_EQ(cc_anexar(&c, CH_PAPEL_GF, g_pac, n, 0), 0);
        CHECK_EQ(cc_quadro(&c, CH_PAPEL_GF, 0, 0), 0);      /* danceLeft  */
        CHECK_EQ(cc_anim(&c, CH_PAPEL_GF), 0);
        CHECK_EQ(cc_quadro(&c, CH_PAPEL_GF, 10, 1), 1);     /* batida 1: danceRight */
        CHECK_EQ(cc_anim(&c, CH_PAPEL_GF), 1);
    }

    printf("== personagem de fundo (sem canto) nao inventa animacao ==\n");
    {
        cc_cena_t c;
        const anim_t fundo[1] = { { "idle", 0, 2, 2400, 1 } };
        unsigned int n = montar(2, 1, fundo);
        cc_reset(&c);
        CHECK_EQ(cc_anexar(&c, CH_PAPEL_DAD, g_pac, n, 0), 0);
        CHECK_EQ(cc_quadro(&c, CH_PAPEL_DAD, 0, 0), 0);
        cc_canto(&c, CH_PAPEL_DAD, 0, 0, 0, 200);
        CHECK_EQ(cc_anim(&c, CH_PAPEL_DAD), 0);             /* continua no idle */
        CHECK_EQ(cc_quadro(&c, CH_PAPEL_DAD, 10, 0), -1);   /* e nao reinicia */
    }

    if (falhas) { printf("\n  %d falha(s)\n", falhas); return 1; }
    printf("\n  cena: idle pela batida, canto com singTime, miss com queda, e o quadro so' quando muda\n");
    return 0;
}
