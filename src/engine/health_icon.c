#include "health_icon.h"

static unsigned int u16le(const unsigned char *p)
{
    return (unsigned int)p[0] | ((unsigned int)p[1] << 8);
}

static unsigned int u32le(const unsigned char *p)
{
    return (unsigned int)p[0] | ((unsigned int)p[1] << 8) |
           ((unsigned int)p[2] << 16) | ((unsigned int)p[3] << 24);
}

int hi_pack_ler(const unsigned char *buf, unsigned int bytes, hi_pack_t *out)
{
    unsigned int i, soma = 0;
    if (!buf || !out) return -1;
    if (bytes < HI_HEADER_BYTES) return -1;
    if (buf[0] != 'I' || buf[1] != 'C' || buf[2] != 'P' || buf[3] != '1') return -2;
    if (u16le(buf + 4) != 1u) return -3;

    out->w = u16le(buf + 6);
    out->h = u16le(buf + 8);
    out->quadros = u16le(buf + 10);
    out->psm = u16le(buf + 12);
    out->passo = u16le(buf + 14);
    out->flags = u16le(buf + 16);
    out->bytes_quadro = u32le(buf + 20);
    out->dados_bytes = u32le(buf + 24);
    out->soma = u32le(buf + 28);

    if (!out->w || !out->h) return -4;                 /* o desenho divide por w */
    if (!out->quadros || out->quadros > 3u) return -5;  /* idle, losing, winning */
    if (out->psm != HI_PSM_16) return -6;
    if (!out->passo || (out->passo & 63u)) return -7;
    if (out->passo < out->w) return -7;
    if (out->bytes_quadro != out->passo * out->h * 2u) return -8;
    if (out->dados_bytes != out->bytes_quadro * out->quadros) return -9;
    if (bytes < HI_HEADER_BYTES + out->dados_bytes) return -9;
    for (i = 0; i < out->dados_bytes; i++) soma += buf[HI_HEADER_BYTES + i];
    if (soma != out->soma) return -10;
    return 0;
}

const unsigned char *hi_quadro(const unsigned char *buf, const hi_pack_t *p,
                               unsigned int i)
{
    if (!buf || !p || i >= p->quadros) return 0;
    return buf + HI_HEADER_BYTES + (unsigned long)i * p->bytes_quadro;
}

unsigned int hi_vram_bytes(const hi_pack_t *p)
{
    if (!p || p->psm != HI_PSM_16) return 0;
    return p->passo * p->h * 2u;
}

float hi_tela_px(void) { return HI_TELA_PX; }

unsigned int hi_estado(float vida, unsigned int quadros)
{
    /* A ordem importa: o original testa "perdendo" primeiro no estado Idle
       (HealthIcon.hx L370-384). Com 2 quadros nao existe a animacao de
       "ganhando" (loadAnimationOld so' a registra com numFrames >= 3), entao o
       teto fica sendo o proprio idle. */
    if (vida < HI_LIMIAR_PERDENDO) return HI_PERDENDO;
    if (quadros >= 3u && vida > HI_LIMIAR_GANHANDO) return HI_GANHANDO;
    return HI_IDLE;
}

float hi_vida_oponente(float vida)
{
    /* MAXIMUM_HEALTH = Constants.HEALTH_MAX = 2.0. */
    return 2.0f - vida;
}

void hi_bop_reset(hi_bop_t *b)
{
    if (!b) return;
    b->bop_t = 0.0f;
    b->bop_dur = 0.0f;
    b->ativo = 0;
}

void hi_bop_passo(hi_bop_t *b, float step_len_s)
{
    float dur;
    if (!b) return;
    /* min(stepLengthMs * 0.002, 0.175) -- HealthIcon.hx L349. */
    dur = step_len_s * 2.0f;
    if (dur > 0.175f) dur = 0.175f;
    b->bop_dur = dur;
    b->bop_t = 0.0f;
    b->ativo = 1;
}

void hi_bop_tick(hi_bop_t *b, float dt)
{
    if (!b || !b->ativo) return;
    b->bop_t += dt;
    if (b->bop_t >= b->bop_dur) {
        b->bop_t = b->bop_dur;
        b->ativo = 0;
    }
}

float hi_bop_escala(const hi_bop_t *b)
{
    float f;
    if (!b || !b->ativo || b->bop_dur <= 0.0f) return 1.0f;
    /* Tween linear de 1.0+BOP_ESCALA de volta para 1.0. */
    f = b->bop_t / b->bop_dur;
    if (f < 0.0f) f = 0.0f;
    if (f > 1.0f) f = 1.0f;
    return 1.0f + HI_BOP_ESCALA * (1.0f - f);
}

void hi_posicao(int jogador, float barra_x, float barra_w, float barra_y,
                float vida, float icone_w, float icone_h,
                float *x, float *y)
{
    float fronteira;
    if (vida < 0.0f) vida = 0.0f;
    if (vida > 2.0f) vida = 2.0f;
    /* remapToRange(vida, 0, 2, 100, 0) * 0.01 = 1 - vida/2. */
    fronteira = barra_x + barra_w * (1.0f - vida * 0.5f);
    if (x) {
        if (jogador == 0) *x = fronteira - HI_POS_OFFSET;
        else              *x = fronteira - (icone_w - HI_POS_OFFSET);
    }
    if (y) *y = barra_y - icone_h * 0.5f;
}
