/* save - o recorde por musica e dificuldade (o `Save` do original, o minimo)
 *
 * POR QUE ESTE ARQUIVO EXISTE
 * `ResultState.hx` L415-426 so' desenha o selo `highscoreNew` quando
 * `params.isNewHighscore` e' verdadeiro, e esse valor vem de
 * `Save.instance.isSongHighScore(...)` (`PlayState.hx` L3503). Sem save, esse
 * campo e' DADO AUSENTE e o selo nunca aparece -- e' o que o port fazia ate'
 * aqui. O que falta para o selo existir nao e' desenho: e' o dado.
 *
 * O QUE ELE GUARDA, E COM QUE REGRA (tudo com a linha da fonte)
 *   `SaveScoreData` = `score` + `tallies` (sick/good/bad/shit/missed/combo/
 *   maxCombo/totalNotesHit/totalNotes). Sao os mesmos numeros que
 *   `engine/gameplay.c` ja' conta durante a musica -- o save nao recalcula
 *   nada, ele guarda.
 *
 *   `isSongHighScore` (Save.hx L568-582): sem registro anterior -> `true`;
 *   senao `score > anterior.score`.
 *   `applySongRank` (Save.hx L531-556): sem registro anterior -> grava; senao
 *   grava o MAIOR placar e os tallies do MELHOR rank (ou, no empate de rank, os
 *   do maior `tallyCompletion`).
 *   O rank sai de `Scoring.calculateRank`, que ja' esta' em `fnf_play_rank`
 *   (mesma formula: SICK em tudo = PERFECT_GOLD, senao o percentual).
 *
 * O QUE ELE NAO FAZ
 *  - nao abre arquivo. Quem grava em memory card e' o `main.c` (o mesmo
 *    `open()`/`read()`/`write()` que ja' le o pendrive); aqui so' ha' modelo e
 *    formato. Assim a regra inteira roda no PC, com teste, e o console so'
 *    precisa acertar o caminho.
 *  - nao inventa dado: registro invalido e' tratado como "nunca jogou" (o
 *    mesmo que o original faz com `getSongScore` nulo), e um arquivo com
 *    cabecalho/CRC errado NAO e' meio lido -- `save_desserializar` recusa
 *    inteiro e devolve erro.
 *
 * C puro: sem GS, sem I/O. Roda no PC (`test/test_save.c`).
 */
#ifndef FNF_SAVE_H
#define FNF_SAVE_H

#ifdef __cplusplus
extern "C" {
#endif

/* 27 musicas (as do `song_registry`) x 3 dificuldades (easy/normal/hard). */
#define SAVE_MUSICAS 27
#define SAVE_DIFS     3

/* 24 bytes por registro no arquivo: 4 de placar + 9 de tallies de 16 bits
 * (`missed`, `combo` e `maxCombo` cabem: o maior combo de um chart do jogo e'
 * 1.400, e o placar maximo e' ~350.000). */
typedef struct {
    unsigned int   score;
    unsigned short sick, good, bad, shit, missed;
    unsigned short combo, max_combo, total_hit, total_notes;
    unsigned char  valido;      /* 1 = existe registro para (musica, dif) */
} save_score_t;

typedef struct {
    save_score_t s[SAVE_MUSICAS][SAVE_DIFS];
    int carregado;              /* 1 = veio de arquivo conferido          */
    int sujo;                   /* 1 = mudou desde a ultima gravacao      */
} save_t;

/* Zera tudo: "nunca jogou nenhuma musica". */
void save_init(save_t *sv);

/* O registro de (musica, dif), ou NULL se nunca jogou / indice invalido. */
const save_score_t *save_obter(const save_t *sv, int musica, int dif);

/* `Save.isSongHighScore`: 1 quando NAO ha' registro anterior (o original
 * devolve true nesse caso) ou quando `score` e' maior que o guardado. */
int save_novo_recorde(const save_t *sv, int musica, int dif, unsigned int score);

/* `Save.applySongRank`: grava o melhor placar e os tallies do melhor rank. */
void save_aplicar(save_t *sv, int musica, int dif, const save_score_t *novo);

/* Rank (0..5, os mesmos de `fnf_rank_t`) do registro, ou -1 se nao houver. */
int save_rank(const save_score_t *r);

/* ---- formato -------------------------------------------------------------
 * Cabecalho de 16 bytes: "FNFS" + versao (u16) + registros (u16) + CRC32 do
 * corpo. Corpo: SAVE_MUSICAS*SAVE_DIFS registros de 24 bytes, na ordem
 * musica*3 + dif. Total: 16 + 81*24 = 1960 bytes (cabe em um bloco de memory
 * card, que e' 8 KB). */

#define SAVE_ARQUIVO_BYTES (16 + SAVE_MUSICAS * SAVE_DIFS * 24)

/* Escreve o arquivo em `dst` (SAVE_ARQUIVO_BYTES). Devolve os bytes escritos. */
unsigned int save_serializar(const save_t *sv, unsigned char *dst);

/* Le' `src` em `sv`. Devolve 0 ou um erro negativo; em erro, `sv` fica como
 * estava (nada meio lido). */
#define SAVE_ERR_CURTO   (-1)   /* menos bytes que o cabecalho               */
#define SAVE_ERR_MAGIC   (-2)   /* nao e' "FNFS"                            */
#define SAVE_ERR_VERSAO  (-3)   /* versao desconhecida                      */
#define SAVE_ERR_CONTAGEM (-4)  /* numero de registros diferente do esperado */
#define SAVE_ERR_CRC     (-5)   /* corpo corrompido                         */
int save_desserializar(save_t *sv, const unsigned char *src, unsigned int bytes);

/* CRC32 (polinomio 0xEDB88320, o mesmo do zlib) -- usado no cabecalho. */
unsigned int save_crc32(const unsigned char *src, unsigned int bytes);

#ifdef __cplusplus
}
#endif

#endif /* FNF_SAVE_H */
