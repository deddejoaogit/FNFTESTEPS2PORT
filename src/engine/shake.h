/* shake - o tremor de camera do `FlxCamera.shake`, com teste no PC
 *
 * POR QUE ISTO EXISTE
 * Quatorze dos 68 pares (personagem, note kind) dos 81 charts pedem tremor:
 * `PlayState.instance.camGame.shake(0.0025, 0.15)` e o irmao
 * `shake(0.002, 0.1)` do `weekend-1-punchhighblocked` (darnell-blazin.hxc
 * L306-311). O port tinha o dado lido e nao aplicava -- o Blazin' inteiro
 * batia sem tremer. Isto e' a conta do tremor, fora do main.c, porque e' uma
 * conta que erra em silencio: tremor com amplitude errada parece "estilo".
 *
 * DE ONDE VEM A REGRA (lida, nao inventada)
 * `FlxCamera.shake(Intensity, Duration)` guarda intensidade e duracao; a cada
 * `update(elapsed)` desconta o tempo e, enquanto sobra tempo, sorteia
 * `offset = random.float(-I, I) * (width, height)`; quando o tempo acaba, o
 * offset volta a zero. Ou seja: a intensidade e' uma FRACAO do tamanho da
 * camera, nao um numero de pixels -- 0,0025 numa camera de 1280x720 da' +-3,2
 * px na horizontal e +-1,8 na vertical.
 *
 * O QUE NAO E' IGUAL, DE PROPOSITO
 * A sequencia de numeros aleatorios. O original sorteia pelo `FlxG.random`, de
 * semente do jogo; o port usa um LCG proprio e deterministico, porque o
 * harness tem de dar o mesmo resultado duas vezes. O que se preserva e' a
 * distribuicao (uniforme em [-I, I]) e a amplitude, nao a sequencia alheia.
 *
 * C puro: sem GS, sem arquivo, sem tempo real. Quem chama passa o dt.
 */
#ifndef FNF_SHAKE_H
#define FNF_SHAKE_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float forca;      /* intensidade: fracao do tamanho da camera */
    float restante;   /* segundos que faltam; <= 0 = parado           */
    float x, y;       /* deslocamento do quadro, em pixels de camera  */
    unsigned int semente;
} shk_t;

/* Zera o tremor e a sequencia (a semente do LCG). */
void shk_reset(shk_t *s, unsigned int semente);

/* `camGame.shake(forca, dur_s)`. Chamar de novo enquanto um tremor roda
   SUBSTITUI o anterior -- e' o que o FlxCamera.shake faz (ele sobrescreve
   `_fxShakeIntensity`/`_fxShakeDuration`). Forca ou duracao <= 0 nao comeca
   nada e NAO apaga o tremor em curso. */
void shk_inicia(shk_t *s, float forca, float dur_s);

/* Um QUADRO: desconta `dt` e sorteia o deslocamento. Com o tempo esgotado,
   x e y ficam em 0 no mesmo passo em que o tremor termina (o original tambem
   zera no ultimo update). */
void shk_passo(shk_t *s, float dt, float cam_w, float cam_h);

#ifdef __cplusplus
}
#endif
#endif /* FNF_SHAKE_H */
