#include "chart.h"
#include <string.h>

/* Leitor de little-endian sem depender de alinhamento do buffer, porque
   o buffer vem de malloc() e no PS2 o EE nao gosta de inteiro
   desalinhado. */
static unsigned int le_u32(const unsigned char *p)
{
    return (unsigned int)p[0] | ((unsigned int)p[1] << 8)
         | ((unsigned int)p[2] << 16) | ((unsigned int)p[3] << 24);
}
static unsigned short le_u16(const unsigned char *p)
{
    return (unsigned short)((unsigned int)p[0] | ((unsigned int)p[1] << 8));
}
static double le_f32(const unsigned char *p)
{
    unsigned int bits = le_u32(p);
    float f;
    memcpy(&f, &bits, 4);
    return (double)f;
}

static const char *const EVENT_NAMES[EV_TIPO_COUNT] = {
    "FocusCamera", "ZoomCamera", "ScrollSpeed", "SetCameraBop",
    "SetHealthIcon", "SetTargetBopSpeed", "PlayAnimation",
    "SetCharacter", "SetStage"
};

const char *chart_event_nome(int tipo)
{
    if (tipo < 0 || tipo >= EV_TIPO_COUNT) return "desconhecido";
    return EVENT_NAMES[tipo];
}

static void copia_str(char *dst, const unsigned char *src, int max)
{
    int i = 0;
    for (; i < max - 1 && src[i]; i++) dst[i] = (char)src[i];
    dst[i] = 0;
}

unsigned int chart_amostra_para_ms(unsigned int amostra)
{
    /* 48000 amostras por segundo: /48, arredondando para o mais proximo.
       Erro maximo de 0,5 ms, que e 1,1% da janela 'sick' do PBOT1. */
    return (amostra + 24u) / 48u;
}

unsigned int chart_ms_para_amostra(unsigned int ms)
{
    return ms * 48u;
}

static unsigned int fnv1a(const unsigned char *p, unsigned int n)
{
    unsigned int h = 2166136261u, i;
    for (i = 0; i < n; i++) { h ^= p[i]; h *= 16777619u; }
    return h;
}

/* --------------------------------------------------------------------
 * FCH3 - o formato que os 81 charts do projeto ja usam
 * --------------------------------------------------------------------
 * Cabecalho de 40 bytes:
 *   0  "FCH3"
 *   4  u16 versao = 3
 *   6  u16 bytes_por_nota = 12
 *   8  u32 quantidade de notas
 *   12 u16 quantidade de bpms
 *   14 u16 quantidade de secoes
 *   16 u16 quantidade de eventos
 *   18 u16 reservado = 0
 *   20 u16 bytes_por_bpm = 16
 *   22 u16 bytes_por_secao = 8
 *   24 u16 bytes_por_evento = 16
 *   26 u16 reservado = 0
 *   28 u32 checksum FNV-1a do payload
 *   32 u32 song_end_sample
 *   36 u32 reservado = 0
 *
 * Payload: notas (sample u32, duration u32, lane u8, must_press u8,
 * ordem u16), bpms (start_step, start_sample, samples_per_step, bpm),
 * secoes (start_sample u32, u8, u8, ordem u16) e eventos (sample u32,
 * type u32, value u32, ordem u32).
 *
 * As validacoes abaixo sao as MESMAS que tutorial_chart.c fazia: um
 * chart corrompido tem de falhar na carga, nao no meio da musica.
 */
static int chart_load_fch3(const unsigned char *p, unsigned int len, chart_t *c)
{
    unsigned int n_notas, n_bpms, n_secoes, n_eventos, fim, off, i, payload;

    if (len < 40) return -2;
    if (le_u16(p + 4) != 3) return -4;
    if (le_u16(p + 6) != 12 || le_u16(p + 20) != 16 || le_u16(p + 22) != 8
        || le_u16(p + 24) != 16) return -4;
    if (le_u16(p + 18) || le_u16(p + 26) || le_u32(p + 36)) return -4;

    n_notas   = le_u32(p + 8);
    n_bpms    = le_u16(p + 12);
    n_secoes  = le_u16(p + 14);
    n_eventos = le_u16(p + 16);
    fim       = le_u32(p + 32);

    if (!n_notas || !fim) return -9;
    if (n_notas > CHART_MAX_NOTES || n_eventos > CHART_MAX_EVENTS) return -5;
    if (n_secoes > CHART_MAX_SECTIONS || n_bpms > CHART_MAX_BPMS) return -5;

    payload = n_notas * 12u + n_bpms * 16u + n_secoes * 8u + n_eventos * 16u;
    if (40u + payload != len) return -6;
    if (fnv1a(p + 40, payload) != le_u32(p + 28)) return -8;

    memset(c, 0, sizeof(*c));
    /* nome e dificuldade nao existem no FCH3: quem carrega preenche. */
    c->fim_amostras = fim;
    c->duracao_ms = chart_amostra_para_ms(fim);

    off = 40;
    for (i = 0; i < n_notas; i++, off += 12) {
        const unsigned char *r = p + off;
        chart_note_t *n = &c->notas[i];
        unsigned int sample = le_u32(r), dur = le_u32(r + 4);
        if (r[8] > 3 || r[9] > 1) return -9;
        if (le_u16(r + 10) != i) return -9;
        if (sample > fim || dur > fim - sample) return -9;
        if (i && sample < le_u32(p + 40 + (i - 1u) * 12u)) return -9;
        n->time_ms   = chart_amostra_para_ms(sample);
        n->length_ms = chart_amostra_para_ms(dur); /* duracao tambem em amostras */
        n->dir       = r[8];
        n->kind      = r[9] ? NOTE_HOLD : NOTE_TAP;
        /* O FCH3 nao guarda de quem e a nota: no tutorial todas sao do
           jogador. Para as musicas o campo e preenchido por
           chart_marcar_oponente() a partir da trilha de vocais, como o
           Balanceamento do jogo faz. */
        n->player    = 1;
        n->usado     = 0;
    }
    c->n_notes = n_notas;
    c->total_notes = n_notas;

    for (i = 0; i < n_bpms; i++, off += 16) {
        const unsigned char *r = p + off;
        chart_bpm_t *b = &c->bpms[i];
        b->start_step       = le_u32(r);
        b->start_sample     = le_u32(r + 4);
        b->samples_per_step = le_u32(r + 8);
        b->bpm_milesimos    = le_u32(r + 12);
        if (!b->samples_per_step || b->start_sample > fim) return -9;
        if (i && (b->start_step <= c->bpms[i - 1].start_step
                  || b->start_sample <= c->bpms[i - 1].start_sample)) return -9;
    }
    c->n_bpms = n_bpms;
    if (n_bpms) {
        if (c->bpms[0].start_step || c->bpms[0].start_sample) return -9;
        c->bpm_inicial = (double)c->bpms[0].bpm_milesimos / 1000.0;
    } else {
        c->bpm_inicial = 100.0;
    }
    c->time_sig_num = 4;
    c->time_sig_den = 4;

    for (i = 0; i < n_secoes; i++, off += 8) {
        const unsigned char *r = p + off;
        unsigned int start = le_u32(r);
        if (r[4] > 1 || !r[5] || le_u16(r + 6) != i) return -9;
        if (i && start <= le_u32(p + off - 8)) return -9;
        c->secoes[i].time_ms = (double)(start / 48u);
        c->secoes[i].bpm = 0.0; /* o BPM vem do vetor de bpms */
    }
    c->n_sections = n_secoes;

    for (i = 0; i < n_eventos; i++, off += 16) {
        const unsigned char *r = p + off;
        chart_event_t *e = &c->eventos[i];
        unsigned int tipo = le_u32(r + 4), valor = le_u32(r + 8);
        if (!tipo || le_u32(r + 12) != i) return -9;
        if (le_u32(r) > fim) return -9;
        if (i && le_u32(r) < le_u32(p + off - 16)) return -9;
        e->time_ms     = chart_amostra_para_ms(le_u32(r));
        e->bruto_tipo  = tipo;
        e->bruto_valor = valor;
        e->n_params    = 2;
        e->p[0]        = (float)valor;
        e->p[1]        = (float)tipo;
        e->ease        = 0xFF;
        /* Tipo 1 e FocusCamera (main.c: "evento tipo 1 do chart =
           FocusCamera; char 0 = BF, 1 = oponente"). Os demais ficam com
           o par cru preservado para o estado de jogo interpretar. */
        e->tipo = (tipo == 1) ? EV_FOCUS_CAMERA : EV_FOCUS_CAMERA;
    }
    c->n_events = n_eventos;
    chart_rewind(c);
    return 0;
}

int chart_load(const void *buf, unsigned int len, chart_t *c)
{
    const unsigned char *p = (const unsigned char *)buf;
    const unsigned char *fim;
    unsigned int n_notas, n_eventos, n_secoes, i, off;
    int desordenado = 0;

    if (!buf || !c) return -1;
    if (len < 40) return -2;
    if (!memcmp(p, "FCH3", 4)) return chart_load_fch3(p, len, c);
    if (len < 88) return -2;
    if (memcmp(p, CHART_MAGIC, 4)) return -3;

    memset(c, 0, sizeof(*c));
    /* 4 magic | 4 versao | 32 nome | 4 dificuldade | 4 bpm | 4 scroll e/n/h
       | 4 timesig | 4 total_notes | 4 n_notas | 4 n_eventos | 4 n_secoes
       | 4 duracao = 76 bytes */
    if (le_u32(p + 4) != 1) return -4;
    copia_str(c->nome, p + 8, 32);
    c->dificuldade  = (int)le_u32(p + 40);
    c->bpm_inicial  = le_f32(p + 44);
    c->scroll_easy  = le_f32(p + 48);
    c->scroll_normal= le_f32(p + 52);
    c->scroll_hard  = le_f32(p + 56);
    c->time_sig_num = (int)le_u32(p + 60);
    c->time_sig_den = (int)le_u32(p + 64);
    c->total_notes  = le_u32(p + 68);
    n_notas         = le_u32(p + 72);
    n_eventos       = le_u32(p + 76);
    n_secoes        = le_u32(p + 80);
    c->duracao_ms   = le_u32(p + 84);

    if (n_notas   > CHART_MAX_NOTES)   return -5;
    if (n_eventos > CHART_MAX_EVENTS)  return -5;
    if (n_secoes  > CHART_MAX_SECTIONS)return -5;

    off = 88;
    fim = p + len;

    /* notas: 12 bytes cada */
    if (off + n_notas * 12u > (unsigned int)(fim - p)) return -6;
    for (i = 0; i < n_notas; i++, off += 12) {
        chart_note_t *n = &c->notas[i];
        n->time_ms   = le_u32(p + off);
        n->length_ms = le_u32(p + off + 4);
        n->dir       = p[off + 8];
        n->kind      = p[off + 9];
        n->player    = p[off + 10];
        n->usado     = 0;
        if (n->dir > 3) n->dir = 0;
        if (i && c->notas[i - 1].time_ms > n->time_ms) desordenado = 1;
    }
    c->n_notes = n_notas;

    /* eventos: 4 tempo | 2 tipo | 1 n_params | 1 ease | 6*4 params | 2*24 strings | 1 flags = 64 */
    if (off + n_eventos * 64u > (unsigned int)(fim - p)) return -6;
    for (i = 0; i < n_eventos; i++, off += 64) {
        chart_event_t *e = &c->eventos[i];
        int k;
        e->time_ms  = le_u32(p + off);
        e->tipo     = le_u16(p + off + 4);
        e->n_params = p[off + 6];
        e->ease     = p[off + 7];
        if (e->n_params > CHART_EVENT_PARAMS) e->n_params = CHART_EVENT_PARAMS;
        for (k = 0; k < CHART_EVENT_PARAMS; k++)
            e->p[k] = (float)le_f32(p + off + 8 + k * 4);
        copia_str(e->s[0], p + off + 32, CHART_STR_LEN);
        copia_str(e->s[1], p + off + 32 + CHART_STR_LEN, CHART_STR_LEN);
        e->flags = p[off + 32 + 2 * CHART_STR_LEN];
        if (e->tipo >= EV_TIPO_COUNT) e->tipo = EV_FOCUS_CAMERA;
        if (e->ease >= 13) e->ease = 0xFF;
        if (i && c->eventos[i - 1].time_ms > e->time_ms) desordenado = 1;
    }
    c->n_events = n_eventos;

    /* secoes: 8 bytes cada (f32 tempo, f32 bpm) */
    if (off + n_secoes * 8u > (unsigned int)(fim - p)) return -6;
    for (i = 0; i < n_secoes; i++, off += 8) {
        c->secoes[i].time_ms = le_f32(p + off);
        c->secoes[i].bpm     = le_f32(p + off + 4);
    }
    c->n_sections = n_secoes;

    chart_rewind(c);
    return desordenado ? -7 : 0;
}

void chart_rewind(chart_t *c)
{
    unsigned int i;
    c->cursor_notas = 0;
    c->cursor_eventos = 0;
    for (i = 0; i < c->n_notes; i++) c->notas[i].usado = 0;
}

int chart_poll_events(chart_t *c, unsigned int agora_ms,
                      const chart_event_t **saida, int max)
{
    int n = 0;
    while (c->cursor_eventos < c->n_events && n < max) {
        const chart_event_t *e = &c->eventos[c->cursor_eventos];
        if (e->time_ms > agora_ms) break;
        if (saida) saida[n] = e;
        c->cursor_eventos++;
        n++;
    }
    return n;
}

int chart_poll_notes(chart_t *c, unsigned int agora_ms, unsigned int antecipacao_ms,
                     const chart_note_t **saida, int max)
{
    int n = 0;
    unsigned int limite = agora_ms + antecipacao_ms;
    while (c->cursor_notas < c->n_notes && n < max) {
        chart_note_t *nt = &c->notas[c->cursor_notas];
        if (nt->time_ms > limite) break;
        nt->usado = 1;
        if (saida) saida[n] = nt;
        c->cursor_notas++;
        n++;
    }
    return n;
}

unsigned int chart_restantes(const chart_t *c)
{
    return (c->cursor_notas < c->n_notes) ? (c->n_notes - c->cursor_notas) : 0u;
}

unsigned int chart_fim_ms(const chart_t *c)
{
    unsigned int fim = 0;
    if (c->n_notes) {
        const chart_note_t *n = &c->notas[c->n_notes - 1];
        fim = n->time_ms + n->length_ms;
    }
    if (c->n_events) {
        unsigned int e = c->eventos[c->n_events - 1].time_ms;
        if (e > fim) fim = e;
    }
    return fim;
}
