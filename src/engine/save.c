/* save - ver o cabecalho do .h. Modelo puro: sem arquivo, sem GS. */
#include <stddef.h>
#include <string.h>

#include "save.h"
#include "gameplay.h"

#define SAVE_VERSAO 1u
#define SAVE_REG_BYTES 24u

void save_init(save_t *sv)
{
    if (!sv) return;
    memset(sv, 0, sizeof(*sv));
}

static int indices_ok(int musica, int dif)
{
    return musica >= 0 && musica < SAVE_MUSICAS && dif >= 0 && dif < SAVE_DIFS;
}

const save_score_t *save_obter(const save_t *sv, int musica, int dif)
{
    if (!sv || !indices_ok(musica, dif)) return 0;
    if (!sv->s[musica][dif].valido) return 0;
    return &sv->s[musica][dif];
}

int save_novo_recorde(const save_t *sv, int musica, int dif, unsigned int score)
{
    const save_score_t *r = save_obter(sv, musica, dif);
    if (!r) return 1;                 /* Save.hx L577-580: sem registro -> true */
    return score > r->score;          /* L581 */
}

/* O rank do registro, pela MESMA funcao que julgou a musica: o registro e'
 * montado num `fnf_play_t` e `fnf_play_rank` responde. Nada de formula nova. */
int save_rank(const save_score_t *r)
{
    fnf_play_t p;
    if (!r || !r->valido) return -1;
    if (r->total_notes == 0) return -1;   /* Scoring.hx L386: sem notas, null */
    fnf_play_reset(&p, FNF_SCORING_PBOT1, (int)r->total_notes);
    p.score = (int)r->score;
    p.sick = (int)r->sick;
    p.good = (int)r->good;
    p.bad = (int)r->bad;
    p.shit = (int)r->shit;
    p.missed = (int)r->missed;
    p.combo = (int)r->combo;
    p.max_combo = (int)r->max_combo;
    p.total_notes_hit = (int)r->total_hit;
    p.total_notes = (int)r->total_notes;
    return (int)fnf_play_rank(&p);
}

static float tally_completion(const save_score_t *r)
{
    float v;
    if (!r || r->total_notes == 0) return 0.0f;
    v = ((float)((int)r->sick + (int)r->good - (int)r->missed)) /
        (float)r->total_notes;
    if (v < 0.0f) v = 0.0f;
    if (v > 1.0f) v = 1.0f;
    return v;
}

void save_aplicar(save_t *sv, int musica, int dif, const save_score_t *novo)
{
    save_score_t *atual;
    int rank_novo, rank_atual;
    if (!sv || !novo || !indices_ok(musica, dif)) return;
    atual = &sv->s[musica][dif];
    if (!atual->valido) {
        /* Save.hx L547-551: sem registro anterior, grava direto. */
        *atual = *novo;
        atual->valido = 1;
        sv->sujo = 1;
        return;
    }
    rank_novo = save_rank(novo);
    rank_atual = save_rank(atual);
    /* Save.hx L538-543: o placar e' o MAIOR dos dois; os tallies sao os do
     * melhor rank e, no empate, os do maior `tallyCompletion`. */
    {
        unsigned int melhor = atual->score > novo->score ? atual->score
                                                         : novo->score;
        int usa_novo = (rank_atual < 0) || (rank_novo > rank_atual) ||
                       (rank_novo == rank_atual &&
                        tally_completion(novo) > tally_completion(atual));
        save_score_t tmp;
        if (usa_novo) {
            tmp = *novo;
            tmp.score = melhor;
        } else {
            tmp = *atual;
            tmp.score = melhor;
        }
        tmp.valido = 1;
        if (tmp.score != atual->score ||
            memcmp(&tmp.sick, &atual->sick, sizeof(tmp) - offsetof(save_score_t, sick)) != 0)
            sv->sujo = 1;
        *atual = tmp;
    }
}

unsigned int save_crc32(const unsigned char *src, unsigned int bytes)
{
    unsigned int crc = 0xFFFFFFFFu, i;
    int k;
    if (!src) return 0;
    for (i = 0; i < bytes; i++) {
        crc ^= (unsigned int)src[i];
        for (k = 0; k < 8; k++)
            crc = (crc >> 1) ^ (0xEDB88320u & (unsigned int)(-(int)(crc & 1u)));
    }
    return crc ^ 0xFFFFFFFFu;
}

static void put16(unsigned char *p, unsigned int v)
{
    p[0] = (unsigned char)(v & 0xFFu);
    p[1] = (unsigned char)((v >> 8) & 0xFFu);
}

static void put32(unsigned char *p, unsigned int v)
{
    p[0] = (unsigned char)(v & 0xFFu);
    p[1] = (unsigned char)((v >> 8) & 0xFFu);
    p[2] = (unsigned char)((v >> 16) & 0xFFu);
    p[3] = (unsigned char)((v >> 24) & 0xFFu);
}

static unsigned int get16(const unsigned char *p)
{
    return (unsigned int)p[0] | ((unsigned int)p[1] << 8);
}

static unsigned int get32(const unsigned char *p)
{
    return (unsigned int)p[0] | ((unsigned int)p[1] << 8) |
           ((unsigned int)p[2] << 16) | ((unsigned int)p[3] << 24);
}

unsigned int save_serializar(const save_t *sv, unsigned char *dst)
{
    unsigned int m, d, off;
    unsigned char *corpo;
    if (!sv || !dst) return 0;
    corpo = dst + 16;
    off = 0;
    for (m = 0; m < SAVE_MUSICAS; m++)
        for (d = 0; d < SAVE_DIFS; d++) {
            const save_score_t *r = &sv->s[m][d];
            unsigned char *p = corpo + off;
            put32(p + 0, r->valido ? r->score : 0u);
            put16(p + 4, r->sick);
            put16(p + 6, r->good);
            put16(p + 8, r->bad);
            put16(p + 10, r->shit);
            put16(p + 12, r->missed);
            put16(p + 14, r->combo);
            put16(p + 16, r->max_combo);
            put16(p + 18, r->total_hit);
            put16(p + 20, r->total_notes);
            p[22] = r->valido ? 1u : 0u;
            p[23] = 0u;
            off += SAVE_REG_BYTES;
        }
    memcpy(dst, "FNFS", 4);
    put16(dst + 4, SAVE_VERSAO);
    put16(dst + 6, (unsigned int)(SAVE_MUSICAS * SAVE_DIFS));
    put32(dst + 8, 0u);            /* reservado */
    put32(dst + 12, save_crc32(corpo, off));
    return 16u + off;
}

int save_desserializar(save_t *sv, const unsigned char *src, unsigned int bytes)
{
    save_t tmp;
    unsigned int m, d, off;
    if (!sv || !src) return SAVE_ERR_CURTO;
    if (bytes < 16u) return SAVE_ERR_CURTO;
    if (memcmp(src, "FNFS", 4) != 0) return SAVE_ERR_MAGIC;
    if (get16(src + 4) != SAVE_VERSAO) return SAVE_ERR_VERSAO;
    if (get16(src + 6) != (unsigned int)(SAVE_MUSICAS * SAVE_DIFS))
        return SAVE_ERR_CONTAGEM;
    if (bytes < (unsigned int)SAVE_ARQUIVO_BYTES) return SAVE_ERR_CURTO;
    if (save_crc32(src + 16, (unsigned int)SAVE_MUSICAS * SAVE_DIFS * SAVE_REG_BYTES)
        != get32(src + 12))
        return SAVE_ERR_CRC;

    save_init(&tmp);
    off = 0;
    for (m = 0; m < SAVE_MUSICAS; m++)
        for (d = 0; d < SAVE_DIFS; d++) {
            const unsigned char *p = src + 16 + off;
            save_score_t *r = &tmp.s[m][d];
            r->score = get32(p + 0);
            r->sick = (unsigned short)get16(p + 4);
            r->good = (unsigned short)get16(p + 6);
            r->bad = (unsigned short)get16(p + 8);
            r->shit = (unsigned short)get16(p + 10);
            r->missed = (unsigned short)get16(p + 12);
            r->combo = (unsigned short)get16(p + 14);
            r->max_combo = (unsigned short)get16(p + 16);
            r->total_hit = (unsigned short)get16(p + 18);
            r->total_notes = (unsigned short)get16(p + 20);
            r->valido = p[22] ? 1u : 0u;
            if (!r->valido) memset(r, 0, sizeof(*r));
            off += SAVE_REG_BYTES;
        }
    tmp.carregado = 1;
    tmp.sujo = 0;
    *sv = tmp;
    return 0;
}
