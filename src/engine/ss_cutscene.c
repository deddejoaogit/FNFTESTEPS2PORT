#include "ss_cutscene.h"

#include <string.h>

/* Cabecalho do arquivo assado (`tools/assar_cutscene_sserafim.py`):
     16 B   "FQC1" | u16 n_sprites | u16 n_frames | u32 fps100 | u32 escala1000
     8 B    por sprite: u16 x, y, w, h  (espaco do DOCUMENTO)
     4 B    por quadro: u32 offset do primeiro quad (relativo ao fim da tabela)
    14 B    por quad
*/
#define SSC_MAGIC "FQC1"
/* 24 B: "FQC1" | u16 n_sprites | u16 n_frames | u32 fps100 | u32 escala1000 |
        u16 tex_w | u16 tex_h | u16 n_labels | u16 reservado (0)            */
#define SSC_CAB 24
#define SSC_SPRITE_BYTES 8
#define SSC_LABEL 12
#define SSC_QUAD_BYTES 14

static unsigned int le_u32(const unsigned char *p)
{
    return (unsigned int)p[0] | ((unsigned int)p[1] << 8) |
           ((unsigned int)p[2] << 16) | ((unsigned int)p[3] << 24);
}

static unsigned int le_u16(const unsigned char *p)
{
    return (unsigned int)p[0] | ((unsigned int)p[1] << 8);
}

static short le_s16(const unsigned char *p)
{
    return (short)((unsigned int)p[0] | ((unsigned int)p[1] << 8));
}

int ssc_abrir(ssc_pack_t *p, const void *dados, unsigned int len)
{
    const unsigned char *d = (const unsigned char *)dados;
    unsigned int tabela;
    if (!p) return SSC_ERRO_MAGIC;
    memset(p, 0, sizeof(*p));
    if (!d || len < SSC_CAB) return SSC_ERRO_TRUNCADO;
    if (memcmp(d, SSC_MAGIC, 4) != 0) return SSC_ERRO_MAGIC;
    p->n_sprites = (int)le_u16(d + 4);
    p->n_frames = (int)le_u16(d + 6);
    p->fps100 = (int)le_u32(d + 8);
    p->escala1000 = (int)le_u32(d + 12);
    p->tex_w = (int)le_u16(d + 16);
    p->tex_h = (int)le_u16(d + 18);
    p->n_labels = (int)le_u16(d + 20);
    if (p->n_sprites <= 0 || p->n_frames <= 0) return SSC_ERRO_VAZIO;
    if (p->fps100 <= 0) return SSC_ERRO_VAZIO;
    /* A textura e' obrigatoria: um pacote sem tamanho de textura nao pode ser
       desenhado, e o defeito tem de aparecer aqui e nao como quad com UV = 0. */
    if (p->tex_w <= 0 || p->tex_h <= 0) return SSC_ERRO_VAZIO;
    if (p->n_labels < 0 || p->n_labels > SSC_MAX_LABELS) return SSC_ERRO_VAZIO;
    tabela = SSC_CAB + (unsigned int)p->n_sprites * SSC_SPRITE_BYTES;
    if (len < tabela + (unsigned int)p->n_labels * SSC_LABEL_BYTES +
                      (unsigned int)p->n_frames * 4u)
        return SSC_ERRO_TRUNCADO;
    p->rects = d + SSC_CAB;
    p->labels = d + tabela;
    tabela += (unsigned int)p->n_labels * SSC_LABEL_BYTES;
    p->offsets = d + tabela;
    p->quads = d + tabela + (unsigned int)p->n_frames * 4u;
    p->quads_bytes = len - (tabela + (unsigned int)p->n_frames * 4u);
    return SSC_OK;
}

int ssc_quadro_em(const ssc_pack_t *p, float t)
{
    int f;
    if (!p || p->n_frames <= 0 || p->fps100 <= 0) return 0;
    if (t < 0.0f) t = 0.0f;
    /* O quadro e' o tempo vezes a taxa, truncado (o flixel avanca um quadro por
       1/fps) e CONGELADO no ultimo -- nao e' modulo.
       Por que nao repete: o `cutsceneMain` tem 631 quadros a 24 fps (26,3 s) e
       a cena usa o sprite so' ate' 563/24 (23,5 s), quando o
       `setCutsceneVisibility(false)` o apaga; o `getup` do GF/BF tem 123
       quadros (5,1 s) comecando em 710/24 e ATRAVESSA o comeco da musica. Com
       laco, os dois voltariam a sentar no meio da `spaghetti`. Uma animacao de
       `FlxAnimationController` sem laco para no ultimo quadro, que e' o que o
       `curFrame = 23` do `playCutsceneFromRestart` tambem assume. */
    f = (int)(t * ((float)p->fps100 / 100.0f));
    if (f >= p->n_frames) f = p->n_frames - 1;
    return f;
}

int ssc_rotulo(const ssc_pack_t *p, const char *nome, int *primeiro, int *quadros)
{
    int i;
    if (!p || !nome || p->n_labels <= 0) return SSC_ERRO_VAZIO;
    for (i = 0; i < p->n_labels; i++) {
        const unsigned char *l = p->labels + (unsigned int)i * SSC_LABEL;
        if (strncmp((const char *)l + 4, nome, 8) != 0) continue;
        if (primeiro) *primeiro = (int)le_u16(l);
        if (quadros) *quadros = (int)le_u16(l + 2);
        return SSC_OK;
    }
    return SSC_ERRO_VAZIO;
}

int ssc_quadro_rotulo(const ssc_pack_t *p, const char *nome, float t)
{
    int ini = 0, n = 0, f;
    if (ssc_rotulo(p, nome, &ini, &n) != SSC_OK || n <= 0) return -1;
    if (n == 1) return ini;               /* o `static`: um quadro so'       */
    if (t < 0.0f) t = 0.0f;
    /* Sem laco, como `ssc_quadro_em`: o rotulo `getup` tem 123 quadros e a
       animacao para no ultimo. */
    f = (int)(t * ((float)p->fps100 / 100.0f));
    if (f >= n) f = n - 1;
    f += ini;
    if (f >= p->n_frames) f = p->n_frames - 1;
    return f;
}

int ssc_quads(const ssc_pack_t *p, int f, ssc_quad_t *out, int max, int *total)
{
    unsigned int ini, fim;
    int n = 0;
    if (!p || !out || max <= 0 || p->n_frames <= 0) return 0;
    if (f < 0) f = 0;
    if (f >= p->n_frames) f = p->n_frames - 1;
    ini = le_u32(p->offsets + (unsigned int)f * 4u);
    if (f + 1 < p->n_frames) fim = le_u32(p->offsets + (unsigned int)(f + 1) * 4u);
    else fim = p->quads_bytes;
    if (fim > p->quads_bytes || ini > fim) {
        if (total) *total = 0;
        return 0;
    }
    if (total) *total = (int)((fim - ini) / SSC_QUAD_BYTES);
    while (ini + SSC_QUAD_BYTES <= fim && n < max) {
        const unsigned char *q = p->quads + ini;
        ssc_quad_t *o = &out[n];
        o->sprite = (unsigned short)le_u16(q);
        o->a = le_s16(q + 2);
        o->b = le_s16(q + 4);
        o->c = le_s16(q + 6);
        o->d = le_s16(q + 8);
        o->tx = le_s16(q + 10);
        o->ty = le_s16(q + 12);
        if ((int)o->sprite >= p->n_sprites) {   /* dado estragado: para aqui */
            if (total) *total = n;
            break;
        }
        ini += SSC_QUAD_BYTES;
        n++;
    }
    return n;
}

int ssc_cantos(const ssc_pack_t *p, const ssc_quad_t *q, float x, float y,
               float *px, float *py)
{
    const unsigned char *r;
    float a, b, c, d, tx, ty, w, h;
    if (!p || !q || !px || !py) return 0;
    if ((int)q->sprite >= p->n_sprites) return 0;
    r = p->rects + (unsigned int)q->sprite * SSC_SPRITE_BYTES;
    w = (float)le_u16(r + 4);
    h = (float)le_u16(r + 6);
    /* 1/4096 nos coeficientes e 1/4 no deslocamento, como no arquivo. */
    a = (float)q->a / 4096.0f;
    b = (float)q->b / 4096.0f;
    c = (float)q->c / 4096.0f;
    d = (float)q->d / 4096.0f;
    tx = (float)q->tx / 4.0f;
    ty = (float)q->ty / 4.0f;
    /* (lx,ly) nos quatro cantos do retangulo, na ordem do desenho. */
    px[0] = x + tx;                 py[0] = y + ty;
    px[1] = x + a * w + tx;         py[1] = y + b * w + ty;
    px[2] = x + a * w + c * h + tx; py[2] = y + b * w + d * h + ty;
    px[3] = x + c * h + tx;         py[3] = y + d * h + ty;
    return 1;
}

/* --------------------------------------------------------------- as curvas */

float ssc_sine_out(float t)
{
    /* sin(pi/2 * t) por serie de Taylor ate o termo de grau 7: em [0,1] o erro
       fica abaixo de 1e-5, e nao puxa libm para dentro do render. */
    float x;
    if (t <= 0.0f) return 0.0f;
    if (t >= 1.0f) return 1.0f;
    x = t * 1.57079632679f;
    return x - (x * x * x) / 6.0f + (x * x * x * x * x) / 120.0f -
           (x * x * x * x * x * x * x) / 5040.0f;
}

float ssc_circ_out(float t)
{
    /* sqrt(1 - (t-1)^2) = sqrt(t*(2-t)). Raiz por Newton, 5 passos, sem libm:
       para t em [0,1] o argumento esta' em [0,1] e a estimativa inicial 0,5
       converge para menos de 1e-6. */
    float u, g;
    int i;
    if (t <= 0.0f) return 0.0f;
    if (t >= 1.0f) return 1.0f;
    u = t * (2.0f - t);
    g = 0.5f + u * 0.5f;
    for (i = 0; i < 5; i++) g = 0.5f * (g + u / g);
    return g;
}

/* --------------------------------------------------------- a linha do tempo */

/* Nome proprio, de proposito: a checagem de fechamento transitivo do
   test/run_tests.sh e' textual e`lerp` tambem existe no main.c -- com o nome
   generico este modulo passaria a "entrar no jogo" sem ninguem chamar. */
static float ssc_mistura(float a, float b, float k)
{
    return a + (b - a) * k;
}

void ssc_estado(float t, ssc_estado_t *e)
{
    if (!e) return;
    if (t < 0.0f) t = 0.0f;

    /* ---- camera (L655-678 e L795-800) ---- */
    if (t < SSC_T_CAM_FIM) {
        float k = ssc_circ_out(t / SSC_T_CAM_FIM);
        e->zoom = ssc_mistura(0.5f, 0.7f, k);
        e->cam_x = ssc_mistura(660.0f, 660.0f, k);
        e->cam_y = ssc_mistura(-200.0f, 300.0f, k);
    } else if (t < SSC_T_SAIDA) {
        e->zoom = 0.7f;
        e->cam_x = 660.0f;
        e->cam_y = 300.0f;
    } else if (t < SSC_T_SAIDA + SSC_T_SAIDA_DUR) {
        float k = ssc_circ_out((t - SSC_T_SAIDA) / SSC_T_SAIDA_DUR);
        e->zoom = ssc_mistura(0.7f, 0.55f, k);
        e->cam_x = 1070.0f;
        e->cam_y = 470.0f;
    } else {
        e->zoom = 0.55f;
        e->cam_x = 1070.0f;
        e->cam_y = 470.0f;
    }

    /* ---- o fade preto da entrada: `camGame.fade(preto, 3, true)` ---- */
    e->fade_preto = t < 3.0f ? 1.0f - t / 3.0f : 0.0f;

    /* ---- o flash BRANCO de 563/24 (L725-731) ---- */
    e->fade_branco = 0.0f;
    if (t >= SSC_T_FLASH) {
        float k = (t - SSC_T_FLASH) / SSC_T_FLASH_DUR;
        e->fade_branco = k >= 1.0f ? 0.0f : 1.0f - k;
    }

    /* ---- a cobertura preta: sobe a 1 em 563/24 e desce `sineOut` em 3 s a
       partir de 650/24 (L726 e L799) ---- */
    if (t < SSC_T_FLASH) e->cover = 0.0f;
    else if (t < SSC_T_SAIDA) e->cover = 1.0f;
    else if (t < SSC_T_SAIDA + SSC_T_SAIDA_DUR)
        e->cover = 1.0f - ssc_sine_out((t - SSC_T_SAIDA) / SSC_T_SAIDA_DUR);
    else e->cover = 0.0f;

    /* ---- a cena: `setCutsceneVisibility(true)` no inicio e `(false)` em
       563/24 (L711 e L733) ---- */
    e->cena = t < SSC_T_CENA_FIM ? 1 : 0;
    /* `camHUD.visible = false` no inicio; volta quando a musica comeca. */
    e->hud = t >= SSC_T_CONTA ? 1 : 0;

    /* ---- os tres sprites (L610-628 e L790-806) ----
       O principal toca desde o inicio; as duas "get up" so' aparecem quando a
       cena volta, em 650/24, e a animacao delas comeca em 710/24. */
    e->principal_visivel = e->cena;
    e->gf_visivel = t >= SSC_T_SAIDA ? 1 : 0;
    e->bf_visivel = e->gf_visivel;
    /* O INSTANTE DENTRO DA ANIMACAO de cada um. O principal toca a linha toda
       desde o quadro 0 (`doAnim` em t=0). As duas "get up" voltam em 650/24 com
       `resetAnim()` (`animation.play("static")`, o rotulo de UM quadro) e trocam
       para `getup` em 710/24 -- nesse instante o relogio do `getup` comeca do
       ZERO, e nao de 710/24 (o `play` do flixel reinicia a animacao). */
    e->t_principal = t;
    e->anim_gf = e->anim_bf = (t >= SSC_T_GETUP_ANIM) ? 1 : 0;
    e->t_gf = e->t_bf = (t >= SSC_T_GETUP_ANIM) ? (t - SSC_T_GETUP_ANIM) : 0.0f;

    e->acabou = t >= SSC_T_CONTA ? 1 : 0;
}
