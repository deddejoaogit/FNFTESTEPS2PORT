/* GERADO POR tools/gen_gameover_sfx.py - NAO EDITAR A MAO.
   Fonte: preload/scripts/characters/<id>.hxc do funkin.assets (724eba9),
   funcao onCreate de cada personagem. Somente atribuicoes literais;
   personagem que nao atribui fica fora da tabela e o jogo usa "",
   que e' o que GameOverSubState.reset() faz (PlayState.reset chama). */
#ifndef GAMEOVER_DADOS_H
#define GAMEOVER_DADOS_H

typedef struct{
  const char*id;
  const char*musica;   /* GameOverSubState.musicSuffix   */
  const char*blueball; /* GameOverSubState.blueBallSuffix */
  const char*anim;     /* GameOverSubState.animationSuffix */
  const char*pausa;    /* PauseSubState.musicSuffix      */
}go_sfx_dados_t;

static const go_sfx_dados_t go_sfx_chars[]={
  {"bf-pixel","-pixel","-pixel","","-pixel"},
  {"pico-blazin","-pico","-pico-gutpunch","","-pico"},
  {"pico-holding-nene","-pico","-pico-and-nene","","-pico"},
  {"pico-pixel","-pixel-pico","-pixel-pico","","-pixel"},
  {"pico-playable","-pico","-pico","","-pico"},
};
#define GO_SFX_CHARS_COUNT ((int)(sizeof(go_sfx_chars)/sizeof(go_sfx_chars[0])))

#endif /* GAMEOVER_DADOS_H */
