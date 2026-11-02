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
    unsigned short w, h;        /* canvas do quadro NO PACOTE       */
    float          fator;       /* tamanho de desenho = w/h * fator */
    unsigned short quadros;     /* quadros da animacao              */
    unsigned short fps;
    unsigned char  looped;
    float          x, y;        /* canto superior esquerdo na tela  */
    unsigned int   inicio_ms;   /* quando a folha aparece           */
    const char    *tex;         /* caminho do pacote CFZ4           */
    const char    *id;
} res_folha_dado_t;

static const res_folha_dado_t g_res_folha[RES_FOLHAS] = {
    { 452u,  60u, 1.2500f, 36u, 24u, 0u, -100.0f, -5.0f,  250u, "resultado/results.cfz4", "results" },
    { 180u, 168u, 1.2500f, 16u, 24u, 0u, -67.5f, 67.5f,  875u, "resultado/ratings.cfz4", "ratings" },
    { 285u,  58u, 1.2500f, 30u, 24u, 0u, -90.0f, 257.5f, 1500u, "resultado/score.cfz4", "score" },
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
    unsigned int   flash_ms;      /* `getFlashDelay()` */
    unsigned int   bf_ms;         /* `getBFDelay()` -- contador pequeno */
    res_img_t      vert, hor;
    float          vert_x, vert_y, vert_vel;
    unsigned int   vert_espera_ms;
    float          hor_x, hor_y0, hor_dy, hor_vel, hor_espaco;
    unsigned char  hor_n;
    unsigned int   flicker_ms, flicker_total_ms, flash_dura_ms;
} res_rank_t;

static const res_rank_t g_res_rank[RES_RANKS] = {
    { "LOSS", 7750u, 3958u,                             /* rank 0 */
      {  20u, 121u, "rank_loss_vert.rgba" }, { 245u,  26u, "rank_loss_hor.rgba" },
      618.0f, 50.0f, -40.0f, 1250u,
      160.0f, 30.0f, 33.75f, 3.50f, 5.0f, 12u,
      83u, 250u, 583u },
    { "GOOD", 4458u, 3958u,                             /* rank 1 */
      {  20u, 121u, "rank_good_vert.rgba" }, { 244u,  26u, "rank_good_hor.rgba" },
      618.0f, 50.0f, -40.0f, 1250u,
      160.0f, 30.0f, 33.75f, 3.50f, 5.0f, 12u,
      83u, 250u, 583u },
    { "GREAT", 4542u, 3958u,                             /* rank 2 */
      {  20u, 152u, "rank_great_vert.rgba" }, { 297u,  26u, "rank_great_hor.rgba" },
      618.0f, 50.0f, -40.0f, 1250u,
      160.0f, 30.0f, 33.75f, 3.50f, 5.0f, 12u,
      83u, 250u, 583u },
    { "EXCELLENT", 5083u, 4042u,                             /* rank 3 */
      {  20u, 278u, "rank_excellent_vert.rgba" }, { 524u,  26u, "rank_excellent_hor.rgba" },
      618.0f, 50.0f, -40.0f, 1250u,
      160.0f, 30.0f, 33.75f, 3.50f, 5.0f, 12u,
      83u, 250u, 583u },
    { "PERFECT", 5375u, 3958u,                             /* rank 4 */
      {  20u, 216u, "rank_perfect_vert.rgba" }, { 408u,  26u, "rank_perfect_hor.rgba" },
      618.0f, 50.0f, -40.0f, 1250u,
      160.0f, 30.0f, 33.75f, 3.50f, 5.0f, 12u,
      83u, 250u, 583u },
    { "PERFECT", 5375u, 3958u,                             /* rank 5 */
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
    unsigned char  bpp;             /* 16 = RGBA5551 (GS_PSM_16) */
    float          x, y, dx;        /* 640x360: 17,5 / 152,5 / 32,5 */
    unsigned char  slots, quadros_por_digito;
    unsigned char  digito_disabled, digito_gone;
    unsigned char  inicio_q, shuffle_q, tween_q, fps;
} res_score_t;

static const res_score_t g_res_score = {
    560u, 227u, "resultado/score_digitos16.rgba", 254240u, 16u,
    17.5f, 152.5f, 32.5f, 10u,
    5u, 10u, 11u,
    37u, 41u, 23u, 24u
};


/* A MAQUINA QUE CAI ATRAS DO PLACAR (`soundSystem`, ResultState.hx
 * L189-199): 65 quadros a 24 fps nao loopados -- congela no ultimo e
 * fica na tela --, em (-15,-180) do original, zIndex 1100. O pacote
 * esta' a `escala` do original e o desenho multiplica `w*fator`. */
#define RES_SS_QUADROS 65u
#define RES_SS_FPS     24u
#define RES_SS_TEX     "resultado/soundsystem.cfz4"
#define RES_SS_VRAM    186224u

typedef struct {
    unsigned short w, h;        /* canvas NO PACOTE               */
    float          fator;       /* desenho = w/h * fator          */
    float          x, y;        /* canto superior esquerdo        */
    unsigned int   inicio_ms;   /* new FlxTimer().start(8 / 24)   */
} res_ss_t;

static const res_ss_t g_res_ss = {
    206u, 226u, 2.1277f, -7.5f, -90.0f,  333u
};



/* O SELO DE RECORDE (`highscoreNew`, ResultState.hx L149 e L408-427):
 * 29 quadros a 24 fps NAO loopados; ao terminar, toca de novo a partir do
 * quadro 16 (`play("new", true, false, 16)`), que e' o que repete.
 * Em (44,557) do original, zIndex 1200. `x`/`y` ja' estao em 640x360.
 * Ele SO' aparece quando o resultado E' recorde -- e o recorde vem do
 * `engine/save.c`, nao de um palpite. */
#define RES_HS_QUADROS 29u
#define RES_HS_FPS     24u
#define RES_HS_LOOP    16u
#define RES_HS_BPP     16u
#define RES_HS_TEX     "resultado/highscoreNew.cfz4"
#define RES_HS_VRAM    42200u

typedef struct {
    unsigned short w, h;      /* canvas NO PACOTE               */
    float          fator;     /* desenho = w/h * fator          */
    float          x, y;      /* canto superior esquerdo        */
    /* inicio (getHighscoreDelay) NAO fica aqui: ele e' do RANK, e mora
     * na tabela do rank (`res_rr_highscore_ms`). */
} res_hs_t;

static const res_hs_t g_res_hs = {
    422u, 50u, 1.0000f, 22.0f, 278.5f
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


#define RES_CP_ATLASES 3u

/* O PERCENTUAL DE CONCLUSAO (ClearPercentCounter.hx L22-140 e
 * ResultState.hx L577-646). Tres atlas de digitos -- `Left` e `Right` sao
 * as duas inclinacoes do glifo grande, `Small` e' o do `%` da barra do
 * topo -- mais o texto "CLEAR" (com o `%` embaixo) e o `%` pequeno.
 * `fator` converte o tamanho guardado no tamanho DESENHADO (o original pela
 * metade): os glifos grandes ficaram a 0,4 do original para caber na VRAM,
 * e sao desenhados a 1,25x. */
typedef struct {
    unsigned char  digito;
    unsigned short ax, ay, aw, ah;
    short          ox, oy;
} res_cp_frame_t;

static const res_cp_frame_t g_res_cp_left[10] = {
    { 0u,    0u,    0u,  32u,  36u,    0,    0 },   /* digito 0 */
    { 1u,   98u,    0u,  25u,  35u,    3,    0 },   /* digito 1 */
    { 2u,  124u,    0u,  30u,  35u,    0,    0 },   /* digito 2 */
    { 3u,    0u,   37u,  31u,  35u,    2,    0 },   /* digito 3 */
    { 4u,   95u,   37u,  32u,  34u,    0,    1 },   /* digito 4 */
    { 5u,   33u,    0u,  32u,  36u,    1,    0 },   /* digito 5 */
    { 6u,   32u,   37u,  30u,  35u,    1,    1 },   /* digito 6 */
    { 7u,  128u,   37u,  30u,  34u,    2,    1 },   /* digito 7 */
    { 8u,   66u,    0u,  31u,  36u,    1,    0 },   /* digito 8 */
    { 9u,   63u,   37u,  31u,  35u,    2,    1 },   /* digito 9 */
};

static const res_cp_frame_t g_res_cp_right[10] = {
    { 0u,   64u,    0u,  35u,  35u,    0,    0 },   /* digito 0 */
    { 1u,  100u,    0u,  25u,  35u,    4,    0 },   /* digito 1 */
    { 2u,   33u,   37u,  30u,  34u,    2,    2 },   /* digito 2 */
    { 3u,   64u,   37u,  31u,  34u,    2,    1 },   /* digito 3 */
    { 4u,   96u,   37u,  31u,  34u,    2,    1 },   /* digito 4 */
    { 5u,  126u,    0u,  32u,  35u,    2,    0 },   /* digito 5 */
    { 6u,    0u,    0u,  30u,  36u,    1,    0 },   /* digito 6 */
    { 7u,  128u,   37u,  32u,  34u,    2,    2 },   /* digito 7 */
    { 8u,   31u,    0u,  32u,  36u,    2,    0 },   /* digito 8 */
    { 9u,    0u,   37u,  32u,  35u,    2,    0 },   /* digito 9 */
};

static const res_cp_frame_t g_res_cp_small[10] = {
    { 0u,    0u,    0u,  14u,  18u,    1,    0 },   /* digito 0 */
    { 1u,  108u,    0u,  10u,  17u,    6,    0 },   /* digito 1 */
    { 2u,   15u,    0u,  15u,  18u,    0,    0 },   /* digito 2 */
    { 3u,   31u,    0u,  15u,  18u,    0,    0 },   /* digito 3 */
    { 4u,  119u,    0u,  14u,  17u,    1,    0 },   /* digito 4 */
    { 5u,   47u,    0u,  15u,  18u,    0,    0 },   /* digito 5 */
    { 6u,  134u,    0u,  14u,  17u,    0,    2 },   /* digito 6 */
    { 7u,   63u,    0u,  14u,  18u,    2,    0 },   /* digito 7 */
    { 8u,   78u,    0u,  14u,  18u,    0,    0 },   /* digito 8 */
    { 9u,   93u,    0u,  14u,  18u,    2,    0 },   /* digito 9 */
};

typedef struct {
    unsigned short        w, h;
    const char           *arquivo;
    unsigned int          vram_bytes;
    float                 fator;
    const res_cp_frame_t *frames;
} res_cp_atlas_t;

static const res_cp_atlas_t g_res_cp_atlas[RES_CP_ATLASES] = {
    { 160u,  72u, "resultado/clear_left.rgba",  46080u, 1.25f, g_res_cp_left },   /* LEFT */
    { 160u,  72u, "resultado/clear_right.rgba",  46080u, 1.25f, g_res_cp_right },   /* RIGHT */
    { 160u,  18u, "resultado/clear_small.rgba",  11520u, 1.25f, g_res_cp_small },   /* SMALL */
};

typedef struct {
    unsigned short w, h;
    const char    *arquivo;
    unsigned int   vram_bytes;
    float          fator;
} res_cp_img_t;

static const res_cp_img_t g_res_cp_texto = { 100u,  63u, "resultado/clear_texto.rgba", 25200u, 1.25f };  /* o "CLEAR" com o `%` */
static const res_cp_img_t g_res_cp_texto_small = {  11u,  13u, "resultado/clear_texto_small.rgba",   572u, 1.25f };  /* o `%` pequeno */

typedef struct {
    float          x, y;             /* canto do grupo, em 640x360      */
    float          passo_grande, passo_pequeno;
    float          y_digitos_grande;
    float          small_x, small_y, small_x_pos, small_y_passo;
    float          small_fixo_x, small_y_alvo, small_texto_x;
    unsigned int   small_atraso_ms, small_tween_ms;  /* o tween de entrada */
    unsigned short inicio_q, tween_q;
    unsigned int   flash_ms, fade_espera_ms, fade_atraso_ms, fade_ms;
    unsigned char  abaixo;           /* `clearPercentLerp` = alvo - 36 */
} res_cp_geo_t;

static const res_cp_geo_t g_res_cp_geo = {
    415.0f, 145.0f, 36.0f, 16.0f, 36.0f,
    470.0f, 130.0f, -12.0f, -2.0f,
    307.5f, 58.5f, 20.0f,
    850u, 500u,   /* startDelay 0,85 s, 0,5 s expoOut */
    37u, 58u,
    400u, 250u, 500u, 500u, 36u
};


/* O NOME DA MUSICA (`FlxBitmapText` sobre `resultScreen/tardlingSpritesheet`,
 * ResultState.hx L119-122). A fonte e' monoespaçada: celula de 49x61, avanco de 34 px (49 - 15 de `letterSpacing`), angulo de -4.4 graus. O texto de cada musica vem PRONTO do empacotador (`tools/empacotar_nome.py`): e' um `BitmapData` por titulo, como o `updateTextBitmap` do original monta. A escala de cada um e' a maior que cabe em 33000 B de VRAM; ela vai no dado. */
#define RES_NOMES 27u

typedef struct {
    const char    *id;        /* o id da musica (SR song id)        */
    const char    *arquivo;
    unsigned short w, h;      /* o bitmap do titulo, em 640x360     */
    unsigned int   vram_bytes;
    float          escala;    /* de quanto ele foi reduzido         */
    float          y_alvo;    /* alvo do tween de entrada em y      */
    float          larg_full; /* `songName.width`: 34n+15 pela metade */
} res_nome_arte_t;

static const res_nome_arte_t g_res_nome[RES_NOMES] = {
    { "tutorial", "resultado/nome_tutorial.rgba", 144u, 30u,  17280u, 0.50f,48.0f,143.5f },
    { "bopeebo", "resultado/nome_bopeebo.rgba", 126u, 30u,  15120u, 0.50f,48.6f,126.5f },
    { "fresh", "resultado/nome_fresh.rgba",  92u, 30u,  11040u, 0.50f,50.0f,92.5f },
    { "dadbattle", "resultado/nome_dadbattle.rgba", 160u, 30u,  19200u, 0.50f,47.3f,160.5f },
    { "spookeez", "resultado/nome_spookeez.rgba", 144u, 30u,  17280u, 0.50f,48.0f,143.5f },
    { "south", "resultado/nome_south.rgba",  92u, 30u,  11040u, 0.50f,50.0f,92.5f },
    { "monster", "resultado/nome_monster.rgba", 126u, 30u,  15120u, 0.50f,48.6f,126.5f },
    { "pico", "resultado/nome_pico.rgba",  76u, 30u,   9120u, 0.50f,50.6f,75.5f },
    { "philly-nice", "resultado/nome_philly-nice.rgba", 194u, 30u,  23280u, 0.50f,46.0f,194.5f },
    { "blammed", "resultado/nome_blammed.rgba", 126u, 30u,  15120u, 0.50f,48.6f,126.5f },
    { "satin-panties", "resultado/nome_satin-panties.rgba", 228u, 30u,  27360u, 0.50f,44.7f,228.5f },
    { "high", "resultado/nome_high.rgba",  76u, 30u,   9120u, 0.50f,50.6f,75.5f },
    { "milf", "resultado/nome_milf.rgba", 126u, 30u,  15120u, 0.50f,48.6f,126.5f },
    { "cocoa", "resultado/nome_cocoa.rgba",  92u, 30u,  11040u, 0.50f,50.0f,92.5f },
    { "eggnog", "resultado/nome_eggnog.rgba", 110u, 30u,  13200u, 0.50f,49.3f,109.5f },
    { "winter-horrorland", "resultado/nome_winter-horrorland.rgba", 267u, 27u,  28836u, 0.45f,42.1f,296.5f },
    { "senpai", "resultado/nome_senpai.rgba", 110u, 30u,  13200u, 0.50f,49.3f,109.5f },
    { "roses", "resultado/nome_roses.rgba",  92u, 30u,  11040u, 0.50f,50.0f,92.5f },
    { "thorns", "resultado/nome_thorns.rgba", 110u, 30u,  13200u, 0.50f,49.3f,109.5f },
    { "ugh", "resultado/nome_ugh.rgba",  58u, 30u,   6960u, 0.50f,51.3f,58.5f },
    { "guns", "resultado/nome_guns.rgba",  76u, 30u,   9120u, 0.50f,50.6f,75.5f },
    { "stress", "resultado/nome_stress.rgba", 110u, 30u,  13200u, 0.50f,49.3f,109.5f },
    { "darnell", "resultado/nome_darnell.rgba", 126u, 30u,  15120u, 0.50f,48.6f,126.5f },
    { "lit-up", "resultado/nome_lit-up.rgba", 110u, 30u,  13200u, 0.50f,49.3f,109.5f },
    { "2hot", "resultado/nome_2hot.rgba",  76u, 30u,   9120u, 0.50f,50.6f,75.5f },
    { "blazin", "resultado/nome_blazin.rgba", 126u, 30u,  15120u, 0.50f,48.6f,126.5f },
    { "spaghetti", "resultado/nome_spaghetti.rgba", 423u, 17u,  28764u, 0.28f,24.5f,755.5f },
};

/* O movimento do nome (`timerThenSongName` e o `update` do ResultState): entrada em y com `expoOut` depois de 900 ms, `speedOfTween` voltando de 0 num tween de 700 ms com `quadIn` que comeca em 1000 ms (1o ciclo) ou 3000 ms (ciclos seguintes), movimento solto 2500 ms depois de o contador pequeno entrar, e o ciclo recomecando quando `x + largura < 50`. */
typedef struct {
    float        angulo, vel_x, vel_y;
    float        x_do_pequeno, clip_x, ciclo_fim_x;
    unsigned int entrada_atraso_ms, entrada_ms, velocidade_ms;
    unsigned int ciclo_espera_ms, ciclo_espera_primeiro_ms;
    unsigned int movimento_atraso_ms;
} res_nome_geo_t;

static const res_nome_geo_t g_res_nome_geo = {
    -4.40f, -59.82f, 4.60f,
    47.0f, 260.0f, 50.0f,
    900u, 500u, 700u, 3000u, 1000u, 2500u
};


#define RES_DIFS 5u

/* A BARRA PRETA DO TOPO. Nao ha' PNG: o original a monta em codigo
 * (`BitmapUtil.createResultsBar`) -- um retangulo preto de 1295x149 girado de -3.8 graus. A forma que sobra na tela e' "tudo acima da reta de baixo":
 * `borda_y0` em x = 0 e `borda_y1` em x = 640 (em 640x360). O jogo desenha
 * faixas pretas, sem textura. A barra entra deslizando de cima: tween de
 * `-altura` ate' 0 em 7/24 s, quartOut, com 3/24 s de atraso.
 * `correcao_y` e' `(blackTopBar.height - 148)`, o deslocamento que a fonte
 * aplica em tudo que fica abaixo da barra. */
typedef struct {
    float          borda_y0, borda_y1, altura, correcao_y;
    unsigned char  atraso_q, tween_q;
} res_barra_t;

static const res_barra_t g_res_barra = {
    74.2f, 31.7f, 74.5f, 0.5f, 3u, 7u
};

/* A IMAGEM DA DIFICULDADE (`resultScreen/diff_<id>`): x = 555 na fonte,
 * entrando de cima em 0,5 s com `expoOut` e 0,8 s de atraso. */
typedef struct {
    unsigned short w, h;
    const char    *arquivo;
    unsigned int   vram_bytes;
} res_dif_t;

static const res_dif_t g_res_dif[RES_DIFS] = {
    {  78u, 26u, "resultado/diff_easy.rgba",  8112u },   /* diff_easy */
    { 114u, 29u, "resultado/diff_normal.rgba", 13224u },   /* diff_normal */
    {  80u, 26u, "resultado/diff_hard.rgba",  8320u },   /* diff_hard */
    { 101u, 28u, "resultado/diff_erect.rgba", 11312u },   /* diff_erect */
    { 164u, 32u, "resultado/diff_nightmare.rgba", 20992u },   /* diff_nightmare */
};

typedef struct {
    float        x, y_alvo;
    unsigned int atraso_ms, tween_ms;
} res_dif_geo_t;

static const res_dif_geo_t g_res_dif_geo = { 277.5f, 61.5f, 800u, 500u };

/* A MUSICA DA TELA DE RESULTADO (ResultState.hx L502-550). O arquivo sai
 * de `getResultsMusicPath(rank)` do DONO do personagem jogavel
 * (`players/<dono>.json`); quando o dono tem `-intro`, o original toca o
 * intro UMA vez e depois o principal em loop (L506-524), e quando nao tem
 * (o `resultsNORMAL`, medido no clone) toca o principal direto (L536).
 * `atraso_ms` e' o `getMusicDelay()` de cada rank (Scoring.hx L530-547) em
 * milissegundos, arredondado para cima como todo `q/24` deste port. */
#define RES_MUSICA_DONOS 2u
#define RES_MUSICA_JOGADORES 8u
#define RES_MUSICA_DONO_PADRAO 0u   /* o `?? 'bf'` do original */

typedef struct {
    const char    *arquivo;   /* o PCM principal (loop)               */
    const char    *intro;     /* o `-intro`, ou NULL quando nao existe */
} res_musica_t;

static const res_musica_t g_res_musica[RES_MUSICA_DONOS][RES_RANKS] = {
    {   /* bf */
     { "res_shit", "res_shit_intro" },   /* SHIT */
     { "res_normal", 0 },   /* GOOD */
     { "res_normal", 0 },   /* GREAT */
     { "res_excellent", "res_excellent_intro" },   /* EXCELLENT */
     { "res_perfect", 0 },   /* PERFECT */
     { "res_perfect", 0 },   /* PERFECT_GOLD */
    },
    {   /* pico */
     { "res_shit_pico", 0 },   /* SHIT */
     { "res_normal_pico", 0 },   /* GOOD */
     { "res_normal_pico", 0 },   /* GREAT */
     { "res_excellent_pico", "res_excellent_pico_intro" },   /* EXCELLENT */
     { "res_perfect_pico", "res_perfect_pico_intro" },   /* PERFECT */
     { "res_perfect_pico", "res_perfect_pico_intro" },   /* PERFECT_GOLD */
    },
};

/* `getMusicDelay()` em ms (q/24 arredondado para cima). */
static const unsigned int g_res_musica_atraso_ms[RES_RANKS] = { 84u, 125u, 209u, 0u, 3959u, 3959u };

typedef struct { const char *id; unsigned char dono; } res_jogador_t;

/* O `player` de cada musica do projeto -> o DONO dos results. Quem nao
 * esta' na tabela cai no `RES_MUSICA_DONO_PADRAO` (o `?? 'bf'`). */
static const res_jogador_t g_res_jogadores[RES_MUSICA_JOGADORES] = {
    { "bf", 0u },
    { "bf-car", 0u },
    { "bf-christmas", 0u },
    { "bf-holding-gf", 0u },
    { "bf-pixel", 0u },
    { "pico-blazin", 1u },
    { "pico-playable", 1u },
    { "sserafim-sakura", 0u },
};

#endif /* RESULTADO_DADOS_H */
