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

unsigned int sp_criado_bytes(int criado)
{
    unsigned int b;
    if (criado < 0 || criado >= BS_CRIADO_COUNT) return 0;
    b = bs_criado_vram_bytes[criado];
    return (b + (VC_ALINHAMENTO - 1u)) & ~(VC_ALINHAMENTO - 1u);
}

int sp_criado(sp_t *p, int criado, unsigned int *endereco)
{
    unsigned int bytes = sp_criado_bytes(criado);
    if (!p) return -1;
    p->pedidos++;
    if (!bytes) { p->falhas++; return -1; }
    return sp_chave(p, SP_CHAVE_CRIADO(criado), bytes, endereco);
}

unsigned int sp_chao_bytes(int chao)
{
    unsigned int b;
    if (chao < 0 || chao >= BS_CHAO_COUNT) return 0;
    b = bs_chao_vram_bytes[chao];
    return (b + (VC_ALINHAMENTO - 1u)) & ~(VC_ALINHAMENTO - 1u);
}

int sp_chao(sp_t *p, int chao, unsigned int *endereco)
{
    unsigned int bytes = sp_chao_bytes(chao);
    if (!p) return -1;
    p->pedidos++;
    if (!bytes) { p->falhas++; return -1; }
    return sp_chave(p, SP_CHAVE_CHAO(chao), bytes, endereco);
}

int sp_stage_demand(const char *stage, unsigned int *bytes)
{
    int si = sl_stage_find(stage), i;
    unsigned int total = 0;
    if (si < 0) return -1;
    for (i = 0; i < bs_stage_props[si].count; i++)
        total += sp_prop_bytes(bs_stage_props[si].first + i);
    /* Os sprites do script do palco moram no mesmo pool: quem pergunta "quanto
       este palco pede" precisa da soma dos dois, senao o numero mente. */
    for (i = 0; i < bs_stage_criado_count[si]; i++) {
        int k = bs_stage_criado_first[si] + i, j;
        /* Duas poeiras que apontam para a MESMA imagem (dust1/dust3) pagam uma
           vez so': o `Paths.image` do original devolve o mesmo FlxGraphic. */
        int repetido = 0;
        for (j = 0; j < i; j++)
            if (!strcmp(bs_criados[k].tex, bs_criados[bs_stage_criado_first[si] + j].tex))
                repetido = 1;
        if (!repetido) total += sp_criado_bytes(k);
    }
    for (i = 0; i < bs_stage_chao_count[si]; i++)
        total += sp_chao_bytes(bs_stage_chao_first[si] + i);
    if (bytes) *bytes = total;
    return 0;
}

/* A MENOR FOLGA DO POOL desde o ultimo `sp_limpar`. E' o numero que responde
   "o palco coube?" com MEDIDA em vez de soma de tabela: `sp_limpar` zera a
   marca, todo pedido desconta o que sobrou e quem chama le no fim da musica
   (o `main.c` regista `pool_min`). Zero significa que o pool encheu.
   Antes disto, "cabe" vinha de somar os bytes dos props no Python, o que
   ignora quando cada um esta' residente de fato. */
static unsigned int g_pool_min_folga;
static int g_pool_min_visto;

void sp_min_folga_reset(sp_t *p)
{
    g_pool_min_visto = 0;
    g_pool_min_folga = p ? sp_livre(p) : 0u;
}

unsigned int sp_min_folga(const sp_t *p)
{
    return g_pool_min_visto ? g_pool_min_folga : (p ? sp_livre(p) : 0u);
}

int sp_min_folga_encheu(void)
{
    return g_pool_min_visto && g_pool_min_folga == 0u;
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
    {
        unsigned int l = sp_livre(p);
        if (!g_pool_min_visto || l < g_pool_min_folga) g_pool_min_folga = l;
        g_pool_min_visto = 1;
    }
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

int sp_liberar(sp_t *p, unsigned int chave)
{
    return p ? vc_liberar(&p->cache, chave) : 0;
}

void sp_limpar(sp_t *p)
{
    if (p) vc_limpar(&p->cache);
    sp_min_folga_reset(p);
}

unsigned int sp_livre(const sp_t *p)
{
    return p ? vc_livre(&p->cache) : 0u;   /* em bytes */
}
