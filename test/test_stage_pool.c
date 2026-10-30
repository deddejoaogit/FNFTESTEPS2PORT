/* Pool de texturas do palco: a demanda REAL dos 21 palcos contra o orcamento.
 *
 * O que este teste decide, e por que ele importa mais que um teste de unidade
 * comum: a pergunta "cabe o palco?" nunca tinha sido feita com os numeros do
 * plano. Quando foi feita na mao, deu que 15 dos 21 nao cabiam -- e a
 * conclusao so' valia porque ninguem tinha errado a conta. Aqui a conta roda
 * sobre os 157 props reais (src/engine/stage_vram_data.h, gerado do mesmo
 * assets-data/stage-props.json que gera o stage_dados.h) e falha se alguem
 * mudar o plano sem olhar o orcamento.
 */
#include <stdio.h>
#include "stage_pool.h"
#include "stage_vram_data.h"

static int falhas;
#define CHECK(c) do { if (!(c)) { printf("  FALHA: %s (linha %d)\n", #c, __LINE__); falhas++; } } while (0)
#define CHECK_EQ(a, b) do { long _a = (long)(a), _b = (long)(b); \
    if (_a != _b) { printf("  FALHA: %s == %s: %ld != %ld (linha %d)\n", #a, #b, _a, _b, __LINE__); falhas++; } } while (0)

int main(void)
{
    int i, k;
    sp_t pool;

    printf("== pool: tamanhos dos props ==\n");
    CHECK_EQ(BS_PROP_VRAM_COUNT, (int)(sizeof(bs_props) / sizeof(bs_props[0])));
    CHECK_EQ(BS_STAGE_VRAM_COUNT, BS_STAGE_PROPS_COUNT);
    for (i = 0; i < BS_PROP_VRAM_COUNT; i++) {
        /* Alinhado a 256 e igual a sw*sh*2 arredondado: o pool e o pacote tem
           de concordar sobre o tamanho do quadro, senao o upload estoura. */
        CHECK(bs_prop_vram_bytes[i] > 0);
        CHECK_EQ(bs_prop_vram_bytes[i] % 256, 0);
        CHECK_EQ(bs_prop_vram_bytes[i], sp_prop_bytes(i));
    }
    CHECK_EQ(sp_prop_bytes(-1), 0);
    CHECK_EQ(sp_prop_bytes(BS_PROP_VRAM_COUNT), 0);

    printf("== pool: a soma por palco bate com o array de props ==\n");
    for (i = 0; i < BS_STAGE_PROPS_COUNT; i++) {
        unsigned int soma = 0, d = 0;
        for (k = 0; k < bs_stage_props[i].count; k++)
            soma += bs_prop_vram_bytes[bs_stage_props[i].first + k];
        CHECK_EQ(soma, bs_stage_vram_bytes[i]);
        CHECK_EQ(sp_stage_demand(bs_stage_props[i].id, &d), 0);
        CHECK_EQ(d, soma);
    }
    CHECK_EQ(sp_stage_demand("naoExiste", NULL), -1);

    printf("== pool: os 21 palcos cabem no orcamento do pool ==\n");
    /* O pior caso e' o que importa: pool vazio, todos os props do palco dentro.
       SP_POOL_BYTES = 4.194.304 - 1.146.880 (dois framebuffers) - 991.232
       (fonttex + gftex16 + tex + tex2). */
    CHECK_EQ(SP_POOL_BYTES, 2056192u);
    CHECK(BS_STAGE_VRAM_MAX <= SP_POOL_BYTES);
    {
        unsigned int pior = 0;
        const char *pior_nome = NULL;
        for (i = 0; i < BS_STAGE_PROPS_COUNT; i++) {
            unsigned int livre;
            const bs_stage_props_t *st = &bs_stage_props[i];
            sp_init(&pool, 534528u, SP_POOL_BYTES);
            for (k = 0; k < st->count; k++) {
                unsigned int addr = 0;
                int r = sp_prop(&pool, st->first + k, &addr);
                if (r < 0) {
                    printf("  FALHA: %s prop %d (%u bytes) nao coube\n",
                           st->id, st->first + k, sp_prop_bytes(st->first + k));
                    falhas++;
                }
                /* 'addr' vem em PALAVRAS de 32 bits (formato do graph_vram_allocate)
                   e o tamanho do prop em BYTES: comparar os dois sem converter
                   daria um teste que passa com o pool pela metade. */
                CHECK(addr >= 534528u);
                CHECK((unsigned long long)addr * 4u + sp_prop_bytes(st->first + k)
                      <= (unsigned long long)534528u * 4u + SP_POOL_BYTES);
            }
            CHECK_EQ(pool.falhas, 0u);
            livre = sp_livre(&pool);
            CHECK_EQ(livre, SP_POOL_BYTES - bs_stage_vram_bytes[i]);   /* bytes */
            if (bs_stage_vram_bytes[i] > pior) { pior = bs_stage_vram_bytes[i]; pior_nome = st->id; }
        }
        printf("  pior palco: %s com %u bytes de %u (sobra %u)\n",
               pior_nome, pior, SP_POOL_BYTES, SP_POOL_BYTES - pior);
        CHECK(pior_nome != NULL);
        CHECK_EQ(pior, BS_STAGE_VRAM_MAX);
    }

    printf("== pool: residente nao custa nada, e o despejo funciona ==\n");
    sp_init(&pool, 534528u, SP_POOL_BYTES);
    {
        unsigned int a1 = 0, a2 = 0;
        int r1 = sp_prop(&pool, 0, &a1);
        int r2 = sp_prop(&pool, 0, &a2);
        CHECK_EQ(r1, 0);            /* alocou agora */
        CHECK_EQ(r2, 1);            /* ja' estava: nada a enviar */
        CHECK_EQ(a1, a2);
        CHECK_EQ(pool.cache.acertos, 1u);
        CHECK_EQ(pool.cache.faltas, 1u);
    }

    printf("== pool: palco que nao cabe falha limpo, sem endereco invalido ==\n");
    {
        unsigned int addr = 0xDEADBEEFu;
        /* Pool de 1 MB: o tankmanBattlefield (1.700.352 B) nao cabe inteiro. */
        sp_init(&pool, 534528u, 1024u * 1024u);
        for (k = 0; k < bs_stage_props[BS_STAGE_PROPS_COUNT - 1].count; k++) {
            const bs_stage_props_t *st = &bs_stage_props[BS_STAGE_PROPS_COUNT - 1];
            if (sp_prop(&pool, st->first + k, &addr) < 0) { addr = 0; break; }
        }
        CHECK_EQ(addr, 0u);
        CHECK(pool.falhas >= 1u);
        /* E um prop maior que o pool inteiro reprova na hora. */
        sp_init(&pool, 0, 1024u);
        CHECK_EQ(sp_prop(&pool, 0, NULL), -1);
        CHECK_EQ(sp_chave(&pool, 0, 100, NULL), -1);   /* chave 0 e' invalida */
    }

    printf("== pool: fixar e soltar (a gftex/stagetex na musica) ==\n");
    {
        /* O ciclo do jogo: no menu, gftex e stagetex nao podem ser despejadas;
           durante a musica elas sao dispensaveis e o palco precisa do espaco.
           Aqui: pool pequeno, as duas fixas, um prop grande que so' cabe se
           elas sairem. Fixas -> falha. Soltas -> cabe. */
        unsigned int a;
        sp_init(&pool, 0, 512u * 1024u);
        CHECK_EQ(sp_chave(&pool, SP_CHAVE_TELA_BASE + 0u, 256u * 256u * 4u, &a), 0);
        sp_fixar(&pool, SP_CHAVE_TELA_BASE + 0u);
        CHECK_EQ(sp_esta_fixo(&pool, SP_CHAVE_TELA_BASE + 0u), 1);
        CHECK_EQ(sp_esta_fixo(&pool, SP_CHAVE_TELA_BASE + 1u), 0);   /* nao existe */
        /* 262.144 B fixos: sobram 262.144, e o prop 0 pede 675.584. */
        CHECK_EQ(sp_prop(&pool, 0, &a), -1);
        sp_soltar(&pool, SP_CHAVE_TELA_BASE + 0u);
        CHECK_EQ(sp_esta_fixo(&pool, SP_CHAVE_TELA_BASE + 0u), 0);
        sp_init(&pool, 0, 1024u * 1024u);
        CHECK_EQ(sp_prop(&pool, 0, &a), 0);
        CHECK_EQ(sp_livre(&pool), 1024u * 1024u - sp_prop_bytes(0));
    }

    printf("== pool: trocar de palco reaproveita a VRAM ==\n");
    {
        /* Duas musicas seguidas: limpar e' o que o jogo faz ao trocar de palco.
           Depois de limpar, o total livre volta ao orcamento inteiro. */
        sp_init(&pool, 534528u, SP_POOL_BYTES);
        for (i = 0; i < 2; i++) {
            int si = (i == 0) ? 0 : BS_STAGE_PROPS_COUNT - 1;
            unsigned int addr;
            for (k = 0; k < bs_stage_props[si].count; k++)
                CHECK(sp_prop(&pool, bs_stage_props[si].first + k, &addr) >= 0);
            sp_limpar(&pool);
            CHECK_EQ(sp_livre(&pool), SP_POOL_BYTES);
        }
    }

    if (falhas) { printf("\n  %d falha(s)\n", falhas); return 1; }
    printf("\n  pool: 21 palcos, %d props, pior %u bytes de %u -- cabe\n",
           BS_PROP_VRAM_COUNT, BS_STAGE_VRAM_MAX, SP_POOL_BYTES);
    return 0;
}
