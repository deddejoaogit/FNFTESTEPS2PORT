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

static int falhas;
#define CHECK(c) do { if (!(c)) { printf("  FALHA: %s (linha %d)\n", #c, __LINE__); falhas++; } } while (0)
#define CHECK_EQ(a, b) do { long _a = (long)(a), _b = (long)(b); \
    if (_a != _b) { printf("  FALHA: %s == %s: %ld != %ld (linha %d)\n", #a, #b, _a, _b, __LINE__); falhas++; } } while (0)
#define CHECK_NEAR(a, b) do { double _a = (double)(a), _b = (double)(b); \
    if (_a < _b - 0.01 || _a > _b + 0.01) { printf("  FALHA: %s ~= %s: %.3f != %.3f (linha %d)\n", #a, #b, _a, _b, __LINE__); falhas++; } } while (0)

/* O item coletado cujo prop tem este nome, ou NULL. */
static const sl_item_t *item_por_nome(const sl_item_t *it, int n, const char *nome)
{
    int i;
    for (i = 0; i < n; i++)
        if (strcmp(bs_props[it[i].prop].name, nome) == 0) return &it[i];
    return NULL;
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
        CHECK_EQ(n, 3);
        CHECK_EQ(it[0].prop, bs_stage_props[2].first);   /* zIndex 10 = fundo */
        CHECK(strcmp(bs_props[it[0].prop].name, "stageBack") == 0);
        CHECK_NEAR(it[0].x0, -485.0075f);
        CHECK_NEAR(it[0].y0, -241.3550f);
        CHECK_NEAR(it[0].w, 1408.0f);
        CHECK_NEAR(it[0].h, 770.0f);
        /* A textura e' 802x438 (o pacote reduzido), nao 2560x1400. */
        CHECK_EQ(it[0].tex_w, 802);
        CHECK_EQ(it[0].tex_h, 438);
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
                const bs_prop_t *p = &bs_props[it[k2].prop];
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

    printf("== palcos: z crescente na ordem de desenho ==\n");
    for (i = 0; i < BS_STAGE_PROPS_COUNT; i++) {
        sl_item_t it[SL_MAX_ITENS];
        int n = sl_collect(bs_stage_props[i].id, 1000.0f, 800.0f, 1.0f, 1.0f, 0, 0.5f, 1.0f, it, SL_MAX_ITENS);
        CHECK(n >= 0);
        for (k = 1; k < n; k++) CHECK(bs_props[it[k - 1].prop].z <= bs_props[it[k].prop].z);
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
                CHECK_EQ(a[j].z, bs_props[a[j].prop].z);
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

    if (falhas) { printf("\n  %d falha(s)\n", falhas); return 1; }
    printf("\n  palcos: 21 palcos, 157 props, conta de tela e ordem de z conferidas\n");
    return 0;
}
