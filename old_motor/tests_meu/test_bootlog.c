/* Teste do instrumento de boot. O que ele precisa garantir e uma coisa
   so: depois de QUALQUER travamento, o arquivo diz onde o jogo estava.
   Sem isso, o passo 66 continua sendo um ponto cego. */
#include "../src/engine/bootlog.h"
#include <stdio.h>
#include <string.h>

static int falhas = 0, checks = 0;
static void ok(int cond, const char *txt)
{
    checks++;
    if (!cond) { falhas++; printf("FALHOU: %s\n", txt); }
}
#define OK(c) ok((c), #c)

static char gravado[8192];
static int n_linhas;
static void escritor(void *ctx, const char *linha)
{
    size_t t;
    (void)ctx;
    n_linhas++;
    t = strlen(gravado);
    if (t + strlen(linha) + 2 < sizeof(gravado)) {
        memcpy(gravado + t, linha, strlen(linha));
        gravado[t + strlen(linha)] = '\n';
        gravado[t + strlen(linha) + 1] = 0;
    }
}

int main(void)
{
    bootlog_t b;

    printf("== marcos escritos na hora ==\n");
    memset(gravado, 0, sizeof(gravado));
    n_linhas = 0;
    bootlog_init(&b, escritor, NULL, 1);
    OK(n_linhas >= 1);                     /* BOOT sai na inicializacao */
    bootlog_marco(&b, "IOP_RESET", 0);
    bootlog_marco(&b, "AUDSRV_INIT", 0);
    bootlog_marco(&b, "USB_INIT", 0);
    OK(n_linhas == 4);
    OK(b.passo == 3);
    OK(!strncmp(gravado, "PASSO=0 MARCO=BOOT", 18));
    OK(strstr(gravado, "PASSO=1 MARCO=IOP_RESET") != NULL);
    OK(strstr(gravado, "PASSO=3 MARCO=USB_INIT") != NULL);

    printf("== anel sobrevive ao limite de escritas ==\n");
    {
        int i;
        for (i = 0; i < 400; i++) bootlog_marco(&b, "REPETIDO", i);
        OK(b.n_marcos == BOOTLOG_MARCOS);
        OK(bootlog_ultimo(&b)->valor == 399);
        OK(!strcmp(bootlog_ultimo(&b)->nome, "REPETIDO"));
    }

    printf("== fase dentro do quadro ==\n");
    memset(gravado, 0, sizeof(gravado));
    n_linhas = 0;
    bootlog_init(&b, escritor, NULL, 1);
    bootlog_quadro_inicio(&b, 66);
    bootlog_fase(&b, BF_PAD);
    bootlog_fase(&b, BF_AUDIO_CAUDAL);
    bootlog_fase(&b, BF_DESENHO);
    bootlog_fase(&b, BF_VSYNC);
    /* 4 trocas geram 4 linhas de FASE (a fase inicial nao conta) */
    OK(n_linhas == 1 + 4);
    OK(strstr(gravado, "QUADRO=66 FASE=VSYNC") != NULL);
    OK(b.quadro == 66);
    OK(!strcmp(bootlog_fase_nome(BF_AUDIO_ENVIO), "AUDIO_ENVIO"));
    OK(!strcmp(bootlog_fase_nome(999), "?"));

    printf("== cao de guarda ==\n");
    memset(gravado, 0, sizeof(gravado));
    n_linhas = 0;
    bootlog_init(&b, escritor, NULL, 1);
    b.limite_ms = 100;
    bootlog_quadro_inicio(&b, 10);
    bootlog_quadro_fim(&b, 33);            /* normal: nao gera alerta */
    OK(b.alertas == 0);
    bootlog_quadro_inicio(&b, 11);
    bootlog_quadro_fim(&b, 900);           /* travado: gera alerta */
    OK(b.alertas == 1);
    OK(strstr(gravado, "ALERTA=QUADRO_LENTO") != NULL);
    printf("   %s", gravado);

    printf("== deteccao de morte anterior ==\n");
    {
        const char *log_ruim = "PASSO=1 MARCO=BOOT\nPASSO=66 MARCO=FRAME\nPASSO=66 MARCO=FASE VALOR=9 QUADRO=12\n";
        const char *log_bom  = "PASSO=1 MARCO=BOOT\nPASSO=66 MARCO=FRAME\nPASSO=90 MARCO=SHUTDOWN\n";
        bootlog_init(&b, escritor, NULL, 1);
        bootlog_analisa_anterior(&b, log_ruim);
        OK(b.morto_sem_fim == 1);    /* sem SHUTDOWN: morreu */
        bootlog_analisa_anterior(&b, log_bom);
        OK(b.morto_sem_fim == 0);    /* saiu limpo */
    }

    printf("== alerta sai mesmo sem verbose ==\n");
    memset(gravado, 0, sizeof(gravado));
    n_linhas = 0;
    bootlog_init(&b, escritor, NULL, 0);
    OK(n_linhas == 1);                       /* BOOT sai sempre: prova de vida */
    bootlog_marco(&b, "SILENCIOSO", 0);
    OK(n_linhas == 1);                       /* marco comum respeita verbose */
    bootlog_alerta(&b, "AUDIO_SECO", 3);
    OK(n_linhas == 2);                       /* alerta nunca e silenciado */
    OK(strstr(gravado, "ALERTA=AUDIO_SECO VALOR=3") != NULL);

    printf("\n%d verificacoes, %d falhas\n", checks, falhas);
    return falhas ? 1 : 0;
}
