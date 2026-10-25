/* GERADO POR tools/tabelas_oficiais.py - NAO EDITAR A MAO.
   Fonte: funkin.assets @ 9300719cb261d23f72344807055be6d109c9949c (tag v0.8.6)
   Cancao: tutorial   Palco: mainStage */
#ifndef OFFICIAL_TABLES_H
#define OFFICIAL_TABLES_H

#define OFF_CAMERA_ZOOM 1.100000f
#define OFF_BPM 100.000000f

/* --- props do palco --- */
#define OFF_PROP_STAGEBACK_X -600.000000f
#define OFF_PROP_STAGEBACK_Y -200.000000f
#define OFF_PROP_STAGEBACK_SCALE_X 1.000000f
#define OFF_PROP_STAGEBACK_SCALE_Y 1.000000f
#define OFF_PROP_STAGEBACK_SCROLL_X 0.900000f
#define OFF_PROP_STAGEBACK_SCROLL_Y 0.900000f
#define OFF_PROP_STAGEBACK_Z 10
#define OFF_PROP_STAGEFRONT_X -650.000000f
#define OFF_PROP_STAGEFRONT_Y 600.000000f
#define OFF_PROP_STAGEFRONT_SCALE_X 1.100000f
#define OFF_PROP_STAGEFRONT_SCALE_Y 1.100000f
#define OFF_PROP_STAGEFRONT_SCROLL_X 0.900000f
#define OFF_PROP_STAGEFRONT_SCROLL_Y 0.900000f
#define OFF_PROP_STAGEFRONT_Z 20
#define OFF_PROP_STAGECURTAINS_X -500.000000f
#define OFF_PROP_STAGECURTAINS_Y -300.000000f
#define OFF_PROP_STAGECURTAINS_SCALE_X 0.900000f
#define OFF_PROP_STAGECURTAINS_SCALE_Y 0.900000f
#define OFF_PROP_STAGECURTAINS_SCROLL_X 1.300000f
#define OFF_PROP_STAGECURTAINS_SCROLL_Y 1.300000f
#define OFF_PROP_STAGECURTAINS_Z 30

/* slot bf ocupado por 'bf' (multianimateatlas) */
#define OFF_BF_FOCUS_X 888.500000f
#define OFF_BF_FOCUS_Y 589.540000f
#define OFF_BF_CORNER_X 758.040000f
#define OFF_BF_CORNER_Y 408.080000f
#define OFF_BF_W 496.920000f /* DERIVADO: caixa de uniao dos 43 quadros que o pack
                    do port contem (Idle 14 + Left 8 + Right 7 + Up 7 +
                    Down 7), medida no Animate Atlas oficial
                    shared/images/characters/bf por render_atlas.py.
                    O 499x492 antigo errava por 2,08 e 1,08. */
#define OFF_BF_H 490.920000f

/* slot dad ocupado por 'gf' (animateatlas) */
#define OFF_DAD_FOCUS_X 485.000000f
#define OFF_DAD_FOCUS_Y 459.575000f
#define OFF_DAD_CORNER_X -28.650000f
#define OFF_DAD_CORNER_Y 214.150000f
#define OFF_DAD_W 703.300000f /* DERIVADO: caixa de uniao dos 87 quadros do Animate
                    Atlas oficial (shared/images/characters/gf), medida
                    por tools/render_atlas.py. x -149,400..553,900 e
                    y -333,650..341,200. O 704x676 antigo era este
                    mesmo numero arredondado para cima. */
#define OFF_DAD_H 674.850000f

/* slot gf: vazio nesta cancao */

#endif
