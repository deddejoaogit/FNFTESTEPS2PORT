/* Harness de link + execucao para o FNF PS2.
 *
 * O que este modulo faz: fornece, no PC, tudo que o jogo pede ao ps2sdk --
 * packet, dma, draw (o codigo REAL do ps2sdk, compilado aqui), graph, pad,
 * audsrv, SifRpc, timer -- e executa o main.c de verdade, quadro a quadro.
 *
 * A peca central eh o PACOTE LIMITADO. packet_init(n) reserva n quadwords
 * MAIS uma zona de guarda cheia de veneno. Toda vez que o jogo manda o
 * pacote para o DMA, este modulo confere:
 *
 *   1. a contagem de quadwords cabe no pacote que foi pedido?
 *   2. a zona de guarda continua intacta (ou seja, ninguem escreveu alem)?
 *
 * Se qualquer uma falhar, o harness ABORTA na hora, com o numero do quadro,
 * o tamanho pedido, o tamanho real e o simbolo culpado. No PS2 esse mesmo
 * estouro nao morre: ele escreve por cima da memoria vizinha e o sintoma
 * aparece muito depois, em outro lugar. Eh por isso que o harness existe.
 *
 * Uso:  tools/run_harness.sh [quadros] [raiz-dos-assets]
 * Variaveis: PS2H_FRAMES, PS2H_ROOT, PS2H_STRICT
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <time.h>
#include <stdint.h>
#include <signal.h>
#include <execinfo.h>

/* Os tipos vem do tamtypes.h REAL do ps2sdk. Redefini-los aqui daria conflito
   com o que os headers do ps2sdk ja declararam -- e o conflito silenciaria
   justamente os avisos de tipo que este harness existe para capturar. */
#include <tamtypes.h>
#include <packet.h>   /* packet_t: a estrutura real, nao uma copia */
#include <malloc.h>   /* memalign: prototipo real, nao um implicito */
#include <sys/stat.h>  /* fstat: distinguir leitura curta legitima de anomalia */
#include <kernel.h>   /* ee_thread_t: o CreateThread recebe este descritor */
#include <libpad.h>   /* struct padButtonStatus REAL. Escrever no offset errado aqui
                          fabricava START+SELECT e o jogo saia no quadro 1. */

#ifndef AT_FDCWD
#define AT_FDCWD (-100)
#endif

/* ------------------------------------------------------------------------- */
/*  Configuracao                                                             */
/* ------------------------------------------------------------------------- */

#define PS2H_GUARD_QWORDS 64u          /* zona de veneno depois do limite   */
#define PS2H_MAX_LIVE     16           /* pacotes vivos ao mesmo tempo      */
#define PS2H_POISON0      0xDEADBEEFCAFEBABEull
#define PS2H_POISON1      0x0123456789ABCDEFull

static unsigned ps2h_frames = 240;     /* quadros antes de pedir saida      */
static unsigned ps2h_qwcap = 0;        /* teto artificial do pacote (teste negativo) */
static unsigned ps2h_force_qwc = 0;     /* qwc artificial (teste negativo do QWC_MAX) */
static const char *ps2h_root = "assets";
static int ps2h_strict = 1;
static int ps2h_ready = 0;

/* ------------------------------------------------------------------------- */
/*  Relatorio                                                                 */
/* ------------------------------------------------------------------------- */

static u32 h_frames_run;
static u32 h_packets_made, h_packets_freed;
static u32 h_sends_normal, h_sends_chain;
static u32 h_qwords_max;               /* maior pacote ja construido        */
static u32 h_qwords_limit_max;
static u32 h_overflows;                /* estouros detectados               */
static u32 h_guard_hits;               /* escritas na zona de veneno        */
static u32 h_oversize_sends;           /* DMA acima de QWC_MAX              */
static u32 h_io_opens, h_io_fail_opens, h_io_reads, h_io_short_reads;

/* Caminhos que nao abriram. Um "5 arquivos nao abriram" sem dizer QUAIS
   obriga a adivinhar; com a lista, o relatorio aponta o asset que falta. */
#define H_FAIL_CAP 32
static char h_fail_path[H_FAIL_CAP][256];
static unsigned h_fail_n;

static void h_note_fail(const char *p)
{
    unsigned i;
    if (!p) return;
    for (i = 0; i < h_fail_n; i++)
        if (!strcmp(h_fail_path[i], p)) return;   /* nao repete o mesmo caminho */
    if (h_fail_n < H_FAIL_CAP) {
        snprintf(h_fail_path[h_fail_n], sizeof h_fail_path[0], "%s", p);
        h_fail_n++;
    }
}

static u32 h_audio_submitted, h_audio_available_calls;
static u32 h_upload_calls, h_upload_bytes;
static u32 h_draw_filled, h_draw_textured, h_draw_env;
typedef struct { unsigned frame; unsigned short btns; } ps2h_input_t;

#define PS2H_MAX_INPUT 64
static ps2h_input_t ps2h_input[PS2H_MAX_INPUT];
static unsigned ps2h_input_n;
static unsigned h_input_fired;


static u32 h_draw_calls;
static int  h_quit_requested;
unsigned ps2h_quit_frame = 0;
static unsigned ps2h_watch_secs = 0;

static void ps2h_band_report(void);

static void ps2h_report(void)
{
    fprintf(stderr,
        "\n================ RELATORIO DO HARNESS ================\n"
        "quadros executados ............ %u\n"
        "pacotes criados / libertados .. %u / %u\n"
        "envios DMA (normal/chain) ..... %u / %u\n"
        "maior pacote construido ....... %u quadwords (limite pedido %u)\n"
        "ESTOUROS DE PACOTE ............ %u\n"
        "escritas na zona de guarda .... %u\n"
        "envios DMA acima de QWC_MAX ... %u\n"
        "uploads de textura ........... %u chamadas, %u bytes\n"
        "setup_environment ............ %u\n"
        "chamadas de desenho .......... %u  (preenchidos %u, texturizados %u)\n"
        "E/S: opens/falhas/leituras ... %u / %u / %u (leituras curtas %u)\n"
        "botoes do roteiro executados . %u de %u\n"
        "audio: amostras submetidas ... %u\n"
        "======================================================\n",
        h_frames_run, h_packets_made, h_packets_freed,
        h_sends_normal, h_sends_chain,
        h_qwords_max, h_qwords_limit_max,
        h_overflows, h_guard_hits, h_oversize_sends,
        h_upload_calls, h_upload_bytes,
        h_draw_env, (unsigned)(h_draw_filled + h_draw_textured), h_draw_filled, h_draw_textured,
        h_io_opens, h_io_fail_opens, h_io_reads, h_io_short_reads,
        h_input_fired, ps2h_input_n, h_audio_submitted);

    ps2h_band_report();

    if (h_overflows || h_guard_hits || h_oversize_sends) {
        fprintf(stderr,
            "\nRESULTADO: FALHOU. O harness detectou %u estouro(s) de pacote,\n"
            "%u escrita(s) na zona de guarda e %u envio(s) DMA acima do limite.\n"
            "No PS2 qualquer um desses corrompe memoria sem travar.\n",
            h_overflows, h_guard_hits, h_oversize_sends);
    } else if (h_io_fail_opens) {
        unsigned i;
        fprintf(stderr,
            "\nRESULTADO: FALHOU. %u arquivo(s) nao abriram. Revise a raiz dos\n"
            "assets (PS2H_ROOT) e os caminhos mass:/ do jogo.\n", h_io_fail_opens);
        for (i = 0; i < h_fail_n; i++)
            fprintf(stderr, "  nao abriu: %s\n", h_fail_path[i]);
    } else {
        fprintf(stderr,
            "\nRESULTADO: PASSOU. %u quadros, %u pacotes, nenhum estouro,\n"
            "nenhuma escrita fora do limite, todo asset abriu.\n",
            h_frames_run, h_packets_made);
    }
    fflush(stderr);
}

/* Falha ruidosa e imediata: eh melhor parar aqui do que chegar no console. */
static void ps2h_die(const char *what, const char *detail,
                     unsigned have, unsigned need, unsigned limit)
{
    void *bt[24];
    int n = backtrace(bt, 24);
    fprintf(stderr,
        "\n#### ESTOURO DETECTADO PELO HARNESS ####\n"
        "  o que ....... %s\n"
        "  detalhe ..... %s\n"
        "  quadro ...... %u\n"
        "  tinha ....... %u quadwords\n"
        "  precisou .... %u quadwords\n"
        "  limite ...... %u quadwords\n"
        "  ----------------------------------------\n"
        "  O pacote foi criado com packet_init(%u) e o codigo de desenho\n"
        "  escreveu %u quadwords. No PS2 isso sobrescreve a memoria seguinte\n"
        "  ao pacote; o sintoma apareceria longe da causa. Abortando aqui.\n\n",
        what, detail ? detail : "-", h_frames_run, have, need, limit, limit, need);
    fflush(stderr);
    backtrace_symbols_fd(bt, n, 2);
    ps2h_report();
    abort();
}

/* ------------------------------------------------------------------------- */
/*  Pacote limitado                                                           */
/* ------------------------------------------------------------------------- */

typedef struct {
    qword_t *base;
    unsigned limit;      /* quadwords pedidos em packet_init                 */
    unsigned total;      /* limite + zona de guarda                          */
    int      live;
} ps2h_pkt_t;

static ps2h_pkt_t ps2h_live[PS2H_MAX_LIVE];

static unsigned ps2h_top(void)
{
    unsigned i;
    for (i = PS2H_MAX_LIVE; i-- > 0; )
        if (ps2h_live[i].live) return i;
    return PS2H_MAX_LIVE;
}

static void ps2h_poison(qword_t *q, unsigned n)
{
    unsigned i;
    for (i = 0; i < n; i += 2) {
        q[i].dw[0] = PS2H_POISON0; q[i].dw[1] = PS2H_POISON0;
        if (i + 1 < n) { q[i+1].dw[0] = PS2H_POISON1; q[i+1].dw[1] = PS2H_POISON1; }
    }
}

/* A zona de guarda intacta eh a prova de que ninguem escreveu alem do limite.
   O veneno alterna dois valores para que uma escrita de zeros ou de lixo
   qualquer seja detectada -- nao apenas "diferente do que era". */
static int ps2h_guard_ok(const ps2h_pkt_t *p)
{
    unsigned i;
    const qword_t *g = p->base + p->limit;
    for (i = 0; i < PS2H_GUARD_QWORDS; i += 2) {
        if (g[i].dw[0] != PS2H_POISON0 || g[i].dw[1] != PS2H_POISON0) return 0;
        if (i + 1 < PS2H_GUARD_QWORDS) {
            if (g[i+1].dw[0] != PS2H_POISON1 || g[i+1].dw[1] != PS2H_POISON1) return 0;
        }
    }
    return 1;
}

/* packet_init do ps2sdk, com o limite do harness.
   A assinatura e a do packet.h real: (int qwords, int type). */
packet_t *packet_init(int qwords, int type)
{
    unsigned lim = (unsigned)qwords;
    unsigned total;

    /* PS2H_QWORDS_CAP existe so para o TESTE NEGATIVO: com o pacote
       artificialmente pequeno, o estouro TEM de ser detectado. Um detector
       que nunca dispara nao vale nada, e ninguem perceberia. */
    if (ps2h_qwcap && lim > ps2h_qwcap) lim = ps2h_qwcap;
    total = lim + PS2H_GUARD_QWORDS;
    unsigned slot;
    qword_t *buf;
    packet_t *p;

    if (lim == 0 || lim > (1u << 20)) {
        fprintf(stderr, "harness: packet_init(%d) com tamanho absurdo\n", qwords);
        abort();
    }

    buf = (qword_t *)memalign(64, (size_t)total * sizeof(qword_t));
    if (!buf) { fprintf(stderr, "harness: sem memoria para packet_init(%d)\n", qwords); abort(); }
    ps2h_poison(buf + lim, PS2H_GUARD_QWORDS);

    p = (packet_t *)calloc(1, sizeof(packet_t));
    if (!p) { fprintf(stderr, "harness: sem memoria para packet_t\n"); abort(); }
    p->qwords = lim;
    p->qwc = 0;
    p->type = (u16)type;
    p->data = buf;

    for (slot = 0; slot < PS2H_MAX_LIVE; slot++)
        if (!ps2h_live[slot].live) break;
    if (slot == PS2H_MAX_LIVE) {
        fprintf(stderr, "harness: %d pacotes vivos ao mesmo tempo\n", PS2H_MAX_LIVE);
        abort();
    }
    ps2h_live[slot].base  = buf;
    ps2h_live[slot].limit = lim;
    ps2h_live[slot].total = total;
    ps2h_live[slot].live  = 1;

    h_packets_made++;
    return p;
}

void packet_free(packet_t *packet)
{
    unsigned i;
    if (!packet) return;
    for (i = 0; i < PS2H_MAX_LIVE; i++) {
        if (ps2h_live[i].live && ps2h_live[i].base == packet->data) {
            ps2h_live[i].live = 0;
            h_packets_freed++;
            break;
        }
    }
    free(packet->data);
    free(packet);
}

void packet_reset(packet_t *packet)
{
    unsigned i;
    if (!packet) return;
    packet->qwc = 0;
    for (i = 0; i < PS2H_MAX_LIVE; i++) {
        if (ps2h_live[i].live && ps2h_live[i].base == packet->data) {
            ps2h_poison(ps2h_live[i].base + ps2h_live[i].limit, PS2H_GUARD_QWORDS);
            break;
        }
    }
}

qword_t *packet_increment_qwc(packet_t *packet, int num)
{
    qword_t *q;
    if (!packet) return NULL;
    q = packet->data + packet->qwc;
    packet->qwc = (u16)(packet->qwc + num);
    return q;
}

/* Ponto de checagem numero 1: o DMA vai ler exatamente 'qwc' quadwords de
   'data'. Se 'qwc' nao cabe no pacote, o DMAC do PS2 leria memoria que nao
   eh do pacote. Aqui isso morre antes. */
static void ps2h_check_send(int channel, void *data, int qwc, const char *kind)
{
    unsigned i, q = (unsigned)qwc;
    if (kind == (const char *)0) kind = "?";

    for (i = 0; i < PS2H_MAX_LIVE; i++) {
        if (ps2h_live[i].live && ps2h_live[i].base == (qword_t *)data) {
            if (!ps2h_guard_ok(&ps2h_live[i])) {
                h_guard_hits++;
                ps2h_die("zona de guarda violada", "algo escreveu alem do limite do pacote",
                         ps2h_live[i].limit, ps2h_live[i].limit + 1, ps2h_live[i].limit);
            }
            if (q > ps2h_live[i].limit) {
                h_overflows++;
                ps2h_die("pacote estourado no DMA", kind,
                         ps2h_live[i].limit, q, ps2h_live[i].limit);
            }
            if (q > h_qwords_max) h_qwords_max = q;
            if (ps2h_live[i].limit > h_qwords_limit_max) h_qwords_limit_max = ps2h_live[i].limit;
            return;
        }
    }
    /* Nao eh um pacote nosso: so registra. */
    if (q > h_qwords_max) h_qwords_max = q;
}

int dma_channel_send_normal(int channel, void *data, int qwc, int flags, int spr)
{
    h_sends_normal++;
    /* PS2H_FORCE_QWC: teste negativo do limite de QWC_MAX. Sem isso ninguem
       saberia se a checagem realmente funciona. */
    if (ps2h_force_qwc) qwc = (int)ps2h_force_qwc;
    if ((unsigned)qwc > 65535u) { h_oversize_sends++; ps2h_die("DMA acima de QWC_MAX", "dma_channel_send_normal", (unsigned)qwc, (unsigned)qwc, 65535u); }
    ps2h_check_send(channel, data, qwc, "dma_channel_send_normal");
    return 0;
}

int dma_channel_send_chain(int channel, void *data, int qwc, int flags, int spr)
{
    h_sends_chain++;
    if ((unsigned)qwc > 65535u) { h_oversize_sends++; ps2h_die("DMA acima de QWC_MAX", "dma_channel_send_chain", (unsigned)qwc, (unsigned)qwc, 65535u); }
    ps2h_check_send(channel, data, qwc, "dma_channel_send_chain");
    return 0;
}

int dma_channel_send_normal_ucab(int channel, void *data, int qwc, int flags)
{ return dma_channel_send_normal(channel, data, qwc, flags, 0); }

int dma_channel_send_chain_ucab(int channel, void *data, int qwc, int flags)
{ return dma_channel_send_chain(channel, data, qwc, flags, 0); }

int dma_channel_send_packet2(void *packet2, int channel, u8 flush_cache)
{ h_sends_chain++; return 0; }

int dma_channel_receive_normal(int c, void *d, int n, int f, int s) { (void)c;(void)d;(void)n;(void)f;(void)s; return 0; }
int dma_channel_receive_chain(int c, void *d, int n, int f, int s) { (void)c;(void)d;(void)n;(void)f;(void)s; return 0; }
int dma_channel_shutdown(int c, int f) { (void)c;(void)f; return 0; }
int dma_channel_wait(int c, int t) { (void)c;(void)t; return 0; }
int dma_reset(void) { return 0; }
int dma_channel_initialize(int channel, void *handler, int flags) { (void)channel;(void)handler;(void)flags; return 0; }
void dma_channel_fast_waits(int channel) { (void)channel; }
void dma_wait_fast(void) { }

/* ------------------------------------------------------------------------- */
/*  Registradores do GS                                                       */
/* ------------------------------------------------------------------------- */

/* O ps2sdk acessa o GS por enderecos absolutos (0x12000000...). No host esses
   enderecos nao existem e o codigo REAL de draw morreria. A solucao eh mapear
   a pagina de verdade: o draw.c do ps2sdk roda sem uma linha alterada. */
#define PS2H_GS_BASE  0x12000000ull
#define PS2H_GS_PAGES 4

static volatile u64 *ps2h_gs(u64 addr) { return (volatile u64 *)(uintptr_t)addr; }

static void ps2h_map_gs(void)
{
    void *m = mmap((void *)(uintptr_t)PS2H_GS_BASE, PS2H_GS_PAGES * 4096,
                   PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
    if (m == MAP_FAILED) {
        fprintf(stderr, "harness: nao consegui mapear os registradores do GS em %#llx\n",
                (unsigned long long)PS2H_GS_BASE);
        exit(2);
    }
    /* CSR com o bit FINISH ligado: draw_wait_finish() do ps2sdk espera exatamente
       esse bit, e sem isso o harness travaria no primeiro quadro. */
    *ps2h_gs(0x12001000ull) = 2;
}

/* ------------------------------------------------------------------------- */
/*  graph                                                                     */
/* ------------------------------------------------------------------------- */

int graph_initialize(int fbp, int width, int height, int psm, int x, int y)
{ (void)fbp;(void)width;(void)height;(void)psm;(void)x;(void)y; return 0; }

void graph_set_bgcolor(unsigned char r, unsigned char g, unsigned char b)
{ (void)r;(void)g;(void)b; }

void graph_set_framebuffer_filtered(int fbp, int width, int psm, int x, int y)
{ (void)fbp;(void)width;(void)psm;(void)x;(void)y; }

void graph_vram_clear(void) { }
int  graph_vram_allocate(int width, int height, int psm, int align)
{ (void)width;(void)height;(void)psm;(void)align; return 0; }

/* Ponto de checagem numero 2, e tambem o relogio do harness: cada chamada eh
   um quadro. Quando o limite de quadros chega, o harness pede a saida pelo
   pad -- o mesmo caminho que o jogador usaria (START com SELECT segurado),
   para que o jogo passe pelo shutdown completo e o log seja fechado. */
#define H_BUSCLK 147456000ull
#define H_TICKS_PER_FRAME (H_BUSCLK / 60ull)
static u64 h_virtual_ticks = H_TICKS_PER_FRAME;   /* comeca em 1 quadro */

void graph_wait_vsync(void)
{
    h_virtual_ticks += H_TICKS_PER_FRAME;
    h_frames_run++;
    if (h_frames_run >= ps2h_frames) {
        h_quit_requested = 1;
        ps2h_quit_frame = h_frames_run;
    }
}

int graph_add_vsync_handler(void (*h)(int), int cooldown) { (void)h;(void)cooldown; return 0; }
void graph_remove_vsync_handler(int id) { (void)id; }
int graph_get_field(void) { return 1; }

/* ------------------------------------------------------------------------- */
/*  pad                                                                       */
/* ------------------------------------------------------------------------- */

int padInit(int mode) { (void)mode; return 1; }
int padPortOpen(int port, int slot, void *padArea) { (void)port;(void)slot;(void)padArea; return 1; }
int padSetMainMode(int port, int slot, int mode, int lock) { (void)port;(void)slot;(void)mode;(void)lock; return 1; }
int padPortClose(int port, int slot) { (void)port;(void)slot; return 1; }
int padEnd(void) { return 1; }
int padGetState(int port, int slot) { (void)port;(void)slot; return 6; /* PAD_STATE_STABLE */ }

/* O pad devolve SELECT segurado o tempo todo -- eh assim que o combo de saida
   do jogo (START com SELECT) fica disponivel. Quando o harness pede a saida,
   START entra por um quadro so. Nenhum outro botao: o jogo comeca no menu. */
unsigned char padRead(int port, int slot, struct padButtonStatus *data)
{
    struct padButtonStatus *b = data;
    unsigned script;
    if (!b) return 0;
    memset(b, 0, sizeof *b);
    b->ok = 1;
    b->mode = 0x73;              /* DualShock, analogico travado */
    b->btns = 0xffffu;           /* bit 0 = solto, bit 1 = apertado */
    b->btns &= (unsigned short)~PAD_SELECT;
    b->rjoy_h = b->rjoy_v = 128;
    b->ljoy_h = b->ljoy_v = 128;

    /* Roteiro de botoes. Sem isso o harness roda 240 quadros parado na tela
       de titulo: o menu, o story e o tutorial -- que eh onde esta o codigo
       arriscado -- nunca sao exercitados. */
    for (script = 0; script < ps2h_input_n; script++) {
        if (ps2h_input[script].frame != h_frames_run) continue;
        b->btns &= (unsigned short)~ps2h_input[script].btns;
        h_input_fired++;
    }

    if (h_quit_requested) {
        b->btns &= (unsigned short)~PAD_START;
        h_quit_requested = 0;    /* so por um quadro */
    }
    return 1;
}

/* ------------------------------------------------------------------------- */
/*  audsrv                                                                    */
/* ------------------------------------------------------------------------- */

int audsrv_init(void) { return 0; }
int audsrv_quit(void) { return 0; }
int audsrv_set_format(void *fmt) { (void)fmt; return 0; }
int audsrv_set_volume(int v) { (void)v; return 0; }
int audsrv_stop_audio(void) { return 0; }
int audsrv_play_audio(const char *chunk, int bytes)
{
    if (bytes > 0) h_audio_submitted += (unsigned)(bytes / 4);
    return bytes > 0 ? bytes : 0;
}
int audsrv_available(void) { h_audio_available_calls++; return 1 << 20; }
int audsrv_queued(void) { return 0; }

/* ------------------------------------------------------------------------- */
/*  SifRpc / kernel / timer                                                   */
/* ------------------------------------------------------------------------- */

int SifInitRpc(int mode) { (void)mode; return 0; }
int SifIopReset(const char *arg, int flag) { (void)arg;(void)flag; return 1; }
int SifIopSync(void) { return 1; }   /* !=0 = reset do IOP concluido */
int SifLoadFileInit(void) { return 0; }
int SifLoadModule(const char *path, int len, void *stat) { (void)path;(void)len;(void)stat; return 0; }
int SifExecModuleBuffer(void *ptr, unsigned size, int len, int *result, int *final)
{ (void)ptr;(void)size;(void)len; if (result) *result = 0; if (final) *final = 0; return 0; }

void FlushCache(int op) { (void)op; }
void DelayThread(int us) { (void)us; }

/*
 * Relogio VIRTUAL, batendo com o do console.
 *
 * O console entrega GetTimerSystemTime() em unidades de BUSCLK e o jogo
 * converte com `dt * AUDIO_RATE / kBUSCLK` para saber quantas amostras de
 * audio passaram -- e' o relogio da musica, o mesmo que posiciona as notas.
 *
 * A versao anterior deste shim devolvia MICROSSEGUNDOS de relogio de parede.
 * Isso nao e' "quase a mesma coisa": a conta do jogo passava a valer
 * 48000/147456000 de amostra por microssegundo, e o relogio da musica andava
 * ~0,0003 amostra por us. Medido no harness: `music_clock` ficou travado em 1
 * durante 800 quadros. Resultado: nenhuma nota chegava perto do receptor, o
 * caminho de acerto/julgamento nunca rodava no PC, e o harness nao via nada do
 * codigo de gameplay -- so' o de desenho.
 *
 * Aqui o tempo avanca por QUADRO, na taxa do console: 1/60 s por
 * graph_wait_vsync(). Vira deterministico (duas rodadas medem o mesmo), o
 * relogio da musica anda 800 amostras por quadro (48000/60), e o jogo passa a
 * se comportar como em 60 fps constantes.
 */
u64 GetTimerSystemTime(void)
{
    return h_virtual_ticks;
}

/* ------------------------------------------------------------------------- */
/*  E/S: mass:/ vira o diretorio de assets do harness                         */
/* ------------------------------------------------------------------------- */

static const char *ps2h_map_path(const char *p, char *buf, size_t n)
{
    if (p && !strncmp(p, "mass:/", 6)) {
        snprintf(buf, n, "%s/%s", ps2h_root, p + 6);
        return buf;
    }
    if (p && !strncmp(p, "host:/", 6)) return p + 6;
    return p;
}

static int ps2h_openat(const char *p, int flags, mode_t mode)
{
    char buf[1024];
    p = ps2h_map_path(p, buf, sizeof buf);
    return (int)syscall(SYS_openat, AT_FDCWD, p, flags, mode);
}

int open(const char *path, int flags, ...)
{
    mode_t mode = 0;
    va_list ap;
    h_io_opens++;
    va_start(ap, flags);
    mode = (mode_t)va_arg(ap, int);
    va_end(ap);
    { int fd = ps2h_openat(path, flags, mode);
      if (fd < 0) { h_io_fail_opens++; h_note_fail(path); }
      return fd; }
}

int open64(const char *path, int flags, ...)
{
    mode_t mode = 0; va_list ap;
    h_io_opens++;
    va_start(ap, flags); mode = (mode_t)va_arg(ap, int); va_end(ap);
    { int fd = ps2h_openat(path, flags, mode);
      if (fd < 0) { h_io_fail_opens++; h_note_fail(path); }
      return fd; }
}

/* Leitura curta nao eh erro por si so: read_exact_path() le em pedacos e a
   ultima batida no fim do arquivo legitimo. O que NUNCA pode acontecer -- e o
   que aponta para corrupcao de buffer ou truncamento de asset -- eh uma
   leitura curta no MEIO do arquivo. Por isso so conta quando ainda ha dados
   depois da posicao atual. */
ssize_t read(int fd, void *buf, size_t n)
{
    ssize_t r = syscall(SYS_read, fd, buf, n);
    h_io_reads++;
    if (r >= 0 && (size_t)r < n) {
        off_t pos = lseek(fd, 0, SEEK_CUR);
        struct stat st;
        if (pos >= 0 && fstat(fd, &st) == 0 && (off_t)st.st_size > pos)
            h_io_short_reads++;      /* curta com dados ainda no arquivo */
    }
    return r;
}

ssize_t write(int fd, const void *buf, size_t n)
{ return syscall(SYS_write, fd, buf, n); }

int close(int fd) { return (int)syscall(SYS_close, fd); }

off_t lseek(int fd, off_t off, int whence)
{ return syscall(SYS_lseek, fd, off, whence); }

/* ------------------------------------------------------------------------- */
/*  libkernel: threads e semaforos                                           */
/* ------------------------------------------------------------------------- */

/* No harness tudo roda numa thread so. As funcoes existem para o jogo linkar
   e para o comportamento ser o obvio: criar da certo, esperar nao bloqueia. */

/* ------------------------------------------------------------------------- */
/*  roteiro de botoes (PS2H_INPUT)                                           */
/* ------------------------------------------------------------------------- */

static unsigned ps2h_button_bit(const char *name)
{
    struct { const char *n; unsigned short b; } t[] = {
        {"SELECT", PAD_SELECT}, {"START", PAD_START}, {"UP", PAD_UP},
        {"RIGHT", PAD_RIGHT},   {"DOWN", PAD_DOWN},   {"LEFT", PAD_LEFT},
        {"L2", PAD_L2}, {"R2", PAD_R2}, {"L1", PAD_L1}, {"R1", PAD_R1},
        {"TRIANGLE", PAD_TRIANGLE}, {"CIRCLE", PAD_CIRCLE},
        {"CROSS", PAD_CROSS},     {"SQUARE", PAD_SQUARE},
    };
    unsigned i;
    for (i = 0; i < sizeof t / sizeof t[0]; i++)
        if (strcmp(name, t[i].n) == 0) return t[i].b;
    return 0;
}

/* Formato: "quadro:BOTAO,quadro:BOTAO+BOTAO"  (ex.: "150:CROSS,320:CIRCLE") */
static void ps2h_parse_input(const char *spec)
{
    char buf[512], *save = NULL, *tok;
    if (!spec || !*spec) return;
    strncpy(buf, spec, sizeof buf - 1);
    buf[sizeof buf - 1] = 0;
    for (tok = strtok_r(buf, ",", &save); tok && ps2h_input_n < PS2H_MAX_INPUT;
         tok = strtok_r(NULL, ",", &save)) {
        char *colon = strchr(tok, ':');
        char *names, *nsave = NULL, *nm;
        unsigned short bits = 0;
        if (!colon) continue;
        *colon = 0;
        names = colon + 1;
        for (nm = strtok_r(names, "+", &nsave); nm; nm = strtok_r(NULL, "+", &nsave)) {
            unsigned bit = ps2h_button_bit(nm);
            if (!bit) {
                fprintf(stderr, "harness: botao desconhecido no roteiro: '%s'\n", nm);
                continue;
            }
            bits = (unsigned short)(bits | bit);
        }
        ps2h_input[ps2h_input_n].frame = (unsigned)strtoul(tok, NULL, 10);
        ps2h_input[ps2h_input_n].btns  = bits;
        ps2h_input_n++;
    }
}

/* ------------------------------------------------------------------------- */
/*  threads: pthread de verdade                                              */
/* ------------------------------------------------------------------------- */
/* O jogo cria uma thread real para pre-carregar a animacao do titulo
   (title_animation_cache_start). Se o harness so devolver um id e nao rodar o
   corpo, esse caminho inteiro fica sem cobertura -- e eh justamente onde
   poderiam estar os erros. Entao CreateThread guarda o descritor e StartThread
   dispara uma pthread de verdade. */

#include <pthread.h>

#define PS2H_MAX_THREADS 8

typedef struct {
    int      used;
    pthread_t tid;
    int      started;
    int      detached;
    void (*func)(void *);
    void *arg;
    int    status;
} ps2h_thread_t;

static ps2h_thread_t ps2h_th[PS2H_MAX_THREADS];

static void *ps2h_thread_entry(void *p)
{
    ps2h_thread_t *t = (ps2h_thread_t *)p;
    t->status = 1;                       /* THS_RUN */
    t->func(t->arg);
    t->status = 0;
    return NULL;
}

int CreateThread(ee_thread_t *t)
{
    ee_thread_t *e = t;
    int i;
    if (!e) return -1;
    for (i = 0; i < PS2H_MAX_THREADS; i++)
        if (!ps2h_th[i].used) break;
    if (i == PS2H_MAX_THREADS) return -1;
    ps2h_th[i].used = 1;
    ps2h_th[i].func = (void (*)(void *))e->func;
    ps2h_th[i].arg  = NULL;
    ps2h_th[i].started = 0;
    ps2h_th[i].detached = 0;
    ps2h_th[i].status = 2;               /* THS_READY */
    return i + 1;                        /* ids comecam em 1, como no EE */
}

int StartThread(int id, void *arg)
{
    ps2h_thread_t *t;
    if (id < 1 || id > PS2H_MAX_THREADS) return -1;
    t = &ps2h_th[id - 1];
    if (!t->used || !t->func || t->started) return -1;
    t->arg = arg;
    t->started = 1;
    if (pthread_create(&t->tid, NULL, ps2h_thread_entry, t) != 0) return -1;
    return 0;
}

void ExitThread(void) { pthread_exit(NULL); }
void ExitDeleteThread(void) { pthread_exit(NULL); }

int TerminateThread(int id)
{
    if (id >= 1 && id <= PS2H_MAX_THREADS && ps2h_th[id - 1].started)
        pthread_cancel(ps2h_th[id - 1].tid);
    return 0;
}

int DeleteThread(int id)
{
    if (id < 1 || id > PS2H_MAX_THREADS) return -1;
    if (!ps2h_th[id - 1].used) return -1;
    if (ps2h_th[id - 1].started) pthread_join(ps2h_th[id - 1].tid, NULL);
    memset(&ps2h_th[id - 1], 0, sizeof ps2h_th[id - 1]);
    return 0;
}

int CreateSema(ee_sema_t *s) { (void)s; return 1; }
int DeleteSema(int id) { (void)id; return 0; }
int SignalSema(int id) { (void)id; return 0; }
int WaitSema(int id) { (void)id; return 0; }

/* O jogo chama SleepThread() quando algo falha de forma irreparavel. Se isso
   virar um no-op o harness continua rodando com estado invalido e o relatorio
   mente. Entao aqui ele realmente dorme para sempre -- o watchdog pega. */
int SleepThread(void)
{
    /* O jogo TERMINA chamando SleepThread() -- no PS2 ele nunca volta do main.
       Entao ha dois casos muito diferentes aqui:
         - o harness ja entregou todos os quadros pedidos  -> shutdown normal,
           que eh SUCESSO. Tem de imprimir o relatorio e sair com 0;
         - qualquer outro momento -> falha irreparavel na inicializacao, e o
           harness tem de parar e dizer onde, nunca continuar com estado
           invalido (um relatorio mentiroso eh pior que nenhum). */
    if (h_frames_run >= ps2h_frames) {
        fprintf(stderr,
            "\nharness: o jogo chegou ao fim (%u quadros) e estacionou no\n"
            "SleepThread() de shutdown -- comportamento normal no PS2.\n",
            h_frames_run);
        fflush(stderr);
        ps2h_report();
        fflush(stderr);
        _exit(0);
    }
    fprintf(stderr,
        "\n#### O JOGO CHAMOU SleepThread() ####\n"
        "  Falha irreparavel: o codigo desistiu com apenas %u quadro(s) de %u.\n"
        "  Onde ele parou:\n\n", h_frames_run, ps2h_frames);
    fflush(stderr);
    {   void *bt[24];
        int n = backtrace(bt, 24);
        backtrace_symbols_fd(bt, n, 2);
    }
    fflush(stderr);
    for (;;) pause();
    return 0;
}

int sceSifInitRpc(int mode) { (void)mode; return 0; }
int fileXioInit(void) { return 0; }

void sbv_patch_disable_prefix_check(void) { }
void sbv_patch_enable_lmb(void) { }

/* O ABI MIPS espera _gp, que o crt0 do PS2 define. O linker do host nao sabe
   disso e reclama. Um simbolo vazio satisfaz o linker sem mudar nada: nenhum
   codigo do jogo o usa. */
void *_gp = 0;

/* ------------------------------------------------------------------------- */
/*  Bootstrap                                                                 */
/* ------------------------------------------------------------------------- */

static void ps2h_read_env(void)
{
    const char *e;
    if ((e = getenv("PS2H_FRAMES"))) ps2h_frames = (unsigned)strtoul(e, NULL, 10);
    if ((e = getenv("PS2H_ROOT")))   ps2h_root = e;
    if ((e = getenv("PS2H_STRICT"))) ps2h_strict = atoi(e);
    if ((e = getenv("PS2H_QWORDS_CAP"))) ps2h_qwcap = (unsigned)strtoul(e, NULL, 10);
    if ((e = getenv("PS2H_FORCE_QWC")))  ps2h_force_qwc = (unsigned)strtoul(e, NULL, 10);
    ps2h_parse_input(getenv("PS2H_INPUT"));
    if (ps2h_frames == 0) ps2h_frames = 1;
}

/* Watchdog: se o jogo travar, o harness nao fica pendurado sem dizer nada --
   que eh o pior resultado possivel para uma ferramenta de verificacao. Ao
   estourar o prazo ele imprime onde o codigo parou e aborta. */
#include <signal.h>

static void ps2h_watchdog(int sig)
{
    void *bt[32];
    int n = backtrace(bt, 32);
    fprintf(stderr,
        "\n#### TRAVAMENTO DETECTADO PELO WATCHDOG ####\n"
        "  O jogo passou %u segundos sem sair do laco. Onde ele parou:\n\n",
        ps2h_watch_secs);
    fflush(stderr);
    backtrace_symbols_fd(bt, n, 2);
    fprintf(stderr, "\n  (quadros executados ate travar: %u)\n", h_frames_run);
    fflush(stderr);
    _exit(9);
}

__attribute__((constructor))
static void ps2h_boot(void)
{
    ps2h_read_env();
    ps2h_map_gs();
    atexit(ps2h_report);

    { const char *w = getenv("PS2H_WATCHDOG");
      if (w && *w) {
          ps2h_watch_secs = (unsigned)strtoul(w, NULL, 10);
          signal(SIGALRM, ps2h_watchdog);
          alarm(ps2h_watch_secs);
      } }
    ps2h_ready = 1;
    fprintf(stderr,
        "harness: %u quadros, raiz de assets '%s', pacote com %u quadwords de guarda\n",
        ps2h_frames, ps2h_root, PS2H_GUARD_QWORDS);
    if (ps2h_qwcap)
        fprintf(stderr, "harness: ATENCAO, teto artificial de %u quadwords por pacote "
                        "(teste negativo do detector)\n", ps2h_qwcap);
    if (ps2h_force_qwc)
        fprintf(stderr, "harness: ATENCAO, todo envio DMA vai com %u quadwords "
                        "(teste negativo do limite QWC_MAX)\n", ps2h_force_qwc);
    if (ps2h_input_n) {
        unsigned i;
        fprintf(stderr, "harness: roteiro de botoes (%u eventos):", ps2h_input_n);
        for (i = 0; i < ps2h_input_n; i++)
            fprintf(stderr, " %u:%04x", ps2h_input[i].frame, ps2h_input[i].btns);
        fprintf(stderr, "\n");
    }
}

/* Contadores que o jogo expoe, para o relatorio do harness. */
void ps2h_note_upload(int bytes) { h_upload_calls++; h_upload_bytes += (unsigned)bytes; }
void ps2h_note_draw(void) { h_draw_calls++; }

/* ------------------------------------------------------------------------- */
/*  contagem de chamadas de desenho (via --wrap do linker)                    */
/* ------------------------------------------------------------------------- */
/* Nao da para contar de dentro: draw_rect_filled etc. estao no draw.c do
   ps2sdk, que nao queremos tocar. O --wrap do linker troca a referencia por
   __wrap_* e ainda deixa o original acessivel em __real_*, entao a contagem
   nao muda uma linha da logica do ps2sdk. */

extern void *__real_draw_rect_filled(void *q, int context, void *v);
extern void *__real_draw_rect_textured(void *q, int context, void *v);
extern void *__real_draw_setup_environment(int context, void *fb, void *zb);
extern qword_t *__real_draw_texture_transfer(qword_t *q, void *src, int width,
                                             int height, int psm, int dest,
                                             int dest_width);

void *__wrap_draw_rect_filled(void *q, int context, void *v)
{ h_draw_filled++; return __real_draw_rect_filled(q, context, v); }

/* --- auditoria da faixa visivel -------------------------------------------
   O jogo publica em g_band_on/g_band_topo/g_band_base a faixa de linhas que
   deveria ser a unica visivel naquele momento (a mesma que o scissor do GS
   recebe). Aqui cada retangulo TEXTURADO eh comparado contra ela. Se algo
   sair, o numero de linhas fora da faixa sai no relatorio -- que eh
   exatamente o defeito que o teste no console mostrou: o fundo do menu
   aparecendo das linhas 46 a 447 e um item inteiro em 404..462 com o scissor
   em 56..391. Um rasterizador de brinquedo nao pega isso; uma conta pega. */
extern int g_band_on, g_band_topo, g_band_base;
extern int g_ui_state_dbg;

static unsigned long h_band_out;       /* retangulos com alguma linha fora */
static unsigned long h_band_lines;     /* total de linhas fora da faixa    */
static unsigned long h_band_worst;     /* pior caso, em linhas             */
static int h_band_frame = -1;          /* quadro do pior caso              */
static float h_band_y0, h_band_y1;     /* o retangulo do pior caso         */

/* Por faixa: um numero agregado esconde ONDE esta o problema. Com a quebra
   da para dizer "o menu principal esta limpo e o story nao" -- o contrario
   de um total unico e sem contexto. */
#define PS2H_BANDS 8
#define PS2H_STATES 10
static int h_band_topo_v[PS2H_BANDS], h_band_base_v[PS2H_BANDS];
static unsigned long h_band_out_v[PS2H_BANDS], h_band_lines_v[PS2H_BANDS];
static int h_band_n;
static unsigned long h_band_out_st[PS2H_STATES], h_band_lines_st[PS2H_STATES];
static unsigned long h_band_rect_st[PS2H_STATES];
static unsigned long h_band_worst_st[PS2H_STATES];
static int h_band_frame_st[PS2H_STATES];
static float h_band_y0_st[PS2H_STATES], h_band_y1_st[PS2H_STATES];
static int h_band_topo_st[PS2H_STATES], h_band_base_st[PS2H_STATES];

void *__wrap_draw_rect_textured(void *q, int context, void *v)
{
    h_draw_textured++;
    if (g_band_on) {
        /* texrect_t REAL do ps2sdk (ee/draw/include/draw2d.h):
         *   { vertex_t v0; texel_t t0; vertex_t v1; texel_t t1; color_t color; }
         * com vertex_t = {float x; float y; unsigned int z;} (12 bytes) e
         * texel_t/color_t packed+aligned(8) (8 bytes). Em floats isso da
         * v0.x=0 v0.y=1 v0.z=2 | t0.s=4 t0.t=5 | v1.x=6 v1.y=7 v1.z=8 |
         * t1.s=10 t1.t=11 | color=12. Ler v1.y no offset 3 pega t0.s, que eh
         * a coordenada U da textura -- a conta "passa" mas nao mede nada. */
        const float *y = (const float *)v;
        float y0 = y[1], y1 = y[7];
        float lo = y0 < y1 ? y0 : y1, hi = y0 < y1 ? y1 : y0;
        float topo = (float)g_band_topo, base = (float)g_band_base;
        float out = 0.0f;
        if (lo < topo) out += topo - lo;
        if (hi > base) out += hi - base;
        if (out > 0.0f) {
            int slot = -1, k;
            for (k = 0; k < h_band_n; k++) {
                if (h_band_topo_v[k] == g_band_topo && h_band_base_v[k] == g_band_base) { slot = k; break; }
            }
            if (slot < 0 && h_band_n < PS2H_BANDS) {
                slot = h_band_n++;
                h_band_topo_v[slot] = g_band_topo;
                h_band_base_v[slot] = g_band_base;
            }
            if (slot >= 0) { h_band_out_v[slot]++; h_band_lines_v[slot] += (unsigned long)out; }
            if (g_ui_state_dbg >= 0 && g_ui_state_dbg < PS2H_STATES) {
                int st = g_ui_state_dbg;
                h_band_out_st[st]++;
                h_band_lines_st[st] += (unsigned long)out;
                h_band_rect_st[st]++;
                if (out > (float)h_band_worst_st[st]) {
                    h_band_worst_st[st] = (unsigned long)out;
                    h_band_frame_st[st] = (int)h_frames_run;
                    h_band_y0_st[st] = lo; h_band_y1_st[st] = hi;
                    h_band_topo_st[st] = g_band_topo;
                    h_band_base_st[st] = g_band_base;
                }
            }
            h_band_out++;
            h_band_lines += (unsigned long)out;
            if (out > (float)h_band_worst) {
                h_band_worst = (unsigned long)out;
                h_band_frame = (int)h_frames_run;
                h_band_y0 = lo; h_band_y1 = hi;
            }
        }
    }
    return __real_draw_rect_textured(q, context, v);
}

void ps2h_band_report(void)
{
    int i;
    fprintf(stderr, "    auditoria da faixa visivel: %lu retangulo(s) com linha fora,"
                    " %lu linha(s) fora no total\n", h_band_out, h_band_lines);
    for (i = 0; i < h_band_n; i++) {
        fprintf(stderr, "      faixa %3d..%3d: %8lu retangulo(s), %10lu linha(s) fora\n",
                h_band_topo_v[i], h_band_base_v[i], h_band_out_v[i], h_band_lines_v[i]);
    }
    for (i = 0; i < PS2H_STATES; i++) {
        if (h_band_rect_st[i]) {
            fprintf(stderr, "      ui_state %d: %8lu retangulo(s), %10lu linha(s) fora"
                            " | pior: quadro %d, y %.1f..%.1f, %lu fora de %d..%d\n",
                    i, h_band_rect_st[i], h_band_lines_st[i],
                    h_band_frame_st[i], h_band_y0_st[i], h_band_y1_st[i],
                    h_band_worst_st[i], h_band_topo_st[i], h_band_base_st[i]);
        }
    }
    if (h_band_out) {
        fprintf(stderr, "    PIOR CASO: quadro %d, retangulo y %.1f..%.1f,"
                        " %lu linha(s) fora (faixa %d..%d)\n",
                h_band_frame, h_band_y0, h_band_y1, h_band_worst,
                g_band_topo, g_band_base);
    }
}

void *__wrap_draw_setup_environment(int context, void *fb, void *zb)
{ h_draw_env++; return __real_draw_setup_environment(context, fb, zb); }

/* A assinatura TEM de ser a do draw.h real, parametro por parametro. Declarar
   o primeiro como int em vez de qword_t* parece inofensivo, mas no x86-64
   escrever num registrador de 32 bits ZERA os 32 bits altos: com o heap do ASan
   acima de 4 GB o ponteiro chega truncado e o draw.c morre com SEGV. Foi assim
   que se descobriu -- e so com o ASan ligado. */
qword_t *__wrap_draw_texture_transfer(qword_t *q, void *src, int width, int height,
                                      int psm, int dest, int dest_width)
{
    h_upload_calls++;
    h_upload_bytes += (unsigned)(width * height * 4);
    return __real_draw_texture_transfer(q, src, width, height, psm, dest, dest_width);
}
