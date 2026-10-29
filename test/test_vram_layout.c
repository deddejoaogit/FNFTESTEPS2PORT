/* Mapa de VRAM: as contas tem de bater com o ps2sdk e caber em 4 MiB.
 *
 * Este teste existe porque uma alocacao nova que estourasse a VRAM nao quebrava
 * nada: graph_vram_allocate() devolve -1 e o jogo desenha com endereco
 * invalido. Aqui a conta e' feita no PC, contra os mesmos numeros que o ps2sdk
 * usa (ee/graph/src/graph_vram.c).
 */
#include <stdio.h>
#include "vram_layout.h"

static int falhas;

#define CHECK(cond) do { if (!(cond)) { \
    printf("  FALHA: %s (linha %d)\n", #cond, __LINE__); falhas++; } } while (0)
#define CHECK_EQ(a, b) do { unsigned int _a = (unsigned int)(a), _b = (unsigned int)(b); \
    if (_a != _b) { printf("  FALHA: %s == %s: %u != %u (linha %d)\n", #a, #b, _a, _b, __LINE__); \
    falhas++; } } while (0)

int main(void)
{
    printf("== vram: conta de tamanho igual ao ps2sdk ==\n");
    /* 640x448 PSMCT32: 640*448 = 286.720 palavras = 1.146.880 bytes.
       E' o numero que a documentacao do projeto usa. */
    CHECK_EQ(vl_size_words(640, 448, VL_PSM_32, VL_ALINHAMENTO_PAGINA), 286720u);
    CHECK_EQ(vl_bytes(VL_FB0), 1146880u);
    /* 320x64 PSMCT16: 320*(64/2) = 10.240 palavras = 40.960 bytes. */
    CHECK_EQ(vl_size_words(320, 64, VL_PSM_16, VL_ALINHAMENTO_BLOCO), 10240u);
    CHECK_EQ(vl_bytes(VL_GFTEX16), 40960u);
    /* Largura que nao e' multipla de 64 sobe para o proximo multiplo. */
    CHECK_EQ(vl_size_words(100, 32, VL_PSM_32, VL_ALINHAMENTO_BLOCO), 4096u);

    printf("== vram: o mapa do port ==\n");
    /* Os enderecos conhecidos do port, um a um. Se um deles mudar, os pacotes
       de textura que a documentacao cita (737.280 para a gftex) ficam errados
       e isso tem de aparecer aqui, nao no console. */
    CHECK_EQ(vl_address(VL_FB0), 0u);
    CHECK_EQ(vl_address(VL_FB1), 286720u);
    CHECK_EQ(vl_address(VL_TEX), 573440u);
    CHECK_EQ(vl_address(VL_TEX2), 655360u);
    CHECK_EQ(vl_address(VL_GFTEX), 737280u);

    printf("== vram: cabe em 4 MiB ==\n");
    CHECK(vl_fits());
    CHECK(vl_used_words() <= VL_TOTAL_WORDS);
    /* Medido: o mapa usa 1.017.856 palavras (4.071.424 bytes) e sobram 30.720
       palavras (122.880 bytes). Antes da fonte encolher eram 24.576 bytes. */
    CHECK_EQ(vl_used_words(), 1017856u);
    CHECK_EQ(vl_free_words(), 30720u);
    CHECK_EQ(vl_free_words() * 4u, 122880u);

    printf("== vram: o que a Fase 4 ganharia com framebuffer de 16 bits ==\n");
    /* 512x384 PSMCT16 = 98.304 palavras = 393.216 bytes por buffer; o par sai de
       573.440 palavras para 196.608, libertando 376.832 palavras = 1.507.328
       bytes. E' o numero que docs/PLANO_COMPLETO.md cita, conferido aqui. */
    CHECK_EQ(vl_size_words(512, 384, VL_PSM_16, VL_ALINHAMENTO_PAGINA), 98304u);
    CHECK_EQ(vl_size_words(640, 448, VL_PSM_32, VL_ALINHAMENTO_PAGINA) * 2u
             - vl_size_words(512, 384, VL_PSM_16, VL_ALINHAMENTO_PAGINA) * 2u,
             376832u);

    if (falhas) { printf("\n  %d falha(s)\n", falhas); return 1; }
    printf("\n  vram: mapa exato e dentro da VRAM\n");
    return 0;
}
