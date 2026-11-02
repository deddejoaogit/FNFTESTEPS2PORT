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

/* ---------------------------------------------------------------------------
 * O PREVIEW DA FREEPLAY E AS FONTES SECUNDARIAS SIMULTANEAS
 *
 * `audio_ring_iniciar_preview` toca um TRECHO de um `stems.pcm` (4 canais, 8
 * bytes por amostra) em laco, como o `partialParams` do `playCurSongPreview`
 * (`FreeplayState.hx` L3005-3023): so' o instrumental (os dois canais de voz
 * ficam de fora), com o `fadeIn(2)`/`fadeOut(2)` a 0,7 de volume aplicado na
 * propria thread. O ganho e' aplicado ANTES da soma dos overlays, porque os
 * jingles do rank sao `playOnce` a volume 1 e nao podem ser atenuados junto.
 *
 * `ini` e `fim` sao BYTES (ver `fp_aud_janela` em engine/freeplay_audio.h).
 *
 * O fd passa a ser da thread. Contrato:
 *   - `audio_ring_iniciar_preview`: depois de `audio_ring_stop()`, o chamador
 *     fecha o fd;
 *   - `audio_ring_preview_trocar`: o fd NOVO passa a ser da thread e o ANTERIOR
 *     e' fechado por ela -- o chamador nao fecha nada.
 * (O comeco da cinematica de rank, com `music.volume = 0`, nao precisa disto:
 * ele usa `audio_ring_start_overlay(-1)`, que ja' e' silencio + overlays.)
 */
int audio_ring_iniciar_preview(int fd, long ini, long fim);
int audio_ring_preview_trocar(int fd, long ini, long fim);

/* Fontes secundarias SIMULTANEAS, cada uma em seu slot. O original pode ter
 * varios `playOnce` tocando juntos -- o jingle do `rankDisplayNew` (0,585 a
 * 1,004 s) ainda esta' no ar quando o do slam entra, 0,5 s depois.
 * `audio_ring_overlay` (alocador automatico) devolve o slot ou -1 quando nao ha'
 * slot livre nem thread; ele aloca do ULTIMO slot para baixo e nunca toca nos
 * slots 0..2, que sao fixos dos `playOnce` de menu (`audio_ring_overlay_slot`).
 * Sao 8 slots: 3 fixos + 5 automaticos. O fd passa a ser da thread;
 * `audio_ring_stop()` fecha. `audio_ring_overlay_parar` devolve o slot ao
 * silencio (o fd e' fechado no `stop`, que e' seguro: quem le e' a thread). */
int audio_ring_overlay(int fd);
void audio_ring_overlay_parar(int slot);

/* O MESMO, mas com slot FIXO e VOLUME por cento. Existe pelos tres `playOnce`
 * de menu da Freeplay (`scrollMenu` a 0,4 -- `FreeplayState.hx` L2918 e L2388 --
 * e `confirmMenu`/`cancelMenu` a 1,0 -- L2764 e L2240), que reaproveitam o fd: o
 * som e' sempre o mesmo arquivo, entao o slot e' do SOM e nao da vez. O fd
 * continua sendo da thread (ela o fecha no `audio_ring_stop`); o slot tem de ser
 * 0..3 e nao pode ser um que outra tela esteja usando com outro fd.
 *
 * `vol_pct` vale para o bloco inteiro lido (`amix_volume`), e e' o volume do
 * `FlxSound` do original -- 1,0 = 100. Reatribuir um slot FECHA o fd anterior
 * dele (o slot trocou de dono).
 *
 * Devolve 0, ou -1 sem thread (ai' o CHAMADOR continua dono do fd). */
int audio_ring_overlay_slot(int slot, int fd, int vol_pct);
int audio_ring_overlay_vol(int fd, int vol_pct);

/* GERACAO DO ANEL: sobe a cada `audio_ring_stop`/inicio. Quem guarda um fd para
 * reusar (os `playOnce` de menu) compara a geracao: se mudou, o anel fechou os
 * fds na parada e o que estava guardado nao vale mais. */
unsigned int audio_ring_geracao(void);
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
