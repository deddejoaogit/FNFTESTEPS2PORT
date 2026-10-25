/* GERADO POR tools/butterscotch/bs.py dados - NAO EDITAR */
#ifndef BS_DADOS_H
#define BS_DADOS_H

/* personagens: offsets, cameraOffsets, singTime */
typedef struct{const char*id;float ox,oy,cx,cy,sing;}bs_char_t;
static const bs_char_t bs_chars[]={
  {"bf-car",17.000f,3.000f,-17.000f,36.000f,8.000f},
  {"bf-christmas",0.000f,0.000f,0.000f,0.000f,8.000f},
  {"bf-dark",16.000f,3.000f,-16.000f,36.000f,8.000f},
  {"bf-holding-gf",0.000f,0.000f,0.000f,0.000f,8.000f},
  {"bf-pixel",0.000f,0.000f,0.000f,0.000f,8.000f},
  {"bf",17.000f,14.000f,-18.000f,36.000f,8.000f},
  {"dad",13.000f,3.000f,11.000f,6.000f,8.000f},
  {"darnell-blazin",523.000f,453.000f,0.000f,0.000f,999.000f},
  {"darnell",18.000f,31.000f,-18.000f,4.000f,8.000f},
  {"gf-car",0.000f,0.000f,0.000f,-199.000f,8.000f},
  {"gf-christmas",-1.000f,5.000f,1.000f,8.000f,8.000f},
  {"gf-dark",-12.000f,4.000f,12.000f,8.000f,8.000f},
  {"gf-pixel",0.000f,0.000f,0.000f,0.000f,8.000f},
  {"gf-tankmen",0.000f,0.000f,0.000f,0.000f,8.000f},
  {"gf",-12.000f,4.000f,12.000f,8.000f,8.000f},
  {"mom-car",0.000f,0.000f,0.000f,-99.000f,8.000f},
  {"monster-christmas",0.000f,1.000f,0.000f,12.000f,8.000f},
  {"monster",0.000f,1.000f,0.000f,29.000f,8.000f},
  {"nene-christmas",-16.000f,-89.000f,-24.000f,34.000f,8.000f},
  {"nene-dark",4.000f,-89.000f,-4.000f,34.000f,8.000f},
  {"nene-pixel",-7.000f,-23.000f,-7.000f,-107.000f,8.000f},
  {"nene-tankmen",-44.000f,-67.000f,44.000f,56.000f,8.000f},
  {"nene",4.000f,-89.000f,-4.000f,34.000f,8.000f},
  {"otis-speaker",-13.000f,-132.000f,-27.000f,57.000f,4.000f},
  {"parents-christmas",1.000f,9.000f,-1.000f,10.000f,8.000f},
  {"pico-blazin",611.000f,652.000f,0.000f,0.000f,999.000f},
  {"pico-christmas",43.000f,32.000f,47.000f,132.000f,8.000f},
  {"pico-dark",19.000f,1.000f,-19.000f,32.000f,8.000f},
  {"pico-holding-nene",52.000f,16.000f,-32.000f,24.000f,8.000f},
  {"pico-pixel",15.000f,0.000f,15.000f,0.000f,8.000f},
  {"pico-playable",19.000f,1.000f,-19.000f,32.000f,8.000f},
  {"pico-speaker",39.000f,6.000f,86.000f,8.000f,8.000f},
  {"pico",-23.000f,1.000f,23.000f,28.000f,8.000f},
  {"senpai-angry",0.000f,0.000f,0.000f,0.000f,8.000f},
  {"senpai",0.000f,0.000f,0.000f,0.000f,8.000f},
  {"spirit",31.000f,40.000f,-31.000f,-40.000f,8.000f},
  {"spooky-dark",0.000f,0.000f,0.000f,0.000f,8.000f},
  {"spooky",0.000f,0.000f,0.000f,0.000f,8.000f},
  {"sserafim-chaewon",0.000f,330.000f,0.000f,330.000f,8.000f},
  {"sserafim-eunchae",0.000f,260.000f,0.000f,260.000f,8.000f},
  {"sserafim-gf",0.000f,0.000f,0.000f,0.000f,8.000f},
  {"sserafim-kazuha",0.000f,200.000f,0.000f,200.000f,8.000f},
  {"sserafim-sakura",0.000f,0.000f,0.000f,0.000f,8.000f},
  {"sserafim-yunjin",2.000f,347.000f,2.000f,347.000f,8.000f},
  {"tankman-bloody",3.000f,-44.000f,-3.000f,32.000f,8.000f},
  {"tankman",3.000f,-44.000f,-3.000f,32.000f,8.000f},
};
#define BS_CHAR_COUNT 46

/* palcos: cameraZoom e posicoes dos tres slots */
typedef struct{const char*id;float zoom;float bf[2],gf[2],dad[2];}bs_stage_t;
static const bs_stage_t bs_stages[]={
  {"limoRide",0.9000f,{1217.50f,592.00f},{787.00f,779.00f},{329.50f,913.00f}},
  {"limoRideErect",0.9000f,{1217.50f,592.00f},{787.00f,779.00f},{329.50f,913.00f}},
  {"mainStage",1.1000f,{989.50f,885.00f},{751.50f,787.00f},{335.00f,885.00f}},
  {"mainStageErect",0.8500f,{977.50f,905.00f},{501.50f,815.00f},{40.00f,885.00f}},
  {"mallEvil",1.0000f,{1297.50f,871.00f},{808.50f,854.00f},{290.50f,869.00f}},
  {"mallXmas",0.8000f,{1177.50f,871.00f},{808.50f,854.00f},{42.00f,882.00f}},
  {"mallXmasErect",0.8000f,{1177.50f,871.00f},{808.50f,854.00f},{42.00f,882.00f}},
  {"phillyBlazin",0.7500f,{-237.00f,100.00f},{1353.00f,1125.00f},{-237.00f,150.00f}},
  {"phillyStreets",0.7700f,{2151.00f,1228.00f},{1453.00f,1100.00f},{920.00f,1310.00f}},
  {"phillyStreetsErect",0.7700f,{2151.00f,1228.00f},{1520.00f,1080.00f},{920.00f,1310.00f}},
  {"phillyTrain",1.1000f,{1020.50f,885.00f},{751.50f,787.00f},{468.00f,885.00f}},
  {"phillyTrainErect",1.1000f,{1020.50f,885.00f},{751.50f,787.00f},{468.00f,885.00f}},
  {"school",1.0000f,{1168.00f,900.00f},{724.00f,823.00f},{306.00f,960.00f}},
  {"schoolErect",1.0000f,{1168.00f,900.00f},{724.00f,763.00f},{306.00f,960.00f}},
  {"schoolEvil",1.0000f,{1168.00f,900.00f},{724.00f,823.00f},{0.00f,648.00f}},
  {"schoolEvilErect",1.0000f,{1168.00f,900.00f},{724.00f,823.00f},{0.00f,648.00f}},
  {"spookyMansion",1.0000f,{989.50f,885.00f},{751.50f,787.00f},{382.00f,865.00f}},
  {"spookyMansionErect",1.0000f,{1250.00f,885.00f},{821.50f,800.00f},{382.00f,865.00f}},
  {"sserafim",0.5000f,{1530.00f,970.00f},{938.50f,392.00f},{70.00f,470.00f}},
  {"tankmanBattlefield",0.9000f,{1090.50f,885.00f},{660.50f,730.00f},{230.50f,996.00f}},
  {"tankmanBattlefieldErect",0.7000f,{1340.50f,885.00f},{800.50f,775.00f},{230.50f,996.00f}},
};
#define BS_STAGE_COUNT 21

#endif
