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
#include "stage_criados_dados.h"

#ifdef __cplusplus
extern "C" {
#endif

/* O espaco de tela do port, o mesmo de main.c: 640x360. */
#define SL_VIEW_W 640.0f
#define SL_VIEW_H 360.0f

/* Um palco tem no maximo 17 props texturizados + 2 cores + os sprites do
   script hoje (1 no phillyBlazin); 32 da' folga para o proximo jogo. */
#define SL_MAX_ITENS 32

/* Um item da lista de desenho: ou um prop TEXTURIZADO (cor < 0, prop >= 0),
   ou um prop de COR (prop < 0, cor >= 0) -- um retangulo chapado, que o
   Stage.hx L203-216 cria e que nao custa textura nem pacote. Os dois entram
   na MESMA lista, porque no original os dois entram na mesma ordenacao por
   zIndex (Stage.hx L300-330). */
typedef struct {
    int prop;       /* indice em bs_props[], -1 quando e' cor           */
    int cor;        /* indice em bs_cores[], -1 quando e' prop          */
    int anim;       /* indice DENTRO do prop (0..prop->anim_count-1)    */
    int frame;      /* quadro dentro dessa animacao                    */
    float x0, y0;   /* canto superior esquerdo, espaco 640x360         */
    float w, h;     /* tamanho na tela (do src do original, ver .c)    */
    int tex_w, tex_h; /* resolucao do quadro no pacote CFZ4 (sw/sh)    */
    int z;          /* zIndex do prop: a ordem de desenho do Stage.hx  */
    float alpha;    /* alpha do dado (1.0 = opaco)                     */
    /* COR do prop pelo script do palco (`lightWindow.color = lightColors[...]`
       no phillyTrain.hxc L149). `pintado = 0` = desenha com o branco de
       sempre. (`cor` ja' e' o indice do prop de COR -- nomes diferentes.) */
    int pintado;    /* 1 = a cor abaixo veio do script do palco           */
    unsigned char rgb[3];
    int blend;      /* 0 normal, 1 ADD, 2 MULTIPLY, 3 SCREEN           */
    /* SPRITE DO SCRIPT DO PALCO (`new FlxTiledSprite`, `new FlxBackdrop`):
       indice em bs_criados[], ou -1. Quando >= 0, `prop` e `cor` sao -1 e os
       campos abaixo dizem como repetir a imagem. */
    int criado;
    float tile;     /* periodo da repeticao em px do original; 0 = sem repetir */
    float tile_off; /* deslocamento do `scroll` no instante t, em px do original */
    /* O PISO EM PERSPECTIVA (`bs_chaos[]`): indice, ou -1. Quando >= 0 o
       item NAO e' um retangulo: `cb_x/cb_y` e `ct_x/ct_y` sao os dois pontos
       de referencia (baixo e cima) JA' em espaco de tela, e o desenho aplica a
       matriz afim de `sl_chao_cantos`. `w`/`h` continuam sendo o tamanho do
       quadro na tela. */
    int chao;
    float cb_x, cb_y, ct_x, ct_y;
    /* Escala do mundo para a tela neste item (px de tela por unidade do
       original). E' o `0,5 * zoom * bop`. O desenho do sprite de script
       precisa dela para o periodo da repeticao: com o `x0` alinhado na borda
       da tela, `w / largura_nativa` NAO e' mais essa escala. */
    float esc;
    /* O `angle` do sprite, em GRAUS, vindo do script do palco (o
       `tankRolling.angle` do tankmanBattlefield.hxc L60 e as `rotations` dos
       carros do phillyStreets.hxc). 0 = sem rotacao, que e' o caso de todos os
       props que o JSON posiciona. O flixel gira em volta do CENTRO do quadro
       (`centerOrigin` no `resetHelpers` do FlxSprite), e e' o que o desenho
       faz. */
    float ang;
} sl_item_t;

/* OS QUATRO CANTOS do piso em perspectiva, em espaco de tela.
 *
 * A formula e' a do `PerspectiveSprite.updateSkew` (preload/scripts/stages/
 * props/PerspectiveSprite.hxc L97-125), transcrita:
 *
 *   distX = cima.x - baixo.x
 *   distY = cima.y - (baixo.y - H)
 *   a = 1   b = 0   c = -distX/H   d = 1 - distY/H   tx = distX/2  ty = distY/2
 *   sprite.x = baixo.x - W/2     sprite.y = baixo.y - H
 *
 * e o quadrilatero e' o da matriz aplicada ao quadro WxH. Ordem dos cantos:
 * superior-esquerdo, superior-direito, inferior-direito, inferior-esquerdo.
 * Devolve 0 quando a altura e' zero (o `H` divide). */
int sl_chao_cantos(float bx, float by, float tx, float ty, float w, float h,
                   float *px, float *py);

/* Indice em bs_stage_props[] ou -1. Compara o id inteiro, nao o prefixo. */
int sl_stage_find(const char *stage);

/* Quantos props o palco tem, e o primeiro em bs_props[]. -1 de palco. */
int sl_stage_prop_count(const char *stage);
int sl_stage_first_prop(const char *stage);

/* Idem para os props de COR (retangulos de bs_cores[]). */
int sl_stage_cor_count(const char *stage);
int sl_stage_first_cor(const char *stage);

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
   startingAnimation do JSON, e nada sem ele).

   Props com `alpha <= 0` NAO entram na lista: desenhar um prop invisivel e'
   desenhar o que o original nao desenha. As cores com alpha 0 tambem ficam de
   fora (o `additionalLighten` do phillyBlazin nasce `visible = false` no
   script do palco, phillyBlazin.hxc L77).

   Os SPRITES DO SCRIPT (`bs_criados[]`, tabela de tools/gen_stage_criados.py)
   entram na MESMA lista e na mesma ordenacao por z: no original eles tambem
   sao `add()`ados ao grupo do palco. `tile_off` ja' vem com o scroll de `t` --
   `vel_x * t` --, que e' o `scrollX -= FlxG.elapsed * 35` do script somado. */
int sl_collect(const char *stage, float cam_x, float cam_y, float zoom,
               float bop, int step, float beat_s, float t, sl_item_t *out, int max);

/* ---- O QUE O SCRIPT DO PALCO MANDA ----
 * Os `.hxc` de palco mudam `alpha`, `x` e a animacao de props durante a musica
 * (`phillyBlazin.hxc` L152-167 acende `skyAdditive` a 0,7 e joga o `lightning`
 * numa animacao `strike` que dura 1,5 s). Sem isso o port desenha o palco no
 * estado inicial e mais nada -- e os tres props do relampago nem aparecem,
 * porque no JSON eles nascem com alpha 0.
 *
 * Os callbacks recebem o indice ABSOLUTO em bs_props[]/bs_cores[] e devolvem
 * "sem override" quando o prop nao e' do script:
 *   alpha     < 0  -> usa o alpha do dado
 *   x         o proprio `x_dado` quando nao mexe
 *   frame     0    -> o layout escolhe animacao/quadro pelo dado
 *   cor_alpha < 0  -> usa o alpha do dado
 *   cor       0    -> o prop nao e' pintado pelo script
 *
 * Sem `sl_set_script()` (testes, palcos sem script) tudo isso e' inerte: o
 * dado manda, exatamente como antes. */
typedef struct {
    float (*alpha)(int prop_abs);
    float (*x)(int prop_abs, float x_dado);
    int   (*frame)(int prop_abs, int *anim, int *frame);
    float (*cor_alpha)(int cor_abs);
    /* Cor do prop (0..255 por canal). Devolve 1 quando pinta. */
    int   (*cor)(int prop_abs, unsigned char *rgb);
    /* Y do prop em unidades do ORIGINAL, ou o proprio `y_dado`. Sem este
       callback o script so' mexia no x, e os carros do `phillyStreets` andam
       numa curva de Bezier (x E y mudam por quadro). */
    float (*y)(int prop_abs, float y_dado);
    /* Angulo do prop em GRAUS. Devolve 1 e escreve `*graus` quando o script
       gira o sprite. */
    int   (*ang)(int prop_abs, float *graus);
    /* O `idleSuffix` do Bopper (school.hxc L19: o `freaks` da `roses` toca
       `danceLeft-scared`). Devolve "" quando o prop nao tem sufixo. */
    const char *(*sufixo)(int prop_abs);
} sl_script_t;

void sl_set_script(const sl_script_t *script);

#ifdef __cplusplus
}
#endif

#endif /* FNF_STAGE_LAYOUT_H */
