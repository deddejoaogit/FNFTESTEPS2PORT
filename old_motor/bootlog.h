/* bootlog.h - marcos de boot, batimento por quadro e cao de guarda
 *
 * POR QUE ISTO EXISTE
 * O jogo trava no console depois do primeiro texto e o arquivo de boot
 * para no "passo 66: FRAME". O mesmo codigo passa de 1400 quadros no PC.
 * Quando o defeito e de hardware que o PC nao imita -- audio, pendrive,
 * sincronismo de tela, temporizacao do processador de I/O -- a unica
 * coisa que se pode fazer com eficiencia e aumentar a RESOLUCAO do
 * instrumento: dizer nao so o passo, mas a FASE dentro do quadro.
 *
 * Este modulo grava:
 *   1. marcos nomeados, com passo automatico, ESCRITOS NA HORA (write
 *      through). Se o console morrer no marco seguinte, o anterior ja
 *      esta no pendrive.
 *   2. a fase corrente do quadro em RAM, atualizada varias vezes por
 *      quadro. Se travar no meio de um quadro, o proximo boot le a fase
 *      em que morreu (o arquivo de fase e regravado a cada troca, 4
 *      bytes, custo desprezivel, e o sistema de arquivos do PS2 nao
 *      perde a ultima gravacao de um arquivo pequeno).
 *   3. deteccao de morte anterior: se o arquivo nao tem "SHUTDOWN", o
 *      boot seguinte marca CICLO_SEM_FIM e mostra onde parou.
 *
 * O modulo nao inclui nada de PS2: a escrita entra por callback, entao o
 * teste roda no PC com um arquivo de verdade.
 */
#ifndef BOOTLOG_H
#define BOOTLOG_H

#ifdef __cplusplus
extern "C" {
#endif

/* Fases de um quadro. A ordem e a ordem real de execucao do laco do
   main.c, para que a fase gravada aponte a LINHA de codigo. */
typedef enum {
    BF_INICIO = 0,
    BF_PAD,           /* padPortRead / poll */
    BF_AUDIO_CAUDAL,  /* audsrv_queued / audsrv_available */
    BF_AUDIO_ENVIO,   /* audsrv_play_audio / ring take */
    BF_AUDIO_LEITURA, /* read() do pcm no pendrive */
    BF_CHART,         /* eventos e notas */
    BF_ESTADO,        /* mudanca de tela */
    BF_DESENHO,       /* draw_rect_textured / lista */
    BF_CAPTURA,       /* captura de framebuffer */
    BF_VSYNC,         /* graph_wait_vsync */
    BF_TROCA_FB,      /* graph_set_framebuffer */
    BF_TELEMETRIA,
    BF_FIM,
    BF_FASE_COUNT
} bootlog_fase_t;

#define BOOTLOG_MARCOS 256

typedef struct {
    int  passo;
    char nome[40];
    int  valor;
} bootlog_marco_t;

/* Escritor: recebe uma linha completa (sem \n) e deve grava-la. */
typedef void (*bootlog_escritor_fn)(void *ctx, const char *linha);

typedef struct {
    bootlog_escritor_fn escritor;
    void  *ctx;
    int    passo;
    int    verbose;
    unsigned int quadro;
    bootlog_fase_t fase;
    /* anel dos ultimos marcos: sobrevive a falha de escrita do sistema
       de arquivos, e no PC e o que o teste inspeciona. */
    bootlog_marco_t marcos[BOOTLOG_MARCOS];
    int    n_marcos;
    int    morto_sem_fim;      /* achou o log anterior sem SHUTDOWN */
    int    fase_anterior;
    unsigned int quadro_anterior;
    /* cao de guarda: se um quadro passar de 'limite_ms', grava ALERTA. */
    unsigned int limite_ms;
    unsigned int ms_inicio_quadro;
    unsigned int alertas;
} bootlog_t;

void bootlog_init(bootlog_t *b, bootlog_escritor_fn escritor, void *ctx, int verbose);
/* Le o arquivo anterior (pelo mesmo escritor nao e possivel; o chamador
   informa o texto lido) e preenche morto_sem_fim/quadro_anterior. */
void bootlog_analisa_anterior(bootlog_t *b, const char *conteudo);

/* Marco numerado e escrito na hora. Devolve o numero do passo. */
int  bootlog_marco(bootlog_t *b, const char *nome, int valor);
/* Marco sem incrementar o passo (para marcos de fase). */
void bootlog_nota(bootlog_t *b, const char *nome, int valor);

void bootlog_quadro_inicio(bootlog_t *b, unsigned int quadro);
/* Troca de fase dentro do quadro. Grava so quando muda. */
void bootlog_fase(bootlog_t *b, bootlog_fase_t fase);
void bootlog_quadro_fim(bootlog_t *b, unsigned int ms_gasto);
/* Cao de guarda: chamar quando ms_gasto estourar o limite. */
void bootlog_alerta(bootlog_t *b, const char *motivo, int valor);

const char *bootlog_fase_nome(int fase);
/* Ultimo marco registrado, para gravar no arquivo ao dar erro. */
const bootlog_marco_t *bootlog_ultimo(const bootlog_t *b);

#ifdef __cplusplus
}
#endif
#endif
