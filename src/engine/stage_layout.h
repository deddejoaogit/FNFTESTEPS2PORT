/* stage_layout - onde cada prop do palco cai na tela, em C puro.
 *
 * POR QUE ISTO EXISTE
 * O port desenha UM palco, o do tutorial, com a composicao toda embutida numa
 * unica textura e a formula escrita a mao dentro do estado 8 do main.c. Para os
 * 21 palcos do jogo isso nao serve: sao 157 props (src/stage_dados.h), cada um
 * com posicao, escala, scrollFactor, zIndex, danceEvery e animacoes proprias.
 *
 * Aqui mora SO' a conta: dado o palco e a camera, quais props aparecem, em que
 * retangulo da tela e em que quadro da animacao, na ordem de z. Nada de GS,
 * nada de arquivo, nada de VRAM -- por isso roda no PC, com teste, contra os
 * mesmos dados que o console vai usar.
 *
 * DE ONDE VEM A FORMULA
 * Nao foi deduzida: e' a que o port JA' usa no tutorial e que bateu com a
 * captura do console (main.c, estado 8: "320 + (mundo - camera*scroll + 640*
 * scroll - 640) * 0.5 * zoom * bop", idem em y com 180/360). O espaco de tela
 * aqui e' o 640x360 do port; 0,5 e' metade do 1280x720 do original, entao
 * escala 1,1 significa 0,55.
 *
 * O QUE NAO ESTA AQUI, DE PROPOSITO
 * - quais props tem textura residente (isso e' o vram_cache / asset_bank);
 * - o desenho (sprite no pacote do GS);
 * - o personagem: ele tambem tem scrollFactor, mas a posicao vem do palco
 *   (bs_stages[].bf/gf/dad) e dos offsets do personagem, e fica para o passo
 *   seguinte, com teste proprio.
 */
#ifndef FNF_STAGE_LAYOUT_H
#define FNF_STAGE_LAYOUT_H

#include "stage_dados.h"

#ifdef __cplusplus
extern "C" {
#endif

/* O espaco de tela do port, o mesmo de main.c: 640x360. */
#define SL_VIEW_W 640.0f
#define SL_VIEW_H 360.0f

/* Um palco tem no maximo 17 props hoje; 32 da' folga para o proximo jogo. */
#define SL_MAX_ITENS 32

typedef struct {
    int prop;       /* indice em bs_props[]                            */
    int anim;       /* indice DENTRO do prop (0..prop->anim_count-1)    */
    int frame;      /* quadro dentro dessa animacao                    */
    float x0, y0;   /* canto superior esquerdo, espaco 640x360         */
    float w, h;     /* tamanho na tela (do src do original, ver .c)    */
    int tex_w, tex_h; /* resolucao do quadro no pacote CFZ4 (sw/sh)    */
    int z;          /* zIndex do prop: a ordem de desenho do Stage.hx  */
} sl_item_t;

/* Indice em bs_stage_props[] ou -1. Compara o id inteiro, nao o prefixo. */
int sl_stage_find(const char *stage);

/* Quantos props o palco tem, e o primeiro em bs_props[]. -1 de palco. */
int sl_stage_prop_count(const char *stage);
int sl_stage_first_prop(const char *stage);

/* Quadro da animacao 'anim' no instante t (segundos). Anima em loop volta ao
   zero; anima que nao repete fica no ultimo quadro. E' o mesmo criterio do
   CFZ4, onde o canvas e' o maior quadro e o pacote nao sabe de tempo. */
int sl_prop_frame(const bs_prop_t *p, int anim, float t);

/* Preenche 'out' (ate' 'max') com os props VISIVEIS do palco, ordenados por
   zIndex crescente -- que e' a ordem de desenho do original. Devolve quantos
   foram escritos, ou -1 se o palco nao existe.

   'step' e' o passo do chart (uma batida = 4 passos; negativo antes da
   musica), 'beat_s' a duracao de UMA batida na secao de BPM vigente e 't' o
   tempo de musica em segundos. E' com eles que sai a animacao de cada prop:
   ver sl_escolhe_anim() em .c para as duas regras do Bopper.hx (danceEvery > 0
   reinicia a cada danceEvery batidas; danceEvery == 0 toca so' o
   startingAnimation do JSON, e nada sem ele). */
int sl_collect(const char *stage, float cam_x, float cam_y, float zoom,
               float bop, int step, float beat_s, float t, sl_item_t *out, int max);

#ifdef __cplusplus
}
#endif

#endif /* FNF_STAGE_LAYOUT_H */
