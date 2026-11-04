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

/* ===================================================================== */
/*  PROVA 3 -- a pergunta e' UMA: qual escrita no pendrive trava o jogo?  */
/* =====================================================================
 *
 * O que a PROVA 2 e os logs do jogo mostraram (2026-11-04):
 *   - o jogo chega ao fim do quadro 0 e para (BOOT.txt: passo 33,
 *     F0_FRAMEBUFFER_SET; sinal de vida na tela parado no primeiro quadrado);
 *   - entre esse ponto e o seguinte existe UMA chamada de E/S: o despejo do
 *     log, que abre o arquivo em O_APPEND, escreve ~4 KB, faz fsync e fecha;
 *   - o unico escritor que SEMPRE funcionou no console e' o BOOT.txt, que abre
 *     com O_TRUNC, escreve 36 bytes, faz fsync e fecha.
 *
 * Hipotese: O_APPEND (ou o arquivo que cresce, ou o tamanho do bloco) trava o
 * usbhdfsd. Esta bateria mede cada variavel SEPARADAMENTE, com o mesmo
 * pendrive, o mesmo driver e o mesmo tamanho de bloco quando possivel.
 *
 * Regra de ouro da bateria: ANTES de cada passo arriscado, o relatorio e' gravado
 * com O_TRUNC. Se o passo travar, o arquivo no cartao diz exatamente qual era.
 * A tela mostra o passo em 16 quadrados, sem depender de nada.
 *
 * Saidas: PROVA3.txt (relatorio parcial, sempre atualizado) e PROVA3.csv.
 */

static int g_passo;
static char g_p3[24000];
static int  g_p3n;

static void p3_txt(const char *fmt, ...)
{
    va_list ap; int n;
    if (g_p3n >= (int)sizeof(g_p3) - 300) return;
    va_start(ap, fmt);
    n = vsnprintf(g_p3 + g_p3n, sizeof(g_p3) - (size_t)g_p3n, fmt, ap);
    va_end(ap);
    if (n > 0) g_p3n += n;
}

/* Grava o relatorio pelo caminho PROVADO (O_TRUNC + close) e pinta o passo. */
static void p3_marca(const char *o_que)
{
    char p[160];
    p3_txt("\n>> passo %d: %s\n", g_passo, o_que);
    snprintf(p, sizeof(p), "mass:/FNFPS2/PROVA3.txt");
    grava_arq(p, g_p3, g_p3n);
    snprintf(p, sizeof(p), "mass:/PROVA3.txt");
    grava_arq(p, g_p3, g_p3n);

    if (g_gs_on) {
        packet_t *pk = packet_init(128, PACKET_NORMAL);
        qword_t *q;
        int i;
        if (pk) {
            q = pk->data;
            q = draw_setup_environment(q, 0, &g_fb, &g_z);
            q = draw_clear(q, 0, 0.0f, 0.0f, (float)SCREEN_W, (float)SCREEN_H, 0, 0, 16);
            for (i = 0; i < 16; i++) {
                float x = 40.0f + (float)i * 36.0f;
                if (i < g_passo % 17)
                    q = retangulo(q, x, 180.0f, x + 28.0f, 300.0f, 0x30, 0xC0, 0x30);
                else
                    q = retangulo(q, x, 180.0f, x + 28.0f, 300.0f, 0x30, 0x30, 0x30);
            }
            /* barra do tamanho do relatorio: mostra que ele esta' crescendo */
            q = retangulo(q, 0.0f, 420.0f, (float)(g_p3n * SCREEN_W / (int)sizeof(g_p3)),
                          440.0f, 0x20, 0x60, 0xC0);
            q = draw_finish(q);
            dma_channel_send_normal(DMA_CHANNEL_GIF, pk->data, q - pk->data, 0, 0);
            dma_wait_fast();
            packet_free(pk);
            graph_wait_vsync();
        }
    }
    g_passo++;
}

/* ---------------- baterias ---------------- */

/* Uma escrita de `bloco` bytes num arquivo, do jeito pedido.
   modo 0 = O_TRUNC (controle)   modo 1 = O_APPEND
   faz fsync e close sempre. Devolve ms gastos, ou -1 se falhou. */
static int escrita(const char *caminho, int bloco, int modo, int bytes_antes)
{
    static unsigned char buf[262144];
    u64 t0, t1;
    int fd, w = 0, flags;
    if (bloco > (int)sizeof(buf)) bloco = (int)sizeof(buf);
    t0 = agora();
    flags = O_WRONLY | O_CREAT | (modo ? O_APPEND : O_TRUNC);
    fd = open(caminho, flags, 0666);
    if (fd < 0) return -1;
    while (w < bloco) { int r = write(fd, buf + w, (u32)(bloco - w)); if (r <= 0) break; w += r; }
    fsync(fd);
    close(fd);
    t1 = agora();
    if (bytes_antes >= 0) {
        int fd2 = open(caminho, O_RDONLY);
        int tam = -1;
        if (fd2 >= 0) { tam = (int)lseek(fd2, 0, SEEK_END); close(fd2); }
        p3_txt("   bloco=%d modo=%s antes=%d depois=%d\n",
               bloco, modo ? "APPEND" : "TRUNC", bytes_antes, tam);
    }
    return (int)(((t1 - t0) * 1000ull) / (u64)kBUSCLK);
}

int main(int argc, char **argv)
{
    int i;
    (void)argc; (void)argv;

    p3_txt("FNF PS2 -- PROVA 3\n"
           "A pergunta: qual escrita no pendrive trava o jogo?\n"
           "Cada passo grava este arquivo ANTES de arriscar. Se o programa parar,\n"
           "o ultimo '>> passo N' diz onde. A tela mostra o passo em 16 quadrados.\n");

    SifInitRpc(0);
    for (i = 0; i < 100; i++) { if (SifIopReset("rom0:UDNL rom0:EELOADCNF", 0)) break; DelayThread(20000); }
    for (i = 0; i < 100; i++) { if (SifIopSync()) break; DelayThread(20000); }
    SifInitRpc(0);
    SifLoadFileInit();

    {
        int res = -1;
        SifExecModuleBuffer(iomanX_irx, size_iomanX_irx, 0, NULL, &res);
        SifExecModuleBuffer(fileXio_irx, size_fileXio_irx, 0, NULL, &res);
        SifExecModuleBuffer(usbd_irx, size_usbd_irx, 0, NULL, &res);
        SifExecModuleBuffer(usbhdfsd_irx, size_usbhdfsd_irx, 0, NULL, &res);
        fileXioInit();
    }
    gs_init();
    p3_txt("modulos carregados, GS ligado\n");

    /* espera o volume aparecer (o reset do IOP desmonta o mass:) */
    {
        u64 t0 = agora();
        int ok = 0, recarregou = 0;
        while (seg(agora() - t0) < 25.0 && !ok) {
            int fd = open("mass:/FNFPS2/assets/menu_cache.rle", O_RDONLY);
            if (fd >= 0) { close(fd); ok = 1; break; }
            if (!recarregou && seg(agora() - t0) > 5.0) {
                int res = -1;
                SifExecModuleBuffer(usbd_irx, size_usbd_irx, 0, NULL, &res);
                SifExecModuleBuffer(usbhdfsd_irx, size_usbhdfsd_irx, 0, NULL, &res);
                recarregou = 1;
            }
            DelayThread(250000);
        }
        p3_txt("pendrive: %s em %.2f s%s\n", ok ? "montou" : "NAO montou",
               seg(agora() - t0), recarregou ? " (com recarga)" : "");
        if (!ok) {
            p3_marca("SEM PENDRIVE -- fim");
            grava("PROVA3.txt", g_p3, g_p3n);
            for (;;) { graph_wait_vsync(); }
        }
    }

    p3_marca("inicio da bateria");

    /* ---- A. o padrao que FUNCIONA (BOOT.txt): TRUNC + 36 B, 20 vezes ---- */
    {
        int total = 0, pior = 0, bk, falhas = 0;
        for (bk = 0; bk < 20; bk++) {
            int ms = escrita("mass:/FNFPS2/P3_TRUNC.bin", 36, 0, -1);
            if (ms < 0) falhas++; else { total += ms; if (ms > pior) pior = ms; }
        }
        p3_txt("A. TRUNC 36 B x20: total=%d ms, pior=%d ms, falhas=%d\n", total, pior, falhas);
        p3_marca("A concluido");
    }

    /* ---- B. o padrao do log do jogo: APPEND + 200 B, 20 vezes ---- */
    {
        int total = 0, pior = 0, bk, falhas = 0;
        for (bk = 0; bk < 20; bk++) {
            int ms = escrita("mass:/FNFPS2/P3_APPEND.bin", 200, 1, -1);
            if (ms < 0) falhas++; else { total += ms; if (ms > pior) pior = ms; }
        }
        p3_txt("B. APPEND 200 B x20: total=%d ms, pior=%d ms, falhas=%d\n", total, pior, falhas);
        p3_marca("B concluido");
    }

    /* ---- C. o despejo REAL do log: APPEND + 4 KB, 20 vezes ---- */
    {
        int total = 0, pior = 0, bk, falhas = 0;
        for (bk = 0; bk < 20; bk++) {
            int ms = escrita("mass:/FNFPS2/P3_APPEND4K.bin", 4096, 1, -1);
            if (ms < 0) falhas++; else { total += ms; if (ms > pior) pior = ms; }
        }
        p3_txt("C. APPEND 4 KB x20: total=%d ms, pior=%d ms, falhas=%d\n", total, pior, falhas);
        p3_marca("C concluido");
    }

    /* ---- D. mesmo tamanho, sem append: isola o O_APPEND ---- */
    {
        int total = 0, pior = 0, bk, falhas = 0;
        for (bk = 0; bk < 20; bk++) {
            int ms = escrita("mass:/FNFPS2/P3_TRUNC4K.bin", 4096, 0, -1);
            if (ms < 0) falhas++; else { total += ms; if (ms > pior) pior = ms; }
        }
        p3_txt("D. TRUNC 4 KB x20: total=%d ms, pior=%d ms, falhas=%d\n", total, pior, falhas);
        p3_marca("D concluido");
    }

    /* ---- E. append COM o tamanho antes e depois de cada escrita ---- */
    {
        int bk, antes = 0;
        for (bk = 0; bk < 8; bk++) {
            escrita("mass:/FNFPS2/P3_CRESCE.bin", 4096, 1, antes);
            antes += 4096;
        }
        p3_txt("E. arquivo crescendo ate %d B: ok\n", antes);
        p3_marca("E concluido");
    }

    /* ---- F. append no fim de um arquivo GRANDE (cadeia de clusters) ---- */
    {
        int bk, ms = 0;
        for (bk = 0; bk < 40; bk++) ms += escrita("mass:/FNFPS2/P3_GRANDE.bin", 131072, 1, -1);
        p3_txt("F. 40 x 128 KB por append = 5 MB: total=%d ms (%.2f MB/s)\n",
               ms, ms ? 5.0 / (ms / 1000.0) : 0.0);
        p3_marca("F concluido");
    }

    /* ---- G. dois descritores abertos AO MESMO TEMPO, ambos escrevendo ---- */
    {
        int a = open("mass:/FNFPS2/P3_A.bin", O_WRONLY | O_CREAT | O_TRUNC, 0666);
        int b = open("mass:/FNFPS2/P3_B.bin", O_WRONLY | O_CREAT | O_TRUNC, 0666);
        p3_txt("G. dois descritores: a=%d b=%d\n", a, b);
        if (a >= 0 && b >= 0) {
            write(a, "aaaa", 4); write(b, "bbbb", 4);
            fsync(a); fsync(b); close(a); close(b);
            p3_txt("G. escrevi 4 B em cada e fechei os dois: ok\n");
        }
        p3_marca("G concluido");
    }

    /* ---- H. o padrao do JOGO: um descritor fica ABERTO para sempre
               (FNF_SCREEN_0000.rgba.txt) e outro abre/escreve/fecha ---- */
    {
        int preso = open("mass:/FNFPS2/P3_PRESO.bin", O_WRONLY | O_CREAT | O_TRUNC, 0666);
        int bk, total = 0;
        p3_txt("H. descritor preso=%d\n", preso);
        for (bk = 0; bk < 20; bk++) {
            int ms = escrita("mass:/FNFPS2/P3_LOG.bin", 4096, 1, -1);
            if (ms > 0) total += ms;
        }
        if (preso >= 0) { write(preso, "fim", 3); fsync(preso); close(preso); }
        p3_txt("H. 20 despejos com outro descritor aberto: total=%d ms\n", total);
        p3_marca("H concluido");
    }

    /* ---- I. 200 aberturas do mesmo arquivo (fadiga / vazamento) ---- */
    {
        int bk, falhas = 0;
        u64 t0 = agora();
        for (bk = 0; bk < 200; bk++) {
            int fd = open("mass:/FNFPS2/P3_ABRE.bin", O_RDONLY);
            if (fd < 0) falhas++; else close(fd);
        }
        p3_txt("I. 200 opens: %d falhas em %d ms\n", falhas,
               (int)(((agora() - t0) * 1000ull) / (u64)kBUSCLK));
        p3_marca("I concluido");
    }

    /* ---- J. apagar e recriar (o que o jogo faz com FNF_ERRO.txt) ---- */
    {
        int r = unlink("mass:/FNFPS2/P3_APAGA.bin");
        int ms = escrita("mass:/FNFPS2/P3_APAGA.bin", 1024, 0, -1);
        int r2 = unlink("mass:/FNFPS2/P3_APAGA.bin");
        p3_txt("J. unlink=%d recria em %d ms unlink2=%d\n", r, ms, r2);
        p3_marca("J concluido");
    }

    p3_txt("\n=== FIM ===  passos executados: %d\n", g_passo);
    p3_marca("fim");
    grava("PROVA3.txt", g_p3, g_p3n);
    /* fica na tela mostrando que terminou */
    for (;;) { graph_wait_vsync(); }
    return 0;
}
