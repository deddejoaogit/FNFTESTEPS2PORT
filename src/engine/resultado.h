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

#endif /* RESULTADO_H */
