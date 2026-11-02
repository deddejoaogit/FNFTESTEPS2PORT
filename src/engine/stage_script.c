#include "stage_script.h"

#include <string.h>

#include "stage_dados.h"

/* ---- AS CONSTANTES DO SCRIPT, com a linha de onde sairam ----
   phillyBlazin.hxc:
     L93   `var lightningTimer:Float = 3.0`
     L149  `var LIGHTNING_FULL_DURATION = 1.5`
     L150  `var LIGHTNING_FADE_DURATION = 0.3`
     L153  skyAdditive.alpha = 0.7
     L159  foregroundMultiply.alpha = 0.64
     L163  additionalLighten.alpha = 0.3
     L169  bool(65)  -> 65% para o primeiro intervalo de x
     L171  int(-250, 280)   L175  int(780, 900)
     L180  FlxTween.color(bf, 0.3, 0xFF606060, 0xFFDEDEDE)
     L182  idem dad               L184  gf ate 0xFF888888
     L187  Paths.soundRandom('Lightning', 1, 3)
     L132  lightningTimer = FlxG.random.float(7, 15)
     L114-117  cores de repouso: bf/dad 0xDEDEDE, gf 0x888888
     L167  lightning.animation.play('strike') -- 24 fps, 30 quadros, em loop
           (o dado do pacote: bs_prop_anims[] do `lightning`) */
#define SS_TIMER_INICIAL   3.0f
#define SS_RAIO_DURACAO    1.5f
#define SS_RAIO_FADE       0.3f
#define SS_ALPHA_SKY       0.7f
#define SS_ALPHA_FG        0.64f
#define SS_ALPHA_LIGHTEN   0.3f
#define SS_RAIO_CHANCE_X65 65.0f
#define SS_X_A0           (-250.0f)
#define SS_X_A1            280.0f
#define SS_X_B0            780.0f
#define SS_X_B1            900.0f
#define SS_ESCURO          0x60
#define SS_BF_DAD_CLARO    0xDE
#define SS_GF_CLARO        0x88
#define SS_STRIKE_FPS      24.0f
#define SS_STRIKE_QUADROS  30

/* ---- spookyMansionErect.hxc ----
     L51-79  a sequencia: 0,06 s apaga as luzes e devolve os personagens;
             0,12 s acende as luzes, apaga os personagens de novo e comeca os
             tweens de 1,5 s (luzes -> 0, personagens -> 1)
     L100-119 doLightningStrike: luzes em 1, personagens em 0, e
             `lightningStrikeOffset = FlxG.random.int(8, 24)`
     L160-174 onBeatHit: em `beat == 4` e na musica `spookeez` um strike SEM
             som; e em `FlxG.random.bool(10)` com `beat > strikeBeat+offset` um
             strike COM som (`Paths.soundRandom('thunder_', 1, 2)`, L106)
     L90-91  o estado inicial (buildStage) e' alpha 0.0001 nas duas luzes --
             e' o alpha do JSON do port, conferido em stage_dados.h */
#define SS_FLASH_FASE1     0.06f
#define SS_FLASH_FASE2     0.12f
#define SS_FLASH_TWEEN     1.5f
#define SS_FLASH_CHANCE    10.0f
#define SS_FLASH_BEAT_ESP  4          /* o beat do `spookeez` (L164) */
#define SS_FLASH_OFF_MIN   8
#define SS_FLASH_OFF_MAX   24

/* ---- phillyTrain.hxc ----
     L95-99   estado do trem: trainCars 8, cooldown 0, trainFrameTiming 0
     L100-110 onUpdate: enquanto `trainMoving`, acumula `elapsed` e anda o trem
              a cada 1/24 s (o acumulador zera no disparo, nao desconta)
     L112-136 onBeatHit: `beat % 8 == 4 && bool(30) && !trainMoving &&
              trainCooldown > 8` -> cooldown = int(-4,0) e trainStart();
              e, em TODO `beat % 4 == 0`, `curLight = int(0,4)`
     L138-146 updateTrainPos: com `trainSound.time >= 4700` ms comeca a andar e
              a gf toca `hairBlow`; depois x -= 400, em -2000 volta para -1150 e
              gasta um vagao, em -4000 (acabando) chama trainReset()
     L148-166 trainReset: gf `hairFall` (se tinha comecado), x = 1280+200,
              8 vagoes, trem parado
     L169-186 onSongEnd: para o trem; L190-203 onSongRetry: trainReset       */
#define SS_TREM_FPS        24.0f
#define SS_TREM_PASSO      400.0f   /* L127 `train.x -= 400`              */
#define SS_TREM_EMBALA     (-2000.0f) /* L129 `if (train.x < -2000 ...)`  */
#define SS_TREM_REENTRA    (-1150.0f) /* L130 `train.x = -1150`           */
#define SS_TREM_FIM        (-4000.0f) /* L133 `if (train.x < -4000 ...)`  */
#define SS_TREM_SOM_MS     4700.0f  /* L121 `trainSound.time >= 4700`     */
#define SS_TREM_CHANCE     30.0f    /* L117 `FlxG.random.bool(30)`        */
#define SS_TREM_COOLDOWN   8        /* L117 `trainCooldown > 8`           */
#define SS_TREM_CD_SORTE   (-4)     /* L119 `int(-4, 0)`                  */
#define SS_COR_LUZES       5        /* L28-34 `lightColors.length`        */

/* `phillyTrain.hxc` L28-34 `lightColors`: as cinco cores das `lights`, em
   0xAARRGGBB como no original. */
static const unsigned char ss_cores_luzes[SS_COR_LUZES][3] = {
    { 0x31, 0xA2, 0xFD },   /* 0xFF31A2FD */
    { 0x31, 0xFD, 0x8C },   /* 0xFF31FD8C */
    { 0xFB, 0x33, 0xF5 },   /* 0xFFFB33F5 */
    { 0xFB, 0xA6, 0x33 },   /* 0xFFFBA633 */
    { 0xFD, 0x45, 0x31 }    /* 0xFFFD4531 */
};

/* Semente fixa do xorshift. Ver o cabecalho: a sequencia nao e' a do original
   (o flixel semeia com o relogio), mas a ordem dos sorteios e' -- bool(65),
   int do x, int do som, float do proximo timer. */
#define SS_SEMENTE 0x5EED1A7Bu

static unsigned int ss_rnd(ss_t *ss)
{
    unsigned int x = ss->rng;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    ss->rng = x ? x : SS_SEMENTE;
    return ss->rng;
}

/* 0..1 com 24 bits de mantissa (o `>> 8` evita o bit de sinal). */
static float ss_rnd_unit(ss_t *ss)
{
    return (float)(ss_rnd(ss) >> 8) / 16777216.0f;
}

static float ss_rnd_float(ss_t *ss, float lo, float hi)
{
    return lo + (hi - lo) * ss_rnd_unit(ss);
}

/* `FlxRandom.int` inclui os DOIS extremos: -250..280 tem 531 valores. */
static int ss_rnd_int(ss_t *ss, int lo, int hi)
{
    unsigned int n = (unsigned int)(hi - lo + 1);
    return lo + (int)(ss_rnd(ss) % n);
}

static int ss_rnd_bool(ss_t *ss, float chance)
{
    return ss_rnd_unit(ss) * 100.0f < chance;
}

/* Procura `nome` nos props do palco `si`. Devolve o indice ABSOLUTO em
   bs_props[], ou -1. Nao adivinha: prop que o JSON nao tem fica -1 e o script
   apenas nao mexe nele (o `tools/analisa_scripts_palco.py` ja' provou que
   nenhum dos 18 scripts cita prop que o JSON nao tenha). */
static int acha_prop(int si, const char *nome)
{
    const bs_stage_props_t *st = &bs_stage_props[si];
    int k;
    for (k = 0; k < st->count; k++) {
        if (strcmp(bs_props[st->first + k].name, nome) == 0) return st->first + k;
    }
    return -1;
}

static int acha_cor(int si, const char *nome)
{
    int k;
    for (k = 0; k < bs_stage_cor_count[si]; k++) {
        if (strcmp(bs_cores[bs_stage_cor_first[si] + k].name, nome) == 0)
            return bs_stage_cor_first[si] + k;
    }
    return -1;
}

static int acha_palco(const char *id)
{
    int i;
    if (!id) return -1;
    for (i = 0; i < BS_STAGE_PROPS_COUNT; i++) {
        if (strcmp(bs_stage_props[i].id, id) == 0) return i;
    }
    return -1;
}

void ss_init(ss_t *ss, const char *palco_id)
{
    memset(ss, 0, sizeof(*ss));
    ss->palco = acha_palco(palco_id);
    ss->i_sky = ss->i_light = ss->i_fg = ss->i_lighten = -1;
    /* -1 e nao 0: o `memset` zera, e o indice 0 e' um prop de verdade
       (`skyAdditive` do phillyBlazin). Sem esta linha, um palco com o script do
       spooky "encontrava" o prop 0 e o flash mexia no prop errado. */
    ss->i_bgl = ss->i_stl = -1;
    ss->i_train = ss->i_lights = -1;
    ss->rng = SS_SEMENTE;
    if (ss->palco < 0) return;

    /* ---- phillyBlazin (relampago de ceu) ---- */
    if (strcmp(bs_stage_props[ss->palco].id, "phillyBlazin") == 0) {
        ss->i_sky = acha_prop(ss->palco, "skyAdditive");
        ss->i_light = acha_prop(ss->palco, "lightning");
        ss->i_fg = acha_prop(ss->palco, "foregroundMultiply");
        ss->i_lighten = acha_cor(ss->palco, "additionalLighten");
        /* Sem os quatro o script nao tem o que acender: e' dado faltando, e o
           dado daqui e' o JSON do palco (conferido por gen_stage_dados.py). */
        if (ss->i_sky < 0 || ss->i_light < 0 || ss->i_fg < 0 || ss->i_lighten < 0)
            return;
        ss->cor_ligada = 1;      /* L114-117: so' ESTE palco pinta personagem */
        ss->ativo = 1;
        ss_reiniciar(ss);
        return;
    }

    /* ---- phillyTrain (o trem que passa e as luzes) ---- */
    if (strcmp(bs_stage_props[ss->palco].id, "phillyTrain") == 0) {
        ss->i_train = acha_prop(ss->palco, "train");
        ss->i_lights = acha_prop(ss->palco, "lights");
        /* Sem o `train` nao ha' trem. As `lights` sao opcionais para o
           movimento, mas sem elas o `color` do L149 nao tem onde pousar: o
           layout continua desenhando o prop com a arte do dado. */
        if (ss->i_train < 0) return;
        ss->ativo = 1;
        ss_reiniciar(ss);
        /* O x INICIAL do trem e' o do dado (o `buildStage` do flixel poe o
           sprite na posicao do JSON e o script so' mexe nele no primeiro
           `train.x -= 400`, L127). O `ss_reiniciar` deixa -1 = "o dado manda",
           entao quem preenche aqui e' o dado de verdade. */
        ss->trem_x = bs_props[ss->i_train].x;
        return;
    }

    /* ---- spookyMansionErect (flash de interior) ---- */
    if (strcmp(bs_stage_props[ss->palco].id, "spookyMansionErect") == 0) {
        ss->i_bgl = acha_prop(ss->palco, "bgLight");
        ss->i_stl = acha_prop(ss->palco, "stairsLight");
        if (ss->i_bgl < 0 || ss->i_stl < 0) return;
        ss->ativo = 1;
        ss_reiniciar(ss);
        return;
    }
    /* Qualquer outro palco fica INATIVO: todas as consultas devolvem "o dado
       manda", entao nenhum palco ganha comportamento por acidente. */
}

void ss_reiniciar(ss_t *ss)
{
    if (!ss->ativo) return;
    ss->t = 0.0f;
    ss->timer = SS_TIMER_INICIAL;      /* L93 */
    ss->ligado = 1;                    /* L36 `lightningActive = true` */
    ss->raios = 0;
    ss->raio_ativo = 0;
    ss->raio_t0 = 0.0f;
    ss->raio_x = 0.0f;
    ss->a_sky = ss->a_fg = ss->a_lighten = 0.0f;
    ss->som = 0;
    ss->som_banco = SS_SOM_LIGHTNING;
    ss->cor_ativa = 0;
    ss->cor_t0 = 0.0f;
    /* L114-117: a cor de REPOUSO dos personagens neste palco nao e' branca --
       e so' existe nos palcos que tem cor de personagem (`cor_ligada`). */
    if (ss->cor_ligada) {
        ss->cor[SS_BF][0] = ss->cor[SS_BF][1] = ss->cor[SS_BF][2] = SS_BF_DAD_CLARO;
        ss->cor[SS_DAD][0] = ss->cor[SS_DAD][1] = ss->cor[SS_DAD][2] = SS_BF_DAD_CLARO;
        ss->cor[SS_GF][0] = ss->cor[SS_GF][1] = ss->cor[SS_GF][2] = SS_GF_CLARO;
    } else {
        ss->cor[SS_BF][0] = ss->cor[SS_BF][1] = ss->cor[SS_BF][2] = 0xFF;
        ss->cor[SS_DAD][0] = ss->cor[SS_DAD][1] = ss->cor[SS_DAD][2] = 0xFF;
        ss->cor[SS_GF][0] = ss->cor[SS_GF][1] = ss->cor[SS_GF][2] = 0xFF;
    }
    /* O flash do spookyMansionErect: luzes no alpha do buildStage (0,0001 vem
       do JSON) e personagens em 1. */
    ss->flash = 0;
    ss->flash_fase = 0;
    ss->a_bgl = ss->a_stl = -1.0f;
    ss->a_char[SS_BF] = ss->a_char[SS_DAD] = ss->a_char[SS_GF] = 1.0f;
    ss->strike_beat = 0;                 /* L26: `lightningStrikeBeat = 0` */
    ss->strike_offset = SS_FLASH_OFF_MIN; /* L27: `lightningStrikeOffset = 8` */
    ss->ultimo_beat = -1;
    /* O TREM. `onCreate` (L46-58) zera o estado e o `onSongRetry` (L190-203)
       chama `trainReset()`: x = 1480, 8 vagoes, parado, e o som parado. O x
       fica em -1 ate' o script mexer nele (o dado manda enquanto isso). */
    ss->trem_ligado = 1;
    ss->trem_movendo = 0;
    ss->trem_som_ligado = 0;
    ss->trem_carros = SS_TREM_CARROS;
    ss->trem_acabando = 0;
    ss->trem_comecou = 0;
    ss->trem_cooldown = 0;
    /* `trainReset()` no `onSongRetry` (L190-203) poe o trem em 1480 -- e nao no
       x do JSON: o `ss_init` sobrepoe este valor com o do dado logo depois, que
       e' o estado do `buildStage`. */
    if (ss->trem_comecou) ss->hair_pedido = 2;      /* `hairFall` */
    ss->trem_x = ss->i_train >= 0 ? SS_TREM_X_RESET : -1.0f;
    ss->trem_t = 0.0f;
    ss->trem_quadro = 0.0f;
    ss->luz_cor = -1;
    ss->hair_pedido = 0;
}

void ss_song_end(ss_t *ss)
{
    if (!ss->ativo) return;
    ss->ligado = 0;                    /* phillyBlazin L227 */
    /* phillyTrain L169-176: o trem para e o som e' interrompido. */
    ss->trem_movendo = 0;
    ss->trem_ligado = 0;
    ss->trem_som_ligado = 0;
}

/* applyLightning(), L142-190. A ORDEM dos sorteios e' a da fonte. */
static void ss_raio(ss_t *ss)
{
    int x65;
    ss->raios++;
    ss->raio_ativo = 1;
    ss->raio_t0 = ss->t;
    ss->a_sky = SS_ALPHA_SKY;
    ss->a_fg = SS_ALPHA_FG;
    ss->a_lighten = SS_ALPHA_LIGHTEN;
    /* L167: `animation.play('strike')` -> o quadro 0 e' o instante do raio
       (quem calcula o quadro e' ss_prop_frame). */
    /* L169-176: o x do relampago. */
    x65 = ss_rnd_bool(ss, SS_RAIO_CHANCE_X65);
    if (x65) ss->raio_x = (float)ss_rnd_int(ss, (int)SS_X_A0, (int)SS_X_A1);
    else ss->raio_x = (float)ss_rnd_int(ss, (int)SS_X_B0, (int)SS_X_B1);
    /* L187: `Paths.soundRandom('Lightning', 1, 3)`. */
    ss->som = ss_rnd_int(ss, 1, 3);
    ss->som_banco = SS_SOM_LIGHTNING;
    /* L180-184: os personagens escurecem por 0,3 s e voltam ao repouso. */
    ss->cor_ativa = 1;
    ss->cor_t0 = ss->t;
    ss->cor[SS_BF][0] = ss->cor[SS_BF][1] = ss->cor[SS_BF][2] = SS_ESCURO;
    ss->cor[SS_DAD][0] = ss->cor[SS_DAD][1] = ss->cor[SS_DAD][2] = SS_ESCURO;
    ss->cor[SS_GF][0] = ss->cor[SS_GF][1] = ss->cor[SS_GF][2] = SS_ESCURO;
}

/* `doLightningStrike` (L100-119) do spookyMansionErect: as duas luzes em 1, os
   tres personagens em 0, a sequencia armada e o offset sorteado. */
static void ss_flash_comeca(ss_t *ss)
{
    ss->flash = 1;
    ss->flash_fase = 0;
    ss->flash_t0 = ss->t;
    ss->a_bgl = 1.0f;
    ss->a_stl = 1.0f;
    ss->a_char[SS_BF] = ss->a_char[SS_DAD] = ss->a_char[SS_GF] = 0.0f;
    /* L119: `lightningStrikeOffset = FlxG.random.int(8, 24)` -- sorteado DEPOIS
       do som (L106) e antes de qualquer outra coisa, como no original. */
    ss->strike_offset = ss_rnd_int(ss, SS_FLASH_OFF_MIN, SS_FLASH_OFF_MAX);
    ss->strike_beat = ss->ultimo_beat;
}

static void ss_trem_comeca(ss_t *ss);
static void ss_trem_anda(ss_t *ss);

/* O `onBeatHit` (L155-174). A ordem dos sorteios e' a da fonte:
   1) `FlxG.random.bool(10)` -- sorteado em TODO beat, mesmo quando o segundo
      `if` nem chega a disparar (curto-circuito do `&&`);
   2) o `int(1,2)` do som, dentro do strike com som;
   3) o `int(8,24)` do offset.
   O primeiro `if` (beat 4 na spookeez) NAO sorteia som. */
int ss_beat(ss_t *ss, int beat, const char *song_id, int tem_personagens)
{
    int chance, disparou = 0;
    if (!ss->ativo) return 0;
    /* Cada beat chega UMA vez; quem chama passa o beat do chart, que pode
       repetir em varios quadros. */
    if (beat == ss->ultimo_beat) return 0;
    ss->ultimo_beat = beat;

    /* ---- phillyTrain L112-149 ---- */
    if (ss->i_train >= 0) {
        if (ss->trem_ligado) {
            /* L115-116: o cooldown sobe em TODO beat parado (nao so' nos
               beats do trem). */
            if (!ss->trem_movendo) ss->trem_cooldown += 1;
            /* L117: a ordem dos operandos e' a da fonte -- `beat % 8 == 4`
               primeiro, o `bool(30)` depois, e so' entao as duas condicoes de
               estado (por isso o sorteio acontece mesmo com o trem andando). */
            if (beat % 8 == 4) {
                int r30 = ss_rnd_bool(ss, SS_TREM_CHANCE);
                if (r30 && !ss->trem_movendo &&
                    ss->trem_cooldown > SS_TREM_COOLDOWN) {
                    ss->trem_cooldown = ss_rnd_int(ss, SS_TREM_CD_SORTE, 0); /* L119 */
                    ss_trem_comeca(ss);
                    disparou = 1;
                }
            }
        }
        /* L142-149: em TODO `beat % 4 == 0` (inclusive o 0) as luzes trocam de
           cor. O `lightShader.reset()` da mesma linha e' shader: nao portado. */
        if (beat % 4 == 0) ss->luz_cor = ss_rnd_int(ss, 0, SS_COR_LUZES - 1);
        return disparou;
    }

    if (ss->i_bgl < 0) return 0;         /* so' o spooky usa beat */

    /* L164: `event.beat == 4 && currentSong.id == "spookeez"` -> sem som. */
    if (beat == SS_FLASH_BEAT_ESP && song_id && strcmp(song_id, "spookeez") == 0) {
        if (tem_personagens) { ss_flash_comeca(ss); disparou = 1; }
    }
    /* L170: `FlxG.random.bool(10) && beat > strikeBeat + strikeOffset`.
       O sorteio acontece SEMPRE (e' o primeiro operando do `&&`). */
    chance = ss_rnd_bool(ss, SS_FLASH_CHANCE);
    if (chance && beat > ss->strike_beat + ss->strike_offset && tem_personagens) {
        ss->som = 1 + ss_rnd_int(ss, 0, 1);   /* Paths.soundRandom('thunder_',1,2) */
        ss->som_banco = SS_SOM_TROVAO;
        ss_flash_comeca(ss);
        disparou = 1;
    }
    return disparou;
}

/* ---- phillyTrain.hxc: o trem --------------------------------------------
   `trainStart()` (L200-204): o trem passa a andar e o som `train_passes`
   comeca do zero (`play(true)` reinicia). */
static void ss_trem_comeca(ss_t *ss)
{
    ss->trem_movendo = 1;
    ss->trem_t = 0.0f;              /* `trainSound.play(true)` -> time = 0 */
    ss->trem_som_ligado = 1;
    ss->som = 1;                    /* o unico som do banco do trem */
    ss->som_banco = SS_SOM_TREM;
}

/* `trainReset()` (L148-166). O `playAnimation('hairFall')` da gf so' acontece
   se o trem tinha comecado a andar -- por isso o `trem_comecou` e' testado
   ANTES de ser zerado. */
static void ss_trem_reset(ss_t *ss)
{
    if (ss->trem_comecou) ss->hair_pedido = 2;      /* `hairFall` */
    ss->trem_x = SS_TREM_X_RESET;                   /* L152 */
    ss->trem_movendo = 0;
    ss->trem_carros = SS_TREM_CARROS;
    ss->trem_acabando = 0;
    ss->trem_comecou = 0;
}

/* `updateTrainPos()` (L138-146): o 1/24 s do trem. */
static void ss_trem_anda(ss_t *ss)
{
    /* L121-126: so' depois de 4,7 s de SOM o trem "aparece" e a gf reage. */
    if (ss->trem_som_ligado && ss->trem_t >= SS_TREM_SOM_MS / 1000.0f) {
        if (!ss->trem_comecou) ss->hair_pedido = 1;   /* `hairBlow` */
        ss->trem_comecou = 1;
    }
    if (!ss->trem_comecou) return;
    ss->trem_x -= SS_TREM_PASSO;                /* L127 */
    if (ss->trem_x < SS_TREM_EMBALA && !ss->trem_acabando) {  /* L129-133 */
        ss->trem_x = SS_TREM_REENTRA;
        ss->trem_carros -= 1;
        if (ss->trem_carros <= 0) ss->trem_acabando = 1;
    }
    if (ss->trem_x < SS_TREM_FIM && ss->trem_acabando)        /* L133-134 */
        ss_trem_reset(ss);
}

static float ss_suave(float u)
{
    if (u <= 0.0f) return 0.0f;
    if (u >= 1.0f) return 1.0f;
    return u;
}

int ss_update(ss_t *ss, float elapsed)
{
    int caiu = 0;
    float dt;
    if (!ss->ativo) return 0;
    if (elapsed < 0.0f) elapsed = 0.0f;
    ss->t += elapsed;

    /* L120-133. So' no `phillyBlazin`: o timer e o raio sao DELE, e um palco
       com outro script nao pode herdar os indices de prop do palco anterior. */
    if (ss->i_sky >= 0) {
        if (ss->ligado) ss->timer -= elapsed;
        else ss->timer = 1.0f;
        if (ss->timer <= 0.0f) {
            ss_raio(ss);
            ss->timer = ss_rnd_float(ss, 7.0f, 15.0f);
            caiu = 1;
        }
    }

    /* ---- O TREM DO phillyTrain (L100-110 + L112-166) ---- */
    if (ss->i_train >= 0) {
        /* L127-131: o relogio do SOM (`trainSound.time`), em segundos. O som
           toca a partir do `trainStart` e o original nunca o reinicia no
           `trainReset`, entao o relogio continua correndo. */
        if (ss->trem_som_ligado) ss->trem_t += elapsed;
        if (ss->trem_ligado && ss->trem_movendo) {
            double passo = 1.0 / (double)SS_TREM_FPS;
            ss->trem_quadro += elapsed;
            if ((double)ss->trem_quadro >= passo) {
                ss_trem_anda(ss);
                ss->trem_quadro = 0.0f;      /* L129 zera, nao desconta */
            }
        }
    }

    /* ---- O FLASH DO spookyMansionErect (L51-79) ---- */
    if (ss->i_bgl >= 0 && ss->flash) {
        float dt = ss->t - ss->flash_t0;
        /* L51-61, 0,06 s: as luzes apagam e os personagens voltam (1 quadro de
           cada estado -- e' um pisca). */
        if (ss->flash_fase < 1 && dt >= SS_FLASH_FASE1) {
            ss->flash_fase = 1;
            ss->a_bgl = ss->a_stl = 0.0f;
            ss->a_char[SS_BF] = ss->a_char[SS_DAD] = ss->a_char[SS_GF] = 1.0f;
        }
        /* L62-78, 0,12 s: acendem de novo, os personagens apagam, e os cinco
           tweens de 1,5 s comecam (luzes -> 0, personagens -> 1). */
        if (ss->flash_fase < 2 && dt >= SS_FLASH_FASE2) {
            ss->flash_fase = 2;
            ss->a_bgl = ss->a_stl = 1.0f;
            ss->a_char[SS_BF] = ss->a_char[SS_DAD] = ss->a_char[SS_GF] = 0.0f;
        }
        if (ss->flash_fase >= 2) {
            float u = ss_suave((dt - SS_FLASH_FASE2) / SS_FLASH_TWEEN);
            ss->a_bgl = ss->a_stl = 1.0f - u;
            ss->a_char[SS_BF] = ss->a_char[SS_DAD] = ss->a_char[SS_GF] = u;
            if (u >= 1.0f) {
                ss->flash = 0;
                ss->a_bgl = ss->a_stl = 0.0f;
            }
        }
    }

    /* Os tres `FlxTween.tween(alpha, 0, ...)` do L154-164. O tween anda no
       mesmo quadro em que nasceu (o flixel roda o update dos tweens depois do
       update do estado). O `cleanupLightning` do L230-240 e' o fim do tween de
       1,5 s: esconde os QUATRO, inclusive o `lightning`. */
    if (ss->raio_ativo) {
        dt = ss->t - ss->raio_t0;
        ss->a_sky = SS_ALPHA_SKY * (1.0f - ss_suave(dt / SS_RAIO_DURACAO));
        ss->a_fg = SS_ALPHA_FG * (1.0f - ss_suave(dt / SS_RAIO_DURACAO));
        ss->a_lighten = SS_ALPHA_LIGHTEN * (1.0f - ss_suave(dt / SS_RAIO_FADE));
        if (dt >= SS_RAIO_DURACAO) {
            ss->raio_ativo = 0;
            ss->a_sky = ss->a_fg = ss->a_lighten = 0.0f;
        }
        /* L180-184: `FlxTween.color(obj, 0.3, 0x606060, claro)`. */
        if (ss->cor_ativa) {
            float u = ss_suave(dt / SS_RAIO_FADE);
            int i;
            for (i = 0; i < 3; i++) {
                int alvo = i == SS_GF ? SS_GF_CLARO : SS_BF_DAD_CLARO;
                ss->cor[i][0] = (unsigned char)(SS_ESCURO + (alvo - SS_ESCURO) * u);
                ss->cor[i][1] = ss->cor[i][0];
                ss->cor[i][2] = ss->cor[i][0];
            }
            if (u >= 1.0f) ss->cor_ativa = 0;
        }
    }
    return caiu;
}

float ss_prop_alpha(const ss_t *ss, int prop_abs)
{
    if (!ss->ativo || prop_abs < 0) return -1.0f;
    /* spookyMansionErect: as duas luzes do flash. Em repouso o alpha e' o do
       dado (0,0001, do `buildStage`), nao um 0 escrito aqui. */
    if (prop_abs == ss->i_bgl) return ss->a_bgl < 0.0f ? -1.0f : ss->a_bgl;
    if (prop_abs == ss->i_stl) return ss->a_stl < 0.0f ? -1.0f : ss->a_stl;
    if (prop_abs == ss->i_sky) return ss->raio_ativo ? ss->a_sky : 0.0f;
    if (prop_abs == ss->i_fg) return ss->raio_ativo ? ss->a_fg : 0.0f;
    if (prop_abs == ss->i_light) return ss->raio_ativo ? 1.0f : 0.0f;
    return -1.0f;
}

float ss_prop_x(const ss_t *ss, int prop_abs, float x_dado)
{
    if (!ss->ativo || prop_abs < 0) return x_dado;
    /* O trem: quem escreve o x e' o `updateTrainPos` (L127-134), e o primeiro
       valor e' o do dado. */
    if (prop_abs == ss->i_train) return ss->trem_x >= 0.0f ? ss->trem_x : x_dado;
    if (prop_abs != ss->i_light) return x_dado;
    /* Antes do PRIMEIRO raio o prop esta' onde o JSON o poe: quem escreve o x e'
       o `applyLightning` (L171/L175), e ate' ele o valor e' o do palco. */
    if (ss->raios == 0) return x_dado;
    return ss->raio_x;
}

int ss_prop_frame(const ss_t *ss, int prop_abs, int *anim, int *frame)
{
    int q;
    if (!ss->ativo || prop_abs != ss->i_light || !ss->raio_ativo) return 0;
    /* `animation.play('strike')` reinicia do quadro 0 a cada raio, 24 fps, e a
       animacao do dado e' em loop (30 quadros). Em 1,5 s de raio o flixel passa
       de 30 (36 quadros) e da' a volta -- por isso o modulo. */
    q = (int)((ss->t - ss->raio_t0) * SS_STRIKE_FPS);
    if (q < 0) q = 0;
    q %= SS_STRIKE_QUADROS;
    if (anim) *anim = 0;      /* o `lightning` so' tem a `strike` */
    if (frame) *frame = q;
    return 1;
}

float ss_cor_alpha(const ss_t *ss, int cor_abs)
{
    if (!ss->ativo || cor_abs < 0) return -1.0f;
    if (cor_abs == ss->i_lighten) return ss->raio_ativo ? ss->a_lighten : 0.0f;
    return -1.0f;
}

int ss_prop_cor(const ss_t *ss, int prop_abs, unsigned char *rgb)
{
    if (!ss->ativo || !rgb || prop_abs < 0) return 0;
    if (prop_abs != ss->i_lights || ss->luz_cor < 0 || ss->luz_cor >= SS_COR_LUZES)
        return 0;
    rgb[0] = ss_cores_luzes[ss->luz_cor][0];
    rgb[1] = ss_cores_luzes[ss->luz_cor][1];
    rgb[2] = ss_cores_luzes[ss->luz_cor][2];
    return 1;
}

int ss_cor_papel(const ss_t *ss, int papel, unsigned char *rgb)
{
    if (!ss->ativo || !ss->cor_ligada || !rgb || papel < 0 || papel > SS_GF) return 0;
    rgb[0] = ss->cor[papel][0];
    rgb[1] = ss->cor[papel][1];
    rgb[2] = ss->cor[papel][2];
    return 1;
}

float ss_alpha_papel(const ss_t *ss, int papel)
{
    if (!ss->ativo || papel < 0 || papel > SS_GF) return -1.0f;
    if (ss->i_bgl < 0) return -1.0f;          /* so' o spooky mexe nisso */
    return ss->a_char[papel];
}
