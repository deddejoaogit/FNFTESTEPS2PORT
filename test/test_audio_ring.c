/* O ANEL DE AUDIO NO MODO PREVIEW DA FREEPLAY: a janela, os 4 canais viram 2,
 * o ganho do fade e a VOLTA para o comeco da janela.
 *
 * POR QUE UM TESTE SO' PARA ISTO
 *
 * `audio_ring_iniciar_preview` e' o unico caminho do jogo em que o produtor do
 * anel:
 *   1. le' um TRECHO do arquivo e volta para o comeco do TRECHO (e nao para o
 *      byte 0) -- e' o `loadPartial` + o `FlxTimer` de `length/1000` que chama
 *      `playCurSongPreview()` de novo (`FreeplayState.hx` L3022-3038);
 *   2. converte os 4 canais do `stems.pcm` em 2, JOGANDO FORA as vozes (o
 *      preview do original e' `pathsFunction: INST`);
 *   3. multiplica o bloco pelo ganho do `fadeIn(2)`/`fadeOut(2)` ANTES de somar
 *      os overlays.
 *
 * O harness do PC nao prova nada disso: com uma musica de verdade a janela tem
 * mais de 5 MB e o roteiro nunca chega no fim dela. Aqui o arquivo e' sintetico
 * e a janela tem 4 s (2 s de fade em cada ponta), entao a volta acontece no
 * meio da leitura.
 *
 * O que NAO prova: que sai som na TV. Isso e' o console.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <stdint.h>
#include <semaphore.h>

#include <kernel.h>          /* os shims de test/ps2sdk-shim: ver o cabecalho */
#include <delaythread.h>

#include "audio_ring.h"
#include "engine/freeplay_audio.h"

/* ---- o kernel minimo (test/ps2sdk-shim/kernel.h) ------------------------- */
/* O `audio_ring.c` passa `&_gp` para o CreateThread (o registrador de gp do
 * MIPS). No PC ele so' precisa existir: e' um simbolo, nao um valor usado. */
void *_gp;
#define MAX_THREADS 8
typedef struct {
    sem_t sem;
    int vivo;
} sema_t;
static sema_t semas[MAX_THREADS];
typedef struct {
    void (*func)(void *);
    int vivo;
    pthread_t th;
    void *arg;
} thread_t;
static thread_t threads[MAX_THREADS];

/* 0.2.170: o portao de I/O identifica o dono por GetThreadId (recursao por
   thread). O shim nao o tinha -- o teste de audio nunca chegava a compilar
   porque a suite morria antes no numpy. Identidade estavel por thread. */
int GetThreadId(void)
{
    return (int)(intptr_t)pthread_self();
}

int CreateSema(ee_sema_t *s)
{
    int i;
    for (i = 0; i < MAX_THREADS; i++) {
        if (!semas[i].vivo) {
            sem_init(&semas[i].sem, 0, s && s->init_count > 0 ? s->init_count : 1);
            semas[i].vivo = 1;
            return i;
        }
    }
    return -1;
}
int DeleteSema(int id)
{
    if (id < 0 || id >= MAX_THREADS || !semas[id].vivo) return -1;
    sem_destroy(&semas[id].sem);
    semas[id].vivo = 0;
    return 0;
}
int WaitSema(int id)
{
    if (id < 0 || id >= MAX_THREADS || !semas[id].vivo) return -1;
    return sem_wait(&semas[id].sem);
}
int SignalSema(int id)
{
    if (id < 0 || id >= MAX_THREADS || !semas[id].vivo) return -1;
    return sem_post(&semas[id].sem);
}
static void *trampolim(void *p)
{
    int i = (int)(long)p;
    thread_t *t = &threads[i];
    t->func(t->arg);
    return NULL;
}
int CreateThread(ee_thread_t *t)
{
    int i;
    if (!t || !t->func) return -1;
    for (i = 0; i < MAX_THREADS; i++) {
        if (!threads[i].vivo) {
            threads[i].vivo = 1;
            threads[i].func = t->func;
            threads[i].arg = NULL;
            return i;
        }
    }
    return -1;
}
int DeleteThread(int id)
{
    if (id < 0 || id >= MAX_THREADS || !threads[id].vivo) return -1;
    threads[id].vivo = 0;
    return 0;
}
int StartThread(int id, void *arg)
{
    if (id < 0 || id >= MAX_THREADS || !threads[id].vivo) return -1;
    threads[id].arg = arg;
    return pthread_create(&threads[id].th, NULL, trampolim, (void *)(long)id) == 0 ? 0 : -1;
}
/* `TerminateThread` nao existe de verdade no PC; o anel so' cai aqui em ultimo
 * recurso (a thread nao saiu em 600 ms), e no teste ela sempre sai. */
int TerminateThread(int id) { return DeleteThread(id); }
void ExitThread(void) { pthread_exit(NULL); }
void DelayThread(unsigned int usec)
{
    struct timespec ts;
    ts.tv_sec = (time_t)(usec / 1000000u);
    ts.tv_nsec = (long)(usec % 1000000u) * 1000L;
    nanosleep(&ts, NULL);
}

static int falhas;

/* ---- o arquivo sintetico ------------------------------------------------- */
#define ARQ_QUADROS 200000L          /* amostras de 4 canais no arquivo      */
#define JANELA_QUADROS 192000L       /* a janela do preview: 4 s a 48 kHz    */
#define FADE_QUADROS 96000           /* 2 s = FP_AUD_FADE_MS                 */
#define VOZ 0x6000                   /* o valor que PROVA o vazamento        */

#define CHECK(c) do { if (!(c)) { printf("  FALHA: %s (linha %d)\n", #c, __LINE__); falhas++; } } while (0)

static void escreve_arquivo(const char *caminho)
{
    FILE *f = fopen(caminho, "wb");
    long i;
    if (!f) { printf("  FALHA: nao criei %s\n", caminho); falhas++; return; }
    for (i = 0; i < ARQ_QUADROS; i++) {
        short quadros[4];
        /* O instrumental e' um dente de serra de -1000 a 1000: qualquer valor
           acima disso na saida so' pode ter vindo das vozes. */
        quadros[0] = (short)((i % 2001L) - 1000L);
        quadros[1] = (short)-quadros[0];
        quadros[2] = (short)VOZ;
        quadros[3] = (short)-VOZ;
        fwrite(quadros, sizeof(quadros), 1, f);
    }
    fclose(f);
}

int main(void)
{
    const char *caminho = "/tmp/fnf_ring_test.pcm";
    long i, total_quadros = JANELA_QUADROS * 2;
    unsigned char *buf;
    short *pcm;
    int fd, maximo = 0, zeros_inicio = 0;

    escreve_arquivo(caminho);
    if (falhas) return 1;

    fd = open(caminho, O_RDONLY);
    if (fd < 0) { printf("  FALHA: nao abri %s\n", caminho); return 1; }

    printf("== anel: a janela do preview (%ld quadros de 4 canais, 4 s)\n", JANELA_QUADROS);
    if (fp_aud_janela_amostras(0, JANELA_QUADROS * 8) != (int)JANELA_QUADROS) {
        printf("  FALHA: a janela de %ld B nao deu %ld amostras\n",
               JANELA_QUADROS * 8, JANELA_QUADROS);
        return 1;
    }
    if (audio_ring_iniciar_preview(fd, 0, JANELA_QUADROS * 8) != 0) {
        printf("  FALHA: o anel nao iniciou no modo preview\n");
        close(fd);
        return 1;
    }

    /* Duas voltas completas da janela. */
    buf = (unsigned char *)malloc((size_t)total_quadros * 4u);
    if (!buf) { printf("  FALHA: sem memoria\n"); audio_ring_stop(); close(fd); return 1; }
    for (i = 0; i < total_quadros * 4L; ) {
        int n = (int)((total_quadros * 4L - i) > 4096L ? 4096L : (total_quadros * 4L - i));
        int got = audio_ring_take(buf + i, n);
        if (got <= 0) { DelayThread(1000); continue; }
        i += got;
    }
    audio_ring_stop();
    close(fd);
    pcm = (short *)buf;

    printf("== anel: o instrumental passa e as vozes NAO\n");
    /* 1. Nenhuma amostra pode passar de 1000: as vozes valem 0x6000 e so'
       apareceriam se os 4 canais fossem para a saida. */
    for (i = 0; i < total_quadros * 2L; i++) {
        int v = pcm[i] < 0 ? -pcm[i] : pcm[i];
        if (v > maximo) maximo = v;
    }
    CHECK(maximo <= 1001);
    CHECK(maximo >= 690);            /* e o instrumental chega ao volume 0,7 */
    printf("  maior amostra na saida ..... %d   (0,7 x 1000 = 700; voz = %d)\n", maximo, VOZ);

    printf("== anel: o fadeIn(2) / fadeOut(2) a 0,7\n");
    /* 2. No comeco da janela o ganho e' 0. */
    for (i = 0; i < 8; i++) if (pcm[i] == 0) zeros_inicio++;
    CHECK(zeros_inicio == 8);
    /* 3. No MEIO da janela (2 s) os dois fades se encontram e o ganho e' 0,7. */
    {
        long k = JANELA_QUADROS / 2;
        int esperado = (int)((double)((k % 2001L) - 1000L) * 0.7 +
                             (((k % 2001L) - 1000L) < 0 ? -0.5 : 0.5));
        int v = pcm[k * 2];
        printf("  no meio (2 s) .............. %d   (esperado ~%d)\n", v, esperado);
        CHECK(v >= esperado - 20 && v <= esperado + 20);
    }
    /* 4. L e R sao opostos (o canal 1 do stems e' -canal 0): nenhum par
       trocado, nenhuma soma dos dois. */
    {
        int opostos = 0;
        for (i = 0; i < total_quadros; i++)
            if (pcm[i * 2] == -pcm[i * 2 + 1] || (pcm[i * 2] == 0 && pcm[i * 2 + 1] == 0)) opostos++;
        CHECK(opostos == (int)total_quadros);
    }

    printf("== anel: a volta para o comeco da JANELA (nao para o byte 0)\n");
    /* 5. No fim da janela o ganho cai a zero e a segunda volta RECOMECA em
       zero -- se o produtor voltasse para o byte 0 do ARQUIVO o conteudo seria
       o mesmo (a janela comeca em 0), entao o que este teste prova e' o
       reinicio do GANHO: com uma janela deslocada o conteudo tambem muda. */
    CHECK(pcm[(JANELA_QUADROS - 1) * 2] == 0 || pcm[(JANELA_QUADROS - 1) * 2] <= 1);
    {
        int z = 0;
        for (i = 0; i < 8; i++) if (pcm[(JANELA_QUADROS + i) * 2] == 0) z++;
        printf("  amostras em zero no recomeco  %d de 8\n", z);
        CHECK(z == 8);
    }
    /* 6. E o conteudo se repete: a amostra k da segunda volta e' a mesma da
       primeira (mesma posicao na janela, mesmo ganho). */
    {
        long ks[3] = { 500L, 5000L, 95000L };
        int ok = 0;
        for (i = 0; i < 3; i++) {
            long k = ks[i];
            if (pcm[(k + JANELA_QUADROS) * 2] == pcm[k * 2] &&
                pcm[(k + JANELA_QUADROS) * 2 + 1] == pcm[k * 2 + 1]) ok++;
        }
        CHECK(ok == 3);
    }

    /* ---- a fonte SECUNDARIA com volume ------------------------------- */
    /* O `scrollMenu` da Freeplay toca a 0,4 (`FreeplayState.hx` L2918) por cima
       do preview, e o preview tem ganho 0 no primeiro quadro: se o volume
       fosse aplicado depois da soma, o sfx sumiria com o fade. Aqui o overlay
       e' um DC de 2000 nos dois canais: 0,4 x 2000 = 800. */
    printf("== anel: a fonte secundaria soma por cima, com o volume dela\n");
    {
        const char *caminho_ov = "/tmp/fnf_ring_overlay.pcm";
        FILE *f = fopen(caminho_ov, "wb");
        long k;
        short *pcm2;
        unsigned char *b2;
        int fd_ov, slot;
        if (!f) { printf("  FALHA: nao criei %s\n", caminho_ov); return 1; }
        for (k = 0; k < 4800; k++) {
            short q[2];
            q[0] = 2000;
            q[1] = 2000;
            fwrite(q, sizeof(q), 1, f);
        }
        fclose(f);

        fd_ov = open(caminho_ov, O_RDONLY);
        if (fd_ov < 0) { printf("  FALHA: nao abri %s\n", caminho_ov); return 1; }
        fd = open(caminho, O_RDONLY);
        if (fd < 0) { printf("  FALHA: nao reabri %s\n", caminho); return 1; }
        if (audio_ring_iniciar_preview(fd, 0, JANELA_QUADROS * 8) != 0) {
            printf("  FALHA: o anel nao reiniciou\n");
            return 1;
        }
        slot = audio_ring_overlay_slot(1, fd_ov, 40);
        if (slot != 0) { printf("  FALHA: o slot 1 do overlay nao aceitou (rc %d)\n", slot); return 1; }

        b2 = (unsigned char *)malloc((size_t)total_quadros * 4u);
        if (!b2) { printf("  FALHA: sem memoria\n"); return 1; }
        for (i = 0; i < total_quadros * 4L; ) {
            int n = (int)((total_quadros * 4L - i) > 4096L ? 4096L : (total_quadros * 4L - i));
            int got = audio_ring_take(b2 + i, n);
            if (got <= 0) { DelayThread(1000); continue; }
            i += got;
        }
        audio_ring_stop();
        close(fd);
        pcm2 = (short *)b2;

        /* 7-9. ONDE O SFX ENTRA.
         *
         * `audio_ring_overlay_slot` PUBLICA um pedido: a thread do anel aplica
         * no topo do bloco seguinte (READ_SIZE quadros). O teste chamava o
         * pedido e assumia que ele valia ja' no PRIMEIRO quadro -- verdade
         * quando a thread ainda nao tinha produzido nada, FALSO quando a
         * maquina esta' carregada (a suite inteira rodando): a thread ja'
         * tinha entregado o primeiro bloco e os primeiros quadros saiam sem o
         * sfx. Medido: 1 falha em 1 execucao da suite, 0 em 20 execucoes do
         * teste sozinho. Nao era o anel: era o teste cobrando uma
         * instantaneidade que a API nao promete (e nao deve prometer, porque no
         * console esperar pelo consumo travava o jogo).
         *
         * O que se cobra agora e' o que a API promete: o sfx entra no bloco
         * seguinte, toca os 4800 quadros dele, e depois o resto da saida e'
         * IDENTICA a' da rodada sem overlay. A defasagem maxima e' um bloco. */
        {
            long ini_sfx = -1, fim_sfx = -1, k;
            for (i = 0; i < total_quadros; i++) {
                if (pcm2[i * 2] + pcm2[i * 2 + 1] == 1600) { ini_sfx = (long)i; break; }
            }
            printf("  o sfx entra no quadro .......... %ld (no maximo 1 bloco de %d)\n",
                   ini_sfx, 4096);
            CHECK(ini_sfx >= 0);
            CHECK(ini_sfx <= 4096);
            if (ini_sfx < 0) ini_sfx = 0;
            /* enquanto o sfx toca: L + R = 2 x 800 (o instrumental e' espelhado
               e some na soma) */
            fim_sfx = ini_sfx + 4800;
            if (fim_sfx > total_quadros) fim_sfx = total_quadros;
            {
                int ok = 0, ruins = 0;
                for (k = ini_sfx; k < fim_sfx; k++) {
                    if (pcm2[k * 2] + pcm2[k * 2 + 1] == 1600) ok++;
                    else if (ruins < 3) { printf("  k=%ld: %d + %d\n", k, pcm2[k*2], pcm2[k*2+1]); ruins++; }
                }
                printf("  L + R = 1600 nos quadros com sfx  %d de %ld\n", ok, fim_sfx - ini_sfx);
                CHECK(ok == fim_sfx - ini_sfx);
            }
            /* depois do fim do sfx a saida e' igual a' da rodada sem overlay --
               o fim do sfx nao corta nem atrasa a primaria */
            {
                long ks[4] = { 4800L, 9000L, 60000L, 150000L };
                int iguais = 0, comparados = 0;
                for (i = 0; i < 4; i++) {
                    k = ks[i];
                    if (k < fim_sfx || k >= total_quadros) continue;
                    comparados++;
                    if (pcm2[k * 2] == pcm[k * 2] && pcm2[k * 2 + 1] == pcm[k * 2 + 1]) iguais++;
                    else printf("  k=%ld: sem sfx %d/%d, com sfx %d/%d\n", k,
                                pcm[k*2], pcm[k*2+1], pcm2[k*2], pcm2[k*2+1]);
                }
                printf("  sem sfx, a saida e' igual a' da 1a rodada  %d de %d\n", iguais, comparados);
                CHECK(comparados >= 2);
                CHECK(iguais == comparados);
            }
        }
        /* 10. E o pedido com slot explicito devolve -1 sem anel: quem chama
           nao perde o fd. */
        CHECK(audio_ring_overlay_slot(1, fd_ov, 40) == -1);
        free(b2);
    }

    /* 11. OVERLAY EM LACO: o `play(true)` do flixel. O caso e' o `train_passes`
       do `phillyTrain` (14,42 s por baixo da musica inteira). O arquivo tem
       100 ms -- N nao e' multiplo do bloco de leitura (READ_SIZE), entao a
       emenda cai no MEIO de um bloco, que e' justamente o caso que parava o
       som antes: o bloco parcial marcava fim de arquivo. */
    printf("== anel: o overlay em LACO emenda o fim no comeco ==\n");
    {
        const char *arq = "/tmp/fnf_ring_loop.pcm";
        const int N = 4801;            /* 100 ms + 1 quadro: de proposito impar */
        const int total = N * 4;
        short quadros[2];
        FILE *f = fopen(arq, "wb");
        int i2, fd_l, slot, ini = -1, ruins = 0;
        short *out;
        unsigned int p, r, l, e, u;
        if (!f) { printf("  FALHA: nao criei %s\n", arq); falhas++; }
        else {
        for (i2 = 0; i2 < N; i2++) {
            /* +3000 na primeira metade, -3000 na segunda: qualquer deslocamento
               na emenda aparece como amostra fora do padrao. */
            quadros[0] = (short)(i2 < N / 2 ? 3000 : -3000);
            quadros[1] = quadros[0];
            fwrite(quadros, sizeof(quadros), 1, f);
        }
        fclose(f);
        fd_l = open(arq, O_RDONLY);
        CHECK(fd_l >= 0);
        CHECK(audio_ring_start_overlay(-1) == 0);   /* silencio + overlays */
        slot = audio_ring_overlay_loop(fd_l);
        CHECK(slot >= 0);
        out = (short *)malloc((size_t)total * 4u);
        if (!out) { printf("  FALHA: sem memoria\n"); falhas++; }
        else {
            for (i2 = 0; i2 < total * 2; ) {
                int quer = (total * 2 - i2) * 2;      /* shorts -> bytes */
                int got;
                if (quer > 4096) quer = 4096;
                got = audio_ring_take((unsigned char *)out + (size_t)i2 * 2u, quer);
                if (got <= 0) { DelayThread(1000); continue; }
                i2 += got / 2;
            }
            audio_ring_stats(&p, &r, &l, &e, &u);
            printf("  voltas do laco .......... %u (esperado >= 3 no arquivo de %d quadros)\n",
                   l, N);
            CHECK(l >= 3);
            /* Onda quadrada: acha a primeira amostra nao nula (o comeco do
               overlay so' entra no bloco seguinte ao pedido) e confere o
               padrao nas voltas seguintes. */
            for (i2 = 0; i2 < total; i2++) if (out[i2 * 2] != 0) { ini = i2; break; }
            CHECK(ini >= 0);
            if (ini >= 0) {
                /* reinicia a contagem no comeco de uma volta do arquivo */
                int base = ini;
                while (base > 0 && out[(base - 1) * 2] == out[base * 2]) base--;
                for (i2 = base; i2 < total; i2++) {
                    int esperado = (i2 % N) < N / 2 ? 3000 : -3000;
                    if (out[i2 * 2] != esperado || out[i2 * 2 + 1] != esperado) {
                        if (ruins < 3)
                            printf("  i=%d (volta %d): %d/%d, esperado %d\n", i2, i2 % N,
                                   out[i2 * 2], out[i2 * 2 + 1], esperado);
                        ruins++;
                    }
                }
                printf("  amostras fora do padrao .. %d\n", ruins);
                CHECK(ruins == 0);
            }
            audio_ring_overlay_parar(slot);
            audio_ring_stop();
            /* o fd era da thread: ela o fechou no stop, o teste nao fecha */
            free(out);
        }
        }
    }

    free(buf);
    if (falhas) { printf("  %d FALHA(S)\n", falhas); return 1; }
    printf("  ok: janela, 4 canais -> 2, ganho do fade e a volta conferem\n");
    printf("ANEL DE AUDIO (PREVIEW) OK\n");
    return 0;
}
