#ifndef RESULTADO_H
#define RESULTADO_H

/* ARTE DA TELA DE RESULTADO: estado, tempo e leitura de quadro.
 *
 * O QUE O ORIGINAL FAZ (ResultState.hx, v0.8.6)
 *   Cada folha e' um FunkinSprite escondido no comeco. Um FlxTimer a torna
 *   visivel e toca a animacao, que e' `false` em `looped` -- ou seja, toca uma
 *   vez e para no ULTIMO quadro:
 *
 *       resultsAnim  6/24 s   "results instance 1"   (36 quadros)
 *       ratingsPopin 21/24 s  "Categories"           (16 quadros)
 *       scorePopin   36/24 s  "tally score"          (30 quadros)
 *
 *   Todas a 24 fps. Como o flixel para no ultimo quadro em vez de sumir, a
 *   folha continua na tela -- e' isso que monta a cena final da tela.
 *
 * O QUE ESTE MODULO GUARDA
 *   O tempo (quando cada folha aparece e em que quadro ela esta') e o pacote
 *   aberto, para o main.c pegar UM quadro por vez. Ele NAO conhece GS, VRAM nem
 *   o desenho: recebe ms e devolve numero de quadro. Por isso roda inteiro no
 *   PC (test/test_resultado.c), com pacotes CFZ4 de verdade.
 */

#include "pack_stream.h"
#include "resultado_dados.h"

/* Pacote de UMA folha. Um personagem pode ter mais de um arquivo (`part%d`),
 * mas aqui cada folha cabe num so': o maior pacote e' 699 KB e o limite do
 * leitor e' 8 MB. Se um dia nao couber, `res_carregar` devolve erro em vez de
 * abrir um pedaco e fingir que abriu tudo. */
typedef struct {
    pst_pack_t pst;
    int aberto;          /* 1 = pacote aberto          */
    int tentou;          /* 1 = ja' tentou abrir        */
    unsigned int vistos; /* quadros pedidos (telemetria) */
} res_folha_t;

typedef struct {
    res_folha_t f[RES_FOLHAS];
} res_t;

/* Zera o estado (nenhum pacote aberto). */
void res_init(res_t *r);

/* Abre os pacotes de todas as folhas a partir de `dir`
 * (ex.: "mass:/FNFPS2/assets/"). Devolve o numero de folhas abertas; o
 * chamador compara com RES_FOLHAS e reporta o que faltou. */
int res_carregar(res_t *r, const char *dir);

/* Fecha os pacotes. Seguro chamar duas vezes. */
void res_liberar(res_t *r);

/* Quadro da folha `i` no instante `ms` (desde o inicio da tela), ou -1 se a
 * folha ainda nao apareceu / nao esta' carregada. */
int res_quadro(res_t *r, int i, unsigned long ms);

/* Copia o quadro `q` da folha `i` para `dst` (frame_bytes bytes).
 * Devolve 0 ou PST_ERR_*. */
int res_pixels(res_t *r, int i, int q, unsigned char *dst);

/* ---- A ARTE DO RANK (ResultState.hx L667-700 + Scoring.hx L627-666) --------
 *
 * Duas imagens do rank conquistado: uma VERTICAL (FlxBackdrop em x=1236,
 * y=100, rolando para cima) e uma HORIZONTAL (12 FlxBackdoor em
 * y = 60 + 67,5*i, metade para cada lado). O rank entra com um flash branco
 * da tela e um piscar de 0,25 s.
 *
 * Tudo aqui e' funcao PURA do tempo: sem arquivo, sem GS. O main.c so' le os
 * dois .rgba e desenha o que estas funcoes mandam.
 */

/* Indice do rank na tabela (0 = SHIT ... 5 = PERFECT_GOLD). Rank invalido cai
 * em SHIT, como `Scoring.calculateRank(...) ?? SHIT` (ResultState.hx L117). */
int res_rank_indice(int rank);

/* Tabela do rank `rank` (nunca nula). */
const res_rank_t *res_rank(int rank);

/* Alpha do FLASH da tela (0..255): 255 no instante do flash, caindo linear ate
 * 0 em `flash_dura_ms` (FlxTween.tween(bgFlash, {alpha: 0}, 14/24)). */
int res_flash_alpha(const res_rank_t *r, unsigned long ms);

/* O piscar de entrada: FlxFlicker.flicker(rankTextVert, 2/24*3, 2/24, true).
 * 1 = desenha, 0 = apagado (durante `flicker_total_ms`, alternando a cada
 * `flicker_ms`). Fora da janela do piscar, sempre 1. */
int res_rank_pisca(const res_rank_t *r, unsigned long ms);

/* Deslocamento (px, negativo para cima) do texto vertical. Zero ate'
 * `flash_ms + vert_espera_ms`; depois cresce com `vert_vel`. */
float res_rank_scroll_y(const res_rank_t *r, unsigned long ms);

/* Deslocamento (px, positivo para a direita) da linha `i` do texto
 * horizontal. As pares vao para a esquerda, as impares para a direita
 * (`(i % 2 == 0) ? -7.0 : 7.0`, L694-698). */
float res_rank_scroll_x(const res_rank_t *r, int linha, unsigned long ms);

/* ---- O SELO DE RECORDE (`highscoreNew`) ----------------------------------
 *
 * `ResultState.hx` L408-427: 29 quadros a 24 fps NAO loopados; ao terminar, toca
 * de novo A PARTIR DO QUADRO 16 e, como o `looped` do anim e' false, isso se
 * repete. O instante de entrada e' o `rank.getHighscoreDelay()`, e ele so'
 * aparece quando o resultado E' recorde -- o que o `engine/save.c` decide.
 * Funcao pura do tempo: nao le' arquivo nem VRAM.
 */
int res_hs_quadro(unsigned long ms, unsigned int inicio_ms);

/* ---- O PLACAR (ResultScore.hx L17-75 e L108-180) --------------------------
 *
 * `ResultState.create()` L151: `new ResultScore(35, 305, 10, score)` -- dez
 * `ScoreNum`, em x = 35 + 65*i, cada um com as 12 animacoes de
 * `resultScreen/score-digital-numbers` (ZERO..NINE com 5 quadros, DISABLED e
 * GONE com um). Tudo aqui e' funcao PURA do tempo: sem arquivo, sem GS.
 */

/* Quantos digitos o placar tem (`scoreStart` do `set_scoreShit`): o numero de
 * digitos decimais de `score`, que a fonte limita a 10 (`Math.pow(10, slots)`).
 * Placar 0 tem ZERO digitos -- e' o que a fonte faz: nenhum slot anima e todos
 * ficam em DISABLED. */
int res_score_digitos(long score);

/* Digito que o slot mostra no fim (o `finalDigit` de cada `ScoreNum`). */
int res_score_final(int slot, long score);

/* Indice em `g_res_score_frame` da animacao (digito, quadro). */
int res_score_indice(int digito, int quadro);

/* Quadro do slot `slot` em `ms` (desde a abertura da tela). Escreve o digito e
 * o quadro da animacao em `*digito`/`*quadro` e devolve 1 se o placar ja'
 * apareceu (`score.visible`), 0 antes disso. */
int res_score_estado(int slot, long score, unsigned long ms,
                     int *digito, int *quadro);

/* 1 = o slot ja' passou do ULTIMO piscar e esta' parado no digito final.
 * E' o que prova que o numero desenhado na tela e' o placar, e nao um passo do
 * shuffle (durante o shuffle o slot tambem mostra o quadro 4). */
int res_score_assentado(int slot, long score, unsigned long ms);

/* ---- OS CONTADORES DE JULGAMENTO (TallyCounter.hx L14-90) ----------------
 *
 * Sete contadores, cada um com o seu numero do jogo, a sua posicao e a sua cor
 * (`cor` no `flavour`). Cada um aparece em `(0.3 * i) + 1.20` s e o numero
 * sobe de 0 ate' o alvo num `FlxTween` de 0,5 s com `FlxEase.quartOut`.
 * Funcoes puras: o main.c so' desenha os glifos que elas apontam.
 */

/* Indice do glifo do digito `d` (0..9) em `g_res_tally_frame`. */
int res_tally_glifo(int d);

/* Quantos digitos o numero `n` ocupa no contador. Zero conta como UM digito
 * (`if (seperatedScore.length == 0) seperatedScore.push(0)`). */
int res_tally_digitos(int n);

/* 1 = o contador `i` ja' apareceu (`(0.3 * i) + 1.20` s). */
int res_tally_visivel(int i, unsigned long ms);

/* O numero desenhado no contador `i` em `ms`, contado do aparecimento dele.
 * Antes de `tween_ms` e' o valor do tween (arredondado como `Math.round`),
 * depois disso e' o alvo: no original o `update` so' redesenha enquanto
 * `curNumber < neededNumber`. */
int res_tally_valor(int alvo, int i, unsigned long ms);

/* Deslocamento em x do contador `i`: `totalNotesHit` e `maxCombo` andam 30 px
 * para a esquerda quando o total de notas passadas de 1000 (L446-450). */
float res_tally_ajuste_x(int i, int total_hit);

/* ---- O PERCENTUAL DE CONCLUSAO (ClearPercentCounter.hx) -------------------
 *
 * O contador GRANDE aparece no meio da tela em 37/24 s (junto do placar),
 * comecando 36 abaixo do alvo (`clearPercentLerp`) e subindo ate' ele num
 * `FlxTween` de 58/24 s com `FlxEase.quartOut`; depois pisca e some.
 * O indice 0/1/2 escolhe o atlas: Left, Right e Small.
 */

#define RES_CP_LEFT  0
#define RES_CP_RIGHT 1
#define RES_CP_SMALL 2

/* Quantos digitos o percentual ocupa (zero conta como um). */
int res_cp_digitos(int alvo);

/* Deslocamento em posicoes do PRIMEIRO digito: 1 com um digito, -1 com tres,
 * 0 nos outros casos (ClearPercentCounter.hx L92-96). */
int res_cp_deslocamento(int digitos);

/* 1 = o digito `ind` usa o atlas `Right`; 0 = usa `Left` (L98-99). */
int res_cp_variante(int digitos, int ind);

/* O numero desenhado em `ms` (desde a abertura da tela): o tween de
 * `abaixo` ate' o alvo, arredondado no fim do tween. */
int res_cp_valor(int alvo, unsigned long ms);

/* 1 enquanto o contador grande esta' na cena (do inicio ate' o fim do fade). */
int res_cp_visivel(unsigned long ms);

/* Alpha do contador grande (255..0): 255 ate' o fade comecar, depois cai. */
int res_cp_alpha(unsigned long ms);

/* 1 durante o `flash(true)` (o `PureColor` branco do original). */
int res_cp_flash(unsigned long ms);

/* ---- O FLASH DA TELA (`bgFlash`, ResultState.hx L141, L573-574, L668-670) --
 *
 * `FlxGradient.createGradientFlxSprite(1280, 720, [0xFFFFF1A6, 0xFFFFF1BE], 90)`:
 * o 4. parametro e' o `chunkSize`, entao a tela e' oito FAIXAS horizontais
 * (720/90) de cor solida, cada uma com a cor de uma linha do gradiente. O
 * sprite nao tem PNG: `res_flash_cor` da' a cor da faixa e o desenho sao oito
 * trapezios. Ele acende duas vezes, alpha 1 -> 0 em queda linear:
 *
 *   37/24 s (com a contagem do percentual), 5/24 s de queda;
 *   `getFlashDelay()` do rank, 14/24 s de queda (`res_flash_alpha`).
 */
#define RES_FLASH_BANDAS 8      /* 720 px / chunkSize 90 */

/* Cor (0..255) da faixa `banda` (0 = topo). */
void res_flash_cor(int banda, int *r, int *g, int *b);

/* Alpha (0..255) do flash do inicio da contagem do percentual. */
int res_flash_alpha_tally(unsigned long ms);

/* ---- A MAQUINA QUE CAI ATRAS DO PLACAR (`soundSystem`) ------------------
 *
 * `ResultState.hx` L189-199: Sparrow `resultScreen/soundSystem`, animacao
 * "idle" (prefixo "sound system"), 65 quadros a 24 fps com `looped = false`,
 * em `(-15, -180)`, `visible = false` ate' um `FlxTimer` de 8/24 s, `zIndex`
 * 1100. Ela toca em TODA partida ("Plays every time!" no comentario da fonte).
 *
 * O `looped = false` do flixel PARA no ultimo quadro: o que fica na tela e' o
 * quadro 64, e e' por isso que o desenho congela e nao some.
 */
int res_ss_quadro(unsigned long ms);

/* ---- O FUNDO (`ResultState.create()` L172-176) -------------------------
 *
 * `FlxGradient.createGradientFlxSprite(1280, 720, [0xFFFECC5C, 0xFFFDC05C], 90)`
 * -- de novo o `chunkSize` 90 sobre 720: OITO faixas horizontais de cor solida.
 * O port desenhava isso como uma textura de 16x360 esticada (23.040 B de VRAM
 * que a tela pagava para sempre); agora sao oito retangulos e a VRAM e' zero.
 * A cor de cada faixa e' a mesma conta do flash, com o par de cores do fundo.
 */
#define RES_BG_FAIXAS 8

/* Cor (0..255) da faixa `banda` (0 = topo) do fundo. */
void res_bg_cor(int banda, int *r, int *g, int *b);

/* ---- O CONTADOR PEQUENO DO TOPO (ClearPercentCounter com `small = true`) --

 * `ResultState.create()` L143 cria `clearPercentSmall` em
 * `(width/2 + 300, height/2 - 100)` com `small = true`, mas quem o poe na cena
 * e' `showSmallClearPercent()`, chamado por `afterRankTallySequence()` no
 * `getBFDelay()` do rank (95/24 s; 97/24 no EXCELLENT). O numero NAO sobe num
 * tween: `curNumber = clearPercentTarget` mostra o valor final de uma vez.
 * `timerThenSongName()` (L741-745) ja' o posicionou em
 * `x = (difficulty.x + difficulty.width) + 60` e `y = 117 + correcao`.
 */

/* 1 = o contador pequeno ja' esta' na cena (`ms >= getBFDelay()` do rank). */
int res_cp_small_visivel(int rank, unsigned long ms);

/* 1 durante o `flash(true)` de 0,4 s que `showSmallClearPercent()` dispara. */
int res_cp_small_flash(int rank, unsigned long ms);

/* x do grupo do contador pequeno, dada a largura da imagem da dificuldade em
 * 640x360: `(difficulty.x + difficulty.width) + 60`, tudo pela metade. */
float res_cp_small_x(float dif_w);

/* ---- O NOME DA MUSICA (FlxBitmapText + tardlingSpritesheet) --------------

 * `ResultState.hx` L119-122 monta o texto com a fonte monoespaçada de 49x61 e
 * `letterSpacing = -15`; L342-360 o poe em `y = -height` (visivel so' quando a
 * barra assenta); L736-765 (`timerThenSongName`) faz a entrada e o resto;
 * L792-836 (`draw`/`update`) recorta e move.
 *
 * O movimento e' um LOOP, nao uma entrada so': `update()` soma `speedOfTween *
 * 60 * elapsed` em x e y dos TRES sprites do topo (nome, dificuldade e
 * percentual pequeno) enquanto `movingSongStuff` estiver ligado, e quando
 * `x + largura < 100` chama `timerThenSongName()` outra vez -- que reseta as
 * posicoes, espera 3,0 s e volta a andar. Este modulo guarda esse estado; o
 * main.c so' le a posicao e desenha um quadrilatero girado.
 */

typedef struct {
    int           ativo;    /* o nome ja' pode aparecer (barra assentada)   */
    int           ciclo;    /* 0 = o primeiro `timerThenSongName()`          */
    unsigned long t0;       /* inicio do ciclo (ms)                          */
    unsigned long move;     /* quando `movingSongStuff` ligou (0 = ainda nao) */
    unsigned long ramp;     /* quando a velocidade comeca a subir de 0       */
    float         x0;       /* x de entrada (clearPercentSmall.x + 94)       */
    float         y_alvo;   /* alvo do tween de entrada em y                 */
    float         larg, alt;/* tamanho do bitmap do titulo                   */
    float         x, y;     /* saida: canto do sprite, em 640x360            */
    float         dx, dy;   /* so' o que o SCROLL somou (sem a entrada)      */
} res_nome_t;

/* Comeca o primeiro ciclo em `ms` (`timerThenSongName(1.0, false)` do create). */
void res_nome_iniciar(res_nome_t *n, unsigned long ms, float x0, float y_alvo,
                      float larg, float alt);

/* `showSmallClearPercent()`: 2,5 s depois de o contador pequeno entrar, o
 * movimento e' solto (`movingSongStuff = true`). Sem esta chamada o nome entra
 * e fica parado -- que e' exatamente o que o original faz ate' esse instante. */
void res_nome_soltar(res_nome_t *n, unsigned long ms);

/* Avanca para o instante `ms`: posicao, e o recomeco do ciclo quando
 * `x + largura < 100`. Chamar uma vez por quadro, ANTES de desenhar. */
void res_nome_passo(res_nome_t *n, unsigned long ms);

/* Corte da esquerda (`clipRect = FlxRect(max(0, 520 - x), ...)`), em pixels
 * LOCAIS do bitmap do titulo. 0 = nada cortado. */
float res_nome_clip_local(const res_nome_t *n);

/* A linha da tabela de titulos da musica `id` (zero se nao houver). */
const res_nome_arte_t *res_nome_do_id(const char *id);

/* ---- A MUSICA DA TELA DE RESULTADO ---------------------------------------
 *
 * `ResultState.hx` L502-550. No `getMusicDelay()` do rank o original pede
 * `getMusicPath(playerCharacter, rank)`; quem decide o arquivo e' o DONO do
 * personagem jogavel (`PlayerRegistry.getCharacterOwnerId(...) ?? 'bf'`), e
 * cada dono tem o seu conjunto (`results*` para o bf, `results*-pico` para o
 * pico). Quando o arquivo base tem um `-intro`, o original toca o intro UMA vez
 * e depois passa para o principal em loop (L506-524); quando nao tem, toca o
 * principal direto (L536-541).
 */

/* O dono dos results do personagem jogavel: 0 = bf, 1 = pico. Um personagem
 * fora da tabela cai no `RES_MUSICA_DONO_PADRAO`, que e' o `?? 'bf'`. */
int res_musica_dono(const char *player);

/* A linha de (dono, rank): o PCM principal e o intro (NULL quando nao existe).
 * `rank` fora da faixa e `dono` invalido caem no primeiro. */
const res_musica_t *res_musica(int dono, int rank);

/* `getMusicDelay()` em ms para o rank (Scoring.hx L530-547). */
unsigned int res_musica_atraso_ms(int rank);

/* Instante (ms desde a abertura da tela) em que o tween do percentual grande
 * termina -- e' o `onComplete` que toca o `confirmMenu` (L609). */
unsigned long res_cp_confirm_ms(void);

/* ---- A BARRA PRETA DO TOPO E A IMAGEM DA DIFICULDADE ---------------------
 *
 * `ResultState.create()` L342-360: a barra nasce em `y = -height` e desliza
 * ate' 0 num `FlxTween` de 7/24 s com `FlxEase.quartOut`, com `startDelay` de
 * 3/24 s; no `onComplete` o nome da musica (`songName.visible = true`) aparece.
 * `timerThenSongName()` (L736-750) poe a dificuldade em
 * `y = -difficulty.height` e a leva ate' `122 + (barra.height - 148)` num
 * tween de 0,5 s com `FlxEase.expoOut` e 0,8 s de atraso.
 *
 * A barra NAO tem PNG: `BitmapUtil.createResultsBar` monta um retangulo preto
 * de 1295x149 girado de -3,8 graus. A geometria que sobra (`g_res_barra`) e' a
 * reta da borda de baixo; o jogo desenha um trapezio de cor solida.
 */

/* Deslocamento em y da barra (0 = assentada, negativo = ainda entrando). */
float res_barra_y(unsigned long ms);

/* 1 = a barra chegou ao fim do tween (`onComplete`: o nome da musica aparece). */
int res_barra_assentada(unsigned long ms);

/* Deslocamento em y da imagem de dificuldade (altura dela em px, 640x360). */
float res_dif_y(float altura, unsigned long ms);

/* O mesmo tween, contado a partir de `desde` -- e' o que `update()` faz a cada
 * recomeco do ciclo: `timerThenSongName()` repo~e `difficulty.y = -height` e
 * repete o tween de 0,8 s + 0,5 s expoOut. `res_dif_y(h, ms)` e'
 * `res_dif_y_desde(h, ms)` (o ciclo zero). */
float res_dif_y_desde(float altura, unsigned long desde);

/* O tween de entrada do PERCENTUAL PEQUENO (`timerThenSongName` L752-757):
 * `small.y = -height` e vai ate' `small_y_alvo` em 0,5 s com expoOut, com
 * 0,85 s de atraso. Contado a partir de `desde` porque ele tambem recomeca. */
float res_cp_small_y_desde(float altura, unsigned long desde);

/* Indice em `g_res_dif` da dificuldade jogada (0..2 = easy/normal/hard), ou
 * -1 se o valor nao for uma das tres do port. */
int res_dif_indice(int dificuldade);

#endif /* RESULTADO_H */
