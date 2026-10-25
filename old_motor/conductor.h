/* conductor.h - relogio musical do jogo, derivado do Conductor.hx oficial
 *
 * TUDO AQUI VEM DO FONTE OFICIAL v0.8.6, NAO DE APROXIMACAO:
 *
 *   Constants.hx:424        STEPS_PER_BEAT = 4
 *   Constants.hx:241        DEFAULT_BOP_INTENSITY = 1.015
 *   Constants.hx:246        DEFAULT_ZOOM_RATE = 4        (batidas)
 *   Constants.hx:251        DEFAULT_ZOOM_OFFSET = 0
 *   Constants.hx:696        DEFAULT_CAMERA_FOLLOW_RATE = 0.04
 *   Conductor.hx:178-181    beatLengthMs = (60000/bpm) * (4/timeSigDenominator)
 *   Conductor.hx:189-191    stepLengthMs = beatLengthMs / STEPS_PER_BEAT
 *   Conductor.hx:470-472    currentStepTime = beatTime*4 + (songPos - timestamp)/stepLengthMs
 *   Conductor.hx:318        stepsPerMeasure = timeSigNum * STEPS_PER_BEAT
 *   PlayState.hx:1217-1227  bop: decay 0.95 por quadro, lerp(1, bop, 0.95^dt)
 *   PlayState.hx:1846-1859  disparo do bop: (step + offset*4) % (rate*4) == 0
 *
 * O relogio do console tem uma diferenca essencial: a posicao NAO vem de
 * FlxG.sound.music.time, vem das AMOSTRAS JA OUVIDAS pelo audsrv. Isso e
 * o que mantem imagem e som juntos quando o pendrive engasga.
 */
#ifndef CONDUCTOR_H
#define CONDUCTOR_H

#ifdef __cplusplus
extern "C" {
#endif

#define COND_SAMPLE_RATE      48000
#define COND_STEPS_PER_BEAT   4
#define COND_MAX_SECTIONS     64
#define COND_MAX_BPM_CHANGES  256

/* As 13 curvas reais do jogo, na ordem dos indices gravados nos charts.
   Nao existem cubic nem sine neste jogo. */
enum {
    EASE_CLASSIC = 0,   /* FlxEase.smoothStep  t*t*(3-2t)        */
    EASE_INSTANT,       /* FlxEase.instant     salto direto      */
    EASE_CIRC,          /* FlxEase.circOut                       */
    EASE_ELASTIC,       /* FlxEase.elasticOut                    */
    EASE_ELASTIC_INOUT, /* FlxEase.elasticInOut                  */
    EASE_EXPO,          /* FlxEase.expoOut                       */
    EASE_EXPO_IN,       /* FlxEase.expoIn                        */
    EASE_EXPO_OUT,      /* FlxEase.expoOut (nome explicito)      */
    EASE_LINEAR,
    EASE_QUAD,          /* FlxEase.quadOut                       */
    EASE_QUAD_INOUT,    /* FlxEase.quadInOut                     */
    EASE_QUART,         /* FlxEase.quartOut                      */
    EASE_QUINT_INOUT,   /* FlxEase.quintInOut                    */
    EASE_COUNT
};

typedef struct {
    double time_ms;    /* inicio da secao no tempo da musica */
    double bpm;
    double beat_time;  /* beatTime acumulado da secao (Conductor.hx:470) */
} cond_section_t;

typedef struct {
    double song_ms;          /* posicao da musica (calculada das amostras ouvidas) */
    double bpm;
    double step_length_ms;
    double beat_length_ms;
    int    time_sig_num;
    int    time_sig_den;
    double current_step_time;
    double current_beat_time;
    int    current_step;
    int    current_beat;
    double section_beat_time;
    cond_section_t sec[COND_MAX_SECTIONS];
    int    n_sec;
    int    sec_index;
    /* bop de camera */
    float  bop_mult;
    float  bop_intensity;
    int    bop_rate;
    int    bop_offset;
    int    last_bop_step;
    /* estatisticas para a telemetria */
    unsigned int frames;
} conductor_t;

void   cond_init(conductor_t *c, double bpm, int time_sig_num, int time_sig_den);
/* Registra uma secao de BPM (BPMChange / secao de chart). Aceita fora de
   ordem: o vetor e mantido ordenado por tempo. */
int    cond_add_section(conductor_t *c, double time_ms, double bpm);
/* Atualiza com a posicao em AMOSTRAS JA OUVIDAS (audsrv). */
void   cond_update_samples(conductor_t *c, unsigned int heard_samples);
/* Atualiza com a posicao em milissegundos (usado nos testes e no PC). */
void   cond_update_ms(conductor_t *c, double song_ms);
/* Chamar uma vez por quadro desenhado, com o dt em segundos. Aplica o
   decaimento do bop exatamente como FlxMath.lerp(1, bop, 0.95^dt). */
void   cond_frame(conductor_t *c, float dt_s);
/* Zoom final da camera = currentCameraZoom * bop_mult (PlayState.hx:1227). */
float  cond_zoom(const conductor_t *c, float current_camera_zoom);
/* Escala de camera com seguir de alvo, FlxCamera.updateFollow:
   adjustedLerp = 1-(1-lerp)^(elapsed*60) (main.c: TUT_CAM_LERP). */
float  cond_follow_lerp(float base_lerp, float dt_s);

/* Curvas. t e grampeado em [0,1] e NaN vira 0. */
float  fnf_ease(int indice, float t);
/* Nome do evento -> indice. Devolve -1 se desconhecido. Cobre os nomes
   usados pelos charts nesta versao do jogo e os nomes do FlxEase. */
int    fnf_ease_from_name(const char *nome);
const char *fnf_ease_name(int indice);

/* Interpolacao de um evento com duracao: valor de partida, alvo, t 0..1. */
float  fnf_ease_apply(int indice, float de, float para, float t);

#ifdef __cplusplus
}
#endif
#endif
