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
 * O QUE NAO E' PORTADO, E POR QUE
 * - O `RuntimeRainShader` (L26, L38-42, L102-104, L136-140): o GS nao tem
 *   fragment shader. Sao 175 das 451 linhas dos 18 scripts; fica declarado
 *   como falta, nao como feito.
 * - `HapticUtil.vibrate` (L194-221): o controle do PS2 nao tem rumble acionado
 *   pelo jogo neste port (nenhum outro evento do original usa).
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

/* Papeis de personagem, na ordem de `char_layout.h` (CH_PAPEL_*). */
#define SS_BF  0
#define SS_DAD 1
#define SS_GF  2

/* Os props que o script do phillyBlazin mexe. -1 quando o palco nao os tem. */
typedef struct {
    int ativo;          /* 1 = este palco tem script portado */
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

    /* --- cor dos personagens (0..255, como o `color` do flixel) --- */
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

    /* --- o `onBeatHit` do spooky: `lightningStrikeBeat/Offset` (L26-27) --- */
    int strike_beat;
    int strike_offset;
    int ultimo_beat;    /* o ultimo beat que chegou (dispara uma vez por beat) */

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
 * Devolve 1 quando um flash comecou (para a telemetria). */
int ss_beat(ss_t *ss, int beat, const char *song_id, int tem_personagens);

/* Alpha do personagem do papel `papel` (SS_BF/SS_DAD/SS_GF), ou -1 quando o
 * palco nao mexe no alpha dos personagens (o desenho sai como sempre). */
float ss_alpha_papel(const ss_t *ss, int papel);

/* Cor do personagem do papel `papel` (SS_BF/SS_DAD/SS_GF), 3 bytes 0..255.
 * Devolve 0 quando o palco nao tem cor de personagem (o desenho sai com o
 * branco de sempre). */
int ss_cor_papel(const ss_t *ss, int papel, unsigned char *rgb);

#ifdef __cplusplus
}
#endif

#endif /* FNF_STAGE_SCRIPT_H */
