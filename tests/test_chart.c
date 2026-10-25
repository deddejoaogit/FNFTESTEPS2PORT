/* Teste do chart e do sistema de eventos.
   A garantia que importa: TODO evento do chart e entregue, na ordem, uma
   unica vez. Este teste conta, nao confia. */
#include "../src/engine/chart.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static int falhas = 0, checks = 0;
static void ok(int cond, const char *txt)
{
    checks++;
    if (!cond) { falhas++; printf("FALHOU: %s\n", txt); }
}
#define OK(c) ok((c), #c)

/* --- construtor de um FNC2 em memoria ----------------------------- */
static void pu32(unsigned char *p, unsigned int v)
{
    p[0] = (unsigned char)(v & 0xFF); p[1] = (unsigned char)((v >> 8) & 0xFF);
    p[2] = (unsigned char)((v >> 16) & 0xFF); p[3] = (unsigned char)((v >> 24) & 0xFF);
}
static void pf32(unsigned char *p, float f) { unsigned int b; memcpy(&b, &f, 4); pu32(p, b); }

typedef struct { unsigned int t, len; unsigned char dir, kind, player; } n_t;
typedef struct { unsigned int t; unsigned short tipo; unsigned char ease; float p[6]; const char *s0; } e_t;

static unsigned char *montar(const char *nome, unsigned int n_notas, const n_t *notas,
                             unsigned int n_eventos, const e_t *eventos,
                             unsigned int n_secoes, unsigned int total_notas,
                             unsigned int *tam_out)
{
    unsigned int tam = 88 + n_notas * 12 + n_eventos * 64 + n_secoes * 8;
    unsigned char *b = (unsigned char *)calloc(1, tam);
    unsigned int off = 88, i, k;
    memcpy(b, "FNC2", 4);
    pu32(b + 4, 1);
    { unsigned int l = (unsigned int)strlen(nome); if (l > 31) l = 31; memcpy(b + 8, nome, l); }
    pu32(b + 40, 1);            /* dificuldade normal */
    pf32(b + 44, 100.0f);       /* bpm */
    pf32(b + 48, 1.0f); pf32(b + 52, 1.0f); pf32(b + 56, 1.0f); /* scroll */
    pu32(b + 60, 4); pu32(b + 64, 4);   /* time sig */
    pu32(b + 68, total_notas);
    pu32(b + 72, n_notas);
    pu32(b + 76, n_eventos);
    pu32(b + 80, n_secoes);
    pu32(b + 84, 200000);
    for (i = 0; i < n_notas; i++, off += 12) {
        pu32(b + off, notas[i].t);
        pu32(b + off + 4, notas[i].len);
        b[off + 8] = notas[i].dir;
        b[off + 9] = notas[i].kind;
        b[off + 10] = notas[i].player;
    }
    for (i = 0; i < n_eventos; i++, off += 64) {
        pu32(b + off, eventos[i].t);
        b[off + 4] = (unsigned char)(eventos[i].tipo & 0xFF);
        b[off + 5] = (unsigned char)(eventos[i].tipo >> 8);
        b[off + 6] = 6;
        b[off + 7] = eventos[i].ease;
        for (k = 0; k < 6; k++) pf32(b + off + 8 + k * 4, eventos[i].p[k]);
        if (eventos[i].s0) memcpy(b + off + 32, eventos[i].s0, strlen(eventos[i].s0));
    }
    for (i = 0; i < n_secoes; i++, off += 8) {
        pf32(b + off, (float)(i * 10000));
        pf32(b + off + 4, (float)(100 + i * 10));
    }
    *tam_out = tam;
    return b;
}

/* --- construtor de um FCH3 (o formato real dos 81 charts) ---------- */
static void pu16(unsigned char *p, unsigned int v)
{
    p[0] = (unsigned char)(v & 0xFF); p[1] = (unsigned char)((v >> 8) & 0xFF);
}
static unsigned int fnv1a(const unsigned char *p, unsigned int n)
{
    unsigned int h = 2166136261u, i;
    for (i = 0; i < n; i++) { h ^= p[i]; h *= 16777619u; }
    return h;
}
static unsigned char *montar_fch3(unsigned int n_notas, unsigned int n_eventos,
                                  unsigned int n_secoes, unsigned int n_bpms,
                                  unsigned int fim_amostras, unsigned int *tam_out)
{
    unsigned int payload = n_notas * 12 + n_bpms * 16 + n_secoes * 8 + n_eventos * 16;
    unsigned int tam = 40 + payload, off = 40, i;
    unsigned char *b = (unsigned char *)calloc(1, tam);
    memcpy(b, "FCH3", 4);
    pu16(b + 4, 3);
    pu16(b + 6, 12);
    pu32(b + 8, n_notas);
    pu16(b + 12, n_bpms);
    pu16(b + 14, n_secoes);
    pu16(b + 16, n_eventos);
    pu16(b + 20, 16);
    pu16(b + 22, 8);
    pu16(b + 24, 16);
    pu32(b + 32, fim_amostras);
    /* notas: 4 por compasso, uma a cada 1/4 de segundo (7200 amostras) */
    for (i = 0; i < n_notas; i++, off += 12) {
        pu32(b + off, 7200u * (i + 1));
        pu32(b + off + 4, 0);
        b[off + 8] = (unsigned char)(i % 4);
        b[off + 9] = 0;
        pu16(b + off + 10, i);
    }
    /* bpms: um, a 100 BPM (100000 milesimos), 7200 amostras por passo */
    for (i = 0; i < n_bpms; i++, off += 16) {
        pu32(b + off, i * 1000u);
        pu32(b + off + 4, i * 7200000u);
        pu32(b + off + 8, 7200u);
        pu32(b + off + 12, 100000u);
    }
    /* secoes */
    for (i = 0; i < n_secoes; i++, off += 8) {
        pu32(b + off, i * 16u * 7200u);
        b[off + 4] = 0;
        b[off + 5] = 4;
        pu16(b + off + 6, i);
    }
    /* eventos: um a cada 137 amostras */
    for (i = 0; i < n_eventos; i++, off += 16) {
        pu32(b + off, 137u * (i + 1));
        pu32(b + off + 4, 1u + (i % 3));
        pu32(b + off + 8, i);
        pu32(b + off + 12, i);
    }
    pu32(b + 28, fnv1a(b + 40, payload));
    *tam_out = tam;
    return b;
}

int main(void)
{
    chart_t *c = (chart_t *)calloc(1, sizeof(chart_t));
    unsigned char *buf;
    unsigned int tam;
    n_t notas[64];
    e_t eventos[256];
    unsigned int i, entregues, total, resto;
    int r;

    printf("== carga basica ==\n");
    memset(notas, 0, sizeof(notas));
    for (i = 0; i < 8; i++) {
        notas[i].t = 1000u + i * 500u;
        notas[i].dir = (unsigned char)(i % 4);
        notas[i].kind = (i % 3 == 0) ? NOTE_HOLD : NOTE_TAP;
        notas[i].len = (i % 3 == 0) ? 750u : 0u;
        notas[i].player = 1;
    }
    for (i = 0; i < 40; i++) {
        eventos[i].t = 500u + i * 137u;
        eventos[i].tipo = (unsigned short)(i % EV_TIPO_COUNT);
        eventos[i].ease = (unsigned char)(i % 13);
        eventos[i].p[0] = (float)i;
        eventos[i].s0 = (i % 5 == 0) ? "bf" : NULL;
    }
    buf = montar("test-song", 8, notas, 40, eventos, 3, 8, &tam);
    r = chart_load(buf, tam, c);
    ok(r == 0, "chart_load devolve 0");
    OK(!strcmp(c->nome, "test-song"));
    OK(c->n_notes == 8);
    OK(c->n_events == 40);
    OK(c->n_sections == 3);
    OK(c->total_notes == 8);
    OK(c->notas[0].time_ms == 1000);
    OK(c->notas[0].length_ms == 750);   /* i%3==0 -> a nota 0 e hold */
    OK(c->notas[0].kind == NOTE_HOLD);
    OK(c->notas[1].length_ms == 0);
    OK(c->notas[1].kind == NOTE_TAP);
    OK(c->eventos[0].ease == 0);
    OK(c->eventos[1].ease == 1);
    OK(!strcmp(c->eventos[0].s[0], "bf"));
    OK(c->eventos[5].tipo == EV_SET_TARGET_BOP_SPEED);

    printf("== nenhum evento perdido ==\n");
    /* caminha o tempo inteiro de 1 ms em 1 ms, pegando no maximo 1 por
       chamada (o pior caso do laco do jogo) */
    total = 0;
    for (i = 0; i < 8000u; i++) {
        const chart_event_t *e;
        int n = chart_poll_events(c, i, &e, 1);
        if (n) total += (unsigned int)n;
    }
    OK(total == 40);
    /* e todos os que ja passaram sumiram */
    OK(chart_poll_events(c, 100000u, 0, 0) == 0);

    printf("== ordem e unicidade ==\n");
    chart_rewind(c);
    {
        unsigned int anterior = 0;
        int ordem_ok = 1, cont = 0;
        const chart_event_t *lote[8];
        unsigned int t;
        for (t = 0; t <= 8000u; t += 50u) {
            int n = chart_poll_events(c, t, lote, 8), k;
            for (k = 0; k < n; k++) {
                if (lote[k]->time_ms < anterior) ordem_ok = 0;
                anterior = lote[k]->time_ms;
                cont++;
            }
        }
        OK(ordem_ok);
        OK(cont == 40);
    }

    printf("== eventos no mesmo instante ==\n");
    for (i = 0; i < 20; i++) { eventos[i].t = 4321u; eventos[i].tipo = EV_ZOOM_CAMERA; }
    buf = montar("empate", 0, notas, 20, eventos, 0, 0, &tam);
    OK(chart_load(buf, tam, c) == 0);
    entregues = 0;
    for (i = 0; i <= 4321u; i += 1u) {
        const chart_event_t *lote[32];
        entregues += (unsigned int)chart_poll_events(c, i, lote, 32);
    }
    OK(entregues == 20);
    free(buf);

    printf("== notas com antecipacao ==\n");
    buf = montar("notas", 8, notas, 0, eventos, 0, 8, &tam);
    OK(chart_load(buf, tam, c) == 0);
    {
        const chart_note_t *lote[4];
        int n = chart_poll_notes(c, 0, 999, lote, 4);
        OK(n == 0);                       /* nenhuma nasce antes de 1000 */
        n = chart_poll_notes(c, 0, 1000, lote, 4);
        OK(n == 1);
        n = chart_poll_notes(c, 0, 1000, lote, 4);
        OK(n == 0);                       /* nao repete */
        n = chart_poll_notes(c, 0, 100000, lote, 4);
        OK(n == 4);                       /* respeita o maximo */
        resto = chart_restantes(c);
        OK(resto == 3);
        n = chart_poll_notes(c, 0, 100000, lote, 4);
        OK(n == 3);
        OK(chart_restantes(c) == 0);
    }
    OK(chart_fim_ms(c) == 1000u + 7u * 500u); /* ultima nota e tap */

    printf("== erros de arquivo ==\n");
    buf = montar("x", 8, notas, 0, eventos, 0, 8, &tam);
    OK(chart_load(NULL, tam, c) == -1);
    OK(chart_load(buf, 10, c) == -2);
    buf[0] = 'X';
    OK(chart_load(buf, tam, c) == -3);
    buf[0] = 'F';
    OK(chart_load(buf, tam - 10, c) == -6);   /* truncado */
    free(buf);

    printf("== capacidade ==\n");
    OK(CHART_MAX_NOTES >= 1035);   /* maior chart do jogo tem 1035 notas */
    OK(sizeof(chart_t) < 400000);  /* cabe num static do EE sem drama */
    printf("   sizeof(chart_t) = %u bytes\n", (unsigned int)sizeof(chart_t));

    printf("== FCH3: o formato real dos 81 charts ==\n");
    {
        /* o maior chart real tem 1035 notas; spaghetti tem 264 eventos */
        unsigned int tam3 = 0;
        unsigned char *b3 = montar_fch3(1035, 264, 12, 1, 1035u * 7200u + 7200u, &tam3);
        r = chart_load(b3, tam3, c);
        ok(r == 0, "FCH3 carrega (retorno 0)");
        OK(c->n_notes == 1035);
        OK(c->n_events == 264);
        OK(c->n_sections == 12);
        OK(c->n_bpms == 1);
        OK(c->bpm_inicial == 100.0);
        OK(c->notas[0].time_ms == 150);      /* 7200 amostras = 150 ms */
        OK(c->notas[0].dir == 0);
        OK(c->notas[1].dir == 1);
        OK(c->notas[3].dir == 3);
        OK(c->eventos[0].bruto_tipo == 1);
        OK(c->eventos[1].bruto_tipo == 2);
        OK(c->eventos[1].bruto_valor == 1);
        /* nenhum evento perdido ao ler o formato antigo */
        {
            unsigned int t, cont = 0;
            const chart_event_t *lote[16];
            for (t = 0; t <= 1035u * 150u; t += 7u)
                cont += (unsigned int)chart_poll_events(c, t, lote, 16);
            ok(cont == 264, "FCH3: 264 eventos entregues uma vez cada");
        }
        /* checksum errado tem de falhar */
        b3[60] ^= 0xFF;
        ok(chart_load(b3, tam3, c) == -8, "checksum FNV-1a errado e recusado");
        b3[60] ^= 0xFF;
        /* versao errada */
        b3[4] = 9;
        ok(chart_load(b3, tam3, c) == -4, "versao do FCH3 diferente e recusada");
        b3[4] = 3;
        /* tamanho que nao bate com o cabecalho */
        ok(chart_load(b3, tam3 - 12, c) == -6, "FCH3 truncado e recusado");
        free(b3);
        /* chart vazio (notas 0) e invalido */
        b3 = montar_fch3(0, 0, 1, 1, 48000, &tam3);
        ok(chart_load(b3, tam3, c) == -9, "FCH3 sem notas e recusado");
        free(b3);
    }

    printf("== nomes de evento ==\n");
    OK(!strcmp(chart_event_nome(EV_FOCUS_CAMERA), "FocusCamera"));
    OK(!strcmp(chart_event_nome(EV_ZOOM_CAMERA), "ZoomCamera"));
    OK(!strcmp(chart_event_nome(EV_SCROLL_SPEED), "ScrollSpeed"));
    OK(!strcmp(chart_event_nome(EV_SET_CAMERA_BOP), "SetCameraBop"));
    OK(!strcmp(chart_event_nome(EV_PLAY_ANIMATION), "PlayAnimation"));
    OK(!strcmp(chart_event_nome(999), "desconhecido"));

    printf("\n%d verificacoes, %d falhas\n", checks, falhas);
    free(c);
    return falhas ? 1 : 0;
}
