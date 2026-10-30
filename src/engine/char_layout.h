/* char_layout - onde um personagem fica na tela e que animacao tocar
 *
 * POR QUE ISTO EXISTE
 * O port desenha DOIS personagens: bf e gf, no tutorial, com a posicao e as
 * faixas de quadro escritas a mao dentro do estado 8 do main.c. O jogo tem 46
 * (assets-data/characters) e 21 palcos, e cada palco define tres SLOTS (bf, gf,
 * dad) com posicao, cameraOffsets e zIndex proprios (bs_slots[] em
 * stage_dados.h).
 *
 * Aqui mora so' a conta: dado o personagem, o slot e algumas medidas, onde ele
 * cai, onde a camera deve olhar, e qual animacao tocar para cada tecla. Nada de
 * GS, arquivo ou VRAM -- roda no PC, com teste, contra os 46 personagens reais.
 *
 * DE ONDE VEM A FORMULA (nao foi deduzida)
 * E' a que o estado 8 do main.c usa e que a captura do console confirmou:
 *   sprite = slot.position - canvas/2 (x) - canvas (y)   [pe' no chao do slot]
 *            + character.offsets - 4/escala_do_pacote
 *   camera = slot.position + character.offsets + character.cameraOffsets
 *            + slot.cameraOffsets - (0, canvas/2)       [altura do peito]
 * Com o mainStage e o BF isso da' o foco (888,50 ; 589,00) que o port ja'
 * reproduz digito a digito -- e o teste exige esse numero.
 *
 * O QUE NAO ESTA AQUI, DE PROPOSITO
 * - o `gf.hx` escreve originalPosition sem globalOffsets, entao o offsets do
 *   personagem NAO entra na posicao do sprite dele. Isso e' do script do
 *   personagem, nao do palco: por isso ch_anchor() recebe a flag
 *   `usa_offsets`, e o tutorial continua decidindo com o valor que ja' foi
 *   aprovado no hardware. A CAMERA nao depende dessa flag: o
 *   resetCameraFocusPoint do BaseCharacter.hx usa originalPosition (que ja'
 *   tem o globalOffsets) + cameraOffsets.
 * - o empacotador: qual quadro do atlas cada animacao ocupa continua sendo
 *   trabalho dele (assets-data/characters + o clone da arte).
 */
#ifndef FNF_CHAR_LAYOUT_H
#define FNF_CHAR_LAYOUT_H

#include "char_dados.h"
#include "song_registry.h"
#include "stage_dados.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float sprite_x, sprite_y;   /* canto superior esquerdo, espaco do original */
    float cam_x, cam_y;         /* ponto que a camera segue                     */
} ch_anchor_t;

/* Indice do personagem em ch_chars[] pelo id, ou -1. */
int ch_find(const char *id);

/* Indice da animacao DENTRO do personagem, pelo nome; -1 se ele nao tem. */
int ch_anim(const char *id, const char *nome);
int ch_anim_i(int ci, const char *nome);

/* Idle: "idle" se existir; senao alterna "danceLeft"/"danceRight" pela batida
   (e' o que a GF e as personagens de fundo tem). Devolve -1 se nao ha nenhuma. */
int ch_anim_idle(int ci, int beat);

/* Animacao de canto: "singLEFT" (ou "singLEFTmiss" quando *miss) em qualquer
   direcao. -1 se o personagem nao canta (personagem de fundo: tem idle e mais
   nada, e ficar parado e' o comportamento certo). */
int ch_anim_sing(int ci, int dir, int missing);

/* Tecla do controle que corresponde a direcao (0..3 = LEFT, DOWN, UP, RIGHT). */
unsigned short ch_dir_button(int dir);

/* Direcao (0..3) para o nome de uma animacao sing, ou -1. */
int ch_anim_dir(const char *nome);

/* Onde o personagem cai e onde a camera olha.
   slot        - bs_slots[] do palco
   canvas_w/h  - tamanho do quadro no espaco do original (nao o do pacote)
   pad         - o ajuste de 4/escala_do_pacote do port
   usa_offsets - 0 para os personagens cujo script nao aplica offsets (gf.hx) */
void ch_anchor(const ch_dados_t *c, const bs_slot_t *slot,
               float canvas_w, float canvas_h, float pad, int usa_offsets,
               ch_anchor_t *out);

/* QUEM ENTRA EM CENA, E EM QUE SLOT.
   O original nao escolhe o slot pelo personagem, e sim pelo PAPEL: o
   jogador no slot `bf`, o oponente no `dad` e a namorada no `gf`
   (PlayState.hx L2200-2225, `currentStage.addCharacter(x, GF|BF|DAD)`,
   e Stage.hx L400-490, que posiciona pelo tipo). Isso tem uma consequencia
   que so' aparece quando se olha o dado: no Tutorial o OPONENTE e' a propria
   GF (`assets-data/songs/tutorial-metadata.json` -> characters.opponent =
   "gf", e nao ha `girlfriend`), entao ela entra pelo slot `dad`.
   E' por isso que a funcao existe: o desenho nao pode deduzir isso do id. */
#define CH_PAPEL_GF 0
#define CH_PAPEL_BF 1
#define CH_PAPEL_DAD 2
#define CH_CENA_PAPEIS 3

typedef struct {
    int papel;          /* CH_PAPEL_*                          */
    int ci;             /* indice em ch_chars[], -1 = slot vazio */
    const char *slot;   /* "gf", "bf", "dad"                   */
} ch_cena_t;

/* Preenche out[0..2] (gf, bf, dad) a partir da musica. Devolve 0, ou -1 se a
   musica ou a saida nao existem. Slot sem personagem fica com ci = -1 -- NAO
   se inventa personagem para tampar buraco: 11 das 27 musicas nao tem
   `girlfriend` no dado, e o slot fica vazio, como no original. */
int ch_cena_plano(const sr_song_t *song, ch_cena_t out[CH_CENA_PAPEIS]);

/* Slot de um palco pelo papel ("bf", "gf", "dad"); 0 se nao existir. */
const bs_slot_t *ch_slot(const char *stage, const char *role);

#ifdef __cplusplus
}
#endif
#endif /* FNF_CHAR_LAYOUT_H */
