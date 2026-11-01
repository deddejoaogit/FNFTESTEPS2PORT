#ifndef AUDIO_RING_H
#define AUDIO_RING_H
int audio_ring_start(int fd);
int audio_ring_start_loop(int fd, int loop);
int audio_ring_take(void *dst, int bytes);

/* ---------------------------------------------------------------------------
 * FONTE SECUNDARIA (overlay) E FONTE AUSENTE (silencio)
 *
 * A tela de game over toca DOIS sons ao mesmo tempo: a musica do game over e o
 * `fnf_loss_sfx` (ou o `fakeout_death`) por cima -- e' o que o mixer do flixel
 * faz com os dois `FlxSound` do original. No port nao ha' mixer de saida: ha'
 * um anel e um `audsrv`. A soma tem de acontecer ANTES do anel, e tem de ser
 * feita por UMA thread so': o usbhdfsd nao e' reentrante e duas leituras
 * simultaneas do pendrive corrompem o driver (medido antes, com a captura de
 * tela). Por isso a fonte secundaria mora DENTRO desta thread, e nao num anel
 * paralelo.
 *
 * `audio_ring_start_overlay` comeca SEM fonte primaria: o produtor escreve
 * silencio e soma o overlay. E' o comeco do game over, quando o original ainda
 * nao comecou a musica (ela so' entra quando o `firstDeath` termina).
 *
 * `audio_ring_trocar_fonte` troca a primaria PRESERVANDO o nivel do anel e o
 * overlay: e' assim que a musica do game over entra por baixo do sfx que ainda
 * esta' tocando.
 */
int audio_ring_start_overlay(int overlay_fd);
int audio_ring_trocar_fonte(int fd, int loop);
int audio_ring_trocar_overlay(int fd);
void audio_ring_parar_overlay(void);
/* 1 = nao ha' fonte primaria (silencio + overlay). */
int audio_ring_sem_fonte(void);

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
 * resto como \"bloco curto\" e o resultado da musica nunca apareceu.
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
