#include "char_layout.h"

#include <stdio.h>
#include <string.h>

int ch_find(const char *id)
{
    int i;
    if (!id) return -1;
    for (i = 0; i < CH_CHAR_COUNT; i++)
        if (strcmp(ch_chars[i].id, id) == 0) return i;
    return -1;
}

int ch_anim_i(int ci, const char *nome)
{
    int i, achou = -1;
    const ch_dados_t *c;
    if (ci < 0 || ci >= CH_CHAR_COUNT || !nome) return -1;
    c = &ch_chars[ci];
    /* A ULTIMA de mesmo nome: o original guarda as animacoes num Map por nome
       (FlxAnimationUtil.addByPrefix -> FlxAnimationController._animations.set),
       entao um nome repetido SOBRESCREVE o anterior. Existe um caso real na
       arte: "Boyfriend (Holding GF)" tem dois `singUP`. */
    for (i = 0; i < c->anim_count; i++)
        if (strcmp(ch_anims[c->anim_first + i].name, nome) == 0) achou = i;
    return achou;
}

int ch_anim(const char *id, const char *nome)
{
    return ch_anim_i(ch_find(id), nome);
}

int ch_anim_idle(int ci, int beat)
{
    int e, d;
    if (ch_anim_i(ci, "idle") >= 0) return ch_anim_i(ci, "idle");
    e = ch_anim_i(ci, "danceLeft");
    d = ch_anim_i(ci, "danceRight");
    if (e >= 0 && d >= 0) return (beat & 1) ? d : e;
    if (e >= 0) return e;
    if (d >= 0) return d;
    return -1;
}

int ch_anim_dir(const char *nome)
{
    static const char *d2v[4] = { "singLEFT", "singDOWN", "singUP", "singRIGHT" };
    int i;
    size_t n;
    if (!nome) return -1;
    n = strlen(nome);
    /* "singLEFTmiss" tambem casa com "singLEFT": o sufixo de erro nao muda a
       direcao, so' o quadro. */
    for (i = 0; i < 4; i++)
        if (n >= strlen(d2v[i]) && strncmp(nome, d2v[i], strlen(d2v[i])) == 0) return i;
    return -1;
}

int ch_anim_sing(int ci, int dir, int missing)
{
    static const char *d2v[4] = { "singLEFT", "singDOWN", "singUP", "singRIGHT" };
    char buf[24];
    int i;
    if (dir < 0 || dir > 3) return -1;
    if (missing) {
        i = snprintf(buf, sizeof(buf), "%smiss", d2v[dir]);
        if (i > 0 && ch_anim_i(ci, buf) >= 0) return ch_anim_i(ci, buf);
    }
    return ch_anim_i(ci, d2v[dir]);
}

unsigned short ch_dir_button(int dir)
{
    /* PAD_LEFT 0x0080, PAD_DOWN 0x0040, PAD_UP 0x0010, PAD_RIGHT 0x0020 --
       os mesmos valores que o tutorial usa em tutorial_lane_button(). Ficam
       aqui como numero porque este modulo e' C puro (o teste roda no PC sem
       os headers do console). */
    static const unsigned short b[4] = { 0x0080, 0x0040, 0x0010, 0x0020 };
    return (dir >= 0 && dir < 4) ? b[dir] : 0;
}

const bs_slot_t *ch_slot(const char *stage, const char *role)
{
    int si, i, first;
    if (!stage || !role) return 0;
    for (si = 0; si < BS_STAGE_PROPS_COUNT; si++)
        if (strcmp(bs_stage_props[si].id, stage) == 0) break;
    if (si >= BS_STAGE_PROPS_COUNT) return 0;
    first = bs_stage_slot_first[si];
    for (i = 0; i < 3; i++) {
        const bs_slot_t *s = &bs_slots[first + i];
        if (strcmp(s->role, role) == 0) return s;
    }
    return 0;
}

void ch_anchor(const ch_dados_t *c, const bs_slot_t *slot,
               float canvas_w, float canvas_h, float pad, int usa_offsets,
               ch_anchor_t *out)
{
    float ox = 0.0f, oy = 0.0f;
    if (!out) return;
    out->sprite_x = out->sprite_y = 0.0f;
    out->cam_x = out->cam_y = 0.0f;
    if (!c || !slot) return;
    if (usa_offsets) { ox = c->ox; oy = c->oy; }
    /* Pe' no chao do slot, centralizado na horizontal; o -4/escala e' o mesmo
       ajuste de meio pixel/borda que o tutorial aplica e a captura confirmou. */
    out->sprite_x = slot->x - canvas_w * 0.5f + ox - pad;
    out->sprite_y = slot->y - canvas_h + oy - pad;
    /* Foco: position + offsets do personagem + cameraOffsets DO PERSONAGEM +
       cameraOffsets do slot, na altura do peito (meio do canvas).
       O cameraOffsets do personagem entra SEMPRE: o BaseCharacter.hx L350-355
       (resetCameraFocusPoint) soma _data.cameraOffsets sobre originalPosition,
       e originalPosition ja' carrega o globalOffsets (L286-287) -- e' por isso
       que `usa_offsets`, que so' diz se o SCRIPT do personagem aplica o
       globalOffsets no sprite (gf.hx nao aplica), nao mexe na camera.
       Com o mainStage/BF: 989,5+17-18-100 = 888,50 e 885+14-246+36-100 = 589,00. */
    out->cam_x = slot->x + c->ox + c->cx + slot->cx;
    out->cam_y = slot->y + c->oy - canvas_h * 0.5f + c->cy + slot->cy;
}

int ch_cena_plano(const sr_song_t *song, ch_cena_t out[CH_CENA_PAPEIS])
{
    if (!song || !out) return -1;
    /* Ordem: gf, bf, dad -- os tres slots que todo palco tem
       (bs_slots[] = 21 palcos x bf/gf/dad, conferido em check_bs_dados). */
    out[CH_PAPEL_GF].papel = CH_PAPEL_GF;
    out[CH_PAPEL_GF].slot = "gf";
    out[CH_PAPEL_GF].ci = song->girlfriend ? ch_find(song->girlfriend) : -1;

    out[CH_PAPEL_BF].papel = CH_PAPEL_BF;
    out[CH_PAPEL_BF].slot = "bf";
    out[CH_PAPEL_BF].ci = song->player ? ch_find(song->player) : -1;

    out[CH_PAPEL_DAD].papel = CH_PAPEL_DAD;
    out[CH_PAPEL_DAD].slot = "dad";
    out[CH_PAPEL_DAD].ci = song->opponent ? ch_find(song->opponent) : -1;
    return 0;
}
