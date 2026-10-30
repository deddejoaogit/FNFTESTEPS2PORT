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

/* Qual animacao tocar. O original escolhe pelo NOME ("danceLeft"/"danceRight"
   alternando a cada batida, "idle" quando o prop nao tem dance). Se o prop nao
   tem nenhuma das duas, fica a primeira animacao do pacote -- que e' o que o
   empacotador gravou primeiro e o que o sprite mostra parado. */
static int sl_escolhe_anim(const bs_prop_t *p, int beat)
{
    int i, esq = -1, dir = -1;
    for (i = 0; i < p->anim_count; i++) {
        const char *n = bs_prop_anims[p->anim_first + i].name;
        if (strcmp(n, "danceLeft") == 0) esq = i;
        else if (strcmp(n, "danceRight") == 0) dir = i;
    }
    if (esq >= 0 && dir >= 0) return (beat & 1) ? dir : esq;
    return 0;
}

int sl_collect(const char *stage, float cam_x, float cam_y, float zoom,
               float bop, int beat, float t, sl_item_t *out, int max)
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
        float x0, y0, w, h;

        /* Formula do port (ver o cabecalho): o termo (640*f - 640) existe
           porque o scrollFactor menor que 1 desloca o prop em relacao ao
           canto da camera, nao ao centro. */
        x0 = SL_VIEW_W * 0.5f + (p->x - cam_x * p->scrollx + 640.0f * p->scrollx - 640.0f) * sc;
        y0 = SL_VIEW_H * 0.5f + (p->y - cam_y * p->scrolly + 360.0f * p->scrolly - 360.0f) * sc;
        /* O tamanho NA TELA vem de src_w/src_h (o tamanho do quadro no espaco
           do original), nao de sw/sh. sw/sh e' a resolucao do pacote, que o
           empacotador reduziu para caber na VRAM (escala 0,31 no mainStage):
           desenhar com sw/sh encolheria o palco inteiro em tres vezes. */
        w = (float)p->src_w * sc;
        h = (float)p->src_h * sc;

        /* Fora da tela nao entra no pacote. 'fastCar' do limoRide fica em
           x=-12600: sem este corte, um prop invisivel custaria uma textura
           por quadro e um quad. */
        if (x0 + w <= 0.0f || x0 >= SL_VIEW_W) continue;
        if (y0 + h <= 0.0f || y0 >= SL_VIEW_H) continue;

        anim = sl_escolhe_anim(p, beat);
        it->prop = st->first + i;
        it->anim = anim;
        it->frame = sl_prop_frame(p, anim, t);
        it->x0 = x0; it->y0 = y0; it->w = w; it->h = h;
        it->tex_w = p->sw; it->tex_h = p->sh;
        n++;
    }

    /* Ordem de desenho = zIndex crescente. Insercao estavel: props com o mesmo
       z mantem a ordem do JSON, que e' a ordem em que o jogo os adicionou. */
    for (i = 1; i < n; i++) {
        sl_item_t chave = out[i];
        int zc = bs_props[chave.prop].z;
        for (j = i - 1; j >= 0 && bs_props[out[j].prop].z > zc; j--) out[j + 1] = out[j];
        out[j + 1] = chave;
    }
    return n;
}
