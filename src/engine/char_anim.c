#include "char_anim.h"

#include "char_layout.h"

#include <limits.h>

void ca_reset(ch_state_t *s, int ci)
{
    if (!s) return;
    s->ci = ci;
    s->anim = -1;
    s->missing = 0;
    s->t0 = CH_ANIM_MS_PADRAO;
    s->lado = 0;
    s->dance_every = (ci >= 0 && ci < CH_CHAR_COUNT) ? ch_chars[ci].dance : 1.0f;
    if (s->dance_every < 0.0f) s->dance_every = 0.0f;
    s->dancas = 0;
    s->ultimo_passo = INT_MIN;
}

int ca_play(ch_state_t *s, int anim, int missing, long now)
{
    if (!s) return 0;
    if (anim == s->anim && missing == s->missing) return 0;
    s->anim = anim;
    s->missing = missing;
    s->t0 = now;
    return 1;
}

int ca_frame(const ch_seq_t *q, long decorrido)
{
    long f;
    if (!q || q->count <= 0) return -1;
    if (decorrido < 0) decorrido = 0;
    /* fps100 = 0 significa taxa desconhecida: mostra o primeiro quadro em vez
       de dividir por zero. Um personagem parado e' errado, mas e' um erro
       visivel; uma conta de divisao por zero nao e'. */
    if (q->fps100 <= 0) return q->first;
    f = (decorrido * (long)q->fps100) / 100000L;
    if (f >= (long)q->count) {
        if (q->looped) f %= (long)q->count;
        else f = (long)q->count - 1;
    }
    if (f < 0) f = 0;
    return q->first + (int)f;
}

int ca_canto(ch_state_t *s, int dir, int missing, long now)
{
    int a;
    if (!s) return 0;
    a = ch_anim_sing(s->ci, dir, missing);
    if (a < 0) {
        /* O personagem nao canta (as GFs, as de fundo): nao ha' o que tocar, e
           fingir que ha' seria um quadro errado. */
        return 0;
    }
    return ca_play(s, a, missing, now);
}

/* A animacao atual e' de canto? (as quatro direcoes, com ou sem o sufixo de
   erro -- que e' o que o singTime governa). Um idle nunca e' cortado por
   tempo. */
static int e_canto(int ci, int anim)
{
    const ch_dados_t *c;
    const char *n;
    if (ci < 0 || ci >= CH_CHAR_COUNT) return 0;
    c = &ch_chars[ci];
    if (anim < 0 || anim >= c->anim_count) return 0;
    n = ch_anims[c->anim_first + anim].name;
    return n && n[0] == 's' && n[1] == 'i' && n[2] == 'n' && n[3] == 'g';
}

int ca_passo_de_danca(float dance_every, int passo)
{
    float janela, resto;
    int voltas;
    if (dance_every <= 0.0f) return 0;        /* danca automatica desligada */
    /* Passo negativo (a contagem regressiva) tambem conta: no original o
       `event.step % (danceEvery*4) == 0` e' verdadeiro para -4, -8..., e o
       personagem ja' danca antes do primeiro verso. */
    /* Bopper.hx L166 compara em FLOAT: `event.step % (danceEvery * 4)`. O resto
       e' comparado com zero; a folga de 1e-4 cobre o arredondamento de um
       danceEvery fracionario sem deixar passar um passo inteiro (o menor passo
       util e' 1/4 de batida, muito maior que 1e-4). Sem math.h de proposito:
       fmodf/fabsf puxariam -lm em todo build de teste. */
    janela = dance_every * (float)CA_STEPS_PER_BEAT;
    if (janela <= 0.0f) return 0;
    voltas = (int)((float)passo / janela);
    resto = (float)passo - (float)voltas * janela;
    if (resto < 0.0f) resto = -resto;
    if (janela - resto < resto) resto = janela - resto;
    return resto < 1e-4f;
}

int ca_danca(ch_state_t *s, long now)
{
    int a;
    if (!s) return 0;
    /* Idle fixo: quem tem `idle` nao alterna lado nenhum. */
    if (ch_anim_i(s->ci, "idle") >= 0) {
        a = ch_anim_i(s->ci, "idle");
        return ca_play(s, a, 0, now);
    }
    /* BaseCharacter/Bopper.hx L185-200: `hasDanced` comeca em false, entao a
       PRIMEIRA danca e' `danceLeft`; cada danca JOGADA inverte o lado. */
    s->lado = s->dancas & 1;
    s->dancas++;
    a = ch_anim_idle(s->ci, s->lado);
    if (a < 0) return 0;
    return ca_play(s, a, 0, now);
}

int ca_ocioso(ch_state_t *s, int passo, long now, long sing_ms)
{
    if (!s) return 0;
    /* Enquanto o canto esta' no ar, nada muda -- e a danca daquele passo e'
       PERDIDA, nao adiada (BaseCharacter.dance() L479-490 sai fora antes de
       tocar em `hasDanced`). */
    if (e_canto(s->ci, s->anim) && sing_ms > 0 && now - s->t0 < sing_ms) return 0;
    if (!ca_passo_de_danca(s->dance_every, passo)) return 0;
    if (passo == s->ultimo_passo) return 0;    /* ja' dancou neste passo */
    s->ultimo_passo = passo;
    return ca_danca(s, now);
}

long ca_passos_ms(int passos, float bpm)
{
    /* UM PASSO E' UM QUARTO DE BATIDA, nao uma batida.
       Conductor.hx L187-192: `stepLengthMs = beatLengthMs / STEPS_PER_BEAT`, e
       Constants.hx L424: `STEPS_PER_BEAT = 4`. Entao um passo = 60000/bpm/4 ms.
       Antes esta conta nao dividia por 4 e todo canto durava QUATRO VEZES o que
       devia -- invisivel no tutorial, onde o tempo de canto era um numero fixo
       escrito a mao, e errado em todas as 27 musicas.
       Com bpm invalido, devolve 0 para o chamador nao esperar para sempre por um
       canto que nao acaba. */
    if (bpm <= 0.0f || passos <= 0) return 0;
    return (long)((float)passos * 60000.0f / bpm / (float)CA_STEPS_PER_BEAT);
}

long ca_canto_ms(float passos, float bpm, int missing)
{
    long ms;
    if (bpm <= 0.0f || passos <= 0.0f) return 0;
    /* `passos` e' float porque singTime tambem e' (CharacterData.hx L451:
       DEFAULT_SINGTIME = 8.0, e a JSON de alguns personagens traz 6.5, 9.0). */
    ms = (long)(passos * 60000.0f / bpm / (float)CA_STEPS_PER_BEAT);
    /* BaseCharacter.hx L434-436: `if (getCurrentAnimation().endsWith('miss'))
       singTimeSec *= 2; // makes it feel more awkward when you miss???`.
       O erro de nota portanto segura a animacao pelo DOBRO do tempo. */
    if (missing) ms *= 2;
    return ms;
}
