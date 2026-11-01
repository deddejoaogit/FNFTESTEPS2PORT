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

/*
 * Soma um bloco ESTEREO intercalado (L,R) de 16 bits sobre outro, no lugar.
 *
 * Existe por causa da tela de game over: ali tocam DOIS sons ao mesmo tempo --
 * a musica do game over e o `fnf_loss_sfx` (ou o `fakeout_death`) por cima. O
 * mixer do flixel soma as duas fontes na saida, e o port tem de fazer o mesmo.
 * No console o `audio_ring` faz essa soma na thread produtora, que e' a UNICA
 * que toca o pendrive (o usbhdfsd nao e' reentrante: duas leituras simultaneas
 * corrompem o driver -- foi medido antes com a captura de tela).
 *
 * `dst` e `src` sao os dois PCM crus do pendrive: 48000 Hz, estereo, s16 LE.
 * A saturacao tem de ser a mesma de `amix_somar` (e' a mesma parede de 16 bits).
 * Devolve o numero de AMOSTRAS (pares L,R) somadas, 0 com ponteiro nulo ou
 * contagem <= 0.
 */
int amix_somar_estereo(short *dst, const short *src, int amostras);

#endif /* FNF_AUDIO_MIX_H */
