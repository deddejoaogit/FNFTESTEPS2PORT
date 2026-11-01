/* GERADO POR tools/gen_resultado_dados.py -- NAO EDITE.
 *
 * Fonte: assets-data/resultado-plan.json, escrito por
 * tools/empacotar_resultado.py depois de recortar as folhas de
 * shared/images/resultScreen. Posicoes, tempos e fps vem de
 * source/funkin/play/ResultState.hx (v0.8.6); as linhas estao no plano.
 */
#ifndef RESULTADO_DADOS_H
#define RESULTADO_DADOS_H

/* Escala: o original monta a tela em 1280x720, o port desenha em 640x360. */
#define RES_ESCALA (0.5f)
#define RES_FOLHAS 3u

/* O fundo: FlxGradient de 90 graus entre estas duas cores. */
#define RES_BG_W 16u
#define RES_BG_H 360u
#define RES_BG_TOPO_R 254u
#define RES_BG_TOPO_G 204u
#define RES_BG_TOPO_B 92u
#define RES_BG_BASE_R 253u
#define RES_BG_BASE_G 192u
#define RES_BG_BASE_B 92u

typedef struct {
    unsigned short w, h;        /* canvas da folha em 640x360       */
    unsigned short quadros;     /* quadros da animacao              */
    unsigned short fps;
    unsigned char  looped;
    float          x, y;        /* canto superior esquerdo na tela  */
    unsigned int   inicio_ms;   /* quando a folha aparece           */
    const char    *tex;         /* caminho do pacote CFZ4           */
    const char    *id;
} res_folha_dado_t;

static const res_folha_dado_t g_res_folha[RES_FOLHAS] = {
    { 565u,  74u, 36u, 24u, 0u, -100.0f, -5.0f,  250u, "resultado/results.cfz4", "results" },
    { 224u, 210u, 16u, 24u, 0u, -67.5f, 67.5f,  875u, "resultado/ratings.cfz4", "ratings" },
    { 356u,  72u, 30u, 24u, 0u, -90.0f, 257.5f, 1500u, "resultado/score.cfz4", "score" },
};

#define RES_RANKS 6u

/* A ARTE DO RANK (ResultState.hx L667-700, Scoring.hx L627-666).
 * `flash_ms` e' o `getFlashDelay()` de cada rank, contado da abertura da
 * tela; `*_x`/`*_y`/`*_vel` ja' estao em 640x360 (metade do original). */
typedef struct {
    unsigned short w, h;
    const char    *arquivo;
} res_img_t;

typedef struct {
    const char    *sufixo;      /* LOSS, GOOD, GREAT, EXCELLENT, PERFECT */
    unsigned int   flash_ms;
    res_img_t      vert, hor;
    float          vert_x, vert_y, vert_vel;
    unsigned int   vert_espera_ms;
    float          hor_x, hor_y0, hor_dy, hor_vel, hor_espaco;
    unsigned char  hor_n;
    unsigned int   flicker_ms, flicker_total_ms, flash_dura_ms;
} res_rank_t;

static const res_rank_t g_res_rank[RES_RANKS] = {
    { "LOSS", 7750u,                                          /* rank 0 */
      {  20u, 121u, "rank_loss_vert.rgba" }, { 245u,  26u, "rank_loss_hor.rgba" },
      618.0f, 50.0f, -40.0f, 1250u,
      160.0f, 30.0f, 33.75f, 3.50f, 5.0f, 12u,
      83u, 250u, 583u },
    { "GOOD", 4458u,                                          /* rank 1 */
      {  20u, 121u, "rank_good_vert.rgba" }, { 244u,  26u, "rank_good_hor.rgba" },
      618.0f, 50.0f, -40.0f, 1250u,
      160.0f, 30.0f, 33.75f, 3.50f, 5.0f, 12u,
      83u, 250u, 583u },
    { "GREAT", 4542u,                                          /* rank 2 */
      {  20u, 152u, "rank_great_vert.rgba" }, { 297u,  26u, "rank_great_hor.rgba" },
      618.0f, 50.0f, -40.0f, 1250u,
      160.0f, 30.0f, 33.75f, 3.50f, 5.0f, 12u,
      83u, 250u, 583u },
    { "EXCELLENT", 5083u,                                          /* rank 3 */
      {  20u, 278u, "rank_excellent_vert.rgba" }, { 524u,  26u, "rank_excellent_hor.rgba" },
      618.0f, 50.0f, -40.0f, 1250u,
      160.0f, 30.0f, 33.75f, 3.50f, 5.0f, 12u,
      83u, 250u, 583u },
    { "PERFECT", 5375u,                                          /* rank 4 */
      {  20u, 216u, "rank_perfect_vert.rgba" }, { 408u,  26u, "rank_perfect_hor.rgba" },
      618.0f, 50.0f, -40.0f, 1250u,
      160.0f, 30.0f, 33.75f, 3.50f, 5.0f, 12u,
      83u, 250u, 583u },
    { "PERFECT", 5375u,                                          /* rank 5 */
      {  20u, 216u, "rank_perfect_vert.rgba" }, { 408u,  26u, "rank_perfect_hor.rgba" },
      618.0f, 50.0f, -40.0f, 1250u,
      160.0f, 30.0f, 33.75f, 3.50f, 5.0f, 12u,
      83u, 250u, 583u },
};


#define RES_SCORE_QUADROS 52u

/* O PLACAR (ResultScore.hx): dez slots, cada um um ScoreNum que toca uma
 * das 12 animacoes de `resultScreen/score-digital-numbers` -- ZERO..NINE
 * com 5 quadros cada, mais DISABLED e GONE com um. Os 52 quadros moram
 * num UNICO atlas (os dez digitos aparecem ao mesmo tempo) e cada um tem
 * seu retangulo nele e o deslocamento com que o Sparrow o posiciona. */
typedef struct {
    unsigned char  digito, quadro;
    unsigned short ax, ay, aw, ah;  /* retangulo no atlas (metade)     */
    short          ox, oy;          /* -frameX/-frameY do XML (metade) */
} res_score_frame_t;

static const res_score_frame_t g_res_score_frame[RES_SCORE_QUADROS] = {
    {  0u, 0u,    0u,    0u,  47u,  62u,    0,    0 },  /* ZERO    0 */
    {  0u, 1u,   48u,    0u,  47u,  62u,    0,    0 },  /* ZERO    1 */
    {  0u, 2u,  432u,   63u,  35u,  50u,    6,    6 },  /* ZERO    2 */
    {  0u, 3u,  468u,   63u,  35u,  50u,    6,    6 },  /* ZERO    3 */
    {  0u, 4u,  237u,  177u,  32u,  47u,    8,    8 },  /* ZERO    4 */
    {  1u, 0u,   96u,    0u,  40u,  62u,    8,    0 },  /* ONE     0 */
    {  1u, 1u,  137u,    0u,  40u,  62u,    8,    0 },  /* ONE     1 */
    {  1u, 2u,  504u,   63u,  34u,  50u,    8,    6 },  /* ONE     2 */
    {  1u, 3u,    0u,  126u,  34u,  50u,    8,    6 },  /* ONE     3 */
    {  1u, 4u,  336u,  177u,  32u,  46u,    8,    8 },  /* ONE     4 */
    {  2u, 0u,  178u,    0u,  47u,  62u,    0,    0 },  /* TWO     0 */
    {  2u, 1u,  226u,    0u,  47u,  62u,    0,    0 },  /* TWO     1 */
    {  2u, 2u,   35u,  126u,  35u,  50u,    6,    6 },  /* TWO     2 */
    {  2u, 3u,   71u,  126u,  35u,  50u,    6,    6 },  /* TWO     3 */
    {  2u, 4u,   72u,  177u,  32u,  48u,    8,    8 },  /* TWO     4 */
    {  3u, 0u,  274u,    0u,  48u,  62u,    0,    0 },  /* THREE   0 */
    {  3u, 1u,  323u,    0u,  48u,  62u,    0,    0 },  /* THREE   1 */
    {  3u, 2u,  107u,  126u,  36u,  50u,    6,    6 },  /* THREE   2 */
    {  3u, 3u,  144u,  126u,  36u,  50u,    6,    6 },  /* THREE   3 */
    {  3u, 4u,  105u,  177u,  32u,  48u,    8,    8 },  /* THREE   4 */
    {  4u, 0u,  372u,    0u,  47u,  62u,    0,    0 },  /* FOUR    0 */
    {  4u, 1u,  420u,    0u,  47u,  62u,    0,    0 },  /* FOUR    1 */
    {  4u, 2u,  181u,  126u,  35u,  50u,    6,    6 },  /* FOUR    2 */
    {  4u, 3u,  217u,  126u,  35u,  50u,    6,    6 },  /* FOUR    3 */
    {  4u, 4u,  369u,  177u,  32u,  46u,    8,    8 },  /* FOUR    4 */
    {  5u, 0u,  468u,    0u,  47u,  62u,    0,    0 },  /* FIVE    0 */
    {  5u, 1u,    0u,   63u,  47u,  62u,    0,    0 },  /* FIVE    1 */
    {  5u, 2u,  253u,  126u,  35u,  50u,    6,    6 },  /* FIVE    2 */
    {  5u, 3u,  289u,  126u,  35u,  50u,    6,    6 },  /* FIVE    3 */
    {  5u, 4u,  138u,  177u,  32u,  48u,    8,    8 },  /* FIVE    4 */
    {  6u, 0u,   48u,   63u,  47u,  62u,    0,    0 },  /* SIX     0 */
    {  6u, 1u,   96u,   63u,  47u,  62u,    0,    0 },  /* SIX     1 */
    {  6u, 2u,  325u,  126u,  35u,  50u,    6,    6 },  /* SIX     2 */
    {  6u, 3u,  361u,  126u,  35u,  50u,    6,    6 },  /* SIX     3 */
    {  6u, 4u,  171u,  177u,  32u,  48u,    8,    8 },  /* SIX     4 */
    {  7u, 0u,  144u,   63u,  47u,  62u,    0,    0 },  /* SEVEN   0 */
    {  7u, 1u,  192u,   63u,  47u,  62u,    0,    0 },  /* SEVEN   1 */
    {  7u, 2u,  397u,  126u,  35u,  50u,    6,    6 },  /* SEVEN   2 */
    {  7u, 3u,  433u,  126u,  35u,  50u,    6,    6 },  /* SEVEN   3 */
    {  7u, 4u,  270u,  177u,  32u,  47u,    8,    8 },  /* SEVEN   4 */
    {  8u, 0u,  240u,   63u,  47u,  62u,    0,    0 },  /* EIGHT   0 */
    {  8u, 1u,  288u,   63u,  47u,  62u,    0,    0 },  /* EIGHT   1 */
    {  8u, 2u,  469u,  126u,  35u,  50u,    6,    6 },  /* EIGHT   2 */
    {  8u, 3u,  505u,  126u,  35u,  50u,    6,    6 },  /* EIGHT   3 */
    {  8u, 4u,  204u,  177u,  32u,  48u,    8,    8 },  /* EIGHT   4 */
    {  9u, 0u,  336u,   63u,  47u,  62u,    0,    0 },  /* NINE    0 */
    {  9u, 1u,  384u,   63u,  47u,  62u,    0,    0 },  /* NINE    1 */
    {  9u, 2u,    0u,  177u,  35u,  50u,    6,    6 },  /* NINE    2 */
    {  9u, 3u,   36u,  177u,  35u,  50u,    6,    6 },  /* NINE    3 */
    {  9u, 4u,  303u,  177u,  32u,  47u,    8,    8 },  /* NINE    4 */
    { 10u, 0u,  402u,  177u,  30u,  44u,    8,    8 },  /* DISABLED 0 */
    { 11u, 0u,  433u,  177u,  30u,  44u,    8,    8 },  /* GONE    0 */
};

/* O atlas e a geometria da tela. `inicio_q`, `shuffle_q` e `tween_q` sao
 * os timers do original em 24 avos de segundo (o mesmo relogio das
 * animacoes), contados da abertura da tela. */
typedef struct {
    unsigned short w, h;
    const char    *arquivo;
    unsigned int   vram_bytes;
    float          x, y, dx;        /* 640x360: 17,5 / 152,5 / 32,5 */
    unsigned char  slots, quadros_por_digito;
    unsigned char  digito_disabled, digito_gone;
    unsigned char  inicio_q, shuffle_q, tween_q, fps;
} res_score_t;

static const res_score_t g_res_score = {
    560u, 227u, "resultado/score_digitos.rgba", 508480u,
    17.5f, 152.5f, 32.5f, 10u,
    5u, 10u, 11u,
    37u, 41u, 23u, 24u
};


#define RES_TALLIES 7u
#define RES_TALLY_FRAMES 10u

/* Os CONTADORES DE JULGAMENTO (TallyCounter.hx L14-90 e ResultState.hx
 * L418-490): dez glifos do atlas `resultScreen/tallieNumber`, um por
 * digito, e os sete contadores -- posicao, cor e qual numero cada um
 * mostra. `campo` e' o indice do valor: 0 totalNotesHit, 1 maxCombo,
 * 2 sick, 3 good, 4 bad, 5 shit, 6 missed (a ordem do `ratingGrp`). */
typedef struct {
    unsigned char  digito;
    unsigned short ax, ay, aw, ah;
    short          ox, oy;
} res_tally_frame_t;

static const res_tally_frame_t g_res_tally_frame[RES_TALLY_FRAMES] = {
    {  0u,   70u,   23u,  17u,  18u,    0,    0 },  /* digito 0 */
    {  1u,    0u,   23u,  16u,  21u,    0,    0 },  /* digito 1 */
    {  2u,   17u,   23u,  18u,  21u,    0,    0 },  /* digito 2 */
    {  3u,    0u,    0u,  17u,  22u,    0,    0 },  /* digito 3 */
    {  4u,   36u,   23u,  16u,  20u,    0,    0 },  /* digito 4 */
    {  5u,   18u,    0u,  18u,  22u,    0,    0 },  /* digito 5 */
    {  6u,   37u,    0u,  17u,  22u,    0,    0 },  /* digito 6 */
    {  7u,   55u,    0u,  17u,  22u,    0,    0 },  /* digito 7 */
    {  8u,   53u,   23u,  16u,  20u,    0,    0 },  /* digito 8 */
    {  9u,   73u,    0u,  16u,  22u,    0,    0 },  /* digito 9 */
};

typedef struct {
    unsigned short w, h;
    const char    *arquivo;
    unsigned int   vram_bytes;
    float          dx;          /* 43 px por digito, em 1280 -> 21,5 */
    unsigned int   atraso_base_ms, atraso_passo_ms, tween_ms;
    unsigned char  ajuste_1000; /* 30 px para a esquerda com 1000+ notas */
} res_tallies_t;

static const res_tallies_t g_res_tallies = {
     96u,  44u, "resultado/tallies.rgba", 16896u, 21.5f,
    1200u, 300u, 500u, 30u
};

typedef struct {
    unsigned char  campo;       /* qual numero do jogo (ver acima) */
    float          x, y;
    unsigned char  r, g, b;     /* a cor do `flavour` do TallyCounter */
} res_tally_t;

static const res_tally_t g_res_tally[RES_TALLIES] = {
    { 0u,  187.5f,   75.0f, 0xFF, 0xFF, 0xFF },  /* total_hit */
    { 1u,  187.5f,  100.0f, 0xFF, 0xFF, 0xFF },  /* max_combo */
    { 2u,  115.0f,  138.5f, 0x89, 0xE5, 0x9E },  /* sick */
    { 3u,  105.0f,  165.5f, 0x89, 0xC9, 0xE5 },  /* good */
    { 4u,   95.0f,  192.5f, 0xE6, 0xCF, 0x8A },  /* bad */
    { 5u,  110.0f,  219.5f, 0xE6, 0x8C, 0x8A },  /* shit */
    { 6u,  130.0f,  246.5f, 0xC6, 0x8A, 0xE6 },  /* missed */
};

#endif /* RESULTADO_DADOS_H */
