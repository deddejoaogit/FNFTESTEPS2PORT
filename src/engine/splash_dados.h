/* GERADO POR tools/gen_splash_dados.py -- NAO EDITE.
 *
 * Fonte: assets-data/splash-plan.json, que tools/empacotar_splash.py
 * escreve depois de recortar os 32 quadros de shared/images/noteSplashes
 * .png+xml. Os numeros abaixo sao do plano, nao digitos escolhidos.
 *
 * A REGIAO vive DENTRO do atlas do HUD (tex2, 320x256 PSMCT32), na faixa
 * livre abaixo da barra de vida: VRAM nova = 0 B. O comentario no topo
 * de src/engine/splash.c explica a regra do original.
 */
#ifndef SPLASH_DADOS_H
#define SPLASH_DADOS_H

#define SPLASH_TEX_W 320u
#define SPLASH_TEX_H 256u
#define SPLASH_REG_X 0u
#define SPLASH_REG_Y 128u
#define SPLASH_REG_W 224u
#define SPLASH_REG_H 124u
#define SPLASH_ANIMS 8u
#define SPLASH_QUADROS 4u
/* Linhas do atlas que pertencem a UMA animacao (um quadro por linha). */
#define SPLASH_FAIXA 31u

/* NoteSplash.hx L13-14: splashFramerate = 24, splashFramerateVariance = 2. */
#define SPLASH_FPS 24u
#define SPLASH_FPS_VAR 2u
/* preload/data/notestyles/funkin.json: assets.noteSplash.alpha = 0.8. */
#define SPLASH_ALPHA 204u

/* Posicao. Strumline.playNoteSplash L1086-1094 e NoteSplash.play L56. */
#define SPLASH_OFF_X (25.0f)
#define SPLASH_OFF_Y (-5.0f)
/* INITIAL_OFFSET = -0.275 * STRUMLINE_SIZE (104). */
#define SPLASH_INITIAL_OFFSET (-28.6000f)
/* offset.set(width * 0.3, height * 0.3), subtraido no desenho. */
#define SPLASH_CENTRO (0.3f)

typedef struct {
    unsigned short cx, cy, cw, ch;  /* celula no atlas (tex2)        */
    unsigned short lw, lh;          /* canvas logico do original     */
    float dw, dh;                   /* tamanho desenhado (640x360)   */
} nspl_cel_t;

static const nspl_cel_t g_splash_cel[SPLASH_ANIMS] = {
    {   0u,   0u, 27u, 31u, 260u, 298u, 130.0f, 149.0f },  /* splashLEFT0 */
    {  27u,   0u, 29u, 28u, 274u, 267u, 137.0f, 133.5f },  /* splashLEFT1 */
    {  56u,   0u, 27u, 31u, 260u, 298u, 130.0f, 149.0f },  /* splashDOWN0 */
    {  83u,   0u, 29u, 28u, 274u, 267u, 137.0f, 133.5f },  /* splashDOWN1 */
    { 112u,   0u, 27u, 31u, 260u, 298u, 130.0f, 149.0f },  /* splashUP0 */
    { 139u,   0u, 29u, 28u, 274u, 267u, 137.0f, 133.5f },  /* splashUP1 */
    { 168u,   0u, 27u, 31u, 260u, 298u, 130.0f, 149.0f },  /* splashRIGHT0 */
    { 195u,   0u, 29u, 28u, 274u, 267u, 137.0f, 133.5f },  /* splashRIGHT1 */
};

#endif /* SPLASH_DADOS_H */
