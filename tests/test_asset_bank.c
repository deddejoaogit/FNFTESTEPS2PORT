/* Teste do modelo de carregamento, com os numeros reais do projeto:
 *
 *   VRAM total ........................... 4.194.304 B
 *   dois framebuffers 640x448 PSMCT32 .... 2.293.760 B
 *   sobra para textura ................... 1.900.544 B
 *   fixos de toda cena (HUD+fonte+palco) . 1.146.880 B  (medido na fase 0)
 *   sobra por cena .......................   753.664 B
 *
 * O "pendrive" e sintetico, mas o ORCAMENTO e o real, e as contagens de
 * texturas vivas sao as medidas nos palcos (tankmanBattlefield 31,
 * phillyStreetsErect 28, mainStage 16, media 21).
 */
#include "../src/engine/asset_bank.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int falhas = 0, checks = 0;
static void ok(int cond, const char *txt)
{
    checks++;
    if (!cond) { falhas++; printf("FALHOU: %s\n", txt); }
}
#define OK(c) ok((c), #c)

#define VRAM_TOTAL   4194304u
#define VRAM_FB      2293760u
#define VRAM_TEX     (VRAM_TOTAL - VRAM_FB)

/* --- pendrive de mentira ------------------------------------------ */
typedef struct {
    unsigned int leituras;
    unsigned int bytes;
    unsigned char *buf;
    unsigned int cap;
} hd_t;

static int fake_carregar(void *ctx, const char *caminho, unsigned char **dados, unsigned int *bytes)
{
    hd_t *h = (hd_t *)ctx;
    unsigned int n = 4096; /* tamanho fixo do "arquivo" */
    (void)caminho;
    h->leituras++;
    h->bytes += n;
    *dados = h->buf;
    *bytes = n;
    return 0;
}
static void fake_enviar(void *ctx, unsigned int chave, unsigned int endereco,
                        const unsigned char *d, unsigned int bytes)
{
    hd_t *h = (hd_t *)ctx;
    (void)chave; (void)endereco; (void)d; (void)bytes;
    h->cap++;
}
static void fake_soltar(void *ctx, unsigned char *d) { (void)ctx; (void)d; }

int main(void)
{
    asset_bank_t banco;
    hd_t hd;
    unsigned char *buf = (unsigned char *)calloc(1, 65536);
    unsigned int addr, i;
    char linha[200];
    char *cenas[] = { (char *)"mainStage", (char *)"tankmanBattlefield",
                      (char *)"phillyStreetsErect", (char *)"limoRideErect",
                      (char *)"sserafim" };

    memset(&hd, 0, sizeof(hd));
    hd.buf = buf;

    printf("== orcamento ==\n");
    printf("   VRAM %u, framebuffers %u, sobra para textura %u\n",
           VRAM_TOTAL, VRAM_FB, VRAM_TEX);
    OK(VRAM_TEX == 1900544u);

    printf("== boot: fixos ==\n");
    ab_init(&banco, VRAM_FB, VRAM_TEX, fake_carregar, fake_enviar, fake_soltar, &hd);
    OK(vc_livre(&banco.vram) == 1900544u);
    /* fonte 327.680 + HUD 294.912 + cenario 524.288 = 1.146.880 (fase 0) */
    OK(ab_precisa_fixo(&banco, 0x1001, "fonte.rgba", AB_FONTE, 327680) >= 0);
    OK(ab_precisa_fixo(&banco, 0x1002, "hud.rgba", AB_HUD, 294912) >= 0);
    OK(ab_precisa_fixo(&banco, 0x1003, "cenario.rgba", AB_PALCO, 524288) >= 0);
    OK(vc_livre(&banco.vram) == VRAM_TEX - 1146880u);
    OK(vc_livre(&banco.vram) == 753664u);
    printf("   sobra por cena: %u bytes\n", vc_livre(&banco.vram));

    printf("== palco com 31 texturas vivas ==\n");
    /* o pior caso medido: 31 texturas. 24 KB cada cabe em 753 KB. */
    ab_cena_entrar(&banco, "tankmanBattlefield");
    for (i = 0; i < 31; i++) {
        int r = ab_precisa(&banco, 0x2000 + i, "tank0.lz4f", 24000, &addr);
        if (r < 0) { ab_relatar(&banco, linha, sizeof(linha)); printf("   textura %u nao coube (%s)\n", i, linha); }
        OK(r >= 0);
    }
    OK(ab_vivos(&banco) == 34); /* 31 + 3 fixos */
    OK(banco.vram.falhas == 0);
    ab_relatar(&banco, linha, sizeof(linha));
    printf("   %s\n", linha);

    printf("== residente nao e reenviado ==\n");
    {
        unsigned int uploads_antes = banco.uploads;
        unsigned int a2;
        OK(ab_precisa(&banco, 0x2000, "tank0.lz4f", 24000, &a2) == 0);
        OK(banco.uploads == uploads_antes);
        { unsigned int a3x = 0; OK(ab_precisa(&banco, 0x2000, "tank0.lz4f", 24000, &a3x) == 0); OK(a3x == a2); }
    }

    printf("== folha compartilhada (limo/henchmen) ==\n");
    {
        unsigned int a1 = 0, a2 = 0, a3 = 0, a4 = 0, a5 = 0;
        ab_cena_entrar(&banco, "limoRideErect");
        OK(ab_precisa(&banco, 0x3000, "henchmen.lz4f", 300000, &a1) == 1);
        OK(ab_alias(&banco, 0x3000, 0x3001) == 0);
        OK(ab_alias(&banco, 0x3000, 0x3002) == 0);
        OK(ab_alias(&banco, 0x3000, 0x3003) == 0);
        OK(ab_alias(&banco, 0x3000, 0x3004) == 0);
        OK(ab_precisa(&banco, 0x3001, "henchmen.lz4f", 0, &a2) == 0);
        OK(ab_precisa(&banco, 0x3004, "henchmen.lz4f", 0, &a3) == 0);
        OK(a2 == a1 && a3 == a1);   /* os 5 sprites apontam para UM bloco */
        (void)a4; (void)a5;
        printf("   endereco unico do bloco compartilhado: %u\n", a1);
        OK(banco.aliases == 4);
    }

    printf("== despejo quando o palco nao cabe inteiro ==\n");
    {
        int r;
        ab_cena_entrar(&banco, "limoRideErect");
        /* 16 texturas de 60 KB = 960 KB > 753 KB disponiveis: o cache
           precisa despejar durante o carregamento, e nao falhar. */
        for (i = 0; i < 16; i++) {
            r = ab_precisa(&banco, 0x4000 + i, "grande.lz4f", 61440, &addr);
            if (r < 0) printf("   falhou na textura %u\n", i);
            OK(r >= 0);
        }
        OK(banco.vram.falhas == 0);
        OK(banco.vram.despejos > 0);
        printf("   despejos=%u, vivos=%d, livre=%u\n",
               banco.vram.despejos, ab_vivos(&banco), vc_livre(&banco.vram));
        /* as fixas continuam no lugar */
        OK(ab_residente(&banco, 0x1001));
        OK(ab_residente(&banco, 0x1002));
    }

    printf("== troca de cena libera tudo ==\n");
    for (i = 0; i < 5; i++) {
        ab_cena_entrar(&banco, cenas[i]);
        OK(ab_precisa(&banco, 0x5000, "a.lz4f", 400000, &addr) >= 0);
        OK(ab_precisa(&banco, 0x5001, "b.lz4f", 300000, &addr) >= 0);
    }
    ab_cena_sair(&banco);
    OK(ab_vivos(&banco) == 0);
    /* depois de sair, os fixos tambem saem (fim de jogo) e a VRAM volta
       inteira: e a prova de que nao existe vazamento. */
    OK(vc_livre(&banco.vram) == VRAM_TEX);
    ab_relatar(&banco, linha, sizeof(linha));
    printf("   %s\n", linha);
    printf("   leituras de pendrive=%u bytes=%u envios=%u\n", hd.leituras, hd.bytes, hd.cap);

    printf("== 200 trocas de cena sem fragmentar ==\n");
    {
        ab_init(&banco, VRAM_FB, VRAM_TEX, fake_carregar, fake_enviar, fake_soltar, &hd);
        for (i = 0; i < 200; i++) {
            ab_cena_entrar(&banco, "x");
            ab_precisa(&banco, 0x6000, "a", 400000, &addr);
            ab_precisa(&banco, 0x6001, "b", 200000, &addr);
            ab_precisa(&banco, 0x6002, "c", 100000, &addr);
            ab_precisa(&banco, 0x6003, "d", 50000, &addr);
        }
        ab_cena_sair(&banco);
        OK(vc_livre(&banco.vram) == VRAM_TEX);
        OK(vc_maior_buraco(&banco.vram) == VRAM_TEX);
        OK(banco.vram.falhas == 0);
    }

    printf("== nao cabe: erro, nao silencio ==\n");
    {
        ab_init(&banco, VRAM_FB, VRAM_TEX, fake_carregar, fake_enviar, fake_soltar, &hd);
        OK(ab_precisa(&banco, 0x7000, "gigante", 2000000, &addr) == -1);
        OK(banco.falhas == 1);
    }

    free(buf);
    printf("\n%d verificacoes, %d falhas\n", checks, falhas);
    return falhas ? 1 : 0;
}
