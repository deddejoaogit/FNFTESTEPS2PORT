/* stage_pool - as texturas do palco, num pool com despejo
 *
 * POR QUE ISTO EXISTE
 * O port desenha UM palco (o do tutorial) numa UNICA textura composta. Os 21
 * palcos do jogo sao 157 props (src/stage_dados.h), cada um com a sua textura e
 * o seu quadro. Medido: o pior palco pede 1.700.352 bytes de um quadro de cada
 * prop, e o jogo tinha 1.269.760 bytes livres com as texturas de menu
 * residentes -- 15 dos 21 palcos nao cabiam.
 *
 * Este modulo e' o pool: quem desenha pede a textura por CHAVE (o indice do prop
 * em bs_props[]) e recebe um endereco de VRAM. Residente = custo zero. Nao
 * coube = despeja a menos usada recentemente. Nao caber de jeito nenhum vira
 * falha contada, nunca um desenho silencioso com o endereco errado.
 *
 * O trabalho de alocar e despejar e' do vc_* (src/engine/vram_cache.c), que ja'
 * tem teste proprio. Aqui a chave e' derivada do prop, o tamanho vem da TABELA
 * GERADA (src/engine/stage_vram_data.h, de assets-data/stage-props.json) e a
 * contabilidade e' a que o log mostra.
 *
 * C puro: sem GS, sem arquivo. Roda no PC, com a demanda real dos 21 palcos.
 *
 * ORCAMENTO DO POOL (medido, ver docs/PLANO_COMPLETO.md, Fase 4 e 6)
 *   VRAM total .............................. 4.194.304 B
 *   dois framebuffers 640x448 PSMCT16 ....... 1.146.880 B
 *   material que fica FORA do pool:
 *     tex2 (notas/receptores) 327.680 + fonttex (HUD) 294.912
 *     + gftex16 (GF do tutorial) 40.960 ......... 663.552 B
 *   -------------------------------------------------------------
 *   POOL .................................... 2.383.872 B
 *   pior palco do plano atual ............... 1.700.352 B
 * Dentro do pool moram, alem dos props: a `tex` do menu (320x256 PSMCT32 =
 * 327.680 B, FIXA no titulo/menu/story e LIBERADA durante a musica), a gftex e a
 * stagetex do menu, os tres personagens da musica (cada um com o tamanho do seu
 * pacote CHP4) e os dois icones de vida do HUD.
 */
#ifndef FNF_STAGE_POOL_H
#define FNF_STAGE_POOL_H

#include "vram_cache.h"
#include "stage_dados.h"
/* `bs_criados[]` (os sprites que o SCRIPT do palco cria) e a tabela de
   bytes deles: o pool guarda props E criados, entao quem pergunta o tamanho
   precisa das duas. */
#include "stage_criados_dados.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Orcamento do pool: 4.194.304 - 1.146.880 (framebuffers) - 663.552 (tex2 +
   fonte + gftex16). A `tex` do menu NAO entra na conta: ela e' chave do pool. */
#define SP_POOL_BYTES 2383872u

/* Chave do prop no cache. 0 e' "chave invalida" no vc_*, entao o prop i usa
   i+1. Improvável de colidir: as chaves de tela que entrarem no mesmo cache
   devem usar faixas separadas (ver sp_chave_de_tela). */
#define SP_CHAVE_PROP(prop) ((unsigned int)(prop) + 1u)

/* Faixa de chaves reservada para texturas de TELA que tambem morem no pool
   (gftex, stagetex). Fica longe da faixa dos props, que hoje vai ate' 157. */
#define SP_CHAVE_TELA_BASE 0x1000u

/* Os SPRITES DO SCRIPT DO PALCO (`bs_criados[]`, src/stage_criados_dados.h) tem
   a sua propria faixa: sao outra tabela de tamanhos e nao podem colidir com as
   chaves dos props nem com as de tela. */
#define SP_CHAVE_CRIADO_BASE 0x2000u
/* OS PISOS EM PERSPECTIVA (`bs_chaos[]`) tem a faixa propria: um piso ocupa
   lugar de palco como qualquer prop e nao pode colidir com um criado de mesmo
   indice. Uma chave so' por piso -- as DUAS imagens dele (a da musica e a da
   cutscene) tem o mesmo tamanho e nunca aparecem juntas: o pacote e' trocado no
   lugar, com um upload, quando a cutscene comeca e quando ela acaba. */
#define SP_CHAVE_CHAO_BASE 0x3000u
#define SP_CHAVE_CHAO(i) (SP_CHAVE_CHAO_BASE + (unsigned int)(i) + 1u)
#define SP_CHAVE_CRIADO(i) (SP_CHAVE_CRIADO_BASE + (unsigned int)(i) + 1u)

typedef struct {
    vc_t cache;
    unsigned int base;      /* em palavras de 32 bits, como o graph_vram_allocate */
    unsigned int tamanho;   /* bytes */
    unsigned int pedidos;   /* chamadas a sp_prop */
    unsigned int falhas;    /* pedidos que nao couberam */
} sp_t;

/* 'base' em PALAVRAS (offset de VRAM, o mesmo numero que o graph_vram_allocate
   devolve) e 'tamanho' em BYTES. O sp_prop/sp_chave devolvem o endereco tambem
   em PALAVRAS; a conversao para bytes mora so' dentro do modulo. */
void sp_init(sp_t *p, unsigned int base_palavras, unsigned int tamanho_bytes);

/* Tamanho em VRAM de um quadro do prop, ja alinhado. 0 se o indice nao existe. */
unsigned int sp_prop_bytes(int prop);

/* Quanto o palco inteiro pede, somando os props dele. -1 se o palco nao existe. */
int sp_stage_demand(const char *stage, unsigned int *bytes);

/* Idem para um sprite do script do palco: tamanho de `bs_criado_vram_bytes[]`
   (o mesmo alinhamento do TBP0) e chave na faixa propria. */
unsigned int sp_criado_bytes(int criado);
/* Idem para os PISOS EM PERSPECTIVA (`bs_chaos[]`). */
unsigned int sp_chao_bytes(int chao);
int sp_criado(sp_t *p, int criado, unsigned int *endereco);
int sp_chao(sp_t *p, int chao, unsigned int *endereco);

/* Pede a textura do prop. Devolve 1 se ja' estava residente (NADA a enviar),
   0 se foi alocada agora (o chamador precisa enviar os pixels) e -1 se nao
   cabe nem despejando tudo. */
int sp_prop(sp_t *p, int prop, unsigned int *endereco);

/* Pede por chave crua, para as texturas de tela que dividem o pool. */
int sp_chave(sp_t *p, unsigned int chave, unsigned int bytes, unsigned int *endereco);

/* Marca uma chave como permanente (nunca despejada). */
void sp_fixar(sp_t *p, unsigned int chave);

/* Desfaz a fixacao: a textura volta a ser despejavel. Ver vc_soltar(). */
void sp_soltar(sp_t *p, unsigned int chave);

/* 1 se a chave esta' fixa. */
int sp_esta_fixo(const sp_t *p, unsigned int chave);

/* Esquece tudo que nao for fixo. Chamar ao trocar de musica/palco. */
void sp_limpar(sp_t *p);

unsigned int sp_livre(const sp_t *p);

#ifdef __cplusplus
}
#endif
#endif /* FNF_STAGE_POOL_H */
