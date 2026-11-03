/* ss_cutscene - a CUTSCENE do `sserafim` em C puro: o pacote assado e a linha
 * do tempo do script.
 *
 * POR QUE ISTO EXISTE
 * `sserafim.hxc` L567-930 (`introCutscene`) e' uma sequencia de ~20 FlxTimers
 * que mexe a camera, dois fades, a cobertura preta, a visibilidade de props e
 * tres sprites de atlas Animate (`cutscene/cutsceneMain`, `cutscene/gfGetUp`,
 * `cutscene/bfGetUp`). O port nao tem interpretador de HScript: o que este
 * modulo entrega e' (a) o pacote de QUADROS ASSADOS de cada atlas e (b) a linha
 * do tempo transcrita, com o instante e a fonte de cada evento.
 *
 * O QUE E' "QUADRO ASSADO"
 * O `Animation.json` do `cutsceneMain` tem 27 camadas, 42 simbolos e simbolos
 * ANINHADOS com matriz propria -- rasterizar 631 quadros daria mais de 1 GB.
 * Entao o que assa e' a GEOMETRIA: por quadro, a lista de (sprite do
 * spritemap, matriz 2x3). O desenho vira o que qualquer sprite do port ja' faz
 * (quad + recorte + zoom), e a cena inteira cabe em ~240 KB de dados. Formato e
 * gerador: `tools/assar_cutscene_sserafim.py` (que confere a matriz contra o
 * rasterizador do `achatar_animate` -- IoU 0,999).
 *
 * O que NAO esta' aqui, de proposito:
 *  - o color grade do `SserafimShader` (brilho/matiz/contraste/saturacao):
 *    e' por pixel, o GS de pipeline fixa nao tem -- mesma falta declarada do
 *    `wiggle.frag`;
 *  - o `FlxText` do "Skip": o port desenha texto com a fonte dele.
 */
#ifndef FNF_SS_CUTSCENE_H
#define FNF_SS_CUTSCENE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Quantos quads um quadro pode ter. Medido nos tres atlas: 26,83 por quadro no
   `cutsceneMain` (o pico e' 30), 9,02 no `gfGetUp` e 6,36 no `bfGetUp`. 64 da'
   o dobro da folga; o excesso e' CONTADO, nunca desenhado pela metade. */
#define SSC_MAX_QUADS 64

/* Um quad assado: qual retangulo do spritemap e a matriz 2x3.
   A matriz vem em 1/4096 (coeficientes) e 1/4 (deslocamento), como no arquivo:
   e' o que cabe em 16 bits sem perder o que o original usa (as escalas medidas
   nos tres atlas sao 1,0 ou 0,984). */
typedef struct {
    unsigned short sprite;   /* indice na tabela de retangulos */
    short a, b, c, d;        /* matriz, em 1/4096               */
    short tx, ty;            /* deslocamento, em 1/4 de px       */
} ssc_quad_t;

/* O pacote assado: o arquivo `.quads` inteiro, em memoria (256 KB no maior). */
typedef struct {
    int n_sprites;                 /* retangulos no spritemap          */
    int n_frames;                  /* quadros da animacao              */
    int fps100;                    /* 24 fps -> 2400                   */
    int escala1000;                /* escala do atlas (1/1000)         */
    int tex_w, tex_h;              /* tamanho da textura no pacote CFZ4 */
    int n_labels;                  /* rotulos de quadro (ver abaixo)    */
    const unsigned char *rects;    /* n_sprites x 8 B: u16 x,y,w,h     */
    const unsigned char *labels;   /* n_labels x 12 B                  */
    const unsigned char *offsets;  /* n_frames x 4 B: u32 inicio       */
    const unsigned char *quads;    /* os quads, 14 B cada              */
    unsigned int quads_bytes;
} ssc_pack_t;

/* Quantos rotulos o pacote pode ter. O export real traz DOIS nos `GetUp`
   (`static` no quadro 0 e `getup` nos 123 seguintes, lidos da camada de
   anotacao do Animate) e nenhum no `cutsceneMain`, que toca a linha toda
   (`animation.play("", true)`). 8 e' teto de sanidade. */
#define SSC_MAX_LABELS 8
#define SSC_LABEL_BYTES 12     /* u16 inicio | u16 quadros | char nome[8] */

#define SSC_OK 0
#define SSC_ERRO_MAGIC -1
#define SSC_ERRO_TRUNCADO -2
#define SSC_ERRO_VAZIO -3

/* Le o cabecalho e valida os limites (nada de ler fora do buffer). */
int ssc_abrir(ssc_pack_t *p, const void *dados, unsigned int len);

/* Quadro da animacao no instante `t` (segundos desde o inicio dela).
   A animacao NAO repete: passado o ultimo quadro, ele fica. A conta e'
   `min(int(t*fps), n_frames-1)` -- ver o comentario em `ssc_quadro_em` para a
   medida que decidiu isso (631 quadros num sprite que so' e' usado ate' o
   quadro 563, e um `getup` de 123 quadros que atravessa o comeco da musica). */
int ssc_quadro_em(const ssc_pack_t *p, float t);

/* Acha um ROTULO de quadro ("static", "getup"). Devolve 0 e escreve o primeiro
   quadro e a contagem, ou negativo quando o rotulo nao existe.
   Os rotulos sao as etiquetas de quadro do Animate (a camada `Layer_15` do
   `gfGetUp` tem `static` no quadro 0 e `getup` nos 123 seguintes), que e' como
   o `SserafimGfSprite.resetAnim()/doAnim()` escolhem a animacao. */
int ssc_rotulo(const ssc_pack_t *p, const char *nome, int *primeiro, int *quadros);

/* Quadro do rotulo `nome` no instante `t`, em segundos desde o comeco DELE.
   Sem o rotulo devolve -1 (nada para desenhar: dado faltando, nunca o quadro
   errado). O rotulo de um quadro so' (o `static`) devolve sempre o mesmo. */
int ssc_quadro_rotulo(const ssc_pack_t *p, const char *nome, float t);

/* Os quads do quadro `f`. Devolve quantos escreveu (no maximo `max`), e o
   numero TOTAL em `*total` quando ele nao e' NULL -- e' assim que o chamador
   descobre que faltou espaco em vez de desenhar meia cena calado. */
int ssc_quads(const ssc_pack_t *p, int f, ssc_quad_t *out, int max, int *total);

/* Geometria de um quad, em unidades do DOCUMENTO (o espaco 1280x720 do
   original), com o canto do sprite em (x,y):
     canto = (x,y) + M * (lx,ly),  (lx,ly) em [0,w] x [0,h] do retangulo
   `px`/`py` recebem os quatro cantos na ordem
   superior-esquerdo, superior-direito, inferior-direito, inferior-esquerdo.
   Devolve 0 quando o indice do sprite nao existe. */
int ssc_cantos(const ssc_pack_t *p, const ssc_quad_t *q, float x, float y,
               float *px, float *py);

/* ---------------------------------------------------------- a linha do tempo
 *
 * Os instantes sao os do script, em SEGUNDOS de cutscene (`n/24` no original):
 *
 *   L655-676  t=0        zoom 0,5 e camera (660,-200) instantaneos; fade PRETO
 *                        de 3 s saindo de 1; `camHUD.visible = false`;
 *                        `setCutsceneVisibility(true)` (a cena da cutscene no
 *                        lugar das garotas e do caminhao); `doAnim` no atlas
 *                        principal.
 *   L677-678  t=0..3     zoom -> 0,7 e camera -> (660,300), `circOut`.
 *   L795-806  t=650/24   zoom volta a 0,7 instantaneo e vai a 0,55 em 3 s
 *                        (`circOut`); camera salta para (1070,470);
 *                        `solidCover.alpha` -> 0 em 3 s (`sineOut`); as duas
 *                        "get up" reaparecem com `resetAnim`.
 *   L812-816  t=710/24   `doAnim` nas duas "get up" (a animacao comeca aqui).
 *   L819-830  t=730/24   `startCountdown()`: a cutscene ACABA e a contagem
 *                        comeca.
 *   L725-742  t=563/24   flash BRANCO de 30/24 s saindo de 1; `solidCover`
 *                        a 1; `setCutsceneVisibility(false)`.
 *
 * O `ssc_estado` devolve tudo isso de uma vez, no instante pedido. */
typedef struct {
    float zoom;            /* `camGame.zoom`                              */
    float cam_x, cam_y;    /* `camGame.scroll`                            */
    float fade_preto;      /* alpha do fade preto da entrada (1 -> 0)     */
    float fade_branco;     /* alpha do flash branco de 563/24 (1 -> 0)    */
    float cover;           /* `solidCover.alpha`                          */
    int cena;              /* 1 = a cena da CUTSCENE no lugar do palco    */
    int hud;               /* `camHUD.visible`                            */
    int principal_visivel; /* o atlas `cutsceneMain`                      */
    int gf_visivel;        /* `sserafimGf` (a "get up" da namorada)       */
    int bf_visivel;        /* `sserafimBf`                                */
    /* O instante DENTRO da animacao corrente de cada sprite, em segundos.
       O quadro sai dele e do pacote (`ssc_quadro_em`/`ssc_quadro_rotulo`),
       porque so' quem tem o pacote sabe a contagem -- e assim o teste da linha
       do tempo nao precisa de arte. */
    float t_principal;
    float t_gf;            /* ja' com o `resetAnim`/`doAnim` resolvido      */
    float t_bf;
    int anim_gf;           /* 0 = `static` (resetAnim), 1 = `getup` (doAnim) */
    int anim_bf;
    int acabou;            /* 1 = `startCountdown()` ja' foi chamado      */
} ssc_estado_t;

/* Os marcos, para quem quiser comparar com a fonte sem repetir numero. */
#define SSC_T_CAM_FIM      3.0f
#define SSC_T_FLASH        (563.0f / 24.0f)
#define SSC_T_FLASH_DUR    (30.0f / 24.0f)
#define SSC_T_CENA_FIM     (563.0f / 24.0f)
#define SSC_T_SAIDA        (650.0f / 24.0f)
#define SSC_T_SAIDA_DUR    3.0f
/* `resetAnim()` (`animation.play("static")`) em 650/24 e `doAnim()`
   (`animation.play("getup")`) em 710/24 -- ver `SserafimGfSprite.hxc`. */
#define SSC_T_STATIC       (650.0f / 24.0f)
#define SSC_T_GETUP_ANIM   (710.0f / 24.0f)
#define SSC_T_CONTA        (730.0f / 24.0f)

/* Avalia a linha do tempo em `t` segundos. */
void ssc_estado(float t, ssc_estado_t *e);

/* ---------------------------------------------------------------- as curvas */
/* `FlxEase` com a formula da fonte (a mesma de engine/stage_script.h):
   circOut(t) = sqrt(1 - (t-1)^2)   (FlxEase.hx L119-121)
   sineOut(t) = sin(PI/2 * t)       (L137-145)
   O port nao tem `sqrt` no caminho do render (nao usa libm): a raiz sai de
   Newton com 4 iteracoes, que num t em [0,1] erra menos de 1e-6 -- medido no
   teste. */
float ssc_circ_out(float t);
float ssc_sine_out(float t);

#ifdef __cplusplus
}
#endif

#endif /* FNF_SS_CUTSCENE_H */
