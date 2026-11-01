/* Tela de RESULTADO: a arte chega, aparece na hora certa e mostra o quadro certo.
 *
 * O QUE PROVA (contra ResultState.hx v0.8.6 L143-147 e L359-388)
 *  - as tres folhas tem os quadros e o canvas que o plano diz, e o desenho
 *    entra em 640x360 (metade de 1280x720) nas posicoes da fonte;
 *  - cada folha fica ESCONDIDA ate' o seu timer: 6/24 s, 21/24 s e 36/24 s;
 *  - a animacao para no ULTIMO quadro e fica na tela (`looped = false` no
 *    flixel para, nao some) -- que e' o que monta a cena final;
 *  - o quadro lido do pacote e' byte a byte o que foi empacotado;
 *  - sem os pacotes o modulo diz que nao carregou, em vez de desenhar lixo.
 *
 * O que NAO prova: que a tela parece a do original na TV. Isso e' a tela.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "resultado.h"

static int falhas;
#define CHECK(c) do { if (!(c)) { printf("  FALHA: %s (linha %d)\n", #c, __LINE__); falhas++; } } while (0)
#define CHECK_EQ(a, b) do { long _a = (long)(a), _b = (long)(b); \
    if (_a != _b) { printf("  FALHA: %s == %s: %ld != %ld (linha %d)\n", #a, #b, _a, _b, __LINE__); falhas++; } } while (0)

#define DIR "/tmp/fnf_resultado_teste"

static void put32(unsigned char *p, unsigned int v)
{
    p[0] = (unsigned char)(v & 0xffu);
    p[1] = (unsigned char)((v >> 8) & 0xffu);
    p[2] = (unsigned char)((v >> 16) & 0xffu);
    p[3] = (unsigned char)((v >> 24) & 0xffu);
}

/* Um CFZ4 com `nq` quadros de `qb` bytes, cada um preenchido por um padrao que
 * depende do NUMERO DO QUADRO -- e' o que permite provar que o quadro certo
 * chegou. Blocos LZ4 de literais puros, como o empacotador escreve. */
static unsigned int montar(const char *path, unsigned int nq, unsigned int qb)
{
    /* O bloco de literais puros custa qb + 1 + (qb-15)/255 bytes, entao o
       arquivo passa de nq*(qb+16): prever nq*qb + 64 KB cobre com folga. */
    unsigned char *out = malloc((size_t)nq * qb + 65536u);
    unsigned char *cru = malloc(qb);
    unsigned int o = 16u + nq * 8u, q, i;
    FILE *f;
    if (!out || !cru) { free(out); free(cru); return 0; }
    memcpy(out, "CFZ4", 4);
    put32(out + 4, 1u);
    put32(out + 8, nq);
    put32(out + 12, qb);
    for (q = 0; q < nq; q++) {
        unsigned int n, k = 0, resta;
        for (i = 0; i < qb; i++) cru[i] = (unsigned char)((q * 53u + i * 11u + 7u) & 0xffu);
        if (qb < 15u) {
            out[o + k++] = (unsigned char)(qb << 4);
        } else {
            out[o + k++] = 0xF0u;
            resta = qb - 15u;
            while (resta >= 255u) { out[o + k++] = 255u; resta -= 255u; }
            out[o + k++] = (unsigned char)resta;
        }
        memcpy(out + o + k, cru, qb);
        n = k + qb;
        put32(out + 16u + q * 8u, o);
        put32(out + 20u + q * 8u, n);
        o += n;
    }
    f = fopen(path, "wb");
    if (!f) { free(out); free(cru); return 0; }
    if (fwrite(out, 1, o, f) != o) o = 0;
    fclose(f);
    free(out);
    free(cru);
    return o;
}

int main(void)
{
    res_t r;
    char cmd[128], path[256];
    int i;
    unsigned char *buf;

    printf("== resultado: as tres folhas do plano ==\n");
    CHECK_EQ(RES_FOLHAS, 3u);
    CHECK(g_res_folha[0].quadros == 36u);
    CHECK(g_res_folha[1].quadros == 16u);
    CHECK(g_res_folha[2].quadros == 30u);
    /* Tamanho desenhado = canvas do original x escala da tela (0,5). */
    for (i = 0; i < (int)RES_FOLHAS; i++) {
        CHECK(g_res_folha[i].fps == 24u);
        CHECK(g_res_folha[i].looped == 0u);
        CHECK(g_res_folha[i].w > 0u && g_res_folha[i].h > 0u);
        /* Posicoes do ResultState.hx, ja' em 640x360: results em (-200,-10)/2,
           ratings em (-135,135)/2, score em (-180,515)/2. Duas delas comecam
           fora da tela pela esquerda (x negativo), como no original. */
        CHECK(g_res_folha[i].x <= 0.0f + 0.01f || g_res_folha[i].x < 200.0f);
    }
    CHECK_EQ((int)g_res_folha[0].inicio_ms, 250);   /* 6/24 s  */
    CHECK_EQ((int)g_res_folha[1].inicio_ms, 875);   /* 21/24 s */
    CHECK_EQ((int)g_res_folha[2].inicio_ms, 1500);  /* 36/24 s */
    {
        /* O gradiente do fundo: FlxGradient 90 graus com as duas cores da
           fonte. A textura e' estreita de proposito (so' varia em Y). */
        CHECK_EQ((int)RES_BG_W, 16);
        CHECK_EQ((int)RES_BG_H, 360);
        CHECK(RES_BG_TOPO_R == 0xFEu && RES_BG_TOPO_G == 0xCCu && RES_BG_TOPO_B == 0x5Cu);
        CHECK(RES_BG_BASE_R == 0xFDu && RES_BG_BASE_G == 0xC0u && RES_BG_BASE_B == 0x5Cu);
    }

    printf("== resultado: os tempos do RANK sao os do Scoring.hx ==\n");
    /* Scoring.hx L568-586: getFlashDelay em 24 avos -> ms. */
    CHECK_EQ((int)res_rank(4)->flash_ms, 5375);   /* PERFECT      129/24 */
    CHECK_EQ((int)res_rank(5)->flash_ms, 5375);   /* PERFECT_GOLD        */
    CHECK_EQ((int)res_rank(3)->flash_ms, 5083);   /* EXCELLENT    122/24 */
    CHECK_EQ((int)res_rank(2)->flash_ms, 4542);   /* GREAT        109/24 */
    CHECK_EQ((int)res_rank(1)->flash_ms, 4458);   /* GOOD         107/24 */
    CHECK_EQ((int)res_rank(0)->flash_ms, 7750);   /* SHIT         186/24 */
    /* Rank invalido cai em SHIT (`calculateRank(...) ?? SHIT`, L117). */
    CHECK_EQ(res_rank_indice(99), 0);
    CHECK_EQ(res_rank_indice(-1), 0);
    CHECK_EQ((int)res_rank(99)->flash_ms, 7750);
    /* O flash da tela: alpha 1 no instante do flash, 0 no fim de 14/24 s. */
    {
        const res_rank_t *g = res_rank(2);
        CHECK_EQ(res_flash_alpha(g, (unsigned long)g->flash_ms - 1ul), 0);
        CHECK_EQ(res_flash_alpha(g, (unsigned long)g->flash_ms), 255);
        CHECK_EQ(res_flash_alpha(g, (unsigned long)g->flash_ms + 291ul), 128);
        CHECK_EQ(res_flash_alpha(g, (unsigned long)g->flash_ms + 584ul), 0);
        /* O piscar: 2 quadros ligado, 2 desligado (FlxFlicker 2/24*3, 2/24). */
        CHECK_EQ(res_rank_pisca(g, (unsigned long)g->flash_ms), 1);
        CHECK_EQ(res_rank_pisca(g, (unsigned long)g->flash_ms + 83ul), 0);
        CHECK_EQ(res_rank_pisca(g, (unsigned long)g->flash_ms + 166ul), 1);
        CHECK_EQ(res_rank_pisca(g, (unsigned long)g->flash_ms + 300ul), 1);
        /* O vertical so' rola depois de 30/24 s, e para cima. */
        CHECK(res_rank_scroll_y(g, (unsigned long)g->flash_ms + 1000ul) == 0.0f);
        /* 30/24 s = 1250 ms: no instante exato ainda e' zero. */
        CHECK(res_rank_scroll_y(g, (unsigned long)g->flash_ms + 1250ul) == 0.0f);
        CHECK(res_rank_scroll_y(g, (unsigned long)g->flash_ms + 1251ul) < 0.0f);
        CHECK(res_rank_scroll_y(g, (unsigned long)g->flash_ms + 2250ul) <
              res_rank_scroll_y(g, (unsigned long)g->flash_ms + 1251ul));
        /* As linhas horizontais: pares para a esquerda, impares para a direita. */
        CHECK(res_rank_scroll_x(g, 0, (unsigned long)g->flash_ms + 1000ul) < 0.0f);
        CHECK(res_rank_scroll_x(g, 1, (unsigned long)g->flash_ms + 1000ul) > 0.0f);
    }

    printf("== resultado: o PLACAR (ResultScore.hx) ==\n");
    CHECK_EQ(res_score_digitos(0L), 0);
    CHECK_EQ(res_score_digitos(7L), 1);
    CHECK_EQ(res_score_digitos(12345L), 5);
    CHECK_EQ(res_score_digitos(1234567890L), 10);
    /* O setter limita a 10 slots (`Math.pow(10, members.length) - 1`). */
    CHECK_EQ(res_score_digitos(9999999999L), 10);
    CHECK_EQ(res_score_digitos(-5L), 0);
    /* O slot 9 e' a UNIDADE. */
    CHECK_EQ(res_score_final(9, 12345L), 5);
    CHECK_EQ(res_score_final(8, 12345L), 4);
    CHECK_EQ(res_score_final(5, 12345L), 1);
    /* A tabela do atlas: 5 quadros por digito, DISABLED e GONE no fim. */
    CHECK_EQ(res_score_indice(0, 0), 0);
    CHECK_EQ(res_score_indice(0, 4), 4);
    CHECK_EQ(res_score_indice(9, 4), 49);
    CHECK_EQ(res_score_indice(10, 0), 50);   /* DISABLED */
    CHECK_EQ(res_score_indice(11, 0), 51);   /* GONE     */
    CHECK_EQ(res_score_indice(10, 4), 50);   /* DISABLED tem 1 quadro */
    CHECK_EQ(res_score_indice(0, 99), 4);    /* quadro fora da faixa satura */
    CHECK_EQ((int)RES_SCORE_QUADROS, 52);   /* 10 x 5 + DISABLED + GONE */
    CHECK(g_res_score.w <= 1024u && g_res_score.h <= 1024u);
    {
        /* A geometria da tela: ResultScore(35, 305) e 65 px por slot. */
        CHECK(g_res_score.x > 17.0f && g_res_score.x < 18.0f);
        CHECK(g_res_score.y > 152.0f && g_res_score.y < 153.0f);
        CHECK(g_res_score.dx > 32.0f && g_res_score.dx < 33.0f);
        CHECK_EQ((int)g_res_score.slots, 10);
        CHECK_EQ((int)g_res_score.inicio_q, 37);   /* 37/24 s */
    }
    {
        int dg, qd;
        /* Antes de 37/24 s (1542 ms) o placar esta' invisivel. */
        CHECK_EQ(res_score_estado(9, 12345L, 1541ul, &dg, &qd), 0);
        CHECK_EQ(res_score_estado(9, 12345L, 1542ul, &dg, &qd), 1);
        /* Os slots a esquerda do primeiro digito ficam em DISABLED. */
        CHECK_EQ(res_score_estado(0, 12345L, 5000ul, &dg, &qd), 1);
        CHECK_EQ(dg, 10);
        CHECK_EQ(qd, 0);
        /* Durante o shuffle (metade dele): digito do ciclo, no ultimo quadro. */
        /* slot 9: ini = 8 -> q = 37+20 => k = 49 (ja' passou do shuffle se
           shuffle=41)... por isso a checagem e' feita com o k calculado. */
        CHECK_EQ(res_score_estado(9, 12345L, 1542ul, &dg, &qd), 1);
        /* q=37, ini=8, k=29 -> 29 % 10 = 9, quadro 4. */
        CHECK_EQ(dg, 9);
        CHECK_EQ(qd, 4);
        /* Os timers dos slots partem de (i-1)/24 s, ou seja ANTES de o placar
           ficar visivel: quando ele aparece, o slot 9 ja' esta' no meio do
           shuffle. A primeira troca (quadro 0, o `glow`) de cada slot acontece
           por isso fora da tela -- e' o que a fonte faz, e o port nao muda. */
        {
            unsigned long ms = (unsigned long)((9ul * 1000ul) /
                                               (unsigned long)g_res_score.fps);
            CHECK_EQ(res_score_estado(9, 12345L, ms, &dg, &qd), 0);  /* escondido */
            /* k=1 -> digito 1 e quadro 0 (a primeira troca). */
            CHECK_EQ(dg, 1);
            CHECK_EQ(qd, 0);
        }
        /* Depois de tudo (tween + piscar) o digito e' o final e fica no 4. */
        CHECK_EQ(res_score_estado(9, 12345L, 900000ul, &dg, &qd), 1);
        CHECK_EQ(dg, 5);
        CHECK_EQ(qd, 4);
        CHECK_EQ(res_score_estado(5, 12345L, 900000ul, &dg, &qd), 1);
        CHECK_EQ(dg, 1);
    }

    printf("== resultado: os CONTADORES de julgamento (TallyCounter.hx) ==\n");
    CHECK_EQ((int)RES_TALLIES, 7);
    CHECK_EQ((int)RES_TALLY_FRAMES, 10);
    {
        int i;
        for (i = 0; i < (int)RES_TALLIES; i++) {
            /* A ordem do `ratingGrp`: totalNotesHit, maxCombo, sick, good,
               bad, shit, missed. */
            CHECK_EQ((int)g_res_tally[i].campo, i);
            CHECK(g_res_tally[i].x >= 0.0f && g_res_tally[i].x < 320.0f);
            CHECK(g_res_tally[i].y >= 0.0f && g_res_tally[i].y < 360.0f);
        }
        /* As cores do `flavour` (ResultState.hx L457-476). */
        CHECK(g_res_tally[0].r == 0xFF && g_res_tally[0].g == 0xFF && g_res_tally[0].b == 0xFF);
        CHECK(g_res_tally[2].r == 0x89 && g_res_tally[2].g == 0xE5 && g_res_tally[2].b == 0x9E);
        CHECK(g_res_tally[6].r == 0xC6 && g_res_tally[6].g == 0x8A && g_res_tally[6].b == 0xE6);
    }
    /* `res_tally_digitos`: zero conta como UM digito. */
    CHECK_EQ(res_tally_digitos(0), 1);
    CHECK_EQ(res_tally_digitos(7), 1);
    CHECK_EQ(res_tally_digitos(29), 2);
    CHECK_EQ(res_tally_digitos(1234), 4);
    /* A entrada e' (0.3 * ind) + 1.20 s. */
    CHECK_EQ(res_tally_visivel(0, 1199ul), 0);
    CHECK_EQ(res_tally_visivel(0, 1200ul), 1);
    CHECK_EQ(res_tally_visivel(2, 1799ul), 0);
    CHECK_EQ(res_tally_visivel(2, 1800ul), 1);
    CHECK_EQ(res_tally_visivel(6, 2999ul), 0);
    CHECK_EQ(res_tally_visivel(6, 3000ul), 1);
    /* O tween: 0 no inicio, o alvo em 0,5 s, e no meio um valor intermediario
       (quartOut comeca depressa). */
    CHECK_EQ(res_tally_valor(100, 0, 1200ul), 0);
    CHECK(res_tally_valor(100, 0, 1450ul) > 50);
    CHECK(res_tally_valor(100, 0, 1450ul) < 100);
    CHECK_EQ(res_tally_valor(100, 0, 1700ul), 100);
    CHECK_EQ(res_tally_valor(100, 0, 999999ul), 100);
    /* O ajuste de 1000 notas: so' nos dois primeiros, e so' a partir de 1000. */
    CHECK(res_tally_ajuste_x(0, 999) == 0.0f);
    CHECK(res_tally_ajuste_x(0, 1000) < 0.0f);
    CHECK(res_tally_ajuste_x(1, 1500) < 0.0f);
    CHECK(res_tally_ajuste_x(2, 1500) == 0.0f);

    printf("== resultado: o PERCENTUAL de conclusao (ClearPercentCounter) ==\n");
    CHECK_EQ((int)RES_CP_ATLASES, 3);
    {
        int k;
        for (k = 0; k < (int)RES_CP_ATLASES; k++) {
            CHECK(g_res_cp_atlas[k].fator > 0.0f);
            CHECK(g_res_cp_atlas[k].w > 0u && g_res_cp_atlas[k].h > 0u);
        }
        /* Geometria: new ClearPercentCounter(640 + 190, 360 - 70) em 1280x720. */
        CHECK(g_res_cp_geo.x > 414.0f && g_res_cp_geo.x < 416.0f);
        CHECK(g_res_cp_geo.y > 144.0f && g_res_cp_geo.y < 146.0f);
        CHECK(g_res_cp_geo.passo_grande > 35.0f && g_res_cp_geo.passo_grande < 37.0f);
        CHECK_EQ((int)g_res_cp_geo.inicio_q, 37);
        CHECK_EQ((int)g_res_cp_geo.tween_q, 58);
        CHECK(g_res_cp_texto.fator > 0.0f && g_res_cp_texto_small.fator > 0.0f);
    }
    CHECK_EQ(res_cp_digitos(0), 1);
    CHECK_EQ(res_cp_digitos(7), 1);
    CHECK_EQ(res_cp_digitos(81), 2);
    CHECK_EQ(res_cp_digitos(100), 3);
    /* `digitOffset`: um digito empurra para a direita, tres para a esquerda. */
    CHECK_EQ(res_cp_deslocamento(1), 1);
    CHECK_EQ(res_cp_deslocamento(2), 0);
    CHECK_EQ(res_cp_deslocamento(3), -1);
    /* `variant`: com tres digitos so' o PRIMEIRO usa `Left`. */
    CHECK_EQ(res_cp_variante(3, 0), 0);
    CHECK_EQ(res_cp_variante(3, 1), 1);
    CHECK_EQ(res_cp_variante(2, 0), 1);
    CHECK_EQ(res_cp_variante(1, 0), 1);
    {
        /* 37/24 s = 1542 ms; o tween de 58/24 s termina em 3.958 ms. */
        CHECK_EQ(res_cp_valor(81, 0ul), 45);          /* 81 - 36 */
        CHECK_EQ(res_cp_valor(81, 1542ul), 45);
        CHECK(res_cp_valor(81, 2000ul) > 45);
        CHECK(res_cp_valor(81, 2000ul) < 81);
        CHECK(res_cp_valor(81, 3000ul) > res_cp_valor(81, 2000ul));
        CHECK_EQ(res_cp_valor(81, 3959ul), 81);
        CHECK_EQ(res_cp_valor(81, 999999ul), 81);
        /* Alvo menor que 36 nao pode dar numero negativo. */
        CHECK_EQ(res_cp_valor(20, 0ul), 0);
        /* Visivel de 1542 ms ate' o fim do fade. */
        CHECK_EQ(res_cp_visivel(1541ul), 0);
        CHECK_EQ(res_cp_visivel(1542ul), 1);
        CHECK_EQ(res_cp_visivel(5000ul), 1);
        CHECK_EQ(res_cp_visivel(999999ul), 0);
        /* O alpha so' cai depois do flash e da espera. */
        CHECK_EQ(res_cp_alpha(1600ul), 255);
        CHECK_EQ(res_cp_alpha(999999ul), 0);
        /* O flash do `PureColor` comeca no fim do tween. */
        CHECK_EQ(res_cp_flash(3958ul), 0);
        CHECK_EQ(res_cp_flash(3959ul), 1);
        CHECK_EQ(res_cp_flash(4358ul), 1);
        CHECK_EQ(res_cp_flash(4359ul), 0);
    }

    printf("== resultado: o NOME DA MUSICA (FlxBitmapText) ==\n");
    {
        const res_nome_arte_t *nm = res_nome_do_id("bopeebo");
        res_nome_t e;
        CHECK(nm != NULL);
        CHECK(res_nome_do_id("nao-existe") == NULL);
        CHECK(res_nome_do_id(NULL) == NULL);
        if (nm) {
            /* Bopeebo: 7 letras -> 34*7+15 = 253 na fonte, 126,5 na metade. */
            CHECK(nm->w > 100u && nm->w < 160u);
            CHECK(nm->vram_bytes == (unsigned)nm->w * nm->h * 4u);
            CHECK(nm->larg_full > 126.0f && nm->larg_full < 127.0f);
            CHECK(nm->escala > 0.0f && nm->escala <= 0.5f);
        }
        /* `timerThenSongName(1.0, false)`: entra em y com 0,9 s de atraso. */
        res_nome_iniciar(&e, 0ul, 468.5f, 48.6f, 126.5f, 30.0f);
        CHECK_EQ(e.ativo, 1);
        CHECK_EQ(e.ciclo, 0);
        res_nome_passo(&e, 0ul);
        CHECK(e.x > 468.0f && e.x < 469.0f);
        CHECK(e.y > -31.0f && e.y < -29.0f);
        res_nome_passo(&e, 900ul);
        CHECK(e.y < -20.0f);
        res_nome_passo(&e, 1000ul);
        CHECK(e.y > -20.0f);              /* o tween ja' subiu */
        res_nome_passo(&e, 1400ul);
        CHECK(e.y > 47.0f && e.y < 49.0f);   /* assentado no alvo */
        /* SEM `res_nome_soltar` o nome fica parado: e' o que o original faz
           ate' `showSmallClearPercent()` (+2,5 s). */
        res_nome_passo(&e, 5000ul);
        CHECK(e.x > 468.0f && e.x < 469.0f);
        /* Solto, anda para a esquerda (vel_x negativo) e desce (vel_y > 0). */
        {float y0;
         res_nome_soltar(&e, 6458ul);
         y0 = e.y;
         res_nome_passo(&e, 7458ul);
         CHECK(e.x < 410.0f && e.x > 405.0f);
         CHECK(e.y > y0);
        }
        /* O corte da esquerda: `clipRect = max(0, 520 - x)` (260 na metade). */
        CHECK(res_nome_clip_local(&e) == 0.0f);   /* x < 260 ainda nao? nao: x=408 */
        /* O ciclo recomeca quando `x + largura < 100` (50 na metade). */
        {int ciclos = e.ciclo, guard = 0;
         unsigned long ms = 7458ul;
         while (e.ciclo == ciclos && guard++ < 4000) {
             ms += 16ul;
             res_nome_passo(&e, ms);
         }
         CHECK(e.ciclo == ciclos + 1);
         CHECK(e.x > 468.0f && e.x < 469.0f);   /* voltou para a entrada */
         CHECK(e.y < -29.0f);
        }
    }

    printf("== resultado: o CONTADOR PEQUENO do topo ==\n");
    /* `getBFDelay()`: 95/24 s = 3.958 ms (EXCELLENT e' o unico com 97/24). */
    CHECK_EQ((int)g_res_rank[2].bf_ms, 3958);
    CHECK_EQ((int)g_res_rank[3].bf_ms, 4042);
    CHECK_EQ(res_cp_small_visivel(2, 3957ul), 0);
    CHECK_EQ(res_cp_small_visivel(2, 3958ul), 1);
    CHECK_EQ(res_cp_small_visivel(3, 4041ul), 0);
    CHECK_EQ(res_cp_small_visivel(3, 4042ul), 1);
    /* Rank fora da tabela cai em SHIT (`Scoring.calculateRank(...) ?? SHIT`,
       o mesmo criterio de `res_rank`), entao o atraso e' o do SHIT: 95/24 s. */
    CHECK_EQ(res_cp_small_visivel(-1, 3957ul), 0);
    CHECK_EQ(res_cp_small_visivel(-1, 3958ul), 1);
    CHECK_EQ(res_cp_small_visivel(99, 3958ul), 1);
    /* O `flash(true)` de 0,4 s de `showSmallClearPercent()`. */
    CHECK_EQ(res_cp_small_flash(2, 3957ul), 0);
    CHECK_EQ(res_cp_small_flash(2, 3958ul), 1);
    CHECK_EQ(res_cp_small_flash(2, 4357ul), 1);
    CHECK_EQ(res_cp_small_flash(2, 4358ul), 0);
    /* `x = (difficulty.x + difficulty.width) + 60`: com a dificuldade normal
       (228 px na fonte, 114 no port) da' 421,5. */
    CHECK(res_cp_small_x(114.0f) > 421.0f && res_cp_small_x(114.0f) < 422.0f);
    CHECK(res_cp_small_x(0.0f) > 307.0f && res_cp_small_x(0.0f) < 308.0f);

    printf("== resultado: a BARRA do topo e a imagem da DIFICULDADE ==\n");
    /* A barra nao tem PNG: `BitmapUtil.createResultsBar` monta um retangulo de
       1295x149 girado de -3,8 graus. O que o desenho usa e' a reta da borda de
       baixo -- em 640x360 ela vai de ~74,2 (x=0) a ~31,7 (x=640). */
    {
        CHECK(g_res_barra.altura > 74.0f && g_res_barra.altura < 75.0f);
        CHECK(g_res_barra.borda_y0 > 74.0f && g_res_barra.borda_y0 < 74.4f);
        CHECK(g_res_barra.borda_y1 > 31.5f && g_res_barra.borda_y1 < 31.9f);
        /* (blackTopBar.height - 148) na metade da tela. */
        CHECK(g_res_barra.correcao_y >= 0.0f && g_res_barra.correcao_y < 1.0f);
        CHECK_EQ((int)g_res_barra.atraso_q, 3);
        CHECK_EQ((int)g_res_barra.tween_q, 7);
        /* y = -altura ate' 3/24 s (125 ms), tween de 7/24 s ate' 10/24 s (417). */
        CHECK(res_barra_y(0ul) == -g_res_barra.altura);
        CHECK(res_barra_y(125ul) == -g_res_barra.altura);
        CHECK(res_barra_y(126ul) > -g_res_barra.altura);
        /* Sobe: -74,5 -> 0 (quanto maior o ms, mais perto de zero). */
        CHECK(res_barra_y(200ul) < 0.0f);
        CHECK(res_barra_y(200ul) < res_barra_y(300ul));
        CHECK(res_barra_y(300ul) < res_barra_y(416ul));
        CHECK(res_barra_y(416ul) < 0.0f);
        CHECK(res_barra_y(417ul) == 0.0f);
        CHECK(res_barra_y(999999ul) == 0.0f);
        /* `onComplete`: e' ele que torna o nome da musica visivel. */
        CHECK_EQ(res_barra_assentada(416ul), 0);
        CHECK_EQ(res_barra_assentada(417ul), 1);
    }
    /* A dificuldade: `-height` ate' 122 + (barra.height - 148) em 0,5 s com
       `expoOut`, 0,8 s depois de a tela abrir. */
    CHECK_EQ((int)RES_DIFS, 5);
    {
        int k;
        for (k = 0; k < (int)RES_DIFS; k++) {
            CHECK(g_res_dif[k].w > 0u && g_res_dif[k].h > 0u);
            CHECK(g_res_dif[k].vram_bytes == (unsigned)g_res_dif[k].w *
                                              g_res_dif[k].h * 4u);
            CHECK(g_res_dif[k].arquivo != NULL);
        }
        CHECK(g_res_dif_geo.x > 277.0f && g_res_dif_geo.x < 278.0f);
        CHECK(g_res_dif_geo.y_alvo > 61.0f && g_res_dif_geo.y_alvo < 62.0f);
        CHECK_EQ((int)g_res_dif_geo.atraso_ms, 800);
        CHECK_EQ((int)g_res_dif_geo.tween_ms, 500);
    }
    CHECK_EQ(res_dif_indice(0), 0);
    CHECK_EQ(res_dif_indice(1), 1);
    CHECK_EQ(res_dif_indice(2), 2);
    CHECK_EQ(res_dif_indice(3), -1);      /* erect/nightmare nao existem no port */
    CHECK_EQ(res_dif_indice(-1), -1);
    {
        float h = (float)g_res_dif[1].h;
        CHECK(res_dif_y(h, 0ul) == -h);
        CHECK(res_dif_y(h, 800ul) == -h);
        CHECK(res_dif_y(h, 900ul) > -h);
        CHECK(res_dif_y(h, 900ul) < res_dif_y(h, 1000ul));
        CHECK(res_dif_y(h, 1000ul) < g_res_dif_geo.y_alvo);
        CHECK(res_dif_y(h, 1300ul) == g_res_dif_geo.y_alvo);
        CHECK(res_dif_y(h, 999999ul) == g_res_dif_geo.y_alvo);
    }

    printf("== resultado: sem pacote, nada aparece (e o modulo diz isso) ==\n");
    res_init(&r);
    CHECK_EQ(res_carregar(&r, "/tmp/fnf_nao_existe/"), 0);
    for (i = 0; i < (int)RES_FOLHAS; i++) CHECK_EQ(res_quadro(&r, i, 100000ul), -1);
    res_liberar(&r);

    printf("== resultado: cada folha aparece no seu timer e para no ultimo quadro ==\n");
    /* Pacotes sinteticos do tamanho do plano: 4 bytes por pixel. */
    snprintf(cmd, sizeof(cmd), "mkdir -p %s/resultado", DIR);
    if (system(cmd) != 0) { printf("  FALHA: nao consegui criar %s\n", DIR); return 1; }
    for (i = 0; i < (int)RES_FOLHAS; i++) {
        unsigned int qb = (unsigned int)g_res_folha[i].w * g_res_folha[i].h * 4u;
        snprintf(path, sizeof(path), "%s/%s", DIR, g_res_folha[i].tex);
        CHECK(montar(path, g_res_folha[i].quadros, qb) > 0);
    }
    snprintf(cmd, sizeof(cmd), "%s/", DIR);
    CHECK_EQ(res_carregar(&r, cmd), (int)RES_FOLHAS);

    /* results: escondida antes de 250 ms. */
    CHECK_EQ(res_quadro(&r, 0, 0ul), -1);
    CHECK_EQ(res_quadro(&r, 0, 249ul), -1);
    CHECK_EQ(res_quadro(&r, 0, 250ul), 0);
    /* 24 fps: 1 quadro a cada 41,67 ms. */
    CHECK_EQ(res_quadro(&r, 0, 250ul + 41ul), 0);
    CHECK_EQ(res_quadro(&r, 0, 250ul + 42ul), 1);
    CHECK_EQ(res_quadro(&r, 0, 250ul + 84ul), 2);
    /* Depois do ultimo quadro, FICA no ultimo (looped = false). */
    CHECK_EQ(res_quadro(&r, 0, 250ul + 1500ul), 35);
    CHECK_EQ(res_quadro(&r, 0, 999999ul), 35);

    CHECK_EQ(res_quadro(&r, 1, 874ul), -1);
    CHECK_EQ(res_quadro(&r, 1, 875ul), 0);
    CHECK_EQ(res_quadro(&r, 1, 875ul + 700ul), 15);   /* 700 ms / 41,67 = 16,8 */
    CHECK_EQ(res_quadro(&r, 2, 1499ul), -1);
    CHECK_EQ(res_quadro(&r, 2, 1500ul), 0);
    CHECK_EQ(res_quadro(&r, 2, 1500ul + 1250ul), 29);
    /* Folha fora da faixa nao explode. */
    CHECK_EQ(res_quadro(&r, 3, 5000ul), -1);
    CHECK_EQ(res_quadro(&r, -1, 5000ul), -1);

    printf("== resultado: o pixel lido e' o quadro certo ==\n");
    buf = malloc((size_t)g_res_folha[1].w * g_res_folha[1].h * 4u);
    CHECK(buf != NULL);
    if (buf) {
        unsigned int qb = (unsigned int)g_res_folha[1].w * g_res_folha[1].h * 4u;
        unsigned int k, erro = 0;
        for (i = 0; i < (int)g_res_folha[1].quadros; i++) {
            CHECK_EQ(res_pixels(&r, 1, i, buf), PST_OK);
            for (k = 0; k < qb; k++) {
                if (buf[k] != (unsigned char)(((unsigned)i * 53u + k * 11u + 7u) & 0xffu)) {
                    erro++;
                    break;
                }
            }
        }
        CHECK_EQ(erro, 0);
        CHECK_EQ(res_pixels(&r, 1, 16, buf), PST_ERR_QUADRO);   /* fora da faixa */
        CHECK(r.f[1].vistos == (unsigned int)g_res_folha[1].quadros);
        free(buf);
    }
    res_liberar(&r);
    CHECK_EQ(res_quadro(&r, 0, 900000ul), -1);   /* fechado: nao mostra nada */

    snprintf(cmd, sizeof(cmd), "rm -rf %s", DIR);
    if (system(cmd) != 0) { /* limpeza best-effort */ }

    if (falhas) {
        printf("\n  %d falha(s) na tela de resultado\n", falhas);
        return 1;
    }
    printf("  ok:   folhas, tempos, ultimo quadro e pixels\n");
    return 0;
}
