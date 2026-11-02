#include "freeplay_audio.h"

int fp_aud_janela(long total_bytes, float ini, float fim, long *ini_b, long *fim_b)
{
    long a, b, amostras;

    if (!ini_b || !fim_b) return -1;
    *ini_b = *fim_b = 0;
    if (total_bytes < FP_AUD_STEMS_BYTES) return -1;

    /* `get_previewStartTime`/`get_previewEndTime` (L3405/L3420) ja' trocam o
       valor pelo padrao quando ele nao faz sentido; aqui a mesma regra, para
       que um metadata futuro com fracao invalida nao crie janela vazia. */
    if (ini < 0.0f || ini > 1.0f) ini = FP_AUD_PREVIEW_INI;
    if (fim <= ini || fim > 1.0f) fim = FP_AUD_PREVIEW_FIM;
    /* Os dois getters do original trocam cada valor pelo padrao
       INDEPENDENTEMENTE (L3414-3418 e L3430-3434), entao um metadata com
       inicio 0,5 e fim 0,2 acaba em 0 -> 0,2. Aqui, a mesma coisa. */
    if (fim <= ini) { ini = FP_AUD_PREVIEW_INI; fim = FP_AUD_PREVIEW_FIM; }
    if (fim <= ini) return -1;

    amostras = total_bytes / FP_AUD_STEMS_BYTES;   /* amostras no arquivo */
    a = (long)((float)amostras * ini);
    b = (long)((float)amostras * fim);
    if (a < 0) a = 0;
    if (b > amostras) b = amostras;
    /* Pelo menos uma amostra: o `playPartial` do original tambem nao aceita
       intervalo vazio (o `loadPartial` recebe inicio e fim e corta). */
    if (b <= a) {
        if (a >= amostras) a = amostras - 1;
        b = a + 1;
    }
    *ini_b = a * FP_AUD_STEMS_BYTES;
    *fim_b = b * FP_AUD_STEMS_BYTES;
    return 0;
}

int fp_aud_janela_amostras(long ini_b, long fim_b)
{
    long n;
    if (fim_b <= ini_b) return 0;
    n = (fim_b - ini_b) / FP_AUD_STEMS_BYTES;
    return n > 0x7fffffffL ? 0x7fffffff : (int)n;
}

float fp_aud_ganho(int pos, int total, float vol)
{
    const float por_ms = (float)FP_AUD_RATE / 1000.0f;   /* 48 amostras por ms */
    const float fade = (float)FP_AUD_FADE_MS * por_ms;   /* 96000 amostras     */
    float t, g_in, g_out, g;

    if (total <= 0) return 0.0f;
    if (pos < 0) pos = 0;
    if (pos > total) pos = total;
    t = (float)pos;

    g_in = t / fade;
    if (g_in > 1.0f) g_in = 1.0f;

    /* `fadeStart = length/1000 - 2` (L3026), em amostras. Com 2 s ou menos o
       timer nasce vencido, o fadeOut (criado depois) vence o fadeIn e captura
       o volume do instante 0 -- que e' 0. Ver o cabecalho. */
    if ((float)total <= fade) {
        return 0.0f;
    } else {
        g_out = ((float)total - t) / fade;
        if (g_out > 1.0f) g_out = 1.0f;
    }
    if (g_out < 0.0f) g_out = 0.0f;

    g = g_in < g_out ? g_in : g_out;
    if (g < 0.0f) g = 0.0f;
    if (g > 1.0f) g = 1.0f;
    return vol * g;
}

/* `switch (fromResultsParams?.newRank)` -- sem queda para o case seguinte, que
 * e' como o `switch` do Haxe funciona (HaxeManual, `05-expression.md`
 * L797-830). `null` cai no `default`. */
const char *fp_aud_jingle_novo(int rank)
{
    switch (rank) {
    case 0: return "rankinbad";       /* SHIT                       */
    case 4:                            /* PERFECT                    */
    case 5: return "rankinperfect";   /* PERFECT_GOLD -> o dourado  */
    default: return "rankinnormal";
    }
}

const char *fp_aud_jingle_slam(int rank)
{
    switch (rank) {
    case 0: return "loss";            /* SHIT */
    case 1: return "good";            /* GOOD */
    case 2: return "great";           /* GREAT */
    case 3: return "excellent";       /* EXCELLENT */
    case 4:                           /* PERFECT */
    case 5: return "perfect";         /* PERFECT_GOLD -> o dourado */
    default: return "loss";
    }
}

int fp_aud_eventos(int antes, int agora)
{
    int ev = 0;
    if (agora <= antes) return 0;
    if (antes < FP_RK_T_FAKE_OFF && agora >= FP_RK_T_FAKE_OFF) ev |= FP_AUD_EV_JINGLE_NOVO;
    if (antes < FP_RK_T_SLAM && agora >= FP_RK_T_SLAM) ev |= FP_AUD_EV_JINGLE_SLAM;
    if (antes < FP_RK_FIM && agora >= FP_RK_FIM) ev |= FP_AUD_EV_PREVIEW;
    return ev;
}
