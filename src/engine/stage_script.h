/* O SCRIPT DO PALCO, PORTADO A MAO: os eventos que o `.hxc` do palco dispara
 * durante a musica.
 *
 * POR QUE ISTO EXISTE
 * Nenhum JSON de palco tem evento. O que muda o palco em jogo esta' no HScript
 * do palco (`assets/preload/scripts/stages/<palco>.hxc`), e o port nao
 * interpreta HScript: o que ele faz e' transcrever as linhas, com a fonte
 * citada, em codigo C com teste. O primeiro caso e' o `phillyBlazin`:
 *
 *   phillyBlazin.hxc L120-133   lightningTimer -= elapsed (ou = 1 quando
 *                               `lightningActive` e' falso) e, quando chega a
 *                               zero, applyLightning() + timer = random(7,15)
 *   L142-190                    applyLightning(): acende `skyAdditive` (0,7),
 *                               `foregroundMultiply` (0,64) e
 *                               `additionalLighten` (0,3) e apaga os tres em
 *                               1,5 s / 1,5 s / 0,3 s; toca a animacao `strike`
 *                               do `lightning`, sorteia o x dele e escurece os
 *                               personagens de 0x606060 para 0xDEDEDE/0x888888
 *                               em 0,3 s; toca `Lightning1..3`
 *   L230-240                    cleanupLightning(): esconde os quatro
 *   L114-117                    cores de REPOUSO dos personagens no palco:
 *                               bf e dad 0xDEDEDE, gf 0x888888
 *   L223-228                    onSongEnd: `lightningActive = false`
 *
 * O segundo e' o `spookyMansionErect` (o flash das duas luzes e o `scared` do
 * bf/gf), o terceiro e' o `phillyTrain`:
 *
 *   phillyTrain.hxc L100-110    o trem anda 400 px a cada 1/24 s, mas so'
 *                               depois de 4,7 s do som `train_passes` (L121)
 *   L127-134                    em -2000 volta para -1150 (um vagao), em
 *                               -4000 com a fila acabando -> trainReset()
 *   L115-121                    `beat % 8 == 4` + `bool(30)` + parado +
 *                               `trainCooldown > 8` -> cooldown int(-4,0)
 *   L142-149                    a cada 4 batidas as `lights` trocam de cor
 *                               (as cinco do L28-34)
 *   L123 / L150                 a gf toca `hairBlow` / `hairFall`
 *   L169-176 / L190-203         fim da musica para o trem; retry reseta
 *
 * O quarto e' o `spookyMansion` (SEM o `Erect`), que e' OUTRO arquivo e o que
 * as tres musicas da semana 2 realmente jogam -- `spookeez`, `south` e
 * `monster` usam este palco, nao o erect:
 *
 *   spookyMansion.hxc L23-24    `lightningStrikeBeat = 0`, offset = 8
 *   L30                         `Paths.soundRandom('thunder_', 1, 2)`
 *   L33                         `halloweenBG.animation.play('lightning')`
 *                               (30 quadros a 24 fps, `looped: false`)
 *   L35-36                      `strikeBeat = beat`, offset = int(8,24)
 *   L38-46                      bf/gf `playAnimation('scared')` (o bf nao se
 *                               estiver em `cheer`)
 *   L80-83 / L87-90             o raio da `beat == 4` da `spookeez`, sem som,
 *                               e o `bool(10)` com `beat > strike+offset`
 *
 * Diferenca que o codigo respeita: o NAO-erect nao tem a guarda
 * `if (bf == null ...) return` (que e' do erect) -- aqui o ceu anima e o trovao
 * toca mesmo sem os tres personagens, entao este ramo NAO usa
 * `tem_personagens`.
 *
 * O QUE NAO E' PORTADO, E POR QUE
 * - Os shaders: o `RuntimeRainShader` do spooky (L26, L38-42, L102-104,
 *   L136-140) e o `BuildingEffectShader` das `lights` do phillyTrain (L39-40,
 *   L57, L144). O GS de pipeline fixa nao tem fragment shader. Sao 175 das 451
 *   linhas dos 18 scripts; fica declarado como falta, nao como feito. O que da'
 *   para fazer sem shader -- a COR das luzes por batida -- esta' portado.
 * - `HapticUtil.vibrate` (spooky L194-221, trem L152-158): o controle do PS2
 *   nao tem rumble acionado pelo jogo neste port.
 *
 * O SORTEIO
 * O original usa `FlxG.random` (semente do relogio). Aqui o gerador e' o
 * xorshift do port, com semente fixa: a SEQUENCIA nao e' a do original (nao ha'
 * como ser), mas os INTERVALOS, as probabilidades e a ORDEM dos sorteios sao as
 * da fonte -- e' o que o teste em `test/test_stage_script.c` cobra.
 */
#ifndef FNF_STAGE_SCRIPT_H
#define FNF_STAGE_SCRIPT_H

#ifdef __cplusplus
extern "C" {
#endif

/* Quantas cores/durations/intensidades o `sserafimPulseLights` guarda. O dado
   real tem 2 (spaghetti), e o evento nao tem limite: 4 e' o teto do port, e um
   evento com mais e' RECUSADO (contado), nunca truncado em silencio. */
#define SS_SSF_MAX 4

/* Papeis de personagem, na ordem de `char_layout.h` (CH_PAPEL_*). */
#define SS_BF  0
#define SS_DAD 1
#define SS_GF  2

/* UM CARRO DO `phillyStreets` em movimento: o estado de dois tweens do flixel
   rodando no mesmo sprite (a posicao pela curva de Bezier e o angulo), mais a
   animacao `car1..car4` e o `offset` da variante.
   `u` e' a fracao de tempo JA com o ease aplicado; com `delay > 0` o carro fica
   parado onde estava ate' o tween comecar (`startDelay` do FlxTween). */
typedef struct {
    int ativo;          /* 1 = um tween esta' rodando neste carro           */
    float t0;           /* instante do gatilho (t do script)                */
    float delay;        /* `startDelay` (so' o `finishCarLights` usa)       */
    float dur;
    int ease_pos;       /* SS_EASE_* da curva                               */
    int ease_ang;       /* SS_EASE_* do angulo                              */
    float p0x, p0y, p1x, p1y, p2x, p2y;   /* a Bezier quadratica           */
    float a0, a1;       /* angulo inicial e final                           */
    int anim;           /* 1..4 = `car<N>`                                  */
    int parou;          /* 1 = este tween foi o `driveCarLights` (parou no sinal) */
    float offx, offy;   /* `sprite.offset` da variante                      */
    float x, y, ang;    /* estado corrente (x/y JA com o offset aplicado)   */
} ss_carro_t;

/* Os props que o script do phillyBlazin mexe. -1 quando o palco nao os tem. */
typedef struct {
    int ativo;          /* 1 = este palco tem script portado */
    /* 1 = o script deste palco tem `onBeatHit` portado. E' o portao do
       `ss_beat`: sem ele o main.c teria de listar os palcos um a um, e foi
       assim que o `phillyStreets` ficou uma build inteira sem sinal e sem
       carros -- nenhum dos indices antigos (`i_bgl`/`i_bg`/`i_train`) e' dele. */
    int tem_beat;
    int palco;          /* indice em bs_stage_props[] */
    int i_sky;          /* `skyAdditive`, indice ABSOLUTO em bs_props[] */
    int i_light;        /* `lightning` (textura com a animacao `strike`) */
    int i_fg;           /* `foregroundMultiply` */
    int i_lighten;      /* `additionalLighten`, indice em bs_cores[] */

    /* Tempo de musica desde o inicio (o `FlxG.elapsed` somado). */
    float t;

    /* --- o timer do relampago (L93, L120-133) --- */
    float timer;        /* lightningTimer */
    int ligado;         /* lightningActive */
    int raios;          /* quantos raios ja' cairam (telemetria/teste) */

    /* --- o raio corrente --- */
    int raio_ativo;     /* 1 enquanto os 4 props estao visiveis */
    float raio_t0;      /* instante do ultimo applyLightning() */
    float raio_x;       /* o x sorteado para o prop `lightning` */

    /* --- alpha dos tres props de luz, ja' interpolado --- */
    float a_sky, a_fg, a_lighten;

    /* --- cor dos personagens (0..255, como o `color` do flixel) ---
       SO' o `phillyBlazin` mexe nisso (L114-117 e L180-184). `cor_ligada` diz
       exatamente isso: nos outros palcos com script a cor fica DESLIGADA,
       porque um `ss_cor_papel` devolvendo 1 com a cor de repouso do blazin
       pintaria os personagens do trem e da mansao com um tom que nenhuma linha
       do `.hxc` deles escreve. */
    int cor_ligada;             /* 1 = este palco tem cor de personagem */
    unsigned char cor[3][3];    /* [papel][r,g,b] */
    float cor_t0;               /* instante do ultimo escurecimento */
    int cor_ativa;

    /* --- som: 1..3 = `Lightning<N>` pendente de tocar, 0 = nada ---
       No `spookyMansionErect` 1..2 = `thunder_<N>`. */
    int som;

    /* ---- O FLASH DO `spookyMansionErect` ----
       phillyBlazin e' relampago de ceu; a mansao e' flash de INTERIOR: as duas
       camadas de luz acendem, os personagens somem, e a sequencia de 0,06 s /
       0,12 s (L51-79) inverte isso duas vezes antes dos tweens de 1,5 s. */
    int i_bgl;          /* `bgLight`, indice ABSOLUTO em bs_props[] */
    int i_stl;          /* `stairsLight` */
    int flash;          /* 1 enquanto a sequencia/tween do flash roda */
    int flash_fase;     /* 0 = nada aplicado, 1 = +0,06 s, 2 = +0,12 s */
    float flash_t0;
    float a_bgl, a_stl; /* alpha das duas luzes */
    float a_char[3];    /* alpha dos personagens (1 = normal) */

    /* ---- O CEU DA MANSao (`spookyMansion.hxc`, o palco de spookeez/south/
       monster): o raio nao acende luz nenhuma, ele toca a ANIMACAO `lightning`
       do prop `halloweenBG` (30 quadros a 24 fps, uma vez) e pede `scared` no
       bf e na gf. */
    int i_bg;           /* prop `halloweenBG` (ABS em bs_props[]), -1 = sem  */
    int bg_anim;        /* indice da animacao `lightning` DENTRO do prop       */
    int bg_quadros;     /* quantos quadros ela tem (do pacote)                 */
    float bg_fps;       /* fps dela (do JSON/pacote)                          */
    float bg_t0;        /* instante do ultimo raio; < 0 = nenhum ainda         */
    int scared_pedido;  /* 1 = bf/gf devem tocar `scared`; o main.c consome    */

    /* ---- O SINAL E OS DOIS CARROS DO `phillyStreets` (darnell, lit-up,
       2hot) -- phillyStreets.hxc L34-70, L189-260, L275-395 ----
       O script e' um mini-tween: cada carro recebe uma curva de Bezier
       quadratica (`FlxTween.quadPath`), um angulo (`FlxTween.angle`) e uma
       animacao `car1..car4`, com a duracao e o `offset` sorteados por variante.
       O sinal troca a cada `changeInterval` batidas e o carro que estava
       esperando sai quando abre o verde. */
    int i_luz;          /* prop `phillyTraffic` (ABS em bs_props[]), -1 = sem */
    int i_car;          /* prop `phillyCars`                                 */
    int i_car2;         /* prop `phillyCars2`                                */
    int luz_parado;     /* lightsStop (L34)                                  */
    int luz_ultimo;     /* lastChange (L35)                                  */
    int luz_intervalo;  /* changeInterval (L36): 8 / 20 / 30                 */
    int luz_anim;       /* 0 = `togreen`, 1 = `tored`                        */
    float luz_t0;       /* instante do ultimo `animation.play` do sinal      */
    int car_livre;      /* carInterruptable (L39)                            */
    int car_esperando;  /* carWaiting (L38)                                  */
    int car2_livre;     /* car2Interruptable (L40)                           */
    ss_carro_t carro[2];/* [0] = `phillyCars`, [1] = `phillyCars2`           */

    /* ---- O TANQUE DO `tankmanBattlefield` (ugh, stress e guns) ----
       tankmanBattlefield.hxc L20-36 `buildStage`: cria as nuvens (sprite do
       script, ver a falta declarada) e sorteia `tankAngle = int(-90,45)` e
       `tankSpeed = float(5,7)`; L54-63 `moveTank(elapsed)`: o angulo anda
       `elapsed * tankSpeed` por quadro, o sprite vai para
       (400 + cos(rad(ang+180))*1500, 1300 + sin(rad(ang+180))*1100) e o
       `angle` do sprite e' `tankAngle - 90 + 15`; L65-69 `onSongRetry` chama
       `moveTank(0)`, que so' reaplica a posicao do angulo atual. */
    int i_tank;         /* prop `tankRolling` (ABS em bs_props[]), -1 = sem */
    float tank_ang;     /* tankAngle                                    */
    float tank_vel;     /* tankSpeed                                    */
    float tank_px, tank_py;   /* a posicao corrente (L61-62)             */

    /* ---- O `school` (senpai e roses): o `idleSuffix` do `freaks` ----
       school.hxc L15-24 `buildStage`: se a musica e' `roses`, o prop `freaks`
       ganha `idleSuffix = '-scared'` -- o `Bopper.dance()` do flixel monta
       `danceLeft`/`danceRight` + o sufixo, e o JSON do palco tem as QUATRO
       animacoes (`danceLeft-scared` e `danceRight-scared` sao as 3a e a 4a).
       O port nao toca o `.hxc` no `buildStage`: quem diz a musica e' o
       `ss_set_song`. */
    int i_freaks;       /* prop `freaks` (ABS em bs_props[]), -1 = sem  */
    int sufixo_medo;    /* 1 = a musica e' `roses` -> o sufixo `-scared`  */

    /* --- o `onBeatHit` do spooky: `lightningStrikeBeat/Offset` (L23-24) --- */
    int strike_beat;
    int strike_offset;
    int ultimo_beat;    /* o ultimo beat que chegou (dispara uma vez por beat) */

    /* ---- O TREM DO `phillyTrain` ----
       phillyTrain.hxc L46-176: o `train` passa no fundo a cada 8 batidas
       (`beat % 8 == 4` + `bool(30)` + `trainCooldown > 8`), anda 400 px a cada
       1/24 s DEPOIS que o som `train_passes` chega a 4,7 s (`trainSound.time`),
       embala em -2000 (volta para -1150 e gasta um vagao) e some em -4000
       (`trainReset`: x = FlxG.width+200 = 1480, 8 vagoes de novo); a gf toca
       `hairBlow` quando o trem comeca a andar e `hairFall` no reset, e as
       `lights` trocam de cor a cada 4 batidas (`lightColors[curLight]`). */
    int i_train;        /* prop `train`    (indice ABSOLUTO em bs_props[]) */
    int i_lights;       /* prop `lights`   (idem, -1 se o palco nao tem)    */
    int trem_ligado;    /* trainEnabled                                     */
    int trem_movendo;   /* trainMoving                                      */
    int trem_som_ligado;/* o `trainSound` esta' tocando (o relogio dele)     */
    int trem_carros;    /* trainCars                                        */
    int trem_acabando;  /* trainFinishing                                   */
    int trem_comecou;   /* startedMoving                                    */
    int trem_cooldown;  /* trainCooldown                                    */
    float trem_x;       /* x do trem em unidades do original; < 0 = o dado  */
    float trem_t;       /* `trainSound.time` em segundos                    */
    float trem_quadro;  /* trainFrameTiming (acumulador de 1/24 s)          */
    int luz_cor;        /* curLight; -1 = nenhuma cor aplicada ainda        */
    int hair_pedido;    /* 1 = `hairBlow`, 2 = `hairFall`; o main.c consome */

    /* ---- O CARRO VELOZ DO `limoRide` (satin-panties, high, milf) ----
       limoRide.hxc L33-56 `buildStage` cria o `carSequence` e chama
       `resetFastCar()` (x -12600, y int(350,380), velocidade 0,
       `fastCarCanDrive = true`); L60-67 `onBeatHit`: `bool(10) &&
       fastCarCanDrive` -> `fastCarDrive()` (L97-110): toca
       `soundRandom('carPass', 0, 1)` a 0,7 e poe
       `velocity.x = int(170,220) / FlxG.elapsed * 3` -- pixels por segundo
       calculados com o delta do QUADRO, o que da' uns 36 000 px/s (o carro
       cruza a tela em ~20 quadros, de proposito: e' o "carro veloz" que passa
       borrado na frente da limusine). A `Sequence` de L44-48 (tempos 1, 1,4 e
       2 s) liga/desliga o `doCarHaptics` e, em 2 s, chama `resetFastCar`.
       O `onPause`/`onResume` (L152-165) pausa/retoma a Sequence -- no port isso
       sai de graca: `ss_update` so' anda com o tempo da MUSICA, e a musica
       parada no pause nao avanca. */
    int i_fastcar;      /* prop `fastCar` (ABS em bs_props[]), -1 = sem      */
    float car_x, car_y; /* posicao corrente (L87-89 e o `velocity.x` do L106) */
    float car_vx;       /* `fastCar.velocity.x`                              */
    int car_pode;       /* `fastCarCanDrive` (L91)                           */
    int car_seq;        /* `carSequence != null` e ainda nao terminou        */
    int car_seq_fase;   /* quantos eventos da Sequence ja' dispararam       */
    float car_seq_resta;/* o que falta do `FlxTimer` corrente (segundos)     */
    int car_haptics;    /* `doCarHaptics` -- so' o vibra do controle, sem
                           efeito visivel; guardado para o teste            */

    /* ---- sserafim (spaghetti): as luzes do caminhao, a luz de fundo e a
       cobertura preta (sserafim.hxc L284-287, L422-500) ----
       Os quatro props ja' existem no palco, TODOS com alpha 0 no JSON: o script
       os acende com `FlxTween` de alpha e os apaga de volta em 0.
         flashTruckLights(amount, duration)  -> truckLight1/2 de `amount` a 0,
                                                `FlxEase.cubeInOut`
         flashBackLight(amount, dur, color)  -> `backLightColor.color` = color,
                                                alpha `amount*0,8`, e
                                                `backLightWhite` `amount*0,7`,
                                                os dois a 0 em `duration`
         setCoverVisible(v)                  -> `solidCover.alpha` = v ? 1 : 0
         setLightState(enabled, ...)         -> em TODO beat, enquanto ligado,
                                                `flashBackLight` com
                                                cores/dur/intensidades ciclados
                                                por `beat % length` */
    int i_ssf_tl1, i_ssf_tl2;   /* `truckLight1`, `truckLight2`            */
    int i_ssf_blc, i_ssf_blw;   /* `backLightColor`, `backLightWhite`      */
    int i_ssf_cob;              /* `solidCover`                           */
    int ssf_ativo;              /* 1 = o script do sserafim esta' instalado */
    struct { int ativo; float a0, a1, t0, dur; } ssf_tw[4];  /* 0..3 na ordem
                                   tl1, tl2, blc, blw */
    float ssf_a[4];             /* alpha corrente de cada alvo             */
    unsigned char ssf_cor[3];   /* a cor do `backLightColor.color`         */
    int ssf_cor_ligada;
    int ssf_luz;                /* lightsEnabled                           */
    int ssf_n;                  /* quantas entradas o pulse tem (0 = nenhum) */
    unsigned int ssf_cores[SS_SSF_MAX];
    float ssf_durs[SS_SSF_MAX], ssf_ints[SS_SSF_MAX];
    int ssf_cob_visivel;        /* `solidCover.alpha` = 1 enquanto 1        */
    int ssf_flash_ms;           /* `camGame.flash(0xFFFFFFFF, ms)` pedido   */
    float ssf_flash_t0;         /* instante em que o flash foi pedido       */
    int ssf_fim;                /* `endStuff` chegou (L680)                 */
    int ssf_kick_pedido;        /* `sserafimKick`: 1 = ha' um pedido novo    */
    int ssf_kick_final;         /* o `final` do evento (kick1 ou kick2)     */
    unsigned int ssf_canta;     /* mascara de `sserafimSing`               */
    unsigned int ssf_mostra;    /* mascara de `sserafimShow`               */

    /* O delta do QUADRO corrente, em segundos: e' o `FlxG.elapsed` que o
       `fastCarDrive` (limoRide L106) usa como divisor da velocidade. Quem
       escreve e' o proprio `ss_beat` (recebe o delta junto com o beat, porque
       o `onBeatHit` do flixel acontece ANTES do `onUpdate` do palco). */
    float ultimo_dt;

    /* O BANCO do som pendente em `som`: ver SS_SOM_*.
       `som_vol` e' o volume em % (100 = cheio): o `carPass` do limoRide toca a
       0,7 (L99), o resto a 1,0. */
    int som_banco;
    int som_vol;

    unsigned int rng;
} ss_t;

/* Liga o script do palco `palco_id` (o id do palco, ex. "phillyBlazin").
 * Palco sem script portado fica com `ativo = 0` e todas as consultas devolvem
 * "o dado manda". */
void ss_init(ss_t *ss, const char *palco_id);

/* Volta ao estado de `onCreate` -- usado no retry da musica (L52-58:
 * `lightningActive = true`). */
void ss_reiniciar(ss_t *ss);

/* Avanca `elapsed` segundos. Devolve 1 se um raio caiu NESTE passo (o main.c
 * usa isso para a telemetria; a logica nao depende da resposta). */
int ss_update(ss_t *ss, float elapsed);

/* O fim da musica (L223-228): para de sortear raios. */
void ss_song_end(ss_t *ss);

/* ---- CONSULTAS DE DESENHO (todas devolvem "sem override" quando o prop nao
 *      e' do script, quando o script nao esta' ativo, ou quando o prop esta'
 *      invisivel) --------------------------------------------------------- */

/* Alpha do prop `prop_abs` (indice absoluto em bs_props[]), ou -1 = usa o do
 * dado. 0 = nao desenha. */
float ss_prop_alpha(const ss_t *ss, int prop_abs);

/* X do prop em unidades do ORIGINAL (o mesmo espaco do JSON), ou `x_dado`
 * quando o script nao mexe na posicao dele. */
float ss_prop_x(const ss_t *ss, int prop_abs, float x_dado);

/* Quadro/animacao do prop. Devolve 1 e escreve `*anim`/`*frame` quando o
 * script manda (o `lightning` toca `strike` do quadro 0 a 24 fps), 0 quando o
 * layout deve escolher pelo dado. */
int ss_prop_frame(const ss_t *ss, int prop_abs, int *anim, int *frame);

/* Alpha do retangulo de cor `cor_abs` (indice em bs_cores[]), ou -1. */
float ss_cor_alpha(const ss_t *ss, int cor_abs);

/* ---- O `onBeatHit` (spookyMansionErect L155-174) ----
 * Chamado uma vez por beat, na ordem da fonte. `song_id` e' o id da musica (o
 * `beat == 4` so' vale na `spookeez`) e `tem_personagens` diz se os tres
 * personagens estao em cena -- o script do original da' `return` sem eles.
 * `elapsed` e' o delta do quadro corrente (o `FlxG.elapsed` do flixel, que o
 * `fastCarDrive` do limoRide divide em L106).
 * Devolve 1 quando um evento comecou (para a telemetria). */
int ss_beat(ss_t *ss, int beat, const char *song_id, int tem_personagens,
            float elapsed);

/* Alpha do FLASH de tela do sserafim (`camGame.flash(0xFFFFFFFF, duration)`,
 * L279-282): 1 no instante do evento e caindo em linha reta ate' 0 no fim da
 * duracao (o `FlxCamera.flash` do flixel usa `_fxFlashDuration` e um decaimento
 * linear). -1 = nao ha' flash agora. */
float ss_ssf_flash_alpha(const ss_t *ss);

/* Alpha da COBERTURA preta do sserafim (`solidCover`, L284-287 e o `endStuff`
 * L686), ou -1 quando o palco nao tem cobertura. 1 = a tela inteira coberta. */
float ss_ssf_cob_alpha(const ss_t *ss);

/* Alpha do personagem do papel `papel` (SS_BF/SS_DAD/SS_GF), ou -1 quando o
 * palco nao mexe no alpha dos personagens (o desenho sai como sempre). */
float ss_alpha_papel(const ss_t *ss, int papel);

/* Cor do personagem do papel `papel` (SS_BF/SS_DAD/SS_GF), 3 bytes 0..255.
 * Devolve 0 quando o palco nao tem cor de personagem (o desenho sai com o
 * branco de sempre). */
int ss_cor_papel(const ss_t *ss, int papel, unsigned char *rgb);

/* Cor do prop `prop_abs` (3 bytes 0..255), ou 0 quando o script nao pinta esse
 * prop. E' o `lightWindow.color = lightColors[curLight]` do `phillyTrain`
 * (L146-149): o GS multiplica a textura pela cor do vertice, entao um `.color`
 * do flixel cabe sem shader. */
int ss_prop_cor(const ss_t *ss, int prop_abs, unsigned char *rgb);

/* Y do prop em unidades do ORIGINAL, ou `y_dado`. E' o `sprite.y` do flixel:
   os carros do `phillyStreets` andam numa curva (`FlxTween.quadPath`) e o
   tanque do `tankmanBattlefield` num circulo (L62). */
float ss_prop_y(const ss_t *ss, int prop_abs, float y_dado);

/* Diz ao script QUAL e' a musica (o `onCreate`/`buildStage` do original le
 * `PlayState.instance.currentSong.id`). So' o `school` usa isso hoje, e o
 * resultado e' o `idleSuffix` do `freaks` (school.hxc L15-24). */
void ss_set_song(ss_t *ss, const char *song_id);

/* O sufixo que o script poe nos nomes das animacoes deste prop (o
 * `idleSuffix` do Bopper), ou "" quando nao ha'. O layout concatena no nome
 * que ele procura (`danceLeft` -> `danceLeft-scared`). */
const char *ss_prop_sufixo(const ss_t *ss, int prop_abs);

/* Angulo do prop em GRAUS (o `sprite.angle`). Devolve 1 quando o script gira o
   sprite e escreve `*graus`; 0 quando o prop nao e' girado por ele. */
int ss_prop_ang(const ss_t *ss, int prop_abs, float *graus);

/* ---- O `sserafim` (spaghetti): os eventos de musica ----
 * O chart do spaghetti tem 468 eventos, 89 deles dirigidos por ESTE script.
 * Cada um chega aqui pelo NOME (o conversor poe o nome na tabela de strings do
 * chart: os ids numericos dos eventos de script sao atribuidos por ordem de
 * aparicao e nao valem entre arquivos).
 * Devolve 1 quando o evento foi aplicado, 0 quando nao e' deste palco (o
 * chamador conta e registra).
 * Os parametros chegam em `ev`: p0..p2 em milesimos (o `fixed` do conversor),
 * `a` com as mascaras de booleanos, `b` com as flags do formato.
 */
typedef struct {
    int p0, p1, p2;
    unsigned int a, b;
} ss_ev_t;
int ss_evento(ss_t *ss, const char *nome, const ss_ev_t *ev);

/* O `sserafimPulseLights` (bloco de parametros): as tres listas ja' decodificadas
 * pelo chamador. `cores` em 0x00RRGGBB. */
void ss_ssf_pulsa_lista(ss_t *ss, int n, const unsigned int *cores,
                        const float *durs, const float *ints);

/* Os tipos do bloco (`encode_block` em tools/fnf_to_fch3.py). */
#define SSF_BLK_FLOAT 0u
#define SSF_BLK_INT   1u
#define SSF_BLK_RGB   2u
#define SSF_BLK_BOOL  3u

/* Decodifica o bloco de parametros do `sserafimPulseLights` (formato do
 * conversor: u32 n, e por parametro u32 nome_id + u32 qtd + qtd pares
 * tipo/valor) e aplica as listas. `nome_de` resolve um id da tabela de strings.
 * 0 = aplicado, -1 = truncado, -2 = listas desalinhadas (recusa, nao inventa). */
int ss_ssf_bloco(ss_t *ss, const void *bloco, unsigned int len,
                 const char *(*nome_de)(unsigned int));

/* As curvas do `FlxEase` que os scripts usam, com a formula da fonte
 * (`FlxEase.hx` L61-64 e L137-145):
 *   cubeOut(t) = 1 + (--t)*t*t        (1 - (1-t)^3)
 *   sineIn(t)  = -cos(PI/2 * t) + 1
 *   sineOut(t) = sin(PI/2 * t)
 *   cubeInOut(t) = 4*t^3 se t <= 0,5 ; 1 - ((-2t+2)^3)/2 no resto
 *                  (FlxEase.hx L63-70, o `cubeInOut` do flixel) */
#define SS_EASE_LINEAR 0
#define SS_EASE_CUBE_OUT 1
#define SS_EASE_SINE_IN 2
#define SS_EASE_SINE_OUT 3
#define SS_EASE_CUBE_IN_OUT 4

/* Os bancos de som de `palco_som_toca`, e o numero dentro do banco vai em
 * `ss->som`. */
#define SS_SOM_LIGHTNING 0   /* `Lightning1..3` (phillyBlazin L187)        */
#define SS_SOM_TROVAO    1   /* `thunder_1..2` (spookyMansionErect L106)   */
#define SS_SOM_TREM      2   /* `train_passes` (phillyTrain L100, so' o 1) */
#define SS_SOM_CARPASS   3   /* `carPass0..1` (limoRide L99)               */

/* Quantos props o script deste palco mexe -- usado pelos testes e pelo log. */
#define SS_TREM_CARROS 8     /* L95 `trainCars:Int = 8` */
#define SS_TREM_X_RESET 1480.0f /* L152 `FlxG.width + 200` (1280 + 200) */

#ifdef __cplusplus
}
#endif

#endif /* FNF_STAGE_SCRIPT_H */
