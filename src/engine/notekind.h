/* notekind - o que um NOTE KIND faz, lido dos scripts do original
 *
 * POR QUE ISTO EXISTE
 * 34 kinds aparecem nos 81 charts. O port implementava dois (`noanim`,
 * `non_scoreable`, que sao CODIGO do jogo) e reportava os outros 32 como
 * "script nao esta' no pacote de dados". Estava errado: os scripts estao em
 * `preload/scripts/` do clone da arte, e `tools/gen_notekinds.py` os le' e
 * gera `notekind_dados.h`. Este modulo e' so' a busca nessa tabela.
 *
 * O QUE O DADO DIZ (e o que nao diz)
 *   tipo 1 -- canto com sufixo: `playSingAnimation(dir, false, 'alt')` vira
 *             `sing<DIR>-alt` (e' o `alt` das pais de Natal, e o `joint`/`bf1`/
 *             `bf2` da Sakura). Se o personagem nao tem a animacao com sufixo,
 *             a nota toca o canto normal e o log diz que nao achou -- o sufixo
 *             nao vira um nome inventado.
 *   tipo 2 -- animacao nomeada: o case do script chama uma funcao que faz
 *             `playAnimation('punchHigh')`. O nome da animacao vem escrito no
 *             script, nao de uma tabela paralela.
 *   `condicional` marca os cases que o script decide por estado (vida baixa,
 *             sorteio, `cantUppercut`): a acao esta' no dado, mas o port NAO
 *             escolhe sozinho qual caminho -- ele registra.
 *   `ordem`/`shake` vem do mesmo trecho (`moveToBack()`/`moveToFront()` e
 *             `camGame.shake(...)`). O shake ainda NAO e' aplicado: e' o
 *             proximo item, e fica no dado para nao ser reinventado.
 *
 * C puro: sem GS, sem arquivo. Roda no PC, com teste.
 */
#ifndef FNF_NOTEKIND_H
#define FNF_NOTEKIND_H

#include "notekind_dados.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Nome de exibicao do kind (o segundo argumento do `super` da classe), ou NULL
   quando o id nao e' um kind de script. */
const char *nk_nome(const char *kind);

/* O que o kind faz NESTE personagem, ou NULL quando nao faz nada (kind de
   codigo, personagem sem script para ele, ou kind que o personagem nao cita). */
const nk_acao_t *nk_acao(const char *personagem, const char *kind);

/* Quantos kinds de script foram lidos (a lista fechada). */
unsigned int nk_kind_count(void);

/* Quantos (personagem, kind) o dado tem -- para o log dizer o tamanho do dado
   em vez de "implementado". */
unsigned int nk_acao_count(void);

#ifdef __cplusplus
}
#endif
#endif /* FNF_NOTEKIND_H */
