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

/* Inicializa o cliente RPC fileXio; as operações de arquivo em si usam a
 * camada POSIX/newlib, como exige esta versão do PS2SDK. */
extern int fileXioInit(void);

#define MAX_FRAMES 18000
#define LOG_PATH "mass:/FNFPS2/FNF_COLHEITA.txt"
#define RENDER_LOG_PATH "mass:/FNFPS2/FNF_COLHEITA.txt"
#define MAX_RENDER_EVENTS 2048

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

int telemetry_init(const char *build_id, const char *elf_path)
{
    int i;
    record_count = 0;
    render_event_count = 0;
    snprintf(build_name, sizeof(build_name), "%s", build_id ? build_id : "unknown");
    snprintf(launch_path, sizeof(launch_path), "%s", elf_path ? elf_path : "unknown");

    sbv_patch_enable_lmb();
    sbv_patch_disable_prefix_check();
    if (exec_irx(iomanX_irx, size_iomanX_irx) < 0) return -1;
    if (exec_irx(fileXio_irx, size_fileXio_irx) < 0) return -2;
    if (exec_irx(usbd_irx, size_usbd_irx) < 0) return -3;
    if (exec_irx(usbhdfsd_irx, size_usbhdfsd_irx) < 0) return -4;
    fileXioInit();

    for (i = 0; i < 50; ++i) {
        int fd = open(LOG_PATH, O_WRONLY | O_CREAT | O_APPEND, 0666);
        if (fd >= 0) {
            const char *probe = "# FNF PS2 telemetry initialized\n";
            write(fd, probe, strlen(probe));
            close(fd);
            usb_ready = 1;
            return 0;
        }
        DelayThread(100000);
    }
    return -5;
}

void telemetry_event(const char *name, int a, int b)
{
    render_event_t *e;
    if (render_event_count >= MAX_RENDER_EVENTS) return;
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

/* usbhdfsd proved more reliable on the SCPH-90006 with small committed blocks. */
#define OUTPUT_BUFFER_SIZE 4096
static char output_buffer[OUTPUT_BUFFER_SIZE];
static unsigned int output_used;
static unsigned int output_checksum;
static int output_error;

static void flush_output(int fd)
{
    unsigned int done = 0;
    while (done < output_used) {
        int result = write(fd, output_buffer + done, output_used - done);
        if (result <= 0) { output_error = 1; break; }
        done += (unsigned int)result;
    }
    /* usbhdfsd não aceita fsync repetido entre blocos de forma confiável.
     * A sincronização é feita uma única vez depois do rodapé. */
    output_used = 0;
}

static void write_text(int fd, const char *text)
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
        if (available == 0) { flush_output(fd); available = OUTPUT_BUFFER_SIZE; }
        amount = length < available ? length : available;
        memcpy(output_buffer + output_used, text, amount);
        output_used += amount;
        text += amount;
        length -= amount;
    }
}

int telemetry_finish(void)
{
    int fd;
    u32 i;
    char line[512];
    if (!usb_ready) return -1;
    fd = open(LOG_PATH, O_WRONLY | O_CREAT | O_APPEND, 0666);
    if (fd < 0) return -2;
    output_used = 0;
    output_error = 0;
    output_checksum = 2166136261u;

    write_text(fd, "# FNF PS2 MENU TELEMETRY 0.2.13\n");
    snprintf(line, sizeof(line), "# build=%.63s\n", build_name); write_text(fd, line);
    snprintf(line, sizeof(line), "# elf=%.255s\n", launch_path); write_text(fd, line);
    write_text(fd, "# physical=640x448 logical=1280x720 region=NTSC framebuffer=PSMCT32 buffering=double\n");
    write_text(fd, "# scope=title,main_menu,story_menu,tutorial audio=audsrv pcm=s16le stereo=48000Hz\n");
    write_text(fd, "# frame_data=timing,input,animation,render_cost,ui_state,audio_queue_and_ring\n");
    snprintf(line, sizeof(line), "# timer_hz=%d records=%u max_records=%d\n",
             kBUSCLK, record_count, MAX_FRAMES); write_text(fd, line);
    write_text(fd, "frame,begin_ticks,draw_done_ticks,vsync_done_ticks,draw_ticks,vsync_wait_ticks,total_ticks,fps_est,pressed,animation,animation_frame,asset_frame,textured_quads,qwords,state,transitions,audio_heard_samples,audio_queued_bytes,ring_level_bytes,underruns\n");

    for (i = 0; i < record_count && !output_error; ++i) {
        frame_record_t *r = &records[i];
        u64 draw_ticks = r->draw_done - r->begin;
        u64 wait_ticks = r->vsync_done - r->draw_done;
        u64 total = r->vsync_done - r->begin;
        unsigned int fps100 = total ? (unsigned int)(((u64)kBUSCLK * 100ULL) / total) : 0;
        snprintf(line, sizeof(line), "%u,%llu,%llu,%llu,%llu,%llu,%llu,%u.%02u,0x%04x,%d,%d,%d,%u,%u,%u,%u,%u,%u,%u,%u\n",
            r->frame, r->begin, r->draw_done, r->vsync_done, draw_ticks, wait_ticks, total,
            fps100 / 100, fps100 % 100, r->pressed, r->animation, r->animation_frame,
            r->asset_frame, r->textured_quads, r->qwords, r->state, r->transitions,
            r->audio_heard, r->audio_queued, r->ring_level, r->underruns);
        write_text(fd, line);
    }
    {
        unsigned int data_checksum = output_checksum;
        snprintf(line, sizeof(line), "# END records=%u fnv1a32=%08x write_error=%d\n",
                 record_count, data_checksum, output_error);
        write_text(fd, line);
    }
    flush_output(fd);
    fsync(fd);
    close(fd);
    {
        int rfd=open(RENDER_LOG_PATH,O_WRONLY|O_CREAT|O_APPEND,0666);u32 j;
        if(rfd>=0){int n=snprintf(line,sizeof(line),"# build=%s events=%u capacity=%u\nticks,event,a,b\n",build_name,render_event_count,(u32)MAX_RENDER_EVENTS);write(rfd,line,n);
            for(j=0;j<render_event_count;j++){n=snprintf(line,sizeof(line),"%llu,%s,%d,%d\n",render_events[j].ticks,render_events[j].name,render_events[j].a,render_events[j].b);write(rfd,line,n);}fsync(rfd);close(rfd);}
    }
    return output_error ? -3 : (int)record_count;
}
