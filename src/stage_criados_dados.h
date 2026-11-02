/* GERADO POR tools/gen_stage_criados.py - NAO EDITAR A MAO.
   Fonte: assets-data/stage-criados.json (com a linha de cada script) e a
   escala do palco em assets-data/stage-props.json.

   Sao os sprites que o SCRIPT do palco cria e que o JSON do palco nao
   tem: `new FlxTiledSprite`/`new FlxBackdrop`. O port nao interpreta
   HScript -- esta tabela e' a transcricao conferida das linhas citadas.

   src_w/src_h = tamanho nativo da imagem (o periodo da repeticao, no
   espaco do original). sw/sh = resolucao do pacote (nativo x escala do
   palco), que e' o que o GS precisa para o bloco sair no tamanho certo.
   vel_x/vel_y = px/s do `scroll`, no espaco do original. */
#ifndef STAGE_CRIADOS_DADOS_H
#define STAGE_CRIADOS_DADOS_H

typedef struct {
    const char *stage;      /* palco dono (conferido no teste)        */
    const char *name;       /* variavel do script                     */
    const char *tex;        /* stages/<palco>/<nome>.cfz4             */
    float x, y;             /* canto do quad, espaco do original      */
    float w, h;             /* tamanho do quad (`width`/`height`)     */
    float src_w, src_h;     /* tamanho nativo = periodo da repeticao  */
    int sw, sh;             /* resolucao do pacote                    */
    float scrollx, scrolly; /* scrollFactor (1 = colado na camera)    */
    int z;                  /* zIndex                                 */
    float alpha;
    int blend;              /* 0 normal 1 ADD 2 MULTIPLY 3 SCREEN     */
    float vel_x, vel_y;     /* px/s do scroll, espaco do original     */
    int tile;               /* 1 = repete na horizontal               */
} bs_criado_t;

static const bs_criado_t bs_criados[] = {
  {"phillyBlazin","scrollingSky","stages/phillyBlazin/scrollingSky.cfz4",
   -700.000000f,-120.000000f,4000.000000f,495.000000f,
   1387.000000f,495.000000f,683,244,
   0.000000f,0.000000f,10,1.000000f,0,
   -35.000000f,0.000000f,1},
};
#define BS_CRIADO_COUNT 1

/* VRAM de UM quadro (o pacote tem um), alinhada ao TBP0. */
static const unsigned int bs_criado_vram_bytes[] = {
  343552u,
};

/* Faixa de cada palco em bs_criados[], na MESMA ordem de
   bs_stage_props[] (alfabetica, a de gen_stage_dados.py). */
static const int bs_stage_criado_first[] = {
  0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
};
static const int bs_stage_criado_count[] = {
  0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

#endif /* STAGE_CRIADOS_DADOS_H */
