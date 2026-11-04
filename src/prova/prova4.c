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
/*  PROVA 4 -- tres perguntas que o console de 2026-11-04 deixou abertas  */
/* =====================================================================
 *
 * 1) TEXTO BORRADO. A captura de tela do jogo mostra os rotulos do menu como
 *    fragmentos. O desenho assume uma grade de 16 colunas x 6 linhas de
 *    celulas 24x32 dentro de 384x192 (src/main.c: FONT_CELL_W/H). Se o
 *    font.rgba real tiver outra grade, cada glifo sai como pedaco de dois.
 *    Aqui a grade e' MEDIDA: perfil de tinta por coluna e por linha, e a
 *    contagem de "corredores" vazios em cada candidato.
 *
 * 2) SEM VOCAIS. O stems.pcm tem 8 B por amostra: Inst L/R + Vox L/R. Se os
 *    bytes de voz forem todos zero, o arquivo nao tem vozes (dado faltando na
 *    geracao). Se tiverem sinal, o problema e' do mixer/flag -- e a 0.2.164
 *    ja' corrigiu o chord SELECT+R3 que desligava as vozes.
 *
 * 3) PALCO E PERSONAGENS MUDOS. Cada prop de palco e' um CFZ4 e cada
 *    personagem um CHP4. Aqui abre-se cada um e confere-se o cabecalho
 *    (magic, versao, contagem, bytes por quadro) contra o que o jogo espera.
 *    Nada e' adivinhado: o que nao casar sai com os bytes na mao.
 *
 * Saidas: PROVA4.txt (parcial, regravado a cada secao) na raiz e em FNFPS2/.
 */

static int  g_passo4;
static char g_p4[40000];
static int  g_p4n;

static void p4_txt(const char *fmt, ...)
{
    va_list ap; int n;
    if (g_p4n >= (int)sizeof(g_p4) - 400) return;
    va_start(ap, fmt);
    n = vsnprintf(g_p4 + g_p4n, sizeof(g_p4) - (size_t)g_p4n, fmt, ap);
    va_end(ap);
    if (n > 0) g_p4n += n;
}

static void p4_marca(const char *o_que)
{
    char p[160];
    p4_txt("\n>> passo %d: %s\n", g_passo4, o_que);
    snprintf(p, sizeof(p), "mass:/FNFPS2/PROVA4.txt"); grava_arq(p, g_p4, g_p4n);
    snprintf(p, sizeof(p), "mass:/PROVA4.txt");        grava_arq(p, g_p4, g_p4n);
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
                if (i < g_passo4 % 17)
                    q = retangulo(q, x, 180.0f, x + 28.0f, 300.0f, 0x30, 0xC0, 0x30);
                else
                    q = retangulo(q, x, 180.0f, x + 28.0f, 300.0f, 0x30, 0x30, 0x30);
            }
            q = retangulo(q, 0.0f, 420.0f,
                          (float)(g_p4n * SCREEN_W / (int)sizeof(g_p4)), 440.0f,
                          0x20, 0x60, 0xC0);
            q = draw_finish(q);
            dma_channel_send_normal(DMA_CHANNEL_GIF, pk->data, q - pk->data, 0, 0);
            dma_wait_fast();
            packet_free(pk);
            graph_wait_vsync();
        }
    }
    g_passo4++;
}

static unsigned char *g_buf4;

/* Le' o arquivo inteiro para g_buf4 (no maximo bytes). Devolve lido ou <0. */
static int le_arquivo(const char *path, int bytes)
{
    int fd = open(path, O_RDONLY), done = 0;
    if (fd < 0) return -1;
    while (done < bytes) {
        int n = read(fd, g_buf4 + done, (u32)(bytes - done));
        if (n <= 0) break;
        done += n;
    }
    close(fd);
    return done;
}

static void secao_fonte(void)
{
    int n, x, y;
    static unsigned short ink_col[384], ink_row[192];
    p4_txt("\n========== 1. FONTE (a grade de verdade) ==========\n");
    n = le_arquivo("mass:/FNFPS2/assets/font.rgba", 384 * 192 * 4);
    p4_txt("font.rgba: %d B (esperado %d)\n", n, 384 * 192 * 4);
    if (n <= 0) { p4_txt("  NAO ABRIU\n"); return; }
    p4_txt("primeiros 32 B: ");
    { int i; for (i = 0; i < 32 && i < n; i++) p4_txt("%02X ", g_buf4[i]); }
    p4_txt("\n");

    for (x = 0; x < 384; x++) ink_col[x] = 0;
    for (y = 0; y < 192; y++) ink_row[y] = 0;
    for (y = 0; y < 192; y++) {
        const unsigned char *lin = g_buf4 + y * 384 * 4;
        for (x = 0; x < 384; x++) {
            if (lin[x * 4 + 3] > 16) { ink_col[x]++; ink_row[y]++; }
        }
    }
    p4_txt("tinta por COLUNA (384 valores, 0-9 = dezenas):\n");
    for (x = 0; x < 384; x++) {
        int v = ink_col[x];
        p4_txt("%c", v == 0 ? '.' : (char)('0' + (v > 9 ? 9 : v)));
        if ((x & 63) == 63) p4_txt("\n");
    }
    p4_txt("tinta por LINHA (192 valores, 0-9 = dezenas):\n");
    for (y = 0; y < 192; y++) {
        int v = ink_row[y];
        p4_txt("%c", v == 0 ? '.' : (char)('0' + (v > 9 ? 9 : v)));
        if ((y & 63) == 63) p4_txt("\n");
    }
    /* corredores vazios em cada grade candidata */
    {
        static const int cand_w[4] = { 16, 20, 24, 32 };
        static const int cand_h[4] = { 16, 20, 24, 32 };
        int a, b;
        p4_txt("corredores vazios (colunas e linhas sem tinta nos limites):\n");
        for (a = 0; a < 4; a++) {
            for (b = 0; b < 4; b++) {
                int cw = cand_w[a], ch = cand_h[b], okc = 0, okr = 0, k;
                if (384 % cw || 192 % ch) continue;
                for (k = 0; k < 384; k += cw) if (ink_col[k] == 0) okc++;
                for (k = 0; k < 192; k += ch) if (ink_row[k] == 0) okr++;
                p4_txt("  celula %2dx%-2d -> %d/%d colunas vazias, %d/%d linhas vazias\n",
                       cw, ch, okc, 384 / cw, okr, 192 / ch);
            }
        }
    }
    /* bbox das 8 primeiras celulas na grade 24x32 */
    p4_txt("bbox das 8 primeiras celulas na grade 24x32 (x0,y0,x1,y1,tinta):\n");
    {
        int c;
        for (c = 0; c < 8; c++) {
            int cx = (c % 16) * 24, cy = (c / 16) * 32;
            int x0 = 9999, y0 = 9999, x1 = -1, y1 = -1, cnt = 0, xx, yy;
            for (yy = 0; yy < 32; yy++) {
                for (xx = 0; xx < 24; xx++) {
                    int px = cx + xx, py = cy + yy;
                    if (px >= 384 || py >= 192) continue;
                    if (g_buf4[(py * 384 + px) * 4 + 3] > 16) {
                        if (px < x0) x0 = px; if (px > x1) x1 = px;
                        if (py < y0) y0 = py; if (py > y1) y1 = py;
                        cnt++;
                    }
                }
            }
            p4_txt("  cel %d: %4d,%4d,%4d,%4d  tinta=%d\n", c, x0, y0, x1, y1, cnt);
        }
    }
    n = le_arquivo("mass:/FNFPS2/assets/font.widths", 96);
    p4_txt("font.widths: %d B: ", n);
    { int i; for (i = 0; i < n && i < 96; i++) p4_txt("%02X ", g_buf4[i]); }
    p4_txt("\n");
}

static void secao_vocais(void)
{
    DIR *d;
    int feitos = 0;
    p4_txt("\n========== 2. VOCAIS DENTRO DO stems.pcm ==========\n");
    p4_txt("8 B por amostra: InstL InstR VoxL VoxR (int16). Se 'vox' der 0.0%%\n"
           "nas tres posicoes, o arquivo NAO TEM vozes.\n");
    d = opendir("mass:/FNFPS2/assets/songs");
    if (!d) { p4_txt("  songs/ nao abriu\n"); return; }
    for (;;) {
        struct dirent *e = readdir(d);
        char path[160];
        int fd, tam;
        if (!e) break;
        if (e->d_name[0] == '.') continue;
        snprintf(path, sizeof(path), "mass:/FNFPS2/assets/songs/%s/stems.pcm", e->d_name);
        fd = open(path, O_RDONLY);
        if (fd < 0) continue;
        tam = (int)lseek(fd, 0, SEEK_END);
        p4_txt("  %-18s %9d B", e->d_name, tam);
        if (tam >= 80000) {
            static const int frac[3] = { 25, 50, 75 };
            int k;
            p4_txt("  (");
            for (k = 0; k < 3; k++) {
                int off = (int)((long long)tam * frac[k] / 100) & ~7;
                int lido, i, nz_i = 0, nz_v = 0, tot = 0;
                lseek(fd, off, SEEK_SET);
                lido = read(fd, g_buf4, 8192) & ~7;
                for (i = 0; i + 7 < lido; i += 8) {
                    short il = (short)(g_buf4[i] | (g_buf4[i + 1] << 8));
                    short ir = (short)(g_buf4[i + 2] | (g_buf4[i + 3] << 8));
                    short vl = (short)(g_buf4[i + 4] | (g_buf4[i + 5] << 8));
                    short vr = (short)(g_buf4[i + 6] | (g_buf4[i + 7] << 8));
                    if (il || ir) nz_i++;
                    if (vl || vr) nz_v++;
                    tot++;
                }
                p4_txt("%d%%: inst=%d%% vox=%d%%%s", frac[k],
                       tot ? nz_i * 100 / tot : 0, tot ? nz_v * 100 / tot : 0,
                       k < 2 ? ", " : "");
            }
            p4_txt(")\n");
        } else {
            p4_txt("  (curto demais para amostrar)\n");
        }
        close(fd);
        if (++feitos >= 6) break;
    }
    closedir(d);
    if (!feitos) p4_txt("  nenhuma pasta de musica com stems.pcm\n");
}

static void secao_pacotes(void)
{
    DIR *d;
    int palcos = 0;
    p4_txt("\n========== 3. PACOTES (palcos CFZ4 e personagens CHP4) ==========\n");
    d = opendir("mass:/FNFPS2/assets/stages");
    if (!d) { p4_txt("  stages/ nao abriu\n"); }
    else {
        for (;;) {
            struct dirent *e = readdir(d);
            DIR *sd;
            char dir[160];
            int arquivos = 0, maus = 0;
            if (!e) break;
            if (e->d_name[0] == '.') continue;
            snprintf(dir, sizeof(dir), "mass:/FNFPS2/assets/stages/%s", e->d_name);
            sd = opendir(dir);
            if (!sd) { p4_txt("  %s: NAO ABRIU\n", e->d_name); continue; }
            p4_txt("  %s:", e->d_name);
            for (;;) {
                struct dirent *f = readdir(sd);
                char path[220];
                int fd, n;
                if (!f) break;
                if (f->d_name[0] == '.') continue;
                snprintf(path, sizeof(path), "%s/%s", dir, f->d_name);
                fd = open(path, O_RDONLY);
                if (fd < 0) { p4_txt(" [%s:nao abre]", f->d_name); maus++; continue; }
                n = (int)read(fd, g_buf4, 32);
                close(fd);
                arquivos++;
                if (n >= 16 && memcmp(g_buf4, "CFZ4", 4) == 0) {
                    p4_txt(" %s(v%u n%u fb%u)", f->d_name,
                           (unsigned)(g_buf4[4] | (g_buf4[5] << 8) | (g_buf4[6] << 16) | ((unsigned)g_buf4[7] << 24)),
                           (unsigned)(g_buf4[8] | (g_buf4[9] << 8) | (g_buf4[10] << 16) | ((unsigned)g_buf4[11] << 24)),
                           (unsigned)(g_buf4[12] | (g_buf4[13] << 8) | (g_buf4[14] << 16) | ((unsigned)g_buf4[15] << 24)));
                } else {
                    p4_txt(" %s[magic %02X%02X%02X%02X]", f->d_name,
                           g_buf4[0], g_buf4[1], g_buf4[2], g_buf4[3]);
                    maus++;
                }
            }
            closedir(sd);
            p4_txt("  -> %d arquivos, %d maus\n", arquivos, maus);
            if (++palcos >= 21) break;
        }
        closedir(d);
    }
    p4_txt("personagens:\n");
    d = opendir("mass:/FNFPS2/assets/chars");
    if (!d) { p4_txt("  chars/ nao abriu\n"); return; }
    {
        int vistos = 0;
        for (;;) {
            struct dirent *e = readdir(d);
            char path[220];
            int fd, n;
            if (!e) break;
            if (e->d_name[0] == '.') continue;
            snprintf(path, sizeof(path), "mass:/FNFPS2/assets/chars/%s", e->d_name);
            fd = open(path, O_RDONLY);
            if (fd < 0) { p4_txt("  %s: nao abre\n", e->d_name); continue; }
            n = (int)read(fd, g_buf4, 32);
            close(fd);
            if (n >= 16 && memcmp(g_buf4, "CHP4", 4) == 0) {
                p4_txt("  %-24s CHP4 v%u n%u fb%u\n", e->d_name,
                       (unsigned)(g_buf4[4] | (g_buf4[5] << 8) | (g_buf4[6] << 16) | ((unsigned)g_buf4[7] << 24)),
                       (unsigned)(g_buf4[8] | (g_buf4[9] << 8) | (g_buf4[10] << 16) | ((unsigned)g_buf4[11] << 24)),
                       (unsigned)(g_buf4[12] | (g_buf4[13] << 8) | (g_buf4[14] << 16) | ((unsigned)g_buf4[15] << 24)));
            } else {
                p4_txt("  %-24s magic %02X%02X%02X%02X\n", e->d_name,
                       g_buf4[0], g_buf4[1], g_buf4[2], g_buf4[3]);
            }
            if (++vistos >= 6) break;
        }
        closedir(d);
    }
}

int main(int argc, char **argv)
{
    int i;
    (void)argc; (void)argv;

    p4_txt("FNF PS2 -- PROVA 4\n"
           "Mede a grade da fonte, o conteudo de voz dos stems e os cabecalhos\n"
           "dos pacotes de palco e personagem. Regrava este arquivo a cada secao:\n"
           "se parar, o ultimo '>> passo N' diz onde.\n");

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
    g_buf4 = (unsigned char *)memalign(64, 400000);
    if (!g_buf4) { p4_txt("sem memoria para o buffer\n"); return 1; }

    {   /* espera o mass: */
        u64 t0 = agora();
        int ok = 0, recarregou = 0;
        while (seg(agora() - t0) < 25.0 && !ok) {
            int fd = open("mass:/FNFPS2/assets/font.rgba", O_RDONLY);
            if (fd >= 0) { close(fd); ok = 1; break; }
            if (!recarregou && seg(agora() - t0) > 5.0) {
                int res = -1;
                SifExecModuleBuffer(usbd_irx, size_usbd_irx, 0, NULL, &res);
                SifExecModuleBuffer(usbhdfsd_irx, size_usbhdfsd_irx, 0, NULL, &res);
                recarregou = 1;
            }
            DelayThread(250000);
        }
        p4_txt("pendrive: %s em %.2f s\n", ok ? "montou" : "NAO montou", seg(agora() - t0));
        if (!ok) { p4_marca("sem pendrive"); for (;;) graph_wait_vsync(); }
    }
    p4_marca("inicio");

    secao_fonte();   p4_marca("fonte medida");
    secao_vocais();  p4_marca("vocais medidos");
    secao_pacotes(); p4_marca("pacotes medidos");

    p4_txt("\n=== FIM ===\n");
    p4_marca("fim");
    grava("PROVA4.txt", g_p4, g_p4n);
    for (;;) graph_wait_vsync();
    return 0;
}
