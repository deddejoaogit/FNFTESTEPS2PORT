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

#include <string.h>
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
        unsigned int soma = 0, d = 0, cri = 0;
        for (k = 0; k < bs_stage_props[i].count; k++)
            soma += bs_prop_vram_bytes[bs_stage_props[i].first + k];
        /* Os sprites do script do palco (`bs_criados[]`) moram no MESMO pool:
           a demanda que o runtime pergunta tem de incluir os dois, senao o
           numero mente e o palco "cabe" sem caber. */
        for (k = 0; k < bs_stage_criado_count[i]; k++) {
            int ck = bs_stage_criado_first[i] + k, j, repetido = 0;
            /* Duas entradas com a MESMA textura (as poeiras dust1/dust3 do
               sserafim) pagam um lugar so': o `Paths.image` do original
               devolve o mesmo FlxGraphic para a mesma chave, e o runtime
               compartilha o texbuffer. Contar as duas faria o palco "pedir"
               VRAM que ele nao pede. */
            for (j = 0; j < k; j++)
                if (!strcmp(bs_criados[ck].tex,
                            bs_criados[bs_stage_criado_first[i] + j].tex))
                    repetido = 1;
            if (!repetido) cri += bs_criado_vram_bytes[ck];
        }
        /* O PISO EM PERSPECTIVA (`bs_chaos[]`) mora no mesmo pool e tem uma
           chave por piso: as duas imagens dele (musica e cutscene) sao
           trocadas no lugar, nunca convivem. */
        for (k = 0; k < bs_stage_chao_count[i]; k++)
            cri += bs_chao_vram_bytes[bs_stage_chao_first[i] + k];
        CHECK_EQ(soma, bs_stage_vram_bytes[i]);
        CHECK_EQ(sp_stage_demand(bs_stage_props[i].id, &d), 0);
        CHECK_EQ(d, soma + cri);
    }
    printf("== pool: os sprites do script tem tamanho e chave proprios ==\n");
    for (i = 0; i < BS_CRIADO_COUNT; i++) {
        /* Alinhado a 256 como os props, e a chave numa FAIXA separada: o
           `sp_criado(0)` nao pode ser o mesmo bloco do `sp_prop(0)`. */
        CHECK(bs_criado_vram_bytes[i] > 0);
        CHECK_EQ(bs_criado_vram_bytes[i] % 256, 0);
        CHECK_EQ(bs_criado_vram_bytes[i], sp_criado_bytes(i));
        CHECK(SP_CHAVE_CRIADO(i) != SP_CHAVE_PROP(i));
        CHECK(SP_CHAVE_CRIADO(i) != SP_CHAVE_PROP(BS_PROP_VRAM_COUNT - 1));
        CHECK(SP_CHAVE_CRIADO(i) >= SP_CHAVE_CRIADO_BASE);
    }
    CHECK_EQ(sp_criado_bytes(-1), 0);
    CHECK_EQ(sp_criado_bytes(BS_CRIADO_COUNT), 0);
    CHECK(SP_CHAVE_CRIADO_BASE > SP_CHAVE_PROP(BS_PROP_VRAM_COUNT - 1));
    {
        /* Duas chaves diferentes para o mesmo pool: alocar os dois e conferir
           que os enderecos nao se sobrepoem. */
        unsigned int a1 = 0, a2 = 0;
        sp_init(&pool, 452608u, SP_POOL_BYTES);
        CHECK_EQ(sp_prop(&pool, 0, &a1), 0);
        CHECK_EQ(sp_criado(&pool, 0, &a2), 0);
        CHECK(a1 != a2);
        if (a1 < a2)
            CHECK((unsigned long long)a1 * 4u + sp_prop_bytes(0) <= (unsigned long long)a2 * 4u);
        else
            CHECK((unsigned long long)a2 * 4u + sp_criado_bytes(0) <= (unsigned long long)a1 * 4u);
    }
    CHECK_EQ(sp_stage_demand("naoExiste", NULL), -1);

    printf("== pool: os 21 palcos cabem no orcamento do pool ==\n");
    /* O pior caso e' o que importa: pool vazio, todos os props do palco dentro.
       SP_POOL_BYTES = 4.194.304 - 1.146.880 (dois framebuffers) - 663.552
       (tex2 + fonttex + gftex16). A `tex` do menu NAO esta' nessa subtracao:
       ela e' chave do pool e fica liberada durante a musica. Os personagens e
       os dois icones tambem saem do pool -- essa conta, que depende do pacote
       de cada personagem, e' tools/orcamento_vram.py (roda sobre os pacotes de
       verdade e reprova a musica que nao couber); aqui se prova a parte que o
       C pode provar sem os pacotes: cada palco cabe sozinho. */
    unsigned int pior_final = 0;
    CHECK_EQ(SP_POOL_BYTES, 2383872u);
    CHECK(BS_STAGE_VRAM_MAX <= SP_POOL_BYTES);
    {
        unsigned int pior = 0;
        const char *pior_nome = NULL;
        for (i = 0; i < BS_STAGE_PROPS_COUNT; i++) {
            unsigned int livre;
            const bs_stage_props_t *st = &bs_stage_props[i];
            sp_init(&pool, 452608u, SP_POOL_BYTES);
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
                CHECK(addr >= 452608u);
                CHECK((unsigned long long)addr * 4u + sp_prop_bytes(st->first + k)
                      <= (unsigned long long)452608u * 4u + SP_POOL_BYTES);
            }
            /* e os sprites do script do mesmo palco, no mesmo pool -- com a
               MESMA regra do runtime: duas entradas com a mesma textura
               (dust1/dust3 do sserafim) ocupam um lugar so'. */
            for (k = 0; k < bs_stage_criado_count[i]; k++) {
                unsigned int addr = 0;
                int r, j, repetido = 0;
                for (j = 0; j < k; j++)
                    if (!strcmp(bs_criados[bs_stage_criado_first[i] + k].tex,
                                bs_criados[bs_stage_criado_first[i] + j].tex))
                        repetido = 1;
                if (repetido) continue;
                r = sp_criado(&pool, bs_stage_criado_first[i] + k, &addr);
                if (r < 0) {
                    printf("  FALHA: %s criado %d (%u bytes) nao coube\n",
                           st->id, bs_stage_criado_first[i] + k,
                           sp_criado_bytes(bs_stage_criado_first[i] + k));
                    falhas++;
                }
                CHECK(addr >= 452608u);
                CHECK((unsigned long long)addr * 4u
                          + sp_criado_bytes(bs_stage_criado_first[i] + k)
                      <= (unsigned long long)452608u * 4u + SP_POOL_BYTES);
            }
            /* o piso em perspectiva, uma chave por piso */
            for (k = 0; k < bs_stage_chao_count[i]; k++) {
                unsigned int addr = 0;
                int r = sp_chao(&pool, bs_stage_chao_first[i] + k, &addr);
                if (r < 0) {
                    printf("  FALHA: %s chao %d (%u bytes) nao coube\n",
                           st->id, bs_stage_chao_first[i] + k,
                           sp_chao_bytes(bs_stage_chao_first[i] + k));
                    falhas++;
                }
                CHECK(addr >= 452608u);
                CHECK((unsigned long long)addr * 4u
                          + sp_chao_bytes(bs_stage_chao_first[i] + k)
                      <= (unsigned long long)452608u * 4u + SP_POOL_BYTES);
            }
            CHECK_EQ(pool.falhas, 0u);
            livre = sp_livre(&pool);
            /* o que sobrou = pool - props - sprites do script do palco */
            {
                unsigned int ocupado = bs_stage_vram_bytes[i];
                for (k = 0; k < bs_stage_criado_count[i]; k++) {
                    int ck = bs_stage_criado_first[i] + k, j, repetido = 0;
                    for (j = 0; j < k; j++)
                        if (!strcmp(bs_criados[ck].tex,
                                    bs_criados[bs_stage_criado_first[i] + j].tex))
                            repetido = 1;
                    if (!repetido) ocupado += bs_criado_vram_bytes[ck];
                }
                for (k = 0; k < bs_stage_chao_count[i]; k++)
                    ocupado += bs_chao_vram_bytes[bs_stage_chao_first[i] + k];
                CHECK_EQ(livre, SP_POOL_BYTES - ocupado);   /* bytes */
                if (ocupado > pior) { pior = ocupado; pior_nome = st->id; }
            }
        }
        printf("  pior palco: %s com %u bytes de %u (sobra %u)\n",
               pior_nome, pior, SP_POOL_BYTES, SP_POOL_BYTES - pior);
        CHECK(pior_nome != NULL);
        /* O pior caso agora e' o `sserafim`, e nao mais o `phillyBlazin`: ele
           tem 13 props + as QUATRO poeiras (duas imagens) + o piso, e o piso
           sozinho custa mais que o ceu do blazin. O numero continua abaixo do
           pool (2.383.872) -- e' ele que decide se o palco cabe ANTES de os
           personagens entrarem, que e' a conta de tools/orcamento_vram.py. */
        CHECK_EQ(pior_nome != NULL, 1);
        if (pior_nome) CHECK(strcmp(pior_nome, "sserafim") == 0);
        CHECK(pior <= SP_POOL_BYTES);
        printf("  (o teto do pool e' %u B; o pior palco ocupa %u)\n",
               SP_POOL_BYTES, pior);
        pior_final = pior;
    }

    printf("== pool: residente nao custa nada, e o despejo funciona ==\n");
    sp_init(&pool, 452608u, SP_POOL_BYTES);
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

    printf("== pool: palco maior que o pool entra por despejo, e prop grande falha limpo ==\n");
    {
        unsigned int addr = 0xDEADBEEFu, maior = 0;
        int i, si_maior = 0;
        /* O palco MAIOR e' medido, nao suposto "o ultimo da tabela". Quando o
           reempacotamento por musica encolheu o tankmanBattlefield (1.700.352 ->
           1.486.592 B), o ultimo da tabela deixou de ser o maior e este teste
           passou a medir outro palco -- acusando falha por motivo errado. */
        for (i = 0; i < BS_STAGE_PROPS_COUNT; i++) {
            unsigned int soma = 0;
            for (k = 0; k < bs_stage_props[i].count; k++)
                soma += sp_prop_bytes(bs_stage_props[i].first + k);
            if (soma > maior) { maior = soma; si_maior = i; }
        }
        /* Pool de 1 MB e o maior palco pedindo MAIS que isso.
           O que tem de acontecer NAO e' falha: e' DESPEJO. O `sp_prop` documenta
           "-1 se nao cabe nem despejando tudo", e o cache e' LRU -- um palco
           maior que o pool carrega inteiro, so' que reenviando o que foi
           despejado (o custo que a telemetria mede como replays de textura).
           Exigir `addr == 0` era exigir o contrario do que o modulo faz: o teste
           so' passava enquanto o ULTIMO palco da tabela fosse o maior e o
           caminho de despejo nao fosse exercitado. */
        CHECK(maior > 1024u * 1024u);
        sp_init(&pool, 452608u, 1024u * 1024u);
        for (k = 0; k < bs_stage_props[si_maior].count; k++) {
            const bs_stage_props_t *st = &bs_stage_props[si_maior];
            addr = 0xDEADBEEFu;
            if (sp_prop(&pool, st->first + k, &addr) < 0) break;
        }
        CHECK(k == bs_stage_props[si_maior].count);   /* todos couberam */
        CHECK_EQ(pool.falhas, 0u);                    /* sem falha: houve despejo */
        CHECK(pool.cache.despejos >= 1u);             /* e o despejo rodou */
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
        sp_init(&pool, 452608u, SP_POOL_BYTES);
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
    printf("\n  pool: 21 palcos, %d props + %d sprite(s) de script + %d piso(s), "
           "pior %u bytes de %u -- cabe\n",
           BS_PROP_VRAM_COUNT, BS_CRIADO_COUNT, BS_CHAO_COUNT, pior_final,
           SP_POOL_BYTES);
    return 0;
}
