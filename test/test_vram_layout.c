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
    /* 640x448 PSMCT32: 640*448 = 286.720 palavras = 1.146.880 bytes. E' o que o
       port usava em cada framebuffer ate' a Fase 4. */
    CHECK_EQ(vl_size_words(640, 448, VL_PSM_32, VL_ALINHAMENTO_PAGINA), 286720u);
    /* 640x448 PSMCT16: 640*(448/2) = 143.360 palavras = 573.440 bytes --
       metade, com a MESMA resolucao. E' o que o port usa agora. */
    CHECK_EQ(vl_size_words(640, 448, VL_PSM_16, VL_ALINHAMENTO_PAGINA), 143360u);
    CHECK_EQ(vl_bytes(VL_FB0), 573440u);
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
    CHECK_EQ(vl_address(VL_FB1), 143360u);
    CHECK_EQ(vl_address(VL_TEX), 286720u);
    CHECK_EQ(vl_address(VL_TEX2), 368640u);
    CHECK_EQ(vl_address(VL_FONTTEX), 450560u);
    CHECK_EQ(vl_address(VL_GFTEX16), 524288u);

    printf("== vram: cabe em 4 MiB ==\n");
    CHECK(vl_fits());
    CHECK(vl_used_words() <= VL_TOTAL_WORDS);
    /* Medido: os slots FIXOS ocupam 534.528 palavras (2.138.112 bytes) e o
       resto -- 514.048 palavras = 2.056.192 bytes -- e' o POOL de texturas.
       Antes dos dois passos da Fase 4 (fonte 384x192 e framebuffer de 16 bits)
       eram 4.071.424 bytes usados e 122.880 livres; com gftex e stagetex ainda
       fixas eram 2.924.544 usados. Agora elas moram dentro do pool. */
    CHECK_EQ(vl_used_words(), 534528u);
    CHECK_EQ(vl_free_words(), 514048u);
    CHECK_EQ(vl_free_words() * 4u, 2056192u);

    printf("== vram: o pool de texturas comeca no fim dos slots fixos ==\n");
    /* O pool e' TODO o resto da VRAM, e o numero tem de ser o mesmo que o
       stage_pool usa (SP_POOL_BYTES = 2.056.192). Se um slot fixo crescer, o
       pool encolhe aqui e o teste do pool passa a relatar um palco sem lugar. */
    CHECK_EQ(vl_pool_base(), 534528u);
    CHECK_EQ(vl_pool_size_bytes(), 2056192u);
    CHECK_EQ(vl_pool_base(), vl_used_words());
    CHECK_EQ(vl_pool_size_bytes(), vl_free_words() * 4u);

    printf("== vram: o que o framebuffer de 16 bits devolveu ==\n");
    /* O par de buffers sai de 573.440 palavras (dois de 640x448 PSMCT32) para
       286.720 (dois de 640x448 PSMCT16): 286.720 palavras = 1.146.880 bytes. */
    CHECK_EQ(vl_size_words(640, 448, VL_PSM_32, VL_ALINHAMENTO_PAGINA) * 2u
             - vl_size_words(640, 448, VL_PSM_16, VL_ALINHAMENTO_PAGINA) * 2u,
             286720u);
    /* E o proximo passo (512x384 PSMCT16) valeria outros 90.112 palavras =
       360.448 bytes, ao preco de reescalar TODO o desenho em 0,8. Fica
       registrado aqui como o numero que o passo tem de entregar. */
    CHECK_EQ(vl_size_words(512, 384, VL_PSM_16, VL_ALINHAMENTO_PAGINA), 98304u);
    CHECK_EQ(vl_size_words(640, 448, VL_PSM_16, VL_ALINHAMENTO_PAGINA) * 2u
             - vl_size_words(512, 384, VL_PSM_16, VL_ALINHAMENTO_PAGINA) * 2u,
             90112u);

    if (falhas) { printf("\n  %d falha(s)\n", falhas); return 1; }
    printf("\n  vram: mapa exato e dentro da VRAM\n");
    return 0;
}
