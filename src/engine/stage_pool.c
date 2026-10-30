#include "stage_pool.h"
#include "stage_vram_data.h"
#include "stage_layout.h"

#include <string.h>

/* ATENCAO A UNIDADE, e o vc_* tem uma so': o cache trabalha o endereco em
   BYTES (inserir() faz endereco + bytes). Quem desenha precisa do endereco em
   PALAVRAS de 32 bits, que e' o numero que o graph_vram_allocate devolve e o
   que vai para o TBP0 do GS. A conversao e' feita aqui, num lugar so', em vez
   de ficar espalhada por quem chama. */
void sp_init(sp_t *p, unsigned int base_palavras, unsigned int tamanho_bytes)
{
    if (!p) return;
    vc_init(&p->cache, base_palavras * 4u, tamanho_bytes);
    p->base = base_palavras;
    p->tamanho = tamanho_bytes & ~(VC_ALINHAMENTO - 1u);
    p->pedidos = 0;
    p->falhas = 0;
}

unsigned int sp_prop_bytes(int prop)
{
    unsigned int b;
    if (prop < 0 || prop >= (int)BS_PROP_VRAM_COUNT) return 0;
    b = bs_prop_vram_bytes[prop];
    return (b + (VC_ALINHAMENTO - 1u)) & ~(VC_ALINHAMENTO - 1u);
}

int sp_stage_demand(const char *stage, unsigned int *bytes)
{
    int si = sl_stage_find(stage), i;
    unsigned int total = 0;
    if (si < 0) return -1;
    for (i = 0; i < bs_stage_props[si].count; i++)
        total += sp_prop_bytes(bs_stage_props[si].first + i);
    if (bytes) *bytes = total;
    return 0;
}

int sp_chave(sp_t *p, unsigned int chave, unsigned int bytes, unsigned int *endereco)
{
    int r;
    if (!p || !chave || !bytes) { if (p) p->falhas++; return -1; }
    {
        unsigned int bytes_off = 0;
        r = vc_pedir(&p->cache, chave, bytes, &bytes_off);
        if (r >= 0 && endereco) *endereco = bytes_off / 4u;   /* bytes -> palavras */
    }
    if (r < 0) p->falhas++;
    return r;
}

int sp_prop(sp_t *p, int prop, unsigned int *endereco)
{
    unsigned int bytes = sp_prop_bytes(prop);
    if (!p) return -1;
    p->pedidos++;
    if (!bytes) { p->falhas++; return -1; }
    return sp_chave(p, SP_CHAVE_PROP(prop), bytes, endereco);
}

void sp_fixar(sp_t *p, unsigned int chave)
{
    if (p) vc_fixar(&p->cache, chave);
}

void sp_soltar(sp_t *p, unsigned int chave)
{
    if (p) vc_soltar(&p->cache, chave);
}

int sp_esta_fixo(const sp_t *p, unsigned int chave)
{
    return p ? vc_esta_fixo(&p->cache, chave) : 0;
}

void sp_limpar(sp_t *p)
{
    if (p) vc_limpar(&p->cache);
}

unsigned int sp_livre(const sp_t *p)
{
    return p ? vc_livre(&p->cache) : 0u;   /* em bytes */
}
