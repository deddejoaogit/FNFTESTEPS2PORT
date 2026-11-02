/* Layout de palco: a conta que decide o que aparece, onde, e em que ordem.
 *
 * Nao ha' como julgar "ficou bonito" no PC. O que da' para provar aqui e' o
 * que o console nao perdoa: os 21 palcos existem, os 157 props estao na faixa
 * certa de cada palco (o header e' gerado, entao isto pega deriva do gerador),
 * a conta de tela e' a que o port ja' usava no tutorial, e prop fora da tela
 * nao consome pacote.
 */
#include <stdio.h>
#include <string.h>
#include "bs_dados.h"
#include "stage_layout.h"
#include "char_layout.h"
#include "vram_layout.h"
#include "stage_vram_data.h"
#include "stage_script.h"

static int falhas;
#define CHECK(c) do { if (!(c)) { printf("  FALHA: %s (linha %d)\n", #c, __LINE__); falhas++; } } while (0)
#define CHECK_EQ(a, b) do { long _a = (long)(a), _b = (long)(b); \
    if (_a != _b) { printf("  FALHA: %s == %s: %ld != %ld (linha %d)\n", #a, #b, _a, _b, __LINE__); falhas++; } } while (0)
#define CHECK_NEAR(a, b) do { double _a = (double)(a), _b = (double)(b); \
    if (_a < _b - 0.01 || _a > _b + 0.01) { printf("  FALHA: %s ~= %s: %.3f != %.3f (linha %d)\n", #a, #b, _a, _b, __LINE__); falhas++; } } while (0)


/* Bytes que UM quadro do prop ocupa no GS -- a conta do hardware, escrita aqui
 * de novo de proposito (nao e' para concordar com o gerador por construcao):
 *   - a largura do buffer (TBW) anda de 64 em 64 pixels;
 *   - a altura e' a do quadro (o upload escreve exatamente `sh` linhas);
 *   - o endereco do TBP0 e' alinhado a 256 bytes.
 * Foi com sw*sh*2 (sem as duas voltas) que 82 dos 157 props passavam a escrever
 * por cima do prop seguinte no pool. */
static unsigned int bytes_gs(const bs_prop_t *p)
{
    unsigned int w = ((unsigned int)p->sw + 63u) & ~63u;
    unsigned int b = w * (unsigned int)p->sh * 2u;
    return (b + 255u) & ~255u;
}

/* O item coletado cujo prop tem este nome, ou NULL. */
static const sl_item_t *item_por_nome(const sl_item_t *it, int n, const char *nome)
{
    int i;
    for (i = 0; i < n; i++) {
        if (it[i].cor >= 0) continue;      /* retangulo de cor: nao tem prop */
        if (it[i].criado >= 0) continue;   /* sprite do script: idem */
        if (strcmp(bs_props[it[i].prop].name, nome) == 0) return &it[i];
    }
    return NULL;
}

/* z e ord de um item, seja ele prop, retangulo de cor ou sprite do script.
 * O `ord` do sprite de script e' o proprio indice dele somado a uma faixa
 * alta (SL_ORD_CRIADO, de stage_layout.c): no original ele entra no grupo
 * depois de todos os props, e o sort do flixel e' estavel. */
#define ORD_CRIADO 0x100000
static int item_z(const sl_item_t *it)
{
    if (it->cor >= 0) return bs_cores[it->cor].z;
    if (it->criado >= 0) return bs_criados[it->criado].z;
    return bs_props[it->prop].z;
}
static int item_ord(const sl_item_t *it)
{
    if (it->cor >= 0) return bs_cores[it->cor].ord;
    if (it->criado >= 0) return ORD_CRIADO + it->criado;
    return bs_props[it->prop].ord;
}

/* Quantos itens da lista sao retangulos de cor. */
static int contem_cor(const sl_item_t *it, int n, const char *nome)
{
    int i, c = 0;
    for (i = 0; i < n; i++) {
        if (it[i].cor < 0) continue;
        if (strcmp(bs_cores[it[i].cor].name, nome) == 0) c++;
    }
    return c;
}

/* ---- O HOOK DO SCRIPT DO PALCO, ligado a um `ss_t` de verdade ----
 * Ficam fora de main() porque `sl_set_script` guarda o PONTEIRO. */
static ss_t g_tss;
static float ov_alpha(int prop_abs) { return ss_prop_alpha(&g_tss, prop_abs); }
static float ov_x(int prop_abs, float x_dado) { return ss_prop_x(&g_tss, prop_abs, x_dado); }
static int ov_frame(int prop_abs, int *anim, int *frame) { return ss_prop_frame(&g_tss, prop_abs, anim, frame); }
static float ov_cor(int cor_abs) { return ss_cor_alpha(&g_tss, cor_abs); }
static int ov_prop_cor(int prop_abs, unsigned char *rgb) { return ss_prop_cor(&g_tss, prop_abs, rgb); }
static float ov_y(int prop_abs, float y_dado) { return ss_prop_y(&g_tss, prop_abs, y_dado); }
static int ov_ang(int prop_abs, float *graus) { return ss_prop_ang(&g_tss, prop_abs, graus); }
/* Avanca o `ss_t` de teste em passos de 1/60 s. */
static void avanca_tss(float segundos)
{
    int i, n = (int)(segundos * 60.0f + 0.5f);
    for (i = 0; i < n; i++) ss_update(&g_tss, 1.0f / 60.0f);
}

int main(void)
{
    int i, k;

    printf("== palcos: os 21 existem, e o id e' o id inteiro ==\n");
    CHECK_EQ(sl_stage_find("mainStage"), 2);
    CHECK_EQ(sl_stage_find("limoRide"), 0);
    CHECK_EQ(sl_stage_find("mainStageErect"), 3);
    CHECK_EQ(sl_stage_find("mainStageXX"), -1);
    CHECK_EQ(sl_stage_find(""), -1);
    CHECK_EQ(sl_stage_find(NULL), -1);
    for (i = 0; i < BS_STAGE_COUNT; i++) CHECK(sl_stage_find(bs_stages[i].id) >= 0);

    printf("== palcos: as faixas de props particionam bs_props[] sem buraco ==\n");
    /* O gerador escreve first/count por palco. Se alguem reordenar os props e
       esquecer o palco, o desenho pega o prop do palco errado -- em silencio. */
    {
        int cobertos = 0;
        for (i = 0; i < BS_STAGE_PROPS_COUNT; i++) {
            const bs_stage_props_t *st = &bs_stage_props[i];
            CHECK(st->count > 0);
            CHECK(st->first >= 0);
            CHECK(st->first == cobertos);   /* sem buraco entre palcos */
            for (k = 0; k < st->count; k++) {
                const bs_prop_t *p = &bs_props[st->first + k];
                CHECK(strcmp(p->stage, st->id) == 0);
                CHECK(strncmp(p->tex, "stages/", 7) == 0);
                CHECK(p->frames >= 1);
                CHECK(p->sw > 0 && p->sh > 0);
                CHECK(p->anim_count >= 1);
                CHECK(p->anim_first + p->anim_count <= (int)(sizeof(bs_prop_anims) / sizeof(bs_prop_anims[0])));
            }
            cobertos += st->count;
        }
        CHECK_EQ(cobertos, (int)(sizeof(bs_props) / sizeof(bs_props[0])));
        CHECK_EQ(cobertos, 157);
    }

    printf("== palcos: a conta de tela e' a do port (tutorial, camera no BF) ==\n");
    {
        /* Camera do tutorial no foco do BF: (888,50 ; 589,00), zoom 1,1 do
           mainStage, bop 1. sc = 0,5*1,1 = 0,55.

           stageBack: position (-600,-200), scroll 0,9, 2560x1400 no original:
             x0 = 320 + (-600 - 888,5*0,9 + 640*0,9 - 640)*0,55 = -485,0075
             y0 = 180 + (-200 - 589,0*0,9 + 360*0,9 - 360)*0,55 = -241,3550
             w  = 2560*0,55 = 1408      h = 1400*0,55 = 770
           Os numeros estao escritos aqui a mao, e nao recalculados com a mesma
           formula do modulo: senao o teste concordaria com qualquer erro. */
        sl_item_t it[SL_MAX_ITENS];
        int n = sl_collect("mainStage", 888.5f, 589.0f, 1.1f, 1.0f, 0, 0.5f, 0.0f, it, SL_MAX_ITENS);
        CHECK_EQ(n, 3);   /* o mainStage nao tem prop de cor */
        CHECK_EQ(it[0].prop, bs_stage_props[2].first);   /* zIndex 10 = fundo */
        CHECK(strcmp(bs_props[it[0].prop].name, "stageBack") == 0);
        CHECK_NEAR(it[0].x0, -485.0075f);
        CHECK_NEAR(it[0].y0, -241.3550f);
        CHECK_NEAR(it[0].w, 1408.0f);
        CHECK_NEAR(it[0].h, 770.0f);
        /* A textura e' a do PACOTE reduzido, nao 2560x1400. Os numeros saem
           do plano (`assets-data/stage-props.json`, 712x389 para o
           `mainStage/stageBack`), e o teste os escreve a mao de proposito: se
           o empacotador mudar a escala, este numero tem de ser reconferido em
           vez de o teste acompanhar sozinho.
           Historico, para nao parecer numero escolhido: 0,313 era a conta sobre
           o maior quadro; 0,278 veio dos DOIS orcamentos (VRAM e RAM) com a
           arte real; 0,302539 e' o empacotamento dos 21 palcos com a arte
           completa no clone (`week*`/`weekend*` inclusos) -- o canvas do
           `stageBack` subiu para 774x424 e a escala acompanhou. */
        CHECK_EQ(it[0].tex_w, 774);
        CHECK_EQ(it[0].tex_h, 424);
        /* O fundo cobre a tela inteira, senao aparece borda preta. */
        CHECK(it[0].x0 <= 0.0f);
        CHECK(it[0].y0 <= 0.0f);
        CHECK(it[0].x0 + it[0].w >= SL_VIEW_W);
        CHECK(it[0].y0 + it[0].h >= SL_VIEW_H);
        /* Zoom maior deixa o palco maior: 1,1 -> 1,065 e' zoom*0,9... aqui o
           teste e' so' de monotonia, e ele pega troca de sinal. */
        {
            sl_item_t zo[SL_MAX_ITENS];
            int nz = sl_collect("mainStage", 888.5f, 589.0f, 1.2f, 1.0f, 0, 0.5f, 0.0f, zo, SL_MAX_ITENS);
            CHECK(nz > 0);
            CHECK(zo[0].w > it[0].w);
            CHECK(zo[0].x0 < it[0].x0);   /* cresce em torno do centro */
        }
    }


    printf("== palcos: o `scale` do prop entra no tamanho (palco de pixel) ==\n");
    {
        /* O `scale` do JSON do palco (Stage.hx L234-240) e' 6 nos palcos de
           pixel e 1 no mainStage. Ele FALTAVA na conta: `school` saia seis
           vezes menor e inteiro fora da tela, e o sweep pegou isso como
           "palco_entra sem nenhum palco_draw" -- nada desenhado, nada no log
           dizendo por que. */
        int si = sl_stage_find("school");
        const bs_stage_props_t *st = &bs_stage_props[si];
        const bs_slot_t *slot = ch_slot("school", "bf");
        ch_anchor_t a;
        sl_item_t it[SL_MAX_ITENS];
        int n = 0, viu = 0, k2;
        CHECK(st != NULL && slot != NULL);
        if (st && slot) {
            /* Confere o campo antes de confiar nele: se o gerador perder o
               scale, isto falha aqui e nao na tela. */
            for (k2 = 0; k2 < st->count; k2++) {
                const bs_prop_t *p = &bs_props[st->first + k2];
                if (strcmp(p->name, "sky") == 0) {
                    CHECK_NEAR(p->sx, 6.0);
                    CHECK_NEAR(p->sy, 6.0);
                    CHECK_EQ(p->src_w, 466);
                }
                if (strcmp(p->name, "street") == 0) {
                    CHECK_NEAR(p->sx, 6.0);   /* o palco inteiro e' de pixel */
                }
            }
            /* E o mainStage, que nao e' de pixel, tem scale perto de 1 (o JSON
               dele traz 1,1 e 0,9 por prop). Se o gerador passasse a escrever
               6 em todo mundo, isto falha -- e o palco do tutorial cresceria
               seis vezes na tela. */
            {
                int msi = sl_stage_find("mainStage");
                const bs_stage_props_t *mst = &bs_stage_props[msi];
                for (k2 = 0; k2 < mst->count; k2++) {
                    CHECK(bs_props[mst->first + k2].sx > 0.5f);
                    CHECK(bs_props[mst->first + k2].sx < 2.0f);
                }
            }
            ch_anchor(&ch_chars[ch_find("bf")], slot, 496.920f, 490.920f, 8.0f, 1, &a);
            n = sl_collect("school", a.cam_x, a.cam_y, 1.0f, 1.0f, 0, 0.5f, 0.0f, it, SL_MAX_ITENS);
            CHECK(n > 0);
            for (k2 = 0; k2 < n; k2++) {
                const bs_prop_t *p;
                if (it[k2].prop < 0) continue;   /* cor ou sprite de script */
                p = &bs_props[it[k2].prop];
                float esperado_w = (float)p->src_w * p->sx * 0.5f;
                float esperado_h = (float)p->src_h * p->sy * 0.5f;
                CHECK_NEAR(it[k2].w, esperado_w);
                CHECK_NEAR(it[k2].h, esperado_h);
                if (it[k2].x0 < 320.0f && it[k2].x0 + it[k2].w > 320.0f) viu++;
            }
            /* E o palco tem de aparecer: pelo menos um prop cobrindo o centro
               da tela. Sem o scale isto dava zero. */
            CHECK(viu > 0);
        }
    }

    printf("== palcos: z crescente na ordem de desenho (props E cores) ==\n");
    for (i = 0; i < BS_STAGE_PROPS_COUNT; i++) {
        sl_item_t it[SL_MAX_ITENS];
        int n = sl_collect(bs_stage_props[i].id, 1000.0f, 800.0f, 1.0f, 1.0f, 0, 0.5f, 1.0f, it, SL_MAX_ITENS);
        CHECK(n >= 0);
        for (k = 1; k < n; k++) {
            int za = item_z(&it[k - 1]);
            int zb = item_z(&it[k]);
            CHECK(za <= zb);
            /* Para z igual, a ordem e' a do JSON (`ord`): o sort do flixel e'
               estavel e o grupo recebe os props na ordem do arquivo. */
            if (za == zb) CHECK(item_ord(&it[k - 1]) <= item_ord(&it[k]));
        }
    }

    printf("== palcos: prop fora da tela nao entra ==\n");
    {
        /* fastCar do limoRide fica em x=-12600: nunca visivel, nunca deve sair
           deste laco. Em contrapartida o fundo do mesmo palco tem de aparecer. */
        sl_item_t it[SL_MAX_ITENS];
        int n = sl_collect("limoRide", 1000.0f, 800.0f, 0.9f, 1.0f, 0, 0.5f, 0.0f, it, SL_MAX_ITENS);
        int tem_fastcar = 0, tem_fundo = 0;
        CHECK(n > 0);
        for (k = 0; k < n; k++) {
            const bs_prop_t *p = &bs_props[it[k].prop];
            if (strcmp(p->name, "fastCar") == 0) tem_fastcar = 1;
            if (strcmp(p->name, "limoSunset") == 0) tem_fundo = 1;
        }
        CHECK_EQ(tem_fastcar, 0);
        CHECK_EQ(tem_fundo, 1);
        /* E 'max' menor que o numero de props corta, nao estoura. */
        CHECK_EQ(sl_collect("tankmanBattlefield", 1000.0f, 800.0f, 0.9f, 1.0f, 0, 0.5f, 0.0f, it, 1), 1);
    }

    printf("== palcos: animacao e quadro ==\n");
    {
        /* bgLimo: 4 quadros a 24 fps em loop. Em t=0 e' o 0; em t=1/24 o 1;
           em t=4/24 volta ao 0. */
        const bs_prop_t *p = NULL;
        for (i = 0; i < (int)(sizeof(bs_props) / sizeof(bs_props[0])); i++)
            if (strcmp(bs_props[i].stage, "limoRide") == 0 && strcmp(bs_props[i].name, "bgLimo") == 0) p = &bs_props[i];
        CHECK(p != NULL);
        if (p) {
            CHECK_EQ(sl_prop_frame(p, 0, 0.0f), 0);
            CHECK_EQ(sl_prop_frame(p, 0, 1.0f / 24.0f), 1);
            CHECK_EQ(sl_prop_frame(p, 0, 4.0f / 24.0f), 0);
            CHECK_EQ(sl_prop_frame(p, 1, 0.0f), 0);      /* animacao invalida: 0, nao lixo */
            CHECK_EQ(sl_prop_frame(p, -1, 0.0f), 0);
            CHECK_EQ(sl_prop_frame(NULL, 0, 0.0f), 0);
        }
        /* Anima que nao repete para no ultimo quadro (shootingStar: 28). */
        for (i = 0; i < (int)(sizeof(bs_props) / sizeof(bs_props[0])); i++) {
            if (strcmp(bs_props[i].name, "shootingStar") == 0) {
                p = &bs_props[i];
                break;
            }
            p = NULL;
        }
        CHECK(p != NULL);
        if (p) CHECK_EQ(sl_prop_frame(p, 0, 100.0f), bs_prop_anims[p->anim_first].count - 1);
    }

    printf("== palcos: danceEvery e startingAnimation (Bopper.hx) ==\n");
    {
        /* As duas regras saem do Bopper.hx, e o dado separa os props assim:
             danceEvery > 0 -> dance() a cada danceEvery batidas, com
               forceRestart (shouldBop = true nos props);
             danceEvery == 0 -> dance() NUNCA; so' o startingAnimation do JSON
               toca, em loop desde o comeco, e sem ele nada toca.
           Antes desta build o port escolhia danceLeft/danceRight pela paridade
           da batida e nunca reiniciava, entao as 25 de danceEvery 1 andavam
           fora de fase. */
        sl_item_t a[SL_MAX_ITENS], b[SL_MAX_ITENS];
        const sl_item_t *x, *y;
        int n;
        const bs_prop_anim_t *an;

        /* --- danceEvery 1 com danceLeft/danceRight (limoDancer1 do limoRide):
               passo 0 toca danceLeft no quadro 0; uma batida depois (passo 4)
               toca danceRight DE NOVO NO QUADRO 0 (reiniciou); duas batidas
               depois (passo 8) volta ao danceLeft. --- */
        n = sl_collect("limoRide", 1000.0f, 800.0f, 1.0f, 1.0f, 0, 0.5f, 0.0f, a, SL_MAX_ITENS);
        x = item_por_nome(a, n, "limoDancer1");
        CHECK(x != NULL);
        if (x) {
            CHECK_EQ(x->anim, 0);
            CHECK(strcmp(bs_prop_anims[bs_props[x->prop].anim_first + x->anim].name,
                         "danceLeft") == 0);
            CHECK_EQ(x->frame, 0);
        }
        n = sl_collect("limoRide", 1000.0f, 800.0f, 1.0f, 1.0f, 2, 0.5f, 0.25f, b, SL_MAX_ITENS);
        y = item_por_nome(b, n, "limoDancer1");
        CHECK(y != NULL);
        if (y) { CHECK_EQ(y->anim, 0); CHECK_EQ(y->frame, 6); }   /* 0,25 s a 24 fps */
        n = sl_collect("limoRide", 1000.0f, 800.0f, 1.0f, 1.0f, 4, 0.5f, 0.5f, b, SL_MAX_ITENS);
        y = item_por_nome(b, n, "limoDancer1");
        CHECK(y != NULL);
        if (y) {
            CHECK_EQ(y->anim, 1);                                  /* danceRight */
            CHECK_EQ(y->frame, 0);                                 /* reiniciou */
            CHECK(strcmp(bs_prop_anims[bs_props[y->prop].anim_first + y->anim].name,
                         "danceRight") == 0);
        }
        n = sl_collect("limoRide", 1000.0f, 800.0f, 1.0f, 1.0f, 8, 0.5f, 1.0f, b, SL_MAX_ITENS);
        y = item_por_nome(b, n, "limoDancer1");
        CHECK(y != NULL);
        if (y) { CHECK_EQ(y->anim, 0); CHECK_EQ(y->frame, 0); }

        /* --- danceEvery 0 COM startingAnimation (bgLimo): toca em loop desde o
               inicio, e o quadro anda com o tempo de musica. --- */
        n = sl_collect("limoRide", 1000.0f, 800.0f, 1.0f, 1.0f, 0, 0.5f, 0.0f, a, SL_MAX_ITENS);
        x = item_por_nome(a, n, "bgLimo");
        CHECK(x != NULL);
        if (x) {
            an = &bs_prop_anims[bs_props[x->prop].anim_first + x->anim];
            CHECK_EQ(x->frame, 0);
            n = sl_collect("limoRide", 1000.0f, 800.0f, 1.0f, 1.0f, 0, 0.5f,
                           (float)(1.5 / (double)an->fps), b, SL_MAX_ITENS);
            y = item_por_nome(b, n, "bgLimo");
            CHECK(y != NULL);
            if (y) CHECK_EQ(y->frame, 1);
        }

        /* --- danceEvery 0 SEM startingAnimation (phillyTraffic): o JSON nao
               manda tocar nada, entao fica no primeiro quadro. A animacao de
               77 quadros desse prop vem de um script de palco do original, que
               este repositorio nao tem -- e' dado faltando, nao um chute. --- */
        n = sl_collect("phillyStreets", 1840.0f, 608.0f, 1.0f, 1.0f, 40, 0.5f, 10.0f, a, SL_MAX_ITENS);
        x = item_por_nome(a, n, "phillyTraffic");
        CHECK(x != NULL);
        if (x) {
            CHECK_EQ(x->anim, 0);
            CHECK_EQ(x->frame, 0);
            CHECK_EQ(bs_prop_anims[bs_props[x->prop].anim_first].count > 1, 1);
        }

        /* --- danceEvery 2 (sniper do tankmanBattlefieldErect): reinicia a cada
               DUAS batidas, e `idle` nao e' em loop (15 quadros), entao passa
               do fim para no ultimo quadro. --- */
        n = sl_collect("tankmanBattlefieldErect", -240.0f, -165.0f, 1.0f, 1.0f,
                       8, 0.5f, 1.0f, a, SL_MAX_ITENS);
        x = item_por_nome(a, n, "sniper");
        CHECK(x != NULL);
        if (x) { CHECK_EQ(x->anim, 0); CHECK_EQ(x->frame, 0); }
        n = sl_collect("tankmanBattlefieldErect", -240.0f, -165.0f, 1.0f, 1.0f,
                       11, 0.5f, 1.375f, b, SL_MAX_ITENS);
        y = item_por_nome(b, n, "sniper");
        if (y) CHECK_EQ(y->frame, 9);                              /* 0,375 s * 24 */
        n = sl_collect("tankmanBattlefieldErect", -240.0f, -165.0f, 1.0f, 1.0f,
                       15, 0.5f, 1.875f, b, SL_MAX_ITENS);
        y = item_por_nome(b, n, "sniper");
        if (y) CHECK_EQ(y->frame, 14);                             /* parou no ultimo */
        /* e no meio da janela (passo 4) o quadro e' o do tempo decorrido */
        n = sl_collect("tankmanBattlefieldErect", -240.0f, -165.0f, 1.0f, 1.0f,
                       4, 0.5f, 0.5f, b, SL_MAX_ITENS);
        y = item_por_nome(b, n, "sniper");
        if (y) CHECK_EQ(y->frame, 12);

        /* --- antes da musica (passo negativo) vale o startingAnimation do
               JSON, que e' o que o palco tocou ao ser construido. O
               limoDancer1 do limoRide comeca em danceRight (indice 1). --- */
        n = sl_collect("limoRide", 1000.0f, 800.0f, 1.0f, 1.0f, -4, 0.5f, -0.5f, a, SL_MAX_ITENS);
        x = item_por_nome(a, n, "limoDancer1");
        CHECK(x != NULL);
        if (x) CHECK_EQ(x->anim, 1);
    }

    printf("== palcos: props e personagens na MESMA lista, por zIndex ==\n");
    {
        /* Stage.hx L300-330: os tres personagens e os props entram num unico
           grupo ordenado por zIndex, e o z de cada personagem vem do slot do
           palco. O desenho no main.c corta essa lista em fatias por z, com um
           personagem entre cada duas. O que se prova aqui:
             1) o z do item e' o z do prop (o corte nao pode usar outro numero);
             2) a lista sai em z nao-decrescente;
             3) as fatias por z sao CONTIGUAS -- cada corte cai entre dois itens
                vizinhos, nunca no meio de um empate. Se nao fossem, um prop com
                o mesmo z de outro seria desenhado do lado errado do personagem;
             4) existem palcos com props na frente de quem canta, que e' o motivo
                de o corte existir: no tankmanBattlefield os figurantes
                (tankmanAudience0..5, z 1000..1100) ficam acima do BF (885) e do
                DAD (996) daquele palco. */
        int st, j, com_frente = 0, com_personagem_no_meio = 0;
        char nomes[8][24];
        for (st = 0; st < BS_STAGE_COUNT; st++) {
            const char *id = bs_stages[st].id;
            const bs_slot_t *slb = ch_slot(id, "bf");
            sl_item_t a[SL_MAX_ITENS];
            int n, antes = 0;
            float camx = slb ? slb->x + slb->cx : 640.0f;
            float camy = slb ? slb->y + slb->cy : 360.0f;
            n = sl_collect(id, camx, camy, 1.0f, 1.0f, 0, 0.5f, 0.0f, a, SL_MAX_ITENS);
            CHECK(n >= 0);
            for (j = 0; j < n; j++) {
                /* o item carrega o z; conferir contra o DADO pega troca de
                   campo (o rectangulo de cor tem z no bs_cores[]) */
                int z_dado = item_z(&a[j]);
                CHECK_EQ(a[j].z, z_dado);
                if (j) CHECK(a[j].z >= a[j - 1].z);
            }
            /* fatias: para cada z de personagem, o numero de itens abaixo dele
               so' pode crescer, e a fatia [z_{k-1}, z_k) e' contigua. */
            {
                int zs[3], m;
                zs[0] = ch_slot(id, "gf") ? ch_slot(id, "gf")->z : 100;
                zs[1] = ch_slot(id, "dad") ? ch_slot(id, "dad")->z : 200;
                zs[2] = slb ? slb->z : 300;
                for (m = 0; m < 3; m++) {
                    int c = 0, dentro = 0, depois = 0;
                    for (j = 0; j < n; j++) {
                        if (a[j].z < zs[m]) c++;
                    }
                    CHECK(c >= antes);      /* prefixo: fatia vazia ou crescente */
                    /* a fatia e' exatamente os itens de indice [antes, c) */
                    for (j = 0; j < n; j++) {
                        if (j >= antes && j < c) dentro++;
                        else if (a[j].z >= zs[m] && j < c) depois++;
                    }
                    CHECK_EQ(dentro, c - antes);
                    CHECK_EQ(depois, 0);
                    antes = c;
                }
            }
            if (n && a[n - 1].z > (slb ? slb->z : 300)) {
                com_frente++;
                if (com_frente <= 8) snprintf(nomes[com_frente - 1], sizeof(nomes[0]), "%s", id);
            }
            if (n) com_personagem_no_meio++;
        }
        printf("     palcos com props na frente do BF: %d\n", com_frente);
        for (j = 0; j < com_frente && j < 8; j++) printf("       %s\n", nomes[j]);
        CHECK(com_frente >= 1);
        CHECK(com_personagem_no_meio >= 12);   /* o sweep ja' desenhou 12 palcos */
    }

    printf("== palcos: o tankmanBattlefield poe os figurantes na frente ==\n");
    {
        /* Os sete `tankmanAudience` (z 1000..1100) do tankmanBattlefield estao
           ACIMA do BF (z 885 pelo slot) e do DAD (996). Com o palco desenhado
           antes dos personagens -- como era antes deste corte -- os sete
           apareciam atras de quem canta. */
        sl_item_t a[SL_MAX_ITENS];
        const bs_slot_t *slb = ch_slot("tankmanBattlefield", "bf");
        const bs_slot_t *sld = ch_slot("tankmanBattlefield", "dad");
        int n, j, frente = 0, atras = 0;
        CHECK(slb != NULL);
        CHECK(sld != NULL);
        /* O z dos personagens NAO e' 100/200/300 fixo: no tankmanBattlefield o
           BF e o DAD estao em 102 e a GF em 100 (bs_slots[]). */
        CHECK(slb && slb->z == 102);
        CHECK(sld && sld->z == 102);
        CHECK(ch_slot("tankmanBattlefield", "gf") &&
              ch_slot("tankmanBattlefield", "gf")->z == 100);
        n = sl_collect("tankmanBattlefield", slb ? slb->x + slb->cx : 0.0f,
                       slb ? slb->y + slb->cy : 0.0f, 1.0f, 1.0f, 0, 0.5f, 0.0f,
                       a, SL_MAX_ITENS);
        CHECK(n > 0);
        for (j = 0; j < n; j++) {
            if (a[j].z > slb->z) frente++;
            else atras++;
        }
        /* Nem os sete figurantes aparecem nesse enquadramento -- so' os que
           estao na tela entram no sl_collect. O que importa e' que exista prop
           dos DOIS lados do personagem: com todos de um lado so', o corte por z
           nao teria efeito nenhum. */
        printf("     tankmanBattlefield: %d prop(s) na frente do BF, %d atras\n",
               frente, atras);
        CHECK(frente >= 1);
        CHECK(atras >= 1);
    }

    printf("== palcos: limites do GS e o pool de VRAM ==\n");
    {
        /* Dois limites que o console cobra e que o PC nao: a largura/altura
           maxima de textura do GS (1024) e o pool de VRAM do palco. O teste
           mede os DOIS nos dados reais, em vez de confiar no empacotador:
             1) nenhum quadro de prop pode passar de 1024 em nenhum eixo;
             2) nenhum quadro sozinho pode passar do pool;
             3) o pior MOMENTO de cada palco (soma dos props visiveis) tem de
                caber no pool. Nao e' a soma do palco inteiro -- o pool e' por
                quadro, com despejo LRU -- e' a soma do que esta na tela ao
                mesmo tempo, que e' o numero que o console ve.
             4) o sl_collect corta em SL_MAX_ITENS itens. Se um palco tiver mais
                props visiveis que isso, o port desenharia menos que o original
                em silencio: aqui isso e' FALHA, com o numero medido. */
        const unsigned int pool = vl_pool_size_bytes();
        int st, j, pior_itens = 0, cortados = 0;
        unsigned int pior_bytes = 0;
        const char *pior_palco = "";
        CHECK(pool > 0);
        printf("     pool de VRAM do palco: %u B\n", pool);
        for (st = 0; st < BS_STAGE_PROPS_COUNT; st++) {
            const bs_stage_props_t *sp = &bs_stage_props[st];
            unsigned int soma = 0;
            for (j = 0; j < sp->count; j++) {
                const bs_prop_t *p = &bs_props[sp->first + j];
                CHECK(p->sw > 0);
                CHECK(p->sh > 0);
                CHECK(p->sw <= 1024);          /* largura do buffer do GS */
                CHECK(p->sh <= 1024);
                CHECK(bytes_gs(p) <= pool);
                /* O pool tem de reservar EXATAMENTE o que a textura ocupa.
                   Menos que isso e' escrita por cima do prop seguinte; mais e'
                   VRAM jogada fora num console de 4 MB. */
                CHECK_EQ(bs_prop_vram_bytes[sp->first + j], bytes_gs(p));
                soma += bytes_gs(p);
            }
            CHECK_EQ(bs_stage_vram_bytes[st], soma);
        }
        /* Varre a camera por cada prop do palco (olhando para ele) e pelo slot
           do BF: e' onde o jogo de fato para. A soma usa o tamanho ALINHADO que
           o pool reserva, nao sw*sh*2. */
        for (st = 0; st < BS_STAGE_PROPS_COUNT; st++) {
            const char *id = bs_stage_props[st].id;
            const bs_stage_props_t *sp = &bs_stage_props[st];
            int k;
            for (k = -1; k < sp->count; k++) {
                float camx, camy;
                sl_item_t a[SL_MAX_ITENS];
                unsigned int bytes = 0;
                int n, i;
                if (k < 0) {
                    const bs_slot_t *sl = ch_slot(id, k == -1 ? "bf" : "gf");
                    if (!sl) continue;
                    camx = sl->x;
                    camy = sl->y;
                } else {
                    const bs_prop_t *p = &bs_props[sp->first + k];
                    camx = p->x;
                    camy = p->y;
                }
                /* O `sl_collect` para em SL_MAX_ITENS. Se ele devolver
                   exatamente o maximo, nao da' para saber se havia mais um
                   prop na tela -- e um palco assim desenharia menos que o
                   original, calado. Por isso `n == SL_MAX_ITENS` conta como
                   truncamento e reprova o teste. */
                n = sl_collect(id, camx, camy, 1.1f, 1.0f, 0, 0.5f, 0.0f, a, SL_MAX_ITENS);
                if (n < 0) n = 0;
                if (n >= SL_MAX_ITENS) cortados++;
                for (i = 0; i < n; i++) {
                    /* o sprite do script ocupa VRAM no MESMO pool */
                    if (a[i].criado >= 0) bytes += bs_criado_vram_bytes[a[i].criado];
                    else if (a[i].prop >= 0) bytes += bs_prop_vram_bytes[a[i].prop];
                }
                if (n > pior_itens) pior_itens = n;
                if (bytes > pior_bytes) { pior_bytes = bytes; pior_palco = id; }
            }
        }
        printf("     pior momento: %u B em %s; pior numero de props na tela: %d (corte em %d)\n",
               pior_bytes, pior_palco, pior_itens, SL_MAX_ITENS);
        CHECK(pior_bytes <= pool);
        CHECK_EQ(cortados, 0);
    }

    printf("== palcos: props de COR (#RRGGBB) -- retangulos, alpha e z ==\n");
    {
        /* 12 props do dado tem assetPath "#RRGGBB": o Stage.hx L203-216 cria um
           retangulo de `scale` x `scale` pixels (o scale E' o tamanho) e o poe
           na mesma lista por zIndex. Sem eles o palco saia sem o fundo chapado
           -- o `solid` preto do schoolEvil, o ceu do tankmanBattlefield, a neve
           do mallXmas. */
        CHECK_EQ(BS_COR_COUNT, 12);
        {
            int soma = 0;
            for (i = 0; i < BS_STAGE_PROPS_COUNT; i++) {
                CHECK_EQ(bs_stage_cor_first[i], soma);
                CHECK(bs_stage_cor_count[i] >= 0);
                soma += bs_stage_cor_count[i];
                for (k = 0; k < bs_stage_cor_count[i]; k++) {
                    const bs_cor_t *c = &bs_cores[bs_stage_cor_first[i] + k];
                    CHECK(strcmp(c->stage, bs_stage_props[i].id) == 0);
                    CHECK(c->w > 0.0f && c->h > 0.0f);
                    CHECK(c->alpha >= 0.0f && c->alpha <= 1.0f);
                }
            }
            CHECK_EQ(soma, BS_COR_COUNT);
        }
        /* O `solid` do schoolEvil e' preto, cobre a tela e vem antes de tudo. */
        {
            sl_item_t it[SL_MAX_ITENS];
            int n = sl_collect("schoolEvil", 0.0f, 0.0f, 1.0f, 1.0f, 0, 0.5f, 0.0f,
                               it, SL_MAX_ITENS);
            CHECK(n > 0);
            CHECK_EQ(contem_cor(it, n, "solid"), 1);
            /* z 0 e ord 0: tem de ser o PRIMEIRO item da lista. */
            CHECK(it[0].cor >= 0);
            CHECK(strcmp(bs_cores[it[0].cor].name, "solid") == 0);
            CHECK_EQ(bs_cores[it[0].cor].r, 0);
            CHECK_EQ(bs_cores[it[0].cor].g, 0);
            CHECK_EQ(bs_cores[it[0].cor].b, 0);
            CHECK_NEAR(it[0].alpha, 1.0f);
            /* O retangulo e' 2400x2000 no espaco do original, com scroll 0:
               com sc = 0,5 ele cobre a tela 640x360 inteira. */
            CHECK(it[0].x0 <= 0.0f && it[0].y0 <= 0.0f);
            CHECK(it[0].x0 + it[0].w >= SL_VIEW_W);
            CHECK(it[0].y0 + it[0].h >= SL_VIEW_H);
        }
        /* mallXmas: a neve por baixo (snowUnder) e' 243,244,245 e fica no
           z 59 -- acima dos props de fundo, abaixo do z alto. */
        {
            sl_item_t it[SL_MAX_ITENS];
            /* A neve fica em y=800 no espaco do original: com a camera no zero
               ela esta' ABAIXO da tela e nem entra na lista (corte de tela).
               A camera aqui e' a que a mostra, como no palco em jogo. */
            int n = sl_collect("mallXmas", 0.0f, 1200.0f, 1.0f, 1.0f, 0, 0.5f, 0.0f,
                               it, SL_MAX_ITENS);
            const sl_item_t *c = NULL;
            for (k = 0; k < n; k++) {
                if (it[k].cor >= 0 && strcmp(bs_cores[it[k].cor].name, "snowUnder") == 0)
                    c = &it[k];
            }
            CHECK(c != NULL);
            if (c) {
                CHECK_EQ(bs_cores[c->cor].r, 243);
                CHECK_EQ(bs_cores[c->cor].g, 244);
                CHECK_EQ(bs_cores[c->cor].b, 245);
                CHECK_EQ(bs_cores[c->cor].z, 59);
            }
        }
        /* alpha 0 nao entra: o `solidCover` do sserafim (z 9999) e' invisivel no
           instante zero -- o script do palco o poe em alpha 0 e o JSON tambem.
           Se ele entrasse, seria um retangulo PRETO cobrindo a tela inteira. */
        {
            sl_item_t it[SL_MAX_ITENS];
            int n = sl_collect("sserafim", 0.0f, 0.0f, 1.0f, 1.0f, 0, 0.5f, 0.0f,
                               it, SL_MAX_ITENS);
            CHECK_EQ(contem_cor(it, n, "solidCover"), 0);
            CHECK_EQ(contem_cor(it, n, "solid"), 1);
        }
        /* O `additionalLighten` do phillyBlazin: blend ADD e `visible = false` no
           script (phillyBlazin.hxc L76-77), logo alpha 0 no dado. */
        {
            sl_item_t it[SL_MAX_ITENS];
            int n = sl_collect("phillyBlazin", 0.0f, 0.0f, 1.0f, 1.0f, 0, 0.5f, 0.0f,
                               it, SL_MAX_ITENS);
            CHECK_EQ(contem_cor(it, n, "additionalLighten"), 0);
        }
    }

    printf("== palcos: prop com alpha 0 fica de fora ==\n");
    {
        /* 9 props do dado declaram alpha 0 (schoolErect/freaks,
           tankmanBattlefield/clouds2, phillyStreetsErect/paper, os caminhoes e o
           solidCover do sserafim). Desenha-los seria desenhar o que o original
           nao desenha. */
        /* 11 props TEXTURIZADOS com alpha 0 + o `solidCover` do sserafim, que
           e' um retangulo de cor (conferido no bloco de cima). Os 8 do JSON
           (schoolErect/freaks, tankmanBattlefield/clouds2,
           phillyStreetsErect/paper, os caminhoes, ...) mais os 3 que o
           `buildStage` do phillyBlazin esconde (`skyAdditive`, `lightning` e
           `foregroundMultiply`, L66-77) -- medido, e' o que o dado diz. */
        int zerados = 0, cores_zeradas = 0;
        for (i = 0; i < BS_PROP_COUNT; i++) if (bs_props[i].alpha <= 0.0f) zerados++;
        for (i = 0; i < BS_COR_COUNT; i++) if (bs_cores[i].alpha <= 0.0f) cores_zeradas++;
        CHECK_EQ(zerados, 11);
        CHECK_EQ(cores_zeradas, 2);   /* solidCover + additionalLighten (ADD) */
        {
            sl_item_t it[SL_MAX_ITENS];
            int n = sl_collect("tankmanBattlefield", 0.0f, 0.0f, 1.0f, 1.0f, 0,
                               0.5f, 1.0f, it, SL_MAX_ITENS);
            CHECK(item_por_nome(it, n, "clouds2") == NULL);
            /* e o resto do palco continua la' */
            CHECK(item_por_nome(it, n, "mountains") != NULL);
        }
        /* alpha entre 0 e 1 chega ao item, para o desenho usar (0,3 = 0x26). */
        {
            sl_item_t it[SL_MAX_ITENS];
            int n = sl_collect("phillyStreetsErect", 0.0f, 0.0f, 1.0f, 1.0f, 0,
                               0.5f, 1.0f, it, SL_MAX_ITENS);
            const sl_item_t *g1 = item_por_nome(it, n, "grey1");
            CHECK(g1 != NULL);
            if (g1) CHECK_NEAR(g1->alpha, 0.3f);
        }
        /* O ESTADO INICIAL DOS SCRIPTS DE PALCO. O `.hxc` do palco esconde
           props no `buildStage` (`phillyBlazin.hxc` L66/L69/L73/L77:
           `skyAdditive`, `lightning`, `foregroundMultiply` e
           `additionalLighten` com `visible = false`) e deixa outros em alpha
           1e-4 (`spookyMansionErect.hxc` L90-91: `bgLight` e `stairsLight`).
           Antes desta extracao o port desenhava os cinco em alpha 1,0 -- e
           dois deles cobrem a tela inteira (2450x1250 e 1150x1250), entao o
           palco saia lavado. Quem os esconde e' `tools/extrair_blend_palcos.py`
           -> `assets-data/stage-script.json` -> `gen_stage_dados.py`. */
        {
            sl_item_t it[SL_MAX_ITENS];
            int n = sl_collect("phillyBlazin", 0.0f, 0.0f, 1.0f, 1.0f, 0,
                               0.5f, 1.0f, it, SL_MAX_ITENS);
            CHECK(item_por_nome(it, n, "skyAdditive") == NULL);
            CHECK(item_por_nome(it, n, "lightning") == NULL);
            CHECK(item_por_nome(it, n, "foregroundMultiply") == NULL);
            CHECK(item_por_nome(it, n, "additionalLighten") == NULL);
            n = sl_collect("spookyMansionErect", 0.0f, 0.0f, 1.0f, 1.0f, 0,
                           0.5f, 1.0f, it, SL_MAX_ITENS);
            {
                const sl_item_t *bg = item_por_nome(it, n, "bgLight");
                const sl_item_t *st = item_por_nome(it, n, "stairsLight");
                CHECK(bg == NULL || bg->alpha <= 0.001f);
                CHECK(st == NULL || st->alpha <= 0.001f);
            }
            /* E o que o script NAO esconde continua sendo desenhado: sem esta
               linha, "apagou tudo" tambem passaria. */
            CHECK(item_por_nome(it, n, "bgTrees") != NULL);
        }
    }

    printf("== palcos: os sprites que o SCRIPT do palco cria (bs_criados[]) ==\n");
    {
        /* `phillyBlazin.hxc` L79 cria
             new FlxTiledSprite(Paths.image('phillyBlazin/skyBlur'), 4000, 495,
                                true, false)
           em (-700,-120), scrollFactor(0,0), zIndex 10, e L106 faz
             scrollX -= FlxG.elapsed * 35.
           O JSON do palco nao tem nada disso: sem esta tabela o fundo do
           `blazin` simplesmente nao existe (o port desenhava 1 item onde o
           original desenha 2). */
        int soma = 0;
        CHECK_EQ(BS_CRIADO_COUNT, 1);
        for (i = 0; i < BS_STAGE_PROPS_COUNT; i++) {
            CHECK_EQ(bs_stage_criado_first[i], soma);
            CHECK(bs_stage_criado_count[i] >= 0);
            soma += bs_stage_criado_count[i];
            for (k = 0; k < bs_stage_criado_count[i]; k++) {
                CHECK(strcmp(bs_criados[bs_stage_criado_first[i] + k].stage,
                             bs_stage_props[i].id) == 0);
            }
        }
        /* A faixa tem de cobrir a tabela inteira: um sprite criado num palco e
           nao indexado em nenhum lugar seria dado morto. */
        CHECK_EQ(soma, BS_CRIADO_COUNT);
        /* A ordem de bs_stage_props[] e' a alfabetica (a de gen_stage_dados.py),
           e o `si` do sl_collect e' esse indice: phillyBlazin = 7. */
        CHECK(strcmp(bs_stage_props[7].id, "phillyBlazin") == 0);

        {
            sl_item_t it[SL_MAX_ITENS];
            int n = sl_collect("phillyBlazin", 0.0f, 0.0f, 1.0f, 1.0f, 0,
                               0.5f, 0.0f, it, SL_MAX_ITENS);
            const sl_item_t *sky = NULL;
            int ncriados = 0;
            CHECK(n > 0);
            for (i = 0; i < n; i++) {
                if (it[i].criado < 0) continue;
                ncriados++;
                sky = &it[i];
                /* prop e cor e criado sao exclusivos: um item com os tres
                   sentinelas trocados seria desenhado como outra coisa. */
                CHECK_EQ(it[i].prop, -1);
                CHECK_EQ(it[i].cor, -1);
            }
            CHECK_EQ(ncriados, 1);
            if (sky) {
                /* 4000x495 em (-700,-120), com a escala 0,492298 do palco e
                   zoom 1: a tela e' 640x360 e o zoom reduz o mundo pela metade
                   (o `sc` ja' inclui o 0,5 do zoom, como nos props).
                   x0 = 320 + (-700)*sc, w = 4000*sc. */
                float sc = 1.0f * 0.5f * 1.0f;
                CHECK_EQ(sky->criado, 0);
                /* scrollFactor(0,0) corta a camera dos DOIS lados da formula
                   do port -- `x - cam*0 + 640*0 - 640` --, entao o sprite fica
                   onde o script o pos, so' escalado: -700*0,5 = -350. */
                CHECK_NEAR(sky->x0, (-700.0f - 640.0f) * sc + 320.0f);
                CHECK_NEAR(sky->x0, -700.0f * sc);
                CHECK_NEAR(sky->y0, -120.0f * sc);
                CHECK_NEAR(sky->w, 4000.0f * sc);
                CHECK_NEAR(sky->h, 495.0f * sc);
                CHECK_EQ(sky->z, 10);
                CHECK_NEAR(sky->alpha, 1.0f);
                CHECK_EQ(sky->blend, 0);          /* NORMAL: nao e' aditivo */
                /* O periodo da repeticao e' o da IMAGEM (1387 px), nao o do
                   quad (4000): trocar um pelo outro muda o numero de emendas
                   em 2,9x. */
                CHECK_NEAR(sky->tile, 1387.0f);
                /* tex_w/tex_h sao a TEXTURA (o pacote), nao o original: 1387
                   vira 683 na escala 0,492298. O periodo continua 1387. */
                CHECK_NEAR(sky->tex_w, 683.0f);
                CHECK_NEAR(sky->tex_h, 244.0f);
                CHECK_NEAR(sky->tile_off, 0.0f);   /* t = 0 */
            }
        }
        /* O SCROLL: `scrollX -= FlxG.elapsed * 35` -> em t segundos o mundo
           andou -35*t px, na horizontal apenas. */
        {
            sl_item_t a[SL_MAX_ITENS], b[SL_MAX_ITENS];
            const sl_item_t *sa = NULL, *sb = NULL;
            int na = sl_collect("phillyBlazin", 0, 0, 1.0f, 1.0f, 0, 0.5f, 2.0f,
                                a, SL_MAX_ITENS);
            int nb = sl_collect("phillyBlazin", 0, 0, 1.0f, 1.0f, 0, 0.5f, 0.0f,
                                b, SL_MAX_ITENS);
            for (i = 0; i < na; i++) if (a[i].criado >= 0) sa = &a[i];
            for (i = 0; i < nb; i++) if (b[i].criado >= 0) sb = &b[i];
            CHECK(sa != NULL && sb != NULL);
            if (sa && sb) {
                CHECK_NEAR(sa->tile_off, -70.0f);   /* -35 * 2 s */
                CHECK_NEAR(sb->tile_off, 0.0f);
                /* -70 px de mundo, e nao de tela (a conversao e' do desenho). */
                CHECK_NEAR(sa->x0, sb->x0);
            }
        }
        /* scrollFactor(0,0): a camera NAO move este sprite. E' o que separa um
           fundo colado na tela de um prop do palco. */
        {
            sl_item_t a[SL_MAX_ITENS], b[SL_MAX_ITENS];
            const sl_item_t *sa = NULL, *sb = NULL;
            int na = sl_collect("phillyBlazin", 0.0f, 0.0f, 1.0f, 1.0f, 0, 0.5f, 0.0f,
                                a, SL_MAX_ITENS);
            int nb = sl_collect("phillyBlazin", 900.0f, 400.0f, 1.0f, 1.0f, 0, 0.5f, 0.0f,
                                b, SL_MAX_ITENS);
            for (i = 0; i < na; i++) if (a[i].criado >= 0) sa = &a[i];
            for (i = 0; i < nb; i++) if (b[i].criado >= 0) sb = &b[i];
            CHECK(sa != NULL && sb != NULL);
            if (sa && sb) {
                CHECK_NEAR(sa->x0, sb->x0);
                CHECK_NEAR(sa->y0, sb->y0);
            }
        }
        /* zIndex 10 e' o MAIOR do phillyBlazin? Nao: o `phillyForegroundCity`
           tem z 900 e fica na frente. O que importa e' a lista sair ordenada
           com o criado no lugar do z dele -- o chart do `blazin` desenha 2
           itens (o props ja' escondidos nao contam), e o criado e' o ULTIMO
           por z 10 < 900. */
        {
            sl_item_t it[SL_MAX_ITENS];
            int n = sl_collect("phillyBlazin", 0.0f, 0.0f, 1.0f, 1.0f, 0, 0.5f, 0.0f,
                               it, SL_MAX_ITENS);
            int ultimo = -1, pz = -1, pcriado = 0;
            CHECK(n > 0);
            for (i = 0; i < n; i++) {
                if (it[i].z < pz) { printf("  FALHA: lista fora de ordem no z\n"); falhas++; }
                pz = it[i].z;
                ultimo = i;
                if (it[i].criado >= 0) pcriado++;
            }
            CHECK_EQ(pcriado, 1);
            CHECK(ultimo >= 0);
            if (ultimo >= 0) CHECK(it[ultimo].criado < 0);  /* o da frente e' o prop */
        }
        /* VRAM: o sprite do script tem de caber na conta do palco. O numero do
           header e' a conta do hardware (TBW de 64 em 64, TBP0 a 256 B). */
        {
            const bs_criado_t *c = &bs_criados[0];
            CHECK_EQ(bs_criado_vram_bytes[0],
                     (((((unsigned)683u + 63u) & ~63u) * 244u * 2u) + 255u) & ~255u);
            CHECK_EQ(c->sw, 683);
            CHECK_EQ(c->sh, 244);
            CHECK(c->sw <= 1024);                 /* largura maxima do GS */
        }
        /* E so' o phillyBlazin tem sprite de script hoje: os outros 20 palcos
           nao podem ganhar item a mais por causa disto. */
        for (i = 0; i < BS_STAGE_PROPS_COUNT; i++) {
            if (bs_stage_criado_count[i] != 0) continue;
            {
                sl_item_t it[SL_MAX_ITENS];
                int n = sl_collect(bs_stage_props[i].id, 0.0f, 0.0f, 1.0f, 1.0f, 0,
                                   0.5f, 0.0f, it, SL_MAX_ITENS);
                int m;
                if (n < 0) continue;
                for (m = 0; m < n; m++) {
                    if (it[m].criado >= 0) {
                        printf("  FALHA: %s ganhou sprite de script\n", bs_stage_props[i].id);
                        falhas++;
                    }
                }
            }
        }
    }

    printf("== palcos: o que o SCRIPT do palco acende (relampago do phillyBlazin) ==\n");
    {
        /* O layout nao interpreta HScript: ele PERGUNTA ao `stage_script`
           (registrado por `sl_set_script`). Aqui o teste registra os callbacks
           ligados a um `ss_t` de verdade e cobra o resultado no LAYOUT -- que
           e' onde o alpha/x/quadro viram desenho. */
        static const sl_script_t hook = { ov_alpha, ov_x, ov_frame, ov_cor, ov_prop_cor,
                                          ov_y, ov_ang };
        sl_item_t it[SL_MAX_ITENS];
        int n;

        ss_init(&g_tss, "phillyBlazin");
        sl_set_script(&hook);

        /* t = 0: os tres props do relampago continuam apagados (alpha 0 do
           dado + 0 do script) -> a lista e' a de antes. */
        n = sl_collect("phillyBlazin", 0.0f, 0.0f, 1.0f, 1.0f, 0, 0.5f, 0.0f,
                       it, SL_MAX_ITENS);
        CHECK_EQ(n, 2);                       /* phillyForegroundCity + scrollingSky */
        CHECK(item_por_nome(it, n, "skyAdditive") == NULL);
        CHECK(item_por_nome(it, n, "lightning") == NULL);
        CHECK(item_por_nome(it, n, "foregroundMultiply") == NULL);
        CHECK_EQ(contem_cor(it, n, "additionalLighten"), 0);

        /* o primeiro raio cai em 3,0 s (L93): os tres acendem, e o retangulo
           do `additionalLighten` tambem (0,3) -- 6 itens na lista. */
        {
            int i2, raio = 0;
            for (i2 = 0; i2 < 200 && !raio; i2++) raio = ss_update(&g_tss, 1.0f / 60.0f);
            CHECK_EQ(raio, 1);
        }
        n = sl_collect("phillyBlazin", 0.0f, 0.0f, 1.0f, 1.0f, 0, 0.5f, g_tss.t,
                       it, SL_MAX_ITENS);
        CHECK_EQ(n, 6);
        {
            const sl_item_t *sky = item_por_nome(it, n, "skyAdditive");
            const sl_item_t *lig = item_por_nome(it, n, "lightning");
            const sl_item_t *fg = item_por_nome(it, n, "foregroundMultiply");
            CHECK(sky != NULL);
            CHECK(lig != NULL);
            CHECK(fg != NULL);
            if (sky) {
                CHECK_NEAR(sky->alpha, 0.7f);
                CHECK_EQ(sky->z, 11);
            }
            if (fg) {
                CHECK_NEAR(fg->alpha, 0.64f);
                CHECK_EQ(fg->z, 31);
            }
            if (lig) {
                CHECK_NEAR(lig->alpha, 1.0f);
                CHECK_EQ(lig->z, 20);
                /* o numero de quadros da `strike` tem de existir no prop */
                CHECK(lig->anim >= 0);
                CHECK(lig->anim < bs_props[lig->prop].anim_count);
                CHECK(lig->frame >= 0);
                /* e o x e' o SORTEADO, convertido para a tela com a MESMA
                   conta dos outros props: scrollFactor 0 zera a camera dos dois
                   lados, entao x0 = 320 + (x - 640) * sc, com sc = 0,5*zoom
                   (aqui zoom 1 -> 0,5). */
                CHECK_NEAR(lig->x0, 320.0f + (g_tss.raio_x - 640.0f) * 0.5f);
            }
            CHECK_EQ(contem_cor(it, n, "additionalLighten"), 1);
            /* a cor entra com o alpha do SCRIPT (0,3), nao com o 0 do dado */
            {
                int m;
                for (m = 0; m < n; m++) {
                    if (it[m].cor < 0) continue;
                    if (strcmp(bs_cores[it[m].cor].name, "additionalLighten") == 0)
                        CHECK_NEAR(it[m].alpha, 0.3f);
                }
            }
            /* e a ordem por z continua valendo com os seis */
            {
                int m;
                for (m = 1; m < n; m++) CHECK(item_z(&it[m - 1]) <= item_z(&it[m]));
            }
        }

        /* depois de 1,5 s o `cleanupLightning` apaga os quatro: volta a 2. */
        {
            int i2;
            for (i2 = 0; i2 < 100; i2++) ss_update(&g_tss, 1.0f / 60.0f);
            CHECK_EQ(g_tss.raio_ativo, 0);
        }
        n = sl_collect("phillyBlazin", 0.0f, 0.0f, 1.0f, 1.0f, 0, 0.5f, g_tss.t,
                       it, SL_MAX_ITENS);
        CHECK_EQ(n, 2);

        /* sem registro, o dado manda de novo (nenhum prop a mais) */
        sl_set_script(NULL);
        n = sl_collect("phillyBlazin", 0.0f, 0.0f, 1.0f, 1.0f, 0, 0.5f, 0.0f,
                       it, SL_MAX_ITENS);
        CHECK_EQ(n, 2);

        /* e um palco SEM script nao ganha nada com os callbacks registrados:
           o `ss_t` de outro palco responde "sem override" a tudo. */
        sl_set_script(&hook);
        ss_init(&g_tss, "mainStage");
        n = sl_collect("mainStage", 0.0f, 0.0f, 1.0f, 1.0f, 0, 0.5f, 0.0f,
                       it, SL_MAX_ITENS);
        {
            /* a lista do mainStage sem script: medida no proprio teste acima */
            sl_item_t it2[SL_MAX_ITENS];
            int n2;
            sl_set_script(NULL);
            n2 = sl_collect("mainStage", 0.0f, 0.0f, 1.0f, 1.0f, 0, 0.5f, 0.0f,
                            it2, SL_MAX_ITENS);
            CHECK_EQ(n, n2);
        }
        sl_set_script(NULL);
    }

    printf("== layout: o TREM e as LUZES do phillyTrain ==\n");
    {
        /* O `train` e' UM sprite que o script empurra para a esquerda (L127):
           a lista tem de mostrar o x do script, e nao o do JSON, assim que ele
           anda. As `lights` ganham a COR do L149 no item -- se ela nao
           chegasse aqui, o prop sairia branco e a troca de cor por beat nao
           apareceria na tela. */
        static const sl_script_t hook2 = { ov_alpha, ov_x, ov_frame, ov_cor, ov_prop_cor,
                                           ov_y, ov_ang };
        sl_item_t it[SL_MAX_ITENS];
        int n, achou_trem = 0, achou_luz = 0;
        ss_init(&g_tss, "phillyTrain");
        sl_set_script(&hook2);
        n = sl_collect("phillyTrain", 0.0f, 0.0f, 1.0f, 1.0f, 0, 0.5f, 0.0f,
                       it, SL_MAX_ITENS);
        CHECK(n > 0);
        {
            const sl_item_t *t = item_por_nome(it, n, "train");
            const sl_item_t *l = item_por_nome(it, n, "lights");
            /* O trem nasce em x = 2000, FORA da tela: ele entra pela direita
               andando. Se ele aparecesse aqui, o corte de fora-de-tela estaria
               errado (e o prop custaria pacote em todo quadro). */
            CHECK(t == NULL);
            CHECK(l != NULL);
            if (l) {
                achou_luz = 1;
                /* antes do beat 0 nao ha' cor sorteada: o prop sai como sempre */
                CHECK_EQ(l->pintado, 0);
            }
        }
        CHECK(achou_luz);

        /* O trem anda: o x do item tem de seguir o script, pela MESMA conta dos
           outros props (320 + (x - 640) * 0,5 com camera em 0 e scrollFactor 1,
           que e' o do `train` no JSON). No instante ZERO o trem esta' em
           x = 2000 e NAO aparece -- ele entra pela direita andando, como no
           original; por isso o x aqui e' um que ja' caiu na tela. */
        ss_beat(&g_tss, 0, "pico", 1);            /* beat 0: sorteia a cor */
        g_tss.trem_x = 300.0f;                    /* como o updateTrainPos faz */
        g_tss.trem_comecou = 1;
        /* Camera do jogo (o `cameraFollowPoint`), nao a do canto: com
           cam_y = 0 o trem em y = 360 ficaria no rodape da tela. */
        n = sl_collect("phillyTrain", 500.0f, 300.0f, 1.0f, 1.0f, 4, 0.5f, 1.0f,
                       it, SL_MAX_ITENS);
        {
            const sl_item_t *t = item_por_nome(it, n, "train");
            const sl_item_t *l = item_por_nome(it, n, "lights");
            CHECK(t != NULL);
            if (t) {
                achou_trem = 1;
                /* 320 + (x - cam_x) * 0,5 com scrollFactor 1 */
                CHECK_NEAR(t->x0, 320.0f + (g_tss.trem_x - 500.0f) * 0.5f);
            }
            CHECK(l != NULL);
            if (l) {
                /* A cor do item e' a do script, canal por canal (tabela do
                   L28-34 repetida aqui de proposito). */
                static const unsigned char esperado[5][3] = {
                    { 0x31, 0xA2, 0xFD }, { 0x31, 0xFD, 0x8C }, { 0xFB, 0x33, 0xF5 },
                    { 0xFB, 0xA6, 0x33 }, { 0xFD, 0x45, 0x31 }
                };
                CHECK_EQ(l->pintado, 1);
                CHECK(g_tss.luz_cor >= 0 && g_tss.luz_cor < 5);
                CHECK_EQ(l->rgb[0], esperado[g_tss.luz_cor][0]);
                CHECK_EQ(l->rgb[1], esperado[g_tss.luz_cor][1]);
                CHECK_EQ(l->rgb[2], esperado[g_tss.luz_cor][2]);
            }
        }
        CHECK_EQ(achou_trem, 1);
        sl_set_script(NULL);
    }

    printf("== layout: os CARROS do phillyStreets (angulo e y do script) ==\n");
    {
        /* O `phillyStreets` e' o primeiro palco que GIRA um prop e que move em
           x E y: os dois carros andam numa Bezier com `angle` do `FlxTween`.
           Aqui o `ss_t` e' de verdade -- e' o mesmo que o main.c registra -- e o
           que se cobra e' que o ITEM do layout saia com o angulo e o y do
           script, e nao com os do JSON. */
        static const sl_script_t hook3 = { ov_alpha, ov_x, ov_frame, ov_cor,
                                           ov_prop_cor, ov_y, ov_ang };
        sl_item_t it[SL_MAX_ITENS];
        int n, b, saiu = 0;
        ss_init(&g_tss, "phillyStreets");
        sl_set_script(&hook3);
        /* roda beats ate' um carro sair (o `bool(10)` do L493) */
        for (b = 1; b < 200 && !saiu; b++) {
            ss_beat(&g_tss, b, "2hot", 1);
            saiu = g_tss.carro[0].ativo;
        }
        CHECK_EQ(saiu, 1);
        /* meio do tween: x, y e angulo longe dos do JSON */
        avanca_tss(g_tss.carro[0].dur * 0.5f);
        /* A camera do palco: os personagens do `phillyStreets` ficam em
           x ~ 1800 e y ~ 900 no espaco do original, e e' por volta dai' que a
           camera do jogo para. Com a camera do canto (0,0) o carro nem entra na
           lista -- o corte de tela o descarta, e o teste cobraria o vazio. */
        n = sl_collect("phillyStreets", 1800.0f, 500.0f, 1.0f, 1.0f, 8, 0.5f,
                       g_tss.t, it, SL_MAX_ITENS);
        {
            const sl_item_t *c = item_por_nome(it, n, "phillyCars");
            CHECK(c != NULL);
            if (c) {
                CHECK_NEAR(c->ang, g_tss.carro[0].ang);
                CHECK(c->ang != 0.0f);
                /* y do script: a mesma formula de tela dos outros props */
                CHECK_NEAR(c->y0, 180.0f + (g_tss.carro[0].y - 500.0f) * 0.5f);
            }
        }
        /* e um prop que o script NAO gira fica em 0 -- nao ha' angulo vazando
           de um prop para o outro */
        {
            const sl_item_t *h = item_por_nome(it, n, "phillyHighway");
            CHECK(h != NULL);
            if (h) CHECK_NEAR(h->ang, 0.0f);
        }
        sl_set_script(NULL);
    }

    if (falhas) { printf("\n  %d falha(s)\n", falhas); return 1; }
    printf("\n  palcos: 21 palcos, 157 props, conta de tela e ordem de z conferidas\n");
    return 0;
}
