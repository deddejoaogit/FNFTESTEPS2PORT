/* Um KERNEL MINIMO DE PC, so' para o teste do anel de audio.
 *
 * POR QUE ESTE ARQUIVO EXISTE
 *
 * `src/audio_ring.c` e' o unico modulo do jogo que roda em THREAD, e a parte
 * nova dele -- a JANELA do preview -- so' pode ser provada com a thread de
 * verdade lendo um arquivo de verdade: a volta para o comeco da janela e' um
 * `lseek` dentro do produtor, e a mistura dos 4 canais em 2 com o ganho do
 * fade acontece ANTES de o bloco entrar no anel.
 *
 * O harness (tools/run_harness.sh) roda a mesma thread, mas com o `kernel.h`
 * do ps2sdk e o `DelayThread` do `test/ps2harness/ps2h.c` -- que avanca um
 * relogio VIRTUAL e e' ligado ao binario do jogo inteiro. Aqui basta o minimo:
 * semaforo binario e thread, implementados em pthread e usados so' por
 * `test/test_audio_ring.c`.
 *
 * O TIPO DE `ee_thread_t.func`: o ps2sdk declara `void *func` e o
 * `audio_ring.c` faz `t.func = producer` (uma funcao `void producer(void*)`).
 * Com o tipo `void (*)(void*)` aqui, a atribuicao continua valida e nao ha'
 * cast nenhum no jogo.
 */
#ifndef FNF_SHIM_KERNEL_H
#define FNF_SHIM_KERNEL_H

#include <tamtypes.h>

typedef struct {
    int init_count;
    int max_count;
    int option;
} ee_sema_t;

typedef struct {
    void (*func)(void *);
    void *stack;
    int stack_size;
    void *gp_reg;
    int initial_priority;
} ee_thread_t;

int CreateSema(ee_sema_t *s);
int DeleteSema(int id);
int WaitSema(int id);
int SignalSema(int id);
int CreateThread(ee_thread_t *t);
int DeleteThread(int id);
int StartThread(int id, void *arg);
int TerminateThread(int id);
void ExitThread(void);

#endif /* FNF_SHIM_KERNEL_H */
