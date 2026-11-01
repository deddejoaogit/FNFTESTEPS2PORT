#ifndef RESULTADO_H
#define RESULTADO_H

/* ARTE DA TELA DE RESULTADO: estado, tempo e leitura de quadro.
 *
 * O QUE O ORIGINAL FAZ (ResultState.hx, v0.8.6)
 *   Cada folha e' um FunkinSprite escondido no comeco. Um FlxTimer a torna
 *   visivel e toca a animacao, que e' `false` em `looped` -- ou seja, toca uma
 *   vez e para no ULTIMO quadro:
 *
 *       resultsAnim  6/24 s   "results instance 1"   (36 quadros)
 *       ratingsPopin 21/24 s  "Categories"           (16 quadros)
 *       scorePopin   36/24 s  "tally score"          (30 quadros)
 *
 *   Todas a 24 fps. Como o flixel para no ultimo quadro em vez de sumir, a
 *   folha continua na tela -- e' isso que monta a cena final da tela.
 *
 * O QUE ESTE MODULO GUARDA
 *   O tempo (quando cada folha aparece e em que quadro ela esta') e o pacote
 *   aberto, para o main.c pegar UM quadro por vez. Ele NAO conhece GS, VRAM nem
 *   o desenho: recebe ms e devolve numero de quadro. Por isso roda inteiro no
 *   PC (test/test_resultado.c), com pacotes CFZ4 de verdade.
 */

#include "pack_stream.h"
#include "resultado_dados.h"

/* Pacote de UMA folha. Um personagem pode ter mais de um arquivo (`part%d`),
 * mas aqui cada folha cabe num so': o maior pacote e' 699 KB e o limite do
 * leitor e' 8 MB. Se um dia nao couber, `res_carregar` devolve erro em vez de
 * abrir um pedaco e fingir que abriu tudo. */
typedef struct {
    pst_pack_t pst;
    int aberto;          /* 1 = pacote aberto          */
    int tentou;          /* 1 = ja' tentou abrir        */
    unsigned int vistos; /* quadros pedidos (telemetria) */
} res_folha_t;

typedef struct {
    res_folha_t f[RES_FOLHAS];
} res_t;

/* Zera o estado (nenhum pacote aberto). */
void res_init(res_t *r);

/* Abre os pacotes de todas as folhas a partir de `dir`
 * (ex.: "mass:/FNFPS2/assets/"). Devolve o numero de folhas abertas; o
 * chamador compara com RES_FOLHAS e reporta o que faltou. */
int res_carregar(res_t *r, const char *dir);

/* Fecha os pacotes. Seguro chamar duas vezes. */
void res_liberar(res_t *r);

/* Quadro da folha `i` no instante `ms` (desde o inicio da tela), ou -1 se a
 * folha ainda nao apareceu / nao esta' carregada. */
int res_quadro(res_t *r, int i, unsigned long ms);

/* Copia o quadro `q` da folha `i` para `dst` (frame_bytes bytes).
 * Devolve 0 ou PST_ERR_*. */
int res_pixels(res_t *r, int i, int q, unsigned char *dst);

/* ---- A ARTE DO RANK (ResultState.hx L667-700 + Scoring.hx L627-666) --------
 *
 * Duas imagens do rank conquistado: uma VERTICAL (FlxBackdrop em x=1236,
 * y=100, rolando para cima) e uma HORIZONTAL (12 FlxBackdoor em
 * y = 60 + 67,5*i, metade para cada lado). O rank entra com um flash branco
 * da tela e um piscar de 0,25 s.
 *
 * Tudo aqui e' funcao PURA do tempo: sem arquivo, sem GS. O main.c so' le os
 * dois .rgba e desenha o que estas funcoes mandam.
 */

/* Indice do rank na tabela (0 = SHIT ... 5 = PERFECT_GOLD). Rank invalido cai
 * em SHIT, como `Scoring.calculateRank(...) ?? SHIT` (ResultState.hx L117). */
int res_rank_indice(int rank);

/* Tabela do rank `rank` (nunca nula). */
const res_rank_t *res_rank(int rank);

/* Alpha do FLASH da tela (0..255): 255 no instante do flash, caindo linear ate
 * 0 em `flash_dura_ms` (FlxTween.tween(bgFlash, {alpha: 0}, 14/24)). */
int res_flash_alpha(const res_rank_t *r, unsigned long ms);

/* O piscar de entrada: FlxFlicker.flicker(rankTextVert, 2/24*3, 2/24, true).
 * 1 = desenha, 0 = apagado (durante `flicker_total_ms`, alternando a cada
 * `flicker_ms`). Fora da janela do piscar, sempre 1. */
int res_rank_pisca(const res_rank_t *r, unsigned long ms);

/* Deslocamento (px, negativo para cima) do texto vertical. Zero ate'
 * `flash_ms + vert_espera_ms`; depois cresce com `vert_vel`. */
float res_rank_scroll_y(const res_rank_t *r, unsigned long ms);

/* Deslocamento (px, positivo para a direita) da linha `i` do texto
 * horizontal. As pares vao para a esquerda, as impares para a direita
 * (`(i % 2 == 0) ? -7.0 : 7.0`, L694-698). */
float res_rank_scroll_x(const res_rank_t *r, int linha, unsigned long ms);

#endif /* RESULTADO_H */
