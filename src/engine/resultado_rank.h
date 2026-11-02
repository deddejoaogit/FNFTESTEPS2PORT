/* resultado_rank - o QUADRO da animacao do personagem na tela de Resultado
 *
 * POR QUE ESTE ARQUIVO EXISTE
 * `ResultState.hx` L201-330 monta, para o rank da partida, uma ou duas
 * animacoes do personagem (`players/<dono>.json` -> `results[rank]`), e o
 * L704-740 as dispara. O que elas fazem no tempo nao e' "toca e para":
 *
 *   1. cada entrada tem um `delay` somado ao `getBFDelay()` do rank, e so'
 *      entao fica visivel (`sprite.visible = true; anim.play(startLabel)`);
 *   2. o `startFrameLabel` diz em que rotulo a animacao COMECA (o `bed` comeca
 *      em `INTRO`, nao no quadro 0 do pacote);
 *   3. ao terminar, `loopFrameLabel` manda tocar aquele rotulo de novo (e o
 *      `curAnim.looped = true` faz isso para sempre) ou, no caso de
 *      `loopFrame`, recomecar no QUADRO n do mesmo anim;
 *   4. sem nenhum dos dois e com `looped = false`, pausa no ULTIMO quadro.
 *
 * Tudo aqui e' funcao PURA do tempo desde a abertura da tela: sem arquivo, sem
 * GS, sem VRAM. E' o que permite provar as quatro regras sem ligar o console.
 */
#ifndef FNF_RESULTADO_RANK_H
#define FNF_RESULTADO_RANK_H

#include "pack_multi.h"
#include "resultado_rank_dados.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Quadro GLOBAL (indice no pacote do rank) que esta entrada desenha em `ms`
 * desde a abertura da tela. -1 = ainda invisivel (antes de `inicio_ms`). */
int res_rr_quadro(const res_rr_entrada_t *e, unsigned long ms);

/* Duracao (ms) de uma passada da animacao a partir de `inicio`, contando o
 * `loop_ini` como recomeco. 0 quando a entrada nao tem animacao. */
unsigned long res_rr_dura_ms(const res_rr_entrada_t *e);

/* ---- O PACOTE ABERTO ------------------------------------------------------
 * Cada entrada do rank e' um pacote CFZ4 (em uma ou mais PARTES) lido quadro a
 * quadro pelo `pack_multi`: na RAM fica a TABELA, na VRAM um quadro. */
typedef struct {
    pm_pack_t pm;
    int aberto;
    int erro;            /* primeiro erro de abertura, para o log */
} res_rr_pack_t;

typedef struct {
    res_rr_pack_t p[RES_RR_MAX_ENTRADAS];
    int idx;             /* rank carregado, -1 = nenhum            */
} res_rr_t;

void res_rr_init(res_rr_t *r);

/* Abre as partes de cada entrada do rank `idx`. Devolve quantas abriram. */
int res_rr_carregar(res_rr_t *r, const char *dir, int idx);

void res_rr_liberar(res_rr_t *r);

/* Quadro `q` da entrada `i` descomprimido em `dst` (w*h*psm/8 bytes).
 * Devolve 0 ou um erro negativo de pack_multi.
 */
int res_rr_pixels(res_rr_t *r, int i, int q, unsigned char *dst);

/* RAM que os pacotes do rank custam (tabelas + um bloco por parte). */
unsigned int res_rr_ram(const res_rr_t *r);

/* Indice do rank na tabela (`0..RES_RR_RANKS-1`), a partir do rank do jogo
 * (`fnf_rank_t`: 0 = SHIT ... 5 = PERFECT_GOLD). -1 se o rank nao existir. */
int res_rr_indice(int rank);

/* Tabela do rank `idx` (0..RES_RR_RANKS-1) ou NULL. */
const res_rr_rank_t *res_rr_rank(int idx);

#ifdef __cplusplus
}
#endif

#endif /* FNF_RESULTADO_RANK_H */
