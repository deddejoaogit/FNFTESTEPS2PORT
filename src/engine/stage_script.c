#include "stage_script.h"

#include <math.h>
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

/* ---- spookyMansion.hxc (o palco da mansao NAO-erect: spookeez, south e
        monster -- o erect e' outra variacao e o script dele e' outro) ----
     L23-24  `lightningStrikeBeat = 0`, `lightningStrikeOffset = 8`
     L26-49  doLightningStrike(playSound, beat):
               `if (playSound) FunkinSound.playOnce(Paths.soundRandom(
                  'thunder_', 1, 2), 1.0)`
               `getNamedProp('halloweenBG').animation.play('lightning')`
               `lightningStrikeBeat = beat`
               `lightningStrikeOffset = FlxG.random.int(8, 24)`
               bf: `hasAnimation('scared') && animation.name != 'cheer'` ->
                   `playAnimation('scared', true, true)`
               gf: `hasAnimation('scared')` -> `playAnimation('scared', ...)`
     L72-91  onBeatHit: `beat == 4` na `spookeez` -> strike SEM som; e
             `FlxG.random.bool(10) && beat > strikeBeat + strikeOffset` ->
             strike COM som
     L124-131 onCountdownStart: zera beat/offset   (idem onSongRetry L115-122)
   DIFERENCAS em relacao ao ERECT, de proposito:
     - nao ha' `if (getBoyfriend() == null ...) return`: aqui o ceu anima e o
       trovao toca mesmo sem os tres personagens em cena (o erect sai fora
       antes de tudo). Por isso o ramo da mansao NAO usa `tem_personagens`.
     - nao ha' luzes de alpha: quem pisca e' a ANIMACAO `lightning` do
       `halloweenBG`, um `play` de 30 quadros a 24 fps (o dado do pacote:
       `bs_prop_anims[]`, `lightning 5..34`). */

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

/* ---- phillyStreets.hxc (o sinal e os dois carros; as tres musicas que usam
        este palco sao `darnell`, `lit-up` e `2hot`) ----
     L34-41   estado: `lightsStop` falso, `lastChange` 0, `changeInterval` 8,
              os carros "interruptaveis"
     L53-70   changeLights: inverte `lightsStop`; vermelho -> `tored` e
              intervalo 20; verde -> `togreen`, intervalo 30 e, se um carro
              estava esperando, ele sai (`finishCarLights`)
     L75-103  resetCar: x 1200, y 818, angulo 0 (left/right)
     L189-207 finishCarLights: `duration = float(1.8, 3)`,
              `startdelay = float(0.2, 1.2)`, a curva de 3 pontos e o angulo
              -5 -> 18 com `sineIn`
     L214-273 driveCarLights: variante `int(1,4)`, duracao e `extraOffset` por
              variante, angulo -7 -> -5 e `cubeOut`, para no ponto perto do sinal
     L278-332 driveCar: idem, atravessa a tela, angulo -8 -> 18, `ease: null`
     L337-395 driveCarBack: a mesma coisa no sentido contrario (carro 2)
     L402-415 resetStageValues: `lastChange` 0, intervalo 8, `togreen`
     L453-484 onUpdate: `scrollingSky.scrollX -= elapsed * 22` (o ceu do script
              e' um sprite criado -- ver a falta declarada) e o shader de chuva
     L486-511 onBeatHit: `bool(10)` para cada carro, com o portao do intervalo,
              e a troca do sinal quando `beat == lastChange + changeInterval` */
#define SS_PHILLY_INT_INICIAL 8
#define SS_PHILLY_INT_VERMELHO 20
#define SS_PHILLY_INT_VERDE 30
#define SS_PHILLY_CAR_CHANCE 10.0f
#define SS_PHILLY_CAR_X 1200.0f     /* L85 `cars.x = 1200`   */
#define SS_PHILLY_CAR_Y 818.0f      /* L86 `cars.y = 818`    */
#define SS_PHILLY_LUZ_FPS 24.0f
/* Os pontos das tres curvas, em unidades do ORIGINAL (L197-198, L242-243,
   L246-247, L297-298, L342-343). O `- offset` do script ja' esta' embutido nos
   numeros da fonte: aqui ficam como o `FlxPoint.get(...)` os monta, e o
   `offset` da variante entra depois, no desenho. */
#define SS_PHILLY_OFFX 306.6f       /* L195 `offset = [306.6, 168.3]` */
#define SS_PHILLY_OFFY 168.3f

/* ---- tankmanBattlefield.hxc (o tanque dando volta no campo) ----
     L44-47   `tankMoving` falso, `tankAngle = int(-90,45)`,
              `tankSpeed = float(5,7)`, `tankX = 400`
     L34-35   `buildStage` SORTEIA DE NOVO os dois (o inicializador da
              declaracao ja' sorteou uma vez: sao QUATRO sorteios, e a ordem
              fica registrada aqui)
     L54-63   `moveTank(elapsed)`: `tankAngle += elapsed * tankSpeed`,
              `angle = tankAngle - 90 + 15`,
              `x = 400 + cos(rad(tankAngle + 180)) * 1500`,
              `y = 1300 + sin(rad(tankAngle + 180)) * 1100`
     L65-69   `onSongRetry` -> `moveTank(0)` (reaplica sem andar)
     L24-31   as nuvens: `FlxTiledSprite('tankClouds', 3200, 235)` em
              (-1100, 20), `scrollFactor` 0,25, z 12, `velocity.x = 8`  */
#define SS_TANK_X 400.0f
#define SS_TANK_CX 1500.0f
#define SS_TANK_CY 1100.0f
#define SS_TANK_Y0 1300.0f
#define SS_TANK_GRAUS (-90.0f + 15.0f)   /* L60 `- 90 + 15` */
#define SS_TANK_ANG_MIN (-90)
#define SS_TANK_ANG_MAX 45
#define SS_TANK_VEL_MIN 5.0f
#define SS_TANK_VEL_MAX 7.0f

/* ---- limoRide.hxc (o carro veloz da semana 4: satin-panties, high e milf) ----
     L33-56   `buildStage`: o shader do ceu (nao portado) e o
              `carSequenceData` = [{1, haptics on}, {1.4, haptics off},
              {2, resetFastCar}]; depois `resetFastCar()`
     L70-105  `resetFastCar`: x -12600, y `int(350,380)`, velocidade 0,
              `fastCarCanDrive = true`
     L60-67   `onBeatHit`: `FlxG.random.bool(10) && fastCarCanDrive` ->
              `fastCarDrive()`
     L97-110  `fastCarDrive`: `playOnce(soundRandom('carPass', 0, 1), 0.7)`,
              `velocity.x = (int(170,220) / FlxG.elapsed) * 3`,
              `fastCarCanDrive = false` e `new Sequence(carSequenceData)`
     L152-165 `onPause`/`onResume` ligam/desligam `carSequence.running`      */
#define SS_CAR_X0       (-12600.0f)  /* L87 `fastCar.x = -12600`         */
#define SS_CAR_Y_MIN    350          /* L88 `int(350, 380)`              */
#define SS_CAR_Y_MAX    380
#define SS_CAR_VEL_MIN  170          /* L106 `int(170, 220)`             */
#define SS_CAR_VEL_MAX  220
#define SS_CAR_VEL_MULT 3.0f         /* L106 `* 3`                       */
#define SS_CAR_VOL      70           /* L99 `playOnce(..., 0.7)`         */
#define SS_CAR_CHANCE   10.0f        /* L66 `FlxG.random.bool(10)`       */
/* Os tres eventos da Sequence, em segundos absolutos desde o `drive` (L44-48),
   ja' ordenados como o `Sequence` do original ordena (`|time|` crescente):
   1 -> `doCarHaptics = true`, 1,4 -> `false`, 2 -> `resetFastCar()`. */
static const float ss_car_seq_t[3] = { 1.0f, 1.4f, 2.0f };

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
static void ss_tank_move(ss_t *ss, float elapsed);
static void ss_fastcar_reset(ss_t *ss);

static int acha_prop(int si, const char *nome)
{
    const bs_stage_props_t *st = &bs_stage_props[si];
    int k;
    for (k = 0; k < st->count; k++) {
        if (strcmp(bs_props[st->first + k].name, nome) == 0) return st->first + k;
    }
    return -1;
}

/* Indice da ANIMACAO `nome` dentro do prop `prop_abs`, ou -1. E' o
   `hasAnimation()`/`animation.play('lightning')` do original. */
static int acha_anim(int prop_abs, const char *nome)
{
    const bs_prop_t *p = &bs_props[prop_abs];
    int k;
    for (k = 0; k < p->anim_count; k++) {
        if (strcmp(bs_prop_anims[p->anim_first + k].name, nome) == 0) return k;
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
    ss->i_luz = ss->i_car = ss->i_car2 = -1;
    ss->i_tank = -1;
    ss->i_fastcar = -1;
    ss->som_vol = 100;
    ss->i_freaks = -1;
    ss->sufixo_medo = 0;
    ss->tank_px = SS_TANK_X;
    ss->tank_py = SS_TANK_Y0;
    ss->i_bg = ss->bg_anim = -1;
    ss->bg_t0 = -1.0f;
    ss->bg_quadros = 0;
    ss->bg_fps = 24.0f;
    ss->scared_pedido = 0;
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
        ss->tem_beat = 1;               /* L112-149 `onBeatHit` */
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

    /* ---- school (o `idleSuffix` do `freaks` quando a musica e' `roses`) ---- */
    if (strcmp(bs_stage_props[ss->palco].id, "school") == 0) {
        ss->i_freaks = acha_prop(ss->palco, "freaks");
        if (ss->i_freaks < 0) return;
        /* O sufixo depende da musica e chega pelo `ss_set_song`; sem ele o
           palco fica igual ao dado (as animacoes sem sufixo). */
        ss->ativo = 1;
        ss_reiniciar(ss);
        return;
    }

    /* ---- tankmanBattlefield (o tanque) ---- */
    if (strcmp(bs_stage_props[ss->palco].id, "tankmanBattlefield") == 0) {
        ss->i_tank = acha_prop(ss->palco, "tankRolling");
        if (ss->i_tank < 0) return;
        ss->ativo = 1;
        ss_reiniciar(ss);
        /* QUATRO sorteios, como na fonte: os dois do inicializador da
           declaracao (L45-46, que rodam na construcao do script) e os dois do
           `buildStage` (L34-35). A sequencia do gerador nao e' a do flixel, mas
           a QUANTIDADE e a ORDEM sao as da fonte. */
        ss->tank_ang = (float)ss_rnd_int(ss, SS_TANK_ANG_MIN, SS_TANK_ANG_MAX);
        ss->tank_vel = ss_rnd_float(ss, SS_TANK_VEL_MIN, SS_TANK_VEL_MAX);
        ss->tank_ang = (float)ss_rnd_int(ss, SS_TANK_ANG_MIN, SS_TANK_ANG_MAX);
        ss->tank_vel = ss_rnd_float(ss, SS_TANK_VEL_MIN, SS_TANK_VEL_MAX);
        ss_tank_move(ss, 0.0f);
        return;
    }

    /* ---- phillyStreets (o sinal e os dois carros) ---- */
    if (strcmp(bs_stage_props[ss->palco].id, "phillyStreets") == 0) {
        ss->tem_beat = 1;               /* L486-511 `onBeatHit` */
        ss->i_luz = acha_prop(ss->palco, "phillyTraffic");
        ss->i_car = acha_prop(ss->palco, "phillyCars");
        ss->i_car2 = acha_prop(ss->palco, "phillyCars2");
        /* Sem o sinal ou sem UM dos carros nao ha' script: o que o `.hxc` faz
           depende dos tres (`getNamedProp` devolveria null e o original
           estouraria). E' dado ausente, e o palco fica sem script. */
        if (ss->i_luz < 0 || ss->i_car < 0 || ss->i_car2 < 0) return;
        ss->ativo = 1;
        ss_reiniciar(ss);
        return;
    }

    /* ---- spookyMansion (a animacao `lightning` do halloweenBG) ---- */
    if (strcmp(bs_stage_props[ss->palco].id, "spookyMansion") == 0) {
        ss->tem_beat = 1;               /* L72-91 `onBeatHit` */
        ss->i_bg = acha_prop(ss->palco, "halloweenBG");
        if (ss->i_bg < 0) return;
        ss->bg_anim = acha_anim(ss->i_bg, "lightning");
        /* A animacao TEM de existir no pacote: sem ela o script nao tem o que
           tocar. O nome e' o do JSON do palco (`animations[].name`), entao um
           pacote sem ela e' dado ausente, e o palco fica sem script. */
        if (ss->bg_anim < 0) return;
        ss->bg_quadros = bs_prop_anims[bs_props[ss->i_bg].anim_first + ss->bg_anim].count;
        ss->bg_fps = bs_prop_anims[bs_props[ss->i_bg].anim_first + ss->bg_anim].fps;
        if (ss->bg_quadros <= 0) return;
        if (ss->bg_fps <= 0.0f) ss->bg_fps = 24.0f;
        ss->ativo = 1;
        ss_reiniciar(ss);
        return;
    }

    /* ---- spookyMansionErect (flash de interior) ---- */
    if (strcmp(bs_stage_props[ss->palco].id, "spookyMansionErect") == 0) {
        ss->tem_beat = 1;               /* L155-174 `onBeatHit` */
        ss->i_bgl = acha_prop(ss->palco, "bgLight");
        ss->i_stl = acha_prop(ss->palco, "stairsLight");
        if (ss->i_bgl < 0 || ss->i_stl < 0) return;
        ss->ativo = 1;
        ss_reiniciar(ss);
        return;
    }
    /* ---- limoRide (o carro veloz que cruza na frente da limusine) ---- */
    if (strcmp(bs_stage_props[ss->palco].id, "limoRide") == 0) {
        ss->tem_beat = 1;               /* L60-67 `onBeatHit` */
        ss->i_fastcar = acha_prop(ss->palco, "fastCar");
        /* Sem o `fastCar` nao ha' o que mover -- e' dado, nao logica. */
        if (ss->i_fastcar < 0) return;
        ss->ativo = 1;
        /* `ss_reiniciar` faz o `resetFastCar()`: o `buildStage` (L55-56) e o
           `onSongRetry` (L147-151) chamam a mesma funcao, entao o y do carro
           e' sorteado uma vez por reinicio -- nem mais, nem menos. */
        ss_reiniciar(ss);
        return;
    }

    /* Qualquer outro palco fica INATIVO: todas as consultas devolvem "o dado
       manda", entao nenhum palco ganha comportamento por acidente. */
}

static void ss_carro_reset(ss_carro_t *c);

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
    /* A animacao `lightning` do halloweenBG: -1 = nenhum raio ainda, entao o
       prop toca o `startingAnimation` do JSON (o `idle`, que nao repete e
       para no ultimo quadro -- criterio do `sl_prop_frame`). */
    ss->bg_t0 = -1.0f;
    ss->scared_pedido = 0;
    /* O `onCreate` do phillyStreets (L111-155) chama `resetCar(true, true)` e
       `resetStageValues()`: o sinal no verde com intervalo 8, e os dois carros
       parados em (1200, 818) com angulo 0. */
    ss->luz_parado = 0;                  /* L34 `lightsStop = false`  */
    ss->luz_ultimo = 0;                  /* L35 `lastChange = 0`      */
    ss->luz_intervalo = SS_PHILLY_INT_INICIAL;   /* L36 = 8            */
    ss->luz_anim = 0;                    /* L408 `traffic.play('togreen')` */
    ss->luz_t0 = 0.0f;
    ss->car_livre = 1;                   /* L39 `carInterruptable = true` */
    ss->car_esperando = 0;               /* L38 `carWaiting = false`  */
    ss->car2_livre = 1;                  /* L40 `car2Interruptable = true` */
    /* `resetCar` (L85-100): x 1200, y 818, angulo 0, sem tween. */
    ss_carro_reset(&ss->carro[0]);
    ss_carro_reset(&ss->carro[1]);
    /* O tanque: o `onSongRetry` so' chama `moveTank(0)` -- ele NAO sorteia
       angulo nem velocidade de novo (L65-69). Os sorteios ficam no `ss_init`. */
    ss_tank_move(ss, 0.0f);
    ss->strike_beat = 0;                 /* L26: `lightningStrikeBeat = 0` */
    ss->strike_offset = SS_FLASH_OFF_MIN; /* L27: `lightningStrikeOffset = 8` */
    ss->ultimo_beat = -1;
    /* O CARRO VELOZ do limoRide: `buildStage` L55 e `onSongRetry` L147-151
       chamam `resetFastCar()`. */
    if (ss->i_fastcar >= 0) ss_fastcar_reset(ss);

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

void ss_set_song(ss_t *ss, const char *song_id)
{
    if (!ss->ativo || ss->i_freaks < 0) return;
    /* L17: `currentSong.id.toLowerCase() == "roses"` -> `idleSuffix = '-scared'`.
       Nao ha' `toLowerCase` a fazer: todos os ids do port ja' sao minusculos e
       o comparador do original e' sobre o id inteiro. */
    ss->sufixo_medo = (song_id && strcmp(song_id, "roses") == 0) ? 1 : 0;
}

const char *ss_prop_sufixo(const ss_t *ss, int prop_abs)
{
    if (!ss->ativo || ss->i_freaks < 0) return "";
    if (prop_abs != ss->i_freaks || !ss->sufixo_medo) return "";
    return "-scared";
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

/* `moveTank` (tankmanBattlefield.hxc L54-63), com o `FlxAngle.asRadians`. */
static void ss_tank_move(ss_t *ss, float elapsed)
{
    float rad;
    if (ss->i_tank < 0) return;
    ss->tank_ang += elapsed * ss->tank_vel;          /* L57 */
    rad = (ss->tank_ang + 180.0f) * (3.14159265358979f / 180.0f);
    ss->tank_px = SS_TANK_X + (float)cos((double)rad) * SS_TANK_CX;   /* L61 */
    ss->tank_py = SS_TANK_Y0 + (float)sin((double)rad) * SS_TANK_CY;  /* L62 */
}

/* ---- limoRide.hxc: o carro veloz --------------------------------------- */

/* `resetFastCar` (L70-105). Sorteia o y e zera a velocidade; o `active = true`
   do sprite e' o que faz o flixel aplicar a velocidade todo quadro, e aqui e'
   o `i_fastcar >= 0` em `ss_update`. */
static void ss_fastcar_reset(ss_t *ss)
{
    if (ss->i_fastcar < 0) return;
    ss->car_x = SS_CAR_X0;                                  /* L87 */
    ss->car_y = (float)ss_rnd_int(ss, SS_CAR_Y_MIN, SS_CAR_Y_MAX);  /* L88 */
    ss->car_vx = 0.0f;                                      /* L89 */
    ss->car_pode = 1;                                       /* L91 */
}

/* `fastCarDrive` (L97-110). O x anda em `ss_update` com a velocidade -- o
   flixel faz o mesmo em `FlxSprite.update` (`x += velocity.x * elapsed`). */
static void ss_fastcar_dirige(ss_t *ss)
{
    /* L99: `FunkinSound.playOnce(Paths.soundRandom('carPass', 0, 1), 0.7)`.
       O sorteio vem ANTES do da velocidade, na mesma linha -- e' a ordem do
       original e o teste cobra. 1..2 = `carPass1`/`carPass2` do pacote (o
       `carPass0`/`carPass1` do clone de arte). */
    ss->som = 1 + ss_rnd_int(ss, 0, 1);
    ss->som_banco = SS_SOM_CARPASS;
    ss->som_vol = SS_CAR_VOL;
    /* L106: `(int(170, 220) / FlxG.elapsed) * 3`. O `FlxG.elapsed` e' o delta
       do quadro corrente, que chega no `ss_beat` (ver o parametro `elapsed`). Se ele
       for zero (nao acontece no laco do jogo, mas a divisao nao pode estourar)
       o carro sai com velocidade zero em vez de infinito -- e o teste cobra
       esse caso. */
    if (ss->ultimo_dt > 0.0f) {
        float v = (float)ss_rnd_int(ss, SS_CAR_VEL_MIN, SS_CAR_VEL_MAX) / ss->ultimo_dt;
        ss->car_vx = v * SS_CAR_VEL_MULT;
    } else {
        ss->car_vx = 0.0f;
    }
    ss->car_pode = 0;                                       /* L107 */
    /* L109: `carSequence = new Sequence(carSequenceData)` -- comeca no
       primeiro evento (1 s), e o `timer` do flixel reinicia por DIFERENCA entre
       eventos, nao acumula o estouro do quadro. */
    ss->car_seq = 1;
    ss->car_seq_fase = 0;
    ss->car_seq_resta = ss_car_seq_t[0];
}

/* O `carSequence` (L44-48 + `util/Sequence.hx`): cada evento dispara quando o
   timer corrente chega a zero, e o proximo timer vale `t[n] - t[n-1]`. */
static void ss_fastcar_seq(ss_t *ss, float elapsed)
{
    if (!ss->car_seq) return;
    ss->car_seq_resta -= elapsed;
    if (ss->car_seq_resta > 0.0f) return;
    switch (ss->car_seq_fase) {
    case 0: ss->car_haptics = 1; break;      /* L45 `doCarHaptics = true`  */
    case 1: ss->car_haptics = 0; break;      /* L46 `doCarHaptics = false` */
    case 2: ss_fastcar_reset(ss); break;     /* L47 `resetFastCar()`       */
    default: break;
    }
    ss->car_seq_fase += 1;
    if (ss->car_seq_fase >= 3) {
        /* `completed` -> `autoDestroy` (Sequence.hx): a Sequence morre. */
        ss->car_seq = 0;
        ss->car_seq_resta = 0.0f;
        return;
    }
    ss->car_seq_resta = ss_car_seq_t[ss->car_seq_fase] -
                        ss_car_seq_t[ss->car_seq_fase - 1];
}

/* ---- phillyStreets.hxc: as curvas do FlxEase ---------------------------- */
static float ss_ease(int tipo, float u)
{
    if (u <= 0.0f) return 0.0f;
    if (u >= 1.0f) return 1.0f;
    switch (tipo) {
    case SS_EASE_CUBE_OUT: {            /* FlxEase.hx L61-64 */
        float t = u - 1.0f;
        return 1.0f + t * t * t;
    }
    case SS_EASE_SINE_IN:               /* FlxEase.hx L137-140 */
        return 1.0f - (float)cos((double)u * 1.5707963267948966);
    case SS_EASE_SINE_OUT:              /* FlxEase.hx L142-145 */
        return (float)sin((double)u * 1.5707963267948966);
    default:
        return u;                       /* `ease: null` do flixel = linear */
    }
}

/* O ponto da Bezier quadratica e o angulo da `AngleTween`, no instante `t`.
   `FlxTween.tween` do flixel so' comeca a mexer no sprite depois do
   `startDelay`, entao com u <= 0 o carro fica onde estava. */
static void ss_carro_anda(ss_carro_t *c, float t)
{
    float u;
    if (!c->ativo) return;
    u = t - c->t0 - c->delay;
    if (u <= 0.0f) return;              /* ainda no `startDelay`            */
    u /= c->dur > 0.0f ? c->dur : 1.0f;
    if (u > 1.0f) u = 1.0f;
    {
        float e = ss_ease(c->ease_pos, u);
        float m = 1.0f - e;
        c->x = c->p0x * m * m + c->p1x * 2.0f * m * e + c->p2x * e * e;
        c->y = c->p0y * m * m + c->p1y * 2.0f * m * e + c->p2y * e * e;
        /* `sprite.offset` desloca o DESENHO: no flixel o canto sai em
           `x - offset.x` (FlxSprite.hx L1007). O main.c desenha pelo canto, e
           girar em volta do centro do quadro e' o mesmo que girar em volta do
           centro depois do offset, entao o offset entra aqui. */
        c->x -= c->offx;
        c->y -= c->offy;
    }
    c->ang = c->a0 + (c->a1 - c->a0) * ss_ease(c->ease_ang, u);
}

/* `resetCar` (L75-103). */
static void ss_carro_reset(ss_carro_t *c)
{
    c->ativo = 0;
    c->t0 = c->delay = c->dur = 0.0f;
    c->anim = 0;
    c->offx = c->offy = 0.0f;
    c->x = SS_PHILLY_CAR_X;
    c->y = SS_PHILLY_CAR_Y;
    c->ang = 0.0f;
}

static void ss_carro_dirige(ss_t *ss, int canal, int parando);
/* `finishCarLights` (L189-207): o carro que estava no sinal arranca e sai da
   tela. O `carWaiting` do L191 e' a PRIMEIRA linha da funcao. */
static void ss_carro_solta(ss_t *ss)
{
    ss_carro_t *c = &ss->carro[0];
    float dur = ss_rnd_float(ss, 1.8f, 3.0f);       /* L192 */
    float delay = ss_rnd_float(ss, 0.2f, 1.2f);     /* L195 */
    ss->car_esperando = 0;
    c->ativo = 1;
    c->parou = 1;
    c->t0 = ss->t;
    c->delay = delay;
    c->dur = dur;
    c->anim = 0;                /* finishCarLights nao troca a animacao */
    c->ease_pos = SS_EASE_SINE_IN;
    c->ease_ang = SS_EASE_SINE_IN;
    c->p0x = 1950.0f - SS_PHILLY_OFFX - 80.0f;
    c->p0y = 980.0f - SS_PHILLY_OFFY + 15.0f;
    c->p1x = 2400.0f - SS_PHILLY_OFFX;
    c->p1y = 980.0f - SS_PHILLY_OFFY - 50.0f;
    c->p2x = 3102.0f - SS_PHILLY_OFFX;
    c->p2y = 1187.0f - SS_PHILLY_OFFY + 40.0f;
    c->a0 = -5.0f;              /* L193 `rotations = [-5, 18]` */
    c->a1 = 18.0f;
    c->ang = -5.0f;             /* o AngleTween poe o `from` na hora */
}

/* `changeLights` (L53-70). A ORDEM e' a da fonte: `lastChange`, inverte,
   escolhe a animacao/intervalo e, no verde, solta o carro que esperava. */
static void ss_troca_luz(ss_t *ss, int beat)
{
    ss->luz_ultimo = beat;
    ss->luz_parado = !ss->luz_parado;
    ss->luz_t0 = ss->t;                 /* `animation.play` reinicia */
    if (ss->luz_parado) {
        ss->luz_anim = 1;               /* L60 `play('tored')`  */
        ss->luz_intervalo = SS_PHILLY_INT_VERMELHO;
    } else {
        ss->luz_anim = 0;               /* L65 `play('togreen')` */
        ss->luz_intervalo = SS_PHILLY_INT_VERDE;
        /* L68: `if (carWaiting == true) finishCarLights(phillyCars)` */
        if (ss->car_esperando) ss_carro_solta(ss);
    }
}

/* `driveCarLights` (L214-273), `driveCar` (L278-332) e `driveCarBack`
   (L337-395). `modo`: 0 = driveCar (atravessa), 1 = driveCarLights (para no
   sinal), 2 = driveCarBack (o carro da direita, sentido contrario).
   A ORDEM dos sorteios e' a da fonte: a variante, depois a duracao. */
static void ss_carro_dirige(ss_t *ss, int canal, int modo)
{
    ss_carro_t *c = &ss->carro[canal];
    int variante = ss_rnd_int(ss, 1, 4);
    float ox = 0.0f, oy = 0.0f, dur = 2.0f;

    if (canal == 0) ss->car_livre = 0; else ss->car2_livre = 0;
    c->parou = (modo == 1);

    switch (variante) {
    case 1:
        dur = modo == 1 ? ss_rnd_float(ss, 1.0f, 1.7f) : ss_rnd_float(ss, 1.0f, 1.7f);
        break;
    case 2:
        ox = 20.0f; oy = -15.0f;
        dur = modo == 1 ? ss_rnd_float(ss, 0.9f, 1.5f) : ss_rnd_float(ss, 0.6f, 1.2f);
        break;
    case 3:
        ox = 30.0f; oy = 50.0f;
        dur = ss_rnd_float(ss, 1.5f, 2.5f);
        break;
    default:
        ox = 10.0f; oy = 60.0f;
        dur = ss_rnd_float(ss, 1.5f, 2.5f);
        break;
    }

    c->ativo = 1;
    c->t0 = ss->t;
    c->delay = 0.0f;
    c->dur = dur;
    c->anim = variante;
    c->offx = ox;
    c->offy = oy;

    if (modo == 0) {
        /* driveCar: `rotations = [-8, 18]`, `ease: null` nos dois. */
        c->ease_pos = SS_EASE_LINEAR;
        c->ease_ang = SS_EASE_LINEAR;
        c->p0x = 1570.0f - SS_PHILLY_OFFX;
        c->p0y = 1049.0f - SS_PHILLY_OFFY - 30.0f;
        c->p1x = 2400.0f - SS_PHILLY_OFFX;
        c->p1y = 980.0f - SS_PHILLY_OFFY - 50.0f;
        c->p2x = 3102.0f - SS_PHILLY_OFFX;
        c->p2y = 1187.0f - SS_PHILLY_OFFY + 40.0f;
        c->a0 = -8.0f; c->a1 = 18.0f;
    } else if (modo == 1) {
        /* driveCarLights: `rotations = [-7, -5]`, `cubeOut`. */
        c->ease_pos = SS_EASE_CUBE_OUT;
        c->ease_ang = SS_EASE_CUBE_OUT;
        c->p0x = 1500.0f - SS_PHILLY_OFFX - 20.0f;
        c->p0y = 1049.0f - SS_PHILLY_OFFY - 20.0f;
        c->p1x = 1770.0f - SS_PHILLY_OFFX - 80.0f;
        c->p1y = 994.0f - SS_PHILLY_OFFY + 10.0f;
        c->p2x = 1950.0f - SS_PHILLY_OFFX - 80.0f;
        c->p2y = 980.0f - SS_PHILLY_OFFY + 15.0f;
        c->a0 = -7.0f; c->a1 = -5.0f;
    } else {
        /* driveCarBack: `rotations = [18, -8]`, caminho invertido. */
        c->ease_pos = SS_EASE_LINEAR;
        c->ease_ang = SS_EASE_LINEAR;
        c->p0x = 3102.0f - SS_PHILLY_OFFX;
        c->p0y = 1127.0f - SS_PHILLY_OFFY + 60.0f;
        c->p1x = 2400.0f - SS_PHILLY_OFFX;
        c->p1y = 980.0f - SS_PHILLY_OFFY - 30.0f;
        c->p2x = 1570.0f - SS_PHILLY_OFFX;
        c->p2y = 1049.0f - SS_PHILLY_OFFY - 10.0f;
        c->a0 = 18.0f; c->a1 = -8.0f;
    }
    c->ang = c->a0;                     /* o AngleTween poe o `from` na hora */
}

/* `doLightningStrike` (spookyMansion.hxc L26-49), na ordem da fonte: som,
   animacao, `strikeBeat`, `strikeOffset` sorteado, e os `scared` do bf e da gf.
   `pede_som` = 0 no raio sincronizado da `spookeez` (L82 `false`). */
static void ss_mansao_raio(ss_t *ss, int beat, int pede_som)
{
    if (pede_som) {
        /* L30: `Paths.soundRandom('thunder_', 1, 2)` */
        ss->som = 1 + ss_rnd_int(ss, 0, 1);
        ss->som_banco = SS_SOM_TROVAO;
    }
    /* L33: `halloweenBG.animation.play('lightning')` -- reinicia do primeiro
       quadro da animacao a cada raio. */
    ss->bg_t0 = ss->t;
    ss->strike_beat = beat;              /* L35 */
    ss->strike_offset = ss_rnd_int(ss, SS_FLASH_OFF_MIN, SS_FLASH_OFF_MAX);  /* L36 */
    /* L38-46: os dois `playAnimation('scared', true, true)`. Quem decide se o
       personagem TEM a animacao e' o main.c (`ch_anim_i`), que e' o
       `hasAnimation()` do original -- aqui so' o pedido, como o `hair_pedido`
       do trem. */
    ss->scared_pedido = 1;
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
    ss->scared_pedido = 1;      /* L38-46 `playAnimation('scared')` no bf e na gf */
}

static void ss_trem_comeca(ss_t *ss);
static void ss_trem_anda(ss_t *ss);
static void ss_mansao_raio(ss_t *ss, int beat, int pede_som);

/* O `onBeatHit` (L155-174). A ordem dos sorteios e' a da fonte:
   1) `FlxG.random.bool(10)` -- sorteado em TODO beat, mesmo quando o segundo
      `if` nem chega a disparar (curto-circuito do `&&`);
   2) o `int(1,2)` do som, dentro do strike com som;
   3) o `int(8,24)` do offset.
   O primeiro `if` (beat 4 na spookeez) NAO sorteia som. */
int ss_beat(ss_t *ss, int beat, const char *song_id, int tem_personagens,
            float elapsed)
{
    int chance, disparou = 0;
    if (!ss->ativo) return 0;
    /* O `FlxG.elapsed` do quadro: `ss_beat` roda no lugar do `onBeatHit`, que
       o flixel chama ANTES do `onUpdate` do palco, entao o delta chega por
       parametro em vez de vir do `ss_update`. */
    ss->ultimo_dt = elapsed < 0.0f ? 0.0f : elapsed;
    /* Cada beat chega UMA vez; quem chama passa o beat do chart, que pode
       repetir em varios quadros. */
    if (beat == ss->ultimo_beat) return 0;
    ss->ultimo_beat = beat;

    /* ---- phillyStreets L486-511 ---- */
    if (ss->i_luz >= 0) {
        int r10;
        /* L493-494: `bool(10) && beat != lastChange + changeInterval &&
           carInterruptable`. O `bool` e' o PRIMEIRO operando, entao ele e'
           sorteado em todo beat, mesmo quando o resto e' falso. */
        r10 = ss_rnd_bool(ss, SS_PHILLY_CAR_CHANCE);
        if (r10 && beat != ss->luz_ultimo + ss->luz_intervalo && ss->car_livre) {
            ss_carro_dirige(ss, 0, ss->luz_parado ? 1 : 0);
            disparou = 1;
        }
        /* L506-508: o carro da direita, so' no verde. */
        r10 = ss_rnd_bool(ss, SS_PHILLY_CAR_CHANCE);
        if (r10 && beat != ss->luz_ultimo + ss->luz_intervalo &&
            ss->car2_livre && !ss->luz_parado) {
            ss_carro_dirige(ss, 1, 2);
            disparou = 1;
        }
        /* L511: a troca do sinal. */
        if (beat == ss->luz_ultimo + ss->luz_intervalo) {
            ss_troca_luz(ss, beat);
            disparou = 1;
        }
        return disparou;
    }

    /* ---- limoRide L60-67 ---- */
    if (ss->i_fastcar >= 0) {
        /* `FlxG.random.bool(10) && fastCarCanDrive`: o sorteio e' o primeiro
           operando e acontece em TODO beat, mesmo com o carro ja' na rua. */
        int r10 = ss_rnd_bool(ss, SS_CAR_CHANCE);
        if (r10 && ss->car_pode) {
            ss_fastcar_dirige(ss);
            disparou = 1;
        }
        return disparou;
    }

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

    /* ---- spookyMansion L72-91 (a versao NAO-erect, a que tres musicas
       jogam: spookeez, south, monster) ---- */
    if (ss->i_bg >= 0) {
        /* L80-83: `beat == 4` na `spookeez` -> strike SEM som. Este `if` nao
           sorteia nada alem do offset, e nao depende dos personagens. */
        if (beat == SS_FLASH_BEAT_ESP && song_id && strcmp(song_id, "spookeez") == 0) {
            ss_mansao_raio(ss, beat, 0);
            disparou = 1;
        }
        /* L87-90: `FlxG.random.bool(10) && beat > strikeBeat + strikeOffset`.
           O sorteio acontece SEMPRE (primeiro operando do `&&`). */
        chance = ss_rnd_bool(ss, SS_FLASH_CHANCE);
        if (chance && beat > ss->strike_beat + ss->strike_offset) {
            ss_mansao_raio(ss, beat, 1);
            disparou = 1;
        }
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

    /* ---- O CARRO VELOZ DO limoRide (L60-67 + L97-110 + a Sequence) ---- */
    if (ss->i_fastcar >= 0) {
        /* O `FlxSprite.update` do flixel: `x += velocity.x * elapsed` -- so'
           com o sprite `active`, que no original e' o `resetFastCar` (L86). */
        if (ss->car_vx != 0.0f) ss->car_x += ss->car_vx * elapsed;
        ss_fastcar_seq(ss, elapsed);
    }

    /* ---- O TANQUE (L38-42: `onUpdate` chama `moveTank(elapsed)` sempre) ---- */
    if (ss->i_tank >= 0) ss_tank_move(ss, elapsed);

    /* ---- OS DOIS CARROS DO phillyStreets ---- */
    if (ss->i_luz >= 0) {
        int k;
        for (k = 0; k < 2; k++) {
            ss_carro_t *c = &ss->carro[k];
            if (!c->ativo) continue;
            ss_carro_anda(c, ss->t);
            /* O `onComplete` do tween: o carro volta a ser "interruptavel" e,
               no `driveCarLights`, avisa que esta' esperando no sinal (L253-257
               e L200-208). */
            if (ss->t - c->t0 - c->delay >= c->dur) {
                c->ativo = 0;
                if (k == 0) {
                    ss->car_livre = 1;
                    if (c->parou) {
                        /* L253-257: `carWaiting = true; if (lightsStop == false)
                           finishCarLights(...)`. */
                        ss->car_esperando = 1;
                        if (!ss->luz_parado) ss_carro_solta(ss);
                    }
                } else {
                    ss->car2_livre = 1;
                }
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
    /* Os carros do phillyStreets: o x vem do tween (e o `offset` da variante
       ja' esta' embutido em `carro[].x`, como no desenho do flixel). */
    if (prop_abs == ss->i_car) return ss->carro[0].x;
    if (prop_abs == ss->i_car2) return ss->carro[1].x;
    if (prop_abs == ss->i_tank) return ss->tank_px;
    /* O carro veloz: `x` do `resetFastCar` (L87) mais `velocity.x * elapsed`
       (o `FlxSprite.update`), escrito em `ss_update`. */
    if (prop_abs == ss->i_fastcar) return ss->car_x;
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
    /* spookyMansion: a `lightning` do `halloweenBG`. Ela NAO repete (o JSON diz
       `looped: false` e o flixel para no ultimo quadro), entao o quadro e' o
       tempo desde o raio, travado no ultimo -- e o proximo raio reinicia. */
    if (ss->ativo && prop_abs == ss->i_bg && ss->bg_t0 >= 0.0f) {
        float dt = ss->t - ss->bg_t0;
        if (dt < 0.0f) dt = 0.0f;
        q = (int)(dt * ss->bg_fps);
        if (q >= ss->bg_quadros) q = ss->bg_quadros - 1;
        if (anim) *anim = ss->bg_anim;
        if (frame) *frame = q;
        return 1;
    }
    /* phillyStreets: a animacao `car1..car4` de cada carro e a `togreen`/
       `tored` do sinal. As duas rodam em loop a 24 fps desde o instante em que
       foram tocadas (`animation.play` reinicia do quadro 0). */
    if (ss->ativo && (prop_abs == ss->i_car || prop_abs == ss->i_car2 ||
                      prop_abs == ss->i_luz)) {
        const bs_prop_t *p = &bs_props[prop_abs];
        int quais = -1, q, qn, fps = 24;
        float t0;
        if (prop_abs == ss->i_luz) {
            quais = ss->luz_anim;               /* 0 = togreen, 1 = tored */
            t0 = ss->luz_t0;
        } else {
            const ss_carro_t *c = &ss->carro[prop_abs == ss->i_car ? 0 : 1];
            if (!c->ativo && c->anim == 0) return 0;   /* parado no dado */
            quais = c->anim > 0 ? c->anim - 1 : 0;     /* `car<N>` -> N-1 */
            t0 = c->t0;
        }
        if (quais < 0 || quais >= p->anim_count) return 0;
        {
            const bs_prop_anim_t *a = &bs_prop_anims[p->anim_first + quais];
            qn = a->count > 0 ? a->count : 1;
            if (a->fps > 0.0f) fps = (int)(a->fps + 0.5f);
        }
        q = (int)((ss->t - t0) * (float)fps);
        if (q < 0) q = 0;
        q %= qn;
        if (anim) *anim = quais;
        if (frame) *frame = q;
        return 1;
    }

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

float ss_prop_y(const ss_t *ss, int prop_abs, float y_dado)
{
    if (!ss->ativo || prop_abs < 0) return y_dado;
    if (prop_abs == ss->i_car) return ss->carro[0].y;
    if (prop_abs == ss->i_car2) return ss->carro[1].y;
    if (prop_abs == ss->i_tank) return ss->tank_py;
    /* O y do carro veloz e' SORTEADO no `resetFastCar` (L88 `int(350,380)`), e
       nao o 160 do JSON: sem este override o carro passaria na altura errada. */
    if (prop_abs == ss->i_fastcar) return ss->car_y;
    return y_dado;
}

int ss_prop_ang(const ss_t *ss, int prop_abs, float *graus)
{
    if (!ss->ativo || prop_abs < 0 || !graus) return 0;
    if (prop_abs == ss->i_car) { *graus = ss->carro[0].ang; return 1; }
    if (prop_abs == ss->i_car2) { *graus = ss->carro[1].ang; return 1; }
    if (prop_abs == ss->i_tank) { *graus = ss->tank_ang + SS_TANK_GRAUS; return 1; }
    return 0;
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
