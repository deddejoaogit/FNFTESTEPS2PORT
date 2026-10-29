#ifndef AUDIO_RING_H
#define AUDIO_RING_H
int audio_ring_start(int fd);
int audio_ring_start_loop(int fd, int loop);
int audio_ring_take(void *dst, int bytes);

/*
 * Tira o que HOUVER no anel, ate `bytes`, e devolve quantos bytes saíram.
 *
 * Existe por causa do fim de arquivo: a fonte le em blocos de READ_SIZE, e o
 * ultimo bloco de um arquivo que nao e' multiplo de READ_SIZE deixa um resto no
 * anel que 'audio_ring_take' nunca entrega (ela so' entrega o tamanho exato
 * pedido, para nao quebrar o envio ao audsrv). Sem esta funcao aquele resto
 * ficava preso para sempre, `audio_ring_eof()` era verdadeiro, o anel nunca
 * esvaziava -- e a musica NUNCA terminava.
 *
 * Foi medido, nao deduzido: a pausa deixou o audio com 1.280 bytes presos no
 * anel (o resto de 24.249.600 depois de retomar a 1.555.200), o jogo tratou o
 * resto como "bloco curto" e o resultado da musica nunca apareceu.
 */
int audio_ring_take_available(void *dst, int bytes);
int audio_ring_level(void);
int audio_ring_level_snapshot(void);
void audio_ring_stats(unsigned int *produced, unsigned int *reads, unsigned int *loops, unsigned int *errors, unsigned int *underruns);
void audio_ring_stop(void);
void audio_ring_pause(int p);
int audio_ring_eof(void);
int audio_ring_reset(void);
int audio_ring_switch(int fd, int loop);
void audio_ring_debug(int *fd, unsigned int *loop, unsigned int *switches, unsigned int *reset_pending, unsigned int *switch_pending);
#endif
