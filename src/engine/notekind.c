#include "notekind.h"

#include <string.h>

const char *nk_nome(const char *kind)
{
    unsigned int i;
    if (!kind) return 0;
    for (i = 0; i < NK_KIND_COUNT; i++)
        if (strcmp(nk_kinds[i].id, kind) == 0) return nk_kinds[i].nome;
    return 0;
}

const nk_acao_t *nk_acao(const char *personagem, const char *kind)
{
    unsigned int c, k;
    if (!personagem || !kind) return 0;
    for (c = 0; c < NK_CHAR_COUNT; c++) {
        if (strcmp(nk_chars[c].personagem, personagem) != 0) continue;
        for (k = 0; k < nk_chars[c].n; k++)
            if (strcmp(nk_chars[c].acoes[k].kind, kind) == 0)
                return &nk_chars[c].acoes[k];
        return 0;
    }
    return 0;
}

unsigned int nk_kind_count(void) { return NK_KIND_COUNT; }

unsigned int nk_acao_count(void)
{
    unsigned int c, n = 0;
    for (c = 0; c < NK_CHAR_COUNT; c++) n += nk_chars[c].n;
    return n;
}
