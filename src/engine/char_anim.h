/* char_anim - qual QUADRO cada personagem esta' mostrando agora
 *
 * POR QUE ISTO EXISTE
 * O estado 8 do main.c hoje escolhe o quadro do BF e da GF por uma tabela fixa
 * (BF_SING/GF_SING, 14 e 30 quadros) escrita para o tutorial. Com 46
 * personagens, cada um com as suas animacoes, as suas taxas de quadro e o seu
 * singTime, isso nao se escreve a mao: vira uma maquina de estado.
 *
 * Aqui mora so' a conta -- nenhum GS, nenhum arquivo, nenhuma VRAM. Roda no PC.
 *
 * DE ONDE VEM CADA REGRA (nao inventada)
 * - O quadro de uma animacao: FlxAnimation tem frameRate e looped; o quadro em
 *   um instante e' floor(tempo*frameRate), com modulo quando e' loop e preso no
 *   ultimo quando nao e' (CharAnimationController/FlxAnimationController).
 * - Idle de quem tem danceLeft/danceRight: Bopper.dance() alterna os dois a
 *   cada `danceEvery` passos, e NAO reinicia a animacao se ja' esta' naquela
 *   (danced flag). Quem tem "idle" toca "idle".
 * - Canto dura `singTime` passos do chart; depois volta a dancar
 *   (BaseCharacter.dance() chamado pelo PlayState quando o tempo do canto
 *   acaba). O `singTime` esta' em char_dados.h, com 8,0 de padrao.
 * - Erro de nota usa "singLEFTmiss" quando o personagem tem; senao o normal
 *   (o original troca o sufixo e cai no mesmo nome-base se nao existir).
 *
 * O QUE NAO ESTA AQUI
 * - a taxa de quadro e a contagem de quadros de cada animacao. Elas NAO estao
 *   no character.json: o frameRate/looped/frameIndices vivem no atlas de cada
 *   personagem (AnimationData), que so' o clone da arte tem. Por isso ch_seq_t
 *   e' preenchido pelo PACOTE, e o pacote e' construido pelo empacotador, que
 *   le' o atlas. Sem o clone, este modulo continua correto e testado; o que
 *   falta e' o dado, e faltar dado nao pode virar numero inventado.
 */
#ifndef FNF_CHAR_ANIM_H
#define FNF_CHAR_ANIM_H

#ifdef __cplusplus
extern "C" {
#endif

/* Uma animacao como o pacote a entrega: onde os quadros comecam, quantos sao,
   a taxa em centesimos de quadro por segundo (2997 = 29,97) e se volta ao
   comeco. */
typedef struct {
    int first;      /* indice do primeiro quadro no pacote do personagem */
    int count;      /* quantos quadros */
    int fps100;     /* quadros por segundo * 100 */
    int looped;
} ch_seq_t;

/* Estado de UM personagem na tela. */
typedef struct {
    int ci;         /* indice em ch_chars[] */
    int anim;       /* indice DENTRO do personagem (ch_anim_i); -1 = nenhum */
    int missing;    /* a animacao atual e' de erro (miss) */
    long t0;        /* ms em que ela comecou */
    int lado;       /* 0 = danceLeft, 1 = danceRight (quem alterna) */
} ch_state_t;

#define CH_ANIM_MS_PADRAO 0L

void ca_reset(ch_state_t *s, int ci);

/* Troca a animacao se for diferente da atual (o original nao reinicia a mesma,
   senao a perna da GF reiniciaria a cada batida). Devolve 1 se trocou. */
int ca_play(ch_state_t *s, int anim, int missing, long now);

/* Quadro da sequencia no tempo decorrido (ms desde o inicio da animacao).
   -1 se a sequencia nao existe. */
int ca_frame(const ch_seq_t *q, long decorrido);

/* Canto: escolhe "sing<DIR>[miss]" e devolve 1 se a animacao mudou. */
int ca_canto(ch_state_t *s, int dir, int missing, long now);

/* Volta a dancar (idle ou danceLeft/Right pela batida `batida`), se estiver
   cantando ha' `sing_ms` ou mais. Devolve 1 se trocou -- e' o sinal que o
   desenho usa para saber que precisa reenviar a textura. */
int ca_ocioso(ch_state_t *s, int batida, long now, long sing_ms);

/* Passos -> ms, para o singTime. bpm vem do chart.
   Constants.hx: STEPS_PER_BEAT = 4 -- um passo e' 1/4 de batida. */
#define CA_STEPS_PER_BEAT 4
long ca_passos_ms(int passos, float bpm);

/* O tempo de canto de verdade: `ca_passos_ms` e, quando a animacao e' de erro,
   o DOBRO (BaseCharacter.hx L436). */
long ca_canto_ms(float passos, float bpm, int missing);

#ifdef __cplusplus
}
#endif
#endif /* FNF_CHAR_ANIM_H */
