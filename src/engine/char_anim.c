#include "char_anim.h"

#include "char_layout.h"

void ca_reset(ch_state_t *s, int ci)
{
    if (!s) return;
    s->ci = ci;
    s->anim = -1;
    s->missing = 0;
    s->t0 = CH_ANIM_MS_PADRAO;
    s->lado = 0;
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

int ca_ocioso(ch_state_t *s, int batida, long now, long sing_ms)
{
    int a;
    if (!s) return 0;
    /* Enquanto o canto esta' no ar, nada muda. */
    if (e_canto(s->ci, s->anim) && sing_ms > 0 && now - s->t0 < sing_ms) return 0;
    /* Quem alterna danceLeft/danceRight usa a batida: o Bopper.dance() troca de
       lado a cada danceEvery passos, e o estado guarda o lado para nao piscar
       quando a batida nao muda. */
    if (ch_anim_i(s->ci, "idle") >= 0) {
        a = ch_anim_i(s->ci, "idle");
        return ca_play(s, a, 0, now);
    }
    s->lado = batida & 1;
    a = ch_anim_idle(s->ci, s->lado);
    if (a < 0) return 0;
    return ca_play(s, a, 0, now);
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
