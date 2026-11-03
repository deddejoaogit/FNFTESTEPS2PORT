/* pack_stream - le' os quadros de um pacote CFZ4/CHP4 DIRETO DO PENDRIVE
 *
 * POR QUE ISTO EXISTE
 * Ate' aqui cada pacote era lido inteiro para a RAM principal e ficava la'. Isso
 * funciona para o tutorial (dois pacotes de 300 KB) e nao funciona para os 21
 * palcos: o pior deles tem 59,4 MB de pacote e o Emotion Engine tem 32 MB. Com
 * o teto que o `main.c` impoe (PALCO_RAM_MAX 4 MB para todos os props juntos),
 * oito dos 21 palcos nao abrem com a arte real -- e' o item "RAM" do
 * docs/PLANO_COMPLETO.md, Fase 6.
 *
 * O formato ja' permite streaming: o CFZ4 guarda uma TABELA de (offset,
 * tamanho) por quadro e cada quadro e' um bloco LZ4 independente. Entao o que
 * precisa ficar na RAM e' a tabela (8 bytes por quadro) e UM bloco comprimido
 * de cada vez -- nao o pacote.
 *
 * O QUE ESTE MODULO NAO FAZ
 *  - nao sabe de GS, VRAM nem palco: le' arquivo e descomprime.
 *  - nao inventa: cabecalho estragado, tabela fora do arquivo, bloco fora do
 *    arquivo, LZ4 que nao fecha no tamanho pedido -- todos devolvem erro
 *    negativo NOMEADO, e nada chega ao chamador meio decodificado.
 *
 * CUSTO MEDIDO QUE MOTIVA A TROCA (USB 1.1 do PS2, ~1,5 MB/s, blocos de 8 KB):
 * um bloco comprimido de prop tem ordem de 5 a 40 KB, ou seja 3 a 27 ms de
 * leitura. E' por quadro, no maximo uma vez por quadro de video, e so' quando
 * aquele prop troca de quadro (danceEvery/bop). A RAM, em troca, cai de
 * "o pacote inteiro" para "a tabela".
 */
#ifndef FNF_PACK_STREAM_H
#define FNF_PACK_STREAM_H

#ifdef __cplusplus
extern "C" {
#endif

#define PST_OK 0
#define PST_ERR_ABRIR    (-1)   /* open() falhou                                  */
#define PST_ERR_CABECALHO (-2)  /* curto demais, magic errado ou versao != 1      */
#define PST_ERR_TABELA   (-3)   /* a tabela de quadros nao cabe no arquivo        */
#define PST_ERR_QUADRO   (-4)   /* count/frame_bytes invalidos                    */
#define PST_ERR_MEMORIA  (-5)   /* sem RAM para a tabela/bloco                    */
#define PST_ERR_LEITURA  (-6)   /* read() curto -- pendrive puxado no meio?       */
#define PST_ERR_BLOCO    (-7)   /* (offset, tamanho) do quadro fora do arquivo    */
#define PST_ERR_LZ4      (-8)   /* o bloco nao descomprimiu no tamanho pedido     */

typedef struct {
    int           fd;          /* -1 quando nao ha' pacote aberto     */
    unsigned int  count;       /* quadros                             */
    unsigned int  frame_bytes;  /* bytes de UM quadro descomprimido   */
    unsigned int  bytes;       /* tamanho do arquivo em disco         */
    unsigned int *offset;      /* count: inicio do bloco              */
    unsigned int *tamanho;     /* count: tamanho do bloco comprimido  */
    unsigned char *bloco;      /* buffer do bloco comprimido          */
    unsigned int  bloco_bytes;  /* quanto foi reservado em `bloco`    */
    unsigned int  ram;         /* bytes que este pacote custa na RAM  */
    const unsigned char *mem;  /* != 0 quando o pacote veio da RAM    */
} pst_pack_t;

/* Abre `path`, le' o cabecalho e a TABELA, e para. Devolve 0 ou PST_ERR_*. */
int pst_abrir(const char *path, pst_pack_t *p);

/* O MESMO pacote, so' que os bytes JA' estao na RAM (nenhum arquivo aberto).
 * Existe por causa do `endStuff` do sserafim: as duas imagens do fim da musica
 * sao lidas no instante do evento, e no console essa leitura (283 504 B no
 * pendrive USB 1.1) cai exatamente nos ultimos 9 s da musica. Aqui o jogo copia
 * os `.cfz4` COMPRIMIDOS (39 951 + 10 102 B) para a RAM no comeco da cutscene e
 * descomprime na hora do evento -- sem tocar no pendrive quando o audio esta'
 * terminando. Nada e' decodificado aqui: `pst_quadro` faz o mesmo trabalho.
 * O buffer tem de continuar vivo enquanto o pacote for usado (o chamador e'
 * dono dele; `pst_fechar` NAO o libera). */
int pst_abrir_mem(const unsigned char *dados, unsigned int bytes, pst_pack_t *p);

/* Le' o bloco do quadro `frame`, descomprime em dst[0..frame_bytes).
   Devolve 0 ou PST_ERR_*. */
int pst_quadro(pst_pack_t *p, unsigned int frame, unsigned char *dst);

/* Fecha o arquivo e devolve a RAM. Seguro em pacote ja' fechado. */
void pst_fechar(pst_pack_t *p);

/* Nome do erro, para o log nao dizer "-7". */
const char *pst_erro(int codigo);

#ifdef __cplusplus
}
#endif
#endif /* FNF_PACK_STREAM_H */
