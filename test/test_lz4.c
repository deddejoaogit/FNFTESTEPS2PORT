/* O decodificador de LZ4 do jogo contra o codificador do empacotador.
 *
 * O QUE PROVA: que os fluxos que o tools/lz4py.py gera (o lado que comprime de
 * verdade, usado pelo empacotador) saem do src/engine/lz4_bloco.c byte a byte
 * iguais ao original, inclusive nos casos dificeis -- match sobreposto, periodo
 * de 4 KB, literais de 255 em 255.
 *
 * O QUE NAO PROVA: que o GS desenha o quadro certo. Isso e' a tela.
 *
 * A segunda metade do teste e' o caminho do ERRO: fluxo truncado, offset
 * invalido, saida menor que o esperado. O decodificador tem de RECUSAR, e nao
 * devolver um quadro meio decodificado -- quadro pela metade no GS aparece como
 * arte rasgada, e nao como erro.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lz4_bloco.h"
#include "lz4_vetores.h"

static int falhas;
#define CHECK(c) do { if (!(c)) { printf("  FALHA: %s (linha %d)\n", #c, __LINE__); falhas++; } } while (0)

int main(void)
{
    unsigned int i;

    printf("== lz4: %d fluxos do empacotador decodificam igual ==\n", LZ4_VETOR_COUNT);
    for (i = 0; i < LZ4_VETOR_COUNT; i++) {
        const lz4_vetor_t *v = &lz4_vetores[i];
        unsigned char *dst;
        int rc;
        if (v->original_bytes == 0) {
            /* Vazio: nada a decodificar, e a saida de tamanho 0 fecha. */
            rc = lz4_bloco_decode(v->comprimido, v->comprimido_bytes, NULL, 0);
            CHECK(rc == LZ4_BLOCO_OK);
            continue;
        }
        dst = malloc(v->original_bytes);
        CHECK(dst != NULL);
        if (!dst) continue;
        memset(dst, 0xAA, v->original_bytes);
        rc = lz4_bloco_decode(v->comprimido, v->comprimido_bytes, dst, v->original_bytes);
        if (rc != LZ4_BLOCO_OK) {
            printf("  FALHA: %s: decode devolveu %d\n", v->nome, rc);
            falhas++;
        } else if (memcmp(dst, v->original, v->original_bytes) != 0) {
            unsigned int k = 0;
            while (k < v->original_bytes && dst[k] == v->original[k]) k++;
            printf("  FALHA: %s: primeiro byte diferente em %u (0x%02x != 0x%02x)\n",
                   v->nome, k, dst[k], v->original[k]);
            falhas++;
        }
        free(dst);
    }

    printf("== lz4: fluxo invalido e' recusado, nunca meio decodificado ==\n");
    {
        /* "0x40 'a' 'b' 'c' 'd'" = token com 4 literais e nenhum match: saida de
           4 bytes. Serve de base para os casos de borda abaixo. */
        static const unsigned char bom[] = { 0x40, 'a', 'b', 'c', 'd' };
        unsigned char dst[8];
        unsigned char ruim[8];

        CHECK(lz4_bloco_decode(bom, sizeof(bom), dst, 4) == LZ4_BLOCO_OK);
        CHECK(memcmp(dst, "abcd", 4) == 0);

        /* Saida maior que o fluxo produz: tem de recusar (quadro incompleto). */
        CHECK(lz4_bloco_decode(bom, sizeof(bom), dst, 5) == LZ4_ERR_TAMANHO);
        /* Saida menor que o fluxo produz. */
        CHECK(lz4_bloco_decode(bom, sizeof(bom), dst, 3) == LZ4_ERR_LIT_FORA);

        /* Token diz 4 literais, mas so' vem 2. */
        memcpy(ruim, bom, sizeof(bom));
        CHECK(lz4_bloco_decode(ruim, 3, dst, 8) < 0);
        /* Token diz 15 literais com extensao e o fluxo acaba na extensao. */
        ruim[0] = 0xF0;
        CHECK(lz4_bloco_decode(ruim, 1, dst, 8) == LZ4_ERR_TRUNCADO_LIT);
        /* Match com offset 0. */
        {
            static const unsigned char off0[] = { 0x40, 'a', 'b', 'c', 'd', 0x00, 0x00 };
            CHECK(lz4_bloco_decode(off0, sizeof(off0), dst, 8) == LZ4_ERR_OFF);
        }
        /* Match com offset para antes do inicio da saida. */
        {
            static const unsigned char offx[] = { 0x40, 'a', 'b', 'c', 'd', 0x09, 0x00 };
            CHECK(lz4_bloco_decode(offx, sizeof(offx), dst, 8) == LZ4_ERR_OFF);
        }
        /* Match maior que o espaco que sobra na saida. */
        {
            static const unsigned char grande[] = { 0x1F, 'a', 0x01, 0x00, 0xFF };
            CHECK(lz4_bloco_decode(grande, sizeof(grande), dst, 8) < 0);
        }
        /* Sem o offset (fluxo termina no meio de uma sequencia). */
        {
            static const unsigned char semoff[] = { 0x41, 'a', 'b', 'c', 'd', 0x00 };
            CHECK(lz4_bloco_decode(semoff, sizeof(semoff), dst, 8) == LZ4_ERR_TRUNCADO_OFF);
        }
        /* Ponteiro nulo com tamanho nao-zero nao pode explodir. */
        CHECK(lz4_bloco_decode(NULL, 5, dst, 8) < 0);
        CHECK(lz4_bloco_decode(bom, sizeof(bom), NULL, 4) < 0);
    }

    printf("== lz4: os assets do harness tem o formato que o jogo le' ==\n");
    {
        /* Os pacotes sinteticos (CFZ4) do harness usam o mesmo bloco. Se o
           cabecalho e a tabela estiverem certos, o primeiro quadro sai. Este
           teste nao abre arquivo (roda antes do harness); ele confere so' a
           leitura de u32 little endian, que e' o que todo cabecalho usa. */
        static const unsigned char h[] = { 0x34, 0x12, 0x00, 0x00,
                                           0xFF, 0xFF, 0xFF, 0xFF };
        CHECK(lz4_u32le(h) == 0x1234u);
        CHECK(lz4_u32le(h + 4) == 0xFFFFFFFFu);
    }

    if (falhas) { printf("\n  %d falha(s)\n", falhas); return 1; }
    printf("\n  lz4: %d fluxos, mais 10 casos invalidos\n", LZ4_VETOR_COUNT);
    return 0;
}
