/* Mixer de duas faixas: instrumental + vozes.
 *
 * O original toca dois `FlxSound` (o `Inst` e o `Voices` da musica) e o mixer do
 * flixel soma os dois no buffer de saida. O port faz a mesma soma, mas as duas
 * faixas chegam no MESMO arquivo (`songs/<id>/stems.pcm`), com 4 canais de
 * 16 bits intercalados:
 *
 *     [instL, instR, voicesL, voicesR]     -> 8 bytes por amostra
 *
 * Somar duas faixas de 16 bits estoura 16 bits, e e' por isso que este modulo
 * existe separado: a saturacao em +-32767/+-32768 tem de ser a mesma em todo
 * caminho, e o tamanho do bloco de saida (metade do de entrada: 4 canais viram
 * 2) ja' foi errado uma vez -- o audio tocava no dobro da velocidade e o relogio
 * da musica ia junto. Aqui as duas contas ficam num lugar so', com teste.
 */
#ifndef FNF_AUDIO_MIX_H
#define FNF_AUDIO_MIX_H

#include <stddef.h>

#define AMIX_CANAIS_ENTRADA 4   /* instL, instR, voicesL, voicesR */
#define AMIX_CANAIS_SAIDA   2   /* estereo                       */

/* Soma `amostras` amostras (8 bytes cada na entrada) em `saida` (4 bytes cada).
 * `saida` pode ser o mesmo buffer de `entrada` (o mixer do main.c mistura no
 * lugar): a escrita de cada amostra usa indices menores que os de leitura.
 * Com `vozes` = 0 o instrumental passa sozinho.
 * Devolve o numero de amostras escritas (= `amostras`, 0 se algum ponteiro for
 * nulo ou `amostras` <= 0). */
int amix_somar(const short *entrada, short *saida, int amostras, int vozes);

/* Bytes de saida para `amostras` amostras: AMIX_CANAIS_SAIDA * 2 por amostra. */
int amix_bytes_saida(int amostras);

#endif /* FNF_AUDIO_MIX_H */
