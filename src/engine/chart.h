/* chart.h - chart generico e sistema de eventos completo
 *
 * REGRA CENTRAL DESTE ARQUIVO: nenhum evento do chart e removido,
 * encurtado, adiado ou simplificado. Todos os 81 charts (27 musicas x 3
 * dificuldades) passam por aqui, e a entrega e garantida por
 * construcao: chart_poll_events() devolve cada evento uma unica vez, em
 * ordem de tempo, e o teste tests/test_chart.c verifica isso contra
 * charts sinteticos com milhares de eventos fora de ordem no arquivo.
 *
 * Os tipos de evento abaixo sao os ids REAIS do fonte oficial v0.8.6
 * (source/funkin/play/event (todos os .hx), chamada super('<id>') no construtor):
 *
 *   FocusCamera.hx:54   'FocusCamera'      x, y, char, duration, ease, easeDir
 *   ZoomCamera.hx:27    'ZoomCamera'       zoom, widescreenScaleX/Y, duration, mode, ease, easeDir
 *   ScrollSpeed.hx:32   'ScrollSpeed'      scroll, duration, ease, easeDir, strumline, absolute
 *   SetCameraBop.hx:35  'SetCameraBop'     intensity, offset, rate
 *   SetHealthIcon.hx:32 'SetHealthIcon'    icon do oponente / do jogador
 *   SetTargetBopSpeed.hx:19 'SetTargetBopSpeed'
 *   PlayAnimation.hx:18 'PlayAnimation'    target, anim, force
 *   SetCharacter / SetStage                  troca de personagem e de palco
 *
 * DOIS FORMATOS DE ARQUIVO
 *   FCH3 - o formato que o port JA usa e que os 81 charts do Drive ja
 *          estao (27 musicas x 3 dificuldades, 672 KB, 34.325 notas).
 *          Cabecalho de 40 bytes, payload com notas/bpms/secoes/eventos e
 *          checksum FNV-1a. Ler FCH3 aqui significa que NENHUM chart
 *          precisa ser reconvertido: o mesmo arquivo que o tutorial usa
 *          hoje serve para as 27 musicas.
 *   FNC2 - formato estendido deste modulo, para os eventos ricos do
 *          0.8.6 (ease, duracao, alvo, strumline) que o FCH3 nao carrega
 *          (ele guarda so tipo+valor por evento).
 * chart_load() detecta pelo magic e aceita os dois.
 */
#ifndef CHART_H
#define CHART_H

#ifdef __cplusplus
extern "C" {
#endif

#define CHART_MAX_NOTES      2048
#define CHART_MAX_EVENTS     1024
#define CHART_MAX_SECTIONS   512   /* FCH3 permite ate 512 secoes */
#define CHART_MAX_BPMS       64
#define CHART_EVENT_PARAMS   6
#define CHART_EVENT_STRS     2
#define CHART_STR_LEN        24
#define CHART_MAGIC          "FNC2"

/* Direcoes oficiais (NoteDirection.hx): LEFT 0, DOWN 1, UP 2, RIGHT 3 */
enum { NOTE_LEFT = 0, NOTE_DOWN, NOTE_UP, NOTE_RIGHT };

/* Tipos de nota */
enum { NOTE_TAP = 0, NOTE_HOLD, NOTE_MINE };

typedef struct {
    unsigned int time_ms;     /* tempo do comeco */
    unsigned int length_ms;   /* duracao (0 em nota simples) */
    unsigned char dir;        /* 0..3 */
    unsigned char kind;       /* NOTE_TAP / NOTE_HOLD / NOTE_MINE */
    unsigned char player;     /* 1 = jogador, 0 = oponente */
    unsigned char usado;      /* estado de consumo do cursor */
} chart_note_t;

enum {
    EV_FOCUS_CAMERA = 0,
    EV_ZOOM_CAMERA,
    EV_SCROLL_SPEED,
    EV_SET_CAMERA_BOP,
    EV_SET_HEALTH_ICON,
    EV_SET_TARGET_BOP_SPEED,
    EV_PLAY_ANIMATION,
    EV_SET_CHARACTER,
    EV_SET_STAGE,
    EV_TIPO_COUNT
};

typedef struct {
    unsigned int time_ms;
    unsigned short tipo;
    unsigned char  n_params;
    unsigned char  ease;      /* indice 0..12, ou 0xFF = nao se aplica */
    float          p[CHART_EVENT_PARAMS];
    char           s[CHART_EVENT_STRS][CHART_STR_LEN];
    unsigned char  flags;     /* bit0 absolute, bit1 force, bit2 loop */
    /* Do FCH3: o par cru (type, value) que o port ja usa. Preservado
       SEMPRE, mesmo quando o tipo nao e conhecido aqui -- e assim que
       nenhum evento se perde ao ler um chart antigo. */
    unsigned int   bruto_tipo;
    unsigned int   bruto_valor;
} chart_event_t;

/* Secao de BPM vinda do FCH3 (amostras, 48 kHz). */
typedef struct {
    unsigned int start_step;
    unsigned int start_sample;
    unsigned int samples_per_step;
    unsigned int bpm_milesimos; /* bpm * 1000, como o Butterscotch grava */
} chart_bpm_t;

typedef struct {
    double time_ms;
    double bpm;
} chart_section_t;

typedef struct {
    char     nome[32];
    int      dificuldade;      /* 0 easy, 1 normal, 2 hard */
    double   bpm_inicial;
    double   scroll_easy, scroll_normal, scroll_hard;
    int      time_sig_num, time_sig_den;
    unsigned int total_notes;  /* notas do jogador (contagem oficial) */
    unsigned int n_notes;
    unsigned int n_events;
    unsigned int n_sections;
    chart_note_t   notas[CHART_MAX_NOTES];
    chart_event_t  eventos[CHART_MAX_EVENTS];
    chart_section_t secoes[CHART_MAX_SECTIONS];
    chart_bpm_t     bpms[CHART_MAX_BPMS];
    unsigned int    n_bpms;
    unsigned int    fim_amostras;   /* song_end_sample do FCH3 */
    /* cursores de reproducao */
    unsigned int cursor_notas;
    unsigned int cursor_eventos;
    unsigned int duracao_ms;
} chart_t;

/* Carrega de FCH3 ou FNC2 (detecta pelo magic). Devolve 0 ou negativo:
 *  -1 ponteiro nulo, -2 tamanho curto, -3 magic errado, -4 versao,
 *  -5 contagem acima da capacidade, -6 buffer truncado, -7 tempo fora
 *  de ordem (o loader ordena, mas reporta para a telemetria),
 *  -8 checksum FNV-1a do FCH3 nao confere,
 *  -9 estrutura interna do FCH3 invalida (ordem, lane, duracao). */
int  chart_load(const void *buf, unsigned int len, chart_t *c);

/* Prepara para tocar: zera cursores. */
void chart_rewind(chart_t *c);

/* Entrega os eventos com time_ms <= agora que ainda nao foram entregues.
 * Escreve ate 'max' ponteiros em 'saida' (o chamador pode pedir 1 por
 * vez num laco). Devolve a quantidade entregue. Complexidade O(1)
 * amortizado por evento; o cursor nunca volta. */
int  chart_poll_events(chart_t *c, unsigned int agora_ms,
                       const chart_event_t **saida, int max);

/* Entrega as notas que devem nascer ate 'agora + antecipacao_ms'.
 * Mesma garantia: uma vez cada, em ordem. */
int  chart_poll_notes(chart_t *c, unsigned int agora_ms, unsigned int antecipacao_ms,
                      const chart_note_t **saida, int max);

/* Quantas notas do jogador faltam ser processadas. */
unsigned int chart_restantes(const chart_t *c);
/* Ultimo tempo relevante (fim da ultima nota ou do ultimo evento). */
unsigned int chart_fim_ms(const chart_t *c);

const char *chart_event_nome(int tipo);

/* Amostra (48 kHz) para milissegundos, como o port faz em todo lugar. */
unsigned int chart_amostra_para_ms(unsigned int amostra);
/* Milissegundos para amostra. */
unsigned int chart_ms_para_amostra(unsigned int ms);

#ifdef __cplusplus
}
#endif
#endif
