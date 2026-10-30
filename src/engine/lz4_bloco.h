/* lz4_bloco - o decodificador do formato dos pacotes, num lugar so'
 *
 * POR QUE ISTO EXISTE
 * O mesmo bloco LZ4 estava escrito TRES vezes dentro do main.c (quadros do
 * titulo, quadros do tutorial, quadros dos props do palco), em tres copias de
 * uma linha ilegivel cada. A quarta copia seria a dos personagens. Uma copia
 * errada num pacote so' se descobre na tela, como arte rasgada.
 *
 * Formato (LZ4 block, sem frame): token com dois nibbles (literais | match-4),
 * extensoes de tamanho em bytes 255, os literais, o offset do match (2 bytes
 * little endian) e o match copiado byte a byte -- pode se sobrepor ao que ja'
 * foi escrito, e e' assim que dado periodico comprime.
 *
 * C puro: sem GS, sem arquivo. Roda no PC. O codificador (o empacotador) e' o
 * tools/lz4py.py, e o teste `test/test_lz4.c` decodifica vetores que ele gerou
 * -- ou seja, as duas metades sao conferidas uma contra a outra.
 */
#ifndef FNF_LZ4_BLOCO_H
#define FNF_LZ4_BLOCO_H

#ifdef __cplusplus
extern "C" {
#endif

#define LZ4_BLOCO_OK 0
#define LZ4_ERR_TRUNCADO_LIT (-1)   /* extensao de literais alem do fim      */
#define LZ4_ERR_LIT_FORA (-2)       /* literais alem do fim da entrada/saida */
#define LZ4_ERR_TRUNCADO_OFF (-3)   /* falta o offset do match               */
#define LZ4_ERR_OFF (-4)            /* offset 0 ou antes do inicio da saida  */
#define LZ4_ERR_TRUNCADO_MATCH (-5) /* extensao do match alem do fim         */
#define LZ4_ERR_MATCH_FORA (-6)     /* match maior que o espaco que sobra    */
#define LZ4_ERR_TAMANHO (-7)        /* a saida nao fechou no tamanho pedido  */

/* Decodifica src[0..src_size) em dst[0..dst_size). Devolve 0 so' quando a
   saida fecha EXATAMENTE em dst_size. Qualquer outra coisa e' erro negativo:
   um quadro meio decodificado nao pode chegar ao GS. */
int lz4_bloco_decode(const unsigned char *src, unsigned int src_size,
                     unsigned char *dst, unsigned int dst_size);

/* u32 little endian, usado por todos os cabecalhos de pacote. */
unsigned int lz4_u32le(const unsigned char *p);

#ifdef __cplusplus
}
#endif
#endif /* FNF_LZ4_BLOCO_H */
