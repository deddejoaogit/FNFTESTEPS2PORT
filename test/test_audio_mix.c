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

    if (falhas) { printf("\n  %d falha(s)\n", falhas); return 1; }
    printf("\n  mixer: soma por canal, saturacao, vozes desligadas, metade dos bytes e in place conferidos\n");
    return 0;
}
