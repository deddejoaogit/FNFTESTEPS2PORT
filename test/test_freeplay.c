/* Tela de FREEPLAY: a geometria e o tempo das capsulas, contra a fonte.
 *
 * O QUE PROVA (SongMenuItem.hx / FreeplayState.hx v0.8.6, com as linhas nos
 * comentarios de src/engine/freeplay.h)
 *  - `intendedX(i) = 270 + 60*sin(i)` e `intendedY(i) = i*((h*0,8)+10) + 120`,
 *    reduzidos a metade (a tela do port e' 640x360, o original 1280x720);
 *  - os desvios das pontas de `changeSelection` (L2941-2949): -50 e +10 nos
 *    itens de fora e -100 nos que estao ACIMA do selecionado;
 *  - o `smoothLerpPrecision` (0,256 s em x e 0,192 s em y): com dt igual a
 *    constante o passo fica exatamente a 1 % do caminho;
 *  - a entrada: os 7 passos de 1/24 s da escala (`xFrames`) e o ciclo de 24 fps
 *    da animacao da capsula (selecionada e nao selecionada);
 *  - o `textAppear`: (1,7 ; 0,2) -> (0,4 ; 1,4) -> (1 ; 1) em 3/24 s.
 *
 * O que NAO prova: que a tela parece a do original na TV. Isso e' a tela.
 */
#include <stdio.h>
#include <math.h>

#include "freeplay.h"

static int falhas;
#define CHECK(c) do { if (!(c)) { printf("  FALHA: %s (linha %d)\n", #c, __LINE__); falhas++; } } while (0)
#define CHECK_EQ(a, b) do { long _a = (long)(a), _b = (long)(b); \
    if (_a != _b) { printf("  FALHA: %s == %s: %ld != %ld (linha %d)\n", #a, #b, _a, _b, __LINE__); falhas++; } } while (0)
/* Comparacao de ponto flutuante com a folga de 0,01 px: a unica coisa que o
 * arredondamento de 32 bits pode mexer nas contas da tela. */
#define CHECK_PX(a, b) do { double _a = (double)(a), _b = (double)(b); \
    if (fabs(_a - _b) > 0.01) { printf("  FALHA: %s == %s: %.4f != %.4f (linha %d)\n", #a, #b, _a, _b, __LINE__); falhas++; } } while (0)

int main(void)
{
    printf("== freeplay: a geometria da lista (SongMenuItem.hx)\n");

    /* intendedX = 270 + 60*sin(i), em 640x360 -> 135 + 30*sin(i). */
    CHECK_PX(fp_x(0), 135.0f);
    CHECK_PX(fp_x(1), 135.0 + 30.0 * sin(1.0));
    CHECK_PX(fp_x(-1), 135.0 - 30.0 * sin(1.0));
    CHECK_PX(fp_x(2), 135.0 + 30.0 * sin(2.0));

    /* intendedY = i*((131*0,8)+10) + 120 em 1280x720 -> i*57,8 + 60. */
    CHECK_PX(fp_y(0), 60.0f);
    CHECK_PX(fp_y(1), 117.8f);
    CHECK_PX(fp_y(4), 291.2f);
    CHECK_PX(fp_y(-1), 2.2f);

    /* O item 1-based: `capsuleIndex = index - curSelected` (L2944-2949), entao o
     * SELECIONADO tem rel = 1 e fica em intendedY(1) = 117,8. */
    CHECK_PX(fp_y_item(1, 0), 117.8f);
    CHECK_PX(fp_y_item(2, 1), 117.8f);
    CHECK_PX(fp_y_item(3, 1), 175.6f);
    /* rel == 0 so' acontece em `updateSongsScroll` (indice == curSelected), e o
     * teste cobra a formula pura: intendedY(0) = 60, sem desvio. */
    CHECK_PX(fp_y_item(1, 1), 60.0f);
    CHECK_PX(fp_y_item(2, 2), 60.0f);
    /* rel > 4: yOffset -= 10 (L2943) -> +5 na metade. */
    CHECK_PX(fp_y_item(5, 0), 60.0 + 5.0 * 57.8 + 5.0);
    CHECK_PX(fp_y_item(6, 0), 60.0 + 6.0 * 57.8 + 5.0);
    /* Acima do selecionado: -25 (rel < 0) e -50 (index < curSelected). */
    CHECK_PX(fp_y_item(1, 2), 2.2 - 25.0 - 50.0);
    CHECK_PX(fp_y_item(2, 3), 2.2 - 25.0 - 50.0);

    printf("== freeplay: o lerp da posicao (MathUtil.smoothLerpPrecision)\n");
    /* dt == tempo -> pow(0,01, 1) = 0,01, ou seja 1 % do caminho. */
    CHECK_PX(fp_suave_x(100.0f, 200.0f, 0.256f), 199.0f);
    CHECK_PX(fp_suave_y(100.0f, 200.0f, 0.192f), 199.0f);
    /* dt == 0 devolve o atual (a fonte nao chama pow com 0). */
    CHECK_PX(fp_suave_x(100.0f, 200.0f, 0.0f), 100.0f);
    /* Ja' no alvo: fica no alvo, sem sobra de arredondamento. */
    CHECK_PX(fp_suave_x(100.0f, 100.0f, 0.05f), 100.0f);
    /* Um passo de 1/60 s anda mais devagar que um de 0,05 s. */
    CHECK(fp_suave_x(0.0f, 100.0f, 1.0f / 60.0f) < fp_suave_x(0.0f, 100.0f, 0.05f));

    printf("== freeplay: a entrada das capsulas (initJumpIn)\n");
    /* A escala do salto: xFrames[passo] em x e o inverso em y (L687-688). */
    CHECK_PX(fp_escala_x(0), 1.70f);
    CHECK_PX(fp_escala_y(0), 1.0 / 1.70);
    CHECK_PX(fp_escala_x(2), 0.85f);
    CHECK_PX(fp_escala_x(FP_XFRAMES_N - 1), 1.00f);
    /* Depois do ultimo passo a escala fica em 1 (o `doJumpIn` desarma). */
    CHECK_PX(fp_escala_x(FP_XFRAMES_N), 1.0f);
    CHECK_PX(fp_escala_y(99), 1.0f);

    /* O passo do `frameInTicker` e' de 1/24 s (41,67 ms), nao de 42 ms. */
    CHECK_EQ(fp_passo_salto(0), 0);
    CHECK_EQ(fp_passo_salto(41), 0);
    CHECK_EQ(fp_passo_salto(42), 1);
    CHECK_EQ(fp_passo_salto(1000), 24);

    /* `targetPos.x = FlxG.width * xPosLerpLol[passo]` -- 640 * 0,245 = 156,8. */
    CHECK_PX(fp_x_do_salto(0), 0.0f);
    CHECK_PX(fp_x_do_salto(6), 156.8f);
    CHECK_PX(fp_x_do_salto(99), 156.8f);

    /* A capsula fica em alpha 0,5 ate' (7/24)*2,5 s = 729 ms e depois em 1. */
    CHECK_EQ(FP_ENTRADA_ESCONDE_MS, 729);

    printf("== freeplay: a animacao da capsula (24 fps, loop)\n");
    /* A cada 1/24 s troca de quadro; 8 quadros por animacao, entao volta ao 0
     * em 1/3 s. O `addByPrefix(..., 24)` do original cicla. */
    CHECK_EQ(fp_quadro(0, 0), 0);
    CHECK_EQ(fp_quadro(0, 41), 0);
    CHECK_EQ(fp_quadro(0, 42), 1);
    CHECK_EQ(fp_quadro(0, 500), 12 % 8);
    CHECK_EQ(fp_quadro(0, 1000), 0);
    /* A selecionada e a nao selecionada tem o mesmo tamanho de ciclo. */
    CHECK_EQ(FP_CAP_SEL_N, 8);
    CHECK_EQ(FP_CAP_NAO_N, 8);
    CHECK_EQ(FP_CAP_FPS, 24);

    printf("== freeplay: o texto da capsula (textAppear)\n");
    {
        float sx = 0.0f, sy = 0.0f;
        fp_texto_escala(0, &sx, &sy);
        CHECK_PX(sx, 1.70f); CHECK_PX(sy, 0.20f);
        fp_texto_escala(41, &sx, &sy);
        CHECK_PX(sx, 1.70f); CHECK_PX(sy, 0.20f);
        fp_texto_escala(42, &sx, &sy);
        CHECK_PX(sx, 0.40f); CHECK_PX(sy, 1.40f);
        fp_texto_escala(124, &sx, &sy);
        CHECK_PX(sx, 0.40f); CHECK_PX(sy, 1.40f);
        fp_texto_escala(125, &sx, &sy);
        CHECK_PX(sx, 1.0f); CHECK_PX(sy, 1.0f);
        fp_texto_escala(100000, 0, 0); /* nao pode estourar com NULL */
    }

    printf("== freeplay: os titulos do pacote\n");
    CHECK(fp_titulo("bopeebo") != 0);
    CHECK(fp_titulo("nao-existe") == 0);
    CHECK(fp_titulo(0) == 0);
    if (fp_titulo("bopeebo"))
    {
        const fp_titulo_t *t = fp_titulo("bopeebo");
        /* O arquivo e' o que o empacotador escreveu em assets/freeplay/. */
        CHECK(t->arquivo != 0 && t->arquivo[0] != 0);
        CHECK(t->w > 0 && t->h > 0);
        /* O maior titulo tem que caber no buffer de leitura do main.c. */
        CHECK(t->vram_bytes <= 24576u);
    }

    printf("== freeplay: o fundo (backingImage) e a entrada de 0,7 s\n");
    {
        /* `FreeplayState.hx` L529-531: x nasce em `-backingImage.width` (574 em
           640x360) e vai para 0 em 700 ms com `FlxEase.quintOut`.
           quintOut(t) = 1 - (1-t)^5. */
        float x0 = fp_fundo_x(0), xf = fp_fundo_x(700), xn = fp_fundo_x(100000);
        CHECK_PX(x0, -574.0f);
        CHECK_PX(xf, 0.0f);
        CHECK_PX(xn, 0.0f);
        /* Em metade do tempo o quintOut ja' passou de 96% do caminho: o valor
           em 350 ms tem de estar entre -574*0,03125 e 0, e mais perto de 0. */
        {
            float xm = fp_fundo_x(350);
            CHECK(xm > -574.0f * 0.04f);
            CHECK(xm < -0.5f);
        }
        /* Monotonico: nunca volta para tras. */
        CHECK(fp_fundo_x(100) < fp_fundo_x(200));
        CHECK(fp_fundo_x(200) < fp_fundo_x(400));
        CHECK(fp_fundo_x(400) < fp_fundo_x(699));
        /* O dado do pacote tem de bater com o que o empacotador escreveu. */
        CHECK(FP_FUNDO_BPP == 16);
        CHECK(FP_FUNDO_W > 0 && FP_FUNDO_H == 360);
        CHECK(FP_FUNDO_VRAM == (unsigned)FP_FUNDO_W * 360u * 2u);
    }

    printf("== freeplay: o brilho do selo PERFECT_GOLD (SongMenuItem.hx)\n");
    {
        /* `sparkleEffect` (L228-233): x = random(x-20, x+3), y = random(y-29,
           y+4) e o proximo disparo em random(1,2 .. 4,5) s; o primeiro vem em
           1 s fixo (L502). Tudo em 640x360, no espaco do item. */
        const float base_x = FP_SP_X, base_y = FP_SP_Y;
        fp_spk_t a, b;
        float t, t_primeiro = 0.0f, t_segundo = 0.0f;
        int k, ultimo = -1, bursts = 0, antes_ok = 1, monotono = 1;

        /* Desarmado: nada a desenhar (rank que nao e' dourado). */
        fp_spk_desarma(&a, 12345u);
        CHECK_EQ(fp_spk_quadro(&a), -1);
        fp_spk_passo(&a, 5.0f, base_x, base_y);
        CHECK_EQ(fp_spk_quadro(&a), -1);
        CHECK_PX(a.x, base_x);
        CHECK_PX(a.y, base_y);

        /* Armado: parado no ULTIMO quadro (a animacao que tocou na criacao ja'
           terminou -- e' o 1x1 vazio), no meio do item. */
        fp_spk_arma(&a, 12345u, base_x, base_y);
        CHECK_EQ(fp_spk_quadro(&a), FP_SP_QUADROS - 1);
        CHECK_PX(a.x, base_x);
        CHECK_PX(a.y, base_y);

        /* O primeiro disparo e' em 1 s: 59 quadros de 1/60 nao chegam, e no
           sexagesimo (1,0 s, ou 61 com a sobra de 32 bits) a animacao ja'
           comecou do quadro 0. */
        for (k = 0; k < 59; k++) fp_spk_passo(&a, 1.0f / 60.0f, base_x, base_y);
        CHECK_PX(a.x, base_x);
        CHECK_PX(a.y, base_y);
        CHECK_EQ(fp_spk_quadro(&a), FP_SP_QUADROS - 1);
        for (k = 0; k < 2 && fp_spk_quadro(&a) != 0; k++)
            fp_spk_passo(&a, 1.0f / 60.0f, base_x, base_y);
        CHECK_EQ(fp_spk_quadro(&a), 0);
        CHECK(a.x >= base_x + FP_SP_DX_MIN && a.x < base_x + FP_SP_DX_MAX);
        CHECK(a.y >= base_y + FP_SP_DY_MIN && a.y < base_y + FP_SP_DY_MAX);

        /* A animacao anda a 24 fps e PARA no ultimo quadro: 10 quadros em
           10/24 s, e nada depois disso ate' o proximo disparo. */
        {
            float acumulado = 0.0f;
            int vistos[FP_SP_QUADROS];
            int i;
            for (i = 0; i < FP_SP_QUADROS; i++) vistos[i] = 0;
            vistos[fp_spk_quadro(&a)] = 1;
            while (acumulado < 0.6f + 0.1f) {
                fp_spk_passo(&a, 1.0f / 240.0f, base_x, base_y);
                acumulado += 1.0f / 240.0f;
                if (acumulado > 0.5f) break;   /* antes do proximo disparo */
                k = fp_spk_quadro(&a);
                if (k != -1) vistos[k] = 1;
                if (k < ultimo) monotono = 0;
                ultimo = k;
            }
            for (i = 0; i < FP_SP_QUADROS; i++) CHECK(vistos[i] == 1);
        }

        /* Uma rajada completa, do zero, medida no relogio: primeiro disparo em
           1 s, o segundo entre 1,2 e 4,5 s depois. E a sequencia e'
           deterministica (mesma semente, mesmo resultado). */
        fp_spk_arma(&a, 777u, base_x, base_y);
        fp_spk_arma(&b, 777u, base_x, base_y);
        {
            float x_antes = base_x, y_antes = base_y;
            for (t = 0.0f; t < 12.0f; t += 1.0f / 240.0f) {
                int qa;
                fp_spk_passo(&a, 1.0f / 240.0f, base_x, base_y);
                fp_spk_passo(&b, 1.0f / 240.0f, base_x, base_y);
                qa = fp_spk_quadro(&a);
                /* A BORDA do disparo: a posicao mudou. O quadro 0 dura 1/24 s
                   (dez passos de 1/240), entao contar todo quadro 0 contaria
                   cada rajada dez vezes. */
                if (a.x != x_antes || a.y != y_antes) {
                    if (bursts == 0) t_primeiro = t + 1.0f / 240.0f;
                    else if (bursts == 1) t_segundo = t + 1.0f / 240.0f;
                    bursts++;
                    if (a.x < base_x + FP_SP_DX_MIN || a.x >= base_x + FP_SP_DX_MAX)
                        antes_ok = 0;
                    if (a.y < base_y + FP_SP_DY_MIN || a.y >= base_y + FP_SP_DY_MAX)
                        antes_ok = 0;
                    x_antes = a.x;
                    y_antes = a.y;
                }
                if (a.x != b.x || a.y != b.y || qa != fp_spk_quadro(&b)) monotono = 0;
            }
        }
        CHECK(antes_ok);
        CHECK(monotono);
        CHECK(t_primeiro >= 59.0f / 60.0f && t_primeiro <= 1.0f + 1.0f / 240.0f);
        CHECK(t_primeiro > 0.0f);
        CHECK(bursts >= 3);        /* 12 s com rajadas a cada 1,2..4,5 s */
        {
            float intervalo = t_segundo - t_primeiro;
            CHECK(intervalo >= FP_SP_INTERVALO_MIN_MS / 1000.0f - 0.001f);
            CHECK(intervalo <= FP_SP_INTERVALO_MAX_MS / 1000.0f + 0.02f);
            /* A distancia entre disparos NAO e' o intervalo do primeiro (1 s):
               e' sorteada. Se fosse a mesma, a tela ficaria metronomica. */
            CHECK(intervalo > 1.05f);
        }

        /* O dado do pacote: 10 quadros, 24 fps, 0,7 de alpha, 16 bpp, e o
           tamanho da VRAM e' o de UM quadro em 16 bpp. */
        CHECK(FP_SP_QUADROS == 10);
        CHECK(FP_SP_FPS == 24);
        CHECK(FP_SPK_QUADROS == FP_SP_QUADROS);
        CHECK(FP_SPK_FPS == FP_SP_FPS);
        CHECK(FP_SP_ALFA > 0.69f && FP_SP_ALFA < 0.71f);
        CHECK(FP_SP_BPP == 16);
        CHECK(FP_SP_VRAM == (unsigned)FP_SP_W * (unsigned)FP_SP_H * 2u);
        CHECK(FP_SP_W > 0 && FP_SP_H > 0);
        CHECK_PX(FP_SP_X, 210.0f);
        CHECK_PX(FP_SP_Y, 20.5f);
    }

    /* ---- O CORACAO DE FAVORITO ------------------------------------------
       `SongMenuItem.hx` L202-219 (as duas copias, ADD, invisiveis), L568-569
       (`curFrame = numFrames - 1`: o repouso e' o ULTIMO quadro), L801-802
       (alfa 1/0,6 no item selecionado/nao) e `FreeplayState.hx` L3079-3130
       (`favoriteSong`: animacao de 13 quadros, reverso do 9, timer de 0,2 s e
       o empurrao de 5 px do item). */
    {
        fp_fav_t f;
        float t;

        /* Sem favorito: nada a desenhar, e o empurrao tambem e' zero. */
        fp_fav_reset(&f);
        CHECK_EQ(fp_fav_visivel(&f), 0);
        CHECK_EQ(fp_fav_quadro(&f), -1);
        CHECK_PX(fp_fav_dy(&f), 0.0f);
        fp_fav_passo(&f, 10.0f);
        CHECK_EQ(fp_fav_visivel(&f), 0);

        /* Favoritada e parada (`refreshDisplay` -> `visible = isFav`): desenha
           o ULTIMO quadro, sem animar e sem empurrao. */
        fp_fav_mostra(&f);
        CHECK_EQ(fp_fav_visivel(&f), 1);
        CHECK_EQ(fp_fav_quadro(&f), FP_FV_QUADROS - 1);
        CHECK_PX(fp_fav_dy(&f), 0.0f);
        fp_fav_passo(&f, 5.0f);
        CHECK_EQ(fp_fav_quadro(&f), FP_FV_QUADROS - 1);

        /* FAVORITAR: do quadro 0 ao 12, uma vez, e PARA no ultimo. */
        fp_fav_toggle(&f, 1);
        CHECK_EQ(fp_fav_visivel(&f), 1);
        CHECK_EQ(fp_fav_quadro(&f), 0);
        {
            int q_ant = fp_fav_quadro(&f), sobe = 1, q_fim;
            for (t = 0.0f; t < 1.2f; t += 1.0f / 240.0f) {
                fp_fav_passo(&f, 1.0f / 240.0f);
                if (fp_fav_quadro(&f) < q_ant) sobe = 0;
                q_ant = fp_fav_quadro(&f);
            }
            q_fim = q_ant;
            CHECK(sobe);
            CHECK_EQ(q_fim, FP_FV_QUADROS - 1);
        }
        /* A duracao da animacao e' `quadros / fps`, e depois dela o coracao
           continua VISIVEL: e' o `looped = false` parando no fim. */
        CHECK_PX(FP_FV_QUADROS / (float)FP_FV_FPS, 13.0f / 24.0f);
        CHECK_EQ(fp_fav_visivel(&f), 1);

        /* O EMPURRAO do item (5 px na fonte, metade na tela): desce no
           favoritar, 0,1 s `expoOut`, 0,1 parado, 0,1 `expoIn` de volta. */
        fp_fav_toggle(&f, 1);
        CHECK_PX(fp_fav_dy(&f), 0.0f);          /* inicio do expoOut */
        fp_fav_passo(&f, 0.15f);
        CHECK_PX(fp_fav_dy(&f), -2.5f);         /* meio do tween, parado  */
        fp_fav_passo(&f, 0.20f);
        CHECK_PX(fp_fav_dy(&f), 0.0f);          /* voltou (t = 0,35)     */

        /* DESFAVORITAR: `play('fav', true, true, 9)` -- REVERSO a partir do
           quadro 9 -- e o `FlxTimer` de 0,2 s esconde os dois. O empurrao vai
           para o lado contrario (sobe). */
        fp_fav_toggle(&f, 0);
        CHECK_EQ(fp_fav_visivel(&f), 1);
        CHECK_EQ(fp_fav_quadro(&f), 9);
        /* Passos de 2,5/24 e 1,4/24 s: cair no MEIO do quadro, para a borda
           do `(int)(t * fps)` nao depender do arredondamento do float. */
        fp_fav_passo(&f, 2.5f / 24.0f);
        CHECK_EQ(fp_fav_quadro(&f), 7);
        fp_fav_passo(&f, 1.4f / 24.0f);
        CHECK_EQ(fp_fav_quadro(&f), 6);
        {
            fp_fav_t g;
            /* O desfavoritar so' acontece numa musica VISIVEL como favorita
               (`favIcon.visible` ja' era `true`): por isso o `mostra` antes. */
            fp_fav_mostra(&g);
            fp_fav_toggle(&g, 0);
            fp_fav_passo(&g, 0.05f);
            /* Metade do expoOut: 2,5 * (1 - 2^-5) = 2,42 -- e do lado
               CONTRARIO ao do favoritar. */
            CHECK(fp_fav_dy(&g) > 2.4f && fp_fav_dy(&g) < 2.5f);
            fp_fav_passo(&g, 0.16f);            /* t = 0,21: o timer vence */
            CHECK_EQ(fp_fav_visivel(&g), 0);
            CHECK_EQ(fp_fav_quadro(&g), -1);
            CHECK_PX(fp_fav_dy(&g), 0.0f);
        }

        /* O dado do pacote e o do sprite. */
        CHECK_EQ(FP_FV_QUADROS, 13);
        CHECK_EQ(FP_FV_FPS, 24);
        CHECK_EQ(FP_FV_BPP, 16);
        CHECK_EQ(FP_FV_VRAM, (unsigned)FP_FV_W * (unsigned)FP_FV_H * 2u);
        CHECK_PX(FP_FV_W, 25.0f);
        CHECK_PX(FP_FV_H, 25.0f);
        CHECK_PX(FP_FV_X, 185.0f);
        CHECK_PX(FP_FV_X_SEM_SELO, 202.5f);
        CHECK_PX(FP_FV_Y, 20.0f);
        CHECK_PX(FP_FV_ALFA_SEL, 1.0f);
        CHECK_PX(FP_FV_ALFA_NAO_SEL, 0.6f);
    }

    if (falhas)
    {
        printf("FREEPLAY: %d FALHA(S)\n", falhas);
        return 1;
    }
    printf("FREEPLAY OK\n");
    return 0;
}
