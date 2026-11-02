#include "stage_layout.h"

#include <string.h>

int sl_stage_find(const char *stage)
{
    int i;
    if (!stage) return -1;
    for (i = 0; i < BS_STAGE_PROPS_COUNT; i++)
        if (strcmp(bs_stage_props[i].id, stage) == 0) return i;
    return -1;
}

int sl_stage_prop_count(const char *stage)
{
    int i = sl_stage_find(stage);
    return i < 0 ? -1 : bs_stage_props[i].count;
}

int sl_stage_first_prop(const char *stage)
{
    int i = sl_stage_find(stage);
    return i < 0 ? -1 : bs_stage_props[i].first;
}

int sl_stage_cor_count(const char *stage)
{
    int i = sl_stage_find(stage);
    return i < 0 ? -1 : bs_stage_cor_count[i];
}

int sl_stage_first_cor(const char *stage)
{
    int i = sl_stage_find(stage);
    return i < 0 ? -1 : bs_stage_cor_first[i];
}

int sl_prop_frame(const bs_prop_t *p, int anim, float t)
{
    const bs_prop_anim_t *a;
    float fps;
    int n, f;

    if (!p || anim < 0 || anim >= p->anim_count) return 0;
    a = &bs_prop_anims[p->anim_first + anim];
    n = a->count > 0 ? a->count : 1;
    fps = a->fps > 0.0f ? a->fps : 24.0f;
    if (t < 0.0f) t = 0.0f;
    f = (int)(t * fps);
    if (a->looped) return f % n;
    return f < n ? f : n - 1;
}

/* Qual animacao tocar, e em que instante comecou -- Bopper.hx.

   Sao dois regimes, e o dado separa os props exatamente assim:

   `danceEvery > 0` (25 props com danceEvery 1, 2 com 2): `onStepHit` chama
   `dance()` quando `step % (danceEvery * STEPS_PER_BEAT) == 0`, com
   `forceRestart = shouldBop`, que nos props e' TRUE. Ou seja: a animacao
   REINICIA a cada danceEvery batidas. Alterna `danceLeft`/`danceRight` quando
   o prop tem as duas (`shouldAlternate = hasAnimation('danceLeft')`), senao
   toca `idle`. O contador comeca em `hasDanced = false`, e a primeira chamada
   (passo 0) toca `danceLeft`.

   `danceEvery == 0` (132 props): `dance()` NUNCA e' chamado. O que toca e'
   so' o `startingAnimation` do JSON (Stage.hx L308-311), em loop desde o
   comeco da musica; sem `startingAnimation`, nada e' tocado e o sprite mostra
   o PRIMEIRO quadro do atlas, parado. 107 dos props nessa situacao tem um
   quadro so', logo a diferenca nao aparece neles; 7 tem varios quadros e sao
   animados por script de palco do original (phillyTraffic e' um deles), script
   que este repositorio nao tem -- por isso eles ficam parados e aparecem no
   log como `prop_sem_inicio`, em vez de o port inventar um movimento que o
   dado nao pede.

   Antes desta build o port escolhia danceLeft/danceRight pela PARIDADE da
   batida para todo prop, e calculava o quadro pelo tempo global: nenhuma
   animacao reiniciava, entao as 25 de danceEvery 1 andavam fora de fase com o
   resto da musica, e as 2 de danceEvery 2 trocavam de lado duas vezes mais
   rapido que o original. */
/* O estado do script do palco, registrado pelo main.c (ver stage_layout.h).
   Fica nulo nos testes e nos palcos sem script: nesse caso o DADO manda, que e'
   o comportamento que ja' existia. */
static const sl_script_t *g_script;

void sl_set_script(const sl_script_t *script)
{
    g_script = script;
}

/* Acha a animacao `nome` + `suf` (o `idleSuffix` do Bopper). Com sufixo, o
   nome COM sufixo ganha do nome puro -- a ordem importa: procurar o puro
   primeiro acharia `danceLeft` e o `freaks` da `roses` dancaria sem o susto. */
static int sl_acha_nome(const bs_prop_t *p, const char *nome, const char *suf)
{
    int i;
    if (suf && *suf) {
        char alvo[64];
        int a = (int)strlen(nome), b = (int)strlen(suf);
        if (a + b < (int)sizeof(alvo)) {
            memcpy(alvo, nome, (size_t)a);
            memcpy(alvo + a, suf, (size_t)b + 1);
            for (i = 0; i < p->anim_count; i++) {
                if (strcmp(bs_prop_anims[p->anim_first + i].name, alvo) == 0)
                    return i;
            }
        }
    }
    for (i = 0; i < p->anim_count; i++) {
        if (strcmp(bs_prop_anims[p->anim_first + i].name, nome) == 0) return i;
    }
    return -1;
}

static int sl_escolhe_anim(const bs_prop_t *p, int prop_abs, int step,
                           float beat_s, float t, float *fase)
{
    int esq, dir, idle;
    int janela, resto, chamadas;
    /* O `idleSuffix` do script do palco (`stage_script.c`, `ss_prop_sufixo`):
       o `Bopper.dance()` do flixel procura `danceLeft`/`danceRight` + o sufixo,
       e cai no nome puro quando ele nao existe. */
    const char *suf = (g_script && g_script->sufixo) ? g_script->sufixo(prop_abs) : "";

    esq = sl_acha_nome(p, "danceLeft", suf);
    dir = sl_acha_nome(p, "danceRight", suf);
    idle = sl_acha_nome(p, "idle", suf);

    /* Antes do passo 0 (contagem regressiva) vale o `startingAnimation`, que e'
       o que o palco tocou ao ser construido. */
    if (step < 0) {
        *fase = t > 0.0f ? t : 0.0f;
        return p->start >= 0 ? p->start : (idle >= 0 ? idle : 0);
    }

    if (p->dance > 0) {
        janela = p->dance * 4;             /* STEPS_PER_BEAT = 4 */
        resto = step % janela;
        chamadas = step / janela;
        /* Tempo dentro da janela: `beat_s` e' a duracao de UMA batida na
           secao de BPM vigente, entao um passo e' beat_s/4. */
        *fase = beat_s > 0.0f ? (float)resto * (beat_s * 0.25f) : 0.0f;
        if (esq >= 0 && dir >= 0) return (chamadas & 1) ? dir : esq;
        if (idle >= 0) return idle;
        return 0;
    }

    /* Sem danca: o `startingAnimation`, em loop desde o inicio. Sem ele, o
       primeiro quadro do atlas e nada mais. */
    if (p->start < 0) {
        *fase = 0.0f;
        return idle >= 0 ? idle : 0;
    }
    *fase = t > 0.0f ? t : 0.0f;
    return p->start;
}

/* Posicao do item na lista `props` do JSON do palco (ver bs_prop_t.ord).
   Os sprites do script do palco vao DEPOIS de tudo com o mesmo z: no original
   eles sao `add()`ados ao grupo no `buildStage`, que ja' chamou o
   `super.buildStage()` (phillyBlazin.hxc L79-86), e a ordenacao do flixel e'
   estavel por zIndex. */
#define SL_ORD_CRIADO (1 << 20)
static int sl_item_ord(const sl_item_t *it)
{
    if (it->cor >= 0) return bs_cores[it->cor].ord;
    if (it->criado >= 0) return SL_ORD_CRIADO;
    return bs_props[it->prop].ord;
}

int sl_collect(const char *stage, float cam_x, float cam_y, float zoom,
               float bop, int step, float beat_s, float t, sl_item_t *out, int max)
{
    const bs_stage_props_t *st;
    int si, i, n = 0, j;
    float sc;

    if (!out || max <= 0) return -1;
    si = sl_stage_find(stage);
    if (si < 0) return -1;
    st = &bs_stage_props[si];

    /* 0,5 = metade do 1280x720 do original; o bop e' o pulso da camera
       (PlayState.camZooming: 1,015 e 0,9 nas batidas). */
    sc = 0.5f * zoom * bop;
    if (sc <= 0.0f) return 0;

    for (i = 0; i < st->count && n < max; i++) {
        const bs_prop_t *p = &bs_props[st->first + i];
        sl_item_t *it = &out[n];
        int anim;
        int prop_abs = st->first + i;
        float fase, x0, y0, w, h, alpha, px, py, ang = 0.0f;

        /* O SCRIPT DO PALCO manda no alpha e no x quando mexe neste prop
           (`phillyBlazin.hxc`: os tres do relampago nascem com alpha 0 e sao
           acesos por 1,5 s). Sem script registrado, `alpha` e `px` sao os do
           dado e nada muda. */
        alpha = g_script && g_script->alpha ? g_script->alpha(prop_abs) : -1.0f;
        if (alpha < 0.0f) alpha = p->alpha;
        px = p->x;
        if (g_script && g_script->x) px = g_script->x(prop_abs, p->x);
        py = p->y;
        if (g_script && g_script->y) py = g_script->y(prop_abs, p->y);

        /* Prop com alpha 0 e' invisivel no original (9 dos 169: freaks,
           clouds2, paper, os caminhoses do sserafim...) -- ou esta' apagado
           agora pelo script. Nao entra na lista: nem pacote, nem VRAM, nem
           desenho. */
        if (alpha <= 0.0f) continue;

        /* Formula do port (ver o cabecalho): o termo (640*f - 640) existe
           porque o scrollFactor menor que 1 desloca o prop em relacao ao
           canto da camera, nao ao centro. */
        x0 = SL_VIEW_W * 0.5f + (px - cam_x * p->scrollx + 640.0f * p->scrollx - 640.0f) * sc;
        y0 = SL_VIEW_H * 0.5f + (py - cam_y * p->scrolly + 360.0f * p->scrolly - 360.0f) * sc;
        /* O tamanho NA TELA vem de src_w/src_h (o tamanho do quadro no espaco
           do original) VEZES o `scale` do prop, nao de sw/sh. Duas escalas
           diferentes, e trocar uma pela outra ja' custou duas vezes:
             sw/sh    -> resolucao do PACOTE, reduzida pelo empacotador para
                         caber na VRAM (0,31 no mainStage);
             src_w/h  -> tamanho do quadro no jogo original;
             sx/sy    -> o `scale` do JSON do palco (Stage.hx L234-240 faz
                         propSprite.scale.set(value)), que nos palcos de pixel
                         vale 6.
           Sem o sx/sy, os palcos de pixel (school, schoolEvil, ...) saiam seis
           vezes menores e FORA da tela: medido no sweep, `palco_entra` sem
           nenhum `palco_draw` -- nada era desenhado e nada dizia por que. */
        w = (float)p->src_w * p->sx * sc;
        h = (float)p->src_h * p->sy * sc;

        /* Fora da tela nao entra no pacote. 'fastCar' do limoRide fica em
           x=-12600: sem este corte, um prop invisivel custaria uma textura
           por quadro e um quad. */
        if (x0 + w <= 0.0f || x0 >= SL_VIEW_W) continue;
        if (y0 + h <= 0.0f || y0 >= SL_VIEW_H) continue;

        anim = sl_escolhe_anim(p, prop_abs, step, beat_s, t, &fase);
        it->prop = prop_abs;
        it->cor = -1;
        it->anim = anim;
        it->frame = sl_prop_frame(p, anim, fase);
        /* A animacao do script (o `strike` do `lightning`) tem de existir no
           prop: um indice fora da faixa poria o quadro de outro prop na tela. */
        if (g_script && g_script->frame) {
            int a2 = anim, f2 = it->frame;
            if (g_script->frame(prop_abs, &a2, &f2) &&
                a2 >= 0 && a2 < p->anim_count && f2 >= 0) {
                it->anim = a2;
                it->frame = f2;
            }
        }
        it->x0 = x0; it->y0 = y0; it->w = w; it->h = h;
        it->tex_w = p->sw; it->tex_h = p->sh;
        it->z = p->z;
        it->alpha = alpha;
        /* A COR que o script poe no prop (o `lightWindow.color` do
           `phillyTrain`). Sem script, `tem_cor` fica 0 e o desenho sai como
           sempre. */
        it->pintado = (g_script && g_script->cor)
                          ? g_script->cor(prop_abs, it->rgb) : 0;
        if (!it->pintado) { it->rgb[0] = it->rgb[1] = it->rgb[2] = 0x80; }
        it->blend = p->blend;
        if (g_script && g_script->ang) g_script->ang(prop_abs, &ang);
        it->ang = ang;
        it->criado = -1;
        it->tile = 0.0f;
        it->tile_off = 0.0f;
        n++;
    }

    /* Os props de COR, com a MESMA conta de tela. A diferenca e' o tamanho: o
       Stage.hx cria o retangulo com `makeSolidColor(scale[0], scale[1])`, ou
       seja o `scale` JA' e' o tamanho em pixels do original -- nao se
       multiplica por scale outra vez (Stage.hx L235 pula o
       `scale.set` quando e' cor solida). */
    {int cf = bs_stage_cor_first[si], cc = bs_stage_cor_count[si];
     for (i = 0; i < cc && n < max; i++) {
        const bs_cor_t *c = &bs_cores[cf + i];
        sl_item_t *it = &out[n];
        float x0, y0, w, h, alpha;
        /* O `additionalLighten` do phillyBlazin e' um retangulo de cor que o
           script acende a 0,3 por 0,3 s (L163). */
        alpha = g_script && g_script->cor_alpha ? g_script->cor_alpha(cf + i) : -1.0f;
        if (alpha < 0.0f) alpha = c->alpha;
        if (alpha <= 0.0f) continue;
        x0 = SL_VIEW_W * 0.5f + (c->x - cam_x * c->scrollx + 640.0f * c->scrollx - 640.0f) * sc;
        y0 = SL_VIEW_H * 0.5f + (c->y - cam_y * c->scrolly + 360.0f * c->scrolly - 360.0f) * sc;
        w = c->w * sc;
        h = c->h * sc;
        if (x0 + w <= 0.0f || x0 >= SL_VIEW_W) continue;
        if (y0 + h <= 0.0f || y0 >= SL_VIEW_H) continue;
        it->prop = -1;
        it->cor = cf + i;
        it->ang = 0.0f;
        it->anim = 0; it->frame = 0;
        it->x0 = x0; it->y0 = y0; it->w = w; it->h = h;
        it->tex_w = 0; it->tex_h = 0;
        it->z = c->z;
        it->alpha = alpha;
        it->pintado = 0;
        it->rgb[0] = it->rgb[1] = it->rgb[2] = 0x80;
        it->blend = c->blend;
        it->criado = -1;
        it->tile = 0.0f;
        it->tile_off = 0.0f;
        n++;
     }
    }

    /* OS SPRITES DO SCRIPT DO PALCO (`bs_criados[]`).
       Mesma conta de tela dos props, com a diferenca que o `w`/`h` vem do dado
       (o `width`/`height` do `new FlxTiledSprite(...)`, nao de um quadro) e que
       o `scroll` anda com o tempo. `tile` e' o tamanho NATIVO da imagem: e' o
       periodo da repeticao no mundo, entao a repeticao na tela tem
       `src_w * 0,5 * zoom * bop` px. Um `alpha` 0 nao entra, como nos props. */
    {int cf = bs_stage_criado_first[si], cc = bs_stage_criado_count[si], k;
     for (k = 0; k < cc && n < max; k++) {
        const bs_criado_t *cr = &bs_criados[cf + k];
        sl_item_t *it = &out[n];
        float x0, y0, w, h;
        if (cr->alpha <= 0.0f) continue;
        x0 = SL_VIEW_W * 0.5f + (cr->x - cam_x * cr->scrollx + 640.0f * cr->scrollx - 640.0f) * sc;
        y0 = SL_VIEW_H * 0.5f + (cr->y - cam_y * cr->scrolly + 360.0f * cr->scrolly - 360.0f) * sc;
        w = cr->w * sc;
        h = cr->h * sc;
        /* A tela e' 640x360 e estes sprites nunca sao menores que ela -- mas
           se um dia forem, o corte tem de ser o mesmo dos props. */
        if (x0 + w <= 0.0f || x0 >= SL_VIEW_W) continue;
        if (y0 + h <= 0.0f || y0 >= SL_VIEW_H) continue;
        it->prop = -1;
        it->cor = -1;
        it->ang = 0.0f;
        it->criado = cf + k;
        it->anim = 0;
        it->frame = 0;
        it->x0 = x0; it->y0 = y0; it->w = w; it->h = h;
        it->tex_w = cr->sw; it->tex_h = cr->sh;
        it->z = cr->z;
        it->alpha = cr->alpha;
        it->pintado = 0;
        it->rgb[0] = it->rgb[1] = it->rgb[2] = 0x80;
        it->blend = cr->blend;
        it->tile = cr->tile ? cr->src_w : 0.0f;
        it->tile_off = cr->vel_x * t;
        n++;
     }
    }

    /* Ordem de desenho = zIndex crescente; para zIndex IGUAL, a ordem do JSON
       (`ord`), que e' a ordem em que o jogo adicionou os props ao grupo. A
       insercao e' estavel, entao comparar (z, ord) reproduz o sort do flixel
       sem depender da ordem em que props e cores entraram nesta lista. */
    for (i = 1; i < n; i++) {
        sl_item_t chave = out[i];
        int zc = chave.z, oc = sl_item_ord(&chave);
        for (j = i - 1; j >= 0; j--) {
            int zj = out[j].z, oj = sl_item_ord(&out[j]);
            if (zj < zc || (zj == zc && oj <= oc)) break;
            out[j + 1] = out[j];
        }
        out[j + 1] = chave;
    }
    return n;
}
