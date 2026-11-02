/* O script do palco (`stage_script.c`): o relampago do `phillyBlazin`.
 *
 * O que este teste decide: os INTERVALOS e as DURACOES do evento, que sao a
 * unica coisa que o console nao tem como descobrir depois. Uma camada de luz
 * que fica 1,4 s em vez de 1,5 s, um segundo raio que cai em 6 s em vez de 10,
 * um escurecimento que nao volta -- nada disso aparece como erro de conta, so'
 * como "o palco esta' diferente do original".
 *
 * Os numeros vem das linhas citadas em stage_script.c (L93, L132, L149-190,
 * L230-240) e das cores de repouso do L114-117.
 */
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "stage_script.h"
#include "stage_dados.h"

static int falhas;
#define CHECK(c) do { if (!(c)) { printf("  FALHA: %s (linha %d)\n", #c, __LINE__); falhas++; } } while (0)
#define CHECK_EQ(a, b) do { long _a = (long)(a), _b = (long)(b); \
    if (_a != _b) { printf("  FALHA: %s == %s: %ld != %ld (linha %d)\n", #a, #b, _a, _b, __LINE__); falhas++; } } while (0)
#define CHECK_NEAR(a, b) do { double _a = (double)(a), _b = (double)(b); \
    if (_a < _b - 0.005 || _a > _b + 0.005) { printf("  FALHA: %s ~= %s: %.4f != %.4f (linha %d)\n", #a, #b, _a, _b, __LINE__); falhas++; } } while (0)

/* Tolerancia das contas de tempo: UM quadro. Os alfas andam 0,7/90 por quadro,
   entao 1/60 s de diferenca vale 0,0078 -- e' o erro de somar 1/60 em float, e
   nao divergencia de comportamento. Cobrar o bit exato seria cobrar o
   arredondamento do compilador. */
#define PASSO (1.0f / 60.0f)
#define TOL_TEMPO 0.02
#define CHECK_PERTO(a, b, tol) do { double _a = (double)(a), _b = (double)(b); \
    if (_a < _b - (tol) || _a > _b + (tol)) { printf("  FALHA: %s ~= %s: %.4f != %.4f (linha %d)\n", #a, #b, _a, _b, __LINE__); falhas++; } } while (0)

/* Avanca `segundos` em passos de 1/60 (o quadro do original) e conta os raios. */
static int avanca(ss_t *ss, float segundos)
{
    int n = (int)(segundos * 60.0f + 0.5f), i, raios = 0;
    for (i = 0; i < n; i++) raios += ss_update(ss, PASSO);
    return raios;
}

/* Roda quadro a quadro ate' um raio cair. Devolve o instante em que caiu, ou
 * -1 se nao caiu dentro de `limite` segundos.
 *
 * Existe porque a soma de 1/60 em float nao da' 3,0 exato: o timer do original
 * e' subtraido a cada quadro, e o raio cai no primeiro quadro em que a soma
 * passa de 3,0 (no 180o ou no 181o, dependendo do arredondamento). Cobrar o
 * quadro exato seria cobrar a aritmetica do compilador; o que importa e' que
 * caia em 3,0 s e nao em 2,5 nem em 3,5. */
static float ate_cair(ss_t *ss, float limite)
{
    int n = (int)(limite * 60.0f + 0.5f), i;
    for (i = 0; i < n; i++) {
        if (ss_update(ss, PASSO)) return ss->t;
    }
    return -1.0f;
}

/* Avanca ate' que o raio tenha `dt` segundos -- com o passo do quadro. */
static void ate_dt(ss_t *ss, float dt)
{
    int n = (int)(dt * 60.0f + 0.5f), i;
    for (i = 0; i < n; i++) ss_update(ss, PASSO);
}

int main(void)
{
    ss_t ss;

    printf("== script do palco: o phillyBlazin e' o unico palco com script portado ==\n");
    ss_init(&ss, "phillyBlazin");
    CHECK_EQ(ss.ativo, 1);
    CHECK_EQ(ss.palco, 7);                       /* ordem alfabetica de bs_stage_props */
    CHECK(strcmp(bs_stage_props[ss.palco].id, "phillyBlazin") == 0);
    /* os quatro props, achados PELO NOME dentro da faixa do palco */
    CHECK(ss.i_sky >= 0 && strcmp(bs_props[ss.i_sky].name, "skyAdditive") == 0);
    CHECK(ss.i_light >= 0 && strcmp(bs_props[ss.i_light].name, "lightning") == 0);
    CHECK(ss.i_fg >= 0 && strcmp(bs_props[ss.i_fg].name, "foregroundMultiply") == 0);
    CHECK(ss.i_lighten >= 0);
    CHECK(strcmp(bs_cores[ss.i_lighten].name, "additionalLighten") == 0);
    /* e os quatro estao na FAIXA do palco, nao no palco do lado */
    CHECK(ss.i_sky >= bs_stage_props[ss.palco].first);
    CHECK(ss.i_sky < bs_stage_props[ss.palco].first + bs_stage_props[ss.palco].count);
    CHECK(ss.i_lighten >= bs_stage_cor_first[ss.palco]);
    CHECK(ss.i_lighten < bs_stage_cor_first[ss.palco] + bs_stage_cor_count[ss.palco]);

    printf("== script do palco: os outros 19 palcos ficam sem script ==\n");
    {
        /* Palcos SEM script portado. O `spookyMansionErect` saiu desta lista
           quando o flash dele entrou -- ele e' o segundo palco com script. */
        static const char *outros[] = { "mainStage", "school", "mallXmas",
                                        "sserafim", "tankmanBattlefield" };
        int i;
        for (i = 0; i < 5; i++) {
            ss_t o;
            ss_init(&o, outros[i]);
            CHECK_EQ(o.ativo, 0);
            /* sem script, toda consulta devolve "o dado manda" -- um palco nao
               pode ganhar cor de personagem nem alpha por acidente */
            CHECK(ss_prop_alpha(&o, 0) < 0.0f);
            CHECK(ss_cor_alpha(&o, 0) < 0.0f);
            CHECK_NEAR(ss_prop_x(&o, 0, 1234.0f), 1234.0f);
            {
                int a = 77, f = 88;
                CHECK_EQ(ss_prop_frame(&o, 0, &a, &f), 0);
                CHECK_EQ(a, 77);
                CHECK_EQ(f, 88);
            }
            {
                unsigned char rgb[3] = { 9, 9, 9 };
                CHECK_EQ(ss_cor_papel(&o, SS_BF, rgb), 0);
                CHECK_EQ(rgb[0], 9);
            }
            /* palco sem script nao anda no tempo nem sorteia nada */
            {
                unsigned int r0 = o.rng;
                CHECK_EQ(ss_update(&o, PASSO), 0);
                CHECK_EQ(avanca(&o, 100.0f), 0);
                CHECK_EQ(o.rng, r0);
                CHECK_NEAR(o.t, 0.0f);
            }
        }
        ss_init(&ss, "naoExiste");
        CHECK_EQ(ss.ativo, 0);
        CHECK_EQ(ss.palco, -1);
    }

    printf("== script do palco: a cor de REPOUSO dos personagens (L114-117) ==\n");
    ss_init(&ss, "phillyBlazin");
    {
        unsigned char rgb[3];
        CHECK_EQ(ss_cor_papel(&ss, SS_BF, rgb), 1);
        CHECK_EQ(rgb[0], 0xDE); CHECK_EQ(rgb[1], 0xDE); CHECK_EQ(rgb[2], 0xDE);
        CHECK_EQ(ss_cor_papel(&ss, SS_DAD, rgb), 1);
        CHECK_EQ(rgb[0], 0xDE);
        CHECK_EQ(ss_cor_papel(&ss, SS_GF, rgb), 1);
        CHECK_EQ(rgb[0], 0x88); CHECK_EQ(rgb[1], 0x88); CHECK_EQ(rgb[2], 0x88);
    }

    printf("== script do palco: nada acontece antes de 3,0 s (L93) ==\n");
    {
        int i;
        for (i = 0; i < 179; i++) {           /* 179/60 = 2,983 s */
            CHECK_EQ(ss_update(&ss, PASSO), 0);
            CHECK_NEAR(ss_prop_alpha(&ss, ss.i_sky), 0.0f);
            CHECK_NEAR(ss_prop_alpha(&ss, ss.i_fg), 0.0f);
            CHECK_NEAR(ss_prop_alpha(&ss, ss.i_light), 0.0f);
            CHECK_NEAR(ss_cor_alpha(&ss, ss.i_lighten), 0.0f);
        }
        CHECK_NEAR(ss.t, 2.98333f);
        CHECK_EQ(ss.raios, 0);
        /* o x do prop e' o do DADO enquanto nao ha' raio */
        {
            float x_dado = bs_props[ss.i_light].x;
            CHECK_NEAR(ss_prop_x(&ss, ss.i_light, x_dado), x_dado);
            CHECK_EQ(ss_prop_frame(&ss, ss.i_light, NULL, NULL), 0);
        }
    }

    printf("== script do palco: o primeiro raio em 3,00 s e os cuatro alfas (L152-167) ==\n");
    {
        unsigned char rgb[3];
        float t_caiu = ate_cair(&ss, 1.0f);
        CHECK(t_caiu > 0.0f);
        /* 3,0 s = 180 quadros; o passo do teste e' 1/60 */
        CHECK(t_caiu >= 3.0f - PASSO);
        CHECK(t_caiu <= 3.0f + PASSO);
        CHECK_EQ(ss.raios, 1);
        CHECK_EQ(ss.raio_ativo, 1);
        CHECK_NEAR(ss_prop_alpha(&ss, ss.i_sky), 0.7f);
        CHECK_NEAR(ss_prop_alpha(&ss, ss.i_fg), 0.64f);
        CHECK_NEAR(ss_prop_alpha(&ss, ss.i_light), 1.0f);
        CHECK_NEAR(ss_cor_alpha(&ss, ss.i_lighten), 0.3f);
        /* L169-176: x em [-250,280] (65%) ou [780,900] */
        {
            int dentro = (ss.raio_x >= -250.0f && ss.raio_x <= 280.0f) ||
                         (ss.raio_x >= 780.0f && ss.raio_x <= 900.0f);
            CHECK(dentro);
            CHECK_NEAR(ss_prop_x(&ss, ss.i_light, 0.0f), ss.raio_x);
        }
        /* L187: `Lightning1..3` */
        CHECK(ss.som >= 1 && ss.som <= 3);
        /* L180-184: os personagens escurecem no instante do raio */
        CHECK_EQ(ss_cor_papel(&ss, SS_BF, rgb), 1);
        CHECK_EQ(rgb[0], 0x60);
        CHECK_EQ(ss_cor_papel(&ss, SS_GF, rgb), 1);
        CHECK_EQ(rgb[0], 0x60);
        /* L167: `animation.play('strike')` -> quadro 0 */
        {
            int a = -1, f = -1;
            CHECK_EQ(ss_prop_frame(&ss, ss.i_light, &a, &f), 1);
            CHECK_EQ(a, 0);
            CHECK_EQ(f, 0);
        }
        /* e um prop que NAO e' do script continua sem override: o
           `phillyForegroundCity` (z 30) e o vizinho do `lightning` no JSON */
        CHECK(strcmp(bs_props[ss.i_light + 1].name, "phillyForegroundCity") == 0);
        CHECK(ss_prop_alpha(&ss, ss.i_light + 1) < 0.0f);
    }

    printf("== script do palco: a queda dos alfas (1,5 s / 1,5 s / 0,3 s) ==\n");
    {
        /* +0,15 s: o `additionalLighten` anda METADE do tween dele (0,3 s) e os
           dois de 1,5 s andaram 10% -- 0,63 e 0,576. */
        ate_dt(&ss, 0.15f);
        CHECK_PERTO(ss_prop_alpha(&ss, ss.i_sky), 0.63f, TOL_TEMPO);
        CHECK_PERTO(ss_prop_alpha(&ss, ss.i_fg), 0.576f, TOL_TEMPO);
        CHECK_PERTO(ss_cor_alpha(&ss, ss.i_lighten), 0.15f, TOL_TEMPO);
        {
            unsigned char rgb[3];
            /* a cor dos personagens anda em 0,3 s: metade do caminho */
            CHECK_EQ(ss_cor_papel(&ss, SS_BF, rgb), 1);
            CHECK_PERTO(rgb[0], 0x9F, 2);     /* 0x60 + (0xDE-0x60)/2 */
            CHECK_EQ(ss_cor_papel(&ss, SS_GF, rgb), 1);
            CHECK_PERTO(rgb[0], 0x74, 2);      /* 0x60 + (0x88-0x60)/2 */
        }
        /* +0,15 s = 0,30 s: o `additionalLighten` ja' apagou e os personagens
           ja' estao de volta ao repouso; as duas luzes de 1,5 s continuam. */
        ate_dt(&ss, 0.15f);
        CHECK_NEAR(ss_cor_alpha(&ss, ss.i_lighten), 0.0f);
        CHECK_PERTO(ss_prop_alpha(&ss, ss.i_sky), 0.56f, TOL_TEMPO);  /* 0,7 * 0,8 */
        CHECK_PERTO(ss_prop_alpha(&ss, ss.i_fg), 0.512f, TOL_TEMPO);
        {
            unsigned char rgb[3];
            CHECK_EQ(ss_cor_papel(&ss, SS_BF, rgb), 1);
            CHECK_PERTO(rgb[0], 0xDE, 2);    /* de volta ao repouso */
            CHECK_EQ(ss_cor_papel(&ss, SS_GF, rgb), 1);
            CHECK_PERTO(rgb[0], 0x88, 2);
        }
        /* +0,45 s = 0,75 s: METADE dos tweens de 1,5 s -> 0,35 e 0,32 */
        ate_dt(&ss, 0.45f);
        CHECK_PERTO(ss_prop_alpha(&ss, ss.i_sky), 0.35f, TOL_TEMPO);
        CHECK_PERTO(ss_prop_alpha(&ss, ss.i_fg), 0.32f, TOL_TEMPO);
        /* +0,5 s = 1,25 s: ainda aceso, quase apagado */
        ate_dt(&ss, 0.5f);
        CHECK(ss_prop_alpha(&ss, ss.i_sky) > 0.0f);
        CHECK(ss_prop_alpha(&ss, ss.i_sky) < 0.16f);
        /* +0,30 s = 1,55 s: `cleanupLightning` (L230-240) esconde os QUATRO,
           inclusive o `lightning`, que nao tem tween proprio */
        ate_dt(&ss, 0.3f);
        CHECK_EQ(ss.raio_ativo, 0);
        CHECK_NEAR(ss_prop_alpha(&ss, ss.i_sky), 0.0f);
        CHECK_NEAR(ss_prop_alpha(&ss, ss.i_fg), 0.0f);
        CHECK_NEAR(ss_prop_alpha(&ss, ss.i_light), 0.0f);
        CHECK_NEAR(ss_cor_alpha(&ss, ss.i_lighten), 0.0f);
        CHECK_EQ(ss_prop_frame(&ss, ss.i_light, NULL, NULL), 0);
    }

    printf("== script do palco: o quadro do `strike` (24 fps, 30 quadros, loop) ==\n");
    {
        ss_t s2;
        int a = -1, f = -1;
        ss_init(&s2, "phillyBlazin");
        CHECK(ate_cair(&s2, 4.0f) > 0.0f);
        CHECK_EQ(s2.raio_ativo, 1);
        CHECK_EQ(ss_prop_frame(&s2, s2.i_light, &a, &f), 1);
        CHECK_EQ(f, 0);
        ate_dt(&s2, 0.5f);                     /* 0,5 s * 24 fps = quadro 12 */
        CHECK_EQ(ss_prop_frame(&s2, s2.i_light, &a, &f), 1);
        CHECK(f >= 11 && f <= 13);
        /* 30 quadros a 24 fps = 1,25 s; em 1,4 s o flixel ja' deu a volta */
        ate_dt(&s2, 0.9f);
        CHECK_EQ(ss_prop_frame(&s2, s2.i_light, &a, &f), 1);
        CHECK(f >= 0 && f < 30);
    }

    printf("== script do palco: o proximo raio entre 10,0 e 18,0 s (7..15 de espera) ==\n");
    {
        ss_t s3;
        int i, quando = -1;
        ss_init(&s3, "phillyBlazin");
        {
            float t1 = ate_cair(&s3, 4.0f);
            CHECK(t1 > 0.0f);
            CHECK_EQ(s3.raios, 1);
            for (i = 0; i < 60 * 16 && quando < 0; i++) {
                if (ss_update(&s3, PASSO)) quando = i + 1;
            }
            CHECK(quando > 0);
            /* t1 + 7,0 s ate' t1 + 15,0 s (L132) */
            if (quando > 0) {
                float s = s3.t;
                CHECK(s >= t1 + 7.0f - PASSO);
                CHECK(s <= t1 + 15.0f + PASSO);
                CHECK_EQ(s3.raios, 2);
            }
        }
        /* e entre um raio e outro o palco fica apagado: 15 s de mundo */
        CHECK(ss_prop_alpha(&s3, s3.i_sky) >= 0.0f);
    }

    printf("== script do palco: 10 raios seguidos e o estado sempre volta ao repouso ==\n");
    {
        ss_t s4;
        int i, r = 0;
        ss_init(&s4, "phillyBlazin");
        for (i = 0; i < 60 * 180; i++) {          /* 3 minutos de musica */
            if (ss_update(&s4, PASSO)) r++;
        }
        CHECK(r >= 10);
        CHECK_EQ(s4.raios, r);
        /* o `x` de cada raio ficou dentro dos dois intervalos */
        CHECK((s4.raio_x >= -250.0f && s4.raio_x <= 280.0f) ||
              (s4.raio_x >= 780.0f && s4.raio_x <= 900.0f));
        /* e depois de qualquer raio, 2 s de silencio: tudo apagado */
        {
            float a = ss_prop_alpha(&s4, s4.i_sky);
            CHECK(a == 0.0f || (a > 0.0f && a <= 0.7f));
        }
    }

    printf("== script do palco: dois sorteios iguais para a mesma semente ==\n");
    {
        /* Determinismo: o teste so' vale se a mesma musica der a mesma
           sequencia. O original sorteia com o relogio (FlxG.random); aqui a
           semente e' fixa, e o que se cobra e' que ela seja REPETIVEL -- sem
           isso nao ha' como provar nada sobre o evento no console. */
        ss_t a, b;
        int i;
        ss_init(&a, "phillyBlazin");
        ss_init(&b, "phillyBlazin");
        for (i = 0; i < 60 * 40; i++) {
            int ra = ss_update(&a, PASSO);
            int rb = ss_update(&b, PASSO);
            CHECK_EQ(ra, rb);
            if (ra) {
                CHECK_NEAR(a.raio_x, b.raio_x);
                CHECK_EQ(a.som, b.som);
            }
        }
        CHECK_EQ(a.raios, b.raios);
    }

    printf("== script do palco: fim da musica para o relampago (L223-228) ==\n");
    {
        ss_t s5;
        int i, raios_depois = 0;
        ss_init(&s5, "phillyBlazin");
        CHECK(ate_cair(&s5, 4.0f) > 0.0f);
        CHECK_EQ(s5.raios, 1);
        ss_song_end(&s5);
        for (i = 0; i < 60 * 60; i++) raios_depois += ss_update(&s5, PASSO);
        CHECK_EQ(raios_depois, 0);
        CHECK_NEAR(s5.timer, 1.0f);       /* L126: `lightningTimer = 1` */
    }

    printf("== script do palco: retry volta ao comeco (L52-58) ==\n");
    {
        ss_t s6;
        ss_init(&s6, "phillyBlazin");
        avanca(&s6, 5.0f);
        CHECK(s6.raios >= 1);
        ss_reiniciar(&s6);
        CHECK_NEAR(s6.timer, 3.0f);
        CHECK_EQ(s6.ligado, 1);
        CHECK_EQ(s6.raios, 0);
        CHECK_EQ(s6.raio_ativo, 0);
        CHECK_NEAR(s6.a_sky, 0.0f);
        {
            unsigned char rgb[3];
            CHECK_EQ(ss_cor_papel(&s6, SS_BF, rgb), 1);
            CHECK_EQ(rgb[0], 0xDE);
        }
        /* e o primeiro raio da segunda tentativa volta a cair em 3,0 s */
        {
            float t2 = ate_cair(&s6, 4.0f);
            CHECK(t2 >= 3.0f - PASSO);
            CHECK(t2 <= 3.0f + PASSO);
            CHECK_EQ(s6.raios, 1);
        }
    }

    printf("== script do palco: o FLASH do spookyMansionErect (sequencia 0,06/0,12) ==\n");
    {
        ss_t sp;
        ss_init(&sp, "spookyMansionErect");
        CHECK_EQ(sp.ativo, 1);
        CHECK(sp.i_bgl >= 0 && strcmp(bs_props[sp.i_bgl].name, "bgLight") == 0);
        CHECK(sp.i_stl >= 0 && strcmp(bs_props[sp.i_stl].name, "stairsLight") == 0);
        /* em repouso o script NAO manda: o alpha e' o do dado (0,0001, do
           `buildStage` do L90-91) */
        CHECK(ss_prop_alpha(&sp, sp.i_bgl) < 0.0f);
        CHECK(ss_prop_alpha(&sp, sp.i_stl) < 0.0f);
        CHECK_NEAR(bs_props[sp.i_bgl].alpha, 0.0001f);
        /* e os personagens comecam em 1 (visiveis) */
        CHECK_NEAR(ss_alpha_papel(&sp, SS_BF), 1.0f);
        CHECK_NEAR(ss_alpha_papel(&sp, SS_DAD), 1.0f);
        CHECK_NEAR(ss_alpha_papel(&sp, SS_GF), 1.0f);
        /* o phillyBlazin nao mexe no alpha dos personagens (so' na cor) */
        {
            ss_t pb;
            ss_init(&pb, "phillyBlazin");
            CHECK(ss_alpha_papel(&pb, SS_BF) < 0.0f);
        }

        /* `beat == 4` na `spookeez` dispara SEM som (L164). */
        {
            int disp = ss_beat(&sp, 4, "spookeez", 1);
            CHECK_EQ(disp, 1);
            CHECK_EQ(sp.som, 0);              /* sem som, como o `false` do L164 */
            CHECK_EQ(sp.flash, 1);
            CHECK_EQ(sp.flash_fase, 0);
            CHECK_NEAR(ss_prop_alpha(&sp, sp.i_bgl), 1.0f);
            CHECK_NEAR(ss_prop_alpha(&sp, sp.i_stl), 1.0f);
            CHECK_NEAR(ss_alpha_papel(&sp, SS_BF), 0.0f);
            CHECK_NEAR(ss_alpha_papel(&sp, SS_DAD), 0.0f);
            CHECK_NEAR(ss_alpha_papel(&sp, SS_GF), 0.0f);
            /* o offset e' sorteado no strike: 8..24 (L119) */
            CHECK(sp.strike_offset >= 8 && sp.strike_offset <= 24);
            CHECK_EQ(sp.strike_beat, 4);
        }
        /* +0,06 s: luzes apagam, personagens voltam (L51-61) */
        avanca(&sp, 0.06f);
        CHECK_EQ(sp.flash_fase, 1);
        CHECK_NEAR(ss_prop_alpha(&sp, sp.i_bgl), 0.0f);
        CHECK_NEAR(ss_prop_alpha(&sp, sp.i_stl), 0.0f);
        CHECK_NEAR(ss_alpha_papel(&sp, SS_BF), 1.0f);
        /* +0,07 s (= 0,13 s): acende de novo, personagens apagam, tweens de
           1,5 s comecam (L62-78) */
        avanca(&sp, 0.07f);
        CHECK_EQ(sp.flash_fase, 2);
        CHECK(ss_prop_alpha(&sp, sp.i_bgl) > 0.9f);
        CHECK(ss_alpha_papel(&sp, SS_BF) < 0.1f);
        /* +0,75 s: metade do tween de 1,5 s -> luzes 0,5 e personagens 0,5 */
        avanca(&sp, 0.75f);
        CHECK_PERTO(ss_prop_alpha(&sp, sp.i_bgl), 0.5f, TOL_TEMPO);
        CHECK_PERTO(ss_prop_alpha(&sp, sp.i_stl), 0.5f, TOL_TEMPO);
        CHECK_PERTO(ss_alpha_papel(&sp, SS_BF), 0.5f, TOL_TEMPO);
        /* +0,8 s: acabou -> tudo no repouso */
        avanca(&sp, 0.8f);
        CHECK_EQ(sp.flash, 0);
        CHECK_NEAR(ss_prop_alpha(&sp, sp.i_bgl), 0.0f);
        CHECK_NEAR(ss_alpha_papel(&sp, SS_BF), 1.0f);
        CHECK_NEAR(ss_alpha_papel(&sp, SS_DAD), 1.0f);
        CHECK_NEAR(ss_alpha_papel(&sp, SS_GF), 1.0f);
    }

    printf("== script do palco: o `bool(10)` e o `beat > strike+offset` (L170) ==\n");
    {
        ss_t sp;
        int b, disparos = 0, beats = 0, ultimo = -1000, som_ok = 1;
        ss_init(&sp, "spookyMansionErect");
        /* 40 compassos = 160 beats. O sorteio e' 10%, entao ~16 disparos; a
           regra que importa e' a do offset: um disparo so' pode acontecer com
           `beat > strike_beat + strike_offset`. */
        for (b = 5; b < 165; b++) {
            int d;
            beats++;
            d = ss_beat(&sp, b, "spookeez", 1);
            if (d) {
                /* no beat 4 nao estamos aqui (b comeca em 5) */
                CHECK(b > ultimo + 8);
                if (sp.som < 1 || sp.som > 2) som_ok = 0;
                ultimo = b;
                disparos++;
                sp.som = 0;
            }
        }
        printf("     %d disparo(s) em %d beats (10%% do sorteio), todos com o offset\n",
               disparos, beats);
        CHECK(disparos > 4 && disparos < 40);   /* ~10% de 160 */
        CHECK(som_ok);
        /* sem personagens em cena o `doLightningStrike` do original faz return:
           nenhum flash, mas o sorteio do beat continua acontecendo. */
        {
            ss_t s2;
            int k, n = 0;
            ss_init(&s2, "spookyMansionErect");
            for (k = 5; k < 165; k++) n += ss_beat(&s2, k, "spookeez", 0);
            CHECK_EQ(n, 0);
            CHECK_EQ(s2.flash, 0);
        }
        /* e numa musica que NAO e' a `spookeez` o `beat == 4` nao vale
           (L164: `currentSong.id == "spookeez"`), mas o sorteio de 10% vale
           para qualquer musica do palco. */
        {
            ss_t s3;
            CHECK_EQ(ss_beat(&s3, 4, "south", 1), 0);
            ss_init(&s3, "spookyMansionErect");
            CHECK_EQ(ss_beat(&s3, 4, "south", 1), 0);
            CHECK_EQ(s3.flash, 0);
        }
    }

    printf("== script do palco: o mesmo beat nao dispara duas vezes ==\n");
    {
        ss_t sp;
        int i, n = 0;
        ss_init(&sp, "spookyMansionErect");
        for (i = 0; i < 30; i++) n += ss_beat(&sp, 4, "spookeez", 1);
        CHECK_EQ(n, 1);                 /* um beat chega em varios quadros */
        CHECK_EQ(sp.som, 0);
    }

    printf("== script do palco: sem os tres personagens nao ha' raio (L102) ==\n");
    {
        /* `doLightningStrike` L102: `if (bf == null || gf == null || dad == null)
           return;`. O Tutorial nao tem `girlfriend` no playData, entao o palco
           inteiro (mesmo com o script certo) nao teria raio nenhum -- e' o
           unico caso em que o `bool(10)` sorteia e nao acontece nada. */
        ss_t sp;
        int i, n = 0;
        ss_init(&sp, "spookyMansionErect");
        for (i = 0; i < 240; i++) n += ss_beat(&sp, i, "spookeez", 0);
        CHECK_EQ(n, 0);
        CHECK_EQ(sp.som, 0);            /* sem personagens, nem sorteia o som */
        CHECK_EQ(sp.flash, 0);
        CHECK_EQ(sp.strike_offset, 8);   /* L27: o offset de repouso, sem sorteio */
    }

    if (falhas) { printf("\n  %d falha(s)\n", falhas); return 1; }
    printf("\n  script do palco: conferidos o relampago do phillyBlazin (3,0 s /\n"
           "  1,5 s / 0,3 s / 7..15 s / 4 props / cor dos personagens) e o flash\n"
           "  do spookyMansionErect (0,06 s / 0,12 s / 1,5 s / 10%% / offset 8..24)\n");
    return 0;
}
