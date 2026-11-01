#include "splash.h"

/* NOTE SPLASH: estado, tempo e geometria.
 *
 * TUDO O QUE ESTA AQUI TEM UMA LINHA NA FONTE v0.8.6 -- e o numero que nao tem
 * nao foi inventado, foi derivado:
 *
 *   duracao            NoteSplash.hx L13-14: splashFramerate 24 e
 *                      splashFramerateVariance 2. `play()` faz
 *                      `frameRate = 24 + FlxG.random.int(-2, 2)`. A animacao
 *                      tem 4 quadros, entao a splash vive 4/fps segundos.
 *   variante           NoteSplash.hx L46-50: sorteia entre as animacoes
 *                      `splash<DIR><n>`; a arte do jogo tem n = 0..1.
 *   alpha              0.8 do note style (preload/data/notestyles/funkin.json),
 *                      nao um numero escolhido.
 *   posicao            Strumline.hx L1086-1094 (ver sp_posicao abaixo).
 *   centro             NoteSplash.hx L56: offset.set(w * 0.3, h * 0.3).
 *
 * O ATLAS. A arte original e' 2048x2048 (16,8 MB) com 32 quadros de 260x298.
 * Nada disso cabe na VRAM de 4 MiB do PS2 junto com palco, personagens e HUD,
 * e o orcamento das 27 musicas esta' apertado a 256 B em `stress`. A splash
 * portanto mora DENTRO do atlas do HUD (tex2, 320x256), na faixa livre abaixo
 * da barra de vida -- VRAM nova: zero. A resolucao da celula e' a maior que
 * cabe nessa faixa, escolhida por medida (tools/empacotar_splash.py), e o
 * tamanho DESENHADO continua o do original: a imagem e' degradada, o evento
 * nao (regra do projeto).
 */

/* O HUD do port desenha em 640x360, metade dos 1280x720 do original. Todo
 * comprimento logico (offsets do note style, INITIAL_OFFSET, tamanho do canvas)
 * atravessa essa metade. */
#define SP_HALF 0.5f

static uint32_t sp_rng(nspl_pool_t *p)
{
    /* xorshift32: determinismo importa para o teste e para o harness, e o
     * original tambem sorteia com uma semente reproduzivel. */
    uint32_t x = p->rng;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    p->rng = x ? x : 0x9e3779b9u;
    return p->rng;
}

static int sp_range(nspl_pool_t *p, int lo, int hi)
{
    return lo + (int)(sp_rng(p) % (uint32_t)(hi - lo + 1));
}

void nspl_init(nspl_pool_t *p, uint32_t seed)
{
    int i;
    for (i = 0; i < SP_MAX; i++) {
        p->it[i].lane = 0;
        p->it[i].variante = 0;
        p->it[i].ativo = 0;
        p->it[i].fps = SPLASH_FPS;
        p->it[i].t0 = 0;
    }
    p->rng = seed ? seed : 0x1234567u;
    p->criados = 0;
    p->vencidos = 0;
    p->reciclados = 0;
}

int nspl_novo(nspl_pool_t *p, int lane, uint32_t ms)
{
    int i, alvo = -1;
    uint32_t mais_velho = 0;
    int achou_velho = 0;

    if (lane < 0 || lane >= (int)(SPLASH_ANIMS / 2u)) return -1;

    for (i = 0; i < SP_MAX; i++) {
        if (!p->it[i].ativo) {
            alvo = i;
            break;
        }
        /* O mais antigo e' o primeiro a vencer: reciclar esse custa menos
         * tempo de animacao perdido. */
        if (!achou_velho || (int32_t)(p->it[i].t0 - mais_velho) < 0) {
            mais_velho = p->it[i].t0;
            alvo = i;
            achou_velho = 1;
        }
    }
    if (alvo < 0) alvo = 0;
    if (p->it[alvo].ativo) p->reciclados++;

    p->it[alvo].lane = (uint8_t)lane;
    p->it[alvo].variante = (uint8_t)sp_range(p, 0, (int)(SPLASH_ANIMS / 2u) - 1);
    p->it[alvo].fps = (uint16_t)((int)SPLASH_FPS +
                                 sp_range(p, -(int)SPLASH_FPS_VAR,
                                          (int)SPLASH_FPS_VAR));
    p->it[alvo].t0 = ms;
    p->it[alvo].ativo = 1;
    p->criados++;
    return alvo;
}

int nspl_quadro(const nspl_pool_t *p, int i, uint32_t ms)
{
    uint32_t dt;
    int q;
    if (i < 0 || i >= SP_MAX) return -1;
    if (!p->it[i].ativo) return -1;
    dt = ms - p->it[i].t0;           /* ms desde o acerto */
    q = (int)((dt * (uint32_t)p->it[i].fps) / 1000u);
    if (q < 0 || q >= (int)SPLASH_QUADROS) return -1;
    return q;
}

int nspl_passo(nspl_pool_t *p, uint32_t ms)
{
    int i, ativos = 0;
    for (i = 0; i < SP_MAX; i++) {
        if (!p->it[i].ativo) continue;
        if (nspl_quadro(p, i, ms) < 0) {
            p->it[i].ativo = 0;
            p->vencidos++;
            continue;
        }
        ativos++;
    }
    return ativos;
}

int nspl_ativos(const nspl_pool_t *p)
{
    int i, n = 0;
    for (i = 0; i < SP_MAX; i++) if (p->it[i].ativo) n++;
    return n;
}

const nspl_cel_t *nspl_celula(int lane, int variante)
{
    int i;
    if (lane < 0 || lane >= (int)(SPLASH_ANIMS / 2u)) return 0;
    if (variante < 0 || variante >= 2) return 0;
    i = lane * 2 + variante;
    if (i < 0 || i >= (int)SPLASH_ANIMS) return 0;
    return &g_splash_cel[i];
}

void nspl_posicao(float receptor_x, float receptor_y, const nspl_cel_t *c,
                float *x, float *y)
{
    float px, py;

    if (!c) {
        if (x) *x = 0.0f;
        if (y) *y = 0.0f;
        return;
    }

    /* Strumline.playNoteSplash L1087-1094, ja' em pixels de 640x360:
     *
     *   splash.x = strum.x + getXPos(dir) + INITIAL_OFFSET + off[0] * scale
     *   splash.y = strum.y - INITIAL_OFFSET              + off[1] * scale
     *
     * `receptor_x`/`receptor_y` e' exatamente `strum.x + getXPos(dir)` e
     * `strum.y` no original: no port os receptores sao desenhados nesse ponto
     * (g_receptor_x/g_receptor_y do main.c). O `+ INITIAL_OFFSET` do x e o
     * `- INITIAL_OFFSET` do y entram na conversao abaixo, em metade. */
    px = receptor_x + (SPLASH_OFF_X * 1.0f) * SP_HALF;
    py = receptor_y + (-SPLASH_INITIAL_OFFSET + SPLASH_OFF_Y * 1.0f) * SP_HALF;

    /* NoteSplash.play L56: `offset.set(width * 0.3, height * 0.3)`. No flixel o
     * offset e' SUBTRAIDO no desenho (FlxSprite.draw: getScreenPosition() -
     * offset), entao o canto superior esquerdo cai em (x - 0.3w, y - 0.3h). */
    if (x) *x = px - SPLASH_CENTRO * c->dw;
    if (y) *y = py - SPLASH_CENTRO * c->dh;
}
