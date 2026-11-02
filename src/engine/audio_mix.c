#include "audio_mix.h"

static short sat16(int v)
{
    if (v > 32767) return 32767;
    if (v < -32768) return -32768;
    return (short)v;
}

int amix_somar(const short *entrada, short *saida, int amostras, int vozes)
{
    int i;
    if (!entrada || !saida || amostras <= 0) return 0;
    /* In-place e' o caso do main.c: le' 4 shorts e escreve 2 em indices
       menores. Para i = 0 a leitura acontece antes da escrita, e para i >= 1
       o maior indice escrito (2i+1) e' menor que o menor lido (4i). */
    for (i = 0; i < amostras; i++) {
        int il = entrada[i * 4 + 0];
        int ir = entrada[i * 4 + 1];
        int vl = vozes ? entrada[i * 4 + 2] : 0;
        int vr = vozes ? entrada[i * 4 + 3] : 0;
        saida[i * 2 + 0] = sat16(il + vl);
        saida[i * 2 + 1] = sat16(ir + vr);
    }
    return amostras;
}

int amix_bytes_saida(int amostras)
{
    if (amostras <= 0) return 0;
    return amostras * AMIX_CANAIS_SAIDA * (int)sizeof(short);
}

int amix_somar_estereo(short *dst, const short *src, int amostras)
{
    int i;
    if (!dst || !src || amostras <= 0) return 0;
    /* 2 shorts por amostra (L,R). Somar em `int` antes de saturar: a soma de
       dois shorts cabe em int com folga e a parede continua sendo 16 bits. */
    for (i = 0; i < amostras * AMIX_CANAIS_SAIDA; i++)
        dst[i] = sat16((int)dst[i] + (int)src[i]);
    return amostras;
}

int amix_somar_ganho(const short *entrada, short *saida, int amostras, float g0, float g1)
{
    int i;
    if (!entrada || !saida || amostras <= 0) return 0;
    for (i = 0; i < amostras; i++) {
        float f = (amostras == 1) ? 0.0f : ((float)i / (float)(amostras - 1));
        float g = g0 + (g1 - g0) * f;
        float il = (float)entrada[i * 4 + 0] * g;
        float ir = (float)entrada[i * 4 + 1] * g;
        /* O +-0,5 antes do truncamento e' o arredondamento para o inteiro mais
           proximo: sem ele, um sinal positivo perde sempre meio bit. */
        int vl = (int)(il < 0.0f ? il - 0.5f : il + 0.5f);
        int vr = (int)(ir < 0.0f ? ir - 0.5f : ir + 0.5f);
        saida[i * 2 + 0] = sat16(vl);
        saida[i * 2 + 1] = sat16(vr);
    }
    return amostras;
}

int amix_volume(short *pcm, int amostras, int vol_pct)
{
    int i, n;
    if (!pcm || amostras <= 0) return 0;
    if (vol_pct < 0) vol_pct = 0;
    if (vol_pct == 100) return amostras;
    n = amostras * AMIX_CANAIS_SAIDA;
    for (i = 0; i < n; i++) {
        int v = (int)pcm[i] * vol_pct;
        /* +-50 antes de dividir: arredondamento para o inteiro mais proximo,
           simetrico (metade para longe do zero). */
        if (v >= 0) v = (v + 50) / 100;
        else v = -((-v + 50) / 100);
        pcm[i] = sat16(v);
    }
    return amostras;
}
