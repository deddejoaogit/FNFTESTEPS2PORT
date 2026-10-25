/* asset_bank.h - modelo de carregamento do port completo
 *
 * O QUE ESTAVA ERRADO (medido no codigo do port 0.2.112):
 *   - as 6 reservas de VRAM acontecem todas na inicializacao;
 *   - existem ZERO chamadas de liberacao de VRAM no projeto inteiro;
 *   - 8 buffers somam 4.169.728 dos 4.194.304 bytes do GS (99,4%);
 *   - title_packs.S e story_packs.S gravam 2.929.792 bytes de pacote
 *     DENTRO do executavel, e esses pacotes servem a 2 das 4 telas.
 *   - so o caminho do tutorial carrega do pendrive e libera ao sair.
 *
 * O QUE ESTE MODULO FAZ
 *   - O pacote mora no pendrive. Entra quando a tela entra, sai quando a
 *     tela sai. Nada de .incbin.
 *   - A VRAM e pedida ao vram_cache por CHAVE, nao por variavel global.
 *   - Duas chaves podem dividir o mesmo bloco (FOLHA COMPARTILHADA), o
 *     que e o caso de limo/erect/limoDrive: 5 sprites, 1 folha.
 *   - O despejo e automatico e acontece no meio do quadro se o palco
 *     tiver mais conteudo do que a regiao inteira.
 *
 * O modulo e C puro. O acesso a arquivo e ao GS entra por ponteiros de
 * funcao, entao o mesmo codigo roda no PC dentro do teste com um
 * "pendrive" sintetico e mede tudo antes de ir ao console.
 */
#ifndef ASSET_BANK_H
#define ASSET_BANK_H

#include "vram_cache.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AB_MAX_ATIVOS      96
#define AB_CHAVE_INVALIDA  0u   /* 0 nunca e usado como chave */

typedef enum {
    AB_FONTE = 0,   /* fixa a vida inteira do jogo */
    AB_HUD,         /* fixa enquanto houver gameplay */
    AB_PALCO,       /* troca por musica */
    AB_PERSONAGEM,  /* troca por musica */
    AB_PROP,        /* por musica */
    AB_UI           /* por tela */
} ab_classe_t;

/* Devolve 0 e preenche dados/bytes. 'dados' deve ser liberado com
   ab_soltar_fn quando o upload terminar (o banco nunca guarda a
   staging area depois do upload). */
typedef int  (*ab_carregar_fn)(void *ctx, const char *caminho,
                               unsigned char **dados, unsigned int *bytes);
typedef void (*ab_enviar_fn)(void *ctx, unsigned int chave_vram,
                             unsigned int endereco_vram,
                             const unsigned char *dados, unsigned int bytes);
typedef void (*ab_soltar_fn)(void *ctx, unsigned char *dados);

typedef struct {
    unsigned int chave;
    unsigned int bytes_vram;
    unsigned int endereco;
    unsigned int bytes_arquivo;
    unsigned short cliente;      /* indice em clientes[] */
    unsigned char  classe;
    unsigned char  residente;
} ab_ativo_t;

typedef struct {
    vc_t   vram;
    ab_ativo_t ativos[AB_MAX_ATIVOS];
    int    n_ativos;

    ab_carregar_fn carregar;
    ab_enviar_fn   enviar;
    ab_soltar_fn   soltar;
    void  *ctx;

    /* contadores para a telemetria e para o teste */
    unsigned int cargas;        /* vezes que abriu arquivo */
    unsigned int uploads;       /* vezes que enviou pixels ao GS */
    unsigned int bytes_lidos;   /* bytes vindos do pendrive */
    unsigned int bytes_enviados;/* bytes enviados ao GS */
    unsigned int aliases;
    unsigned int falhas;
    unsigned int maior_vivos_tela;
    const char  *cena;
} asset_bank_t;

void ab_init(asset_bank_t *b, unsigned int vram_base, unsigned int vram_bytes,
             ab_carregar_fn carregar, ab_enviar_fn enviar, ab_soltar_fn soltar,
             void *ctx);

/* Marca o que e permanente. Chamar no boot, depois de ab_init.
   Fonte e HUD entram pelo mesmo caminho e ficam presos. */
int ab_precisa_fixo(asset_bank_t *b, unsigned int chave, const char *caminho,
                    ab_classe_t classe, unsigned int bytes_vram);

/* Pede um asset de cena. Devolve:
 *   0 = ja estava na VRAM, nada foi enviado
 *   1 = carregado do pendrive e enviado agora
 *  -1 = nao coube nem despejando / erro de E-S / acima da capacidade */
int ab_precisa(asset_bank_t *b, unsigned int chave, const char *caminho,
               unsigned int bytes_vram, unsigned int *endereco);

/* Segunda chave no MESMO bloco (folha compartilhada). */
int ab_alias(asset_bank_t *b, unsigned int chave_origem, unsigned int chave_nova);

/* Troca de cena: despeja tudo que nao esta preso. Chamar com a cena nova. */
void ab_cena_entrar(asset_bank_t *b, const char *nome_cena);
/* Fim da cena: libera tudo, inclusive pedidos feitos com ab_precisa(). */
void ab_cena_sair(asset_bank_t *b);

int  ab_residente(const asset_bank_t *b, unsigned int chave);
int  ab_vivos(const asset_bank_t *b);

/* Relatorio de uma linha, sem printf. */
void ab_relatar(const asset_bank_t *b, char *destino, int tam);

#ifdef __cplusplus
}
#endif
#endif
