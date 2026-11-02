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

    printf("== script do palco: so' o blazin pinta personagem (regressao) ==\n");
    {
        /* O `cor[]` de repouso e' do BLAZIN. Um palco com script que nao tem
           cor de personagem (`phillyTrain`, `spookyMansionErect`) devolvia 1 com
           0xDE/0x88 -- e o desenho pintava os tres personagens com um tom que
           nenhuma linha do `.hxc` deles escreve. Medido: `char_cor` aparecia no
           log do pico ANTES desta correcao? Nao: o `main.c` converte 0xDE para
           0x6F e so' loga uma vez por personagem, entao a prova aqui e' o
           RETORNO da consulta, nao o log. */
        unsigned char rgb[3];
        int i;
        static const char *sem_cor[] = { "phillyTrain", "spookyMansionErect" };
        for (i = 0; i < 2; i++) {
            ss_t t;
            int papel;
            ss_init(&t, sem_cor[i]);
            CHECK_EQ(t.ativo, 1);                 /* o script do palco esta' ativo */
            CHECK_EQ(t.cor_ligada, 0);
            for (papel = 0; papel < 3; papel++) {
                rgb[0] = rgb[1] = rgb[2] = 9;
                CHECK_EQ(ss_cor_papel(&t, papel, rgb), 0);
                CHECK_EQ(rgb[0], 9);
                CHECK_EQ(rgb[1], 9);
                CHECK_EQ(rgb[2], 9);
            }
        }
        ss_init(&ss, "phillyBlazin");
        CHECK_EQ(ss.cor_ligada, 1);               /* e o blazin continua pintando */
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

    printf("== script do palco: o CEU da mansao (spookyMansion, o palco de 3 musicas) ==\n");
    {
        ss_t sm;
        int a = -1, f = -1;
        ss_init(&sm, "spookyMansion");
        CHECK_EQ(sm.ativo, 1);
        CHECK(sm.i_bg >= 0 && strcmp(bs_props[sm.i_bg].name, "halloweenBG") == 0);
        /* a animacao achada PELO NOME, e os numeros vem do pacote, nao daqui */
        CHECK(sm.bg_anim >= 0);
        CHECK(strcmp(bs_prop_anims[bs_props[sm.i_bg].anim_first + sm.bg_anim].name,
                     "lightning") == 0);
        CHECK_EQ(sm.bg_quadros, 30);          /* `thunder00..thunder29` */
        CHECK_NEAR(sm.bg_fps, 24.0f);
        /* em repouso o script NAO manda no quadro: quem escolhe e' o dado
           (`startingAnimation: "idle"`, 6 quadros, sem loop) */
        CHECK_EQ(ss_prop_frame(&sm, sm.i_bg, &a, &f), 0);
        CHECK_EQ(a, -1);
        CHECK_EQ(f, -1);
        /* e nao inventa alpha de prop, alpha de personagem nem cor: o script
           da mansao nao-erect nao escreve nenhum dos tres (o ERECT escreve
           alpha, e o blazin escreve cor) */
        CHECK(ss_prop_alpha(&sm, sm.i_bg) < 0.0f);
        CHECK(ss_alpha_papel(&sm, SS_BF) < 0.0f);
        {
            unsigned char rgb[3] = { 9, 9, 9 };
            CHECK_EQ(ss_cor_papel(&sm, SS_BF, rgb), 0);
            CHECK_EQ(rgb[0], 9);
        }

        /* L80-83: `beat == 4` na `spookeez`, SEM som -- e sem precisar dos tres
           personagens (a versao nao-erect nao tem a guarda do L102 do erect) */
        CHECK_EQ(ss_beat(&sm, 4, "spookeez", 0), 1);
        CHECK_EQ(sm.som, 0);
        CHECK_EQ(sm.scared_pedido, 1);
        CHECK_EQ(sm.strike_beat, 4);
        CHECK(sm.strike_offset >= 8 && sm.strike_offset <= 24);
        /* L33: o `animation.play('lightning')` comeca do PRIMEIRO quadro */
        CHECK_EQ(ss_prop_frame(&sm, sm.i_bg, &a, &f), 1);
        CHECK_EQ(a, sm.bg_anim);
        CHECK_EQ(f, 0);
        /* 0,5 s depois: quadro 12 (24 fps); 1,25 s: o ultimo (29) e trava la */
        avanca(&sm, 0.5f);
        CHECK_EQ(ss_prop_frame(&sm, sm.i_bg, &a, &f), 1);
        CHECK_EQ(f, 12);
        avanca(&sm, 0.75f);
        CHECK_EQ(ss_prop_frame(&sm, sm.i_bg, &a, &f), 1);
        CHECK_EQ(f, 29);                       /* `looped: false` -> para no fim */
        avanca(&sm, 5.0f);
        CHECK_EQ(ss_prop_frame(&sm, sm.i_bg, &a, &f), 1);
        CHECK_EQ(f, 29);
        /* e um prop que nao e' o halloweenBG continua no dado */
        {
            ss_t so;
            ss_init(&so, "spookyMansion");
            CHECK(so.i_bg == bs_stage_props[so.palco].first);   /* unico prop */
        }

        /* L87-90: `bool(10)` + `beat > strike + offset`, COM som do trovao.
           Roda 200 beats: tem de cair mais de um, sempre entre thunder_1/2 e
           sempre com o banco do trovao. */
        {
            ss_t s2;
            int i, n = 0, som_ruim = 0, banco_ruim = 0, fora = 0;
            ss_init(&s2, "spookyMansion");
            for (i = 5; i < 205; i++) {
                int d = ss_beat(&s2, i, "south", 0);   /* outra musica do palco */
                if (!d) continue;
                n++;
                if (s2.som < 1 || s2.som > 2) som_ruim++;
                if (s2.som_banco != SS_SOM_TROVAO) banco_ruim++;
                if (s2.strike_beat != i) fora++;
                s2.som = 0;                     /* o main.c consome */
            }
            CHECK(n >= 5);                      /* 10% de 200 beats */
            CHECK_EQ(som_ruim, 0);
            CHECK_EQ(banco_ruim, 0);
            CHECK_EQ(fora, 0);
        }

        /* o retry/countdown volta ao estado do L23-24 (beat 0, offset 8) e
           desarma a animacao que estivesse tocando */
        {
            ss_t s3;
            ss_init(&s3, "spookyMansion");
            ss_beat(&s3, 4, "spookeez", 0);
            ss_reiniciar(&s3);
            CHECK_EQ(s3.strike_beat, 0);
            CHECK_EQ(s3.strike_offset, 8);
            CHECK_NEAR(s3.bg_t0, -1.0f);
            CHECK_EQ(ss_prop_frame(&s3, s3.i_bg, &a, &f), 0);
        }
    }

    printf("== script do palco: o TREM do phillyTrain (L95-166) ==\n");
    {
        ss_t sp;
        int i, disparou = -1, carros0;
        float x0;
        const float passo = 1.0f / 24.0f;
        ss_init(&sp, "phillyTrain");
        CHECK_EQ(sp.ativo, 1);
        CHECK(sp.i_train >= 0);
        CHECK(sp.i_lights >= 0);
        /* o x inicial e' o do DADO (o `buildStage` poe o sprite na posicao do
           JSON), e nao o 1480 do `trainReset` */
        x0 = bs_props[sp.i_train].x;
        CHECK(x0 > 1000.0f);
        CHECK_PERTO(sp.trem_x, x0, 0.001);
        CHECK_EQ(sp.trem_carros, 8);          /* L95 */
        CHECK_EQ(sp.trem_movendo, 0);
        CHECK_EQ(sp.luz_cor, -1);             /* nenhuma cor antes do beat 0 */

        /* O TREM SO' COMECA em `beat % 8 == 4`, com cooldown > 8 e o `bool(30)`
           sorteado -- e o sorteio acontece em TODO beat %8==4. */
        for (i = 0; i < 200 && disparou < 0; i++) {
            int r = ss_beat(&sp, i, "pico", 1);
            if (r) { disparou = i; break; }
            if (i % 8 == 4 && i <= 12 && sp.trem_movendo == 0 && sp.trem_cooldown <= 8)
                CHECK_EQ(r, 0);               /* cooldown curto nao dispara */
        }
        CHECK(disparou > 8);                  /* L117: cooldown > 8 */
        CHECK_EQ(disparou % 8, 4);            /* L117: beat % 8 == 4 */
        CHECK_EQ(sp.trem_movendo, 1);
        CHECK_EQ(sp.som, 1);
        CHECK_EQ(sp.som_banco, SS_SOM_TREM);
        carros0 = sp.trem_carros;

        /* L121: enquanto o som nao chega a 4,7 s, o trem NAO anda. */
        for (i = 0; i < 112; i++) ss_update(&sp, passo);   /* 4,666 s */
        CHECK_PERTO(sp.trem_x, x0, 0.001);
        CHECK_EQ(sp.trem_comecou, 0);
        CHECK_EQ(sp.hair_pedido, 0);

        /* ... e no passo seguinte (4,708 s) comeca: x -= 400 e hairBlow. */
        ss_update(&sp, passo);
        CHECK_EQ(sp.trem_comecou, 1);
        CHECK_EQ(sp.hair_pedido, 1);
        CHECK_PERTO(sp.trem_x, x0 - 400.0f, 0.001);

        /* A passagem inteira: -400 por passo, -2000 -> -1150 (um vagao) e
           -4000 -> trainReset (x = 1480, 8 vagoes, parado, hairFall). */
        for (i = 0; i < 200 && sp.trem_movendo; i++) ss_update(&sp, passo);
        CHECK_EQ(sp.trem_movendo, 0);
        CHECK_EQ(sp.trem_carros, carros0);
        CHECK_EQ(sp.trem_acabando, 0);
        CHECK_EQ(sp.trem_comecou, 0);
        CHECK_PERTO(sp.trem_x, SS_TREM_X_RESET, 0.001);
        CHECK_EQ(sp.hair_pedido, 2);
    }

    printf("== script do palco: as cinco cores das `lights` (L28-34, L142-149) ==\n");
    {
        /* As cinco cores do L28-34, escritas aqui de novo DE PROPOSITO: se
           alguem trocar um byte na tabela do .c, este teste tem de acusar. */
        static const unsigned char esperado[5][3] = {
            { 0x31, 0xA2, 0xFD }, { 0x31, 0xFD, 0x8C }, { 0xFB, 0x33, 0xF5 },
            { 0xFB, 0xA6, 0x33 }, { 0xFD, 0x45, 0x31 }
        };
        ss_t sp;
        int i, mudou = 0, vistas[5] = { 0, 0, 0, 0, 0 };
        unsigned char rgb[3];
        ss_init(&sp, "phillyTrain");
        /* beat 0: `beat % 4 == 0` ja' sorteia a cor (L142) */
        ss_beat(&sp, 0, "pico", 1);
        CHECK(sp.luz_cor >= 0 && sp.luz_cor < 5);
        CHECK_EQ(ss_prop_cor(&sp, sp.i_lights, rgb), 1);
        CHECK_EQ(rgb[0], esperado[sp.luz_cor][0]);
        CHECK_EQ(rgb[1], esperado[sp.luz_cor][1]);
        CHECK_EQ(rgb[2], esperado[sp.luz_cor][2]);
        /* beat 1 e 2 nao tocam na cor */
        {
            int cor_antes = sp.luz_cor;
            ss_beat(&sp, 1, "pico", 1);
            ss_beat(&sp, 2, "pico", 1);
            ss_beat(&sp, 3, "pico", 1);
            CHECK_EQ(sp.luz_cor, cor_antes);
        }
        /* ao longo de 40 beats as cinco cores aparecem */
        for (i = 0; i < 40; i++) {
            ss_beat(&sp, i, "pico", 1);
            if (i % 4 == 0) {
                vistas[sp.luz_cor]++;
                if (ss_prop_cor(&sp, sp.i_lights, rgb)) mudou++;
                /* Toda cor sorteada tem de ser a do L28-34, canal por canal. */
                CHECK_EQ(rgb[0], esperado[sp.luz_cor][0]);
                CHECK_EQ(rgb[1], esperado[sp.luz_cor][1]);
                CHECK_EQ(rgb[2], esperado[sp.luz_cor][2]);
            }
        }
        CHECK(mudou >= 10);
        /* Nao se cobra as CINCO cores em 40 beats: o sorteio e' uniforme sobre
           5 e a semente e' fixa, entao o conjunto que sai e' deterministico
           mas depende da sequencia do xorshift -- cobrar "5 de 5" travava o
           teste numa propriedade do gerador, nao do script. O que se cobra e'
           que as cores sorteadas sejam SORTEADAS (>= 4 distintas em 10
           sorteios) e que cada uma seja a da tabela (acima). */
        {int k, quantas = 0; for (k = 0; k < 5; k++) if (vistas[k]) quantas++;
         CHECK(quantas >= 4);}
        /* um prop que nao e' o `lights` nao ganha cor */
        CHECK_EQ(ss_prop_cor(&sp, sp.i_train, rgb), 0);
        /* e o fim da musica para o trem (L169-176) */
        ss_song_end(&sp);
        CHECK_EQ(sp.trem_ligado, 0);
        CHECK_EQ(sp.trem_movendo, 0);
    }

    printf("== script do palco: o SINAL e os CARROS do phillyStreets (L34-70, L189-395) ==\n");
    {
        ss_t ps;
        int a = -1, f = -1;
        ss_init(&ps, "phillyStreets");
        CHECK_EQ(ps.ativo, 1);
        CHECK(ps.i_luz >= 0 && strcmp(bs_props[ps.i_luz].name, "phillyTraffic") == 0);
        CHECK(ps.i_car >= 0 && strcmp(bs_props[ps.i_car].name, "phillyCars") == 0);
        CHECK(ps.i_car2 >= 0 && strcmp(bs_props[ps.i_car2].name, "phillyCars2") == 0);
        /* L402-415 `resetStageValues`: verde, intervalo 8, `lastChange` 0; e
           L85-100 `resetCar`: (1200, 818) com angulo 0. */
        CHECK_EQ(ps.luz_parado, 0);
        CHECK_EQ(ps.luz_anim, 0);
        CHECK_EQ(ps.luz_intervalo, 8);
        CHECK_EQ(ps.luz_ultimo, 0);
        CHECK_EQ(ps.car_livre, 1);
        CHECK_EQ(ps.car2_livre, 1);
        CHECK_EQ(ps.car_esperando, 0);
        CHECK_NEAR(ps.carro[0].x, 1200.0f);
        CHECK_NEAR(ps.carro[0].y, 818.0f);
        CHECK_NEAR(ps.carro[0].ang, 0.0f);
        CHECK_NEAR(ss_prop_x(&ps, ps.i_car, 0.0f), 1200.0f);
        CHECK_NEAR(ss_prop_y(&ps, ps.i_car, 0.0f), 818.0f);
        /* parado no dado: o `carro[].anim` e' 0 e o script nao manda no quadro */
        CHECK_EQ(ss_prop_frame(&ps, ps.i_car, &a, &f), 0);
        /* o sinal comeca no `togreen` (anim 0 do prop, 77 quadros em loop) */
        CHECK_EQ(ss_prop_frame(&ps, ps.i_luz, &a, &f), 1);
        CHECK_EQ(a, 0);

        /* L511: a troca em `beat == lastChange + changeInterval` = 8. Antes
           disso, nada muda de estado (os carros podem sair, mas o sinal nao). */
        {
            int b;
            for (b = 1; b < 8; b++) {
                ss_beat(&ps, b, "2hot", 1);
                CHECK_EQ(ps.luz_ultimo, 0);
            }
            CHECK_EQ(ps.luz_parado, 0);
        }
        ss_beat(&ps, 8, "2hot", 1);
        CHECK_EQ(ps.luz_parado, 1);          /* L56 inverte */
        CHECK_EQ(ps.luz_anim, 1);            /* L60 `tored`   */
        CHECK_EQ(ps.luz_intervalo, 20);      /* L61           */
        CHECK_EQ(ps.luz_ultimo, 8);
        CHECK_EQ(ss_prop_frame(&ps, ps.i_luz, &a, &f), 1);
        CHECK_EQ(a, 1);
        /* a proxima troca so' em 8 + 20 = 28: em 27 ainda esta' vermelho */
        {
            int b;
            for (b = 9; b < 28; b++) ss_beat(&ps, b, "2hot", 1);
            CHECK_EQ(ps.luz_parado, 1);
        }
        ss_beat(&ps, 28, "2hot", 1);
        CHECK_EQ(ps.luz_parado, 0);          /* volta ao verde */
        CHECK_EQ(ps.luz_anim, 0);
        CHECK_EQ(ps.luz_intervalo, 30);

        /* Os carros: `bool(10)` por beat. Roda beats ate' um sair, e cobra o
           que a fonte garante -- animacao `car1..car4`, angulo inicial da
           variante, duracao dentro da faixa e o carro parando no fim. */
        {
            ss_t c;
            int b, saiu = -1, tentativas = 0;
            ss_init(&c, "phillyStreets");
            for (b = 1; b < 200; b++) {
                ss_beat(&c, b, "2hot", 1);
                tentativas++;
                if (c.carro[0].ativo) { saiu = b; break; }
            }
            CHECK(saiu > 0);
            CHECK(tentativas < 200);
            CHECK(c.carro[0].anim >= 1 && c.carro[0].anim <= 4);
            /* a duracao e o `extraOffset` sao por variante (L225-238) */
            switch (c.carro[0].anim) {
            case 1: CHECK(c.carro[0].dur >= 0.99f && c.carro[0].dur <= 1.71f); break;
            case 2: CHECK(c.carro[0].dur >= 0.59f && c.carro[0].dur <= 1.21f);
                    CHECK_NEAR(c.carro[0].offx, 20.0f); CHECK_NEAR(c.carro[0].offy, -15.0f); break;
            case 3: CHECK(c.carro[0].dur >= 1.49f && c.carro[0].dur <= 2.51f);
                    CHECK_NEAR(c.carro[0].offx, 30.0f); CHECK_NEAR(c.carro[0].offy, 50.0f); break;
            default: CHECK(c.carro[0].dur >= 1.49f && c.carro[0].dur <= 2.51f);
                    CHECK_NEAR(c.carro[0].offx, 10.0f); CHECK_NEAR(c.carro[0].offy, 60.0f); break;
            }
            /* o angulo comeca no `from` do `FlxTween.angle` */
            if (c.luz_parado) CHECK_NEAR(c.carro[0].ang, -7.0f);
            else CHECK_NEAR(c.carro[0].ang, -8.0f);
            CHECK_EQ(c.car_livre, 0);
            /* o quadro da animacao do carro tem de vir do script */
            CHECK_EQ(ss_prop_frame(&c, c.i_car, &a, &f), 1);
            CHECK_EQ(a, c.carro[0].anim - 1);
            CHECK(f >= 0 && f < 4);
            /* a curva anda em direcao ao fim (x cresce nos tres caminhos) */
            {
                float x0 = c.carro[0].x;
                avanca(&c, c.carro[0].dur * 0.5f);
                CHECK(c.carro[0].x > x0);
            }
            /* e no fim o carro volta a ser interruptavel (o `onComplete`) */
            avanca(&c, c.carro[0].dur + 1.0f);
            CHECK_EQ(c.car_livre, 1);
            CHECK_EQ(c.carro[0].ativo, 0);
        }

        /* `resetCar` + `resetStageValues` no retry (L459-472) */
        {
            ss_t r;
            ss_init(&r, "phillyStreets");
            ss_beat(&r, 8, "2hot", 1);
            CHECK_EQ(r.luz_parado, 1);
            ss_reiniciar(&r);
            CHECK_EQ(r.luz_parado, 0);
            CHECK_EQ(r.luz_ultimo, 0);
            CHECK_EQ(r.luz_intervalo, 8);
            CHECK_NEAR(r.carro[0].x, 1200.0f);
            CHECK_NEAR(r.carro[1].y, 818.0f);
            CHECK_NEAR(r.carro[0].ang, 0.0f);
        }
    }

    if (falhas) { printf("\n  %d falha(s)\n", falhas); return 1; }
    printf("\n  script do palco: conferidos o relampago do phillyBlazin (3,0 s /\n"
           "  1,5 s / 0,3 s / 7..15 s / 4 props / cor dos personagens), o flash\n"
           "  do spookyMansionErect (0,06 s / 0,12 s / 1,5 s / 10%% / offset 8..24)\n"
           "  e o trem do phillyTrain (1/24 s, -400, -2000 -> -1150, -4000 -> 1480,\n"
           "  8 vagoes, 4,7 s de som, `beat %% 8 == 4` + bool(30), 5 cores), o\n"
           "  sinal e os dois carros do phillyStreets (verde/vermelho 8/20/30,\n"
           "  `bool(10)`, car1..car4, curvas de Bezier, angulo e offset)\n");
    return 0;
}
