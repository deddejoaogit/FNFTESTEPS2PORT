/* resultado_rank: o QUADRO da animacao do personagem na tela de Resultado.
 *
 * O QUE PROVA
 * `ResultState.hx` L201-330 monta as animacoes do rank e o L704-740 as
 * dispara. As quatro regras que o port precisa seguir, e que aqui sao
 * exercitadas com entradas sinteticas (o dado real vem da tabela gerada):
 *
 *  1. antes de `inicio_ms` (getBFDelay + delay) a entrada esta' INVISIVEL
 *     (-1), e no instante exato ela ja' mostra o primeiro quadro;
 *  2. `startFrameLabel` faz a animacao comecar num quadro que nao e' o 0;
 *  3. `loopFrameLabel`/`loopFrame` recomecam no quadro do rotulo e repetem
 *     com periodo (quadros - loop_ini), para sempre;
 *  4. sem loop, para no ULTIMO quadro (nao volta ao comeco, nao desaparece).
 *
 * Tambem confere a TABELA de verdade (`resultado_rank_dados.h`): todo rank do
 * jogo tem indice, toda entrada cabe no proprio pacote e o pior rank cabe nos
 * 350.208 B medidos livres na tela -- orcamento medido, nao estimado.
 */
#include <stdio.h>
#include <string.h>

#include "resultado_rank.h"

static int falhas;
#define CHECK(c) do { if (!(c)) { printf("  FALHA: %s (linha %d)\n", #c, __LINE__); falhas++; } } while (0)
#define CHECK_EQ(a, b) do { long _a = (long)(a), _b = (long)(b); \
    if (_a != _b) { printf("  FALHA: %s == %s: %ld != %ld (linha %d)\n", #a, #b, _a, _b, __LINE__); falhas++; } } while (0)

/* VRAM livre na tela de Resultado, medida no harness depois do atlas do placar
 * ir para 16 bpp (`resultado_vram,350208,186224`). */
#define VRAM_LIVRE 350208u

static res_rr_anim_t anims1[1] = {{ "INTRO", 0, 20, 2400 }};

static res_rr_entrada_t entrada(int quadros, int inicio, int loop_ini, int inicio_ms)
{
    res_rr_entrada_t e;
    memset(&e, 0, sizeof(e));
    e.arquivo = "x.cfz4";
    e.w = 10; e.h = 10; e.quadros = (unsigned short)quadros;
    e.psm = 16;
    e.escala = 1.0f;
    e.inicio_ms = (unsigned short)inicio_ms;
    e.inicio = (short)inicio;
    e.loop_ini = (short)loop_ini;
    e.n_anims = 1;
    e.anims = anims1;
    return e;
}

int main(void)
{
    res_rr_entrada_t e;
    int i, r;
    unsigned int pior = 0;

    printf("== animacao do personagem na tela de Resultado (resultado_rank) ==\n");

    /* ---- 1. invisivel antes do instante, e o primeiro quadro no instante --- */
    e = entrada(20, 0, -1, 4000);
    CHECK_EQ(res_rr_quadro(&e, 0), -1);
    CHECK_EQ(res_rr_quadro(&e, 3999), -1);
    CHECK_EQ(res_rr_quadro(&e, 4000), 0);
    /* 24 fps: 41 ms por quadro (o port usa o instante inteiro, sem arredondar
       meio quadro). */
    CHECK_EQ(res_rr_quadro(&e, 4000 + 41), 0);
    CHECK_EQ(res_rr_quadro(&e, 4000 + 42), 1);
    CHECK_EQ(res_rr_quadro(&e, 4000 + 500), 12);
    /* ---- 4. sem loop: para no ULTIMO quadro ------------------------------- */
    CHECK_EQ(res_rr_quadro(&e, 4000 + 800), 19);
    CHECK_EQ(res_rr_quadro(&e, 4000 + 60000), 19);
    printf("  sem loop: invisivel ate' 4000 ms, 0..19 e para no 19\n");

    /* ---- 2. `startFrameLabel`: comeca no quadro do rotulo ------------------ */
    e = entrada(40, 15, 25, 1000);
    CHECK_EQ(res_rr_quadro(&e, 999), -1);
    CHECK_EQ(res_rr_quadro(&e, 1000), 15);
    CHECK_EQ(res_rr_quadro(&e, 1000 + 41), 15);
    CHECK_EQ(res_rr_quadro(&e, 1000 + 500), 27);
    /* ---- 3. o loop recomeca no `loop_ini` e gira com periodo 40-25 = 15 --- */
    /* A primeira passada e' de 15 a 39: 25 quadros a 24 fps = 1042 ms. So'
       depois disso o `loop_ini` (25) entra, e o periodo passa a ser 15. */
    CHECK_EQ(res_rr_quadro(&e, 1000 + 1041), 39);
    CHECK_EQ(res_rr_quadro(&e, 1000 + 1042), 25);
    CHECK_EQ(res_rr_quadro(&e, 1000 + 1042 + 42), 26);
    CHECK_EQ(res_rr_quadro(&e, 1000 + 1042 + 625), 25);
    CHECK_EQ(res_rr_quadro(&e, 1000 + 620), 29);
    CHECK_EQ(res_rr_quadro(&e, 1000 + 600000), res_rr_quadro(&e, 1000 + 600000 - 15000));
    printf("  com rotulo inicial e loop: 15..39 e recomeca em 25 (periodo 15)\n");

    /* ---- 4b. `loopFrame` no MEIO (o caso do GREAT: loopFrame 15) ---------- */
    e = entrada(39, 0, 15, 3958);
    CHECK_EQ(res_rr_quadro(&e, 3958), 0);
    CHECK_EQ(res_rr_quadro(&e, 3958 + 1600), 38);
    CHECK_EQ(res_rr_quadro(&e, 3958 + 1650), 15);   /* 39 quadros = 1625 ms */
    CHECK_EQ(res_rr_quadro(&e, 3958 + 2500), 15 + 21);
    printf("  loop por quadro: 0..38 e recomeca no 15 (periodo 24)\n");

    /* ---- a TABELA de verdade --------------------------------------------- */
    printf("  tabela: %d ranks\n", RES_RR_RANKS);
    for (r = 0; r < 6; r++) {
        int idx = res_rr_indice(r);
        CHECK(idx >= 0);
        if (idx < 0) continue;
        printf("   rank %d (%-12s): %d entrada(s)", r, g_res_rr[idx].rank,
               (int)g_res_rr[idx].n);
        for (i = 0; i < (int)g_res_rr[idx].n; i++) {
            const res_rr_entrada_t *en = &g_res_rr[idx].entradas[i];
            CHECK(en->quadros > 0);
            CHECK(en->w > 0 && en->h > 0);
            CHECK(en->w <= 1024 && en->h <= 1024);
            CHECK(en->psm == 16 || en->psm == 32);
            CHECK(en->vram_bytes == (unsigned int)en->w * en->h *
                                    (unsigned int)(en->psm / 8));
            CHECK(en->n_partes >= 1);
            CHECK(en->loop_ini == -1 || en->loop_ini < en->quadros);
            CHECK(en->inicio < en->quadros);
            /* Todo rotulo da tabela do pacote tem de caber no pacote. */
            {
                int a;
                for (a = 0; a < (int)en->n_anims; a++) {
                    const res_rr_anim_t *an = &en->anims[a];
                    CHECK(an->quantos > 0);
                    CHECK((unsigned int)an->primeiro + an->quantos <= en->quadros);
                }
            }
            printf("  %s %dx%d %dq z%d %.0f,%.0f esc %.2f inicio %dms",
                   en->n_partes > 1 ? "+" : " ",
                   en->w, en->h, en->quadros, en->z, en->x, en->y, en->escala,
                   en->inicio_ms);
        }
        printf("\n");
        if (g_res_rr[idx].n > 0) {
            unsigned int soma = 0;
            for (i = 0; i < (int)g_res_rr[idx].n; i++)
                soma += g_res_rr[idx].entradas[i].vram_bytes;
            if (soma > pior) pior = soma;
        }
    }
    printf("  pior rank: %u B de VRAM contra %u B medidos livres na tela\n",
           pior, VRAM_LIVRE);
    CHECK(pior > 0);
    CHECK(pior <= VRAM_LIVRE);
    CHECK_EQ(res_rr_indice(-1), -1);
    CHECK_EQ(res_rr_indice(9), -1);

    printf(falhas ? "\nRESULTADO_RANK FALHOU (%d)\n" : "\nRESULTADO_RANK OK\n",
           falhas);
    return falhas ? 1 : 0;
}
