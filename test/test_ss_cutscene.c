/* A CUTSCENE do `sserafim`: o pacote assado e a linha do tempo.
 *
 * Tudo aqui roda no PC: o pacote e' sintetico (tres quadros de dois quads) e a
 * linha do tempo e' a do script, com o instante e a linha da fonte em
 * `src/engine/ss_cutscene.h`. O que se prova: o leitor nao le' fora do buffer,
 * aceita e recusa exatamente os arquivos que deve, o quadro certo para cada
 * instante, a geometria dos quads e cada marco da cutscene (camera, fades,
 * cobertura, visibilidade e o fim em 730/24 s).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "engine/ss_cutscene.h"

static int falhas;
#define CHECK(c) do { if (!(c)) { printf("  FALHA: %s (linha %d)\n", #c, __LINE__); falhas++; } } while (0)
#define CHECK_EQ(a, b) do { long _a = (long)(a), _b = (long)(b); if (_a != _b) { \
    printf("  FALHA: %s == %s (%ld != %ld, linha %d)\n", #a, #b, _a, _b, __LINE__); falhas++; } } while (0)
#define CHECK_NEAR(a, b) do { double _a = (double)(a), _b = (double)(b), _d = _a - _b; \
    if (_d < 0) _d = -_d; \
    if (_d > 1e-3) { \
    printf("  FALHA: %s ~= %s (%.6f != %.6f, linha %d)\n", #a, #b, (double)_a, (double)_b, __LINE__); falhas++; } } while (0)

/* Monta um pacote de teste no formato do tools/cutscene_pacote.py (FQC1 v2):
   2 sprites (8x4 e 3x2), 3 quadros (1, 2 e 1 quads) e DOIS rotulos -- o
   `static` no quadro 0 e o `getup` nos dois seguintes, que sao as etiquetas da
   camada `Layer_15` do `gfGetUp` (sserafim.hxc). */
static unsigned char *pacote_com(int rotulos, int *tamanho)
{
    const int nsp = 2, nfr = 3;
    const int nlab = rotulos ? 2 : 0;
    const int cab = 24 + nsp * 8 + nlab * 12 + nfr * 4;
    const int quads[4][7] = {
        /* sprite, a, b, c, d, tx, ty   (16 bits cada) */
        {0, 4096, 0, 0, 4096, 40, 80},
        {1, 4096, 0, 0, 4096, 0, 0},
        {1, 2048, 0, 0, 2048, -4, 8},
        {0, 4096, 0, 0, 4096, 0, 0}
    };
    const int quadro_ini[3] = {0, 14, 42};   /* em bytes, relativo ao fim da tabela */
    unsigned char *d = (unsigned char *)calloc(1, (size_t)(cab + 56));
    int i, off;
    memcpy(d, "FQC1", 4);
    d[4] = (unsigned char)nsp; d[5] = 0;
    d[6] = (unsigned char)nfr; d[7] = 0;
    /* fps100 = 2400 e escala 1,0 */
    d[8] = 0x60; d[9] = 0x09;
    d[12] = 0xE8; d[13] = 0x03;
    /* tex_w = 64, tex_h = 32, n_labels, reservado */
    d[16] = 64; d[18] = 32;
    d[20] = (unsigned char)nlab; d[21] = 0;
    /* sprites: x,y,w,h */
    {const unsigned short spr[2][4] = {{0, 0, 8, 4}, {8, 0, 3, 2}};
     for (i = 0; i < 2; i++) {
         memcpy(d + 24 + i * 8, spr[i], 8);
     }}
    off = 24 + nsp * 8;
    if (nlab) {
        /* u16 primeiro | u16 quadros | char nome[8] */
        const unsigned short lab[2][2] = {{0, 1}, {1, 2}};
        const char *nomes[2] = {"static", "getup"};
        int k;
        for (i = 0; i < 2; i++) {
            unsigned char *l = d + off + i * 12;
            for (k = 0; k < 2; k++) {
                l[k * 2] = (unsigned char)(lab[i][k] & 0xFF);
                l[k * 2 + 1] = (unsigned char)((lab[i][k] >> 8) & 0xFF);
            }
            memcpy(l + 4, nomes[i], strlen(nomes[i]));
        }
    }
    off += nlab * 12;
    for (i = 0; i < 3; i++) {
        d[off + i * 4] = (unsigned char)(quadro_ini[i] & 0xFF);
    }
    off += 12;
    for (i = 0; i < 4; i++) {
        unsigned char *q = d + off + i * 14;
        int k;
        for (k = 0; k < 7; k++) {
            unsigned int v = (unsigned int)(unsigned short)quads[i][k];
            q[k * 2] = (unsigned char)(v & 0xFF);
            q[k * 2 + 1] = (unsigned char)((v >> 8) & 0xFF);
        }
    }
    *tamanho = off + 56;
    return d;
}
static unsigned char *pacote(int *tamanho) { return pacote_com(1, tamanho); }

int main(void)
{
    int len = 0, total = 0, n, i;
    unsigned char *d = pacote(&len);
    ssc_pack_t p;
    ssc_quad_t q[SSC_MAX_QUADS];

    printf("== cutscene: o leitor do pacote assado ==\n");
    CHECK_EQ(ssc_abrir(&p, d, (unsigned int)len), SSC_OK);
    CHECK_EQ(p.n_sprites, 2);
    CHECK_EQ(p.n_frames, 3);
    CHECK_EQ(p.fps100, 2400);
    CHECK_EQ(p.escala1000, 1000);
    CHECK_EQ(p.quads_bytes, 56u);
    CHECK_EQ(p.tex_w, 64);
    CHECK_EQ(p.tex_h, 32);
    CHECK_EQ(p.n_labels, 2);
    /* Cabecalho errado e arquivo curto tem de ser RECUSADOS: um pacote de
       outra coisa desenharia lixo em vez de faltar. */
    d[0] = 'X';
    CHECK_EQ(ssc_abrir(&p, d, (unsigned int)len), SSC_ERRO_MAGIC);
    d[0] = 'F';
    CHECK_EQ(ssc_abrir(&p, d, 10u), SSC_ERRO_TRUNCADO);
    CHECK_EQ(ssc_abrir(&p, d, (unsigned int)len - 1u), SSC_OK);   /* 55 B: ainda 3 quads */
    CHECK_EQ(ssc_abrir(&p, NULL, (unsigned int)len), SSC_ERRO_TRUNCADO);
    CHECK_EQ(ssc_abrir(&p, d, (unsigned int)len), SSC_OK);

    printf("== cutscene: o quadro de cada instante (24 fps, sem laco) ==\n");
    /* 3 quadros a 24 fps = 0,125 s. Passado o ultimo, ele FICA: o `cutsceneMain`
       tem 631 quadros e a cena usa 563; com laco a cena recomecaria no meio. */
    CHECK_EQ(ssc_quadro_em(&p, 0.0f), 0);
    CHECK_EQ(ssc_quadro_em(&p, 0.02f), 0);
    CHECK_EQ(ssc_quadro_em(&p, 0.045f), 1);
    CHECK_EQ(ssc_quadro_em(&p, 0.09f), 2);
    CHECK_EQ(ssc_quadro_em(&p, 0.13f), 2);      /* congelou no ultimo */
    CHECK_EQ(ssc_quadro_em(&p, -1.0f), 0);      /* antes do inicio: quadro 0 */
    CHECK_EQ(ssc_quadro_em(&p, 100.0f), 2);     /* sem estourar a faixa */

    printf("== cutscene: os ROTULOS de quadro (o `static` e o `getup`) ==\n");
    {
        int ini = -1, quantos = -1;
        /* `static` (quadro 0) e `getup` (quadros 1 e 2) -- as etiquetas que o
           `SserafimGfSprite.resetAnim()/doAnim()` tocam. */
        CHECK_EQ(ssc_rotulo(&p, "static", &ini, &quantos), SSC_OK);
        CHECK_EQ(ini, 0); CHECK_EQ(quantos, 1);
        CHECK_EQ(ssc_rotulo(&p, "getup", &ini, &quantos), SSC_OK);
        CHECK_EQ(ini, 1); CHECK_EQ(quantos, 2);
        /* rotulo que o pacote nao tem: NADA para desenhar (-1), nunca o quadro
           errado -- um `getup` ausente nao pode cair no `static`. */
        CHECK(ssc_rotulo(&p, "nada", &ini, &quantos) != SSC_OK);
        CHECK_EQ(ssc_quadro_rotulo(&p, "nada", 0.0f), -1);
        /* o rotulo de um quadro so' devolve sempre o mesmo */
        CHECK_EQ(ssc_quadro_rotulo(&p, "static", 0.0f), 0);
        CHECK_EQ(ssc_quadro_rotulo(&p, "static", 9.0f), 0);
        /* o `getup` comeca no quadro 1 e para no 2 (sem laco) */
        CHECK_EQ(ssc_quadro_rotulo(&p, "getup", 0.0f), 1);
        CHECK_EQ(ssc_quadro_rotulo(&p, "getup", 0.05f), 2);
        CHECK_EQ(ssc_quadro_rotulo(&p, "getup", 0.09f), 2);
        CHECK_EQ(ssc_quadro_rotulo(&p, "getup", 5.0f), 2);
    }
    /* Um pacote SEM tabela de rotulos: as duas consultas tem de dizer -1, e nao
       devolver o quadro 0 disfarcado. */
    {
        int len2 = 0;
        unsigned char *d2 = pacote_com(0, &len2);
        ssc_pack_t p2;
        CHECK_EQ(ssc_abrir(&p2, d2, (unsigned int)len2), SSC_OK);
        CHECK_EQ(p2.n_labels, 0);
        CHECK_EQ(ssc_quadro_rotulo(&p2, "getup", 0.0f), -1);
        CHECK_EQ(ssc_quadro_em(&p2, 0.09f), 2);
        free(d2);
    }

    printf("== cutscene: os quads do quadro e o corte do excesso ==\n");
    CHECK_EQ(ssc_quads(&p, 0, q, SSC_MAX_QUADS, &total), 1);
    CHECK_EQ(total, 1);
    CHECK_EQ(q[0].sprite, 0);
    CHECK_EQ(q[0].tx, 40);
    CHECK_EQ(ssc_quads(&p, 1, q, SSC_MAX_QUADS, &total), 2);
    CHECK_EQ(total, 2);
    CHECK_EQ(q[1].a, 2048);
    CHECK_EQ(q[1].tx, -4);
    /* Espaco menor que o numero de quads: desenha o que cabe e DIZ o total --
       meia cena com o numero na mao e' melhor que meia cena calada. */
    CHECK_EQ(ssc_quads(&p, 1, q, 1, &total), 1);
    CHECK_EQ(total, 2);
    /* Quadro fora da faixa e' preso na faixa, nao lido do lixo. */
    CHECK_EQ(ssc_quads(&p, 99, q, SSC_MAX_QUADS, &total), 1);
    CHECK_EQ(ssc_quads(&p, -5, q, SSC_MAX_QUADS, &total), 1);

    printf("== cutscene: a geometria de um quad (matriz 2x3) ==\n");
    {
        float px[4], py[4];
        /* sprite 0 = 8x4, matriz identidade, deslocamento (40/4, 80/4) = (10,20) */
        CHECK_EQ(ssc_cantos(&p, &q[0], 100.0f, 200.0f, px, py), 1);
        CHECK_NEAR(px[0], 110.0); CHECK_NEAR(py[0], 220.0);
        CHECK_NEAR(px[1], 118.0); CHECK_NEAR(py[1], 220.0);
        CHECK_NEAR(px[2], 118.0); CHECK_NEAR(py[2], 224.0);
        CHECK_NEAR(px[3], 110.0); CHECK_NEAR(py[3], 224.0);
        /* sprite 1 = 3x2 com escala 0,5 (2048/4096) e deslocamento (-1, 2) */
        CHECK_EQ(ssc_cantos(&p, &q[1], 0.0f, 0.0f, px, py), 1);
        CHECK_NEAR(px[0], -1.0); CHECK_NEAR(py[0], 2.0);
        CHECK_NEAR(px[1], -1.0 + 1.5); CHECK_NEAR(py[1], 2.0);
        CHECK_NEAR(px[2], 0.5); CHECK_NEAR(py[2], 3.0);
        CHECK_NEAR(px[3], -1.0); CHECK_NEAR(py[3], 3.0);
        /* indice de sprite invalido nao desenha */
        q[2].sprite = 7;
        CHECK_EQ(ssc_cantos(&p, &q[2], 0, 0, px, py), 0);
    }

    printf("== cutscene: as curvas do `FlxEase` ==\n");
    {
        /* Os valores de referencia sao da formula, nao do port: em t=0,5
           circOut = sqrt(1-0,25) = 0,866025 e sineOut = sin(pi/4) = 0,707107. */
        CHECK_NEAR(ssc_circ_out(0.0f), 0.0f);
        CHECK_NEAR(ssc_circ_out(1.0f), 1.0f);
        CHECK_NEAR(ssc_circ_out(0.5f), 0.866025);
        CHECK_NEAR(ssc_sine_out(0.0f), 0.0f);
        CHECK_NEAR(ssc_sine_out(1.0f), 1.0f);
        CHECK_NEAR(ssc_sine_out(0.5f), 0.707107);
        /* monotona e dentro de [0,1] -- uma curva que passe de 1 poria a
           camera alem do alvo */
        for (i = 0; i <= 100; i++) {
            float t = (float)i / 100.0f;
            CHECK(ssc_circ_out(t) >= -1e-4f && ssc_circ_out(t) <= 1.0f + 1e-4f);
            CHECK(ssc_sine_out(t) >= -1e-4f && ssc_sine_out(t) <= 1.0f + 1e-4f);
        }
    }

    printf("== cutscene: a linha do tempo (camera, fades, cena e o fim) ==\n");
    {
        ssc_estado_t e;
        /* t=0: zoom 0,5, camera (660,-200), fade preto cheio, cena ligada,
           HUD fora, e as "get up" ainda nao. */
        ssc_estado(0.0f, &e);
        CHECK_NEAR(e.zoom, 0.5);
        CHECK_NEAR(e.cam_x, 660.0);
        CHECK_NEAR(e.cam_y, -200.0);
        CHECK_NEAR(e.fade_preto, 1.0);
        CHECK_NEAR(e.fade_branco, 0.0);
        CHECK_NEAR(e.cover, 0.0);
        CHECK_EQ(e.cena, 1);
        CHECK_EQ(e.hud, 0);
        CHECK_EQ(e.principal_visivel, 1);
        CHECK_EQ(e.gf_visivel, 0);
        CHECK_EQ(e.acabou, 0);

        /* t=3 (SSC_T_CAM_FIM): zoom 0,7 e camera (660,300) -- o fim do
           `circOut` de 3 s. */
        ssc_estado(3.0f, &e);
        CHECK_NEAR(e.zoom, 0.7);
        CHECK_NEAR(e.cam_y, 300.0);
        CHECK_NEAR(e.fade_preto, 0.0);

        /* t=1,5: meio caminho do circOut -> zoom 0,5 + 0,2*0,866025 */
        ssc_estado(1.5f, &e);
        CHECK_NEAR(e.zoom, 0.5 + 0.2 * 0.866025);
        CHECK_NEAR(e.cam_y, -200.0 + 500.0 * 0.866025);

        /* t=563/24 (23,458 s): o flash branco nasce cheio, a cobertura sobe a
           1 e a cena da cutscene sai. */
        ssc_estado(563.0f / 24.0f, &e);
        CHECK_NEAR(e.fade_branco, 1.0);
        CHECK_NEAR(e.cover, 1.0);
        CHECK_EQ(e.cena, 0);
        CHECK_EQ(e.principal_visivel, 0);
        /* meio do flash (30/24 = 1,25 s): ainda 0,5 */
        ssc_estado(563.0f / 24.0f + 0.625f, &e);
        CHECK_NEAR(e.fade_branco, 0.5);

        /* t=650/24 (27,083): a cobertura comeca a descer (`sineOut`, 3 s), a
           camera salta para (1070,470) e as duas "get up" aparecem. */
        ssc_estado(650.0f / 24.0f, &e);
        CHECK_NEAR(e.cover, 1.0);
        CHECK_NEAR(e.cam_x, 1070.0);
        CHECK_NEAR(e.cam_y, 470.0);
        CHECK_EQ(e.gf_visivel, 1);
        CHECK_EQ(e.bf_visivel, 1);
        /* meio caminho da descida: 1 - sin(pi/4) */
        ssc_estado(650.0f / 24.0f + 1.5f, &e);
        CHECK_NEAR(e.cover, 1.0 - 0.707107);
        /* fim da descida: 0 */
        ssc_estado(650.0f / 24.0f + 3.0f, &e);
        CHECK_NEAR(e.cover, 0.0);
        CHECK_NEAR(e.zoom, 0.55);

        /* t=730/24 (30,417 s): ACABOU -- e' quando o `startCountdown()` do
           script roda. Um quadro antes, nao. */
        ssc_estado(730.0f / 24.0f - 0.01f, &e);
        CHECK_EQ(e.acabou, 0);
        CHECK_EQ(e.hud, 0);
        ssc_estado(730.0f / 24.0f, &e);
        CHECK_EQ(e.acabou, 1);
        CHECK_EQ(e.hud, 1);

        /* OS RELOGIOS DAS DUAS ANIMACOES. Antes de 650/24 nao ha' animacao
           (`resetAnim`); de 650/24 a 710/24 o `static` (um quadro); de 710/24 em
           diante o `getup` com o relogio DO ZERO -- e' o `doAnim()` do script. */
        ssc_estado(600.0f / 24.0f, &e);
        CHECK_EQ(e.anim_gf, 0);
        CHECK_EQ(e.anim_bf, 0);
        ssc_estado(700.0f / 24.0f, &e);
        CHECK_EQ(e.anim_gf, 0);
        CHECK_EQ(e.anim_bf, 0);
        ssc_estado(710.0f / 24.0f, &e);
        CHECK_EQ(e.anim_gf, 1);
        CHECK_EQ(e.anim_bf, 1);
        CHECK_NEAR(e.t_gf, 0.0);
        CHECK_NEAR(e.t_bf, 0.0);
        ssc_estado(710.0f / 24.0f + 1.0f, &e);
        CHECK_NEAR(e.t_gf, 1.0);
        CHECK_NEAR(e.t_bf, 1.0);
        /* O relogio do atlas principal e' o da cena inteira. */
        ssc_estado(4.0f, &e);
        CHECK_NEAR(e.t_principal, 4.0);

        /* A camera nunca sai de [0,5..0,7] e a cobertura nunca passa de 1: um
           alpha acima de 1 no GS inverte a cor do que esta' embaixo. */
        for (i = 0; i <= 800; i++) {
            ssc_estado((float)i * 0.04f, &e);
            CHECK(e.zoom >= 0.4999f && e.zoom <= 0.7001f);
            CHECK(e.fade_preto >= 0.0f && e.fade_preto <= 1.0f);
            CHECK(e.fade_branco >= 0.0f && e.fade_branco <= 1.0f);
            CHECK(e.cover >= -1e-4f && e.cover <= 1.0f + 1e-4f);
        }
    }

    free(d);
    if (falhas) { printf("\n  %d falha(s)\n", falhas); return 1; }
    printf("\n  cutscene do sserafim: pacote assado, geometria, curvas e os "
           "6 marcos da linha do tempo conferidos\n");
    (void)n;
    return 0;
}
