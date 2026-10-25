/* io_fila - fila unica e fatiada para o acesso ao pendrive
 *
 * POR QUE ISTO EXISTE
 * O usbhdfsd do PS2 nao e reentrante. Este projeto perdeu dois ciclos
 * para isso:
 *   0.2.64  a captura de tela gravava 1.146.880 B pela thread principal
 *           enquanto a thread de audio lia a musica em blocos de 32 KB.
 *           O driver corrompia e a musica e as animacoes paravam DE VEZ.
 *   0.2.74  serializei com uma pausa: parou de corromper, mas o audio
 *           ficava mudo durante a gravacao inteira.
 *
 * Serializar resolve a corrupcao e cria um segundo problema: quem
 * precisa de pouco e sempre espera quem precisa de muito. A solucao e
 * FATIAR. Nenhuma operacao segura o dispositivo por mais de um pedaco;
 * entre pedacos, quem tem prioridade maior passa na frente.
 *
 * Com fatia de 32 KB e 37 MB/s medidos, cada pedaco prende o
 * dispositivo por cerca de 0,9 ms. Uma gravacao de 1,1 MB vira 35
 * pedacos e o audio se intercala entre eles, em vez de esperar 31 ms.
 *
 * O modulo e C puro e a camada de disco entra por ponteiros de funcao,
 * para rodar dentro de um teste no PC antes de ir para o console.
 */
#ifndef IO_FILA_H
#define IO_FILA_H

#ifdef __cplusplus
extern "C" {
#endif

#define IO_MAX_PEDIDOS 16
#define IO_FATIA 32768u        /* mesmo tamanho do bloco do anel de audio */

enum { IO_LER = 0, IO_ESCREVER = 1 };
/* Prioridades. Menor numero passa primeiro. */
enum { IO_PRIO_AUDIO = 0, IO_PRIO_NORMAL = 1, IO_PRIO_FUNDO = 2 };

typedef struct {
    int tipo, fd, prio;
    unsigned char *buf;
    unsigned int bytes, feitos;
    int ativo;                 /* 1 = na fila */
    volatile int pronto;       /* 1 = terminou; o dono poe, o cliente le */
    int resultado;             /* bytes transferidos, ou negativo em erro */
    unsigned int selo;         /* ordem de chegada, desempata prioridade igual */
} io_pedido_t;

typedef struct {
    io_pedido_t p[IO_MAX_PEDIDOS];
    unsigned int relogio;
    /* camada de disco, trocavel para o teste */
    int (*ler)(int fd, void *buf, unsigned int n);
    int (*escrever)(int fd, const void *buf, unsigned int n);
    /* contadores */
    unsigned int fatias, trocas_de_dono, maior_espera;
} io_fila_t;

void io_init(io_fila_t *f,
             int (*ler)(int, void *, unsigned int),
             int (*escrever)(int, const void *, unsigned int));

/* Enfileira. Devolve o indice do pedido, ou -1 se a fila estiver cheia. */
int io_enfileirar(io_fila_t *f, int tipo, int fd, void *buf,
                  unsigned int bytes, int prio);

/* Executa UMA fatia do pedido mais prioritario. Devolve 1 se fez algo,
 * 0 se nao havia nada a fazer. E esta funcao que a thread dona do
 * dispositivo chama em laco. */
int io_bombear(io_fila_t *f);

int io_terminado(const io_fila_t *f, int id);
int io_resultado(io_fila_t *f, int id);   /* colhe e libera o pedido */
int io_pendentes(const io_fila_t *f);

#ifdef __cplusplus
}
#endif
#endif
