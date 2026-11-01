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

#endif /* RESULTADO_DADOS_H */
