#include "telemetry.h"
#include "audio_ring.h"  /* 0.2.168: io_gate -- o usbhdfsd nao e' reentrante */
#include <delaythread.h>
#include <kernel.h>
#include <loadfile.h>
#include <sbv_patches.h>
#include <timer.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* Inicializa o cliente RPC fileXio; as operacoes de arquivo em si usam a
 * camada POSIX/newlib, como exige esta versao do PS2SDK. */
extern int fileXioInit(void);

#define MAX_FRAMES 512
#define MAX_RENDER_EVENTS 4096

typedef struct {
    u32 frame;
    u64 begin, draw_done, vsync_done;
    u16 pressed;
    s32 animation, animation_frame, asset_frame;
    u32 textured_quads;
    u16 qwords, state, transitions;
    u32 audio_heard, audio_queued, ring_level, underruns;
} frame_record_t;

static frame_record_t records[MAX_FRAMES];
static u32 record_count;
static char build_name[64];
static char launch_path[256];
static int usb_ready;
typedef struct {u64 ticks;char name[24];s32 a,b;} render_event_t;
static render_event_t render_events[MAX_RENDER_EVENTS];
static u32 render_event_count;
static u32 render_events_dropped;

extern unsigned char iomanX_irx[];
extern unsigned int size_iomanX_irx;
extern unsigned char fileXio_irx[];
extern unsigned int size_fileXio_irx;
extern unsigned char usbd_irx[];
extern unsigned int size_usbd_irx;
extern unsigned char usbhdfsd_irx[];
extern unsigned int size_usbhdfsd_irx;

static int exec_irx(void *data, unsigned int size)
{
    int result = -1;
    return SifExecModuleBuffer(data, size, 0, NULL, &result);
}

/* ---------------------------------------------------------------- saida
 *
 * O descritor fica ABERTO a partir do init. A versao anterior abria e fechava
 * em cada escrita, e so escrevia no fim -- de modo que um travamento, que eh
 * justamente quando o log importa, nao deixava nada. Aqui cada bloco sai
 * pendurado no mesmo descritor, com buffer proprio, e telemetry_flush() esvazia
 * esse buffer quando quem chama quiser.
 *
 * usbhdfsd provou mais confiavel no SCPH-90006 com blocos pequenos e
 * comprometidos, e NAO aceita fsync repetido entre blocos: a sincronizacao
 * acontece uma vez, no flush, e nao em cada linha.
 */
#define OUTPUT_BUFFER_SIZE (192*1024)
static char output_buffer[OUTPUT_BUFFER_SIZE];
static unsigned int output_used;
static unsigned int output_checksum;
static int output_error;
static int log_fd = -1;
static int init_result;
static u32 flush_count;
static u32 frames_logged;

/* Escreve o buffer no descritor. NAO faz fsync: usbhdfsd nao aceita fsync
   repetido entre blocos, e a sincronizacao acontece uma vez por quadro, em
   telemetry_flush().

   Se a escrita falhar, o log MORRE de proposito. Um log que nao escreve nao
   pode travar o jogo -- essa eh a regra que a build 0.2.108 quebrou, e eh por
   isso que ela nao abriu no console. */
/* PREFIXOS, nao nomes fixos (0.2.164). O log deixou de ser um arquivo unico
   aberto em O_APPEND: agora cada despejo escreve um arquivo NOVO numerado, com
   O_TRUNC + write + fsync + close -- exatamente a receita que o BOOT.txt usou
   401 vezes no console do usuario sem falhar. O motivo de nao usar append:
   nas builds 0.2.161/0.2.162 o jogo morria no fim do quadro 0, e a unica
   diferenca entre o que travava (o despejo do log) e o que funcionava (o
   BOOT.txt) era o O_APPEND.                                          
   A numeracao roda em 32 nomes (LOG_00..LOG_31) e volta ao 00: no cartao ficam
   sempre os 32 despejos mais recentes (~256 KB de historia), sem nunca crescer
   um arquivo. */
static const char *const log_prefixes[] = {
    "mass:/FNFPS2/FNF_LOG",
    "mass:/FNF_LOG",
    "host:/FNF_LOG",
    "mc0:/FNF_LOG",
    0
};
static unsigned log_seq;

static void log_path_for(int idx, unsigned seq, char *out, unsigned int size)
{
    snprintf(out, size, "%s%02u.txt", log_prefixes[idx], seq);
}
/* Despejo DURAVEL (0.2.162).
   Medido no console do usuario em 2026-11-04: o log do jogo veio com 0 bytes
   enquanto o BOOT.txt -- que abre, escreve, sincroniza e FECHA a cada marco --
   veio completo. A diferenca nao era o conteudo: era o close().
   O usbhdfsd mantem o tamanho do arquivo no cache do FAT e so o publica no
   close(); com o descritor aberto para sempre, um desligamento (ou um jogo que
   trava) deixa o arquivo existindo com zero bytes. Ou seja: TODA a telemetria
   per-quadro deste port -- escrita justamente para diagnosticar travamento --
   nunca chegou ao cartao. Era cega por construcao.

   Aqui cada despejo abre o arquivo, escreve, sincroniza e fecha. Custa um
   open+close por despejo; em troca, o que esta no cartao eh exatamente o que o
   jogo conseguiu escrever. O descritor guardado em log_fd deixa de ser usado
   para escrita (fica so' como "ja sei em qual caminho abrir"), para nao haver
   dois escritores no mesmo arquivo -- que ja' custou uma colheita vazia antes. */
static int log_path_index = -1;
static unsigned int output_dropped;   /* 0.2.169: linhas descartadas */

/* Anel de quadros completo (0.2.165). Fica em RAM (512 x ~70 B = 35 KB) e
   so' vai para o cartao quando algo anormal acontece -- ver telemetry_frame2()
   no fim do arquivo. Declarado AQUI porque o cabecalho de cada despejo diz
   quantos quadros o anel tem. */
#define FRAME_RING 512
static fnf_frame_t g_fring[FRAME_RING];
static unsigned int g_fring_wp, g_fring_count;
static unsigned int fring_n(void) { return g_fring_count; }
static u64 g_boot_t0;
static unsigned int segundos_desde_boot(void)
{
    return (unsigned int)((GetTimerSystemTime() - g_boot_t0) / (u64)kBUSCLK);
}

static int out_fd = -1;                 /* arquivo do despejo corrente */
static unsigned int g_flush_us_last;    /* custo do ultimo despejo, em us */

static volatile int g_busy;             /* trava simples contra o cao de guarda */
static unsigned g_flush_skip;           /* 0.2.174: despejos saltados (portao ocupado) */
unsigned telemetry_flush_skip(void){return g_flush_skip;}
/* 0.2.175: O QUADRO NUNCA ESCREVE. As escritas sao PEDIDAS aqui e FEITAS pela
   thread escritora (main.c), com o produtor de audio pausado. Causa unica do
   freeze: o burst de escrita do despejo (write grande + fsync) envenena o
   usbhdfsd, o produtor prende numa leitura segurando o portao, e o IOP morre
   arrastando o quadro pelas chamadas SIF (audsrv/pad). 3/3 sessoes: o freeze
   nasceu sempre logo apos um despejo. */
static volatile unsigned g_flush_pede=0, g_anom_pede=0;
static char g_anom_motivo[48];
void telemetry_despejo_pendente(void){g_flush_pede=1;}
int telemetry_tem_pendente(void){return (int)(g_flush_pede|g_anom_pede);}
void telemetry_servir_pendentes(void){
    /* 0.2.177: SEM escrita de anomalias durante o jogo -- as anomalias sao
       gravadas no TRAVA (fnwd_dump chama telemetry_anomaly_exec la'). */
    if(g_flush_pede){g_flush_pede=0;telemetry_flush();}
}

/* Escreve direto no arquivo aberto. Varias chamadas no MESMO descritor sao
   seguras (o que nao era seguro, e matou as 0.2.161/0.2.162, era o O_APPEND).
   0.2.176: CONTA-GOTAS -- fatias de 4 KB com 4 ms de intervalo. A 0.2.175 o
   despejo continuou a morrer no usbhdfsd: o produtor PAUSADO nao chega se a
   rajada inteira (40 KB+) entrar de uma vez. Sem rajada, sem veneno. */
static void out_push(const char *p, unsigned int n)
{
    unsigned int done = 0;
    io_gate_lock();
    while (done < n) {
        unsigned int fatia = n - done;
        int r;
        if (fatia > 4096u) fatia = 4096u;
        r = write(out_fd, p + done, fatia);
        if (r <= 0) { io_gate_unlock(); output_error = 1; telemetry_error("out_push.write", r); return; }
        done += (unsigned int)r;
        if (done < n) DelayThread(4000);   /* 4 ms entre fatias: nunca uma rajada */
    }
    io_gate_unlock();
}

/* ---- 0.2.168: REGISTO UNIVERSAL DE ERROS (pedido do utilizador: "tudo") -
   Toda a falha de abertura/leitura/escrita/RPC entra aqui com local e codigo.
   Anel de 64 em RAM + contagem por local (32); sai como linhas ERR, em todo
   despejo de telemetria e no FNF_TRAVA do cao de guarda. NUNCA escreve em
   USB por si (regra 7: o diagnostico nao pode travar o jogo). */
#define ERR_RING 64
#define ERR_SITES 32
typedef struct { unsigned int t_ms; short site; short code; } err_evt_t;
static err_evt_t g_err_ring[ERR_RING];
static unsigned int g_err_wp, g_err_total;
static char g_err_sites[ERR_SITES][24];
static unsigned int g_err_site_n[ERR_SITES];
static unsigned int g_err_site_last[ERR_SITES];
static int g_err_sites_n;
void telemetry_error(const char *onde, long codigo)
{
    short site = -1; int i;
    for (i = 0; i < g_err_sites_n; i++)
        if (!strncmp(g_err_sites[i], onde, 23)) { site = (short)i; break; }
    if (site < 0 && g_err_sites_n < ERR_SITES) {
        site = (short)g_err_sites_n++;
        strncpy(g_err_sites[site], onde, 23); g_err_sites[site][23] = 0;
    }
    if (site >= 0) { g_err_site_n[site]++; g_err_site_last[site] = (unsigned int)codigo; }
    g_err_ring[g_err_wp % ERR_RING].t_ms = segundos_desde_boot() * 1000u;
    g_err_ring[g_err_wp % ERR_RING].site = site;
    g_err_ring[g_err_wp % ERR_RING].code = (short)codigo;
    g_err_wp++; g_err_total++;
}
unsigned int telemetry_error_total(void) { return g_err_total; }
unsigned int telemetry_errors_txt(char *b, unsigned int cap)
{
    unsigned int n = 0, i, start, count; int k;
    if (cap < 64) return 0;
    k = snprintf(b, cap, "# erros total=%u sites=%d\n", g_err_total, g_err_sites_n);
    if (k > 0) n = (unsigned int)k < cap ? (unsigned int)k : cap - 1;
    for (i = 0; i < (unsigned int)g_err_sites_n && n + 64 < cap; i++) {
        k = snprintf(b + n, cap - n, "ERRO_LOCAL,%s,n=%u,ult=%u\n",
                     g_err_sites[i], g_err_site_n[i], g_err_site_last[i]);
        if (k > 0) n += ((unsigned int)k < cap - n) ? (unsigned int)k : (cap - n - 1);
    }
    count = g_err_wp < ERR_RING ? g_err_wp : ERR_RING;
    start = g_err_wp - count;
    for (i = 0; i < count && n + 48 < cap; i++) {
        err_evt_t *e = &g_err_ring[(start + i) % ERR_RING];
        k = snprintf(b + n, cap - n, "ERR,%u,%s,%d\n", e->t_ms,
                     e->site >= 0 ? g_err_sites[e->site] : "?", (int)e->code);
        if (k > 0) n += ((unsigned int)k < cap - n) ? (unsigned int)k : (cap - n - 1);
    }
    return n;
}

/* Abre o arquivo do despejo corrente e escreve o cabecalho.
   Numeracao: o despejo 0 vai para FNF_LOG00 (o boot, guardado para sempre);
   os seguintes giram em FNF_LOG01..31. Assim os primeiros quadros da sessao
   -- que sao onde o boot e a entrada no jogo aparecem -- nunca sao apagados. */
static int out_begin(void)
{
    char path[96];
    char hdr[224];
    unsigned int num;
    int k;
    if (out_fd >= 0) return 0;
    if (log_path_index < 0) return -1;
    num = (log_seq == 0u) ? 0u : (1u + ((log_seq - 1u) % 31u));
    log_path_for(log_path_index, num, path, sizeof(path));
    io_gate_lock(); out_fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666); io_gate_unlock();
    if (out_fd < 0) { output_error = 1; return -1; }
    output_error = 0;   /* 0.2.171: um open que passa recupera o canal */
    k = snprintf(hdr, sizeof(hdr),
        "# FNF PS2 %s | log v2 | despejo %u | arquivo %02u | quadros ate' %u |"
        " anel %u | t=%u ms\n",
        build_name, (unsigned)log_seq, num, (unsigned)frames_logged,
        (unsigned)fring_n(), (unsigned)segundos_desde_boot());
    if (k > 0) out_push(hdr, (unsigned)k);
    return 1;
}

static void out_end(void)
{
    if (out_fd < 0) return;
    io_gate_lock();
    /* 0.2.175: SEM fsync. Medido: o usbhdfsd nao aceita fsync repetido (3
       builds mortas por isso: 0.2.108-110) e o passo que publica o arquivo
       e' o CLOSE (licao 0.2.162: BOOT.txt com open/write/fsync/close chegou
       completo 401 vezes; o log sem close chegou com 0 bytes). O fsync do
       despejo ficou para o TRAVA de emergencia do vigia. */
    close(out_fd);   /* e' o close que publica o tamanho no diretorio do FAT */
    io_gate_unlock();
    out_fd = -1;
}

/* Fecha o despejo: escreve o que sobrou no buffer, sincroniza e fecha. Um
   despejo = UM arquivo. Antes, o buffer de 8 KB rotacionava no meio do
   despejo e 120 quadros gastavam 15 arquivos dos 32 -- trinta segundos de
   historia. Agora cada arquivo tem o despejo inteiro e o cabecalho diz qual
   pedaco da sessao ele cobre. */
static void flush_output(void)
{
    u64 t0, t1;
    if (log_path_index < 0) { output_used = 0; return; }
    if (output_used == 0 && out_fd < 0) return;
    t0 = GetTimerSystemTime();
    /* 0.2.174: O QUADRO NUNCA ESPERA PELA E/S. Se o portao esta' com o
       produtor (leitura em curso -- ou o produtor PRESO dentro do usbhdfsd),
       o despejo SALTA e o buffer fica para a proxima tentativa. Antes isto
       era io_gate_lock() e um produtor preso congelava o jogo para sempre
       (sessoes 0.2.172/0.2.173: freeze sempre a seguir a um despejo). */
    if (!io_gate_trylock()) { g_flush_skip++; return; }
    g_busy = 1;
    if (out_fd < 0 && out_begin() < 0) { g_busy = 0; output_used = 0; io_gate_unlock(); return; }
    if (output_used) { out_push(output_buffer, output_used); output_used = 0; }
    out_end();
    log_seq++;
    flush_count++;
    g_busy = 0;
    io_gate_unlock();
    t1 = GetTimerSystemTime();
    g_flush_us_last = (unsigned int)(((t1 - t0) * 1000000ull) / (u64)kBUSCLK);
}

static void write_text(const char *text)
{
    unsigned int length = (unsigned int)strlen(text);
    unsigned int k;
    for (k = 0; k < length; ++k) {
        output_checksum ^= (unsigned char)text[k];
        output_checksum *= 16777619u;
    }
    /* 0.2.171: o buffer RAM enche SEMPRE; o disco pode falhar e voltar.
       Antes, o primeiro erro de escrita silenciava a telemetria para sempre
       (sessao 0.2.170: Q-lines pararam no f=696 e o freeze ficou cego). */
    while (length > 0) {
        unsigned int available = OUTPUT_BUFFER_SIZE - output_used;
        unsigned int amount;
        if (available == 0) {
            /* 0.2.169: NUNCA abrir/escrever arquivo a meio do quadro. Buffer
               cheio = descartar a linha nova e contar (DROPS no despejo). */
            output_dropped++;
            return;
        }
        amount = length < available ? length : available;
        memcpy(output_buffer + output_used, text, amount);
        output_used += amount;
        text += amount;
        length -= amount;
    }
}

static void write_line(const char *text)
{
    write_text(text);
    if (!output_error) write_text("\n");
}


/* Por que o log vinha vazio: o init carregava iomanX, fileXio, usbd e usbhdfsd
   e DESISTIA no primeiro que falhasse. Se o jogo eh lancado por um gerenciador
   (uLE, OPL, ESR) que ja montou o pendrive, o segundo carregamento do usbd
   falha, o init retorna -3, e o arquivo nunca eh aberto -- enquanto o mass:
   continua funcionando normalmente, porque foi o lancador que o montou. O
   resultado era o pior possivel: jogo rodando, assets carregando, log ausente
   e nenhuma pista do motivo. Agora ele tenta abrir o arquivo DIRETO primeiro,
   so carrega modulo se precisar, nao desiste por falha de modulo, e registra
   o resultado de cada etapa. */
/* Tenta abrir o log em qualquer um dos caminhos. Separada do init porque o
   init NAO PODE ser a unica chance.

   reset_iop() desmonta o pendrive que o lancador montou. Depois de recarregar
   usbd/usbhdfsd, a re-enumeracao do USB levou mais de 5 segundos no console do
   usuario: o init esperava 5 segundos, desistia, e o log nunca era aberto.
   Todas as escritas seguintes viraram no-op silencioso, e a colheita veio com
   0 bytes tres vezes seguidas.

   A prova de que o mass: aparece DEPOIS: o boot_mark() do main.c, que abre o
   PROPRIO arquivo e nao depende desta maquina, escreveu o passo 1 normalmente
   -- porque quando ele rodou ja tinham se passado os 6,5 segundos de espera do
   init e o pendrive tinha montado. O dispositivo aparece; o init eh que tinha
   desistido cedo demais.

   A funcao eh barata -- um punhado de open() que falham rapido -- e por isso
   pode ser chamada de novo mais tarde, sem custo para o jogo. */
static void log_try_open(int *path_index)
{
    int i;
    if (log_fd >= 0) return;
    for (i = 0; log_prefixes[i] && log_fd < 0; ++i) {
        int t;
        int tentativas = (i == 0) ? 50 : 5;
        for (t = 0; t < tentativas; ++t) {
            char path[96];
            log_path_for(i, 0, path, sizeof(path));
            log_fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
            if (log_fd >= 0) {
                /* Guarda SO' o caminho e fecha: quem escreve e' flush_output(),
                   com open/write/fsync/close proprios. log_fd fica como
                   sentinela "aberto" (>=0) para os testes que ja' existiam. */
                close(log_fd);
                log_path_index = i;
                *path_index = i;
                break;
            }
            DelayThread(100000);
        }
    }
}

/* Quantas vezes a reabertura tardia ja foi tentada. Limitada para nao virar
   um laco infinito de open() falhando a cada quadro, para sempre. */
static int log_late_attempts;
#define LOG_LATE_MAX 40

int telemetry_init(const char *build_id, const char *elf_path)
{
    int modules_loaded = 0;
    int module_failures = 0;
    int path_index = -1;

    g_boot_t0 = GetTimerSystemTime();
    record_count = 0;
    render_event_count = 0;
    render_events_dropped = 0;
    flush_count = 0;
    output_used = 0;
    output_error = 0;
    init_result = 0;
    snprintf(build_name, sizeof(build_name), "%s", build_id ? build_id : "unknown");
    snprintf(launch_path, sizeof(launch_path), "%s", elf_path ? elf_path : "unknown");

    /* 1. Carrega os modulos ANTES de tocar em qualquer arquivo, e nessa ordem.
       A tentativa de abrir o mass: antes do fileXioInit() quebrou a build: o
       jogo nao abria, nao saida som, e nada era escrito -- porque abrir um
       caminho de rede sem o RPC inicializado eh chamar um subsistema que ainda
       nao existe. A ordem tem de ser esta, sempre. Falha de modulo nao eh mais
       fatal, mas a ordem nao eh negociovel. */
    sbv_patch_enable_lmb();
    sbv_patch_disable_prefix_check();
    if (exec_irx(iomanX_irx, size_iomanX_irx) < 0) module_failures++;
    if (exec_irx(fileXio_irx, size_fileXio_irx) < 0) module_failures++;
    if (exec_irx(usbd_irx, size_usbd_irx) < 0) module_failures++;
    if (exec_irx(usbhdfsd_irx, size_usbhdfsd_irx) < 0) module_failures++;
    modules_loaded = 1;
    fileXioInit();

    /* 2. Agora sim tenta os caminhos, com repeticao para o caso de o pendrive
       ainda estar sendo montado pelo lancador. */
    /* O primeiro caminho (mass:/FNFPS2/) espera ate 5 segundos, porque pode ser
       que o lancador ainda esteja montando o pendrive. Os outros esperam meio
       segundo cada: sao so um esteio. No total a espera maxima eh de 6,5
       segundos -- antes eram 20, e 20 segundos de tela preta antes de o jogo
       comecar eh indistinguivel de "o jogo nao abriu". */
    log_try_open(&path_index);

    if (log_fd < 0) {
        /* Nao abriu em lugar nenhum. O numero volta codificado para a tela
           poder mostrar: -100 menos a quantidade de modulos que falharam. */
        init_result = -100 - module_failures;
        return init_result;
    }

    usb_ready = 1;
    {
        char line[512];
        /* O cabecalho sai no init, nao no fim: se o port travar no meio, o
           arquivo jah comeca dizendo que build eh este e em que modo. */
        output_checksum = 2166136261u;
        snprintf(line, sizeof(line), "# FNF PS2 TELEMETRIA CONTINUA build=%.63s", build_name);
        write_line(line);
        snprintf(line, sizeof(line), "# elf=%.255s", launch_path);
        write_line(line);
        write_line("# physical=640x448 logical=1280x720 region=NTSC framebuffer=PSMCT32 buffering=double");
        write_line("# scope=title,main_menu,story_menu,tutorial audio=audsrv pcm=s16le stereo=48000Hz");
        write_line("# formato: uma linha por evento; tudo em texto, ordenado por emissao");
        write_line("# colunas estado: quadro,ui,target,sel,nivel,diff,trans_frame,trans,transport,audio_on,audio_tocando,menu_susp,chart,tut_pronta,tut_audio,story_assets,menu_cache,title_anim,gf_up,bf_up,gf16_up,story_gf_up,story_bf_up,aspect169,vp360,music_clock,enviado,ring,underruns,fd_audio,fd_title,acertos,erros,combo,pontos,feedback,cursor_eventos");
        write_line("# colunas quadro: quadro,begin,draw,vsync,draw_ticks,espera_ticks,total,fps,pressed,anim,anim_frame,asset_frame,quads,qwords,ui,trans,ouvido,fila_audio,ring,underruns");
        /* Resultado da abertura: diz se o lancador jah tinha montado o mass:,
           quantos modulos falharam, e em qual caminho o arquivo foi parar. */
        snprintf(line, sizeof(line), "# init ok caminho=%d modulos_carregados=%d falhas_modulo=%d aberto_direto=%d",
                 path_index, modules_loaded, module_failures, modules_loaded ? 0 : 1);
        write_line(line);
        flush_output();
        /* fsync ja feito dentro de flush_output() */
    }
    return 0;
}

/* O que a tela mostra no canto. Se o log nao abriu, isso eh a UNICA pista que
   existe -- e por isso ele precisa estar visivel o tempo todo, nao enterrado
   num arquivo que talvez nao tenha sido criado. */
void telemetry_status(int *result, unsigned int *frames, unsigned int *events,
                      unsigned int *drops, unsigned int *flushes)
{
    if (result) *result = init_result;
    if (frames) *frames = frames_logged;
    if (events) *events = render_event_count;
    if (drops) *drops = render_events_dropped;
    if (flushes) *flushes = flush_count;
}

void telemetry_event(const char *name, int a, int b)
{
    render_event_t *e;
    if (render_event_count >= MAX_RENDER_EVENTS) {
        /* Conta o que foi descartado em vez de engolir em silencio: um buraco
           no log que ninguem ve eh pior que um numero dizendo que houve buraco. */
        render_events_dropped++;
        return;
    }
    e=&render_events[render_event_count++];e->ticks=GetTimerSystemTime();
    snprintf(e->name,sizeof(e->name),"%s",name?name:"unknown");e->a=a;e->b=b;
}

void telemetry_frame(u32 frame, u64 begin, u64 after_draw, u64 after_vsync,
                     unsigned short pressed, int animation, int animation_frame,
                     int asset_frame, u32 textured_quads, int qwords, int state,
                     int transitions, u32 audio_heard, u32 audio_queued,
                     u32 ring_level, u32 underruns)
{
    frame_record_t *r;
    if (record_count >= MAX_FRAMES) return;
    r = &records[record_count++];
    r->frame = frame;
    r->begin = begin;
    r->draw_done = after_draw;
    r->vsync_done = after_vsync;
    r->pressed = (u16)pressed;
    r->animation = animation;
    r->animation_frame = animation_frame;
    r->asset_frame = asset_frame;
    r->textured_quads = textured_quads;
    r->qwords = (u16)qwords;
    r->state = (u16)state;
    r->transitions = (u16)transitions;
    r->audio_heard = audio_heard;
    r->audio_queued = audio_queued;
    r->ring_level = ring_level;
    r->underruns = underruns;
}

void telemetry_state(u32 frame, int ui_state, int target_state, int selection,
                     int story_level, int story_diff, int transition_frame,
                     int transitions, int transport_state, int audio_started,
                     int audio_playing, int menu_audio_suspended,
                     int chart_status, int tutorial_scene_ready,
                     int tutorial_audio_status, int story_assets_ready,
                     int menu_cache_ready, int title_anim_status,
                     int gf_uploaded, int bf_uploaded, int gf16_uploaded,
                     int story_gf_uploaded, int story_bf_uploaded,
                     int aspect_169, int viewport_360,
                     u32 music_clock, u32 audio_submitted, u32 ring_level,
                     u32 underruns, int audio_fd, int title_audio_fd,
                     int note_hits, int note_misses, int note_combo,
                     int note_score, int note_feedback, int event_cursor,
                     u32 band_clip_rects, u32 band_clip_lines,
                     u32 band_cull_rects)
{
    char line[512];
    /* A mesma segunda chance, uma vez por quadro. Se o pendrive montar depois
       do boot, o log passa a sair a partir do quadro em que isso acontecer, em
       vez de ficar mudo para sempre. */
    if (log_fd < 0 && !output_error && log_late_attempts < LOG_LATE_MAX) {
        int dummy = -1;
        log_late_attempts++;
        log_try_open(&dummy);
    }
    if (!usb_ready || log_fd < 0) return;
    frames_logged++;
    snprintf(line, sizeof(line),
        "S,%u,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%u,%u,%u,%u,%d,%d,%d,%d,%d,%d,%d,%d,%u,%u,%u",
        frame, ui_state, target_state, selection, story_level, story_diff,
        transition_frame, transitions, transport_state, audio_started,
        audio_playing, menu_audio_suspended, chart_status, tutorial_scene_ready,
        tutorial_audio_status, story_assets_ready, menu_cache_ready,
        title_anim_status, gf_uploaded, bf_uploaded, gf16_uploaded,
        story_gf_uploaded, story_bf_uploaded, aspect_169, viewport_360,
        music_clock, audio_submitted, ring_level, underruns, audio_fd,
        title_audio_fd, note_hits, note_misses, note_combo, note_score,
        note_feedback, event_cursor,
        /* Os tres ultimos: o recorte por software da faixa visivel. Sem eles
           o log nao explica o que aconteceu com um sprite que saia da faixa --
           que era exatamente o defeito das capturas do console. */
        band_clip_rects, band_clip_lines, band_cull_rects);
    write_line(line);
}

/* Linha de volume: acumula no buffer e nao escreve. Uma escrita no mass: pelo
   fileXio eh um RPC sincrono para o IOP, que escreve um bloco no USB -- custa
   de poucos a muitos milissegundos. A lista de sprites de um quadro pode ter
   centenas de linhas; chamar telemetry_raw() para cada uma delas colocava
   centenas de escritas bloqueantes dentro do laco de render, e foi isso que
   deixou a 0.2.108 sem abrir no console. Aqui a linha custa um memcpy. */
void telemetry_line(const char *text)
{
    if (!usb_ready || log_fd < 0 || output_error) return;
    write_text(text);
    if (!output_error) write_text("\n");
}

/* Linha crua, escrita NA HORA.
   O buffer de 8 KB servia para nao custar uma chamada de escrita por linha,
   mas cobrava caro justamente quando o log importa: se o jogo pendura, tudo
   que ainda estava no buffer desaparece. Foi assim que a ultima colheita veio
   com 0 bytes -- o arquivo existia, criado pelo open, e nada dentro. Aqui a
   linha sai no mesmo instante em que eh gerada, e quem chua decide o ritmo. */
void telemetry_raw(const char *text)
{
    if (!usb_ready || log_fd < 0 || output_error) return;
    write_text(text);
    write_text("\n");
    flush_output();
    /* Sincroniza aqui, e so aqui e em telemetry_phase(): as duas sao chamadas
       poucas vezes na vida do processo -- os marcos de boot, um cabecalho. Se
       o jogo pendurar nessa altura, a linha TEM de estar no cartao, porque o
       usuario vai resetar e nunca vai haver um close() limpo. Como sao raras,
       o fsync nao custa nada. */
}

/* Marco de fase, com o mesmo descritor unico. A versao anterior abria e
   fechava o arquivo a cada evento, com fsync no meio: dois escritores no
   mesmo arquivo, um com descritor longo e buffer, outro abrindo e truncando.
   No pendrive isso nao sobrevive. */
void telemetry_phase(const char *phase, int row, int code)
{
    char line[192];
    /* Segunda chance: se o init desistiu cedo demais, eh aqui. telemetry_phase()
       eh chamado poucas vezes no boot e quase nunca durante o jogo, entao
       tentar de novo nao custa nada. */
    if (log_fd < 0 && !output_error && log_late_attempts < LOG_LATE_MAX) {
        int dummy = -1;
        log_late_attempts++;
        log_try_open(&dummy);
    }
    if (!usb_ready || log_fd < 0 || output_error) return;
    snprintf(line, sizeof(line), "P,%s,%d,%d", phase ? phase : "?", row, code);
    write_line(line);
    /* 0.2.166: SEM flush aqui. Cada chamada a flush_output() cria um arquivo
       novo (um despejo = um arquivo), e os marcos de fase aconteciam varias
       vezes por segundo: foi assim que a 0.2.164 deixou 32 ficheiros com UMA
       linha cada e perdeu a historia toda. A linha sai no proximo despejo
       periodico (5 s) ou no despejo de anomalia, quando e' que importa. */
}

void telemetry_io(const char *op, const char *path, long result, long detail)
{
    char line[512];
    if (!usb_ready) return;
    snprintf(line, sizeof(line), "IO,%s,%s,%ld,%ld",
             op ? op : "?", path ? path : "?", result, detail);
    write_line(line);
}

void telemetry_flush(void)
{
    char line[512];
    u32 i;
    if (!usb_ready) return;

    /* 0.2.169: F-records FORA do despejo (o anel Q cobre os quadros).
       Corta o despejo de ~118 KB para ~60 KB. */
    record_count = 0;
    for (i = 0; i < 0 && !output_error; ++i) {
        frame_record_t *r = &records[i];
        u64 draw_ticks = r->draw_done - r->begin;
        u64 wait_ticks = r->vsync_done - r->draw_done;
        u64 total = r->vsync_done - r->begin;
        unsigned int fps100 = total ? (unsigned int)(((u64)kBUSCLK * 100ULL) / total) : 0;
        snprintf(line, sizeof(line), "F,%u,%llu,%llu,%llu,%llu,%llu,%llu,%u.%02u,0x%04x,%d,%d,%d,%u,%u,%u,%u,%u,%u,%u,%u\n",
            r->frame, r->begin, r->draw_done, r->vsync_done, draw_ticks, wait_ticks, total,
            fps100 / 100, fps100 % 100, r->pressed, r->animation, r->animation_frame,
            r->asset_frame, r->textured_quads, r->qwords, r->state, r->transitions,
            r->audio_heard, r->audio_queued, r->ring_level, r->underruns);
        write_text(line);
        write_text("\n");
    }
    record_count = 0;

    for (i = 0; i < render_event_count && !output_error; ++i) {
        snprintf(line, sizeof(line), "E,%llu,%s,%d,%d",
                 render_events[i].ticks, render_events[i].name,
                 render_events[i].a, render_events[i].b);
        write_line(line);
    }
    if (render_events_dropped) {
        snprintf(line, sizeof(line), "E-DROP,%u eventos descartados por excesso de capacidade", render_events_dropped);
        write_line(line);
        render_events_dropped = 0;
    }
    render_event_count = 0;

    flush_output();
    /* fsync NO MAXIMO A CADA 30 QUADROS -- duas vezes por segundo.
       A versao que foi para o console sincronizava EM TODO QUADRO: 60 fsync
       por segundo no mesmo descritor. O usbhdfsd nao aceita fsync repetido, e
       cada uma forca um flush do USB que custa de dezenas a centenas de
       milissegundos. O IOP passava mais tempo sincronizando o pendrive do que
       servindo o jogo, o fluxo de dados parava, e o jogo nao abriu -- tres
       builds seguidas, 0.2.108, 0.2.109 e mais esta. Duas sincronizacoes por
       segundo garantem que um reset nao perca mais de meio segundo de log, e
       o custo de sincronizacao cai para menos de 5% do tempo de quadro. */
    if (!output_error && (flush_count % 30u) == 0u) /* fsync ja feito dentro de flush_output() */
    flush_count++;
}

int telemetry_finish(void)
{
    char line[512];
    if (!usb_ready) return -1;
    telemetry_flush();
    if (output_error) return -3;
    {
        unsigned int data_checksum = output_checksum;
        snprintf(line, sizeof(line), "# END flushes=%u fnv1a32=%08x write_error=%d",
                 flush_count, data_checksum, output_error);
        write_line(line);
    }
    flush_output();
    /* fsync ja feito dentro de flush_output() */
    close(log_fd);
    log_fd = -1;
    usb_ready = 0;
    return (int)record_count;
}

/* ===================================================================== */
/*  LOG MASSIVO v2 -- 0.2.165                                           */
/* =====================================================================
 *
 * O que este bloco grava, e por que:
 *
 *  Q  uma linha por quadro, com o tempo de CADA fase (entrada, atualizacao,
 *     envio de DMA, espera do desenho, captura, vsync, pos), o tamanho da
 *     lista de desenho (qwords), os sprites desenhados, os binds de textura,
 *     os bytes subidos para a VRAM, o recorte por software, o estado da
 *     maquina de estados, o pad, o relogio da musica, o anel de audio, a
 *     VRAM livre e a RAM dos buffers do port. E' o "porque" do lag: cada
 *     numero e' um pedaco do quadro, e o que crescer e' o culpado.
 *  E  cada evento do jogo (troca de estado, falha de palco, textura aberta,
 *     pause, captura...), com quadro e instante.
 *  A  o ANEL INTEIRO (512 quadros) despejado num arquivo proprio quando um
 *     quadro passa de 250 ms ou quando o cao de guarda ve o jogo parado.
 *  H  histograma de tempos, os 5 piores quadros e o custo dos despejos.
 *
 * Tudo isso entra no MESMO fluxo de arquivos numerados, entao o que estiver
 * no cartao depois de um reset e' sempre a cauda da sessao.
 */
static unsigned int g_slow_frames;
static unsigned int g_max_frame_us;
static unsigned int g_hist[8];          /* <8,16,25,33,50,100,250,>=250 ms */
static fnf_frame_t g_worst[5];
static unsigned int g_worst_n;
static unsigned int g_up_bytes_frame;
static unsigned int g_anom_files;
static u64 g_anom_last;
static unsigned int g_und_anterior;
static char g_boot_buf[12288];
static unsigned int g_boot_used;

static unsigned int agora_us(void) __attribute__((unused));
static unsigned int agora_us(void)
{
    return (unsigned int)((GetTimerSystemTime() * 1000000ull) / (u64)kBUSCLK);
}

static void hist_add(unsigned int ms_x10)
{
    unsigned int i = 0;
    if (ms_x10 < 80u) i = 0;
    else if (ms_x10 < 160u) i = 1;
    else if (ms_x10 < 250u) i = 2;
    else if (ms_x10 < 330u) i = 3;
    else if (ms_x10 < 500u) i = 4;
    else if (ms_x10 < 1000u) i = 5;
    else if (ms_x10 < 2500u) i = 6;
    else i = 7;
    g_hist[i]++;
}

void telemetry_note_upload(unsigned int bytes) { g_up_bytes_frame += bytes; }
u32 telemetry_last_flush_us(void) { return g_flush_us_last; }

static void q_line(char *b, unsigned int tam, const fnf_frame_t *f)
{
    snprintf(b, tam,
        "Q,f=%u,t=%u,in=%u,upd=%u,dma=%u,wait=%u,cap=%u,vs=%u,post=%u,fl=%u,"
        "qw=%u,qd=%u,bx=%u,upn=%u,upk=%u,ovf=%u,cl=%u,cu=%u,"
        "st=%u,tr=%u,sel=%u,pad=%04X,prs=%04X,an=%d,af=%d,"
        "clk=%u,q=%u,r=%u,un=%u,eof=%u,pl=%u,"
        "vr=%u,rb=%u,ek=%u,er=%u,ck=%d,so=%u,lr=%u",
        (unsigned)f->frame, (unsigned)f->t_total, (unsigned)f->t_in,
        (unsigned)f->t_upd, (unsigned)f->t_dma, (unsigned)f->t_wait,
        (unsigned)f->t_cap, (unsigned)f->t_vsync, (unsigned)f->t_post,
        (unsigned)f->t_flush,
        (unsigned)f->qwords, (unsigned)f->quads, (unsigned)f->binds,
        (unsigned)f->uploads, (unsigned)f->up_kb, (unsigned)f->ovf,
        (unsigned)f->clip, (unsigned)f->cull,
        (unsigned)f->st, (unsigned)f->tr, (unsigned)f->sel,
        (unsigned)f->pad, (unsigned)f->pressed,
        (int)f->anim, (int)f->anim_frame,
        (unsigned)f->clk, (unsigned)f->queued, (unsigned)f->ring,
        (unsigned)f->underruns, (unsigned)f->eof, (unsigned)f->paused,
        (unsigned)f->vram_free, (unsigned)f->ram_bufs, (unsigned)f->io_kb,
        telemetry_error_total(), audio_ring_stack_ok(),
        (unsigned)f->sub_ok, (unsigned)f->lr_bytes);
}

void telemetry_frame2(const fnf_frame_t *f)
{
    fnf_frame_t r = *f;
    char b[448];

    r.t_flush = g_flush_us_last;
    r.uploads = (u16)(g_up_bytes_frame > 0u ? 1u : 0u);
    r.up_kb = (u16)(g_up_bytes_frame >> 10);
    g_up_bytes_frame = 0;

    g_fring[g_fring_wp % FRAME_RING] = r;
    g_fring_wp++;
    if (g_fring_count < FRAME_RING) g_fring_count++;
    frames_logged++;

    hist_add(r.t_total / 100u);          /* decimos de ms */
    if (r.t_total > g_max_frame_us) g_max_frame_us = r.t_total;
    if (r.t_total > 25000u) g_slow_frames++;

    /* Os cinco piores quadros da sessao, com a reparticao do tempo. */
    if (g_worst_n < 5u || r.t_total > g_worst[g_worst_n - 1u].t_total) {
        unsigned int pos = g_worst_n < 5u ? g_worst_n : 4u;
        while (pos > 0u && g_worst[pos - 1u].t_total < r.t_total) {
            g_worst[pos] = g_worst[pos - 1u];
            pos--;
        }
        g_worst[pos] = r;
        if (g_worst_n < 5u) g_worst_n++;
    }

    if (!usb_ready || log_fd < 0) {
        if (r.t_total > 250000u) telemetry_anomaly("quadro_lento_sem_cartao");
        return;
    }

    /* Linha de fluxo: 10 Hz, mais TODOS os quadros lentos ou com novidade
       (overflow de lista, seca de audio, upload de textura). Assim o arquivo
       fica pequeno no caminho bom e completo no caminho ruim. */
    if ((r.frame % 6u) == 0u || r.t_total > 25000u || r.ovf ||
        r.underruns != g_und_anterior || r.up_kb > 0u) {
        q_line(b, sizeof(b), &r);
        write_line(b);
        g_und_anterior = r.underruns;
    }

    if (r.t_total > 250000u) telemetry_anomaly("quadro_acima_de_250ms");
}

void telemetry_stats(u32 *frames, u32 *slow, u32 *max_us, u32 *hist)
{
    int i;
    if (frames) *frames = frames_logged;
    if (slow) *slow = g_slow_frames;
    if (max_us) *max_us = g_max_frame_us;
    if (hist) for (i = 0; i < 8; i++) hist[i] = g_hist[i];
}

/* Despeja o anel inteiro num arquivo proprio. Nao passa pelo buffer comum: se
   o jogo esta' travado, o despejo tem de sair AGORA, inteiro, com fsync. */
/* 0.2.175: o publico apenas ENFILEIRA (seguro em qualquer contexto, inclusive
   o laco do quadro). A escrita real e' telemetry_anomaly_exec, chamada pela
   thread escritora com o produtor pausado. */
void telemetry_anomaly(const char *motivo)
{
    if (motivo) { strncpy(g_anom_motivo, motivo, sizeof(g_anom_motivo)-1);
                  g_anom_motivo[sizeof(g_anom_motivo)-1] = 0; }
    else g_anom_motivo[0] = 0;
    g_anom_pede = 1;
}

void telemetry_anomaly_exec(const char *motivo)
{
    char path[96];
    char b[448];
    int fd, k;
    u64 agora = GetTimerSystemTime();
    unsigned int i, n;

    /* 0.2.177: se houve anomalia registada durante o jogo (fila em RAM),
       o ficheiro do TRAVA leva o MOTIVO REAL dela, nao o do vigia. */
    if (g_anom_pede && g_anom_motivo[0]) { motivo = g_anom_motivo; g_anom_pede = 0; }

    if (log_path_index < 0) return;
    if (g_busy) return;                       /* o laco do jogo esta' escrevendo */
    if (g_anom_files >= 12u) return;          /* teto: nao enche o cartao */
    if (g_anom_last && (agora - g_anom_last) < (u64)kBUSCLK) return;  /* 1 por segundo */

    /* 0.2.174: portao + nunca bloquear. Sem portao esta' escrita corria em
       paralelo com o produtor (usbhdfsd nao reentrante); com espera ficava
       presa para sempre quando o driver encrava. Falhou o portao = SALTA. */
    if (!io_gate_trylock()) return;
    g_busy = 1;
    g_anom_last = agora;
    {
        unsigned int num = g_anom_files;
        /* Mesmo diretorio dos logs, nome proprio. */
        static const char *const dirs[4] = { "mass:/FNFPS2/", "mass:/", "host:/", "mc0:/" };
        snprintf(path, sizeof(path), "%sFNF_ANOM_%02u.txt",
                 (log_path_index >= 0 && log_path_index < 4) ? dirs[log_path_index] : "mass:/",
                 num);
    }
    fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0) { g_busy = 0; io_gate_unlock(); return; }
    g_anom_files++;

    k = snprintf(b, sizeof(b),
        "# ANOMALIA: %s | %s | t=%u ms | anel=%u | quadros=%u lentos=%u pior=%u us\n",
        motivo ? motivo : "?", build_name, segundos_desde_boot() * 1000u,
        fring_n(), frames_logged, g_slow_frames, g_max_frame_us);
    if (k > 0) { int w = write(fd, b, (u32)k); if (w <= 0) { close(fd); g_busy = 0; io_gate_unlock(); return; } }

    n = fring_n();
    for (i = 0; i < n; i++) {
        unsigned int idx = (g_fring_wp + FRAME_RING - n + i) % FRAME_RING;
        const fnf_frame_t *r = &g_fring[idx];
        q_line(b, sizeof(b), r);
        { int w = write(fd, b, (u32)strlen(b));
          char nl = '\n';
          if (w <= 0) break;
          if (write(fd, &nl, 1) <= 0) break; }
    }
    fsync(fd);
    close(fd);
    g_busy = 0;
    io_gate_unlock();
}

void telemetry_snapshot(const char *tag, const char *texto)
{
    char b[220];
    const char *p;
    if (!usb_ready || log_fd < 0 || output_error) return;
    snprintf(b, sizeof(b), "# --- SNAPSHOT %s | t=%u ms ---", tag ? tag : "?",
             segundos_desde_boot() * 1000u);
    write_line(b);
    for (p = texto; p && *p; ) {
        const char *nl = p;
        while (*nl && *nl != '\n') nl++;
        if (nl > p) {
            unsigned int len = (unsigned int)(nl - p);
            if (len > 200u) len = 200u;
            write_text("# ");
            { char tmp[202]; memcpy(tmp, p, len); tmp[len] = 0; write_line(tmp); }
        }
        p = (*nl == '\n') ? nl + 1 : nl;
    }
}

void telemetry_boot(const char *linha)
{
    unsigned int len;
    if (!linha) return;
    len = (unsigned int)strlen(linha);
    if (g_boot_used + len + 2u >= sizeof(g_boot_buf)) return;
    memcpy(g_boot_buf + g_boot_used, linha, len);
    g_boot_used += len;
    g_boot_buf[g_boot_used++] = '\n';
}

/* FNF_SESSAO.txt: reescrito inteiro (open O_TRUNC + write + fsync + close).
   Chamado no fim do boot e em cada troca de estado -- poucas vezes, nunca no
   laco. E' o arquivo que responde "em que maquina isto rodou". */
void telemetry_boot_flush(void)
{
    char path[96];
    static const char *const dirs[4] = { "mass:/FNFPS2/", "mass:/", "host:/", "mc0:/" };
    int fd, k;
    if (log_path_index < 0) return;
    snprintf(path, sizeof(path), "%sFNF_SESSAO.txt",
             (log_path_index >= 0 && log_path_index < 4) ? dirs[log_path_index] : "mass:/");
    fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0) return;
    k = (int)g_boot_used;
    { int done = 0;
      while (done < k) { int w = write(fd, g_boot_buf + done, (u32)(k - done));
                         if (w <= 0) break; done += w; } }
    fsync(fd);
    close(fd);
}
