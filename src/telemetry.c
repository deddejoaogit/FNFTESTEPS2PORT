#include "telemetry.h"
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
#define LOG_PATH "mass:/FNFPS2/FNF_COLHEITA.txt"
#define RENDER_LOG_PATH "mass:/FNFPS2/FNF_COLHEITA.txt"
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
#define OUTPUT_BUFFER_SIZE 8192
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
static void flush_output(void)
{
    unsigned int done = 0;
    if (log_fd < 0 || output_used == 0) { output_used = 0; return; }
    while (done < output_used) {
        int result = write(log_fd, output_buffer + done, output_used - done);
        /* result == 0 nao eh "escreveu zero, tenta de novo": no fileXio quer
           dizer falha. Contado como erro, senao este laco nao termina nunca. */
        if (result <= 0) { output_error = 1; break; }
        done += (unsigned int)result;
    }
    output_used = 0;
}

static void write_text(const char *text)
{
    unsigned int length = (unsigned int)strlen(text);
    unsigned int k;
    for (k = 0; k < length; ++k) {
        output_checksum ^= (unsigned char)text[k];
        output_checksum *= 16777619u;
    }
    while (length > 0 && !output_error) {
        unsigned int available = OUTPUT_BUFFER_SIZE - output_used;
        unsigned int amount;
        if (available == 0) { flush_output(); available = OUTPUT_BUFFER_SIZE; }
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

static const char *const log_paths[] = {
    "mass:/FNFPS2/FNF_COLHEITA.txt",
    "mass:/FNF_COLHEITA.txt",
    "host:/FNF_COLHEITA.txt",
    "mc0:/FNF_COLHEITA.txt",
    0
};

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
    for (i = 0; log_paths[i] && log_fd < 0; ++i) {
        int t;
        int tentativas = (i == 0) ? 50 : 5;
        for (t = 0; t < tentativas; ++t) {
            log_fd = open(log_paths[i], O_WRONLY | O_CREAT | O_APPEND, 0666);
            if (log_fd >= 0) { *path_index = i; break; }
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
        fsync(log_fd);
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
    if (!usb_ready || log_fd < 0 || output_error) return;
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
    if (!output_error) fsync(log_fd);
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
    flush_output();
    if (!output_error) fsync(log_fd);
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

    for (i = 0; i < record_count && !output_error; ++i) {
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
    if (!output_error && (flush_count % 30u) == 0u) fsync(log_fd);
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
    fsync(log_fd);
    close(log_fd);
    log_fd = -1;
    usb_ready = 0;
    return (int)record_count;
}
