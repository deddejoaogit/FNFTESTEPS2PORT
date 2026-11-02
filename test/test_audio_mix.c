/* Mixer de duas faixas (instrumental + vozes).
 *
 * Este e' um teste de CONTA, e existe porque a conta ja' esteve errada no port
 * real: o bloco de saida do mixer saia com o dobro do tamanho, o audio dobrava
 * de velocidade e o relogio da musica ia junto (medido: 4.755.264 amostras num
 * arquivo de 3.225.600). Nada disso aparece no PC sem olhar os bytes.
 *
 * O que se prova aqui:
 *   1) a soma e' por canal (instL+voicesL, instR+voicesR), e nao canal errado
 *      com canal certo -- trocar deixa o som "quase certo", o pior caso;
 *   2) satura em +32767 / -32768 (16 bits nao estoura e nao inverte o sinal);
 *   3) com as vozes desligadas o instrumental passa intacto;
 *   4) a saida tem metade dos bytes da entrada (4 canais viram 2), nos
 *      tamanhos que o main.c usa;
 *   5) funciona IN PLACE, que e' como o main.c chama (mesmo buffer).
 */
#include <stdio.h>
#include <string.h>
#include "audio_mix.h"

static int falhas;
#define CHECK(c) do { if (!(c)) { printf("  FALHA: %s (linha %d)\n", #c, __LINE__); falhas++; } } while (0)
#define CHECK_EQ(a, b) do { long _a = (long)(a), _b = (long)(b); \
    if (_a != _b) { printf("  FALHA: %s == %s: %ld != %ld (linha %d)\n", #a, #b, _a, _b, __LINE__); falhas++; } } while (0)

int main(void)
{
    printf("== mixer: soma por canal, com as duas faixas de 16 bits ==\n");
    {
        /* [instL, instR, voicesL, voicesR] x 2 amostras */
        const short in[8] = { 100, -100, 50, 50, 1000, -1000, -300, 300 };
        short out[4];
        CHECK_EQ(amix_somar(in, out, 2, 1), 2);
        CHECK_EQ(out[0], 150);        /* instL + voicesL */
        CHECK_EQ(out[1], -50);        /* instR + voicesR */
        CHECK_EQ(out[2], 700);        /* 1000 + (-300) */
        CHECK_EQ(out[3], -700);       /* -1000 + 300   */
        /* Trocar os canais das vozes (voicesR no lugar de voicesL) daria
           150 e -50 na primeira amostra e 1300/-500 na segunda: o teste tem de
           separar os dois casos, senao nao pega essa troca. */
        CHECK(out[2] != 1300);
        CHECK(out[3] != -500);
    }

    printf("== mixer: a saturacao para em 16 bits, sem inverter o sinal ==\n");
    {
        const short in[8] = { 32000, -32000, 32000, -32000, 32767, -32768, 1, -1 };
        short out[4];
        amix_somar(in, out, 2, 1);
        CHECK_EQ(out[0], 32767);
        CHECK_EQ(out[1], -32768);
        CHECK_EQ(out[2], 32767);      /* 32767 + 1 satura, nao vira -32768 */
        CHECK_EQ(out[3], -32768);     /* -32768 - 1 satura, nao vira 32767  */
    }

    printf("== mixer: vozes desligadas (R3) deixam o instrumental sozinho ==\n");
    {
        const short in[8] = { 100, -100, 50, 50, 1234, -5678, 7, 9 };
        short out[4];
        amix_somar(in, out, 2, 0);
        CHECK_EQ(out[0], 100);
        CHECK_EQ(out[1], -100);
        CHECK_EQ(out[2], 1234);
        CHECK_EQ(out[3], -5678);
    }

    printf("== mixer: a saida tem METADE dos bytes da entrada ==\n");
    {
        const int amostras[3] = { 1, 256, 4096 };
        int k;
        for (k = 0; k < 3; k++) {
            CHECK_EQ(amix_bytes_saida(amostras[k]), amostras[k] * 4);
            CHECK_EQ(amix_bytes_saida(amostras[k]) * 2, amostras[k] * 8);
        }
        CHECK_EQ(amix_bytes_saida(0), 0);
        CHECK_EQ(amix_bytes_saida(-4), 0);
        CHECK_EQ(amix_bytes_saida(4096), 16384);
    }

    printf("== mixer: in place (o main.c mistura no mesmo buffer) ==\n");
    {
        /* 8 amostras, para exercitar o caso em que a escrita alcanca o que
           ainda vai ser lido se os indices estiverem errados. */
        short buf[32];
        short ref[32];
        int i;
        for (i = 0; i < 32; i++) buf[i] = (short)(i * 100 - 1500);
        memcpy(ref, buf, sizeof(ref));
        CHECK_EQ(amix_somar(buf, buf, 8, 1), 8);
        for (i = 0; i < 8; i++) {
            int il = ref[i * 4 + 0], ir = ref[i * 4 + 1];
            int vl = ref[i * 4 + 2], vr = ref[i * 4 + 3];
            int el = il + vl, er = ir + vr;
            if (el > 32767) el = 32767; else if (el < -32768) el = -32768;
            if (er > 32767) er = 32767; else if (er < -32768) er = -32768;
            CHECK_EQ(buf[i * 2 + 0], el);
            CHECK_EQ(buf[i * 2 + 1], er);
        }
    }

    printf("== mixer: entrada nula ou vazia nao escreve nada ==\n");
    {
        short out[4] = { 7, 7, 7, 7 };
        const short in[4] = { 1, 2, 3, 4 };
        CHECK_EQ(amix_somar(NULL, out, 1, 1), 0);
        CHECK_EQ(amix_somar(in, NULL, 1, 1), 0);
        CHECK_EQ(amix_somar(in, out, 0, 1), 0);
        CHECK_EQ(out[0], 7);
    }

    /* OVERLAY: o segundo som da tela de game over (a musica + o fnf_loss_sfx).
       E' a mesma conta de saturacao, em cima de um bloco estereo REGULAR -- nao
       dos 4 canais do stems. Somar o overlay no canal errado (L com R) daria um
       som "quase certo", que e' o pior caso para se olhar de fora. */
    printf("== mixer: overlay estereo (musica de game over + sfx) ==\n");
    {
        short dst[6] = { 100, -100, 32760, -32760, 0, 0 };
        const short src[6] = { 50, 50, 50, -50, 1000, -2000 };
        CHECK_EQ(amix_somar_estereo(dst, src, 3), 3);
        CHECK_EQ(dst[0], 150);       /* L: 100 + 50   */
        CHECK_EQ(dst[1], -50);       /* R: -100 + 50  */
        CHECK_EQ(dst[2], 32767);     /* satura em cima */
        CHECK_EQ(dst[3], -32768);    /* satura embaixo */
        CHECK_EQ(dst[4], 1000);
        CHECK_EQ(dst[5], -2000);
        /* Nada a somar nao pode apagar o que ja' estava. */
        CHECK_EQ(amix_somar_estereo(NULL, src, 1), 0);
        CHECK_EQ(amix_somar_estereo(dst, NULL, 1), 0);
        CHECK_EQ(amix_somar_estereo(dst, src, 0), 0);
        CHECK_EQ(dst[0], 150);
        /* Um bloco parcial (o resto da leitura no fim do arquivo) soma so' o
           que veio: a thread do anel chama assim quando o overlay acaba. */
        dst[0] = 10; dst[1] = 20;
        CHECK_EQ(amix_somar_estereo(dst, src, 1), 1);
        CHECK_EQ(dst[0], 60);
        CHECK_EQ(dst[1], 70);
    }

    /* O PREVIEW DA FREEPLAY: 4 canais -> 2, com o ganho do fade DENTRO do
       bloco. O `fadeIn(2)`/`fadeOut(2)` a 0,7 vale so' para o preview, e os
       jingles do rank entram por cima depois (`audio_ring.c`), a volume 1. */
    printf("== mixer: ganho por bloco (o preview da Freeplay) ==\n");
    {
        short in[4 * 4] = { 1000, -1000, 3000, -3000,      /* amostra 0 */
                            2000, -2000, 3000, -3000,      /* amostra 1 */
                            32767, -32768, 3000, -3000,    /* amostra 2 (fundo) */
                            100, -100, 3000, -3000 };      /* amostra 3 */
        short out[2 * 4];
        /* g0 = 0, g1 = 1: o bloco inteiro e' um ramo linear de 0 a 1. */
        CHECK_EQ(amix_somar_ganho(in, out, 4, 0.0f, 1.0f), 4);
        CHECK_EQ(out[0], 0);            /* 1000 * 0        */
        CHECK_EQ(out[1], 0);
        CHECK_EQ(out[2], 667);          /* 2000 * 1/3      */
        CHECK_EQ(out[3], -667);
        CHECK_EQ(out[4], 21845);        /* 32767 * 2/3 -- nao satura        */
        CHECK_EQ(out[5], -21845);
        CHECK_EQ(out[6], 100);          /* 100 * 1         */
        CHECK_EQ(out[7], -100);
        /* So' o instrumental: os dois canais de voz (3000/-3000) NAO entram. */
        /* Um ganho constante e' o caminho normal do meio do trecho (0,7). */
        CHECK_EQ(amix_somar_ganho(in, out, 1, 0.7f, 0.7f), 1);
        CHECK_EQ(out[0], 700);
        CHECK_EQ(out[1], -700);
        /* Satura quando o ganho e' maior que 1 (o `fadeIn` nunca passa de 1,
           mas a parede de 16 bits tem de existir do mesmo jeito). */
        CHECK_EQ(amix_somar_ganho(in, out, 1, 2.0f, 2.0f), 1);
        CHECK_EQ(out[0], 2000);
        in[0] = 30000;
        CHECK_EQ(amix_somar_ganho(in, out, 1, 2.0f, 2.0f), 1);
        CHECK_EQ(out[0], 32767);
        in[0] = 1000;
        /* In place: `saida` pode ser o mesmo buffer. */
        CHECK_EQ(amix_somar_ganho(in, in, 4, 0.5f, 0.5f), 4);
        CHECK_EQ(in[0], 500);
        CHECK_EQ(in[1], -500);
        CHECK_EQ(in[2], 1000);
        /* Ponteiro nulo e contagem zero nao escrevem nada. */
        CHECK_EQ(amix_somar_ganho(NULL, out, 4, 1.0f, 1.0f), 0);
        CHECK_EQ(amix_somar_ganho(in, NULL, 4, 1.0f, 1.0f), 0);
        CHECK_EQ(amix_somar_ganho(in, out, 0, 1.0f, 1.0f), 0);
    }

    /* O VOLUME POR FONTE (`amix_volume`): e' o volume do `FlxSound` do
       original, sem rampa -- 0,4 no `scrollMenu` da Freeplay
       (`FreeplayState.hx` L2918), 1,0 no `confirmMenu`/`cancelMenu`. */
    printf("== mixer: o volume de uma fonte secundaria (0,4 do scrollMenu)\n");
    {
        short pcm[8];
        pcm[0] = 1000;  pcm[1] = -1000;
        pcm[2] = 250;   pcm[3] = -250;
        pcm[4] = 32767; pcm[5] = -32768;
        pcm[6] = 0;     pcm[7] = 0;
        CHECK_EQ(amix_volume(pcm, 4, 40), 4);
        CHECK_EQ(pcm[0], 400);      /* 1000 * 0,4                     */
        CHECK_EQ(pcm[1], -400);     /* simetrico                       */
        CHECK_EQ(pcm[2], 100);      /* 250 * 0,4 = 100, exato          */
        CHECK_EQ(pcm[3], -100);
        CHECK_EQ(pcm[4], 13107);    /* 32767 * 0,4 = 13106,8 -> 13107  */
        CHECK_EQ(pcm[5], -13107);   /* -32767,2 -> -13107 (longe do 0) */
        /* Volume 0 zera, 100 nao mexe, negativo vira 0. */
        pcm[0] = 1234; pcm[1] = -1234;
        CHECK_EQ(amix_volume(pcm, 4, 0), 4);
        CHECK_EQ(pcm[0], 0);
        CHECK_EQ(pcm[1], 0);
        pcm[0] = 1234; pcm[1] = -1234;
        CHECK_EQ(amix_volume(pcm, 4, 100), 4);
        CHECK_EQ(pcm[0], 1234);
        pcm[0] = 1234;
        CHECK_EQ(amix_volume(pcm, 4, -5), 4);
        CHECK_EQ(pcm[0], 0);
        /* Satura quando o volume passa de 100 (nenhuma linha do original pede,
           mas a parede de 16 bits tem de existir). */
        pcm[0] = 30000;
        CHECK_EQ(amix_volume(pcm, 4, 200), 4);
        CHECK_EQ(pcm[0], 32767);
        /* Ponteiro nulo e contagem zero nao escrevem nada. */
        CHECK_EQ(amix_volume(NULL, 4, 40), 0);
        CHECK_EQ(amix_volume(pcm, 0, 40), 0);
    }

    if (falhas) { printf("\n  %d falha(s)\n", falhas); return 1; }
    printf("\n  mixer: soma por canal, saturacao, vozes desligadas, metade dos bytes, in place, ganho por bloco e volume por fonte conferidos\n");
    return 0;
}
