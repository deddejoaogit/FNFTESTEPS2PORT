/* notekind_dados - GERADO por tools/gen_notekinds.py. NAO EDITE.
 *
 * Fonte lida do clone da arte: preload/scripts/notekinds/ e preload/scripts/characters/ (.hxc)
 *
 * Por que este dado existe: 34 kinds aparecem nos 81 charts e o port so'
 * implementava dois (`noanim`, `non_scoreable`). Os outros 32 estao nos
 * scripts do original, e o que o script FAZ pode ser lido: escolher a
 * animacao de canto com sufixo (`sing<DIR>-<sufixo>`) ou tocar uma
 * animacao nomeada. E' isso que esta' aqui -- nada de comportamento
 * condicional, que o dado marca com `condicional` e o jogo NAO adivinha.
 */
#ifndef FNF_NOTEKIND_DADOS_H
#define FNF_NOTEKIND_DADOS_H

#define NK_KIND_COUNT 35u

/* id -> nome de exibicao (o segundo argumento do `super` da classe). */
typedef struct {
 const char *id;
 const char *nome;
} nk_kind_t;

static const nk_kind_t nk_kinds[NK_KIND_COUNT]={
 {"censor","[UH-OH!] Censor Bar"},
 {"hehPrettyGood","Tankman Heh Pretty Good (Week 7)"},
 {"mom","Mom Sings (Week 5)"},
 {"ugh","Tankman Ugh (Week 7)"},
 {"weekend-1-blockhigh","Block High (Blazin')"},
 {"weekend-1-blocklow","Block Low (Blazin')"},
 {"weekend-1-blockspin","Block High (Spin) (Blazin')"},
 {"weekend-1-cockgun","Pico Cock Gun (2hot)"},
 {"weekend-1-darnelluppercut","Darnell Uppercut (Blazin')"},
 {"weekend-1-darnelluppercutprep","Darnell Uppercut (Prep) (Blazin')"},
 {"weekend-1-dodgehigh","Dodge High (Blazin')"},
 {"weekend-1-dodgelow","Dodge Low (Blazin')"},
 {"weekend-1-dodgespin","Dodge High (Spin) (Blazin')"},
 {"weekend-1-fakeout","Fakeout (Blazin')"},
 {"weekend-1-firegun","Pico Fire Gun (2hot)"},
 {"weekend-1-hithigh","Hit High (Blazin')"},
 {"weekend-1-hitlow","Hit Low (Blazin')"},
 {"weekend-1-hitspin","Hit High (Spin) (Blazin')"},
 {"weekend-1-idle","Idle (Blazin')"},
 {"weekend-1-kickcan","Darnell Kick Can (2hot)"},
 {"weekend-1-kneecan","Darnell Knee Can (2hot)"},
 {"weekend-1-lightcan","Darnell Light Can (2hot)"},
 {"weekend-1-picouppercut","Pico Uppercut (Blazin')"},
 {"weekend-1-picouppercutprep","Pico Uppercut (Prep) (Blazin')"},
 {"weekend-1-punchhigh","Punch High (Blazin')"},
 {"weekend-1-punchhighblocked","Punch High (Block) (Blazin')"},
 {"weekend-1-punchhighdodged","Punch High (Dodge) (Blazin')"},
 {"weekend-1-punchhighspin","Punch High (Spin) (Blazin')"},
 {"weekend-1-punchlow","Punch Low (Blazin')"},
 {"weekend-1-punchlowblocked","Punch Low (Block) (Blazin')"},
 {"weekend-1-punchlowdodged","Punch Low (Dodge) (Blazin')"},
 {"weekend-1-punchlowspin","Punch High (Spin) (Blazin')"},
 {"weekend-1-reversefakeout","Fakeout (Reverse) (Blazin')"},
 {"weekend-1-taunt","Taunt (If Fakeout) (Blazin')"},
 {"weekend-1-tauntforce","Taunt (Forced) (Blazin')"},
};

/* O que o kind faz no personagem: 0 = nada lido, 1 = canto com sufixo,
   2 = animacao nomeada. `ordem` (0 nada, 1 fundo, 2 frente) e `shake`
   (forca x1000, ms) vem do mesmo trecho do script. */
typedef struct {
 const char *kind;
 unsigned char tipo;
 unsigned char ordem;
 unsigned char condicional;
 const char *valor;   /* sufixo do canto, ou nome da animacao */
 const char *valor2;  /* a outra metade quando o nome ALTERNA (punchHigh1/2) */
 unsigned char alterna;
 int shake_forca1000;
 int shake_ms;
} nk_acao_t;

static const nk_acao_t nk_bf_christmas[]={
 {"censor",1,0,0,"censor","",0,0,0},
 {"hey",2,0,0,"hey","",0,0,0},
};
static const nk_acao_t nk_darnell[]={
 {"weekend-1-kickcan",2,0,0,"kickCan","",0,0,0},
 {"weekend-1-kneecan",2,0,0,"kneeCan","",0,0,0},
 {"weekend-1-lightcan",2,0,0,"lightCan","",0,0,0},
};
static const nk_acao_t nk_darnell_blazin[]={
 {"weekend-1-blockhigh",2,2,0,"punchHigh1","punchHigh2",1,0,0},
 {"weekend-1-blocklow",2,2,0,"punchLow1","punchLow2",1,0,0},
 {"weekend-1-blockspin",2,2,0,"punchHigh1","punchHigh2",1,0,0},
 {"weekend-1-darnelluppercut",2,2,0,"uppercut","",0,0,0},
 {"weekend-1-darnelluppercutprep",2,2,0,"uppercutPrep","",0,0,0},
 {"weekend-1-dodgehigh",2,2,0,"punchHigh1","punchHigh2",1,0,0},
 {"weekend-1-dodgelow",2,2,0,"punchLow1","punchLow2",1,0,0},
 {"weekend-1-dodgespin",2,2,0,"punchHigh1","punchHigh2",1,0,0},
 {"weekend-1-fakeout",2,1,0,"cringe","",0,0,0},
 {"weekend-1-hithigh",2,2,0,"punchHigh1","punchHigh2",1,0,0},
 {"weekend-1-hitlow",2,2,0,"punchLow1","punchLow2",1,0,0},
 {"weekend-1-hitspin",2,2,0,"punchHigh1","punchHigh2",1,0,0},
 {"weekend-1-idle",2,1,0,"idle","",0,0,0},
 {"weekend-1-picouppercut",2,1,0,"uppercutHit","",0,0,0},
 {"weekend-1-punchhigh",2,1,0,"hitHigh","",0,2,150},
 {"weekend-1-punchhighblocked",2,1,0,"block","",0,2,100},
 {"weekend-1-punchhighdodged",2,1,0,"dodge","",0,0,0},
 {"weekend-1-punchhighspin",2,1,0,"hitSpin","",0,2,150},
 {"weekend-1-punchlow",2,1,0,"hitLow","",0,2,150},
 {"weekend-1-punchlowblocked",2,1,0,"block","",0,2,100},
 {"weekend-1-punchlowdodged",2,1,0,"dodge","",0,0,0},
 {"weekend-1-punchlowspin",2,1,0,"hitSpin","",0,2,150},
 {"weekend-1-reversefakeout",2,1,0,"fakeout","",0,0,0},
 {"weekend-1-tauntforce",2,1,0,"pissed","",0,0,0},
};
static const nk_acao_t nk_parents_christmas[]={
 {"mom",1,0,0,"alt","",0,0,0},
};
static const nk_acao_t nk_pico_blazin[]={
 {"weekend-1-blockhigh",2,1,0,"block","",0,2,100},
 {"weekend-1-blocklow",2,1,0,"block","",0,2,100},
 {"weekend-1-blockspin",2,1,0,"block","",0,2,100},
 {"weekend-1-darnelluppercut",2,1,0,"uppercutHit","",0,5,250},
 {"weekend-1-darnelluppercutprep",2,1,0,"idle","",0,0,0},
 {"weekend-1-dodgehigh",2,1,0,"dodge","",0,0,0},
 {"weekend-1-dodgelow",2,1,0,"dodge","",0,0,0},
 {"weekend-1-dodgespin",2,1,0,"dodge","",0,0,0},
 {"weekend-1-fakeout",2,1,0,"fakeout","",0,0,0},
 {"weekend-1-hithigh",2,1,0,"hitHigh","",0,2,150},
 {"weekend-1-hitlow",2,1,0,"hitLow","",0,2,150},
 {"weekend-1-hitspin",2,1,0,"hitSpin","",0,2,150},
 {"weekend-1-idle",2,1,0,"idle","",0,0,0},
 {"weekend-1-picouppercut",2,2,0,"uppercut","",0,5,250},
 {"weekend-1-picouppercutprep",2,2,0,"uppercutPrep","",0,0,0},
 {"weekend-1-punchhigh",2,2,0,"punchHigh1","punchHigh2",1,0,0},
 {"weekend-1-punchhighblocked",2,2,0,"punchHigh1","punchHigh2",1,0,0},
 {"weekend-1-punchhighdodged",2,2,0,"punchHigh1","punchHigh2",1,0,0},
 {"weekend-1-punchhighspin",2,2,0,"punchHigh1","punchHigh2",1,0,0},
 {"weekend-1-punchlow",2,2,0,"punchLow1","punchLow2",1,0,0},
 {"weekend-1-punchlowblocked",2,2,0,"punchLow1","punchLow2",1,0,0},
 {"weekend-1-punchlowdodged",2,2,0,"punchLow1","punchLow2",1,0,0},
 {"weekend-1-punchlowspin",2,2,0,"punchLow1","punchLow2",1,0,0},
 {"weekend-1-reversefakeout",2,1,0,"idle","",0,0,0},
 {"weekend-1-tauntforce",2,1,0,"taunt","",0,0,0},
};
static const nk_acao_t nk_pico_playable[]={
 {"censor",1,0,0,"censor","",0,0,0},
 {"cheer",2,0,0,"cheer","",0,0,0},
 {"hey",2,0,0,"hey","",0,0,0},
 {"weekend-1-cockgun",2,0,0,"cock","",0,0,0},
 {"weekend-1-firegun",2,0,0,"shoot","",0,0,0},
};
static const nk_acao_t nk_spooky_dark[]={
 {"cheer",2,0,0,"cheer","",0,0,0},
};
static const nk_acao_t nk_sserafim_sakura[]={
 {"sakura-bf1",1,0,0,"bf1","",0,0,0},
 {"sakura-bf2",1,0,0,"bf2","",0,0,0},
 {"sakura-joint",1,0,0,"joint","",0,0,0},
};
static const nk_acao_t nk_tankman[]={
 {"hehPrettyGood",2,0,0,"hehPrettyGood","",0,0,0},
 {"ugh",2,0,0,"ugh","",0,0,0},
};
static const nk_acao_t nk_tankman_bloody[]={
 {"hehPrettyGood",2,0,0,"hehPrettyGood","",0,0,0},
 {"ugh",2,0,0,"ugh","",0,0,0},
};

#define NK_CHAR_COUNT 10u
typedef struct {
 const char *personagem;
 const nk_acao_t *acoes;
 unsigned int n;
} nk_char_t;

static const nk_char_t nk_chars[NK_CHAR_COUNT]={
 {"bf-christmas",nk_bf_christmas,2u},
 {"darnell",nk_darnell,3u},
 {"darnell-blazin",nk_darnell_blazin,24u},
 {"parents-christmas",nk_parents_christmas,1u},
 {"pico-blazin",nk_pico_blazin,25u},
 {"pico-playable",nk_pico_playable,5u},
 {"spooky-dark",nk_spooky_dark,1u},
 {"sserafim-sakura",nk_sserafim_sakura,3u},
 {"tankman",nk_tankman,2u},
 {"tankman-bloody",nk_tankman_bloody,2u},
};

#endif /* FNF_NOTEKIND_DADOS_H */
