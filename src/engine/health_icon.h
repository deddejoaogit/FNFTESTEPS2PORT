/* health_icon - o icone de vida do HUD: pacote, estado e posicao
 *
 * POR QUE ISTO EXISTE
 * Os dois icones ao lado da barra de vida eram os unicos elementos do HUD
 * desenhados com arte de reserva. O original cria um `HealthIcon` para o
 * oponente (`PlayState.hx` L2155) e um para o jogador (L2190), ambos na
 * `camHUD`, e cada um mostra o rosto do personagem: `CharacterData.healthIcon`
 * (`funkin/data/character/CharacterData.hx`) ou, quando o personagem nao
 * declara um, `Constants.DEFAULT_HEALTH_ICON` = 'face' (util/Constants.hx L210).
 *
 * O QUE VEM DA FONTE, LINHA POR LINHA (play/components/HealthIcon.hx)
 *   L150  HEALTH_ICON_SIZE = 150        (num espaco de 1280x720)
 *   L157  PIXEL_ICON_SIZE  = 32
 *   L163  BOP_SCALE        = 0.2
 *   L168  POSITION_OFFSET  = 26
 *   L124  WINNING_THRESHOLD = 0.8 * 2   (sobre a vida, que vai de 0 a 2)
 *   L129  LOSING_THRESHOLD  = 0.2 * 2
 *   L284  updatePosition():
 *           eixo Y: y = healthBar.y - (this.height / 2)
 *           jogador 0 (BF):  x = bar.x + bar.w * (remapToRange(value,0,2,100,0)*0.01) - 26
 *           jogador 1 (oponente): x = bar.x + bar.w * (...)*0.01 - (this.width - 26)
 *         `remapToRange(v,0,2,100,0)` = 100 - 50*v (FlxMath), e `healthBar.value`
 *         e' a propria vida (FlxBar com min 0 e max 2, `PlayState.hx` L811/L1452).
 *         Com isso a conta vira, escrita uma vez so':
 *             fronteira = bar.x + bar.w * (1 - vida/2)
 *         ou seja os dois icones andam JUNTO com a divisa entre o vermelho e o
 *         verde. (E' o que o jogo faz: no comeco da musica os dois ficam no
 *         canto esquerdo, no fim no canto direito.)
 *   L337  onStepHit(): a cada `bopEvery` passos (default STEPS_PER_BEAT = 4)
 *           setGraphicSize(this.width + HEALTH_ICON_SIZE*size*BOP_SCALE)
 *           e um tween NUM de volta para HEALTH_ICON_SIZE*size em
 *           min(stepLengthMs * 0.002, 0.175) segundos, LINEAR
 *           (FlxTween.num sem `ease` usa o padrao, que e' linear).
 *   L587  loadCharacter(): sem PNG do icone, cai no `face` e avisa.
 *
 * A POSICAO E O ESTADO SAO PUROS: nada aqui desenha nem abre arquivo, entao o
 * teste do PC compara com as contas acima e o console so' precisa confirmar o
 * que ja' foi medido.
 */
#ifndef FNF_HEALTH_ICON_H
#define FNF_HEALTH_ICON_H

#ifdef __cplusplus
extern "C" {
#endif

/* Cabecalho do pacote de icone (tools/empacotar_icones.py, formato ICP1). */
#define HI_HEADER_BYTES 32u
#define HI_PSM_16 16u

/* `HEALTH_ICON_SIZE` (150) no espaco de 640x360 que o port desenha: 75 px. */
#define HI_TELA_PX 75.0f
/* `BOP_SCALE` (HealthIcon.hx L163). */
#define HI_BOP_ESCALA 0.2f
/* `POSITION_OFFSET` (L168), tambem em px de 640x360: 26 no original de 1280. */
#define HI_POS_OFFSET 26.0f
/* Vida (0..2) em que o icone troca de estado: 0.8*2 e 0.2*2. */
#define HI_LIMIAR_GANHANDO 1.6f
#define HI_LIMIAR_PERDENDO 0.4f

/* Estados, na ordem dos quadros do pacote legado (HealthIcon.loadAnimationOld):
   quadro 0 = idle, quadro 1 = losing, quadro 2 = winning (so' quando existe). */
#define HI_IDLE 0u
#define HI_PERDENDO 1u
#define HI_GANHANDO 2u

typedef struct {
    unsigned int w, h;          /* tamanho do quadro em pixels */
    unsigned int quadros;       /* 1..3 */
    unsigned int psm;           /* HI_PSM_16 */
    unsigned int passo;         /* largura da textura (TBW), multipla de 64 */
    unsigned int flags;         /* bit 0 = isPixel */
    unsigned int bytes_quadro;
    unsigned int dados_bytes;
    unsigned int soma;          /* soma dos bytes, conferida no leitor */
} hi_pack_t;

/* Le o cabecalho de um pacote. Devolve 0 se esta' tudo certo, negativo se nao:
   -1 cabecalho curto, -2 magic, -3 versao, -4 tamanho de quadro zero,
   -5 quadros fora de 1..3, -6 psm desconhecido, -7 passo fora de 64 em 64,
   -8 bytes_quadro nao bate com passo*h*2, -9 dados com tamanho diferente do
   que o cabecalho declara, -10 soma diferente. */
int hi_pack_ler(const unsigned char *buf, unsigned int bytes, hi_pack_t *out);

/* Dados do quadro i (o comeco do bloco). 0 se o indice nao existe. */
const unsigned char *hi_quadro(const unsigned char *buf, const hi_pack_t *p,
                               unsigned int i);

/* Bytes de VRAM de UMA textura (um quadro): passo * h * 2. */
unsigned int hi_vram_bytes(const hi_pack_t *p);

/* Quantos pixels na tela o icone ocupa (sempre HI_TELA_PX: o original faz
   setGraphicSize(150) tanto no icone normal quanto no de pixel art). */
float hi_tela_px(void);

/* Estado do icone para uma vida. `quadros` importa: um pacote de 2 quadros nao
   tem animacao de "ganhando", e o original so' a registra quando o spritesheet
   tem 3 quadros (loadAnimationOld). */
unsigned int hi_estado(float vida, unsigned int quadros);

/* Vida INVERTIDA que o icone do oponente usa: HealthIcon.hx L305 chama
   updateHealthIcon(MAXIMUM_HEALTH - health). Fica aqui para nao virar uma
   subtracao solta dentro do desenho. */
float hi_vida_oponente(float vida);

/* O pulo do icone a cada passo (onStepHit). `bop_t` e' o tempo desde o ultimo
   pulo e `bop_dur` a duracao do tween, calculada do stepLength. */
typedef struct {
    float bop_t;
    float bop_dur;
    int ativo;
} hi_bop_t;

void hi_bop_reset(hi_bop_t *b);
/* Um passo chegou (curStep % bopEvery == 0). step_len_s = stepLengthMs/1000. */
void hi_bop_passo(hi_bop_t *b, float step_len_s);
void hi_bop_tick(hi_bop_t *b, float dt);
/* Escala RELATIVA ao tamanho na tela: 1.0 parado, 1.2 logo depois do pulo. */
float hi_bop_escala(const hi_bop_t *b);

/* Posicao no espaco de 640x360 (camHUD). jogador: 0 = BF, 1 = oponente. */
void hi_posicao(int jogador, float barra_x, float barra_w, float barra_y,
                float vida, float icone_w, float icone_h,
                float *x, float *y);

#ifdef __cplusplus
}
#endif
#endif /* FNF_HEALTH_ICON_H */
