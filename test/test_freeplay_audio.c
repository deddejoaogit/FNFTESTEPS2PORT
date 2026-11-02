/* O AUDIO DA FREEPLAY: a janela do preview, o ganho dos fades, os jingles do
 * rank e a agenda da cinematica.
 *
 * O QUE PROVA (as linhas da fonte estao em src/engine/freeplay_audio.h)
 *  - a JANELA: 0 -> 20 % das amostras do arquivo (`Constants. DEFAULT_PREVIEW_*`
 *    em `util/Constants.hx` L306/L311), em bytes, alinhada a uma amostra de
 *    4 canais, com o grampo nas fracoes invalidas;
 *  - o GANHO: `fadeIn(2)` de 0 a 0,7, `fadeOut(2)` nos ultimos 2 s, e o caso
 *    da janela curta (fadeOut que nasce vencido);
 *  - os JINGLES: um som por rank em cada um dos dois instantes, sem queda de
 *    `switch` para o case seguinte;
 *  - a AGENDA: 600 ms, 1100 ms e 2200 ms, disparando uma vez so' e sem pular
 *    quando um quadro longo atravessa dois instantes.
 *
 * O que NAO prova: que sai som na TV. Isso e' o console.
 */
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "freeplay_audio.h"

static int falhas;
#define CHECK(c) do { if (!(c)) { printf("  FALHA: %s (linha %d)\n", #c, __LINE__); falhas++; } } while (0)
#define CHECK_EQ(a, b) do { long _a = (long)(a), _b = (long)(b); \
    if (_a != _b) { printf("  FALHA: %s == %s: %ld != %ld (linha %d)\n", #a, #b, _a, _b, __LINE__); falhas++; } } while (0)
#define CHECK_F(a, b, tol) do { double _a = (double)(a), _b = (double)(b); \
    if (fabs(_a - _b) > (tol)) { printf("  FALHA: %s == %s: %.6f != %.6f (linha %d)\n", #a, #b, _a, _b, __LINE__); falhas++; } } while (0)

/* 10 s de stems de 4 canais: 480 000 amostras x 8 B = 3 840 000 B. E' o
 * tamanho do `stems.pcm` sintetico do harness (10 s a 48 kHz). */
#define ARQ_BYTES 3840000L
#define ARQ_AMOSTRAS 480000L

int main(void)
{
    long ini, fim;
    int n;

    printf("== preview: a janela do `loadPartial` (Constants.hx L306/L311)\n");
    CHECK_EQ(FP_AUD_VOLUME * 1000.0f, 700);
    CHECK_EQ(FP_AUD_FADE_MS, 2000);
    CHECK_EQ(FP_AUD_ATRASO_MS, 250);
    CHECK_EQ(FP_AUD_STEMS_BYTES, 8);
    CHECK_EQ(fp_aud_janela(ARQ_BYTES, FP_AUD_PREVIEW_INI, FP_AUD_PREVIEW_FIM, &ini, &fim), 0);
    CHECK_EQ(ini, 0);
    /* 20 % de 480 000 amostras = 96 000 amostras = 768 000 B */
    CHECK_EQ(fim, 768000);
    CHECK_EQ(fp_aud_janela_amostras(ini, fim), 96000);

    /* Uma fracao de metadata REAL (o port nao tem nenhuma hoje, mas o caminho
       tem de funcionar se um dia tiver): 10 % -> 50 % de 10 s. */
    CHECK_EQ(fp_aud_janela(ARQ_BYTES, 0.1f, 0.5f, &ini, &fim), 0);
    CHECK_EQ(ini, 384000L);   /* 48 000 amostras x 8 B */
    CHECK_EQ(fim, 240000L * 8L);

    /* Fracoes impossiveis caem no padrao, um por um, como nos dois getters
       `get_previewStartTime`/`get_previewEndTime` (L3405/L3420). */
    CHECK_EQ(fp_aud_janela(ARQ_BYTES, 0.5f, 0.2f, &ini, &fim), 0);
    CHECK_EQ(ini, 0);
    CHECK_EQ(fim, 768000);
    CHECK_EQ(fp_aud_janela(ARQ_BYTES, 1.5f, 0.2f, &ini, &fim), 0);
    CHECK_EQ(ini, 0);
    CHECK_EQ(fp_aud_janela(ARQ_BYTES, 0.0f, 2.0f, &ini, &fim), 0);
    CHECK_EQ(fim, 768000);

    /* Arquivo menor que uma amostra de 4 canais: sem janela. */
    CHECK_EQ(fp_aud_janela(7, 0.0f, 0.2f, &ini, &fim), -1);
    /* Arquivo minusculo (1 amostra): janela de 1 amostra, nunca vazia. */
    CHECK_EQ(fp_aud_janela(8, 0.0f, 0.2f, &ini, &fim), 0);
    CHECK_EQ(ini, 0);
    CHECK_EQ(fim, 8);
    CHECK_EQ(fp_aud_janela_amostras(0, 8), 1);
    CHECK_EQ(fp_aud_janela_amostras(8, 8), 0);

    printf("== preview: o ganho do fadeIn(2) / fadeOut(2) a 0,7\n");
    /* Janela de 10 s (480 000 amostras): 2 s de fade em cada ponta. */
    n = (int)ARQ_AMOSTRAS;
    CHECK_F(fp_aud_ganho(0, n, FP_AUD_VOLUME), 0.0, 1e-6);
    CHECK_F(fp_aud_ganho(48000, n, FP_AUD_VOLUME), 0.35, 1e-6);   /* 1 s       */
    CHECK_F(fp_aud_ganho(96000, n, FP_AUD_VOLUME), 0.70, 1e-6);   /* 2 s       */
    CHECK_F(fp_aud_ganho(240000, n, FP_AUD_VOLUME), 0.70, 1e-6);  /* 5 s       */
    CHECK_F(fp_aud_ganho(384000, n, FP_AUD_VOLUME), 0.70, 1e-6);  /* 8 s       */
    CHECK_F(fp_aud_ganho(432000, n, FP_AUD_VOLUME), 0.35, 1e-6);  /* 9 s       */
    CHECK_F(fp_aud_ganho(n, n, FP_AUD_VOLUME), 0.0, 1e-6);
    /* Fora da janela nao existe posicao negativa nem maior que o fim. */
    CHECK_F(fp_aud_ganho(-100, n, FP_AUD_VOLUME), 0.0, 1e-6);
    CHECK_F(fp_aud_ganho(n + 500, n, FP_AUD_VOLUME), 0.0, 1e-6);

    /* Janela CURTA: 2 s. `fadeStart` fica 0, o fadeOut nasce vencido e captura
       volume 0 -- o trecho inteiro sai em SILENCIO. E' o que a fonte faz; ver
       o cabecalho do modulo. */
    CHECK_F(fp_aud_ganho(0, 96000, FP_AUD_VOLUME), 0.0, 1e-6);
    CHECK_F(fp_aud_ganho(24000, 96000, FP_AUD_VOLUME), 0.0, 1e-6);
    CHECK_F(fp_aud_ganho(48000, 96000, FP_AUD_VOLUME), 0.0, 1e-6);
    CHECK_F(fp_aud_ganho(96000, 96000, FP_AUD_VOLUME), 0.0, 1e-6);
    /* 1 s e' ainda mais curto: idem. */
    CHECK_F(fp_aud_ganho(0, 48000, FP_AUD_VOLUME), 0.0, 1e-6);
    /* 2 s + 1 amostra ja' entra no caminho normal (fadeOut comeca em +1). */
    CHECK_F(fp_aud_ganho(96000, 96001, FP_AUD_VOLUME) > 0.0, 1, 1);

    printf("== preview: a interpolacao dentro do bloco do anel\n");
    CHECK_F(fp_aud_ganho_interp(0.0f, 0.7f, 0, 11), 0.0, 1e-6);
    CHECK_F(fp_aud_ganho_interp(0.0f, 0.7f, 10, 11), 0.7, 1e-6);
    CHECK_F(fp_aud_ganho_interp(0.0f, 0.7f, 5, 11), 0.35, 1e-6);
    CHECK_F(fp_aud_ganho_interp(0.3f, 0.3f, 7, 11), 0.3, 1e-6);
    CHECK_F(fp_aud_ganho_interp(0.2f, 0.9f, 0, 1), 0.2, 1e-6);   /* 1 amostra */

    printf("== jingles: um som por rank, sem queda de switch (L1190-1246)\n");
    /* `rankDisplayNew` (+0,1 s do display = 600 ms). */
    CHECK(!strcmp(fp_aud_jingle_novo(0), "rankinbad"));
    CHECK(!strcmp(fp_aud_jingle_novo(1), "rankinnormal"));
    CHECK(!strcmp(fp_aud_jingle_novo(2), "rankinnormal"));
    CHECK(!strcmp(fp_aud_jingle_novo(3), "rankinnormal"));
    CHECK(!strcmp(fp_aud_jingle_novo(4), "rankinperfect"));
    CHECK(!strcmp(fp_aud_jingle_novo(5), "rankinperfect"));
    CHECK(!strcmp(fp_aud_jingle_novo(-1), "rankinnormal"));   /* default */
    /* `rankAnimSlam` (1100 ms). */
    CHECK(!strcmp(fp_aud_jingle_slam(0), "loss"));
    CHECK(!strcmp(fp_aud_jingle_slam(1), "good"));
    CHECK(!strcmp(fp_aud_jingle_slam(2), "great"));
    CHECK(!strcmp(fp_aud_jingle_slam(3), "excellent"));
    CHECK(!strcmp(fp_aud_jingle_slam(4), "perfect"));
    CHECK(!strcmp(fp_aud_jingle_slam(5), "perfect"));
    CHECK(!strcmp(fp_aud_jingle_slam(-1), "loss"));           /* default */

    printf("== a agenda da cinematica: 600 / 1100 / 2200 ms\n");
    CHECK_EQ(fp_aud_eventos(0, 100), 0);
    CHECK_EQ(fp_aud_eventos(599, 600), FP_AUD_EV_JINGLE_NOVO);
    CHECK_EQ(fp_aud_eventos(600, 601), 0);                    /* uma vez so' */
    CHECK_EQ(fp_aud_eventos(700, 1099), 0);
    CHECK_EQ(fp_aud_eventos(1099, 1100), FP_AUD_EV_JINGLE_SLAM);
    CHECK_EQ(fp_aud_eventos(1100, 2199), 0);
    CHECK_EQ(fp_aud_eventos(2199, 2200), FP_AUD_EV_PREVIEW);
    CHECK_EQ(fp_aud_eventos(2200, 3000), 0);
    /* Um quadro longo atravessa os dois jingles: os dois disparam (o
       `FlxTimer` do original tambem os dispara no mesmo update). */
    CHECK_EQ(fp_aud_eventos(0, 1200), FP_AUD_EV_JINGLE_NOVO | FP_AUD_EV_JINGLE_SLAM);
    CHECK_EQ(fp_aud_eventos(0, 2200),
             FP_AUD_EV_JINGLE_NOVO | FP_AUD_EV_JINGLE_SLAM | FP_AUD_EV_PREVIEW);
    /* Tempo parado ou para tras nao dispara nada. */
    CHECK_EQ(fp_aud_eventos(700, 700), 0);
    CHECK_EQ(fp_aud_eventos(700, 600), 0);

    if (falhas) {
        printf("  %d FALHA(S)\n", falhas);
        return 1;
    }
    printf("  ok: janela, ganho, jingles e agenda conferem\n");
    printf("AUDIO DA FREEPLAY OK\n");
    return 0;
}
