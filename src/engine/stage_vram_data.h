/* GERADO POR tools/gen_stage_vram.py - NAO EDITAR A MAO.
   Fonte: assets-data/stage-props.json (o mesmo plano que gera
   src/stage_dados.h). Cada valor e' sw*sh*2 arredondado para 256
   bytes, que e' o alinhamento do TBP0 do GS (VC_ALINHAMENTO).
   O indice de cada linha e' o MESMO de bs_props[]. */
#ifndef STAGE_VRAM_DATA_H
#define STAGE_VRAM_DATA_H

/* Bytes de VRAM de UM quadro de cada prop, na ordem de bs_props[]. */
static const unsigned int bs_prop_vram_bytes[] = {
  729600u, 231424u, 359424u, 266240u, 56832u, 56832u,
  56832u, 56832u, 56832u, 740608u, 18688u, 201216u,
  309760u, 216832u, 52992u, 52992u, 52992u, 52992u,
  52992u, 728832u, 302848u, 728832u, 107520u, 189440u,
  783104u, 189952u, 355328u,  1536u,  7936u,  7680u,
  156928u, 987392u, 193536u, 182272u, 585728u, 108800u,
  720384u, 88320u, 136192u, 87552u, 65024u, 477440u,
  86528u, 552960u, 79616u, 139264u, 335104u, 71936u,
  58368u, 353536u, 45056u, 657408u, 657408u, 377344u,
  80384u, 50176u, 32768u, 32768u, 151552u, 445440u,
  16384u, 16384u, 29696u, 29696u, 514560u, 13056u,
  308480u, 69632u, 69632u, 44288u, 24064u, 22528u,
  132608u, 263936u, 263936u, 14592u, 14592u, 25856u,
  25856u, 404224u, 148736u, 214272u, 232704u, 119808u,
  246528u, 262144u, 246528u, 214272u, 232704u, 119808u,
  246528u, 262144u, 246528u, 164864u, 164864u, 185600u,
  174080u, 144384u, 524288u, 103168u, 98304u, 164864u,
  164864u, 185600u, 174080u, 144384u, 524288u, 103168u,
  98304u, 164864u, 185600u, 174080u, 174080u, 164864u,
   4608u, 185600u, 174080u, 1749248u, 46592u, 582144u,
  582144u, 291072u, 291072u, 336384u, 181504u, 95232u,
   4864u, 11776u, 57600u, 342272u, 198912u, 244736u,
  12288u, 172032u, 112896u, 41216u, 40704u, 249344u,
  278784u, 216576u, 119808u, 108032u, 119808u, 58112u,
  27904u, 460800u,  4352u, 38400u, 27392u, 23040u,
  42240u, 38400u, 15872u, 1071104u, 36864u, 29952u,
  12800u,
};
#define BS_PROP_VRAM_COUNT 157

/* Soma por palco, na ordem de bs_stage_props[]. Serve para o teste
   saber, sem varrer o array, quanto cada palco pede. */
static const unsigned int bs_stage_vram_bytes[] = {
   1870848u,  1752064u,  1760512u,  1799424u,
   1363200u,  1792000u,  1801216u,  1713408u,
   1790208u,  1832960u,  1321984u,  1321984u,
   1559552u,  1559552u,   698624u,   529152u,
   1749248u,  1793024u,  1811712u,  1869568u,
   1150720u,
};
#define BS_STAGE_VRAM_COUNT 21

/* Pior palco do plano atual: limoRide. */
#define BS_STAGE_VRAM_MAX 1870848u

#endif /* STAGE_VRAM_DATA_H */
