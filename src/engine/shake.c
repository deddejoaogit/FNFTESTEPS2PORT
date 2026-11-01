#include "shake.h"

/* Numeros do LCG de Numerical Recipes: periodo 2^32, sem multiplicacao que
   estoure num unsigned de 32 bits. A semente e' do chamador para o resultado
   ser repetivel. */
static float sorteia(shk_t *s)
{
    s->semente = s->semente * 1664525u + 1013904223u;
    /* 16 bits centrais -> [-1, 1). O /32767.5 poe o zero no meio da faixa. */
    return ((float)((s->semente >> 8) & 0xFFFFu) / 32767.5f) - 1.0f;
}

void shk_reset(shk_t *s, unsigned int semente)
{
    if (!s) return;
    s->forca = 0.0f;
    s->restante = 0.0f;
    s->x = s->y = 0.0f;
    s->semente = semente;
}

void shk_inicia(shk_t *s, float forca, float dur_s)
{
    if (!s) return;
    if (forca <= 0.0f || dur_s <= 0.0f) return;
    s->forca = forca;
    s->restante = dur_s;
}

void shk_passo(shk_t *s, float dt, float cam_w, float cam_h)
{
    if (!s) return;
    if (s->restante <= 0.0f) {
        s->x = s->y = 0.0f;
        return;
    }
    s->restante -= dt;
    if (s->restante <= 0.0f) {
        /* Acabou: o offset volta a zero NESTE quadro, nao no seguinte. */
        s->x = s->y = 0.0f;
        s->forca = 0.0f;
        return;
    }
    s->x = sorteia(s) * s->forca * cam_w;
    s->y = sorteia(s) * s->forca * cam_h;
}
