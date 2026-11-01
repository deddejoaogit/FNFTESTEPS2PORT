/* char_cena - o estado de UM personagem em cena, por papel
 *
 * POR QUE ISTO EXISTE
 * `char_anim.c` sabe QUAL QUADRO de uma sequencia mostrar, e `char_pack.c` sabe
 * LER os quadros de um personagem. Falta a ligacao entre os dois, que e' uma
 * maquina de estado por personagem:
 *
 *   - quando ele esta' parado, dança (idle, ou danceLeft/danceRight pela batida);
 *   - quando chega uma nota do lado dele, canta por `singTime` passos e volta a
 *     dançar;
 *   - erro de nota usa a animacao `...miss`, e quem nao tem cai na normal.
 *
 * E, principalmente: QUANDO REENVIAR A TEXTURA. O custo real do port nao e'
 * decidir o quadro, e' decodificar LZ4 e subir o quadro para a VRAM. Por isso
 * `cc_quadro()` devolve o quadro novo so' quando ele MUDOU (ou -1), em vez de
 * um numero por quadro para o chamador comparar.
 *
 * Aqui nao ha GS, arquivo nem VRAM: o pacote ja' vem pronto do chamador. Roda no
 * PC, com teste.
 *
 * DE ONDE VEM CADA REGRA
 * - idle/danceLeft/danceRight pela batida: Bopper.dance() + IdleCharacter.
 * - canto dura singTime passos: BaseCharacter / PlayState.
 * - nome repetido vale o ULTIMO: FlxAnimationController._animations.set.
 * - "miss" cai no nome-base quando nao existe: troca de sufixo do BaseCharacter.
 */
#ifndef FNF_CHAR_CENA_H
#define FNF_CHAR_CENA_H

#include "char_pack.h"
#include "char_layout.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int papel;              /* CH_PAPEL_*                                  */
    int ativo;              /* 1 = pacote aberto e conferido               */
    cp_pack_t pacote;
    int anim;               /* indice da animacao no pacote; -1 = nenhuma  */
    int quadro;             /* ultimo quadro devolvido; -1 = nada desenhado */
    long t0;                /* ms em que a animacao atual comecou          */
    long canto_ate;         /* ms em que o canto acaba; 0 = nao canta       */
    int especial;           /* tocando animacao nomeada (hey, cheer...)     */
    int ultimo_passo;       /* em que passo o dance() rodou por ultimo      */
    float danca_cada;       /* danceEvery em BATIDAS (1.0 = toda batida)    */
    int dancas;             /* quantas dancas ja' jogou (`hasDanced`)       */
    int pulos;              /* passos de danca perdidos por estar cantando  */
} cc_papel_t;

typedef struct {
    cc_papel_t p[CH_CENA_PAPEIS];   /* indice = CH_PAPEL_*                */
} cc_cena_t;

void cc_reset(cc_cena_t *c);

/* Anexa o pacote de um personagem ao papel. Devolve 0, ou o erro do
   `cp_abrir`/`cp_conferir` -- e nesse caso o papel fica INATIVO: sem pacote
   conferido nao se desenha nada, em vez de desenhar lixo. */
int cc_anexar(cc_cena_t *c, int papel, const unsigned char *pacote,
              unsigned int bytes, long agora);

/* Chegou nota do lado desse personagem: canta na direcao `dir` (0..3) por
   `sing_ms`. `missing` escolhe a animacao de erro quando ela existe. */
void cc_canto(cc_cena_t *c, int papel, int dir, int missing, long agora, long sing_ms);

/* Toca uma animacao NOMEADA (evento PlayAnimation: "hey", "cheer", "scared"...).
   Enquanto ela nao termina, o personagem NAO volta a dancar -- e' a regra do
   BaseCharacter.dance() L479-497: quem esta' numa animacao que nao comeca com
   "dance"/"idle" e nao terminou nao e' interrompido. Devolve 1 se tocou, 0 se o
   personagem nao tem essa animacao (aí o evento vira log, nao invencao). */
int cc_toca(cc_cena_t *c, int papel, const char *nome, long agora);

/* danceEvery do personagem do papel, em BATIDAS (ch_dados_t.dance). Chamado
   depois de `cc_anexar`; sem ele vale 1.0, que e' o DEFAULT_DANCEEVERY. */
void cc_danca_cada(cc_cena_t *c, int papel, float cada);

/* COMBO e DROP das personagens do tipo GF (BaseCharacter.hx L586-600, L651-681).
   `combo<N>` toca quando o jogador ACERTA a nota N de uma sequencia (o
   `event.comboCount` do original e' `tallies.combo + 1`, ou seja o combo ja'
   contando esta nota) e so' existe para os valores que o personagem tem --
   medido nos dados: combo50 e combo200 nas GF/nene, e so' isso.
   `drop<N>` toca quando o combo quebra: vale o MAIOR N que o combo ainda
   alcanca (drop70 nas 8 personagens que tem drop), e nenhum se o combo e'
   menor que todos.
   As duas devolvem 1 se tocaram uma animacao e 0 se o personagem nao tem
   nenhuma para aquele numero -- nada e' inventado, o silencio e' o dado. */
int cc_combo(cc_cena_t *c, int papel, int combo, long agora);
int cc_drop(cc_cena_t *c, int papel, int combo, long agora);

/* O quadro a desenhar AGORA, ou -1 quando nada mudou. `passo` e' o passo do
   chart (1/4 de batida): e' ele que decide QUANDO dancar -- a cada
   `danca_cada * 4` passos, como o `onStepHit` do Bopper -- e o lado alterna por
   quantas dancas ja' foram jogadas, nao pela paridade do passo. */
int cc_quadro(cc_cena_t *c, int papel, long agora, int passo);

/* A animacao atual (indice no pacote), -1 se nenhuma. Para o log. */
int cc_anim(const cc_cena_t *c, int papel);

#ifdef __cplusplus
}
#endif
#endif /* FNF_CHAR_CENA_H */
