#include "vram_layout.h"

/* O mapa do port, na ORDEM em que main.c aloca. Mudar a ordem muda os
   enderecos; o boot compara com o alocador real e diz no log se divergiu. */
static const vl_slot_t g_slots[VL_SLOTS] = {
    /* nome        w    h   psm      alinhamento */
    /* Framebuffers em PSMCT16 (A1B5G5R5): 640*(448/2) = 143.360 palavras
       cada, contra 286.720 em PSMCT32. O par devolve 286.720 palavras =
       1.146.880 bytes de VRAM. A resolucao e' a mesma; o que muda e' a
       profundidade de cor da tela (dithering ligado por quadro). */
    { "fb0",      640, 448, VL_PSM_16, VL_ALINHAMENTO_PAGINA },
    { "fb1",      640, 448, VL_PSM_16, VL_ALINHAMENTO_PAGINA },
    /* Texturas que o GAMEPLAY usa: BF, notas/receptores, fonte do HUD e a GF.
       Elas nunca saem, entao ficam antes do pool (que e' o resto da VRAM). */
    { "tex",      320, 256, VL_PSM_32, VL_ALINHAMENTO_BLOCO  },
    { "tex2",     320, 256, VL_PSM_32, VL_ALINHAMENTO_BLOCO  },
    /* fonttex era 384x256 (98.304 palavras) e o atlas da fonte e' 384x192
       (FONT_W x FONT_H em main.c). As 64 linhas a mais nunca eram enviadas nem
       lidas: eram 24.576 palavras -- 98.304 bytes -- paradas. */
    { "fonttex",  384, 192, VL_PSM_32, VL_ALINHAMENTO_BLOCO  },
    { "gftex16",  320,  64, VL_PSM_16, VL_ALINHAMENTO_BLOCO  }
};

const vl_slot_t *vl_slots(void) { return g_slots; }
unsigned int vl_slot_count(void) { return VL_SLOTS; }

unsigned int vl_size_words(unsigned int w, unsigned int h, unsigned int psm,
                           unsigned int alinhamento)
{
    unsigned int largura = w, palavras;

    /* graph_vram_size(): largura para multiplo de 64 (128 em 4/8 bits) e, para
       larguras ate' 16, nada -- e' paleta, tratada como paleta. */
    if (largura > 16u) largura = (largura + 63u) & ~63u;

    if (psm == VL_PSM_16) palavras = largura * (h >> 1);
    else                  palavras = largura * h;

    return (palavras + (alinhamento - 1u)) & ~(alinhamento - 1u);
}

unsigned int vl_bytes(unsigned int slot)
{
    const vl_slot_t *s;
    if (slot >= VL_SLOTS) return 0;
    s = &g_slots[slot];
    return vl_size_words(s->w, s->h, s->psm, s->alinhamento) * 4u;
}

unsigned int vl_address(unsigned int slot)
{
    unsigned int i, cursor = 0;
    if (slot >= VL_SLOTS) return 0;
    for (i = 0; i < slot; i++) {
        cursor += vl_size_words(g_slots[i].w, g_slots[i].h, g_slots[i].psm,
                                g_slots[i].alinhamento);
        /* graph_vram_allocate() alinha o ponteiro DEPOIS de somar. */
        cursor = (cursor + (g_slots[i].alinhamento - 1u)) & ~(g_slots[i].alinhamento - 1u);
    }
    return cursor;
}

unsigned int vl_pool_base(void)
{
    return vl_used_words();
}

unsigned int vl_pool_size_bytes(void)
{
    unsigned int usadas = vl_used_words();
    return usadas >= VL_TOTAL_WORDS ? 0u : (VL_TOTAL_WORDS - usadas) * 4u;
}

unsigned int vl_used_words(void)
{
    return vl_address(VL_SLOTS - 1) + vl_size_words(g_slots[VL_SLOTS - 1].w,
                                                    g_slots[VL_SLOTS - 1].h,
                                                    g_slots[VL_SLOTS - 1].psm,
                                                    g_slots[VL_SLOTS - 1].alinhamento);
}

unsigned int vl_free_words(void)
{
    unsigned int used = vl_used_words();
    return used >= VL_TOTAL_WORDS ? 0u : VL_TOTAL_WORDS - used;
}

int vl_fits(void) { return vl_used_words() <= VL_TOTAL_WORDS; }
