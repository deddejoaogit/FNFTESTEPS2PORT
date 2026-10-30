/* GERADO POR tools/gen_stage_vram.py - NAO EDITAR A MAO.
   Fonte: assets-data/stage-props.json (o mesmo plano que gera
   src/stage_dados.h). Cada valor e' sw*sh*2 arredondado para 256
   bytes, que e' o alinhamento do TBP0 do GS (VC_ALINHAMENTO).
   O indice de cada linha e' o MESMO de bs_props[]. */
#ifndef STAGE_VRAM_DATA_H
#define STAGE_VRAM_DATA_H

/* Bytes de VRAM de UM quadro de cada prop, na ordem de bs_props[]. */
static const unsigned int bs_prop_vram_bytes[] = {
  675584u, 222976u, 346368u, 240896u, 42496u, 42496u,
  42496u, 42496u, 42496u, 737536u, 16384u, 194816u,
  302848u, 207872u, 47872u, 47872u, 47872u, 47872u,
  47872u, 702720u, 292096u, 702720u, 94976u, 182272u,
  749056u, 170496u, 349696u,  1536u,  7424u,  7168u,
  135424u, 987392u, 159744u, 182272u, 584192u, 99840u,
  673024u, 69120u, 136192u, 85248u, 50944u, 470016u,
  77824u, 540928u, 56064u, 124672u, 318976u, 68352u,
  41216u, 353536u, 37120u, 653568u, 653568u, 367360u,
  79872u, 49408u, 28672u, 28672u, 151296u, 413184u,
  15616u, 15616u, 20224u, 20224u, 501248u,  8192u,
  288256u, 62208u, 62208u, 38912u, 23040u, 21248u,
  114688u, 262144u, 262144u, 12288u, 12288u, 15360u,
  15616u, 368640u, 140800u, 206592u, 229632u, 110336u,
  237312u, 262144u, 237312u, 206592u, 229632u, 110336u,
  237312u, 262144u, 237312u, 150272u, 164608u, 180736u,
  158464u, 132352u, 524288u, 100352u, 86784u, 150272u,
  164608u, 180736u, 158464u, 132352u, 524288u, 100352u,
  86784u, 164608u, 180736u, 158464u, 158464u, 164608u,
   1024u, 180736u, 158464u, 1696512u, 44288u, 562688u,
  562688u, 263936u, 263936u, 318976u, 169472u, 89088u,
    256u,  9472u, 52992u, 319232u, 187136u, 243456u,
   7680u, 155904u, 104960u, 39936u,   768u, 230144u,
  257536u, 196864u, 112128u, 95744u, 118016u, 56064u,
  23296u, 460288u,  4352u, 31744u, 20992u, 17920u,
  29696u, 31744u, 13056u, 1071104u, 34048u, 24064u,
  10240u,
};
#define BS_PROP_VRAM_COUNT 157

/* Soma por palco, na ordem de bs_stage_props[]. Serve para o teste
   saber, sem varrer o array, quanto cada palco pede. */
static const unsigned int bs_stage_vram_bytes[] = {
   1698304u,  1698816u,  1697536u,  1698048u,
   1329408u,  1698560u,  1698048u,  1697792u,
   1699584u,  1699840u,  1283328u,  1283328u,
   1497856u,  1497856u,   662272u,   504832u,
   1696512u,  1697536u,  1698560u,  1700352u,
   1139456u,
};
#define BS_STAGE_VRAM_COUNT 21

/* Pior palco do plano atual: tankmanBattlefield. */
#define BS_STAGE_VRAM_MAX 1700352u

#endif /* STAGE_VRAM_DATA_H */
