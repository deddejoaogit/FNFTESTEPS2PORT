/* Personagens: os 46 existem, os dados batem com o bs_dados.h, e a conta de
 * posicao/camera reproduz os numeros que o console ja' aprovou.
 *
 * O que este teste pode provar no PC: que os dados nao divergiram da fonte,
 * que todo personagem tem o que desenhar, e que a formula usada pelo modulo e'
 * a mesma que produziu o foco de camera (888,50 ; 589,00) do BF no tutorial --
 * numero que veio de captura de tela do console, nao de conta feita aqui.
 *
 * O que ele NAO prova: que o personagem parece certo na tela. Para isso existe
 * a comparacao com evidencia-console/, que so' a tela responde.
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "bs_dados.h"
#include "char_layout.h"

static int falhas;
#define CHECK(c) do { if (!(c)) { printf("  FALHA: %s (linha %d)\n", #c, __LINE__); falhas++; } } while (0)
#define CHECK_EQ(a, b) do { long _a = (long)(a), _b = (long)(b); \
    if (_a != _b) { printf("  FALHA: %s == %s: %ld != %ld (linha %d)\n", #a, #b, _a, _b, __LINE__); falhas++; } } while (0)
#define CHECK_NEAR(a, b, tol) do { double _a = (double)(a), _b = (double)(b); \
    if (fabs(_a - _b) > (tol)) { printf("  FALHA: %s ~= %s: %.4f != %.4f (linha %d)\n", #a, #b, _a, _b, __LINE__); falhas++; } } while (0)

/* O bs_dados.h tem os mesmos campos, gerados do mesmo JSON por outra
   ferramenta (tools/butterscotch/bs.py). Comparar os dois e' o teste que pega
   gerador velho: se um lado mudar e o outro nao, isto acusa. */
static const bs_char_t *bs_char(const char *id)
{
    int i;
    for (i = 0; i < BS_CHAR_COUNT; i++)
        if (strcmp(bs_chars[i].id, id) == 0) return &bs_chars[i];
    return NULL;
}

int main(void)
{
    int i, k;

    printf("== personagens: os 46 existem ==\n");
    CHECK_EQ(CH_CHAR_COUNT, 46);
    CHECK_EQ(CH_CHAR_COUNT, BS_CHAR_COUNT);
    CHECK_EQ(ch_find("bf"), 5);            /* ordem alfabetica do gerador */
    CHECK_EQ(ch_find("gf"), 14);
    CHECK_EQ(ch_find("naoExiste"), -1);
    CHECK_EQ(ch_find(NULL), -1);
    CHECK_EQ(ch_find(""), -1);
    /* "bf" nao pode casar com "bf-car": o id e' o id inteiro. */
    CHECK(ch_find("bf") != ch_find("bf-car"));

    printf("== personagens: offsets batem com o bs_dados.h (46 contra 46) ==\n");
    for (i = 0; i < CH_CHAR_COUNT; i++) {
        const ch_dados_t *c = &ch_chars[i];
        const bs_char_t *b = bs_char(c->id);
        if (!b) {
            printf("  FALHA: %s existe no char_dados.h e nao no bs_dados.h\n", c->id);
            falhas++;
            continue;
        }
        CHECK_NEAR(c->ox, b->ox, 1e-4);
        CHECK_NEAR(c->oy, b->oy, 1e-4);
        CHECK_NEAR(c->cx, b->cx, 1e-4);
        CHECK_NEAR(c->cy, b->cy, 1e-4);
        /* singTime: o bs_dados.h grava 8,0 quando o JSON nao tem o campo. */
        CHECK_NEAR(c->sing ? c->sing : 8.0, b->sing, 1e-4);
    }

    printf("== personagens: todo personagem tem o que desenhar ==\n");
    {
        int com_idle = 0, com_canto = 0, sem_canto = 0;
        for (i = 0; i < CH_CHAR_COUNT; i++) {
            const ch_dados_t *c = &ch_chars[i];
            CHECK(c->anim_count > 0);
            CHECK(c->render >= 0 && c->render <= CH_RENDER_PACKER);
            CHECK(c->anim_first + c->anim_count <= CH_ANIM_COUNT);
            for (k = 0; k < c->anim_count; k++) {
                const ch_anim_dados_t *a = &ch_anims[c->anim_first + k];
                CHECK(a->name && a->name[0]);
                CHECK(a->prefix && a->prefix[0]);
            }
            if (ch_anim_idle(i, 0) >= 0) com_idle++;
            if (ch_anim_sing(i, 0, 0) >= 0) com_canto++; else sem_canto++;
        }
        /* Todos tem idle; 32 cantam e 14 nao (as GFs e as de fundo). Sem esta
           conta, um gerador que perdesse a lista de animacoes passaria. */
        CHECK_EQ(com_idle, 46);
        CHECK_EQ(com_canto, 32);
        CHECK_EQ(sem_canto, 14);
        /* Tipo de desenho: a conta que dimensiona VRAM/RAM depende dela, entao
           ela tambem e' conferida. 7 personagens nao tem renderType no JSON e
           caem em sparrow (DEFAULT_RENDERTYPE do CharacterData.hx), por isso 8
           sparrow e nao 1. O unico packer e' o spirit. */
        {
            int n[5] = { 0, 0, 0, 0, 0 };
            for (i = 0; i < CH_CHAR_COUNT; i++) n[ch_chars[i].render]++;
            CHECK_EQ(n[CH_RENDER_MULTIANIMATEATLAS], 10);
            CHECK_EQ(n[CH_RENDER_MULTISPARROW], 3);
            CHECK_EQ(n[CH_RENDER_ANIMATEATLAS], 24);
            CHECK_EQ(n[CH_RENDER_SPARROW], 8);
            CHECK_EQ(n[CH_RENDER_PACKER], 1);
            CHECK_EQ(ch_chars[ch_find("spirit")].render, CH_RENDER_PACKER);
            CHECK_EQ(ch_chars[ch_find("bf")].render, CH_RENDER_MULTIANIMATEATLAS);/* bf.json: multianimateatlas */
            CHECK_EQ(ch_chars[ch_find("spooky")].render, CH_RENDER_SPARROW);
        }
    }

    printf("== personagens: idle e canto ==\n");
    {
        int bf = ch_find("bf");
        int gf = ch_find("gf");
        CHECK(ch_anim_idle(bf, 0) >= 0);
        CHECK(strcmp(ch_anims[ch_chars[bf].anim_first + ch_anim_idle(bf, 0)].name, "idle") == 0);
        /* A GF nao tem "idle": alterna danceLeft/danceRight pela batida. */
        CHECK(ch_anim_i(gf, "idle") < 0);
        if (ch_anim_idle(gf, 0) >= 0 && ch_anim_idle(gf, 1) >= 0) {
            CHECK(ch_anim_idle(gf, 0) != ch_anim_idle(gf, 1));
            CHECK(strcmp(ch_anims[ch_chars[gf].anim_first + ch_anim_idle(gf, 0)].name, "danceLeft") == 0);
            CHECK(strcmp(ch_anims[ch_chars[gf].anim_first + ch_anim_idle(gf, 1)].name, "danceRight") == 0);
        } else {
            CHECK(0);
        }
        CHECK_EQ(ch_anim_sing(bf, 0, 0), ch_anim_i(bf, "singLEFT"));
        CHECK_EQ(ch_anim_sing(bf, 3, 0), ch_anim_i(bf, "singRIGHT"));
        CHECK_EQ(ch_anim_sing(bf, 9, 0), -1);            /* direcao invalida */
        /* Personagem de fundo nao canta: -1 e' a resposta certa, e nao um
           quadro qualquer. */
        CHECK_EQ(ch_anim_sing(ch_find("darnell-blazin"), 0, 0), -1);
        CHECK_EQ(ch_anim_dir("singLEFT"), 0);
        CHECK_EQ(ch_anim_dir("singUP"), 2);
        CHECK_EQ(ch_anim_dir("singRIGHTmiss"), 3);
        CHECK_EQ(ch_anim_dir("idle"), -1);
        CHECK_EQ(ch_dir_button(0), 0x0080);
        CHECK_EQ(ch_dir_button(3), 0x0020);
        CHECK_EQ(ch_dir_button(-1), 0);
    }

    printf("== personagens: posicao e camera reproduzem o tutorial ==\n");
    {
        /* Numeros do main.c, que a captura do console confirmou:
             BF no slot bf do mainStage: foco (888,50 ; 589,00)
             posicao: (989,5+17)-18-100 = 888,50  /  (885+14-246)+36-100 = 589,00
           O canvas do BF no espaco do original e' 496,920 x 490,920. */
        const bs_slot_t *slot = ch_slot("mainStage", "bf");
        ch_anchor_t a;
        CHECK(slot != NULL);
        if (slot) {
            CHECK_NEAR(slot->x, 989.5, 1e-3);
            CHECK_NEAR(slot->y, 885.0, 1e-3);
            CHECK_NEAR(slot->cx, -100.0, 1e-3);
            CHECK_NEAR(slot->cy, -100.0, 1e-3);
            CHECK_EQ(slot->z, 300);
            ch_anchor(&ch_chars[ch_find("bf")], slot, 496.920f, 490.920f, 8.0f, 1, &a);
            CHECK_NEAR(a.cam_x, 888.50, 0.01);
            /* O port usa 246 para meio canvas; o valor exato e' 245,46. A
               diferenca de 0,54 e' a mesma que o tutorial carrega. */
            CHECK_NEAR(a.cam_y, 589.00, 0.6);
            /* Sprite: pe' no chao do slot, centralizado, menos o pad. */
            CHECK_NEAR(a.sprite_x, 989.5f - 496.920f * 0.5f + 17.0f - 8.0f, 0.01);
            CHECK_NEAR(a.sprite_y, 885.0f - 490.920f + 14.0f - 8.0f, 0.01);
        }
        /* GF: o script dela nao aplica os offsets (gf.hx), por isso
           usa_offsets=0. No slot gf do mainStage: 751,5+0+0 = 751,50. */
        slot = ch_slot("mainStage", "gf");
        CHECK(slot != NULL);
        if (slot) {
            ch_anchor(&ch_chars[ch_find("gf")], slot, 703.300f, 674.850f, 9.0f, 0, &a);
            /* GF: offsets (-12,4) e cameraOffsets (12,8) se anulam no x.
               camera y = 787 + 4 - 674,85/2 + 8 = 461,575. */
            CHECK_NEAR(a.cam_x, 751.50, 0.01);
            CHECK_NEAR(a.cam_y, 787.0f + 4.0f - 674.850f * 0.5f + 8.0f, 0.01);
            /* E o SPRITE dela nao leva os offsets (gf.hx): 751,5-703,3/2-9. */
            CHECK_NEAR(a.sprite_x, 751.5f - 703.300f * 0.5f - 9.0f, 0.01);
            CHECK_NEAR(a.sprite_y, 787.0f - 674.850f - 9.0f, 0.01);
        }
        /* E os 21 palcos tem os tres slots, sempre. */
        for (i = 0; i < BS_STAGE_PROPS_COUNT; i++) {
            const char *roles[3] = { "bf", "gf", "dad" };
            for (k = 0; k < 3; k++)
                CHECK(ch_slot(bs_stage_props[i].id, roles[k]) != NULL);
        }
        CHECK(ch_slot("mainStage", "xx") == NULL);
        CHECK(ch_slot("naoExiste", "bf") == NULL);
        CHECK(ch_slot(NULL, "bf") == NULL);
    }

    printf("== personagens: 46 x 21 x 3 posicoes finitas ==\n");
    {
        /* Nenhuma combinacao pode dar NaN/inf: o desenho com coordenada
           invalida trava o GS, e no PC isso nao apareceria. */
        int casos = 0;
        for (i = 0; i < CH_CHAR_COUNT; i++) {
            int si;
            for (si = 0; si < BS_STAGE_PROPS_COUNT; si++) {
                const char *roles[3] = { "bf", "gf", "dad" };
                for (k = 0; k < 3; k++) {
                    const bs_slot_t *slot = ch_slot(bs_stage_props[si].id, roles[k]);
                    ch_anchor_t a;
                    if (!slot) { CHECK(0); continue; }
                    ch_anchor(&ch_chars[i], slot, 500.0f, 500.0f, 8.0f, 1, &a);
                    CHECK(isfinite(a.sprite_x) && isfinite(a.sprite_y));
                    CHECK(isfinite(a.cam_x) && isfinite(a.cam_y));
                    casos++;
                }
            }
        }
        CHECK_EQ(casos, CH_CHAR_COUNT * BS_STAGE_PROPS_COUNT * 3);
    }

    if (falhas) { printf("\n  %d falha(s)\n", falhas); return 1; }
    printf("\n  personagens: 46 personagens, %d animacoes, %d posicoes conferidas\n",
           CH_ANIM_COUNT, CH_CHAR_COUNT * BS_STAGE_PROPS_COUNT * 3);
    return 0;
}
