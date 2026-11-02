/* save: o recorde por musica e dificuldade (o dado que faltava para o selo de
 * recorde existir).
 *
 * O QUE PROVA, com a linha da fonte de cada regra:
 *  1. `Save.isSongHighScore` (Save.hx L568-582): sem registro -> verdadeiro
 *     (o original devolve `true`, nao `false`), e depois disso so' um placar
 *     MAIOR conta;
 *  2. `Save.applySongRank` (L531-566): guarda o MAIOR placar e os tallies que a
 *     condicao da fonte escolhe -- os anteriores quando
 *     `previousRank > newRank || tallyCompletion(previous) > tallyCompletion(new)`,
 *     senao os novos; e um registro sem rank (totalNotes 0) nao entra;
 *  3. o rank do registro sai de `fnf_play_rank` (a MESMA funcao que julgou a
 *     musica durante o jogo) -- sem formula paralela;
 *  4. o formato: ida e volta exata, e recusa INTEIRA de arquivo curto, com
 *     magic errado, versao errada, contagem errada e CRC errado. Nada meio
 *     lido: um save corrompido nao pode virar "voce tem 999.999 pontos".
 */
#include <stdio.h>
#include <string.h>

#include "save.h"

static int falhas;
#define CHECK(c) do { if (!(c)) { printf("  FALHA: %s (linha %d)\n", #c, __LINE__); falhas++; } } while (0)
#define CHECK_EQ(a, b) do { long _a = (long)(a), _b = (long)(b); \
    if (_a != _b) { printf("  FALHA: %s == %s: %ld != %ld (linha %d)\n", #a, #b, _a, _b, __LINE__); falhas++; } } while (0)

static save_score_t registro(unsigned int score, int sick, int good, int bad,
                             int shit, int missed, int total)
{
    save_score_t r;
    memset(&r, 0, sizeof(r));
    r.score = score;
    r.sick = (unsigned short)sick;
    r.good = (unsigned short)good;
    r.bad = (unsigned short)bad;
    r.shit = (unsigned short)shit;
    r.missed = (unsigned short)missed;
    r.combo = (unsigned short)total;
    r.max_combo = (unsigned short)total;
    r.total_hit = (unsigned short)(sick + good + bad + shit);
    r.total_notes = (unsigned short)total;
    r.valido = 1;
    return r;
}

int main(void)
{
    save_t sv, volta;
    static unsigned char buf[SAVE_ARQUIVO_BYTES];
    unsigned int n;
    save_score_t a, b;

    printf("== save: recorde por musica e dificuldade ==\n");

    /* ---- 0. save vazio nao tem nada ------------------------------- */
    save_init(&sv);
    CHECK(!sv.carregado);
    CHECK(!sv.sujo);
    CHECK(save_obter(&sv, 0, 0) == 0);
    CHECK_EQ(save_rank(save_obter(&sv, 0, 0)), -1);
    CHECK(save_obter(&sv, -1, 0) == 0);
    CHECK(save_obter(&sv, SAVE_MUSICAS, 0) == 0);
    CHECK(save_obter(&sv, 0, SAVE_DIFS) == 0);
    printf("  save vazio: nenhum registro, indices invalidos devolvem NULL\n");

    /* ---- 1. primeiro resultado E' recorde ------------------------- */
    a = registro(50000u, 30, 2, 1, 0, 1, 34);
    CHECK(save_novo_recorde(&sv, 3, 1, 50000u));
    /* ate' com placar zero: sem registro, o original responde true
       (`currentScore == null -> return true`). */
    CHECK(save_novo_recorde(&sv, 3, 1, 0u));
    save_aplicar(&sv, 3, 1, &a);
    CHECK(sv.sujo);
    CHECK(save_obter(&sv, 3, 1) != 0);
    CHECK_EQ(save_obter(&sv, 3, 1)->score, 50000);
    printf("  primeiro resultado: recorde=true, gravado 50000\n");

    /* ---- 1b. o empate NAO e' recorde (score > anterior) ----------- */
    CHECK(!save_novo_recorde(&sv, 3, 1, 50000u));
    CHECK(save_novo_recorde(&sv, 3, 1, 50001u));
    /* em outra dificuldade e' independente */
    CHECK(save_novo_recorde(&sv, 3, 0, 0u));
    CHECK(save_novo_recorde(&sv, 4, 1, 0u));
    printf("  empate nao conta; dificuldades e musicas sao independentes\n");

    /* ---- 2. applySongRank: maior placar + tallies do melhor rank -- */
    /* 40 notas: rank de 30 sick + 2 good + 1 bad + 1 miss = (30+2-1)/34 = 0,911
       -> EXCELLENT (>= 0,90). O novo tem 20/20 perfect? nao: sick=34 de 34 com
       placar MENOR -- o rank e' melhor (PERFECT_GOLD, sick == totalNotes) mas o
       placar e' pior: o save tem de ficar com o MAIOR placar E os tallies do
       melhor rank. */
    b = registro(40000u, 34, 0, 0, 0, 0, 34);
    save_aplicar(&sv, 3, 1, &b);
    CHECK_EQ(save_obter(&sv, 3, 1)->score, 50000);       /* maior placar */
    CHECK_EQ(save_obter(&sv, 3, 1)->sick, 34);           /* tallies melhores */
    CHECK_EQ(save_obter(&sv, 3, 1)->missed, 0);
    CHECK_EQ(save_rank(save_obter(&sv, 3, 1)), 5);       /* PERFECT_GOLD */
    printf("  applySongRank: placar 50000 (o maior) com os tallies do"
           " PERFECT_GOLD\n");

    /* pior rank: os tallies ficam, o placar sobe se for maior. */
    save_aplicar(&sv, 3, 1, &b);
    CHECK_EQ(save_obter(&sv, 3, 1)->score, 50000);
    CHECK_EQ(save_obter(&sv, 3, 1)->sick, 34);
    {
        save_score_t pior = registro(60000u, 1, 1, 1, 1, 30, 34);
        save_aplicar(&sv, 3, 1, &pior);
        CHECK_EQ(save_obter(&sv, 3, 1)->score, 60000);   /* o placar subiu */
        CHECK_EQ(save_obter(&sv, 3, 1)->sick, 34);       /* o rank nao caiu */
        CHECK_EQ(save_rank(save_obter(&sv, 3, 1)), 5);
    }
    /* ---- 2b. o empate EXATO cai para os tallies NOVOS ---------------- */
    /* A condicao da fonte (L554-556) e' um OU de duas comparacoes:
     *   previousRank > newRank  ||  tallyCompletion(previous) > tallyCompletion(new)
     * Empatados rank E completion, o OU e' falso e o original fica com os
     * tallies NOVOS -- nao com os anteriores. Os dois registros abaixo tem o
     * MESMO rank e a MESMA completion (0,80) e tallies diferentes, entao este
     * caso distingue as duas leituras. */
    {
        save_t t;
        /* completion = (sick + good - missed) / total: as duas linhas dao
           0,80 por caminhos diferentes. 18 de 20 (e nao 10 de 10) para nao
           cair no PERFECT_GOLD, que e' `sick == totalNotes`. */
        save_score_t prev = registro(1000u, 18, 0, 0, 0, 2, 20);  /* 0,80 */
        save_score_t novo = registro(1000u, 17, 1, 0, 0, 2, 20);  /* 0,80 */
        save_init(&t);
        save_aplicar(&t, 5, 2, &prev);
        CHECK_EQ(save_rank(&prev), 2);
        CHECK_EQ(save_rank(&novo), 2);
        save_aplicar(&t, 5, 2, &novo);
        CHECK_EQ(save_obter(&t, 5, 2)->sick, 17);  /* os NOVOS */
        CHECK_EQ(save_obter(&t, 5, 2)->good, 1);
        CHECK_EQ(save_obter(&t, 5, 2)->score, 1000);
        printf("  empate de rank e de completion: os tallies sao os NOVOS\n");
    }

    /* ---- 2c. registro novo SEM rank (totalNotes == 0) nao entra ------ */
    /* `if (newScoreData == null || newRank == null) return;` (L536):
     * `Scoring.calculateRank` devolve null sem notas, e o applySongRank sai
     * sem tocar no save -- nem no placar. */
    {
        save_t t;
        save_score_t prev = registro(1000u, 8, 0, 0, 0, 2, 10);
        save_score_t vazio = registro(999999u, 0, 0, 0, 0, 0, 0);
        save_init(&t);
        save_aplicar(&t, 6, 0, &prev);
        t.sujo = 0;
        save_aplicar(&t, 6, 0, &vazio);
        CHECK_EQ(save_obter(&t, 6, 0)->score, 1000);
        CHECK(!t.sujo);
        CHECK_EQ(save_rank(&vazio), -1);
        printf("  registro sem notas (rank null) nao entra no save\n");
    }

    printf("  placar pior + rank melhor: guarda o maior placar e o melhor"
           " rank\n");

    /* ---- 3. o rank vem do MESMO calculo do jogo ------------------- */
    {
        /* sick em tudo = PERFECT_GOLD; 90% = EXCELLENT; 50% = SHIT (abaixo de
           GOOD, que e' 0,60); zero notas = sem rank. */
        save_score_t r1 = registro(1u, 10, 0, 0, 0, 0, 10);
        save_score_t r2 = registro(1u, 9, 0, 0, 0, 0, 10);  /* 0,90 */
        save_score_t r2b = registro(1u, 9, 0, 0, 0, 1, 10); /* 0,80 -> GREAT */
        save_score_t r3 = registro(1u, 5, 0, 0, 0, 5, 10);
        save_score_t r4 = registro(1u, 0, 0, 0, 0, 0, 0);
        CHECK_EQ(save_rank(&r1), 5);   /* PERFECT_GOLD */
        CHECK_EQ(save_rank(&r2), 3);   /* EXCELLENT (0,90) */
        CHECK_EQ(save_rank(&r2b), 2);  /* GREAT (0,80) */
        CHECK_EQ(save_rank(&r3), 0);   /* SHIT */
        CHECK_EQ(save_rank(&r4), -1);  /* sem notas */
        CHECK_EQ(save_rank(0), -1);
    }
    printf("  rank do registro: PERFECT_GOLD / EXCELLENT / SHIT / -1\n");

    /* ---- 4. o formato: ida e volta -------------------------------- */
    save_aplicar(&sv, 0, 0, &a);
    save_aplicar(&sv, SAVE_MUSICAS - 1, SAVE_DIFS - 1, &b);
    n = save_serializar(&sv, buf);
    CHECK_EQ(n, SAVE_ARQUIVO_BYTES);
    CHECK(memcmp(buf, "FNFS", 4) == 0);
    CHECK_EQ(buf[16 + 22], 1);       /* o `valido` do primeiro registro */
    save_init(&volta);
    CHECK_EQ(save_desserializar(&volta, buf, n), 0);
    CHECK(volta.carregado);
    CHECK(!volta.sujo);
    CHECK_EQ(memcmp(&volta, &sv, 0), 0);   /* ignora carregado/sujo abaixo */
    CHECK_EQ(save_obter(&volta, 3, 1)->score, 60000);
    CHECK_EQ(save_obter(&volta, 3, 1)->sick, 34);
    CHECK_EQ(save_obter(&volta, 0, 0)->score, 50000);
    CHECK_EQ(save_obter(&volta, SAVE_MUSICAS - 1, SAVE_DIFS - 1)->score, 40000);
    CHECK(save_obter(&volta, 1, 1) == 0);  /* o que nunca foi jogado continua 0 */
    printf("  ida e volta: %u bytes, tres registros conferidos, o resto vazio\n", n);

    /* ---- 4b. qualquer corrupcao e' recusada INTEIRA ----------------- */
    {
        unsigned char copia[SAVE_ARQUIVO_BYTES];
        save_t destino;

        memcpy(copia, buf, sizeof(copia));
        copia[0] = 'X';
        save_init(&destino);
        CHECK_EQ(save_desserializar(&destino, copia, n), SAVE_ERR_MAGIC);
        CHECK(!destino.carregado);
        CHECK(save_obter(&destino, 3, 1) == 0);

        memcpy(copia, buf, sizeof(copia));
        copia[4] = 99;
        CHECK_EQ(save_desserializar(&destino, copia, n), SAVE_ERR_VERSAO);

        memcpy(copia, buf, sizeof(copia));
        copia[6] = 7;
        CHECK_EQ(save_desserializar(&destino, copia, n), SAVE_ERR_CONTAGEM);

        memcpy(copia, buf, sizeof(copia));
        copia[16 + 24 * 5 + 1] ^= 0xFF;   /* um byte do placar de um registro */
        CHECK_EQ(save_desserializar(&destino, copia, n), SAVE_ERR_CRC);
        CHECK(save_obter(&destino, 3, 1) == 0);

        memcpy(copia, buf, 16);
        CHECK_EQ(save_desserializar(&destino, copia, 16), SAVE_ERR_CURTO);
        printf("  corrupcao: magic/versao/contagem/CRC/curto recusados,"
               " e o destino fica intocado\n");
    }

    printf("== save: o FAVORITO e o arquivo da VERSAO 1\n");
    {
        /* `Save.favoriteSong`/`unfavoriteSong` (Save.hx L671-686) e
           `isSongFavorited` (L660-668): o favorito e' por MUSICA, nao por
           dificuldade. */
        unsigned int r;
        save_init(&sv);
        CHECK_EQ(save_favorito(&sv, 3), 0);
        CHECK_EQ(save_favorito(&sv, 99), 0);          /* fora da faixa */
        CHECK_EQ(save_favorito(&sv, -1), 0);
        save_favoritar(&sv, 99, 1);                   /* nao estoura */
        CHECK(!sv.sujo);
        save_favoritar(&sv, 3, 1);
        CHECK_EQ(save_favorito(&sv, 3), 1);
        CHECK(sv.sujo);
        CHECK_EQ(save_favorito(&sv, 0), 0);
        CHECK_EQ(save_favorito(&sv, 4), 0);

        n = save_serializar(&sv, buf);
        CHECK_EQ(n, SAVE_ARQUIVO_BYTES);
        CHECK_EQ(SAVE_ARQUIVO_BYTES, 1992u);
        CHECK_EQ(SAVE_ARQUIVO_BYTES_V1, 1960u);
        CHECK_EQ(save_desserializar(&volta, buf, n), 0);
        CHECK_EQ(save_favorito(&volta, 3), 1);
        CHECK_EQ(save_favorito(&volta, 4), 0);
        CHECK(!volta.sujo);
        /* Repetir o mesmo valor nao suja o arquivo; trocar, sim. */
        save_favoritar(&volta, 3, 1);
        CHECK(!volta.sujo);
        save_favoritar(&volta, 3, 0);
        CHECK_EQ(save_favorito(&volta, 3), 0);
        CHECK(volta.sujo);

        /* O ARQUIVO DA VERSAO 1 (1960 B): mesmo corpo, versao 1, SEM a secao do
           favorito. Tem de ser lido inteiro -- recusa-lo apagaria o placar de
           quem jogou na build anterior --, com favorito nenhum. */
        {
            static unsigned char v1[SAVE_ARQUIVO_BYTES_V1];
            save_t velho;
            save_init(&sv);
            save_aplicar(&sv, 5, 2, &a);
            n = save_serializar(&sv, buf);
            CHECK_EQ(n, SAVE_ARQUIVO_BYTES);
            memcpy(v1, buf, SAVE_ARQUIVO_BYTES_V1);
            v1[4] = 1; v1[5] = 0;                     /* versao 1 */
            {
                unsigned int crc = save_crc32(v1 + 16, SAVE_CORPO_V1);
                v1[12] = (unsigned char)(crc & 0xFFu);
                v1[13] = (unsigned char)((crc >> 8) & 0xFFu);
                v1[14] = (unsigned char)((crc >> 16) & 0xFFu);
                v1[15] = (unsigned char)((crc >> 24) & 0xFFu);
            }
            save_init(&velho);
            r = save_desserializar(&velho, v1, SAVE_ARQUIVO_BYTES_V1);
            CHECK_EQ(r, 0);
            CHECK(velho.carregado);
            CHECK(save_obter(&velho, 5, 2) != 0);
            CHECK_EQ(save_obter(&velho, 5, 2)->score, a.score);
            CHECK_EQ(save_favorito(&velho, 5), 0);
            CHECK_EQ(save_favorito(&velho, 3), 0);
            /* E os 1960 B NAO valem como versao 2: o CRC cobre outro tamanho. */
            memcpy(v1, buf, SAVE_ARQUIVO_BYTES_V1);
            CHECK_EQ(save_desserializar(&velho, v1, SAVE_ARQUIVO_BYTES_V1),
                     SAVE_ERR_CURTO);
            printf("  favorito por musica; v2 ida e volta; v1 lido com o placar\n");
        }
    }

    printf(falhas ? "\nSAVE FALHOU (%d)\n" : "\nSAVE OK\n", falhas);
    return falhas ? 1 : 0;
}
