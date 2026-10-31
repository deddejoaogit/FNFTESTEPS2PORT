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
