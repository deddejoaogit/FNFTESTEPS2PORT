/* Teste do icone de vida do HUD (engine/health_icon.c).
 *
 * O que este teste fixa: as constantes do original (HealthIcon.hx L150, L157,
 * L163, L168, L124, L129), a conta da posicao (L284), o pulo por passo (L337) e
 * o leitor do pacote ICP1 (tools/empacotar_icones.py). Sem ele, "o icone esta'
 * no lugar" seria impressao de quem olhou a tela.
 */
#include <stdio.h>
#include <string.h>

#include "health_icon.h"

static int falhas;

#define OK(c) do { if (!(c)) { printf("  FALHA: %s (linha %d)\n", #c, __LINE__); falhas++; } } while (0)
#define PERTO(a, b, eps) do { float _a = (a), _b = (b); \
    if (!(_a > _b - (eps) && _a < _b + (eps))) { \
        printf("  FALHA: %s ~ %s: %.4f != %.4f (linha %d)\n", #a, #b, _a, _b, __LINE__); falhas++; } } while (0)

/* Monta um pacote ICP1 valido em memoria (96x96, 2 quadros, passo 128). */
static unsigned int monta_pacote(unsigned char *buf, unsigned int quadros,
                                 unsigned int w, unsigned int h, unsigned int passo)
{
    unsigned int i, soma = 0;
    unsigned int bytes_quadro = passo * h * 2u;
    unsigned int dados = bytes_quadro * quadros;
    memset(buf, 0, HI_HEADER_BYTES + dados);
    buf[0] = 'I'; buf[1] = 'C'; buf[2] = 'P'; buf[3] = '1';
    buf[4] = 1; buf[5] = 0;
    buf[6] = (unsigned char)(w & 0xFF); buf[7] = (unsigned char)(w >> 8);
    buf[8] = (unsigned char)(h & 0xFF); buf[9] = (unsigned char)(h >> 8);
    buf[10] = (unsigned char)quadros; buf[11] = 0;
    buf[12] = 16; buf[13] = 0;
    buf[14] = (unsigned char)(passo & 0xFF); buf[15] = (unsigned char)(passo >> 8);
    /* bytes_quadro em 20, dados em 24, soma em 28 */
    buf[20] = (unsigned char)(bytes_quadro & 0xFF);
    buf[21] = (unsigned char)((bytes_quadro >> 8) & 0xFF);
    buf[22] = (unsigned char)((bytes_quadro >> 16) & 0xFF);
    buf[23] = (unsigned char)((bytes_quadro >> 24) & 0xFF);
    buf[24] = (unsigned char)(dados & 0xFF);
    buf[25] = (unsigned char)((dados >> 8) & 0xFF);
    buf[26] = (unsigned char)((dados >> 16) & 0xFF);
    buf[27] = (unsigned char)((dados >> 24) & 0xFF);
    for (i = 0; i < dados; i++) {
        buf[HI_HEADER_BYTES + i] = (unsigned char)(i & 0xFF);
        soma += buf[HI_HEADER_BYTES + i];
    }
    buf[28] = (unsigned char)(soma & 0xFF);
    buf[29] = (unsigned char)((soma >> 8) & 0xFF);
    buf[30] = (unsigned char)((soma >> 16) & 0xFF);
    buf[31] = (unsigned char)((soma >> 24) & 0xFF);
    return HI_HEADER_BYTES + dados;
}

int main(void)
{
    static unsigned char buf[HI_HEADER_BYTES + 128 * 96 * 2 * 2];
    hi_pack_t p;
    unsigned int n;

    printf("== icone de vida: pacote ICP1 ==\n");
    n = monta_pacote(buf, 2, 96, 96, 128);
    OK(hi_pack_ler(buf, n, &p) == 0);
    OK(p.w == 96 && p.h == 96);
    OK(p.quadros == 2);
    OK(p.passo == 128);
    OK(p.bytes_quadro == 128u * 96u * 2u);
    /* Um quadro na tela: passo * h * 2 = 24.576 B (o segundo quadro NAO ocupa
       VRAM; ele e' enviado quando o estado troca). */
    OK(hi_vram_bytes(&p) == 24576u);
    OK(hi_quadro(buf, &p, 0) == buf + HI_HEADER_BYTES);
    OK(hi_quadro(buf, &p, 1) == buf + HI_HEADER_BYTES + 128u * 96u * 2u);
    OK(hi_quadro(buf, &p, 2) == 0);

    printf("== icone de vida: pacote quebrado e' recusado ==\n");
    buf[0] = 'X';
    OK(hi_pack_ler(buf, n, &p) == -2);
    n = monta_pacote(buf, 2, 96, 96, 128);
    OK(hi_pack_ler(buf, 8, &p) == -1);
    buf[4] = 9;
    OK(hi_pack_ler(buf, n, &p) == -3);
    n = monta_pacote(buf, 2, 96, 96, 128);
    buf[10] = 4;                        /* quadros demais */
    OK(hi_pack_ler(buf, n, &p) == -5);
    n = monta_pacote(buf, 2, 96, 96, 128);
    buf[12] = 32;                       /* psm diferente de 16 */
    OK(hi_pack_ler(buf, n, &p) == -6);
    n = monta_pacote(buf, 2, 96, 96, 96); /* passo nao e' multiplo de 64 */
    OK(hi_pack_ler(buf, n, &p) == -7);
    n = monta_pacote(buf, 2, 96, 96, 128);
    buf[21] ^= 0x01;                    /* bytes_quadro nao bate */
    OK(hi_pack_ler(buf, n, &p) == -8);
    n = monta_pacote(buf, 2, 96, 96, 128);
    buf[HI_HEADER_BYTES + 5] ^= 0xFF;   /* soma errada */
    OK(hi_pack_ler(buf, n, &p) == -10);
    n = monta_pacote(buf, 2, 96, 96, 128);
    OK(hi_pack_ler(buf, n - 1, &p) == -9);

    printf("== icone de vida: estado pelo limiar do original ==\n");
    /* LOSE = 0.2*2 = 0.4, WIN = 0.8*2 = 1.6, sobre a vida de 0 a 2. */
    OK(hi_estado(1.0f, 2) == HI_IDLE);
    OK(hi_estado(0.39f, 2) == HI_PERDENDO);
    OK(hi_estado(0.41f, 2) == HI_IDLE);
    /* com 2 quadros nao ha' animacao de ganhando (loadAnimationOld exige 3) */
    OK(hi_estado(1.9f, 2) == HI_IDLE);
    OK(hi_estado(1.9f, 3) == HI_GANHANDO);
    /* o oponente usa a vida invertida (L305: MAXIMUM_HEALTH - health) */
    PERTO(hi_vida_oponente(2.0f), 0.0f, 1e-6f);
    OK(hi_estado(hi_vida_oponente(2.0f), 2) == HI_PERDENDO);
    OK(hi_estado(hi_vida_oponente(0.0f), 2) == HI_IDLE);

    printf("== icone de vida: tamanho na tela ==\n");
    /* HEALTH_ICON_SIZE 150 no espaco de 1280x720 = 75 no de 640x360. A
       tolerancia e' 1e-3 e nao 1e-6 porque o ulp de um float perto de 75 e'
       7,6e-6: com 1e-6 o proprio `75.0f - eps` arredonda para 75.0f e a
       comparacao vira "igual", que e' o que este teste NAO quer medir. */
    PERTO(hi_tela_px(), 75.0f, 1e-3f);

    printf("== icone de vida: pulo por passo (HealthIcon.hx L337) ==\n");
    {
        hi_bop_t b;
        hi_bop_reset(&b);
        PERTO(hi_bop_escala(&b), 1.0f, 1e-6f);
        /* Um passo de 100 BPM: stepLengthMs = 150 ms -> dur = min(0.3, 0.175). */
        hi_bop_passo(&b, 0.150f);
        PERTO(b.bop_dur, 0.175f, 1e-6f);
        PERTO(hi_bop_escala(&b), 1.2f, 1e-4f);
        hi_bop_tick(&b, 0.175f * 0.5f);
        PERTO(hi_bop_escala(&b), 1.1f, 2e-3f);
        hi_bop_tick(&b, 0.175f);
        PERTO(hi_bop_escala(&b), 1.0f, 1e-6f);
        /* Passo curto: dur = stepLength*2 e' menor que o teto. */
        hi_bop_passo(&b, 0.05f);
        PERTO(b.bop_dur, 0.1f, 1e-6f);
        /* dt = 0 nao anda (a pausa congela o pulo) */
        {
            float antes = hi_bop_escala(&b);
            hi_bop_tick(&b, 0.0f);
            PERTO(hi_bop_escala(&b), antes, 1e-6f);
        }
        hi_bop_reset(&b);
        PERTO(hi_bop_escala(&b), 1.0f, 1e-6f);
    }

    printf("== icone de vida: posicao (HealthIcon.hx L284) ==\n");
    {
        /* Barra do original em 640x360: x = 343.5/2 = 171.75, largura 593/2 =
           296.5, y = 652/2 = 326 (PlayState.hx L1995-2005). */
        const float bx = 171.75f, bw = 296.5f, by = 326.0f;
        float x = 0.0f, y = 0.0f;
        /* vida cheia: a divisa encosta na esquerda, os dois icones junto dela */
        hi_posicao(0, bx, bw, by, 2.0f, 75.0f, 75.0f, &x, &y);
        PERTO(x, bx - 26.0f, 1e-4f);
        PERTO(y, by - 37.5f, 1e-4f);
        hi_posicao(1, bx, bw, by, 2.0f, 75.0f, 75.0f, &x, &y);
        PERTO(x, bx - 49.0f, 1e-4f);
        /* meia vida: divisa no meio da barra */
        hi_posicao(0, bx, bw, by, 1.0f, 75.0f, 75.0f, &x, &y);
        PERTO(x, bx + bw * 0.5f - 26.0f, 1e-4f);
        /* vida zerada: divisa na direita */
        hi_posicao(0, bx, bw, by, 0.0f, 75.0f, 75.0f, &x, &y);
        PERTO(x, bx + bw - 26.0f, 1e-4f);
        /* vida fora da faixa nao joga o icone para fora da barra */
        hi_posicao(0, bx, bw, by, 5.0f, 75.0f, 75.0f, &x, &y);
        PERTO(x, bx - 26.0f, 1e-4f);
    }

    printf("\n%s (%d falha(s))\n", falhas ? "FALHOU" : "PASSOU", falhas);
    return falhas ? 1 : 0;
}
