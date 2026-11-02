/* pack_multi - ver o cabecalho. Le' um CFZ4 dividido em partes. */
#include "pack_multi.h"

#include <stdio.h>
#include <string.h>

int pm_abrir(const char *dir, const char *const *partes, int n_partes,
             pm_pack_t *m)
{
    int i;
    unsigned int primeiro = 0;

    if (!m) return PM_ERR_VAZIO;
    memset(m, 0, sizeof(*m));
    if (!partes || n_partes <= 0 || n_partes > PM_MAX_PARTES) return PM_ERR_VAZIO;

    for (i = 0; i < n_partes; i++) {
        char caminho[192];
        int rc;
        if (!partes[i] || !partes[i][0]) return PM_ERR_PARTE;
        if (dir && dir[0])
            snprintf(caminho, sizeof(caminho), "%s/%s", dir, partes[i]);
        else
            snprintf(caminho, sizeof(caminho), "%s", partes[i]);
        rc = pst_abrir(caminho, &m->parte[i].p);
        if (rc != PST_OK) {
            /* Nenhuma parte aberta ja' e' aberta: fecha o que veio antes para
               nao vazar descritor nem tabela. */
            pm_fechar(m);
            m->n = 0;
            return PM_ERR_PARTE;
        }
        m->parte[i].aberto = 1;
        m->parte[i].primeiro = primeiro;
        m->parte[i].quantos = m->parte[i].p.count;
        primeiro += m->parte[i].p.count;
        if (i == 0) {
            m->frame_bytes = m->parte[i].p.frame_bytes;
            m->maior = m->parte[i].p.bloco_bytes;
        } else {
            /* Um pacote so' pode ter um tamanho de quadro: partes com tamanhos
               diferentes seriam arquivos de pacotes distintos misturados. */
            if (m->parte[i].p.frame_bytes != m->frame_bytes) {
                pm_fechar(m);
                m->n = 0;
                return PM_ERR_PARTE;
            }
            if (m->parte[i].p.bloco_bytes > m->maior)
                m->maior = m->parte[i].p.bloco_bytes;
        }
        m->n = i + 1;
    }
    m->total = primeiro;
    return 0;
}

void pm_fechar(pm_pack_t *m)
{
    int i;
    if (!m) return;
    for (i = 0; i < PM_MAX_PARTES; i++) {
        if (m->parte[i].aberto) {
            pst_fechar(&m->parte[i].p);
            m->parte[i].aberto = 0;
        }
        m->parte[i].primeiro = 0;
        m->parte[i].quantos = 0;
    }
    m->n = 0;
    m->total = 0;
    m->maior = 0;
}

int pm_quadro(pm_pack_t *m, unsigned int frame, unsigned char *dst)
{
    int i;
    if (!m || !dst || m->n <= 0) return PM_ERR_VAZIO;
    if (frame >= m->total) return PM_ERR_QUADRO;
    for (i = 0; i < m->n; i++) {
        const pm_parte_t *pa = &m->parte[i];
        if (!pa->aberto) continue;
        if (frame >= pa->primeiro && frame < pa->primeiro + pa->quantos)
            return pst_quadro(&m->parte[i].p, frame - pa->primeiro, dst);
    }
    return PM_ERR_QUADRO;
}

unsigned int pm_ram(const pm_pack_t *m)
{
    int i;
    unsigned int r = 0;
    if (!m) return 0;
    for (i = 0; i < m->n; i++)
        if (m->parte[i].aberto) r += m->parte[i].p.ram;
    return r;
}
