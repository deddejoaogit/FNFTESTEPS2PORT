#include <delaythread.h>
#include <dma.h>
#include <draw.h>
#include <draw_tests.h>
#include <draw_primitives.h>
#include <draw_sampling.h>
#include <graph.h>
#include <gs_psm.h>
#include <iopcontrol.h>
#include <kernel.h>
#include <libpad.h>
#include <loadfile.h>
#include <packet.h>
#include <sifrpc.h>
#include <timer.h>
#include <audsrv.h>
#include <gif_tags.h>
#include <gs_gp.h>
#include <math.h>
#include "telemetry.h"
#include "audio_ring.h"
#include "menu_generated.h"
#include "story_generated.h"
#include "story_titles_generated.h"
#include "chart.h"
#include "chart_events.h"
#include "engine/gameplay.h"
#include "engine/vram_layout.h"
#include "engine/stage_pool.h"
#include "engine/audio_mix.h"
#include "engine/pack_stream.h"
#include "engine/shake.h"
#include "engine/stage_layout.h"
#include "engine/lz4_bloco.h"
#include "engine/char_pack.h"
#include "engine/char_cena.h"
#include "engine/char_layout.h"
#include "engine/notekind.h"
#include "engine/health_icon.h"
#include "engine/splash.h"
#include "engine/resultado.h"
#include "engine/resultado_rank.h"
#include "engine/freeplay.h"
#include "engine/pack_multi.h"
#include "engine/freeplay_dj_dados.h"
#include "gameover_dados.h"
#include "song_registry.h"
#include "bs_dados.h"    /* bs_stages[]: o cameraZoom de cada palco */
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#ifndef FNF_PS2_VERSION
#define FNF_PS2_VERSION "0.2.35-official-element-geometry"
#endif
#define SCREEN_W 640
#define SCREEN_H 448
/* Formato do FRAMEBUFFER (nao das texturas).
 *
 * O port desenhava em PSMCT32: 32 bits por pixel nos dois buffers de quadro,
 * 286.720 palavras cada, 573.440 no par. Em PSMCT16 (A1B5G5R5) cada buffer cai
 * para 143.360 palavras e o par para 286.720 -- 1.146.880 bytes de VRAM de
 * volta, que e' o orcamento que falta para os palcos e personagens das 27
 * musicas.
 *
 * O que se perde: cor de 8 para 5 bits por canal e o canal alfa do framebuffer
 * de 8 para 1 bit. O segundo nao importa (o buffer nunca e' composto sobre
 * nada); o primeiro e' dithering, ligado no inicio de cada quadro. As
 * primitivas continuam sendo escritas em 32 bits: quem converte e' o GS, com
 * o FBA ja' configurado pelo draw_setup_environment do ps2sdk (ele escolhe
 * ALPHA_CORRECT_RGBA16 sozinho quando ve' framebuffer de 16 bits).
 *
 * A resolucao NAO muda aqui: 640x448 continua igual, e portanto nenhuma
 * coordenada de nada precisa mudar. Encostar na resolucao e' outro passo, com
 * outro motivo (preenchimento), e nao se misturam dois motivos num build. */
#define FB_PSM GS_PSM_16
#define PAD_PORT 0
#define PAD_SLOT 0
#define MAX_ANIMS 8
#define ANIM_RECORD_BYTES 32

typedef struct { char name[17]; unsigned short start,count,loop,flags; int origin_x,origin_y; } bf_anim_t;
typedef struct { unsigned short width,height,anim_count,frame_count,fps; bf_anim_t anim[MAX_ANIMS]; } bf_meta_t;
static bf_meta_t g_bf,g_gf;
static unsigned char *g_pixels;
static unsigned char *g_gf_pixels;
static unsigned char *g_title_anim_pixels;
static volatile int g_title_anim_status=0;
static int g_title_anim_thread_id=-1,g_title_anim_ready_logged=0;
static volatile u32 g_title_logo_ready=0,g_title_gf_ready=0;
static u32 g_title_anim_heard_samples=0,g_title_anim_last_submitted=0;
static volatile int g_audio_ready=0;
static unsigned char g_title_anim_thread_stack[16384] __attribute__((aligned(128)));
extern void *_gp;
static int g_tutorial_scene_ready=0,g_tutorial_scene_attempted=0,g_tutorial_scene_status=0;
/* Camera do Tutorial. Evento tipo 1 do chart = FocusCamera.
   FocusCameraSongEvent.hx: char 0 = BF, 1 = oponente (no Tutorial, a GF).
   PlayState.resetCamera: FlxG.camera.follow(cameraFollowPoint, LOCKON,
   Constants.DEFAULT_CAMERA_FOLLOW_RATE = 0.04). FlxCamera.updateFollow usa
   adjustedLerp = 1 - (1-followLerp)^(elapsed*60). Em NTSC elapsed*60 =
   60/(60000/1001) = 1.001, logo 1 - 0.96^1.001 = 0.040042306. */
/* Janela de mundo coberta por mainstage_tutorial_parallax.rgba (512x256).
   Composta da arte oficial stageback/stagefront/stagecurtains com as
   posicoes e escalas de mainStage.json. Cobre toda a viagem da camera
   entre o foco do BF (888,50;589,00) e o da GF (485,00;459,00). */
#define STAGE_SCROLL 0.9f
#define STAGE_WIN_X (88.3500f)
#define STAGE_WIN_Y (78.9000f)
#define STAGE_WIN_W (1427.3000f)
#define STAGE_WIN_H (859.2000f)
#define GF_OFFSET_X (-12.0f)
#define GF_OFFSET_Y (4.0f)
#define GF_CAMOFF_X (12.0f)
#define GF_CAMOFF_Y (8.0f)
#define TUT_CAM_LERP 0.040042306f
/* ZoomCameraSongEvent: DEFAULT_MODE e 'direct', e os eventos do chart
   nao trazem "mode", logo targetZoom = zoom * FlxCamera.defaultZoom =
   zoom absoluto. Nao se multiplica pelo cameraZoom 1,1 do palco.
   duration 4 passos * stepLengthMs 150 (100 BPM) / 1000 = 0,600 s.
   ease elasticInOut. Antes do primeiro evento vale o 1,1 do palco. */
#define TUT_DT 0.016683333f
/* g_tut_zoom mirrors chart_events_state()->zoom so the draw code below reads a
   plain float. The tween itself, its duration and its ease all live in
   chart_events.c, where they come from the event rather than from here. */
static float g_tut_zoom=1.1f;
/* camHUD.zoom (PlayState.hx L1227 e L1846-1859), lido do chart_events a cada
   quadro. TODO o HUD do original esta' nesta camera -- notas e receptores
   (L2257-2264), barra de vida e placar (L2024-2026), icones (L2160/L2195) e
   ate' a marca de versao (L959-960) --, e o zoom dela e' aplicado em torno do
   CENTRO da tela do jogo: no FlxCamera, `screen = (world - scroll - size/2) *
   zoom + size/2`, com scroll 0 no HUD. Em 640x360 isso da' centro (320,180).
   Antes o HUD nao piscava: o valor era zero no port. */
static float g_hud_zoom=1.0f;
static float g_hud_zoom_max=1.0f;   /* maior valor visto, para a telemetria */
static int g_hud_zoom_visto=0;      /* 1 depois do primeiro pulso medido    */
static int g_hud_zoom_forte=0;      /* 1 depois do primeiro pulso > 1,05    */
static float hz_x(float x){return 320.0f+(x-320.0f)*g_hud_zoom;}
static float hz_y(float y){return 180.0f+(y-180.0f)*g_hud_zoom;}
static float hz_s(float v){return v*g_hud_zoom;}
/* Os dois numeros com que a musica COMECA, tirados do dado dela: o zoom do
   palco e o scrollSpeed do chart daquela dificuldade. Ficam aqui porque o macro
   de reset (FNF_RESET_PLAY, mais abaixo) e' o lugar que aplica os dois. */
static float g_play_zoom=1.1f;
static float g_play_scroll=1.0f;

/* Zoom inicial da musica: o `cameraZoom` do PALCO (corpo mais abaixo, onde
   bs_dados e o zoom do mainStage ja' existem). */
static float cena_zoom_do_palco(const sr_song_t *song);

/* ScrollSpeedEvent.hx. O evento do Tutorial aos 60.000 ms e
     {absolute:false, duration:4, ease:'expoIn', scroll:1.2, strumline:'both'}
   Como absolute e falso, scroll = 1.2 * chart.scrollSpeed, e o
   tutorial-chart.json traz scrollSpeed {easy:1, normal:1, hard:1}, logo o
   alvo e 1,2 mesmo. duration 4 passos * stepLengthMs 150 = 0,600 s.
   FlxEase.expoIn(t) = (t==0) ? 0 : 2^(10*(t-1)). */
static float g_tut_scroll=1.0f;   /* mirrors chart_events_state()->scroll */
/* ANIMACOES DE CANTO.
   Os quadros JA ESTAVAM nos packs e nunca eram usados: o port tocava so
   af%14 do BF e af%30 da GF, deixando 29 e 57 quadros parados. As faixas
   abaixo foram descobertas por tools/identificar_pack.py, que decodifica
   o pack e casa cada quadro com o Animate Atlas oficial por miniatura de
   luminancia. A ferramenta foi validada contra o pack da GF, que eu mesmo
   gerei: reproduziu a origem -149,400 / -333,650 e as cinco faixas com
   similaridade 0,999.
   Ordem de pista oficial, NoteDirection.hx: LEFT 0, DOWN 1, UP 2, RIGHT 3.
     BF 43 quadros: Idle 0..13, Left 14..21, Down 22..28, Up 29..35, Right 36..42
     GF 87 quadros: Idle 0..29, Up 30..36, Down 37..56, Left 57..71, Right 72..86
   Duracao da pose, BaseCharacter.hx:433-445:
     singTimeSec = singTime * stepLengthMs / 1000
   CharacterData.hx:451 da DEFAULT_SINGTIME 8.0 e o Tutorial tem
   stepLengthMs 150 (100 BPM), logo 8,0*150/1000 = 1,200 s. A animacao
   roda a 24 fps e segura o ultimo quadro ate esse tempo acabar. O numero
   (1.200 ms) e' so' o do Tutorial: quem vale para as 27 musicas e' o
   `cena_canto_ms`, que le' singTime e bpm do dado. */
/* BOP DE CAMERA. PlayState.hx:1846-1859 e :1217-1227.
     Constants.DEFAULT_BOP_INTENSITY 1.015
     Constants.DEFAULT_ZOOM_RATE 4 batidas, STEPS_PER_BEAT 4 -> a cada 16 passos
     Constants.DEFAULT_ZOOM_OFFSET 0
   No passo que satisfaz (currentStep + offset*4) % (rate*4) == 0:
     cameraBopMultiplier = cameraBopIntensity
   E a cada quadro:
     decayRate 0.95, dt = elapsed*60
     cameraBopMultiplier = FlxMath.lerp(1.0, cameraBopMultiplier, 0.95^dt)
     zoomPlusBop = currentCameraZoom * cameraBopMultiplier
   FlxMath.lerp(a,b,t) = a + (b-a)*t, logo bop = 1 + (bop-1)*0.95^dt.
   Com TUT_DT 0,016683333 s, dt = 1,0010 e 0,95^dt = 0,949951.
   Passo em amostras: stepLengthMs 150 * 48000 / 1000 = 7200.
   Nao implementei o teto MAX_RELATIVE_CAM_ZOOM 1.35 porque ele vigia o
   zoom da camera do HUD, que o port ainda nao tem separada. */
#define TUT_SAMPLES_PER_STEP 7200u
/* The bop's intensity, rate and offset are SetCameraBop's to set, and its decay
   is PlayState.hx:1217's 0.95^(dt*60). All of it now lives in chart_events.c;
   g_tut_bop mirrors the multiplier so the draw code reads a plain float. */
static float g_tut_bop=1.0f;
/* As faixas fixas de quadro do Tutorial (`BF_SING`/`GF_SING`) e o
   `tut_anim_idx` sairam com a ultima carga de quadro do caminho antigo: quem
   escolhe o quadro agora e' `cc_quadro`, a partir da tabela que viaja dentro do
   pacote CHP4. `g_bf_sing`/`g_gf_sing` continuam porque alimentam o estado das
   notas (e o log), nao o desenho. */
static int g_bf_sing=-1,g_gf_sing=-1;static u32 g_bf_sing_at=0,g_gf_sing_at=0;
static int g_tut_cam_focus=0,g_tut_cam_ready=0;static float g_tut_cam_x=0.0f,g_tut_cam_y=0.0f;
/* Papel que a camera segue. O original comeca seguindo o OPONENTE
   (PlayState.hx L2225-2227: `cameraFollowPoint.setPosition(dad.cameraFocusPoint)`
   ao adicionar o `dad`) e so' muda por evento FocusCamera. Aqui o valor inicial
   continua o do BF, que e' o que a captura de console do Tutorial conferiu --
   mudar o inicial e' outra causa, e fica para um build proprio.
   Sem cena generica (nenhum pacote), o alvo cai nas constantes do Tutorial. */
static int g_cam_papel=0;   /* CH_PAPEL_BF */
static float dad_cam_x=0.0f,dad_cam_y=0.0f;
/* Tween do FocusCamera (FocusCameraSongEvent.hx L138-160). CLASSIC so' move o
   ponto de foco e deixa a perseguicao levar a camera; INSTANT poe a camera no
   ponto na hora; qualquer outra ease interpola a POSICAO da camera no tempo do
   evento. Sem estes tres campos, um evento com `ease` era aplicado como se
   fosse CLASSIC -- 46 dos 664 eventos dos 81 charts. */
static int   g_cam_seq=-1;
static float g_cam_from_x=0.0f,g_cam_from_y=0.0f,g_cam_to_x=0.0f,g_cam_to_y=0.0f;
static float g_cam_t=1.0f,g_cam_dur=0.0f;
static unsigned g_cam_ease=0u,g_cam_dir=0u;
/* Quantos eventos FocusCamera de cada modo chegaram nesta musica: e' a prova
   de que os tres caminhos existem e sao usados (o log diz qual deles tocou). */
static int g_cam_modo_visto[3]={0,0,0};
static int g_tutorial_bf_w=0,g_tutorial_bf_h=0,g_tutorial_gf_w=0,g_tutorial_gf_h=0;
static unsigned char *g_tutorial_bf_pack=0,*g_tutorial_gf_pack=0;static u32 g_tutorial_bf_pack_bytes=0,g_tutorial_gf_pack_bytes=0;static int g_tutorial_bf_uploaded=-1,g_tutorial_gf_uploaded=-1;
static const unsigned char *g_story_bf_pack=0,*g_story_gf_pack=0;static u32 g_story_bf_pack_bytes=0,g_story_gf_pack_bytes=0;static int g_story_bf_uploaded=-1,g_story_gf_uploaded=-1,g_story_anim_ready=0;static u32 g_story_anim_heard_samples=0;
static unsigned char *g_story_bf_pixels=0,*g_story_gf_pixels=0;
extern const unsigned char g_story_gf_pack_start[],g_story_gf_pack_end[],g_story_bf_pack_start[],g_story_bf_pack_end[];
static int g_tutorial_audio_status=-1,g_tutorial_audio_finished=0,g_chart_status=-1;
/* Diagnostico do transporte de audio do tutorial, sem mudar o comportamento
   normal: quantos blocos vieram curtos e quantos quadros seguidos falharam. */
static u32 g_tut_partial_blocks=0,g_tut_audio_fail_streak=0,g_tut_audio_given_up=0;
/* Qual musica e' a da vez. Antes o estado de jogo tinha "tutorial" escrito no
   codigo (sr_song_by_id("tutorial")) e um unico arquivo de audio
   (assets/audio_stems.pcm), entao so' o Tutorial podia tocar. Agora a musica
   vem do registro a partir do nivel do Story Menu, e o audio e' o daquela
   musica. */
/* GANCHO DO HARNESS (e SO' dele).
   O define PS2H so' existe quando o jogo e' compilado como harness no PC
   (tools/run_harness*.sh). No console ele nao existe, nada disto entra no
   binario, e o caminho do jogador continua sendo o menu.
   Serve para o teste conseguir comecar QUALQUER uma das 27 musicas sem
   atravessar a Freeplay: a Freeplay do port so' toca a primeira da lista, e
   as outras 26 nunca tinham sido iniciadas no PC. */
#ifdef PS2H
const char *ps2h_musica_inicial(void);
int ps2h_dificuldade_inicial(void);
int ps2h_semana_inicial(void);
int ps2h_rank_forcado(void);
#endif
static const sr_song_t *g_play_song=0;
static unsigned g_play_level=0,g_play_song_index=0;
/* CAMPANHA DO STORY MODE (PlayStatePlaylist.hx).
   O original NAO toca uma musica por semana: `selectLevel()` monta
   playlistSongIds = allSongs da semana, toca a primeira, e cada fim de musica
   faz shift() e carrega a proxima SEM passar pelo resultado -- o resultado so'
   aparece quando a lista esvazia (PlayState.hx L3560-3660). O placar exibido no
   fim e' o da campanha (L3885: isStoryMode ? campaignScore : songScore).
   No port cada semana tocava so' a musica 0 e voltava ao menu: 20 das 27
   musicas existiam e nunca eram alcancadas pelo Story Mode. */
static int g_story_mode=0;        /* 1 = a musica atual faz parte de uma campanha */
static int g_story_week=0;        /* a semana da campanha (o Story Menu pode estar
                                     em outra quando a musica comeca: o
                                     `story_level` do menu e' sobrescrito) */
static int g_playlist_n=0;        /* musicas da semana */
static int g_playlist_i=0;        /* qual delas esta' tocando */
static long g_campaign_score=0;   /* soma dos placares das musicas ja' tocadas */
/* OS TALLIES DA SEMANA. No Story Mode o ResultState NAO usa os tallies da
   musica: usa `Highscore.talliesLevel` (`PlayState.hx` L3873: `talliesToUse =
   isStoryMode ? talliesLevel : tallies`), que soma todas as musicas jogadas na
   semana. O port somava so' o placar; agora soma os julgamentos e o combo
   tambem, senao a tela de resultado do Story mostraria os numeros da ULTIMA
   musica com o placar da semana. */
static int g_camp_sick=0,g_camp_good=0,g_camp_bad=0,g_camp_shit=0;
static int g_camp_missed=0,g_camp_maxcombo=0,g_camp_hits=0;
/* Total de notas da semana: o denominador do percentual de conclusao no Story
   Mode (`Scoring.tallyCompletion` divide por `totalNotes`). */
static int g_camp_total_notas=0;
static void campanha_zerar(void){g_campaign_score=0;g_camp_sick=0;g_camp_good=0;
 g_camp_bad=0;g_camp_shit=0;g_camp_missed=0;g_camp_maxcombo=0;g_camp_hits=0;
 g_camp_total_notas=0;}
static void campanha_somar(const void *play_v){
 const fnf_play_t *p=(const fnf_play_t*)play_v;
 g_campaign_score+=p->score;
 g_camp_sick+=p->sick;g_camp_good+=p->good;g_camp_bad+=p->bad;g_camp_shit+=p->shit;
 g_camp_missed+=p->missed;g_camp_hits+=p->sick+p->good+p->bad+p->shit;
 g_camp_total_notas+=p->total_notes;
 if(p->max_combo>g_camp_maxcombo)g_camp_maxcombo=p->max_combo;}
/* Passo do chart em amostras, da secao de BPM vigente. 0 = nenhum chart. */
static u32 g_play_samples_per_step=0;
/* Tela de resultado (ui_state 9). O original vai para ResultState quando a
   musica termina; aqui os numeros vem do mesmo nucleo que pontua durante o
   jogo (engine/gameplay.h), nao de contadores paralelos. */
static int g_result_wait=0;
/* ARTE DA TELA DE RESULTADO (ui_state 9). O estado/tempo vem de
   engine/resultado.c; aqui ficam o pacote aberto, os enderecos de VRAM pedidos
   ao pool (o palco ja' foi solto nessa tela) e o instante em que a tela abriu. */
static res_t g_res;
static int g_res_ok=0;              /* folhas abertas (0..RES_FOLHAS)          */
static int g_res_bg_ok=0;           /* o fundo (agora 8 faixas, sem textura)   */
/* A MAQUINA (`soundSystem`): um pacote CFZ4 e UM quadro na VRAM. */
static pst_pack_t g_res_ss_pack;
static int g_res_ss_ok=0,g_res_ss_tentou=0,g_res_ss_frame=-1,g_res_ss_logado=0;
static unsigned int g_res_ss_addr=0;
static unsigned int g_res_addr[RES_FOLHAS];
static unsigned int g_res_frame[RES_FOLHAS];
static unsigned int g_res_bg_addr=0;
static unsigned char *g_res_pixels=0;   /* um quadro descomprimido (maior)      */
static unsigned int g_res_pixels_bytes=0;
static u64 g_res_t0=0;              /* GetTimerSystemTime no instante da abertura */
static int g_res_ms=0;              /* ms desde a abertura                     */
static int g_res_logado=0;
static int g_res_desenhadas=-1;   /* folhas desenhadas no ultimo quadro (telemetria) */
/* ARTE DO RANK: as duas imagens (vertical e horizontal) do rank conquistado.
   So' um par e' carregado por partida -- o maior custa 76.736 B de VRAM. */
static int g_res_rank_ok=0;
static unsigned int g_res_rank_vert_addr=0,g_res_rank_hor_addr=0;
static int g_res_rank_logado=0;
/* A ANIMACAO DO PERSONAGEM POR RANK (`players/<dono>.json` -> `results[rank]`).
   Duas entradas no maximo, cada uma um pacote CFZ4 lido quadro a quadro: na RAM
   fica a tabela (~8 B por quadro) e na VRAM UM quadro por entrada. O conjunto
   cabe nos 350.208 B que a tela tem livres (o pior rank medido: 308.320 B). */
static res_rr_t g_res_rrf;
static unsigned int g_res_rrf_addr[RES_RR_MAX_ENTRADAS];
static unsigned int g_res_rrf_frame[RES_RR_MAX_ENTRADAS];
static unsigned char g_res_rrf_vram[RES_RR_MAX_ENTRADAS];  /* 1 = alocou      */
static unsigned char *g_res_rrf_pixels=0;   /* UM quadro descomprimido       */
static unsigned int g_res_rrf_pixels_bytes=0;
static int g_res_rrf_ok=0;                  /* entradas com VRAM + pacote    */
static int g_res_rrf_rank=-1;
static int g_res_rrf_desenho=-1;            /* mascara do ultimo conjunto    */
static int g_res_rrf_logado=0;
static int g_res_rrf_visto=0;              /* mascara: entrada ja' apareceu  */
static int g_res_hs_logado=0;              /* o timer do selo de recorde     */
static unsigned int g_res_rrf_ram=0;
/* O PLACAR: os 52 quadros dos dez digitos num UNICO atlas (os dez aparecem ao
   mesmo tempo e durante o shuffle qualquer valor pode estar em qualquer
   posicao, entao o conjunto inteiro mora na VRAM: 508.480 B). */
static int g_res_score_ok=0;
static unsigned int g_res_score_addr=0;
/* OS CONTADORES DE JULGAMENTO: dez glifos (96x44 = 16.896 B) e sete
   contadores. `g_res_tally_logado[i]` marca o contador que ja' foi reportado
   depois de assentar (telemetria). */
static int g_res_tally_ok=0;
static unsigned int g_res_tally_addr=0;
static unsigned char g_res_tally_logado[RES_TALLIES];
static int g_res_tally_desenho=-1;
/* O PERCENTUAL DE CONCLUSAO: tres atlas de digitos (Left, Right e o Small da
   barra do topo) e as duas imagens de texto. 129.452 B no total, com os glifos
   grandes guardados a 0,4 do original (desenhados a 1,25x). */
static int g_res_cp_ok=0;
static unsigned int g_res_cp_addr[RES_CP_ATLASES];
static unsigned int g_res_cp_texto_addr=0,g_res_cp_texto_small_addr=0;
static int g_res_cp_logado=0,g_res_cp_final=0;
static int g_res_cp_small_logado=0;   /* ja' reportou o contador pequeno do topo */

/* A BARRA DO TOPO E A DIFICULDADE. A barra nao tem textura (trapezio de cor
   solida: 0 B de VRAM); a imagem da dificuldade e' UM dos cinco `diff_*.rgba`
   -- cada partida carrega so' o da dificuldade jogada. */
static int g_res_dif_ok=0;
static unsigned int g_res_dif_addr=0;
static int g_res_dif_ind=-1;             /* indice em g_res_dif (0 = easy) */
static float g_res_dif_h=0.0f;
static int g_res_barra_logado=0;
/* `bgFlash` acende duas vezes; um log por janela (0 = na contagem do
   percentual, 1 = no getFlashDelay do rank). */
static int g_res_flash_logado[2]={0,0};
/* O NOME DA MUSICA: um bitmap por titulo (`FlxBitmapText` pre-renderizado) e o
   estado do loop de `timerThenSongName()` (entrada, espera e recirculacao). */
static int g_res_nome_ok=0;
static unsigned int g_res_nome_addr=0;
static res_nome_t g_res_nome_estado;
static int g_res_nome_visto=0;      /* ja' reportou o primeiro desenho */
static int g_res_nome_ciclos=0;
static int g_res_nome_pronto=0;   /* `res_nome_iniciar` ja' rodou */
static int g_res_rank_forcado_logado=0;
static int g_res_nome_moveu=0;    /* ja' reportou o primeiro passo do scroll */
/* O que o SCROLL do grupo somou: o mesmo `speedOfTween * 60 * elapsed` vai para
   o nome, a dificuldade e o percentual pequeno (`update()` L806-826), entao o
   deslocamento e' um so' e `g_res_t_rel` e' o tempo desde o inicio do ciclo
   (os tres reentram juntos no recomeco). */
static float g_res_off_x=0.0f;
static float g_res_off_y=0.0f;
static unsigned long g_res_t_rel=0ul;

static int g_res_score_logado=0;   /* ja' reportou o instante em que apareceu */
static int g_res_score_final=0;    /* ja' reportou o numero assentado        */

/* Carrega a ARTE da tela de resultado: abre os CFZ4 das tres folhas, pede ao
   pool a VRAM de um quadro de cada e le' o gradiente do fundo. Chamado UMA vez
   por entrada na tela (ui_state 9), quando o palco ja' foi solto -- medido no
   log do harness: 1.269.760 B livres no pool nessa hora, contra 480.968 B que
   a tela precisa (um quadro de cada folha + o gradiente).
   Nada aqui e' obrigatorio: sem uma folha, a tela desenha o texto de sempre. */
/* Quantos quads texturizados o bloco da tela de resultado emitiu na primeira
   vez que desenhou. E' a prova de que a tela nao ficou em branco: o harness le
   o valor e exige que seja maior que zero. Sem isso, "ui_state 9 apareceu no
   log" so' provaria que o estado existe, nao que alguma coisa foi desenhada. */
static int g_result_drawn=0;
/* CONTAGEM REGRESSIVA (Countdown.hx).
   g_countdown_left: amostras que faltam para ela acabar; 0 = musica liberada.
   g_countdown_step: 0=BEFORE, 1="3", 2="2", 3="1", 4="GO!", 5=AFTER -- os
   mesmos seis passos do enum CountdownStep do original.
   g_countdown_beat: uma batida em amostras (4 passos do chart), que e' o
   intervalo do FlxTimer da contagem (Countdown.performCountdown).
   g_chart_clock: relogio do CHART, que e' NEGATIVO durante a contagem. O
   original ja' move o Conductor para -5 batidas antes de a musica comecar. */
static s32 g_countdown_left=0,g_countdown_step=0,g_countdown_beat=0,g_chart_clock=0;
/* GAME OVER (ui_state 10, GameOverSubState.hx).
   g_gameover_frames conta os quadros desde a morte: o original so' aceita
   entrada 1 s depois de abrir (`new FlxTimer().start(1, ... canInput = true)`,
   GameOverSubState.hx:186-189), para um aperto nervoso nao pular a tela.
   g_gameover_drawn serve de prova, como g_result_drawn: "o estado existe" nao
   e' o mesmo que "desenhou alguma coisa". */
static int g_gameover_frames=0,g_gameover_drawn=0;
/* CAMERA E ANIMACAO DA TELA DE GAME OVER (GameOverSubState.hx).
   O original NAO congela a cena: ao morrer, o BF sai do palco e entra no
   substate na MESMA posicao (`boyfriend.resetCharacter(false)`), a camera
   passa a seguir um ponto que ele calcula em setCameraTarget() (L196-222) e o
   zoom persegue `stage.camZoom * death.cameraZoom` -- os dois com
   suavizacao. Antes o port so' desenhava preto e texto: metade dos eventos
   dessa tela (a camera indo para o BF, o zoom abrindo ou fechando, o BF
   morrendo) nao existia.

   Constantes, todas de linha do original:
   - GO_CAM_LERP: FlxG.camera.follow(..., LOCKON, DEFAULT_CAMERA_FOLLOW_RATE/2)
     (L220) com DEFAULT_CAMERA_FOLLOW_RATE = 0.04 -> 0.02, e o
     `adjustedLerp = 1 - (1-followLerp)^(elapsed*60)` do FlxCamera.updateFollow:
     1 - 0.98^1.001 = 0.020019798.
   - GO_ZOOM_K: MathUtil.smoothLerpPrecision(base, target, elapsed,
     CAMERA_ZOOM_DURATION = 0.5) (L100, L265) = target + (base-target) *
     pow(0.01, elapsed/0.5); com elapsed = 1/59.94 s o fator e' 0.85756425.
   - GO_FADE_SAIDA_QUADROS / GO_FADE_ENTRADA_QUADROS: depois do CROSS o
     original toca `deathConfirm`, espera FADE_TIMER =
     (comprimento da musica de game over)/7000, faz FlxG.camera.fade(BLACK, 2)
     e, no fim, resetPlaying() -> fade(BLACK, 1, true) + needsReset
     (GameOverSubState.hx L369-438). 2 s para preto e 1 s de volta.
     A musica e o sfx DESSA tela nao estavam no pacote de dados; hoje estao
     (tools/empacotar_audio.py) e o audio de verdade toca -- ver o bloco
     AUDIO DA TELA DE GAME OVER mais abaixo. */
#define GO_CAM_LERP 0.020019798f
#define GO_ZOOM_K 0.857564250f
/* 2 s para preto (FlxG.camera.fade(BLACK, 2) / RetroCameraFade com duracao 2) e
   1 s de volta (resetPlaying -> fade(BLACK, 1, true)), a 59,94 quadros/s. */
#define GO_FADE_SAIDA_QUADROS 120
#define GO_FADE_ENTRADA_QUADROS 60
static float g_go_cam_x=0.0f,g_go_cam_y=0.0f,g_go_zoom=1.0f,g_go_ms=0.0f;
static int g_go_iniciado=0,g_go_anim=-1,g_go_terminando=0,g_go_fade=0;
/* `blueballed` (GameOverSubState.hx L64): e' `playBlueBalledSFX()` quem marca,
   e as DUAS saidas da tela exigem ele -- ACCEPT_P (L271-278) e o goBack do
   BACK_P (L546). Por isso ele entra como estado: no caminho do `fakeoutDeath`
   (L250-253) o som NAO toca, `blueballed` fica falso e o substate nao sai nem
   com CROSS nem com CIRCLE. E' um beco sem saida de 1 em 4096, e o port
   reproduz isso em vez de "consertar" -- a telemetria avisa qual caminho foi. */
static int g_go_blueballed=0;
static unsigned int g_go_sorteio=0;
/* ---------------------------------------------------------------------------
 * AUDIO DA TELA DE GAME OVER
 *
 * O original toca tres coisas nessa tela, e todas as tres estavam ausentes:
 *   1) `fnf_loss_sfx` + `blueBallSuffix` no primeiro `update`
 *      (playBlueBalledSFX(), GameOverSubState.hx L600-611);
 *   2) `gameOver` + `musicSuffix` quando o `firstDeath` termina, em loop
 *      (startDeathMusic(1.0,false) -> resolveMusicPath);
 *   3) `gameOverEnd` + `musicSuffix` no confirmDeath, sem loop
 *      (startDeathMusic(1.0,true)), e o comprimento DELE e' o FADE_TIMER.
 * E o `fakeoutDeath` do bf.hxc toca `fakeout_death` quando comeca.
 *
 * Nada disso estava no pacote de dados do repositorio -- nem o .ogg, nem os
 * sufixos (que moram nos scripts de personagem). As duas coisas agora sao
 * DADO, nao chute: o audio vem de `shared/...` do funkin.assets por
 * tools/empacotar_audio.py (PCM 48 kHz, que e' o que o audsrv aceita) e os
 * sufixos vem de preload/scripts/characters (um .hxc por personagem) por
 * tools/gen_gameover_sfx.py (src/gameover_dados.h).
 *
 * COMO TOCA: a tela tem DOIS sons ao mesmo tempo (musica + sfx) e o port tem um
 * anel e um audsrv. A soma e' feita na thread do anel (`audio_ring` overlay),
 * porque o usbhdfsd nao e' reentrante. O comeco do game over nao tem musica:
 * o anel roda em SILENCIO somando o sfx, e quando o `firstDeath` termina a
 * fonte primaria entra por baixo (audio_ring_trocar_fonte), como no original.
 * ------------------------------------------------------------------------- */
#define GO_ASSET_DIR "mass:/FNFPS2/assets/gameover/"
#define GO_SUF_MAX 24
static const char *g_go_mus_suf="",*g_go_bb_suf="";   /* do personagem do slot bf */
static const char *g_pause_suf="";  /* PauseSubState.musicSuffix, mesmo personagem */
static int g_go_audio_fd=-1;   /* fonte primaria: musica de game over / End     */
static int g_go_ov_fd=-1;      /* fonte secundaria: o sfx que toca por cima     */
/* A troca de fonte primaria (musica -> End) e' um PEDIDO: o fd antigo fica
   aberto ate' o fim da tela, porque a thread pode ainda estar lendo dele. */
static int g_go_mus_antigo=-1;
static int g_go_audio_estado=0;/* 0 = parado, 1 = silencio+sfx, 2 = musica, 3 = End */
static u32 g_go_submetido=0;   /* amostras mandadas ao audsrv nessa tela        */
static int g_go_mus_amostras=0;/* comprimento do arquivo de musica aberto       */
static int g_go_end_amostras=0;/* comprimento do gameOverEnd -> FADE_TIMER      */
static float g_go_espera_ms=0.0f;  /* FADE_TIMER do original em ms               */
static int g_go_saida=0;       /* 0 = deathLoop, 1 = esperando, 2 = fade, 3 = feito */
static float g_go_espera_ms_alvo=0.0f; /* FADE_TIMER do confirmDeath (ms)       */
/* Espelho de `audio_ok` (que e' local de main()): as funcoes abaixo sao
   estaticas e nao tem acesso ao local, e o audio do game over so' pode comecar
   se o audsrv subiu. */
static int g_audio_ok=0;
/* `g_go_fade` (quadros do fade para preto) ja' existe no bloco acima. */
/* FADE-IN DE 1 s DO `resetPlaying` (GameOverSubState.hx L393): no original ele
   e' criado no fim do fade de 2 s, com o PlayState JA' reiniciado, e clareia a
   tela de preto. Vive fora do estado 10 de proposito: quem desenha e' o estado
   seguinte (o gameplay), e sem esta variavel o port reiniciava com corte seco. */
static int g_fade_in=0;
/* PAUSA (ui_state continua 8: no original a pausa e' um SUBSTATE, o PlayState
   continua atras, congelado). g_pause_mode 0 = menu padrao, 1 = troca de
   dificuldade. As entradas do menu padrao sao as do original
   (PAUSE_MENU_ENTRIES_STANDARD), menos "Enable Practice Mode": o Practice Mode
   do original e' um modo de jogo inteiro (sem morte e comecando numa secao) e
   ainda nao existe no port -- a entrada entra quando o modo existir, porque
   oferecer uma entrada que nao faz o que promete e' pior que nao oferecer. */
static int g_paused=0,g_pause_sel=0,g_pause_frame=0,g_pause_mode=0,g_pause_drawn=0;
/* FREEPLAY (FreeplayState.hx).
   A lista e' montada uma vez a partir do registro (27 musicas, 10 semanas),
   sem duplicar musica que aparece em mais de uma semana. A dificuldade segue
   `SR_DIFF_*` da propria musica: a lista so' oferece o que existe.
   g_fp_active diz de onde viemos: quando a musica termina (ou o jogador sai),
   a Freeplay continua sendo o destino, e nao o Story Menu. */
#define FREEPLAY_MAX 64
static const sr_song_t* g_fp[FREEPLAY_MAX];
static int g_fp_n=0,g_fp_sel=0,g_fp_diff=1,g_fp_active=0,g_fp_drawn=0;
static const sr_song_t* g_fp_play_song=0;
static void freeplay_build(void){
 unsigned int li,si;const sr_level_t*lv;
 g_fp_n=0;
 for(li=0;li<sr_level_count()&&g_fp_n<FREEPLAY_MAX;li++){
  lv=sr_level(li);
  if(!lv)continue;
  for(si=0;si<lv->song_count&&g_fp_n<FREEPLAY_MAX;si++){
   const sr_song_t*song=lv->songs[si];int k,dup=0;
   if(!song)continue;
   for(k=0;k<g_fp_n;k++)if(g_fp[k]==song||strcmp(g_fp[k]->id,song->id)==0){dup=1;break;}
   if(!dup)g_fp[g_fp_n++]=song;
  }
 }
}
/* Primeira dificuldade que a musica tem, para a selecao nunca cair numa que
   nao existe (a Freeplay do original tambem so' lista as disponiveis). */
static int freeplay_diff_valida(const sr_song_t*song,int d){
 if(!song)return 1;
 if(d<0)d=0;
 if(d>2)d=2;
 if(song->difficulties&(1u<<d))return d;
 if(song->difficulties&1u)return 0;
 if(song->difficulties&2u)return 1;
 if(song->difficulties&4u)return 2;
 return 1;
}
/* Gancho opcional do harness de PC. Fraco de proposito: no console o simbolo
   nao existe, o ponteiro e' nulo e nada acontece. */
extern void ps2h_song_clock(unsigned int) __attribute__((weak));
static void (*const ps2h_song_clock_hook)(unsigned int)=ps2h_song_clock;
/* deathCounter do PlayState: o original incrementa a cada morte e mostra no
   menu de pausa como "N Blue Balls". */
static int g_death_counter=0;
/* Practice Mode (PlayState.isPracticeMode). Campo do PlayState no original e
   por isso NAO e' zerado por `resetPlaying`: Restart Song e Change Difficulty
   continuam no Practice (PauseSubState.changeDifficulty so' marca
   `needsReset`). Quem zera e' a criacao de um PlayState novo -- as cinco
   entradas de musica (story, freeplay, proxima da semana e os dois inicios do
   harness) fazem isso de proposito. */
static int g_practice_mode=0;
/* Uma vez por musica: a vida chegou a HEALTH_MIN. E' instrumento, nao regra --
   e' o que prova no log que o Practice Mode passou pelo chao da vida (no
   original nada disso aparece; o que muda e' `dead` continuar em zero). */
static int g_vida_zero_visto=0;
/* Entradas do menu da pausa, na ordem de
   PauseSubState.PAUSE_MENU_ENTRIES_STANDARD (L60-73). "Enable Practice Mode"
   carrega um `filter` (`!isPracticeMode`), entao ela DESAPARECE quando o modo
   ja' esta' ligado e as de baixo sobem um degrau -- por isso a lista e'
   MONTADA a cada quadro, e nao um indice fixo. */
#define PAUSE_ACAO_RESUME        0
#define PAUSE_ACAO_REINICIAR     1
#define PAUSE_ACAO_DIFICULDADE   2
#define PAUSE_ACAO_PRATICA       3
#define PAUSE_ACAO_SAIR          4
#define PAUSE_ENTRADAS_PADRAO    5   /* teto da lista (o Practice some) */
static int pause_acoes(int *out)
{
 int n=0;
 out[n++]=PAUSE_ACAO_RESUME;
 out[n++]=PAUSE_ACAO_REINICIAR;
 out[n++]=PAUSE_ACAO_DIFICULDADE;
 if(!g_practice_mode)out[n++]=PAUSE_ACAO_PRATICA;
 out[n++]=PAUSE_ACAO_SAIR;
 return n;
}
static void pause_menu_telemetria(void)
{
 int a[PAUSE_ENTRADAS_PADRAO];
 telemetry_event("pause_menu",pause_acoes(a),g_practice_mode);
}
static const char *pause_rotulo(int acao)
{
 switch(acao){
  case PAUSE_ACAO_RESUME:      return "RESUME";
  case PAUSE_ACAO_REINICIAR:   return "RESTART SONG";
  case PAUSE_ACAO_DIFICULDADE: return "CHANGE DIFFICULTY";
  case PAUSE_ACAO_PRATICA:     return "ENABLE PRACTICE MODE";
  default:                     return "EXIT TO MENU";
 }
}

/* Dois caminhos fazem exatamente a mesma coisa a partir de dois lugares (o
   Restart Song da pausa e o reinicio do game over), e por isso eles saem daqui
   em vez de duas copias que podem divergir: zerar o transporte, o relogio, o
   cursor do chart, os tallies, a camera e a contagem regressiva, e rebobinar o
   audio. Usam variaveis de main() de proposito -- sao macros, nao funcoes,
   para nao passar quinze ponteiros. */
/* Rebobinar o audio depende de a thread produtora ver o pedido de reset. No
   harness ela pode nao ser escalonada dentro dos 500 ms de espera e o reset
   devolve -1 -- medido: o reinicio do game over ficava com o anel cheio e a
   musica parada. A reserva e' parar e comecar de novo, que e' o que o Resume da
   pausa ja' faz: a thread e' recriada e le do offset atual. */
#define PLAY_REBOBINAR_AUDIO(fd) do{ \
    if((fd)>=0){int _rc; \
     lseek((fd),0,SEEK_SET);_rc=audio_ring_reset(); \
     if(_rc!=0){audio_ring_stop();lseek((fd),0,SEEK_SET);_rc=audio_ring_start((fd));} \
     telemetry_event("rebobinar",_rc,(int)frame_no); \
     if(_rc==0){audio_playing=1;transport_state=1;g_tutorial_audio_status=0;} \
     else g_tutorial_audio_status=-9;} \
  }while(0)
#define PLAY_REINICIAR_MUSICA() do{ \
    audsrv_stop_audio();audio_playing=0;audio_started=0;audio_submitted=0;music_clock=0; \
    music_clock_tick=music_clock_fraction=0;clock_hold=1;transport_state=0; \
    g_tutorial_audio_finished=0;g_chart_cursor=g_chart_seen=0; \
    g_chart_last_type=g_chart_last_value=g_chart_last_sample=0; \
    chart_events_reset(g_play_zoom, g_play_scroll); \
    g_hud_zoom=1.0f;g_hud_zoom_max=1.0f;g_hud_zoom_visto=0;g_hud_zoom_forte=0; \
    g_ev_icone=g_ev_script=g_ev_sem_efeito=g_ev_avisou=0; \
    g_tut_cam_focus=0;g_tut_cam_ready=0;g_cam_papel=0;g_cam_seq=-1;g_cam_t=1.0f;g_cam_dur=0.0f; \
    g_cam_modo_visto[0]=g_cam_modo_visto[1]=g_cam_modo_visto[2]=0;g_tut_zoom=g_play_zoom; \
    g_tut_scroll=g_play_scroll;g_bf_sing=g_gf_sing=-1;g_tut_bop=1.0f; \
    tutorial_notes_reset(); \
    g_countdown_beat=(s32)(4u*g_play_samples_per_step); \
    if(g_countdown_beat<=0)g_countdown_beat=(s32)(4u*TUT_SAMPLES_PER_STEP); \
    g_countdown_left=g_countdown_beat*5;g_countdown_step=0;g_chart_clock=-g_countdown_left; \
    /* A vida comeca em HEALTH_STARTING (= HEALTH_MAX/2 = 1,0) e a interpolada \
       nasce igual a ela; o pulo dos icones comeca do zero. */ \
    g_vida_lerp=1.0f;g_icone_ultimo_passo=-1;g_icone_desenhos=0;g_vida_zero_visto=0; \
    {int _l;for(_l=0;_l<ICONE_LADOS;_l++)hi_bop_reset(&g_icone_bop[_l]);} \
  }while(0)
/* Volta a tocar do ponto em que a pausa pegou. */
#define PLAY_RETOMAR_AUDIO(fd,amostra) do{ \
    if((fd)>=0){int _fim;audsrv_stop_audio();audio_ring_stop(); \
     _fim=(int)lseek((fd),0,SEEK_END); \
     if(_fim>0){u64 _bp=(u64)(amostra)*8u;if(_bp>(u64)(u32)_fim)_bp=(u64)(u32)_fim;lseek((fd),(int)_bp,SEEK_SET);} \
     if(audio_ring_start((fd))==0){audio_submitted=(amostra);music_clock_tick=GetTimerSystemTime(); \
      music_clock_fraction=0;clock_hold=1;audio_playing=1;audio_started=1;transport_state=2;}} \
  }while(0)
static chart_info_t g_chart_info;
static u32 g_chart_cursor=0,g_chart_seen=0,g_chart_last_type=0,g_chart_last_value=0,g_chart_last_sample=0;
/* EVENTOS QUE O PORT NAO CONSEGUE REPRODUZIR, contados por musica.
   Dois grupos, e os dois por falta de dado neste repositorio -- nao por
   esquecimento:
     icone  -> SetHealthIcon (15 eventos, todos em `spaghetti`): o id do icone
               ("yunjin", "kazuha", "sakura", ...) e' uma ARTE que nao esta' no
               pacote de dados (assets-data nao tem HealthIcon); o port desenha
               os dois icones fixos do HUD.
     script -> os eventos `sserafim*` (89 no mesmo chart, e sserafimPulseLights
               fora do mesmo): eles tocam props do palco por um SCRIPT do
               original, que nao esta' no repositorio.
   Contar e escrever no log e' o que a regra 4 pede: ausencia declarada, nunca
   efeito inventado. */
static u32 g_ev_icone=0,g_ev_script=0,g_ev_sem_efeito=0,g_ev_avisou=0;
static int g_tutorial_note_asset_status=-1,g_tutorial_note_visible=-1,g_tutorial_note_feedback=0;
/* NOTE SPLASH: o brilho do acerto SICK. O estado e a geometria vivem em
   engine/splash.c (que roda no PC); aqui ficam so' o pool, a flag de arte
   carregada e a telemetria. */
static nspl_pool_t g_splash;
static int g_splash_arte_ok=0;      /* 1 = a regiao foi escrita no atlas do HUD */
static int g_splash_log=0;          /* criacoes ja registradas no log          */
static int g_splash_desenho=-1;     /* splashes desenhadas no ultimo quadro     */
/* Every asset the port reads lives under this root on the memory card.
   The registry builds paths beneath it, so a song's chart is
   "<root>/songs/<id>/<difficulty>.chart". */
#define FNF_ASSET_ROOT "mass:/FNFPS2/assets"

#define CHART_MAX_NOTE_STATES 1280u /* acompanha CHART_MAX_NOTES: 2hot-HARD tem 1.035 notas */
static unsigned char g_tutorial_note_state[CHART_MAX_NOTE_STATES];
/* s32 no placar, nao u32: o original NAO limita songScore em zero, entao uma
   passagem ruim deixa o placar negativo. Com u32 isso virava 4 bilhoes no log
   e no HUD. Os outros contadores so' crescem e ficam u32. */
static s32 g_tutorial_note_score=0;
/* Custo medido da musica: quadros DESENHADOS com musica tocando (o denominador),
   uploads de quadro de personagem e de prop do palco. Fase 5 do plano pede
   numero, e numero sem denominador nao e' medida. */
static u32 g_custo_frames=0;
static int g_custo_emitido=0;
static u32 g_tutorial_note_hits=0,g_tutorial_note_misses=0,g_tutorial_note_combo=0,g_tutorial_note_feedback_frame=0;
/* Estado da tentativa, com os numeros do original (ver engine/gameplay.h).
   O HUD passou a ler daqui em vez de manter contadores proprios: era assim que
   "Score: 0" ficava fixo na tela mesmo com notas sendo acertadas. */
static fnf_play_t g_play;


static int g_gf_reload_fd=-1,g_gf_reload_active=0; static u32 g_gf_reload_offset=0,g_gf_reload_total=0;

static int g_title_audio_fd=-1; static u32 g_title_audio_submitted=0;
static u32 g_title_intro_heard_samples=0,g_title_intro_phase_latched=0,g_title_intro_phase_reported=0xffffffffu;

#define TITLE_LOGO_W 320
#define TITLE_LOGO_H 240
#define TITLE_LOGO_FRAMES 15
#define TITLE_GF_W 256
#define TITLE_GF_H 235
#define TITLE_GF_FRAMES 30
#define TITLE_LOGO_BYTES (TITLE_LOGO_W*TITLE_LOGO_H*4*TITLE_LOGO_FRAMES)
#define TITLE_GF_BYTES (TITLE_GF_W*TITLE_GF_H*4*TITLE_GF_FRAMES)
#define TITLE_LOGO_FRAME_BYTES (TITLE_LOGO_W*TITLE_LOGO_H*4)
#define TITLE_GF_FRAME_BYTES (TITLE_GF_W*TITLE_GF_H*4)
#define TITLE_CACHE_READ_CHUNK (64*1024)
#define TITLE_CACHE_AUDIO_RESERVE (192*1024)
#define TITLE_LZ4_MAX_BLOCK (64*1024)
#define TITLE_GF_STORE_OFFSET TITLE_LOGO_FRAME_BYTES
#define TITLE_NG_STORE_OFFSET (TITLE_GF_STORE_OFFSET+TITLE_GF_FRAME_BYTES)
#define TITLE_STORE_BYTES (TITLE_NG_STORE_OFFSET+181u*175u*4u)


static int g_menu_audio_suspended=0;
static int g_audio_current_rate=0;
static u32 g_menu_sfx_pos=0,g_menu_sfx_end=0,g_menu_sfx_phase=0;
static unsigned char *g_menu_pixels;
static unsigned char *g_menu_sfx_pixels;
static int g_menu_cache_ready=0;
static int g_story_wait=0;
static unsigned char *g_menu_compressed;
static unsigned char *g_up_bounce;
#define MENU_RLE_BYTES 1288182u
#define FONT_RLE_BYTES 38342u
#define PRELOAD_RLE_BYTES (MENU_RLE_BYTES+FONT_RLE_BYTES+STORY_RLE_BYTES+STORY_TITLES_RLE_BYTES)
#define MENU_FRAMES_BYTES 2437560u
#define MENU_CACHE_BYTES 2961848u
#define TITLE_SNAPSHOT_ALPHA (MENU_CACHE_BYTES)
#define TITLE_SNAPSHOT_PROMPT_L (TITLE_SNAPSHOT_ALPHA+384u*192u*4u)
#define TITLE_SNAPSHOT_PROMPT_R (TITLE_SNAPSHOT_PROMPT_L+512u*192u*4u)
#define TITLE_SNAPSHOT_END (TITLE_SNAPSHOT_PROMPT_R+320u*192u*4u)
static unsigned char *g_stage_pixels;
static unsigned char *g_hud_pixels;
static unsigned char *g_font_pixels;
static unsigned char g_font_widths[96];
static unsigned char *g_audio_pcm;
#define AUDIO_FILE_BYTES 3225600
#define AUDIO_STREAM_BUFFER 32768
#define AUDIO_READ_CHUNK 4096
#define STEM_READ_CHUNK 8192
#define AUDIO_RATE 48000
#define TUTORIAL_AUDIO_SAMPLES 3225600u
#define TUTORIAL_NOTE_AHEAD 76800u
/* Janela de julgamento e de nota perdida: AS DUAS SAO O MESMO NUMERO no
   original, Constants.HIT_WINDOW_MS = 160 ms, aplicado simetricamente em
   GRhythmUtil.getHitWindow() e cobrado em processWindow() (songPosition > end
   -> hasMissed). Aqui 160 ms a 48 kHz = 7.680 amostras. Estavam 7.200 (150 ms)
   e 12.000 (250 ms), dois valores que nao existem na fonte. Derivados da
   constante, nao digitados: assim nao voltam a divergir. */
#define TUTORIAL_NOTE_LATE ((u32)(FNF_HIT_WINDOW_MS*(float)AUDIO_RATE/1000.0f))
#define TUTORIAL_NOTE_JUDGE_WINDOW TUTORIAL_NOTE_LATE
/* Quantos quadros seguidos o audio pode falhar antes de o transporte parar.
   120 quadros = 2 s a 60 fps: tempo de sobra para o pendrive entregar um bloco
   de 8 KB (a 1,5 MB/s isso leva 5 ms). Antes, UM bloco curto ja' parava tudo. */
#define TUT_AUDIO_FAIL_LIMIT 120
/* Passo (em pixels) entre as celulas de nota no atlas do HUD: colunas lane*56
   e duas linhas (y=0 notas, y=56 receptores). E' o MESMO numero do
   `sprite_region_alpha` abaixo; antes ele aparecia digitado quatro vezes e
   esta macro existia sem uso. */
#define TUTORIAL_NOTE_SOURCE_TILE 56.0f
static const float g_note_w[4]={54.0f,55.0f,55.0f,54.0f};
static const float g_note_h[4]={55.0f,54.0f,54.0f,55.0f};
static const float g_receptor_x[4]={22.65f,78.65f,135.0f,190.65f};
static const float g_receptor_y[4]={25.30f,24.95f,25.30f,26.0f};
static const float g_note_x[4]={22.05f,77.525f,133.525f,190.05f};
#define FONT_W 384
#define FONT_H 192
#define FONT_CELL_W 24
#define FONT_CELL_H 32
#define FONT_DRAW_W 12
#define FONT_DRAW_H 16
static unsigned char g_pad_buffer[256] __attribute__((aligned(64)));
static struct padButtonStatus g_buttons;
static unsigned short g_held;
extern unsigned char audsrv_irx[];
extern unsigned int size_audsrv_irx;






static int read_exact_path(const char *path, void *dst, int bytes){
 int fd=open(path,O_RDONLY),done=0; if(fd<0)return -1;
 while(done<bytes){int n=read(fd,(unsigned char*)dst+done,bytes-done);if(n<=0){close(fd);return -2;}done+=n;}
 close(fd);return 0;
}

typedef struct {u32 offset,size;} title_lz4_entry_t;
static u32 title_u32le(const unsigned char*p){return lz4_u32le(p);}
/* O decodificador fica em src/engine/lz4_bloco.c (C puro, com teste proprio
   contra o codificador do empacotador). Aqui havia TRES copias desta mesma
   funcao, uma por familia de pacote; a quarta seria a dos personagens. */
static int title_lz4_decode(const unsigned char*src,u32 src_size,unsigned char*dst,u32 dst_size){return lz4_bloco_decode(src,src_size,dst,dst_size);}
static int tutorial_pack_load(const char*path,unsigned char**out,u32*out_bytes,u32 expected_count,u32 expected_frame){int fd,n,done=0;off_t end;unsigned char*p;if(*out)return 0;fd=open(path,O_RDONLY);if(fd<0)return-1;end=lseek(fd,0,SEEK_END);if(end<16||end>8*1024*1024||lseek(fd,0,SEEK_SET)<0){close(fd);return-2;}p=memalign(128,(u32)end);if(!p){close(fd);return-3;}while(done<end){n=read(fd,p+done,(int)end-done);if(n<=0){free(p);close(fd);return-4;}done+=n;}close(fd);if(memcmp(p,"CFZ4",4)||title_u32le(p+4)!=1||title_u32le(p+8)!=expected_count||title_u32le(p+12)!=expected_frame){free(p);return-5;}*out=p;*out_bytes=(u32)end;return 0;}
static int tutorial_pack_frame(const unsigned char*pack,u32 bytes,u32 frame,u32 count,u32 frame_bytes,unsigned char*dst){u32 table=16u+count*8u,off,size;if(!pack||frame>=count||bytes<table)return-1;off=title_u32le(pack+16u+frame*8u);size=title_u32le(pack+20u+frame*8u);if(off>bytes||size>bytes-off)return-2;return title_lz4_decode(pack+off,size,dst,frame_bytes);}
static void tutorial_packs_free(void){if(g_tutorial_bf_pack)free(g_tutorial_bf_pack);if(g_tutorial_gf_pack)free(g_tutorial_gf_pack);g_tutorial_bf_pack=g_tutorial_gf_pack=0;g_tutorial_bf_pack_bytes=g_tutorial_gf_pack_bytes=0;g_tutorial_bf_uploaded=g_tutorial_gf_uploaded=-1;}
/* A GF do Tutorial vive na MESMA VRAM da gftex, mas em PSMCT16.
   196.480 B por quadro em vez de 392.960, medidos a 23,3 MB/s de taxa
   efetiva de upload: 8,4 ms em vez de 16,9 ms. Cabe nos 262.144 B ja
   alocados, entao o mapa de VRAM volta ao da 0.2.56. */
static texbuffer_t g_gftex16;
/* DESPEJO DE LISTA DE DESENHO.
   Ate aqui eu depurava graficos RECONSTRUINDO no PC o que eu achava que
   o port desenhava. Errei cinco vezes seguidas no traco do MERCH por
   causa disso: o simulador desenha limpo porque o meu modelo e que esta
   incompleto, nao o console.
   Isto grava os parametros REAIS de cada sprite de UM quadro num csv,
   para o simulador reexecutar exatamente aquilo. Custa nada enquanto
   desligado: um teste de inteiro por chamada. */
#define DL_MAX 384
typedef struct{
  unsigned int tex,tbw,psm;short tw,th,filtro,wms;
  float x,y,w,h,u,v,uw,vh;float vx0,vy0,vx1,vy1;
  unsigned char alfa;unsigned char estado;
}dl_item_t;
static dl_item_t g_dl[DL_MAX];
static int g_dl_n=0,g_dl_armado=0,g_dl_gravando=0,g_forcar_captura=0;
static unsigned int g_dl_overflow=0;
static unsigned int g_dl_tex=0,g_dl_tbw=0,g_dl_psm=0;
static short g_dl_tw=0,g_dl_th=0,g_dl_filtro=0;
static unsigned char g_dl_estado=0;
/* ==== FNX_V1_INCLUSIVO =====================================================
 * O libdraw trata v1 como o ULTIMO pixel (convencao INCLUSIVA), nao como
 * "um alem do ultimo" (exclusiva). Prova: START_OFFSET=2047.5625 e
 * END_OFFSET=2048.5625 diferem de exatamente 1.0; com XYOFFSET=2048 e
 * ftoi4 truncante, v0==v1 produz span de 16/16 = 1 pixel exato.
 * Regra de cobertura do GS: pixel n coberto <=> n+0.5 em [X0,X1).
 * Sem esta correcao TODO sprite e TODO retangulo sai com +1 px em X e Y,
 * e a linha/coluna extra amostra UV alem de t1 -> CLAMP repete a ultima
 * linha de texel (borda antialiased) -> franja de alfa parcial.
 * Medido no console (0.2.82-aa): quad do MERCH v0=(225,256) v1=(416,316)
 * cobrindo colunas 225..416 e linhas 256..316, com alfa 1,2,3,7,10,18,28,34
 * exatamente na linha 316. Ver tools/prova_cobertura_gs.py e
 * tools/validar_drawlist.py.
 * Compilar com -DFNX_V1_INCLUSIVO para ativar. Sem a flag, comportamento
 * identico ao anterior (permite gerar dois ELFs e comparar).
 * ==========================================================================*/

/* ==== FNX_VERTICE_EXATO ====================================================
 * O libdraw desloca os vertices antes de manda-los ao GS:
 *   draw2d.c   START_OFFSET 2047.5625f   END_OFFSET 2048.5625f
 *   draw.c     GS_SET_XYOFFSET(ftoi4(2048.0f), ftoi4(2048.0f))
 *   draw_types.h  ftoi4(F) = (int)((F)*16.0f)      [truncamento]
 * Resultado: X0 = v0 - 0.4375   e   X1 = v1 + 0.5625.
 * Regra de cobertura do GS: pixel n coberto <=> n+0.5 em [X0, X1).
 * Logo v1 e INCLUSIVO (v0==v1 cobre exatamente 1 pixel) e o retangulo sai
 * com +1 px em cada eixo. Estas macros invertem os dois deslocamentos, de
 * modo que o GS receba X0 = v0 e X1 = v1 exatamente como o jogo pretende.
 * Prova e numeros: tools/prova_cobertura_gs.py, tools/validar_drawlist.py.
 * Sem -DFNX_VERTICE_EXATO o comportamento e identico ao anterior.
 * ========================================================================*/
#define FNX_LIBDRAW_START 2047.5625f
#define FNX_LIBDRAW_END   2048.5625f
#define FNX_LIBDRAW_XYOFF 2048.0f
#ifndef FNX_VERTICE_EXATO
#define FNX_INI(c) (c)
#define FNX_FIM(c) (c)
#else
#define FNX_INI(c) ((c) + (FNX_LIBDRAW_XYOFF - FNX_LIBDRAW_START))  /* +0.4375 */
#define FNX_FIM(c) ((c) - (FNX_LIBDRAW_END - FNX_LIBDRAW_XYOFF))    /* -0.5625 */
#endif

static float vy(float y);
static void write_all_fd(int fd, const void *p, u32 n);
static void capture_status(const char *phase, int row, int code);
static float g_dl_gs[DL_MAX*4];
/* Cabecalho da lista, uma vez so. */
static int g_dl_hdr=0;
static void dl_header(void){
  if(g_dl_hdr){return;}
  g_dl_hdr=1;
  /* Uma linha, uma vez na vida do processo: pode ser telemetry_raw(), que
     escreve na hora. O que NAO pode eh telemetry_raw() por sprite -- ver
     telemetry_line(). */
  telemetry_raw("# colunas sprite: D,quadro,i,estado,x,y,w,h,u,v,uw,vh,alfa,"
                "vy0,vy1,vy_span,gsx0,gsy0,gsx1,gsy1  (geometria JA recortada)");
}
/* Uma linha por sprite, saida no mesmo instante. x,y,w,h,u,v,uw,vh sao os
   valores que o helper recebeu DEPOIS do recorte da faixa; vy0/vy1 sao os
   vertices que foram ao GS. Comparando vy_span com (gsy1-gsy0) da para ver
   na hora um sprite que foi esticado, e comparando vy0/vy1 com a faixa
   publicada da para ver um que saiu dela. */
static void dl_dump(u32 frame){
  char b[640];int i;
  dl_header();
  for(i=0;i<g_dl_n;i++){
    dl_item_t*d=&g_dl[i];
    snprintf(b,sizeof(b),
      "D,%u,%d,%u,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%u,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f",
      frame,i,(unsigned)d->estado,d->x,d->y,d->w,d->h,d->u,d->v,d->uw,d->vh,
      (unsigned)d->alfa,d->vy0,d->vy1,d->vy1-d->vy0,
      g_dl_gs[i*4+0],g_dl_gs[i*4+1],g_dl_gs[i*4+2],g_dl_gs[i*4+3]);
    /* telemetry_line(), NAO telemetry_raw(). A lista inteira sai no flush do
       quadro, em UMA escrita. A versao que foi para o console chamava
       telemetry_raw() por sprite, e cada chamada era uma escrita bloqueante
       no pendrive dentro do laco de render: ~20 escritas por quadro, o jogo
       caiu para poucos quadros por segundo e a tela ficou parada -- foi lido
       como "o jogo nao abriu". */
    telemetry_line(b);
  }
}
static void dl_gravar(void){
  dl_header();
  capture_status("DRAWLIST",g_dl_n,(int)g_dl_overflow);
}
static void dl_sprite(float x,float y,float w,float h,
                      float u,float v,float uw,float vh,unsigned char a,
                      const texrect_t *rv){
  dl_item_t*d;
  /* Registra sempre. A versao anterior so gravava quando alguem armava o
     disparador, entao a colheita de um teste normal nao trazia uma unica
     linha de sprite -- e era exatamente a linha que diria qual sprite saiu
     fora da faixa e com qual pedaco de textura. Com o recorte por software
     ativo, a geometria que sai daqui JA eh a recortada: o log passa a mostrar
     o que foi ao GS, nao a intencao. */
  if(g_dl_n>=DL_MAX){g_dl_overflow++;return;}
  d=&g_dl[g_dl_n++];
  d->tex=g_dl_tex;d->tbw=g_dl_tbw;d->psm=g_dl_psm;
  d->tw=g_dl_tw;d->th=g_dl_th;d->filtro=g_dl_filtro;d->wms=WRAP_REGION_CLAMP;
  d->x=x;d->y=y;d->w=w;d->h=h;d->u=u;d->v=v;d->uw=uw;d->vh=vh;
  /* os vertices COMO FORAM PARA O GS, ja passados por vy(). E o unico
     dado que ainda me falta: a captura mostra o GS escrevendo uma
     linha e uma coluna alem do quad, com alfa parcial, e eu preciso
     ver o valor exato que saiu daqui para saber se a sobra nasce na
     coordenada ou na rasterizacao. */
  /* AGORA SIM: os vertices que draw_rect_textured vai receber, lidos do
     texrect_t real, e nao recalculados dos parametros crus. Antes desta
     versao o drawlist registrava a INTENCAO do codigo, nunca o que ia ao
     GS - por isso o CSV da 0.2.87 e o da 0.2.88 sairam identicos apesar
     de a 0.2.88 ter a correcao de vertice ativa no binario. */
  if(rv){d->vx0=rv->v0.x;d->vy0=rv->v0.y;d->vx1=rv->v1.x;d->vy1=rv->v1.y;}
  else{d->vx0=x;d->vy0=vy(y);d->vx1=x+w;d->vy1=vy(y+h);}
  /* e o que o GS recebe de fato, depois dos offsets do libdraw:
     X = ftoi4(v + OFFSET) - ftoi4(2048.0), em unidades de 1/16 de pixel.
     Guardado em g_dl_gs[] para o CSV. */
  if(rv&&g_dl_n>0){float gx0=(float)((int)((rv->v0.x+2047.5625f)*16.0f)-32768);
    float gy0=(float)((int)((rv->v0.y+2047.5625f)*16.0f)-32768);
    float gx1=(float)((int)((rv->v1.x+2048.5625f)*16.0f)-32768);
    float gy1=(float)((int)((rv->v1.y+2048.5625f)*16.0f)-32768);
    g_dl_gs[(g_dl_n-1)*4+0]=gx0/16.0f;g_dl_gs[(g_dl_n-1)*4+1]=gy0/16.0f;
    g_dl_gs[(g_dl_n-1)*4+2]=gx1/16.0f;g_dl_gs[(g_dl_n-1)*4+3]=gy1/16.0f;}
  d->alfa=a;d->estado=g_dl_estado;
}
static void upload_raw_psm(texbuffer_t*t,unsigned char*source,int w,int h,int psm);
static void setup_texture_dims(texbuffer_t*t,int w,int h);
static void upload_raw(texbuffer_t*t,unsigned char*source,int w,int h);
static void story_packs_free(void){g_story_bf_pack=g_story_gf_pack=0;g_story_bf_pack_bytes=g_story_gf_pack_bytes=0;g_story_bf_uploaded=g_story_gf_uploaded=-1;g_story_anim_ready=0;}
static int story_pack_mem(const unsigned char*base,u32 bytes,u32 count,u32 frame_bytes){u32 i,table=16u+count*8u;if(bytes<table||memcmp(base,"CFZ4",4)||title_u32le(base+4)!=1||title_u32le(base+8)!=count||title_u32le(base+12)!=frame_bytes)return-1;for(i=0;i<count;i++){u32 off=title_u32le(base+16u+i*8u),size=title_u32le(base+20u+i*8u);if(off>bytes||size>bytes-off)return-2;}return 0;}
static int story_packs_load(texbuffer_t*gf,texbuffer_t*bf){int rg,rb;story_packs_free();g_story_gf_pack=g_story_gf_pack_start;g_story_gf_pack_bytes=(u32)(g_story_gf_pack_end-g_story_gf_pack_start);g_story_bf_pack=g_story_bf_pack_start;g_story_bf_pack_bytes=(u32)(g_story_bf_pack_end-g_story_bf_pack_start);rg=story_pack_mem(g_story_gf_pack,g_story_gf_pack_bytes,30,170u*157u*4u);rb=story_pack_mem(g_story_bf_pack,g_story_bf_pack_bytes,14,164u*165u*4u);if(rg||rb){story_packs_free();return rg?rg:rb;}rg=tutorial_pack_frame(g_story_gf_pack,g_story_gf_pack_bytes,29,30,170u*157u*4u,g_story_gf_pixels);rb=tutorial_pack_frame(g_story_bf_pack,g_story_bf_pack_bytes,0,14,164u*165u*4u,g_story_bf_pixels);if(rg||rb){story_packs_free();return rg?rg:rb;}FlushCache(0);setup_texture_dims(gf,170,157);upload_raw(gf,g_story_gf_pixels,170,157);setup_texture_dims(bf,164,165);upload_raw(bf,g_story_bf_pixels,164,165);g_story_gf_uploaded=29;g_story_bf_uploaded=0;g_story_anim_ready=1;return 0;}

extern const unsigned char g_title_logo_pack_start[],g_title_logo_pack_end[],g_title_gf_pack_start[],g_title_gf_pack_end[];
static int title_read_pack_header_mem(const unsigned char*base,u32 bytes,u32 expected_count,u32 expected_frame,title_lz4_entry_t*entries){u32 i,table=16u+8u*expected_count;if(bytes<table||memcmp(base,"TLZ4",4)||title_u32le(base+4)!=1||title_u32le(base+8)!=expected_count||title_u32le(base+12)!=expected_frame)return-1;for(i=0;i<expected_count;i++){const unsigned char*e=base+16u+i*8u;entries[i].offset=title_u32le(e);entries[i].size=title_u32le(e+4);if(!entries[i].size||entries[i].size>TITLE_LZ4_MAX_BLOCK||entries[i].offset>bytes||entries[i].size>bytes-entries[i].offset)return-2;}return 0;}
static int title_load_packed_frame_mem(const unsigned char*base,const title_lz4_entry_t*e,unsigned char*dst,u32 frame_bytes){int r=title_lz4_decode(base+e->offset,e->size,dst,frame_bytes);if(r<0)return r;FlushCache(0);return 0;}
static void title_animation_cache_worker(void*arg){title_lz4_entry_t le[TITLE_LOGO_FRAMES],ge[TITLE_GF_FRAMES];u32 lb=(u32)(g_title_logo_pack_end-g_title_logo_pack_start),gb=(u32)(g_title_gf_pack_end-g_title_gf_pack_start);int step,err=0;(void)arg;if(title_read_pack_header_mem(g_title_logo_pack_start,lb,TITLE_LOGO_FRAMES,TITLE_LOGO_FRAME_BYTES,le)<0||title_read_pack_header_mem(g_title_gf_pack_start,gb,TITLE_GF_FRAMES,TITLE_GF_FRAME_BYTES,ge)<0)err=-1;for(step=0;!err&&step<TITLE_GF_FRAMES;step++){if(title_load_packed_frame_mem(g_title_gf_pack_start,&ge[step],g_title_anim_pixels+TITLE_LOGO_BYTES+(u32)step*TITLE_GF_FRAME_BYTES,TITLE_GF_FRAME_BYTES)<0){err=-2;break;}g_title_gf_ready|=1u<<step;if(step<TITLE_LOGO_FRAMES){if(title_load_packed_frame_mem(g_title_logo_pack_start,&le[step],g_title_anim_pixels+(u32)step*TITLE_LOGO_FRAME_BYTES,TITLE_LOGO_FRAME_BYTES)<0){err=-3;break;}g_title_logo_ready|=1u<<step;}}g_title_anim_status=err?-2:1;ExitDeleteThread();}
static int title_animation_cache_start(void){ee_thread_t t;int id;if(g_title_anim_status!=0)return-1;g_title_anim_pixels=memalign(128,TITLE_LOGO_BYTES+TITLE_GF_BYTES);if(!g_title_anim_pixels){g_title_anim_status=-1;return-2;}memset(&t,0,sizeof(t));t.func=title_animation_cache_worker;t.stack=g_title_anim_thread_stack;t.stack_size=sizeof(g_title_anim_thread_stack);t.gp_reg=&_gp;t.initial_priority=50;id=CreateThread(&t);if(id<0){free(g_title_anim_pixels);g_title_anim_pixels=0;g_title_anim_status=-1;return-3;}g_title_anim_thread_id=id;g_title_anim_status=2;if(StartThread(id,0)<0){DeleteThread(id);g_title_anim_thread_id=-1;free(g_title_anim_pixels);g_title_anim_pixels=0;g_title_anim_status=-1;return-4;}return 0;}
static void setup_texture_dims(texbuffer_t*t,int w,int h);
static void upload_raw(texbuffer_t*t,unsigned char*source,int w,int h);
static void capture_status(const char*phase,int row,int code);
static int tutorial_scene_load(texbuffer_t*stagetex,texbuffer_t*bftex,texbuffer_t*gftex){
 int r;
 g_tutorial_scene_attempted=1;g_tutorial_scene_ready=0;g_tutorial_scene_status=0;
 telemetry_event("tut_stage_begin",0,0);
 r=read_exact_path("mass:/FNFPS2/assets/mainstage_tutorial_parallax.rgba",g_stage_pixels,512*256*4);
 telemetry_event("tut_stage_done",r,0);if(r<0)capture_status("TUT_STAGE_READ_FAIL",-1,r);
 if(r<0){g_tutorial_scene_status=-10+r;return g_tutorial_scene_status;}
 FlushCache(0);setup_texture_dims(stagetex,512,256);upload_raw(stagetex,g_stage_pixels,512,256);
 telemetry_event("tut_stage_upload",0,0);
 telemetry_event("tut_bf_begin",0,0);
 r=tutorial_pack_load("mass:/FNFPS2/assets/bf_frames.lz4f",&g_tutorial_bf_pack,&g_tutorial_bf_pack_bytes,43,258u*254u*4u);g_tutorial_bf_w=258;g_tutorial_bf_h=254;
 if(r==0)r=tutorial_pack_frame(g_tutorial_bf_pack,g_tutorial_bf_pack_bytes,0,43,258u*254u*4u,g_stage_pixels);
 telemetry_event("tut_bf_done",r,0);if(r<0)capture_status("TUT_BF_READ_FAIL",-1,r);if(r<0){g_tutorial_scene_status=-20+r;return g_tutorial_scene_status;}FlushCache(0);setup_texture_dims(bftex,258,254);upload_raw(bftex,g_stage_pixels,258,254);g_tutorial_bf_uploaded=0;
 telemetry_event("tut_gf_begin",0,0);
 r=tutorial_pack_load("mass:/FNFPS2/assets/gf_frames.lz4f",&g_tutorial_gf_pack,&g_tutorial_gf_pack_bytes,87,320u*307u*2u);g_tutorial_gf_w=320;g_tutorial_gf_h=307;
 if(r==0)r=tutorial_pack_frame(g_tutorial_gf_pack,g_tutorial_gf_pack_bytes,0,87,320u*307u*2u,g_hud_pixels);
 telemetry_event("tut_gf_done",r,0);if(r<0)capture_status("TUT_GF_READ_FAIL",-1,r);if(r<0){g_tutorial_scene_status=-30+r;return g_tutorial_scene_status;}FlushCache(0);g_gftex16=*gftex;g_gftex16.width=320;g_gftex16.psm=GS_PSM_16;setup_texture_dims(&g_gftex16,320,307);upload_raw_psm(&g_gftex16,g_hud_pixels,320,307,GS_PSM_16);g_tutorial_gf_uploaded=0;
 g_tutorial_scene_ready=1;return 0;
}


static int load_metadata(void){
 g_gf_pixels=memalign(128,TITLE_STORE_BYTES);if(!g_gf_pixels)return -1;
 g_pixels=memalign(128,TITLE_SNAPSHOT_END);if(!g_pixels)return -2;
 g_stage_pixels=memalign(128,512u*256u*4u);if(!g_stage_pixels)return -3;
 g_hud_pixels=memalign(128,320u*256u*4u);g_up_bounce=memalign(128,512u*256u*4u);if(!g_hud_pixels)return -4;
 g_story_gf_pixels=memalign(128,170u*157u*4u);if(!g_story_gf_pixels)return -6;
 g_story_bf_pixels=memalign(128,164u*165u*4u);if(!g_story_bf_pixels)return -7;
 g_font_pixels=memalign(128,FONT_W*256u*4u);if(!g_font_pixels)return -5;
 if(read_exact_path("mass:/FNFPS2/assets/font.rgba",g_font_pixels,FONT_W*FONT_H*4)<0)return -6;
 if(read_exact_path("mass:/FNFPS2/assets/font.widths",g_font_widths,sizeof(g_font_widths))<0)return -7;
 g_audio_pcm=memalign(128,AUDIO_STREAM_BUFFER);if(!g_audio_pcm)return -8;
 g_bf.width=TITLE_LOGO_W;g_bf.height=TITLE_LOGO_H;g_bf.frame_count=TITLE_LOGO_FRAMES;g_bf.fps=24;
 g_gf.width=TITLE_GF_W;g_gf.height=TITLE_GF_H;g_gf.frame_count=TITLE_GF_FRAMES;g_gf.fps=24;
 return 0;
}
static int g_viewport_360=0;
/* g_vy_direto desliga a conversao logica->fisica por um trecho, para o
   chamador passar coordenada FISICA ja pronta.
   Existe por causa do traco de 1 px nos botoes do menu, que resistiu a
   SEIS hipoteses minhas. A causa estava aqui o tempo todo: vy() nao e
   um deslocamento, e uma REDUCAO de 14/15. Os 360 do espaco logico
   viram 336 linhas fisicas, entao NADA e desenhado 1:1 na vertical.
   Um item de 60 linhas de arte ocupa 56 de tela, e com NEAREST isso
   DESCARTA 4 linhas. Onde a descartada e o contorno branco que separa
   a letra preta do fundo amarelo, sobra uma fileira escura: o traco.
   E as bordas caem em coordenada fracionaria, o MERCH de 166,133 a
   222,133, e o GS cobre parcialmente a linha seguinte. Foi por isso
   que medi alfa 1, 2, 3, 34 e 54 na linha 222 do framebuffer, onde o
   quad nem deveria tocar. */
static int g_vy_direto=0;
/* MODO DE PROPORCAO, decidido no hardware e nao por suposicao minha.
   O PS2 NTSC desenha em 640x448 e a TV estica isso para a sua propria
   proporcao. Logo o pixel NAO e quadrado, e o fator depende da TV:

     TV 4:3     PAR = (4/3)/(640/448)  = 0,933333 = 14/15
                para 16:9: altura = 640*0,933333*9/16 = 336,0000 linhas
                fator de vy() 0,933333, offset 56, scissor 56..391
     TV 16:9    PAR = (16/9)/(640/448) = 1,244444
                para 16:9: altura = 640*1,244444*9/16 = 448,0000 linhas
                fator de vy() 1,244444, offset 0, scissor 0..447

   Os dois dao 16:9 EXATO, com erro 0,0000%, cada um na sua TV. O port
   so tinha o primeiro. Se a TV estiver em 16:9 esticado, o jogo sai
   com a proporcao errada e com duas barras pretas que nao deviam
   existir. SELECT+TRIANGULO alterna, e o despejo de estado registra
   qual esta ativo. */
static int g_aspect_169=0;
/* Padrao de calibracao. Desenha figuras de lado CONHECIDO no espaco
   logico de 640x360, que e 16:9 de pixel quadrado por definicao. Se a
   cadeia de proporcao estiver certa, o quadrado aparece QUADRADO na
   TV. Se aparecer achatado ou esticado, o quanto ele desvia da o fator
   que falta, sem eu supor nada sobre o aparelho.
   Medidas, em unidades logicas:
     quadrado central  180 x 180, branco com miolo vermelho
     cruz verde        640 x 1 e 1 x 360, pelo centro
     cantos amarelos   40 x 40 nos quatro extremos, para revelar o
                       corte de borda que quase toda TV aplica
     regua azul        tracos de 64 em 64 na horizontal e de 36 em 36
                       na vertical. 64/36 = 16/9, entao os dois contam
                       10 tracos e devem ter o MESMO espacamento fisico */
static int g_calib=0;
static qword_t *rect(qword_t*q,float x0,float y0,float x1,float y1,
                     unsigned char r,unsigned char g,unsigned char b);
static qword_t *desenhar_calibracao(qword_t *q)
{
    int i;
    q = rect(q, 320.0f - 90.0f, 180.0f - 90.0f, 320.0f + 90.0f, 180.0f + 90.0f,
             255, 255, 255);
    q = rect(q, 320.0f - 86.0f, 180.0f - 86.0f, 320.0f + 86.0f, 180.0f + 86.0f,
             200, 40, 40);
    q = rect(q, 0.0f, 179.5f, 640.0f, 180.5f, 0, 255, 0);
    q = rect(q, 319.5f, 0.0f, 320.5f, 360.0f, 0, 255, 0);
    q = rect(q, 0.0f, 0.0f, 40.0f, 40.0f, 255, 255, 0);
    q = rect(q, 600.0f, 0.0f, 640.0f, 40.0f, 255, 255, 0);
    q = rect(q, 0.0f, 320.0f, 40.0f, 360.0f, 255, 255, 0);
    q = rect(q, 600.0f, 320.0f, 640.0f, 360.0f, 255, 255, 0);
    for (i = 0; i <= 10; i++) {
        float x = i * 64.0f;
        q = rect(q, x - 1.0f, 348.0f, x + 1.0f, 360.0f, 0, 160, 255);
    }
    for (i = 0; i <= 10; i++) {
        float y = i * 36.0f;
        q = rect(q, 0.0f, y - 1.0f, 12.0f, y + 1.0f, 0, 160, 255);
    }
    return q;
}
#define VY_FATOR (g_aspect_169?(448.0f/360.0f):(14.0f/15.0f))
#define VY_OFFSET (g_aspect_169?0.0f:56.0f)
#define SCISSOR_TOPO (g_aspect_169?0:56)
#define SCISSOR_BASE (g_aspect_169?447:391)
static float vy(float y){return (g_viewport_360&&!g_vy_direto)?VY_OFFSET+y*VY_FATOR:y;}
static color_t rgba(unsigned char r,unsigned char g,unsigned char b,unsigned char a){color_t c;c.r=r;c.g=g;c.b=b;c.a=a;c.q=1.0f;return c;}
static qword_t *rect_alpha(qword_t*q,float x0,float y0,float x1,float y1,unsigned char r,unsigned char g,unsigned char b,unsigned char a){rect_t v;v.v0.x=FNX_INI(x0);v.v0.y=FNX_INI(vy(y0));v.v0.z=0;v.v1.x=FNX_FIM(x1);v.v1.y=FNX_FIM(vy(y1));v.v1.z=0;v.color=rgba(r,g,b,a);return draw_rect_filled(q,0,&v);}
static qword_t *rect(qword_t*q,float x0,float y0,float x1,float y1,unsigned char r,unsigned char g,unsigned char b){return rect_alpha(q,x0,y0,x1,y1,r,g,b,0x80);}
/* TRAPEZIO DE COR SOLIDA, SEM TEXTURA (a barra preta do topo do ResultState).
   `rect`/`draw_rect_filled` so' fazem retangulo alinhado aos eixos; a barra do
   original tem a BORDA DE BAIXO INCLINADA (o PNG e' um retangulo preto girado
   de -3,8 graus), entao ela precisa de quatro vertices quaisquer.

   Os dois triangulos vao num COMANDO so' (um TRIANGLE_FAN de quatro vertices:
   v0,v1,v2 e v0,v2,v3), no MESMO formato de qwords que o ps2sdk usa para o
   retangulo -- se o formato divergisse, o pacote corromperia o resto do quadro
   e nao so' a barra. Custo por quadro: 3 qwords, zero textura, zero VRAM. */
static u32 g_frame_flat_quads;   /* trapezios de cor solida (a barra do topo) */
static qword_t *barra_trapezio(qword_t*q,float x0,float x1,float ytop,
                               float yb0,float yb1,unsigned char a){
 color_t cor;
 cor.rgbaq=0;cor.r=0;cor.g=0;cor.b=0;cor.a=a;cor.q=1.0f;
 /* REGLIST com 6 registros num UNICO laco -- o mesmo formato que
    draw_rect_filled emite (draw2d.c L190-210: tag NLOOP=1, NREG=4,
    PRIM+RGBAQ num qword e dois XYZ2 no outro). Aqui sao QUATRO vertices,
    entao NREG=6 e o reglist tem dois XYZ2 a mais. */
 q->dw[0]=GIF_SET_TAG(1,0,0,0,GIF_FLG_REGLIST,6);
 q->dw[1]=((u64)GIF_REG_PRIM)<<0|((u64)GIF_REG_RGBAQ)<<4|
          ((u64)GIF_REG_XYZ2)<<8|((u64)GIF_REG_XYZ2)<<12|
          ((u64)GIF_REG_XYZ2)<<16|((u64)GIF_REG_XYZ2)<<20;
 q++;
 q->dw[0]=GIF_SET_PRIM(GS_PRIM_TRIANGLE_FAN,0,0,0,1,0,0,0,0);
 q->dw[1]=cor.rgbaq;
 q++;
 q->dw[0]=GIF_SET_XYZ(ftoi4(x0+2047.5625f),ftoi4(vy(ytop)+2047.5625f),0);
 q->dw[1]=GIF_SET_XYZ(ftoi4(x1+2047.5625f),ftoi4(vy(ytop)+2047.5625f),0);
 q++;
 q->dw[0]=GIF_SET_XYZ(ftoi4(x1+2048.5625f),ftoi4(vy(yb1)+2048.5625f),0);
 q->dw[1]=GIF_SET_XYZ(ftoi4(x0+2048.5625f),ftoi4(vy(yb0)+2048.5625f),0);
 q++;
 g_frame_flat_quads++;
 return q;
}
/* Dithering do GS: obrigatorio quando o framebuffer e' de 16 bits.
   draw_setup_environment() desliga (GS_SET_DTHE(GS_DISABLE)) a cada quadro,
   porque o padrao do ps2sdk assume framebuffer de 32 bits -- onde dither e'
   desperdicio. Em PSMCT16 ele e' o que separa um degrade' de uma escada de
   cinco tons: sem ele, cada canal perde 3 bits arredondando para o mesmo
   valor em faixas inteiras da tela. Custa 2 quadwords por quadro. */
static qword_t *gs_dither16(qword_t*q){
    q->dw[0]=GIF_SET_TAG(1,0,0,0,GIF_FLG_PACKED,1);q->dw[1]=GIF_REG_AD;q++;
    q->dw[0]=GS_SET_DTHE(GS_ENABLE);q->dw[1]=GS_REG_DTHE;q++;
    return q;
}
static u32 g_frame_texture_quads;

#define QUAD_NREG 10
#define QUAD_REGLIST (((u64)GIF_REG_PRIM)<<0|((u64)GIF_REG_RGBAQ)<<4|((u64)GIF_REG_UV)<<8|((u64)GIF_REG_XYZ2)<<12|((u64)GIF_REG_UV)<<16|((u64)GIF_REG_XYZ2)<<20|((u64)GIF_REG_UV)<<24|((u64)GIF_REG_XYZ2)<<28|((u64)GIF_REG_UV)<<32|((u64)GIF_REG_XYZ2)<<36)
/* QUADRILATERO TEXTURIZADO LIVRE (quatro vertices quaisquer e um UV por
   vertice). `sprite_region` so' faz retangulo alinhado aos eixos; o nome da
   musica do ResultState e' um sprite girado de -4,4 graus, entao os quatro
   cantos nao formam retangulo na tela. O pacote e' o QUAD_REGLIST de 10
   registros (PRIM, RGBAQ e quatro pares UV+XYZ2), o mesmo formato que o
   draw_rect_textured do ps2sdk emite para o retangulo texturizado.
   `px`/`py` em sentido horario a partir do canto superior esquerdo. */
static qword_t *sprite_quad_uv(qword_t*q,const float *px,const float *py,
                               float u0,float v0,float u1,float v1,
                               unsigned char a){
 color_t cor;int i;
 cor.rgbaq=0;cor.r=0x80;cor.g=0x80;cor.b=0x80;cor.a=a;cor.q=1.0f;
 q->dw[0]=GIF_SET_TAG(5,0,0,0,GIF_FLG_REGLIST,2);
 q->dw[1]=QUAD_REGLIST;
 q++;
 q->dw[0]=GIF_SET_PRIM(GS_PRIM_TRIANGLE_FAN,0,1,0,1,0,0,0,0);
 q->dw[1]=cor.rgbaq;
 q++;
 /* (u0,v0) (u1,v0) (u1,v1) (u0,v1), em dois qwords de dois vertices. */
 {float uu[4],vv[4];
  uu[0]=u0;uu[1]=u1;uu[2]=u1;uu[3]=u0;
  vv[0]=v0;vv[1]=v0;vv[2]=v1;vv[3]=v1;
  for(i=0;i<4;i+=2){
   q->dw[0]=(u64)((ftoi4(uu[i])&0xFFFF))|((u64)(ftoi4(vv[i])&0xFFFF)<<16);
   q->dw[1]=GIF_SET_XYZ(ftoi4(px[i]+2047.5625f),ftoi4(vy(py[i])+2047.5625f),0);
   q++;
   q->dw[0]=(u64)((ftoi4(uu[i+1])&0xFFFF))|((u64)(ftoi4(vv[i+1])&0xFFFF)<<16);
   q->dw[1]=GIF_SET_XYZ(ftoi4(px[i+1]+2047.5625f),ftoi4(vy(py[i+1])+2047.5625f),0);
   q++;
  }}
 g_frame_texture_quads++;
 return q;
}
static void reset_iop(void){SifInitRpc(0);while(!SifIopReset("rom0:UDNL rom0:EELOADCNF",0));while(!SifIopSync());SifInitRpc(0);SifLoadFileInit();}
static int pad_ready(void){for(;;){int s=padGetState(0,0);if(s==PAD_STATE_STABLE||s==PAD_STATE_FINDCTP1)return 0;if(s==PAD_STATE_DISCONN||s==PAD_STATE_ERROR)return -1;DelayThread(10000);}}
static int init_pad(void){if(SifLoadModule("rom0:SIO2MAN",0,NULL)<0)return -1;if(SifLoadModule("rom0:PADMAN",0,NULL)<0)return -2;if(!padInit(0)||!padPortOpen(0,0,g_pad_buffer)||pad_ready()<0)return -3;padSetMainMode(0,0,PAD_MMODE_DUALSHOCK,PAD_MMODE_LOCK);return pad_ready();}
static unsigned short buttons_pressed(void){static unsigned short old;unsigned short now=old,p;int s=padGetState(0,0);if((s==PAD_STATE_STABLE||s==PAD_STATE_FINDCTP1)&&padRead(0,0,&g_buttons))now=(unsigned short)(0xffffu^g_buttons.btns);p=now&(unsigned short)~old;old=now;g_held=now;return p;}
typedef struct {u32 address;int width,height,valid;} texture_extent_t;
#define TEXTURE_EXTENT_SLOTS 16
static texture_extent_t g_texture_extent[TEXTURE_EXTENT_SLOTS];
static void texture_extent_set(texbuffer_t*t,int width,int height){int i,free_slot=-1;for(i=0;i<TEXTURE_EXTENT_SLOTS;i++){if(g_texture_extent[i].valid&&g_texture_extent[i].address==t->address){g_texture_extent[i].width=width;g_texture_extent[i].height=height;return;}if(!g_texture_extent[i].valid&&free_slot<0)free_slot=i;}if(free_slot>=0){g_texture_extent[free_slot].valid=1;g_texture_extent[free_slot].address=t->address;g_texture_extent[free_slot].width=width;g_texture_extent[free_slot].height=height;}}
static int texture_extent_get(texbuffer_t*t,int*width,int*height){int i;for(i=0;i<TEXTURE_EXTENT_SLOTS;i++)if(g_texture_extent[i].valid&&g_texture_extent[i].address==t->address){*width=g_texture_extent[i].width;*height=g_texture_extent[i].height;return 1;}return 0;}
static qword_t *bind_texture_filter(qword_t*q,texbuffer_t*t,int linear){static u32 last_addr=0xffffffffu;lod_t l={0};clutbuffer_t c={0};texwrap_t w={0};int actual_w=1,actual_h=1;texture_extent_get(t,&actual_w,&actual_h);if(last_addr!=t->address){static u32 logged_addr[8]={0};int bi,seen=0;for(bi=0;bi<8;bi++){if(logged_addr[bi]==t->address){seen=1;break;}if(logged_addr[bi]==0){logged_addr[bi]=t->address;break;}}if(!seen)telemetry_event("bind_once",(int)t->address,((actual_w&0xffff)<<16)|(actual_h&0xffff));last_addr=t->address;}
/* TEXA, dentro do pacote de bind e nao na inicializacao.
   Em PSMCT16 o alfa tem 1 bit e o GS o expande por este registrador:
   TA0 vale para A=0 e TA1 para A=1. O padrao do ps2sdk e TA0=0x80, ou
   seja, OPACO, e por isso a 0.2.58 desenhou um retangulo preto solido
   do tamanho do quad da GF. Escrever TEXA uma vez na inicializacao nao
   adianta: draw_setup_environment roda a cada quadro e o sobrescreve.
   Aqui TA0=0 (transparente) e TA1=0x80 (opaco). Em PSMCT32 o GS ignora
   TEXA, entao isto nao afeta nenhuma outra textura. */
 if(t->psm==GS_PSM_16){q->dw[0]=GIF_SET_TAG(1,0,0,0,GIF_FLG_PACKED,1);q->dw[1]=GIF_REG_AD;q++;
  q->dw[0]=GS_SET_TEXA(0,0,0x80);q->dw[1]=GS_REG_TEXA;q++;}
 l.calculation=LOD_USE_K;l.max_level=0;l.mag_filter=linear?LOD_MAG_LINEAR:LOD_MAG_NEAREST;l.min_filter=linear?LOD_MIN_LINEAR:LOD_MIN_NEAREST;l.mipmap_select=LOD_MIPMAP_REGISTER;l.l=0;l.k=0;
 c.storage_mode=CLUT_STORAGE_MODE1;c.start=0;c.psm=0;c.load_method=CLUT_NO_LOAD;c.address=0;
/* WRAP_REGION_CLAMP, nao WRAP_CLAMP.
    No GS o modo CLAMP, que vale 1, grampeia a coordenada na TEXTURA
    INTEIRA, ou seja [0, 2^TW-1], e IGNORA os campos MINU/MAXU/MINV/MAXV.
    Quem usa esses campos e o modo REGION_CLAMP, que vale 2.
    O port preenchia os campos e pedia o modo 1: eles nunca valeram.
    Consequencia: as dimensoes reais quase nunca sao potencia de dois,
    entao sobrava area amostravel com LIXO de um upload anterior na
    mesma textura. Exemplos, com as medidas do menu:
      MERCH   193x62 -> TW=256 TH=64, 2 linhas de lixo abaixo da arte
      OPTIONS 244x56 -> TW=256 TH=64, 8 linhas de lixo
      CREDITS 220x62 -> TW=256 TH=64, 2 linhas de lixo
    Era o traco escuro de 1 px na base do MERCH, com a largura exata do
    quad, que resistiu a quatro hipoteses minhas: nao era wrap, nao era
    meio pixel, nao era a arte e nao era o fundo. A simulacao offline do
    menu, em tools/butterscotch/bs.py simular-menu, sai limpa justamente
    porque no PC nao existe VRAM suja embaixo da imagem.
    Com REGION_CLAMP os limites passam a valer e nada fora da area
    enviada pode ser amostrado. */
 w.horizontal=WRAP_REGION_CLAMP;w.vertical=WRAP_REGION_CLAMP;w.minu=0;w.maxu=actual_w-1;w.minv=0;w.maxv=actual_h-1;
 g_dl_tex=t->address;g_dl_tbw=t->width;g_dl_psm=t->psm;
 g_dl_tw=(short)actual_w;g_dl_th=(short)actual_h;g_dl_filtro=(short)linear;
 q=draw_texture_sampling(q,0,&l);q=draw_texture_wrapping(q,0,&w);return draw_texturebuffer(q,0,t,&c);}
static qword_t *bind_texture(qword_t*q,texbuffer_t*t){return bind_texture_filter(q,t,1);}
static void setup_texture_dims(texbuffer_t*t,int w,int h){packet_t*p=packet_init(128,PACKET_NORMAL);qword_t*q=p->data;texture_extent_set(t,w,h);
 t->info.width=draw_log2(w);t->info.height=draw_log2(h);t->info.components=TEXTURE_COMPONENTS_RGBA;t->info.function=TEXTURE_FUNCTION_MODULATE;
 q=bind_texture(q,t);q=draw_finish(q);dma_channel_send_normal(DMA_CHANNEL_GIF,p->data,q-p->data,0,0);draw_wait_finish();packet_free(p);}
static void setup_texture(texbuffer_t*t){setup_texture_dims(t,g_bf.width,g_bf.height);}
static unsigned short anim_button(int anim)
{
    if (anim == 1) return PAD_LEFT;
    if (anim == 2) return PAD_DOWN;
    if (anim == 3) return PAD_UP;
    if (anim == 4) return PAD_RIGHT;
    return 0;
}
static void upload_raw_psm(texbuffer_t*t,unsigned char*source,int w,int h,int psm){packet_t*p=packet_init(256,PACKET_NORMAL);qword_t*q=p->data;q=draw_texture_transfer(q,source,w,h,psm,t->address,t->width);q=draw_texture_flush(q);dma_channel_send_chain(DMA_CHANNEL_GIF,p->data,q-p->data,0,0);dma_wait_fast();packet_free(p);}
static void upload_raw(texbuffer_t*t,unsigned char*source,int w,int h){packet_t*p=packet_init(256,PACKET_NORMAL);qword_t*q=p->data;q=draw_texture_transfer(q,source,w,h,GS_PSM_32,t->address,t->width);q=draw_texture_flush(q);dma_channel_send_chain(DMA_CHANNEL_GIF,p->data,q-p->data,0,0);dma_wait_fast();packet_free(p);}

/* FAIXA VISIVEL, PUBLICADA PARA AUDITORIA.
   O harness de link + execucao (test/ps2harness/ps2h.c) nao tem rasterizador:
   ele conta chamadas e mede pacotes. Para ele poder PROVAR que nada eh
   desenhado fora da area de jogo -- que eh o defeito que o teste no console
   mostrou e que um rasterizador de brinquedo nao pegaria -- o jogo publica
   aqui a faixa que vale naquele momento, e o harness compara cada retangulo
   texturizado contra ela. Sem isso a unica forma de saber era olhar a captura
   de tela no console. */
int g_band_on=0,g_band_topo=0,g_band_base=0;
int g_ui_state_dbg=-1;   /* estado do menu, so para o harness auditar */
/* Contadores do recorte por software, para o log do proprio console. Sem eles
   o unico jeito de saber se a faixa foi respeitada era olhar captura de tela
   -- que eh justamente o que nao da para fazer de dentro do jogo. */
unsigned g_band_clip_rects=0,g_band_clip_lines=0,g_band_cull_rects=0;
static void band_set(int topo,int base){g_band_on=1;g_band_topo=topo;g_band_base=base;}
static void band_counters_reset(void){g_band_clip_rects=0;g_band_clip_lines=0;g_band_cull_rects=0;}

/* RECORTE DA FAIXA VISIVEL, EM SOFTWARE.
   O scissor do GS (draw_scissor_area com SCISSOR_TOPO/SCISSOR_BASE) nao esta
   bastando no console: nas capturas do teste o fundo do menu aparece das
   linhas 46 a 447 e um item inteiro (sprite 3, v 404..462) e desenhado com o
   scissor em 56..391. A tela de titulo, que usa o mesmo scissor, sai certinha
   em 56..391 -- ou seja, o problema nao e o valor do scissor e sim o fato de
   que um sprite grande o atravessa sem ser cortado. Nao da para diagnosticar
   isso de fora, entao a resposta e nao depender do scissor: recortar aqui e
   mandar so o pedaco que cabe na faixa. O resultado visual é identico ao de
   um scissor que funcione.
   y0..y1 e v0..v1 sao o retangulo fisico e o trecho de textura; saida pelos
   ponteiros. Devolve 0 quando nao ha nada visivel. */
static int band_clip_phys(float y0,float y1,float v0,float v1,float*yy0,float*yy1,float*vv0,float*vv1){
 float oy0=y0,ov0=v0,ov1=v1;
 float top=(float)SCISSOR_TOPO,base=(float)SCISSOR_BASE,h=y1-y0;
 if(h<=0.0f)return 0;
 if(y1<=top||y0>=base)return 0;
 /* AS DUAS FRACOES SAO MEDIDAS A PARTIR DO y0 ORIGINAL.
    A versao anterior media a segunda a partir do y0 JA cortado pelo primeiro
    if. Quando o sprite atravessa a faixa DOS DOIS LADOS -- que eh o caso do
    fundo do menu, 406,9 linhas fisicas contra uma faixa de 335 -- a fragao de
    baixo saida pequena demais, o trecho de textura vinha estreito e o sprite
    saia ESTICADO na vertical. Foi exatamente o que o teste no console viu:
    "o background se estica para cima e para baixo em vez de subir e descer".
    A conta do log mostra: h=335.00 com vh=205.56, escala 1.6297, quando a
    correta eh 406.893/256 = 1.5892 -- 2,55% de esticamento. */
 if(y0<top){float f=(top-oy0)/h;y0=top;v0=ov0+(ov1-ov0)*f;}
 if(y1>base){float f=(base-oy0)/h;if(f>1.0f)f=1.0f;y1=oy0+h*f;v1=ov0+(ov1-ov0)*f;}
 if(y1<=y0)return 0;
 *yy0=y0;*yy1=y1;*vv0=v0;*vv1=v1;
 return 1;
}

static int band_clip_v(float py0,float py1,float v0,float v1,
                      float*oy0,float*oy1,float*ov0,float*ov1){
 float antes,depois;
 if(!g_band_on)return 1;   /* faixa nao publicada: nao mexe em nada */
 if(!band_clip_phys(py0,py1,v0,v1,oy0,oy1,ov0,ov1)){
  g_band_cull_rects++;     /* inteiramente fora: nem sai para o GIF */
  return 0;
 }
 /* Quantas linhas o recorte tirou: a diferenca de altura, na unidade da tela. */
 antes=py1-py0; depois=*oy1-*oy0;
 if(antes>depois){
  g_band_clip_rects++;
  g_band_clip_lines+=(unsigned)(antes-depois+.5f);
 }
 return 1;
}

/* Ajuste do meio-texel: o inset de 0,5 px evita sangramento da textura
   vizinha. Quando o recorte encolhe o trecho para menos de um texel os dois
   extremos se cruzariam e o GIF inverteria a textura -- ai os dois ficam
   iguais, no centro do trecho que sobrou. */
static void band_clip_texel(float v0,float v1,float*lo,float*hi){
 float a=v0+.5f,b=v1-.5f;
 if(b<=a){float m=(v0+v1)*.5f;a=m;b=m;}
 *lo=a;*hi=b;
}

static qword_t *sprite_dims(qword_t*q,float x,float y,float w,float h,int tw,int th){texrect_t v;float cy0,cy1,cv0,cv1;
 if(!band_clip_v(vy(y),vy(y+h),0.0f,(float)th,&cy0,&cy1,&cv0,&cv1))return q;
 g_frame_texture_quads++;v.v0.x=FNX_INI(x);v.v0.y=FNX_INI(cy0);v.v0.z=0;v.v1.x=FNX_FIM(x+w);v.v1.y=FNX_FIM(cy1);v.v1.z=0;
 band_clip_texel(cv0,cv1,&v.t0.v,&v.t1.v);ch_uv_do_quadro(0,tw,&v.t0.u,&v.t1.u);
 v.color=rgba(0x80,0x80,0x80,0x80);dl_sprite(x,y,w,h,0.0f,0.0f,(float)tw,(float)th,0x80,&v);return draw_rect_textured(q,0,&v);}

/* O MESMO sprite_dims, ESPELHADO na horizontal.
   `CharacterData.flipX` e' dado: 14 dos 46 personagens tem `true` (todos os bf,
   todos os pico, o sserafim-sakura), e o original aplica em
   BaseCharacter.hx L192 (`this.flipX = _data.flipX`), que no flixel espelha a
   arte DENTRO do proprio quadro. O port copiava o quadro sem espelhar, entao
   esses 14 eram desenhados virados para o lado errado -- o BF do lado direito da
   tela tem de olhar para a esquerda, onde esta' o oponente.
   Espelhar e' trocar as duas coordenadas U do retangulo de textura; a posicao na
   tela e o recorte vertical nao mudam. */
static qword_t *sprite_dims_flip(qword_t*q,float x,float y,float w,float h,int tw,int th){texrect_t v;float cy0,cy1,cv0,cv1;
 if(!band_clip_v(vy(y),vy(y+h),0.0f,(float)th,&cy0,&cy1,&cv0,&cv1))return q;
 g_frame_texture_quads++;v.v0.x=FNX_INI(x);v.v0.y=FNX_INI(cy0);v.v0.z=0;v.v1.x=FNX_FIM(x+w);v.v1.y=FNX_FIM(cy1);v.v1.z=0;
 band_clip_texel(cv0,cv1,&v.t0.v,&v.t1.v);ch_uv_do_quadro(1,tw,&v.t0.u,&v.t1.u);
 v.color=rgba(0x80,0x80,0x80,0x80);dl_sprite(x,y,w,h,(float)tw,0.0f,(float)tw,(float)th,0x80,&v);return draw_rect_textured(q,0,&v);}

static qword_t *sprite_region(qword_t*q,float x,float y,float w,float h,float u,float v,float uw,float vh){texrect_t r;float cy0,cy1,cv0,cv1;
 if(!band_clip_v(vy(y),vy(y+h),v,v+vh,&cy0,&cy1,&cv0,&cv1))return q;
 g_frame_texture_quads++;r.v0.x=FNX_INI(x);r.v0.y=FNX_INI(cy0);r.v0.z=0;r.v1.x=FNX_FIM(x+w);r.v1.y=FNX_FIM(cy1);r.v1.z=0;
 band_clip_texel(cv0,cv1,&r.t0.v,&r.t1.v);r.t0.u=u+.5f;r.t1.u=u+uw-.5f;
 r.color=rgba(0x80,0x80,0x80,0x80);dl_sprite(x,y,w,h,u,cv0,uw,cv1-cv0,0x80,&r);return draw_rect_textured(q,0,&r);}
static qword_t *sprite_region_phys(qword_t*q,float x,float y0,float w,float hp,float u,float v,float uw,float vh,unsigned char a){texrect_t r;float cy0,cy1,cv0,cv1;
 if(!band_clip_v(y0,y0+hp,v,v+vh,&cy0,&cy1,&cv0,&cv1))return q;
 g_frame_texture_quads++;r.v0.x=FNX_INI(x);r.v0.y=FNX_INI(cy0);r.v0.z=0;r.v1.x=FNX_FIM(x+w);r.v1.y=FNX_FIM(cy1);r.v1.z=0;
 band_clip_texel(cv0,cv1,&r.t0.v,&r.t1.v);r.t0.u=u+.5f;r.t1.u=u+uw-.5f;
 r.color=rgba(0x80,0x80,0x80,a);dl_sprite(x,cy0,w,cy1-cy0,u,cv0,uw,cv1-cv0,a,&r);return draw_rect_textured(q,0,&r);}
/* Igual a sprite_region_alpha, mas com COR no vertice: e' o `color` do sprite
   do flixel (o `flavour` do TallyCounter). No GS o vertice multiplica a textura
   (TEXTURE_FUNCTION_MODULATE) e 0x80 vale 1,0 -- por isso a cor do flixel
   (0..255) e' dividida por 2 aqui: 0xFF vira 0x80, e o branco nao apaga nada. */
static qword_t *sprite_region_cor(qword_t*q,float x,float y,float w,float h,float u,float v,float uw,float vh,unsigned char cr,unsigned char cg,unsigned char cb){texrect_t r;float cy0,cy1,cv0,cv1;
 if(!band_clip_v(vy(y),vy(y+h),v,v+vh,&cy0,&cy1,&cv0,&cv1))return q;
 g_frame_texture_quads++;r.v0.x=FNX_INI(x);r.v0.y=FNX_INI(cy0);r.v0.z=0;r.v1.x=FNX_FIM(x+w);r.v1.y=FNX_FIM(cy1);r.v1.z=0;
 band_clip_texel(cv0,cv1,&r.t0.v,&r.t1.v);r.t0.u=u+.5f;r.t1.u=u+uw-.5f;
 r.color=rgba(cr,cg,cb,0x80);dl_sprite(x,cy0,w,cy1-cy0,u,cv0,uw,cv1-cv0,0x80,&r);return draw_rect_textured(q,0,&r);}
static qword_t *sprite_region_alpha(qword_t*q,float x,float y,float w,float h,float u,float v,float uw,float vh,unsigned char a){texrect_t r;float cy0,cy1,cv0,cv1;
 if(!band_clip_v(vy(y),vy(y+h),v,v+vh,&cy0,&cy1,&cv0,&cv1))return q;
 g_frame_texture_quads++;r.v0.x=FNX_INI(x);r.v0.y=FNX_INI(cy0);r.v0.z=0;r.v1.x=FNX_FIM(x+w);r.v1.y=FNX_FIM(cy1);r.v1.z=0;
 band_clip_texel(cv0,cv1,&r.t0.v,&r.t1.v);r.t0.u=u+.5f;r.t1.u=u+uw-.5f;
 r.color=rgba(0x80,0x80,0x80,a);dl_sprite(x,y,w,h,u,cv0,uw,cv1-cv0,a,&r);return draw_rect_textured(q,0,&r);}
static qword_t *text_draw(qword_t*q,const char*s,float x,float y,float scale,int align);

/* =====================================================================
   PALCO GENERICO: os props do palco, do pacote para o pool de VRAM.

   O tutorial desenhava o palco como UMA textura (mainstage_tutorial_
   parallax.rgba, 512x256) composta fora do jogo. Isso servia para um palco
   so'. Aqui cada prop do palco vem do seu proprio pacote CFZ4
   (stages/<palco>/<prop>.cfz4), ganha um lugar no pool (engine/stage_pool.c)
   e e' desenhado na posicao que o engine/stage_layout.c calculou.

   ORCAMENTO: o pool tem 2.056.192 B e o mainStage pede 1.697.032 B. O
   tutorial continua cabendo -- e' o primeiro palco que usa este caminho.

   RAM: o pacote NAO fica mais na RAM principal. Cada prop mantem aberto o
   arquivo mais a TABELA de (offset, tamanho) por quadro, e le' um bloco LZ4 por
   vez (engine/pack_stream.c) -- medido no teste: arquivo de 66.384 B custa
   2.053 B de RAM. Antes o arquivo inteiro era lido e mantido, e o teto
   PALCO_RAM_MAX 4 MB barrava oito dos 21 palcos com a arte real (o pior pacote,
   spookyMansion, tem 59,4 MB e a maquina tem 32 MB). O teto continua existindo,
   agora contando TABELAS: e' uma rede de seguranca, nao o orcamento.

   QUANDO DA' ERRADO: se um pacote nao abrir, o prop nao e' desenhado e o
   evento palco_falha diz qual. O estado 8, nesse caso, volta para a textura
   composta do tutorial (que continua no pool) -- nunca uma tela vazia.
   ===================================================================== */
static sp_t g_pool;   /* texturas do palco + as de tela que dividem a VRAM */
#define PALCO_MAX_PROPS  32
#define PALCO_RAM_MAX    (4u*1024u*1024u)   /* TABELAS dos props abertos, na RAM */

typedef struct {
    const char *stage;                  /* palco carregado, ou 0            */
    int n;                              /* props do palco                   */
    int first;                          /* primeiro indice em bs_props[]    */
    /* O pacote NAO fica na RAM: fica a TABELA dele aberta (fd + offsets) e um
       bloco comprimido por vez (engine/pack_stream.c). Antes o arquivo inteiro
       era lido para a RAM e mantido, e por isso existia o teto
       PALCO_RAM_MAX 4 MB -- com a arte real, oito dos 21 palcos nao abriam. */
    pst_pack_t pst[PALCO_MAX_PROPS];
    unsigned int bytes[PALCO_MAX_PROPS];
    unsigned int count[PALCO_MAX_PROPS];      /* quadros no pacote          */
    unsigned int frame_bytes[PALCO_MAX_PROPS];
    texbuffer_t tex[PALCO_MAX_PROPS];
    int residente[PALCO_MAX_PROPS];     /* 1 = tem endereco no pool         */
    int quadro[PALCO_MAX_PROPS];        /* ultimo quadro enviado, -1 = nenhum */
    int tentou[PALCO_MAX_PROPS];        /* 1 = ja' tentou abrir o pacote    */
    unsigned int ram_bytes;
    unsigned int falhas;
    unsigned int uploads;
    int desenhou;                       /* props desenhados na ultima chamada */
} palco_t;
static palco_t g_palco;
static unsigned char *g_palco_frame;    /* um quadro descomprimido */
static unsigned int g_palco_frame_cap;
static int g_palco_ok=0;                /* 1 = o palco desenha por props */

static int g_palco_fds_ok = 0;   /* 0 = os fds do vetor ainda nao foram zerados */

static void palco_soltar(void)
{
    int i;
    for (i = 0; i < PALCO_MAX_PROPS; i++) {
        /* fd ZERO nao e' "fechado": e' a entrada padrao. Na primeira chamada o
           vetor esta' zerado pela inicializacao estatica, entao ele e' marcado
           como fechado (-1) em vez de passar pelo close(). Sem isso o primeiro
           palco fecharia o stdin do processo. */
        if (g_palco_fds_ok) pst_fechar(&g_palco.pst[i]);
        else g_palco.pst[i].fd = -1;
        g_palco.bytes[i] = g_palco.count[i] = g_palco.frame_bytes[i] = 0;
        g_palco.residente[i] = 0;
        g_palco.quadro[i] = -1;
        g_palco.tentou[i] = 0;
    }
    g_palco_fds_ok = 1;
    g_palco.ram_bytes = 0;
    g_palco.desenhou = -1;
    if (g_palco.stage) sp_limpar(&g_pool);   /* devolve os props ao pool */
    g_palco.stage = 0;
    g_palco.n = 0;
    g_palco_ok = 0;
}

/* Abre o pacote de UM prop pelo leitor de streaming (engine/pack_stream.c):
   cabecalho + TABELA ficam na RAM, os quadros nao. Os erros sao os mesmos de
   antes, agora com nome (`pst_erro`) em vez de numero solto:
     -1 abrir  -2 cabecalho  -3 tabela  -4 quadro  -5 memoria
     -6 leitura  -7 bloco fora do arquivo  -8 LZ4
   O teto PALCO_RAM_MAX continua existindo, mas agora ele conta TABELAS: o
   pacote inteiro nao entra mais na conta. */
static int palco_pack_abrir(const char *path, int r)
{
    int rc;
    pst_pack_t *pk = &g_palco.pst[r];
    rc = pst_abrir(path, pk);
    if (rc != PST_OK) {
        telemetry_event("palco_pacote", r, rc);
        return rc;
    }
    if (g_palco.ram_bytes + pk->ram > PALCO_RAM_MAX) {
        pst_fechar(pk);
        telemetry_event("palco_pacote", r, -9);
        return -9;
    }
    g_palco.ram_bytes += pk->ram;
    g_palco.bytes[r] = pk->bytes;
    g_palco.count[r] = pk->count;
    g_palco.frame_bytes[r] = pk->frame_bytes;
    telemetry_event("palco_aberto", r, (int)(pk->bytes ? pk->ram * 100u / pk->bytes : 0u));
    /* A RAM QUE O PALCO REALMENTE COBRA. `pk->ram` e' a conta do leitor por
       quadro (engine/pack_stream.c: tabela de offsets + UM bloco comprimido),
       nao o pacote inteiro. O evento existia como razao (percentual do
       arquivo) e nao como numero absoluto; sem ele, "quanto o palco ocupa da
       RAM" so' tinha resposta na planilha. Agora tem resposta MEDIDA, e o
       teto (PALCO_RAM_MAX) vem junto para o harness comparar. */
    telemetry_event("palco_ram", (int)g_palco.ram_bytes, (int)PALCO_RAM_MAX);
    return 0;
}

static void palco_carregar(const char *stage)
{
    int si, i;
    palco_soltar();
    si = sl_stage_find(stage);
    if (si < 0) return;
    g_palco.stage = stage;
    g_palco.n = bs_stage_props[si].count;
    g_palco.first = bs_stage_props[si].first;
    if (g_palco.n > PALCO_MAX_PROPS) g_palco.n = PALCO_MAX_PROPS;
    for (i = 0; i < g_palco.n; i++) g_palco.quadro[i] = -1;
    telemetry_event("palco_entra", si, g_palco.n);
    /* PROPS QUE O JSON NAO MANDARIA ANIMAR: `danceEvery == 0` e sem
       `startingAnimation`, mas com mais de um quadro. No original eles NAO
       tocam nada por si: quem os anima e' um script de palco (phillyTraffic e'
       um), e esse script nao esta' neste repositorio. O stage_layout os deixa
       no primeiro quadro -- o que o dado diz -- e este evento e' o registro de
       que ha' movimento faltando por dado, nao por esquecimento. */
    {int congelados = 0, j;
     for (j = 0; j < g_palco.n; j++) {
         const bs_prop_t *p = &bs_props[g_palco.first + j];
         if (p->dance == 0 && p->start < 0 && p->anim_count > 0 &&
             bs_prop_anims[p->anim_first].count > 1) {
             telemetry_event("prop_sem_inicio", g_palco.first + j, p->frames);
             congelados++;
         }
     }
     if (congelados) telemetry_event("palco_props_sem_inicio", congelados, si);
    }
}

/* Abre o pacote do prop 'r' (indice relativo ao palco) na primeira vez que ele
   e' necessario. Devolve 0 se esta' pronto. */
static int palco_prop_pronto(const char *stage, int r)
{
    char path[128];
    const bs_prop_t *p;
    int rc;
    if (r < 0 || r >= g_palco.n) return -1;
    if (g_palco.pst[r].fd >= 0) return 0;
    if (g_palco.tentou[r]) return -2;          /* falhou antes: nao insiste por quadro */
    g_palco.tentou[r] = 1;
    p = &bs_props[g_palco.first + r];
    snprintf(path, sizeof(path), "mass:/FNFPS2/assets/%s", p->tex);
    rc = palco_pack_abrir(path, r);
    p = &bs_props[g_palco.first + r];   /* bs_props nao muda, mas o caminho passou por snprintf */
    (void)stage;
    if (rc != 0) {
        g_palco.falhas++;
        telemetry_event("palco_falha", g_palco.first + r, rc);
        return -3;
    }
    /* O pacote tem de casar com o tamanho que o pool reservou e com o que o
       layout vai desenhar. Um pacote de outro prop passaria despercebido ate'
       aparecer com a arte trocada na tela. */
    if (g_palco.frame_bytes[r] != (unsigned int)p->sw * (unsigned int)p->sh * 2u) {
        g_palco.falhas++;
        telemetry_event("palco_falha", p - bs_props, -9);
        g_palco.ram_bytes -= g_palco.pst[r].ram;
        pst_fechar(&g_palco.pst[r]);
        g_palco.bytes[r] = g_palco.count[r] = g_palco.frame_bytes[r] = 0;
        return -4;
    }
    if (g_palco.frame_bytes[r] > g_palco_frame_cap) {
        if (g_palco_frame) free(g_palco_frame);
        g_palco_frame = memalign(128, g_palco.frame_bytes[r]);
        g_palco_frame_cap = g_palco_frame ? g_palco.frame_bytes[r] : 0;
        if (!g_palco_frame) { g_palco.falhas++; return -5; }
    }
    return 0;
}

/* Sobe o quadro 'q' do prop 'r' para o pool. Devolve o texbuffer do prop, ou 0
   se nao deu. */
static texbuffer_t *palco_prop_textura(int r, int q)
{
    const bs_prop_t *p = &bs_props[g_palco.first + r];
    texbuffer_t *t = &g_palco.tex[r];
    unsigned int addr = 0;
    int rc;

    if (!g_palco.residente[r]) {
        rc = sp_prop(&g_pool, g_palco.first + r, &addr);
        if (rc < 0) {
            g_palco.falhas++;
            telemetry_event("palco_sem_vram", g_palco.first + r, (int)sp_livre(&g_pool));
            g_palco.tentou[r] = 1;   /* nao adianta insistir: nao cabe */
            return 0;
        }
        g_palco.residente[r] = 1;
        g_palco.quadro[r] = -1;
        t->address = addr;
        t->width = (p->sw + 63) & ~63;
        t->psm = GS_PSM_16;
        setup_texture_dims(t, p->sw, p->sh);
    }
    if (g_palco.quadro[r] != q) {
        if (pst_quadro(&g_palco.pst[r], (unsigned int)q, g_palco_frame) != PST_OK) {
            g_palco.falhas++;
            telemetry_event("palco_falha", g_palco.first + r, -10);
            return 0;
        }
        upload_raw_psm(t, g_palco_frame, p->sw, p->sh, GS_PSM_16);
        g_palco.quadro[r] = q;
        g_palco.uploads++;
    }
    return t;
}

/* Troca de tela: quem manda na VRAM e' o estado.
   Durante a musica o palco precisa de tudo (o pior palco pede 1.513.472 B dos
   2.383.872 B do pool, e o resto e' dos tres personagens e dos icones); no
   menu, no titulo e no story quem precisa sao gftex, stagetex e a `tex`. Entao:
     entrando no jogo -> solta as tres (o palco pode despeja-las);
     saindo do jogo   -> limpa os props, solta os tres personagens e volta a
                         fixar as tres texturas de tela, com endereco novo se
                         elas tiverem sido despejadas.
   Sem esse caminho de volta, uma textura de menu ficaria apontando para VRAM
   que agora e' de um prop -- e o sintoma seria arte trocada na tela, sem log
   nenhum dizendo por que. */
static void palco_ui_entrou(texbuffer_t *gf, texbuffer_t *stage, texbuffer_t *tex,
                           int novo_ui)
{
    unsigned int a = 0;
    if (novo_ui == 8 || novo_ui == 10) {
        /* Na musica (e no Game Over, que desenha por cima dela e usa os mesmos
           personagens): gftex, stagetex e a `tex` do menu nao servem para nada (o
           palco vem dos pacotes e o HUD tem textura propria) -- elas viram
           espaco para os props, para os tres personagens e para os icones. */
        sp_soltar(&g_pool, SP_CHAVE_TELA_BASE + 0u);
        sp_soltar(&g_pool, SP_CHAVE_TELA_BASE + 1u);
        sp_soltar(&g_pool, SP_CHAVE_TELA_BASE + 3u);
        telemetry_event("pool_cena", 8, (int)sp_livre(&g_pool));
        return;
    }
    /* Saindo da musica: os tres personagens deixam de ser fixos ANTES do
       sp_limpar, senao eles ficariam presos na VRAM da proxima cena. */
    sp_soltar(&g_pool, SP_CHAVE_TELA_BASE + 2u);
    sp_soltar(&g_pool, SP_CHAVE_TELA_BASE + 4u);
    sp_soltar(&g_pool, SP_CHAVE_TELA_BASE + 5u);
    sp_limpar(&g_pool);       /* dois cliques: solta o que nao for fixo */
    /* A `tex` do menu volta a ser fixa, no endereco que o pool der. O conteudo
       dela e' reenviado por quem desenha a tela (o titulo, o menu e o story
       todos reenviam o que precisam ao entrar). */
    if (sp_chave(&g_pool, SP_CHAVE_TELA_BASE + 3u, 320u * 256u * 4u, &a) >= 0) {
        tex->address = a;
        sp_fixar(&g_pool, SP_CHAVE_TELA_BASE + 3u);
    }
    if (sp_chave(&g_pool, SP_CHAVE_TELA_BASE + 0u, 256u * 256u * 4u, &a) >= 0) {
        gf->address = a;
        sp_fixar(&g_pool, SP_CHAVE_TELA_BASE + 0u);
    }
    if (sp_chave(&g_pool, SP_CHAVE_TELA_BASE + 1u, 512u * 256u * 4u, &a) >= 0) {
        stage->address = a;
        sp_fixar(&g_pool, SP_CHAVE_TELA_BASE + 1u);
    }
    telemetry_event("pool_cena", novo_ui, (int)sp_livre(&g_pool));
}

/* ============ FREEPLAY: AS CAPSULAS DE MUSICA ============
   `SongMenuItem.hx` L84-87: cada musica e' um Sparrow de 16 quadros (8 da
   animacao `selected`, 8 da `unselected`) a 24 fps, desenhado a
   `realScaled = 0,8`; `intendedX/intendedY` (L762-770) poem a lista numa curva
   de seno (x) e num passo vertical (y), e o sprite PERSEGUE o alvo com
   `smoothLerpPrecision` (0,256 s em x, 0,192 s em y -- L735-736). A entrada e'
   o `initJumpIn` (L624-647 e L670-700): uma casa de escala por 1/24 s, com
   `xFrames[k]` em x e o INVERSO em y, saindo de `x = FlxG.width` (fora da
   tela) e aparecendo com alpha 1 depois de `(7/24)*2,5` s.

   A arte vem de `tools/empacotar_freeplay.py`: um CFZ4 com os 16 quadros da
   capsula (306x66, 80.784 B por quadro) e um RGBA por titulo (no maximo
   20.672 B). Na VRAM ficam DOIS quadros de capsula -- o da selecionada e o da
   nao selecionada -- e ate' oito titulos, num cache que troca por leitura
   quando a lista rola. */
#define FP_DIR "mass:/FNFPS2/assets/"
#define FP_CHAVE_CAP (SP_CHAVE_TELA_BASE + 0x40u)   /* +0 selecionada, +1 nao */
#define FP_CHAVE_TIT (SP_CHAVE_TELA_BASE + 0x50u)   /* os 27 titulos (1 por musica) */
#define FP_CHAVE_FUNDO (SP_CHAVE_TELA_BASE + 0x42u) /* o fundo (1 quadro, 16 bpp) */
#define FP_CHAVE_DJ    (SP_CHAVE_TELA_BASE + 0x43u) /* o DJ (1 quadro, 16 bpp)    */
#define FP_TIT_SLOTS 8
#define FP_JANELA    9      /* quantas capsulas a tela desenha por vez      */

static pst_pack_t g_fpc_pack;
static int g_fpc_ok = 0, g_fpc_tentou = 0;
static unsigned char *g_fpc_buf = 0;
static unsigned int g_fpc_buf_bytes = 0;
static unsigned int g_fpc_addr[2] = {0, 0};
static int g_fpc_addr_ok[2] = {0, 0};
static int g_fpc_frame[2] = {-1, -1};
static int g_fpc_desenhos = 0;
/* O FUNDO (`backingImage`): um CFZ4 de UM quadro em 16 bpp (PSMCT16), carregado
   uma vez ao entrar na tela. 574x360 = 413.280 B -- em 32 bpp seriam 826.560,
   metade do pool da Freeplay, e a regra do projeto e' imagem ceder. */
static pst_pack_t g_fpf_pack;
static int g_fpf_ok = 0, g_fpf_tentou = 0;
static unsigned char *g_fpf_buf = 0;
static unsigned int g_fpf_buf_bytes = 0, g_fpf_addr = 0;
static int g_fpf_logado = 0;
static texbuffer_t g_fpf_tex;
/* O DJ da tela (`AnimateAtlasFreeplayDJ`). O pacote tem 678 quadros de 370x375
   a 16 bpp em DOIS arquivos (o `write_packs` corta em 8 MB), lidos por
   `pack_multi`: fica na RAM a tabela e um bloco por parte, e na VRAM UM quadro
   por vez -- 277.500 B. A maquina de estados e' a da fonte (L62-140):
     Intro -> Idle (loop) -> Confirm | IdleEasterEgg | Cartoon | FistPump
   `timeIdling` conta so' no Idle e zera em qualquer acao do jogador
   (`onPlayerAction` -> `resetAFKTimer`). */
#define FP_DJ_INTRO    0
#define FP_DJ_IDLE     1
#define FP_DJ_CONFIRM  2
#define FP_DJ_EGG      3
#define FP_DJ_CARTOON  4
#define FP_DJ_FIST     5      /* fistPumpIntro: repete [ini..introFim]  */
#define FP_DJ_LOSS     6      /* fistPumpLossIntro                      */
#define FP_DJ_LOOP_PUMP 7     /* fistPump: [loopIni..loopFim] uma vez   */
#define FP_DJ_LOOP_LOSS 8     /* fistPumpLoss                           */
#define FP_DJ_EGG_PERIODO_MS   60000    /* IDLE_EGG_PERIOD = 60 s     */
#define FP_DJ_CARTOON_PERIODO_MS 120000 /* IDLE_CARTOON_PERIOD = 120 s */
static pm_pack_t g_dj_pack;
static int g_dj_ok = 0, g_dj_tentou = 0;
static unsigned char *g_dj_buf = 0;
static unsigned int g_dj_buf_bytes = 0, g_dj_addr = 0;
static int g_dj_frame = -1;         /* quadro que esta' na VRAM */
/* Os estados `FistPumpIntro`/`FistPump` nao usam `indice`: eles tocam uma
   JANELA de quadros (`curFrame >= endFrame` -> recomeca). O quadro pedido por
   eles vem aqui (-1 = "usa o indice normal"). */
static int g_dj_forcado = -1;
/* O rank da volta do resultado (`fromResultsParams?.newRank`), -1 = nenhum. */
static int g_dj_volta_rank = -1;
static texbuffer_t g_dj_tex;
static int g_dj_estado = FP_DJ_INTRO;
static unsigned long g_dj_t_estado = 0;   /* ms dentro do estado */
static unsigned long g_dj_idle_ms = 0;    /* `timeIdling` */
static int g_dj_viu_egg = 0;              /* `seenIdleEasterEgg` */
static int g_dj_logado = 0;
static int g_dj_desenhos = 0;
static unsigned char g_fpt_buf[24576];
/* Um slot POR MUSICA: os 27 titulos somam 122.740 B de VRAM (o maior,
   `spaghetti`, 20.672 B), e todos cabem no pool da tela -- entao eles sobem
   TODOS na abertura e a lista rola SEM I/O. O cache de 8 slots anterior
   re lia um arquivo por quadro quando a janela passava de oito musicas. */
static struct { int song; unsigned int addr; int w, h, ok; } g_fpt[FREEPLAY_MAX];
static unsigned int g_fpt_bytes = 0;
static int g_fpt_falhas = 0;
/* O estado de cada capsula: e' POR MUSICA (a posicao persegue o alvo com lerp,
   e a animacao recomeca quando ela passa a ser a selecionada). */
typedef struct { int sel; unsigned long t_anim; float x, y; int vivo;
              int texto_ok; unsigned long t_texto; } fp_cap_st;
static fp_cap_st g_fpcs[FREEPLAY_MAX];
static u64 g_fp_t0 = 0;
static int g_fp_ms = 0;
static int g_fp_cap_logado = 0;
/* O `texbuffer_t` ja' montado de cada um dos dois quadros de capsula (o
   `freeplay_capsula_quadro` devolve o que subiu, para a tela nao remontar). */
static texbuffer_t g_fptex_cap[2];
static texbuffer_t g_fpc_tpl;   /* o texbuffer de onde os uploads copiam o resto */
static texbuffer_t g_fptex_sel, g_fptex_nao;
#define FP_LARGURA 640.0f   /* `FlxG.width` do original (1280) na metade */
/* O desenho da capsula e' `capsule.width/height * realScaled(0,8) * metade`,
   vezes a escala do salto de entrada. */
static float fp_desenho_w(float s) { return FP_DESENHO_W * s; }
static float fp_desenho_h(float s) { return FP_DESENHO_H * s; }

static void freeplay_dj_soltar(void);
static void freeplay_dj_carregar(texbuffer_t *tpl);
static void freeplay_fundo_carregar(texbuffer_t *tpl);

static void freeplay_capsula_soltar(void)
{
    int i;
    freeplay_dj_soltar();
    /* O fundo sai junto: e' a mesma tela e o mesmo pool. */
    if (g_fpf_tentou) pst_fechar(&g_fpf_pack);
    g_fpf_tentou = 0; g_fpf_ok = 0; g_fpf_logado = 0;
    if (g_fpf_buf) { free(g_fpf_buf); g_fpf_buf = 0; g_fpf_buf_bytes = 0; }
    if (g_fpf_addr) { sp_soltar(&g_pool, FP_CHAVE_FUNDO); g_fpf_addr = 0; }
    if (g_fpc_tentou) pst_fechar(&g_fpc_pack);
    g_fpc_ok = 0; g_fpc_tentou = 0;
    if (g_fpc_buf) { free(g_fpc_buf); g_fpc_buf = 0; g_fpc_buf_bytes = 0; }
    for (i = 0; i < 2; i++) {
        sp_soltar(&g_pool, FP_CHAVE_CAP + (unsigned)i);
        g_fpc_addr_ok[i] = 0; g_fpc_addr[i] = 0; g_fpc_frame[i] = -1;
    }
    for (i = 0; i < FREEPLAY_MAX; i++) {
        if (g_fpt[i].ok || g_fpt[i].addr) sp_soltar(&g_pool, FP_CHAVE_TIT + (unsigned)i);
        g_fpt[i].ok = 0; g_fpt[i].song = -1; g_fpt[i].addr = 0;
    }
    g_fpt_bytes = 0; g_fpt_falhas = 0;
    for (i = 0; i < FREEPLAY_MAX; i++) {
        g_fpcs[i].vivo = 0; g_fpcs[i].sel = 0; g_fpcs[i].t_anim = 0;
    }
}

/* O FUNDO da tela de Freeplay (`backingImage`). Um quadro so', em 16 bpp: ele
   sobe uma vez por entrada e fica ate' a tela sair. Sem o arquivo, o desenho
   cai no retangulo escuro (a tela nao quebra, e a telemetria diz o motivo). */
static void freeplay_fundo_carregar(texbuffer_t *tpl)
{
    char caminho[128];
    unsigned int bytes;
    g_fpf_tentou = 0; g_fpf_ok = 0; g_fpf_logado = 0;
    snprintf(caminho, sizeof(caminho), FP_DIR "%s", FP_FUNDO_TEX);
    if (pst_abrir(caminho, &g_fpf_pack) != PST_OK) {
        telemetry_event("freeplay_fundo_falta", 0, (int)sp_livre(&g_pool));
        return;
    }
    g_fpf_tentou = 1;
    bytes = g_fpf_pack.frame_bytes;
    if (bytes != (unsigned int)FP_FUNDO_W * (unsigned int)FP_FUNDO_H * 2u) {
        telemetry_event("freeplay_fundo_tamanho", (int)bytes,
                        FP_FUNDO_W * FP_FUNDO_H * 2);
        pst_fechar(&g_fpf_pack); g_fpf_tentou = 0;
        return;
    }
    if (bytes > g_fpf_buf_bytes) {
        free(g_fpf_buf);
        g_fpf_buf = (unsigned char *)memalign(64, (size_t)bytes);
        g_fpf_buf_bytes = g_fpf_buf ? bytes : 0u;
    }
    if (!g_fpf_buf) { pst_fechar(&g_fpf_pack); g_fpf_tentou = 0; return; }
    if (sp_chave(&g_pool, FP_CHAVE_FUNDO, bytes, &g_fpf_addr) < 0) {
        telemetry_event("freeplay_fundo_sem_vram", 0, (int)sp_livre(&g_pool));
        pst_fechar(&g_fpf_pack); g_fpf_tentou = 0; g_fpf_addr = 0;
        return;
    }
    if (pst_quadro(&g_fpf_pack, 0, g_fpf_buf) != PST_OK) {
        telemetry_event("freeplay_fundo_quadro", 0, 0);
        sp_soltar(&g_pool, FP_CHAVE_FUNDO); g_fpf_addr = 0;
        pst_fechar(&g_fpf_pack); g_fpf_tentou = 0;
        return;
    }
    {
        texbuffer_t t = tpl ? *tpl : g_fpc_tpl;
        t.width = FP_FUNDO_W;
        t.psm = GS_PSM_16;
        t.address = g_fpf_addr;
        setup_texture_dims(&t, FP_FUNDO_W, FP_FUNDO_H);
        FlushCache(0);
        upload_raw_psm(&t, g_fpf_buf, FP_FUNDO_W, FP_FUNDO_H, GS_PSM_16);
        g_fpf_tex = t;
    }
    g_fpf_ok = 1;
    telemetry_event("freeplay_fundo", 1, (int)bytes);
}

/* O DJ DA TELA. Abre as partes do pacote (CHP4 divide em 8 MB), reserva UM
   quadro de 16 bpp na VRAM e deixa o estado no comeco (`Intro`, que toca a
   animacao 'intro' uma vez e cai no `Idle`). Sem o pacote, a tela continua
   legivel: o fundo, as capsulas e os titulos nao dependem dele. */
static void freeplay_dj_soltar(void)
{
    if (g_dj_tentou) pm_fechar(&g_dj_pack);
    g_dj_tentou = 0; g_dj_ok = 0; g_dj_logado = 0; g_dj_frame = -1;
    if (g_dj_buf) { free(g_dj_buf); g_dj_buf = 0; g_dj_buf_bytes = 0; }
    if (g_dj_addr) { sp_soltar(&g_pool, FP_CHAVE_DJ); g_dj_addr = 0; }
    g_dj_estado = FP_DJ_INTRO;
    g_dj_t_estado = 0; g_dj_idle_ms = 0; g_dj_viu_egg = 0; g_dj_desenhos = 0;
    g_dj_forcado = -1;
}

static void freeplay_dj_carregar(texbuffer_t *tpl)
{
    char dir[64];
    unsigned int bytes;
    freeplay_dj_soltar();
    snprintf(dir, sizeof(dir), FP_DIR "freeplay");
    if (pm_abrir(dir, fp_dj_partes, FP_DJ_PARTES_N, &g_dj_pack) != 0) {
        telemetry_event("freeplay_dj_falta", 0, (int)sp_livre(&g_pool));
        return;
    }
    g_dj_tentou = 1;
    bytes = g_dj_pack.frame_bytes;
    if (bytes != (unsigned int)FP_DJ_W * (unsigned int)FP_DJ_H * 2u ||
        g_dj_pack.total != (unsigned int)FP_DJ_QUADROS) {
        telemetry_event("freeplay_dj_tamanho", (int)bytes, (int)g_dj_pack.total);
        pm_fechar(&g_dj_pack); g_dj_tentou = 0;
        return;
    }
    if (bytes > g_dj_buf_bytes) {
        free(g_dj_buf);
        g_dj_buf = (unsigned char *)memalign(64, (size_t)bytes);
        g_dj_buf_bytes = g_dj_buf ? bytes : 0u;
    }
    if (!g_dj_buf) { pm_fechar(&g_dj_pack); g_dj_tentou = 0; return; }
    if (sp_chave(&g_pool, FP_CHAVE_DJ, bytes, &g_dj_addr) < 0) {
        telemetry_event("freeplay_dj_sem_vram", 0, (int)sp_livre(&g_pool));
        pm_fechar(&g_dj_pack); g_dj_tentou = 0; g_dj_addr = 0;
        return;
    }
    {
        texbuffer_t t = tpl ? *tpl : g_fpc_tpl;
        t.width = FP_DJ_W;
        t.psm = GS_PSM_16;
        t.address = g_dj_addr;
        setup_texture_dims(&t, FP_DJ_W, FP_DJ_H);
        g_dj_tex = t;
    }
    g_dj_ok = 1;
    telemetry_event("freeplay_dj", 1, (int)bytes);
}

static int freeplay_capsula_carregar(texbuffer_t *tpl)
{
    char caminho[128];
    unsigned int bytes;
    int i;
    freeplay_capsula_soltar();
    if (tpl) g_fpc_tpl = *tpl;
    snprintf(caminho, sizeof(caminho), FP_DIR "%s", FP_CAP_TEX);
    if (pst_abrir(caminho, &g_fpc_pack) != PST_OK) {
        g_fpc_tentou = 0;
        telemetry_event("freeplay_sem_capsula", 0, (int)sp_livre(&g_pool));
        return 0;
    }
    g_fpc_tentou = 1;
    bytes = g_fpc_pack.frame_bytes;
    if (bytes > g_fpc_buf_bytes) {
        free(g_fpc_buf);
        g_fpc_buf = (unsigned char *)memalign(64, (size_t)bytes);
        g_fpc_buf_bytes = g_fpc_buf ? bytes : 0u;
    }
    if (!g_fpc_buf) {
        pst_fechar(&g_fpc_pack);
        g_fpc_tentou = 0;
        return 0;
    }
    for (i = 0; i < 2; i++) {
        unsigned int addr = 0;
        if (sp_chave(&g_pool, FP_CHAVE_CAP + (unsigned)i, bytes, &addr) >= 0) {
            g_fpc_addr[i] = addr;
            g_fpc_addr_ok[i] = 1;
            g_fpc_frame[i] = -1;
        }
    }
    g_fpc_ok = (g_fpc_addr_ok[0] && g_fpc_addr_ok[1]);
    telemetry_event("freeplay_capsula", g_fpc_ok, (int)bytes);
    /* OS TITULOS: todos de uma vez, porque somados eles sao menores que o
       maior quadro de capsula (ver o comentario de `g_fpt`). */
    for (i = 0; i < g_fp_n && i < FREEPLAY_MAX; i++) {
        const sr_song_t *song = g_fp[i];
        const fp_titulo_t *t;
        char caminho[192];
        unsigned int b, addr = 0;
        if (!song) continue;
        t = fp_titulo(song->id);
        if (!t || !t->w || !t->h) { g_fpt_falhas++; continue; }
        b = (unsigned)t->w * (unsigned)t->h * 4u;
        if (b > sizeof(g_fpt_buf)) { g_fpt_falhas++; continue; }
        snprintf(caminho, sizeof(caminho), FP_DIR "freeplay/%s", t->arquivo);
        if (read_exact_path(caminho, g_fpt_buf, (int)b) != 0) {
            telemetry_event("freeplay_titulo_falta", i, 0);
            g_fpt_falhas++;
            continue;
        }
        if (sp_chave(&g_pool, FP_CHAVE_TIT + (unsigned)i, b, &addr) < 0) {
            telemetry_event("freeplay_titulo_sem_vram", i, (int)sp_livre(&g_pool));
            g_fpt_falhas++;
            continue;
        }
        {
            texbuffer_t up = g_fpc_tpl;
            up.width = (unsigned)t->w;
            up.psm = GS_PSM_32;
            up.address = addr;
            setup_texture_dims(&up, (int)t->w, (int)t->h);
            FlushCache(0);
            upload_raw(&up, g_fpt_buf, (int)t->w, (int)t->h);
        }
        g_fpt[i].song = i;
        g_fpt[i].addr = addr;
        g_fpt[i].w = (int)t->w;
        g_fpt[i].h = (int)t->h;
        g_fpt[i].ok = 1;
        g_fpt_bytes += b;
    }
    telemetry_event("freeplay_titulos", g_fpt_bytes, g_fpt_falhas);
    freeplay_fundo_carregar(tpl);
    freeplay_dj_carregar(tpl);
    return g_fpc_ok;
}

/* Leva o quadro `quadro` da animacao `sel` para a VRAM. Devolve 1 quando o
   quadro esta' la' (o cache evita reenviar o mesmo quadro duas vezes). */
static int freeplay_capsula_quadro(texbuffer_t *tpl, int sel, int quadro)
{
    texbuffer_t t;
    unsigned int frame;
    if (!g_fpc_ok || !g_fpc_buf || sel < 0 || sel > 1) return 0;
    if (quadro < 0) return 0;
    if (g_fpc_frame[sel] == quadro) return 1;
    frame = (unsigned int)((sel ? FP_CAP_SEL_INI : FP_CAP_NAO_INI) + quadro);
    if (pst_quadro(&g_fpc_pack, frame, g_fpc_buf) != PST_OK) return 0;
    t = *tpl;
    t.width = FP_CAP_W;
    t.psm = GS_PSM_32;
    t.address = g_fpc_addr[sel];
    setup_texture_dims(&t, FP_CAP_W, FP_CAP_H);
    FlushCache(0);
    upload_raw(&t, g_fpc_buf, FP_CAP_W, FP_CAP_H);
    g_fpc_frame[sel] = quadro;
    g_fptex_cap[sel] = t;
    if (sel) g_fptex_sel = t; else g_fptex_nao = t;
    g_fpc_desenhos++;
    return 1;
}

/* ---- O DJ: a maquina de estados e o quadro da vez ----------------------
 *
 * `AnimateAtlasFreeplayDJ.update` (L62-140) escolhe a animacao por estado e o
 * `onFinishAnim` (L172-230) decide para onde ir quando ela termina:
 *
 *   Intro      'intro'      sem loop -> ao terminar, Idle   (L174-185)
 *   Idle       'idle'       LOOP     -> a cada volta olha `timeIdling`
 *   Confirm    'confirm'    sem loop -> fica no ultimo quadro (L197-200)
 *   IdleEasterEgg 'idleEasterEgg' -> ao terminar, Idle      (L205-209)
 *   Cartoon    'cartoon'    sem loop -> fica (nao ha' branch no onFinish)
 *   FistPump   'fistPump' / 'loss' com as janelas do `fistPump` do JSON
 *
 * `timeIdling` (BaseFreeplayDJ L84-127) so' cresce no Idle e zera em qualquer
 * acao do jogador (`onPlayerAction` -> `resetAFKTimer`). No fim de uma volta do
 * Idle: >= 60 s e ainda nao viu -> IdleEasterEgg; >= 120 s -> Cartoon.
 */
static const fp_dj_anim_t *freeplay_dj_anim(const char *nome)
{
    int i;
    /* Compara so' os `strlen(nome)` primeiros bytes: a tabela do CHP4 vem do
       pacote e pode trazer o nome preenchido com espacos. */
    for (i = 0; i < (int)FP_DJ_ANIMS_N; i++)
        if (strncmp(fp_dj_anims[i].nome, nome, strlen(nome)) == 0) {
            const char *r = fp_dj_anims[i].nome + strlen(nome);
            while (*r == ' ') r++;
            if (*r == '\0') return &fp_dj_anims[i];
        }
    return 0;
}

/* Quadro da vez dentro da animacao `a`, com `ms` desde que ela comecou. */
static int freeplay_dj_indice(const fp_dj_anim_t *a, unsigned long ms)
{
    unsigned long passo;
    int k;
    if (!a || a->quantos <= 0) return -1;
    passo = (unsigned long)(a->fps100 > 0 ? a->fps100 : 2400) / 100ul; /* fps */
    if (!passo) passo = 24ul;
    k = (int)((ms * passo) / 1000ul);
    if (a->looped) {
        /* `addByPrefix(..., 24)` com `looped`: cicla. */
        if (a->quantos > 0) k %= a->quantos;
    } else if (k >= a->quantos) {
        k = a->quantos - 1;   /* `looped = false` para no ULTIMO quadro */
    }
    return a->primeiro + k;
}

/* Duracao (ms) de uma passada da animacao. */
static unsigned long freeplay_dj_dura(const fp_dj_anim_t *a)
{
    unsigned long fps;
    if (!a || a->quantos <= 0) return 0ul;
    fps = (unsigned long)(a->fps100 > 0 ? a->fps100 : 2400) / 100ul;
    if (!fps) fps = 24ul;
    return ((unsigned long)a->quantos * 1000ul + fps - 1ul) / fps;
}

/* Quadro DENTRO da animacao para uma janela [ini..fim] que se repete.
   E' o que `playFlashAnimation(..., Frame)` + o `update` do estado fazem. */
static int freeplay_dj_janela(const fp_dj_anim_t *a, unsigned long ms,
                              int ini, int fim, int repete)
{
    unsigned long fps;
    int k, n;
    if (!a || a->quantos <= 0) return 0;
    fps = (unsigned long)(a->fps100 > 0 ? a->fps100 : 2400) / 100ul;
    if (!fps) fps = 24ul;
    /* `curFrame >= endFrame` recomeca: o periodo visivel e' [ini .. fim-1]. */
    n = fim - ini;
    if (n <= 0) n = 1;
    k = (int)((ms * fps) / 1000ul);
    if (repete) k %= n;
    if (k > n - 1) k = n - 1;
    k += ini;
    if (k >= a->quantos) k = a->quantos - 1;
    if (k < 0) k = 0;
    return k;
}

/* Duracao (ms) de uma janela [ini..fim] tocada uma vez. */
static unsigned long freeplay_dj_dura_janela(const fp_dj_anim_t *a,
                                             int ini, int fim)
{
    unsigned long fps;
    if (!a) return 0ul;
    fps = (unsigned long)(a->fps100 > 0 ? a->fps100 : 2400) / 100ul;
    if (!fps) fps = 24ul;
    if (fim < ini) return 0ul;
    return ((unsigned long)(fim - ini + 1) * 1000ul + fps - 1ul) / fps;
}

/* O aniversario de uma acao do jogador: `dj.onPlayerAction()`, que e'
   so' `resetAFKTimer()` (BaseFreeplayDJ.hx L68-72). Ele NAO troca de estado --
   e' o que zera o `timeIdling` que conta para o Easter Egg e o Cartoon. */
static void freeplay_dj_acao(void)
{
    g_dj_idle_ms = 0;
    g_dj_viu_egg = 0;
}

/* Troca de estado do DJ, zerando o tempo da animacao (todo `playFlashAnimation`
   da fonte comeca do quadro 0 da janela). */
static void freeplay_dj_estado_novo(int estado, const char *porque)
{
    if (estado == g_dj_estado) { g_dj_t_estado = 0; return; }
    g_dj_estado = estado;
    g_dj_t_estado = 0;
    g_dj_forcado = -1;
    telemetry_event("freeplay_dj_estado", estado, (int)g_dj_idle_ms);
    (void)porque;
}

/* `dj.onConfirm()` (BaseFreeplayDJ.hx L128-149): `Confirm`, a nao ser que haja
   personagem novo (NewUnlock). O port nao guarda "personagem novo" no save --
   o original so' marca isso quando um novo jogador e' desbloqueado
   (`PlayerRegistry.hasNewCharacter()`); sem esse estado no save, o ramo
   NewUnlock e' dado ausente e o Confirm e' o caminho. */
static void freeplay_dj_confirmar(void)
{
    if (!g_dj_ok) return;
    freeplay_dj_estado_novo(FP_DJ_CONFIRM, "onConfirm");
}

/* `fistPumpIntro()` / `fistPumpLossIntro()` (BaseFreeplayDJ.hx L171-190,
   L202-221), chamados ao VOLTAR do resultado (`FreeplayState.hx` L1084) com o
   rank da musica que acabou de ser jogada. */
static void freeplay_dj_fist_pump(int rank)
{
    if (!g_dj_ok) return;
    g_dj_idle_ms = 0; g_dj_viu_egg = 0;
    if (rank == (int)FNF_RANK_SHIT) {
        freeplay_dj_estado_novo(FP_DJ_LOSS, "fistPumpLossIntro");
    } else {
        freeplay_dj_estado_novo(FP_DJ_FIST, "fistPumpIntro");
    }
}

/* Um passo por quadro: anda o tempo do estado e troca de estado quando a
   animacao termina, exatamente como o `onFinishAnim` da fonte. */
static void freeplay_dj_passo(void)
{
    const fp_dj_anim_t *a;
    unsigned long dur;
    if (!g_dj_ok) return;
    if (g_dj_estado != FP_DJ_FIST && g_dj_estado != FP_DJ_LOSS &&
        g_dj_estado != FP_DJ_LOOP_PUMP && g_dj_estado != FP_DJ_LOOP_LOSS)
        g_dj_forcado = -1;
    g_dj_t_estado += (unsigned long)(TUT_DT * 1000.0f);
    if (g_dj_estado == FP_DJ_IDLE) g_dj_idle_ms += (unsigned long)(TUT_DT * 1000.0f);
    switch (g_dj_estado) {
    case FP_DJ_INTRO:
        a = freeplay_dj_anim("intro");
        if (g_dj_t_estado >= freeplay_dj_dura(a)) {
            g_dj_estado = FP_DJ_IDLE; g_dj_t_estado = 0;
            telemetry_event("freeplay_dj_estado", FP_DJ_IDLE, (int)g_dj_idle_ms);
        }
        break;
    case FP_DJ_IDLE:
        a = freeplay_dj_anim("idle");
        dur = freeplay_dj_dura(a);
        if (dur && g_dj_t_estado >= dur) {
            /* Fim de UMA volta: e' aqui que o original olha o `timeIdling`. */
            g_dj_t_estado = 0;
            if (g_dj_idle_ms >= FP_DJ_EGG_PERIODO_MS && !g_dj_viu_egg) {
                g_dj_estado = FP_DJ_EGG; g_dj_viu_egg = 1;
                telemetry_event("freeplay_dj_estado", FP_DJ_EGG, (int)g_dj_idle_ms);
            } else if (g_dj_idle_ms >= FP_DJ_CARTOON_PERIODO_MS) {
                g_dj_estado = FP_DJ_CARTOON;
                telemetry_event("freeplay_dj_estado", FP_DJ_CARTOON, (int)g_dj_idle_ms);
            }
        }
        break;
    case FP_DJ_EGG:
        a = freeplay_dj_anim("idleEasterEgg");
        if (g_dj_t_estado >= freeplay_dj_dura(a)) {
            g_dj_estado = FP_DJ_IDLE; g_dj_t_estado = 0;
            telemetry_event("freeplay_dj_estado", FP_DJ_IDLE, (int)g_dj_idle_ms);
        }
        break;
    case FP_DJ_FIST:      /* `FistPumpIntro` com `fistPump`/`loss` */
    case FP_DJ_LOSS:
    {
        /* `fistPumpIntro()` toca do `introStartFrame` SEM loop e o `update`
           (L96-118) faz `playFlashAnimation(prefix, true, false, false,
           introStartFrame)` de novo assim que `curFrame >= introEndFrame`: a
           janela [ini .. introFim-1] se repete enquanto o estado durar, e ele
           dura ate' o `fistPump()` do timer de 0,5 s.
           `introEndFrame` -1 desliga a repeticao (`endFrame > -1` na fonte). */
        int ini_f, fim_f;
        a = freeplay_dj_anim(g_dj_estado == FP_DJ_FIST ? "fistPump" : "loss");
        if (!a) { g_dj_estado = FP_DJ_IDLE; g_dj_t_estado = 0; break; }
        if (g_dj_estado == FP_DJ_FIST) {
            ini_f = FP_DJ_FIST_INI; fim_f = FP_DJ_FIST_INTRO_FIM;
        } else {
            ini_f = FP_DJ_FIST_INI_BAD; fim_f = FP_DJ_FIST_INTRO_FIM_BAD;
        }
        if (fim_f > -1 && fim_f <= ini_f) fim_f = ini_f + 1; /* 1 quadro */
        g_dj_forcado = a->primeiro +
            (fim_f > -1 ? freeplay_dj_janela(a, g_dj_t_estado, ini_f, fim_f, 1)
                        : ini_f);
        /* `new FlxTimer().start(0.5, ...) -> dj.fistPump()` (L1259). */
        if (g_dj_t_estado >= 500ul) {
            g_dj_estado = (g_dj_estado == FP_DJ_FIST) ? FP_DJ_LOOP_PUMP
                                                      : FP_DJ_LOOP_LOSS;
            g_dj_t_estado = 0; g_dj_forcado = -1;
            telemetry_event("freeplay_dj_estado", g_dj_estado, 500);
        }
        return;
    }
    case FP_DJ_LOOP_PUMP: /* `FistPump`: [loopIni .. loopFim] */
    case FP_DJ_LOOP_LOSS:
    {
        int ini_f, fim_f, k;
        unsigned long fps;
        a = freeplay_dj_anim(g_dj_estado == FP_DJ_LOOP_PUMP ? "fistPump" : "loss");
        if (!a) { g_dj_estado = FP_DJ_IDLE; g_dj_t_estado = 0; break; }
        if (g_dj_estado == FP_DJ_LOOP_PUMP) {
            ini_f = FP_DJ_FIST_LOOP_INI; fim_f = FP_DJ_FIST_LOOP_FIM;
        } else {
            ini_f = FP_DJ_FIST_LOOP_INI_BAD; fim_f = FP_DJ_FIST_LOOP_FIM_BAD;
        }
        if (fim_f >= a->quantos) fim_f = a->quantos - 1;
        if (fim_f > -1) {
            /* `loopEndFrame` valido: recomeca em `loopStartFrame`, igual ao
               intro -- [ini .. fim-1] em loop enquanto o estado durar. */
            if (fim_f <= ini_f) fim_f = ini_f + 1;
            g_dj_forcado = a->primeiro +
                freeplay_dj_janela(a, g_dj_t_estado, ini_f, fim_f, 1);
            return;
        }
        /* `loopEndFrame` -1: `endFrame > -1` e' falso, entao NUNCA recomeca --
           toca de `loopStart` ate' o fim da animacao e o `onFinishAnim` manda
           o Idle (L210-214). */
        fps = (unsigned long)(a->fps100 > 0 ? a->fps100 : 2400) / 100ul;
        if (!fps) fps = 24ul;
        k = ini_f + (int)((g_dj_t_estado * fps) / 1000ul);
        if (k >= a->quantos) {
            g_dj_estado = FP_DJ_IDLE; g_dj_t_estado = 0; g_dj_forcado = -1;
            telemetry_event("freeplay_dj_estado", FP_DJ_IDLE, (int)g_dj_idle_ms);
            break;
        }
        g_dj_forcado = a->primeiro + k;
        return;
    }
    default:
        break;
    }
}

/* O titulo da musica `si` da lista, na VRAM -- so' CONSULTA: quem sobe a arte
   e' `freeplay_titulos_carregar()`, uma vez por entrada na tela. */
static const fp_titulo_t *freeplay_titulo(int si, texbuffer_t *out)
{
    const sr_song_t *song;
    const fp_titulo_t *t;
    if (si < 0 || si >= g_fp_n || si >= FREEPLAY_MAX) return 0;
    if (!g_fpt[si].ok) return 0;
    song = g_fp[si];
    if (!song) return 0;
    t = fp_titulo(song->id);
    if (!t) return 0;
    out->width = (unsigned)g_fpt[si].w;
    out->psm = GS_PSM_32;
    out->address = g_fpt[si].addr;
    setup_texture_dims(out, g_fpt[si].w, g_fpt[si].h);
    return t;
}

/* O "letreiro" do titulo longo (`CapsuleText.moveTextRight/Left`, L126-166):
 * 0,6 s parado, 2 s indo para a direita com `sineInOut`, 0,3 s parado, 2 s
 * voltando, 0,3 s parado -- e recomeca. Devolve o deslocamento em px da tela. */
static float freeplay_letreiro(unsigned long desde_ms, float distancia)
{
    float t = (float)(desde_ms % 5200ul) / 1000.0f;
    float u;
    if (distancia <= 0.0f) return 0.0f;
    if (t < 0.6f) return 0.0f;
    if (t < 2.6f) { u = (t - 0.6f) / 2.0f; return distancia * (0.5f - 0.5f * cosf(3.14159265f * u)); }
    if (t < 2.9f) return distancia;
    if (t < 4.9f) { u = (t - 2.9f) / 2.0f; return distancia * (0.5f + 0.5f * cosf(3.14159265f * u)); }
    return 0.0f;
}

/* ===== ICONES DE VIDA DO HUD (engine/health_icon.c) =====
   Um por lado: 0 = jogador (BF), 1 = oponente. O pacote ICP1
   (tools/empacotar_icones.py) traz os quadros no formato do original --
   idle, losing e, quando existe, winning -- e o jogo troca de quadro pelo
   limiar do `HealthIcon.updateHealthIcon`. A arte e' a do repositorio oficial
   (preload/images/icons/icon-<id>.png), que sempre existiu e nunca tinha sido
   empacotada. */
#define ICONE_JOGADOR 0
#define ICONE_OPONENTE 1
#define ICONE_LADOS 2
static hi_pack_t g_icone_pack[ICONE_LADOS];
static unsigned char *g_icone_mem[ICONE_LADOS];
static unsigned int g_icone_mem_bytes[ICONE_LADOS];
static texbuffer_t g_icone_tex[ICONE_LADOS];
static unsigned int g_icone_chave[ICONE_LADOS];
static int g_icone_ativo[ICONE_LADOS];
static int g_icone_quadro[ICONE_LADOS];   /* quadro que esta' na VRAM */
static int g_icone_estado[ICONE_LADOS];   /* HI_IDLE/PERDENDO/GANHANDO   */
static int g_icone_flip[ICONE_LADOS];
static int g_icone_pixel[ICONE_LADOS];
static int g_icone_bopa[ICONE_LADOS];     /* shouldBop do CharacterData  */
static float g_icone_escala[ICONE_LADOS]; /* `scale` do healthIcon        */
static float g_icone_offx[ICONE_LADOS],g_icone_offy[ICONE_LADOS];
static hi_bop_t g_icone_bop[ICONE_LADOS];
static int g_icone_ultimo_passo=-1;
static unsigned int g_icone_desenhos=0;    /* quantos icones foram ao pacote */
/* Vida INTERPOLADA (PlayState.healthLerp, L2767-2779): a barra e os icones
   andam por ela, e o estado dos icones pela vida crua (HealthIcon L305/L325).
   Sem isso a barra salta de um valor ao outro no mesmo quadro do acerto. */
static float g_vida_lerp=1.0f;

/* ============ ICONES DE VIDA: carga, estado e desenho ============
   O pacote vem de mass:/FNFPS2/assets/icons/<id>.icp (tools/empacotar_icones.py).
   Cada lado tem a sua chave no pool (+6 jogador, +7 oponente) e fica FIXO
   enquanto a musica roda -- o quadro e' reenviado quando o estado muda, e um
   despejo no meio do quadro daria icone em branco. */
#define ICONE_CHAVE_JOGADOR (SP_CHAVE_TELA_BASE + 6u)
#define ICONE_CHAVE_OPONENTE (SP_CHAVE_TELA_BASE + 7u)

static u32 cena_spv(void);   /* definida mais abaixo, usada pelo pulo do icone */

static float icone_escala_quadro(int lado)
{
    if (lado < 0 || lado >= ICONE_LADOS) return 1.0f;
    return hi_bop_escala(&g_icone_bop[lado]);
}

static void icone_soltar(void)
{
    int i;
    for (i = 0; i < ICONE_LADOS; i++) {
        sp_soltar(&g_pool, ICONE_CHAVE_JOGADOR + (unsigned int)i);
        if (g_icone_mem[i]) { free(g_icone_mem[i]); g_icone_mem[i] = 0; }
        g_icone_ativo[i] = 0;
        g_icone_quadro[i] = -1;
        g_icone_estado[i] = -1;
        hi_bop_reset(&g_icone_bop[i]);
    }
}

/* Sobe UM quadro do icone para a textura dele. */
static int icone_envia(int lado, int quadro)
{
    const unsigned char *src;
    if (lado < 0 || lado >= ICONE_LADOS) return -1;
    if (!g_icone_ativo[lado] || quadro < 0) return -1;
    src = hi_quadro(g_icone_mem[lado], &g_icone_pack[lado], (unsigned int)quadro);
    if (!src) return -1;
    FlushCache(0);
    upload_raw_psm(&g_icone_tex[lado], (unsigned char *)src,
                   (int)g_icone_pack[lado].passo, (int)g_icone_pack[lado].h, GS_PSM_16);
    g_icone_quadro[lado] = quadro;
    return 0;
}

/* Carrega o icone do lado `lado` para o personagem `ci` (-1 = ninguem). */
static int icone_carregar(int lado, int ci)
{
    char path[96];
    int fd, rc;
    long tam = 0;
    unsigned int key = ICONE_CHAVE_JOGADOR + (unsigned int)lado;
    const char *id;
    if (lado < 0 || lado >= ICONE_LADOS) return -1;
    g_icone_ativo[lado] = 0;
    g_icone_quadro[lado] = -1;
    g_icone_estado[lado] = -1;
    sp_soltar(&g_pool, key);
    if (ci < 0 || ci >= CH_CHAR_COUNT) {
        /* Sem personagem nao ha' icone: o original tambem nao cria o
           `HealthIcon` quando o papel nao existe (PlayState L2155/L2190 estao
           dentro dos `if` de cada personagem). */
        telemetry_event("icone_sem_personagem", lado, ci);
        return -1;
    }
    id = ch_chars[ci].icone ? ch_chars[ci].icone : "face";
    snprintf(path, sizeof path, "mass:/FNFPS2/assets/icons/%s.icp", id);
    fd = open(path, O_RDONLY);
    if (fd < 0) {
        telemetry_event("icone_sem_arte", lado, (int)id[0]);
        return -2;
    }
    tam = lseek(fd, 0, SEEK_END);
    if (tam <= 0 || tam > 262144L) {
        close(fd);
        telemetry_event("icone_tamanho", lado, (int)tam);
        return -3;
    }
    lseek(fd, 0, SEEK_SET);
    if (!g_icone_mem[lado] || g_icone_mem_bytes[lado] < (unsigned int)tam) {
        if (g_icone_mem[lado]) free(g_icone_mem[lado]);
        g_icone_mem[lado] = memalign(128, (unsigned int)tam);
        g_icone_mem_bytes[lado] = g_icone_mem[lado] ? (unsigned int)tam : 0u;
    }
    if (!g_icone_mem[lado]) { close(fd); return -4; }
    if (read(fd, g_icone_mem[lado], (int)tam) != (int)tam) {
        close(fd);
        telemetry_event("icone_leitura", lado, (int)tam);
        return -5;
    }
    close(fd);
    rc = hi_pack_ler(g_icone_mem[lado], (unsigned int)tam, &g_icone_pack[lado]);
    if (rc != 0) {
        telemetry_event("icone_pacote", lado, rc);
        return -6;
    }
    g_icone_tex[lado].width = g_icone_pack[lado].passo;
    g_icone_tex[lado].psm = GS_PSM_16;
    g_icone_tex[lado].address = 0;
    { unsigned int addr = 0;
      if (sp_chave(&g_pool, key, hi_vram_bytes(&g_icone_pack[lado]), &addr) < 0) {
          telemetry_event("icone_sem_vram", lado, (int)hi_vram_bytes(&g_icone_pack[lado]));
          return -7;
      }
      g_icone_tex[lado].address = addr;
      sp_fixar(&g_pool, key); }
    g_icone_chave[lado] = key;
    g_icone_flip[lado] = ch_chars[ci].icone_flip;
    g_icone_pixel[lado] = ch_chars[ci].icone_pixel;
    g_icone_bopa[lado] = ch_chars[ci].icone_bop;
    g_icone_escala[lado] = ch_chars[ci].icone_scale > 0.0f ? ch_chars[ci].icone_scale : 1.0f;
    g_icone_offx[lado] = ch_chars[ci].icone_ox;
    g_icone_offy[lado] = ch_chars[ci].icone_oy;
    g_icone_ativo[lado] = 1;
    hi_bop_reset(&g_icone_bop[lado]);
    g_icone_estado[lado] = -1;
    telemetry_event("icone_carregado", lado, (int)hi_vram_bytes(&g_icone_pack[lado]));
    telemetry_event("icone_quadros", (int)g_icone_pack[lado].quadros, (int)g_icone_pack[lado].w);
    return icone_envia(lado, 0);
}

/* A cada passo: se o passo casa com bopEvery (4 = uma vez por batida, o
   `Constants.STEPS_PER_BEAT` do original) e o personagem deve pular, comeca o
   tween do pulo (HealthIcon L337-360). */
static void icone_passo(int passo)
{
    int lado;
    if (passo < 0 || passo == g_icone_ultimo_passo) return;
    g_icone_ultimo_passo = passo;
    if (passo % 4 != 0) return;
    for (lado = 0; lado < ICONE_LADOS; lado++) {
        if (!g_icone_ativo[lado] || !g_icone_bopa[lado]) continue;
        /* stepLengthMs / 1000: o pulo dura min(stepLength*2, 0,175) s. */
        hi_bop_passo(&g_icone_bop[lado], (float)cena_spv() / 48000.0f);
    }
}

/* Estado do icone pela vida CRUA (HealthIcon L325: updateHealthIcon(health))
   e subida do quadro quando ele muda. */
static void icone_atualiza(float health)
{
    int lado;
    for (lado = 0; lado < ICONE_LADOS; lado++) {
        unsigned int estado;
        float vida;
        if (!g_icone_ativo[lado]) continue;
        vida = (lado == ICONE_JOGADOR) ? health : hi_vida_oponente(health);
        estado = hi_estado(vida, g_icone_pack[lado].quadros);
        if ((int)estado != g_icone_estado[lado]) {
            g_icone_estado[lado] = (int)estado;
            if (icone_envia(lado, (int)estado) == 0)
                telemetry_event("icone_estado", lado, (int)estado);
        }
    }
}

/* Desenha os dois icones na camHUD. `vida_lerp` e' a vida interpolada (0..2),
   que e' o que o `healthBar.value` do original vale (PlayState L2767-2779). */
static qword_t *icone_desenha(qword_t *q, float vida_lerp)
{
    int lado;
    /* A barra de verdade, em px de 640x360: fundo (169,75; 324) 300,5x9,5 do
       original; o FlxBar interno tem 8 px a menos (593 -> 296,5) e comeca 4 px
       dentro (x=171,75), com altura 326 (y do fundo + 4). */
    const float barra_x = 171.75f, barra_w = 296.5f, barra_y = 326.0f;
    for (lado = 0; lado < ICONE_LADOS; lado++) {
        float x = 0.0f, y = 0.0f, px, w, h;
        if (!g_icone_ativo[lado]) continue;
        /* `size` escala o icone (snapToTargetSize: setGraphicSize(150 * size))
           e o pulo soma 20 % sobre ele. */
        px = HI_TELA_PX * g_icone_escala[lado] * icone_escala_quadro(lado);
        w = px + g_icone_offx[lado];
        h = px + g_icone_offy[lado];
        hi_posicao(lado == ICONE_JOGADOR ? 0 : 1, barra_x, barra_w, barra_y,
                   vida_lerp, w, h, &x, &y);
        g_icone_desenhos++;
        q = bind_texture_filter(q, &g_icone_tex[lado], g_icone_pixel[lado] ? 0 : 1);
        if (g_icone_flip[lado]) {
            texrect_t r2;
            float cy0, cy1, cv0, cv1;
            float dx = hz_x(x), dy = hz_y(y), dw = hz_s(w), dh = hz_s(h);
            if (band_clip_v(vy(dy), vy(dy + dh), 0.0f, (float)g_icone_pack[lado].h,
                            &cy0, &cy1, &cv0, &cv1)) {
                g_frame_texture_quads++;
                r2.v0.x = FNX_INI(dx); r2.v0.y = FNX_INI(cy0); r2.v0.z = 0;
                r2.v1.x = FNX_FIM(dx + dw); r2.v1.y = FNX_FIM(cy1); r2.v1.z = 0;
                band_clip_texel(cv0, cv1, &r2.t0.v, &r2.t1.v);
                /* flipX: o original espelha o sprite, ou seja troca o u dos
                   dois cantos. */
                r2.t0.u = (float)g_icone_pack[lado].w - 0.5f;
                r2.t1.u = 0.5f;
                r2.color = rgba(0x80, 0x80, 0x80, 0x80);
                dl_sprite(dx, dy, dw, dh, (float)g_icone_pack[lado].w, cv0,
                          -(float)g_icone_pack[lado].w, cv1 - cv0, 0x80, &r2);
                q = draw_rect_textured(q, 0, &r2);
            }
        } else {
            q = sprite_region(q, hz_x(x), hz_y(y), hz_s(w), hz_s(h), 0.0f, 0.0f,
                              (float)g_icone_pack[lado].w, (float)g_icone_pack[lado].h);
        }
    }
    return q;
}

/* z dos personagens de um palco. A ordem de desenho do Stage.hx e' a de um
   unico grupo ordenado por `zIndex`, com personagens e props juntos, e o z de
   CADA personagem esta' no slot do palco (bs_slots[]). Os valores de reserva
   sao os do mainStage, que e' o unico palco que a captura de console provou. */
static int palco_slot_z(const char *stage, const char *papel)
{
    const bs_slot_t *sl = ch_slot(stage, papel);
    if (sl) return sl->z;
    return papel && papel[0] == 'g' ? 100 : papel && papel[0] == 'd' ? 200 : 300;
}

/* ===== PALCO POR zIndex =================================================
   Stage.hx L300-330 poe os tres personagens (`addCharacter`) e os props
   (`addBopper`) num UNICO grupo ordenado por `zIndex`. Nao existe "o palco
   primeiro e as pessoas por cima": a lista e' uma so' e a posicao de cada
   personagem nela vem do z do slot daquele palco. Por isso os props saem
   daqui em FATIAS, uma entre cada par de personagens.

   A coleta (`sl_collect`) e' feita UMA vez por quadro e as fatias saem dessa
   mesma lista -- a ordem dentro de cada fatia continua o z crescente com
   insercao estavel que o sl_collect garante. */
#define PALCO_Z_FIM (1 << 30)
typedef struct {
    sl_item_t it[SL_MAX_ITENS];
    int n;              /* props na tela neste quadro                      */
    int desenhados;     /* quantos ja' foram para a tela (soma das fatias) */
} palco_lista_t;

static void palco_abre(palco_lista_t *L, const char *stage, float cam_x,
                       float cam_y, float zoom, float bop, int step,
                       float beat_s, float t)
{
    L->n = 0;
    L->desenhados = 0;
    if (!stage) return;
    if (g_palco.stage != stage) palco_carregar(stage);
    if (!g_palco.stage) return;
    L->n = sl_collect(stage, cam_x, cam_y, zoom, bop, step, beat_s, t,
                      L->it, SL_MAX_ITENS);
    if (L->n < 0) L->n = 0;
}

/* Desenha os props de z em [zmin, zmax). */
static qword_t *palco_fatia(qword_t *q, palco_lista_t *L, int zmin, int zmax)
{
    int i;
    for (i = 0; i < L->n; i++) {
        const bs_prop_t *p = &bs_props[L->it[i].prop];
        const bs_prop_anim_t *a;
        texbuffer_t *tx;
        int r = L->it[i].prop - g_palco.first;
        int quadro;
        if (L->it[i].z < zmin || L->it[i].z >= zmax) continue;
        if (palco_prop_pronto(g_palco.stage, r) != 0) continue;
        a = &bs_prop_anims[p->anim_first + L->it[i].anim];
        /* sl_prop_frame devolve o quadro DENTRO da animacao; no pacote o
           indice e' absoluto, por isso soma-se o primeiro quadro dela. */
        quadro = a->first + L->it[i].frame;
        if (quadro < 0 || (unsigned int)quadro >= g_palco.count[r]) continue;
        tx = palco_prop_textura(r, quadro);
        if (!tx) continue;
        q = bind_texture_filter(q, tx, 1);
        q = sprite_dims(q, L->it[i].x0, L->it[i].y0, L->it[i].w, L->it[i].h,
                        L->it[i].tex_w, L->it[i].tex_h);
        L->desenhados++;
    }
    return q;
}

/* Telemetria do palco, uma vez por quadro, depois da ultima fatia. */
static void palco_fecha(const palco_lista_t *L)
{
    int total = L->desenhados;
    g_palco_ok = total > 0;
    /* Uma linha por MUDANCA de resultado, e nao por quadro: "desenhou 3 props"
       precisa aparecer no log uma vez, nao 5.600. Se cair para 0 (pacote
       faltando, VRAM sem lugar), aparece a queda com o total de falhas. */
    if (total != g_palco.desenhou) {
        g_palco.desenhou = total;
        telemetry_event("palco_draw", total, (int)g_palco.falhas);
        telemetry_event("palco_pool", (int)sp_livre(&g_pool), (int)g_palco.uploads);
        /* Palco com props na tela e NENHUM desenhado: todos falharam (pacote ou
           VRAM). A reserva composta ja' foi decidida antes, entao nao da' para
           desenha-la atras agora -- mas o log tem de dizer que a tela esta'
           vazia, em vez de deixar o sintoma sem causa. */
        if (total == 0 && L->n > 0)
            telemetry_event("palco_vazio", L->n, (int)g_palco.falhas);
    }
}

/* Quando entra a textura composta do tutorial (a reserva). Tres casos, e so'
   eles:
     1) nao ha' nenhum prop na tela (fora do enquadramento);
     2) o primeiro prop do quadro nao abre o pacote (pendrive sem os .cfz4);
     3) os props do quadro pedem mais que o pool INTEIRO -- ai nenhum despejo
        resolve, e a tela ficaria preta.
   Uma falha de VRAM num prop sozinho NAO troca o palco inteiro pela arte
   composta: os outros props ainda desenham, e o log diz qual falhou. */
static int palco_reserva(const palco_lista_t *L, const char *stage)
{
    unsigned int soma = 0;
    int i;
    if (L->n <= 0) return 1;
    if (palco_prop_pronto(stage, L->it[0].prop - g_palco.first) != 0) return 1;
    for (i = 0; i < L->n; i++) soma += sp_prop_bytes(L->it[i].prop);
    return soma > SP_POOL_BYTES;
}
/* ===== PERSONAGENS GENERICOS ==========================================
   O estado 8 desenhava DOIS personagens: o BF e a GF do tutorial, com os
   pacotes `bf_frames.lz4f` / `gf_frames.lz4f` e faixas de quadro escritas no
   codigo. Sao 46 personagens, e o OPONENTE nao era desenhado em 26 das 27
   musicas -- a tela mostrava o palco, o BF, a GF e mais ninguem.

   Aqui entra o oponente, que e' o que falta: quem ele e' sai do registro da
   musica (`sr_song_t.opponent`) pelo papel (`ch_cena_plano`), os quadros saem
   do pacote `assets/chars/<id>.chp4` (`char_pack.c`), a animacao de cada
   instante sai de `char_cena.c`, e a posicao sai do slot `dad` do palco
   (`char_layout.c`).

   O BF e a GF do tutorial continuam vindo do caminho antigo NESTA build --
   trocar os tres de uma vez seria tres causas no mesmo build (regra 8). */
static cc_cena_t g_cena;
static char g_cena_song[64];
static unsigned char *g_cena_mem[CH_CENA_PAPEIS];
static unsigned int g_cena_mem_bytes[CH_CENA_PAPEIS];
static texbuffer_t g_cena_tex;          /* textura do oponente no pool    */
static int g_cena_tex_w=0,g_cena_tex_h=0;
/* Textura de CADA papel. Onde cada uma mora nao foi escolhido agora: o BF ja'
   tinha o slot `tex` (320x256 PSMCT32 = 327.680 B) e a GF ja' usava o slot
   `gftex` do pool (256x256 PSMCT32 = 262.144 B, que o tutorial enche com
   196.480 B de PSMCT16). Reaproveitar os dois nao gasta um byte de VRAM nova --
   e o pacote real do BF (258x254 PSMCT32 = 262.128 B) e o da GF (320x307
   PSMCT16 = 196.480 B) cabem exatamente onde ja' estavam. */
/* Indice em ch_chars[] de cada papel carregado (-1 = papel vazio). O NOTE KIND
   e' resolvido por PERSONAGEM (o script e' do personagem, nao do papel), e o
   unico lugar que sabe o id e' ch_chars[ci].id. */
static int g_cena_ci[CH_CENA_PAPEIS];
/* `doAlternate()` dos scripts Blazin': os kinds `punchhigh`/`punchlow` pedem
   `'punchHigh' + postfix`, e o postfix alterna '1'/'2' a cada chamada -- os dois
   nomes existem nos dados ('punchHigh1'/'punchHigh2'). O contador e' POR PAPEL,
   porque o `alternate` do script e' um campo do personagem. */
static int g_kind_alt[CH_CENA_PAPEIS];
/* TREMOR DE CAMERA (`PlayState.instance.camGame.shake(...)`), o unico pedaco do
   NOTE KIND que ainda nao era aplicado. Sao 14 dos 68 pares (personagem, kind):
   o `weekend-1-punchhigh` do Darnell pede shake(0.0025, 0.15) e o
   `punchhighblocked` pede (0.002, 0.1) -- os numeros vem do script, nao daqui.

   A forma e' a do FlxCamera.shake: a intensidade e' uma FRACAO da tela da
   camera e o deslocamento e' sorteado a cada quadro em [-I, I] * tamanho,
   ate' o tempo acabar. FlxCamera nao guarda a sequencia: quem sorteia e' o
   `FlxG.random`, com semente do jogo. O port usa um LCG proprio e deterministico
   (o harness precisa do mesmo resultado duas vezes); o que se preserva e' a
   DISTRIBUICAO e a amplitude, nao a sequencia de numeros aleatorios alheia. */
static shk_t g_shake;
#define SHAKE_CAM_W 1280.0f      /* tamanho da camera do original (1280x720) */
#define SHAKE_CAM_H 720.0f
/* zIndex que o script pede: 2000 = moveToBack, 3000 = moveToFront
   (darnell-blazin.hxc L271-281). 0 = usa o z do slot do palco. */
static int g_z_extra[CH_CENA_PAPEIS];
static texbuffer_t g_cena_texde[CH_CENA_PAPEIS];
static int g_cena_texde_w[CH_CENA_PAPEIS],g_cena_texde_h[CH_CENA_PAPEIS];
static int g_cena_quadros[CH_CENA_PAPEIS];      /* quadros diferentes subidos */
static int g_cena_desenhos[CH_CENA_PAPEIS];
/* Uma vez por papel e por musica: o tamanho desenhado e a escala do pacote, para
   o log poder provar que o personagem nao esta' nem 2x menor nem 6x menor. */
static int g_cena_tam_visto[CH_CENA_PAPEIS];
static int g_cena_avisou[CH_CENA_PAPEIS];
static int g_cena_hold_visto[CH_CENA_PAPEIS];
static int g_cena_pulos_visto[CH_CENA_PAPEIS];
/* Um aviso por papel: a primeira animacao com `offsets` proprio aplicada (ver
   o bloco OFFSETS POR ANIMACAO em cena_desenha). */
static int g_cena_off_visto[CH_CENA_PAPEIS];
static unsigned char *g_cena_pixels=0;
static unsigned int g_cena_pixels_cap=0;
static int g_cena_estado=0;             /* 0 = nao tentou, 1 = pronto, <0 = falhou */

static unsigned int g_cena_ram=0;
static int g_cena_dad_ci=-1;         /* personagem do slot dad        */
static int g_cena_bf_ci=-1,g_cena_gf_ci=-1;   /* jogador e namorada     */
/* 1 = BF e GF desenhados pelo caminho generico; 0 = caminho do tutorial.
   O TUTORIAL continua no caminho antigo de proposito: e' a unica musica cujas
   posicoes e a camera foram conferidas numa captura do console, e trocar isso
   por deducao de codigo seria jogar fora uma medida (regra 5). */
static int g_cena_modo=0;
/* Slots de VRAM que o BF e a GF ja' ocupavam, apontados de dentro do main()
   (sao variaveis locais dele). Assim a carga dos personagens nao precisa de um
   byte de VRAM nova: ela desenha nos enderecos que ja' existiam. */
static texbuffer_t *g_slot_bf=0,*g_slot_gf=0;
/* Um pacote de personagem nao pode passar disto: o maior do jogo (as formas
   do pai) tem algumas dezenas de quadros. O limite e' conferido, nao suposto,
   e existe para um arquivo corrompido no pendrive nao pedir 2 GB de malloc. */
#define CENA_PACK_MAX (32u*1024u*1024u)

/* CUSTO DA MUSICA, medido: quadros desenhados com musica tocando (o
   denominador), uploads de quadro de personagem e de prop do palco. Sai no fim
   da musica E na saida da cena -- musica que termina por tempo (chart menor que
   o audio, saida pelo menu, Game Over) nunca passaria pelo fim e ficaria sem
   medida nenhuma. Uma vez por musica: `g_custo_emitido` guarda. */
/* Uma amostra do custo acumulado ate' agora. Os numeros sao CUMULATIVOS: a
   ultima amostra da musica e' o total dela, e as do meio servem para ver se o
   custo muda ao longo da musica (entrada de prop, troca de animacao). */
static void custo_amostra(void)
{
 u32 fr=g_custo_frames?g_custo_frames:1u;int cu=0,r;
 for(r=0;r<CH_CENA_PAPEIS;r++)cu+=g_cena_quadros[r];
 telemetry_event("custo_char",cu,(int)((u32)cu*100u/fr));
 telemetry_event("custo_palco",(int)g_palco.uploads,(int)((u32)g_palco.uploads*100u/fr));
 telemetry_event("custo_quadros",(int)fr,(int)(g_chart_clock>0?(u32)g_chart_clock/48u:0u));
}

/* Amostra periodica: 240 quadros de musica (~4 s a 60 Hz). Sem ela, uma musica
   cujo audio nao termina dentro da passada do teste ficaria SEM medida, e foi o
   que aconteceu: so' 43 das 81 combinacoes chegavam ao fim do audio. */
#define CUSTO_CADA 240u

static void custo_emite(void)
{
 /* Sem quadros de musica nao ha' o que medir: `cena_soltar` roda tambem ao
    ENTRAR na cena (antes de tocar), e emitir ali gravaria "0 uploads em 0
    quadros" para toda musica, que e' pior que nao medir. */
 if(g_custo_emitido||g_custo_frames==0)return;
 g_custo_emitido=1;
 custo_amostra();
}

static void cena_soltar(void)
{
 custo_emite();
 int i;
 /* O icone tambem e' da cena: solta a chave do pool e a memoria junto com os
    pacotes de personagem (senao a proxima musica herdaria o icone da anterior
    e a VRAM ficaria presa). */
 icone_soltar();
 for(i=0;i<CH_CENA_PAPEIS;i++){if(g_cena_mem[i])free(g_cena_mem[i]);g_cena_mem[i]=0;g_cena_mem_bytes[i]=0;}
 g_cena_ram=0;
 g_cena_estado=0;
 g_cena_modo=0;
 g_cena_song[0]=0;
 {int i;for(i=0;i<CH_CENA_PAPEIS;i++){g_cena_texde_w[i]=g_cena_texde_h[i]=0;
  g_cena_quadros[i]=g_cena_desenhos[i]=g_cena_avisou[i]=g_cena_hold_visto[i]=0;
  g_cena_tam_visto[i]=0;g_cena_pulos_visto[i]=0;}}
 cc_reset(&g_cena);
}

/* Le' o arquivo inteiro para a memoria. Devolve 0 ou negativo. */
static int cena_ler(const char *path,unsigned char **out,unsigned int *out_bytes)
{
 int fd,n;off_t end;unsigned char *p;int done=0;
 *out=0;*out_bytes=0;
 fd=open(path,O_RDONLY);
 if(fd<0)return -1;
 end=lseek(fd,0,SEEK_END);
 if(end<=0||(unsigned int)end>CENA_PACK_MAX||lseek(fd,0,SEEK_SET)<0){close(fd);return -2;}
 p=memalign(128,(unsigned int)end);
 if(!p){close(fd);return -3;}
 while(done<(int)end){
  n=read(fd,p+done,(int)end-done);
  if(n<=0){free(p);close(fd);return -4;}
  done+=n;
 }
 close(fd);
 *out=p;*out_bytes=(unsigned int)end;
 return 0;
}

/* Abre os pacotes dos tres papeis da musica. Devolve 0 quando ha' o que
   desenhar; negativo diz o que faltou. Nao inventa personagem: papel sem
   pacote fica inativo e o evento `char_falha` diz qual. */
static int cena_carregar(const sr_song_t *song)
{
 ch_cena_t plano[CH_CENA_PAPEIS];
 int i;
 char path[96];
 if(!song)return -1;
 if(g_cena_estado!=0&&strcmp(g_cena_song,song->id)==0)return g_cena_estado>0?0:-1;
 cena_soltar();
 if(ch_cena_plano(song,plano)!=0)return -2;
 /* O TUTORIAL NAO TEM MAIS EXCECAO. Ele ficou por ultimo no caminho antigo --
    e aquele caminho, depois da limpeza do renderizador ("Clean Tutorial
    renderer foundation"), nao desenhava personagem nenhum: o evento
    `char_tutorial_legado` barrava a cena generica e o que sobrava era palco,
    notas e HUD. Medido no harness: ZERO eventos `char_bf_primeiro`,
    `char_gf_primeiro` e `char_dad_primeiro` numa passada inteira do Tutorial,
    contra um de cada nas outras 26 musicas. As posicoes e a camera do
    mainStage que a captura de console fixou continuam valendo: `ch_anchor`
    reproduz a mesma conta do `Stage.addCharacter` + `BaseCharacter.setScale`,
    e o caminho generico ja registra esses numeros digito a digito.
    Nao ha' caminho de reserva: pacote que falta vira `char_sem_pacote` com o
    id no log, e o papel fica vazio -- dado faltando e' reportado, nao tapado. */
 strncpy(g_cena_song,song->id,sizeof g_cena_song-1);
 g_cena_song[sizeof g_cena_song-1]=0;
 for(i=0;i<CH_CENA_PAPEIS;i++){g_cena_ci[i]=-1;g_kind_alt[i]=0;g_z_extra[i]=0;}
 shk_reset(&g_shake,0x9e3779b9u);
 for(i=0;i<CH_CENA_PAPEIS;i++){
  int a;
  if(plano[i].ci<0){telemetry_event("char_vazio",i,0);continue;}
  /* Um personagem NAO entra duas vezes na mesma cena. A regra e' do DADO, e
     nao do nome: um papel e' pulado quando o MESMO personagem ja' ocupa um
     papel anterior. No Tutorial isso nao acontece -- o playData dele
     (`assets-data/songs/tutorial-metadata.json`) tem `player: bf` e
     `opponent: gf`, e NENHUM `girlfriend`; logo o slot gf fica vazio
     (`char_vazio`) e a GF aparece como OPONENTE, no slot dad, que e' o que o
     dado diz. A versao anterior pulava o oponente so' porque o id era "gf", e
     com isso a unica musica sem `girlfriend` ficava sem oponente na tela. */
  {int j,repetido=0;
   for(j=0;j<i;j++)if(plano[j].ci>=0&&plano[j].ci==plano[i].ci)repetido=1;
   if(repetido){telemetry_event("char_papel_repetido",i,plano[i].ci);continue;}}
  snprintf(path,sizeof path,"mass:/FNFPS2/assets/chars/%s.chp4",ch_chars[plano[i].ci].id);
  a=cena_ler(path,&g_cena_mem[i],&g_cena_mem_bytes[i]);
  if(a!=0){telemetry_event("char_sem_pacote",plano[i].ci,a);continue;}
  g_cena_ram+=g_cena_mem_bytes[i];
  a=cc_anexar(&g_cena,i,g_cena_mem[i],g_cena_mem_bytes[i],0);
  if(a!=0){telemetry_event("char_pacote_ruim",plano[i].ci,a);free(g_cena_mem[i]);g_cena_mem[i]=0;continue;}
  /* de quantas em quantas BATIDAS este personagem danca (ch_dados_t.dance):
     1.0 nos 46, porque nenhum dos JSON traz `danceEvery` e o CharacterData
     preenche o DEFAULT_DANCEEVERY. O campo existe para nao haver uma constante
     escondida no meio do codigo se algum personagem passar a trazer. */
  cc_danca_cada(&g_cena,i,ch_chars[plano[i].ci].dance);
  g_cena_ci[i]=plano[i].ci;
  telemetry_event("char_carregado",plano[i].ci,(int)g_cena_mem_bytes[i]);
  /* Quantos NOTE KINDS este personagem implementa no script dele. O dado passa
     a existir (tools/gen_notekinds.py le' preload/scripts/), entao o numero
     deixa de ser "0, falta dado" e vira uma medida por personagem -- e um
     personagem que perca o script dele aparece aqui como 0. */
  {const char *pid=ch_chars[plano[i].ci].id;unsigned int k,n=0;
   for(k=0;k<NK_KIND_COUNT;k++)if(nk_acao(pid,nk_kinds[k].id))n++;
   telemetry_event("char_kinds",(int)n,(int)plano[i].ci);}
 }
 /* TEXTURA DE CADA PAPEL, no MESMO formato de pixel do pacote (PSMCT32 ou
    PSMCT16). Nao se converte 32 -> 16 aqui: essa conta de cor nao foi medida, e
    pixel inventado e' pior que pixel caro. O tamanho, portanto, e' o
    `bytes_por_quadro` do pacote -- exatamente o que o `cp_quadro` escreve no
    staging. (A primeira versao reservava w*h*2 e mandava descomprimir w*h*4:
    estouro de 8 KB no heap, que o glibc acusou como "corrupted double-linked
    list".)

    Cada papel usa um endereco que JA' EXISTIA: o BF o slot `tex`, a GF o slot
    `gftex` do pool, o oponente uma chave livre do pool. Nenhum byte de VRAM
    novo -- por isso o orcamento dos 21 palcos nao muda. Um pacote que nao caiba
    no seu slot fica de fora com o evento `char_sem_vram`, em vez de escrever
    por cima do vizinho. */
 g_cena_tex_w=g_cena_tex_h=0;
 g_cena_dad_ci=plano[CH_PAPEL_DAD].ci;
 g_cena_bf_ci=plano[CH_PAPEL_BF].ci;g_cena_gf_ci=plano[CH_PAPEL_GF].ci;
 g_cena_quadros[CH_PAPEL_DAD]=g_cena_desenhos[CH_PAPEL_DAD]=g_cena_avisou[CH_PAPEL_DAD]=0;
 {int r;unsigned int maior=0;
  for(r=0;r<CH_CENA_PAPEIS;r++){
   const cp_pack_t *pk;
   unsigned int addr=0,capacidade=0,bytes;
   texbuffer_t *tpl;
   if(!g_cena.p[r].ativo)continue;
   pk=&g_cena.p[r].pacote;
   /* O QUE A TEXTURA OCUPA no GS, e nao o que o decodificador escreve.
      O BITBLTBUF anda em unidades de 64 pixels (`dest_width>>6` no
      draw_texture_transfer do ps2sdk): subir um quadro de 258 px numa
      textura com `width=258` faz o GS usar passo de 256 px e a imagem sai
      enviesada, dois pixels por linha. O pacote de 258 continua certo -- o
      que muda e' o passo da textura e, com ele, a VRAM reservada. */
   bytes=cp_tex_bytes(pk->w,pk->h,pk->psm);
   /* OS TRES PAPEIS SAEM DO POOL, cada um na sua chave e com o tamanho do SEU
      pacote. Antes o BF morava no slot fixo `tex` (320x256 PSMCT32 = 327.680 B)
      e a GF no slot `gftex` -- que o `palco_ui_entrou` LIBERA ao entrar na
      musica, ou seja a GF escrevia numa area que o palco podia reusar: dois
      desenhos no mesmo endereco de VRAM, invisivel no harness (que nao
      rasteriza) e visivel no console. Agora:
        +2 oponente   +4 BF   +5 GF
      e as tres chaves ficam FIXAS durante a musica (sp_fixar abaixo), porque a
      textura do personagem e' escrita uma vez por quadro e nao pode ser
      despejada no meio: quem pede props de palco e' o desenho, no mesmo quadro.
      Ao sair da musica o palco_ui_entrou solta as tres. */
   {unsigned int chave=0;
    if(r==CH_PAPEL_DAD)chave=SP_CHAVE_TELA_BASE+2u;
    else if(r==CH_PAPEL_BF)chave=SP_CHAVE_TELA_BASE+4u;
    else if(r==CH_PAPEL_GF)chave=SP_CHAVE_TELA_BASE+5u;
    tpl=g_slot_bf?g_slot_bf:&g_gftex16;
    capacidade=bytes;
    if(!chave){
     /* Um quarto papel nao tem slot: isso e' dado novo, e o log tem de dizer. */
     telemetry_event("char_sem_slot",r,(int)bytes);
     g_cena.p[r].ativo=0;
     continue;
    }
    if(sp_chave(&g_pool,chave,bytes,&addr)<0)addr=0u;
    else sp_fixar(&g_pool,chave);}
   if(addr==0||bytes>capacidade){
    telemetry_event("char_sem_vram",(int)bytes,(int)capacidade);
    g_cena.p[r].ativo=0;
    continue;
   }
   g_cena_texde[r]=*tpl;
   g_cena_texde[r].width=(unsigned int)((pk->w+63)&~63);
   g_cena_texde[r].psm=(pk->psm==16)?GS_PSM_16:GS_PSM_32;
   g_cena_texde[r].address=addr;
   g_cena_texde_w[r]=pk->w;g_cena_texde_h[r]=pk->h;
   if(bytes>maior)maior=bytes;
   telemetry_event(r==CH_PAPEL_DAD?"char_dad_vram":"char_vram",(int)bytes,(int)pk->w);
   telemetry_event("char_stride",pk->w,(int)((pk->w+63)&~63));
   /* Pacote v1 nao guarda a escala do canvas. O desenho assume 1,0 (canvas no
      tamanho do original) e o log diz que assumiu -- em vez de sair 2x menor
      sem ninguem saber por que. */
   if(pk->escala<=0.0f)telemetry_event("char_escala_v1",r,(int)bytes);
   /* Quantas animacoes "-hold" este personagem TEM. O sweep usa isto para
      cobrar que, se existe alguma, ela aparece em cena -- sem esse numero, um
      personagem que nunca entra no "-hold" passaria despercebido. */
   /* So' contam as animacoes "-hold" cujo par o port ainda pode tocar: `idle`,
      `sing*`, `dance*`. As outras (taunt-hold, uppercut-hold, shoot1-hold)
      sao pedidas por NOTE KINDS ("weekend-1-taunt", "weekend-1-picouppercut",
      "weekend-1-firegun"), e os scripts desses kinds nao estao no pacote de
      dados -- o port nao tem como alcancar essas animacoes, e cobra-las no
      sweep seria cobrar o que nao ha' dado para fazer. */
   {int k,tem=0;
    for(k=0;k<pk->n_anims;k++){const char*nm=pk->anim[k].nome;size_t ln=strlen(nm);
     if(ln>5&&strcmp(nm+ln-5,"-hold")==0){
      const char*base=nm;size_t bl=ln-5;
      if((bl==4&&memcmp(base,"idle",4)==0)||(bl>=4&&memcmp(base,"sing",4)==0)||
         (bl>=5&&memcmp(base,"dance",5)==0))tem++;
     }}
    telemetry_event("char_hold_tem",r,tem);}
   /* Quantas animacoes de COMBO/DROP o personagem tem. So' as do tipo GF sao
      tocadas (BaseCharacter.hx L588-600), e nos dados sao combo50/combo200 nas
      gf/nene e drop70 nas 8 -- mas o numero vai para o log em vez de ficar
      escrito no codigo: um personagem que ganhe um degrau novo nao passa
      despercebido, e um papel sem nenhuma delas nao inventa animacao. */
   {int k,nc=0,nd=0;
    for(k=0;k<pk->n_anims;k++){const char*nm=pk->anim[k].nome;
     if(strncmp(nm,"combo",5)==0)nc++;
     else if(strncmp(nm,"drop",4)==0)nd++;}
    telemetry_event("char_combo_anims",nc,nd);}
  }
  if(maior>g_cena_pixels_cap){
   if(g_cena_pixels)free(g_cena_pixels);
   g_cena_pixels=memalign(128,maior);
   g_cena_pixels_cap=g_cena_pixels?maior:0u;
  }
  if(!g_cena_pixels||g_cena_pixels_cap<maior){
   telemetry_event("char_sem_staging",(int)maior,g_cena_pixels?1:0);
   for(r=0;r<CH_CENA_PAPEIS;r++){g_cena.p[r].ativo=0;g_cena_texde_w[r]=0;}
  }
 }
 if(g_cena.p[CH_PAPEL_DAD].ativo){g_cena_tex=g_cena_texde[CH_PAPEL_DAD];g_cena_tex_w=g_cena_texde_w[CH_PAPEL_DAD];g_cena_tex_h=g_cena_texde_h[CH_PAPEL_DAD];}
 /* O caminho generico substitui o antigo quando o JOGADOR esta' pronto. Antes
    exigia tambem a GF, porque o caminho antigo desenhava a GF do Tutorial ao
    lado -- e o Tutorial e' a UNICA musica sem `girlfriend` no playData: com a
    exigencia da GF ele caia no caminho antigo, que nao desenha mais ninguem, e
    a cena ficava so' com palco, notas e HUD. Medido no harness: 0 eventos
    `char_bf_primeiro` no Tutorial contra 1 em cada uma das outras 26 musicas.
    O oponente do Tutorial e' a propria GF, e ela entra pelo slot `dad`. */
 g_cena_modo=g_cena.p[CH_PAPEL_BF].ativo?1:0;
 g_cena_estado=g_cena.p[CH_PAPEL_DAD].ativo?1:-3;
 /* Os dois icones do HUD: o do jogador e o do oponente, com a arte que o
    `healthIcon` do personagem pede (HealthIcon.hx; ausente = 'face'). Eles
    dependem da CENA, entao nascem aqui junto com os pacotes. */
 icone_carregar(ICONE_JOGADOR,g_cena_bf_ci);
 icone_carregar(ICONE_OPONENTE,g_cena_dad_ci);
 return g_cena_estado>0?0:-3;
}


/* Quanto tempo o canto de UM papel dura, em ms, pela regra do original:
   singTime do personagem (passos) x stepLengthMs do chart, e o DOBRO quando a
   animacao e' de erro. O tempo fixo de 1.200 ms de antes era um numero do tutorial
   aplicado a todas as 27 musicas -- errado em duas frentes (passos e bpm). */
/* O PASSO E A DURACAO DELE NA SECAO DE BPM VIGENTE.
   Conductor.hx refaz `stepLengthMs = beatLengthMs / 4` a cada BPM novo, e o
   chart carrega a tabela (start_sample -> samples_per_step). O port calculava os
   dois UMA VEZ no comeco da musica (`chart_samples_per_step(0)`), o que da' o
   mesmo numero para a musica inteira. Em 26 das 27 musicas isso e' exato porque
   o BPM nao muda; em `monster` sao 14 mudancas (95 a 132 bpm) e, depois da
   primeira, tudo que anda por PASSO -- danca ociosa, props do palco, bop da
   camera, duracao do canto e das animacoes -- ficava no tempo do primeiro BPM.
   Estes dois leitores olham a tabela na posicao ATUAL. */
static u32 cena_spv(void)
{
    u32 s = (g_chart_clock > 0) ? chart_samples_per_step((u32)g_chart_clock) : 0u;
    if (!s) s = g_play_samples_per_step;
    if (!s) s = TUT_SAMPLES_PER_STEP;
    return s;
}

static int cena_passo(void)
{
    if (g_chart_clock < 0) return (int)(g_chart_clock / (s32)cena_spv());
    return (int)chart_step_at((u32)g_chart_clock);
}

static long cena_canto_ms(int papel,int missing)
{
 int ci=papel==CH_PAPEL_GF?g_cena_gf_ci:(papel==CH_PAPEL_BF?g_cena_bf_ci:g_cena_dad_ci);
 float passos,bpm;
 if(ci<0||ci>=CH_CHAR_COUNT)return 0;
 passos=ch_chars[ci].sing>0.0f?ch_chars[ci].sing:8.0f;   /* CharacterData DEFAULT_SINGTIME */
 /* 720000 / samples_por_passo = bpm (48000*60/(bpm*4)), na secao VIGENTE: em
    `monster` o tempo muda 14 vezes e a duracao do canto tem de acompanhar. */
 {float spv=(float)cena_spv();
  bpm=spv>0.0f?720000.0f/spv:((g_play_song&&g_play_song->bpm>0.0f)?g_play_song->bpm:100.0f);}
 return ca_canto_ms(passos,bpm,missing);
}

/* TAMANHO do canvas do papel em unidades do ORIGINAL (1280x720).
   Uma funcao so' para o desenho e para o foco da camera: quando cada um fazia a
   propria conta, a camera passava canvas 0x0 e o foco saia meia-personagem
   abaixo do ponto certo (medido: `- canvas_h/2` do ch_anchor virando 0). */
static int cena_canvas_mundo(int papel,const bs_slot_t *slot,float *w,float *h)
{
 int ci;
 if(papel<0||papel>=CH_CENA_PAPEIS||!slot||!g_cena.p[papel].ativo)return 0;
 ci=papel==CH_PAPEL_GF?g_cena_gf_ci:(papel==CH_PAPEL_BF?g_cena_bf_ci:g_cena_dad_ci);
 if(ci<0||ci>=CH_CHAR_COUNT)return 0;
 {float em=ch_escala_mundo(cp_escala(&g_cena.p[papel].pacote),
                           ch_chars[ci].scale,slot->scale);
  if(w)*w=(float)g_cena_texde_w[papel]*em;
  if(h)*h=(float)g_cena_texde_h[papel]*em;}
 return 1;
}

/* O PONTO QUE A CAMERA SEGUE NA TELA DE GAME OVER, em unidades do original.
   GameOverSubState.hx L196-222 (setCameraTarget), linha por linha:

     cameraFollowPoint = boyfriend.cameraFocusPoint
         - boyfriend._data.cameraOffsets
         - currentStage._data.characters.bf.cameraOffsets
         + boyfriend.getDeathCameraOffsets()

   e o `cameraFocusPoint` do BaseCharacter.hx L350-355 e'
   `originalPosition + width/2, height/2 + cameraOffsets` -- ou seja, o
   cameraOffsets do PERSONAGEM entra e sai (some), e o que sobra e' o centro
   do sprite (o `originalPosition + canvas/2`, que em ch_anchor e'
   sprite + canvas/2) MENOS o cameraOffsets do SLOT do palco MAIS o do death.
   Nao da' para reaproveitar `ch_anchor`: ele SOMA os dois cameraOffsets, que
   e' a formula do gameplay. Escrever a do original aqui, com as duas
   truncagens (`Std.int`) que ela faz, e' a unica forma de nao inventar valor.

   Devolve 0 quando nao ha' cena (sem pacote, sem slot ou antes da carga): quem
   chama mantem a camera onde ela esta', em vez de pular para um ponto que nao
   existe. */
static int go_foco_bf(float *fx,float *fy)
{
 const char *stg=g_play_song?g_play_song->stage:"mainStage";
 const bs_slot_t *sl;
 ch_anchor_t an;
 float cw=0.0f,chh=0.0f;
 int ci=g_cena_bf_ci;
 if(!g_cena_modo||ci<0||ci>=CH_CHAR_COUNT)return 0;
 sl=ch_slot(stg,"bf");
 if(!sl||!cena_canvas_mundo(CH_PAPEL_BF,sl,&cw,&chh))return 0;
 /* A conta vive em char_layout (ch_foco_morte) para poder ser testada sem GS. */
 ch_foco_morte(&ch_chars[ci],sl,cw,chh,&an);
 if(fx)*fx=an.cam_x;
 if(fy)*fy=an.cam_y;
 return 1;
}

/* Desenha UM papel generico: pega o quadro quando ele muda, sobe para a VRAM
   e posiciona pelo slot do palco. Devolve o qword do pacote. Sem pacote, sem
   slot ou sem quadro, devolve o pacote como esta' -- nada em silencio: o
   primeiro desenho e os quadros contados vao para o log. */
static qword_t *cena_desenha(qword_t *q,int papel,const bs_slot_t *slot,int passo,long ms,float cx,float cy,float sc)
{
 int qd;
 if(papel<0||papel>=CH_CENA_PAPEIS)return q;
 if(!g_cena.p[papel].ativo||!slot||g_cena_texde_w[papel]<=0)return q;
 qd=cc_quadro(&g_cena,papel,ms,passo);
 if(qd>=0&&g_cena_pixels&&cp_quadro(&g_cena.p[papel].pacote,(unsigned)qd,g_cena_pixels)==0){
  FlushCache(0);
  upload_raw_psm(&g_cena_texde[papel],g_cena_pixels,g_cena_texde_w[papel],g_cena_texde_h[papel],
                 g_cena.p[papel].pacote.psm==16?GS_PSM_16:GS_PSM_32);
  g_cena_quadros[papel]++;
 }
 {
  ch_anchor_t an;
  int ci=papel==CH_PAPEL_GF?g_cena_gf_ci:(papel==CH_PAPEL_BF?g_cena_bf_ci:g_cena_dad_ci);
  /* TAMANHO. O canvas do pacote esta' na escala em que foi empacotado (0,5 nos
     46 personagens reais); o original desenha a arte vezes `CharacterData.scale`
     vezes o `scale` do slot do palco (Stage.hx L451-455). `ch_escala_mundo`
     junta as tres coisas e devolve quantas unidades do ORIGINAL vale 1 pixel de
     canvas -- com os dados reais, 2. Antes o desenho multiplicava o canvas pelo
     `sc` da tela (0,5 x zoom x bop) e o personagem saia com METADE do tamanho
     que o original mostra: o BF do mainStage ocupava 127 das 360 linhas em vez
     de 254. A conta de posicao ja' estava certa porque `ch_anchor` recebe o
     tamanho em unidades do original -- era so' o tamanho que faltava. */
  float em=ch_escala_mundo(cp_escala(&g_cena.p[papel].pacote),
                           ci>=0?ch_chars[ci].scale:1.0f,slot->scale);
  float wm=0.0f,hm=0.0f;
  cena_canvas_mundo(papel,slot,&wm,&hm);   /* a MESMA conta que a camera usa */
  ch_anchor(&ch_chars[ci],slot,wm,hm,0.0f,papel==CH_PAPEL_GF?0:1,&an);
  /* OFFSETS POR ANIMACAO: a posicao do personagem NAO e' a mesma em toda
     animacao. Bopper.hx L321-325 copia `animationOffsets.get(name)` para
     `animOffsets` a cada troca de animacao e L338-339 subtrai
     `(animOffsets - globalOffsets) * scale` no getScreenPosition. Hoje 155 das
     523 animacoes dos 46 personagens tem offsets proprios (maior: o `ugh` do
     tankman, [5,-40], que entra justamente pelo NOTE KIND). Sem isto o quadro
     certo aparecia no lugar do quadro errado -- deslocado, nao faltando, que e'
     o tipo de erro que so' a captura de tela pega.
     Fica DEPOIS do ch_anchor porque o foco da CAMERA nao muda: o
     resetCameraFocusPoint (BaseCharacter.hx L350-355) usa originalPosition +
     cameraOffsets, que nao levam animOffsets.
     Aplicado apenas no desenho; `an.cam_x/cam_y` continua saindo do ch_anchor. */
  {int aoff=cc_anim(&g_cena,papel);
   if(ci>=0&&aoff>=0&&aoff<g_cena.p[papel].pacote.n_anims){
    int ai2=ch_anim_i(ci,g_cena.p[papel].pacote.anim[aoff].nome);
    float odx=0.0f,ody=0.0f;
    if(ch_offset_anim(ci,ai2,ch_chars[ci].scale,slot->scale,&odx,&ody)){
     an.sprite_x+=odx;an.sprite_y+=ody;
     /* Uma medida por papel e por animacao seria ruido; uma por papel, com a
        PRIMEIRA que teve offset, prova no log do console que a conta rodou. */
     if(!g_cena_off_visto[papel]){g_cena_off_visto[papel]=1;
      /* Em centesimos de pixel: os offsets do JSON sao inteiros, e o log tem
         de mostrar o MESMO numero do dado (o `ugh` do tankman tem de sair -500
         e +4000, nao -5 e 40). */
      telemetry_event("char_anim_offset",papel,(int)(odx*100.0f));
      telemetry_event("char_anim_offset_y",papel,(int)(ody*100.0f));}
    }
   }}
  q=bind_texture_filter(q,&g_cena_texde[papel],1);
  {
   float sx=320.0f+(an.sprite_x-cx)*sc,sy=180.0f+(an.sprite_y-cy)*sc;
   if(ci>=0&&ch_chars[ci].flipx)
    q=sprite_dims_flip(q,sx,sy,wm*sc,hm*sc,g_cena_texde_w[papel],g_cena_texde_h[papel]);
   else
    q=sprite_dims(q,sx,sy,wm*sc,hm*sc,g_cena_texde_w[papel],g_cena_texde_h[papel]);
  }
  /* UM evento por papel, uma vez por musica, com as tres parcelas do tamanho.
     Sem ele, "o personagem esta' do tamanho certo" nao teria medida nenhuma. */
  /* Passos de danca perdidos por o personagem estar cantando: e' o numero que
     mostra, no log do console, se a alternancia danceLeft/danceRight andou fora
     de fase por causa de um canto -- e por isso este evento existe. */
  if(!g_cena_pulos_visto[papel]&&g_cena.p[papel].pulos>0){
   g_cena_pulos_visto[papel]=1;
   telemetry_event("char_danca_pulada",papel,g_cena.p[papel].pulos);
  }
  if(!g_cena_tam_visto[papel]){
   g_cena_tam_visto[papel]=1;
   telemetry_event("char_tamanho",papel,(int)(wm*sc));
   telemetry_event("char_escala",papel,(int)(em*100.0f));
   telemetry_event("char_flip",papel,ci>=0?ch_chars[ci].flipx:0);
  }
  /* PROVA do "-hold": quando o personagem entra numa animacao "-hold" (os olhos
     do pai, a chama do isqueiro do Darnell), o log diz uma vez por papel. Sem
     isso, "o -hold funciona" seria afirmacao sem medida. */
  {int an=cc_anim(&g_cena,papel);
   if(an>=0&&!g_cena_hold_visto[papel]&&an<g_cena.p[papel].pacote.n_anims){
    const char*nm=g_cena.p[papel].pacote.anim[an].nome;
    size_t ln=strlen(nm);
    if(ln>5&&strcmp(nm+ln-5,"-hold")==0){g_cena_hold_visto[papel]=1;telemetry_event("char_hold",papel,an);}
   }}
  g_cena_desenhos[papel]++;
  if(g_cena_desenhos[papel]==1&&papel==CH_PAPEL_DAD)telemetry_event("char_dad_ancora",(int)an.sprite_x,(int)an.sprite_y);
  if(g_cena_desenhos[papel]==1)telemetry_event(papel==CH_PAPEL_GF?"char_gf_primeiro":(papel==CH_PAPEL_BF?"char_bf_primeiro":"char_dad_primeiro"),g_cena_quadros[papel],g_cena_texde_w[papel]);
  if(!g_cena_avisou[papel]&&g_cena_quadros[papel]>=10){g_cena_avisou[papel]=1;telemetry_event(papel==CH_PAPEL_GF?"char_gf_animando":(papel==CH_PAPEL_BF?"char_bf_animando":"char_dad_animando"),g_cena_desenhos[papel],(int)cc_anim(&g_cena,papel));}
 }
 return q;
}

static unsigned short tutorial_lane_button(unsigned int lane){static const unsigned short masks[4]={PAD_LEFT,PAD_DOWN,PAD_UP,PAD_RIGHT};return lane<4?masks[lane]:0;}
/* NOTE KINDS. Nove das 81 combinacoes trazem `k` na nota, e o port jogava o
   campo fora. Dois desses kinds sao CODIGO, nao script, e por isso o
   comportamento deles esta' provado pela fonte, sem invencao:
     `noanim`        -> NoAnimNoteKind.hx: `noanim = true`, e BaseCharacter.hx
                        L561/L577 deixa de tocar o canto quando o kind tem
                        `noanim`. A nota ainda pontua normalmente.
     `non_scoreable` -> NonScoreableNoteKind.hx: `scoreable = false`. Nao entra
                        em tallies.totalNotes (PlayState.hx L2538-2554), o hit
                        nao soma pontos/vida (L3159) nem combo (L3143), e no
                        miss o kind esconde a nota e cancela o evento.
   Os outros 32 kinds (weekend-1-*, mom, ugh, hehPrettyGood, sakura-*,
   censor) NAO sao mais "dado que falta": `tools/gen_notekinds.py` le' os
   scripts do clone da arte e gera `src/engine/notekind_dados.h`, e
   `cena_nota_kind()` aplica o que cada um pede (canto com sufixo ou animacao
   nomeada). O que o script faz por CONDICAO (vida baixa, sorteio, estado) fica
   marcado `condicional` no dado e o port NAO escolhe sozinho. */
/* O QUE O NOTE KIND MANDA TOCAR NESTA NOTA (tools/gen_notekinds.py).
   O script do personagem e' quem decide, e o dado diz:
     tipo 1 -> canto com sufixo ("sing<DIR>-alt", "sing<DIR>-joint"...);
     tipo 2 -> uma animacao nomeada ("punchHigh", "block", "ugh"...).
   Devolve 1 quando o kind RESPONDE por esta nota -- tocando o que o script pede
   ou, quando o personagem nao tem a animacao que o script pede, nao tocando
   nada (no original o playAnimation de um nome inexistente so' registra o
   aviso e o personagem fica onde estava; cair no canto normal seria uma
   animacao que o script nao pediu). E' o `nota_kind_sem_anim` do log: o dado
   pede, a arte nao entrega. Devolve 0 so' quando nao ha kind nenhum a aplicar,
   e aí quem chamou toca o canto normal. */
/* O passo do tremor, uma vez por quadro (o `FlxCamera.update`). O modulo
   `engine/shake.c` tem a conta e o teste; aqui so' se aplica o resultado. */
static void cena_shake_passo(void){shk_passo(&g_shake,TUT_DT,SHAKE_CAM_W,SHAKE_CAM_H);}
static int cena_nota_kind(int papel,const chart_note_t *n,int dir,int missing,
                          long agora,long sing_ms){
 const nk_acao_t *a;const char *kind,*pid;
 if(!n||n->kind==CHART_KIND_NONE)return 0;
 if(papel<0||papel>=CH_CENA_PAPEIS||!g_cena.p[papel].ativo)return 0;
 if(g_cena_ci[papel]<0||g_cena_ci[papel]>=CH_CHAR_COUNT)return 0;
 kind=chart_note_kind_name(n->kind);
 pid=ch_chars[g_cena_ci[papel]].id;
 a=nk_acao(pid,kind);
 if(!a)return 0;
 /* `ordem` e `shake` sao chamadas INCONDICIONAIS dentro da funcao do script
    (darnell-blazin.hxc L306-311: playAnimation, shake, moveToBack) -- por isso
    entram antes de saber se a animacao existe. */
 if(a->ordem==1){g_z_extra[papel]=2000;telemetry_event("nota_kind_fundo",papel,2000);}
 else if(a->ordem==2){g_z_extra[papel]=3000;telemetry_event("nota_kind_frente",papel,3000);}
 if(a->shake_ms>0){
  shk_inicia(&g_shake,(float)a->shake_forca1000/1000.0f,(float)a->shake_ms/1000.0f);
  telemetry_event("nota_kind_shake",a->shake_forca1000,a->shake_ms);
 }
 if(a->tipo==1){
  if(cc_canto_sufixo(&g_cena,papel,dir,missing,a->valor,agora,sing_ms)){
   telemetry_event("nota_kind_sing",papel,(int)dir);
   return 1;}
  telemetry_event("nota_kind_sem_anim",papel,1);
  return 1;   /* o kind agiu: o script NAO cai no canto normal */
 }
 if(a->tipo==2){
  const char *anim=a->valor;
  /* Nome que ALTERNA (punchHigh1/punchHigh2): `alternate = !alternate; return
     alternate ? '1' : '2'` comeca em false, entao a PRIMEIRA chamada usa '1'
     (valor) e a seguinte '2' (valor2). O flip acontece mesmo quando o
     personagem nao tem a animacao, como no script (doAlternate e' chamado
     antes do playAnimation). */
  if(a->alterna&&a->valor2&&a->valor2[0]){
   if(g_kind_alt[papel])anim=a->valor2;
   g_kind_alt[papel]=!g_kind_alt[papel];
  }
  /* A animacao nomeada do kind nao e' canto: cc_toca a coloca como especial e
     ela segura o personagem ate' acabar (o `holdTimer = 0` + a animacao do
     script). Se o personagem nao tem a animacao, fica o registro. */
  if(cc_toca(&g_cena,papel,anim,agora)){
   telemetry_event("nota_kind_anim",papel,(int)strlen(anim));
   return 1;}
  telemetry_event("nota_kind_sem_anim",papel,2);
  return 1;
 }
 return 0;
}
static int nota_noanim(const chart_note_t *n){return n&&chart_note_kind_is(n->kind,"noanim");}
static int nota_nonscoreable(const chart_note_t *n){return n&&chart_note_kind_is(n->kind,"non_scoreable");}
static void tutorial_notes_reset(void){
 u32 k,scorable=0,comkind=0,sempontos=0,noanim=0;
 memset(g_tutorial_note_state,0,sizeof(g_tutorial_note_state));
 /* NOTE SPLASH: pool zerado por musica. A semente muda com o chart para o
    sorteio de variante/quadros nao repetir a mesma sequencia em toda musica,
    mas continua deterministica -- o harness tem de poder repetir a corrida. */
 nspl_init(&g_splash,0x5EED1234u^(u32)g_chart_info.note_count);
 g_splash_log=0;g_splash_desenho=-1;
 g_tutorial_note_score=0;g_tutorial_note_hits=0;g_tutorial_note_misses=0;g_tutorial_note_combo=0;
 g_tutorial_note_feedback=0;g_tutorial_note_feedback_frame=0;g_tutorial_note_visible=-1;g_custo_frames=0;g_custo_emitido=0;
 /* tallies.totalNotes do original: quantas notas do chart contam para o
    resultado. Sao as notas do jogador (must_press), nao as do oponente -- e
    NonScoreableNoteKind.scoreable = false tira as dela do total
    (PlayState.hx L2538-2554). */
 for(k=0;k<g_chart_info.note_count&&k<CHART_MAX_NOTE_STATES;k++){
  chart_note_t nt;
  if(chart_note(k,&nt)<0)continue;
  if(nt.must_press&&!nota_nonscoreable(&nt))scorable++;
  /* Quantas notas chegaram com nome de kind, e quantas sao dos dois kinds que
     o port implementa. E' o que o sweep usa para provar que o `k` sobreviveu
     do JSON ao runtime -- sem isso, um formato que perdesse o campo de novo
     passaria despercebido (foi o que aconteceu ate' a 0.2.128). */
  if(nt.kind!=CHART_KIND_NONE){comkind++;if(nota_nonscoreable(&nt))sempontos++;if(nota_noanim(&nt))noanim++;}
 }
 telemetry_event("chart_notas_com_kind",(int)comkind,(int)sempontos);
 if(noanim)telemetry_event("chart_notas_noanim",(int)noanim,0);
 fnf_play_reset(&g_play,FNF_SCORING_PBOT1,(int)scorable);
}
static void tutorial_notes_update(u32 sample,unsigned short pressed,u32 frame){
 u32 i;unsigned int lane;
 if(g_chart_status!=0)return;
 /* A splash vive 4 quadros a 24 fps +- 2 (~167 ms): quem a mata e' o relogio
    da musica, nao o quadro. NoteSplash.onAnimationFinished -> kill(). */
 nspl_passo(&g_splash,(u32)(sample/48u));
 for(i=0;i<g_chart_info.note_count&&i<CHART_MAX_NOTE_STATES;i++){
  chart_note_t note;
  if(chart_note(i,&note)<0||!note.must_press||g_tutorial_note_state[i])continue;
  if((s32)sample-(s32)note.sample>(s32)TUTORIAL_NOTE_LATE){
   /* NonScoreableNoteKind.onNoteMiss: esconde a nota e CANCELA o evento --
      sem dano, sem miss, sem canto de erro. Estado 4 = "cancelada": o
      desenho ja' pula qualquer estado diferente de 0. */
   if(nota_nonscoreable(&note)){g_tutorial_note_state[i]=4;telemetry_event("nota_kind_cancelada",(int)i,note.kind);continue;}
   g_tutorial_note_state[i]=2;
   /* COMBO DROP da GF. BaseCharacter.hx L614-626 (onNoteMiss): o evento leva
      `Highscore.tallies.combo` -- o combo ATUAL, antes de ser zerado -- e a GF
      toca o maior `drop<N>` que ele alcanca. Tem de ser lido antes do
      fnf_play_miss(), que e' quem zera o combo. */
   {int combo_antes=g_play.combo;
    if(!nota_noanim(&note)&&g_cena.p[CH_PAPEL_BF].ativo&&note.lane<=3){
     long ms=(long)(sample/48u),cm=cena_canto_ms(CH_PAPEL_BF,1);
     if(!cena_nota_kind(CH_PAPEL_BF,&note,(int)note.lane,1,ms,cm))
      cc_canto(&g_cena,CH_PAPEL_BF,(int)note.lane,1,ms,cm);
    }
    fnf_play_miss(&g_play);
    if(g_cena.p[CH_PAPEL_GF].ativo&&cc_drop(&g_cena,CH_PAPEL_GF,combo_antes,(long)(sample/48u)))telemetry_event("char_combo_drop",combo_antes,0);}g_tutorial_note_misses=(u32)g_play.missed;g_tutorial_note_combo=(u32)g_play.combo;g_tutorial_note_score=(s32)g_play.score;g_tutorial_note_feedback=4;g_tutorial_note_feedback_frame=frame;}
 }
 /* Notas do oponente: no Tutorial a GF e a oponente, pelo gf.hxc, entao
    ela canta quando a nota dela chega. Estado 3 marca "ja cantada". */
 for(i=0;i<g_chart_info.note_count&&i<CHART_MAX_NOTE_STATES;i++){
  chart_note_t note;
  if(chart_note(i,&note)<0||note.must_press||g_tutorial_note_state[i]||note.lane>3)continue;
  if((s32)sample>=(s32)note.sample){g_tutorial_note_state[i]=3;g_gf_sing=(int)note.lane;g_gf_sing_at=sample;/* o OPONENTE generico canta a mesma nota: o slot dad e' o papel de quem nao e' o jogador (ch_cena_plano) */if(note.lane<=3){
   long ms=(long)(sample/48u);
   long cm=cena_canto_ms(CH_PAPEL_DAD,0),cg=cena_canto_ms(CH_PAPEL_GF,0);
   if(!nota_noanim(&note)){
    if(g_cena.p[CH_PAPEL_DAD].ativo&&!cena_nota_kind(CH_PAPEL_DAD,&note,(int)note.lane,0,ms,cm))
     cc_canto(&g_cena,CH_PAPEL_DAD,(int)note.lane,0,ms,cm);
    if(g_cena.p[CH_PAPEL_GF].ativo&&!cena_nota_kind(CH_PAPEL_GF,&note,(int)note.lane,0,ms,cg))
     cc_canto(&g_cena,CH_PAPEL_GF,(int)note.lane,0,ms,cg);
   }
  }}
 }
 for(lane=0;lane<4;lane++)if(pressed&tutorial_lane_button(lane)){
  int best=-1;u32 best_abs=0xffffffffu;
  for(i=0;i<g_chart_info.note_count&&i<CHART_MAX_NOTE_STATES;i++){
   chart_note_t note;s32 delta;u32 absolute;
   if(g_tutorial_note_state[i]||chart_note(i,&note)<0||!note.must_press||note.lane!=lane)continue;
   delta=(s32)note.sample-(s32)sample;absolute=(u32)(delta<0?-delta:delta);
   if(absolute<=TUTORIAL_NOTE_JUDGE_WINDOW&&absolute<best_abs){best=(int)i;best_abs=absolute;}
  }
  if(best>=0){s32 offset;chart_note_t note;
   /* chart_note returns -1 for an out-of-range index and leaves *out alone.
    * The (void) cast that used to be here does not silence warn_unused_result
    * -- it only silences -Wunused-value -- so on a newer gcc the build died on
    * this line, and on an older one it silently read an uninitialised note if
    * best ever pointed past the table. best comes from the loop above, which
    * already bounds it, but "already bounds it" is not a contract. */
   if(chart_note((u32)best,&note)<0)best=-1;
   else{fnf_judge_t judge;
    offset=(s32)sample-(s32)note.sample;g_tutorial_note_state[best]=1;g_bf_sing=(int)lane;g_bf_sing_at=sample;
    if(!nota_noanim(&note)&&g_cena.p[CH_PAPEL_BF].ativo&&lane<=3){
     long ms=(long)(sample/48u),cm=cena_canto_ms(CH_PAPEL_BF,0);
     if(!cena_nota_kind(CH_PAPEL_BF,&note,(int)lane,0,ms,cm))
      cc_canto(&g_cena,CH_PAPEL_BF,(int)lane,0,ms,cm);
    }
    /* NonScoreableNoteKind: a nota e' consumida, o strum reage, mas nada
       entra em pontos, vida, combo ou acerto -- PlayState.hx L3159 poe todo
       o applyScore() dentro de `if (note.scoreable)`. */
    if(nota_nonscoreable(&note)){telemetry_event("nota_kind_sem_pontos",best,(int)offset);continue;}
    /* O original mede o erro em milissegundos (PlayState.goodNoteHit:
       noteDiff = songPosition - noteTime). A 48 kHz, um sample e' 1/48 ms. */
    judge=fnf_play_hit(&g_play,(float)offset/48.0f);
    /* NOTE SPLASH: so' em SICK, e so' quando a nota conta. PlayState.goodNoteHit
       poe `doesNotesplash = daRating == 'sick'` no evento, e
       Strumline.playNoteSplash (L1076) exige showNotesplash. Uma nota
       non_scoreable nao chega aqui (sai no `continue` acima), como no original,
       onde ela nem entra no goodNoteHit. */
    if(judge==FNF_JUDGE_SICK&&g_splash_arte_ok){
     int si=nspl_novo(&g_splash,(int)lane,(u32)(sample/48u));
     /* Uma linha por splash: o harness exige que o numero de `splash_novo` seja
        EXATAMENTE o de SICKs do resultado -- nem um a mais (apareceria em
        GOOD/BAD) nem um a menos (SICK sem brilho). */
     if(si>=0){g_splash_log++;
      telemetry_event("splash_novo",(int)g_splash.it[si].lane,g_splash.it[si].variante);}}
    /* COMBO / DROP da GF no ACERTO. BaseCharacter.hx L586-600: o evento leva
       `tallies.combo + 1`, ou seja o combo ja' contando esta nota -- que e' o
       `g_play.combo` depois do fnf_play_hit. `sick`/`good` tocam `combo<N>`
       (so' o numero EXATO: combo50 nao vale no 51); `bad`/`shit` quebram o
       combo e tocam o `drop<N>`. Sem nenhuma das duas animacoes no
       personagem, cc_combo/cc_drop devolvem 0 e nada e' inventado. */
    if(g_cena.p[CH_PAPEL_GF].ativo&&(judge==FNF_JUDGE_SICK||judge==FNF_JUDGE_GOOD)){
     if(cc_combo(&g_cena,CH_PAPEL_GF,(int)g_play.combo,(long)(sample/48u)))telemetry_event("char_combo",(int)g_play.combo,judge);
    }else if(g_cena.p[CH_PAPEL_GF].ativo){
     if(cc_drop(&g_cena,CH_PAPEL_GF,(int)g_play.combo,(long)(sample/48u)))telemetry_event("char_combo_drop",(int)g_play.combo,judge);
    }
    g_tutorial_note_hits=(u32)g_play.total_notes_hit;
    g_tutorial_note_combo=(u32)g_play.combo;
    g_tutorial_note_score=(s32)g_play.score;
    g_tutorial_note_feedback=judge==FNF_JUDGE_SICK?1:judge==FNF_JUDGE_GOOD?2:judge==FNF_JUDGE_BAD?3:4;
    g_tutorial_note_feedback_frame=frame;telemetry_event("tutorial_note_hit",best,(int)offset);}}
  else{fnf_play_ghost(&g_play);g_tutorial_note_score=(s32)g_play.score;g_tutorial_note_feedback=5;g_tutorial_note_feedback_frame=frame;telemetry_event("tutorial_note_ghost",(int)lane,(int)sample);}
 }
}
static float tutorial_note_y(s32 delta){return 26.3f+(float)delta*(0.45f/(48.0f*2.0f))*g_tut_scroll;}
static qword_t *tutorial_notes_draw(qword_t*q,texbuffer_t*atlas,texbuffer_t*font,u32 sample,unsigned short held,u32 frame){
 u32 i,visible=0;unsigned int lane;if(g_tutorial_note_asset_status!=0||g_chart_status!=0)return q;q=bind_texture_filter(q,atlas,0);
 /* ZOOM DO HUD (camHUD.zoom): receptores, trilhas e notas estao nessa camera
    (PlayState.hx L2257-2264), entao posicao E tamanho saem escalados por
    g_hud_zoom em torno do centro da tela. O trecho de TEXTURA nao muda -- o
    zoom e' so' de tela. */
 for(lane=0;lane<4;lane++){
  float rw=hz_s(g_note_w[lane]),rh=hz_s(g_note_h[lane]),ry=hz_y(g_receptor_y[lane]);
  float x=hz_x(g_receptor_x[lane]);
  /* `uw`/`vh` sao o tamanho do PEDACO DA TEXTURA, nao o tamanho na tela: por
     isso NAO levam hz_s() (o zoom do camHUD muda quanto a nota ocupa na tela,
     nao quantos pixels do atlas ela amostra). O tamanho desenhado -- `rw`/`rh`
     acima -- e' que passa por hz_s().

     E' a metade desenhada da arte do original: `notes.png` tem quadros de
     157x154 (down/up) e 154x157 (left/right) e o note style os desenha em
     `scale` 0,7 (preload/data/notestyles/funkin.json) -> 109,9x107,8 unidades
     de 1280x720 -> 55x54 em 640x360. g_note_w/g_note_h guardam exatamente
     isso, e a celula do atlas tem TUTORIAL_NOTE_SOURCE_TILE px.

     PENDENCIA REGISTRADA, NAO ADIVINHADA: amostrar (u,v)+(54,55) de uma celula
     de 56 px corta ~2 px da borda direita/embaixo; amostrar a celula inteira
     (56x56) preservaria a arte toda, mas mostraria a borda do vizinho SE o
     atlas empacotar as celulas sem folga. Quem decide e' o atlas
     `tutorial_hud_official_320x256_gsalpha.rgba`, que NAO esta' neste
     repositorio (veio pronto do Drive) -- entao o comportamento fica como
     esta' ate' haver o arquivo para medir. */
  q=sprite_region_alpha(q,x,ry,rw,rh,lane*TUTORIAL_NOTE_SOURCE_TILE,TUTORIAL_NOTE_SOURCE_TILE,g_note_w[lane],g_note_h[lane],128);
  q=sprite_region_alpha(q,hz_x(g_receptor_x[lane]+320.0f),ry,rw,rh,lane*TUTORIAL_NOTE_SOURCE_TILE,TUTORIAL_NOTE_SOURCE_TILE,g_note_w[lane],g_note_h[lane],128);}
 for(i=0;i<g_chart_info.note_count&&i<CHART_MAX_NOTE_STATES;i++){chart_note_t note;s32 delta,tail_delta;float x,y,tail_y;int state;if(chart_note(i,&note)<0||note.lane>3)continue;state=g_tutorial_note_state[i];delta=(s32)note.sample-(s32)sample;tail_delta=(s32)(note.sample+note.duration)-(s32)sample;x=g_note_x[note.lane]+(note.must_press?320.0f:0.0f);if(note.duration&&(!note.must_press||state!=2)&&tail_delta>0&&delta<(s32)TUTORIAL_NOTE_AHEAD){y=tutorial_note_y(delta);tail_y=tutorial_note_y(tail_delta);if(tail_y>y){float top=y+g_note_h[note.lane]*.28f;if(top<26.3f)top=26.3f;if(tail_y>360.0f)tail_y=360.0f;if(tail_y>top)q=rect_alpha(q,hz_x(x+g_note_w[note.lane]*.39f),hz_y(top),hz_x(x+g_note_w[note.lane]*.61f),hz_y(tail_y),224,224,240,150);}}/* As notas do oponente nunca eram retiradas: so o teste de must_press
   marcava estado, entao elas desciam alem do receptor e iam parar
   perto dos pes do BF. No oficial elas somem quando chegam. O canto
   da GF ja marca estado 3 exatamente nesse instante, entao basta
   usar o mesmo marcador para retira-las. */
if((note.must_press&&state!=0)||(!note.must_press&&state==3)||delta<-(s32)TUTORIAL_NOTE_LATE||delta>(s32)TUTORIAL_NOTE_AHEAD)continue;
y=tutorial_note_y(delta);q=sprite_region(q,hz_x(x),hz_y(y),hz_s(g_note_w[note.lane]),hz_s(g_note_h[note.lane]),note.lane*TUTORIAL_NOTE_SOURCE_TILE,0.0f,g_note_w[note.lane],g_note_h[note.lane]);visible++;}if((int)visible!=g_tutorial_note_visible){g_tutorial_note_visible=(int)visible;telemetry_event("tutorial_notes_visible",(int)visible,(int)sample);}(void)font;(void)frame;(void)held;return q;}

/* NOTE SPLASH no quadro. No original o grupo `noteSplashes` tem zIndex 50 e as
   notas tem 30 (Strumline.hx L225-238), entao o brilho desenha POR CIMA das
   notas -- e' por isso que esta chamada vem depois de tutorial_notes_draw.
   A celula mora no atlas do HUD (tex2), na faixa livre abaixo da barra de
   vida (ver engine/splash.c): nenhuma textura nova, nenhum byte de VRAM novo. */
static qword_t *splash_draw(qword_t*q,texbuffer_t*t,u32 sample){
 int i,n=0;u32 ms=(u32)(sample/48u);
 if(!g_splash_arte_ok)return q;
 q=bind_texture_filter(q,t,1);   /* LINEAR: a celula e' ampliada ~5x */
 for(i=0;i<SP_MAX;i++){
  int qd=nspl_quadro(&g_splash,i,ms),lane;const nspl_cel_t*c;float x,y;
  if(qd<0)continue;
  lane=(int)g_splash.it[i].lane;
  c=nspl_celula(lane,(int)g_splash.it[i].variante);
  if(!c)continue;
  /* O original ancora o splash no canto do receptor (Strumline.hx L1087:
     strum.x + getXPos + INITIAL_OFFSET) e desloca o desenho 0,3 do tamanho
     para cima e para a esquerda (NoteSplash.hx L56). A conta esta' em
     engine/splash.c, com a fonte linha a linha. */
  nspl_posicao(g_receptor_x[lane],g_receptor_y[lane],c,&x,&y);
  q=sprite_region_alpha(q,hz_x(x),hz_y(y),hz_s(c->dw),hz_s(c->dh),
    (float)SPLASH_REG_X+(float)c->cx,
    (float)(SPLASH_REG_Y+(u32)qd*SPLASH_FAIXA+(SPLASH_FAIXA-c->ch)/2u),
    (float)c->cw,(float)c->ch,(unsigned char)SPLASH_ALPHA);
  n++;
 }
 if(n!=g_splash_desenho){g_splash_desenho=n;telemetry_event("splash_ativos",n,g_splash.criados);}
 return q;}
#define RES_ASSET_DIR "mass:/FNFPS2/assets/"
static void resultado_soltar(void);
static int resultado_rank(void);   /* o rank da TELA (com o override de harness) */
static void resultado_carregar(sp_t *pool,texbuffer_t *tpl,int dificuldade)
{
 int i,maior=0;
 resultado_soltar();
 g_res_ok=res_carregar(&g_res,RES_ASSET_DIR);
 g_res_bg_ok=0;g_res_bg_addr=0;
 for(i=0;i<(int)RES_FOLHAS;i++){
  unsigned int bytes=(unsigned int)g_res_folha[i].w*g_res_folha[i].h*4u,addr=0;
  g_res_addr[i]=0;g_res_frame[i]=0xffffffffu;
  if(bytes>=(unsigned int)maior)maior=(int)bytes;
  if(i<(int)g_res_ok&&sp_chave(pool,SP_CHAVE_TELA_BASE+0x10u+(unsigned)i,bytes,&addr)>=0){
   g_res_addr[i]=addr;
  }else if(i<(int)g_res_ok){
   telemetry_event("resultado_sem_vram",i,(int)sp_livre(pool));
  }
 }
 /* A MAQUINA QUE CAI ATRAS DO PLACAR: um pacote CFZ4 e UM quadro na VRAM (o
    resto vem do pendrive quadro a quadro, como as folhas). */
 g_res_ss_ok=0;g_res_ss_tentou=0;g_res_ss_addr=0;g_res_ss_frame=-1;
 {
  unsigned int bytes=(unsigned int)g_res_ss.w*(unsigned int)g_res_ss.h*4u,addr=0;
  if(bytes>(unsigned int)maior)maior=(int)bytes;
  if(pst_abrir(RES_ASSET_DIR RES_SS_TEX,&g_res_ss_pack)==PST_OK){
   g_res_ss_tentou=1;
   if(sp_chave(pool,SP_CHAVE_TELA_BASE+0x26u,bytes,&addr)>=0){
    g_res_ss_addr=addr;g_res_ss_ok=1;
   }else telemetry_event("resultado_sem_vram",0x26,(int)sp_livre(pool));
  }
  telemetry_event("resultado_soundsystem",g_res_ss_ok,(int)bytes);
 }
 telemetry_event("resultado_arte",g_res_ok,(int)RES_FOLHAS);
 if(g_res_ok>0){
  if(g_res_pixels_bytes<(unsigned int)maior){
   free(g_res_pixels);
   g_res_pixels=(unsigned char*)memalign(64,(size_t)maior);
   g_res_pixels_bytes=g_res_pixels?((unsigned int)maior):0u;
   if(!g_res_pixels)g_res_ok=0;
  }
  if(g_res_pixels)telemetry_event("resultado_pixels",(int)maior,0);
 }
 /* O FUNDO NAO TEM TEXTURA. `FlxGradient.createGradientFlxSprite(1280, 720,
   [0xFFFECC5C, 0xFFFDC05C], 90)` e' um `chunkSize` de 90 px sobre 720, isto e',
   OITO faixas horizontais de cor solida -- o desenho faz oito retangulos
   (`res_bg_cor`) em vez de esticar uma textura de 16x360. Eram 23.040 B de
   VRAM que a tela pagava para sempre; agora sao zero, e o espaco vai para a
   arte que ainda faltava (a maquina do `soundSystem`). */
 g_res_bg_ok=1;
 telemetry_event("resultado_fundo",g_res_bg_ok,0);
/* A ARTE DO RANK. Duas imagens estaticas do rank CONQUISTADO (o par escolhido
    por `fnf_play_rank`), lidas para `g_up_bounce` e subidas para o pool. Nao se
    carrega os cinco ranks: cada partida mostra um. */
 g_res_rank_ok=0;g_res_rank_vert_addr=0;g_res_rank_hor_addr=0;
 {const res_rank_t *rr=res_rank(resultado_rank());
  int okv=0,okh=0;
  if(rr){
   char caminho[96];unsigned int addr=0;
   snprintf(caminho,sizeof(caminho),"mass:/FNFPS2/assets/resultado/%s",rr->vert.arquivo);
   if(read_exact_path(caminho,g_up_bounce,(int)(rr->vert.w*rr->vert.h*4u))==0){
    texbuffer_t t=*tpl;t.width=rr->vert.w;t.psm=GS_PSM_32;t.address=0;
    if(sp_chave(pool,SP_CHAVE_TELA_BASE+0x21u,rr->vert.w*rr->vert.h*4u,&addr)>=0){
     t.address=addr;setup_texture_dims(&t,rr->vert.w,rr->vert.h);
     FlushCache(0);upload_raw(&t,g_up_bounce,rr->vert.w,rr->vert.h);
     g_res_rank_vert_addr=addr;okv=1;
    }
   }
   snprintf(caminho,sizeof(caminho),"mass:/FNFPS2/assets/resultado/%s",rr->hor.arquivo);
   if(read_exact_path(caminho,g_up_bounce,(int)(rr->hor.w*rr->hor.h*4u))==0){
    texbuffer_t t=*tpl;t.width=rr->hor.w;t.psm=GS_PSM_32;t.address=0;
    if(sp_chave(pool,SP_CHAVE_TELA_BASE+0x22u,rr->hor.w*rr->hor.h*4u,&addr)>=0){
     t.address=addr;setup_texture_dims(&t,rr->hor.w,rr->hor.h);
     FlushCache(0);upload_raw(&t,g_up_bounce,rr->hor.w,rr->hor.h);
     g_res_rank_hor_addr=addr;okh=1;
    }
   }
   g_res_rank_ok=okv&&okh;
   telemetry_event("resultado_rank_arte",g_res_rank_ok,
                   (int)((rr->vert.w*rr->vert.h*4u)+(rr->hor.w*rr->hor.h*4u)));
  }}
 /* O PLACAR. Um atlas RGBA cru (nao CFZ4): as dez posicoes mostram digitos
    diferentes ao mesmo tempo, e durante o shuffle qualquer digito pode cair em
    qualquer slot. `g_up_bounce` tem 512x256x4 = 524.288 B e o atlas ocupa
    508.480 B, entao ele cabe no buffer de leitura que a tela ja' usa. */
 g_res_score_ok=0;g_res_score_addr=0;
 {unsigned int bytes=(unsigned int)g_res_score.w*g_res_score.h*
                      (unsigned int)(g_res_score.bpp/8u),addr=0;
  int psm=g_res_score.bpp==16?GS_PSM_16:GS_PSM_32;
  char caminho[96];
  snprintf(caminho,sizeof(caminho),"mass:/FNFPS2/assets/%s",g_res_score.arquivo);
  if(bytes<=512u*256u*4u&&
     read_exact_path(caminho,g_up_bounce,(int)bytes)==0&&
     sp_chave(pool,SP_CHAVE_TELA_BASE+0x23u,bytes,&addr)>=0){
   texbuffer_t t=*tpl;
   t.width=g_res_score.w;t.psm=psm;t.address=addr;
   setup_texture_dims(&t,(int)g_res_score.w,(int)g_res_score.h);
   FlushCache(0);
   if(psm==GS_PSM_16)upload_raw_psm(&t,g_up_bounce,(int)g_res_score.w,
                                    (int)g_res_score.h,GS_PSM_16);
   else upload_raw(&t,g_up_bounce,(int)g_res_score.w,(int)g_res_score.h);
   g_res_score_addr=addr;g_res_score_ok=1;
  }else{
   telemetry_event("resultado_sem_vram",-2,(int)sp_livre(pool));
  }
  telemetry_event("resultado_placar_arte",g_res_score_ok,(int)bytes);}
 /* A ANIMACAO DO PERSONAGEM POR RANK. Cada entrada e' um pacote CFZ4 aberto
    pelo `pack_multi` (tabela na RAM, quadro no pendrive) e UM quadro na VRAM.
    O pacote do rank `PERFECT` e' o MESMO arquivo do `PERFECT_GOLD` (o
    empacotador reaproveita a arte), entao abrir os dois custaria duas tabelas
    para o mesmo dado -- inofensivo, e cada partida so' abre o rank que mostra.
    O `PERFECT`/`PERFECT_GOLD` tem a entrada dos `hearts` 4,41 s depois da
    primeira; as duas ficam residentes, que e' o que o orcamento contou. */
 g_res_rrf_ok=0;
 {int idx=res_rr_indice(resultado_rank());
  if(idx>=0){
   const res_rr_rank_t *rk=res_rr_rank(idx);
   unsigned int maior=0;
   int k,abertos;
   for(k=0;k<(int)rk->n;k++){
    unsigned int b=(unsigned int)rk->entradas[k].w*(unsigned int)rk->entradas[k].h*
                   (unsigned int)(rk->entradas[k].psm/8u);
    if(b>maior)maior=b;
   }
   if(maior>g_res_rrf_pixels_bytes){
    free(g_res_rrf_pixels);
    g_res_rrf_pixels=(unsigned char*)memalign(64,(size_t)maior);
    g_res_rrf_pixels_bytes=g_res_rrf_pixels?maior:0u;
   }
   res_rr_init(&g_res_rrf);
   abertos=g_res_rrf_pixels?res_rr_carregar(&g_res_rrf,RES_ASSET_DIR "resultado/",idx):0;
   for(k=0;k<(int)rk->n;k++){
    const res_rr_entrada_t *e=&rk->entradas[k];
    unsigned int addr=0;
    g_res_rrf_addr[k]=0;g_res_rrf_frame[k]=0xffffffffu;g_res_rrf_vram[k]=0;
    if(!g_res_rrf.p[k].aberto)continue;
    if(sp_chave(pool,SP_CHAVE_TELA_BASE+0x27u+(unsigned)k,e->vram_bytes,&addr)>=0){
     g_res_rrf_addr[k]=addr;g_res_rrf_vram[k]=1;g_res_rrf_ok++;
    }else telemetry_event("resultado_sem_vram",0x27+k,(int)sp_livre(pool));
   }
   g_res_rrf_rank=idx;
   g_res_rrf_ram=res_rr_ram(&g_res_rrf);
   telemetry_event("resultado_rrf",g_res_rrf_ok,
     (int)(rk->n*100u+abertos)+0);
   {unsigned int soma=0;
    for(k=0;k<(int)rk->n;k++)
     if(g_res_rrf_vram[k])soma+=rk->entradas[k].vram_bytes;
    telemetry_event("resultado_rrf_vram",(int)soma,(int)sp_livre(pool));}
  }else telemetry_event("resultado_rrf",-1,0);}
 /* OS CONTADORES DE JULGAMENTO (96x44 = 16.896 B). Aqui cabe tudo no buffer de
    leitura; se nao coubesse, o teste acusaria o tamanho errado no plano. */
 g_res_tally_ok=0;g_res_tally_addr=0;
 {unsigned int bytes=(unsigned int)g_res_tallies.w*g_res_tallies.h*4u,addr=0;
  char caminho[96];
  snprintf(caminho,sizeof(caminho),"mass:/FNFPS2/assets/%s",
           g_res_tallies.arquivo);
  if(bytes<=512u*256u*4u&&
     read_exact_path(caminho,g_up_bounce,(int)bytes)==0&&
     sp_chave(pool,SP_CHAVE_TELA_BASE+0x24u,bytes,&addr)>=0){
   texbuffer_t t=*tpl;
   t.width=g_res_tallies.w;t.psm=GS_PSM_32;t.address=addr;
   setup_texture_dims(&t,(int)g_res_tallies.w,(int)g_res_tallies.h);
   FlushCache(0);
   upload_raw(&t,g_up_bounce,(int)g_res_tallies.w,(int)g_res_tallies.h);
   g_res_tally_addr=addr;g_res_tally_ok=1;
  }else{
   telemetry_event("resultado_sem_vram",-3,(int)sp_livre(pool));
  }
  telemetry_event("resultado_tallies_arte",g_res_tally_ok,(int)bytes);}
 /* O PERCENTUAL DE CONCLUSAO: tres atlas de digitos e as duas imagens de texto.
    Cada um pede o seu pedaco de VRAM; se algum nao couber, o teste acusa (a
    tela do original tem os cinco, e degradar a imagem e' o que resolve --
    sumir com a peca, nao). */
 g_res_cp_ok=0;g_res_cp_texto_addr=0;g_res_cp_texto_small_addr=0;
 {int k,certo=1;unsigned int bytes_total=0;
  for(k=0;k<(int)RES_CP_ATLASES;k++){
   const res_cp_atlas_t *at=&g_res_cp_atlas[k];
   unsigned int bytes=(unsigned int)(at->w*at->h*4u),addr=0;
   char caminho[96];
   g_res_cp_addr[k]=0;
   snprintf(caminho,sizeof(caminho),"mass:/FNFPS2/assets/%s",at->arquivo);
   if(bytes<=512u*256u*4u&&
      read_exact_path(caminho,g_up_bounce,(int)bytes)==0&&
      sp_chave(pool,SP_CHAVE_TELA_BASE+0x25u+(unsigned)k,bytes,&addr)>=0){
    texbuffer_t t=*tpl;
    t.width=at->w;t.psm=GS_PSM_32;t.address=addr;
    setup_texture_dims(&t,(int)at->w,(int)at->h);
    FlushCache(0);
    upload_raw(&t,g_up_bounce,(int)at->w,(int)at->h);
    g_res_cp_addr[k]=addr;bytes_total+=bytes;
   }else{certo=0;telemetry_event("resultado_sem_vram",-(4+k),(int)sp_livre(pool));}
  }
  /* As duas imagens de texto: "CLEAR" com o `%` e o `%` pequeno da barra. */
  {const res_cp_img_t *imgs[2];unsigned int *dest[2];int q;
   imgs[0]=&g_res_cp_texto;dest[0]=&g_res_cp_texto_addr;
   imgs[1]=&g_res_cp_texto_small;dest[1]=&g_res_cp_texto_small_addr;
   for(q=0;q<2;q++){
    const res_cp_img_t *im=imgs[q];
    unsigned int bytes=(unsigned int)(im->w*im->h*4u),addr=0;
    char caminho[96];
    *dest[q]=0;
    snprintf(caminho,sizeof(caminho),"mass:/FNFPS2/assets/%s",im->arquivo);
    if(bytes<=512u*256u*4u&&
       read_exact_path(caminho,g_up_bounce,(int)bytes)==0&&
       sp_chave(pool,SP_CHAVE_TELA_BASE+0x28u+(unsigned)q,bytes,&addr)>=0){
     texbuffer_t t=*tpl;
     t.width=im->w;t.psm=GS_PSM_32;t.address=addr;
     setup_texture_dims(&t,(int)im->w,(int)im->h);
     FlushCache(0);
     upload_raw(&t,g_up_bounce,(int)im->w,(int)im->h);
     *dest[q]=addr;bytes_total+=bytes;
    }else{certo=0;telemetry_event("resultado_sem_vram",-(6+q),(int)sp_livre(pool));}
   }
  }
  g_res_cp_ok=certo;
  telemetry_event("resultado_clear_arte",g_res_cp_ok,(int)bytes_total);}
 /* A IMAGEM DA DIFICULDADE (`resultScreen/diff_<id>`, ResultState.hx L332-334).
    Nao se carrega os cinco arquivos: cada partida mostra o da dificuldade
    jogada. Se o arquivo nao estiver no pendrive, a tela perde so' essa imagem
    (a barra preta e' desenhada de qualquer jeito, porque ela nao e' textura). */
 g_res_dif_ok=0;g_res_dif_addr=0;g_res_dif_ind=-1;g_res_dif_h=0.0f;
 {int di=res_dif_indice(dificuldade);
  if(di>=0&&di<(int)RES_DIFS){
   const res_dif_t *df=&g_res_dif[di];
   unsigned int bytes=(unsigned int)df->w*df->h*4u,addr=0;
   char caminho[96];
   snprintf(caminho,sizeof(caminho),"mass:/FNFPS2/assets/%s",df->arquivo);
   g_res_dif_ind=di;
   if(bytes<=512u*256u*4u&&
      read_exact_path(caminho,g_up_bounce,(int)bytes)==0&&
      sp_chave(pool,SP_CHAVE_TELA_BASE+0x30u,bytes,&addr)>=0){
    texbuffer_t t=*tpl;
    t.width=df->w;t.psm=GS_PSM_32;t.address=addr;
    setup_texture_dims(&t,(int)df->w,(int)df->h);
    FlushCache(0);
    upload_raw(&t,g_up_bounce,(int)df->w,(int)df->h);
    g_res_dif_addr=addr;g_res_dif_ok=1;
    g_res_dif_h=(float)df->h;
   }else{
    telemetry_event("resultado_sem_vram",-9,(int)sp_livre(pool));
   }
   telemetry_event("resultado_dif_arte",g_res_dif_ok,bytes);
  }else telemetry_event("resultado_dif_arte",-1,dificuldade);}
 /* O NOME DA MUSICA (`resultScreen/tardlingSpritesheet` + `FlxBitmapText`).
    Um bitmap por titulo, do tamanho exato do texto: e' o que o
    `updateTextBitmap` do original monta em tempo de execucao e aqui vem
    pronto do empacotador. A escala de cada titulo esta' no dado. */
 g_res_nome_ok=0;g_res_nome_addr=0;g_res_nome_pronto=0;
 {const res_nome_arte_t *nm=g_play_song?res_nome_do_id(g_play_song->id):0;
  if(nm){
   unsigned int bytes=nm->vram_bytes,addr=0;
   char caminho[96];
   snprintf(caminho,sizeof(caminho),"mass:/FNFPS2/assets/%s",nm->arquivo);
   if(bytes<=512u*256u*4u&&
      read_exact_path(caminho,g_up_bounce,(int)bytes)==0&&
      sp_chave(pool,SP_CHAVE_TELA_BASE+0x31u,bytes,&addr)>=0){
    texbuffer_t t=*tpl;
    t.width=nm->w;t.psm=GS_PSM_32;t.address=addr;
    setup_texture_dims(&t,(int)nm->w,(int)nm->h);
    FlushCache(0);
    upload_raw(&t,g_up_bounce,(int)nm->w,(int)nm->h);
    g_res_nome_addr=addr;g_res_nome_ok=1;
   }else{
    telemetry_event("resultado_sem_vram",-10,(int)sp_livre(pool));
   }
   telemetry_event("resultado_nome_arte",g_res_nome_ok,(int)bytes);
  }else telemetry_event("resultado_nome_arte",-1,0);} /* Quanto sobrou do pool DEPOIS de toda a arte da tela: e' o numero que
    decide se cabe a proxima peca (medido, nao estimado). */
 telemetry_event("resultado_vram",(int)sp_livre(pool),(int)RES_SS_VRAM);
}
/* A ANIMACAO DO PERSONAGEM POR RANK, no desenho.
   ONDE ELA ENTRA: as entradas tem `zIndex` 499..501 -- DEPOIS dos percentuais
   (450) e ANTES da faixa vertical do rank (990). Como sao no maximo duas, uma
   ordenacao de dois itens basta e nao inventa ordem nenhuma.
   CADA QUADRO SO' SOBE PARA A VRAM QUANDO MUDA (`g_res_rrf_frame`), igual as
   folhas: reler o bloco do pendrive num quadro de video em que o quadro da
   animacao e' o mesmo seria I/O a toa -- e' o mesmo criterio do `pack_stream`.
   O que NAO se pula: a leitura do bloco no quadro em que ele muda. A tabela do
   pacote diz o offset, o leitor descomprime no buffer de UM quadro. */
static qword_t *resultado_rrf_arte(qword_t *q, texbuffer_t *tpl)
{
 int k,i,n=0,desenhadas=0,ordem[RES_RR_MAX_ENTRADAS];
 const res_rr_rank_t *rk;
 unsigned long rms=(unsigned long)g_res_ms;
 if(!g_res_rrf_ok)return q;
 rk=res_rr_rank(g_res_rrf_rank);
 if(!rk)return q;
 for(k=0;k<(int)rk->n&&k<(int)RES_RR_MAX_ENTRADAS;k++)
  if(g_res_rrf_vram[k])ordem[n++]=k;
 for(i=0;i<n;i++)for(k=i+1;k<n;k++)
  if(rk->entradas[ordem[k]].z<rk->entradas[ordem[i]].z){
   int t=ordem[i];ordem[i]=ordem[k];ordem[k]=t;}
 for(i=0;i<n;i++){
  const res_rr_entrada_t *e=&rk->entradas[ordem[i]];
  int fr=res_rr_quadro(e,rms);
  texbuffer_t t;
  if(fr<0)continue;                      /* ainda invisivel: so' o delay  */
  /* O INSTANTE em que a entrada aparece: `getBFDelay()` + o `delay` dela. E'
     o que prova o dado do plano (o `hearts` do PERFECT entra 4,41 s depois do
     `bed`) sem depender de olhar a tela. */
  if(!(g_res_rrf_visto&(1<<ordem[i]))){g_res_rrf_visto|=1<<ordem[i];
   telemetry_event("resultado_rrf_entra",ordem[i],g_res_ms);}
  if(fr!=(int)g_res_rrf_frame[ordem[i]]){
   if(res_rr_pixels(&g_res_rrf,ordem[i],fr,g_res_rrf_pixels)!=0)continue;
   t=*tpl;t.width=e->w;t.psm=e->psm==16?GS_PSM_16:GS_PSM_32;
   t.address=g_res_rrf_addr[ordem[i]];
   setup_texture_dims(&t,(int)e->w,(int)e->h);
   FlushCache(0);
   if(e->psm==16)upload_raw_psm(&t,g_res_rrf_pixels,(int)e->w,(int)e->h,GS_PSM_16);
   else upload_raw(&t,g_res_rrf_pixels,(int)e->w,(int)e->h);
   g_res_rrf_frame[ordem[i]]=(unsigned int)fr;
  }
  t=*tpl;t.width=e->w;t.psm=e->psm==16?GS_PSM_16:GS_PSM_32;
  t.address=g_res_rrf_addr[ordem[i]];
  setup_texture_dims(&t,(int)e->w,(int)e->h);
  q=bind_texture_filter(q,&t,1);
  q=sprite_region(q,e->x,e->y,(float)e->w*e->escala,(float)e->h*e->escala,
                  0.0f,0.0f,(float)e->w,(float)e->h);
  desenhadas++;
 }
 if(desenhadas!=g_res_rrf_desenho){g_res_rrf_desenho=desenhadas;
  telemetry_event("resultado_rrf_desenho",desenhadas,g_res_rrf_rank);}
 return q;
}

static void resultado_soltar(void)
{
 int i;
 res_liberar(&g_res);
 g_res_ok=0;g_res_bg_ok=0;g_res_bg_addr=0;
 if(g_res_ss_tentou){pst_fechar(&g_res_ss_pack);g_res_ss_tentou=0;}
 if(g_res_ss_addr)sp_soltar(&g_pool,SP_CHAVE_TELA_BASE+0x26u);
 g_res_ss_ok=0;g_res_ss_addr=0;g_res_ss_frame=-1;g_res_ss_logado=0;
 g_res_rank_ok=0;g_res_rank_vert_addr=0;g_res_rank_hor_addr=0;g_res_rank_logado=0;
 {int k;for(k=0;k<(int)RES_RR_MAX_ENTRADAS;k++){
   if(g_res_rrf_vram[k])sp_soltar(&g_pool,SP_CHAVE_TELA_BASE+0x27u+(unsigned)k);
   g_res_rrf_vram[k]=0;g_res_rrf_addr[k]=0;g_res_rrf_frame[k]=0xffffffffu;}}
 res_rr_liberar(&g_res_rrf);
 if(g_res_rrf_pixels){free(g_res_rrf_pixels);g_res_rrf_pixels=0;g_res_rrf_pixels_bytes=0;}
 g_res_rrf_ok=0;g_res_rrf_rank=-1;g_res_rrf_desenho=-1;g_res_rrf_logado=0;g_res_rrf_ram=0;
 g_res_rrf_visto=0;
 g_res_score_ok=0;g_res_score_addr=0;g_res_score_logado=0;g_res_score_final=0;
 g_res_tally_ok=0;g_res_tally_addr=0;g_res_tally_desenho=-1;
 {int k;for(k=0;k<(int)RES_TALLIES;k++)g_res_tally_logado[k]=0;}
 g_res_cp_ok=0;g_res_cp_texto_addr=0;g_res_cp_texto_small_addr=0;
 g_res_cp_logado=0;g_res_cp_final=0;g_res_cp_small_logado=0;
 g_res_dif_ok=0;g_res_dif_addr=0;g_res_dif_ind=-1;g_res_dif_h=0.0f;
 g_res_barra_logado=0;
 g_res_flash_logado[0]=0;g_res_flash_logado[1]=0;
 g_res_nome_ok=0;g_res_nome_addr=0;g_res_nome_pronto=0;g_res_nome_visto=0;
 g_res_nome_ciclos=0;g_res_nome_moveu=0;
 {int k;for(k=0;k<(int)RES_CP_ATLASES;k++)g_res_cp_addr[k]=0;}
 for(i=0;i<(int)RES_FOLHAS;i++){g_res_addr[i]=0;g_res_frame[i]=0xffffffffu;}
 if(g_res_pixels){free(g_res_pixels);g_res_pixels=0;g_res_pixels_bytes=0;}
}
/* O PERCENTUAL que a tela de Resultado mostra. No Story Mode e' o da SEMANA
   (`talliesLevel`, o mesmo criterio dos contadores); na Freeplay, o da musica
   (`fnf_play_clear_percent`). Usado pelo contador GRANDE e pelo PEQUENO do
   topo -- os dois mostram o mesmo numero. */
/* DIAGNOSTICO DE HARNESS (`-DPS2H`): forca o rank da tela de resultado.
   Existe porque cada rank muda a ARTE (par vertical/horizontal do rankText), os
   ATRASOS (`getMusicDelay`/`getFlashDelay`/`getHighscoreDelay`) e o par
   musica+intro -- e sem isto so' o rank que o roteiro produz (GREAT) seria
   exercitado. Nunca ativo no console: `ps2h_rank_forcado` so' existe no
   binario do harness e devolve -1 em producao. */
static int resultado_rank(void)
{
 int r=(int)fnf_play_rank(&g_play);
#ifdef PS2H
 {int f=ps2h_rank_forcado();
  if(f>=0&&f<(int)RES_RANKS){
   if(!g_res_rank_forcado_logado){g_res_rank_forcado_logado=1;
    telemetry_event("resultado_rank_forcado",f,r);}
   return f;
  }}
#endif
 return r;
}
static int resultado_pct_alvo(void)
{
 if(g_story_mode){
  float c=g_camp_total_notas>0
          ?((float)(g_camp_sick+g_camp_good-g_camp_missed)/(float)g_camp_total_notas):0.0f;
  if(c<0.0f)c=0.0f;if(c>1.0f)c=1.0f;
  return (int)(c*100.0f);
 }
 return fnf_play_clear_percent(&g_play);
}
/* O GRUPO QUE ROLA. `ResultState.update()` (L806-826) soma o MESMO
   `speedOfTween * 60 * elapsed` em x e y do nome da musica, da imagem da
   dificuldade e do percentual pequeno, e quando `songName.x + songName.width <
   100` chama `timerThenSongName()` -- que poe os tres de volta em
   `y = -height` e repete os tweens de entrada (0,8 s / 0,85 s / 0,9 s com
   0,5 s de expoOut). Ou seja: a cada ciclo os tres sobem de novo.
   Roda uma vez por quadro, ANTES de qualquer desenho, porque a dificuldade e''
   desenhada antes do nome na ordem do original. */
static void resultado_scroll_passo(void)
{
 const res_nome_arte_t *nm;
 g_res_off_x=0.0f;g_res_off_y=0.0f;g_res_t_rel=(unsigned long)g_res_ms;
 if(!(g_res_nome_ok&&g_res_dif_ind>=0))return;
 nm=res_nome_do_id(g_play_song?g_play_song->id:0);
 if(!(nm&&nm->id))return;
 if(!g_res_nome_pronto){
  float sx=res_cp_small_x((float)g_res_dif[g_res_dif_ind].w)+
           g_res_nome_geo.x_do_pequeno;
  res_nome_iniciar(&g_res_nome_estado,0ul,sx,
                   nm->y_alvo+g_res_barra.correcao_y,
                   nm->larg_full,(float)nm->h);
  g_res_nome_pronto=1;
 }
 /* `showSmallClearPercent()`: 2,5 s depois, `movingSongStuff = true`. */
 {const res_rank_t *rr=res_rank(resultado_rank());
  if(rr&&(unsigned long)g_res_ms>=(unsigned long)rr->bf_ms+
                                   (unsigned long)g_res_nome_geo.movimento_atraso_ms)
   res_nome_soltar(&g_res_nome_estado,(unsigned long)g_res_ms);}
 if(g_res_nome_ciclos!=g_res_nome_estado.ciclo){
  g_res_nome_ciclos=g_res_nome_estado.ciclo;
  telemetry_event("resultado_nome_ciclo",g_res_nome_ciclos,g_res_ms);}
 res_nome_passo(&g_res_nome_estado,(unsigned long)g_res_ms);
 g_res_off_x=g_res_nome_estado.dx;
 g_res_off_y=g_res_nome_estado.dy;
 g_res_t_rel=(unsigned long)g_res_ms-(unsigned long)g_res_nome_estado.t0;
}
static qword_t *title_alpha(qword_t*q,texbuffer_t*t,const char*s,float cx,float y,float scale){static const unsigned char gw[36]={38,32,38,37,30,30,38,31,30,38,31,29,39,31,35,32,36,33,34,31,31,38,41,38,38,36,24,16,26,24,24,29,27,28,24,24};float ds=scale*.7142857f,total=0,x;int i,n,idx;for(i=0;s[i];i++){if(s[i]==' ')total+=20*ds;else{idx=(s[i]>='A'&&s[i]<='Z')?s[i]-'A':26+s[i]-'0';if(idx>=0&&idx<36)total+=(gw[idx]+3)*ds;}}x=cx-total*.5f;q=bind_texture(q,t);for(i=0;s[i];i++){if(s[i]==' '){x+=20*ds;continue;}idx=(s[i]>='A'&&s[i]<='Z')?s[i]-'A':26+s[i]-'0';if(idx<0||idx>=36)continue;n=gw[idx];q=sprite_region(q,x,y,n*ds,47*ds,(idx%9)*42,(idx/9)*48,n,47);x+=(n+3)*ds;}return q;}
/* audsrv_stop_audio() mutes the stream but does not flush its internal ring.
 * Re-applying the format resets that ring (including when the rate is
 * unchanged), so a new menu/Tutorial stream can start after a state change. */
static int audio_set_stream_rate(int volume,int rate){audsrv_fmt_t f;f.freq=rate;f.bits=16;f.channels=2;if(audsrv_set_format(&f)!=0)return -1;if(audsrv_set_volume(volume)!=0)return -2;g_audio_current_rate=rate;return 0;}
static int audio_set_stream(int volume){return audio_set_stream_rate(volume,AUDIO_RATE);}
/* FIX27: lossless audsrv submission. The PS2SDK EE wrapper advances by the
 * requested RPC copy even when the IOP accepted fewer bytes. Never call it
 * with more than the IOP currently reports available, and retry the exact
 * unaccepted tail instead of allowing a caller to discard it. */
static u32 g_audsrv_partial_events=0,g_audsrv_zero_events=0,g_audsrv_submit_errors=0;
static int audio_play_complete(const char *data,int bytes){int off=0,guard=2000;if(bytes<=0)return 0;while(off<bytes&&guard--){int avail=audsrv_available(),want,r;if(avail<=0){g_audsrv_zero_events++;DelayThread(1000);continue;}want=bytes-off;if(want>avail)want=avail;want&=~3;if(want<=0){DelayThread(1000);continue;}r=audsrv_play_audio(data+off,want);if(r<0){g_audsrv_submit_errors++;return off?off:r;}if(r==0){g_audsrv_zero_events++;DelayThread(1000);continue;}if(r<want)g_audsrv_partial_events++;off+=r;}if(off<bytes)g_audsrv_submit_errors++;return off;}

static int tutorial_audio_mix_submit(int voices_enabled){
 short *samples=(short*)g_audio_pcm;
 int i,n;
 if(audsrv_available()<AUDIO_READ_CHUNK)return 0;
 /* Antes: `if(audio_ring_level()<STEM_READ_CHUNK)return 0;` e depois exigir que
    audio_ring_take() entregasse o bloco inteiro. No fim do arquivo sobra um
    resto menor que o bloco, que nunca era consumido: o anel nunca esvaziava,
    audio_ring_eof() ja' era verdadeiro e a musica NUNCA terminava -- o fim de
    musica dependia de o arquivo ter tamanho multiplo do bloco lido. Medido:
    depois de retomar da pausa sobravam 1.280 bytes presos no anel. */
 if(audio_ring_level()<STEM_READ_CHUNK&&!audio_ring_eof())return 0;
 n=audio_ring_take(g_audio_pcm,STEM_READ_CHUNK);
 if(n<=0){
  if(!audio_ring_eof())return 0;
  n=audio_ring_take_available(g_audio_pcm,STEM_READ_CHUNK);
  n-=(n&7);   /* so' amostras inteiras: 8 bytes = 2 canais x 2 fontes */
  if(n<=0)return 0;
 }
 /* A soma das duas faixas (e a saturacao) saiu daqui para
    `engine/audio_mix.c`, que tem teste proprio: era nesta conta que o bloco
    de saida saia com o dobro do tamanho e o audio dobrava de velocidade. */
 i=amix_somar(samples,samples,n/8,voices_enabled);
 /* ATENCAO ao fator: o anel entrega 8 bytes por amostra (instrumental +
    vozes, 16 bits por canal), e o mixer escreve 4 bytes por amostra (estereo).
    Entao o bloco de SAIDA tem n/2 bytes. Antes o codigo lia STEM_READ_CHUNK e
    enviava AUDIO_READ_CHUNK -- que e' exatamente a metade -- e por isso parecia
    certo. Enviar `n` dobrava o audio entregue ao audsrv, e o relogio da musica
    (limitado por audio_submitted) passava a correr o dobro: medido, o relogio
    chegou a 4.755.264 amostras num arquivo de 3.225.600. */
 return audio_play_complete((const char*)g_audio_pcm,amix_bytes_saida(i));
}
static int audio_init(void){int result=-1;if(SifLoadModule("rom0:LIBSD",0,NULL)<0)return -1;if(SifExecModuleBuffer(audsrv_irx,size_audsrv_irx,0,NULL,&result)<0||result<0)return -2;if(audsrv_init()!=0)return -3;if(audio_set_stream(70)<0)return -4;return 0;}
static int menu_audio_prime(void){int total=0,guard=1000;while(audio_ring_level()<65536&&guard--){DelayThread(1000);}guard=32;while(audsrv_available()>=AUDIO_READ_CHUNK&&guard--){int j;if(audio_ring_take(g_audio_pcm,AUDIO_READ_CHUNK)!=AUDIO_READ_CHUNK){DelayThread(1000);continue;}j=audio_play_complete((char*)g_audio_pcm,AUDIO_READ_CHUNK);if(j<=0)break;total+=j;g_title_audio_submitted+=(u32)(j/4);}telemetry_event("menu_prime",total,audsrv_queued());return total;}

/* REGIAO DA BARRA DE VIDA DENTRO DO ATLAS DO HUD (tex2, 320x256).
   As coordenadas sao as do `sprite_region` do desenho do HUD (0,112,301,10) e
   as do arquivo `health_bar.rgba` que tools/empacotar_ui.py gera. */
#define HEALTH_BAR_X 0
#define HEALTH_BAR_Y 112
#define HEALTH_BAR_W 301
#define HEALTH_BAR_H 10
#define HUD_TEX_W 320

/* ===================== AUDIO DA TELA DE GAME OVER ======================== */
/* Empacota ate' 4 caracteres num int, para a telemetria (que leva dois ints)
   poder publicar qual sufixo foi usado sem inventar um formato novo de log. */
static int go_pack_suf(const char *s)
{
 int v=0,i;
 if(!s)return -1;
 for(i=0;i<4&&s[i];i++)v=(v<<8)|(unsigned char)s[i];
 return i?v:-1;
}
/* Sufixos do personagem que esta' morrendo. A tabela vem dos scripts
   (`src/gameover_dados.h`, gerado por tools/gen_gameover_sfx.py): personagem
   que nao declara nada no `onCreate` fica com "", que e' o padrao do
   GameOverSubState.reset(). */
static void go_audio_sufixos(void)
{
 int ci=g_cena_bf_ci,i;
 g_go_mus_suf="";g_go_bb_suf="";g_pause_suf="";
 if(ci<0||ci>=CH_CHAR_COUNT)return;
 for(i=0;i<GO_SFX_CHARS_COUNT;i++){
  if(strcmp(go_sfx_chars[i].id,ch_chars[ci].id)==0){
   g_go_mus_suf=go_sfx_chars[i].musica;
   g_go_bb_suf=go_sfx_chars[i].blueball;
   g_pause_suf=go_sfx_chars[i].pausa;
   break;
  }
 }
 telemetry_event("gameover_sufixo",go_pack_suf(g_go_mus_suf),go_pack_suf(g_go_bb_suf));
}
/* resolveMusicPath (GameOverSubState.hx L471-487).
   `Paths.music(base+suffix)`; enquanto nao existir e o sufixo nao estiver
   vazio, `suffix = suffix.split('-').slice(0,-1).join('-')` -- tira o ULTIMO
   segmento. E' assim que `-pico-explode` cai em `-pico` e depois em "".
   `prefixo` aqui ja' inclui o "End" quando e' o caso (o original monta
   basePath + 'End' antes de procurar).
   Devolve o fd aberto, ou -1 se nem o sufixo vazio existe. `usado` recebe o
   sufixo que de fato resolveu -- vai para o log. */
static int go_abrir_resolvendo(const char *prefixo,const char *sufixo,char *usado,int nusado)
{
 char caminho[96],suf[GO_SUF_MAX];
 int fd;
 if(!sufixo)sufixo="";
 snprintf(suf,GO_SUF_MAX,"%s",sufixo);
 for(;;){
  char *p;
  snprintf(caminho,sizeof(caminho),GO_ASSET_DIR "%s%s.pcm",prefixo,suf);
  fd=open(caminho,O_RDONLY);
  if(fd>=0){if(usado&&nusado>0)snprintf(usado,(size_t)nusado,"%s",suf);return fd;}
  if(suf[0]=='\0')break;
  p=strrchr(suf,'-');
  if(!p)break;
  *p='\0';
 }
 if(usado&&nusado>0)usado[0]='\0';
 return -1;
}
/* playBlueBalledSFX (L600-611): `Assets.exists(Paths.sound(base+suffix))` e, se
   nao existir, `FlxG.log.error`. O sufixo NAO e' reduzido aqui -- quem reduz e'
   o resolveMusicPath da musica. O `pico-blazin` pede `-pico-gutpunch`, que nao
   existe na arte: o original loga erro e nao toca nada, e o port faz o mesmo em
   vez de "consertar" e tocar o som generico, que seria divergencia. */
static int go_abrir_sfx(const char *prefixo,const char *sufixo)
{
 char caminho[96];
 int fd;
 snprintf(caminho,sizeof(caminho),GO_ASSET_DIR "%s%s.pcm",prefixo,sufixo?sufixo:"");
 fd=open(caminho,O_RDONLY);
 if(fd<0)telemetry_event("gameover_sfx_ausente",go_pack_suf(sufixo),0);
 return fd;
}
/* Primeiro quadro do game over: o original (ainda) nao tem musica aqui. Ele
   chama playBlueBalledSFX() no update do substate e so' comeca a musica quando
   o `firstDeath` termina. O anel entra em modo SILENCIO + overlay. */
static void gameover_audio_iniciar(void)
{
 int rc;
 g_go_submetido=0;g_go_mus_amostras=0;g_go_end_amostras=0;
 g_go_espera_ms=0.0f;g_go_saida=0;g_go_fade=0;g_go_audio_estado=0;
 g_go_audio_fd=-1;g_go_ov_fd=-1;g_go_mus_antigo=-1;
 go_audio_sufixos();
 if(!g_audio_ok)return;
 g_go_ov_fd=go_abrir_sfx("loss_sfx",g_go_bb_suf);
 rc=audio_ring_start_overlay(g_go_ov_fd);
 if(rc!=0){
  telemetry_event("gameover_audio_falhou",rc,g_go_ov_fd);
  if(g_go_ov_fd>=0){close(g_go_ov_fd);g_go_ov_fd=-1;}
  return;
 }
 g_go_audio_estado=1;
 telemetry_event("gameover_audio_inicio",g_go_ov_fd>=0,(int)g_go_mus_suf[0]);
}
/* Troca o overlay por outro sfx (o `fakeout_death`). O fd antigo e' fechado
   depois de a thread confirmar o pedido: ate' la' ela podia estar lendo dele. */
static void gameover_audio_troca_sfx(const char *prefixo,const char *sufixo)
{
 int fd;
 if(!g_audio_ok||g_go_audio_estado<=0)return;
 fd=go_abrir_sfx(prefixo,sufixo);
 if(audio_ring_trocar_overlay(fd)!=0){
  if(fd>=0)close(fd);
  telemetry_event("gameover_sfx_falhou",0,0);
  return;
 }
 /* O fd antigo NAO e' fechado aqui: quem le o overlay e' a thread do anel, e
    ela fecha o que deixou de usar quando aplica o pedido (audio_ring.c). */
 g_go_ov_fd=fd;
 telemetry_event("gameover_sfx",go_pack_suf(sufixo),fd>=0);
}
/* Fim do `firstDeath`: startDeathMusic(1.0, false) -- musica em LOOP por baixo
   do sfx que ainda esta' tocando (o anel troca so' a fonte primaria). */
static void gameover_audio_musica(void)
{
 char usado[GO_SUF_MAX];
 int fd,tam;
 if(!g_audio_ok||g_go_audio_estado!=1)return;
 fd=go_abrir_resolvendo("gameover",g_go_mus_suf,usado,(int)sizeof(usado));
 if(fd<0){telemetry_event("gameover_sem_musica",go_pack_suf(g_go_mus_suf),0);return;}
 tam=(int)lseek(fd,0,SEEK_END);lseek(fd,0,SEEK_SET);
 if(audio_ring_trocar_fonte(fd,1)!=0){close(fd);telemetry_event("gameover_musica_falhou",0,0);return;}
 g_go_audio_fd=fd;g_go_mus_amostras=tam/4;g_go_audio_estado=2;
 telemetry_event("gameover_musica",g_go_mus_amostras,go_pack_suf(usado));
}
/* confirmDeath (L330-372): startDeathMusic(1.0, true) com isEnding = true, ou
   seja `gameOverEnd` SEM loop, e:
       FADE_TIMER = (gameOverMusic?.length ?? 0) / 7000;
   O `length` e' do arquivo que acabou de tocar. Sem `gameOverEnd` o original
   loga warn e RETORNA sem parar a musica -- entao o FADE_TIMER continua sendo o
   comprimento da musica de game over que segue tocando. */
static void gameover_audio_fim(void)
{
 char usado[GO_SUF_MAX];
 int fd,tam;
 if(!g_audio_ok)return;
 fd=go_abrir_resolvendo("gameover_end",g_go_mus_suf,usado,(int)sizeof(usado));
 if(fd<0){
  tam=g_go_mus_amostras;
  telemetry_event("gameover_sem_end",go_pack_suf(g_go_mus_suf),tam);
 }else{
  tam=(int)lseek(fd,0,SEEK_END);lseek(fd,0,SEEK_SET);
  if(audio_ring_trocar_fonte(fd,0)==0){
   if(g_go_mus_antigo>=0)close(g_go_mus_antigo);
   g_go_mus_antigo=g_go_audio_fd;   /* fecha no fim da tela */
   g_go_audio_fd=fd;g_go_audio_estado=3;
   telemetry_event("gameover_end",tam/4,go_pack_suf(usado));
  }else{
   close(fd);tam=g_go_mus_amostras;
   telemetry_event("gameover_end_falhou",0,0);
  }
 }
 g_go_end_amostras=tam;
 /* FADE_TIMER = length/7000, com `length` em MILISSEGUNDOS:
       amostras = bytes/4 ;  ms = amostras/48 ;  FADE_TIMER(ms) = ms/7
    ou seja bytes/4/48/7 = bytes/1344 (= amostras/336). `tam` esta' em BYTES.
    (Antes desta conta o divisor era aplicado em bytes e o fade esperava 4x.)
    O ALVO vai para `g_go_espera_ms_alvo`; `g_go_espera_ms` e' o cronometro que
    o update soma e TEM de comecar em zero no CROSS. */
 g_go_espera_ms_alvo=(float)(tam/4)/336.0f;
 telemetry_event("gameover_fade_espera",(int)g_go_espera_ms_alvo,tam);
}
/* Para todo o audio do game over. O ring e' parado ANTES dos close: e' ele que
   esta' lendo esses fds (o audio_ring_stop espera a leitura pendente sair). */
static void gameover_audio_parar(void)
{
 if(!g_audio_ok)return;
 audsrv_stop_audio();
 audio_ring_stop();
 /* O overlay e' fechado pelo audio_ring_stop (a thread e' quem o le). */
 g_go_ov_fd=-1;
 if(g_go_audio_fd>=0){close(g_go_audio_fd);g_go_audio_fd=-1;}
 if(g_go_mus_antigo>=0){close(g_go_mus_antigo);g_go_mus_antigo=-1;}
 if(g_go_audio_estado>0)telemetry_event("gameover_audio_fim",(int)g_go_submetido,g_go_audio_estado);
 g_go_audio_estado=0;
}
/* Consumidor: manda ao audsrv o que o anel tiver. Sem pre-buffer de 64 KB como
   o menu: aqui o que importa e' o som comecar perto do evento (o sfx entra no
   primeiro quadro do substate e a musica no fim do firstDeath), e um anel cheio
   antes de enviar seria 1,3 s de atraso. */
static int gameover_audio_bombear(void)
{
 int voltas=0,enviado=0;
 while(audsrv_available()>=AUDIO_READ_CHUNK&&voltas++<64){
  int n=audio_ring_take(g_audio_pcm,AUDIO_READ_CHUNK);
  int j;
  if(n<=0)break;
  j=audio_play_complete((char*)g_audio_pcm,n);
  if(j<=0)break;
  g_go_submetido+=(u32)(j/4);
  enviado+=j;
 }
 return enviado;
}

/* ========================= MUSICA DA PAUSA =============================== */
/* PauseSubState.startPauseMusic (v0.8.6 L365-383):
     pauseMusic = Paths.music('breakfast$musicSuffix/breakfast$musicSuffix');
     if (pauseMusic == null) { trace('Could not play pause music'); return; }
     pauseMusic.play(false, FlxG.random.int(0, Std.int(pauseMusic.length / 2)));
     pauseMusic.fadeIn(MUSIC_FADE_IN_TIME, 0, MUSIC_FINAL_VOLUME);
   com MUSIC_FADE_IN_TIME = 5 (s) e MUSIC_FINAL_VOLUME = 0.75 (L127/L132), e
   musicSuffix vindo do personagem no slot do BF (coluna `pausa` de
   src/gameover_dados.h: os pico* pedem `-pico`; o bf-pixel pede `-pixel`, que
   nao existe na arte -- e' o caso do trace acima, sem som).
   O port nao tinha som nenhum na pausa (`audsrv_stop_audio()` + ring parado).
   Agora a pausa tem transporte proprio: o transporte do nivel fica congelado
   onde esta' e o anel e' reiniciado com o arquivo da pausa, em loop. */
#define PAUSE_ASSET_DIR "mass:/FNFPS2/assets/pause/"
#define PAUSE_FADE_S   5.0f   /* MUSIC_FADE_IN_TIME  */
#define PAUSE_VOLUME   0.75f  /* MUSIC_FINAL_VOLUME */
static int g_pause_fd=-1,g_pause_on=0,g_pause_abertos=0;
/* PauseSubState.musicSuffix: o original o' le' do personagem do slot `bf` que
   esta' no PlayState. A tabela e' a mesma do game over (tools/gen_gameover_sfx.py,
   coluna `pausa`); personagem fora da tabela nao tem som de pausa. */
static void pause_audio_sufixo(void)
{
 int ci=g_cena_bf_ci,i;
 g_pause_suf="";
 if(ci<0||ci>=CH_CHAR_COUNT)return;
 for(i=0;i<GO_SFX_CHARS_COUNT;i++)
  if(strcmp(go_sfx_chars[i].id,ch_chars[ci].id)==0){g_pause_suf=go_sfx_chars[i].pausa;break;}
}
static u32 g_pause_amostras=0;      /* amostras ja' tocadas: base do fade-in */
static int pause_audio_iniciar(void)
{
 char caminho[96];
 int fd,tam;
 g_pause_fd=-1;g_pause_on=0;g_pause_amostras=0;
 if(!g_audio_ok)return -1;
 snprintf(caminho,sizeof(caminho),PAUSE_ASSET_DIR "breakfast%s.pcm",
          g_pause_suf?g_pause_suf:"");
 fd=open(caminho,O_RDONLY);
 if(fd<0){
  /* `-pixel` nao existe na arte: o original loga e fica sem som. */
  telemetry_event("pause_sem_musica",go_pack_suf(g_pause_suf),0);
  return -1;
 }
 tam=(int)lseek(fd,0,SEEK_END);
 /* `FlxG.random.int(0, length/2)` -- o original sorteia mesmo. Aqui o sorteio e'
    um LCG semeado pelo numero de pausas ja' abertas (deterministico entre duas
    rodadas do harness com a mesma entrada) e o offset cai em multiplo de 4
    bytes, que e' uma amostra estereo. */
 {unsigned int r=(unsigned int)(++g_pause_abertos)*2654435761u+12345u;int meio=tam/2,off=0;
  r=r*1664525u+1013904223u;
  if(meio>=4){unsigned int n=(unsigned)(meio/4);off=(int)((r>>8)%(n+1u))*4;}
  lseek(fd,off,SEEK_SET);
  telemetry_event("pause_musica_offset",off,tam);}
 if(audio_ring_start_loop(fd,1)!=0){
  close(fd);
  telemetry_event("pause_musica_falhou",0,0);
  return -1;
 }
 g_pause_fd=fd;g_pause_on=1;
 /* campo 1 em BYTES (mesma unidade do `pause_musica_offset`, para o teste poder
    conferir sozinho que o trecho sorteado caiu na primeira metade). */
 telemetry_event("pause_musica",tam,go_pack_suf(g_pause_suf));
 return 0;
}
/* Para a musica da pausa. O ring e' parado ANTES do close: e' ele que le esse
   fd, e o audio_ring_stop espera a leitura pendente sair (mesma ordem do
   gameover_audio_parar -- fechar antes trava a thread de leitura). */
static void pause_audio_parar(void)
{
 if(!g_audio_ok)return;
 audsrv_stop_audio();
 audio_ring_stop();
 if(g_pause_fd>=0){close(g_pause_fd);g_pause_fd=-1;}
 if(g_pause_on)telemetry_event("pause_musica_fim",(int)g_pause_amostras,0);
 g_pause_on=0;
}
/* Consumidor da pausa com o fade-in do original (`fadeIn(5, 0, 0.75)`):
   ganho linear 0 -> 0,75 ao longo de 5 s de musica tocada. O ganho e' aplicado
   por AMOSTRA, nao por bloco -- trocar o ganho de um bloco para o seguinte
   produz degrau audivel. */
static int pause_audio_bombear(void)
{
 int voltas=0,enviado=0;
 while(audsrv_available()>=AUDIO_READ_CHUNK&&voltas++<64){
  int n=audio_ring_take(g_audio_pcm,AUDIO_READ_CHUNK),j,k,frames;
  if(n<=0)break;
  frames=n/4;
  {short*sp=(short*)g_audio_pcm;float base=(float)AUDIO_RATE*PAUSE_FADE_S;
   for(k=0;k<frames;k++){
    float t=(float)(g_pause_amostras+(u32)k);
    float g=(t<base)?(PAUSE_VOLUME*t/base):PAUSE_VOLUME;
    sp[k*2]=(short)((float)sp[k*2]*g);
    sp[k*2+1]=(short)((float)sp[k*2+1]*g);
   }
   g_pause_amostras+=(u32)frames;}
  j=audio_play_complete((char*)g_audio_pcm,n);
  if(j<=0)break;
  enviado+=j;
 }
 return enviado;
}
/* ======================= FIM DA MUSICA DA PAUSA =========================== */

/* =================== MUSICA E SONS DA TELA DE RESULTADO ==================== */
/* `ResultState.hx` L502-550 (musica), L598 e L609 (os dois `playOnce`).
 *
 * A musica sai do DONO do personagem jogavel, nao da musica: `bf` toca
 * `results*` e `pico` toca `results*-pico`, e o `spaghetti` (sserafim) nao tem
 * dono no registro -- cai no `?? 'bf'` do original. O `-intro`, quando existe,
 * toca UMA vez e depois passa para o principal em loop (o `onComplete` do
 * `FunkinSound.load` do intro, L510-523).
 *
 * DOIS `playOnce` na tela: `scrollMenu` a cada AUMENTO do percentual durante o
 * tween de 58/24 s (L592-604, so' quando o numero muda) e `confirmMenu` no
 * `onComplete` do tween (L609).
 *
 * O LIMITE CONHECIDO: o anel de audio tem UMA voz secundaria (o overlay). Cada
 * `playOnce` do original e' um `FlxSound` independente e os dois podem soar
 * juntos; aqui um tick novo RECOMECA o overlay em vez de somar com o anterior.
 * Os eventos sao os mesmos (cada um comeca no seu instante) -- o que se perde
 * e' a cauda de um tick quando outro chega em menos de 0,44 s, e o
 * `confirmMenu` cortando o ultimo tick. E' limite de vozes do anel, medido, e
 * esta' na lista de pendencias do console. */
#define RES_AUDIO_DIR "mass:/FNFPS2/assets/result/"
/* 0 = nenhum anel; 1 = intro tocando; 2 = principal em loop; 3 = silencio +
   overlay (o primeiro tick chegou antes da musica). */
static int g_resmus_estado=0;
static int g_resmus_fd=-1;        /* principal (loop)                   */
static int g_resmus_intro_fd=-1;  /* intro (toca uma vez)               */
static int g_resmus_tick_fd=-1;   /* scrollMenu                         */
static int g_resmus_conf_fd=-1;   /* confirmMenu                        */
static int g_resmus_ultimo=-1;    /* ultimo valor do percentual (tick)  */
static int g_resmus_confirmou=0;  /* o confirmMenu ja' tocou            */
static int g_resmus_musica_ms=-1; /* instante do `getMusicDelay` deste rank */
static unsigned int g_resmus_enviado=0;
static int g_resmus_ticks=0;      /* quantos ticks o percentual disparou */
static int g_resmus_pediu=0;      /* a musica ja' foi pedida (nao repetir) */

static int resmus_abrir(const char *nome)
{
 char caminho[96];
 if(!nome||!nome[0])return -1;
 snprintf(caminho,sizeof(caminho),RES_AUDIO_DIR "%s.pcm",nome);
 return open(caminho,O_RDONLY);
}

/* Para tudo e fecha os fds. O anel e' parado ANTES do close: e' ele que le
 * esses fds (mesma ordem de pause_audio_parar). */
static void resmus_parar(void)
{
 if(g_resmus_estado==0&&g_resmus_fd<0&&g_resmus_intro_fd<0&&
    g_resmus_tick_fd<0&&g_resmus_conf_fd<0)return;
 if(g_audio_ok){
  audsrv_stop_audio();
  audio_ring_stop();
 }
 if(g_resmus_fd>=0){close(g_resmus_fd);g_resmus_fd=-1;}
 if(g_resmus_intro_fd>=0){close(g_resmus_intro_fd);g_resmus_intro_fd=-1;}
 if(g_resmus_tick_fd>=0){close(g_resmus_tick_fd);g_resmus_tick_fd=-1;}
 if(g_resmus_conf_fd>=0){close(g_resmus_conf_fd);g_resmus_conf_fd=-1;}
 g_resmus_estado=0;g_resmus_ultimo=-1;g_resmus_confirmou=0;
 g_resmus_musica_ms=-1;g_resmus_enviado=0;g_resmus_ticks=0;g_resmus_pediu=0;
}

/* Um `playOnce`: reusa o fd (volta ao inicio) e pede a voz secundaria. Sem anel
 * ainda (rank com `getMusicDelay` grande), abre o anel em silencio+overlay --
 * e' o modo que o `audio_ring_start_overlay` ja' existe para o game over. */
static void resmus_sfx(int *cache,const char *nome)
{
 int rc;
 if(!g_audio_ok)return;
 if(*cache<0)*cache=resmus_abrir(nome);
 if(*cache<0){telemetry_event("resultado_sfx_ausente",(int)nome[0],0);return;}
 if(g_resmus_estado==0){
  rc=audio_ring_start_overlay(*cache);
  if(rc!=0){telemetry_event("resultado_sfx_falhou",rc,0);return;}
  g_resmus_estado=3;
  return;
 }
 rc=audio_ring_trocar_overlay(*cache);
 if(rc!=0)telemetry_event("resultado_sfx_falhou",rc,1);
}

/* `getMusicDelay()`: abre a musica do rank (intro + loop, ou so' loop). */
static void resmus_iniciar(int rank)
{
 const res_musica_t *m;
 int rc;
 if(!g_audio_ok)return;
 m=res_musica(res_musica_dono(g_play_song?g_play_song->player:0),rank);
 if(!m||!m->arquivo)return;
 if(m->intro&&m->intro[0]){
  g_resmus_intro_fd=resmus_abrir(m->intro);
  g_resmus_fd=resmus_abrir(m->arquivo);
  if(g_resmus_intro_fd>=0&&g_resmus_fd>=0){
   if(g_resmus_estado==3)rc=audio_ring_trocar_fonte(g_resmus_intro_fd,0);
   else rc=audio_ring_start_loop(g_resmus_intro_fd,0);
   if(rc==0){
    g_resmus_estado=1;
    telemetry_event("resultado_musica",1,g_resmus_intro_fd);
    return;
   }
   telemetry_event("resultado_musica_falhou",rc,1);
  }
  if(g_resmus_intro_fd>=0){close(g_resmus_intro_fd);g_resmus_intro_fd=-1;}
 }else{
  g_resmus_fd=resmus_abrir(m->arquivo);
  if(g_resmus_fd>=0){
   if(g_resmus_estado==3)rc=audio_ring_trocar_fonte(g_resmus_fd,1);
   else rc=audio_ring_start_loop(g_resmus_fd,1);
   if(rc==0){
    g_resmus_estado=2;
    telemetry_event("resultado_musica",2,g_resmus_fd);
    return;
   }
   telemetry_event("resultado_musica_falhou",rc,2);
  }
 }
 telemetry_event("resultado_sem_musica",0,(int)(m->arquivo[0]));
}

/* Bombeia o anel para o audsrv (o mesmo laco do game over e da pausa). */
static int resmus_bombear(void)
{
 int voltas=0,enviado=0;
 if(!g_audio_ok)return 0;
 while(audsrv_available()>=AUDIO_READ_CHUNK&&voltas++<64){
  int n=audio_ring_take(g_audio_pcm,AUDIO_READ_CHUNK),j;
  if(n<=0)break;
  j=audio_play_complete((char*)g_audio_pcm,n);
  if(j<=0)break;
  g_resmus_enviado+=(unsigned int)(j/4);
  enviado+=j;
 }
 return enviado;
}

/* Um passo por quadro na tela de resultado: o atraso do rank, a troca
 * intro->loop no fim do intro, os ticks do percentual e o confirmMenu. */
static void resmus_passo(int rank,int pct_alvo)
{
 int valor;
 if(!g_audio_ok)return;
 if(g_resmus_musica_ms<0){
  g_resmus_musica_ms=(int)res_musica_atraso_ms(rank);
  telemetry_event("resultado_musica_atraso",g_resmus_musica_ms,rank);
 }
 /* A musica entra no `getMusicDelay` do rank. O anel pode ja' estar rodando em
    SILENCIO+OVERLAY (estado 3), porque os ticks do percentual comecam em 37/24 s
    e o PERFECT so' pede musica em 95/24 s -- nesse caso `resmus_iniciar` usa
    `audio_ring_trocar_fonte`, como o game over faz com a musica que entra por
    baixo do sfx. */
 if(!g_resmus_pediu&&(g_resmus_estado==0||g_resmus_estado==3)&&
    g_res_ms>=(unsigned long)g_resmus_musica_ms){
  g_resmus_pediu=1;
  resmus_iniciar(rank);
 }
 /* `onComplete` do intro: `audio_ring_start_loop(fd,0)` para no fim e
    `audio_ring_eof()` e' o aviso; o principal entra em loop no lugar. */
 if(g_resmus_estado==1&&audio_ring_eof()){
  if(g_resmus_fd>=0&&audio_ring_trocar_fonte(g_resmus_fd,1)==0){
   g_resmus_estado=2;
   telemetry_event("resultado_musica",3,g_resmus_fd);
  }
 }
 /* Os ticks: so' quando o numero AUMENTA (L592-604). */
 if(res_cp_visivel((unsigned long)g_res_ms)){
  valor=res_cp_valor(pct_alvo,(unsigned long)g_res_ms);
  if(g_resmus_ultimo<0)g_resmus_ultimo=valor;
  else if(valor>g_resmus_ultimo){
   resmus_sfx(&g_resmus_tick_fd,"scroll_menu");
   g_resmus_ticks++;
   g_resmus_ultimo=valor;
  }
 }
 if(!g_resmus_confirmou&&
    (unsigned long)g_res_ms>=res_cp_confirm_ms()){
  g_resmus_confirmou=1;
  resmus_sfx(&g_resmus_conf_fd,"confirm_menu");
  telemetry_event("resultado_confirm",(int)g_res_ms,0);
  /* A prova de que os dois `playOnce` sairam e de que o anel ENTREGOU audio:
     `resultado_ticks` conta os `scrollMenu` (um por aumento do percentual, como
     o `if (clearPercentLerp != curNumber)` do L592-604) e `resultado_audio`
     traz as amostras que foram para o audsrv. */
  telemetry_event("resultado_ticks",g_resmus_ticks,0);
  telemetry_event("resultado_audio",(int)g_resmus_enviado,g_resmus_estado);
 }
 resmus_bombear();
}


static qword_t *text_draw_alpha(qword_t*q,const char*s,float x,float y,float scale,int align,unsigned char alpha){int len=(int)strlen(s),i;float total=0,cursor;
 for(i=0;i<len;i++){unsigned char ch=(unsigned char)s[i];if(ch<32||ch>127)ch='?';total+=g_font_widths[ch-32]*0.5f*scale;}
 if(align==1)x-=total*0.5f;else if(align==2)x-=total;cursor=x;
 for(i=0;i<len;i++){g_frame_texture_quads++;unsigned char ch=(unsigned char)s[i];texrect_t v;int n,advance;if(ch<32||ch>127)ch='?';n=ch-32;advance=g_font_widths[n];v.v0.x=FNX_INI(cursor);v.v0.y=FNX_INI(vy(y));v.v0.z=0;v.v1.x=FNX_FIM(cursor+FONT_DRAW_W*scale);v.v1.y=FNX_FIM(vy(y)+FONT_DRAW_H*scale);v.v1.z=0;v.t0.u=(n%16)*FONT_CELL_W+0.5f;v.t0.v=(n/16)*FONT_CELL_H+0.5f;v.t1.u=(n%16+1)*FONT_CELL_W-0.5f;v.t1.v=(n/16+1)*FONT_CELL_H-0.5f;v.color=rgba(0x80,0x80,0x80,alpha);q=draw_rect_textured(q,0,&v);cursor+=advance*0.5f*scale;}return q;}
static qword_t *text_draw(qword_t*q,const char*s,float x,float y,float scale,int align){return text_draw_alpha(q,s,x,y,scale,align,128);}
/* FlxEase.cubeInOut, que e' a curva do tween de alpha da contagem
   (Countdown.showCountdownGraphic: var fadeEase = FlxEase.cubeInOut). */
static float ease_cube_in_out(float t){if(t<=0.0f)return 0.0f;if(t>=1.0f)return 1.0f;if(t<0.5f)return 4.0f*t*t*t;{float u=-2.0f*t+2.0f;return 1.0f-u*u*u*0.5f;}}
/* Grafico da contagem regressiva (Countdown.showCountdownGraphic).
   O original pega o sprite do noteStyle, centraliza na camHUD e faz um tween
   de alpha 1 -> 0 com FlxEase.cubeInOut durante UMA batida. Aqui e' o texto com
   a mesma posicao (centro do quadro), a mesma curva e a mesma duracao; a arte do
   noteStyle entra quando o atlas de countdown estiver no pacote. */
static qword_t *countdown_draw(qword_t*q,texbuffer_t*font){
 const char*lb=0;float prog,alpha;s32 st;
 if(g_countdown_left<=0||g_countdown_beat<=0)return q;
 st=(s32)(5-(g_countdown_left+(g_countdown_beat-1))/g_countdown_beat);
 if(st==1)lb="3";else if(st==2)lb="2";else if(st==3)lb="1";else if(st==4)lb="GO!";else return q;
 prog=(float)((5-st)*g_countdown_beat-g_countdown_left)/(float)g_countdown_beat;
 alpha=1.0f-ease_cube_in_out(prog);
 if(alpha<0.0f)alpha=0.0f;
 if(alpha>1.0f)alpha=1.0f;
 q=bind_texture_filter(q,font,1);
 /* Countdown.hx L239-240: o sprite da contagem vive na camHUD, entao ele
    acompanha o zoom do HUD (posicao E tamanho). */
 return text_draw_alpha(q,lb,hz_x(320.0f),hz_y(132.0f),2.4f*g_hud_zoom,1,(unsigned char)(alpha*128.0f));
}
static int rle_decode(const unsigned char*src,u32 src_bytes,unsigned char*dst,u32 dst_bytes){u32 si=0,di=0;while(si<src_bytes&&di<dst_bytes){unsigned int c=src[si++];if(c<128u){u32 n=(c+1u)*4u;if(si+n>src_bytes||di+n>dst_bytes)return-1;memcpy(dst+di,src+si,n);si+=n;di+=n;}else if(c>128u){u32 count=257u-c;if(si+4u>src_bytes||di+count*4u>dst_bytes)return-2;while(count--){memcpy(dst+di,src+si,4);di+=4;}si+=4;}else return-3;}if(si!=src_bytes||di!=dst_bytes)return-4;FlushCache(0);return 0;}
static u32 g_menu_decode_si=0,g_menu_decode_di=0;static int g_menu_decode_active=0,g_menu_decode_error=0;
static u32 g_menu_alpha_clamped=0,g_menu_rgb_dilated=0,g_menu_atlas_fixed=0;
/* 0.2.105 C7: copia mutavel da tabela de quadros. A decodagem entrega os
   quadros com a altura ORIGINAL do atlas (62 linhas etc.); a reamostragem
   por area (C7) compacta cada quadro para a altura FISICA hp=round(h*14/15)
   e reescreve offset/altura aqui. Todo uso do atlas le esta copia. */
static unsigned int g_menu_frame_rw[5][2][3][3];static int g_menu_frame_rw_ready=0;static u32 g_menu_phys_removed=0,g_menu_phys_rows=0;
/* 0.2.105 C4: replica exata de FlxFlicker+FlxTimer (flixel/effects/FlxFlicker.hx):
   loops=Std.int(duration/interval); a cada intervalo visivel=!visivel; no fim
   visivel=endVisibility e roda o completionCallback. Item: (1,0.06,true) ->
   16 toggles; magenta: (1.1,0.15,false) -> 7 toggles. FlxTimer: timeLeft-=
   elapsed(1/60); se <=0 dispara e timeLeft+=interval. */
static double g_fl_item_tl=0,g_fl_mag_tl=0;static int g_fl_item_left=0,g_fl_mag_left=0,g_fl_item_vis=1,g_fl_mag_vis=0,g_fl_item_on=0,g_fl_mag_on=0;static int g_fl_item_toggles=0,g_fl_mag_toggles=0;static int menu_exit_frames=0;static int g_accept_press_frame=0,g_accept_item_end=-1,g_accept_switch=-1;
/* 0.2.104 C2: dilatacao do RGB dos texeis transparentes do atlas do menu.
   Medicao (tools/forense_J_rgb_transparente.py): 100% dos 90.580 texeis com
   alfa 0 tem RGB exatamente (0,0,0). Com filtragem LINEAR o GS interpola RGB
   junto com o alfa, entao toda borda de arte seria puxada para o preto
   (franja escura). Copiar o RGB do vizinho opaco mais proximo para dentro dos
   transparentes e o equivalente direto do canvas ARGB premultiplicado que o
   Lime/OpenFL usa no original antes de texturizar.
   Feito POR QUADRO (30 quadros, tabela g_menu_frame de menu_generated.h) para
   que a vizinhanca nunca atravesse a fronteira entre dois quadros, e em uma
   unica passada: o alfa nunca e reescrito, entao o teste "vizinho opaco" e
   estavel. Uma passada cobre o anel de 1 texel, que e exatamente o alcance do
   filtro bilinear. Custo medido no PC: 609.390 pixels. */
static void menu_atlas_dilate_rgb(void){int mi,se,af,r,c;
 if(g_menu_atlas_fixed){return;}
 g_menu_atlas_fixed=1;
 for(mi=0;mi<5;mi++)for(se=0;se<2;se++)for(af=0;af<3;af++){
  const unsigned int*fr=g_menu_frame_rw[mi][se][af];
  u32 off=fr[0],w=fr[1],h=fr[2];
  for(r=0;r<(int)h;r++)for(c=0;c<(int)w;c++){
   unsigned char*p=g_menu_pixels+off+((u32)r*w+(u32)c)*4u;const unsigned char*s=0;
   if(p[3])continue;
   if(r+1<(int)h){const unsigned char*t=p+w*4u;if(t[3])s=t;}
   if(!s&&r>0){const unsigned char*t=p-w*4u;if(t[3])s=t;}
   if(!s&&c+1<(int)w){const unsigned char*t=p+4u;if(t[3])s=t;}
   if(!s&&c>0){const unsigned char*t=p-4u;if(t[3])s=t;}
   if(s){p[0]=s[0];p[1]=s[1];p[2]=s[2];g_menu_rgb_dilated++;}
  }
 }
 FlushCache(0);
 telemetry_event("atlas_dilate",(int)g_menu_rgb_dilated,(int)g_menu_alpha_clamped);
}
/* 0.2.105 C7: reamostragem POR AREA dos 30 quadros do atlas para a altura
   fisica hp=round(h*14/15). Motivo medido (tools/prova_fix_02105.py): a saida
   640x448 em TV 4:3 comprime 14/15 na vertical; qualquer amostragem do GS
   (NEAREST ou LINEAR) de 62 texels em 58 linhas descarta/mistura linhas com
   fase dependente do dy, e o pe antialiased da arte vira traco tracejado.
   Pre-reamostrar por area e desenhar 1:1 torna a saida IDENTICA a referencia
   por area (RMSE 0 por construcao), que e o proxy mensuravel do 1:1 do
   original. Media PREMULTIPLICADA (alfa conv. 128=opaco): P=Sum w*a*rgb,
   A=Sum w*a; out_a=A/SumW; out_rgb=P/A; sem alfa fica o RGB dilatado (C2).
   Compactacao in-place: offsets originais crescem e so encolhemos, entao o
   ponteiro de escrita nunca alcanca a fonte. O fundo (MENU_FRAMES_BYTES em
   diante) nao se move: comeca depois dos 30 quadros originais. */
static void menu_atlas_resample_phys(void){int mi,se,af;u32 wp=0;
 if(!g_menu_frame_rw_ready){memcpy(g_menu_frame_rw,g_menu_frame,sizeof(g_menu_frame_rw));g_menu_frame_rw_ready=1;}
 g_menu_phys_removed=0;g_menu_phys_rows=0;
 for(mi=0;mi<5;mi++)for(se=0;se<2;se++)for(af=0;af<3;af++){
  unsigned int*fr=g_menu_frame_rw[mi][se][af];
  u32 src=fr[0],w=fr[1],h=fr[2],hp=(u32)((float)h*14.0f/15.0f+0.5f),j,c,i;
  for(j=0;j<hp;j++){
   float a0=(float)j*(float)h/(float)hp,a1=(float)(j+1)*(float)h/(float)hp;
   u32 i0=(u32)a0,i1=(u32)(a1+0.999999f);if(i1>h)i1=h;if(i1<=i0)i1=i0+1;
   for(c=0;c<w;c++){
    float wsum=0.0f,pa=0.0f,pr=0.0f,pg=0.0f,pb=0.0f;
    for(i=i0;i<i1;i++){float lo=(i>a0?(float)i:a0),hi=((i+1)<a1?(float)(i+1):a1),wgt=hi-lo;const unsigned char*sp=g_menu_pixels+src+((u32)i*w+c)*4u;float a=sp[3]/128.0f;wsum+=wgt;pa+=wgt*sp[3];pr+=wgt*a*sp[0];pg+=wgt*a*sp[1];pb+=wgt*a*sp[2];}
    unsigned char*dp=g_menu_pixels+wp+((u32)j*w+c)*4u;
    if(pa>0.0001f){float ia=128.0f/pa;dp[0]=(unsigned char)(pr*ia+0.5f);dp[1]=(unsigned char)(pg*ia+0.5f);dp[2]=(unsigned char)(pb*ia+0.5f);}else{const unsigned char*sp=g_menu_pixels+src+((u32)i0*w+c)*4u;dp[0]=sp[0];dp[1]=sp[1];dp[2]=sp[2];}
    {int av=(int)(pa/wsum+0.5f);if(av>128)av=128;if(av<0)av=0;dp[3]=(unsigned char)av;}
   }
  }
  fr[0]=wp;fr[2]=hp;wp+=w*hp*4u;g_menu_phys_removed+=h-hp;g_menu_phys_rows+=hp;
 }
 FlushCache(0);
 telemetry_event("atlas_phys",(int)g_menu_phys_removed,(int)g_menu_phys_rows);
}
/* 0.2.105 C4: um tick de FlxTimer por quadro, logica identica a do flixel. */
static void menu_flicker_step(u32 fn){
 if(g_fl_item_on){g_fl_item_tl-=1.0/60.0;if(g_fl_item_tl<=0.0){g_fl_item_vis=!g_fl_item_vis;g_fl_item_toggles++;g_fl_item_left--;if(g_fl_item_left>0){g_fl_item_tl+=0.06;}else{g_fl_item_vis=1;g_fl_item_on=0;menu_exit_frames=24;g_accept_item_end=(int)fn-g_accept_press_frame;}}}
 if(g_fl_mag_on){g_fl_mag_tl-=1.0/60.0;if(g_fl_mag_tl<=0.0){g_fl_mag_vis=!g_fl_mag_vis;g_fl_mag_toggles++;g_fl_mag_left--;if(g_fl_mag_left>0){g_fl_mag_tl+=0.15;}else{g_fl_mag_vis=0;g_fl_mag_on=0;}}}
}

static void menu_decode_begin(void){g_menu_decode_si=g_menu_decode_di=0;g_menu_decode_error=0;g_menu_decode_active=1;g_menu_cache_ready=0;g_menu_atlas_fixed=0;g_menu_alpha_clamped=0;g_menu_rgb_dilated=0;telemetry_event("rle_at_begin",(int)(((u32)g_menu_compressed[0]<<24)|((u32)g_menu_compressed[1]<<16)|((u32)g_menu_compressed[2]<<8)|(u32)g_menu_compressed[3]),(int)(((u32)g_menu_compressed[MENU_RLE_BYTES]<<24)|((u32)g_menu_compressed[MENU_RLE_BYTES+1]<<16)|((u32)g_menu_compressed[MENU_RLE_BYTES+2]<<8)|(u32)g_menu_compressed[MENU_RLE_BYTES+3]));telemetry_event("menu_dec_begin",MENU_RLE_BYTES,MENU_CACHE_BYTES);}
static void menu_decode_step(u32 budget){u32 begin=g_menu_decode_di;if(!g_menu_decode_active)return;while(g_menu_decode_si<MENU_RLE_BYTES&&g_menu_decode_di<MENU_CACHE_BYTES&&g_menu_decode_di-begin<budget){unsigned int c=g_menu_compressed[g_menu_decode_si++];if(c<128u){u32 n=(c+1u)*4u;if(g_menu_decode_si+n>MENU_RLE_BYTES||g_menu_decode_di+n>MENU_CACHE_BYTES){g_menu_decode_error=-1;break;}memcpy(g_menu_pixels+g_menu_decode_di,g_menu_compressed+g_menu_decode_si,n);{u32 bi;for(bi=0;bi<n;bi+=4u){unsigned char*px=g_menu_pixels+g_menu_decode_di+bi;if(px[3]>0x80u){px[3]=0x80u;g_menu_alpha_clamped++;}}}g_menu_decode_si+=n;g_menu_decode_di+=n;}else if(c>128u){u32 count=257u-c;if(g_menu_decode_si+4u>MENU_RLE_BYTES||g_menu_decode_di+count*4u>MENU_CACHE_BYTES){g_menu_decode_error=-2;break;}while(count--){memcpy(g_menu_pixels+g_menu_decode_di,g_menu_compressed+g_menu_decode_si,4);if(g_menu_pixels[g_menu_decode_di+3u]>0x80u){g_menu_pixels[g_menu_decode_di+3u]=0x80u;g_menu_alpha_clamped++;}g_menu_decode_di+=4;}g_menu_decode_si+=4;}else{g_menu_decode_error=-3;break;}}if(g_menu_decode_error||g_menu_decode_si==MENU_RLE_BYTES||g_menu_decode_di==MENU_CACHE_BYTES){g_menu_decode_active=0;g_menu_cache_ready=(!g_menu_decode_error&&g_menu_decode_si==MENU_RLE_BYTES&&g_menu_decode_di==MENU_CACHE_BYTES);FlushCache(0);if(g_menu_cache_ready){menu_atlas_dilate_rgb();menu_atlas_resample_phys();}telemetry_event("menu_dec_pos",(int)g_menu_decode_si,(int)g_menu_decode_di);telemetry_event("menu_dec_done",g_menu_decode_error,g_menu_cache_ready);}}
static int menu_font_restore(void){return rle_decode(g_menu_compressed+MENU_RLE_BYTES,FONT_RLE_BYTES,g_font_pixels,FONT_W*FONT_H*4);}
static int story_ui_restore(void){return rle_decode(g_menu_compressed+MENU_RLE_BYTES+FONT_RLE_BYTES,STORY_RLE_BYTES,g_stage_pixels,512*256*4);}
static int story_titles_restore(void){return rle_decode(g_menu_compressed+MENU_RLE_BYTES+FONT_RLE_BYTES+STORY_RLE_BYTES,STORY_TITLES_RLE_BYTES,g_stage_pixels,STORY_TITLES_RAW_BYTES);}
/* M6.8 official restart: source-locked mainStage 1.0.0 metadata.
 * Source: official assets/data/stages/mainStage.json. */
typedef struct { float x,y,sx,sy; int z; } official_stage_node_t;
static const official_stage_node_t g_official_mainstage_props[3]={
 {-600.0f,-200.0f,0.9f,0.9f,10},
 {-650.0f, 600.0f,0.9f,0.9f,20},
 {-500.0f,-300.0f,1.3f,1.3f,30}
};
static const official_stage_node_t g_official_mainstage_chars[3]={
 {751.5f,787.0f,0.0f,0.0f,100},
 {335.0f,885.0f,150.0f,-100.0f,200},
 {989.5f,885.0f,-100.0f,-100.0f,300}
};
static const float g_official_mainstage_camera_zoom=1.1f;

/* ZOOM INICIAL DA CAMERA = o `cameraZoom` do PALCO daquela musica.
   Cada palco declara o seu em assets-data/stages/*.json (bs_stages[].zoom):
   mainStage 1,10; limoRide 0,90; mallXmas 0,80; tankmanBattlefieldErect 0,70;
   sserafim 0,80. O port usava 1,10 (o do mainStage) em TODAS as musicas, e esse
   numero nao e' um detalhe: `sc = 0,5 * zoom * bop` e' o tamanho de TUDO que
   esta' em cena (sprites, props e a propria camera) -- nas outras 20 musicas o
   mundo saia com a escala errada. PlayState.hx usa stageData.cameraZoom como
   defaultCamZoom (o valor que resetCamera volta a por). */
static float cena_zoom_do_palco(const sr_song_t *song)
{
    int i;
    const char *st = song && song->stage ? song->stage : "mainStage";
    for (i = 0; i < BS_STAGE_COUNT; i++)
        if (bs_stages[i].id && strcmp(bs_stages[i].id, st) == 0)
            return bs_stages[i].zoom;
    return g_official_mainstage_camera_zoom;
}

static const char*g_story_names[9]={"TEACHING TIME","DADDY DEAREST","SPOOKY MONTH","PICO","MOMMY MUST MURDER","RED SNOW","HATING SIMULATOR FT. MOAWLING","TANKMAN FT. JOHNNYUTAH","DUE DEBTS"};
static const char*g_story_tracks[9]={"Tutorial","Bopeebo\nFresh\nDadbattle","Spookeez\nSouth\nMonster","Pico\nPhilly Nice\nBlammed","Satin Panties\nHigh\nM.I.L.F","Cocoa\nEggnog\nWinter Horrorland","Senpai\nRoses\nThorns","Ugh\nGuns\nStress","Darnell\nLit Up\n2hot\nBlazin'"};
static int g_story_static_ready=0;
/* LevelTitle.update: y = MathUtil.smoothLerpPrecision(y,targetY,elapsed,0.451).
   MathUtil: lerp(target,base,pow(precision,dt/duration)), precision padrao 1/100.
   Logo o fator por quadro e alpha = 1 - 0.01^(dt/0.451). Com dt = 1001/60000 s
   (NTSC) isso da 0.156633667. O valor .451f usado antes era o parametro de
   DURACAO aplicado como se fosse alpha: 2.879x rapido demais, convergindo em
   8 quadros em vez de 28. */
#define STORY_TITLE_LERP_ALPHA 0.156633667f
#define STORY_GF_W 169
#define STORY_GF_H 155
#define STORY_BF_W 163
#define STORY_BF_H 157
static int story_font_alpha_decode(const unsigned char*src,u32 src_bytes,unsigned char*dst,u32 pixels){u32 si=0,di=0;while(si<src_bytes&&di<pixels){unsigned int c=src[si++],n;if(c<128u){n=c+1u;if(si+n>src_bytes||di+n>pixels)return-1;while(n--){dst[di*4u]=255;dst[di*4u+1]=255;dst[di*4u+2]=255;dst[di*4u+3]=src[si++];di++;}}else if(c>128u){unsigned char a;if(si>=src_bytes)return-2;n=257u-c;a=src[si++];if(di+n>pixels)return-3;while(n--){dst[di*4u]=255;dst[di*4u+1]=255;dst[di*4u+2]=255;dst[di*4u+3]=a;di++;}}else return-4;}FlushCache(0);return(si==src_bytes&&di==pixels)?0:-5;}
static int tutorial_vcr_restore(void){u32 fp;int r=story_font_alpha_decode(g_menu_sfx_pixels+241920u+22656u,7672u,g_font_pixels,160u*120u);if(r<0)return r;for(fp=0;fp<160u*120u;fp++){unsigned int a=g_font_pixels[fp*4u+3];g_font_pixels[fp*4u+3]=(unsigned char)((a*128u+127u)/255u);}FlushCache(0);return 0;}
static int g_menu_remembered=0;  /* FIDELIDADE 0.2.96: rememberedSelectedIndex do original */
static int g_story_vcr_vbase=0;
static qword_t*story_vcr_line(qword_t*q,const char*s,int count,float x,float y,int align,unsigned char r,unsigned char g,unsigned char b,unsigned char a){int i;float adv=9.375f,total=adv*count,cursor;if(align==1)x-=total*.5f;else if(align==2)x-=total;cursor=x;for(i=0;i<count;i++){g_frame_texture_quads++;unsigned char ch=(unsigned char)s[i];texrect_t v;int n;if(ch<32||ch>127)ch='?';n=ch-32;v.v0.x=FNX_INI(cursor);v.v0.y=FNX_INI(vy(y-3));v.v0.z=0;v.v1.x=FNX_FIM(cursor+10);v.v1.y=FNX_FIM(vy(y-3)+20);v.v1.z=0;v.t0.u=(n%16)*10+.5f;v.t0.v=g_story_vcr_vbase+(n/16)*20+.5f;v.t1.u=(n%16+1)*10-.5f;v.t1.v=g_story_vcr_vbase+(n/16+1)*20-.5f;v.color=rgba(r,g,b,a);q=draw_rect_textured(q,0,&v);cursor+=adv;}return q;}
/* PlayState.updateScoreText() + FlxStringUtil.formatMoney(v,false,true):
   "Score: N" com virgula a cada tres digitos. Devolve o comprimento, porque
   hud_score_line() recebe contagem explicita (ela nao mede a string). */
static int fmt_score_line(char*out,int cap,int score){
 int n=0,digits[10],nd=0,neg=0,base=0,val=score;
 const char*pre="Score: ";
 if(!out||cap<10)return 0;
 while(*pre&&n<cap-1)out[n++]=*pre++;
 if(val<0){neg=1;val=-val;}
 do{digits[nd++]=val%10;val/=10;}while(val&&nd<10);
 if(neg&&n<cap-1)out[n++]='-';
 while(nd>0){
  nd--;
  if(n>=cap-1)break;
  out[n++]=(char)('0'+digits[nd]);
  /* A virgula entra depois do digito quando o que sobra a' direita e'
     multiplo de tres -- o mesmo resultado do formatMoney. */
  base=nd;
  if(base>0&&(base%3)==0&&n<cap-1)out[n++]=',';
 }
 out[n]=0;
 return n;
}
static qword_t*hud_score_line(qword_t*q,const char*s,int count,float x,float y,int align,unsigned char r,unsigned char g,unsigned char b,unsigned char a){int i;/* O TEXTO DO HUD TAMBEM PISCA: o scoreText (e as duas marcas de versao) estao
   na camHUD no original (PlayState.hx L2026, L959-960), entao o zoom do HUD os
   escala como qualquer outra peca. Aqui isso vira: posicao pelo hz_*, avanco e
   altura do glifo x zoom. Com zoom 1,0 os numeros sao identicos aos de antes. */
 float adv=4.6875f*g_hud_zoom,total=adv*count,cursor,x0=hz_x(x),yb=hz_y(y-1.5f);if(align==1)x0-=total*.5f;else if(align==2)x0-=total;cursor=x0;for(i=0;i<count;i++){g_frame_texture_quads++;unsigned char ch=(unsigned char)s[i];texrect_t v;int n;if(ch<32||ch>127)ch='?';n=ch-32;v.v0.x=FNX_INI(cursor);v.v0.y=FNX_INI(vy(yb));v.v0.z=0;v.v1.x=FNX_FIM(cursor+5.0f*g_hud_zoom);v.v1.y=FNX_FIM(vy(yb)+10.0f*g_hud_zoom);v.v1.z=0;v.t0.u=(n%16)*10+.5f;v.t0.v=g_story_vcr_vbase+(n/16)*20+.5f;v.t1.u=(n%16+1)*10-.5f;v.t1.v=g_story_vcr_vbase+(n/16+1)*20-.5f;v.color=rgba(r,g,b,a);q=draw_rect_textured(q,0,&v);cursor+=adv;}return q;}
static qword_t*story_vcr(qword_t*q,const char*s,float x,float y,int align,unsigned char r,unsigned char g,unsigned char b,unsigned char a){const char*p=s,*e;float yy=y;while(1){e=strchr(p,'\n');q=story_vcr_line(q,p,e?(int)(e-p):(int)strlen(p),x,yy,align,r,g,b,a);if(!e)break;p=e+1;yy+=16;}return q;}
static unsigned char*menu_idle_blend(int mi,int af,int phase){
 const unsigned int*a=g_menu_frame_rw[mi][0][af],*b=g_menu_frame_rw[mi][0][(af+1)%3];u32 bytes=a[1]*a[2]*4u,j;unsigned int wb=(unsigned int)phase,wa=3u-wb;
 const unsigned char*pa=g_menu_pixels+a[0],*pb=g_menu_pixels+b[0];
 /* Era uma divisao inteira POR BYTE. Na EE uma divisao custa dezenas de
    ciclos, e sao 242*61*4 + 193*62*4 = 106.912 bytes por tick de
    animacao: dai os 33 ms em 40% dos quadros do menu, que e exatamente
    a fracao de quadros em que tick = frame_no*24/60 muda (71 de 178).
    Duas mudancas:
      fase 0 -> wa=3, wb=0, e (3*pa+1)/3 == pa, entao nao ha mistura
                nenhuma a fazer: devolve o ponteiro de origem, zero copia.
      fases 1 e 2 -> x/3 vira (x*21846)>>16, exato para todo x ate 766,
                que e o maximo possivel aqui (2*255+255+1). */
 /* NAO devolver pa direto. upload_raw usa DMA, que exige origem
    alinhada em 128 B, e os offsets dos quadros do menu nao sao:
      FREEPLAY  626352 %128=48   685400 %128=88
      MERCH    1183352 %128=120 1231216 %128=112
    Devolver esses ponteiros deslocava a imagem 12 e 22 pixels, e so na
    fase 0, ou seja 1 de cada 3 ticks: era a oscilacao esquerda-direita
    da 0.2.66. g_hud_pixels vem de memalign(128), sempre alinhado.
    Copiar custa quase nada perto da divisao que foi removida. */
 if(wb==0u){memcpy(g_hud_pixels,pa,bytes);return g_hud_pixels;}
 for(j=0;j<bytes;j++)g_hud_pixels[j]=(unsigned char)(((pa[j]*wa+pb[j]*wb+1u)*21846u)>>16);
 return g_hud_pixels;
}
static void story_upload_title(texbuffer_t*t,int level){const unsigned int*r=g_story_title[level];u32 bytes=r[1]*r[2]*4u;memcpy(g_hud_pixels,g_stage_pixels+r[0],bytes);FlushCache(0);setup_texture_dims(t,(int)r[1],(int)r[2]);upload_raw(t,g_hud_pixels,(int)r[1],(int)r[2]);{static unsigned int logged=0;if(level>=0&&level<9&&!(logged&(1u<<level))){telemetry_event("story_title_upload",level,(int)bytes);logged|=1u<<level;}}}
#define AV_TRACE_CAP 4096
typedef struct {u32 frame,ui,target,trans,transport,rate,suspended,menu_fd,stem_fd,queued,available,ring,ring_fd,ring_loop,ring_switches,ring_reset_pending,ring_switch_pending,produced,reads,loops,errors,underruns,title_submitted,music_clock,audio_started,audio_playing,gf_frame,bf_frame,event_cursor;u64 ticks;} av_trace_t;
static av_trace_t g_av_trace[AV_TRACE_CAP];static u32 g_av_trace_wp=0,g_av_trace_count=0;
static void av_trace_capture(u32 frame,int ui,int target,int trans,int transport,int stem_fd,u32 music_clock,int audio_started,int audio_playing,int gf_frame,int bf_frame,u32 event_cursor){av_trace_t*r=&g_av_trace[g_av_trace_wp];int rfd=-1;u32 loop=0,sw=0,rp=0,sp=0,prod=0,reads=0,loops=0,errs=0,unders=0;audio_ring_debug(&rfd,&loop,&sw,&rp,&sp);audio_ring_stats(&prod,&reads,&loops,&errs,&unders);memset(r,0,sizeof(*r));r->frame=frame;r->ui=ui;r->target=target;r->trans=trans;r->transport=transport;r->rate=g_audio_current_rate;r->suspended=g_menu_audio_suspended;r->menu_fd=g_title_audio_fd;r->stem_fd=stem_fd;if(g_title_audio_fd>=0||stem_fd>=0){r->queued=(u32)audsrv_queued();r->available=(u32)audsrv_available();}else{r->queued=0;r->available=0;}r->ring=(u32)audio_ring_level();r->ring_fd=rfd;r->ring_loop=loop;r->ring_switches=sw;r->ring_reset_pending=rp;r->ring_switch_pending=sp;r->produced=prod;r->reads=reads;r->loops=loops;r->errors=errs;r->underruns=unders;r->title_submitted=g_title_audio_submitted;r->music_clock=music_clock;r->audio_started=audio_started;r->audio_playing=audio_playing;r->gf_frame=(u32)gf_frame;r->bf_frame=(u32)bf_frame;r->event_cursor=event_cursor;r->ticks=GetTimerSystemTime();g_av_trace_wp=(g_av_trace_wp+1u)%AV_TRACE_CAP;if(g_av_trace_count<AV_TRACE_CAP)g_av_trace_count++;}
static void av_trace_dump(void){int fd=open("mass:/FNFPS2/FNF_COLHEITA.txt",O_WRONLY|O_CREAT|O_APPEND,0666);u32 n,start,i;char line[512];const char*hdr="# build=" FNF_PS2_VERSION " cap=4096 ring_order=oldest_to_newest\nticks,frame,ui,target,transition,transport,rate,suspended,menu_fd,stem_fd,audsrv_queued,audsrv_available,ring_level,ring_fd,ring_loop,ring_switches,ring_reset_pending,ring_switch_pending,produced,reads,source_loops,errors,underruns,title_submitted_samples,music_clock,audio_started,audio_playing,gf_frame,bf_frame,event_cursor\n";if(fd<0)return;write(fd,hdr,strlen(hdr));n=g_av_trace_count;start=(g_av_trace_wp+AV_TRACE_CAP-n)%AV_TRACE_CAP;for(i=0;i<n;i++){av_trace_t*r=&g_av_trace[(start+i)%AV_TRACE_CAP];int len=snprintf(line,sizeof(line),"%llu,%u,%u,%u,%u,%u,%u,%u,%d,%d,%u,%u,%u,%d,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u\n",r->ticks,r->frame,r->ui,r->target,r->trans,r->transport,r->rate,r->suspended,r->menu_fd,r->stem_fd,r->queued,r->available,r->ring,r->ring_fd,r->ring_loop,r->ring_switches,r->ring_reset_pending,r->ring_switch_pending,r->produced,r->reads,r->loops,r->errors,r->underruns,r->title_submitted,r->music_clock,r->audio_started,r->audio_playing,r->gf_frame,r->bf_frame,r->event_cursor);write(fd,line,len);}fsync(fd);close(fd);}


/* TEMP_SCREEN_READBACK 0.1.99: remove after geometry validation is complete. */
static unsigned int g_capture_index=0;
static int g_capture_raw_fd=-1;
static int g_capture_chord_latched=0;
static float g_story_title_y[9]={0};
static int g_story_title_y_ready=0;
static void story_title_targets(int selected,float*out){int i;out[selected]=284.0f;for(i=selected-1;i>=0;i--){float h=(float)g_story_title[i][2],step=fmaxf(h+10.0f,62.5f);out[i]=out[i+1]-step;}for(i=selected+1;i<9;i++){float ph=(float)g_story_title[i-1][2];out[i]=out[i-1]+ph+10.0f;}}
static void story_title_update_positions(int selected){float t[9];int i;story_title_targets(selected,t);if(!g_story_title_y_ready){for(i=0;i<9;i++)g_story_title_y[i]=t[i];g_story_title_y_ready=1;}else for(i=0;i<9;i++)g_story_title_y[i]+=(t[i]-g_story_title_y[i])*STORY_TITLE_LERP_ALPHA;}
static void write_all_fd(int fd,const void*buf,u32 n){const unsigned char*p=(const unsigned char*)buf;while(n){int w=write(fd,p,n);if(w<=0)break;p+=w;n-=w;}}
static volatile int g_capture_status_write_enabled=1;
/* Marco de fase. EScrevia direto no arquivo, abrindo e fechando com fsync a
   cada chamada, enquanto a telemetria segurava outro descritor aberto com
   buffer. Dois escritores no mesmo arquivo: no pendrive a ultima colheita veio
   com 0 bytes -- o arquivo existia e nao tinha nada dentro. Agora tudo sai pelo
   descritor unico da telemetria. */
/* Marcador de boot: UM arquivo, reescrito a cada passo, com fsync.
   O log sozinho nao basta para diagnosticar um boot que nao acontece: se o
   pendrive nunca montou, nao ha log nenhum, e eu fico sem nenhuma pista. Este
   arquivo diz o ultimo passo que o jogo completou. Ele para de ser escrito
   depois da fase de boot, para nao custar E/S no jogo rodando. */
static int g_telemetry_init_result=0;
static int g_boot_mark_n=0;
static void boot_mark(const char*phase,int code){
  int fd;char b[128];int n;
  if(g_boot_mark_n>400)return;
  g_boot_mark_n++;
  fd=open("mass:/FNFPS2/BOOT.txt",O_WRONLY|O_CREAT|O_TRUNC,0666);
  if(fd<0)fd=open("mass:/BOOT.txt",O_WRONLY|O_CREAT|O_TRUNC,0666);
  if(fd<0)return;
  n=snprintf(b,sizeof(b),"passo %d: %s code=%d\n",g_boot_mark_n,phase,code);
  write_all_fd(fd,b,n);
  fsync(fd);
  close(fd);
}
static void capture_status(const char*phase,int row,int code){
  if(!g_capture_status_write_enabled)return;
  boot_mark(phase,code);
  telemetry_phase(phase,row,code);}

/* ===================== DIAGNOSTICO DE TRAVAMENTO 0.2.116 ===================
   O BOOT.txt da sessao de 2026-10-27 parou em "passo 48: FRAME code=0": o jogo
   entrou no laco de quadros e morreu DENTRO do corpo de um quadro, na tela de
   titulo, por volta do quadro 15. Nao dava para saber em qual chamada, porque
   em ui_state 0 o FRAME e o primeiro marco do quadro e o proximo so vem no
   quadro seguinte.

   O que entra aqui NAO conserta nada: coleta. Regra 4 do projeto -- sem dado
   nao se escolhe causa.

   Duas pecas, com papeis diferentes:

   1) frame_mark(): marco fino (sub-passo dentro do quadro) escrito pelo MESMO
      caminho do boot_mark(), que foi o unico escritor que comprovadamente
      escreveu no console do usuario (open -> write -> fsync -> close).
      Custo LIMITADO POR CONSTRUCAO: no maximo uma escrita a cada 200 ms, com
      fsync em uma a cada quatro. A 0.2.108 nao abriu no console por ~20
      escritas POR QUADRO; isto e no maximo 5 por segundo.

   2) Cao de guarda: as posicoes ficam tambem num anel de RAM, sem portao. Uma
      thread de prioridade baixa vigia o contador de quadros; se ele nao mudar
      em 8 acordadas de 200 ms (1,6 s), ela grava as ultimas 16 posicoes em
      FNF_TRAVA.txt. E o unico jeito de saber onde o processo parou quando ele
      para de responder. No caminho normal isso NAO escreve nada.

   Ordem de grandeza do custo total: 5 escritas/s + 0 no caminho normal do cao
   de guarda, contra os ~1200/s da 0.2.108. */
#define FNMARK_MIN_GAP_TICKS ((u64)kBUSCLK/5u)   /* 200 ms */
#define FNMARK_RING 16
#define FNWD_WAKE_US 200000
#define FNWD_STALL_WAKES 15                       /* 15 x 200 ms = 3 s parado.
                                                     Carregar packs do tutorial
                                                     leva ~1 s; 1,6 s dava falso
                                                     positivo em carga normal. */
#ifndef FNMARK_FRAME_GATE
#define FNMARK_FRAME_GATE 300u                    /* ver comentario em frame_mark */
#endif
#define FNWD_MAX_DUMPS 3
#define FNWD_PAUSE_WAIT_US 2000000

typedef struct { const char*phase; int code; u32 frame; u32 queued; u32 ring;
                 u32 underruns; u32 submits; u32 zeros; } fnmark_t;

static fnmark_t g_fnring[FNMARK_RING];
static volatile u32 g_fnring_wp=0;
static volatile u32 g_fn_beat=0;          /* muda a cada quadro completo */
static volatile u32 g_fn_frame=0;         /* quadro corrente, para o cao */
static volatile int g_fnwd_stop=0;
static int g_fnwd_thread=-1;
static u64 g_fn_last_write=0;
static u64 g_fn_last_write_frame=0;
static u32 g_fn_writes=0;
static volatile int g_fn_write_ok=1;      /* vira 0 na primeira falha: desliga */

/* Contadores do anel de audio. audio_underruns eh uma LOCAL do main(), entao
   aqui se le pela API do anel: mesma fonte, sem depender de escopo. */
static u32 g_fn_underruns=0;
static void fn_ler_contadores(void){
  u32 p=0,r=0,l=0,e=0,u=0;
  audio_ring_stats(&p,&r,&l,&e,&u);
  g_fn_underruns=u;
}

/* Despeja o anel inteiro num arquivo so. Custo por despejo: UM open, UM write
   de ~1 KB e UM close (uma chamada de RPC, nao dezesseis). O fsync acontece em
   um despejo a cada quatro, entao o pior atraso entre o que aconteceu e o que
   esta no pendrive e' de 800 ms -- e o cao de guarda cobre o caso de travamento
   com um despejo sincronizado proprio.

   POR QUE O ANEL INTEIRO E NAO UMA LINHA: a primeira versao gravava so a marca
   do instante do portao, e como o portao abre sempre na primeira marca do
   quadro, o arquivo saia com F.IN em toda linha -- 81 linhas de F.IN no teste do
   harness. Aquilo nao localiza nada. Com o anel, a cauda do arquivo e' sempre a
   sequencia de fases do ultimo quadro, que e' exatamente o que se precisa para
   saber onde o jogo para. */
static void fnmark_flush(void){
  int fd;char b[256];u32 i,base;
  if(!g_fn_write_ok)return;
  fd=open("mass:/FNFPS2/BOOT.txt",O_WRONLY|O_CREAT|O_TRUNC,0666);
  if(fd<0)fd=open("mass:/BOOT.txt",O_WRONLY|O_CREAT|O_TRUNC,0666);
  if(fd<0){g_fn_write_ok=0;return;}
  fn_ler_contadores();
  g_fn_writes++;
  {
    int n=snprintf(b,sizeof(b),"# despejo %u marcas f=%u q=%u ring=%u und=%u sub=%u zero=%u\n",
      (unsigned)g_fn_writes,(unsigned)g_fn_frame,(unsigned)audsrv_queued(),
      (unsigned)audio_ring_level(),(unsigned)g_fn_underruns,
      (unsigned)g_audsrv_submit_errors,(unsigned)g_audsrv_zero_events);
    write_all_fd(fd,b,n);
  }
  base=(g_fnring_wp>FNMARK_RING)?(g_fnring_wp-FNMARK_RING):0u;
  for(i=0;i<FNMARK_RING&&(u32)(base+i)<g_fnring_wp;i++){
    const fnmark_t*r=&g_fnring[(base+i)%FNMARK_RING];
    int n=snprintf(b,sizeof(b),"rt %s code=%d f=%u q=%u ring=%u und=%u sub=%u zero=%u\n",
      r->phase?r->phase:"?",r->code,(unsigned)r->frame,(unsigned)r->queued,
      (unsigned)r->ring,(unsigned)r->underruns,(unsigned)r->submits,(unsigned)r->zeros);
    write_all_fd(fd,b,n);
  }
  if((g_fn_writes&3u)==0u)fsync(fd);
  close(fd);
}

/* Registra o sub-passo no anel de RAM (sempre) e despeja o anel quando o
   portao abre. Dois portoes independentes:
     tempo   -> no maximo 5 despejos por segundo (o portao que vale no console);
     quadros -> no maximo 1 despejo a cada 300 quadros (5 s a 60 fps). Existe
                porque o harness do PC mede o tempo em microssegundos, nao em
                unidades de BUSCLK: sem ele o caminho de escrita ficaria sem
                cobertura la. No console o portao de tempo sempre abre antes. */
static void frame_mark(const char*phase,int code){
  fnmark_t*r=&g_fnring[g_fnring_wp%FNMARK_RING];
  u64 now=GetTimerSystemTime();
  r->phase=phase;r->code=code;r->frame=g_fn_frame;
  fn_ler_contadores();
  r->underruns=g_fn_underruns;
  r->queued=(u32)audsrv_queued();r->ring=(u32)audio_ring_level();
  r->submits=(u32)g_audsrv_submit_errors;r->zeros=(u32)g_audsrv_zero_events;
  g_fnring_wp++;
  if(g_fn_last_write&&now-g_fn_last_write<FNMARK_MIN_GAP_TICKS&&
     (g_fn_frame-g_fn_last_write_frame)<FNMARK_FRAME_GATE)return;
  g_fn_last_write=now;
  g_fn_last_write_frame=g_fn_frame;
  fnmark_flush();
}

/* Grava as ultimas posicoes. Usada SO pelo cao de guarda, quando o jogo parou. */
static void fnwd_dump(int dumps){
  int fd;char b[256],hdr[160];int i,n,u32base;
  /* O usbhdfsd nao e reentrante: o produtor de audio para de tocar o USB antes.
     A espera e limitada (2 s) e a gravacao acontece mesmo se ele nao parar --
     e o ultimo recurso de diagnostico. */
  audio_ring_pause(1);
  fd=open("mass:/FNFPS2/FNF_TRAVA.txt",O_WRONLY|O_CREAT|O_APPEND,0666);
  if(fd<0){
    audio_ring_pause(0);
    return;
  }
  n=snprintf(hdr,sizeof(hdr),
    "# build=" FNF_PS2_VERSION " dumps=%d parado_ha=%dms f=%u q=%u ring=%u und=%u sub=%u zero=%u\n",
    dumps,FNWD_STALL_WAKES*FNWD_WAKE_US/1000,(unsigned)g_fn_frame,
    (unsigned)audsrv_queued(),(unsigned)audio_ring_level(),(unsigned)g_fn_underruns,
    (unsigned)g_audsrv_submit_errors,(unsigned)g_audsrv_zero_events);
  write_all_fd(fd,hdr,n);
  u32base=(g_fnring_wp>FNMARK_RING)?(g_fnring_wp-FNMARK_RING):0u;
  for(i=0;i<FNMARK_RING&&(u32)(u32base+(u32)i)<g_fnring_wp;i++){
    fnmark_t*r=&g_fnring[(u32base+i)%FNMARK_RING];
    n=snprintf(b,sizeof(b),"  %s code=%d f=%u q=%u ring=%u und=%u sub=%u zero=%u\n",
      r->phase?r->phase:"?",r->code,(unsigned)r->frame,(unsigned)r->queued,
      (unsigned)r->ring,(unsigned)r->underruns,(unsigned)r->submits,(unsigned)r->zeros);
    write_all_fd(fd,b,n);
  }
  fsync(fd);
  close(fd);
  audio_ring_pause(0);
}

static unsigned char fnwd_stack[16384] __attribute__((aligned(128)));
static void fnwd_thread_fn(void*arg);
static void fnwd_start(void){
  ee_thread_t t;
  if(g_fnwd_thread>=0)return;
  memset(&t,0,sizeof(t));
  t.func=fnwd_thread_fn;t.stack=fnwd_stack;t.stack_size=sizeof(fnwd_stack);
  t.gp_reg=&_gp;t.initial_priority=10;   /* abaixo do audio (40) e do titulo (50) */
  g_fnwd_thread=CreateThread(&t);
  if(g_fnwd_thread<0)return;
  if(StartThread(g_fnwd_thread,0)<0){DeleteThread(g_fnwd_thread);g_fnwd_thread=-1;}
}
static void fnwd_thread_fn(void*arg){
  u32 last_beat=0;
  u32 wakes=0;
  int dumps=0;
  int paradas=0;      /* acordadas seguidas sem o quadro avancar */
  (void)arg;
  /* Teto de vida: 6000 acordadas de 200 ms = 20 minutos. Depois disso a
     thread sai sozinha, para nao existir laco eterno de diagnostico. */
  while(!g_fnwd_stop&&dumps<FNWD_MAX_DUMPS&&wakes<6000u){
    DelayThread(FNWD_WAKE_US);
    wakes++;
    if(g_fn_beat!=last_beat){last_beat=g_fn_beat;paradas=0;continue;}
    paradas++;
    if(paradas<FNWD_STALL_WAKES)continue;
    paradas=0;
    dumps++;
    fnwd_dump(dumps);
  }
  ExitThread();
}
static u32 title_intro_phase(void){
 int queued_bytes;
 if(g_title_audio_fd>=0){
  queued_bytes=audsrv_queued();
  /* Invalid queue readings must hold the clock instead of resetting it. */
  if(queued_bytes>=0){
   u32 queued_samples=(u32)queued_bytes/4u;
   u32 heard=g_title_audio_submitted>queued_samples?g_title_audio_submitted-queued_samples:0u;
   if(heard>g_title_intro_heard_samples)g_title_intro_heard_samples=heard;
  }
 }
 {
  u64 source_samples=((u64)g_title_intro_heard_samples*147u)/320u;
  u32 phase=(u32)(source_samples/12970u);
  if(phase>g_title_intro_phase_latched)g_title_intro_phase_latched=phase;
 }
 return g_title_intro_phase_latched;
}

#define CAP_D1_CHCR (*(volatile u32*)0x10009000)
#define CAP_D1_MADR (*(volatile u32*)0x10009010)
#define CAP_D1_QWC (*(volatile u32*)0x10009020)
#define CAP_GS_CSR (*(volatile u64*)0x12001000)
#define CAP_GS_IMR (*(volatile u64*)0x12001010)
#define CAP_GS_BUSDIR (*(volatile u64*)0x12001040)
#define CAP_VIF1_STAT (*(volatile u32*)0x10003c00)
#define CAP_VIF1_FIFO (*(volatile u128*)0x10005000)
#define CAP_VIF_MSKPATH3(v) ((u32)(v)|((u32)0x06<<24))
#define CAP_VIF_FLUSHA ((u32)0x13<<24)
#define CAP_VIF_DIRECT(n) ((u32)(n)|((u32)0x50<<24))
#define CAP_GIFTAG(n,eop) (((u64)(n))|((u64)(eop)<<15)|((u64)1<<60))
static int cap_wait_clear32(volatile u32*r,u32 m,u64 t){u64 s=GetTimerSystemTime();while(*r&m)if(GetTimerSystemTime()-s>t)return-1;return 0;}
static int cap_wait_set64(volatile u64*r,u64 m,u64 t){u64 s=GetTimerSystemTime();while(!(*r&m))if(GetTimerSystemTime()-s>t)return-1;return 0;}
/* O framebuffer agora e' PSMCT16, entao a transferencia D1 leva 2 bytes por
   pixel (QWC = largura*altura*2/16 = largura*h/8) e o BITBLTBUF tem de declarar
   o formato REAL do buffer. Com GS_PSM_32 aqui, o GS leria o dobro de bytes por
   linha e a captura sairia embaralhada -- ou seja, a evidencia de um bug
   viraria ela mesma um bug. */
static int safe_readback_rect(void*d,u32 v,u32 y,u32 h){u32 c[40]__attribute__((aligned(16))),*p=c,qwc=SCREEN_W*h/8,oc=CAP_D1_CHCR;u64*q=(u64*)(p+4),oi=CAP_GS_IMR,t=294912000ull;int r=-1;if(!h||qwc>65535u||(oc&0x100u))return-10;p[0]=0;p[1]=CAP_VIF_MSKPATH3(0x8000);p[2]=CAP_VIF_FLUSHA;p[3]=CAP_VIF_DIRECT(6);q[0]=CAP_GIFTAG(5,1);q[1]=0x0e;q[2]=GS_SET_BITBLTBUF(v,SCREEN_W/64,FB_PSM,0,0,FB_PSM);q[3]=GS_REG_BITBLTBUF;q[4]=GS_SET_TRXPOS(0,y,0,0,0);q[5]=GS_REG_TRXPOS;q[6]=GS_SET_TRXREG(SCREEN_W,h);q[7]=GS_REG_TRXREG;q[8]=0;q[9]=0x61;q[10]=GS_SET_TRXDIR(1);q[11]=GS_REG_TRXDIR;CAP_GS_IMR=oi|0x200u;CAP_GS_CSR=2;FlushCache(0);CAP_D1_QWC=7;CAP_D1_MADR=(u32)c;CAP_D1_CHCR=0x101;EE_SYNCL();if(cap_wait_clear32(&CAP_D1_CHCR,0x100,t)<0){r=-11;goto x;}if(cap_wait_set64(&CAP_GS_CSR,2,t)<0){r=-12;goto x;}if(cap_wait_clear32(&CAP_VIF1_STAT,0x1f000000u,t)<0){r=-13;goto x;}CAP_VIF1_STAT=1u<<23;CAP_GS_BUSDIR=1;FlushCache(0);CAP_D1_QWC=qwc;CAP_D1_MADR=(u32)d;CAP_D1_CHCR=0x100;EE_SYNCL();if(cap_wait_clear32(&CAP_D1_CHCR,0x100,t)<0){r=-14;goto x;}r=1;x:CAP_D1_CHCR=oc;EE_SYNCL();CAP_VIF1_STAT=0;CAP_GS_BUSDIR=0;CAP_GS_IMR=oi;CAP_GS_CSR=2;{u128 e=(u128)CAP_VIF_MSKPATH3(0);CAP_VIF1_FIFO=e;}return r;}
static int raw_write_chunks(int f,const unsigned char*p,u32 n){while(n){u32 a=n>32768u?32768u:n;int w=write(f,p,a);if(w!=(int)a)return w<0?-21:-22;p+=a;n-=a;}return 0;}
/* Durante a captura o anel de audio PARA de ler do pendrive. O
   usbhdfsd nao e reentrante e esta funcao grava 1,1 MB nele pela
   thread principal. Sem isso, depois de algumas capturas a musica e
   as animacoes paravam de vez. */
/* O framebuffer e' de 16 bits, mas o arquivo de captura continua igual ao de
   antes: 32 bits por pixel, na ordem R,G,B,A. A conversao e' feita AQUI, no
   proprio buffer, de tras para a frente (o destino ocupa o dobro de cada
   origem, entao ir de tras para a frente nunca pisa no que falta ler).
   Escrever .rgba com 2 bytes por pixel quebraria todas as ferramentas que ja'
   leem a captura, e a captura existe para ser olhada. */
static void cap_expand16(u32 n)
{
    const unsigned char *src = (const unsigned char *)g_stage_pixels;
    unsigned char *dst = g_stage_pixels;
    u32 i;
    for (i = n; i-- > 0;) {
        u32 v = (u32)src[i * 2u] | ((u32)src[i * 2u + 1u] << 8);
        u32 r = v & 0x1fu, gg = (v >> 5) & 0x1fu, b = (v >> 10) & 0x1fu;
        dst[i * 4u + 0u] = (unsigned char)((r << 3) | (r >> 2));
        dst[i * 4u + 1u] = (unsigned char)((gg << 3) | (gg >> 2));
        dst[i * 4u + 2u] = (unsigned char)((b << 3) | (b >> 2));
        dst[i * 4u + 3u] = 255;
    }
}
static int capture_framebuffer_raw(u32 fb,u32 fr,u32 ui){const u32 b0=640u*200u*4u,b1=640u*200u*4u,b2=640u*48u*4u;int r=0;(void)fr;(void)ui;audio_ring_pause(1);
 /* AUTO-REPARO DO DESCRITOR DE CAPTURA.
    O fd era aberto uma unica vez no boot e so reaberto DEPOIS de uma captura
    que desse certo. Se o open do boot falhasse -- e no teste no console falhou,
    todas as 10 chamadas devolveram -2 -- nenhuma captura posterior tentava de
    novo: a captura de tela ficava morta para o resto da sessao e eu perdia
    justamente a evidencia de que precisava para diagnosticar o resto.
    Agora, se o fd esta fechado, ele e reaberto aqui, com o proximo numero
    livre, e a captura segue. O numero so avanca quando a escrita da certo. */
 if(g_capture_raw_fd<0){char np[96];int nf;g_capture_index++;snprintf(np,sizeof(np),"mass:/FNFPS2/FNF_SCREEN_%04u.rgba.txt",g_capture_index);nf=open(np,O_WRONLY|O_CREAT|O_TRUNC,0666);if(nf>=0){g_capture_raw_fd=nf;fsync(nf);telemetry_event("capture_reopen",(int)g_capture_index,0);}else{telemetry_event("capture_reopen",(int)g_capture_index,nf);g_capture_index--;}}
 if(g_capture_raw_fd<0)r=-2;else if(safe_readback_rect(g_stage_pixels,fb,0,200)!=1)r=-11;else{cap_expand16(640u*200u);if(raw_write_chunks(g_capture_raw_fd,g_stage_pixels,b0)<0)r=-21;else if(safe_readback_rect(g_stage_pixels,fb,200,200)!=1)r=-12;else{cap_expand16(640u*200u);if(raw_write_chunks(g_capture_raw_fd,g_stage_pixels,b1)<0)r=-22;else if(safe_readback_rect(g_stage_pixels,fb,400,48)!=1)r=-13;else{cap_expand16(640u*48u);if(raw_write_chunks(g_capture_raw_fd,g_stage_pixels,b2)<0)r=-23;}}}if(g_capture_raw_fd>=0){fsync(g_capture_raw_fd);close(g_capture_raw_fd);g_capture_raw_fd=-1;}capture_status(r?"RAW_FAILED":"RAW_COMPLETE",r?(-1):448,r);telemetry_event("screen_capture_raw",(int)g_capture_index,r);g_capture_index++;if(r==0&&g_capture_raw_fd<0){char np[96];snprintf(np,sizeof(np),"mass:/FNFPS2/FNF_SCREEN_%04u.rgba.txt",g_capture_index);g_capture_raw_fd=open(np,O_WRONLY|O_CREAT|O_TRUNC,0666);if(g_capture_raw_fd>=0)fsync(g_capture_raw_fd);}audio_ring_pause(0);return r;}
/* =====================================================================
   DESPEJO DE ESTADO COMPLETO
   SELECT+R1 grava mass:/FNFPS2/FNF_ESTADO.txt com tudo que da para
   saber da maquina naquele instante: posicoes na tela, o que esta
   carregado em memoria, estado do som, VRAM, arquivos abertos e
   contadores.
   Existe porque o traco do MERCH ja derrubou seis hipoteses minhas e
   os vertices saem INTEIROS EXATOS, 225,0000 a 416,0000, e mesmo assim
   o GS escreve a linha 316 de um quad que vai ate 315. Quando o modelo
   nao explica, a saida nao e um setimo palpite: e olhar a maquina
   inteira de uma vez.
   ===================================================================== */
typedef struct {
    int ui, sel, trans, alvo, afd, tfd, astart, aplay, avol, voices, ph, tick;
    u32 clock, frame, subm;
    float camy;
    unsigned short held, pressed;
    u32 fb0, fb1;
    texbuffer_t *tex, *tex2, *fonttex, *gftex, *stagetex;
} estado_t;

static int g_congelar = 0;
static int g_cap_busy = 0;   /* 0.2.101: captura em andamento */
static int g_cong_frames = 0;

static void fnx_hex(int fd, const unsigned char *p, int n, const char *rot)
{
    char b[72];
    int i, j, k;
    for (i = 0; i < n; i += 16) {
        k = snprintf(b, sizeof(b), "  %s+%05d ", rot, i);
        for (j = 0; j < 16 && i + j < n; j++) k += snprintf(b + k, sizeof(b) - k, "%02x", p[i + j]);
        k += snprintf(b + k, sizeof(b) - k, "\\n");
        write_all_fd(fd, b, k);
    }
}


static void fnx_congelado2(int fd, u32 fn0, int ui0, int sel0, float camy0, unsigned short held0)
{
    char b[512];
    int k, mi;
    av_trace_dump();  /* 0.2.102: o traco AV entra no arquivo unico sob demanda,
                         ja que o shutdown nao dumpa mais (era o "travamento") */
    if (fd < 0) fd = open("mass:/FNFPS2/FNF_COLHEITA.txt", O_WRONLY | O_CREAT | O_APPEND, 0666);
    int tick = (int)(fn0 * 24u / 60u);
    float camoff = (camy0 - 360.0f) * 0.4f;
    if (fd < 0) return;
    k = snprintf(b, sizeof(b),
        "# FNF PS2 %s CONGELADO\n"
        "quadro %u | ui_state %d | selection %d | cam_y %.3f | tick %d | held %04x\n",
        FNF_PS2_VERSION, fn0, ui0, sel0, (double)camy0, tick, held0);
    write_all_fd(fd, b, k);
    k = snprintf(b, sizeof(b), "[ITENS]\n"); write_all_fd(fd, b, k);
    for (mi = 0; mi < 5; mi++) {
        int sel = (mi == sel0);
        int af = sel ? (tick % 3) : g_menu_idle_timeline[mi][tick % 9];
        const unsigned int *fr = g_menu_frame_rw[mi][sel][af];
        float cy = (40.0f + 160.0f * (float)mi - camoff) * .5f;
        float dx = floorf(320.0f - fr[1] * .5f + .5f);
        float dy = floorf(vy(cy) - fr[2] * 0.5f + 0.5f); /* topo fisico apos C7 */
        /* 0.2.104 X6: replicar o desenho REAL. O inset de 1 px foi removido na
           0.2.102 e este diagnostico continuou somando +-1,0, entao publicava
           gs 1,5625 px deslocado e qualquer comparacao com a captura dava uma
           linha de erro. Agora vale exatamente o que sprite_region_alpha e
           sprite_region_phys mandam para o libdraw. */
        float vx0, vy0, vx1, vy1;
        /* 0.2.105 X7: o desenho agora e phys 1:1 para os cinco itens. */
        vx0 = FNX_INI((float)dx); vy0 = FNX_INI(dy);
        vx1 = FNX_FIM((float)dx + (float)fr[1]); vy1 = FNX_INI(dy + (float)fr[2]);
        float gx0 = (float)((int)((vx0 + 2047.5625f) * 16.0f) - 32768) / 16.0f;
        float gy0 = (float)((int)((vy0 + 2047.5625f) * 16.0f) - 32768) / 16.0f;
        float gx1 = (float)((int)((vx1 + 2048.5625f) * 16.0f) - 32768) / 16.0f;
        float gy1 = (float)((int)((vy1 + 2048.5625f) * 16.0f) - 32768) / 16.0f;
        int lr0 = (int)(gy0 + 0.5f), lr1 = (int)(gy1 - 0.5f);
        int tr0 = 0, tr1 = (int)fr[2] - 1; /* 1:1 apos C7 */
        k = snprintf(b, sizeof(b),
            " item %d sel=%d af=%d fase=%d fr_off=%u fr=%ux%u\n"
            "   dx,dy=%.2f,%.2f  v(%.4f,%.4f)-(%.4f,%.4f)  gs(%.4f,%.4f)-(%.4f,%.4f)\n"
            "   linhas tela %d..%d  texel row primeira=%d ultima=%d\n"
            "   uv(%.4f..%.4f) filtro=%d clamp v=%d..%d atlas alfa>128 corrigidos=%u rgb dilatados=%u\n"
            "   phys1:1 hp=%u linhas_removidas=%u flick_item_vis=%d toggles=%d flick_mag_vis=%d toggles=%d exit=%d fim_item=%d switch=%d\n",
            mi, sel, af, tick % 3, fr[0], fr[1], fr[2], (double)dx, (double)dy,
            vx0, vy0, vx1, vy1, gx0, gy0, gx1, gy1, lr0, lr1, tr0, tr1,
            0.5, (double)fr[2] - 0.5, 1, 0, (int)fr[2] - 1,
            g_menu_alpha_clamped, g_menu_rgb_dilated,
            fr[2], g_menu_phys_removed, g_fl_item_vis, g_fl_item_toggles,
            g_fl_mag_vis, g_fl_mag_toggles, menu_exit_frames,
            g_accept_item_end, g_accept_switch);
        write_all_fd(fd, b, k);
        if (fr[2] >= 3) {
            const unsigned char *p = g_menu_pixels + fr[0] + (u32)(fr[2] - 3) * fr[1] * 4u;
            k = snprintf(b, sizeof(b), "   atlas ultimas 3 linhas (RAM):\n");
            write_all_fd(fd, b, k);
            fnx_hex(fd, p, (int)(3u * fr[1] * 4u), "row");
        }
    }
    fsync(fd);
    close(fd);
    capture_status("CONGELADO", tick, 0);
}

static void ed_tex(int fd, const char *nome, texbuffer_t *t)
{
    char b[200];
    int w = 1, h = 1, k;
    texture_extent_get(t, &w, &h);
    k = snprintf(b, sizeof(b),
        "  %-9s addr %8u palavras = %9u bytes | tbw %3u | psm %u | "
        "extensao %4dx%-4d | log2 %dx%d\n",
        nome, t->address, t->address * 4u, t->width, t->psm, w, h,
        (int)t->info.width, (int)t->info.height);
    write_all_fd(fd, b, k);
}

/* A tabela de buffers que o estado e o despejo mostram, num lugar so'. O
   tamanho de cada um e' o que foi pedido ao memalign; os dois packs do tutorial
   entram com o tamanho REAL lido do arquivo. Isto e' a parte da RAM que o jogo
   aloca, e ela e' usada tambem pelo orcamento de RAM do empacotador
   (tools/check_stage_budget.py le o evento ram_buffers do log). */
typedef struct { const char *n; void *p; unsigned int t; } heap_ent_t;
static void heap_tab(heap_ent_t *hs, int *n)
{
    int i = 0;
    hs[i].n = "g_menu_pixels";     hs[i].p = g_menu_pixels;     hs[i++].t = TITLE_SNAPSHOT_END;
    hs[i].n = "g_menu_compressed"; hs[i].p = g_menu_compressed; hs[i++].t = PRELOAD_RLE_BYTES;
    hs[i].n = "g_menu_sfx_pixels"; hs[i].p = g_menu_sfx_pixels; hs[i++].t = 281088u;
    hs[i].n = "g_stage_pixels";    hs[i].p = g_stage_pixels;    hs[i++].t = 512u*256u*4u;
    hs[i].n = "g_hud_pixels";      hs[i].p = g_hud_pixels;      hs[i++].t = 320u*256u*4u;
    hs[i].n = "g_font_pixels";     hs[i].p = g_font_pixels;     hs[i++].t = FONT_W*256u*4u;
    hs[i].n = "g_gf_pixels";       hs[i].p = g_gf_pixels;       hs[i++].t = TITLE_STORE_BYTES;
    hs[i].n = "g_story_gf_pixels"; hs[i].p = g_story_gf_pixels; hs[i++].t = 170u*157u*4u;
    hs[i].n = "g_story_bf_pixels"; hs[i].p = g_story_bf_pixels; hs[i++].t = 164u*165u*4u;
    hs[i].n = "g_tutorial_bf_pack";hs[i].p = g_tutorial_bf_pack;hs[i++].t = g_tutorial_bf_pack_bytes;
    hs[i].n = "g_tutorial_gf_pack";hs[i].p = g_tutorial_gf_pack;hs[i++].t = g_tutorial_gf_pack_bytes;
    *n = i;
}
static unsigned int ram_buffers_total(void)
{
    heap_ent_t hs[16];
    int n = 0, i;
    unsigned int soma = 0;
    heap_tab(hs, &n);
    for (i = 0; i < n; i++) soma += hs[i].p ? hs[i].t : 0u;
    return soma;
}

static void estado_dump(const estado_t *e)
{
    char b[640];  /* 256 era pouco para a secao [GS]: pior caso ~331 B */
    int fd, k, i;
    unsigned int prod, reads, loops, errs, unders;
    int rfd; unsigned int rloop, rsw, rpend, rspend;

    fd = open("mass:/FNFPS2/FNF_COLHEITA.txt", O_WRONLY | O_CREAT | O_APPEND, 0666);
    if (fd < 0) return;

    k = snprintf(b, sizeof(b), "FNF PS2 %s - despejo de estado\n"
                 "quadro %u | ui_state %d | transition_frame %d -> alvo %d\n\n",
                 FNF_PS2_VERSION, e->frame, e->ui, e->trans, e->alvo);
    write_all_fd(fd, b, k);

    /* Mapa de VRAM calculado (engine/vram_layout.c), nao um numero escrito a
       mao: o texto antigo dizia "total 4194304, framebuffers 2293760" e nada
       mais, e ninguem podia conferir se a soma das texturas cabia. */
    k = snprintf(b, sizeof(b),
        "[VRAM] total %u bytes | usado %u | livre %u | cabe %d\n",
        VL_TOTAL_WORDS * 4u, vl_used_words() * 4u, vl_free_words() * 4u, vl_fits());
    write_all_fd(fd, b, k);
    /* Endereco CALCULADO ao lado do endereco que o alocador do ps2sdk deu de
       verdade. Se as duas colunas divergirem em qualquer linha, o mapa do
       projeto esta' errado e o log mostra onde -- nao ha' como "achar" que a
       conta fecha. A coluna real sai do que esta' de fato alocado nos objetos
       de textura; o par de buffers de quadro vem em e->fb0/e->fb1. */
    {
        unsigned int real[VL_SLOTS];
        real[VL_FB0] = e->fb0; real[VL_FB1] = e->fb1;
        /* A `tex` nao esta' mais na tabela do mapa: ela e' chave do pool. */
        real[VL_TEX2] = e->tex2->address;
        real[VL_FONTTEX] = e->fonttex->address;
        real[VL_GFTEX16] = g_gftex16.address;
        for (i = 0; i < (int)vl_slot_count(); i++) {
            const vl_slot_t *sl = &vl_slots()[i];
            k = snprintf(b, sizeof(b),
                "  %-9s %4ux%-4u psm %2u | calculado %8u | alocado %8u | %s\n",
                sl->nome, sl->w, sl->h, sl->psm, vl_address((unsigned)i),
                real[i], real[i] == vl_address((unsigned)i) ? "igual" : "DIVERGE");
            write_all_fd(fd, b, k);
        }
    }
    ed_tex(fd, "tex", e->tex);
    ed_tex(fd, "tex2", e->tex2);
    ed_tex(fd, "gftex", e->gftex);
    ed_tex(fd, "stagetex", e->stagetex);
    ed_tex(fd, "fonttex", e->fonttex);
    ed_tex(fd, "gftex16", &g_gftex16);

    k = snprintf(b, sizeof(b), "\n[HEAP] ponteiro e tamanho de cada buffer\n");
    write_all_fd(fd, b, k);
    {
        heap_ent_t hs[16];
        int hn = 0;
        unsigned int soma = 0;
        heap_tab(hs, &hn);
        for (i = 0; i < hn; i++) {
            soma += hs[i].p ? hs[i].t : 0u;
            k = snprintf(b, sizeof(b), "  %-20s %s  %9u bytes\n",
                         hs[i].n, hs[i].p ? "alocado" : "  NULO ", hs[i].t);
            write_all_fd(fd, b, k);
        }
        k = snprintf(b, sizeof(b), "  %-20s          %9u bytes\n", "SOMA", soma);
        write_all_fd(fd, b, k);
    }

    audio_ring_stats(&prod, &reads, &loops, &errs, &unders);
    audio_ring_debug(&rfd, &rloop, &rsw, &rpend, &rspend);
    k = snprintf(b, sizeof(b),
        "\n[AUDIO]\n  audsrv queued %d available %d volume %d vozes %d\n"
        "  audio_fd %d | title_fd %d | started %d playing %d\n"
        "  music_clock %u | submetido %u\n",
        audsrv_queued(), audsrv_available(), e->avol, e->voices,
        e->afd, e->tfd, e->astart, e->aplay, e->clock, e->subm);
    write_all_fd(fd, b, k);
    k = snprintf(b, sizeof(b),
        "  anel: nivel %d fd %d loop %u trocas %u reset_pend %u troca_pend %u\n"
        "  anel: produzido %u leituras %u voltas %u erros %u secas %u eof %d\n",
        audio_ring_level_snapshot(), rfd, rloop, rsw, rpend, rspend,
        prod, reads, loops, errs, unders, audio_ring_eof());
    write_all_fd(fd, b, k);

    /* [GS] registradores privilegiados de saida de video, lidos DIRETO do
       hardware. E o unico jeito de saber se o GSM do OPL (ou outro loader)
       reescreveu o modo por baixo do jogo. Sem isto, a conta de proporcao pode
       estar exata para NTSC 4:3 e ainda assim sair errada na TV, e nada que o
       jogo produza por si mesmo revelaria a diferenca. */
    {
      u64 P  = *(volatile u64*)0x12000000;  /* PMODE    */
      u64 S  = *(volatile u64*)0x12000020;  /* SMODE2   */
      u64 F1 = *(volatile u64*)0x12000070;  /* DISPFB1  */
      u64 D1 = *(volatile u64*)0x12000080;  /* DISPLAY1 */
      u64 F2 = *(volatile u64*)0x12000090;  /* DISPFB2  */
      u64 D2 = *(volatile u64*)0x120000A0;  /* DISPLAY2 */
      u64 EB = *(volatile u64*)0x120000B0;  /* EXTBUF   */
      u64 ED = *(volatile u64*)0x120000C0;  /* EXTDATA  */
      u64 EW = *(volatile u64*)0x120000D0;  /* EXTWRITE */
      u64 C  = *(volatile u64*)0x12001000;  /* CSR      */
      k = snprintf(b, sizeof(b),
        "\n[GS] registradores de saida, lidos do hardware\n"
        "  PMODE    %08X%08X\n"
        "  SMODE2   %08X%08X\n"
        "  DISPFB1  %08X%08X\n"
        "  DISPFB2  %08X%08X\n"
        "  EXTBUF   %08X%08X\n"
        "  EXTDATA  %08X%08X   EXTWRITE %08X%08X\n"
        "  CSR      %08X%08X\n",
        (unsigned)(P>>32),(unsigned)P, (unsigned)(S>>32),(unsigned)S,
        (unsigned)(F1>>32),(unsigned)F1, (unsigned)(F2>>32),(unsigned)F2,
        (unsigned)(EB>>32),(unsigned)EB, (unsigned)(ED>>32),(unsigned)ED,
        (unsigned)(EW>>32),(unsigned)EW, (unsigned)(C>>32),(unsigned)C);
      write_all_fd(fd, b, k);
      /* DISPLAY1 e DISPLAY2 abertos campo a campo: sao os decisivos.
         MAGH/MAGV = magnificacao; DW/DH = area exibida. E aqui que o GSM mexe. */
      k = snprintf(b, sizeof(b),
        "  DISPLAY1 %08X%08X  DX=%u DY=%u MAGH=%u MAGV=%u DW=%u DH=%u\n",
        (unsigned)(D1>>32),(unsigned)D1,
        (unsigned)(D1 & 0xFFF), (unsigned)((D1>>12) & 0x7FF),
        (unsigned)((D1>>23) & 0xF), (unsigned)((D1>>27) & 0x3),
        (unsigned)((D1>>32) & 0xFFF), (unsigned)((D1>>44) & 0x7FF));
      write_all_fd(fd, b, k);
      k = snprintf(b, sizeof(b),
        "  DISPLAY2 %08X%08X  DX=%u DY=%u MAGH=%u MAGV=%u DW=%u DH=%u\n",
        (unsigned)(D2>>32),(unsigned)D2,
        (unsigned)(D2 & 0xFFF), (unsigned)((D2>>12) & 0x7FF),
        (unsigned)((D2>>23) & 0xF), (unsigned)((D2>>27) & 0x3),
        (unsigned)((D2>>32) & 0xFFF), (unsigned)((D2>>44) & 0x7FF));
      write_all_fd(fd, b, k);
      k = snprintf(b, sizeof(b),
        "  -> largura exibida = (DW+1)/(MAGH+1) px de framebuffer\n"
        "  -> altura  exibida = (DH+1)/(MAGV+1) linhas de framebuffer\n");
      write_all_fd(fd, b, k);
    }

    k = snprintf(b, sizeof(b),
        "\n[PROPORCAO] modo %s | fator vy %.6f | offset %.1f | scissor %d..%d\n"
        "  640x448 em TV %s -> PAR %.6f -> altura para 16:9 = %.2f linhas\n"
        "\n[MENU] selection %d placeholder %d cam_y %.3f tick %d\n",
        g_aspect_169 ? "16:9" : "4:3", (double)VY_FATOR, (double)VY_OFFSET,
        SCISSOR_TOPO, SCISSOR_BASE,
        g_aspect_169 ? "16:9" : "4:3",
        g_aspect_169 ? 1.244444 : 0.933333,
        g_aspect_169 ? 448.0 : 336.0,
        e->sel, e->ph, e->camy, e->tick);
    write_all_fd(fd, b, k);

    k = snprintf(b, sizeof(b),
        "\n[TUTORIAL] cena pronta %d status %d | audio %d\n"
        "  packs: bf %u bytes quadro %d | gf %u bytes quadro %d\n"
        "  canto: bf %d (em %u) gf %d (em %u)\n"
        "  zoom %.4f bop %.5f scroll %.4f\n"
        "  chart: status %d notas %u eventos %u cursor %u vistos %u\n",
        g_tutorial_scene_ready, g_tutorial_scene_status, g_tutorial_audio_status,
        g_tutorial_bf_pack_bytes, g_tutorial_bf_uploaded,
        g_tutorial_gf_pack_bytes, g_tutorial_gf_uploaded,
        g_bf_sing, g_bf_sing_at, g_gf_sing, g_gf_sing_at,
        g_tut_zoom, g_tut_bop, g_tut_scroll,
        g_chart_status, g_chart_info.note_count,
        g_chart_info.event_count, g_chart_cursor,
        g_chart_seen);
    write_all_fd(fd, b, k);

    k = snprintf(b, sizeof(b),
        "\n[FAIXA] visivel %d..%d | recortados %u retangulo(s), %u linha(s) tiradas"
        " | descartados %u inteiramente fora\n",
        SCISSOR_TOPO, SCISSOR_BASE,
        g_band_clip_rects, g_band_clip_lines, g_band_cull_rects);
    write_all_fd(fd, b, k);

    k = snprintf(b, sizeof(b),
        "\n[ENTRADA] held 0x%04X pressed 0x%04X\n"
        "\n[DESENHO] ultima lista: %d sprites, quads texturizados no quadro %d\n",
        (unsigned)e->held, (unsigned)e->pressed, g_dl_n, g_frame_texture_quads);
    write_all_fd(fd, b, k);
    for (i = 0; i < g_dl_n && i < 32; i++) {
        dl_item_t *d = &g_dl[i];
        k = snprintf(b, sizeof(b),
            "  %2d tex %8u tbw %3u %4dx%-4d f%d w%d  v (%.1f,%.1f)-(%.1f,%.1f)"
            "  uv (%.1f,%.1f) %.0fx%.0f\n",
            i, d->tex, d->tbw, (int)d->tw, (int)d->th, (int)d->filtro,
            (int)d->wms, d->vx0, d->vy0, d->vx1, d->vy1,
            d->u, d->v, d->uw, d->vh);
        write_all_fd(fd, b, k);
    }
    fsync(fd);
    close(fd);
    capture_status("ESTADO", g_dl_n, 0);
}


int main(int argc,char**argv){framebuffer_t fb[2];zbuffer_t z;texbuffer_t tex,tex2,fonttex,gftex,stagetex;packet_t* packet;int draw=1,running=1,anim=0,deferred_menu_start=1,last=-1,last2=-1,transform_mode=0,components_mode=0,ui_state=0,target_state=-1,transition_frame=0,selection=0,settings_sel=0,toggle_value=1,slider_value=7,tab_value=0,repeat_dir=0,repeat_frames=0,transitions=0,audio_ok=0,audio_playing=1,audio_started=0,audio_volume=70,voices_enabled=1,audio_fd=-1,title_logo_last=-1,title_gf_last=-1,title_intro_active=1,title_confirm_frames=0,title_ng_restored=0,title_intro_flash_frames=0,menu_placeholder=0,menu_anim_tick=-1,story_diff=1,story_level=0,menu_back_transition=0,story_assets_ready=0,story_diff_tween=5;float menu_cam_y=40.0f;u32 frame_no=0,visual_frame=0,anim_start=0,audio_submitted=0,music_clock=0,audio_read_bytes=0,audio_loops=0,audio_read_errors=0,audio_underruns=0;u64 music_clock_tick=0,music_clock_fraction=0;int latency_samples=0,clock_hold=0,transport_state=1;u32 chart_event_cursor=0;
 reset_iop();g_telemetry_init_result=telemetry_init(FNF_PS2_VERSION,argc?argv[0]:"unknown");capture_status("META_BEGIN",-1,0);g_capture_raw_fd=open("mass:/FNFPS2/FNF_SCREEN_0000.rgba.txt",O_WRONLY|O_CREAT|O_TRUNC,0666);
/* O log NAO eh truncado aqui. telemetry_init() jah o abriu com O_APPEND e jah
   escreveu o cabecalho; reabrir com O_TRUNC apagava esse cabecalho e deixava
   um segundo escritor brigando pelo mesmo arquivo. O cabecalho do log unico
   sai do proprio telemetry_init. */
if(g_capture_raw_fd>=0){fsync(g_capture_raw_fd);}
{int lm=load_metadata();capture_status("META_DONE",-1,lm);boot_mark("TELEMETRIA_RESULTADO",g_telemetry_init_result);if(lm<0)SleepThread();}{int ip=init_pad();capture_status("PAD_DONE",-1,ip);if(ip<0)SleepThread();}g_menu_pixels=g_pixels;capture_status("CACHE_ALLOC_BEGIN",-1,0);g_menu_compressed=memalign(128,PRELOAD_RLE_BYTES);g_menu_sfx_pixels=memalign(128,281088);if(!g_menu_compressed||!g_menu_sfx_pixels){capture_status("CACHE_ALLOC_FAIL",-1,-1);SleepThread();}capture_status("CACHE_READ_BEGIN",-1,0);if(read_exact_path("mass:/FNFPS2/assets/menu_cache.rle",g_menu_compressed,PRELOAD_RLE_BYTES)<0){capture_status("CACHE_RLE_FAIL",-1,-2);SleepThread();}telemetry_event("rle_size",(int)PRELOAD_RLE_BYTES,(int)MENU_RLE_BYTES);telemetry_event("rle_head",(int)(((u32)g_menu_compressed[0]<<24)|((u32)g_menu_compressed[1]<<16)|((u32)g_menu_compressed[2]<<8)|(u32)g_menu_compressed[3]),(int)(((u32)g_menu_compressed[MENU_RLE_BYTES]<<24)|((u32)g_menu_compressed[MENU_RLE_BYTES+1]<<16)|((u32)g_menu_compressed[MENU_RLE_BYTES+2]<<8)|(u32)g_menu_compressed[MENU_RLE_BYTES+3]));{int rfd=open("mass:/FNFPS2/assets/menu_cache.rle",O_RDONLY);if(rfd>=0){unsigned char rb[16];int rn=read(rfd,rb,16);close(rfd);telemetry_event("rle_reread",rn,rn>=4?(int)(((u32)rb[0]<<24)|((u32)rb[1]<<16)|((u32)rb[2]<<8)|(u32)rb[3]):-999);}else telemetry_event("rle_reread",-1,-998);}if(read_exact_path("mass:/FNFPS2/assets/menu_sfx.pcm",g_menu_sfx_pixels,281088)<0){capture_status("CACHE_SFX_FAIL",-1,-3);SleepThread();}capture_status("AUDIO_INIT_BEGIN",-1,0);audio_ok=(audio_init()==0);g_audio_ok=audio_ok;g_audio_ready=audio_ok;capture_status("AUDIO_INIT_DONE",-1,audio_ok?0:-1);if(!audio_ok)audio_playing=0;else audio_playing=0;transport_state=0;clock_hold=1;
 dma_channel_initialize(DMA_CHANNEL_GIF,NULL,0);dma_channel_fast_waits(DMA_CHANNEL_GIF);graph_vram_clear();
 fb[0].width=SCREEN_W;fb[0].height=SCREEN_H;fb[0].psm=FB_PSM;fb[0].mask=0;fb[0].address=graph_vram_allocate(SCREEN_W,SCREEN_H,FB_PSM,GRAPH_ALIGN_PAGE);fb[1]=fb[0];fb[1].address=graph_vram_allocate(SCREEN_W,SCREEN_H,FB_PSM,GRAPH_ALIGN_PAGE);
 z.enable=DRAW_DISABLE;z.address=0;z.mask=0;z.zsm=0;z.method=0;
 /* A `tex` (320x256 PSMCT32) NAO e' mais alocada aqui: ela e' a chave
    SP_CHAVE_TELA_BASE+3 do pool, pedida logo abaixo, DEPOIS do sp_init. Ela
    serve ao MENU (logo do titulo, titulos do Story, itens do menu) e fica FIXA
    fora da musica; ao entrar na musica ela e' liberada e devolve 327.680 B para
    os props, os tres personagens e os icones do HUD. Antes ela era fixa e o BF
    morava dentro dela -- 124.740 B parados na cena em que falta VRAM. */
 tex.width=320;tex.psm=GS_PSM_32;
 tex2=tex;tex2.address=graph_vram_allocate(320,256,GS_PSM_32,GRAPH_ALIGN_BLOCK);
 /* A fonte nunca foi maior que FONT_W x FONT_H (384x192): as 64 linhas a mais
    da alocacao antiga (384x256) eram 98.304 bytes parados na VRAM. O mapa
    completo, com teste, esta' em engine/vram_layout.c. */
 fonttex.width=FONT_W;fonttex.psm=GS_PSM_32;fonttex.address=graph_vram_allocate(FONT_W,FONT_H,GS_PSM_32,GRAPH_ALIGN_BLOCK);
 /* gftex16 (320x64 PSMCT16, 40.960 B) e' a GF no gameplay: fica FORA do pool,
    fixa, e por isso e' alocada aqui, antes de o pool comecar -- se viesse
    depois, o graph_vram_allocate a poria exatamente no inicio do pool e as
    duas coisas se sobreporiam. O motivo de ela nao compartilhar o endereco da
    gftex continua valendo: um upload PSMCT16 sobre dados PSMCT32 reinterpreta
    os bytes e o MERCH ganha uma linha tracejada. */
 g_gftex16=gftex;g_gftex16.width=320;g_gftex16.psm=GS_PSM_16;
 g_gftex16.address=graph_vram_allocate(320,64,GS_PSM_16,GRAPH_ALIGN_BLOCK);
 /* Os dois slots que o BF e a GF ja' ocupavam, visiveis para a carga dos
    personagens (que e' uma funcao de arquivo, e nao enxerga estas locais). */
 g_slot_bf=&tex;g_slot_gf=&gftex;
 /* ===== POOL DE TEXTURAS (engine/stage_pool.c) =====
    A ordem das alocacoes acima E' o mapa de engine/vram_layout.c: os quatro
    slots que o gameplay liga sempre (tex, tex2, fonttex, gftex16) vem
    primeiro, e TODO o resto da VRAM vira o pool -- que o graph_vram_allocate
    nao conhece, porque quem aloca dentro dele e' o stage_pool.

    Antes, gftex (262.144 B) e stagetex (524.288 B) eram reservas fixas fora do
    pool, e sobravam 1.269.760 B -- 15 dos 21 palcos nao cabiam. Dentro do pool
    o orcamento e' 2.056.192 B (test/test_stage_pool.c prova que os 21 cabem, o
    pior com 355.840 B de sobra).

    Nesta build gftex e stagetex entram no pool FIXAS: os enderecos mudam de
    lugar, mas continuam imutaveis durante a sessao, entao nada do que desenha
    hoje precisa saber que o pool existe. O passo seguinte (palco generico) e'
    que passa a pedir os props ao mesmo pool, e ai' as duas viram despejaveis. */
 sp_init(&g_pool, vl_pool_base(), vl_pool_size_bytes());
 {unsigned int a=0;
  /* A `tex` do menu entra no pool como chave FIXA (ver vram_layout.h). */
  if(sp_chave(&g_pool,SP_CHAVE_TELA_BASE+3u,320u*256u*4u,&a)<0)a=0u;
  tex.address=a;
  sp_fixar(&g_pool,SP_CHAVE_TELA_BASE+3u);
  gftex.width=256;gftex.psm=GS_PSM_32;
  if(sp_chave(&g_pool,SP_CHAVE_TELA_BASE+0u,256u*256u*4u,&a)<0)a=0u;
  gftex.address=a;
  stagetex.width=512;stagetex.psm=GS_PSM_32;
  if(sp_chave(&g_pool,SP_CHAVE_TELA_BASE+1u,512u*256u*4u,&a)<0)a=0u;
  stagetex.address=a;
  sp_fixar(&g_pool,SP_CHAVE_TELA_BASE+0u);
  sp_fixar(&g_pool,SP_CHAVE_TELA_BASE+1u);}
 /* O mapa calculado em engine/vram_layout.c tem de dar os MESMOS enderecos que
    o alocador do ps2sdk. Se divergir, o mapa do projeto (e a conta de quanto
    sobra de VRAM) esta' errado -- e isso vale no log, nao no papel. */
 {int bad=0;
  if((unsigned)fb[0].address!=vl_address(VL_FB0))bad++;
  if((unsigned)fb[1].address!=vl_address(VL_FB1))bad++;
  if((unsigned)tex2.address!=vl_address(VL_TEX2))bad++;
  if((unsigned)fonttex.address!=vl_address(VL_FONTTEX))bad++;
  if((unsigned)g_gftex16.address!=vl_address(VL_GFTEX16))bad++;
  /* O pool comeca onde o alocador do ps2sdk parou. Se divergir, o pool esta'
     por cima de uma textura fixa. */
  if(g_pool.base!=vl_pool_base())bad++;
  telemetry_event("vram_mapa",bad,(int)vl_free_words());
  telemetry_event("pool",(int)g_pool.base,(int)g_pool.tamanho);
  /* Onde gftex e stagetex cairam dentro do pool, e quanto sobra. Sem isto, um
     pool mal iniciado so' apareceria como textura borrada no console. */
  telemetry_event("pool_tela",(int)gftex.address,(int)stagetex.address);
  telemetry_event("pool_livre",(int)sp_livre(&g_pool),(int)g_pool.cache.despejos);
  {   /* Uma linha por slot, com calculado e alocado lado a lado. Uma escrita
         so', no boot: nada disso entra no laco de quadro. */
      char vb[1024]; int off=0; unsigned int si; u32 real_end[VL_SLOTS];
      real_end[VL_FB0]=(u32)fb[0].address; real_end[VL_FB1]=(u32)fb[1].address;
      real_end[VL_TEX2]=(u32)tex2.address;
      real_end[VL_FONTTEX]=(u32)fonttex.address; real_end[VL_GFTEX16]=(u32)g_gftex16.address;
      for(si=0;si<vl_slot_count();si++){
        const vl_slot_t*sl=&vl_slots()[si];
        off+=snprintf(vb+off,sizeof(vb)-(size_t)off,
          "VRAM %-8s %4ux%-4u psm %2u calculado %8u alocado %8u %s\n",
          sl->nome,sl->w,sl->h,sl->psm,vl_address(si),real_end[si],
          real_end[si]==vl_address(si)?"igual":"DIVERGE");
        if(off>=(int)sizeof(vb)-160)break;
      }
      telemetry_raw(vb);}
  telemetry_event("vram_livre",(int)(vl_free_words()*4u),(int)(vl_used_words()*4u));}
 telemetry_event("fb0",(int)fb[0].address,(SCREEN_W<<16)|SCREEN_H);telemetry_event("fb1",(int)fb[1].address,(SCREEN_W<<16)|SCREEN_H);telemetry_event("tex",(int)tex.address,(320<<16)|256);telemetry_event("tex2",(int)tex2.address,(320<<16)|256);telemetry_event("gftex",(int)gftex.address,(256<<16)|256);telemetry_event("stagetex",(int)stagetex.address,(512<<16)|256);telemetry_event("fonttex",(int)fonttex.address,(384<<16)|256);telemetry_event("tex1_fix",LOD_MIPMAP_REGISTER,0);telemetry_event("stage_src",g_official_mainstage_props[0].z,g_official_mainstage_chars[2].z);telemetry_event("stage_zoom",(int)(g_official_mainstage_camera_zoom*1000.0f),(int)g_official_mainstage_chars[0].x);telemetry_event("idle_blend",1,2);telemetry_event("merch_trim",59,3);capture_status("GS_BEGIN",-1,0);graph_initialize(fb[0].address,SCREEN_W,SCREEN_H,FB_PSM,0,0);graph_set_bgcolor(0,0,0);packet=packet_init(4096,PACKET_NORMAL);setup_texture(&tex);setup_texture_dims(&tex2,320,256);setup_texture_dims(&gftex,g_gf.width,g_gf.height);setup_texture_dims(&stagetex,512,256);setup_texture_dims(&fonttex,FONT_W,FONT_H);capture_status("GS_ENV_READY",-1,0);if(read_exact_path("mass:/FNFPS2/assets/title_alphabet.rgba",g_font_pixels,384*192*4)==0){setup_texture_dims(&fonttex,384,192);upload_raw(&fonttex,g_font_pixels,384,192);memcpy(g_menu_pixels+TITLE_SNAPSHOT_ALPHA,g_font_pixels,384*192*4);capture_status("TITLE_ALPHA_READY",-1,0);}if(read_exact_path("mass:/FNFPS2/assets/title_prompt_l.rgba",g_stage_pixels,512*192*4)==0){setup_texture_dims(&stagetex,512,192);upload_raw(&stagetex,g_stage_pixels,512,192);memcpy(g_menu_pixels+TITLE_SNAPSHOT_PROMPT_L,g_stage_pixels,512*192*4);capture_status("TITLE_PROMPT_L_READY",-1,0);}if(read_exact_path("mass:/FNFPS2/assets/title_prompt_r.rgba",g_hud_pixels,320*192*4)==0){setup_texture_dims(&tex2,320,192);upload_raw(&tex2,g_hud_pixels,320,192);memcpy(g_menu_pixels+TITLE_SNAPSHOT_PROMPT_R,g_hud_pixels,320*192*4);capture_status("TITLE_PROMPT_R_READY",-1,0);}if(read_exact_path("mass:/FNFPS2/assets/title_logoBumpin.frames",g_gf_pixels,TITLE_LOGO_FRAME_BYTES)==0&&read_exact_path("mass:/FNFPS2/assets/title_gfDanceTitle.frames",g_gf_pixels+TITLE_GF_STORE_OFFSET,TITLE_GF_FRAME_BYTES)==0){setup_texture_dims(&tex,TITLE_LOGO_W,TITLE_LOGO_H);gftex.width=256;setup_texture_dims(&gftex,TITLE_GF_W,TITLE_GF_H);upload_raw(&tex,g_gf_pixels,TITLE_LOGO_W,TITLE_LOGO_H);menu_anim_tick=-1;upload_raw(&gftex,g_gf_pixels+TITLE_GF_STORE_OFFSET,TITLE_GF_W,TITLE_GF_H);menu_anim_tick=-1;capture_status("TITLE_LOGO_GF_READY",-1,0);}if(read_exact_path("mass:/FNFPS2/assets/title_newgrounds.rgba",g_gf_pixels+TITLE_NG_STORE_OFFSET,181*175*4)==0){setup_texture_dims(&tex,181,175);upload_raw(&tex,g_gf_pixels+TITLE_NG_STORE_OFFSET,181,175);menu_anim_tick=-1;capture_status("TITLE_NG_READY",-1,0);} {int anim_rc=title_animation_cache_start();telemetry_event("title_anim_cache",anim_rc==0,anim_rc);telemetry_event("title_cache_policy",TITLE_CACHE_READ_CHUNK,TITLE_CACHE_AUDIO_RESERVE);}capture_status("FIRST_FRAME_PENDING",-1,0);{packet_t*tp=packet_init(8,PACKET_NORMAL);qword_t*tq=tp->data;tq->dw[0]=GIF_SET_TAG(1,1,0,0,GIF_FLG_PACKED,1);tq->dw[1]=GIF_REG_AD;tq++;tq->dw[0]=((u64)0x80<<32);tq->dw[1]=GS_REG_TEXA;tq++;dma_channel_send_normal(DMA_CHANNEL_GIF,tp->data,tq-tp->data,0,0);dma_wait_fast();packet_free(tp);telemetry_event("texa",0x80,0);}/* RAM propria do jogo no boot: a soma dos buffers que ele aloca. E' o numero
 que tools/check_stage_budget.py usa para saber quanto sobra para o pacote do
 palco -- sem ele o orcamento de RAM seria um chute. */
 {unsigned int rb=ram_buffers_total();telemetry_event("ram_buffers",(int)rb,(int)(rb/1024u));}
 capture_status("F0_BLEND_BEGIN",-1,0);draw_enable_blending();capture_status("F0_BLEND_DONE",-1,0);
/* O cao de guarda NAO nasce aqui: durante o boot (6,5 s de espera de
    dispositivo) o contador de quadros ainda esta em zero, ele contaria isso
    como "parado" e gastaria os tres despejos antes de o jogo comecar --
    exatamente quando ele deixa de servir. Ele nasce depois do primeiro quadro
    completo, em fnwd_start(), chamado no fim do laco. */
 while(running){u64 begin,after_draw,after_vsync;unsigned short pressed;if(frame_no==0){capture_status("F0_LOOP_ENTER",-1,0);}begin=GetTimerSystemTime();g_frame_texture_quads=0;band_counters_reset();g_dl_n=0;g_dl_overflow=0;
  /* Um marco por quadro nos primeiros 300 quadros (5 segundos). Sem isto o
     BOOT.txt parava no ultimo marco de frame_no==0 e eu nao tinha como saber
     se o jogo tinha chegado ao quadro 1, ao 3 ou tinha pendurado no meio.
     frame_no<300 porque depois disso o custo de abrir/escrever/sincronizar um
     arquivo por quadro nao se justifica -- e o boot_mark() para sozinho. */
  g_fn_frame=frame_no;
  if(frame_no<300u){capture_status("FRAME",(int)frame_no,ui_state);}
  if(g_congelar){g_congelar=0;fnx_congelado2(-1,frame_no,ui_state,selection,menu_cam_y,g_held);g_cong_frames=180;}if(g_cong_frames>0){g_cong_frames--;graph_wait_vsync();continue;}if(frame_no==0){capture_status("F0_TIMER_DONE",-1,0);}if(frame_no==0){capture_status("F0_PAD_READ_BEGIN",-1,0);}pressed=buttons_pressed();frame_mark("F.IN",(int)pressed);
#ifdef PS2H
  if(frame_no==0){int wk=ps2h_semana_inicial();if(wk>=0){const sr_level_t*lvs=sr_level(wk);if(lvs&&lvs->song_count){int d=ps2h_dificuldade_inicial();if(d>=0&&d<3)story_diff=d;g_story_mode=1;g_story_week=wk;g_fp_active=0;g_fp_play_song=0;g_playlist_n=(int)lvs->song_count;g_playlist_i=0;campanha_zerar();g_play_level=(unsigned)wk;g_play_song_index=0;g_practice_mode=0;fnf_play_set_practice(&g_play,0);target_state=8;transition_frame=1;transitions++;telemetry_event("harness_start",wk,g_playlist_n);}}}
  if(frame_no==0){const char*sid=ps2h_musica_inicial();if(sid){const sr_song_t*ss=sr_song_by_id(sid);if(ss){int d=ps2h_dificuldade_inicial();g_fp_play_song=ss;g_fp_active=1;if(d>=0&&d<3)story_diff=d;g_practice_mode=0;fnf_play_set_practice(&g_play,0);target_state=8;transition_frame=1;transitions++;telemetry_event("harness_start",(int)ss->difficulties,story_diff);}else telemetry_event("harness_start",-1,0);}}
#endif
  if(frame_no==0){capture_status("F0_PAD_READ_DONE",-1,(int)pressed);}g_viewport_360=(ui_state==0||ui_state==6||ui_state==7||ui_state==8||ui_state==9||ui_state==10||ui_state==11);g_ui_state_dbg=ui_state;unsigned local,global;u32 heard,queued_bytes,ring_bytes;qword_t*q;int qw,i;
  if(g_title_anim_thread_id>=0&&g_title_anim_status!=2)g_title_anim_thread_id=-1;
 /* START sozinho encerrava o jogo em qualquer tela fora do Tutorial.
   No oficial START confirma, nunca sai. Sair exige SELECT+START, que
   e deliberado e dificil de apertar sem querer. E START sem SELECT
   passa a valer como CROSS, que e o confirmar do port, para bater
   com o oficial. No Tutorial nao, para nao criar entrada nova no
   meio da musica. */
if((pressed&PAD_START)&&(g_held&PAD_SELECT))running=0;
  /* SELECT+L1 arma o despejo da lista de desenho do PROXIMO quadro.
     E o que permite o simulador no PC reexecutar exatamente o que o
     console desenhou, em vez de eu reconstruir de memoria. */
  if((pressed&PAD_L1)&&(g_held&PAD_SELECT))g_dl_armado=1;
  if((pressed&PAD_L2)&&(g_held&PAD_SELECT)){g_dl_armado=1;g_congelar=1;}
  /* SELECT+TRIANGULO alterna 4:3 e 16:9. Os dois sao geometricamente
     exatos, cada um para a sua TV; so o hardware pode dizer qual e a
     sua. */
  if((pressed&PAD_TRIANGLE)&&(g_held&PAD_SELECT))g_aspect_169^=1;
  /* SELECT+QUADRADO liga e desliga o padrao de calibracao. */
  if((pressed&PAD_SQUARE)&&(g_held&PAD_SELECT))g_calib^=1;
  /* SELECT+R1: despejo de estado completo. Tudo que da para saber da
     maquina num arquivo so. */
  /* SELECT+L1 faz TUDO: lista de desenho, foto do mesmo quadro e
     despejo de estado. SELECT+R1 faz so o despejo, para inspecionar
     sem gastar 1,8 s gravando a foto. */
  if(((pressed&PAD_R1)||(pressed&PAD_L1)||(pressed&PAD_L2))&&(g_held&PAD_SELECT)){
    estado_t ed;
    ed.ui=ui_state;ed.sel=selection;ed.trans=transition_frame;ed.alvo=target_state;
    ed.afd=audio_fd;ed.tfd=g_title_audio_fd;ed.astart=audio_started;
    ed.aplay=audio_playing;ed.avol=audio_volume;ed.voices=voices_enabled;
    ed.ph=menu_placeholder;ed.tick=menu_anim_tick;ed.clock=music_clock;ed.frame=frame_no;
    ed.subm=audio_submitted;ed.camy=menu_cam_y;ed.held=g_held;ed.pressed=pressed;
    ed.fb0=fb[0].address;ed.fb1=fb[1].address;
    ed.tex=&tex;ed.tex2=&tex2;ed.fonttex=&fonttex;ed.gftex=&gftex;
    ed.stagetex=&stagetex;
    estado_dump(&ed);
    fnx_congelado2(-1, frame_no, ui_state, selection, menu_cam_y, g_held);
  }
  /* Este else pertence ao teste de SELECT+START la de cima, nao ao de
     L1. Na 0.2.77 eu inseri a linha do L1 no meio e o encadeamento
     ficou ambiguo; funcionava por acidente. */
  if(!((pressed&PAD_START)&&(g_held&PAD_SELECT))&&(pressed&PAD_START)&&ui_state!=8)
    pressed|=PAD_CROSS;
  if(audio_ok&&(pressed&PAD_R3)&&!(g_held&PAD_SELECT))voices_enabled^=1;
  if(audio_ok&&(g_title_audio_fd>=0||ui_state==8)&&(pressed&PAD_L2)&&audio_volume>=10){audio_volume-=10;audsrv_set_volume(audio_volume);}
  if(audio_ok&&(g_title_audio_fd>=0||ui_state==8)&&(pressed&PAD_R2)&&audio_volume<=90){audio_volume+=10;audsrv_set_volume(audio_volume);}
  if(!components_mode&&(pressed&PAD_L1)&&latency_samples>-4800)latency_samples-=240;
  if(!components_mode&&(pressed&PAD_R1)&&latency_samples<4800)latency_samples+=240;
  if(ui_state==0||ui_state==6||ui_state==7||ui_state==11)transport_state=0;else if(!audio_playing)transport_state=3;else if(audio_started)transport_state=2;
  /* 0.2.116: este laco nao tinha guarda nem saida por erro. audio_play_complete
     desiste em 2000 x 1 ms; se audsrv_play_audio parar de aceitar bytes,
     audsrv_available() nao diminui e o laco NUNCA sai, a ate 2 s por volta --
     "ficou lento e travou" na tela de titulo (o mesmo ui_state do BOOT.txt do
     console). O limite e 64 voltas: 64 x 4 KB = 256 KB, o dobro do anel de
     audio deste jogo, entao o caminho saudavel sai pela mesma condicao de
     antes. Quando o limite ou o erro pegam, fica registrado em AUDIO_STALL. */
  if((ui_state==0||ui_state==6||ui_state==7||ui_state==11)&&!g_menu_audio_suspended&&g_title_audio_fd>=0){int voltas=0;while(audsrv_available()>=AUDIO_READ_CHUNK){int n;int j;if(++voltas>64){frame_mark("A.STOP_LACO",voltas);break;}n=audio_ring_take(g_audio_pcm,AUDIO_READ_CHUNK);if(n>0){if(g_menu_sfx_pos<g_menu_sfx_end){int k,frames=n/4;short*dst=(short*)g_audio_pcm;for(k=0;k<frames&&g_menu_sfx_pos<g_menu_sfx_end;k++){short*fx=(short*)(g_menu_sfx_pixels+g_menu_sfx_pos);int vl=(int)dst[k*2]+(int)fx[0],vr=(int)dst[k*2+1]+(int)fx[1];if(vl>32767)vl=32767;if(vl<-32768)vl=-32768;if(vr>32767)vr=32767;if(vr<-32768)vr=-32768;dst[k*2]=(short)vl;dst[k*2+1]=(short)vr;g_menu_sfx_phase+=147u;if(g_menu_sfx_phase>=320u){g_menu_sfx_phase-=320u;g_menu_sfx_pos+=4u;}}}j=audio_play_complete((char*)g_audio_pcm,n);if(j>0)g_title_audio_submitted+=(u32)(j/4);else{frame_mark("A.STOP_ERRO",j);break;}}else break;} }
  if(ui_state==8&&audio_ok&&audio_fd>=0&&g_tutorial_audio_status==0&&!g_paused){
   /* A musica so' comeca depois da contagem: no original startSong() e'
      chamado quando o Conductor chega em 0 (PlayState.hx:1144-1151). */
   if(g_tutorial_scene_ready&&audio_playing&&!audio_started&&g_countdown_left<=0&&audio_ring_level()>=65536){
    audio_started=1;clock_hold=0;music_clock_tick=begin;music_clock_fraction=0;transport_state=2;
    telemetry_event("tutorial_audio_start",(int)frame_no,(int)audio_ring_level());
   }
   /* Sem exigir `audio_ring_level()>=STEM_READ_CHUNK` aqui: no fim do arquivo
      sobra um resto menor que o bloco, e essa exigencia impedia o mixer de ser
      chamado -- o resto ficava preso no anel para sempre e a musica nunca
      terminava. Quem decide o que ha' para enviar e' o mixer. */
   if(audio_started&&audio_playing&&audsrv_available()>=AUDIO_READ_CHUNK){
    int submitted=tutorial_audio_mix_submit(voices_enabled);
    /* UM BLOCO CURTO NAO PODE MATAR A MUSICA.
       Antes, qualquer valor diferente de AUDIO_READ_CHUNK marcava
       g_tutorial_audio_status=-6, chamava audsrv_stop_audio() e congelava o
       relogio da musica -- para o resto da tentativa, sem retentativa. Um
       bloco curto acontece quando a fila do audsrv enche no meio do envio, o
       que no console e' normal (pendrive a ~1,5 MB/s, anel de 20 KB). O
       sintoma que o usuario descreveu no PS2 -- "a musica comeca, fica muito
       lenta e engasgada, e trava" -- tem exatamente esse formato.
       Agora: bloco curto conta e segue; o transporte so' para depois de
       TUT_AUDIO_FAIL_LIMIT quadros seguidos sem conseguir enviar nada. */
    if(submitted>0){
     audio_submitted+=(u32)(submitted/4);
     if(submitted!=AUDIO_READ_CHUNK){
      if(g_tut_partial_blocks<8u)telemetry_event("tutorial_audio_curto",submitted,(int)frame_no);
      g_tut_partial_blocks++;
     }
     g_tut_audio_fail_streak=0;
    }else{
     g_tut_audio_fail_streak++;
     if(g_tut_audio_fail_streak>=TUT_AUDIO_FAIL_LIMIT){
      g_tut_audio_given_up=1;
      g_tutorial_audio_status=-6;audio_playing=0;audio_started=0;audsrv_stop_audio();
      telemetry_event("tutorial_audio_submit",submitted,(int)frame_no);
     }
    }
   }
  }
  /* AUDIO DO GAME OVER: um transporte proprio (o do nivel esta' parado nesta
     tela desde a morte). O anel pode estar em SILENCIO+sfx ou tocando a
     musica do game over; quem decide e' o audio_ring. Sem pre-buffer grande:
     o sfx tem de comecar no primeiro quadro do substate. */
  if(ui_state==10&&transition_frame==0&&g_audio_ok&&g_go_audio_estado>0){
   int enviado=gameover_audio_bombear();
   if(enviado>0)frame_mark("A.GAMEOVER",enviado);
  }
  /* MUSICA DA PAUSA: transporte proprio, com o fade-in do original. Bombeia
     enquanto a pausa esta' aberta -- inclusive nos quadros em que o menu
     nao aceita entrada (o justOpened), porque o original tambem toca ali. */
  if(ui_state==8&&g_paused&&g_audio_ok&&g_pause_on){
   int enviado=pause_audio_bombear();
   if(enviado>0)frame_mark("A.PAUSA",enviado);
  }
  frame_mark("F.AUDIO",transport_state);
  {u32 reads=0;audio_ring_stats(&audio_read_bytes,&reads,&audio_loops,&audio_read_errors,&audio_underruns);(void)reads;}
  if(frame_no==0){capture_status("F0_UPDATE_AUDIO_DONE",-1,0);}queued_bytes=(audio_ok&&(g_title_audio_fd>=0||audio_fd>=0))?(u32)audsrv_queued():0;ring_bytes=(u32)audio_ring_level();heard=audio_submitted>=queued_bytes/4?audio_submitted-queued_bytes/4:0;
  /* Acumulador continuo: o RPC bruto apenas valida/corrige deriva; nao ancora
   * cada bloco. O resto fracionario evita perder subamostras entre frames. */
  if(clock_hold&&heard>=music_clock)clock_hold=0;
  /* As amostras decorridas neste quadro sobem para fora do ramo do relogio da
     musica porque a CONTAGEM REGRESSIVA tambem precisa de um relogio, e antes
     de a musica comecar nao ha' audio de onde tirar o tempo. O tempo vem do
     vsync, que e' exatamente a base do FlxTimer da contagem no original
     (Countdown.performCountdown). */
  {u64 dt=music_clock_tick?begin-music_clock_tick:0;
   u64 scaled=dt*(u64)AUDIO_RATE+music_clock_fraction;
   u32 advance=(u32)(scaled/(u64)kBUSCLK);
   music_clock_fraction=scaled%(u64)kBUSCLK;
   music_clock_tick=begin;
   if(g_paused){
    /* Pausado: nada avanca. O substate do original para o PlayState inteiro
       (shouldSubstatePause), entao relogio, contagem e musica ficam parados. */
   }else if(ui_state==8&&g_countdown_left>0){
    g_countdown_left-=(s32)advance;
    if(g_countdown_left<0)g_countdown_left=0;
    {s32 st=(s32)(5-(g_countdown_left+(g_countdown_beat-1))/g_countdown_beat);
     if(st!=g_countdown_step){
      g_countdown_step=st;
      telemetry_event("countdown_step",(int)st,(int)frame_no);
      /* O som de cada passo vem do noteStyle no original
         (Countdown.playCountdownSound). Esse asset ainda nao esta' no pacote,
         e o log registra o passo que ficou sem som em vez de fingir que tocou. */
      if(st>=1&&st<=4)telemetry_event("countdown_sound_ausente",(int)st,0);
     }}
   }else if(audio_playing&&audio_started&&!clock_hold){
    if(advance&&music_clock+advance>music_clock)music_clock+=advance;
    if(music_clock>audio_submitted)music_clock=audio_submitted;
    /* Corrige somente deriva grande e gradualmente, sem salto de bloco. */
    if(heard>music_clock+1600u){u32 correction=heard-music_clock-1600u;if(correction>32u)correction=32u;music_clock+=correction;}
   }
  }
  /* Durante a contagem o relogio do chart e' NEGATIVO, como no original: o
     Conductor ja' foi para -5 batidas e as notas do inicio ja' descem. Sem
     isso o campo de notas ficaria congelado 5 batidas e pularia de uma vez. */
  g_chart_clock=(g_countdown_left>0)?-(s32)g_countdown_left:(s32)music_clock;
  /* Gancho do harness (fraco: no console o simbolo nao existe e o if e' falso).
     Ele permite ao roteiro do PC apertar uma tecla quando a MUSICA chega em tal
     amostra, em vez de num quadro fixo -- o relogio da musica anda por tempo, e
     por quadro o aperto erra assim que o tempo por quadro muda. */
  if(ps2h_song_clock_hook)ps2h_song_clock_hook(g_chart_clock<0?0u:(u32)g_chart_clock);
  heard=music_clock;
  frame_mark("F.CLOCK",(int)music_clock);
  if(ui_state==8&&audio_started&&g_chart_status==0){
   const chart_event_t *event;
   while(g_chart_cursor<g_chart_info.event_count){
    event=chart_event(g_chart_cursor);if(!event||event->sample>(u32)g_chart_clock)break;
    /* Every event's meaning now lives in chart_events.c, which reads the FCH4
       fields. The v3 code that was here packed one parameter into `value` and
       dropped the rest, so SetCameraBop, SetHealthIcon and PlayAnimation never
       reached the port at all. */
    chart_events_apply(event);
    g_chart_last_type=event->type;g_chart_last_value=event->value;g_chart_last_sample=event->sample;
    g_chart_cursor++;g_chart_seen++;

    telemetry_event("tutorial_chart_event",(int)event->type,(int)event->sample);
    /* Classificacao do que o port NAO reproduz (ver os contadores la' em cima).
       O aviso sai uma vez por musica e por grupo: o log do console diz que a
       musica tem esses eventos e que o efeito falta por dado, sem repetir a
       mesma linha 89 vezes. */
    if(event->type==CHART_EV_SET_HEALTH_ICON){
     const chart_event_state_t*es=chart_events_state();
     g_ev_icone++;
     if(!(g_ev_avisou&1u)){g_ev_avisou|=1u;telemetry_event("evento_sem_icone",es->icon_char,(int)es->icon_ref);}
    }else if(event->type>=0x0100u){
     g_ev_script++;
     if(!(g_ev_avisou&2u)){g_ev_avisou|=2u;telemetry_event("evento_sem_script",(int)event->type,(int)event->sample);}
    }else if(event->type==CHART_EV_CHANGE_CHARACTER||event->type==CHART_EV_ALT_IDLE_ANIM){
     g_ev_sem_efeito++;
     if(!(g_ev_avisou&4u)){g_ev_avisou|=4u;telemetry_event("evento_sem_efeito",(int)event->type,(int)event->sample);}
    }
   }
  }
  {u32 aref=0;int at=-1,af2=0;
   while(chart_events_take_anim(&aref,&at,&af2)){
    const char*an=chart_event_string(aref);
    /* PlayAnimationSongEvent.hx L41-69: o alvo do evento e' um PERSONAGEM
       nomeado ("bf"/"boyfriend" -> 0, "dad" -> 2, "gf" -> 3; nomes de prop do
       palco para o resto). Os tres primeiros viram `cc_toca`; prop de palco
       ainda nao existe no port e vira log, nao invencao. */
    {
     int papel=-1,tocou=0;
     if(at==0)papel=CH_PAPEL_BF;else if(at==2)papel=CH_PAPEL_DAD;else if(at==3)papel=CH_PAPEL_GF;
     if(papel>=0&&an)tocou=cc_toca(&g_cena,papel,an,(long)(g_chart_clock<0?0:(u32)(g_chart_clock/48)));
     /* O log sai de TODO evento, com ou sem animacao tocada: e' ele que
        denuncia um chart pedindo animacao que o pacote nao tem (o tutorial pede
        'hey' no BF e 'cheer' no dad, e nenhum dos dois existe nos pacotes do
        tutorial). */
     telemetry_event(tocou?"play_anim_tocou":"play_anim_sem_frame",at,(int)((an?(unsigned char)an[0]:0)|(af2<<8)));
    }
   }}
  /* QUADROS DE MUSICA: o denominador do custo por quadro (evento `custo_*` no
     fim da musica). Sem ele, "N uploads" nao diz nada -- 40 uploads em 3 s e'
     caro, em 3 min nao. */
  if(ui_state==8&&audio_started&&!g_paused){g_custo_frames++;if(!(g_custo_frames%CUSTO_CADA))custo_amostra();}
  /* Os icones de vida, medidos DURANTE a musica e nao so' no fim dela.
     Antes o unico `icone_desenho` saia na entrada do resultado; uma musica que
     nao termina dentro dos quadros do harness (as 27 do sweep nao terminam
     iguais) ficava sem prova nenhuma de que o icone foi desenhado -- e o sweep
     reprovava por "quase nao desenhados" quando o problema era a falta do
     evento, nao do desenho. A cada 300 quadros de gameplay (5 s) o numero
     corrente vai para o log; o valor do fim continua sendo o total. */
  if(ui_state==8&&audio_started&&!g_paused&&g_custo_frames>0&&!(g_custo_frames%300))
   telemetry_event("icone_desenho",(int)g_icone_desenhos,
      (g_icone_ativo[ICONE_JOGADOR]?1:0)|(g_icone_ativo[ICONE_OPONENTE]?2:0));
  if(ui_state==8&&audio_started&&!g_paused)tutorial_notes_update((u32)g_chart_clock,pressed,frame_no);
  /* MORTE -> GAME OVER.
     PlayState.hx:1255: `if (health <= Constants.HEALTH_MIN && !isPracticeMode
     && !isPlayerDying)` -> pausa a musica/vozes e chama moveToGameOver(), que
     abre o GameOverSubState SEM transicao (skipNextTransIn/Out = true). O port
     nao tinha game over: a vida chegava a zero, o HUD mostrava a barra vazia e
     o jogo continuava como se nada tivesse acontecido -- dava para terminar a
     musica morto. */
  if(ui_state==8&&g_play.dead&&g_tutorial_scene_ready){
   g_death_counter++;
   telemetry_event("play_death",(int)g_play.missed,(int)frame_no);
   audsrv_stop_audio();audio_playing=0;audio_started=0;transport_state=0;
   clock_hold=1;music_clock_tick=0;music_clock_fraction=0;
   /* moveToGameOver(): zera o placar e devolve a vida inicial, sem tocar nos
      tallies. */
   fnf_play_move_to_game_over(&g_play);
   ui_state=10;target_state=-1;transition_frame=0;transform_mode=0;
   g_gameover_frames=0;g_gameover_drawn=0;
   last=-1;last2=-1;
   /* O ring da musica do nivel para aqui: o game over tem transporte proprio.
      `audio_set_stream` reaplica o formato para LIMPAR o que ainda esta' na
      fila do audsrv -- senao o resto da musica do nivel tocava por cima do
      comeco do sfx. (audsrv_stop_audio so' silencia; nao esvazia a fila.) */
   if(audio_ok&&audio_set_stream(audio_volume)!=0)
    telemetry_event("gameover_stream",-2,(int)frame_no);
   gameover_audio_iniciar();
   telemetry_event("state",10,(int)frame_no);
   telemetry_event("gameover_enter",(int)frame_no,(int)g_chart_clock);
  }
  /* Fim da musica -> resultado, depois de uma pausa curta para o ultimo quadro
     aparecer. O original tambem nao corta na hora: ha' a transicao de saida
     do PlayState antes do ResultState. */
  if(ui_state==8&&g_tutorial_audio_finished&&transition_frame==0){
   g_result_wait++;
   if(g_result_wait==90&&target_state<0){
    fnf_rank_t rk=fnf_play_rank(&g_play);
    telemetry_event("play_result",(int)rk,fnf_play_clear_percent(&g_play));
    telemetry_event("play_result_score",g_play.score,g_play.max_combo);
    telemetry_event("play_result_tallies",(g_play.sick<<24)|(g_play.good<<16)|(g_play.bad<<8)|(g_play.shit&0xff),g_play.missed);
    campanha_somar(&g_play);
    if(g_story_mode&&g_playlist_i+1<g_playlist_n){
     /* Proxima musica da semana, SEM tela de resultado e com o mesmo
        corte de transicao que o original (skipNextTransIn/Out). */
     g_playlist_i++;
     g_play_song_index=(unsigned)g_playlist_i;
     g_result_wait=0;g_result_drawn=0;
     /* PlayState novo: o Practice Mode e' campo dele e nao sobrevive. */
     g_practice_mode=0;fnf_play_set_practice(&g_play,0);
     g_tutorial_scene_attempted=0;g_tutorial_scene_ready=0;
     target_state=8;transition_frame=1;transitions++;
     telemetry_event("story_next",g_playlist_i,(int)g_campaign_score);
    }else{
     target_state=9;transition_frame=1;transitions++;
    }
   }
  }
  if(ui_state==8&&audio_started&&audio_ring_eof()&&ring_bytes==0&&queued_bytes==0){
   audsrv_stop_audio();audio_playing=0;audio_started=0;g_tutorial_audio_finished=1;transport_state=4;
   /* O fim da musica e' o fim do CHART (song_end_sample), nao
      TUTORIAL_AUDIO_SAMPLES, que e' a duracao do Tutorial escrita no codigo.
      Para outra musica aquela constante seria o tempo errado, e o resultado
      sairia com o relogio de outra faixa. */
   music_clock=g_chart_info.song_end_sample?g_chart_info.song_end_sample:TUTORIAL_AUDIO_SAMPLES;
   heard=music_clock;
   telemetry_event("tutorial_audio_end",(int)music_clock,(int)frame_no);
   custo_emite();
  }
  /* VIDA INTERPOLADA E ICONES DE VIDA (HealthIcon).
     O original atualiza a vida da barra por quadro com
     `healthLerp = FlxMath.lerp(healthLerp, health, 0.15)` (PlayState.hx
     L2767-2779) e os icones usam `healthBar.value` (= healthLerp) para a
     POSICAO e `health` para o ESTADO (HealthIcon.hx L305, L325). Como o lerp do
     original nao usa o tempo decorrido, ele e' por QUADRO -- copiar com dt
     daria a velocidade errada.
     O pulo do icone roda a cada passo que casa com bopEvery (4). */
  if(ui_state==8){
   float alvo=g_play.health;
   if(alvo<0.0f)alvo=0.0f;
   if(alvo>2.0f)alvo=2.0f;
   g_vida_lerp+=(alvo-g_vida_lerp)*0.15f;
   /* A vida no chao: no original isso e' Game Over, MENOS no Practice Mode
      (`PlayState.hx` L1255), onde o jogo segue. `g_play.dead` diz qual dos dois
      caminhos o engine tomou -- e e' o que este evento publica. */
   if(g_play.health<=FNF_HEALTH_MIN&&!g_vida_zero_visto){
    g_vida_zero_visto=1;
    telemetry_event("vida_zero",(int)g_play.dead,(int)frame_no);
   }
   icone_atualiza(alvo);
   icone_passo(cena_passo());
   {int _l;for(_l=0;_l<ICONE_LADOS;_l++)hi_bop_tick(&g_icone_bop[_l],TUT_DT);}
  }
  /* Tutorial note timing follows the submitted/played audio sample clock. */
  

  if(ui_state==6&&transition_frame>0)menu_cam_y+=(40.0f+160.0f*(float)selection-menu_cam_y)*0.06f;
  frame_mark("F.TITLE",ui_state);
  if(frame_no==0){capture_status("F0_INPUT_STATE_DONE",-1,0);}if(ui_state==0&&title_intro_active&&!title_ng_restored){if(title_intro_phase()>=8u){setup_texture_dims(&tex,TITLE_LOGO_W,TITLE_LOGO_H);upload_raw(&tex,g_gf_pixels,TITLE_LOGO_W,TITLE_LOGO_H);menu_anim_tick=-1;title_ng_restored=1;}}
  if((ui_state==0||ui_state==6||ui_state==7)&&g_gf_reload_active==1){u32 left=g_gf_reload_total-g_gf_reload_offset,want=left>32768u?32768u:left;int got=read(g_gf_reload_fd,g_gf_pixels+g_gf_reload_offset,(int)want);if(got<=0){close(g_gf_reload_fd);g_gf_reload_fd=-1;g_gf_reload_active=3;}else{g_gf_reload_offset+=(u32)got;if(g_gf_reload_offset==g_gf_reload_total){close(g_gf_reload_fd);g_gf_reload_fd=-1;g_gf_reload_active=2;FlushCache(0);}}}
  if(ui_state==0&&title_confirm_frames>0){title_confirm_frames--;if(title_confirm_frames==0){target_state=6;transition_frame=1;transitions++;}}
  /* Incremental menu ownership: begin as soon as the fade starts, process at
   * most 64 KiB output per video frame, and hold only at the opaque midpoint. */
  if(target_state==6&&transition_frame>0&&!g_menu_cache_ready&&!g_menu_decode_active&&!g_menu_decode_error)menu_decode_begin();
  if(g_menu_decode_active){frame_mark("F.DECODE",(int)g_menu_decode_di);menu_decode_step(65536u);}
  /* Stop menu audio before blocking title-bank I/O so the SPU cannot hold a
   * single sample while the screen is opaque. */
  /* Story resources are restored while the fade is opaque. Stop the previous
   * stream first so blocking RLE/upload work cannot produce a held note after
   * Story becomes visible. */
  if(target_state==7&&transition_frame==(menu_back_transition?42:15)&&!story_assets_ready){telemetry_event("story_transicao_inicio",transition_frame,menu_back_transition);if(ui_state==8||ui_state==10){telemetry_event("back_audsrv_begin",0,0);audsrv_stop_audio();telemetry_event("back_audsrv_done",0,0);audio_ring_stop();telemetry_event("back_ring_done",0,0);if(audio_fd>=0){close(audio_fd);audio_fd=-1;}telemetry_event("back_fd_closed",0,0);audio_started=0;audio_playing=0;audio_submitted=music_clock=0;music_clock_tick=music_clock_fraction=0;clock_hold=1;transport_state=0;tutorial_packs_free();palco_soltar();telemetry_event("back_packs_freed",0,0);telemetry_event("tutorial_back_transport",0,(int)frame_no);}int su,st,sf,tb=1;if(ui_state==1){if(g_title_audio_fd>=0){close(g_title_audio_fd);g_title_audio_fd=-1;}audsrv_stop_audio();tb=(read_exact_path("mass:/FNFPS2/assets/title_logoBumpin.frames",g_gf_pixels,TITLE_LOGO_FRAME_BYTES)==0&&read_exact_path("mass:/FNFPS2/assets/title_gfDanceTitle.frames",g_gf_pixels+TITLE_GF_STORE_OFFSET,TITLE_GF_FRAME_BYTES)==0&&read_exact_path("mass:/FNFPS2/assets/title_alphabet.rgba",g_menu_pixels+TITLE_SNAPSHOT_ALPHA,384*192*4)==0&&read_exact_path("mass:/FNFPS2/assets/title_prompt_l.rgba",g_menu_pixels+TITLE_SNAPSHOT_PROMPT_L,512*192*4)==0&&read_exact_path("mass:/FNFPS2/assets/title_prompt_r.rgba",g_menu_pixels+TITLE_SNAPSHOT_PROMPT_R,320*192*4)==0);telemetry_event("title_resident",tb,37);}telemetry_event("back_packs_begin",0,0);{int sr=story_packs_load(&tex2,&gftex);menu_anim_tick=-1;telemetry_event("back_packs_done",sr,0);if(sr)capture_status("BACK_STORY_PACKS_FAIL",-1,sr);g_story_static_ready=(sr==0);telemetry_event("story_anim_load",g_story_static_ready,sr);}telemetry_event("back_ui_begin",0,0);su=story_ui_restore();telemetry_event("back_ui_done",su,0);if(su)capture_status("BACK_UI_FAIL",-1,su);if(su==0){setup_texture_dims(&stagetex,512,256);upload_raw(&stagetex,g_stage_pixels,512,256);}telemetry_event("back_titles_begin",0,0);st=story_titles_restore();telemetry_event("back_titles_done",st,0);if(st)capture_status("BACK_TITLES_FAIL",-1,st);if(st==0){story_upload_title(&tex,0);telemetry_event("tex_story",(int)tex.address,0);}sf=story_font_alpha_decode(g_menu_sfx_pixels+241920u+22656u,7672u,g_font_pixels,160u*120u);if(sf==0){u32 fp;unsigned int amin=255,amax=0,nmin=128,nmax=0;for(fp=0;fp<160u*120u;fp++){unsigned int a=g_font_pixels[fp*4u+3],na=(a*128u+127u)/255u;if(a<amin)amin=a;if(a>amax)amax=a;g_font_pixels[fp*4u+3]=(unsigned char)na;if(na<nmin)nmin=na;if(na>nmax)nmax=na;}FlushCache(0);setup_texture_dims(&fonttex,160,120);upload_raw(&fonttex,g_font_pixels,160,120);telemetry_event("story_alpha_src",(int)amin,(int)amax);telemetry_event("story_alpha_gs",(int)nmin,(int)nmax);telemetry_event("story_vtx_alpha",90,128);telemetry_event("story_gs_contract",(TEXTURE_COMPONENTS_RGBA<<16)|TEXTURE_FUNCTION_MODULATE,(1<<16)|0x5A);}story_assets_ready=(su==0&&st==0&&sf==0&&tb);if(!story_assets_ready)capture_status("BACK_ASSETS_FAIL",(su&255)|((st&255)<<8)|((sf&255)<<16),0);telemetry_event("story_assets",story_assets_ready,(su&255)|((st&255)<<8)|((sf&255)<<16));}
  if(target_state==8&&transition_frame==15&&!g_tutorial_scene_attempted){
  int fr,sr,chart_rc,format_rc=-99;
  capture_status("TUT_PREP_BEGIN",(int)frame_no,ui_state);
  story_packs_free();
  audsrv_stop_audio();audio_ring_stop();
  if(g_title_audio_fd>=0){close(g_title_audio_fd);g_title_audio_fd=-1;}
  if(audio_fd>=0){close(audio_fd);audio_fd=-1;}
  g_menu_audio_suspended=1;g_title_audio_submitted=0;story_assets_ready=0;
  audio_started=0;audio_playing=0;audio_submitted=0;music_clock=0;
  music_clock_tick=music_clock_fraction=0;clock_hold=1;transport_state=0;
  g_tutorial_audio_finished=0;g_tutorial_audio_status=-1;
  g_vida_zero_visto=0;
  g_countdown_left=0;g_countdown_step=0;g_chart_clock=0;
  capture_status("TUT_FONT_BEGIN",(int)frame_no,0);
  fr=tutorial_vcr_restore();
  capture_status("TUT_FONT_DONE",(int)frame_no,fr);
  if(fr==0){setup_texture_dims(&fonttex,160,120);upload_raw(&fonttex,g_font_pixels,160,120);}
  capture_status("TUT_SCENE_BEGIN",(int)frame_no,0);
  sr=tutorial_scene_load(&stagetex,&tex,&gftex);
  capture_status("TUT_SCENE_DONE",(int)frame_no,sr);
  g_tutorial_note_asset_status=read_exact_path("mass:/FNFPS2/assets/tutorial_hud_official_320x256_gsalpha.rgba",g_hud_pixels,320*256*4);
  capture_status("TUT_NOTE_ATLAS_DONE",(int)frame_no,g_tutorial_note_asset_status);
  telemetry_event("tutorial_note_atlas",g_tutorial_note_asset_status,224*112*4);
  if(g_tutorial_note_asset_status==0){FlushCache(0);setup_texture_dims(&tex2,320,256);upload_raw(&tex2,g_hud_pixels,320,256);}
  /* A BARRA DE VIDA COM A ARTE DE VERDADE.
     O atlas do HUD (que veio pronto do Drive) tem a regiao da barra em
     (0,112) 301x10, e era um retangulo branco sem a moldura do original. A
     arte oficial e' um arquivo a parte -- `shared/images/healthBar.png`,
     601x19 em LA --, e `tools/empacotar_ui.py` reduz 2x (o HUD do port e' 640
     x360; o original roda a 1280x720) e escreve `health_bar.rgba`. Aqui ele e'
     copiado POR CIMA da regiao, ANTES do upload: nenhuma textura nova e
     nenhum endereco de VRAM novo, que e' o que importa com 1.792 B livres no
     pool. Sem o arquivo, o atlas fica como veio e o log diz
     (`health_bar_arte` = 0). */
  {int hb=read_exact_path("mass:/FNFPS2/assets/health_bar.rgba",g_up_bounce,
                          HEALTH_BAR_W*HEALTH_BAR_H*4);
   int yy;
   telemetry_event("health_bar_arte",hb==0,HEALTH_BAR_W*HEALTH_BAR_H*4);
   if(hb==0){
    for(yy=0;yy<HEALTH_BAR_H;yy++)
     memcpy(g_hud_pixels+(((u32)(HEALTH_BAR_Y+yy)*HUD_TEX_W+(u32)HEALTH_BAR_X)*4u),
            g_up_bounce+(u32)yy*HEALTH_BAR_W*4u,(size_t)HEALTH_BAR_W*4u);
    FlushCache(0);
    setup_texture_dims(&tex2,320,256);
    upload_raw(&tex2,g_hud_pixels,320,256);
   }}
  /* NOTE SPLASH: a arte vai para DENTRO do atlas do HUD, na faixa livre abaixo
     da barra de vida (SPLASH_REG_X, SPLASH_REG_Y) -- as mesmas coordenadas que
     tools/empacotar_splash.py usou. O arquivo tem a largura da REGIAO
     (SPLASH_REG_W) e a textura tem SPLASH_TEX_W, entao a copia e' linha a
     linha, como a barra de vida faz.
     Custo de VRAM: ZERO. Sem o arquivo, a splash simplesmente nao aparece e o
     log diz (`splash_arte` = 0) -- nada mais do jogo depende dele. */
  {int sp=read_exact_path("mass:/FNFPS2/assets/splash.rgba",g_up_bounce,
                          SPLASH_REG_W*SPLASH_REG_H*4);
   telemetry_event("splash_arte",sp==0,SPLASH_REG_W*SPLASH_REG_H*4);
   g_splash_arte_ok=0;
   if(sp==0){
    u32 yy;
    for(yy=0;yy<SPLASH_REG_H;yy++)
     memcpy(g_hud_pixels+(((u32)(SPLASH_REG_Y+yy)*HUD_TEX_W+(u32)SPLASH_REG_X)*4u),
            g_up_bounce+(u32)yy*SPLASH_REG_W*4u,(size_t)SPLASH_REG_W*4u);
    FlushCache(0);
    setup_texture_dims(&tex2,320,256);
    upload_raw(&tex2,g_hud_pixels,320,256);
    g_splash_arte_ok=1;
   }}
  memset(&g_chart_info,0,sizeof(g_chart_info));g_chart_cursor=g_chart_seen=0;g_chart_last_type=g_chart_last_value=g_chart_last_sample=0;
  /* O reset das notas NAO pode ficar aqui: ele tambem chama fnf_play_reset()
     com o total de notas pontuaveis, e aqui o chart ainda nao foi carregado --
     note_count ainda e' 0. Isso deixava o total em zero pelo resto da musica,
     e o resultado saia sempre com rank NONE e 0% de conclusao, porque
     fnf_play_completion() devolve 0 quando nao sabe o total. O reset vai
     depois do chart_load, em TUT_CHART_BEGIN. */
  capture_status("TUT_CHART_BEGIN",(int)frame_no,0);
  /* The chart is resolved through the registry, not hardcoded. The tutorial is
     what the port plays, so its entry is looked up by id; the difficulty comes
     from the story menu's choice. sr_chart_path builds
     "<root>/songs/tutorial/<difficulty>.chart", which is where the game keeps
     it -- so the asset packaging has to match, and that is now the registry's
     job rather than a string literal buried in main.c. */
  /* A musica vem do registro, pelo nivel escolhido no Story Menu. Se aquele
     nivel nao existir (ou nao tiver musica), cai no Tutorial -- que e' o que o
     port sabia tocar -- em vez de nao tocar nada. O id vai para a telemetria
     (FNV-1a do nome) para que o log do console diga qual musica tentou. */
  {const sr_song_t*song=0;
   const char*diff=(story_diff==0)?"easy":(story_diff==2)?"hard":"normal";
   const sr_level_t*lv;
   /* O nivel vem do Story Menu (story_level), nao de um indice fixo. Uma
      semana tem ate 4 musicas e o Story Menu ainda nao escolhe qual delas,
      entao a 0 e' a da vez -- e isso vai dito na telemetria, em vez de
      parecer que o port toca a semana inteira. */
   /* A semana da campanha NAO e' a que o Story Menu esta' mostrando: o menu
      fica em 0 e a campanha pode ser de qualquer semana. Era isso que fazia o
      Story Mode tocar o Tutorial em vez da musica escolhida. */
   g_play_level=g_story_mode?(unsigned)g_story_week:((story_level<0)?0u:(unsigned)story_level);
   if(g_play_level>=sr_level_count())g_play_level=0u;
   /* Em campanha a musica e' a da playlist (o original faz shift() a cada
      fim de musica); fora dela, a 0 da semana. */
   g_play_song_index=0u;
   if(g_story_mode)g_play_song_index=(g_playlist_i>=0&&g_playlist_i<g_playlist_n)?(unsigned)g_playlist_i:0u;
   if(g_fp_active&&g_fp_play_song){
    /* Veio da Freeplay: a musica e' a escolhida na lista, nao a da semana. */
    song=g_fp_play_song;g_play_level=0u;g_play_song_index=0u;
   }else{
    lv=sr_level(g_play_level);
    if(lv&&lv->song_count){if(g_play_song_index>=lv->song_count)g_play_song_index=0;song=lv->songs[g_play_song_index];}
   }
   if(!song)song=sr_song_by_id("tutorial");
   g_play_song=song;
   {u32 h=2166136261u;const char*q=song?song->id:"?";while(*q){h^=(unsigned char)*q++;h*=16777619u;}
    telemetry_event("play_song",(int)(h&0x7fffffffu),(int)((g_play_level<<8)|g_play_song_index));}
   if(song){
    int layout=-1;
    /* NAO e so o caminho do jogo original: no console o arquivo estava la, no
       caminho exato, e o open() devolvia -1 do mesmo jeito. Tentar os layouts
       planos e a unica defesa possivel quando o log nao consegue dizer qual
       foi a falha. O layout que abrir sai no telemetria. */
    g_chart_status=chart_load_song_layout(FNF_ASSET_ROOT,song,diff,&g_chart_info,&layout);
    telemetry_event("chart_layout",layout,g_chart_status);
    /* FCH3 NAO CARREGA OS PARAMETROS DOS EVENTOS. O formato antigo tem so'
       `sample/type/value/order` por evento; zoom, bop, alvo de animacao e id de
       icone moram nos campos que a v4 criou. Um chart v3 com eventos carrega
       (a compatibilidade e' de proposito) mas chega aqui com todos os
       parametros zerados -- e o sintoma seria a camera indo para zoom 0 sem
       nada no log. Avisa uma vez, com o numero de eventos, em vez de deixar a
       diferenca aparecer como defeito de desenho. */
    if(g_chart_status==0&&g_chart_info.version==3u&&g_chart_info.event_count)
     telemetry_event("chart_v3_eventos",(int)g_chart_info.event_count,0);
   } else g_chart_status=-3;
   telemetry_event("chart_path",g_chart_status,(int)(song?song->bpm*1000.0f:0));}
  /* The event module reads the chart through the same loader, so it needs the
     info struct and the step length. The tutorial is 100 BPM, so a step is
     150 ms, which at 48 kHz is 7200 samples. */
  /* O passo do chart vem do PROPRIO chart (tabela de BPM). Antes era
     TUT_SAMPLES_PER_STEP = 7200 fixo, que so' vale para 100 BPM; a 182 BPM o
     passo e' 3.956 amostras, e usar 7.200 punha o bop e os eventos fora do
     tempo. chart_samples_per_step() devolve 0 sem chart carregado, e ai o
     codigo usa o valor do Tutorial explicitamente, nao em silencio. */
  g_play_samples_per_step=chart_samples_per_step(0);
  if(!g_play_samples_per_step)g_play_samples_per_step=TUT_SAMPLES_PER_STEP;
  telemetry_event("play_step",(int)g_play_samples_per_step,(int)g_chart_info.initial_bpm);
  chart_events_init(g_play_samples_per_step);
  chart_events_bind(&g_chart_info);
  /* Agora sim: o chart esta carregado, entao o total de notas pontuaveis do
     resultado (tallies.totalNotes do original) sai do proprio chart. */
  tutorial_notes_reset();
  telemetry_event("play_total_notes",(int)g_play.total_notes,0);
  chart_rc=g_chart_status;
  capture_status("TUT_CHART_DONE",(int)frame_no,chart_rc);
  telemetry_event("tutorial_chart",g_chart_status,(int)g_chart_info.checksum);
  capture_status("TUT_FORMAT_BEGIN",(int)frame_no,sr);
  if(sr==0&&audio_ok)format_rc=audio_set_stream(audio_volume);
  capture_status("TUT_FORMAT_DONE",(int)frame_no,format_rc);
  if(sr==0&&audio_ok&&format_rc==0){
   capture_status("TUT_PCM_OPEN_BEGIN",(int)frame_no,0);
   /* Audio da musica: <root>/songs/<id>/Inst.pcm, o mesmo nome que o
      sr_instrumental_path() usa e que o jogo original guarda em
      songs/<id>/Inst.ogg. Se esse arquivo nao existir -- pendrive antigo, que
      so' tem o stems do Tutorial -- cai no arquivo plano que o port usava,
      sem esconder a diferenca: a telemetria diz qual dos dois abriu. */
   audio_fd=-1;
   if(g_play_song){
    char ap[192];
    if(sr_stems_path(ap,(unsigned)sizeof(ap),FNF_ASSET_ROOT,g_play_song)>0)audio_fd=open(ap,O_RDONLY);
    telemetry_event("play_audio_path",audio_fd,(int)sizeof(ap));
   }
   /* A reserva so' vale para o Tutorial, que e' a musica a que aquele arquivo
      pertence. Usar o stems do Tutorial para tocar Bopeebo seria tocar a musica
      errada com o chart certo -- pior do que admitir que falta o audio. */
   if(audio_fd<0&&g_play_song&&strcmp(g_play_song->id,"tutorial")==0)
    audio_fd=open("mass:/FNFPS2/assets/audio_stems.pcm",O_RDONLY);
   if(audio_fd<0&&g_play_song&&strcmp(g_play_song->id,"tutorial")!=0)
    telemetry_event("play_audio_ausente",0,(int)g_play_level);
   capture_status("TUT_PCM_OPEN_DONE",(int)frame_no,audio_fd);
   if(audio_fd>=0){
    capture_status("TUT_RING_BEGIN",(int)frame_no,audio_fd);
    {int ring_rc=audio_ring_start(audio_fd);capture_status("TUT_RING_DONE",(int)frame_no,ring_rc);telemetry_event("tutorial_ring_start",ring_rc,audio_fd);if(ring_rc==0){g_tutorial_audio_status=0;audio_playing=1;transport_state=1;}
     else{audio_ring_stop();close(audio_fd);audio_fd=-1;g_tutorial_audio_status=-3;}}
   }else g_tutorial_audio_status=-2;
  }else g_tutorial_audio_status=sr<0?-4:-5;
  capture_status("TUT_PREP_DONE",(int)frame_no,g_tutorial_audio_status);
  telemetry_event("tutorial_audio",g_tutorial_audio_status,(g_tutorial_bf_w<<16)|(g_tutorial_gf_w&0xffff));
 }
  if(transition_frame>0){int transition_mid=menu_back_transition?42:15,transition_end=menu_back_transition?102:30;
   /* Espera limitada. Antes, se a recarga do Story falhasse, o portao
      segurava a transicao no meio do fade para sempre: tela preta eterna.
      Agora conta os quadros, grava no CAPTURE_STATUS e desiste em 4 s. */
   if(target_state==7&&transition_frame==transition_mid&&!story_assets_ready){
     if(++g_story_wait==1)capture_status("STORY_WAIT_BEGIN",(int)transition_frame,0);
     if(g_story_wait>240){capture_status("STORY_WAIT_GIVEUP",g_story_wait,0);story_assets_ready=1;}
   }else if(target_state!=7)g_story_wait=0;
   if(!((ui_state==0||ui_state==6||ui_state==7)&&transition_frame==transition_mid&&g_gf_reload_active==1)&&!(target_state==6&&transition_frame==transition_mid&&!g_menu_cache_ready)&&!(target_state==7&&transition_frame==transition_mid&&!story_assets_ready))transition_frame++;
   if(transition_frame==transition_mid+1){int previous_state=ui_state;ui_state=target_state;/* A arte das capsulas da Freeplay so' vive enquanto a tela esta' na frente: sai com a tela e devolve a VRAM ao pool. */if(ui_state!=11)freeplay_capsula_soltar();palco_ui_entrou(&gftex,&stagetex,&tex,ui_state);telemetry_event("state",ui_state,(int)frame_no);if(ui_state==8){g_play_zoom=cena_zoom_do_palco(g_play_song);g_play_scroll=sr_scroll(g_play_song,story_diff);telemetry_event("cena_inicio",(int)(g_play_zoom*1000.0f),(int)(g_play_scroll*1000.0f));g_tut_cam_focus=0;g_tut_cam_ready=0;g_tut_zoom=g_play_zoom;g_tut_scroll=g_play_scroll;g_bf_sing=g_gf_sing=-1;g_tut_bop=1.0f;chart_events_reset(g_official_mainstage_camera_zoom,1.0f);/* Batida = 4 passos do chart; 5 batidas antes da musica, como Countdown.hx. */g_countdown_beat=(s32)(4u*g_play_samples_per_step);if(g_countdown_beat<=0)g_countdown_beat=(s32)(4u*TUT_SAMPLES_PER_STEP);g_countdown_left=g_countdown_beat*5;g_countdown_step=0;g_chart_clock=-g_countdown_left;telemetry_event("countdown_begin",(int)g_countdown_beat,(int)g_countdown_left);capture_status("TUT_STATE_ACTIVE",(int)frame_no,g_tutorial_audio_status);}transform_mode=0;last=-1;last2=-1;if(ui_state==6){selection=g_menu_remembered;/* O original SNAPA a camera para o item lembrado, nao varre: L242
   selectItem() dispara onChange (MenuList.hx L341, sem guarda) que move
   camFollow para o item selecionado ANTES de resetCamStuff()/snapToTarget()
   (L258 + L343-345). Por isso menu_cam_y ja nasce no alvo. */menu_cam_y=40.0f+160.0f*(float)g_menu_remembered;menu_placeholder=0;menu_anim_tick=-1;if(previous_state==1){audsrv_stop_audio();g_title_audio_submitted=0;g_menu_sfx_pos=g_menu_sfx_end=g_menu_sfx_phase=0;if(g_title_audio_fd>=0)close(g_title_audio_fd);g_title_audio_fd=-1;if(audio_ok&&audio_set_stream(audio_volume)==0)g_title_audio_fd=open("mass:/FNFPS2/assets/freakyMenu_48000.pcm",O_RDONLY);if(g_title_audio_fd>=0){audio_ring_stop();audio_ring_start_loop(g_title_audio_fd,1);}telemetry_event("menu_restore",0,g_title_audio_fd>=0); }setup_texture_dims(&stagetex,512,256);memcpy(g_up_bounce,g_menu_pixels+MENU_FRAMES_BYTES,512u*256u*4u);upload_raw(&stagetex,g_up_bounce,512,256);}else if(ui_state==9){palco_soltar();audsrv_stop_audio();audio_playing=0;audio_started=0;transport_state=0;g_result_wait=0;g_result_drawn=0;resmus_parar();telemetry_event("result_enter",(int)fnf_play_rank(&g_play),fnf_play_clear_percent(&g_play));
   /* A ARTE DA TELA DE RESULTADO. O palco ja' foi solto logo acima, entao o
      pool tem ~1,2 MB livres: e' aqui que as tres folhas e o gradiente do
      fundo entram. `g_res_t0` marca o instante da abertura, porque os timers
      do original (6/24, 21/24 e 36/24 s) contam a partir dela. */
   resultado_carregar(&g_pool,&stagetex,story_diff);
   g_res_t0=GetTimerSystemTime();g_res_ms=0;g_res_logado=0;
   /* O ZOOM DO HUD, medido: o maior valor da musica e quantas vezes o HUD
      subiu. Sem isso, "o HUD pisca" nao teria numero nenhum no log do
      console -- e um zero aqui significa que a condicao de passo nunca casou. */
   telemetry_event("hud_zoom",(int)(g_hud_zoom_max*1000.0f),(int)chart_events_state()->hud_zoom_pulsos);
   /* Os dois icones de vida: carregaram, desenharam e trocaram de estado?
      Sem esta linha, "o icone aparece" nao teria prova. */
   telemetry_event("icone_desenho",(int)g_icone_desenhos,
      (g_icone_ativo[ICONE_JOGADOR]?1:0)|(g_icone_ativo[ICONE_OPONENTE]?2:0));
   /* SetCameraBop sem rate = chart v3 (sem parametro de evento). O build do
      pendrive usa FCH5, entao um numero aqui significa que o chart carregado
      nao era o do build. */
   if(chart_events_state()->bop_sem_parametros)telemetry_event("bop_sem_parametros",
      (int)chart_events_state()->bop_sem_parametros,(int)(g_play_song?g_play_song->id[0]:0));if(g_ev_icone||g_ev_script||g_ev_sem_efeito)telemetry_event("eventos_sem_efeito",(int)(g_ev_icone|(g_ev_script<<8)),(int)g_ev_sem_efeito);}else if(ui_state==11){g_fp_active=0;g_fp_drawn=0;menu_anim_tick=-1;/* a arte das capsulas e a contagem do tempo da tela (os timers do "initJumpIn" contam daqui) */freeplay_capsula_carregar(&stagetex);g_fp_t0=GetTimerSystemTime();g_fp_ms=0;g_fp_cap_logado=0;
    /* O DJ: vindo do resultado com rank, o fist pump (ou a reacao de perda);
       senao, o `Intro` que o `freeplay_dj_carregar` ja deixou armado. */
    if(g_dj_volta_rank>=0){freeplay_dj_fist_pump(g_dj_volta_rank);
     telemetry_event("freeplay_dj_volta",g_dj_volta_rank,g_dj_estado);g_dj_volta_rank=-1;}
    else{g_dj_volta_rank=-1;}
    telemetry_event("freeplay_enter",g_fp_sel,g_fp_diff);}else if(ui_state==7){{u32 aq0=(g_title_audio_fd>=0)?(u32)audsrv_queued():0u;g_story_anim_heard_samples=g_title_audio_submitted>aq0/4u?g_title_audio_submitted-aq0/4u:0u;}story_diff=1;story_level=0;g_story_title_y_ready=0;if(previous_state==1||previous_state==8||previous_state==10){audsrv_stop_audio();g_title_audio_submitted=0;g_menu_sfx_pos=g_menu_sfx_end=g_menu_sfx_phase=0;if(g_title_audio_fd>=0)close(g_title_audio_fd);g_title_audio_fd=-1;if(audio_ok&&audio_set_stream(audio_volume)==0)g_title_audio_fd=open("mass:/FNFPS2/assets/freakyMenu_48000.pcm",O_RDONLY);if(g_title_audio_fd>=0){audio_ring_stop();audio_ring_start_loop(g_title_audio_fd,1);menu_audio_prime();g_menu_audio_suspended=0;}}telemetry_event("story_music",previous_state,g_title_audio_fd>=0);}else if(ui_state==0){telemetry_event("title_anim_reuse",g_title_anim_status,g_title_anim_pixels!=0);audio_started=0;audio_playing=0;clock_hold=1;transport_state=0;title_intro_active=0;title_confirm_frames=0;title_intro_flash_frames=60;title_logo_last=title_gf_last=-1;setup_texture_dims(&fonttex,384,192);memcpy(g_up_bounce,g_menu_pixels+TITLE_SNAPSHOT_ALPHA,384u*192u*4u);upload_raw(&fonttex,g_up_bounce,384,192);menu_anim_tick=-1;setup_texture_dims(&stagetex,512,192);memcpy(g_up_bounce,g_menu_pixels+TITLE_SNAPSHOT_PROMPT_L,512u*192u*4u);upload_raw(&stagetex,g_up_bounce,512,192);setup_texture_dims(&tex2,320,192);memcpy(g_up_bounce,g_menu_pixels+TITLE_SNAPSHOT_PROMPT_R,320u*192u*4u);upload_raw(&tex2,g_up_bounce,320,192);menu_anim_tick=-1;setup_texture_dims(&tex,TITLE_LOGO_W,TITLE_LOGO_H);gftex.width=256;setup_texture_dims(&gftex,TITLE_GF_W,TITLE_GF_H);upload_raw(&tex,g_gf_pixels,TITLE_LOGO_W,TITLE_LOGO_H);menu_anim_tick=-1;upload_raw(&gftex,g_gf_pixels+TITLE_GF_STORE_OFFSET,TITLE_GF_W,TITLE_GF_H);menu_anim_tick=-1;title_ng_restored=1;if(g_title_audio_fd<0&&audio_ok&&audio_set_stream(audio_volume)==0){g_title_audio_fd=open("mass:/FNFPS2/assets/freakyMenu_48000.pcm",O_RDONLY);if(g_title_audio_fd>=0){audio_ring_stop();audio_ring_start_loop(g_title_audio_fd,1);telemetry_event("ring_source",0,1);}}}}
   if(transition_frame>transition_end){transition_frame=0;target_state=-1;repeat_dir=0;repeat_frames=0;menu_back_transition=0;}
  }
  if(transition_frame==0&&ui_state==8&&g_tutorial_audio_finished&&(pressed&PAD_CROSS)&&g_tutorial_audio_status==0&&audio_fd>=0){
   audsrv_stop_audio();audio_started=0;audio_playing=0;audio_submitted=music_clock=0;
   music_clock_tick=music_clock_fraction=0;clock_hold=1;g_tutorial_audio_finished=0;g_chart_cursor=g_chart_seen=0;g_chart_last_type=g_chart_last_value=g_chart_last_sample=0;tutorial_notes_reset();
   g_countdown_left=g_countdown_beat*5;g_countdown_step=0;g_chart_clock=-g_countdown_left;
   if(audio_ring_reset()==0){audio_playing=1;transport_state=1;g_tutorial_audio_status=0;telemetry_event("tutorial_audio_replay",(int)frame_no,0);}
   else g_tutorial_audio_status=-7;
  }
  /* FREEPLAY: navegacao e inicio da musica.
     O original anda pela grade de capsules com as quatro direcoes e troca a
     dificuldade com o "bubble" da esquerda/direita; aqui, sem a arte das
     capsules, a lista e' vertical (UP/DOWN) e LEFT/RIGHT troca a dificuldade
     entre as que a musica realmente tem. CROSS toca, CIRCLE volta ao menu. */
  if(transition_frame==0&&ui_state==11&&g_fp_n>0){
   /* Todo movimento da lista e' um `dj.onPlayerAction()` no original (L1764
      roda do mouse, L1821 das teclas, L1844/L1850 e L2548 das bordas e do
      arrasto de dificuldade): zera o `timeIdling`, nao troca a animacao. */
   if(pressed&PAD_UP){g_fp_sel=(g_fp_sel+g_fp_n-1)%g_fp_n;g_fp_diff=freeplay_diff_valida(g_fp[g_fp_sel],g_fp_diff);freeplay_dj_acao();telemetry_event("freeplay_sel",g_fp_sel,g_fp_diff);}
   else if(pressed&PAD_DOWN){g_fp_sel=(g_fp_sel+1)%g_fp_n;g_fp_diff=freeplay_diff_valida(g_fp[g_fp_sel],g_fp_diff);freeplay_dj_acao();telemetry_event("freeplay_sel",g_fp_sel,g_fp_diff);}
   if(pressed&PAD_LEFT||pressed&PAD_RIGHT){int d=g_fp_diff,passo=(pressed&PAD_RIGHT)?1:2,n;
    for(n=0;n<3;n++){d=(d+passo)%3;if(g_fp[g_fp_sel]->difficulties&(1u<<d))break;}
    g_fp_diff=d;freeplay_dj_acao();telemetry_event("freeplay_diff",g_fp_diff,0);}
   if(pressed&PAD_CIRCLE){target_state=6;transition_frame=1;transitions++;telemetry_event("freeplay_back",0,0);}
   else if(pressed&PAD_CROSS){
    /* `FreeplayState.hx` L2765: `dj?.onConfirm()` antes de sair para o palco. */
    freeplay_dj_confirmar();
    story_diff=g_fp_diff;g_fp_play_song=g_fp[g_fp_sel];g_fp_active=1;g_story_mode=0;
    g_practice_mode=0;fnf_play_set_practice(&g_play,0);
    g_tutorial_scene_attempted=0;g_tutorial_scene_ready=0;
    {u32 h=2166136261u;const char*q=g_fp_play_song->id;while(*q){h^=(unsigned char)*q++;h*=16777619u;}
     telemetry_event("freeplay_play",(int)(h&0x7fffffffu),g_fp_diff);}
    target_state=8;transition_frame=1;transitions++;
   }
  }
  /* PAUSA (PauseSubState.hx).
     Abre no START, congela o jogo, escurece a tela com um retangulo preto
     alfa 0,6 (transitionIn(): tween de alpha 0 -> 0,6 em 0,8 s com quartOut) e
     mostra o menu. O primeiro quadro depois de abrir nao aceita entrada --
     e' o `justOpened` do original, que existe para o mesmo START que abriu nao
     fechar na hora. */
  if(ui_state==8&&transition_frame==0&&g_tutorial_scene_ready&&!g_play.dead){
   if(!g_paused&&(pressed&PAD_START)&&!(g_held&PAD_SELECT)&&!g_tutorial_audio_finished){
    g_paused=1;g_pause_sel=0;g_pause_frame=0;g_pause_mode=0;g_pause_drawn=0;
    audsrv_stop_audio();audio_playing=0;audio_ring_stop();transport_state=3;
    telemetry_event("pause_open",(int)music_clock,(int)frame_no);
    /* startPauseMusic(): `breakfast$musicSuffix` em loop, trecho sorteado,
       fade-in de 5 s ate 0,75. O sufixo vem do BF em cena (o BM que morre nao
       pausa -- aqui so' se pausa com o jogo vivo). */
    pause_audio_sufixo();
    pause_audio_iniciar();
    pause_menu_telemetria();
   }else if(g_paused){
    g_pause_frame++;
    if(g_pause_frame>=2){
     /* Entradas visiveis neste modo. */
     int n_sel=0,song_diffs=0,diff_list[3],n_ent,d,acoes[PAUSE_ENTRADAS_PADRAO],acao=-1;
     unsigned int mask=(g_play_song?g_play_song->difficulties:SR_DIFF_ALL);
     if(g_pause_mode){
      for(d=0;d<3;d++)if(mask&(1u<<d))diff_list[song_diffs++]=d;
      n_sel=song_diffs+1;   /* as dificuldades e, no fim o "Back" */
     }else n_sel=pause_acoes(acoes);
     n_ent=n_sel;
     if(!g_pause_mode&&g_pause_sel<n_ent)acao=acoes[g_pause_sel];
     if(pressed&PAD_UP){g_pause_sel=(g_pause_sel+n_ent-1)%n_ent;telemetry_event("pause_sel",g_pause_sel,(int)g_pause_mode);}
     else if(pressed&PAD_DOWN){g_pause_sel=(g_pause_sel+1)%n_ent;telemetry_event("pause_sel",g_pause_sel,(int)g_pause_mode);}
     if(pressed&PAD_START){g_paused=0;telemetry_event("pause_close",(int)music_clock,0);
      pause_audio_parar();
      PLAY_RETOMAR_AUDIO(audio_fd,music_clock);
     }else if((pressed&PAD_CROSS)&&n_ent>0){
      if(!g_pause_mode){
       if(acao==PAUSE_ACAO_RESUME){ /* Resume */
        g_paused=0;telemetry_event("pause_close",(int)music_clock,0);
        pause_audio_parar();
        PLAY_RETOMAR_AUDIO(audio_fd,music_clock);
       }else if(acao==PAUSE_ACAO_REINICIAR){ /* Restart Song: mesmo reinicio do game over */
        g_paused=0;
        pause_audio_parar();
        PLAY_REINICIAR_MUSICA();
        PLAY_REBOBINAR_AUDIO(audio_fd);
        telemetry_event("pause_restart",(int)frame_no,0);
       }else if(acao==PAUSE_ACAO_DIFICULDADE){ /* Change Difficulty */
        g_pause_mode=1;g_pause_sel=0;telemetry_event("pause_difficulty",(int)mask,0);
       }else if(acao==PAUSE_ACAO_PRATICA){
        /* enablePracticeMode (PauseSubState L1129-1135): liga o modo e
           REGENERA o menu (`regenerateMenu()`), o que reinicia a selecao no
           primeiro item e some com esta propria entrada. A musica da pausa e
           o relogio congelado nao mudam: o substate continua aberto. */
        g_practice_mode=1;fnf_play_set_practice(&g_play,1);
        g_pause_sel=0;
        telemetry_event("pause_practice",(int)g_play.dead,(int)frame_no);
        pause_menu_telemetria();
       }else{ /* Exit to Menu */
        g_paused=0;
        pause_audio_parar();
        audsrv_stop_audio();audio_playing=0;audio_started=0;transport_state=0;
        g_menu_cache_ready=0;story_assets_ready=0;target_state=g_fp_active?11:7;transition_frame=1;transitions++;
        telemetry_event("pause_exit",(int)frame_no,0);
       }
      }else{
       /* Modo dificuldade: as entradas sao as dificuldades da musica (as que o
          registro diz que existem) e, no fim, "Back". */
       for(d=0;d<3;d++)if(mask&(1u<<d))diff_list[song_diffs++]=d;
       if(g_pause_sel<song_diffs){
        /* Trocar a dificuldade recarrega o chart: a entrada do PlayState
           recarrega cena, chart e audio, como o restartPlayState do original. */
        /* Trocar de dificuldade ZERA o placar da campanha no original, mas
           so' quando a dificuldade realmente muda -- "para que trocar na ultima
           musica da semana nao de' um placar geral baixissimo"
           (PauseSubState.hx L1046-1056). Nao era o caso do reinicio: parar e
           recoemecar a MESMA musica nao apaga o que as musicas anteriores
           somaram. */
        if(diff_list[g_pause_sel]!=story_diff)campanha_zerar();
        story_diff=diff_list[g_pause_sel];
        g_paused=0;g_pause_mode=0;g_pause_sel=0;
        pause_audio_parar();
        g_tutorial_scene_attempted=0;g_tutorial_scene_ready=0;
        target_state=8;transition_frame=1;transitions++;
        telemetry_event("pause_change_difficulty",(int)story_diff,(int)frame_no);
       }else{g_pause_mode=0;g_pause_sel=2;telemetry_event("pause_back",0,0);}
      }
     }else if((pressed&PAD_CIRCLE)&&g_pause_mode){g_pause_mode=0;g_pause_sel=2;telemetry_event("pause_back",0,0);}
    }
   }
  }
  /* GAME OVER: entrada, reinicio e saida.
     CROSS reinicia a musica e CIRCLE volta ao Story Menu, como no original
     (ACCEPT_P -> confirmDeath() -> needsReset; BACK_P -> goBack()).
     O `canInput` de 1 s do original (L186-189) NAO bloqueia o controle: ele
     existe para o toque, e o ACCEPT_P/BACK_P nao o consultam (L271-278). Por
     isso aqui as duas teclas valem a partir do segundo quadro do substate --
     o primeiro e' o que arma `blueballed`. */
  /* O FADE-IN DE 1 s do resetPlaying (GameOverSubState.hx L393) roda no
     PlayState JA' reiniciado, por isso ele e' contado fora do estado 10. */
  if(g_fade_in>0)g_fade_in--;
  if(transition_frame==0&&ui_state==10){
   g_gameover_frames++;
   g_go_ms+=TUT_DT*1000.0f;
   /* PRIMEIRO QUADRO: a animacao e a camera da tela nascem aqui, e nao na
      transicao, porque no original elas nascem no `update` do substate (o
      `hasStartedAnimation` de GameOverSubState.hx L230-262) -- o substate e'
      criado no meio do quadro da morte e so' age no seguinte. */
   if(!g_go_iniciado){
    const char *stg=g_play_song?g_play_song->stage:"mainStage";
    const bs_slot_t *slb=ch_slot(stg,"bf");
    int ci=g_cena_bf_ci,tem_fake=0;
    g_go_iniciado=1;
    /* A camera COMECA onde o gameplay a deixou (o substate nasce sobre o
       PlayState e a camera do jogo continua sendo a mesma) e o zoom tambem. */
    g_go_cam_x=g_tut_cam_x;g_go_cam_y=g_tut_cam_y;g_go_zoom=g_tut_zoom;
    g_go_anim=-1;g_go_terminando=0;g_go_fade=0;g_go_blueballed=0;
    /* O avatar do jogador: e' ele que morre (GameOverSubState usa o BF do
       PlayState, que nos pacotes e' o personagem do slot `bf`). */
    if(g_cena_modo&&slb&&ci>=0&&g_cena.p[CH_PAPEL_BF].ativo){
     tem_fake=ch_anim_i(ci,"fakeoutDeath")>=0;
     /* FlxG.random.bool((1/4096)*100) -- 1 em 4096. O sorteio e'
        deterministico (LCG semeado pelo quadro) para que duas rodadas do
        harness com o mesmo roteiro de entrada desenhem o mesmo. */
     g_go_sorteio=(unsigned int)frame_no*2654435761u+1u;
     g_go_sorteio=g_go_sorteio*1664525u+1013904223u;
     if(tem_fake&&(g_go_sorteio>>20)%4096u==0u){
      if(cc_toca(&g_cena,CH_PAPEL_BF,"fakeoutDeath",0)){
       /* O `fakeoutDeath` do bf.hxc (playAnimation) chama doFakeoutDeath():
          toca `gameplay/gameover/fakeout_death`, marca `mustNotExit` e, no fim
          da animacao, ENTRA NO `firstDeath` e chama playBlueBalledSFX().
          Antes o port tocava o fakeout e ia direto para o deathLoop: faltava
          a segunda metade -- o BF nunca morria de verdade nesse caminho. */
       g_go_anim=2;g_go_ms=0.0f;
       gameover_audio_troca_sfx("fakeout_death","");
       /* `blueballed` NAO e' marcado aqui -- e' o que o original faz. */
       telemetry_event("gameover_fakeout",1,(int)frame_no);
      }
     }
     if(g_go_anim<0){
      if(cc_toca(&g_cena,CH_PAPEL_BF,"firstDeath",0)){
       g_go_anim=0;
       /* `isDead = true`: o personagem NAO volta a dancar quando a morte
          acaba (GameOverSubState.hx L172-177). Sem isso o BF morto dancava. */
       cc_especial_fixo(&g_cena,CH_PAPEL_BF,1);
       /* playBlueBalledSFX(): marca `blueballed`. O som nao existe no pacote,
          mas o ESTADO sim -- e' ele que libera o CROSS/CIRCLE. */
       g_go_blueballed=1;
       telemetry_event("gameover_primeira_morte",ci,(int)frame_no);
      }else telemetry_event("gameover_sem_anim_morte",ci,0);
     }
    }else{
     /* Sem cena (personagem sem pacote) o original tambem chama
        playBlueBalledSFX(). */
     g_go_blueballed=1;
     telemetry_event("gameover_sem_cena",g_cena_modo,ci);
    }
    /* `playBlueBalledSFX()` marca `blueballed = true` (L600-623) e e' o que
       libera o CROSS; aqui nao ha' o som, entao o que resta e' o estado. */
    telemetry_event("gameover_enter_camera",(int)g_go_cam_x,(int)g_go_cam_y);
    { /* O ALVO da camera, medido uma vez: e' contra ele que o harness cobra a
         convergencia, e no console e' o numero que se compara com a conta do
         ch_foco_morte (test/test_char_layout.c) e com o `death` do JSON. */
     float fx=0.0f,fy=0.0f;
     if(go_foco_bf(&fx,&fy))telemetry_event("gameover_foco",(int)fx,(int)fy);
     else telemetry_event("gameover_foco",-1,-1);}
   }
   {
    /* O `fakeoutDeath` ACABOU: `doFakeoutDeath` marca mustNotExit = false,
       toca `firstDeath` e chama playBlueBalledSFX() -- so' agora `blueballed`
       existe, e e' por isso que o CROSS/CIRCLE nao funcionam durante o
       fakeout (beco sem saida de 1 em 4096 no original, reproduzido). */
    if(g_go_anim==2&&!g_go_terminando&&g_cena.p[CH_PAPEL_BF].ativo&&
       cc_especial_terminou(&g_cena,CH_PAPEL_BF,(long)g_go_ms)){
     if(cc_toca(&g_cena,CH_PAPEL_BF,"firstDeath",(long)g_go_ms)){
      g_go_anim=0;g_go_ms=0.0f;
      cc_especial_fixo(&g_cena,CH_PAPEL_BF,1);
      g_go_blueballed=1;
      gameover_audio_troca_sfx("loss_sfx",g_go_bb_suf);
      telemetry_event("gameover_fakeout_fim",1,(int)frame_no);
     }
    }
    /* A ANIMACAO SEGUE O RELOGIO DO SUBSTATE: acabou o `firstDeath`, entra o
       `deathLoop` (L305-318) e a musica junto (startDeathMusic(1.0,false)). */
    if(g_go_anim==0&&!g_go_terminando&&g_cena.p[CH_PAPEL_BF].ativo&&
       cc_especial_terminou(&g_cena,CH_PAPEL_BF,(long)g_go_ms)){
     if(cc_toca(&g_cena,CH_PAPEL_BF,"deathLoop",(long)g_go_ms)){
      g_go_anim=1;g_go_ms=0.0f;
      telemetry_event("gameover_loop_morte",0,(int)frame_no);
      gameover_audio_musica();
     }
    }
    /* A CAMERA: perseguicao do ponto do BF e zoom para
       `stage.camZoom * death.cameraZoom`, os dois suavizados com as
       constantes do original. Roda tambem durante a saida: no original o
       substate continua atualizando a camera ate' fechar. */
    {
     float fx=0.0f,fy=0.0f;
     if(go_foco_bf(&fx,&fy)){
      g_go_cam_x+=(fx-g_go_cam_x)*GO_CAM_LERP;
      g_go_cam_y+=(fy-g_go_cam_y)*GO_CAM_LERP;
     }
     {
      int ci=g_cena_bf_ci;
      float tz=g_play_zoom*((ci>=0&&ci<CH_CHAR_COUNT)?ch_chars[ci].dzoom:1.0f);
      g_go_zoom=tz+(g_go_zoom-tz)*GO_ZOOM_K;
     }
    }
   }
   /* A SAIDA DA TELA (confirmDeath, GameOverSubState.hx L369-438).
      A ordem do original, e ela nao e' "1 s de fade":
        1) startDeathMusic(1.0, true) -> `gameOverEnd` (sem loop);
        2) espera FADE_TIMER = gameOverMusic.length / 7000
           (com o End real de 7,21 s isso da' 1,03 s);
        3) FlxG.camera.fade(BLACK, 2, false, ...) -- DOIS segundos indo para
           preto;
        4) no fim do fade: resetPlaying() -> fade(BLACK, 1, true) [fade IN de
           1 s], needsReset = true e close().
      Antes o port reiniciava 60 quadros depois do CROSS (1 s, o fade de 2 s
      nao existia e o FADE_TIMER tambem nao) -- dois eventos a menos e um
      reinicio cedo demais. */
   if(g_go_terminando){
    g_go_espera_ms+=TUT_DT*1000.0f;
    if(g_go_saida==1&&g_go_espera_ms>=g_go_espera_ms_alvo){
     g_go_saida=2;g_go_fade=0;
     telemetry_event("gameover_fade_out",(int)g_go_espera_ms,(int)g_go_espera_ms_alvo);
    }
    if(g_go_saida==2)g_go_fade++;
   }
   if(g_go_saida==2&&g_go_fade>=GO_FADE_SAIDA_QUADROS){
    {
    /* Reinicio NO LUGAR: o original fecha o substate e marca needsReset, e o
       PlayState se reinicia. Aqui a cena ja' esta' carregada, entao o que
       precisa voltar ao comeco e' o transporte, o relogio, o cursor do chart,
       os tallies, a camera e a contagem regressiva (PLAY_REINICIAR_MUSICA,
       a mesma macro que o Restart Song da pausa usa). */
    PLAY_REINICIAR_MUSICA();
    /* O ring do game over esta' lendo os arquivos dele: parar ANTES de fechar
       e de reabrir o do nivel. O `PLAY_REBOBINAR_AUDIO` seria uma espera de
       500 ms a' toa, porque a thread do anel ja' foi encerrada aqui. */
    gameover_audio_parar();
    if(audio_set_stream(audio_volume)!=0)telemetry_event("gameover_stream",-1,(int)frame_no);
    if(audio_fd>=0){lseek(audio_fd,0,SEEK_SET);
     if(audio_ring_start(audio_fd)==0){audio_playing=1;transport_state=1;g_tutorial_audio_status=0;}
     else telemetry_event("gameover_rebobinar",-1,(int)frame_no);}
    /* `boyfriend.isDead = false` e `resetCameraZoom()` (L406-413): o
       personagem volta a dancar e a camera volta ao zoom do palco. */
    if(g_cena.p[CH_PAPEL_BF].ativo){
     /* `boyfriend.resetCharacter(true)` volta o personagem para o idle. */
     int ai=ch_anim_idle(g_cena_bf_ci,0);
     cc_especial_fixo(&g_cena,CH_PAPEL_BF,0);
     if(ai>=0&&g_cena_bf_ci>=0)cc_toca(&g_cena,CH_PAPEL_BF,
        ch_anims[ch_chars[g_cena_bf_ci].anim_first+ai].name,0);
    }
    g_go_iniciado=0;g_go_anim=-1;g_go_terminando=0;g_go_ms=0.0f;g_go_saida=3;
    /* `FlxG.camera.fade(BLACK, 1, true)` criado pelo resetPlaying: a tela
       volta do preto em 1 s, ja' no PlayState reiniciado. */
    g_fade_in=GO_FADE_ENTRADA_QUADROS;
    ui_state=8;target_state=-1;transition_frame=0;transform_mode=0;last=-1;last2=-1;
    telemetry_event("state",8,(int)frame_no);
    telemetry_event("gameover_restart",(int)frame_no,(int)g_play.health);
    /* Prova de que a camera e o zoom REALMENTE chegaram onde o original manda:
       o ponto do BF (setCameraTarget) e stage.camZoom * death.cameraZoom. */
    telemetry_event("gameover_cam_final",(int)g_go_cam_x,(int)g_go_cam_y);
    {int ci=g_cena_bf_ci;
     telemetry_event("gameover_zoom_final",(int)(g_go_zoom*1000.0f),
        (int)(g_play_zoom*((ci>=0&&ci<CH_CHAR_COUNT)?ch_chars[ci].dzoom:1.0f)*1000.0f));}
    }
   }else if(pressed&PAD_CROSS&&!g_go_terminando&&g_gameover_frames>=2&&g_go_blueballed){
    /* confirmDeath() (L330-372): `isEnding = true`, toca `deathConfirm` +
       `animationSuffix`, troca a musica por `gameOverEnd` (sem loop), calcula
       o FADE_TIMER a partir do comprimento dela e so' depois comeca o fade. */
    g_go_terminando=1;g_go_fade=0;g_go_saida=1;g_go_ms=0.0f;
    g_go_espera_ms=0.0f;g_go_espera_ms_alvo=0.0f;
    gameover_audio_fim();
    if(g_cena.p[CH_PAPEL_BF].ativo&&cc_toca(&g_cena,CH_PAPEL_BF,"deathConfirm",0))
     telemetry_event("gameover_confirmar",1,(int)frame_no);
    else telemetry_event("gameover_confirmar",0,(int)frame_no);
   }else if(pressed&PAD_CIRCLE&&!g_go_terminando&&g_gameover_frames>=2&&g_go_blueballed){
    /* goBack() (L544-570): so' depois de `blueballed` (o primeiro quadro) e
       nunca durante a saida. O deathCounter volta a zero, e e' isso que zera
       "BLUE BALLS" no HUD do original (PlayState.deathCounter = 0). */
    g_death_counter=0;
    gameover_audio_parar();
    g_menu_cache_ready=0;story_assets_ready=0;target_state=g_fp_active?11:7;transition_frame=1;transitions++;
    g_story_mode=0;   /* semana abandonada: nao e' mais campanha */
    telemetry_event("gameover_exit",(int)frame_no,0);
   }
  }
  if(transition_frame==0 && ui_state==0){
   int move=0,dir=0;
   if(0&&(pressed&PAD_TRIANGLE)){components_mode^=1;repeat_dir=0;repeat_frames=0;}
   if(pressed&PAD_CIRCLE && components_mode){components_mode=0;repeat_dir=0;repeat_frames=0;}
   if(pressed&PAD_UP){move=-1;dir=-1;}else if(pressed&PAD_DOWN){move=1;dir=1;}else if(g_held&PAD_UP)dir=-1;else if(g_held&PAD_DOWN)dir=1;
   if(dir!=repeat_dir){repeat_dir=dir;repeat_frames=0;}else if(dir){repeat_frames++;if(repeat_frames>=20&&((repeat_frames-20)%6)==0)move=dir;}else repeat_frames=0;
   if(components_mode){
    if(move)settings_sel=(settings_sel+move+5)%5;
    if(pressed&PAD_CROSS && settings_sel==0)toggle_value^=1;
    if(settings_sel==1 && (pressed&PAD_LEFT) && slider_value>0)slider_value--;
    if(settings_sel==1 && (pressed&PAD_RIGHT) && slider_value<10)slider_value++;
    if(pressed&PAD_L1)tab_value=(tab_value+2)%3;
    if(pressed&PAD_R1)tab_value=(tab_value+1)%3;
   }else{
    if(pressed&PAD_CROSS&&title_intro_active){title_intro_active=0;title_intro_flash_frames=60;setup_texture_dims(&tex,TITLE_LOGO_W,TITLE_LOGO_H);upload_raw(&tex,g_gf_pixels,TITLE_LOGO_W,TITLE_LOGO_H);menu_anim_tick=-1;title_ng_restored=1;}else if((pressed&PAD_CROSS)&&title_confirm_frames==0){title_confirm_frames=120;}
   }
  }else if(transition_frame==0&&ui_state==6){int oldsel=selection;menu_flicker_step(frame_no);if(menu_placeholder){if(pressed&PAD_CIRCLE){menu_placeholder=0;menu_anim_tick=-1;}}else if(!g_fl_item_on&&!menu_exit_frames){if(pressed&PAD_UP)selection=(selection+4)%5;else if(pressed&PAD_DOWN)selection=(selection+1)%5;if(pressed&PAD_CIRCLE){/* 0.2.104 A5: goBack() do original guarda a selecao antes de sair
     (MainMenuState.hx L627: rememberedSelectedIndex = menuItems?.selectedIndex ?? 0).
     O porto so guardava no CROSS (startExitState, L426); voltar com CIRCLE
     perdia a selecao e reentrar no menu abria sempre no STORY MODE.
     Pendencia registrada: L629 toca Paths.sound('cancelMenu'), e o
     menu_sfx.pcm do porto so tem scroll [0,39168) e confirm [39168,241920);
     [241920,281088) e a fonte VCR usada por tutorial_vcr_restore (main.c
     L687). Sem a amostra cancelMenu no asset nao da para tocar - nao vou
     inventar offset. */g_menu_remembered=selection;g_menu_sfx_pos=g_menu_sfx_end=g_menu_sfx_phase=0;g_menu_cache_ready=0;title_logo_last=title_gf_last=-1;menu_back_transition=0;target_state=0;transition_frame=1;transitions++;}else if(pressed&PAD_CROSS){g_menu_sfx_pos=39168;g_menu_sfx_end=241920;g_menu_sfx_phase=0;g_menu_remembered=selection;
     /* 0.2.105 C4: o original (MenuList.hx L286-305) nao tem janela de 24
        quadros. accept() dispara onAcceptPress (flicker do magenta 1,1 s /
        0,15 s) e FlxFlicker.flicker(item, 1, 0.06, true, false, callback):
        o ITEM pisca 16 vezes (1 s) e SO ENTAO o callback roda, que e
        startExitState (MainMenuState.hx L431-457): selected.visible=false,
        alpha->0 em 0,4 s (quadOut) nos outros, switchState apos 0,4 s.
        Total CROSS->troca = 0,96+0,4 = 1,36 s = 82 quadros; o confirmMenu
        (202752 B/4/48000 = 1,056 s = 63,4 quadros) toca INTEIRO. A janela
        de 24 quadros da 0.2.104 cortava o som pela metade: foi o "piscar
        muito rapido + som cortado" medido no console. */
     g_accept_press_frame=(int)frame_no;g_fl_item_on=1;g_fl_item_tl=0.06;g_fl_item_left=16;g_fl_item_vis=1;g_fl_item_toggles=0;g_fl_mag_on=1;g_fl_mag_tl=0.15;g_fl_mag_left=7;g_fl_mag_vis=0;g_fl_mag_toggles=0;menu_exit_frames=0;g_accept_item_end=g_accept_switch=-1;}}if(selection!=oldsel){menu_anim_tick=-1;g_menu_sfx_pos=0;g_menu_sfx_end=39168;g_menu_sfx_phase=0;}if(menu_exit_frames){menu_exit_frames--;if(menu_exit_frames==0){g_accept_switch=(int)frame_no-g_accept_press_frame;if(selection==0){/* FIDELIDADE 0.2.103: o original, ao confirmar STORY MODE,
     faz FunkinMemory.clearFreeplay() e purgeCache() (MainMenuState.hx L136-140).
     Equivalente do port: soltar os packs do Tutorial, que o Story nao usa;
     tutorial_pack_load() recarrega ao entrar no Tutorial de novo (pointer 0). */
tutorial_packs_free();story_assets_ready=0;target_state=7;transition_frame=1;transitions++;}else if(selection==1){/* FREEPLAY: monta a lista do registro e abre a tela. */
    freeplay_build();g_fp_sel=0;g_fp_diff=freeplay_diff_valida(g_fp_n?g_fp[0]:0,story_diff);g_fp_active=0;g_fp_play_song=0;
    telemetry_event("freeplay_open",g_fp_n,0);target_state=11;transition_frame=1;transitions++;}
else{menu_placeholder=selection;if(menu_font_restore()==0){setup_texture_dims(&fonttex,FONT_W,FONT_H);upload_raw(&fonttex,g_font_pixels,FONT_W,FONT_H);}}}}menu_cam_y+=(40.0f+160.0f*(float)selection-menu_cam_y)*0.06f;
  }else if(transition_frame==0&&ui_state==7){if(pressed&PAD_UP){int sr;story_level=(story_level+8)%9;sr=story_titles_restore();if(sr==0)story_upload_title(&tex,story_level);telemetry_event("story_title_restore",story_level,sr);g_menu_sfx_pos=0;g_menu_sfx_end=39168;g_menu_sfx_phase=0;}else if(pressed&PAD_DOWN){int sr;story_level=(story_level+1)%9;sr=story_titles_restore();if(sr==0)story_upload_title(&tex,story_level);telemetry_event("story_title_restore",story_level,sr);g_menu_sfx_pos=0;g_menu_sfx_end=39168;g_menu_sfx_phase=0;}if(pressed&PAD_LEFT){story_diff=(story_diff+2)%3;story_diff_tween=0;telemetry_event("story_diff_tween",story_diff,0);g_menu_sfx_pos=0;g_menu_sfx_end=39168;g_menu_sfx_phase=0;}else if(pressed&PAD_RIGHT){story_diff=(story_diff+1)%3;story_diff_tween=0;telemetry_event("story_diff_tween",story_diff,0);g_menu_sfx_pos=0;g_menu_sfx_end=39168;g_menu_sfx_phase=0;}if(pressed&PAD_CIRCLE){target_state=6;transition_frame=1;transitions++;}else if(pressed&PAD_CROSS){/* Campanha: monta a playlist da semana e comeca na primeira musica
     (StoryMenuState.hx L571-598). Antes era `story_level==0`, entao so' a
     semana do Tutorial podia ser jogada. */
    const sr_level_t*lvsel=sr_level(story_level);
    if(lvsel&&lvsel->song_count){
     g_story_mode=1;g_story_week=story_level;g_fp_active=0;g_fp_play_song=0;
     g_playlist_n=(int)lvsel->song_count;g_playlist_i=0;campanha_zerar();
     g_practice_mode=0;fnf_play_set_practice(&g_play,0);
     g_play_level=(unsigned)story_level;g_play_song_index=0;
     g_tutorial_scene_attempted=0;g_tutorial_scene_ready=0;
     target_state=8;transition_frame=1;transitions++;
     telemetry_event("story_begin",story_level,g_playlist_n);
    }else telemetry_event("story_sem_musicas",story_level,0);}
  }else if(transition_frame==0 && (pressed&PAD_CIRCLE)){if(ui_state==0&&components_mode)components_mode=0;else if(ui_state==8){g_menu_cache_ready=0;story_assets_ready=0;target_state=g_fp_active?11:7;transition_frame=1;transitions++;}
   else if(ui_state==9){g_result_wait=0;g_menu_cache_ready=0;story_assets_ready=0;
    /* `FreeplayState.hx` L1084: ao voltar do resultado a Freeplay recebe
       `fromResultsParams.newRank` e o DJ faz `fistPumpIntro()` (ou
       `fistPumpLossIntro()` quando o rank e' SHIT). O rank aqui e' o MESMO que
       a tela de resultado mostrou (`resultado_rank()`), nao um recalculo. */
    g_dj_volta_rank=g_fp_active?resultado_rank():-1;
    target_state=g_fp_active?11:7;transition_frame=1;transitions++;resmus_parar();}
   else if(ui_state==10){g_menu_cache_ready=0;story_assets_ready=0;target_state=g_fp_active?11:7;transition_frame=1;transitions++;}else if(ui_state==1){g_menu_cache_ready=0;story_assets_ready=0;target_state=7;transition_frame=1;transitions++;}else{target_state=0;transition_frame=1;transitions++;}}
  frame_mark("F.STATE",ui_state*100+transition_frame);
  if(audio_playing)visual_frame++;
  if(transition_frame==0){if(pressed&PAD_LEFT){anim=1;anim_start=visual_frame;}if(pressed&PAD_DOWN){anim=2;anim_start=visual_frame;}if(pressed&PAD_UP){anim=3;anim_start=visual_frame;}if(pressed&PAD_RIGHT){anim=4;anim_start=visual_frame;}if(pressed&PAD_CROSS){anim=0;anim_start=visual_frame;}}
  frame_mark("F.ANIM",anim);
  local=((visual_frame-anim_start)*g_bf.fps)/60u;
  if(local>=g_bf.anim[anim].count){
   if(anim>0 && (g_held&anim_button(anim))){anim_start=visual_frame;local=0;}
   else {anim=0;anim_start=visual_frame;local=0;}
  }
  global=g_bf.anim[anim].start+local;

  if(ui_state==0){if(g_title_anim_status==-2){if(g_title_anim_pixels)free(g_title_anim_pixels);g_title_anim_pixels=0;g_title_anim_status=-3;}if(g_title_anim_status==1&&!g_title_anim_ready_logged){telemetry_event("title_anim_ready",TITLE_LOGO_BYTES,TITLE_GF_BYTES);g_title_anim_ready_logged=1;}u32 raw_th=0,bs=12970u;int tqb=(g_title_audio_fd>=0?audsrv_queued():-1);if(g_title_audio_submitted<g_title_anim_last_submitted)g_title_anim_heard_samples=0;g_title_anim_last_submitted=g_title_audio_submitted;if(tqb>=0){u32 tq=(u32)tqb/4u;raw_th=g_title_audio_submitted>tq?g_title_audio_submitted-tq:0u;if(raw_th>g_title_anim_heard_samples)g_title_anim_heard_samples=raw_th;}u64 oldth=((u64)g_title_anim_heard_samples*147u)/320u;u32 phase=(u32)(oldth%bs),beat=(u32)(oldth/bs);unsigned int lf=(unsigned int)(((oldth*24u)/22050u)%TITLE_LOGO_FRAMES),gf=(beat&1u)*15u+(unsigned int)(((u64)phase*24u)/22050u);if(gf>=((beat&1u)*15u+15u))gf=(beat&1u)*15u+14u;if((!title_intro_active||title_ng_restored)&&(int)lf!=title_logo_last){setup_texture_dims(&tex,TITLE_LOGO_W,TITLE_LOGO_H);upload_raw(&tex,(g_title_logo_ready&(1u<<lf))?g_title_anim_pixels+(u32)lf*TITLE_LOGO_FRAME_BYTES:g_gf_pixels,TITLE_LOGO_W,TITLE_LOGO_H);title_logo_last=(int)lf;}if(!title_intro_active&&(int)gf!=title_gf_last){if(frame_no==0)capture_status("F0_TITLE_UPLOAD_BEGIN",-1,0);upload_raw(&gftex,(g_title_gf_ready&(1u<<gf))?g_title_anim_pixels+TITLE_LOGO_BYTES+(u32)gf*TITLE_GF_FRAME_BYTES:g_gf_pixels+TITLE_GF_STORE_OFFSET,TITLE_GF_W,TITLE_GF_H);if(frame_no==0)capture_status("F0_TITLE_UPLOAD_DONE",-1,0);title_gf_last=(int)gf;}}
  if(ui_state==7&&story_level==0&&g_story_anim_ready){u32 aq=(g_title_audio_fd>=0)?(u32)audsrv_queued():0u;u32 raw_heard=g_title_audio_submitted>aq/4u?g_title_audio_submitted-aq/4u:0u;if(raw_heard>g_story_anim_heard_samples)g_story_anim_heard_samples=raw_heard;u32 heard=g_story_anim_heard_samples;u64 units=(u64)heard*102u;u32 dance=(u32)(units/5760000u),phase=(u32)(((units%5760000u)*24u)/5760000u),gfi,bf;if(phase>14u)phase=14u;gfi=(dance&1u)?14u+phase:(phase?phase-1u:29u);bf=phase>13u?13u:phase;if((int)bf!=g_story_bf_uploaded&&tutorial_pack_frame(g_story_bf_pack,g_story_bf_pack_bytes,bf,14,164u*165u*4u,g_story_bf_pixels)==0){FlushCache(0);setup_texture_dims(&gftex,164,165);upload_raw(&gftex,g_story_bf_pixels,164,165);g_story_bf_uploaded=(int)bf;}if((int)gfi!=g_story_gf_uploaded&&tutorial_pack_frame(g_story_gf_pack,g_story_gf_pack_bytes,gfi,30,170u*157u*4u,g_story_gf_pixels)==0){FlushCache(0);setup_texture_dims(&tex2,170,157);upload_raw(&tex2,g_story_gf_pixels,170,157);g_story_gf_uploaded=(int)gfi;}}
  /* FocusCameraSongEvent.hx: char 0 is BF, 1 is the opponent, 2 is GF. The
     tutorial's opponent IS gf, so both 1 and 2 focus her and 0 focuses BF. */
  /* FOCO DA CAMERA POR PAPEL. O booleano de antes (`g_tut_cam_focus`) tratava
     "GF" e "DAD" como a mesma coisa, o que so' da' certo no Tutorial (onde o
     oponente E' a GF). Um chart que foca o oponente (char 1: 265 dos 664
     eventos FocusCamera dos 81 charts) mandava a camera para o ponto da GF --
     outro personagem, outra posicao. Agora o papel sai do valor do evento e o
     ponto sai do `ch_anchor` daquele papel.
     E o alvo so' troca se o personagem EXISTE na cena: FocusCameraSongEvent.hx
     L104-133 faz `return` quando `getDad()`/`getGirlfriend()` devolve null, ou
     seja, o evento nao mexe na camera. */
  {const chart_event_state_t*ev=chart_events_state();int c=ev->cam_char,papel=-1;
   if(c==CHART_FOCUS_BF)papel=CH_PAPEL_BF;
   else if(c==CHART_FOCUS_DAD)papel=CH_PAPEL_DAD;
   else if(c==CHART_FOCUS_GF)papel=CH_PAPEL_GF;
   if(papel>=0&&g_cena.p[papel].ativo)g_cam_papel=papel;
   g_tut_cam_focus=(g_cam_papel!=CH_PAPEL_BF)?1:0;}
  /* NAO HA' MAIS CARGA DE QUADRO AQUI. Este bloco subia os pacotes do Tutorial
     (`bf_frames.lz4f`, `gf_frames.lz4f`) para `tex` e `gftex16` a cada troca de
     quadro, em QUALQUER musica -- e esses dois enderecos sao exatamente os
     slots que o caminho generico usa para o BF e para a GF (`cena_carregar`).
     Medido no harness antes de remover: 282 subidas numa passada do `bopeebo`
     e 193 numa do `senpai`, escrevendo a arte do Tutorial por cima do
     personagem daquela musica. As 27 musicas agora usam o caminho generico
     (pacote CHP4 + `char_cena` + `ch_anchor`), que sobe o quadro quando ele
     muda e no endereco do proprio papel; este bloco nao tinha mais quem o
     desenhasse. */
  if(ui_state==6){frame_mark("F.BG",(int)menu_cam_y);qword_t*mq=packet->data;int tick=(int)(frame_no*24u/60u),mi;setup_texture_dims(&stagetex,512,256);memcpy(g_up_bounce,g_menu_pixels+MENU_FRAMES_BYTES,512u*256u*4u);upload_raw(&stagetex,g_up_bounce,512,256);mq=draw_setup_environment(mq,0,&fb[draw],&z);mq=gs_dither16(mq);mq=draw_clear(mq,0,0,0,SCREEN_W,SCREEN_H,0,0,0);mq=bind_texture(mq,&stagetex);{/* 0.2.104 A4: parallax do fundo, scrollFactor.y = 0,17 no original
     (MainMenuState.hx L119-120). Derivacao, em unidades do original:
       bg.y = -75,92   (setGraphicSize(1280*1,2)=1536 de largura, altura
                        730*1536/1286 = 871,83, screenCenter -> (720-871,83)/2)
       camera.scroll.y = cam_y - 360   (follow em camFollow, L343)
       bg na tela = bg.y - (cam_y-360)*0,17
     Passando para as unidades do porto (metade) e usando camoff=(cam_y-360)*0,4:
       bg_port = -37,96 - camoff*0,2125  =>  d(bg_port)/d(cam_y) = -0,085
     O coeficiente -0,085 e exato. A constante (-37,96 + 27,2 = -10,76 no
     repouso) NAO foi mexida: o fundo ja esta em y=0 ha 100 ciclos e a textura
     de fundo e 512x256 (proporcao 2:1) contra 1536x872 (1,76:1) do original,
     ou seja o descanso do fundo e uma aproximacao anterior. Mudar a constante
     agora arriscaria regressao visivel; fica registrado como pendencia. */
   /* 0.2.105 C5: geometria EXATA do fundo do original (MainMenuState.hx
      L121-126): setGraphicSize(Std.int(FlxG.width*1.2)) = 1536 de largura;
      altura = 730*1536/1286 = 871,9129 (menuBG.png 1286x730 medido em
      uploads/RELATORIO.json.txt); screenCenter() -> bg.y = (720-871,9129)/2
      = -75,95645; scrollFactor.y = 0,17. No porto (metade das unidades):
      y = -37,978227 - (cam_y-360)*0,085, altura 435,956454, x=-64, largura
      768. A 0.2.104 usava altura 360 (so a viewport) e descanso 0: com o
      parallax A4 o fundo passou a descobrir 50 linhas embaixo quando a
      camera esta em CREDITS (medido em uploads/9_FNF_SCREEN_0000: bg acaba
      na linha 341, scissor vai a 391) = "botoes saindo do cenario".
      Com a altura real a cobertura sobra nas duas pontas para TODO cam_y
      em [40,680] (tabela em tools/prova_fix_02105.py). */
   float bg_y=-37.978227f-(menu_cam_y-360.0f)*0.085f;
   /* O fundo tem de ficar DENTRO da area de jogo. Este pacote limpa a tela
      inteira para preto e so depois desenha o sprite -- sem scissor, o sprite
      de 768x435,96 em x=-64 cobre as duas barras e o letterbox desaparece.
      Era exatamente o sintoma medido nas capturas: 39 a 45 linhas pretas em
      448, quando o modo 4:3 precisa de 112 (56 em cima e 56 embaixo). O
      resultado na tela eh o fundo ocupando tudo e os botoes recortados, que
      lembra 4:3 esticado em vez do aspecto do original.
      A ordem importa: o scissor vem DEPOIS do clear, senao o clear respeitaria
      o scissor e as barras ficariam com o quadro anterior. */
   mq=draw_scissor_area(mq,0,0,SCREEN_W-1,SCISSOR_TOPO,SCISSOR_BASE);band_set(SCISSOR_TOPO,SCISSOR_BASE);
   /* O fundo mede 435,956 linhas virtuais, o que da 406,9 linhas fisicas:
      mais que a faixa visivel de 56..391. Recortado aqui em software porque
      o scissor do GS nao o esta cortando no console (ver band_clip_phys). */
   /* O recorte em si agora acontece dentro de sprite_region_phys, que eh o
      gargalo por onde passam TODOS os sprites -- assim um unico lugar responde
      por isso e o contador de linhas recortadas (que vai para o log) mede o
      que realmente saiu do GIF, nao uma conta paralela. */
   mq=sprite_region_phys(mq,-64.0f,vy(bg_y),768.0f,
                         vy(bg_y+435.956454f)-vy(bg_y),
                         0.0f,0.0f,512.0f,256.0f,0x80);
   /* 0.2.105 C6: o magenta do original (MainMenuState.hx L128-132) nao e um
      retangulo fixo na tela: menuBGMagenta herda scrollFactor, x, y e
      largura do bg. A 0.2.104 desenhava rect(0,0,640,360) fixo. Agora o
      flash acompanha o fundo exatamente. Cor (210,20,120): aproximarao
      rotulada - o asset menuBGMagenta.png nao esta no repo (assets/ vazio)
      nem no rar do Drive; a forma (retangulo cheio) e exata porque o
      menuBG.png e opaco em toda a area (ver prova_fix_02105.py). */
   if(g_fl_mag_on&&g_fl_mag_vis)mq=rect_alpha(mq,-64.0f,bg_y,768.0f,435.956454f,210,20,120,0x80);}mq=draw_finish(mq);dma_channel_send_normal(DMA_CHANNEL_GIF,packet->data,mq-packet->data,0,0);draw_wait_finish();if(!menu_placeholder&&tick!=menu_anim_tick){texbuffer_t*mt[5]={&fonttex,&tex,&gftex,&tex2,&stagetex};for(mi=0;mi<4;mi++){int sel=(mi==selection),af=sel?(tick%3):g_menu_idle_timeline[mi][tick%9];const unsigned int*fr=g_menu_frame_rw[mi][sel][af];setup_texture_dims(mt[mi],(int)fr[1],(int)fr[2]);{const unsigned char*srcu=(!sel&&(mi==1||mi==2))?menu_idle_blend(mi,af,tick%3):g_menu_pixels+fr[0];u32 bbw=(u32)fr[1]*(u32)fr[2]*4u;memcpy(g_up_bounce,srcu,bbw);upload_raw(mt[mi],g_up_bounce,(int)fr[1],(int)fr[2]);}}menu_anim_tick=tick;}
/* CREDITS fica na stagetex, que e a MESMA textura usada para o fundo
   do menu. O fundo e reenviado em todo quadro, logo acima, e apaga o
   CREDITS. Por isso este upload TEM de ficar fora do guarda de tick.
   Na 0.2.65 eu o movi para dentro achando que economizava 1,5 ms, e
   o resultado foi o CREDITS piscando a cada quadro. Revertido. */
if(!menu_placeholder){int sel4=(selection==4),af4=sel4?(tick%3):g_menu_idle_timeline[4][tick%9];const unsigned int*fr4=g_menu_frame_rw[4][sel4][af4];setup_texture_dims(&stagetex,(int)fr4[1],(int)fr4[2]);memcpy(g_up_bounce,g_menu_pixels+fr4[0],(u32)fr4[1]*(u32)fr4[2]*4u);upload_raw(&stagetex,g_up_bounce,(int)fr4[1],(int)fr4[2]);}}
  frame_mark("F.PACKET",(int)frame_no);
  if(frame_no==0){capture_status("F0_PACKET_BEGIN",-1,0);}
  /* SELECT+L1 grava a lista de desenho E a imagem do MESMO quadro.
     Sem isso eu recebo a lista de um quadro e a foto de outro, e a
     comparacao nao vale nada. */
  if(g_dl_armado){g_dl_armado=0;g_dl_gravando=1;g_dl_n=0;g_forcar_captura=1;}
  g_dl_estado=(unsigned char)ui_state;
  q=packet->data;q=draw_setup_environment(q,0,&fb[draw],&z);q=gs_dither16(q);/* NAO limpar no estado 6. O menu desenha o fundo num pacote proprio,
   que JA FOI ENVIADO ao GS antes deste. Limpar aqui apaga o fundo e
   deixa a tela preta. Foi o que eu causei na 0.2.82. */
if(ui_state!=6){q=draw_clear(q,0,0,0,SCREEN_W,SCREEN_H,(ui_state==0||ui_state==1||ui_state==6||ui_state==7||ui_state==8)?0:(24),(ui_state==0||ui_state==1||ui_state==6||ui_state==7||ui_state==8)?0:(18),(ui_state==0||ui_state==1||ui_state==6||ui_state==7||ui_state==8)?0:(40));}if(ui_state==0||ui_state==6||ui_state==7||ui_state==8){q=draw_scissor_area(q,0,0,SCREEN_W-1,SCISSOR_TOPO,SCISSOR_BASE);band_set(SCISSOR_TOPO,SCISSOR_BASE);}
  if(ui_state==7){const short*df=g_story_uv[1+story_diff];const short*la=g_story_uv[(g_held&PAD_LEFT)?6:4];const short*ra=g_story_uv[(g_held&PAD_RIGHT)?7:5];float dp=story_diff_tween>=5?1.0f:story_diff_tween/5.0f,dy=(232.5f+(245.0f-232.5f)*dp)-(df[3]-g_story_uv[2][3])*.5f;unsigned char da=(unsigned char)(128.0f*dp);q=draw_scissor_area(q,0,0,SCREEN_W-1,SCISSOR_TOPO,SCISSOR_BASE);q=rect(q,0,0,640,28,0,0,0);q=rect(q,0,28,640,228,249,207,81);q=rect(q,0,228,640,360,0,0,0);q=bind_texture_filter(q,&fonttex,1);g_story_vcr_vbase=0;q=story_vcr(q,"LEVEL SCORE: 0",5,5,0,128,128,128,128);g_story_vcr_vbase=0;q=story_vcr(q,g_story_names[story_level],635,5,2,128,128,128,90);g_story_vcr_vbase=0;q=story_vcr(q,"TRACKS",108,251,1,115,44,60,128);q=story_vcr(q,g_story_tracks[story_level],108,283,1,115,44,60,128);q=bind_texture_filter(q,&stagetex,0);q=sprite_region(q,435,240,la[2],la[3],la[0],la[1],la[2],la[3]);q=sprite_region_alpha(q,435+g_story_uv[4][2]+5+(g_story_uv[2][2]-df[2])*.5f,dy,df[2],df[3],df[0],df[1],df[2],df[3],da);if(story_diff_tween<5)story_diff_tween++;q=sprite_region(q,622.5f,240,ra[2],ra[3],ra[0],ra[1],ra[2],ra[3]);if(story_level==0&&g_story_anim_ready){q=bind_texture_filter(q,&tex2,1);q=sprite_dims(q,40.0f,40.0f,170,157,170,157);q=bind_texture_filter(q,&gftex,1);q=sprite_dims(q,235.0f,40.0f,164,165,164,165);}q=draw_scissor_area(q,0,0,SCREEN_W-1,0,SCREEN_H-1);}
  if(ui_state==0&&!title_intro_active){q=bind_texture(q,&tex);q=sprite_dims(q,-75.0f,-50.0f,469.5f,351.5f,TITLE_LOGO_W,TITLE_LOGO_H);q=bind_texture(q,&gftex);q=sprite_dims(q,256.0f,25.2f,360.5f,331.0f,TITLE_GF_W,TITLE_GF_H);}
  if(ui_state==6){q=draw_scissor_area(q,0,0,SCREEN_W-1,SCISSOR_TOPO,SCISSOR_BASE);int order,mi;float camoff=(menu_cam_y-360.0f)*0.4f;texbuffer_t*mt[5]={&fonttex,&tex,&gftex,&tex2,&stagetex};if(!menu_placeholder)for(order=0;order<5;order++){mi=(order==4)?selection:(order>=selection?order+1:order);if(mi>=5)continue;{int selected=(mi==selection),af=selected?((int)(frame_no*24u/60u)%3):g_menu_idle_timeline[mi][(frame_no*24u/60u)%9u];const unsigned int*fr=g_menu_frame_rw[mi][selected][af];float cy=(40.0f+160.0f*mi-camoff)*.5f;if(selected&&(!g_fl_item_vis||menu_exit_frames))continue;
  /* ANTIALIASING DO PRIMITIVO DESLIGADO.
     prim_t tem um campo antialiasing e o port NUNCA chamou
     draw_primitive_override_setting: esse bit sempre foi o padrao do
     ps2sdk e eu nunca olhei para ele.
     Com AA1 ligado o GS estende o primitivo em 1 pixel nas bordas e
     escreve alfa proporcional a COBERTURA. E exatamente a assinatura
     medida: na linha 316, de um quad que termina em 315, o alfa do
     framebuffer saiu 1, 2, 3, 7, 10, e so na largura do quad.
     Tambem explica por que o traco muda de forma a cada quadro da
     animacao e some quando o item troca de posicao: a cobertura
     depende do conteudo e da posicao subpixel.
     Vale so para estes sprites; desligo o override logo apos, para
     nao afetar os retangulos sem textura. */
  {prim_t pr;
   pr.type=PRIM_SPRITE;pr.shading=PRIM_SHADE_FLAT;pr.mapping=1;
   pr.fogging=0;pr.blending=1;pr.antialiasing=0;
   pr.mapping_type=PRIM_MAP_UV;pr.colorfix=PRIM_UNFIXED;
   q=draw_primitive_override_setting(q,0,&pr);
   q=draw_primitive_override(q,1);}
  q=bind_texture_filter(q,mt[mi],1);/* 0.2.104 C3: LINEAR, como o renderer do Lime no original.
   Os itens do menu saem de vy() com escala vertical 14/15 = 0,933333, que nao e
   inteira. Com NEAREST o GS descarta linhas de texel inteiras (medido: MERCH
   62 texels em 58 linhas, row 61 nunca aparece) e duplica outras, e a escolha
   depende da fase subpixel de dy. E isso que faz o traco tracejado na base. *//* Os itens do menu sao blit 1:1, origem e destino do mesmo tamanho.
   x = 320 - largura/2 cai em meio pixel sempre que a largura e
   impar, e e exatamente o caso dos tres itens com artefato:
     MERCH nao selecionado  193 -> x 223,5
     OPTIONS selecionado    305 -> x 167,5
     CREDITS selecionado    261 -> x 189,5
   Em meio pixel o amostrador cai na fronteira entre texels e duplica
   ou mistura a linha da borda, que na arte e o contorno preto: dai a
   risca clara ou escura na lateral e embaixo. O y tem o mesmo
   problema, porque cy depende de camoff, que e fracionario enquanto
   a camera do menu desliza. Arredondar o destino alinha os centros
   dos texels e o blit vira exato. */
{float dx=floorf(320.0f-fr[1]*.5f+.5f);float cyphys=vy(cy);
  /* REVERTIDO o desvio da 0.2.79. A conta de proporcao prova que o
     14/15 do vy() esta EXATO, nao e defeito:
       PS2 NTSC 640x448 numa TV 4:3 da pixel com PAR = (4/3)/(640/448)
       = 0.933333 = 14/15. Para 16:9 a altura tem de ser
       640 * 0.933333 * 9/16 = 336,0000 linhas, e vy() mapeia
       360 logicas em exatamente 336 fisicas. Erro: 0,0000%.
     Ao desligar isso eu deixei os botoes 7,14% altos demais, ou seja
     fui eu que quebrei a proporcao do menu. */
/* Recorte de 1 px em volta. Auditei o asset com tools/auditar_menu.py:
   os 30 quadros tem arte opaca encostando nas quatro bordas, e a nossa
   arte e FIEL a oficial (diferenca media de 1,88 em RGB contra o
   recorte oficial reduzido). Ou seja o traco nao esta no arquivo, e eu
   tambem ja descartei wrap de textura, que e CLAMP, e amostragem em
   meio pixel, que foi arredondada na 0.2.68. Nao consegui reproduzir a
   causa fora do console, entao ataco o sintoma: descartar a fileira
   mais externa de cada lado elimina qualquer efeito de borda, venha de
   onde vier, ao custo de 1 px de contorno, que e imperceptivel.
   O recorte de 3 linhas que havia so no MERCH some: ele cortava arte
   de verdade e nao resolvia nada. */
{unsigned char fa2=0x80;
   /* 0.2.105 C4: fade dos NAO selecionados = tween {alpha:0} de 0,4 s com
      FlxEase.quadOut (MainMenuState.hx L443): alpha(p) = (1-p)^2. O selected
      some ja (visible=false, L444). Na 0.2.104 o elapsed vinha de
      "66-menu_confirm_frames" com contador de 24: t sempre >1, fa2 sempre 0
      = os cinco botoes SUMIAM por 24 quadros (medido no console). */
   if(menu_exit_frames){float t=(float)(24-menu_exit_frames)/24.0f;float rr=1.0f-t;fa2=(unsigned char)(128.0f*rr*rr);}
   if(fa2){/* 0.2.102: o experimento de 5 canais da 0.2.101 respondeu: o unico
     tratamento sem linha na base foi o MERCH SEM RECORTE NENHUM. O recorte
     de 1 px vinha da 0.2.73 contra um defeito de borda que as correcoes de
     geometria (0.2.89) e de DMA (0.2.94) ja removeram. O original desenha o
     sprite inteiro; agora nos tambem. */
/* 0.2.105 C7: com o atlas ja reamostrado para hp linhas, o blit e 1:1 em
        linhas fisicas: topo = round(vy(cy) - hp/2). Nenhum texel e descartado
        ou duplicado, para qualquer fase de dy. */
{float y0r=floorf(cyphys-(float)fr[2]*0.5f+0.5f);
  /* Mesmo motivo do fundo: um item que deslizou para fora da faixa precisa
     ser descartado ou recortado dentro do helper, nao pelo scissor do GS.
     Sem isto, ao voltar do tutorial o menu mostra botoes inteiros flutuando
     fora da area de jogo, que e o defeito que o teste relatou. */
   q=sprite_region_phys(q,dx,y0r,(float)fr[1],(float)fr[2],
                        0.0f,0.0f,(float)fr[1],(float)fr[2],fa2);}}}
  q=draw_primitive_override(q,0);}}}if(menu_placeholder){q=rect(q,70,71,570,291,12,10,18);q=bind_texture_filter(q,&fonttex,1);q=text_draw(q,selection==1?"FREEPLAY":selection==2?"MERCH":selection==3?"OPTIONS":"CREDITS",320,136,1.4f,1);q=text_draw(q,"COMING IN NEXT STAGE",320,181,.9f,1);q=text_draw(q,"CIRCLE  RETURN",320,226,.9f,1);}
/* A marca de versao NAO pode ficar aqui. No estado 6 as cinco
   texturas guardam os cinco itens do menu, e mt[0] e justamente a
   fonttex, que carrega a imagem do item STORY MODE. Desenhar texto
   com ela produz lixo: foi a textura estranha no canto que apareceu
   na 0.2.71, diferente no Story porque la a fonttex tem a fonte de
   verdade. O menu principal simplesmente nao tem fonte carregada.
   Movida para o estado 7, onde a fonttex e a fonte. */
q=draw_scissor_area(q,0,0,SCREEN_W-1,0,SCREEN_H-1);}
  /* Clean Tutorial renderer foundation: no legacy colored geometry or test-note renderer. */

  q=bind_texture(q,&tex);
  /* M3.10: receptores e notas; Y deriva apenas da distancia em amostras. */
  /* Estado visual dos eventos reais: foco, zoom e PlayAnimation. */

  if(ui_state==0 && components_mode){
   for(i=0;i<5;i++)q=rect(q,130,78+i*62,510,128+i*62,i==settings_sel?95:45,i==settings_sel?105:50,i==settings_sel?155:75);
   q=rect(q,330,158,330+slider_value*15,174,245,70,150);
  }else if(ui_state==0){
   /* Official title atlas; no provisional menu rectangles. */
  }else if(ui_state==8&&g_tutorial_scene_ready){q=draw_scissor_area(q,0,0,SCREEN_W-1,SCISSOR_TOPO,SCISSOR_BASE);band_set(SCISSOR_TOPO,SCISSOR_BASE);/* Official character-space reconstruction. BFA2 canvases are already stored at half of the original 1280x720 character canvas, while stage positions and metadata offsets remain original-space values. Final top-left follows Stage.addCharacter/BaseCharacter.setScale: feet - full canvas origin + per-character global offset. Camera focus follows BaseCharacter.resetCameraFocusPoint plus stage camera offset. */{const chart_event_state_t*ev=chart_events_state();{u32 sp=g_play_samples_per_step;if(!sp)sp=TUT_SAMPLES_PER_STEP;
    /* Com a pausa aberta o relogio da musica esta' parado; passar dt=0
       mantem bop da camera e zoom do HUD parados, como no original, onde o
       substate suspende o update do PlayState. (O game over tem bloco de
       desenho proprio, entao nao chega aqui.) */
    chart_events_tick(g_paused?0.0f:TUT_DT,music_clock/sp);}g_tut_zoom=ev->zoom;g_tut_scroll=ev->scroll;g_tut_bop=ev->bop;
   /* PAUSA: no original o PlayState.update nao roda com substate aberto
      (shouldSubstatePause), entao nem o bop da camera nem o zoom do HUD
      decaem -- eles CONGELAM. Passar dt=0 e' a traducao direta disso. */
   {float hzm=ev->hud_zoom;if(hzm>g_hud_zoom_max)g_hud_zoom_max=hzm;
    /* Uma medicao por musica, no primeiro pulso: "o HUD pisca" deixa de ser
       afirmacao e vira numero no log do console (milesimos do zoom + quantos
       pulsos ja' aconteceram). */
    if(!g_hud_zoom_visto&&hzm>1.01f){g_hud_zoom_visto=1;
     telemetry_event("hud_zoom_pulso",(int)(hzm*1000.0f),(int)ev->hud_zoom_pulsos);}
    /* Segundo limiar: so' e' alcancado quando um SetCameraBop com intensidade
       maior que a padrao entra em vigor (spaghetti tem 103 deles, ate' 2,5x e
       uma batida por pulso). E' a prova de que o EVENTO chega ao HUD. */
    if(!g_hud_zoom_forte&&hzm>1.05f){g_hud_zoom_forte=1;
     telemetry_event("hud_zoom_forte",(int)(hzm*1000.0f),(int)ev->hud_zoom_pulsos);}
    g_hud_zoom=hzm;}}float sc=.5f*g_tut_zoom*g_tut_bop;/* Alturas de ORIGEM (1280x720) do BF e da GF, usadas abaixo para o foco de
   camera. As larguras e as escalas do pacote (`bf_source_w`, `bf_pack_scale`,
   `gf_source_w`, `gf_pack_scale`) saiam daqui e so' alimentavam dois cantos
   (`bf_world_x/y`, `gf_world_x/y`) que ninguem mais lia -- o canto do sprite
   hoje vem do `ch_anchor`. Saem com o compilador confirmando: eram as quatro
   variaveis nao usadas do aviso. */const float bf_source_h=490.920f,gf_source_h=674.850f;/* A GF ocupa o slot gf [751.5,787], provado pela captura do jogo real:
   a arte dela comeca em 6 px dentro da tela de 255x245 do pack, o que
   da 6/0,35 = 17,14 unidades de mundo. No slot dad isso cairia em
   -181,5 de tela, fora do ecra; no slot gf cai em +47,6, visivel.
   Nenhuma largura <= 728 faz o slot dad alcancar a posicao observada. */
/* gf.hxc escreve originalPosition = pos - characterOrigin e chama
   resetPosition(), que em Bopper.hx so faz x=originalPosition.x. Nao
   ha globalOffsets nessa linha, entao o -12/+4 do gf.json NAO entra
   na posicao do sprite. O foco de camera fica como esta: o script nao
   chama resetCameraFocusPoint e o teste de hardware aprovou. */
/* cameraFocusPoint = originalPosition + tamanho/2 + cameraOffsets, mais o
   deslocamento de camera do palco (-18,+36), que vale para todo personagem.
   BF: 989.5+17-18-100 = 888.50 ; 885+14-246+36-100 = 589.00  (inalterado)
   GF: 335-12-18+150  = 455.00 ; 885+4-338+36-100  = 487.00 */
float bf_cam_x=g_official_mainstage_chars[2].x+17.0f-18.0f+g_official_mainstage_chars[2].sx,bf_cam_y=g_official_mainstage_chars[2].y+14.0f-bf_source_h*.5f+36.0f+g_official_mainstage_chars[2].sy;/* cameraFocusPoint = (position + offsets) + char.cameraOffsets +
   slot.cameraOffsets. Reproduz o BF digito a digito:
   (989,5+17)-18-100 = 888,50 ; (885+14-246)+36-100 = 589,00.
   GF no slot gf: (751,5-12+12)+0 = 751,50 ;
                  (787+4-338+8)+0 = 461,00 */
float gf_cam_x=g_official_mainstage_chars[0].x+GF_OFFSET_X+GF_CAMOFF_X+g_official_mainstage_chars[0].sx,gf_cam_y=g_official_mainstage_chars[0].y+GF_OFFSET_Y-gf_source_h*.5f+GF_CAMOFF_Y+g_official_mainstage_chars[0].sy; /* Modo generico: o foco da camera vem da MESMA formula do char_layout
    (position + offsets + cameraOffsets do personagem + do slot), que e' a do
    BaseCharacter.resetCameraFocusPoint e ja' reproduz o BF do mainStage digito
    a digito. No tutorial nada muda: continua o caminho conferido no console. */
{const bs_slot_t*slb=ch_slot(g_play_song?g_play_song->stage:"mainStage","bf");
 const bs_slot_t*slg=ch_slot(g_play_song?g_play_song->stage:"mainStage","gf");
 const bs_slot_t*sld=ch_slot(g_play_song?g_play_song->stage:"mainStage","dad");
 dad_cam_x=bf_cam_x;dad_cam_y=bf_cam_y;
 if(g_cena_modo&&slb){ch_anchor_t an;float cw=0.0f,chh=0.0f;cena_canvas_mundo(CH_PAPEL_BF,slb,&cw,&chh);ch_anchor(&ch_chars[g_cena_bf_ci],slb,cw,chh,0.0f,1,&an);bf_cam_x=an.cam_x;bf_cam_y=an.cam_y;}
 if(g_cena_modo&&slg&&g_cena.p[CH_PAPEL_GF].ativo){ch_anchor_t an;float cw=0.0f,chh=0.0f;cena_canvas_mundo(CH_PAPEL_GF,slg,&cw,&chh);ch_anchor(&ch_chars[g_cena_gf_ci],slg,cw,chh,0.0f,1,&an);gf_cam_x=an.cam_x;gf_cam_y=an.cam_y;}
 if(g_cena_modo&&sld&&g_cena.p[CH_PAPEL_DAD].ativo){ch_anchor_t an;float cw=0.0f,chh=0.0f;cena_canvas_mundo(CH_PAPEL_DAD,sld,&cw,&chh);ch_anchor(&ch_chars[g_cena_dad_ci],sld,cw,chh,0.0f,1,&an);dad_cam_x=an.cam_x;dad_cam_y=an.cam_y;}}
float tcx,tcy,cx,cy;
/* A CAMERA COMECA NO OPONENTE, nao no jogador. PlayState.hx L2224-2227:
   `currentStage.addCharacter(dad, DAD); cameraFollowPoint.setPosition(dad
   .cameraFocusPoint.x, dad.cameraFocusPoint.y);` -- e logo depois o L936 chama
   resetCamera(), que SNAPA a camera nesse ponto. Antes o port comecava no BF e
   so' ia para o oponente no primeiro evento, entao a contagem inicial de toda
   musica era enquadrada no lado errado da tela. Sem oponente em cena (nao
   acontece nos 27 dados, mas e' o caso que os scripts do original tratam), cai
   no primeiro papel ativo, na mesma ordem do initCharacters. */
if(!g_tut_cam_ready){
 if(g_cena.p[CH_PAPEL_DAD].ativo)g_cam_papel=CH_PAPEL_DAD;
 else if(g_cena.p[CH_PAPEL_GF].ativo)g_cam_papel=CH_PAPEL_GF;
 else g_cam_papel=CH_PAPEL_BF;
}
{const chart_event_state_t*ev=chart_events_state();
 /* char -1 e' o proprio (x,y): FocusCameraSongEvent L102-103 nao soma personagem
    nenhum. Nos outros valores x/y sao OFFSET do ponto daquele personagem
    (L104-133). Antes o port ignorava x/y -- 123 dos 664 eventos dos 81 charts
    tem offset, e 86 focam uma posicao fixa. */
 if(g_cam_papel==CH_PAPEL_GF){tcx=gf_cam_x;tcy=gf_cam_y;}
 else if(g_cam_papel==CH_PAPEL_DAD){tcx=dad_cam_x;tcy=dad_cam_y;}
 else{tcx=bf_cam_x;tcy=bf_cam_y;}
 tcx+=ev->cam_x;tcy+=ev->cam_y;
 if(ev->cam_char==CHART_FOCUS_STATIC){tcx=ev->cam_x;tcy=ev->cam_y;}
 if(g_cam_seq<0)g_cam_seq=(int)ev->cam_seq;
 else if((int)ev->cam_seq!=g_cam_seq){
  g_cam_seq=(int)ev->cam_seq;
  g_cam_from_x=g_tut_cam_x;g_cam_from_y=g_tut_cam_y;
  g_cam_to_x=tcx;g_cam_to_y=tcy;
  g_cam_dur=ev->cam_dur_s;g_cam_ease=ev->cam_ease;g_cam_dir=ev->cam_ease_dir;
  if(ev->cam_mode==CHART_FOCUS_MODE_INSTANT){
   /* tweenCameraToPosition(x,y,0) -> resetCamera(...,snap=true): a camera
      chega no ponto no mesmo quadro. */
   g_cam_t=1.0f;g_tut_cam_x=tcx;g_tut_cam_y=tcy;
  }else if(ev->cam_mode==CHART_FOCUS_MODE_TWEEN&&g_cam_dur>0.0f){
   g_cam_t=0.0f;
  }else{
   /* CLASSIC: o ponto de foco muda e a perseguicao leva a camera. */
   g_cam_t=1.0f;
  }
  /* O ALVO, medido. Sem ele, "a camera esta' no ponto do original" nao tem
     numero: o y do foco carrega meia altura de canvas, e era justamente essa
     metade que faltava quando o ch_anchor recebia canvas 0x0. */
  telemetry_event("cam_foco_alvo",(int)tcx,(int)tcy);
  if(ev->cam_mode>=0&&ev->cam_mode<3&&!g_cam_modo_visto[ev->cam_mode]){
   g_cam_modo_visto[ev->cam_mode]=1;
   telemetry_event(ev->cam_mode==CHART_FOCUS_MODE_INSTANT?"cam_foco_instant":
                   (ev->cam_mode==CHART_FOCUS_MODE_TWEEN?"cam_foco_tween":"cam_foco_classic"),
                   (int)ev->cam_char,(int)ev->cam_ease);
  }
 }}if(!g_tut_cam_ready){
 /* A camera so' NASCE quando a cena dos personagens esta' carregada
   (`g_cena_estado` != 0). Antes disso os papeis ainda estao inativos e o papel
   escolhido sairia BF: medido, `cam_foco_inicial,1,589` -- e o 589 e' o valor
   fixo antigo do mainStage, nao o foco de ninguem em cena. Nos quadros
   anteriores o ponto calculado continua servindo para desenhar; so' o
   nascimento (e o papel escolhido) espera a cena. */
 g_tut_cam_x=tcx;g_tut_cam_y=tcy;
 if(g_cena_estado!=0){
  g_tut_cam_ready=1;
  /* O papel E o ponto onde a camera nasceu. O segundo numero e' o que se
     confere contra o original: ele carrega a metade do canvas do personagem. */
  telemetry_event("cam_foco_inicial",g_cam_papel,(int)g_tut_cam_y);
 }
}
else if(g_cam_t<1.0f){
 /* FlxTween sobre camera.scroll: posicao = from + (to-from)*ease(t), com
    t avancando dt/duracao. chart_ease_eval e' a mesma tabela do zoom. */
 float k;
 g_cam_t+=TUT_DT/g_cam_dur;if(g_cam_t>1.0f)g_cam_t=1.0f;
 k=chart_ease_eval(g_cam_ease,g_cam_dir,g_cam_t);
 g_tut_cam_x=g_cam_from_x+(g_cam_to_x-g_cam_from_x)*k;
 g_tut_cam_y=g_cam_from_y+(g_cam_to_y-g_cam_from_y)*k;
}else{g_tut_cam_x+=(tcx-g_tut_cam_x)*TUT_CAM_LERP;g_tut_cam_y+=(tcy-g_tut_cam_y)*TUT_CAM_LERP;}
/* O TREMOR desloca a CAMERA, nao o mundo: e' o que o FlxCamera.shake faz. A
   amplitude vem em pixels da tela da camera (1280x720); para virar deslocamento
   de camera basta dividir pela escala com que o mundo e' desenhado. O HUD nao
   treme -- ele esta' em outra camera no original tambem. */
 cena_shake_passo();
cx=g_tut_cam_x;cy=g_tut_cam_y;
 if(sc>0.0f){cx-=g_shake.x/sc;cy-=g_shake.y/sc;}
float x,y,w,h;
   /* PALCO: os props do mainStage, um por um, do pacote para o pool.
      O caminho antigo (a textura composta de 512x256 que cobre a viagem toda
      da camera) continua aqui como RESERVA: se um pacote nao abrir ou nao
      couber na VRAM, o tutorial desenha o palco como antes em vez de ficar sem
      fundo. O evento palco_falha diz qual prop falhou e por que. */
   {float st=(g_chart_clock<0?0.0f:(float)g_chart_clock/48000.0f);
    /* O passo do chart (uma batida = 4 passos) e a duracao da batida na secao
       de BPM vigente: sao eles que decidem a animacao de cada prop do palco
       (Bopper.hx). Antes da musica o passo e' negativo de proposito -- e' a
       janela em que vale o `startingAnimation` do prop. */
    {s32 spv=(s32)cena_spv();
     int bt=cena_passo();
     float bts=(float)(4.0f*(float)spv/48000.0f);
     /* O chart_events converte duracao de evento (em PASSOS) para segundos com o
        passo que recebeu; ele tem de ser o da secao vigente, nao o do comeco. */
     chart_events_init((u32)spv);
    /* A carga e' conferida AQUI, e nao so' na transicao: o `g_play_song` pode
       chegar depois dela (no caminho do harness por musica, por exemplo), e o
       desenho era o unico lugar que sempre sabe qual e' a musica. A funcao sai
       na hora quando a musica nao mudou. */
   if(ui_state==8)cena_carregar(g_play_song);
   /* ORDEM DE DESENHO: props e personagens na MESMA lista, por zIndex.
      Stage.hx L300-330 poe os tres personagens (`addCharacter`) e os props
      (`addBopper`) num unico grupo ordenado por `zIndex` -- nao existe "o
      palco primeiro e as pessoas por cima". A posicao de cada personagem
      nessa lista e' o z do slot daquele palco (bs_slots[]), e ela muda de
      palco para palco. Medido nos dados, 11 palcos tem props acima do menor
      z dos personagens: os 7 `tankmanAudience` (z 1000..1100) do
      tankmanBattlefield, o `limo` do limoRide (150) e o `santa` do mallXmas
      (210) ficam NA FRENTE de quem canta. Desenhar todo prop antes dos
      personagens punha os figurantes do tankman atras do BF.
      A ordem DENTRO de cada faixa continua a do sl_collect (z crescente e
      insercao estavel). O caminho legado do Tutorial, conferido em captura de
      console, nao muda: ele cai no else abaixo, com a ordem antiga. */
   if(g_cena_modo){
    /* GF, BF e o oponente pelo mesmo caminho: quadro de `cc_quadro`, posicao
       do `ch_anchor` no slot do palco. O upload so' acontece quando o quadro
       MUDA -- e' o custo real (LZ4 + VRAM). */
    const char *stg=g_play_song?g_play_song->stage:"mainStage";
    int bt2=cena_passo();     /* o passo do chart: e' ele que decide quando dancar */
    long ms2=(long)(g_chart_clock<0?0:(u32)(g_chart_clock/48));
    int zz[3],ord[3]={CH_PAPEL_GF,CH_PAPEL_BF,CH_PAPEL_DAD},ai,bi,limite;
    palco_lista_t pl;
    /* O zIndex do SLOT do palco, ou o que o NOTE KIND mandou (moveToBack/Front).
       O script faz `this.zIndex = 2000/3000; currentStage.refresh()`, e o
       refresh reordena o grupo -- ou seja, a troca VALE dali em diante e nao
       volta sozinha. Antes isto era dado lido e nao aplicado: os 49 pares com
       `ordem` (o Darnell do blazin inteiro) desenhavam sempre no z do slot. */
    /* ATENCAO a ORDEM DESTE VETOR: ele anda junto com ord[] na ordenacao
       (o laco troca zz[i] e ord[i] de lugar juntos), entao zz[i] tem de ser o
       z do papel ord[i] -- GF, BF, DAD. Estava GF, DAD, BF: o z do DAD era
       aplicado ao BF e vice-versa, e os dois trocavam de lugar em TODO palco
       onde os z diferem. Erro de leitura de dado, nao de desenho: o numero
       certo estava la', preso ao papel errado. */
    zz[0]=g_z_extra[CH_PAPEL_GF]?g_z_extra[CH_PAPEL_GF]:palco_slot_z(stg,"gf");
    zz[1]=g_z_extra[CH_PAPEL_BF]?g_z_extra[CH_PAPEL_BF]:palco_slot_z(stg,"bf");
    zz[2]=g_z_extra[CH_PAPEL_DAD]?g_z_extra[CH_PAPEL_DAD]:palco_slot_z(stg,"dad");
    /* z crescente, como o `FunkinGroup.refresh()` (SortUtil.byZIndex) que o
       Stage chama depois de adicionar todo mundo. A ordem de adicao do
       PlayState e' GF (L2207), BF (L2216) e DAD (L2225), e ela e' o desempate
       aqui porque o corte e' estavel. */
    for(ai=1;ai<3;ai++){int zi=zz[ai],zo=ord[ai];
     for(bi=ai-1;bi>=0&&zz[bi]>zi;bi--){zz[bi+1]=zz[bi];ord[bi+1]=ord[bi];}
     zz[bi+1]=zi;ord[bi+1]=zo;}
    palco_abre(&pl,stg,cx,cy,g_tut_zoom,g_tut_bop,bt,bts,st);
    /* A reserva composta e' o FUNDO: quando entra, vai antes de todos. */
    if(palco_reserva(&pl,stg)){q=bind_texture_filter(q,&stagetex,1);{float stx=320.0f+(STAGE_WIN_X-cx*STAGE_SCROLL+640.0f*STAGE_SCROLL-640.0f)*sc,sty=180.0f+(STAGE_WIN_Y-cy*STAGE_SCROLL+360.0f*STAGE_SCROLL-360.0f)*sc;q=sprite_dims(q,stx,sty,STAGE_WIN_W*sc,STAGE_WIN_H*sc,512,256);}}
    /* [props z < gf] gf [props z < dad] dad [props z < bf] bf [props z >= bf] */
    limite=-PALCO_Z_FIM;
    for(ai=0;ai<3;ai++){
     const char *rp=ord[ai]==CH_PAPEL_GF?"gf":ord[ai]==CH_PAPEL_DAD?"dad":"bf";
     q=palco_fatia(q,&pl,limite,zz[ai]);
     q=cena_desenha(q,ord[ai],ch_slot(stg,rp),bt2,ms2,cx,cy,sc);
     limite=zz[ai];
    }
    q=palco_fatia(q,&pl,limite,PALCO_Z_FIM);
    palco_fecha(&pl);
   }else{
    /* Tutorial: o mesmo caminho de antes, agora com a lista por z do palco --
       props, e so' depois o oponente e os dois sprites do BF/GF. */
    const char *stg0=g_play_song?g_play_song->stage:"mainStage";
    palco_lista_t pl;
    palco_abre(&pl,stg0,cx,cy,g_tut_zoom,g_tut_bop,bt,bts,st);
    if(palco_reserva(&pl,stg0)){q=bind_texture_filter(q,&stagetex,1);{float stx=320.0f+(STAGE_WIN_X-cx*STAGE_SCROLL+640.0f*STAGE_SCROLL-640.0f)*sc,sty=180.0f+(STAGE_WIN_Y-cy*STAGE_SCROLL+360.0f*STAGE_SCROLL-360.0f)*sc;q=sprite_dims(q,stx,sty,STAGE_WIN_W*sc,STAGE_WIN_H*sc,512,256);}}
    q=palco_fatia(q,&pl,-PALCO_Z_FIM,PALCO_Z_FIM);
    palco_fecha(&pl);
    q=cena_desenha(q,CH_PAPEL_DAD,ch_slot(g_play_song?g_play_song->stage:"mainStage","dad"),
                   bt,(long)(g_chart_clock<0?0:(u32)(g_chart_clock/48)),cx,cy,sc);
   }
   }
   }
  }
  if(ui_state==8&&g_tutorial_scene_ready){q=tutorial_notes_draw(q,&tex2,&fonttex,(u32)g_chart_clock,g_held,frame_no);q=splash_draw(q,&tex2,(u32)g_chart_clock);q=countdown_draw(q,&fonttex);/* Official HUD core at half of 1280x720 camHUD coordinates. healthBarBG=(339.5,648), inner bar=(343.5,652), health=1.0 gives 50% red/green, icons use HealthIcon size 150 and POSITION_OFFSET 26. */q=bind_texture_filter(q,&tex2,1);q=sprite_region(q,hz_x(169.75f),hz_y(324.0f),hz_s(300.5f),hz_s(9.5f),0,112,301,10);{/* A barra oficial e' um FlxBar RIGHT_TO_LEFT de largura 593 (601 do fundo
     menos 8) e valor 0..2 (HEALTH_MAX), verde do jogador preenchido a partir
     da DIREITA. Em 640x360, x0=171.75 e largura 296.5, entao a divisao esta'
     em x1 - largura*(health/2). Com health=1,0 isso da' 320.0 -- que era o
     numero fixo que estava aqui e so' valia no primeiro quadro. */
    /* A barra usa a vida INTERPOLADA (healthBar.value = healthLerp), nao a
       crua: o original nunca mostra o valor instantaneo. */
    float hf=g_vida_lerp*0.5f,sx=468.25f-296.5f*hf;
    if(sx<171.75f)sx=171.75f;
    if(sx>468.25f)sx=468.25f;
    if(sx>171.75f){
        q=rect_alpha(q,hz_x(171.75f),hz_y(326.0f),hz_x(sx),hz_y(331.5f),255,0,0,128);
    }
    if(sx<468.25f){
        q=rect_alpha(q,hz_x(sx),hz_y(326.0f),hz_x(468.25f),hz_y(331.5f),102,255,51,128);
    }}/* Os dois icones tambem sao camHUD (PlayState.hx L2160, L2195). A arte vem do
   pacote de icone de cada personagem (assets/icons/<id>.icp), e nao mais de um
   pedaco do atlas do tutorial: o rosto e' o do personagem que esta' cantando. */
 q=icone_desenha(q,g_vida_lerp);/* scoreText: FlxBitmapText defaults autoSize=true and fieldWidth=0; official FlxBitmapText states alignment has no visual effect in autoSize mode. Therefore glyph origin is exactly x=(339.5+601-190)/2=375.25, not a 190px field ending at 470.25. Four half-pixel black passes reconstruct borderSize=1 before the white glyph pass. */{/* PlayState.updateScoreText(): "Score: " + FlxStringUtil.formatMoney(
     songScore, SHOW_DECIMALS=false, COMMA_SEPARATED=true). Sem decimais e com
     virgula de milhar, entao 1500 vira "1,500". O texto era literalmente
     "Score: 0" e nunca mudava. */
    char sb[32];int sl=(int)fmt_score_line(sb,(int)sizeof(sb),(int)g_play.score);
    q=bind_texture_filter(q,&fonttex,1);
    q=hud_score_line(q,sb,sl,374.75f,339.0f,0,0,0,0,128);
    q=hud_score_line(q,sb,sl,375.75f,339.0f,0,0,0,0,128);
    q=hud_score_line(q,sb,sl,375.25f,338.5f,0,0,0,0,128);
    q=hud_score_line(q,sb,sl,375.25f,339.5f,0,0,0,0,128);
    q=hud_score_line(q,sb,sl,375.25f,339.0f,0,128,128,128,128);}
/* MARCA DE VERSAO, no lugar certo desta vez.
   O oficial mostra Constants.VERSION em exatamente dois pontos:
     MainMenuState.hx:321  leftWatermarkText  canto inferior ESQUERDO
     PlayState.hx:965      rightWatermarkText canto inferior DIREITO
   e nunca no Story Menu, que foi onde eu a pus por engano na 0.2.72.
   MusicBeatState.hx:196-204 define y = FlxG.height - 18 = 702, que em
   640x360 vira 351, com zIndex 100000, ou seja sempre por cima.
   O menu principal nao pode receber texto: as cinco texturas estao
   ocupadas pelos itens e a fonttex carrega a imagem do STORY MODE.
   Aqui no Tutorial a fonttex e a fonte de verdade, entao a versao vai
   para o canto inferior direito, como o PlayState faz. y=345 e o
   limite: o scissor do Tutorial termina em 391 fisico, que e 347
   logico. Desenhada por ultimo, logo fica por cima de tudo. */
#define TUT_VER_TXT "PS2 " FNF_PS2_VERSION
q=hud_score_line(q,TUT_VER_TXT,(int)(sizeof(TUT_VER_TXT)-1),632.5f,345.5f,2,0,0,0,128);
q=hud_score_line(q,TUT_VER_TXT,(int)(sizeof(TUT_VER_TXT)-1),632.0f,345.0f,2,128,128,128,128);
q=draw_scissor_area(q,0,0,SCREEN_W-1,0,SCREEN_H-1);}
  q=bind_texture_filter(q,&fonttex,1);
  /* FREEPLAY (ui_state 11).
     O original mostra as capsules numa grade de 5 colunas com a musica
     selecionada em destaque, o titulo, o artista e a dificuldade. Sem a arte
     das capsules no pacote, a tela desenha a LISTA com a mesma informacao:
     nome, artista, BPM e a dificuldade escolhida, com a musica atual em
     destaque. A janela de 9 linhas existe porque sao 27 musicas e nao cabe
     tudo na tela. */
  if(ui_state==11){
   char lb[96];int i,q0=g_frame_texture_quads,ini,ndes=0;
   /* ===== O DJ (`AnimateAtlasFreeplayDJ`) ===== */
   freeplay_dj_passo();
   static const char*dn[3]={"EASY","NORMAL","HARD"};
   int passo,qsel,qnao;
   texbuffer_t tsel=g_fptex_sel, tnao=g_fptex_nao;
   unsigned long ms;
   {u64 agora=GetTimerSystemTime();g_fp_ms=(int)(((agora-g_fp_t0)*1000ull)/(u64)kBUSCLK);}
   ms=(unsigned long)g_fp_ms;
   if(g_fp_sel<0)g_fp_sel=0;
   if(g_fp_sel>=g_fp_n)g_fp_sel=g_fp_n?g_fp_n-1:0;
   q=draw_scissor_area(q,0,0,SCREEN_W-1,SCISSOR_TOPO,SCISSOR_BASE);
   band_set(SCISSOR_TOPO,SCISSOR_BASE);
   /* O FUNDO da Freeplay (`backingImage`, `freeplayBGweek1-bf`). Ele NASCE fora
      da tela, em `x = -width`, e entra ate' 0 em 700 ms com `quintOut`
      (`FreeplayState.hx` L529-531); a altura e' a da tela
      (`setGraphicSize(0, FlxG.height + 1)`, L456), o que da' 574x360 aqui --
      821x516 na arte, com a largura vindo da proporcao. Fica atras de tudo.
      Sem o arquivo (ou sem VRAM), o retangulo escuro mantem a tela legivel e a
      telemetria diz qual dos dois faltou. */
   if(g_fpf_ok){
    q=bind_texture_filter(q,&g_fpf_tex,0);
    q=sprite_region(q,fp_fundo_x(ms),FP_FUNDO_Y,
                    (float)FP_FUNDO_W,(float)FP_FUNDO_H,
                    0.0f,0.0f,(float)FP_FUNDO_W,(float)FP_FUNDO_H);
    if(!g_fpf_logado){g_fpf_logado=1;
     telemetry_event("freeplay_fundo_desenho",(int)(fp_fundo_x(ms)*10.0f),ms);}
   }else{
    q=rect_alpha(q,0.0f,0.0f,(float)SCREEN_W,360.0f,8,8,16,255);
   }
   /* O DJ (`AnimateAtlasFreeplayDJ`), DEPOIS do fundo e ANTES das capsulas: e'
      a ordem em que o `FreeplayState.create()` adiciona os dois (`add(backingImage)`
      em L475, `add(dj)` em L423). A posicao vem de `resetPosition()`: com
      `useAnimatePosition` (o bf) e' (0,0), porque `CUTOUT_WIDTH` e' 0 no PS2.
      Um quadro por vez na VRAM, 16 bpp (277.500 B). */
   if(g_dj_ok){
    int qd=freeplay_dj_indice(freeplay_dj_anim(
             g_dj_estado==FP_DJ_INTRO?"intro":
             g_dj_estado==FP_DJ_CONFIRM?"confirm":
             g_dj_estado==FP_DJ_EGG?"idleEasterEgg":
             g_dj_estado==FP_DJ_CARTOON?"cartoon":
             (g_dj_estado==FP_DJ_FIST||g_dj_estado==FP_DJ_LOOP_PUMP)?"fistPump":
             (g_dj_estado==FP_DJ_LOSS||g_dj_estado==FP_DJ_LOOP_LOSS)?"loss":"idle"),
             g_dj_t_estado);
    /* Os estados de fist pump decidem o quadro por JANELA (`freeplay_dj_passo`). */
    if(g_dj_forcado>=0)qd=g_dj_forcado;
    if(qd>=0&&qd!=g_dj_frame){
     int rcq=pm_quadro(&g_dj_pack,(unsigned int)qd,g_dj_buf);
     if(rcq!=0){
      /* Um quadro que nao descomprime nao pode ser silencio: sem este evento
         o DJ simplesmente nao aparece e o log nao diz por que. */
      if(!g_dj_logado){g_dj_logado=1;
       telemetry_event("freeplay_dj_quadro_falhou",qd,rcq);}
     }else{
      g_dj_tex.width=FP_DJ_W;g_dj_tex.psm=GS_PSM_16;g_dj_tex.address=g_dj_addr;
      setup_texture_dims(&g_dj_tex,FP_DJ_W,FP_DJ_H);
      FlushCache(0);
      upload_raw_psm(&g_dj_tex,g_dj_buf,FP_DJ_W,FP_DJ_H,GS_PSM_16);
      g_dj_frame=qd;
     }
    }
    if(g_dj_frame>=0){
     q=bind_texture_filter(q,&g_dj_tex,1);
     q=sprite_region(q,FP_DJ_X,FP_DJ_Y,(float)FP_DJ_W,(float)FP_DJ_H,
                     0.0f,0.0f,(float)FP_DJ_W,(float)FP_DJ_H);
     g_dj_desenhos++;
     if(!g_dj_logado){g_dj_logado=1;
      telemetry_event("freeplay_dj_desenho",g_dj_estado,g_dj_frame);}
    }
   }
   /* OS DOIS QUADROS DE CAPSULA na VRAM. A animacao e' de 24 fps e e' loopada.
      Todas as capsulas andam JUNTAS: cada `updateSelected` reinicia a animacao
      de todas, entao a fase e' uma so' (e a VRAM guarda um quadro de cada). */
   passo=fp_passo_salto(ms);
   qsel=fp_quadro(1,ms);
   qnao=fp_quadro(0,ms);
   freeplay_capsula_quadro(&stagetex,0,qnao);
   freeplay_capsula_quadro(&stagetex,1,qsel);
   /* A JANELA da lista: so' as capsulas que cabem na tela entram no laco. */
   ini=g_fp_sel-FP_JANELA/2;
   if(ini<0)ini=0;
   if(ini>g_fp_n-FP_JANELA&&g_fp_n>FP_JANELA)ini=g_fp_n-FP_JANELA;
   if(ini<0)ini=0;
   for(i=ini;i<ini+FP_JANELA&&i<g_fp_n;i++){
    int sel=(i==g_fp_sel);
    fp_cap_st *st=&g_fpcs[i];
    float ax=fp_x(i-g_fp_sel),ay=fp_y_item(i+1,g_fp_sel),sx,sy,ox;
    float alpha=(ms<(unsigned long)FP_ENTRADA_ESCONDE_MS)?0.5f:1.0f;
    const fp_titulo_t *tit;
    texbuffer_t tt;
    /* `generateSongList`: nasce em `initPosition(FlxG.width, 0)` com
       `y = intendedY(index) + 10` -- entra pela direita e sobe/desce. */
    if(!st->vivo){st->vivo=1;st->x=FP_LARGURA;st->y=ay+10.0f;st->sel=sel;}
    if(st->sel!=sel){
     /* `updateSelected()` reinicia a animacao e chama `textAppear()`. */
     st->sel=sel;st->t_texto=(unsigned long)g_fp_ms;st->texto_ok=1;
    }
    st->x=fp_suave_x(st->x,ax,1.0f/60.0f);
    st->y=fp_suave_y(st->y,ay,1.0f/60.0f);
    /* O salto de entrada: `scale.x = xFrames[passo]`, `scale.y = 1/xFrames[passo]`
       (L687-688) -- e o passo e' de 1/24 s, 7 vezes. */
    sx=fp_escala_x(passo);sy=fp_escala_y(passo);
    if(!st->texto_ok&&ms>=(unsigned long)FP_ENTRADA_ESCONDE_MS){
     /* `setVisibleGrp(true)` no timer de 729 ms: o texto aparece e faz o
        `textAppear()` (1,7;0,2 -> 0,4;1,4 -> 1;1). */
     st->texto_ok=1;st->t_texto=(unsigned long)g_fp_ms;
    }
    if(g_fpc_ok){
     float dx=fp_desenho_w(sx),dy=fp_desenho_h(sy);
     texbuffer_t ct=(sel?tsel:tnao);
     /* L795: `capsule.offset.x = isSelected ? 0 : -5` (metade: -2,5). */
     ox=sel?0.0f:-2.5f;
     q=bind_texture_filter(q,&ct,1);
     q=sprite_region_alpha(q,st->x+ox,st->y,dx,dy,0.0f,0.0f,
                           (float)FP_CAP_W,(float)FP_CAP_H,
                           (unsigned char)(alpha*255.0f+0.5f));
     ndes++;
    }
    if(!st->texto_ok)continue;
    tit=freeplay_titulo(i,&tt);
    if(tit){
     float tw=(float)tit->w,th=(float)tit->h,u=0.0f,tsx,tsy;
     unsigned char ta=(unsigned char)((sel?1.0f:0.6f)*alpha*255.0f+0.5f);
     fp_texto_escala((unsigned long)g_fp_ms-st->t_texto,&tsx,&tsy);
     if(tw>FP_TEXTO_CLIP){
      float larga=tw-FP_TEXTO_CLIP;
      u=sel?freeplay_letreiro((unsigned long)g_fp_ms-st->t_texto,larga):0.0f;
      if(u<0.0f)u=0.0f;
      if(u>larga)u=larga;
      tw=FP_TEXTO_CLIP;
     }
     q=bind_texture_filter(q,&tt,1);
     q=sprite_region_alpha(q,st->x+FP_TEXTO_X,st->y+FP_TEXTO_Y,tw*tsx,th*tsy,
                           u,0.0f,tw,th,ta);
    }
   }
   /* A faixa de baixo. O `FreeplayScore`/as estrelas de dificuldade do original
      ainda nao estao portados; o que esta' aqui e' a informacao que a tela ja'
      mostrava (dificuldade, musica e posicao na lista) e os controles. */
   {const sr_song_t*song=g_fp_n?g_fp[g_fp_sel]:0;
    if(song){
     int bl=(g_fp_diff<0?0:(g_fp_diff>2?2:g_fp_diff));
     q=rect_alpha(q,0.0f,300.0f,100.0f,344.0f,16,16,28,225);
     snprintf(lb,sizeof(lb),"%s  %d/%d",dn[bl],g_fp_sel+1,g_fp_n);
     q=text_draw(q,lb,56.0f,314.0f,0.62f,1);
    }}
   q=draw_scissor_area(q,0,0,SCREEN_W-1,0,SCREEN_H-1);
   if(!g_fp_drawn){g_fp_drawn=1;telemetry_event("freeplay_draw",g_frame_texture_quads-q0,g_fp_n);}
   if(!g_fp_cap_logado&&ndes>0){g_fp_cap_logado=1;
    telemetry_event("freeplay_capsulas",ndes,(int)(g_fpcs[g_fp_sel].x*10.0f));}
  }

  /* SOBREPOSICAO DA PAUSA (ui_state continua 8).
     Fundo preto com alpha subindo de 0 a 0,6 em 0,8 s por FlxEase.quartOut
     (PauseSubState.transitionIn + bgColor 0x99000000 = 60 % de preto) e o menu
     por cima. As entradas ficam em y = 70*i + 30 e x = 0 nas coordenadas de
     1280x720 do original (clearAndAddMenuEntries), ou seja metade disso em
     640x360. O texto do original e' AtlasText com as letras dobradas de
     tamanho; aqui e' a fonte do port. */
  if(ui_state==8&&g_paused){
   char lb[64];int i,q0=g_frame_texture_quads;
   static const char*dnames[3]={"EASY","NORMAL","HARD"};
   unsigned int mask=(g_play_song?g_play_song->difficulties:SR_DIFF_ALL);
   int n_ent,list[3],nd=0,acoes[PAUSE_ENTRADAS_PADRAO];
   float t=(float)g_pause_frame/(0.8f*60.0f),a;
   if(t>1.0f)t=1.0f;
   /* FlxEase.quartOut: 1-(1-t)^4 */
   {float u=1.0f-t;a=1.0f-u*u*u*u;}
   if(g_pause_mode){
    for(i=0;i<3;i++)if(mask&(1u<<i))list[nd++]=i;
    n_ent=nd+1;
   }else n_ent=pause_acoes(acoes);
   q=draw_scissor_area(q,0,0,SCREEN_W-1,SCISSOR_TOPO,SCISSOR_BASE);
   band_set(SCISSOR_TOPO,SCISSOR_BASE);
   q=rect_alpha(q,0.0f,(float)SCISSOR_TOPO,(float)SCREEN_W,(float)(SCISSOR_BASE+1),0,0,0,(unsigned char)(153.0f*a));
   q=bind_texture_filter(q,&fonttex,1);
   for(i=0;i<n_ent;i++){
    const char*txt;
    unsigned char alpha=(i==g_pause_sel)?(unsigned char)128:77;
    if(!g_pause_mode)txt=pause_rotulo(acoes[i]);
    else if(i<nd)txt=dnames[list[i]];
    else txt="BACK";
    q=text_draw_alpha(q,txt,6.0f,15.0f+35.0f*(float)i,0.85f,0,alpha);
   }
   /* Texto de metadados do original (canto superior direito): nome da musica,
      dificuldade e o contador de "Blue Balls" (mortes). */
   snprintf(lb,sizeof(lb),"%s",g_play_song?g_play_song->display_name:"?");
   q=text_draw_alpha(q,lb,634.0f,15.0f,0.7f,2,128);
   snprintf(lb,sizeof(lb),"%s",(story_diff==0)?"EASY":(story_diff==2)?"HARD":"NORMAL");
   q=text_draw_alpha(q,lb,634.0f,33.0f,0.7f,2,128);
   snprintf(lb,sizeof(lb),"%d BLUE BALLS",g_death_counter);
   q=text_draw_alpha(q,lb,634.0f,51.0f,0.7f,2,128);
   /* metadataPractice (L512-516): 'PRACTICE MODE' embaixo do contador, so'
      quando o modo esta' ligado. */
   if(g_practice_mode)q=text_draw_alpha(q,"PRACTICE MODE",634.0f,69.0f,0.7f,2,128);
   q=draw_scissor_area(q,0,0,SCREEN_W-1,0,SCREEN_H-1);
   if(!g_pause_drawn){g_pause_drawn=1;telemetry_event("pause_draw",g_frame_texture_quads-q0,(int)g_pause_mode);}
  }
  /* GAME OVER (ui_state 10).
     O original cobre a tela com um retangulo PRETO opaco (FunkinSprite
     makeSolidColor(FlxG.width*2, FlxG.height*2, BLACK) com alpha 1.0 --
     GameOverSubState.hx:149-155) e poe o BF, na MESMA posicao, na animacao
     firstDeath (L163-172) -- e' TUDO o que a tela tem. Nao ha' arte de fundo
     nem texto nela: o que o original faz e observavel e ja' esta' aqui -- a
     tela parar (persistentUpdate = false), o placar voltar a zero, a entrada
     ficar bloqueada 1 s e a camera seguir o BF morto. */
  if(ui_state==10){
   char lb[48];int q0=g_frame_texture_quads;
   q=draw_scissor_area(q,0,0,SCREEN_W-1,SCISSOR_TOPO,SCISSOR_BASE);
   band_set(SCISSOR_TOPO,SCISSOR_BASE);
   q=rect_alpha(q,0.0f,(float)SCISSOR_TOPO,(float)SCREEN_W,(float)(SCISSOR_BASE+1),0,0,0,255);
   /* O BF MORTO, com a camera e o zoom da tela (go_foco_bf + GO_CAM_LERP /
      GO_ZOOM_K, no bloco de entrada acima). O original tira o BF do palco e o
      poe no substate na MESMA posicao, com o fundo PRETO na frente do palco --
      por isso nada do palco aparece, mas ele aparece. Os textos de HUD tambem
      nao: a tela de game over cobre tudo.
      `passo` vai 0 de proposito: no substate nao existe batida, e quem manda
      na animacao e' o cc_especial_fixo (o personagem esta' morto). */
   if(g_cena_modo&&g_cena.p[CH_PAPEL_BF].ativo&&g_cena_texde_w[CH_PAPEL_BF]>0){
    const char *stg=g_play_song?g_play_song->stage:"mainStage";
    const bs_slot_t *slb=ch_slot(stg,"bf");
    int qa=g_frame_texture_quads;
    if(slb)q=cena_desenha(q,CH_PAPEL_BF,slb,0,(long)g_go_ms,g_go_cam_x,g_go_cam_y,0.5f*g_go_zoom);
    if(g_gameover_drawn==0)telemetry_event("gameover_bf_quads",g_frame_texture_quads-qa,g_cena_desenhos[CH_PAPEL_BF]);
   }
   /* O FADE DA SAIDA: 2 s indo para preto (FlxG.camera.fade(BLACK, 2) ou o
      RetroCameraFade de 2 s no `-pixel`), linear como o FlxFlash do original.
      O reinicio acontece no FIM dele, e a volta do preto (1 s) e' desenhada
      pelo estado seguinte, em `g_fade_in`. */
   if(g_go_saida==2&&g_go_fade>0){
    int a=255*g_go_fade/GO_FADE_SAIDA_QUADROS;
    if(a>255)a=255;
    if(a>0)q=rect_alpha(q,0.0f,(float)SCISSOR_TOPO,(float)SCREEN_W,(float)(SCISSOR_BASE+1),0,0,0,(unsigned char)a);
   }
   /* NENHUM TEXTO. `GameOverSubState.create()` (L139-172) monta exatamente
      tres coisas: o retangulo preto, o BF (pego do palco, na MESMA posicao) e
      o `cameraFollowPoint`. Nao existe "GAME OVER", nao existe "MISSES", nao
      existe dica de controle -- as dicas de toque do original sao
      `#if mobile` (`addBackButton`) e o port nao e' mobile.
      Antes desta build o port DESENHAVA "GAME OVER", "MISSES n" e
      "CROSS RETRY CIRCLE EXIT": texto que o original nunca mostra. Invencao e'
      divergencia, entao saiu. O campo 2 do evento (`texto`) e' 0 e o harness
      cobra que continue 0 -- se alguem voltar a inventar texto, o teste acusa. */
   q=draw_scissor_area(q,0,0,SCREEN_W-1,0,SCREEN_H-1);
   if(!g_gameover_drawn){g_gameover_drawn=1;
    /* DIAGNOSTICO: quais condicoes deixaram o BF morto de fora. Campo 1 =
       bits (1 = cena generica, 2 = papel ativo, 4 = textura com largura,
       8 = slot `bf` do palco achado); campo 2 = largura da textura. Sem isto,
       "a tela desenhou so' o fundo" nao diz QUAL das quatro falhou. */
    {const char *stg=g_play_song?g_play_song->stage:"mainStage";
     telemetry_event("gameover_bf",
      (g_cena_modo?1:0)|((g_cena.p[CH_PAPEL_BF].ativo?1:0)<<1)|
      ((g_cena_texde_w[CH_PAPEL_BF]>0?1:0)<<2)|((ch_slot(stg,"bf")?1:0)<<3),
      g_cena_texde_w[CH_PAPEL_BF]);}
    telemetry_event("gameover_draw",g_frame_texture_quads-q0,0);}
  }
  /* TELA DE RESULTADO (ui_state 9).
     Todos os numeros vem de engine/gameplay.h -- as mesmas funcoes que
     julgaram as notas -- entao o que aparece aqui e' o que o jogo calculou, e
     nao um contador paralelo que pode divergir. O original (ResultState.hx)
     mostra rank, placar, os cinco tallies, maior combo e o percentual de
     conclusao; e' o mesmo conjunto. A ARTE (fundo, "RESULTS!", as categorias e
     o "SCORE") vem de resultScreen e entra pela metade -- mas os NUMEROS
     continuam sendo texto, porque a arte dos digitos
     (`resultScreen/score-digital-numbers`) e do rank (`rankText`, `results-bf`)
     nao foi empacotada ainda. O que sai e' o painel preto de reserva: ele
     existia para o texto ter contraste e sumiria com a arte por baixo. */
  if(ui_state==9){
   char lb[64];int q0=g_frame_texture_quads;
   int res_arte=0;
   /* O grupo do topo (nome, dificuldade e percentual pequeno) anda junto: o
      estado tem que ser andado ANTES de desenhar, porque a dificuldade e' a
      primeira das tres na ordem do original. */
   resultado_scroll_passo();
   /* A musica e os dois `playOnce` da tela (o `getMusicDelay` do rank, os
      ticks do percentual e o `confirmMenu` no fim do tween). */
   resmus_passo(resultado_rank(),resultado_pct_alvo());
   q=draw_scissor_area(q,0,0,SCREEN_W-1,SCISSOR_TOPO,SCISSOR_BASE);
   band_set(SCISSOR_TOPO,SCISSOR_BASE);
   /* A ARTE DO ORIGINAL, quando ela carregou (ver resultado_carregar).
      Ordem igual as `zIndex` do ResultState: fundo (10) < folhas (1200). As
      tres folhas aparecem nos timers da fonte -- `res_quadro` devolve -1 antes
      disso -- e param no ultimo quadro (`looped = false` no flixel para, nao
      some), que e' o que monta a cena final da tela.
      Cada quadro e' LIDO DO PENDRIVE na hora (pack_stream) e sobe para o
      endereco que o pool deu: a RAM paga a tabela, a VRAM paga um quadro. */
   if(g_res_ok>0||g_res_bg_ok){
    int desenhadas=0;
    res_arte=1;
    /* ms desde a abertura da tela. `GetTimerSystemTime()` conta em ticks de
       kBUSCLK (147.456.000 Hz) -- a MESMA base que o relogio da musica usa
       logo acima (`scaled/(u64)kBUSCLK`). Dividir por kBUSCLK/256 daria 256x o
       tempo: medido no harness, a tela pulava de 0 para 4.266 ms num quadro. */
    {u64 agora=GetTimerSystemTime();
     g_res_ms=(int)(((agora-(u64)g_res_t0)*1000ull)/(u64)kBUSCLK);}
    if(g_res_bg_ok){
     /* Oito faixas de 45 px (720/90 = 8 linhas na fonte, metade = 45 aqui). */
     int fb;
     for(fb=0;fb<RES_BG_FAIXAS;fb++){
      int fr,fg,fb2;
      float y0=360.0f*(float)fb/(float)RES_BG_FAIXAS;
      float y1=360.0f*(float)(fb+1)/(float)RES_BG_FAIXAS;
      res_bg_cor(fb,&fr,&fg,&fb2);
      q=rect_alpha(q,0.0f,y0,(float)SCREEN_W,y1,
                   (unsigned char)fr,(unsigned char)fg,(unsigned char)fb2,255);
     }
    }
    /* bgFlash (zIndex 20): as DUAS piscadas da tela, sem PNG nenhum. O
       `FlxGradient` do original e' um `chunkSize` de 90 px sobre 720, isto e',
       oito FAIXAS horizontais de cor solida (sem interpolacao entre elas), com
       o azul indo de 166 no topo a 190 embaixo -- as cores saem de
       `res_flash_cor`. Acende em 37/24 s com 5/24 s de queda (junto com a
       contagem do percentual) e de novo no `getFlashDelay()` do rank com 14/24
       s. Fica ACIMA do fundo e abaixo de tudo o mais, como o zIndex 20 pede. */
    if(g_res_rank_ok){
     const res_rank_t *rr=res_rank(resultado_rank());
     unsigned long rms=(unsigned long)g_res_ms;
     int fa=res_flash_alpha(rr,rms),fta=res_flash_alpha_tally(rms),b;
     if(fa>0||fta>0){
      /* Um so' `bgFlash` no original: se as duas janelas se tocassem, valeria
         a ultima escrita do alpha. Aqui as duas nunca se sobrepoem (1,5 s e
         4,4 s no pior caso), e o maior alpha cobre o caso geral. */
      unsigned char al=(unsigned char)(fa>fta?fa:fta);
      if(fta>0&&!g_res_flash_logado[0]){g_res_flash_logado[0]=1;
       telemetry_event("resultado_flash",0,(int)rms);}
      if(fa>0&&!g_res_flash_logado[1]){g_res_flash_logado[1]=1;
       telemetry_event("resultado_flash",1,(int)rms);}
      for(b=0;b<RES_FLASH_BANDAS;b++){
       int cr,cg,cb;
       res_flash_cor(b,&cr,&cg,&cb);
       q=rect_alpha(q,0.0f,(float)(b*360/RES_FLASH_BANDAS),
                    (float)SCREEN_W,(float)((b+1)*360/RES_FLASH_BANDAS),
                    (unsigned char)cr,(unsigned char)cg,(unsigned char)cb,al);
      }
     }
    }
    /* A ARTE DO RANK (ResultState.displayRankText, L667-700). Entra
       `getFlashDelay()`/24 s depois de a tela abrir (5,375 s no PERFECT, 4,542
       no GREAT, 7,75 no SHIT -- Scoring.hx L568-586) e as duas imagens do rank
       rolam: a vertical (zIndex 990) para cima a -80 px/s em 1280x720 e as 12
       horizontais (zIndex 100) para os lados alternados, +-7 px/s. */
    if(g_res_rank_ok){
     const res_rank_t *rr=res_rank(resultado_rank());
     unsigned long rms=(unsigned long)g_res_ms;
     if(rr&&rms>=(unsigned long)rr->flash_ms){
      /* rankTextBack (zIndex 100): as 12 linhas horizontais. Cada uma e' um
         FlxBackdrop -- a imagem repetida com espaco --, entao desenha-se ate'
         cobrir a largura, com a fase vinda da velocidade. */
      if(g_res_rank_hor_addr){
       texbuffer_t ht=stagetex;int li;
       ht.width=rr->hor.w;ht.psm=GS_PSM_32;ht.address=g_res_rank_hor_addr;
       setup_texture_dims(&ht,rr->hor.w,rr->hor.h);
       q=bind_texture_filter(q,&ht,1);
       for(li=0;li<(int)rr->hor_n;li++){
        float y=rr->hor_y0+rr->hor_dy*(float)li;
        float fase,passo=(float)rr->hor.w+rr->hor_espaco,x;
        if(y>=(float)SCREEN_H||y+(float)rr->hor.h<=0.0f)continue;
        fase=res_rank_scroll_x(rr,li,rms);
        /* fmod manual: a fase pode ser negativa (linhas pares). */
        while(fase>0.0f)fase-=passo;
        while(fase<=-passo)fase+=passo;
        for(x=rr->hor_x+fase;x<(float)SCREEN_W;x+=passo){
         if(x+passo<0.0f)continue;
         q=sprite_region(q,x,y,(float)rr->hor.w,(float)rr->hor.h,
                         0.0f,0.0f,(float)rr->hor.w,(float)rr->hor.h);
        }
       }
      }
     }
    }
    /* O PERCENTUAL DE CONCLUSAO (ClearPercentCounter.hx L22-140 +
       ResultState.hx L577-646). O texto "CLEAR" com o `%` fica em (0,0) do
       grupo e os digitos abaixo dele, em `y = 36`; cada digito usa o glifo
       `Left` ou `Right` conforme a posicao, e o primeiro anda 1 posicao para a
       direita com um digito / para a esquerda com tres. O numero sobe do
       `alvo - 36` ate' o alvo num tween quartOut de 58/24 s e depois some.
       O ALVO e' o mesmo que a fonte usa: no Story Mode o percentual da SEMANA
       (`talliesLevel`), fora dele o da musica. */
    if(g_res_cp_ok&&res_cp_visivel((unsigned long)g_res_ms)){
     const res_cp_geo_t *geo=&g_res_cp_geo;
     int pct,valor,nd,ind,off,alpha=res_cp_alpha((unsigned long)g_res_ms);
     pct=resultado_pct_alvo();
     valor=res_cp_valor(pct,(unsigned long)g_res_ms);
     nd=res_cp_digitos(valor);
     off=res_cp_deslocamento(nd);
     /* O texto "CLEAR" + "%". */
     {texbuffer_t ct=stagetex;
      ct.width=g_res_cp_texto.w;ct.psm=GS_PSM_32;ct.address=g_res_cp_texto_addr;
      setup_texture_dims(&ct,(int)g_res_cp_texto.w,(int)g_res_cp_texto.h);
      q=bind_texture_filter(q,&ct,1);
      q=sprite_region_alpha(q,geo->x,geo->y,
        (float)g_res_cp_texto.w*g_res_cp_texto.fator,
        (float)g_res_cp_texto.h*g_res_cp_texto.fator,
        0.0f,0.0f,(float)g_res_cp_texto.w,(float)g_res_cp_texto.h,
        (unsigned char)alpha);}
     for(ind=0;ind<nd;ind++){
      int d,p10=1,m,vq;const res_cp_atlas_t *at;const res_cp_frame_t *fr;
      texbuffer_t gt=stagetex;
      for(m=nd-1;m>ind;m--)p10*=10;
      d=(valor/p10)%10;
      vq=res_cp_variante(nd,ind);
      at=&g_res_cp_atlas[vq?RES_CP_RIGHT:RES_CP_LEFT];
      fr=&at->frames[d];
      gt.width=at->w;gt.psm=GS_PSM_32;gt.address=g_res_cp_addr[vq?RES_CP_RIGHT:RES_CP_LEFT];
      setup_texture_dims(&gt,(int)at->w,(int)at->h);
      q=bind_texture_filter(q,&gt,1);
      q=sprite_region_alpha(q,
        geo->x+((float)ind+(float)off)*geo->passo_grande+(float)fr->ox,
        geo->y+geo->y_digitos_grande+(float)fr->oy,
        (float)fr->aw*at->fator,(float)fr->ah*at->fator,
        (float)fr->ax,(float)fr->ay,(float)fr->aw,(float)fr->ah,
        (unsigned char)alpha);
     }
     /* Telemetria: o valor no primeiro quadro e o valor ASSENTADO (depois do
        tween), que e' o numero que fica na tela antes do fade. */
     if(!g_res_cp_logado){g_res_cp_logado=1;
      telemetry_event("resultado_clear",pct,g_res_ms);}
     if(!g_res_cp_final&&valor==pct){g_res_cp_final=1;
      telemetry_event("resultado_clear_final",valor,g_res_ms);}
    }
    /* O CONTADOR PEQUENO DO TOPO (`ClearPercentCounter` com `small = true`).
       Entra em `getBFDelay()` (95/24 s; 97/24 s no EXCELLENT), ja' com o valor
       FINAL -- nao ha' tween aqui: `showSmallClearPercent()` faz
       `curNumber = clearPercentTarget` e o `drawNumbers()` do grupo mostra o
       numero de uma vez. O texto (`clearPercentTextSmall`, so' o `%`) fica em
       x = 40 do grupo e os digitos abaixo, com passo de 32 px, deslocamento
       de -24 px em x e -4 px em y por digito (L63-80).
       Posicao: `x = (difficulty.x + difficulty.width) + 60` e
       `y = (122 - 5) + (blackTopBar.height - 148)`, posto por
       `timerThenSongName()` (L741-745) -- ou seja, o contador pequeno anda
       junto com a imagem da dificuldade. */
    if(g_res_cp_ok&&g_res_dif_ind>=0&&
       res_cp_small_visivel(resultado_rank(),(unsigned long)g_res_ms)){
     const res_cp_geo_t *geo=&g_res_cp_geo;
     const res_cp_atlas_t *at=&g_res_cp_atlas[RES_CP_SMALL];
     float sx=res_cp_small_x((float)g_res_dif[g_res_dif_ind].w)+g_res_off_x;
     float sy=res_cp_small_y_desde((float)g_res_cp_atlas[RES_CP_SMALL].h,
                                   g_res_t_rel)+g_res_barra.correcao_y+g_res_off_y;
     int pct=resultado_pct_alvo();
     int nd=res_cp_digitos(pct),off=res_cp_deslocamento(nd),ind;
     {texbuffer_t ct=stagetex;
      ct.width=g_res_cp_texto_small.w;ct.psm=GS_PSM_32;
      ct.address=g_res_cp_texto_small_addr;
      setup_texture_dims(&ct,(int)g_res_cp_texto_small.w,
                         (int)g_res_cp_texto_small.h);
      q=bind_texture_filter(q,&ct,1);
      q=sprite_region(q,sx+geo->small_texto_x,sy,
        (float)g_res_cp_texto_small.w*g_res_cp_texto_small.fator,
        (float)g_res_cp_texto_small.h*g_res_cp_texto_small.fator,
        0.0f,0.0f,(float)g_res_cp_texto_small.w,
        (float)g_res_cp_texto_small.h);}
     if(g_res_cp_addr[RES_CP_SMALL]){
      texbuffer_t gt=stagetex;
      gt.width=at->w;gt.psm=GS_PSM_32;gt.address=g_res_cp_addr[RES_CP_SMALL];
      setup_texture_dims(&gt,(int)at->w,(int)at->h);
      q=bind_texture_filter(q,&gt,1);
      for(ind=0;ind<nd;ind++){
       int d,p10=1,m;const res_cp_frame_t *fr;
       for(m=nd-1;m>ind;m--)p10*=10;
       d=(pct/p10)%10;
       fr=&at->frames[d];
       q=sprite_region(q,
         sx+((float)ind+(float)off)*geo->passo_pequeno+geo->small_x_pos+(float)fr->ox,
         sy+((float)ind+(float)off)*geo->small_y_passo+(float)fr->oy,
         (float)fr->aw*at->fator,(float)fr->ah*at->fator,
         (float)fr->ax,(float)fr->ay,(float)fr->aw,(float)fr->ah);
      }
     }
     if(!g_res_cp_small_logado){g_res_cp_small_logado=1;
      telemetry_event("resultado_clear_small",pct,(int)(sx*10.0f));}
    }
    /* O SELO DE RECORDE (`highscoreNew`, zIndex 1200 -- ResultState.hx L409-427).
       O TIMER e' real: `rank.getHighscoreDelay()` depois da abertura da tela. O
       que NAO existe e' o dado que ele testa, `params.isNewHighscore`, que vem
       de `Save.instance.isNewHighscore(...)` -- e o port nao tem save (nem
       memory card). Entao o caminho tomado e' o `else` da fonte: o selo fica
       invisivel. Isso e' DADO AUSENTE, nao decisao de projeto, e por isso sai
       no log com o instante do timer: `resultado_highscore,0,<ms>`. Um dia, com
       save, o primeiro campo vira 1 e o desenho entra no mesmo lugar. */
    if(g_res_rrf_ok&&!g_res_hs_logado){
     unsigned int hs=res_rr_highscore_ms(g_res_rrf_rank);
     if(hs&&(unsigned long)g_res_ms>=hs){
      g_res_hs_logado=1;
      telemetry_event("resultado_highscore",0,(int)hs);
     }
    }
    /* AS ANIMACOES DO PERSONAGEM DO RANK (zIndex 499..501). Entram entre os
       percentuais (450) e a faixa vertical (990) -- a ordem das `zIndex` do
       original, nao a ordem em que os pacotes foram empacotados. */
    q=resultado_rrf_arte(q,&stagetex);
    /* rankTextVert (zIndex 990): a faixa vertical em x=618, rolando para
       cima, com o piscar de entrada. Vem DEPOIS do percentual (450) e ANTES da
       dificuldade/nome (1000), que e' a ordem das `zIndex` do original. */
    if(g_res_rank_ok){
     const res_rank_t *rr=res_rank(resultado_rank());
     unsigned long rms=(unsigned long)g_res_ms;
     if(rr&&rms>=(unsigned long)rr->flash_ms){
      if(g_res_rank_vert_addr&&res_rank_pisca(rr,rms)){
       texbuffer_t vt=stagetex;float oy,y;
       vt.width=rr->vert.w;vt.psm=GS_PSM_32;vt.address=g_res_rank_vert_addr;
       setup_texture_dims(&vt,rr->vert.w,rr->vert.h);
       q=bind_texture_filter(q,&vt,1);
       oy=res_rank_scroll_y(rr,rms);
       while(oy>0.0f)oy-=(float)rr->vert.h;
       while(oy<=-(float)rr->vert.h)oy+=(float)rr->vert.h;
       for(y=rr->vert_y+oy;y<(float)SCREEN_H;y+=(float)rr->vert.h){
        if(y+(float)rr->vert.h<=0.0f)continue;
        q=sprite_region(q,rr->vert_x,y,(float)rr->vert.w,(float)rr->vert.h,
                        0.0f,0.0f,(float)rr->vert.w,(float)rr->vert.h);
       }
      }
      if(!g_res_rank_logado&&res_rank_pisca(rr,rms)){g_res_rank_logado=1;
       telemetry_event("resultado_rank",resultado_rank(),(int)rms);}
     }
    }
    /* A IMAGEM DA DIFICULDADE (ResultState.hx L332-334 e L736-750). Ela entra
       de cima: y = -altura em 0,8 s e depois um tween de 0,5 s com `expoOut`
       ate' 61,5 (`122 + (blackTopBar.height - 148)`, na metade da tela).
       zIndex 1000 no original, ou seja por baixo da barra (1010) e do grupo
       do placar/contadores (1200): durante a entrada ela passa POR TRAS da
       barra preta, que ja' esta' descendo ao mesmo tempo.
       O `LeftMaskShader` do original (L355-357) NAO recorta nada: `swagMaskX`
       e' `difficulty.x - 30` e `swagSprX` e' `difficulty.x` (L811), entao
       `cutOff/textureWidth` e' negativo e a condicao `uv.x < negativo` nunca
       vale. Nao ha' shader a reproduzir. */
    if(g_res_dif_ok&&g_res_dif_ind>=0){
     texbuffer_t dt=stagetex;
     const res_dif_t *df=&g_res_dif[g_res_dif_ind];
     float dy=res_dif_y_desde(g_res_dif_h,g_res_t_rel)+g_res_off_y;
     if(vy(dy+g_res_dif_h)>0.0f&&vy(dy)<(float)SCREEN_H){
      dt.width=df->w;dt.psm=GS_PSM_32;dt.address=g_res_dif_addr;
      setup_texture_dims(&dt,(int)df->w,(int)df->h);
      q=bind_texture_filter(q,&dt,1);
      q=sprite_region(q,g_res_dif_geo.x+g_res_off_x,dy,(float)df->w,(float)df->h,
                      0.0f,0.0f,(float)df->w,(float)df->h);
     }
    }
    /* O NOME DA MUSICA (ResultState.hx L119-122 e L736-836). O bitmap do
       titulo vem pronto do empacotador e e' desenhado como UM quadrilatero
       girado de -4,4 graus em torno do centro do texto -- e' o que o
       `FlxSprite` do original faz com o `BitmapData` que o `FlxBitmapText`
       monta. Ele so' APARECE quando a barra assenta (`onComplete` do tween da
       barra: `songName.visible = true`), mas o ciclo comecou junto com a tela.
       O recorte da esquerda (`clipRect = max(0, 520 - x)`, L800) corta o texto
       quando ele passa por cima do placar. */
    if(g_res_nome_ok&&g_res_dif_ind>=0){
     const res_nome_arte_t *nm=res_nome_do_id(g_play_song?g_play_song->id:0);
     if(nm&&nm->id){
      /* A posicao ja' foi andada por `resultado_scroll_passo()`: aqui so' se
         desenha o quadrilatero girado (o mesmo do `FlxSprite` do original). */
      if(res_barra_assentada((unsigned long)g_res_ms)){
       const res_nome_t *e=&g_res_nome_estado;
       float clip=res_nome_clip_local(e);
       float w=(float)nm->w,h=(float)nm->h,cx=w*0.5f,cy=h*0.5f;
       float rad=g_res_nome_geo.angulo*(3.14159265f/180.0f);
       float ca=cosf(rad),sa=sinf(rad);
       float lx[4],ly[4],px[4],py[4];
       int c;
       if(clip<w-1.0f){
        texbuffer_t nt=stagetex;
        nt.width=nm->w;nt.psm=GS_PSM_32;nt.address=g_res_nome_addr;
        setup_texture_dims(&nt,(int)nm->w,(int)nm->h);
        lx[0]=clip;ly[0]=0.0f;
        lx[1]=w;   ly[1]=0.0f;
        lx[2]=w;   ly[2]=h;
        lx[3]=clip;ly[3]=h;
        for(c=0;c<4;c++){
         float dx=lx[c]-cx,dy=ly[c]-cy;
         px[c]=e->x+cx+(dx*ca-dy*sa);
         py[c]=e->y+cy+(dx*sa+dy*ca);
        }
        q=bind_texture_filter(q,&nt,1);
        q=sprite_quad_uv(q,px,py,clip,0.0f,w,h,0x80);
        if(!g_res_nome_visto){g_res_nome_visto=1;
         telemetry_event("resultado_nome",(int)(e->x*10.0f),(int)(e->y*10.0f));}
        /* O `update()` do original so' anda depois de `showSmallClearPercent()`
           (+2,5 s): este evento marca o primeiro quadro em que o nome SAiu do
           x de entrada, e e' a prova de que o scroll comecou. */
        if(!g_res_nome_moveu&&e->x<e->x0-5.0f){g_res_nome_moveu=1;
         telemetry_event("resultado_nome_move",(int)(e->x*10.0f),g_res_ms);}
       }
      }
     }
    }
    /* A BARRA PRETA DO TOPO (ResultState.hx L342-346). Nao ha' PNG: o original
       monta um retangulo preto de 1295x149 girado de -3,8 graus
       (`BitmapUtil.createResultsBar`) e o poe em `y = -height`, animando ate' 0
       em 7/24 s com `quartOut` depois de 3/24 s. A borda de baixo e' uma RETA,
       entao aqui ela e' um trapezio de cor solida: 0 byte de VRAM, 3 qwords por
       quadro. `borda_y0/y1` saem da matriz do original (ver
       tools/empacotar_topbar.py). */
    {float by=res_barra_y((unsigned long)g_res_ms);
     float b0=by+g_res_barra.borda_y0,b1=by+g_res_barra.borda_y1;
     q=barra_trapezio(q,0.0f,(float)SCREEN_W,-80.0f,b0,b1,0x80);
     if(!g_res_barra_logado&&res_barra_assentada((unsigned long)g_res_ms)){
      g_res_barra_logado=1;
      telemetry_event("resultado_barra",(int)(b0*10.0f),(int)(b1*10.0f));
     }
    }
    /* A MAQUINA QUE CAI ATRAS DO PLACAR (`soundSystem`, zIndex 1100): acima da
       barra do topo (1010) e abaixo das folhas e do placar (1200). Ela entra em
       8/24 s, roda 65 quadros a 24 fps e CONGELA no ultimo (`looped = false`).
       Um quadro por vez na VRAM, lido do pendrive na troca de quadro. */
    if(g_res_ss_ok&&g_res_pixels){
     int qd=res_ss_quadro((unsigned long)g_res_ms);
     if(qd>=0&&qd!=g_res_ss_frame&&
        pst_quadro(&g_res_ss_pack,(unsigned)qd,g_res_pixels)==PST_OK){
      texbuffer_t st=stagetex;
      st.width=(unsigned)g_res_ss.w;st.psm=GS_PSM_32;st.address=g_res_ss_addr;
      setup_texture_dims(&st,(int)g_res_ss.w,(int)g_res_ss.h);
      FlushCache(0);
      upload_raw(&st,g_res_pixels,(int)g_res_ss.w,(int)g_res_ss.h);
      g_res_ss_frame=qd;
     }
     if(g_res_ss_frame>=0){
      texbuffer_t st=stagetex;
      st.width=(unsigned)g_res_ss.w;st.psm=GS_PSM_32;st.address=g_res_ss_addr;
      setup_texture_dims(&st,(int)g_res_ss.w,(int)g_res_ss.h);
      q=bind_texture_filter(q,&st,1);
      q=sprite_region(q,g_res_ss.x,g_res_ss.y,
                      (float)g_res_ss.w*g_res_ss.fator,
                      (float)g_res_ss.h*g_res_ss.fator,
                      0.0f,0.0f,(float)g_res_ss.w,(float)g_res_ss.h);
      if(!g_res_ss_logado){g_res_ss_logado=1;
       telemetry_event("resultado_maquina",g_res_ms,(int)(g_res_ss.w*g_res_ss.fator));}
     }
    }
    for(i=0;i<(int)RES_FOLHAS;i++){
     int qd;texbuffer_t ft=stagetex;
     if(i>=g_res_ok||!g_res_addr[i]||!g_res_pixels)continue;
     qd=res_quadro(&g_res,i,(unsigned long)g_res_ms);
     if(qd<0)continue;
     ft.width=(unsigned int)g_res_folha[i].w;ft.psm=GS_PSM_32;ft.address=g_res_addr[i];
     if((unsigned int)qd!=g_res_frame[i]){
      if(res_pixels(&g_res,i,qd,g_res_pixels)==PST_OK){
       setup_texture_dims(&ft,(int)g_res_folha[i].w,(int)g_res_folha[i].h);
       FlushCache(0);
       upload_raw(&ft,g_res_pixels,(int)g_res_folha[i].w,(int)g_res_folha[i].h);
       g_res_frame[i]=(unsigned int)qd;
      }else{qd=-1;}
     }
     if(qd<0)continue;
     setup_texture_dims(&ft,(int)g_res_folha[i].w,(int)g_res_folha[i].h);
     q=bind_texture_filter(q,&ft,1);
     /* O pacote esta' em `escala` do original e a tela e' a metade: o desenho
        multiplica por `fator` (1,25 com o pacote a 0,4) para a folha sair no
        tamanho da fonte. E' a MESMA troca dos glifos do percentual: so' nitidez. */
     q=sprite_region(q,g_res_folha[i].x,g_res_folha[i].y,
                     (float)g_res_folha[i].w*g_res_folha[i].fator,
                     (float)g_res_folha[i].h*g_res_folha[i].fator,
                     0.0f,0.0f,(float)g_res_folha[i].w,(float)g_res_folha[i].h);
     desenhadas++;
    }
    /* Quantas folhas estao na tela AGORA (e em que ms). A primeira chamada
       desenha so' o fundo -- as folhas entram em 250/875/1500 ms --, entao sem
       este evento "a tela desenhou 99 quads" nao diz se a ARTE chegou a
       aparecer ou se ficou so' o fundo. */
    if(desenhadas!=g_res_desenhadas){g_res_desenhadas=desenhadas;
     telemetry_event("resultado_folhas",desenhadas,g_res_ms);}

    /* O PLACAR (ResultScore.hx). Dez slots em x = 17,5 + 32,5*i, y = 152,5
       (o original: ResultScore(35, 305) com 65 px por slot, em 1280x720).
       O tempo e o digito vem de `res_score_estado`, que segue os timers do
       original em 24 avos. Desenha DEPOIS das folhas porque o placar e'
       `zIndex 1200` e e' o ultimo a entrar na cena.
       O atlas e' um so': cada slot desenha o retangulo do quadro que a
       animacao escolheu. */
    if(g_res_score_ok){
     texbuffer_t st=stagetex;int slot,desenhados=0;long valor=0;int assentado=1;
     /* O numero que a tela mostra: o da CAMPANHA no Story Mode, o da musica
        fora dele -- o mesmo valor que o texto de reserva usa logo abaixo. */
     long placar=g_story_mode?g_campaign_score:(long)g_play.score;
     int digitos=res_score_digitos(placar);
     st.width=g_res_score.w;
     st.psm=g_res_score.bpp==16?GS_PSM_16:GS_PSM_32;
     st.address=g_res_score_addr;
     setup_texture_dims(&st,(int)g_res_score.w,(int)g_res_score.h);
     q=bind_texture_filter(q,&st,1);
     for(slot=0;slot<(int)g_res_score.slots;slot++){
      int dg,qd,idx;const res_score_frame_t *fr;
      if(!res_score_estado(slot,placar,(unsigned long)g_res_ms,&dg,&qd))continue;
      idx=res_score_indice(dg,qd);
      if(idx<0||idx>=(int)RES_SCORE_QUADROS)continue;
      fr=&g_res_score_frame[idx];
      q=sprite_region(q,g_res_score.x+g_res_score.dx*(float)slot+(float)fr->ox,
                      g_res_score.y+(float)fr->oy,
                      (float)fr->aw,(float)fr->ah,
                      (float)fr->ax,(float)fr->ay,(float)fr->aw,(float)fr->ah);
      desenhados++;
      /* Monta o numero que ESTA' na tela, nao o que o jogo guarda: e' isso que
         prova que o digito de cada slot e' o certo. */
      if(slot>=(int)g_res_score.slots-digitos){
       valor=valor*10+(long)dg;
       /* `qd == 4` NAO quer dizer parado: durante o shuffle quase todo quadro
          desenhado tambem e' o ultimo. Quem diz "assentou" e' o tempo. */
       if(!res_score_assentado(slot,placar,(unsigned long)g_res_ms))assentado=0;
      }
     }
     if(desenhados>0&&!g_res_score_logado){g_res_score_logado=1;
      telemetry_event("resultado_placar",desenhados,g_res_ms);}
     /* Quando todos os digitos estao no ultimo quadro (a animacao assentou), o
        numero montado dos pixels desenhados tem de ser o placar. */
     if(g_res_score_logado&&assentado&&!g_res_score_final){
      g_res_score_final=1;
      telemetry_event("resultado_placar_final",(int)valor,g_res_ms);
     }
    }
    /* OS SETE CONTADORES DE JULGAMENTO (TallyCounter.hx, ResultState.hx
       L418-490). Cada um aparece em `(0.3*i) + 1.20` s e conta de 0 ate' o
       alvo num tween de 0,5 s (quartOut). Os numeros vem do MESMO lugar que a
       fonte usa: no Story Mode sao os tallies da SEMANA (`talliesLevel`), fora
       dele os da musica (`tallies`). */
    if(g_res_tally_ok){
     int alvo[RES_TALLIES],ci,desenhados=0,hit;
     texbuffer_t tt=stagetex;
     /* `totalNotesHit`: cada nota pontuavel acertada conta UMA vez
        (`PlayState.hx` L3161), e cada acerto vira um julgamento -- por isso a
        soma dos quatro. */
     if(g_story_mode){
      hit=g_camp_sick+g_camp_good+g_camp_bad+g_camp_shit;
      alvo[0]=hit;alvo[1]=g_camp_maxcombo;alvo[2]=g_camp_sick;
      alvo[3]=g_camp_good;alvo[4]=g_camp_bad;alvo[5]=g_camp_shit;
      alvo[6]=g_camp_missed;
     }else{
      hit=g_play.sick+g_play.good+g_play.bad+g_play.shit;
      alvo[0]=hit;alvo[1]=g_play.max_combo;alvo[2]=g_play.sick;
      alvo[3]=g_play.good;alvo[4]=g_play.bad;alvo[5]=g_play.shit;
      alvo[6]=g_play.missed;
     }
     tt.width=g_res_tallies.w;tt.psm=GS_PSM_32;tt.address=g_res_tally_addr;
     setup_texture_dims(&tt,(int)g_res_tallies.w,(int)g_res_tallies.h);
     q=bind_texture_filter(q,&tt,1);
     for(ci=0;ci<(int)RES_TALLIES;ci++){
      const res_tally_t *ct=&g_res_tally[ci];
      unsigned char cr=(unsigned char)((ct->r*128u+127u)/255u);
      unsigned char cg=(unsigned char)((ct->g*128u+127u)/255u);
      unsigned char cb=(unsigned char)((ct->b*128u+127u)/255u);
      int valor,nd,k,v;
      float bx;
      if(!res_tally_visivel(ci,(unsigned long)g_res_ms))continue;
      valor=res_tally_valor(alvo[ct->campo],ci,(unsigned long)g_res_ms);
      nd=res_tally_digitos(valor);
      bx=ct->x+res_tally_ajuste_x(ct->campo,hit);
      v=valor;
      for(k=0;k<nd;k++){
       /* Do digito MAIS significativo para o menos: o original posiciona o
          primeiro do array (que ja' vem invertido) em x = 0. */
       int p10=1,m,d;
       const res_tally_frame_t *fr;
       for(m=nd-1;m>k;m--)p10*=10;
       d=(v/p10)%10;
       fr=&g_res_tally_frame[res_tally_glifo(d)];
       q=sprite_region_cor(q,bx+(float)k*g_res_tallies.dx+(float)fr->ox,
        ct->y+(float)fr->oy,(float)fr->aw,(float)fr->ah,
        (float)fr->ax,(float)fr->ay,(float)fr->aw,(float)fr->ah,cr,cg,cb);
       desenhados++;
      }
      /* Um evento por contador, no instante em que ele ASSENTA: e' o numero
         que a tela mostra, para o harness comparar com o do jogo. */
      if(!g_res_tally_logado[ci]&&
         (unsigned long)g_res_ms>=(unsigned long)(g_res_tallies.atraso_base_ms+
          g_res_tallies.atraso_passo_ms*ci+g_res_tallies.tween_ms)){
       g_res_tally_logado[ci]=1;
       telemetry_event("resultado_tally",ct->campo,valor);
      }
     }
     if(desenhados!=g_res_tally_desenho){g_res_tally_desenho=desenhados;
      telemetry_event("resultado_tallies",desenhados,g_res_ms);}
    }
    q=bind_texture_filter(q,&fonttex,1);
   }
   if(!res_arte)q=rect_alpha(q,60.0f,60.0f,580.0f,292.0f,0,0,0,200);
   q=bind_texture_filter(q,&fonttex,1);
   /* Com a arte carregada, os numeros vao para a direita: as tres folhas
      ocupam x = -100..165 (ratings/score) e o "RESULTS!" o topo. E' degradacao
      de LEIAUTE (o dado e' o mesmo), nao de evento. */
   {float tx=res_arte?470.0f:320.0f;float ty=res_arte?-24.0f:0.0f;
   q=text_draw(q,"RESULTS",tx,ty+72,1.6f,1);
   q=text_draw(q,fnf_rank_name(resultado_rank()),tx,ty+104,1.4f,1);
   /* Placar: com a ARTE dos digitos carregada, o numero NAO e' mais texto --
      ele e' desenhado pelo atlas acima, na posicao da fonte. Sem a arte (pendrive
      sem o arquivo), o texto continua como reserva. No Story Mode o numero e' o
      da CAMPANHA (PlayState.hx L3885), e e' esse que o atlas mostra. */
   if(!g_res_score_ok){
    snprintf(lb,sizeof(lb),"SCORE %ld",g_story_mode?g_campaign_score:(long)g_play.score);
    q=text_draw(q,lb,tx,ty+140,1.1f,1);
   }
   /* Com a arte dos contadores carregada, os cinco julgamentos e o combo NAO
      sao mais texto: sao os `TallyCounter` desenhados acima, na posicao e na
      cor do original. O texto fica so' como reserva. */
   if(!g_res_tally_ok){
    snprintf(lb,sizeof(lb),"SICK %d   GOOD %d   BAD %d",g_play.sick,g_play.good,g_play.bad);
    q=text_draw(q,lb,tx,ty+168,0.9f,1);
    snprintf(lb,sizeof(lb),"SHIT %d   MISS %d",g_play.shit,g_play.missed);
    q=text_draw(q,lb,tx,ty+190,0.9f,1);
    snprintf(lb,sizeof(lb),"MAX COMBO %d",g_play.max_combo);
    q=text_draw(q,lb,tx,ty+216,0.9f,1);
   }
   /* O percentual de conclusao, quando a arte dele carregou, e' o contador
      desenhado acima (com o "CLEAR" e o `%` do original). */
   if(!g_res_cp_ok){
    snprintf(lb,sizeof(lb),"CLEAR %d%%",fnf_play_clear_percent(&g_play));
    q=text_draw(q,lb,tx,ty+242,1.1f,1);
   }
   q=text_draw(q,"CROSS  CONTINUE",tx,ty+272,0.8f,1);}   /* fecha o bloco do leiaute */
   q=draw_scissor_area(q,0,0,SCREEN_W-1,0,SCREEN_H-1);
   if(!g_result_drawn){g_result_drawn=1;
    telemetry_event("result_draw",g_frame_texture_quads-q0,0);
    /* O que a arte fez: folhas abertas, fundo carregado e quantos quads
       texturizados o bloco emitiu. O harness cobra o conjunto. */
    telemetry_event("resultado_cena",g_res_ok|(g_res_bg_ok<<8),(int)g_frame_texture_quads-q0);}
  }
  if(ui_state==8&&!g_tutorial_scene_ready){char msg[40];snprintf(msg,sizeof(msg),"TUTORIAL ASSET ERROR %d",g_tutorial_scene_status);q=text_draw(q,msg,320,200,1.0f,1);q=text_draw(q,"CIRCLE  RETURN TO STORY",320,260,.70f,1);}
  if(ui_state==0 && components_mode){char v[32];q=text_draw(q,"UI COMPONENTS",320,30,2.0f,1);snprintf(v,sizeof(v),"TOGGLE: %s",toggle_value?"ON":"OFF");q=text_draw(q,v,155,94,1.2f,0);snprintf(v,sizeof(v),"SLIDER: %d",slider_value);q=text_draw(q,v,155,156,1.2f,0);snprintf(v,sizeof(v),"TAB: %d   L1/R1",tab_value+1);q=text_draw(q,v,155,218,1.2f,0);q=text_draw(q,"DISABLED OPTION",155,280,1.2f,0);q=text_draw(q,"SCROLL ITEM",155,342,1.2f,0);q=text_draw(q,"TRIANGLE/CIRCLE: BACK",320,410,1.0f,1);}
  else if(ui_state==0){u32 ib=title_intro_phase();if(title_intro_active){if(ib!=g_title_intro_phase_reported){telemetry_event("title_credit_phase",(int)ib,(int)g_title_intro_heard_samples);g_title_intro_phase_reported=ib;}if(ib>=1&&ib<4){q=title_alpha(q,&fonttex,"NINJAMUFFIN99",320,100,1.0f);q=title_alpha(q,&fonttex,"PHANTOMARCADE",320,130,1.0f);q=title_alpha(q,&fonttex,"KAWAISPRITE",320,160,1.0f);q=title_alpha(q,&fonttex,"EVILSK8R",320,190,1.0f);if(ib>=3)q=title_alpha(q,&fonttex,"PRESENT",320,220,1.0f);}else if(ib>=5&&ib<8){q=title_alpha(q,&fonttex,"IN ASSOCIATION",320,100,1.0f);q=title_alpha(q,&fonttex,"WITH",320,130,1.0f);if(ib>=7){q=title_alpha(q,&fonttex,"NEWGROUNDS",320,160,1.0f);q=bind_texture(q,&tex);q=sprite_dims(q,229.5f,187.2f,181.0f,175.0f,181,175);}}else if(ib>=9&&ib<12){q=title_alpha(q,&fonttex,"DOPE ASS GAME",320,100,1.0f);if(ib>=11)q=title_alpha(q,&fonttex,"PLAYSTATION MAGAZINE",320,130,.75f);}else if(ib>=13&&ib<16){q=title_alpha(q,&fonttex,"FRIDAY",320,100,1.0f);if(ib>=14)q=title_alpha(q,&fonttex,"NIGHT",320,130,1.0f);if(ib>=15)q=title_alpha(q,&fonttex,"FUNKIN",320,160,1.0f);}else if(ib>=16){title_intro_flash_frames=60;title_intro_active=0;}}else{unsigned int pf=title_confirm_frames>0?(2u+((title_confirm_frames/4u)&1u)):((frame_no/30u)&1u);q=bind_texture(q,&stagetex);q=sprite_region(q,50,288,512,40,0,pf*48+4,512,40);q=bind_texture(q,&tex2);q=sprite_region(q,562,288,228,40,0,pf*48+4,228,40);q=bind_texture_filter(q,&fonttex,1);}}
  if(ui_state==0&&title_confirm_frames>60){unsigned char ca=(unsigned char)(((u32)(title_confirm_frames-60)*0x80u)/60u);q=rect_alpha(q,0,0,SCREEN_W,SCREEN_H,255,255,255,ca);}
  if(ui_state==0&&title_intro_flash_frames>0){unsigned char fa=(unsigned char)(((u32)title_intro_flash_frames*0x80u)/60u);q=rect_alpha(q,0,0,SCREEN_W,SCREEN_H,255,255,255,fa);title_intro_flash_frames--;}
  if(transition_frame>0){int tm=menu_back_transition?42:15,te=menu_back_transition?102:30,p=transition_frame<=tm?transition_frame:(te-transition_frame);unsigned char a=(unsigned char)(transition_frame<=tm?((p*0x80)/tm):((p*0x80)/(te-tm)));q=rect_alpha(q,0,0,SCREEN_W,SCREEN_H,0,0,0,a);}
  if(g_calib){q=draw_scissor_area(q,0,0,SCREEN_W-1,0,SCREEN_H-1);
              q=desenhar_calibracao(q);}
  /* FADE-IN DE 1 s DO RESET (o `g_fade_in`): cobre a tela inteira, em qualquer
     estado, porque e' o FlxFlash da camera do original -- ele nao pertence a
     nenhum sprite. */
  if(g_fade_in>0){
   int a=255*g_fade_in/GO_FADE_ENTRADA_QUADROS;
   if(a>255)a=255;
   if(a>0){
    q=draw_scissor_area(q,0,0,SCREEN_W-1,0,SCREEN_H-1);
    q=rect_alpha(q,0.0f,0.0f,(float)SCREEN_W,(float)SCREEN_H,0,0,0,(unsigned char)a);
   }
  }
  q=draw_finish(q);qw=(int)(q-packet->data);if(frame_no==0)capture_status("F0_PACKET_DONE",-1,qw);frame_mark("F.DMA",qw);dma_channel_send_normal(DMA_CHANNEL_GIF,packet->data,qw,0,0);if(frame_no==0)capture_status("F0_DMA_SENT",-1,0);frame_mark("F.WAIT",qw);draw_wait_finish();frame_mark("F.DRAWN",qw);if(frame_no==0)capture_status("F0_DRAW_FINISH_DONE",-1,0);if(ui_state==7&&transition_frame==0){int si;story_title_update_positions(story_level);for(si=0;si<9;si++){const unsigned int*sr=g_story_title[si];float yy=g_story_title_y[si]-44.0f,hh=(float)sr[2];if(vy(yy+hh)<269.0f||vy(yy)>391.0f)continue;story_upload_title(&tex,si);menu_anim_tick=-1;{qword_t*tq=packet->data;tq=draw_setup_environment(tq,0,&fb[draw],&z);tq=gs_dither16(tq);tq=draw_scissor_area(tq,0,0,SCREEN_W-1,269,391);band_set(269,391);tq=bind_texture_filter(tq,&tex,0);tq=sprite_region_alpha(tq,320.0f-(float)sr[1]*.5f,yy,(float)sr[1],hh,0,0,(float)sr[1],hh,si==story_level?128:77);tq=draw_finish(tq);dma_channel_send_normal(DMA_CHANNEL_GIF,packet->data,tq-packet->data,0,0);draw_wait_finish();}}}after_draw=GetTimerSystemTime();if((g_held&(PAD_SELECT|PAD_R3))!=(PAD_SELECT|PAD_R3))g_capture_chord_latched=0;if(!g_cap_busy&&(g_forcar_captura||(!g_capture_chord_latched&&transition_frame==0&&(g_held&(PAD_SELECT|PAD_R3))==(PAD_SELECT|PAD_R3)&&(pressed&(PAD_SELECT|PAD_R3))))){g_forcar_captura=0;int cr,rr=0,endpos,restore_rc=-1;u32 saved_sample=0;int capture_ui=ui_state;g_capture_chord_latched=1;if(capture_ui==8&&audio_fd>=0&&g_tutorial_audio_status==0){{u32 aq_t=(u32)audsrv_queued();saved_sample=audio_submitted>=aq_t/4u?audio_submitted-aq_t/4u:0u;if(saved_sample>TUTORIAL_AUDIO_SAMPLES)saved_sample=TUTORIAL_AUDIO_SAMPLES;}capture_status("RAW_TUT_PAUSE",(int)frame_no,(int)saved_sample);audsrv_stop_audio();audio_playing=0;audio_started=0;clock_hold=1;transport_state=0;audio_ring_stop();close(audio_fd);audio_fd=-1;DelayThread(500000);cr=capture_framebuffer_raw(fb[draw].address<<5,frame_no,ui_state);audio_fd=open("mass:/FNFPS2/assets/audio_stems.pcm",O_RDONLY);if(audio_fd>=0){endpos=(int)lseek(audio_fd,0,SEEK_END);if(endpos>0){u64 bp=(u64)saved_sample*8u;if(bp>(u64)(u32)endpos)bp=(u64)(u32)endpos;lseek(audio_fd,(int)bp,SEEK_SET);}restore_rc=audio_ring_start(audio_fd);if(restore_rc==0){audio_submitted=music_clock=saved_sample;music_clock_tick=GetTimerSystemTime();music_clock_fraction=0;clock_hold=1;audio_playing=1;audio_started=0;transport_state=1;g_tutorial_audio_status=0;}else{close(audio_fd);audio_fd=-1;g_tutorial_audio_status=-8;}}capture_status(restore_rc==0?"RAW_TUT_RESUMED":"RAW_TUT_RESUME_FAIL",(int)frame_no,restore_rc);telemetry_event("capture_tutorial",cr,restore_rc);}else if(g_title_audio_fd>=0){u32 aq=(u32)audsrv_queued(),menu_heard=g_title_audio_submitted>=aq/4u?g_title_audio_submitted-aq/4u:0;capture_status("RAW_MENU_PAUSE",(int)frame_no,(int)menu_heard);audsrv_stop_audio();g_menu_audio_suspended=1;audio_ring_stop();close(g_title_audio_fd);g_title_audio_fd=-1;DelayThread(500000);cr=capture_framebuffer_raw(fb[draw].address<<5,frame_no,ui_state);if(cr==0&&ui_state==7)rr=story_titles_restore();else if(cr==0&&ui_state==0)memcpy(g_stage_pixels,g_menu_pixels+TITLE_SNAPSHOT_PROMPT_L,512*192*4);g_title_audio_fd=open("mass:/FNFPS2/assets/freakyMenu_48000.pcm",O_RDONLY);if(g_title_audio_fd>=0){endpos=(int)lseek(g_title_audio_fd,0,SEEK_END);if(endpos>0)lseek(g_title_audio_fd,(int)((menu_heard*4u)%(u32)endpos),SEEK_SET);g_title_audio_submitted=menu_heard;if(audio_ring_start_loop(g_title_audio_fd,1)>=0){int guard=2000;while(audio_ring_level()<65536&&guard--){DelayThread(1000);}guard=64;while(audsrv_available()>=AUDIO_READ_CHUNK&&guard--){int n,j;if((n=audio_ring_take(g_audio_pcm,AUDIO_READ_CHUNK))!=AUDIO_READ_CHUNK){DelayThread(1000);continue;}j=audio_play_complete((char*)g_audio_pcm,n);if(j<=0)break;g_title_audio_submitted+=(u32)(j/4);}g_menu_audio_suspended=0;restore_rc=0;}}capture_status(restore_rc==0?"RAW_MENU_RESUMED":"RAW_MENU_RESUME_FAIL",(int)frame_no,restore_rc);telemetry_event("capture_menu",cr,rr);}else if(capture_ui==10&&g_go_audio_estado>0){
   /* GAME OVER: o anel esta' lendo os arquivos DELE. A captura grava ~1,1 MB no
      mesmo pendrive, e o usbhdfsd nao e' reentrante -- a pausa cooperativa e'
      o que existe para isso (audio_ring_pause espera a leitura pendente sair).
      Nao para o transporte: a tela continua com o mesmo som antes e depois. */
   capture_status("RAW_GO_PAUSE",(int)frame_no,0);
   audio_ring_pause(1);
   cr=capture_framebuffer_raw(fb[draw].address<<5,frame_no,ui_state);
   audio_ring_pause(0);
   telemetry_event("capture_gameover",cr,0);
  }else{capture_status("RAW_NO_TRANSPORT",(int)frame_no,ui_state);}}frame_mark("F.VSYNC",0);if(frame_no==0)capture_status("F0_VSYNC_BEGIN",-1,0);graph_wait_vsync();if(frame_no==0)capture_status("F0_VSYNC_DONE",-1,0);graph_set_framebuffer_filtered(fb[draw].address,SCREEN_W,FB_PSM,0,0);if(frame_no==0)capture_status("F0_FRAMEBUFFER_SET",-1,0);after_vsync=GetTimerSystemTime();if(deferred_menu_start&&frame_no>=3u){deferred_menu_start=0;capture_status("DEFERRED_AUDIO_BEGIN",-1,0);if(audio_ok&&audio_set_stream(audio_volume)==0){g_title_audio_fd=open("mass:/FNFPS2/assets/freakyMenu_48000.pcm",O_RDONLY);if(g_title_audio_fd>=0)audio_ring_start_loop(g_title_audio_fd,1);}}
  if(g_dl_gravando){g_dl_gravando=0;dl_gravar();}
  /* A lista do quadro sai antes do flush, em fluxo: se o jogo pendurar no
     quadro seguinte, a ultima coisa no cartao eh exatamente o quadro em que
     ele parou, com todos os sprites e a geometria recortada de cada um. */
  dl_dump(frame_no-1);
   telemetry_frame(frame_no++,begin,after_draw,after_vsync,pressed,anim,(int)local,(int)global,g_frame_texture_quads,qw,transition_frame?10+(transition_frame>15):(components_mode?4:(transform_mode?3:ui_state)),transitions,heard,queued_bytes,ring_bytes,audio_underruns);
   /* Uma linha de estado por quadro, e o log despejado no mesmo instante. A
      versao anterior acumulava tudo em RAM e so escrevia no exit: se o port
      pendurasse -- que eh o caso que se quer investigar -- o arquivo ficava
      vazio. Agora cada quadro deixa rastro no cartao, e da para ver exatamente
      em qual ui_state e em qual transicao a coisa parou. */
   telemetry_state(frame_no-1,ui_state,target_state,selection,story_level,story_diff,transition_frame,transitions,transport_state,audio_started,audio_playing,g_menu_audio_suspended,g_chart_status,g_tutorial_scene_ready,g_tutorial_audio_status,story_assets_ready,g_menu_cache_ready,g_title_anim_status,g_tutorial_gf_uploaded,g_tutorial_bf_uploaded,(int)(g_gftex16.address!=0),g_story_gf_uploaded,g_story_bf_uploaded,g_aspect_169,g_viewport_360,music_clock,audio_submitted,ring_bytes,audio_underruns,audio_fd,g_title_audio_fd,g_tutorial_note_hits,g_tutorial_note_misses,g_tutorial_note_combo,g_tutorial_note_score,g_tutorial_note_feedback,chart_event_cursor,g_band_clip_rects,g_band_clip_lines,g_band_cull_rects);
   telemetry_flush();
   frame_mark("F.POST",(int)frame_no);g_fn_beat++;
   if(frame_no==1u)fnwd_start();
   av_trace_capture(frame_no-1,ui_state,target_state,transition_frame,transport_state,audio_fd,music_clock,audio_started,audio_playing,last2,last,chart_event_cursor);draw^=1;
 }
 g_fnwd_stop=1;if(g_fnwd_thread>=0){DelayThread(250000);DeleteThread(g_fnwd_thread);g_fnwd_thread=-1;}
 telemetry_event("audsrv_partial",(int)g_audsrv_partial_events,(int)g_audsrv_zero_events);telemetry_event("audsrv_submit_errors",(int)g_audsrv_submit_errors,0);capture_status("SHUTDOWN",-1,0); /* 0.2.102: dumps de ~2-3 MB sincronos no shutdown = ~4 s de "travamento"; saem sob demanda por L1/R1/L2 */audio_ring_stop();DelayThread(20000);if(audio_fd>=0)close(audio_fd);if(audio_ok)audsrv_quit();padPortClose(0,0);padEnd();SleepThread();return 0;}




