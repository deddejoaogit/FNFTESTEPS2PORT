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
    ss->rng = SS_SEMENTE;
    /* So' o phillyBlazin tem script portado. Um palco sem script fica inativo,
       e todas as consultas devolvem "o dado manda" -- nenhum outro palco pode
       ganhar comportamento por acidente. */
    if (ss->palco < 0 || strcmp(bs_stage_props[ss->palco].id, "phillyBlazin") != 0)
        return;
    ss->i_sky = acha_prop(ss->palco, "skyAdditive");
    ss->i_light = acha_prop(ss->palco, "lightning");
    ss->i_fg = acha_prop(ss->palco, "foregroundMultiply");
    ss->i_lighten = acha_cor(ss->palco, "additionalLighten");
    /* Sem os quatro o script nao tem o que acender: e' dado faltando, e o dado
       daqui e' o JSON do palco (conferido por tools/gen_stage_dados.py). */
    if (ss->i_sky < 0 || ss->i_light < 0 || ss->i_fg < 0 || ss->i_lighten < 0)
        return;
    ss->ativo = 1;
    ss_reiniciar(ss);
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
    ss->cor_ativa = 0;
    ss->cor_t0 = 0.0f;
    /* L114-117: a cor de REPOUSO dos personagens neste palco nao e' branca. */
    ss->cor[SS_BF][0] = ss->cor[SS_BF][1] = ss->cor[SS_BF][2] = SS_BF_DAD_CLARO;
    ss->cor[SS_DAD][0] = ss->cor[SS_DAD][1] = ss->cor[SS_DAD][2] = SS_BF_DAD_CLARO;
    ss->cor[SS_GF][0] = ss->cor[SS_GF][1] = ss->cor[SS_GF][2] = SS_GF_CLARO;
}

void ss_song_end(ss_t *ss)
{
    if (!ss->ativo) return;
    ss->ligado = 0;                    /* L227 */
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
    /* L180-184: os personagens escurecem por 0,3 s e voltam ao repouso. */
    ss->cor_ativa = 1;
    ss->cor_t0 = ss->t;
    ss->cor[SS_BF][0] = ss->cor[SS_BF][1] = ss->cor[SS_BF][2] = SS_ESCURO;
    ss->cor[SS_DAD][0] = ss->cor[SS_DAD][1] = ss->cor[SS_DAD][2] = SS_ESCURO;
    ss->cor[SS_GF][0] = ss->cor[SS_GF][1] = ss->cor[SS_GF][2] = SS_ESCURO;
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

    /* L120-133 */
    if (ss->ligado) ss->timer -= elapsed;
    else ss->timer = 1.0f;
    if (ss->timer <= 0.0f) {
        ss_raio(ss);
        ss->timer = ss_rnd_float(ss, 7.0f, 15.0f);
        caiu = 1;
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
    if (prop_abs == ss->i_sky) return ss->raio_ativo ? ss->a_sky : 0.0f;
    if (prop_abs == ss->i_fg) return ss->raio_ativo ? ss->a_fg : 0.0f;
    if (prop_abs == ss->i_light) return ss->raio_ativo ? 1.0f : 0.0f;
    return -1.0f;
}

float ss_prop_x(const ss_t *ss, int prop_abs, float x_dado)
{
    if (!ss->ativo || prop_abs < 0 || prop_abs != ss->i_light) return x_dado;
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

int ss_cor_papel(const ss_t *ss, int papel, unsigned char *rgb)
{
    if (!ss->ativo || !rgb || papel < 0 || papel > SS_GF) return 0;
    rgb[0] = ss->cor[papel][0];
    rgb[1] = ss->cor[papel][1];
    rgb[2] = ss->cor[papel][2];
    return 1;
}
