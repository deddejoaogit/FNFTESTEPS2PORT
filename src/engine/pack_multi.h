/* pack_multi - um pacote CFZ4 dividido em PARTES, lido quadro a quadro
 *
 * POR QUE ISTO EXISTE
 * `pack_stream.c` abre UM arquivo CFZ4. Isso cobre os props do palco e as tres
 * folhas da tela de Resultado, mas nao cobre pacotes grandes: o empacotador
 * (`tools/pacote_personagem.write_packs`) corta o pacote em arquivos de no
 * maximo 8 MB, porque o leitor antigo recusava mais do que isso, e o resultado
 * e' `x.cfz4` + `x.cfz4.part1` + ... O DJ da tela de Freeplay cai nesse caso:
 * 678 quadros de 370x375 a 16 bpp dao' 12,6 MB em DOIS arquivos.
 *
 * O QUE ELE NAO FAZ
 *  - nao carrega o pacote inteiro na RAM. Cada parte mantem a TABELA (8 bytes
 *    por quadro) e o buffer de UM bloco comprimido -- os mesmos numeros do
 *    `pack_stream`. O que fica na RAM e' a tabela das duas partes (~8 KB) e um
 *    bloco por parte.
 *  - nao inventa ordem: as partes sao abertas em sequencia e cada uma diz
 *    quantos quadros tem. Um buraco na numeracao (`x.cfz4.part1` sem
 *    `x.cfz4.part2` quando o plano pede tres) e' ERRO nomeado, nao silencio.
 *
 * C puro: sem GS, sem VRAM. Roda no PC.
 */
#ifndef FNF_PACK_MULTI_H
#define FNF_PACK_MULTI_H

#include "pack_stream.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PM_MAX_PARTES 8

#define PM_ERR_VAZIO   (-20)   /* nenhuma parte abriu                          */
#define PM_ERR_PARTE   (-21)   /* uma parte pedida pelo plano nao abriu        */
#define PM_ERR_QUADRO  (-22)   /* quadro fora do total das partes              */

typedef struct {
    pst_pack_t p;
    int aberto;
    unsigned int primeiro;   /* indice do primeiro quadro DESTA parte */
    unsigned int quantos;
} pm_parte_t;

typedef struct {
    pm_parte_t parte[PM_MAX_PARTES];
    int n;
    unsigned int total;      /* quadros somados das partes */
    unsigned int frame_bytes;/* tem de ser o MESMO em todas */
    unsigned int maior;      /* maior bloco comprimido (para o buffer do chamador) */
} pm_pack_t;

/* Abre `caminho` e as partes que o plano declarar. `partes` e' a lista de
 * nomes relativos ao diretorio de `caminho` (ex.: {"dj_bf.cfz4",
 * "dj_bf.cfz4.part1"}), com `n_partes` entradas. Devolve 0 ou PM_ERR_*. */
int pm_abrir(const char *dir, const char *const *partes, int n_partes,
             pm_pack_t *m);

void pm_fechar(pm_pack_t *m);

/* Quadro `frame` (indice GLOBAL, como no plano) descomprimido em `dst`
 * (m->frame_bytes bytes). Devolve 0, um PST_ERR_* ou um PM_ERR_*. */
int pm_quadro(pm_pack_t *m, unsigned int frame, unsigned char *dst);

/* RAM que o pacote custa (tabelas + um bloco por parte). */
unsigned int pm_ram(const pm_pack_t *m);

#ifdef __cplusplus
}
#endif

#endif /* FNF_PACK_MULTI_H */
