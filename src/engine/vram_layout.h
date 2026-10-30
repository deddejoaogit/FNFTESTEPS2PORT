/* vram_layout - o mapa de VRAM do port, em codigo e com teste.
 *
 * POR QUE ISTO EXISTE
 * O mapa de VRAM do port estava so' na cabeca de quem escreveu main.c: oito
 * graph_vram_allocate() em sequencia, nenhuma conta escrita em lugar nenhum, e a
 * documentacao do projeto falando de "99,4 % da VRAM" sem que nada verificasse
 * isso. Um alocacao nova que estourasse a VRAM nao quebrava nenhum teste: o
 * graph_vram_allocate() devolve -1 e o port desenhava com um endereco invalido.
 *
 * Aqui o mapa e' uma TABELA. As contas seguem graph_vram_size() do ps2sdk,
 * linha por linha (ee/graph/src/graph_vram.c):
 *   - largura arredondada para cima em 64 (128 nos formatos de 4/8 bits);
 *   - tamanho em palavras de 32 bits: w*h (32 bpp), w*(h/2) (16 bpp);
 *   - alinhamento em 2.048 palavras para buffers de quadro, 64 para texturas.
 * O total tem de caber em 1.048.576 palavras (4.194.304 bytes), e o teste
 * test/test_vram_layout.c falha se nao couber.
 *
 * Endereco devolvido aqui e' o MESMO numero que graph_vram_allocate() devolve
 * (offset em palavras), entao main.c pode conferir no console, no boot, que o
 * mapa calculado e o alocador real concordam -- e registrar isso no log.
 */
#ifndef FNF_VRAM_LAYOUT_H
#define FNF_VRAM_LAYOUT_H

#ifdef __cplusplus
extern "C" {
#endif

/* Uma palavra de VRAM e' um pixel de 32 bits: 1.048.576 palavras = 4 MiB. */
#define VL_TOTAL_WORDS 1048576u
#define VL_ALINHAMENTO_PAGINA 2048u   /* GRAPH_ALIGN_PAGE  */
#define VL_ALINHAMENTO_BLOCO    64u   /* GRAPH_ALIGN_BLOCK */

/* Formatos que o port usa. Os numeros sao os do GS (gs_psm.h). */
#define VL_PSM_16 0x02u
#define VL_PSM_32 0x00u

typedef struct {
    const char *nome;
    unsigned int w, h, psm, alinhamento;
} vl_slot_t;

/* Indices fixos: quem desenha pede por nome do slot e nao por numero solto.
 *
 * ORDEM = ORDEM DE ALOCACAO em main.c. As texturas que o GAMEPLAY usa ficam
 * fixas e no comeco; o resto da VRAM e' o POOL (engine/stage_pool.c), que
 * acomoda gftex, stagetex e as texturas dos props de palco com despejo por
 * menos-usado-recentemente. Antes gftex e stagetex eram fixas e ficavam fora do
 * pool: sobravam 1.269.760 B e 15 dos 21 palcos nao cabiam. Dentro do pool, o
 * orcamento e' 2.056.192 B e todos os 21 cabem (test/test_stage_pool.c). */
enum {
    VL_FB0 = 0,
    VL_FB1,
    VL_TEX,
    VL_TEX2,
    VL_FONTTEX,    /* HUD: texto do jogo inteiro */
    VL_GFTEX16,    /* a GF no gameplay */
    VL_SLOTS
};

const vl_slot_t *vl_slots(void);
unsigned int vl_slot_count(void);

/* Tamanho em PALAVRAS de 32 bits, pela conta de graph_vram_size(). */
unsigned int vl_size_words(unsigned int w, unsigned int h, unsigned int psm,
                           unsigned int alinhamento);

/* Endereco calculado do slot, do mesmo jeito que o alocador sequencial do
 * ps2sdk chega nele (cada slot alinhado depois do anterior). */
unsigned int vl_address(unsigned int slot);

/* Bytes ocupados pelo slot (palavras x 4). */
unsigned int vl_bytes(unsigned int slot);

/* Somatorio em palavras, e quanto sobra. */
unsigned int vl_used_words(void);

/* O pool comeca aqui (palavras) e vai ate' o fim da VRAM (bytes). O vram_layout
   NAO aloca o pool: so' diz onde ele comeca e quanto tem, para o main.c pedir
   ao graph_vram_allocate um bloco que caiba exatamente nesse buraco. */
unsigned int vl_pool_base(void);
unsigned int vl_pool_size_bytes(void);
unsigned int vl_free_words(void);

/* 1 quando o mapa cabe na VRAM. */
int vl_fits(void);

#ifdef __cplusplus
}
#endif
#endif
