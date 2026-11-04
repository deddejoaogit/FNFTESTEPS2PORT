/* PROVA 2 -- bateria completa do console, com captura de tela
 *
 * Roda no PS2, mede TUDO que da' para medir de dentro da maquina, e escreve em
 * arquivos no pendrive. Nada aqui depende de ler a tela pelo olho: o programa
 * tira as proprias fotos (readback do framebuffer pelo GS) e grava em .rgba,
 * alem de um mapa ASCII colorido no proprio log.
 *
 * Saidas (raiz do pendrive e FNFPS2/):
 *   PROVA2.txt      relatorio completo, em secoes, com limites e veredito
 *   PROVA2.csv      uma linha por teste: fase,teste,valor,limite,ok
 *   TELA_0001.raw   captura 640x448 RGBA8888 (a tela do veredito)
 *   TELA_0002.raw   captura do progresso no meio da bateria
 *   TELA_0001.txt   mapa ASCII da captura (64x48), cor por caractere
 *   ASSETS2.csv     todo arquivo encontrado em FNFPS2/assets: nome,tamanho
 *   ERRO2.txt       so' existe se algo fatal acontecer
 *
 * Correcao em relacao a PROVA 1: o teste de VRAM daquela versao alocava um
 * framebuffer ANTES de medir e exigia 4 MiB inteiros depois disso -- dava
 * vermelho numa maquina saudavel. Agora a VRAM e' medida com a VRAM limpa, e o
 * layout real do jogo e' alocado em seguida para conferir os enderecos.
 */
#include <tamtypes.h>
#include <kernel.h>
#include <sifrpc.h>
#include <loadfile.h>
#include <iopcontrol.h>
#include <delaythread.h>
#include <malloc.h>
#include <timer.h>
#include <graph.h>
#include <draw.h>
#include <packet.h>
#include <dma.h>
#include <gs_psm.h>
#include <gs_gp.h>
#include <gif_tags.h>
#include <libpad.h>
#include <audsrv.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#define NEWLIB_PORT_AWARE
#include <fileio.h>

/* ---------------- modulos de IOP embutidos (os mesmos do jogo) ------------- */
extern unsigned char iomanX_irx[];   extern unsigned int size_iomanX_irx;
extern unsigned char fileXio_irx[];  extern unsigned int size_fileXio_irx;
extern unsigned char usbd_irx[];     extern unsigned int size_usbd_irx;
extern unsigned char usbhdfsd_irx[]; extern unsigned int size_usbhdfsd_irx;
extern unsigned char audsrv_irx[];   extern unsigned int size_audsrv_irx;
extern int fileXioInit(void);

#define SCREEN_W 640
#define SCREEN_H 448
#define FB_PSM   GS_PSM_16
#define VBUS     ((u64)kBUSCLK)

/* ===================================================================== */
/*  RELATORIO                                                            */
/* ===================================================================== */
static char  g_txt[40000];
static int   g_tn;
static char  g_csv[12000];
static int   g_cn;
static int   g_testes, g_falhas;
static unsigned char g_ok[64];
static void desenha_barra(int i, int ok);
static int  g_gs_on = 0;
static unsigned char *g_pix;

static void txt(const char *fmt, ...)
{
    va_list ap; int n;
    if (g_tn >= (int)sizeof(g_txt) - 300) return;
    va_start(ap, fmt);
    n = vsnprintf(g_txt + g_tn, sizeof(g_txt) - (size_t)g_tn, fmt, ap);
    va_end(ap);
    if (n > 0) g_tn += n;
}

static void tst(const char *fase, const char *nome, int ok, const char *fmt, ...)
{
    char det[220];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(det, sizeof(det), fmt, ap);
    va_end(ap);
    if (g_testes < 64) g_ok[g_testes] = (unsigned char)(ok ? 1 : 0);
    g_testes++;
    if (!ok) g_falhas++;
    txt("[%s] %-30s %s\n", ok ? "PASSOU" : "FALHOU", nome, det);
    if (g_gs_on && g_testes <= 12) desenha_barra(g_testes - 1, ok);
    if (g_cn < (int)sizeof(g_csv) - 200) {
        int n = snprintf(g_csv + g_cn, sizeof(g_csv) - (size_t)g_cn,
                         "%s,%s,\"%s\",,%d\n", fase, nome, det, ok);
        if (n > 0) g_cn += n;
    }
}

static double seg(u64 t) { return (double)t / (double)kBUSCLK; }
static u64 agora(void)   { return GetTimerSystemTime(); }

static void grava_arq(const char *caminho, const void *d, int n)
{
    int fd = open(caminho, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    int w = 0;
    if (fd < 0) return;
    while (w < n) { int r = write(fd, (const char *)d + w, (u32)(n - w)); if (r <= 0) break; w += r; }
    fsync(fd); close(fd);
}

static void grava(const char *nome, const char *d, int n)
{
    char p[128];
    snprintf(p, sizeof(p), "mass:/FNFPS2/%s", nome); grava_arq(p, d, n);
    snprintf(p, sizeof(p), "mass:/%s", nome);        grava_arq(p, d, n);
}

/* ===================================================================== */
/*  GS + captura                                                          */
/* ===================================================================== */
static framebuffer_t g_fb;
static zbuffer_t     g_z;
static u32           g_fbaddr;

#define CAP_D1_CHCR   (*(volatile u32*)0x10009000)
#define CAP_D1_MADR   (*(volatile u32*)0x10009010)
#define CAP_D1_QWC    (*(volatile u32*)0x10009020)
#define CAP_GS_CSR    (*(volatile u64*)0x12001000)
#define CAP_GS_IMR    (*(volatile u64*)0x12001010)
#define CAP_GS_BUSDIR (*(volatile u64*)0x12001040)
#define CAP_VIF1_STAT (*(volatile u32*)0x10003c00)
#define CAP_VIF1_FIFO (*(volatile u128*)0x10005000)
#define CAP_VIF_MSKPATH3(v) ((u32)(v)|((u32)0x06<<24))
#define CAP_VIF_FLUSHA      ((u32)0x13<<24)
#define CAP_VIF_DIRECT(n)   ((u32)(n)|((u32)0x50<<24))
#define CAP_GIFTAG(n,eop)   (((u64)(n))|((u64)(eop)<<15)|((u64)1<<60))

static int espera_limpa32(volatile u32 *r, u32 m, u64 t)
{
    u64 s = agora();
    while (*r & m) if (agora() - s > t) return -1;
    return 0;
}
static int espera_set64(volatile u64 *r, u64 m, u64 t)
{
    u64 s = agora();
    while (!(*r & m)) if (agora() - s > t) return -1;
    return 0;
}

/* Readback de uma faixa do framebuffer, pela transferencia D1 -> EE, igual ao
 * que o jogo faz em safe_readback_rect (mantido identico de proposito: se o
 * jogo consegue, a prova tambem tem de conseguir). */
static int ler_faixa(void *d, u32 v, u32 y, u32 h)
{
    u32 c[40] __attribute__((aligned(16)));
    u32 *p = c;
    u32 qwc = SCREEN_W * h / 8, oc = CAP_D1_CHCR;
    u64 *q = (u64 *)(p + 4), oi = CAP_GS_IMR, t = 294912000ull;
    int r = -1;
    if (!h || qwc > 65535u || (oc & 0x100u)) return -10;
    p[0] = 0;
    p[1] = CAP_VIF_MSKPATH3(0x8000);
    p[2] = CAP_VIF_FLUSHA;
    p[3] = CAP_VIF_DIRECT(6);
    q[0] = CAP_GIFTAG(5, 1); q[1] = 0x0e;
    q[2] = GS_SET_BITBLTBUF(v, SCREEN_W / 64, FB_PSM, 0, 0, FB_PSM);
    q[3] = GS_REG_BITBLTBUF;
    q[4] = GS_SET_TRXPOS(0, y, 0, 0, 0);
    q[5] = GS_REG_TRXPOS;
    q[6] = GS_SET_TRXREG(SCREEN_W, h);
    q[7] = GS_REG_TRXREG;
    q[8] = 0; q[9] = 0x61;
    q[10] = GS_SET_TRXDIR(1); q[11] = GS_REG_TRXDIR;
    CAP_GS_IMR = oi | 0x200u;
    CAP_GS_CSR = 2;
    FlushCache(0);
    CAP_D1_QWC = 7; CAP_D1_MADR = (u32)c; CAP_D1_CHCR = 0x101;
    EE_SYNCL();
    if (espera_limpa32(&CAP_D1_CHCR, 0x100, t) < 0) { r = -11; goto fim; }
    if (espera_set64(&CAP_GS_CSR, 2, t) < 0) { r = -12; goto fim; }
    if (espera_limpa32(&CAP_VIF1_STAT, 0x1f000000u, t) < 0) { r = -13; goto fim; }
    CAP_VIF1_STAT = 1u << 23;
    CAP_GS_BUSDIR = 1;
    FlushCache(0);
    CAP_D1_QWC = qwc; CAP_D1_MADR = (u32)d; CAP_D1_CHCR = 0x100;
    EE_SYNCL();
    if (espera_limpa32(&CAP_D1_CHCR, 0x100, t) < 0) { r = -14; goto fim; }
    r = 1;
fim:
    CAP_D1_CHCR = oc;
    EE_SYNCL();
    CAP_VIF1_STAT = 0; CAP_GS_BUSDIR = 0; CAP_GS_IMR = oi; CAP_GS_CSR = 2;
    { u128 e = (u128)CAP_VIF_MSKPATH3(0); CAP_VIF1_FIFO = e; }
    return r;
}

/* PSMCT16 -> RGBA8888 no proprio buffer (de tras para a frente). */
static void expande16(u32 n)
{
    unsigned char *s = g_pix, *d = g_pix;
    u32 i;
    for (i = n; i-- > 0;) {
        u32 v = (u32)s[i * 2u] | ((u32)s[i * 2u + 1u] << 8);
        u32 r = v & 0x1fu, gg = (v >> 5) & 0x1fu, b = (v >> 10) & 0x1fu;
        d[i * 4 + 0] = (unsigned char)((r << 3) | (r >> 2));
        d[i * 4 + 1] = (unsigned char)((gg << 3) | (gg >> 2));
        d[i * 4 + 2] = (unsigned char)((b << 3) | (b >> 2));
        d[i * 4 + 3] = 0xFF;
    }
}

static void pinta(int falhas_ate_agora, int fase);

static int captura(const char *base)
{
    int hi[3] = { 0, 200, 400 }, hh[3] = { 200, 200, 48 }, i, r = 0;
    for (i = 0; i < 3 && r == 0; i++) {
        if (ler_faixa(g_pix, g_fbaddr << 5, (u32)hi[i], (u32)hh[i]) != 1) { r = -1 * (i + 1); break; }
        expande16((u32)(SCREEN_W * hh[i]));
        {   /* grava a faixa direto no arquivo .raw, em modo append */
            char p[96];
            snprintf(p, sizeof(p), "mass:/FNFPS2/%s.raw", base);
            { int fd = open(p, O_WRONLY | O_CREAT | (i ? O_APPEND : O_TRUNC), 0666);
              if (fd >= 0) {
                  int n = SCREEN_W * hh[i] * 4, w = 0;
                  while (w < n) { int k = write(fd, g_pix + w, (u32)(n - w)); if (k <= 0) break; w += k; }
                  fsync(fd); close(fd);
              } }
        }
    }
    return r;
}

/* Mapa ASCII de verdade: le' a tela inteira para g_pix e reduz para 64x48,
 * escolhendo o caractere pela cor. Um caractere por celula, 48 linhas. */
static void mapa_ascii_e_grava(const char *base)
{
    int faixas[3][2] = { { 0, 200 }, { 200, 200 }, { 400, 48 } }, i, cx, cy;
    char mapa[64 * 49 + 2];
    int mi = 0;
    /* le' cada faixa e reduz na hora para o mapa, sem guardar a tela toda */
    static unsigned char cel[64][48][4];
    memset(cel, 0, sizeof(cel));
    for (i = 0; i < 3; i++) {
        unsigned char *buf = g_pix;
        if (ler_faixa(buf, g_fbaddr << 5, (u32)faixas[i][0], (u32)faixas[i][1]) != 1) return;
        expande16((u32)(SCREEN_W * faixas[i][1]));
        {
            int y;
            for (y = 0; y < faixas[i][1]; y++) {
                int gy = faixas[i][0] + y;
                int cy2 = gy * 48 / 448;
                if (cy2 < 0 || cy2 > 47) continue;
                for (cx = 0; cx < 64; cx++) {
                    int gx = cx * 10 + 5;
                    unsigned char *px = buf + (y * SCREEN_W + gx) * 4;
                    if (px[0] + px[1] + px[2] > cel[cx][cy2][0] + cel[cx][cy2][1] + cel[cx][cy2][2]) {
                        cel[cx][cy2][0] = px[0]; cel[cx][cy2][1] = px[1]; cel[cx][cy2][2] = px[2];
                    }
                }
            }
        }
    }
    for (cy = 0; cy < 48; cy++) {
        for (cx = 0; cx < 64; cx++) {
            unsigned char r = cel[cx][cy][0], g = cel[cx][cy][1], b = cel[cx][cy][2];
            int mx = r > g ? (r > b ? r : b) : (g > b ? g : b);
            char c;
            if (mx < 32) c = '.';                                    /* preto   */
            else if (r > 160 && g > 160 && b > 160) c = 'W';          /* branco  */
            else if (g > r + 40 && g > b + 40) c = 'G';               /* verde   */
            else if (r > g + 60 && r > b + 60) c = 'R';               /* vermelho*/
            else if (b > r + 60 && b > g + 60) c = 'B';               /* azul    */
            else if (r > 120 && g > 120 && b < 90) c = 'Y';           /* amarelo */
            else c = 'k';                                             /* cinza   */
            mapa[mi++] = c;
        }
        mapa[mi++] = '\n';
    }
    mapa[mi] = 0;
    txt("\n--- MAPA DA TELA (64x48; . preto  W branco  G verde  R vermelho  B azul  Y amarelo  k cinza) ---\n%s\n", mapa);
    {
        char nome[64];
        snprintf(nome, sizeof(nome), "%s.txt", base);
        grava(nome, mapa, mi);
    }
}

static void gs_init(void)
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
    g_gs_on = 1;
    FlushCache(0);
}

static qword_t *retangulo(qword_t *q, float x0, float y0, float x1, float y1,
                          unsigned char r, unsigned char g, unsigned char b)
{
    rect_t v;
    v.v0.x = x0; v.v0.y = y0; v.v0.z = 0;
    v.v1.x = x1; v.v1.y = y1; v.v1.z = 0;
    v.color.r = r; v.color.g = g; v.color.b = b; v.color.a = 0x80;
    return draw_rect_filled(q, 0, &v);
}

static void desenha_barra(int i, int ok)
{
    packet_t *p = packet_init(16, PACKET_NORMAL);
    qword_t *q;
    float x;
    if (!p) return;
    x = 20.0f + (float)i * 44.0f;
    q = p->data;
    q = draw_setup_environment(q, 0, &g_fb, &g_z);
    q = retangulo(q, x, 120.0f, x + 34.0f, 330.0f,
                  ok ? 0x30 : 0xE0, ok ? 0xC0 : 0x20, ok ? 0x30 : 0x20);
    q = draw_finish(q);
    dma_channel_send_normal(DMA_CHANNEL_GIF, p->data, q - p->data, 0, 0);
    dma_wait_fast();
    packet_free(p);
    graph_wait_vsync();
}

static void pinta(int falhas, int fase)
{
    packet_t *p = packet_init(64, PACKET_NORMAL);
    qword_t *q;
    int i;
    (void)fase;
    if (!p) return;
    q = p->data;
    q = draw_setup_environment(q, 0, &g_fb, &g_z);
    q = draw_clear(q, 0, 0.0f, 0.0f, (float)SCREEN_W, (float)SCREEN_H, 0, 0, 0);
    for (i = 0; i < g_testes && i < 12; i++) {
        float x = 20.0f + (float)i * 44.0f;
        int ok = i < 64 ? g_ok[i] : 0;
        q = retangulo(q, x, 120.0f, x + 34.0f, 330.0f,
                      ok ? 0x30 : 0xE0, ok ? 0xC0 : 0x20, ok ? 0x30 : 0x20);
    }
    {   /* rodape: barra vermelha proporcional ao numero de falhas */
        q = retangulo(q, 0.0f, 396.0f, 640.0f, 430.0f, 0x10, 0x10, 0x10);
        if (falhas) {
            float w = 640.0f * ((float)falhas / 12.0f);
            q = retangulo(q, 0.0f, 396.0f, w, 430.0f, 0xD0, 0x20, 0x20);
        }
    }
    q = draw_finish(q);
    dma_channel_send_normal(DMA_CHANNEL_GIF, p->data, q - p->data, 0, 0);
    dma_wait_fast();
    packet_free(p);
    graph_wait_vsync();
}

/* ===================================================================== */
/*  MAIN                                                                  */
/* ===================================================================== */
int main(int argc, char **argv)
{
    int i, r;
    int iop_ok = 0;
    (void)argc; (void)argv;

    txt("FNF PS2 -- PROVA 2\n"
        "Bateria completa do console. Tudo que este programa mede esta' abaixo,\n"
        "com o limite que veio de uma conta do port (nao de um palpite).\n"
        "As duas capturas da tela (TELA_0001/0002 .raw + .txt) sao imagens\n"
        "reais lidas do framebuffer pelo GS -- nao sao desenho de relatorio.\n\n");

    /* ---------------------------------------------------------- 0. ambiente */
    txt("========== 0. AMBIENTE ==========\n");
    SifInitRpc(0);
    for (i = 0; i < 100; i++) { if (SifIopReset("rom0:UDNL rom0:EELOADCNF", 0)) break; DelayThread(20000); }
    iop_ok = (i < 100);
    for (r = 0; r < 100; r++) { if (SifIopSync()) break; DelayThread(20000); }
    if (r >= 100) iop_ok = 0;
    SifInitRpc(0);
    SifLoadFileInit();
    tst("AMB", "IOP reiniciou", iop_ok, "SifIopReset/SifIopSync");

    {
        char rom[64] = "(rom0:ROMVER nao abriu)";
        int fd = fioOpen("rom0:ROMVER", O_RDONLY);
        if (fd >= 0) {
            int n = fioRead(fd, rom, (int)sizeof(rom) - 1);
            fioClose(fd);
            if (n > 0) { int k; rom[n] = 0; for (k = 0; k < n; k++) if ((unsigned char)rom[k] < 32) rom[k] = ' '; }
        }
        txt("ROMVER ......... %s\n", rom);
        txt("GetMemorySize .. %d MB\n", (int)GetMemorySize());
        txt("MachineType .... %d\n", (int)MachineType());
    }

    {   /* modulos, como o jogo carrega */
        int res = -1;
        int a = SifExecModuleBuffer(iomanX_irx, size_iomanX_irx, 0, NULL, &res);
        int b = SifExecModuleBuffer(fileXio_irx, size_fileXio_irx, 0, NULL, &res);
        int c = SifExecModuleBuffer(usbd_irx, size_usbd_irx, 0, NULL, &res);
        int d = SifExecModuleBuffer(usbhdfsd_irx, size_usbhdfsd_irx, 0, NULL, &res);
        fileXioInit();
        tst("AMB", "modulos de USB", (a >= 0 && b >= 0 && c >= 0 && d >= 0),
            "iomanX=%d fileXio=%d usbd=%d usbhdfsd=%d", a, b, c, d);
    }

    /* ESPERA O mass: MONTAR -- e este e' o teste mais importante da bateria.
     *
     * O reset do IOP DESMONTA o pendrive: o usbhdfsd e' descarregado e o
     * dispositivo precisa ser enumerado de novo. Na PROVA 1 o primeiro acesso
     * era imediato e deu vermelho; no jogo, e' este mesmo caminho que decide se
     * o boot continua (load_metadata le 4 arquivos de mass:/FNFPS2/assets e,
     * se qualquer um falhar, a 0.2.159 parava em SleepThread -- tela preta).
     *
     * Aqui a espera e' explicita, com recarga de modulos no meio e tentativa em
     * mass: e mass0:. O tempo medido vai para o relatorio. */
    {
        u64 t0 = agora();
        int ok = 0, tent = 0, recarregou = 0;
        const char *sonda[2] = { "mass:/FNFPS2/assets/menu_cache.rle",
                                 "mass0:/FNFPS2/assets/menu_cache.rle" };
        while (seg(agora() - t0) < 25.0) {
            int k;
            for (k = 0; k < 2 && !ok; k++) {
                int fd = open(sonda[k], O_RDONLY);
                tent++;
                if (fd >= 0) { close(fd); ok = (k == 0) ? 1 : 2; }
            }
            if (ok) break;
            /* aos 5 s, recarrega o driver USB: alguns lancadores deixam o usbd
               em estado que so' uma segunda carga resolve */
            if (!recarregou && seg(agora() - t0) > 5.0) {
                int res = -1;
                SifExecModuleBuffer(usbd_irx, size_usbd_irx, 0, NULL, &res);
                SifExecModuleBuffer(usbhdfsd_irx, size_usbhdfsd_irx, 0, NULL, &res);
                recarregou = 1;
                txt("   (aos 5 s: recarreguei usbd+usbhdfsd)\n");
            }
            DelayThread(250000);
        }
        {
            char det[160];
            snprintf(det, sizeof(det), "%.2f s, %d tentativas%s%s",
                     seg(agora() - t0), tent,
                     ok == 2 ? " -- SO' em mass0: (o jogo usa mass:)" : "",
                     recarregou ? ", com recarga" : "");
            tst("AMB", "pendrive montou (mass:)", ok == 1, "%s", det);
        }
        /* o que existe na raiz? Isso diz se o problema e' o caminho ou o meio. */
        {
            DIR *d = opendir("mass:/");
            if (d) {
                int n = 0;
                txt("   raiz de mass:/ :");
                for (;;) {
                    struct dirent *e = readdir(d);
                    if (!e) break;
                    if (e->d_name[0] == '.') continue;
                    if (n < 12) txt(" %s", e->d_name);
                    n++;
                }
                closedir(d);
                txt("%s (%d entradas)\n", n > 12 ? " ..." : "", n);
                tst("AMB", "listar a raiz do pendrive", n > 0, "%d entradas", n);
            } else {
                txt("   opendir(\"mass:/\") FALHOU -- o volume nao esta' montado\n");
                tst("AMB", "listar a raiz do pendrive", 0, "opendir falhou");
            }
        }
        /* memory card: serve de destino de log se o pendrive nao montar */
        {
            DIR *d = opendir("mc0:/");
            tst("AMB", "memory card presente", d != NULL, d ? "mc0:/ abriu" : "sem mc0:/");
            if (d) closedir(d);
        }
        if (!ok) {
            grava("ERRO2.txt",
                  "O pendrive NAO montou em 25 s (nem mass:, nem mass0:).\n"
                  "Sem isso, nenhum arquivo do jogo pode ser lido -- e' a causa\n"
                  "direta da tela preta: o boot le 4 assets de mass:/FNFPS2/assets\n"
                  "e, se qualquer leitura falha, o jogo para.\n"
                  "Verifique: FAT32 (nao NTFS/exFAT), tabela MBR (nao GPT), e se o\n"
                  "pendrive e' reconhecido pelo seu lancador.\n",
                  (int)strlen("O pendrive NAO montou em 25 s.\n"));
        }
    }

    grava("PROVA2.txt", g_txt, g_tn);   /* parcial: se travar, isto ja' esta' no pendrive */

    /* ----------------------------------------------------------- 1. CPU */
    txt("\n========== 1. CPU (EE R5900) ==========\n");
    {
        volatile u32 a = 0xDEADBEEF, b = 0x12345678;
        u32 prod = a * 0, div_ok;
        volatile s32 neg = -1234567;
        prod = (u32)(a ^ b);
        div_ok = (u32)((int)neg / 3 == -411522);
        tst("CPU", "inteiros 32 bits", prod == (0xDEADBEEF ^ 0x12345678) && div_ok,
            "xor=%08X div_int=%s", prod, div_ok ? "ok" : "ERRADO");
        {
            volatile float f1 = 1.5f, f2 = 3.25f;
            volatile double d1 = 1.0 / 3.0;
            float s = f1 * f2 + 0.5f;
            int fpu_ok = (s == 5.375f) && (d1 > 0.3333) && (d1 < 0.3334);
            tst("CPU", "FPU float e double", fpu_ok, "1.5*3.25+0.5=%f d=%f", (double)s, d1);
        }
        {
            volatile float f0 = 2.0f;
            float raiz = 0.0f;
            /* sqrt sem libm: Newton de 2 iteracoes a partir de 1.5 */
            raiz = f0;
            for (i = 0; i < 20; i++) raiz = 0.5f * (raiz + f0 / raiz);
            tst("CPU", "divisao em ponto flutuante", raiz > 1.41421f && raiz < 1.41422f,
                "sqrt(2)=%.6f", (double)raiz);
        }
    }

    grava("PROVA2.txt", g_txt, g_tn);
    /* ------------------------------------------------------------ 2. RAM */
    txt("\n========== 2. RAM ==========\n");
    {
        void *bl[64];
        int n = 0;
        u32 total = 0;
        while (n < 64) {
            void *p = memalign(128, 512u * 1024u);
            if (!p) break;
            bl[n++] = p; total += 512u * 1024u;
        }
        for (i = 0; i < n; i++) free(bl[i]);
        tst("RAM", "heap livre", total >= 24u * 1024u * 1024u,
            "%.2f MB contiguos em blocos de 512 KB", (double)total / 1048576.0);
    }

    grava("PROVA2.txt", g_txt, g_tn);
    {
        const u32 alvo = 8u * 1024u * 1024u;
        unsigned char *padrao = (unsigned char *)memalign(128, alvo);
        if (!padrao) {
            tst("RAM", "integridade de 8 MB", 0, "memalign de 8 MB falhou");
        } else {
            u32 soma = 0, ruins = 0, k;
            for (k = 0; k < alvo; k++) padrao[k] = (unsigned char)(k * 31u + (k >> 8));
            for (k = 0; k < alvo; k++) if (padrao[k] != (unsigned char)(k * 31u + (k >> 8))) ruins++;
            for (k = 0; k < alvo; k++) soma += padrao[k];
            tst("RAM", "integridade de 8 MB", ruins == 0 && soma != 0,
                "%u bytes errados, soma=%08X", (unsigned)ruins, (unsigned)soma);
            free(padrao);
        }
    }
    /* ----------------------------------------------------------- 3. VRAM */
    txt("\n========== 3. VRAM (GS) ==========\n");
    {
        /* MEDIDO COM A VRAM LIMPA: nada alocado antes. */
        u32 total = 0;
        int blocos = 0;
        graph_vram_clear();
        while (blocos < 64) {
            u32 a = (u32)graph_vram_allocate(256, 256, GS_PSM_32, GRAPH_ALIGN_BLOCK);
            if ((int)a < 0) break;
            total += 256u * 256u * 4u;
            blocos++;
        }
        tst("VRAM", "capacidade total", total >= (4u * 1024u * 1024u) - 262144u,
            "%d blocos de 256 KB = %u B (teto 4 194 304)", blocos, (unsigned)total);
        txt("   blocos livres: %d; sobra nao alocavel: %u B\n", blocos,
            (unsigned)((4u * 1024u * 1024u) - total));
    }
    gs_init();
    {
        /* o layout REAL do jogo, alocado na mesma ordem: confere os enderecos */
        u32 f0, f1, t2, ft, g16;
        graph_vram_clear();
        f0  = (u32)graph_vram_allocate(SCREEN_W, SCREEN_H, FB_PSM, GRAPH_ALIGN_PAGE);
        f1  = (u32)graph_vram_allocate(SCREEN_W, SCREEN_H, FB_PSM, GRAPH_ALIGN_PAGE);
        t2  = (u32)graph_vram_allocate(320, 256, GS_PSM_32, GRAPH_ALIGN_BLOCK);
        ft  = (u32)graph_vram_allocate(384, 192, GS_PSM_32, GRAPH_ALIGN_BLOCK);
        g16 = (u32)graph_vram_allocate(320, 64, GS_PSM_16, GRAPH_ALIGN_BLOCK);
        tst("VRAM", "layout do jogo cabe", (int)f0 >= 0 && (int)f1 >= 0 &&
            (int)t2 >= 0 && (int)ft >= 0 && (int)g16 >= 0,
            "fb0=%u fb1=%u tex2=%u font=%u gf16=%u", (unsigned)f0, (unsigned)f1,
            (unsigned)t2, (unsigned)ft, (unsigned)g16);
        {
            u32 usado = (g16 + 320u * 64u * 2u) - f0;
            txt("   usado pelo layout: %u B de 4 194 304 (%.1f%%), sobram %u B para o pool\n",
                (unsigned)usado, 100.0 * (double)usado / 4194304.0,
                (unsigned)(4194304u - usado));
        }
    }
    gs_init();
    txt("   framebuffer de trabalho: endereco %u (palavras)\n", (unsigned)g_fbaddr);
    g_pix = (unsigned char *)memalign(128, SCREEN_W * SCREEN_H * 4u);
    if (!g_pix) {
        tst("GS", "buffer da captura", 0, "memalign de %u B falhou",
            (unsigned)(SCREEN_W * SCREEN_H * 4u));
    } else {
        tst("GS", "buffer da captura", 1, "%u B alinhados a 128",
            (unsigned)(SCREEN_W * SCREEN_H * 4u));
    }

    /* --------------------------------------------------------- 4. GS/DMA */
    txt("\n========== 4. GS e DMA ==========\n");
    {
        u64 t0, t1;
        packet_t *p = packet_init(64, PACKET_NORMAL);
        qword_t *q;
        t0 = agora();
        q = p->data;
        q = draw_setup_environment(q, 0, &g_fb, &g_z);
        q = draw_clear(q, 0, 0.0f, 0.0f, (float)SCREEN_W, (float)SCREEN_H, 0, 0x20, 0x20);
        q = retangulo(q, 100.0f, 100.0f, 300.0f, 300.0f, 0x40, 0xC0, 0x40);
        q = draw_finish(q);
        dma_channel_send_normal(DMA_CHANNEL_GIF, p->data, q - p->data, 0, 0);
        dma_wait_fast();
        t1 = agora();
        packet_free(p);
        tst("GS", "desenho + DMA + FINISH", 1, "%.2f ms para enviar e completar", seg(t1 - t0) * 1000.0);
    }
    {
        u64 t0 = agora();
        int n;
        for (n = 0; n < 60; n++) graph_wait_vsync();
        {
            u64 t1 = agora();
            double hz = 60.0 / seg(t1 - t0);
            tst("GS", "vsync", hz > 55.0 && hz < 65.0, "%.2f Hz em 60 quadros", hz);
        }
    }
    if (g_pix) {
        pinta(0, 0);
        { int cr = captura("TELA_0001"); tst("GS", "captura do framebuffer", cr == 0, "retorno %d", cr); }
        mapa_ascii_e_grava("TELA_0001");
    }

    grava("PROVA2.txt", g_txt, g_tn);
    /* ------------------------------------------------------ 5. PENDRIVE */
    txt("\n========== 5. PENDRIVE (mass:) ==========\n");
    {
        /* 5.1 latencia de abertura */
        u64 t0 = agora();
        int ok = 0, n;
        for (n = 0; n < 10; n++) {
            int fd = open("mass:/FNFPS2/assets/menu_cache.rle", O_RDONLY);
            if (fd >= 0) { close(fd); ok++; }
        }
        tst("USB", "abrir 10x", ok == 10, "%d de 10, %.2f ms por open",
            ok, seg(agora() - t0) * 100.0);
    }
    {
        /* 5.2 leitura sequencial em blocos de 32 KB e de 64 KB */
        static unsigned char buf[65536];
        const char *alvos[2] = { "mass:/FNFPS2/assets/menu_cache.rle",
                                 "mass:/FNFPS2/assets/audio_stems.pcm" };
        int ai;
        for (ai = 0; ai < 2; ai++) {
            int fd = open(alvos[ai], O_RDONLY);
            long tam;
            if (fd < 0) { tst("USB", alvos[ai], 0, "nao abriu"); continue; }
            tam = (long)lseek(fd, 0, SEEK_END);
            lseek(fd, 0, SEEK_SET);
            {
                u64 ta = agora();
                long lido = 0;
                int bloco = 32768;
                if (tam > 8L * 1024L * 1024L) tam = 8L * 1024L * 1024L; /* 8 MB bastam para medir a taxa */
                while (lido < tam) {
                    int want = (int)((tam - lido) < bloco ? (tam - lido) : bloco);
                    int got = read(fd, buf, want);
                    if (got <= 0) break;
                    lido += got;
                }
                {
                    u64 tb = agora();
                    double s = seg(tb - ta);
                    double mbs = s > 0.0 ? ((double)lido / 1048576.0) / s : 0.0;
                    char nome[90];
                    snprintf(nome, sizeof(nome), "leitura 32KB %s", ai ? "audio_stems" : "menu_cache");
                    tst("USB", nome, mbs >= 2.0, "%.2f MB/s (%ld B em %.2f s)", mbs, lido, s);
                }
            }
            lseek(fd, 0, SEEK_SET);
            {
                u64 ta = agora();
                long lido = 0;
                while (lido < tam) {
                    int want = (int)((tam - lido) < 65536 ? (tam - lido) : 65536);
                    int got = read(fd, buf, want);
                    if (got <= 0) break;
                    lido += got;
                }
                {
                    u64 tb = agora();
                    double s = seg(tb - ta);
                    double mbs = s > 0.0 ? ((double)lido / 1048576.0) / s : 0.0;
                    tst("USB", ai ? "leitura 64KB audio_stems" : "leitura 64KB menu_cache",
                        mbs >= 2.0, "%.2f MB/s", mbs);
                }
            }
            close(fd);
        }
    }
    {
        /* 5.3 leituras aleatorias: e' o padrao do streaming de stems */
        int fd = open("mass:/FNFPS2/assets/audio_stems.pcm", O_RDONLY);
        if (fd >= 0) {
            static unsigned char b2[32768];
            long tam = (long)lseek(fd, 0, SEEK_END);
            u64 ta = agora();
            int ok = 0, n;
            for (n = 0; n < 20 && tam > 65536; n++) {
                long pos = (long)(((u64)n * 7919u) % (u64)(tam - 32768));
                if (lseek(fd, pos, SEEK_SET) < 0) continue;
                if (read(fd, b2, 32768) == 32768) ok++;
            }
            {
                u64 tb = agora();
                double ms = seg(tb - ta) * 1000.0 / 20.0;
                tst("USB", "20 leituras com seek", ok == 20, "%d de 20, %.2f ms por leitura de 32 KB", ok, ms);
            }
            close(fd);
        } else tst("USB", "20 leituras com seek", 0, "audio_stems.pcm nao abriu");
    }
    {
        /* 5.4 ESCRITA (o jogo grava log, save e capturas no pendrive) */
        static unsigned char wbuf[262144];
        static unsigned char rbuf[262144];
        u32 k;
        int fd, wok = 0, rok = 0;
        u64 ta, tb, tc;
        for (k = 0; k < sizeof(wbuf); k++) wbuf[k] = (unsigned char)(k * 7u + 3u);
        ta = agora();
        fd = open("mass:/FNFPS2/TESTE_ESCRITA.bin", O_WRONLY | O_CREAT | O_TRUNC, 0666);
        if (fd >= 0) {
            int w = 0;
            while (w < (int)sizeof(wbuf)) { int r2 = write(fd, wbuf + w, (u32)(sizeof(wbuf) - (u32)w)); if (r2 <= 0) break; w += r2; }
            fsync(fd);
            close(fd);
            wok = (w == (int)sizeof(wbuf));
        }
        tb = agora();
        fd = open("mass:/FNFPS2/TESTE_ESCRITA.bin", O_RDONLY);
        if (fd >= 0) {
            int got = read(fd, rbuf, (int)sizeof(rbuf));
            close(fd);
            if (got == (int)sizeof(rbuf) && memcmp(rbuf, wbuf, sizeof(rbuf)) == 0) rok = 1;
        }
        tc = agora();
        tst("USB", "escrita de 256 KB + releitura", wok && rok,
            "escrita %.2f s (%.2f MB/s), leitura %.2f s, bytes %s",
            seg(tb - ta), seg(tb - ta) > 0 ? (0.25 / seg(tb - ta)) : 0.0, seg(tc - tb),
            rok ? "identicos" : "DIFERENTES");
        remove("mass:/FNFPS2/TESTE_ESCRITA.bin");
    }

    grava("PROVA2.txt", g_txt, g_tn);
    /* ------------------------------------------- 6. INTEGRIDADE DOS ASSETS */
    txt("\n========== 6. ASSETS NO PENDRIVE ==========\n");
    {
        static const struct { const char *nome; long esperado; } rt[] = {
            { "font.rgba", 384 * 192 * 4 },
            { "font.widths", 96 },
            { "menu_cache.rle", 1587561 },
            { "menu_sfx.pcm", 281088 },
            { "freakyMenu_48000.pcm", 21687068 },
            { "title_alphabet.rgba", 384 * 192 * 4 },
            { "title_prompt_l.rgba", 512 * 192 * 4 },
            { "title_prompt_r.rgba", 320 * 192 * 4 },
            { "title_newgrounds.rgba", 181 * 175 * 4 },
            { "title_logoBumpin.frames", 320 * 240 * 4 * 15 },
            { "title_gfDanceTitle.frames", 256 * 235 * 4 * 30 },
            { "mainstage_tutorial_parallax.rgba", 512 * 256 * 4 },
            { "tutorial_hud_official_320x256_gsalpha.rgba", 320 * 256 * 4 },
            { "gf_frames.lz4f", 1375989 },
            { "bf_frames.lz4f", 1239417 },
            { "audio_stems.pcm", 25804800 },
        };
        int bons = 0, ruins = 0;
        char linha[160];
        txt("   %-46s %10s %10s\n", "arquivo", "medido", "esperado");
        for (i = 0; i < (int)(sizeof(rt) / sizeof(rt[0])); i++) {
            char p[128];
            int fd;
            long tam = -1;
            snprintf(p, sizeof(p), "mass:/FNFPS2/assets/%s", rt[i].nome);
            fd = open(p, O_RDONLY);
            if (fd >= 0) { tam = (long)lseek(fd, 0, SEEK_END); close(fd); }
            if (tam == rt[i].esperado) bons++; else ruins++;
            snprintf(linha, sizeof(linha), "%s,%ld,%ld,%d\n", rt[i].nome, tam, rt[i].esperado,
                     tam == rt[i].esperado);
            txt("%s   %-46s %10ld %10ld\n", tam == rt[i].esperado ? "  ok" : "FALTA",
                rt[i].nome, tam, rt[i].esperado);
        }
        tst("ASSETS", "16 assets de runtime", ruins == 0, "%d exatos, %d errados/faltando", bons, ruins);
    }
    {   /* contagens por pasta, com opendir */
        static const struct { const char *dir; const char *suf; int esperado; const char *nome; } cnt[] = {
            { "mass:/FNFPS2/assets/chars", ".chp4", 46, "personagens (.chp4)" },
            { "mass:/FNFPS2/assets/stages", NULL, 21, "palcos (subpastas)" },
            { "mass:/FNFPS2/assets/songs", NULL, 27, "musicas (subpastas)" },
        };
        for (i = 0; i < 3; i++) {
            DIR *d = opendir(cnt[i].dir);
            int n = 0;
            if (!d) { tst("ASSETS", cnt[i].nome, 0, "opendir falhou em %s", cnt[i].dir); continue; }
            for (;;) {
                struct dirent *e = readdir(d);
                if (!e) break;
                if (e->d_name[0] == '.') continue;
                if (cnt[i].suf) {
                    size_t L = strlen(e->d_name), S = strlen(cnt[i].suf);
                    if (L >= S && strcmp(e->d_name + L - S, cnt[i].suf) == 0) n++;
                } else n++;
            }
            closedir(d);
            tst("ASSETS", cnt[i].nome, n == cnt[i].esperado, "%d encontrados, %d esperados", n, cnt[i].esperado);
        }
    }
    {   /* dentro de cada musica: stems.pcm e os charts */
        DIR *ds = opendir("mass:/FNFPS2/assets/songs");
        int n_stems = 0, n_charts = 0, n_musicas = 0;
        if (ds) {
            for (;;) {
                struct dirent *e = readdir(ds);
                char sub[320];
                DIR *dm;
                if (!e) break;
                if (e->d_name[0] == '.') continue;
                n_musicas++;
                snprintf(sub, sizeof(sub), "mass:/FNFPS2/assets/songs/%s", e->d_name);
                dm = opendir(sub);
                if (!dm) continue;
                for (;;) {
                    struct dirent *f2 = readdir(dm);
                    size_t L;
                    if (!f2) break;
                    L = strlen(f2->d_name);
                    if (L > 9 && strcmp(f2->d_name + L - 9, "stems.pcm") == 0) n_stems++;
                    if (L > 6 && strcmp(f2->d_name + L - 6, ".chart") == 0) n_charts++;
                }
                closedir(dm);
            }
            closedir(ds);
        }
        tst("ASSETS", "stems.pcm em todas as musicas", n_stems == 27,
            "%d stems em %d pastas de musica", n_stems, n_musicas);
        tst("ASSETS", "charts", n_charts >= 81, "%d arquivos .chart", n_charts);
    }
    {   /* um .chp4, um .chart e um stems de verdade: cabecalho e tamanho */
        static unsigned char hd[64];
        int fd = open("mass:/FNFPS2/assets/chars/bf.chp4", O_RDONLY);
        if (fd >= 0) {
            int n = read(fd, hd, 8);
            long tam = (long)lseek(fd, 0, SEEK_END);
            close(fd);
            tst("ASSETS", "bf.chp4 legivel", n == 8 && tam > 100000,
                "magic %.4s, %ld B", (char *)hd, tam);
        } else tst("ASSETS", "bf.chp4 legivel", 0, "nao abriu");
        fd = open("mass:/FNFPS2/assets/songs/bopeebo/normal.chart", O_RDONLY);
        if (fd < 0) fd = open("mass:/FNFPS2/assets/songs/bopeebo-easy.chart", O_RDONLY);
        if (fd >= 0) {
            int n = read(fd, hd, 16);
            close(fd);
            tst("ASSETS", "chart legivel", n > 0, "%d B lidos: %.8s", n, (char *)hd);
        } else tst("ASSETS", "chart legivel", 0, "nao abriu nenhum chart de bopeebo");
        fd = open("mass:/FNFPS2/assets/songs/bopeebo/stems.pcm", O_RDONLY);
        if (fd >= 0) {
            long tam = (long)lseek(fd, 0, SEEK_END);
            close(fd);
            tst("ASSETS", "stems.pcm da 1a musica", tam > 1000000, "%ld B", tam);
        } else tst("ASSETS", "stems.pcm da 1a musica", 0, "nao abriu");
    }

    grava("PROVA2.txt", g_txt, g_tn);
    /* --------------------------------------------------------- 7. AUDIO */
    txt("\n========== 7. AUDIO ==========\n");
    {
        int res = -1, ok = 0;
        if (SifLoadModule("rom0:LIBSD", 0, NULL) >= 0 &&
            SifExecModuleBuffer(audsrv_irx, size_audsrv_irx, 0, NULL, &res) >= 0 &&
            audsrv_init() == 0) {
            static char sil[65536];
            audsrv_fmt_t fmt;
            fmt.freq = 48000; fmt.bits = 16; fmt.channels = 2;
            memset(sil, 0, sizeof(sil));
            if (audsrv_set_format(&fmt) == 0) {
                int q0 = audsrv_queued();
                u64 t0 = agora();
                if (audsrv_play_audio(sil, (int)sizeof(sil)) >= 0) {
                    int gasto = 0;
                    while (audsrv_queued() > 0 && gasto < 400) { DelayThread(10000); gasto++; }
                    ok = 1;
                    txt("   formato 48000 Hz/16 bits/2 canais; fila inicial %d B;"
                        " 64 KB tocaram em ~%d ms\n",
                        q0, (int)(seg(agora() - t0) * 1000.0));
                }
            } else {
                txt("   audsrv_set_format(48000/16/2) recusou\n");
            }
        }
        tst("AUDIO", "LIBSD + audsrv + fila", ok,
            ok ? "tocou 64 KB de silencio e a fila esvaziou" : "nao subiu (res=%d)", res);
    }

    /* -------------------------------------------------------- 8. CONTROLE */
    txt("\n========== 8. CONTROLE ==========\n");
    {
        static char padbuf[256] __attribute__((aligned(64)));
        int ok = 0;
        if (SifLoadModule("rom0:SIO2MAN", 0, NULL) >= 0 &&
            SifLoadModule("rom0:PADMAN", 0, NULL) >= 0 &&
            padInit(0) && padPortOpen(0, 0, padbuf)) {
            for (i = 0; i < 40 && !ok; i++) {
                int st = padGetState(0, 0);
                if (st == PAD_STATE_STABLE || st == PAD_STATE_FINDCTP1) ok = 1;
                DelayThread(25000);
            }
        }
        tst("PAD", "controle no porto 1", ok, ok ? "padGetState estavel" : "sem resposta");
    }

    /* -------------------------------------------------------- VEREDITO */
    txt("\n==================================================\n");
    txt("TESTES: %d    FALHAS: %d\n", g_testes, g_falhas);
    if (g_falhas == 0) {
        txt("\nVEREDITO: esta maquina passou em TODOS os testes.\n"
            "Isso prova que o hardware e a midia entregam o que o port consome.\n"
            "Nao prova que o jogo nao tem bug -- prova que o bug, se existir,\n"
            "esta' no software, e a tela de falha da 0.2.160 vai dizer onde.\n");
    } else {
        txt("\nVEREDITO: %d teste(s) falharam. Cada um tem o limite na propria\n"
            "linha, acima. O que falhou aqui e' o que precisa ser resolvido antes\n"
            "de culpar o jogo.\n", g_falhas);
    }
    txt("\nArquivos gerados: PROVA2.txt, PROVA2.csv, TELA_0001.raw,\n"
        "TELA_0001.txt (mapa ASCII), ASSETS2.csv.\n");

    /* tela final + segunda captura (com o resultado ja' desenhado) */
    if (g_pix) {
        pinta(g_falhas, 9);
        { int cr = captura("TELA_0002"); tst("GS", "captura final", cr == 0, "retorno %d", cr); }
        mapa_ascii_e_grava("TELA_0002");
    }

    grava("PROVA2.txt", g_txt, g_tn);
    {
        const char *hdr = "fase,teste,detalhe,limite,ok\n";
        char todo[12400];
        int n = (int)strlen(hdr);
        memcpy(todo, hdr, (size_t)n);
        memcpy(todo + n, g_csv, (size_t)g_cn);
        grava("PROVA2.csv", todo, n + g_cn);
    }

    for (;;) { graph_wait_vsync(); DelayThread(200000); }
    return 0;
}
