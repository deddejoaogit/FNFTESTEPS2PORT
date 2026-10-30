/* char_pack - o pacote de UM personagem: quadros + tabela de animacoes
 *
 * POR QUE ISTO EXISTE
 * O palco ja' e' generico (`stage_pool.c` le CFZ4, `stage_layout.c` decide o que
 * aparece), mas os personagens nao: o estado 8 do main.c conhece DOIS pacotes
 * escritos no codigo (`bf_frames.lz4f`, `gf_frames.lz4f`), com 43 e 87 quadros,
 * e escolhe o quadro por tabelas fixas (BF_SING/GF_SING). Sao 46 personagens.
 *
 * Cada personagem vira UM arquivo, `assets/chars/<id>.chp4`, com os quadros
 * comprimidos (o mesmo LZ4 do resto do jogo) e, junto deles, a tabela que diz
 * QUANTOS quadros cada animacao tem, a que taxa e se volta ao comeco.
 *
 * POR QUE A TABELA VAI NO PACOTE, E NAO NO `char_dados.h`
 * Porque ela nao existe fora da arte. Medido: `assets-data/characters` (um JSON por personagem)
 * tem o NOME e o prefixo de cada animacao, e so' em algumas o `frameIndices` e
 * o `looped`; a taxa de quadro e a lista de quadros vivem no atlas
 * (AnimationData), que so' o clone da arte tem. Inventar esses numeros aqui
 * seria divergencia (regra 4: dado faltando vira coleta, nao chute). Quem le' o
 * atlas e' o empacotador; o jogo confia no pacote e CONFERE contra ele -- se a
 * tabela nao bater com os quadros que o pacote tem, o personagem nao entra em
 * cena e o log diz por que (`char_pack_erro`).
 *
 * FORMATO CHP4 (little endian)
 *   0   "CHP4"
 *   4   u32 versao = 1
 *   8   u32 n_quadros
 *   12  u32 bytes_por_quadro        (w*h*4 para PSMCT32, w*h*2 para PSMCT16)
 *   16  u32 w
 *   20  u32 h
 *   24  u32 n_animacoes
 *   28  u32 psm                     (32 ou 16)
 *   32  u32 reservado = 0
 *   36  n_quadros * (u32 offset, u32 tamanho)   -- blocos LZ4
 *   ... n_animacoes * 40 bytes:
 *         char nome[24]            (o mais longo do original tem 23:
 *                                   "singRIGHTmiss-beautiful")
 *         u32 primeiro, u32 quantos, u32 fps*100, u32 looped
 *   ... os blocos
 *
 * NAO E' O CFZ4 DO PALCO, E O MOTIVO E' CONCRETO
 * O CFZ4 tem UM tamanho de quadro para o pacote inteiro e nenhuma tabela de
 * animacao (o palco tira isso do `stage_dados.h`, gerado da JSON, que TEM a
 * taxa e o loop de cada prop). Para personagem esse dado nao existe fora da
 * arte, entao ele viaja no pacote. O resto -- LZ4 bloco a bloco, um quadro por
 * vez na VRAM -- e' identico.
 *
 * C puro: sem GS, sem arquivo, sem alocacao. Roda no PC.
 */
#ifndef FNF_CHAR_PACK_H
#define FNF_CHAR_PACK_H

#include "char_anim.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CHP_MAGIC "CHP4"
#define CHP_VERSAO 1u
#define CHP_CABECALHO 36u
#define CHP_NOME 24       /* o mais longo do original: 23 letras + NUL */
/* 46 personagens nao passam disso: o maior (bf-pixel e as formas do pai) tem 37
   animacoes. O limite existe para o cabecalho nao poder pedir memoria demais a
   um arquivo corrompido -- e' conferido, nao assumido. */
#define CHP_MAX_ANIMS 64
/* 24 de nome + 4 campos de 4 bytes */
#define CHP_ANIM_ENTRADA 40u

#define CHP_ERR_MAGIC     -1  /* nao e' CHP4                     */
#define CHP_ERR_VERSAO    -2  /* versao diferente                */
#define CHP_ERR_CABECALHO -3  /* menor que o cabecalho           */
#define CHP_ERR_TABELA    -4  /* tabela de quadros fora do arquivo */
#define CHP_ERR_ANIMS     -5  /* n_animacoes zero ou acima do limite */
#define CHP_ERR_QUADRO    -6  /* bytes_por_quadro ou w*h zerados ou absurdos */
#define CHP_ERR_PSM       -7  /* psm diferente de 32 e 16        */
#define CHP_ERR_BLOCO     -8  /* offset/tamanho de um bloco fora do arquivo */
#define CHP_ERR_TAMANHO   -9  /* bytes_por_quadro != w*h*(psm/8)  */
#define CHP_ERR_LZ4       -10 /* o bloco nao decodifica no tamanho pedido */

typedef struct {
    char nome[CHP_NOME];
    int primeiro;   /* indice do primeiro quadro no pacote */
    int quantos;
    int fps100;     /* quadros por segundo * 100 (0 = taxa desconhecida) */
    int looped;
} cp_anim_t;

typedef struct {
    const unsigned char *pacote;
    unsigned int bytes;
    int w, h, psm;
    unsigned int n_quadros, bytes_por_quadro;
    int n_anims;
    cp_anim_t anim[CHP_MAX_ANIMS];
} cp_pack_t;

/* Le' o cabecalho e a tabela de animacoes. Devolve 0 ou CHP_ERR_*. Nao copia os
   quadros: `p->pacote` aponta para o buffer do chamador, que tem de continuar
   vivo enquanto o pacote for usado. */
int cp_abrir(const unsigned char *pacote, unsigned int bytes, cp_pack_t *p);

/* Descomprime o quadro `q` em `dst` (que precisa de bytes_por_quadro bytes).
   Devolve 0 ou negativo. */
int cp_quadro(const cp_pack_t *p, unsigned int q, unsigned char *dst);

/* Indice da animacao pelo nome, -1 se o personagem nao a tem. */
int cp_anim_i(const cp_pack_t *p, const char *nome);

/* A animacao no formato que o `char_anim.c` consome. 0 ou negativo. */
int cp_seq(const cp_pack_t *p, int anim, ch_seq_t *out);

/* Confere a tabela inteira contra os quadros: nenhuma animacao pode apontar
   para um quadro que o pacote nao tem. E' a checagem que impede um atlas
   remontado de mostrar o quadro errado no meio de uma musica -- e ela e'
   barata o bastante para rodar no carregamento, nao num teste. */
int cp_conferir(const cp_pack_t *p);

/* Nome do erro, para o log. Nunca devolve 0. */
const char *cp_erro(int rc);

#ifdef __cplusplus
}
#endif
#endif /* FNF_CHAR_PACK_H */
