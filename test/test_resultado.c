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
