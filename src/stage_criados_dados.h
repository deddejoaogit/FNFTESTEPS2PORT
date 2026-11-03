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
    int tile;               /* periodo da repeticao, px do original   */
    unsigned char rgb[3];   /* o `color` do sprite (o GS multiplica)  */
    int pintado;            /* 1 = a cor acima veio do script         */
    int backdrop;           /* 1 = FlxBackdrop (emendas na borda)     */
} bs_criado_t;

/* O PISO EM PERSPECTIVA (`PerspectiveSprite` = FlxSkewedSprite com
   `transformMatrix`). Nao e' um quad reto: os dois pontos de baixo e de
   cima sao corrigidos pelo scroll da camera COM scrollFactors proprios e
   viram uma matriz afim por quadro (PerspectiveSprite.hxc L97-125):

     distX = cima.x - baixo.x
     distY = cima.y - (baixo.y - H)
     a=1  b=0  c=-distX/H  d=1-distY/H  tx=distX/2  ty=distY/2
     sprite.x = baixo.x - W/2   sprite.y = baixo.y - H

   `x/y` aqui sao os do sprite com a camera em (0,0) -- existem para o
   teste conferir a conta contra a formula da fonte. */
typedef struct {
    const char *stage;
    const char *name;
    const char *tex;        /* durante a musica                     */
    const char *tex_cut;    /* o `loadGraphic(floor-cutscene)`       */
    float bottom_x, bottom_y, top_x, top_y;   /* `setPositions`     */
    float bsf_x, bsf_y, tsf_x, tsf_y;         /* `setScrollFactors` */
    float src_w, src_h;     /* H e W da formula (tamanho nativo)     */
    int sw, sh;             /* resolucao do pacote                   */
    int z;
} bs_chao_t;

static const bs_criado_t bs_criados[] = {
  {"phillyBlazin","scrollingSky","stages/phillyBlazin/skyBlur.cfz4",
   -700.000000f,-120.000000f,4000.000000f,495.000000f,
   1387.000000f,495.000000f,683,244,
   0.000000f,0.000000f,10,1.000000f,0,
   -35.000000f,0.000000f,1387,{128,128,128},0,0},
  {"sserafim","dust1","stages/sserafim/dustMid.cfz4",
   -650.000000f,-200.000000f,3072.000000f,1920.000000f,
   2048.000000f,1280.000000f,289,181,
   1.100000f,1.100000f,2000,0.800000f,0,
   350.000000f,0.000000f,3072,{76,66,63},1,1},
  {"sserafim","dust2","stages/sserafim/dustBack.cfz4",
   -650.000000f,-250.000000f,3072.000000f,1920.000000f,
   2048.000000f,1280.000000f,289,181,
   1.150000f,1.150000f,2000,0.900000f,0,
   -300.000000f,0.000000f,3072,{70,54,50},1,1},
  {"sserafim","dust3","stages/sserafim/dustMid.cfz4",
   -650.000000f,-400.000000f,4096.000000f,2560.000000f,
   2048.000000f,1280.000000f,289,181,
   1.200000f,1.200000f,2000,0.800000f,0,
   -200.000000f,0.000000f,4096,{55,50,46},1,1},
  {"sserafim","dust4","stages/sserafim/dustBack.cfz4",
   -650.000000f,-1300.000000f,7168.000000f,4480.000000f,
   2048.000000f,1280.000000f,289,181,
   1.250000f,1.250000f,2000,0.900000f,0,
   -150.000000f,0.000000f,7168,{68,53,48},1,1},
};
#define BS_CRIADO_COUNT 5

/* VRAM de UM quadro (o pacote tem um), alinhada ao TBP0. */
static const unsigned int bs_criado_vram_bytes[] = {
  343552u, 115968u, 115968u, 115968u, 115968u,
};

/* Faixa de cada palco em bs_criados[], na MESMA ordem de
   bs_stage_props[] (alfabetica, a de gen_stage_dados.py). */
static const int bs_stage_criado_first[] = {
  0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 5, 5,
};
static const int bs_stage_criado_count[] = {
  0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0,
};

static const bs_chao_t bs_chaos[] = {
  {"sserafim","perspectiveFloor","stages/sserafim/floor.cfz4","stages/sserafim/floor-cutscene.cfz4",
   760.000000f,1375.000000f,790.000000f,625.000000f,
   1.050000f,1.050000f,0.930000f,0.930000f,
   5362.000000f,754.000000f,757,106,11},
};
#define BS_CHAO_COUNT 1

/* VRAM de UM quadro de cada piso (o pacote tem um), alinhada ao TBP0. */
static const unsigned int bs_chao_vram_bytes[] = {
  162816u,
};

/* Faixa de cada palco em bs_chaos[], na MESMA ordem de bs_stage_props[]. */
static const int bs_stage_chao_first[] = {
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1,
};
static const int bs_stage_chao_count[] = {
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0,
};

#endif /* STAGE_CRIADOS_DADOS_H */
