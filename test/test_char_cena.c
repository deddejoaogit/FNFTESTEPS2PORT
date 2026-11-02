/* A maquina de estado de um personagem em cena (char_cena).
 *
 * O que prova: danca no passo que o danceEvery manda, canta por singTime e volta, erro de nota usa
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
        CHECK_EQ(cc_quadro(&c, CH_PAPEL_DAD, 1000, 4), -1);
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
        CHECK_EQ(cc_quadro(&c, CH_PAPEL_DAD, 60, 4), 2);
        CHECK_EQ(cc_anim(&c, CH_PAPEL_DAD), 1);              /* continua */
        /* e volta so' quando ela ACABA (200 ms depois de comecar) */
        /* 211 ms: o "hey" acabou (200 ms) e o idle volta, com quadro novo. */
        CHECK_EQ(cc_quadro(&c, CH_PAPEL_DAD, 211, 4), 0);
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

    printf("== `cc_toca_forcado` e o `danceEvery = 0` (as dancarinas do sserafim) ==\n");
    {
        /* `sserafim.hxc` L230: `playAnimation('kick2', true, false)` -- o
           `forced` reinicia mesmo que a animacao ja' seja essa. Sem isso dois
           chutes seguidos apareciam como um so'. E L943: `danceEvery = 0`
           poe a yunjin parada no ultimo quadro da animacao (a porta fechada),
           em vez de voltar a dancar. */
        cc_cena_t c;
        const anim_t a[3] = {
            { "doorclosed", 0, 2, 2400, 1 },  /* 41,7 ms por quadro */
            { "kick1", 2, 3, 2400, 0 },       /* 125 ms por quadro  */
            { "kick2", 5, 3, 2400, 0 },
        };
        unsigned int n = montar(8, 3, a);
        cc_reset(&c);
        CHECK_EQ(cc_anexar(&c, CH_PAPEL_DC0, g_pac, n, 0), 0);
        cc_danca_cada(&c, CH_PAPEL_DC0, 0.0f);
        CHECK_EQ(cc_toca_forcado(&c, CH_PAPEL_DC0, "doorclosed", 0), 1);
        CHECK_EQ(cc_anim(&c, CH_PAPEL_DC0), 0);
        CHECK_EQ(cc_quadro(&c, CH_PAPEL_DC0, 0, 0), 0);
        /* Mesma animacao de novo: o forcado REINICIA do quadro 0. */
        CHECK_EQ(cc_quadro(&c, CH_PAPEL_DC0, 50, 0), 1);
        CHECK_EQ(cc_toca_forcado(&c, CH_PAPEL_DC0, "doorclosed", 60), 1);
        CHECK_EQ(cc_quadro(&c, CH_PAPEL_DC0, 60, 0), 0);
        /* Um nome que ele nao tem nao inventa nada. */
        CHECK_EQ(cc_toca_forcado(&c, CH_PAPEL_DC0, "kick3", 70), 0);
        CHECK_EQ(cc_anim(&c, CH_PAPEL_DC0), 0);
        /* O `kick1` termina em 375 ms e, com danceEvery = 0, ela FICA nele
           (ultimo quadro), sem voltar para o `doorclosed`. */
        CHECK_EQ(cc_toca_forcado(&c, CH_PAPEL_DC0, "kick1", 100), 1);
        CHECK_EQ(cc_quadro(&c, CH_PAPEL_DC0, 600, 4), 4);   /* 500/125 = 4 */
        CHECK_EQ(cc_anim(&c, CH_PAPEL_DC0), 1);             /* ainda kick1 */
        /* E o mesmo pedido NAO reinicia pelo cc_toca comum (a diferenca entre
           os dois); o forcado reinicia. */
        CHECK_EQ(cc_toca(&c, CH_PAPEL_DC0, "kick1", 700), 1);
        CHECK_EQ(cc_anim(&c, CH_PAPEL_DC0), 1);
    }

    printf("== animacao SEGURA para a tela de GAME OVER ==\n");
    {
        /* O `firstDeath` -> `deathLoop` da tela de morte
           (GameOverSubState.hx L285-292): com `cc_especial_fixo` o personagem
           NAO volta a dancar quando a animacao acaba -- ele fica nela ate' o
           jogo trocar por outra. Sem isso o BF morto voltaria a dancar no meio
           da tela de game over. */
        cc_cena_t c;
        const anim_t a[3] = {
            { "idle",    0, 2, 2400, 1 },
            { "firstDeath", 2, 2, 1000, 0 },   /* 200 ms */
            { "deathLoop",  4, 2, 1000, 1 },
        };
        unsigned int n = montar(6, 3, a);
        cc_reset(&c);
        CHECK_EQ(cc_anexar(&c, CH_PAPEL_BF, g_pac, n, 0), 0);
        CHECK_EQ(cc_quadro(&c, CH_PAPEL_BF, 0, 0), 0);

        CHECK_EQ(cc_toca(&c, CH_PAPEL_BF, "firstDeath", 0), 1);
        CHECK_EQ(cc_especial_terminou(&c, CH_PAPEL_BF, 100), 0);
        CHECK_EQ(cc_especial_terminou(&c, CH_PAPEL_BF, 200), 1);   /* 2 x 100 ms */
        /* Sem fixo, o quadro seguinte devolve a danca (comportamento normal). */
        CHECK_EQ(cc_quadro(&c, CH_PAPEL_BF, 201, 0), 0);
        CHECK_EQ(cc_anim(&c, CH_PAPEL_BF), 0);

        /* Com fixo, ela nunca acaba sozinha: cc_especial_terminou continua
           dizendo que terminou (e' o que a tela usa para encadear o loop), mas
           o personagem NAO sai dela. */
        cc_especial_fixo(&c, CH_PAPEL_BF, 1);
        CHECK_EQ(cc_toca(&c, CH_PAPEL_BF, "firstDeath", 1000), 1);
        CHECK_EQ(cc_quadro(&c, CH_PAPEL_BF, 1300, 4), 3);   /* ultimo quadro do firstDeath */
        CHECK_EQ(cc_anim(&c, CH_PAPEL_BF), 1);              /* ainda firstDeath */
        CHECK_EQ(cc_especial_terminou(&c, CH_PAPEL_BF, 1300), 1);
        /* Troca para o loop: tambem fica. */
        CHECK_EQ(cc_toca(&c, CH_PAPEL_BF, "deathLoop", 1400), 1);
        CHECK_EQ(cc_anim(&c, CH_PAPEL_BF), 2);
        /* deathLoop e' LOOP: 600 ms depois de comecar, o quadro e'
           600/100 = 6, e 6 % 2 = 0 -> o primeiro quadro dela (4). */
        CHECK_EQ(cc_quadro(&c, CH_PAPEL_BF, 2000, 8), 4);
        CHECK_EQ(cc_anim(&c, CH_PAPEL_BF), 2);
        /* Papel parado nao "terminou" nada, e o indice invalido nao estoura. */
        CHECK_EQ(cc_especial_terminou(&c, CH_PAPEL_DAD, 0), 0);
        CHECK_EQ(cc_especial_terminou(&c, 99, 0), 0);
        CHECK_EQ(cc_especial_terminou(NULL, 0, 0), 0);
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
        CHECK_EQ(cc_quadro(&c, CH_PAPEL_GF, 10, 4), 1);     /* batida 1: danceRight */
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

    printf("== danceEvery: quem danca de 2 em 2 batidas NAO danca na batida 1 ==\n");
    {
        /* Bopper.hx L166: `event.step % (danceEvery * STEPS_PER_BEAT) == 0`.
           danceEvery = 2 -> so' nos passos 0, 8, 16... (batidas 0, 2, 4). Antes
           o port dançava em TODA batida, duas vezes mais rapido que o dado. */
        cc_cena_t c;
        unsigned int n = montar(4, 3, gf_anims);
        cc_reset(&c);
        CHECK_EQ(cc_anexar(&c, CH_PAPEL_GF, g_pac, n, 0), 0);
        cc_danca_cada(&c, CH_PAPEL_GF, 2.0f);
        CHECK_EQ(cc_quadro(&c, CH_PAPEL_GF, 0, 0), 0);      /* passo 0: danceLeft */
        CHECK_EQ(cc_anim(&c, CH_PAPEL_GF), 0);
        /* passo 4 (batida 1): nao e' multiplo de 8 -> nada muda */
        CHECK_EQ(cc_quadro(&c, CH_PAPEL_GF, 10, 4), -1);
        CHECK_EQ(cc_anim(&c, CH_PAPEL_GF), 0);
        /* passo 8 (batida 2): danca, e troca o lado */
        CHECK_EQ(cc_quadro(&c, CH_PAPEL_GF, 20, 8), 1);
        CHECK_EQ(cc_anim(&c, CH_PAPEL_GF), 1);
        /* e danceEvery = 0 DESLIGA a danca automatica (132 props e nenhum
           personagem hoje, mas a regra e' a mesma do Bopper) */
        cc_danca_cada(&c, CH_PAPEL_GF, 0.0f);
        CHECK_EQ(cc_quadro(&c, CH_PAPEL_GF, 30, 16), -1);
        CHECK_EQ(cc_anim(&c, CH_PAPEL_GF), 1);
    }

    printf("== danca pulada por canto NAO troca o lado (hasDanced) ==\n");
    {
        /* BaseCharacter.dance() L479-490: se o personagem esta' cantando, o
           dance() sai fora ANTES de mexer em `hasDanced`. Logo a alternancia
           danceLeft/danceRight conta DANÇAS JOGADAS, e nao a paridade do passo:
           com `passo & 1` (ou `batida & 1`), a primeira danca pulada jogava o
           resto da musica no pe' trocado.
           Sequencia: passo 0 danca L; passos 4 e 8 sao pulados (cantando); o
           canto acaba no passo 10 (que nao e' passo de danca) e a danca
           FORCADA do fim do canto toca R; no passo 12 a danca normal toca L --
           a TERCEIRA danca jogada. Pela paridade do passo 12 (batida 3, impar)
           viria R: e' essa a diferenca que este bloco mede. */
        cc_cena_t c;
        unsigned int n = montar(4, 3, gf_anims);
        cc_reset(&c);
        CHECK_EQ(cc_anexar(&c, CH_PAPEL_GF, g_pac, n, 0), 0);
        CHECK_EQ(cc_quadro(&c, CH_PAPEL_GF, 0, 0), 0);      /* passo 0: L */
        CHECK_EQ(cc_anim(&c, CH_PAPEL_GF), 0);
        cc_canto(&c, CH_PAPEL_GF, 2, 0, 0, 1000);           /* singUP ate' 1000 ms */
        CHECK_EQ(cc_quadro(&c, CH_PAPEL_GF, 400, 4), 2);    /* passo 4: cantando */
        CHECK_EQ(cc_anim(&c, CH_PAPEL_GF), 2);
        CHECK_EQ(cc_quadro(&c, CH_PAPEL_GF, 800, 8), -1);   /* passo 8: ainda cantando */
        CHECK_EQ(cc_anim(&c, CH_PAPEL_GF), 2);
        /* os dois passos de danca perdidos ficam contados (o log os mostra) */
        CHECK_EQ(c.p[CH_PAPEL_GF].pulos, 2);
        CHECK_EQ(c.p[CH_PAPEL_GF].dancas, 1);
        /* passo 10: o canto acabou -> danca FORCADA (segunda jogada: R). */
        CHECK_EQ(cc_quadro(&c, CH_PAPEL_GF, 1000, 10), 1);
        CHECK_EQ(cc_anim(&c, CH_PAPEL_GF), 1);
        /* passo 12: danca normal, terceira jogada -> L. */
        CHECK_EQ(cc_quadro(&c, CH_PAPEL_GF, 1200, 12), 0);
        CHECK_EQ(cc_anim(&c, CH_PAPEL_GF), 0);
        /* passo 16: quarta jogada -> R. */
        CHECK_EQ(cc_quadro(&c, CH_PAPEL_GF, 1600, 16), 1);
        CHECK_EQ(cc_anim(&c, CH_PAPEL_GF), 1);
    }


    printf("== combo/drop da GF (BaseCharacter L586-600, L651-681) ==\n");
    {
        /* Os numeros NAO sao inventados: sao exatamente os que os 8 personagens
           com essas animacoes tem nos dados (combo50, combo200, drop70 nas
           gf/nene; drop70 sozinho na gf-tankmen). */
        cc_cena_t c;
        const anim_t gfc[5] = {
            { "danceLeft", 0, 1, 2400, 1 },
            { "danceRight", 1, 1, 2400, 1 },
            { "combo50", 2, 1, 2400, 0 },
            { "combo200", 3, 1, 2400, 0 },
            { "drop70", 4, 1, 2400, 0 },
        };
        unsigned int n = montar(5, 5, gfc);
        cc_reset(&c);
        CHECK_EQ(cc_anexar(&c, CH_PAPEL_GF, g_pac, n, 0), 0);
        /* combo<N> so' no numero EXATO: 50 toca, 51 nao. */
        CHECK_EQ(cc_combo(&c, CH_PAPEL_GF, 50, 100), 1);
        CHECK_EQ(cc_anim(&c, CH_PAPEL_GF), 2);
        CHECK_EQ(cc_combo(&c, CH_PAPEL_GF, 51, 200), 0);
        CHECK_EQ(cc_anim(&c, CH_PAPEL_GF), 2);
        CHECK_EQ(cc_combo(&c, CH_PAPEL_GF, 200, 300), 1);
        CHECK_EQ(cc_anim(&c, CH_PAPEL_GF), 3);
        CHECK_EQ(cc_combo(&c, CH_PAPEL_GF, 0, 350), 0);
        CHECK_EQ(cc_combo(&c, CH_PAPEL_GF, -5, 350), 0);
        /* drop<N>: o maior degrau que o combo alcanca; abaixo do menor, nada. */
        CHECK_EQ(cc_drop(&c, CH_PAPEL_GF, 70, 400), 1);
        CHECK_EQ(cc_anim(&c, CH_PAPEL_GF), 4);
        CHECK_EQ(cc_drop(&c, CH_PAPEL_GF, 120, 500), 1);   /* 120 >= 70 */
        CHECK_EQ(cc_anim(&c, CH_PAPEL_GF), 4);
        CHECK_EQ(cc_drop(&c, CH_PAPEL_GF, 69, 600), 0);    /* 69 < 70 */
        /* a animacao de combo NAO e' interrompida pelo passo que danca:
           um quadro so' a 24 fps dura ~42 ms, entao em 420 ms ela ainda esta'
           la' (e o quadro nao mudou). */
        CHECK_EQ(cc_quadro(&c, CH_PAPEL_GF, 420, 8), 4);
        CHECK_EQ(cc_anim(&c, CH_PAPEL_GF), 4);
        CHECK_EQ(cc_quadro(&c, CH_PAPEL_GF, 430, 8), -1);
        CHECK_EQ(cc_anim(&c, CH_PAPEL_GF), 4);
        /* e quando ela ACABA, a GF volta a dancar (onAnimationFinished ->
           dance(true), que conta como danca jogada: a primeira -> danceLeft).
           O passo 9 nao e' passo de danca, entao so' a danca forcada roda. */
        CHECK_EQ(cc_quadro(&c, CH_PAPEL_GF, 500, 9), 0);
        CHECK_EQ(cc_anim(&c, CH_PAPEL_GF), 0);
        /* no passo 12 a danca normal toca a SEGUNDA -> danceRight */
        CHECK_EQ(cc_quadro(&c, CH_PAPEL_GF, 600, 12), 1);
        CHECK_EQ(cc_anim(&c, CH_PAPEL_GF), 1);
        /* personagem sem combo/drop: os dois devolvem 0, nada e' inventado */
        {
            cc_cena_t d;
            unsigned int m = montar(4, 3, gf_anims);
            cc_reset(&d);
            CHECK_EQ(cc_anexar(&d, CH_PAPEL_GF, g_pac, m, 0), 0);
            CHECK_EQ(cc_combo(&d, CH_PAPEL_GF, 50, 0), 0);
            CHECK_EQ(cc_drop(&d, CH_PAPEL_GF, 70, 0), 0);
            CHECK_EQ(cc_anim(&d, CH_PAPEL_GF), -1);
        }
    }

    if (falhas) { printf("\n  %d falha(s)\n", falhas); return 1; }
    printf("\n  cena: idle pela batida, canto com singTime, miss com queda, e o quadro so' quando muda\n");
    return 0;
}
