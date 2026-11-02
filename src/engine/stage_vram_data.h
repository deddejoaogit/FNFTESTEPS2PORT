/* GERADO POR tools/gen_stage_vram.py - NAO EDITAR A MAO.
   Fonte: assets-data/stage-props.json (o mesmo plano que gera
   src/stage_dados.h). Cada valor e' sw*sh*2 arredondado para 256
   bytes, que e' o alinhamento do TBP0 do GS (VC_ALINHAMENTO).
   O indice de cada linha e' o MESMO de bs_props[]. */
#ifndef STAGE_VRAM_DATA_H
#define STAGE_VRAM_DATA_H

/* Bytes de VRAM de UM quadro de cada prop, na ordem de bs_props[]. */
static const unsigned int bs_prop_vram_bytes[] = {
  637952u, 202752u, 342528u, 222208u, 54272u, 54272u,
  54272u, 54272u, 54272u, 272384u,  5376u, 67328u,
  113920u, 71680u, 20224u, 20224u, 20224u, 20224u,
  20224u, 705536u, 292864u, 705536u, 95232u, 152064u,
  595968u, 144384u, 293888u,  1280u,  7168u,  6912u,
  111104u, 987392u, 193536u, 182272u, 456960u, 74752u,
  528384u, 50688u, 100608u, 62720u, 55808u, 381440u,
  67584u, 450560u, 47104u, 108544u, 270080u, 57600u,
  34560u, 343552u, 43776u, 637440u, 637440u, 360448u,
  76800u, 48128u, 31488u, 31488u, 145152u, 390144u,
  15616u, 15616u, 28160u, 28160u, 491520u, 12544u,
  246528u, 61440u, 61440u, 39168u, 21504u, 19968u,
  101376u, 207872u, 207872u, 13056u, 13056u, 22784u,
  23040u, 293888u, 116736u, 214272u, 232704u, 119808u,
  246528u, 262144u, 246528u, 214272u, 232704u, 119808u,
  246528u, 262144u, 246528u, 124672u, 124672u, 142336u,
  131840u, 106752u, 397824u, 89088u, 74496u, 124672u,
  124672u, 142336u, 131840u, 106752u, 397824u, 89088u,
  74496u, 164864u, 185600u, 174080u, 174080u, 164864u,
   4608u, 185600u, 174080u, 486400u, 41472u, 473088u,
  473088u, 215040u, 215040u, 327168u, 162816u, 84480u,
   4608u, 11264u, 50688u, 333056u, 193536u, 238592u,
  12032u, 167680u, 109824u, 40448u, 36096u, 179712u,
  219136u, 169984u, 88320u, 95232u, 105984u, 51456u,
  24576u, 366336u,  3840u, 33792u, 24320u, 15360u,
  24832u, 33792u, 13824u, 1071104u, 36864u, 29952u,
  12800u,
};
#define BS_PROP_VRAM_COUNT 157

/* Soma por palco, na ordem de bs_stage_props[]. Serve para o teste
   saber, sem varrer o array, quanto cada palco pede. */
static const unsigned int bs_stage_vram_bytes[] = {
   1676800u,   631808u,  1703936u,  1408000u,
   1363200u,  1329920u,  1417472u,  1662208u,
   1675264u,  1449728u,  1321984u,  1321984u,
   1191680u,  1191680u,   698624u,   529152u,
    486400u,  1417728u,  1736192u,  1486592u,
   1150720u,
};
#define BS_STAGE_VRAM_COUNT 21

/* Pior palco do plano atual: sserafim. */
#define BS_STAGE_VRAM_MAX 1736192u

#endif /* STAGE_VRAM_DATA_H */
