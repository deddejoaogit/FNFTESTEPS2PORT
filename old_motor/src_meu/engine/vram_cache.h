/* vram_cache - alocador de VRAM com despejo por menos-usado-recentemente
 *
 * POR QUE ISTO EXISTE
 * O port atual tem CINCO texturas fixas: tex, tex2, gftex, stagetex e
 * fonttex. Cada personagem ou prop animado precisa de uma, e isso
 * limita a nove palcos do jogo, entre eles o tankmanBattlefield, que
 * sozinho pede treze. Reduzir resolucao nao cria slot: o que falta e
 * quantidade de texturas vivas, nao tamanho.
 *
 * Este modulo substitui os slots fixos. Quem desenha pede uma textura
 * por CHAVE e recebe um endereco de VRAM. Se ja estiver residente, o
 * custo e zero e nada e reenviado. Se nao couber, o cache despeja a
 * menos usada recentemente ate abrir espaco contiguo.
 *
 * O codigo e C puro, sem nada de PS2, de proposito: assim roda no PC
 * dentro de um teste e e exercitado com as demandas reais das 21 cenas
 * antes de pisar no console.
 *
 * ORCAMENTO, medido no SCPH-90006 deste projeto:
 *   VRAM total ............ 4.194.304 B
 *   dois framebuffers ..... 2.293.760 B
 *   sobra para texturas ... 1.900.544 B
 *   upload ................ 37 MB/s, logo 524.783 B por quadro
 */
#ifndef VRAM_CACHE_H
#define VRAM_CACHE_H

#ifdef __cplusplus
extern "C" {
#endif

#define VC_MAX_BLOCOS 128
#define VC_ALINHAMENTO 256u   /* bloco do GS; TBP0 conta de 256 em 256 */

typedef struct {
    unsigned int chave;       /* identificador estavel do conteudo */
    unsigned int endereco;    /* offset dentro da regiao de texturas */
    unsigned int bytes;       /* tamanho ja alinhado */
    unsigned int selo;        /* ultimo uso, para o despejo */
    unsigned char usado;      /* 0 = buraco livre */
    unsigned char fixo;       /* 1 = nunca despejar (fonte, HUD) */
} vc_bloco_t;

typedef struct {
    unsigned int base, tamanho, relogio;
    int n;
    vc_bloco_t b[VC_MAX_BLOCOS];
    /* contadores, para o teste e para a telemetria */
    unsigned int acertos, faltas, despejos, falhas, bytes_enviados;
} vc_t;

void vc_init(vc_t *c, unsigned int base, unsigned int tamanho);

/* Pede a textura identificada por 'chave'.
 * Devolve 1 se ja estava residente (nada a enviar) e 0 se foi alocada
 * agora (o chamador precisa enviar os pixels). Devolve -1 se nem
 * despejando tudo cabe, o que indica erro de planejamento de cena.
 * O endereco sai em *saida. */
int vc_pedir(vc_t *c, unsigned int chave, unsigned int bytes, unsigned int *saida);

/* Marca como nunca despejavel. Use so para o que e realmente permanente. */
void vc_fixar(vc_t *c, unsigned int chave);

/* Esquece tudo que nao for fixo. Usar ao trocar de cena. */
void vc_limpar(vc_t *c);

unsigned int vc_livre(const vc_t *c);
unsigned int vc_maior_buraco(const vc_t *c);

/* --- extensoes do port completo (v0.3) ---------------------------------
 * vc_pedir() sozinho nao resolve dois problemas que o jogo inteiro
 * expoe: (1) duas chaves com o MESMO endereco quando o mesmo arquivo
 * alimenta varios sprites (limo/henchmen: 5 sprites, 1 folha) e (2) um
 * palco cujo conjunto de texturas e maior que a regiao inteira, caso em
 * que e preciso despejar DURANTE o desenho e nao so na troca de cena.
 *
 * vc_apelido()  : segunda chave apontando para o bloco de uma primeira.
 * vc_prender()  : aumenta a contagem de fixacao; vc_soltar() decrementa.
 * vc_despejar_ate(): despeja LRU ate existir buraco contiguo de 'bytes'.
 * vc_relatar()  : escreve um relatorio de uma linha (telemetria).
 */
int  vc_apelido(vc_t *c, unsigned int chave_original, unsigned int chave_nova);
void vc_prender(vc_t *c, unsigned int chave);
void vc_soltar(vc_t *c, unsigned int chave);
int  vc_despejar_ate(vc_t *c, unsigned int bytes);
void vc_relatar(const vc_t *c, char *destino, int tam);

#ifdef __cplusplus
}
#endif
#endif
