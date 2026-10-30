/* Maquina de estado do quadro de cada personagem.
 *
 * O que prova: a conta de quadro (taxa, loop, preso no ultimo), a alternancia
 * danceLeft/danceRight, o canto voltando a dancar depois do singTime, e o
 * fallback de erro ("miss" quando existe). O que NAO prova: que a animacao
 * certa foi escolhida em relacao ao que o jogador apertou -- isso e' o
 * gameplay, que ja' tem teste proprio.
 */
#include <stdio.h>
#include <string.h>
#include "char_anim.h"
#include "char_layout.h"

static int falhas;
#define CHECK(c) do { if (!(c)) { printf("  FALHA: %s (linha %d)\n", #c, __LINE__); falhas++; } } while (0)
#define CHECK_EQ(a, b) do { long _a = (long)(a), _b = (long)(b); \
    if (_a != _b) { printf("  FALHA: %s == %s: %ld != %ld (linha %d)\n", #a, #b, _a, _b, __LINE__); falhas++; } } while (0)

static const char *nome_anim(const ch_state_t *s)
{
    const ch_dados_t *c;
    if (s->ci < 0 || s->ci >= CH_CHAR_COUNT) return "<ci invalido>";
    c = &ch_chars[s->ci];
    if (s->anim < 0 || s->anim >= c->anim_count) return "<nenhuma>";
    return ch_anims[c->anim_first + s->anim].name;
}

int main(void)
{
    printf("== quadro de uma sequencia ==\n");
    {
        ch_seq_t q;
        q.first = 100; q.count = 12; q.fps100 = 2400; q.looped = 1;
        CHECK_EQ(ca_frame(&q, 0), 100);
        /* 24 fps: 1 quadro a cada 41,66 ms. */
        CHECK_EQ(ca_frame(&q, 41), 100);
        CHECK_EQ(ca_frame(&q, 42), 101);
        /* Loop: o resto e' sobre a CONTAGEM, nao sobre o quadro preso -- 1 s
           depois de 24 quadros a 24 fps volta ao primeiro. */
        CHECK_EQ(ca_frame(&q, 999), q.first + 11);   /* 23 quadros % 12 */
        CHECK_EQ(ca_frame(&q, 1000), q.first);
        CHECK_EQ(ca_frame(&q, 1250), q.first + 6);   /* 30 % 12 */
        /* Tempo negativo (o relogio do chart e' negativo na contagem) nao pode
           indexar antes do primeiro quadro. */
        CHECK_EQ(ca_frame(&q, -500), q.first);
        /* Sem loop: preso no ultimo, para sempre. */
        q.looped = 0;
        CHECK_EQ(ca_frame(&q, 1000), 100 + 11);
        CHECK_EQ(ca_frame(&q, 999999), 100 + 11);
        /* Taxa desconhecida: primeiro quadro, e nao divisao por zero. */
        q.fps100 = 0;
        CHECK_EQ(ca_frame(&q, 500), 100);
        q.fps100 = -2400;
        CHECK_EQ(ca_frame(&q, 500), 100);
        /* Sequencia vazia/ausente. */
        q.count = 0;
        CHECK_EQ(ca_frame(&q, 500), -1);
        CHECK_EQ(ca_frame(NULL, 500), -1);
        /* 29,97 fps tem de ser exato o suficiente para nao pular quadro. */
        q.first = 0; q.count = 1000; q.fps100 = 2997; q.looped = 1;
        CHECK_EQ(ca_frame(&q, 1000), 29);
        CHECK_EQ(ca_frame(&q, 10000), 299);
    }

    printf("== passos -> ms ==\n");
    {
        /* 1 passo = 1 beat = 60000/bpm ms. A 100 BPM sao 600 ms. */
        CHECK_EQ(ca_passos_ms(1, 100.0f), 600);
        CHECK_EQ(ca_passos_ms(8, 100.0f), 4800);
        CHECK_EQ(ca_passos_ms(8, 150.0f), 3200);
        CHECK_EQ(ca_passos_ms(0, 100.0f), 0);
        CHECK_EQ(ca_passos_ms(8, 0.0f), 0);
        CHECK_EQ(ca_passos_ms(8, -1.0f), 0);
    }

    printf("== canto e volta ao idle (BF) ==\n");
    {
        ch_state_t s;
        int bf = ch_find("bf");
        long sing = ca_passos_ms(8, 100.0f);
        ca_reset(&s, bf);
        CHECK_EQ(s.anim, -1);
        CHECK_EQ(ca_canto(&s, 0, 0, 0), 1);
        CHECK(strcmp(nome_anim(&s), "singLEFT") == 0);
        CHECK_EQ(s.missing, 0);
        /* Repetir o mesmo canto NAO reinicia (senao a nota tremelicaria). */
        CHECK_EQ(ca_canto(&s, 0, 0, 100), 0);
        /* Erro de nota, com animacao de erro existente. */
        CHECK_EQ(ca_canto(&s, 0, 1, 200), 1);
        CHECK(strcmp(nome_anim(&s), "singLEFTmiss") == 0);
        CHECK_EQ(s.missing, 1);
        /* Dentro do singTime, nao volta a dancar. */
        CHECK_EQ(ca_ocioso(&s, 0, 200 + sing - 1, sing), 0);
        CHECK(strcmp(nome_anim(&s), "singLEFTmiss") == 0);
        /* No fim do singTime, volta para o idle. */
        CHECK_EQ(ca_ocioso(&s, 0, 200 + sing, sing), 1);
        CHECK(strcmp(nome_anim(&s), "idle") == 0);
        CHECK_EQ(s.missing, 0);
        /* E ja' no idle, pedir de novo nao troca (o desenho nao reenvia a
           textura a' toa). */
        CHECK_EQ(ca_ocioso(&s, 4, 200 + sing + 1, sing), 0);
    }

    printf("== danceLeft/danceRight (GF) ==\n");
    {
        ch_state_t s;
        int gf = ch_find("gf");
        long sing = ca_passos_ms(8, 100.0f);
        ca_reset(&s, gf);
        /* A GF tem sing (4 direcoes) mas NAO tem idle: ela alterna
           danceLeft/danceRight. Sem animacao nenhuma ainda, o primeiro pedido
           de ociosidade cai no danceLeft. */
        CHECK_EQ(s.anim, -1);
        /* Batida par -> danceLeft, impar -> danceRight. */
        CHECK_EQ(ca_ocioso(&s, 0, 0, sing), 1);
        CHECK(strcmp(nome_anim(&s), "danceLeft") == 0);
        CHECK_EQ(ca_ocioso(&s, 1, 600, sing), 1);
        CHECK(strcmp(nome_anim(&s), "danceRight") == 0);
        CHECK_EQ(ca_ocioso(&s, 2, 1200, sing), 1);
        CHECK(strcmp(nome_anim(&s), "danceLeft") == 0);
        /* A mesma batida duas vezes nao pisca. */
        CHECK_EQ(ca_ocioso(&s, 2, 1800, sing), 0);
        CHECK(strcmp(nome_anim(&s), "danceLeft") == 0);
        /* E o canto dela tambem funciona (as GFs do jogo cantam). */
        CHECK_EQ(ca_canto(&s, 1, 0, 2000), 1);
        CHECK(strcmp(nome_anim(&s), "singDOWN") == 0);
    }

    printf("== personagem que nao canta nao inventa animacao ==\n");
    {
        ch_state_t s;
        int da = ch_find("darnell-blazin");
        ca_reset(&s, da);
        CHECK_EQ(ch_anim_sing(da, 0, 0), -1);
        CHECK_EQ(ca_canto(&s, 0, 0, 0), 0);
        CHECK_EQ(s.anim, -1);
        CHECK_EQ(ca_canto(&s, 2, 1, 100), 0);
        CHECK_EQ(s.anim, -1);
        /* Mas ele tem idle, entao danca. */
        CHECK_EQ(ca_ocioso(&s, 0, 200, 0), 1);
        CHECK(strcmp(nome_anim(&s), "idle") == 0);
    }

    printf("== os 46 personagens atravessam o ciclo ==\n");
    {
        int i, dir;
        long cycle_ok = 0;
        for (i = 0; i < CH_CHAR_COUNT; i++) {
            ch_state_t s;
            const ch_dados_t *c = &ch_chars[i];
            long sing = ca_passos_ms((int)c->sing, 100.0f);
            int temalgo = 0;
            ca_reset(&s, i);
            /* Toda batida de 0..7 tem de dar uma animacao valida para todos os
               46 -- ou -1 quando o personagem realmente nao canta. */
            for (dir = -1; dir < 6; dir++) {
                int trocou;
                long t = (long)(dir + 1) * 1000;
                if (dir < 0) {
                    trocou = ca_ocioso(&s, 0, t, sing);
                } else {
                    trocou = ca_canto(&s, dir, 0, t);
                    /* Direcao 4/5 nao existe: ca_canto tem de devolver 0. */
                    if (dir >= 4) CHECK_EQ(trocou, 0);
                }
                (void)trocou;
                if (s.anim >= 0) {
                    CHECK(s.anim < c->anim_count);
                    temalgo = 1;
                }
            }
            if (temalgo) cycle_ok++;
            ca_ocioso(&s, 3, 99000, sing);
            CHECK(s.anim >= 0);       /* todos tem idle ou dance */
            CHECK(s.anim < c->anim_count);
        }
        CHECK_EQ(cycle_ok, 46);
    }

    if (falhas) { printf("\n  %d falha(s)\n", falhas); return 1; }
    printf("\n  animacao: taxa, loop, canto, idle, danceLeft/danceRight e os 46\n");
    return 0;
}
