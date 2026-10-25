/* Teste do alocador de VRAM, incluindo as tres funcoes novas
   (apelido, prender/soltar, despejar_ate). */
#include "../src/engine/vram_cache.h"
#include <stdio.h>
#include <string.h>

static int falhas = 0, checks = 0;
static void ok(int cond, const char *txt)
{
    checks++;
    if (!cond) { falhas++; printf("FALHOU: %s\n", txt); }
}
#define OK(c) ok((c), #c)

int main(void)
{
    vc_t c;
    unsigned int a1 = 0, a2 = 0, a3 = 0;
    char linha[160];

    printf("== alinhamento em 256 ==\n");
    vc_init(&c, 0x100000, 100000 & ~255u);
    OK(c.tamanho == 99840);
    OK(vc_livre(&c) == 99840);
    OK(vc_maior_buraco(&c) == 99840);
    OK(vc_pedir(&c, 11, 100, &a1) == 0);     /* 100 -> 256 */
    OK(a1 == 0x100000);
    OK(vc_livre(&c) == 99840 - 256);
    OK(vc_pedir(&c, 11, 100, &a2) == 1);     /* residente */
    OK(a2 == a1);
    OK(c.acertos == 1 && c.faltas == 1);

    printf("== divisao e fusao de buracos ==\n");
    vc_init(&c, 0, 4096);
    OK(vc_pedir(&c, 1, 1024, &a1) == 0);
    OK(vc_pedir(&c, 2, 1024, &a2) == 0);
    OK(vc_pedir(&c, 3, 1024, &a3) == 0);
    OK(a1 == 0 && a2 == 1024 && a3 == 2048);
    OK(vc_pedir(&c, 4, 1024, &a1) == 0);     /* ultimo espaco */
    OK(vc_livre(&c) == 0);
    /* nada mais cabe: despeja o LRU (a chave 1) e usa o buraco */
    OK(vc_pedir(&c, 5, 1024, &a1) == 0);
    OK(c.despejos == 1);
    OK(vc_livre(&c) == 0);

    printf("== despejo por menos usado recentemente ==\n");
    vc_init(&c, 0, 2048);
    vc_pedir(&c, 10, 1024, &a1);
    vc_pedir(&c, 20, 1024, &a2);
    vc_pedir(&c, 10, 1024, &a3);             /* 10 vira o mais recente */
    OK(vc_pedir(&c, 30, 1024, &a3) == 0);    /* despeja o 20, nao o 10 */
    OK(c.despejos == 1);
    OK(vc_livre(&c) == 0);

    printf("== fixo nunca e despejado ==\n");
    vc_init(&c, 0, 2048);
    vc_pedir(&c, 1, 1024, &a1);
    vc_fixar(&c, 1);
    vc_pedir(&c, 2, 1024, &a2);
    vc_fixar(&c, 2);
    OK(vc_pedir(&c, 3, 1024, &a3) == -1);    /* nao ha o que despejar */
    OK(c.falhas == 1);
    vc_limpar(&c);                           /* limpar tambem respeita */
    OK(vc_livre(&c) == 0);

    printf("== prender/soltar com contagem ==\n");
    {
        /* Duas telas prendem a mesma chave. Enquanto UMA delas existir, a
           textura nao pode ser despejada. Com o antigo vc_fixar booleano,
           a primeira tela a sair soltava a textura da outra. */
        unsigned int k;
        int sobreviveu = 1;
        vc_init(&c, 0, 2048);
        vc_pedir(&c, 1, 1024, &a1);
        vc_prender(&c, 1);
        vc_prender(&c, 1);
        vc_pedir(&c, 2, 1024, &a2);
        /* 4 pedidos de 1024 em 2048: so cabe despejando o 2, nunca o 1 */
        for (k = 3; k <= 6; k++) {
            vc_pedir(&c, k, 1024, &a3);
            if (vc_pedir(&c, 1, 1024, &a2) != 1 || a2 != a1) sobreviveu = 0;
        }
        OK(sobreviveu);
        OK(c.despejos > 0);
        vc_soltar(&c, 1);
        /* uma tela saiu, a outra continua: ainda preso */
        OK(vc_pedir(&c, 1, 1024, &a2) == 1);
        vc_soltar(&c, 1);
        vc_soltar(&c, 1); /* soltar demais nao vira negativo */
        OK(c.b[0].fixo == 0);
    }

    printf("== apelido: 5 sprites, 1 textura ==\n");
    vc_init(&c, 0, 1048576);
    OK(vc_pedir(&c, 77, 524288, &a1) == 0);
    {
        unsigned int livre_antes = vc_livre(&c);
        OK(vc_apelido(&c, 77, 78) == 0);
        OK(vc_apelido(&c, 77, 79) == 0);
        OK(vc_apelido(&c, 77, 80) == 0);
        OK(vc_apelido(&c, 77, 81) == 0);
        OK(vc_livre(&c) == livre_antes);     /* apelido nao ocupa VRAM */
        OK(vc_pedir(&c, 81, 524288, &a2) == 1);
        OK(a2 == a1);
        OK(vc_pedir(&c, 78, 524288, &a2) == 1);
        OK(a2 == a1);
    }
    /* apelido de chave inexistente falha */
    OK(vc_apelido(&c, 12345, 12346) == -1);

    printf("== despejar_ate ==\n");
    vc_init(&c, 0, 4096);
    vc_pedir(&c, 1, 1024, &a1);
    vc_pedir(&c, 2, 1024, &a2);
    vc_pedir(&c, 3, 1024, &a3);
    vc_pedir(&c, 4, 1024, &a1);
    OK(vc_maior_buraco(&c) < 2048);
    OK(vc_despejar_ate(&c, 2048) == 0);
    OK(vc_maior_buraco(&c) >= 2048);
    OK(vc_despejar_ate(&c, 100000) == -1);   /* nao existe espaco possivel */

    printf("== guarda de limite (o defeito que so aparece no console) ==\n");
    {
        /* A regiao de textura comeca depois dos dois framebuffers. Nenhum
           pedido pode terminar fora dela: um envio que passa do fim da
           VRAM trava o caminho GIF/DMA e o console congela no primeiro
           vsync, enquanto no PC o mesmo codigo passa de 1400 quadros. */
        vc_init(&c, 2293760, 1900544);
        OK(vc_pedir(&c, 1, 1900544, &a1) == 0);
        OK(a1 == 2293760);
        OK(a1 + 1900544 == 4194304);      /* topo exato da VRAM */
        OK(vc_pedir(&c, 2, 1900544, &a2) == 0);  /* despeja e reusa */
        OK(a2 == 2293760);
        OK(vc_pedir(&c, 3, 1900800, &a3) == -1); /* maior que a regiao */
        OK(c.falhas >= 1);
        /* e nenhum bloco alem do topo, nunca */
        {
            int k, dentro = 1;
            for (k = 0; k < c.n; k++) {
                if (c.b[k].usado && c.b[k].endereco + c.b[k].bytes > 4194304) dentro = 0;
                if (c.b[k].usado && c.b[k].endereco < 2293760) dentro = 0;
            }
            OK(dentro);
        }
    }

    printf("== relatorio ==\n");
    vc_relatar(&c, linha, sizeof(linha));
    printf("   %s\n", linha);
    OK(strstr(linha, "livre=") != NULL);
    OK(strstr(linha, "despejos=") != NULL);

    printf("== fragmentacao: 200 ciclos ==\n");
    {
        int i;
        vc_init(&c, 0, 1900544);
        for (i = 0; i < 200; i++) {
            vc_pedir(&c, 1000 + (unsigned)i, 40000, &a1);
            vc_pedir(&c, 2000 + (unsigned)i, 120000, &a2);
            vc_limpar(&c);
        }
        OK(vc_livre(&c) == 1900544);
        OK(vc_maior_buraco(&c) == 1900544);
        OK(c.falhas == 0);
    }

    printf("\n%d verificacoes, %d falhas\n", checks, falhas);
    return falhas ? 1 : 0;
}
