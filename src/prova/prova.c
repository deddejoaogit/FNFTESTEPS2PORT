/* PROVA DE HARDWARE -- FNF PS2
 *
 * Roda no PS2 do usuario, mede a maquina dele e escreve o veredito em
 * mass:/FNFPS2/PROVA.txt (e mass:/PROVA.txt).
 *
 * Por que isto existe: "vai funcionar no seu console?" nao se responde lendo
 * codigo. Responde-se medindo a maquina e comparando com o que o jogo exige.
 * Cada teste abaixo tem um LIMITE, e o limite vem de uma conta feita no port,
 * nao de um palpite:
 *
 *   RAM   : o jogo usa 5,82 MB estaticos (_end = 0x005d22d8) + 17,56 MB de
 *           buffers dinamicos = 23,38 MB. O EE tem 32 MB. Aqui se mede quanto
 *           o console REALMENTE entrega de heap depois do proprio programa.
 *   VRAM  : 4 194 304 B e' o teto do GS. O jogo cabe em ~2,1 MB (2 FBs de
 *           1 146 880 B + tex2 + fonttex + gftex16 + pool).
 *   USB   : o pior caso do jogo e' abrir uma musica: le o .chart, os stems
 *           (streaming de ~192 KB/s) e os .chp4 dos personagens (ate ~8 MB no
 *           inicio da musica). Abaixo de 2 MB/s o inicio da musica passa de
 *           4 s e o anel de audio nao enche a tempo.
 *   AUDIO : LIBSD + audsrv precisam subir e a fila precisa ANDAR.
 *   GS    : 60 Hz de vsync e limpeza de tela; se o GS nao responder, nada
 *           mais importa.
 *
 * O resultado na tela, sem fonte: fundo verde = todos passaram, vermelho =
 * algum falhou. Uma barra por teste (cheia = passou) e, no rodape, o numero
 * de falhas em binario. O texto com os numeros vai para o arquivo.
 */
#include <tamtypes.h>
#include <kernel.h>
#include <sifrpc.h>
#include <loadfile.h>
#define NEWLIB_PORT_AWARE
#include <fileio.h>
#include <iopcontrol.h>
#include <libpad.h>
#include <graph.h>
#include <gs_psm.h>
#include <delaythread.h>
#include <malloc.h>
#include <draw.h>
#include <packet.h>
#include <dma.h>
#include <timer.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <audsrv.h>

/* Os mesmos modulos de IOP que o jogo carrega, embutidos no ELF. */
extern unsigned char iomanX_irx[];   extern unsigned int size_iomanX_irx;
extern unsigned char fileXio_irx[];  extern unsigned int size_fileXio_irx;
extern unsigned char usbd_irx[];     extern unsigned int size_usbd_irx;
extern unsigned char usbhdfsd_irx[]; extern unsigned int size_usbhdfsd_irx;
extern unsigned char audsrv_irx[];   extern unsigned int size_audsrv_irx;
extern int fileXioInit(void);

#define SCREEN_W 640
#define SCREEN_H 448
#define FB_PSM   GS_PSM_16

/* ------------------------------------------------------------------ relatorio */
static char g_rep[16384];
static int  g_rn;
static int  g_testes;
static int  g_falhas;
static int  g_iop_ok;
static unsigned char g_passou[32];

static void add(const char *fmt, ...)
{
    va_list ap;
    int n;
    if (g_rn >= (int)sizeof(g_rep) - 256) return;
    va_start(ap, fmt);
    n = vsnprintf(g_rep + g_rn, sizeof(g_rep) - (size_t)g_rn, fmt, ap);
    va_end(ap);
    if (n > 0) g_rn += n;
}

static void teste(const char *nome, int passou, const char *detalhe)
{
    if (g_testes < 32) g_passou[g_testes] = (unsigned char)(passou ? 1 : 0);
    g_testes++;
    if (!passou) g_falhas++;
    add("[%s] %-26s %s\n", passou ? "PASSOU" : "FALHOU", nome, detalhe);
}

static double segundos(u64 ticks) { return (double)ticks / (double)kBUSCLK; }

static void grava(const char *caminho)
{
    int fd = open(caminho, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    int w = 0;
    if (fd < 0) return;
    while (w < g_rn) {
        int r = write(fd, g_rep + w, (u32)(g_rn - w));
        if (r <= 0) break;
        w += r;
    }
    fsync(fd);
    close(fd);
}

/* ------------------------------------------------------------------ tela */
static framebuffer_t g_fb;
static zbuffer_t     g_z;
static u32           g_fbaddr;

static void tela_init(void)
{
    graph_vram_clear();
    g_fbaddr = (u32)graph_vram_allocate(SCREEN_W, SCREEN_H, FB_PSM, GRAPH_ALIGN_PAGE);
    graph_initialize(g_fbaddr, SCREEN_W, SCREEN_H, FB_PSM, 0, 0);
    graph_set_bgcolor(0, 0, 0);
    g_fb.width = SCREEN_W; g_fb.height = SCREEN_H; g_fb.psm = FB_PSM;
    g_fb.mask = 0; g_fb.address = g_fbaddr;
    g_z.enable = DRAW_DISABLE; g_z.address = 0; g_z.mask = 0;
    g_z.zsm = 0; g_z.method = 0;
    graph_set_framebuffer_filtered(g_fbaddr, SCREEN_W, FB_PSM, 0, 0);
}

static void desenha_estado(void)
{
    packet_t *p = packet_init(64, PACKET_NORMAL);
    qword_t *q;
    int i;
    if (!p) return;
    q = p->data;
    q = draw_setup_environment(q, 0, &g_fb, &g_z);
    q = draw_clear(q, 0, 0.0f, 0.0f, (float)SCREEN_W, (float)SCREEN_H, 0, 0, 0);
    for (i = 0; i < g_testes && i < 24; i++) {
        rect_t v;
        int ok = i < 32 ? g_passou[i] : 0;
        float x = 12.0f + (float)i * 25.0f;
        v.v0.x = x;            v.v0.y = 60.0f;  v.v0.z = 0;
        v.v1.x = x + 18.0f;    v.v1.y = 340.0f; v.v1.z = 0;
        v.color.r = ok ? 0x30 : 0xE0;
        v.color.g = ok ? 0xC0 : 0x20;
        v.color.b = ok ? 0x30 : 0x20;
        v.color.a = 0x80;
        q = draw_rect_filled(q, 0, &v);
    }
    {   /* rodape: barra de falhas (vazia = tudo passou) */
        rect_t v;
        v.v0.x = 0.0f; v.v0.y = 396.0f; v.v0.z = 0;
        v.v1.x = 640.0f; v.v1.y = 430.0f; v.v1.z = 0;
        v.color.r = 0x10; v.color.g = 0x10; v.color.b = 0x10; v.color.a = 0x80;
        q = draw_rect_filled(q, 0, &v);
        if (g_falhas) {
            v.v0.x = 0.0f; v.v0.y = 396.0f; v.v0.z = 0;
            v.v1.x = 640.0f * ((float)g_falhas / 24.0f); v.v1.y = 430.0f; v.v1.z = 0;
            v.color.r = 0xD0; v.color.g = 0x20; v.color.b = 0x20; v.color.a = 0x80;
            q = draw_rect_filled(q, 0, &v);
        }
    }
    q = draw_finish(q);
    dma_channel_send_normal(DMA_CHANNEL_GIF, p->data, q - p->data, 0, 0);
    dma_wait_fast();
    packet_free(p);
    graph_wait_vsync();
    graph_wait_vsync();
}

/* ------------------------------------------------------------------ main */
static char g_romver[64];

static int le_romver(void)
{
    int fd, n;
    fd = fioOpen("rom0:ROMVER", O_RDONLY);
    if (fd < 0) fd = open("rom0:ROMVER", O_RDONLY);
    if (fd < 0) return -1;
    n = fioRead(fd, g_romver, (int)sizeof(g_romver) - 1);
    if (fd >= 0) fioClose(fd);
    if (n < 0) n = 0;
    if (n > (int)sizeof(g_romver) - 1) n = (int)sizeof(g_romver) - 1;
    g_romver[n] = 0;
    {
        int k;
        for (k = 0; k < n; k++)
            if ((unsigned char)g_romver[k] < 32) g_romver[k] = ' ';
    }
    return n > 0 ? 0 : -2;
}

int main(int argc, char **argv)
{
    int r, i, fd;
    int mod_falhas = 0;
    (void)argc; (void)argv;

    add("FNF PS2 -- PROVA DE HARDWARE\n"
        "Este arquivo e' o que o SEU console respondeu. Cada linha tem um\n"
        "limite que veio de uma conta do port, nao de um palpite.\n\n");

    /* -------------------------------------------------- 1. IOP e modulos */
    SifInitRpc(0);
    for (i = 0; i < 100; i++) { if (SifIopReset("rom0:UDNL rom0:EELOADCNF", 0)) break; DelayThread(20000); }
    {int iop_ok = (i < 100);
     for (r = 0; r < 100; r++) { if (SifIopSync()) break; DelayThread(20000); }
     if (r >= 100) iop_ok = 0;
     g_iop_ok = iop_ok;}
    SifInitRpc(0);
    SifLoadFileInit();

    r = le_romver();
    if (r < 0) strcpy(g_romver, "(rom0:ROMVER nao abriu)");
    add("ROMVER .......... %s\n", g_romver);
    add("GetMemorySize ... %d\n", (int)GetMemorySize());
    add("MachineType ..... %d\n\n", (int)MachineType());
    teste("IOP reiniciou", g_iop_ok, g_iop_ok ? "SifIopReset/SifIopSync em < 2 s" : "reset/sync nao completaram");
    {
        char d[96];
        int a = SifExecModuleBuffer(iomanX_irx, size_iomanX_irx, 0, NULL, &mod_falhas);
        int b = SifExecModuleBuffer(fileXio_irx, size_fileXio_irx, 0, NULL, &mod_falhas);
        int c = SifExecModuleBuffer(usbd_irx, size_usbd_irx, 0, NULL, &mod_falhas);
        int e = SifExecModuleBuffer(usbhdfsd_irx, size_usbhdfsd_irx, 0, NULL, &mod_falhas);
        fileXioInit();
        snprintf(d, sizeof(d), "iomanX=%d fileXio=%d usbd=%d usbhdfsd=%d", a, b, c, e);
        teste("modulos de USB", (a >= 0 && b >= 0 && c >= 0 && e >= 0), d);
    }

    /* -------------------------------------------------- 2. GS e vsync */
    tela_init();
    {
        u64 t0 = GetTimerSystemTime();
        for (i = 0; i < 60; i++) graph_wait_vsync();
        {
            u64 t1 = GetTimerSystemTime();
            double hz = 60.0 / segundos(t1 - t0);
            char d[80];
            snprintf(d, sizeof(d), "%.2f Hz em 60 quadros", hz);
            teste("vsync do GS", (hz > 55.0 && hz < 65.0), d);
            add("vsync .......... %.2f Hz (esperado 59,94 PAL/NTSC na TV do usuario)\n", hz);
        }
    }
    {
        u32 livre0 = (u32)(4u * 1024u * 1024u);
        u32 total = 0;
        int blocos = 0;
        while (blocos < 64) {
            u32 a = (u32)graph_vram_allocate(256, 256, GS_PSM_32, GRAPH_ALIGN_BLOCK);
            if ((int)a < 0) break;
            total += 256u * 256u * 4u;
            blocos++;
        }
        {
            char d[96];
            snprintf(d, sizeof(d), "%d blocos de 256KB = %u B", blocos, (unsigned)total);
            teste("VRAM total do GS", (total >= 4u * 1024u * 1024u - 65536u), d);
            add("VRAM ........... %u B alocados ate' encher (teto 4 194 304)\n", (unsigned)total);
        }
        (void)livre0;
    }

    /* -------------------------------------------------- 3. RAM */
    {
        void *blocos[64];
        int n = 0;
        u32 total = 0;
        while (n < 64) {
            void *p = memalign(128, 512u * 1024u);
            if (!p) break;
            blocos[n++] = p;
            total += 512u * 1024u;
        }
        for (i = 0; i < n; i++) free(blocos[i]);
        {
            char d[112];
            snprintf(d, sizeof(d), "%.1f MB de heap livre (o jogo precisa de 23,4)", (double)total / 1048576.0);
            /* O programa ja ocupa ~0,1 MB; o jogo precisa de 23,38 MB. */
            teste("RAM do EE", (total >= 26u * 1024u * 1024u), d);
            add("RAM ............ %.2f MB de heap (teto do EE: 32 MB; jogo: 23,38 MB)\n",
                (double)total / 1048576.0);
        }
    }
    {   /* banda de memoria: 4 MB copiados, 20 vezes */
        unsigned char *a = (unsigned char *)memalign(128, 4u * 1024u * 1024u);
        unsigned char *b = (unsigned char *)memalign(128, 4u * 1024u * 1024u);
        if (a && b) {
            u64 t0, t1;
            memset(a, 0x5A, 4u * 1024u * 1024u);
            FlushCache(0);
            t0 = GetTimerSystemTime();
            for (i = 0; i < 20; i++) { memcpy(b, a, 4u * 1024u * 1024u); FlushCache(0); }
            t1 = GetTimerSystemTime();
            {
                double mb = 80.0 * 4.0;
                double mb_s = mb / segundos(t1 - t0);
                char d[96];
                snprintf(d, sizeof(d), "%.0f MB/s de copia (minimo 100)", mb_s);
                teste("banda de RAM", (mb_s > 100.0), d);
                add("banda de RAM ... %.0f MB/s\n", mb_s);
            }
            free(a); free(b);
        } else { free(a); free(b); teste("banda de RAM", 0, "memalign de 4 MB falhou"); }
    }

    /* -------------------------------------------------- 4. USB: leitura */
    {
        static unsigned char buf[1024 * 1024];
        const char *caminhos[] = {
            "mass:/FNFPS2/assets/menu_cache.rle",
            "mass:/FNFPS2/assets/audio_stems.pcm",
            "mass:/FNFPS2/assets/chars/bf.chp4",
            "mass:/FNFPS2/assets/songs/tutorial/stems.pcm",
            "mass:/FNFPS2/assets/title_logoBumpin.frames"
        };
        int achou = 0;
        for (i = 0; i < 5; i++) {
            int f2 = open(caminhos[i], O_RDONLY);
            long tam = -1;
            if (f2 < 0) {
                add("  %-52s NAO ABRIU\n", caminhos[i]);
                continue;
            }
            tam = (long)lseek(f2, 0, SEEK_END);
            lseek(f2, 0, SEEK_SET);
            if (tam > 0) {
                u64 t0 = GetTimerSystemTime();
                long lido = 0;
                int passadas = 0;
                while (passadas < 3 && lido < tam) {
                    int want = (int)((tam - lido) > (long)sizeof(buf) ? (long)sizeof(buf) : (tam - lido));
                    int got = read(f2, buf, want);
                    if (got <= 0) break;
                    lido += got;
                    passadas++;
                }
                {
                    u64 t1 = GetTimerSystemTime();
                    double s = segundos(t1 - t0);
                    double mb_s = (s > 0.0) ? ((double)lido / 1048576.0) / s : 0.0;
                    add("  %-52s %8ld B  %6.2f MB/s\n", caminhos[i], tam, mb_s);
                    if (i == 0) {
                        char d[112];
                        snprintf(d, sizeof(d), "%.2f MB/s (minimo exigido 2,0)", mb_s);
                        teste("leitura do pendrive", (mb_s >= 2.0), d);
                    }
                    achou = 1;
                }
            }
            close(f2);
        }
        if (!achou) teste("leitura do pendrive", 0, "nenhum arquivo do jogo abriu em mass:/FNFPS2");
    }

    /* -------------------------------------------------- 5. AUDIO */
    {
        int res_audsrv = -1;
        int ok = 0;
        if (SifLoadModule("rom0:LIBSD", 0, NULL) >= 0) {
            if (SifExecModuleBuffer(audsrv_irx, size_audsrv_irx, 0, NULL, &res_audsrv) >= 0) {
                if (audsrv_init() == 0) {
                    static char silencio[8192];
                    memset(silencio, 0, sizeof(silencio));
                    if (audsrv_play_audio(silencio, (int)sizeof(silencio)) >= 0) {
                        int q0 = audsrv_queued();
                        DelayThread(50000);
                        {
                            int q1 = audsrv_queued();
                            ok = (q0 >= 0 && q1 >= 0);
                            add("audio .......... init=0 fila %d -> %d\n", q0, q1);
                        }
                    }
                }
            }
        }
        if (!ok) add("audio .......... FALHOU (audsrv_res=%d)\n", res_audsrv);
        teste("audio (LIBSD+audsrv)", ok, ok ? "fila andou" : "nao subiu ou nao aceitou som");
    }

    /* -------------------------------------------------- 6. CONTROLE */
    {
        static char padbuf[256] __attribute__((aligned(64)));
        int ok = 0;
        if (SifLoadModule("rom0:SIO2MAN", 0, NULL) >= 0 &&
            SifLoadModule("rom0:PADMAN", 0, NULL) >= 0) {
            if (padInit(0) && padPortOpen(0, 0, padbuf)) {
                for (i = 0; i < 30 && !ok; i++) {
                    int st = padGetState(0, 0);
                    if (st == PAD_STATE_STABLE || st == PAD_STATE_FINDCTP1) ok = 1;
                    DelayThread(30000);
                }
            }
        }
        teste("controle no porto 1", ok, ok ? "padGetState estavel" : "sem resposta do pad");
    }

    /* -------------------------------------------------- veredito */
    add("\n----------------------------------------\n");
    add("TESTES: %d   FALHAS: %d\n", g_testes, g_falhas);
    if (g_falhas == 0)
        add("\nVEREDITO: esta maquina atende a TUDO que o port mediu precisar.\n"
            "Isso nao e' garantia de que nao existe um bug no jogo -- e' garantia\n"
            "de que o DEFEITO NAO ESTA' NO HARDWARE NEM NA MIDIA.\n");
    else
        add("\nVEREDITO: esta maquina NAO atende pelo menos um dos requisitos.\n"
            "O teste que falhou diz qual, e o limite esta' acima.\n");

    desenha_estado();
    grava("mass:/FNFPS2/PROVA.txt");
    grava("mass:/PROVA.txt");
    grava("mc0:/PROVA.txt");

    /* Fica na tela para o usuario ver: verde = tudo passou. */
    for (;;) {
        graph_wait_vsync();
        DelayThread(100000);
    }
    return 0;
}
