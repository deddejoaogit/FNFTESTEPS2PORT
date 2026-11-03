/* pack_stream - ver o cabecalho do .h: le' os quadros direto do arquivo. */
#include "pack_stream.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "lz4_bloco.h"

const char *pst_erro(int codigo)
{
    switch (codigo) {
    case PST_OK:            return "ok";
    case PST_ERR_ABRIR:     return "nao abriu o arquivo";
    case PST_ERR_CABECALHO: return "cabecalho invalido (magic/versao/tamanho)";
    case PST_ERR_TABELA:    return "tabela de quadros fora do arquivo";
    case PST_ERR_QUADRO:    return "numero de quadros ou bytes por quadro invalido";
    case PST_ERR_MEMORIA:   return "sem RAM para a tabela ou para o bloco";
    case PST_ERR_LEITURA:   return "leitura curta do arquivo";
    case PST_ERR_BLOCO:     return "bloco do quadro fora do arquivo";
    case PST_ERR_LZ4:       return "bloco LZ4 nao decodificou no tamanho pedido";
    default:                return "erro desconhecido do pacote";
    }
}

void pst_fechar(pst_pack_t *p)
{
    if (!p) return;
    /* Em modo RAM nao ha' fd e o buffer e' do chamador: nada para fechar nem
       para libertar aqui (so' a tabela e o bloco, que sao deste modulo). */
    if (p->fd >= 0) close(p->fd);
    free(p->offset);
    free(p->tamanho);
    free(p->bloco);
    p->fd = -1;
    p->mem = 0;
    p->offset = 0;
    p->tamanho = 0;
    p->bloco = 0;
    p->bloco_bytes = 0;
    p->ram = 0;
    p->count = 0;
    p->frame_bytes = 0;
    p->bytes = 0;
}

/* Lê exatamente `n` bytes na posicao `at`, do arquivo ou da RAM. Devolve 0 ou
   PST_ERR_LEITURA. */
static int ler_em(pst_pack_t *p, unsigned int at, unsigned char *dst, unsigned int n)
{
    int done = 0;
    if (p->mem) {
        if (at > p->bytes || n > p->bytes - at) return PST_ERR_LEITURA;
        memcpy(dst, p->mem + at, n);
        return PST_OK;
    }
    if (lseek(p->fd, (off_t)at, SEEK_SET) < 0) return PST_ERR_LEITURA;
    while ((unsigned int)done < n) {
        int r = read(p->fd, dst + done, (int)(n - (unsigned int)done));
        if (r <= 0) return PST_ERR_LEITURA;
        done += r;
    }
    return PST_OK;
}

/* O corpo comum: cabecalho, tabela e o buffer do bloco. `mem` != 0 dispensa
   arquivo (`path` entao e' NULL) e o buffer e' do CHAMADOR. */
static int pst_abrir_core(const char *path, const unsigned char *mem,
                          unsigned int mem_bytes, pst_pack_t *p)
{
    unsigned char cab[16];
    unsigned char *tab;
    unsigned int i, maior = 0, bytes;

    if (!p) return PST_ERR_CABECALHO;
    memset(p, 0, sizeof(*p));
    p->fd = -1;
    if (mem) {
        if (!mem_bytes) return PST_ERR_CABECALHO;
        p->mem = mem;
        p->bytes = mem_bytes;
    } else {
        if (!path) return PST_ERR_ABRIR;
        p->fd = open(path, O_RDONLY);
        if (p->fd < 0) return PST_ERR_ABRIR;
    }

    /* O cabecalho tem de ser lido de verdade: o tamanho do arquivo vem do
       lseek, mas o que interessa (count, frame_bytes) so' aparece aqui. */
    if (ler_em(p, 0, cab, 16) != PST_OK) { pst_fechar(p); return PST_ERR_LEITURA; }
    /* CFZ4 (props do palco) e CHP4 (personagem) comecam igual nos 16 primeiros
       bytes; este leitor e' o dos props. Versao 1 e' a dos dois formatos. */
    if (memcmp(cab, "CFZ4", 4) != 0) { pst_fechar(p); return PST_ERR_CABECALHO; }
    if (lz4_u32le(cab + 4) != 1u) { pst_fechar(p); return PST_ERR_CABECALHO; }
    p->count = lz4_u32le(cab + 8);
    p->frame_bytes = lz4_u32le(cab + 12);
    if (p->count == 0 || p->count > 65536u) { pst_fechar(p); return PST_ERR_QUADRO; }
    /* Um quadro tem de caber numa textura de 1024x1024 a 32 bpp: acima disso e'
       pacote de outro formato, nao um quadro grande. */
    if (p->frame_bytes == 0 || p->frame_bytes > 1024u * 1024u * 4u) {
        pst_fechar(p);
        return PST_ERR_QUADRO;
    }

    if (!mem) {
        off_t fim = lseek(p->fd, 0, SEEK_END);
        if (fim <= 16) { pst_fechar(p); return PST_ERR_CABECALHO; }
        p->bytes = (unsigned int)fim;
    } else if (p->bytes <= 16u) {
        pst_fechar(p);
        return PST_ERR_CABECALHO;
    }
    if (16u + p->count * 8u > p->bytes) { pst_fechar(p); return PST_ERR_TABELA; }

    tab = (unsigned char *)malloc(p->count * 8u);
    if (!tab) { pst_fechar(p); return PST_ERR_MEMORIA; }
    if (ler_em(p, 16u, tab, p->count * 8u) != PST_OK) {
        free(tab);
        pst_fechar(p);
        return PST_ERR_LEITURA;
    }
    p->offset = (unsigned int *)malloc(p->count * sizeof(unsigned int));
    p->tamanho = (unsigned int *)malloc(p->count * sizeof(unsigned int));
    if (!p->offset || !p->tamanho) {
        free(tab);
        pst_fechar(p);
        return PST_ERR_MEMORIA;
    }
    for (i = 0; i < p->count; i++) {
        p->offset[i] = lz4_u32le(tab + i * 8u);
        p->tamanho[i] = lz4_u32le(tab + i * 8u + 4u);
        /* Toda entrada tem de estar DENTRO do arquivo. Conferir aqui, uma vez,
           evita descobrir na tela (arte rasgada) e evita ler lixo do pendrive. */
        if (p->tamanho[i] == 0 || p->offset[i] > p->bytes ||
            p->tamanho[i] > p->bytes - p->offset[i]) {
            free(tab);
            pst_fechar(p);
            return PST_ERR_BLOCO;
        }
        if (p->tamanho[i] > maior) maior = p->tamanho[i];
    }
    free(tab);

    /* Um bloco comprimido de cada vez; o maior decide o buffer. */
    bytes = maior;
    p->bloco = (unsigned char *)malloc(bytes ? bytes : 1u);
    if (!p->bloco) { pst_fechar(p); return PST_ERR_MEMORIA; }
    p->bloco_bytes = bytes;
    p->ram = (unsigned int)(p->count * 8u + bytes + 2u * p->count * sizeof(unsigned int));
    return PST_OK;
}

int pst_abrir(const char *path, pst_pack_t *p)
{
    return pst_abrir_core(path, 0, 0, p);
}

int pst_abrir_mem(const unsigned char *dados, unsigned int bytes, pst_pack_t *p)
{
    if (!dados) return PST_ERR_CABECALHO;
    return pst_abrir_core(0, dados, bytes, p);
}

int pst_quadro(pst_pack_t *p, unsigned int frame, unsigned char *dst)
{
    int rc;
    if (!p || (!p->mem && p->fd < 0) || !p->offset || !p->tamanho || !p->bloco)
        return PST_ERR_ABRIR;
    if (!dst) return PST_ERR_QUADRO;
    if (frame >= p->count) return PST_ERR_QUADRO;
    if (p->tamanho[frame] > p->bloco_bytes) return PST_ERR_BLOCO;
    rc = ler_em(p, p->offset[frame], p->bloco, p->tamanho[frame]);
    if (rc != PST_OK) return rc;
    if (lz4_bloco_decode(p->bloco, p->tamanho[frame], dst, p->frame_bytes) != 0)
        return PST_ERR_LZ4;
    return PST_OK;
}
