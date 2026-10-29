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
#include "song_registry.h"
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
   roda a 24 fps e segura o ultimo quadro ate esse tempo acabar. */
#define TUT_SING_MS 1200u
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
static const unsigned char BF_SING[4][2]={{14,8},{22,7},{29,7},{36,7}};
static const unsigned char GF_SING[4][2]={{57,15},{37,20},{30,7},{72,15}};
static int g_bf_sing=-1,g_gf_sing=-1;static u32 g_bf_sing_at=0,g_gf_sing_at=0;
static int tut_anim_idx(int *sing,u32 at,u32 now,const unsigned char tb[4][2],u32 idle_len,u32 af){
 if(*sing>=0){
  u32 ms=(now>at)?((now-at)/48u):0u;
  if(ms<TUT_SING_MS){u32 len=tb[*sing][1],k=ms*24u/1000u;if(k>=len)k=len-1u;return (int)(tb[*sing][0]+k);}
  *sing=-1;
 }
 return (int)(af%idle_len);
}
static int g_tut_cam_focus=0,g_tut_cam_ready=0;static float g_tut_cam_x=0.0f,g_tut_cam_y=0.0f;
static int g_tutorial_bf_w=0,g_tutorial_bf_h=0,g_tutorial_gf_w=0,g_tutorial_gf_h=0;
static unsigned char *g_tutorial_bf_pack=0,*g_tutorial_gf_pack=0;static u32 g_tutorial_bf_pack_bytes=0,g_tutorial_gf_pack_bytes=0;static int g_tutorial_bf_uploaded=-1,g_tutorial_gf_uploaded=-1;
static const unsigned char *g_story_bf_pack=0,*g_story_gf_pack=0;static u32 g_story_bf_pack_bytes=0,g_story_gf_pack_bytes=0;static int g_story_bf_uploaded=-1,g_story_gf_uploaded=-1,g_story_anim_ready=0;static u32 g_story_anim_heard_samples=0;
static unsigned char *g_story_bf_pixels=0,*g_story_gf_pixels=0;
extern const unsigned char g_story_gf_pack_start[],g_story_gf_pack_end[],g_story_bf_pack_start[],g_story_bf_pack_end[];
static int g_tutorial_audio_status=-1,g_tutorial_audio_finished=0,g_chart_status=-1;
static chart_info_t g_chart_info;
static u32 g_chart_cursor=0,g_chart_seen=0,g_chart_last_type=0,g_chart_last_value=0,g_chart_last_sample=0;
static int g_tutorial_note_asset_status=-1,g_tutorial_note_visible=-1,g_tutorial_note_feedback=0;
/* Every asset the port reads lives under this root on the memory card.
   The registry builds paths beneath it, so a song's chart is
   "<root>/songs/<id>/<difficulty>.chart". */
#define FNF_ASSET_ROOT "mass:/FNFPS2/assets"

#define CHART_MAX_NOTE_STATES 1280u /* acompanha CHART_MAX_NOTES: 2hot-HARD tem 1.035 notas */
static unsigned char g_tutorial_note_state[CHART_MAX_NOTE_STATES];
static u32 g_tutorial_note_score=0,g_tutorial_note_hits=0,g_tutorial_note_misses=0,g_tutorial_note_combo=0,g_tutorial_note_feedback_frame=0;
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
#define TUTORIAL_NOTE_LATE 12000u
#define TUTORIAL_NOTE_JUDGE_WINDOW 7200u
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
static u32 title_u32le(const unsigned char*p){return(u32)p[0]|((u32)p[1]<<8)|((u32)p[2]<<16)|((u32)p[3]<<24);}
static int title_lz4_decode(const unsigned char*src,u32 src_size,unsigned char*dst,u32 dst_size){const unsigned char*ip=src,*iend=src+src_size;unsigned char*op=dst,*oend=dst+dst_size;while(ip<iend){u32 lit,match,off;unsigned token=*ip++;lit=token>>4;if(lit==15){unsigned v;do{if(ip>=iend)return-1;v=*ip++;lit+=v;}while(v==255);}if((u32)(iend-ip)<lit||(u32)(oend-op)<lit)return-2;memcpy(op,ip,lit);ip+=lit;op+=lit;if(ip==iend)break;if((u32)(iend-ip)<2)return-3;off=(u32)ip[0]|((u32)ip[1]<<8);ip+=2;if(!off||off>(u32)(op-dst))return-4;match=token&15;if(match==15){unsigned v;do{if(ip>=iend)return-5;v=*ip++;match+=v;}while(v==255);}match+=4;if((u32)(oend-op)<match)return-6;while(match--){*op=*(op-off);op++;}}return op==oend?0:-7;}
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
static u32 g_frame_texture_quads;

#define QUAD_NREG 10
#define QUAD_REGLIST (((u64)GIF_REG_PRIM)<<0|((u64)GIF_REG_RGBAQ)<<4|((u64)GIF_REG_UV)<<8|((u64)GIF_REG_XYZ2)<<12|((u64)GIF_REG_UV)<<16|((u64)GIF_REG_XYZ2)<<20|((u64)GIF_REG_UV)<<24|((u64)GIF_REG_XYZ2)<<28|((u64)GIF_REG_UV)<<32|((u64)GIF_REG_XYZ2)<<36)
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
 band_clip_texel(cv0,cv1,&v.t0.v,&v.t1.v);v.t0.u=.5f;v.t1.u=tw-.5f;
 v.color=rgba(0x80,0x80,0x80,0x80);dl_sprite(x,y,w,h,0.0f,0.0f,(float)tw,(float)th,0x80,&v);return draw_rect_textured(q,0,&v);}

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
static qword_t *sprite_region_alpha(qword_t*q,float x,float y,float w,float h,float u,float v,float uw,float vh,unsigned char a){texrect_t r;float cy0,cy1,cv0,cv1;
 if(!band_clip_v(vy(y),vy(y+h),v,v+vh,&cy0,&cy1,&cv0,&cv1))return q;
 g_frame_texture_quads++;r.v0.x=FNX_INI(x);r.v0.y=FNX_INI(cy0);r.v0.z=0;r.v1.x=FNX_FIM(x+w);r.v1.y=FNX_FIM(cy1);r.v1.z=0;
 band_clip_texel(cv0,cv1,&r.t0.v,&r.t1.v);r.t0.u=u+.5f;r.t1.u=u+uw-.5f;
 r.color=rgba(0x80,0x80,0x80,a);dl_sprite(x,y,w,h,u,cv0,uw,cv1-cv0,a,&r);return draw_rect_textured(q,0,&r);}
static qword_t *text_draw(qword_t*q,const char*s,float x,float y,float scale,int align);
static unsigned short tutorial_lane_button(unsigned int lane){static const unsigned short masks[4]={PAD_LEFT,PAD_DOWN,PAD_UP,PAD_RIGHT};return lane<4?masks[lane]:0;}
static void tutorial_notes_reset(void){
 u32 k,scorable=0;
 memset(g_tutorial_note_state,0,sizeof(g_tutorial_note_state));
 g_tutorial_note_score=0;g_tutorial_note_hits=0;g_tutorial_note_misses=0;g_tutorial_note_combo=0;
 g_tutorial_note_feedback=0;g_tutorial_note_feedback_frame=0;g_tutorial_note_visible=-1;
 /* tallies.totalNotes do original: quantas notas do chart contam para o
    resultado. Sao as notas do jogador (must_press), nao as do oponente. */
 for(k=0;k<g_chart_info.note_count&&k<CHART_MAX_NOTE_STATES;k++){
  chart_note_t nt;
  if(chart_note(k,&nt)<0)continue;
  if(nt.must_press)scorable++;
 }
 fnf_play_reset(&g_play,FNF_SCORING_PBOT1,(int)scorable);
}
static void tutorial_notes_update(u32 sample,unsigned short pressed,u32 frame){
 u32 i;unsigned int lane;
 if(g_chart_status!=0)return;
 for(i=0;i<g_chart_info.note_count&&i<CHART_MAX_NOTE_STATES;i++){
  chart_note_t note;
  if(chart_note(i,&note)<0||!note.must_press||g_tutorial_note_state[i])continue;
  if((s32)sample-(s32)note.sample>(s32)TUTORIAL_NOTE_LATE){g_tutorial_note_state[i]=2;fnf_play_miss(&g_play);g_tutorial_note_misses=(u32)g_play.missed;g_tutorial_note_combo=(u32)g_play.combo;g_tutorial_note_score=(u32)g_play.score;g_tutorial_note_feedback=4;g_tutorial_note_feedback_frame=frame;}
 }
 /* Notas do oponente: no Tutorial a GF e a oponente, pelo gf.hxc, entao
    ela canta quando a nota dela chega. Estado 3 marca "ja cantada". */
 for(i=0;i<g_chart_info.note_count&&i<CHART_MAX_NOTE_STATES;i++){
  chart_note_t note;
  if(chart_note(i,&note)<0||note.must_press||g_tutorial_note_state[i]||note.lane>3)continue;
  if((s32)sample>=(s32)note.sample){g_tutorial_note_state[i]=3;g_gf_sing=(int)note.lane;g_gf_sing_at=sample;}
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
    /* O original mede o erro em milissegundos (PlayState.goodNoteHit:
       noteDiff = songPosition - noteTime). A 48 kHz, um sample e' 1/48 ms. */
    judge=fnf_play_hit(&g_play,(float)offset/48.0f);
    g_tutorial_note_hits=(u32)g_play.total_notes_hit;
    g_tutorial_note_combo=(u32)g_play.combo;
    g_tutorial_note_score=(u32)g_play.score;
    g_tutorial_note_feedback=judge==FNF_JUDGE_SICK?1:judge==FNF_JUDGE_GOOD?2:judge==FNF_JUDGE_BAD?3:4;
    g_tutorial_note_feedback_frame=frame;telemetry_event("tutorial_note_hit",best,(int)offset);}}
  else{fnf_play_ghost(&g_play);g_tutorial_note_score=(u32)g_play.score;g_tutorial_note_feedback=5;g_tutorial_note_feedback_frame=frame;telemetry_event("tutorial_note_ghost",(int)lane,(int)sample);}
 }
}
static float tutorial_note_y(s32 delta){return 26.3f+(float)delta*(0.45f/(48.0f*2.0f))*g_tut_scroll;}
static qword_t *tutorial_notes_draw(qword_t*q,texbuffer_t*atlas,texbuffer_t*font,u32 sample,unsigned short held,u32 frame){
 u32 i,visible=0;unsigned int lane;if(g_tutorial_note_asset_status!=0||g_chart_status!=0)return q;q=bind_texture_filter(q,atlas,0);
 for(lane=0;lane<4;lane++){float x=g_receptor_x[lane];q=sprite_region_alpha(q,x,g_receptor_y[lane],g_note_w[lane],g_note_h[lane],lane*56.0f,56.0f,g_note_w[lane],g_note_h[lane],128);q=sprite_region_alpha(q,x+320.0f,g_receptor_y[lane],g_note_w[lane],g_note_h[lane],lane*56.0f,56.0f,g_note_w[lane],g_note_h[lane],128);}
 for(i=0;i<g_chart_info.note_count&&i<CHART_MAX_NOTE_STATES;i++){chart_note_t note;s32 delta,tail_delta;float x,y,tail_y;int state;if(chart_note(i,&note)<0||note.lane>3)continue;state=g_tutorial_note_state[i];delta=(s32)note.sample-(s32)sample;tail_delta=(s32)(note.sample+note.duration)-(s32)sample;x=g_note_x[note.lane]+(note.must_press?320.0f:0.0f);if(note.duration&&(!note.must_press||state!=2)&&tail_delta>0&&delta<(s32)TUTORIAL_NOTE_AHEAD){y=tutorial_note_y(delta);tail_y=tutorial_note_y(tail_delta);if(tail_y>y){float top=y+g_note_h[note.lane]*.28f;if(top<26.3f)top=26.3f;if(tail_y>360.0f)tail_y=360.0f;if(tail_y>top)q=rect_alpha(q,x+g_note_w[note.lane]*.39f,top,x+g_note_w[note.lane]*.61f,tail_y,224,224,240,150);}}/* As notas do oponente nunca eram retiradas: so o teste de must_press
   marcava estado, entao elas desciam alem do receptor e iam parar
   perto dos pes do BF. No oficial elas somem quando chegam. O canto
   da GF ja marca estado 3 exatamente nesse instante, entao basta
   usar o mesmo marcador para retira-las. */
if((note.must_press&&state!=0)||(!note.must_press&&state==3)||delta<-(s32)TUTORIAL_NOTE_LATE||delta>(s32)TUTORIAL_NOTE_AHEAD)continue;
y=tutorial_note_y(delta);q=sprite_region(q,x,y,g_note_w[note.lane],g_note_h[note.lane],note.lane*56.0f,0.0f,g_note_w[note.lane],g_note_h[note.lane]);visible++;}if((int)visible!=g_tutorial_note_visible){g_tutorial_note_visible=(int)visible;telemetry_event("tutorial_notes_visible",(int)visible,(int)sample);}(void)font;(void)frame;(void)held;return q;}
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
 if(audsrv_available()<AUDIO_READ_CHUNK||audio_ring_level()<STEM_READ_CHUNK)return 0;
 n=audio_ring_take(g_audio_pcm,STEM_READ_CHUNK);
 if(n!=STEM_READ_CHUNK)return n<0?n:0;
 for(i=0;i<STEM_READ_CHUNK/8;i++){
  int left=(int)samples[i*4]+(voices_enabled?(int)samples[i*4+2]:0);
  int right=(int)samples[i*4+1]+(voices_enabled?(int)samples[i*4+3]:0);
  if(left>32767)left=32767;else if(left<-32768)left=-32768;
  if(right>32767)right=32767;else if(right<-32768)right=-32768;
  samples[i*2]=(short)left;samples[i*2+1]=(short)right;
 }
 return audio_play_complete((const char*)g_audio_pcm,AUDIO_READ_CHUNK);
}
static int audio_init(void){int result=-1;if(SifLoadModule("rom0:LIBSD",0,NULL)<0)return -1;if(SifExecModuleBuffer(audsrv_irx,size_audsrv_irx,0,NULL,&result)<0||result<0)return -2;if(audsrv_init()!=0)return -3;if(audio_set_stream(70)<0)return -4;return 0;}
static int menu_audio_prime(void){int total=0,guard=1000;while(audio_ring_level()<65536&&guard--){DelayThread(1000);}guard=32;while(audsrv_available()>=AUDIO_READ_CHUNK&&guard--){int j;if(audio_ring_take(g_audio_pcm,AUDIO_READ_CHUNK)!=AUDIO_READ_CHUNK){DelayThread(1000);continue;}j=audio_play_complete((char*)g_audio_pcm,AUDIO_READ_CHUNK);if(j<=0)break;total+=j;g_title_audio_submitted+=(u32)(j/4);}telemetry_event("menu_prime",total,audsrv_queued());return total;}
static qword_t *text_draw(qword_t*q,const char*s,float x,float y,float scale,int align){int len=(int)strlen(s),i;float total=0,cursor;
 for(i=0;i<len;i++){unsigned char ch=(unsigned char)s[i];if(ch<32||ch>127)ch='?';total+=g_font_widths[ch-32]*0.5f*scale;}
 if(align==1)x-=total*0.5f;else if(align==2)x-=total;cursor=x;
 for(i=0;i<len;i++){g_frame_texture_quads++;unsigned char ch=(unsigned char)s[i];texrect_t v;int n,advance;if(ch<32||ch>127)ch='?';n=ch-32;advance=g_font_widths[n];v.v0.x=FNX_INI(cursor);v.v0.y=FNX_INI(vy(y));v.v0.z=0;v.v1.x=FNX_FIM(cursor+FONT_DRAW_W*scale);v.v1.y=FNX_FIM(vy(y)+FONT_DRAW_H*scale);v.v1.z=0;v.t0.u=(n%16)*FONT_CELL_W+0.5f;v.t0.v=(n/16)*FONT_CELL_H+0.5f;v.t1.u=(n%16+1)*FONT_CELL_W-0.5f;v.t1.v=(n/16+1)*FONT_CELL_H-0.5f;v.color=rgba(0x80,0x80,0x80,0x80);q=draw_rect_textured(q,0,&v);cursor+=advance*0.5f*scale;}return q;}
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
static qword_t*hud_score_line(qword_t*q,const char*s,int count,float x,float y,int align,unsigned char r,unsigned char g,unsigned char b,unsigned char a){int i;float adv=4.6875f,total=adv*count,cursor;if(align==1)x-=total*.5f;else if(align==2)x-=total;cursor=x;for(i=0;i<count;i++){g_frame_texture_quads++;unsigned char ch=(unsigned char)s[i];texrect_t v;int n;if(ch<32||ch>127)ch='?';n=ch-32;v.v0.x=FNX_INI(cursor);v.v0.y=FNX_INI(vy(y-1.5f));v.v0.z=0;v.v1.x=FNX_FIM(cursor+5.0f);v.v1.y=FNX_FIM(vy(y-1.5f)+10.0f);v.v1.z=0;v.t0.u=(n%16)*10+.5f;v.t0.v=g_story_vcr_vbase+(n/16)*20+.5f;v.t1.u=(n%16+1)*10-.5f;v.t1.v=g_story_vcr_vbase+(n/16+1)*20-.5f;v.color=rgba(r,g,b,a);q=draw_rect_textured(q,0,&v);cursor+=adv;}return q;}
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
static int safe_readback_rect(void*d,u32 v,u32 y,u32 h){u32 c[40]__attribute__((aligned(16))),*p=c,qwc=SCREEN_W*h/4,oc=CAP_D1_CHCR;u64*q=(u64*)(p+4),oi=CAP_GS_IMR,t=294912000ull;int r=-1;if(!h||qwc>65535u||(oc&0x100u))return-10;p[0]=0;p[1]=CAP_VIF_MSKPATH3(0x8000);p[2]=CAP_VIF_FLUSHA;p[3]=CAP_VIF_DIRECT(6);q[0]=CAP_GIFTAG(5,1);q[1]=0x0e;q[2]=GS_SET_BITBLTBUF(v,SCREEN_W/64,GS_PSM_32,0,0,GS_PSM_32);q[3]=GS_REG_BITBLTBUF;q[4]=GS_SET_TRXPOS(0,y,0,0,0);q[5]=GS_REG_TRXPOS;q[6]=GS_SET_TRXREG(SCREEN_W,h);q[7]=GS_REG_TRXREG;q[8]=0;q[9]=0x61;q[10]=GS_SET_TRXDIR(1);q[11]=GS_REG_TRXDIR;CAP_GS_IMR=oi|0x200u;CAP_GS_CSR=2;FlushCache(0);CAP_D1_QWC=7;CAP_D1_MADR=(u32)c;CAP_D1_CHCR=0x101;EE_SYNCL();if(cap_wait_clear32(&CAP_D1_CHCR,0x100,t)<0){r=-11;goto x;}if(cap_wait_set64(&CAP_GS_CSR,2,t)<0){r=-12;goto x;}if(cap_wait_clear32(&CAP_VIF1_STAT,0x1f000000u,t)<0){r=-13;goto x;}CAP_VIF1_STAT=1u<<23;CAP_GS_BUSDIR=1;FlushCache(0);CAP_D1_QWC=qwc;CAP_D1_MADR=(u32)d;CAP_D1_CHCR=0x100;EE_SYNCL();if(cap_wait_clear32(&CAP_D1_CHCR,0x100,t)<0){r=-14;goto x;}r=1;x:CAP_D1_CHCR=oc;EE_SYNCL();CAP_VIF1_STAT=0;CAP_GS_BUSDIR=0;CAP_GS_IMR=oi;CAP_GS_CSR=2;{u128 e=(u128)CAP_VIF_MSKPATH3(0);CAP_VIF1_FIFO=e;}return r;}
static int raw_write_chunks(int f,const unsigned char*p,u32 n){while(n){u32 a=n>32768u?32768u:n;int w=write(f,p,a);if(w!=(int)a)return w<0?-21:-22;p+=a;n-=a;}return 0;}
/* Durante a captura o anel de audio PARA de ler do pendrive. O
   usbhdfsd nao e reentrante e esta funcao grava 1,1 MB nele pela
   thread principal. Sem isso, depois de algumas capturas a musica e
   as animacoes paravam de vez. */
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
 if(g_capture_raw_fd<0)r=-2;else if(safe_readback_rect(g_stage_pixels,fb,0,200)!=1)r=-11;else if(raw_write_chunks(g_capture_raw_fd,g_stage_pixels,b0)<0)r=-21;else if(safe_readback_rect(g_stage_pixels,fb,200,200)!=1)r=-12;else if(raw_write_chunks(g_capture_raw_fd,g_stage_pixels,b1)<0)r=-22;else if(safe_readback_rect(g_stage_pixels,fb,400,48)!=1)r=-13;else if(raw_write_chunks(g_capture_raw_fd,g_stage_pixels,b2)<0)r=-23;if(g_capture_raw_fd>=0){fsync(g_capture_raw_fd);close(g_capture_raw_fd);g_capture_raw_fd=-1;}capture_status(r?"RAW_FAILED":"RAW_COMPLETE",r?(-1):448,r);telemetry_event("screen_capture_raw",(int)g_capture_index,r);g_capture_index++;if(r==0&&g_capture_raw_fd<0){char np[96];snprintf(np,sizeof(np),"mass:/FNFPS2/FNF_SCREEN_%04u.rgba.txt",g_capture_index);g_capture_raw_fd=open(np,O_WRONLY|O_CREAT|O_TRUNC,0666);if(g_capture_raw_fd>=0)fsync(g_capture_raw_fd);}audio_ring_pause(0);return r;}
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

    k = snprintf(b, sizeof(b), "[VRAM] total 4194304, framebuffers 2293760\n");
    write_all_fd(fd, b, k);
    ed_tex(fd, "tex", e->tex);
    ed_tex(fd, "tex2", e->tex2);
    ed_tex(fd, "gftex", e->gftex);
    ed_tex(fd, "stagetex", e->stagetex);
    ed_tex(fd, "fonttex", e->fonttex);
    ed_tex(fd, "gftex16", &g_gftex16);

    k = snprintf(b, sizeof(b), "\n[HEAP] ponteiro e tamanho de cada buffer\n");
    write_all_fd(fd, b, k);
    {
        struct { const char *n; void *p; unsigned int t; } hs[] = {
            {"g_menu_pixels",    g_menu_pixels,    TITLE_SNAPSHOT_END},
            {"g_menu_compressed",g_menu_compressed,PRELOAD_RLE_BYTES},
            {"g_menu_sfx_pixels",g_menu_sfx_pixels,281088u},
            {"g_stage_pixels",   g_stage_pixels,   512u*256u*4u},
            {"g_hud_pixels",     g_hud_pixels,     320u*256u*4u},
            {"g_font_pixels",    g_font_pixels,    FONT_W*256u*4u},
            {"g_gf_pixels",      g_gf_pixels,      TITLE_STORE_BYTES},
            {"g_story_gf_pixels",g_story_gf_pixels,170u*157u*4u},
            {"g_story_bf_pixels",g_story_bf_pixels,164u*165u*4u},
            {"g_tutorial_bf_pack",g_tutorial_bf_pack,0},
            {"g_tutorial_gf_pack",g_tutorial_gf_pack,0},
        };
        unsigned int soma = 0;
        for (i = 0; i < (int)(sizeof(hs)/sizeof(hs[0])); i++) {
            unsigned int t = hs[i].t;
            if (i == 9) t = g_tutorial_bf_pack_bytes;
            if (i == 10) t = g_tutorial_gf_pack_bytes;
            soma += hs[i].p ? t : 0u;
            k = snprintf(b, sizeof(b), "  %-20s %s  %9u bytes\n",
                         hs[i].n, hs[i].p ? "alocado" : "  NULO ", t);
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
{int lm=load_metadata();capture_status("META_DONE",-1,lm);boot_mark("TELEMETRIA_RESULTADO",g_telemetry_init_result);if(lm<0)SleepThread();}{int ip=init_pad();capture_status("PAD_DONE",-1,ip);if(ip<0)SleepThread();}g_menu_pixels=g_pixels;capture_status("CACHE_ALLOC_BEGIN",-1,0);g_menu_compressed=memalign(128,PRELOAD_RLE_BYTES);g_menu_sfx_pixels=memalign(128,281088);if(!g_menu_compressed||!g_menu_sfx_pixels){capture_status("CACHE_ALLOC_FAIL",-1,-1);SleepThread();}capture_status("CACHE_READ_BEGIN",-1,0);if(read_exact_path("mass:/FNFPS2/assets/menu_cache.rle",g_menu_compressed,PRELOAD_RLE_BYTES)<0){capture_status("CACHE_RLE_FAIL",-1,-2);SleepThread();}telemetry_event("rle_size",(int)PRELOAD_RLE_BYTES,(int)MENU_RLE_BYTES);telemetry_event("rle_head",(int)(((u32)g_menu_compressed[0]<<24)|((u32)g_menu_compressed[1]<<16)|((u32)g_menu_compressed[2]<<8)|(u32)g_menu_compressed[3]),(int)(((u32)g_menu_compressed[MENU_RLE_BYTES]<<24)|((u32)g_menu_compressed[MENU_RLE_BYTES+1]<<16)|((u32)g_menu_compressed[MENU_RLE_BYTES+2]<<8)|(u32)g_menu_compressed[MENU_RLE_BYTES+3]));{int rfd=open("mass:/FNFPS2/assets/menu_cache.rle",O_RDONLY);if(rfd>=0){unsigned char rb[16];int rn=read(rfd,rb,16);close(rfd);telemetry_event("rle_reread",rn,rn>=4?(int)(((u32)rb[0]<<24)|((u32)rb[1]<<16)|((u32)rb[2]<<8)|(u32)rb[3]):-999);}else telemetry_event("rle_reread",-1,-998);}if(read_exact_path("mass:/FNFPS2/assets/menu_sfx.pcm",g_menu_sfx_pixels,281088)<0){capture_status("CACHE_SFX_FAIL",-1,-3);SleepThread();}capture_status("AUDIO_INIT_BEGIN",-1,0);audio_ok=(audio_init()==0);g_audio_ready=audio_ok;capture_status("AUDIO_INIT_DONE",-1,audio_ok?0:-1);if(!audio_ok)audio_playing=0;else audio_playing=0;transport_state=0;clock_hold=1;
 dma_channel_initialize(DMA_CHANNEL_GIF,NULL,0);dma_channel_fast_waits(DMA_CHANNEL_GIF);graph_vram_clear();
 fb[0].width=SCREEN_W;fb[0].height=SCREEN_H;fb[0].psm=GS_PSM_32;fb[0].mask=0;fb[0].address=graph_vram_allocate(SCREEN_W,SCREEN_H,GS_PSM_32,GRAPH_ALIGN_PAGE);fb[1]=fb[0];fb[1].address=graph_vram_allocate(SCREEN_W,SCREEN_H,GS_PSM_32,GRAPH_ALIGN_PAGE);
 z.enable=DRAW_DISABLE;z.address=0;z.mask=0;z.zsm=0;z.method=0;
 tex.width=320;tex.psm=GS_PSM_32;tex.address=graph_vram_allocate(320,256,GS_PSM_32,GRAPH_ALIGN_BLOCK);
 tex2=tex;tex2.address=graph_vram_allocate(320,256,GS_PSM_32,GRAPH_ALIGN_BLOCK);
 gftex.width=256;gftex.psm=GS_PSM_32;gftex.address=graph_vram_allocate(256,256,GS_PSM_32,GRAPH_ALIGN_BLOCK);g_gftex16=gftex;g_gftex16.width=320;g_gftex16.psm=GS_PSM_16;/* ANTES: g_gftex16 compartilhava o endereco da gftex (737280). Um upload PSMCT16 ali reinterpreta os dados PSMCT32 do MERCH: bytes de alfa viram lixo e pixels transparentes saem preto opaco -- a linha tracejada sob o MERCH. VRAM livre apos o fonttex: 163.840 B; este bloco usa 40.960 B. */g_gftex16.address=graph_vram_allocate(320,64,GS_PSM_16,GRAPH_ALIGN_BLOCK);
 stagetex.width=512;stagetex.psm=GS_PSM_32;stagetex.address=graph_vram_allocate(512,256,GS_PSM_32,GRAPH_ALIGN_BLOCK);
 fonttex.width=384;fonttex.psm=GS_PSM_32;fonttex.address=graph_vram_allocate(384,256,GS_PSM_32,GRAPH_ALIGN_BLOCK);
 telemetry_event("fb0",(int)fb[0].address,(SCREEN_W<<16)|SCREEN_H);telemetry_event("fb1",(int)fb[1].address,(SCREEN_W<<16)|SCREEN_H);telemetry_event("tex",(int)tex.address,(320<<16)|256);telemetry_event("tex2",(int)tex2.address,(320<<16)|256);telemetry_event("gftex",(int)gftex.address,(256<<16)|256);telemetry_event("stagetex",(int)stagetex.address,(512<<16)|256);telemetry_event("fonttex",(int)fonttex.address,(384<<16)|256);telemetry_event("tex1_fix",LOD_MIPMAP_REGISTER,0);telemetry_event("stage_src",g_official_mainstage_props[0].z,g_official_mainstage_chars[2].z);telemetry_event("stage_zoom",(int)(g_official_mainstage_camera_zoom*1000.0f),(int)g_official_mainstage_chars[0].x);telemetry_event("idle_blend",1,2);telemetry_event("merch_trim",59,3);capture_status("GS_BEGIN",-1,0);graph_initialize(fb[0].address,SCREEN_W,SCREEN_H,GS_PSM_32,0,0);graph_set_bgcolor(0,0,0);packet=packet_init(4096,PACKET_NORMAL);setup_texture(&tex);setup_texture_dims(&tex2,320,256);setup_texture_dims(&gftex,g_gf.width,g_gf.height);setup_texture_dims(&stagetex,512,256);setup_texture_dims(&fonttex,FONT_W,FONT_H);capture_status("GS_ENV_READY",-1,0);if(read_exact_path("mass:/FNFPS2/assets/title_alphabet.rgba",g_font_pixels,384*192*4)==0){setup_texture_dims(&fonttex,384,192);upload_raw(&fonttex,g_font_pixels,384,192);memcpy(g_menu_pixels+TITLE_SNAPSHOT_ALPHA,g_font_pixels,384*192*4);capture_status("TITLE_ALPHA_READY",-1,0);}if(read_exact_path("mass:/FNFPS2/assets/title_prompt_l.rgba",g_stage_pixels,512*192*4)==0){setup_texture_dims(&stagetex,512,192);upload_raw(&stagetex,g_stage_pixels,512,192);memcpy(g_menu_pixels+TITLE_SNAPSHOT_PROMPT_L,g_stage_pixels,512*192*4);capture_status("TITLE_PROMPT_L_READY",-1,0);}if(read_exact_path("mass:/FNFPS2/assets/title_prompt_r.rgba",g_hud_pixels,320*192*4)==0){setup_texture_dims(&tex2,320,192);upload_raw(&tex2,g_hud_pixels,320,192);memcpy(g_menu_pixels+TITLE_SNAPSHOT_PROMPT_R,g_hud_pixels,320*192*4);capture_status("TITLE_PROMPT_R_READY",-1,0);}if(read_exact_path("mass:/FNFPS2/assets/title_logoBumpin.frames",g_gf_pixels,TITLE_LOGO_FRAME_BYTES)==0&&read_exact_path("mass:/FNFPS2/assets/title_gfDanceTitle.frames",g_gf_pixels+TITLE_GF_STORE_OFFSET,TITLE_GF_FRAME_BYTES)==0){setup_texture_dims(&tex,TITLE_LOGO_W,TITLE_LOGO_H);gftex.width=256;setup_texture_dims(&gftex,TITLE_GF_W,TITLE_GF_H);upload_raw(&tex,g_gf_pixels,TITLE_LOGO_W,TITLE_LOGO_H);menu_anim_tick=-1;upload_raw(&gftex,g_gf_pixels+TITLE_GF_STORE_OFFSET,TITLE_GF_W,TITLE_GF_H);menu_anim_tick=-1;capture_status("TITLE_LOGO_GF_READY",-1,0);}if(read_exact_path("mass:/FNFPS2/assets/title_newgrounds.rgba",g_gf_pixels+TITLE_NG_STORE_OFFSET,181*175*4)==0){setup_texture_dims(&tex,181,175);upload_raw(&tex,g_gf_pixels+TITLE_NG_STORE_OFFSET,181,175);menu_anim_tick=-1;capture_status("TITLE_NG_READY",-1,0);} {int anim_rc=title_animation_cache_start();telemetry_event("title_anim_cache",anim_rc==0,anim_rc);telemetry_event("title_cache_policy",TITLE_CACHE_READ_CHUNK,TITLE_CACHE_AUDIO_RESERVE);}capture_status("FIRST_FRAME_PENDING",-1,0);{packet_t*tp=packet_init(8,PACKET_NORMAL);qword_t*tq=tp->data;tq->dw[0]=GIF_SET_TAG(1,1,0,0,GIF_FLG_PACKED,1);tq->dw[1]=GIF_REG_AD;tq++;tq->dw[0]=((u64)0x80<<32);tq->dw[1]=GS_REG_TEXA;tq++;dma_channel_send_normal(DMA_CHANNEL_GIF,tp->data,tq-tp->data,0,0);dma_wait_fast();packet_free(tp);telemetry_event("texa",0x80,0);}capture_status("F0_BLEND_BEGIN",-1,0);draw_enable_blending();capture_status("F0_BLEND_DONE",-1,0);
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
  if(g_congelar){g_congelar=0;fnx_congelado2(-1,frame_no,ui_state,selection,menu_cam_y,g_held);g_cong_frames=180;}if(g_cong_frames>0){g_cong_frames--;graph_wait_vsync();continue;}if(frame_no==0){capture_status("F0_TIMER_DONE",-1,0);}if(frame_no==0){capture_status("F0_PAD_READ_BEGIN",-1,0);}pressed=buttons_pressed();frame_mark("F.IN",(int)pressed);if(frame_no==0){capture_status("F0_PAD_READ_DONE",-1,(int)pressed);}g_viewport_360=(ui_state==0||ui_state==6||ui_state==7||ui_state==8);g_ui_state_dbg=ui_state;unsigned local,global;u32 heard,queued_bytes,ring_bytes;qword_t*q;int qw,i;
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
  if(ui_state==0||ui_state==6||ui_state==7)transport_state=0;else if(!audio_playing)transport_state=3;else if(audio_started)transport_state=2;
  /* 0.2.116: este laco nao tinha guarda nem saida por erro. audio_play_complete
     desiste em 2000 x 1 ms; se audsrv_play_audio parar de aceitar bytes,
     audsrv_available() nao diminui e o laco NUNCA sai, a ate 2 s por volta --
     "ficou lento e travou" na tela de titulo (o mesmo ui_state do BOOT.txt do
     console). O limite e 64 voltas: 64 x 4 KB = 256 KB, o dobro do anel de
     audio deste jogo, entao o caminho saudavel sai pela mesma condicao de
     antes. Quando o limite ou o erro pegam, fica registrado em AUDIO_STALL. */
  if((ui_state==0||ui_state==6||ui_state==7)&&!g_menu_audio_suspended&&g_title_audio_fd>=0){int voltas=0;while(audsrv_available()>=AUDIO_READ_CHUNK){int n;int j;if(++voltas>64){frame_mark("A.STOP_LACO",voltas);break;}n=audio_ring_take(g_audio_pcm,AUDIO_READ_CHUNK);if(n>0){if(g_menu_sfx_pos<g_menu_sfx_end){int k,frames=n/4;short*dst=(short*)g_audio_pcm;for(k=0;k<frames&&g_menu_sfx_pos<g_menu_sfx_end;k++){short*fx=(short*)(g_menu_sfx_pixels+g_menu_sfx_pos);int vl=(int)dst[k*2]+(int)fx[0],vr=(int)dst[k*2+1]+(int)fx[1];if(vl>32767)vl=32767;if(vl<-32768)vl=-32768;if(vr>32767)vr=32767;if(vr<-32768)vr=-32768;dst[k*2]=(short)vl;dst[k*2+1]=(short)vr;g_menu_sfx_phase+=147u;if(g_menu_sfx_phase>=320u){g_menu_sfx_phase-=320u;g_menu_sfx_pos+=4u;}}}j=audio_play_complete((char*)g_audio_pcm,n);if(j>0)g_title_audio_submitted+=(u32)(j/4);else{frame_mark("A.STOP_ERRO",j);break;}}else break;} }
  if(ui_state==8&&audio_ok&&audio_fd>=0&&g_tutorial_audio_status==0){
   if(g_tutorial_scene_ready&&audio_playing&&!audio_started&&audio_ring_level()>=65536){
    audio_started=1;clock_hold=0;music_clock_tick=begin;music_clock_fraction=0;transport_state=2;
    telemetry_event("tutorial_audio_start",(int)frame_no,(int)audio_ring_level());
   }
   if(audio_started&&audio_playing&&audsrv_available()>=AUDIO_READ_CHUNK&&audio_ring_level()>=STEM_READ_CHUNK){
    int submitted=tutorial_audio_mix_submit(voices_enabled);
    if(submitted==AUDIO_READ_CHUNK)audio_submitted+=(u32)(submitted/4);
    else{g_tutorial_audio_status=-6;audio_playing=0;audio_started=0;audsrv_stop_audio();telemetry_event("tutorial_audio_submit",submitted,(int)frame_no);}
   }
  }
  frame_mark("F.AUDIO",transport_state);
  {u32 reads=0;audio_ring_stats(&audio_read_bytes,&reads,&audio_loops,&audio_read_errors,&audio_underruns);(void)reads;}
  if(frame_no==0){capture_status("F0_UPDATE_AUDIO_DONE",-1,0);}queued_bytes=(audio_ok&&(g_title_audio_fd>=0||audio_fd>=0))?(u32)audsrv_queued():0;ring_bytes=(u32)audio_ring_level();heard=audio_submitted>=queued_bytes/4?audio_submitted-queued_bytes/4:0;
  /* Acumulador continuo: o RPC bruto apenas valida/corrige deriva; nao ancora
   * cada bloco. O resto fracionario evita perder subamostras entre frames. */
  if(clock_hold&&heard>=music_clock)clock_hold=0;
  if(!audio_playing||!audio_started||clock_hold){
   music_clock_tick=begin;
  }else{
   u64 dt=music_clock_tick?begin-music_clock_tick:0;
   u64 scaled=dt*(u64)AUDIO_RATE+music_clock_fraction;
   u32 advance=(u32)(scaled/(u64)kBUSCLK);
   music_clock_fraction=scaled%(u64)kBUSCLK;
   music_clock_tick=begin;
   if(advance&&music_clock+advance>music_clock)music_clock+=advance;
   if(music_clock>audio_submitted)music_clock=audio_submitted;
   /* Corrige somente deriva grande e gradualmente, sem salto de bloco. */
   if(heard>music_clock+1600u){u32 correction=heard-music_clock-1600u;if(correction>32u)correction=32u;music_clock+=correction;}
  }
  heard=music_clock;
  frame_mark("F.CLOCK",(int)music_clock);
  if(ui_state==8&&audio_started&&g_chart_status==0){
   const chart_event_t *event;
   while(g_chart_cursor<g_chart_info.event_count){
    event=chart_event(g_chart_cursor);if(!event||event->sample>music_clock)break;
    /* Every event's meaning now lives in chart_events.c, which reads the FCH4
       fields. The v3 code that was here packed one parameter into `value` and
       dropped the rest, so SetCameraBop, SetHealthIcon and PlayAnimation never
       reached the port at all. */
    chart_events_apply(event);
    g_chart_last_type=event->type;g_chart_last_value=event->value;g_chart_last_sample=event->sample;
    g_chart_cursor++;g_chart_seen++;
    telemetry_event("tutorial_chart_event",(int)event->type,(int)event->sample);
   }
  }
  {u32 aref=0;int at=-1,af2=0;
   while(chart_events_take_anim(&aref,&at,&af2)){
    const char*an=chart_event_string(aref);
    /* PlayAnimationSongEvent.hx: target 0 is bf/boyfriend, 2 is dad, 3 is gf,
       and force also decides whether the character plays vocals. The tutorial
       fires 'hey' on boyfriend and 'cheer' on dad; neither exists as a frame in
       the tutorial's packs, so the port reports the event instead of dropping
       it. A song whose packs carry those anims is where this becomes visible. */
    /* the anim name's first byte and the force flag, packed into one word */
    telemetry_event("tut_play_anim",at,(int)((an?(unsigned char)an[0]:0)|(af2<<8)));
   }}
  if(ui_state==8&&audio_started)tutorial_notes_update(music_clock,pressed,frame_no);
  if(ui_state==8&&audio_started&&audio_ring_eof()&&ring_bytes==0&&queued_bytes==0){
   audsrv_stop_audio();audio_playing=0;audio_started=0;g_tutorial_audio_finished=1;transport_state=4;
   music_clock=TUTORIAL_AUDIO_SAMPLES;heard=music_clock;
   telemetry_event("tutorial_audio_end",(int)music_clock,(int)frame_no);
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
  if(target_state==7&&transition_frame==(menu_back_transition?42:15)&&!story_assets_ready){telemetry_event("story_transicao_inicio",transition_frame,menu_back_transition);if(ui_state==8){telemetry_event("back_audsrv_begin",0,0);audsrv_stop_audio();telemetry_event("back_audsrv_done",0,0);audio_ring_stop();telemetry_event("back_ring_done",0,0);if(audio_fd>=0){close(audio_fd);audio_fd=-1;}telemetry_event("back_fd_closed",0,0);audio_started=0;audio_playing=0;audio_submitted=music_clock=0;music_clock_tick=music_clock_fraction=0;clock_hold=1;transport_state=0;tutorial_packs_free();telemetry_event("back_packs_freed",0,0);telemetry_event("tutorial_back_transport",0,(int)frame_no);}int su,st,sf,tb=1;if(ui_state==1){if(g_title_audio_fd>=0){close(g_title_audio_fd);g_title_audio_fd=-1;}audsrv_stop_audio();tb=(read_exact_path("mass:/FNFPS2/assets/title_logoBumpin.frames",g_gf_pixels,TITLE_LOGO_FRAME_BYTES)==0&&read_exact_path("mass:/FNFPS2/assets/title_gfDanceTitle.frames",g_gf_pixels+TITLE_GF_STORE_OFFSET,TITLE_GF_FRAME_BYTES)==0&&read_exact_path("mass:/FNFPS2/assets/title_alphabet.rgba",g_menu_pixels+TITLE_SNAPSHOT_ALPHA,384*192*4)==0&&read_exact_path("mass:/FNFPS2/assets/title_prompt_l.rgba",g_menu_pixels+TITLE_SNAPSHOT_PROMPT_L,512*192*4)==0&&read_exact_path("mass:/FNFPS2/assets/title_prompt_r.rgba",g_menu_pixels+TITLE_SNAPSHOT_PROMPT_R,320*192*4)==0);telemetry_event("title_resident",tb,37);}telemetry_event("back_packs_begin",0,0);{int sr=story_packs_load(&tex2,&gftex);menu_anim_tick=-1;telemetry_event("back_packs_done",sr,0);if(sr)capture_status("BACK_STORY_PACKS_FAIL",-1,sr);g_story_static_ready=(sr==0);telemetry_event("story_anim_load",g_story_static_ready,sr);}telemetry_event("back_ui_begin",0,0);su=story_ui_restore();telemetry_event("back_ui_done",su,0);if(su)capture_status("BACK_UI_FAIL",-1,su);if(su==0){setup_texture_dims(&stagetex,512,256);upload_raw(&stagetex,g_stage_pixels,512,256);}telemetry_event("back_titles_begin",0,0);st=story_titles_restore();telemetry_event("back_titles_done",st,0);if(st)capture_status("BACK_TITLES_FAIL",-1,st);if(st==0){story_upload_title(&tex,0);telemetry_event("tex_story",(int)tex.address,0);}sf=story_font_alpha_decode(g_menu_sfx_pixels+241920u+22656u,7672u,g_font_pixels,160u*120u);if(sf==0){u32 fp;unsigned int amin=255,amax=0,nmin=128,nmax=0;for(fp=0;fp<160u*120u;fp++){unsigned int a=g_font_pixels[fp*4u+3],na=(a*128u+127u)/255u;if(a<amin)amin=a;if(a>amax)amax=a;g_font_pixels[fp*4u+3]=(unsigned char)na;if(na<nmin)nmin=na;if(na>nmax)nmax=na;}FlushCache(0);setup_texture_dims(&fonttex,160,120);upload_raw(&fonttex,g_font_pixels,160,120);telemetry_event("story_alpha_src",(int)amin,(int)amax);telemetry_event("story_alpha_gs",(int)nmin,(int)nmax);telemetry_event("story_vtx_alpha",90,128);telemetry_event("story_gs_contract",(TEXTURE_COMPONENTS_RGBA<<16)|TEXTURE_FUNCTION_MODULATE,(1<<16)|0x5A);}story_assets_ready=(su==0&&st==0&&sf==0&&tb);if(!story_assets_ready)capture_status("BACK_ASSETS_FAIL",(su&255)|((st&255)<<8)|((sf&255)<<16),0);telemetry_event("story_assets",story_assets_ready,(su&255)|((st&255)<<8)|((sf&255)<<16));}
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
  memset(&g_chart_info,0,sizeof(g_chart_info));g_chart_cursor=g_chart_seen=0;g_chart_last_type=g_chart_last_value=g_chart_last_sample=0;
  tutorial_notes_reset();
  capture_status("TUT_CHART_BEGIN",(int)frame_no,0);
  /* The chart is resolved through the registry, not hardcoded. The tutorial is
     what the port plays, so its entry is looked up by id; the difficulty comes
     from the story menu's choice. sr_chart_path builds
     "<root>/songs/tutorial/<difficulty>.chart", which is where the game keeps
     it -- so the asset packaging has to match, and that is now the registry's
     job rather than a string literal buried in main.c. */
  {const sr_song_t*song=sr_song_by_id("tutorial");
   const char*diff=(story_diff==0)?"easy":(story_diff==2)?"hard":"normal";
   if(song){
    int layout=-1;
    /* NAO e so o caminho do jogo original: no console o arquivo estava la, no
       caminho exato, e o open() devolvia -1 do mesmo jeito. Tentar os layouts
       planos e a unica defesa possivel quando o log nao consegue dizer qual
       foi a falha. O layout que abrir sai no telemetria. */
    g_chart_status=chart_load_song_layout(FNF_ASSET_ROOT,song,diff,&g_chart_info,&layout);
    telemetry_event("chart_layout",layout,g_chart_status);
   } else g_chart_status=-3;
   telemetry_event("chart_path",g_chart_status,(int)(song?song->bpm*1000.0f:0));}
  /* The event module reads the chart through the same loader, so it needs the
     info struct and the step length. The tutorial is 100 BPM, so a step is
     150 ms, which at 48 kHz is 7200 samples. */
  chart_events_init(TUT_SAMPLES_PER_STEP);
  chart_events_bind(&g_chart_info);
  chart_rc=g_chart_status;
  capture_status("TUT_CHART_DONE",(int)frame_no,chart_rc);
  telemetry_event("tutorial_chart",g_chart_status,(int)g_chart_info.checksum);
  capture_status("TUT_FORMAT_BEGIN",(int)frame_no,sr);
  if(sr==0&&audio_ok)format_rc=audio_set_stream(audio_volume);
  capture_status("TUT_FORMAT_DONE",(int)frame_no,format_rc);
  if(sr==0&&audio_ok&&format_rc==0){
   capture_status("TUT_PCM_OPEN_BEGIN",(int)frame_no,0);
   audio_fd=open("mass:/FNFPS2/assets/audio_stems.pcm",O_RDONLY);
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
   if(transition_frame==transition_mid+1){int previous_state=ui_state;ui_state=target_state;telemetry_event("state",ui_state,(int)frame_no);if(ui_state==8){g_tut_cam_focus=0;g_tut_cam_ready=0;g_tut_zoom=g_official_mainstage_camera_zoom;g_tut_scroll=1.0f;g_bf_sing=g_gf_sing=-1;g_tut_bop=1.0f;chart_events_reset(g_official_mainstage_camera_zoom,1.0f);capture_status("TUT_STATE_ACTIVE",(int)frame_no,g_tutorial_audio_status);}transform_mode=0;last=-1;last2=-1;if(ui_state==6){selection=g_menu_remembered;/* O original SNAPA a camera para o item lembrado, nao varre: L242
   selectItem() dispara onChange (MenuList.hx L341, sem guarda) que move
   camFollow para o item selecionado ANTES de resetCamStuff()/snapToTarget()
   (L258 + L343-345). Por isso menu_cam_y ja nasce no alvo. */menu_cam_y=40.0f+160.0f*(float)g_menu_remembered;menu_placeholder=0;menu_anim_tick=-1;if(previous_state==1){audsrv_stop_audio();g_title_audio_submitted=0;g_menu_sfx_pos=g_menu_sfx_end=g_menu_sfx_phase=0;if(g_title_audio_fd>=0)close(g_title_audio_fd);g_title_audio_fd=-1;if(audio_ok&&audio_set_stream(audio_volume)==0)g_title_audio_fd=open("mass:/FNFPS2/assets/freakyMenu_48000.pcm",O_RDONLY);if(g_title_audio_fd>=0){audio_ring_stop();audio_ring_start_loop(g_title_audio_fd,1);}telemetry_event("menu_restore",0,g_title_audio_fd>=0); }setup_texture_dims(&stagetex,512,256);memcpy(g_up_bounce,g_menu_pixels+MENU_FRAMES_BYTES,512u*256u*4u);upload_raw(&stagetex,g_up_bounce,512,256);}else if(ui_state==7){{u32 aq0=(g_title_audio_fd>=0)?(u32)audsrv_queued():0u;g_story_anim_heard_samples=g_title_audio_submitted>aq0/4u?g_title_audio_submitted-aq0/4u:0u;}story_diff=1;story_level=0;g_story_title_y_ready=0;if(previous_state==1||previous_state==8){audsrv_stop_audio();g_title_audio_submitted=0;g_menu_sfx_pos=g_menu_sfx_end=g_menu_sfx_phase=0;if(g_title_audio_fd>=0)close(g_title_audio_fd);g_title_audio_fd=-1;if(audio_ok&&audio_set_stream(audio_volume)==0)g_title_audio_fd=open("mass:/FNFPS2/assets/freakyMenu_48000.pcm",O_RDONLY);if(g_title_audio_fd>=0){audio_ring_stop();audio_ring_start_loop(g_title_audio_fd,1);menu_audio_prime();g_menu_audio_suspended=0;}}telemetry_event("story_music",previous_state,g_title_audio_fd>=0);}else if(ui_state==0){telemetry_event("title_anim_reuse",g_title_anim_status,g_title_anim_pixels!=0);audio_started=0;audio_playing=0;clock_hold=1;transport_state=0;title_intro_active=0;title_confirm_frames=0;title_intro_flash_frames=60;title_logo_last=title_gf_last=-1;setup_texture_dims(&fonttex,384,192);memcpy(g_up_bounce,g_menu_pixels+TITLE_SNAPSHOT_ALPHA,384u*192u*4u);upload_raw(&fonttex,g_up_bounce,384,192);menu_anim_tick=-1;setup_texture_dims(&stagetex,512,192);memcpy(g_up_bounce,g_menu_pixels+TITLE_SNAPSHOT_PROMPT_L,512u*192u*4u);upload_raw(&stagetex,g_up_bounce,512,192);setup_texture_dims(&tex2,320,192);memcpy(g_up_bounce,g_menu_pixels+TITLE_SNAPSHOT_PROMPT_R,320u*192u*4u);upload_raw(&tex2,g_up_bounce,320,192);menu_anim_tick=-1;setup_texture_dims(&tex,TITLE_LOGO_W,TITLE_LOGO_H);gftex.width=256;setup_texture_dims(&gftex,TITLE_GF_W,TITLE_GF_H);upload_raw(&tex,g_gf_pixels,TITLE_LOGO_W,TITLE_LOGO_H);menu_anim_tick=-1;upload_raw(&gftex,g_gf_pixels+TITLE_GF_STORE_OFFSET,TITLE_GF_W,TITLE_GF_H);menu_anim_tick=-1;title_ng_restored=1;if(g_title_audio_fd<0&&audio_ok&&audio_set_stream(audio_volume)==0){g_title_audio_fd=open("mass:/FNFPS2/assets/freakyMenu_48000.pcm",O_RDONLY);if(g_title_audio_fd>=0){audio_ring_stop();audio_ring_start_loop(g_title_audio_fd,1);telemetry_event("ring_source",0,1);}}}}
   if(transition_frame>transition_end){transition_frame=0;target_state=-1;repeat_dir=0;repeat_frames=0;menu_back_transition=0;}
  }
  if(transition_frame==0&&ui_state==8&&g_tutorial_audio_finished&&(pressed&PAD_CROSS)&&g_tutorial_audio_status==0&&audio_fd>=0){
   audsrv_stop_audio();audio_started=0;audio_playing=0;audio_submitted=music_clock=0;
   music_clock_tick=music_clock_fraction=0;clock_hold=1;g_tutorial_audio_finished=0;g_chart_cursor=g_chart_seen=0;g_chart_last_type=g_chart_last_value=g_chart_last_sample=0;tutorial_notes_reset();
   if(audio_ring_reset()==0){audio_playing=1;transport_state=1;g_tutorial_audio_status=0;telemetry_event("tutorial_audio_replay",(int)frame_no,0);}
   else g_tutorial_audio_status=-7;
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
tutorial_packs_free();story_assets_ready=0;target_state=7;transition_frame=1;transitions++;}else{menu_placeholder=selection;if(menu_font_restore()==0){setup_texture_dims(&fonttex,FONT_W,FONT_H);upload_raw(&fonttex,g_font_pixels,FONT_W,FONT_H);}}}}menu_cam_y+=(40.0f+160.0f*(float)selection-menu_cam_y)*0.06f;
  }else if(transition_frame==0&&ui_state==7){if(pressed&PAD_UP){int sr;story_level=(story_level+8)%9;sr=story_titles_restore();if(sr==0)story_upload_title(&tex,story_level);telemetry_event("story_title_restore",story_level,sr);g_menu_sfx_pos=0;g_menu_sfx_end=39168;g_menu_sfx_phase=0;}else if(pressed&PAD_DOWN){int sr;story_level=(story_level+1)%9;sr=story_titles_restore();if(sr==0)story_upload_title(&tex,story_level);telemetry_event("story_title_restore",story_level,sr);g_menu_sfx_pos=0;g_menu_sfx_end=39168;g_menu_sfx_phase=0;}if(pressed&PAD_LEFT){story_diff=(story_diff+2)%3;story_diff_tween=0;telemetry_event("story_diff_tween",story_diff,0);g_menu_sfx_pos=0;g_menu_sfx_end=39168;g_menu_sfx_phase=0;}else if(pressed&PAD_RIGHT){story_diff=(story_diff+1)%3;story_diff_tween=0;telemetry_event("story_diff_tween",story_diff,0);g_menu_sfx_pos=0;g_menu_sfx_end=39168;g_menu_sfx_phase=0;}if(pressed&PAD_CIRCLE){target_state=6;transition_frame=1;transitions++;}else if(pressed&PAD_CROSS&&story_level==0){g_tutorial_scene_attempted=0;g_tutorial_scene_ready=0;target_state=8;transition_frame=1;transitions++;}
  }else if(transition_frame==0 && (pressed&PAD_CIRCLE)){if(ui_state==0&&components_mode)components_mode=0;else if(ui_state==8){g_menu_cache_ready=0;story_assets_ready=0;target_state=7;transition_frame=1;transitions++;}else if(ui_state==1){g_menu_cache_ready=0;story_assets_ready=0;target_state=7;transition_frame=1;transitions++;}else{target_state=0;transition_frame=1;transitions++;}}
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
  {const chart_event_state_t*ev=chart_events_state();g_tut_cam_focus=(ev->cam_char==CHART_FOCUS_GF||ev->cam_char==CHART_FOCUS_DAD)?1:0;}
  if(ui_state==8&&g_tutorial_scene_ready&&g_tutorial_audio_status==0){u32 af=(u32)(((u64)music_clock*24u)/48000u);int bfi=tut_anim_idx(&g_bf_sing,g_bf_sing_at,music_clock,BF_SING,14u,af),gfi=tut_anim_idx(&g_gf_sing,g_gf_sing_at,music_clock,GF_SING,30u,af);if(bfi!=g_tutorial_bf_uploaded&&tutorial_pack_frame(g_tutorial_bf_pack,g_tutorial_bf_pack_bytes,(u32)bfi,43,258u*254u*4u,g_stage_pixels)==0){FlushCache(0);setup_texture_dims(&tex,258,254);upload_raw(&tex,g_stage_pixels,258,254);g_tutorial_bf_uploaded=bfi;}if(gfi!=g_tutorial_gf_uploaded&&tutorial_pack_frame(g_tutorial_gf_pack,g_tutorial_gf_pack_bytes,(u32)gfi,87,320u*307u*2u,g_hud_pixels)==0){FlushCache(0);setup_texture_dims(&g_gftex16,320,307);upload_raw_psm(&g_gftex16,g_hud_pixels,320,307,GS_PSM_16);g_tutorial_gf_uploaded=gfi;}}
  if(ui_state==6){frame_mark("F.BG",(int)menu_cam_y);qword_t*mq=packet->data;int tick=(int)(frame_no*24u/60u),mi;setup_texture_dims(&stagetex,512,256);memcpy(g_up_bounce,g_menu_pixels+MENU_FRAMES_BYTES,512u*256u*4u);upload_raw(&stagetex,g_up_bounce,512,256);mq=draw_setup_environment(mq,0,&fb[draw],&z);mq=draw_clear(mq,0,0,0,SCREEN_W,SCREEN_H,0,0,0);mq=bind_texture(mq,&stagetex);{/* 0.2.104 A4: parallax do fundo, scrollFactor.y = 0,17 no original
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
  q=packet->data;q=draw_setup_environment(q,0,&fb[draw],&z);/* NAO limpar no estado 6. O menu desenha o fundo num pacote proprio,
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
  }else if(ui_state==8&&g_tutorial_scene_ready){q=draw_scissor_area(q,0,0,SCREEN_W-1,SCISSOR_TOPO,SCISSOR_BASE);band_set(SCISSOR_TOPO,SCISSOR_BASE);/* Official character-space reconstruction. BFA2 canvases are already stored at half of the original 1280x720 character canvas, while stage positions and metadata offsets remain original-space values. Final top-left follows Stage.addCharacter/BaseCharacter.setScale: feet - full canvas origin + per-character global offset. Camera focus follows BaseCharacter.resetCameraFocusPoint plus stage camera offset. */{const chart_event_state_t*ev=chart_events_state();chart_events_tick(TUT_DT,music_clock/TUT_SAMPLES_PER_STEP);g_tut_zoom=ev->zoom;g_tut_scroll=ev->scroll;g_tut_bop=ev->bop;}float sc=.5f*g_tut_zoom*g_tut_bop;const float bf_source_w=496.920f,bf_source_h=490.920f,bf_pack_scale=.5f,gf_source_w=703.300f,gf_source_h=674.850f,gf_pack_scale=.443623f;/* A GF ocupa o slot gf [751.5,787], provado pela captura do jogo real:
   a arte dela comeca em 6 px dentro da tela de 255x245 do pack, o que
   da 6/0,35 = 17,14 unidades de mundo. No slot dad isso cairia em
   -181,5 de tela, fora do ecra; no slot gf cai em +47,6, visivel.
   Nenhuma largura <= 728 faz o slot dad alcancar a posicao observada. */
/* gf.hxc escreve originalPosition = pos - characterOrigin e chama
   resetPosition(), que em Bopper.hx so faz x=originalPosition.x. Nao
   ha globalOffsets nessa linha, entao o -12/+4 do gf.json NAO entra
   na posicao do sprite. O foco de camera fica como esta: o script nao
   chama resetCameraFocusPoint e o teste de hardware aprovou. */
float gf_world_x=g_official_mainstage_chars[0].x-gf_source_w*.5f-(4.0f/gf_pack_scale),gf_world_y=g_official_mainstage_chars[0].y-gf_source_h-(4.0f/gf_pack_scale);float bf_world_x=g_official_mainstage_chars[2].x-bf_source_w*.5f+17.0f-(4.0f/bf_pack_scale),bf_world_y=g_official_mainstage_chars[2].y-bf_source_h+14.0f-(4.0f/bf_pack_scale);/* cameraFocusPoint = originalPosition + tamanho/2 + cameraOffsets, mais o
   deslocamento de camera do palco (-18,+36), que vale para todo personagem.
   BF: 989.5+17-18-100 = 888.50 ; 885+14-246+36-100 = 589.00  (inalterado)
   GF: 335-12-18+150  = 455.00 ; 885+4-338+36-100  = 487.00 */
float bf_cam_x=g_official_mainstage_chars[2].x+17.0f-18.0f+g_official_mainstage_chars[2].sx,bf_cam_y=g_official_mainstage_chars[2].y+14.0f-bf_source_h*.5f+36.0f+g_official_mainstage_chars[2].sy;/* cameraFocusPoint = (position + offsets) + char.cameraOffsets +
   slot.cameraOffsets. Reproduz o BF digito a digito:
   (989,5+17)-18-100 = 888,50 ; (885+14-246)+36-100 = 589,00.
   GF no slot gf: (751,5-12+12)+0 = 751,50 ;
                  (787+4-338+8)+0 = 461,00 */
float gf_cam_x=g_official_mainstage_chars[0].x+GF_OFFSET_X+GF_CAMOFF_X+g_official_mainstage_chars[0].sx,gf_cam_y=g_official_mainstage_chars[0].y+GF_OFFSET_Y-gf_source_h*.5f+GF_CAMOFF_Y+g_official_mainstage_chars[0].sy;float tcx=g_tut_cam_focus?gf_cam_x:bf_cam_x,tcy=g_tut_cam_focus?gf_cam_y:bf_cam_y,cx,cy;if(!g_tut_cam_ready){g_tut_cam_x=tcx;g_tut_cam_y=tcy;g_tut_cam_ready=1;}else{g_tut_cam_x+=(tcx-g_tut_cam_x)*TUT_CAM_LERP;g_tut_cam_y+=(tcy-g_tut_cam_y)*TUT_CAM_LERP;}cx=g_tut_cam_x;cy=g_tut_cam_y;float x,y,w,h;q=bind_texture_filter(q,&stagetex,1);{float stx=320.0f+(STAGE_WIN_X-cx*STAGE_SCROLL+640.0f*STAGE_SCROLL-640.0f)*sc,sty=180.0f+(STAGE_WIN_Y-cy*STAGE_SCROLL+360.0f*STAGE_SCROLL-360.0f)*sc;q=sprite_dims(q,stx,sty,STAGE_WIN_W*sc,STAGE_WIN_H*sc,512,256);}q=bind_texture_filter(q,&g_gftex16,1);x=320.0f+(gf_world_x-cx)*sc;y=180.0f+(gf_world_y-cy)*sc;w=g_tutorial_gf_w*sc/gf_pack_scale;h=g_tutorial_gf_h*sc/gf_pack_scale;q=sprite_dims(q,x,y,w,h,g_tutorial_gf_w,g_tutorial_gf_h);q=bind_texture_filter(q,&tex,1);x=320.0f+(bf_world_x-cx)*sc;y=180.0f+(bf_world_y-cy)*sc;w=g_tutorial_bf_w*sc/bf_pack_scale;h=g_tutorial_bf_h*sc/bf_pack_scale;q=sprite_dims(q,x,y,w,h,g_tutorial_bf_w,g_tutorial_bf_h);
  }
  if(ui_state==8&&g_tutorial_scene_ready){q=tutorial_notes_draw(q,&tex2,&fonttex,music_clock,g_held,frame_no);/* Official HUD core at half of 1280x720 camHUD coordinates. healthBarBG=(339.5,648), inner bar=(343.5,652), health=1.0 gives 50% red/green, icons use HealthIcon size 150 and POSITION_OFFSET 26. */q=bind_texture_filter(q,&tex2,1);q=sprite_region(q,169.75f,324.0f,300.5f,9.5f,0,112,301,10);{/* A barra oficial e' um FlxBar RIGHT_TO_LEFT de largura 593 (601 do fundo
     menos 8) e valor 0..2 (HEALTH_MAX), verde do jogador preenchido a partir
     da DIREITA. Em 640x360, x0=171.75 e largura 296.5, entao a divisao esta'
     em x1 - largura*(health/2). Com health=1,0 isso da' 320.0 -- que era o
     numero fixo que estava aqui e so' valia no primeiro quadro. */
    float hf=fnf_play_health_ratio(&g_play),sx=468.25f-296.5f*hf;
    if(sx<171.75f)sx=171.75f;
    if(sx>468.25f)sx=468.25f;
    if(sx>171.75f){
        q=rect_alpha(q,171.75f,326.0f,sx,331.5f,255,0,0,128);
    }
    if(sx<468.25f){
        q=rect_alpha(q,sx,326.0f,468.25f,331.5f,102,255,51,128);
    }}q=sprite_region(q,258.0f,288.5f,75.0f,75.0f,0,128,75,75);q=sprite_region(q,307.0f,288.5f,75.0f,75.0f,155,128,-75,75);/* scoreText: FlxBitmapText defaults autoSize=true and fieldWidth=0; official FlxBitmapText states alignment has no visual effect in autoSize mode. Therefore glyph origin is exactly x=(339.5+601-190)/2=375.25, not a 190px field ending at 470.25. Four half-pixel black passes reconstruct borderSize=1 before the white glyph pass. */{/* PlayState.updateScoreText(): "Score: " + FlxStringUtil.formatMoney(
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
  if(ui_state==8&&!g_tutorial_scene_ready){char msg[40];snprintf(msg,sizeof(msg),"TUTORIAL ASSET ERROR %d",g_tutorial_scene_status);q=text_draw(q,msg,320,200,1.0f,1);q=text_draw(q,"CIRCLE  RETURN TO STORY",320,260,.70f,1);}
  if(ui_state==0 && components_mode){char v[32];q=text_draw(q,"UI COMPONENTS",320,30,2.0f,1);snprintf(v,sizeof(v),"TOGGLE: %s",toggle_value?"ON":"OFF");q=text_draw(q,v,155,94,1.2f,0);snprintf(v,sizeof(v),"SLIDER: %d",slider_value);q=text_draw(q,v,155,156,1.2f,0);snprintf(v,sizeof(v),"TAB: %d   L1/R1",tab_value+1);q=text_draw(q,v,155,218,1.2f,0);q=text_draw(q,"DISABLED OPTION",155,280,1.2f,0);q=text_draw(q,"SCROLL ITEM",155,342,1.2f,0);q=text_draw(q,"TRIANGLE/CIRCLE: BACK",320,410,1.0f,1);}
  else if(ui_state==0){u32 ib=title_intro_phase();if(title_intro_active){if(ib!=g_title_intro_phase_reported){telemetry_event("title_credit_phase",(int)ib,(int)g_title_intro_heard_samples);g_title_intro_phase_reported=ib;}if(ib>=1&&ib<4){q=title_alpha(q,&fonttex,"NINJAMUFFIN99",320,100,1.0f);q=title_alpha(q,&fonttex,"PHANTOMARCADE",320,130,1.0f);q=title_alpha(q,&fonttex,"KAWAISPRITE",320,160,1.0f);q=title_alpha(q,&fonttex,"EVILSK8R",320,190,1.0f);if(ib>=3)q=title_alpha(q,&fonttex,"PRESENT",320,220,1.0f);}else if(ib>=5&&ib<8){q=title_alpha(q,&fonttex,"IN ASSOCIATION",320,100,1.0f);q=title_alpha(q,&fonttex,"WITH",320,130,1.0f);if(ib>=7){q=title_alpha(q,&fonttex,"NEWGROUNDS",320,160,1.0f);q=bind_texture(q,&tex);q=sprite_dims(q,229.5f,187.2f,181.0f,175.0f,181,175);}}else if(ib>=9&&ib<12){q=title_alpha(q,&fonttex,"DOPE ASS GAME",320,100,1.0f);if(ib>=11)q=title_alpha(q,&fonttex,"PLAYSTATION MAGAZINE",320,130,.75f);}else if(ib>=13&&ib<16){q=title_alpha(q,&fonttex,"FRIDAY",320,100,1.0f);if(ib>=14)q=title_alpha(q,&fonttex,"NIGHT",320,130,1.0f);if(ib>=15)q=title_alpha(q,&fonttex,"FUNKIN",320,160,1.0f);}else if(ib>=16){title_intro_flash_frames=60;title_intro_active=0;}}else{unsigned int pf=title_confirm_frames>0?(2u+((title_confirm_frames/4u)&1u)):((frame_no/30u)&1u);q=bind_texture(q,&stagetex);q=sprite_region(q,50,288,512,40,0,pf*48+4,512,40);q=bind_texture(q,&tex2);q=sprite_region(q,562,288,228,40,0,pf*48+4,228,40);q=bind_texture_filter(q,&fonttex,1);}}
  if(ui_state==0&&title_confirm_frames>60){unsigned char ca=(unsigned char)(((u32)(title_confirm_frames-60)*0x80u)/60u);q=rect_alpha(q,0,0,SCREEN_W,SCREEN_H,255,255,255,ca);}
  if(ui_state==0&&title_intro_flash_frames>0){unsigned char fa=(unsigned char)(((u32)title_intro_flash_frames*0x80u)/60u);q=rect_alpha(q,0,0,SCREEN_W,SCREEN_H,255,255,255,fa);title_intro_flash_frames--;}
  if(transition_frame>0){int tm=menu_back_transition?42:15,te=menu_back_transition?102:30,p=transition_frame<=tm?transition_frame:(te-transition_frame);unsigned char a=(unsigned char)(transition_frame<=tm?((p*0x80)/tm):((p*0x80)/(te-tm)));q=rect_alpha(q,0,0,SCREEN_W,SCREEN_H,0,0,0,a);}
  if(g_calib){q=draw_scissor_area(q,0,0,SCREEN_W-1,0,SCREEN_H-1);
              q=desenhar_calibracao(q);}
  q=draw_finish(q);qw=(int)(q-packet->data);if(frame_no==0)capture_status("F0_PACKET_DONE",-1,qw);frame_mark("F.DMA",qw);dma_channel_send_normal(DMA_CHANNEL_GIF,packet->data,qw,0,0);if(frame_no==0)capture_status("F0_DMA_SENT",-1,0);frame_mark("F.WAIT",qw);draw_wait_finish();frame_mark("F.DRAWN",qw);if(frame_no==0)capture_status("F0_DRAW_FINISH_DONE",-1,0);if(ui_state==7&&transition_frame==0){int si;story_title_update_positions(story_level);for(si=0;si<9;si++){const unsigned int*sr=g_story_title[si];float yy=g_story_title_y[si]-44.0f,hh=(float)sr[2];if(vy(yy+hh)<269.0f||vy(yy)>391.0f)continue;story_upload_title(&tex,si);menu_anim_tick=-1;{qword_t*tq=packet->data;tq=draw_setup_environment(tq,0,&fb[draw],&z);tq=draw_scissor_area(tq,0,0,SCREEN_W-1,269,391);band_set(269,391);tq=bind_texture_filter(tq,&tex,0);tq=sprite_region_alpha(tq,320.0f-(float)sr[1]*.5f,yy,(float)sr[1],hh,0,0,(float)sr[1],hh,si==story_level?128:77);tq=draw_finish(tq);dma_channel_send_normal(DMA_CHANNEL_GIF,packet->data,tq-packet->data,0,0);draw_wait_finish();}}}after_draw=GetTimerSystemTime();if((g_held&(PAD_SELECT|PAD_R3))!=(PAD_SELECT|PAD_R3))g_capture_chord_latched=0;if(!g_cap_busy&&(g_forcar_captura||(!g_capture_chord_latched&&transition_frame==0&&(g_held&(PAD_SELECT|PAD_R3))==(PAD_SELECT|PAD_R3)&&(pressed&(PAD_SELECT|PAD_R3))))){g_forcar_captura=0;int cr,rr=0,endpos,restore_rc=-1;u32 saved_sample=0;int capture_ui=ui_state;g_capture_chord_latched=1;if(capture_ui==8&&audio_fd>=0&&g_tutorial_audio_status==0){{u32 aq_t=(u32)audsrv_queued();saved_sample=audio_submitted>=aq_t/4u?audio_submitted-aq_t/4u:0u;if(saved_sample>TUTORIAL_AUDIO_SAMPLES)saved_sample=TUTORIAL_AUDIO_SAMPLES;}capture_status("RAW_TUT_PAUSE",(int)frame_no,(int)saved_sample);audsrv_stop_audio();audio_playing=0;audio_started=0;clock_hold=1;transport_state=0;audio_ring_stop();close(audio_fd);audio_fd=-1;DelayThread(500000);cr=capture_framebuffer_raw(fb[draw].address<<5,frame_no,ui_state);audio_fd=open("mass:/FNFPS2/assets/audio_stems.pcm",O_RDONLY);if(audio_fd>=0){endpos=(int)lseek(audio_fd,0,SEEK_END);if(endpos>0){u64 bp=(u64)saved_sample*8u;if(bp>(u64)(u32)endpos)bp=(u64)(u32)endpos;lseek(audio_fd,(int)bp,SEEK_SET);}restore_rc=audio_ring_start(audio_fd);if(restore_rc==0){audio_submitted=music_clock=saved_sample;music_clock_tick=GetTimerSystemTime();music_clock_fraction=0;clock_hold=1;audio_playing=1;audio_started=0;transport_state=1;g_tutorial_audio_status=0;}else{close(audio_fd);audio_fd=-1;g_tutorial_audio_status=-8;}}capture_status(restore_rc==0?"RAW_TUT_RESUMED":"RAW_TUT_RESUME_FAIL",(int)frame_no,restore_rc);telemetry_event("capture_tutorial",cr,restore_rc);}else if(g_title_audio_fd>=0){u32 aq=(u32)audsrv_queued(),menu_heard=g_title_audio_submitted>=aq/4u?g_title_audio_submitted-aq/4u:0;capture_status("RAW_MENU_PAUSE",(int)frame_no,(int)menu_heard);audsrv_stop_audio();g_menu_audio_suspended=1;audio_ring_stop();close(g_title_audio_fd);g_title_audio_fd=-1;DelayThread(500000);cr=capture_framebuffer_raw(fb[draw].address<<5,frame_no,ui_state);if(cr==0&&ui_state==7)rr=story_titles_restore();else if(cr==0&&ui_state==0)memcpy(g_stage_pixels,g_menu_pixels+TITLE_SNAPSHOT_PROMPT_L,512*192*4);g_title_audio_fd=open("mass:/FNFPS2/assets/freakyMenu_48000.pcm",O_RDONLY);if(g_title_audio_fd>=0){endpos=(int)lseek(g_title_audio_fd,0,SEEK_END);if(endpos>0)lseek(g_title_audio_fd,(int)((menu_heard*4u)%(u32)endpos),SEEK_SET);g_title_audio_submitted=menu_heard;if(audio_ring_start_loop(g_title_audio_fd,1)>=0){int guard=2000;while(audio_ring_level()<65536&&guard--){DelayThread(1000);}guard=64;while(audsrv_available()>=AUDIO_READ_CHUNK&&guard--){int n,j;if((n=audio_ring_take(g_audio_pcm,AUDIO_READ_CHUNK))!=AUDIO_READ_CHUNK){DelayThread(1000);continue;}j=audio_play_complete((char*)g_audio_pcm,n);if(j<=0)break;g_title_audio_submitted+=(u32)(j/4);}g_menu_audio_suspended=0;restore_rc=0;}}capture_status(restore_rc==0?"RAW_MENU_RESUMED":"RAW_MENU_RESUME_FAIL",(int)frame_no,restore_rc);telemetry_event("capture_menu",cr,rr);}else{capture_status("RAW_NO_TRANSPORT",(int)frame_no,ui_state);}}frame_mark("F.VSYNC",0);if(frame_no==0)capture_status("F0_VSYNC_BEGIN",-1,0);graph_wait_vsync();if(frame_no==0)capture_status("F0_VSYNC_DONE",-1,0);graph_set_framebuffer_filtered(fb[draw].address,SCREEN_W,GS_PSM_32,0,0);if(frame_no==0)capture_status("F0_FRAMEBUFFER_SET",-1,0);after_vsync=GetTimerSystemTime();if(deferred_menu_start&&frame_no>=3u){deferred_menu_start=0;capture_status("DEFERRED_AUDIO_BEGIN",-1,0);if(audio_ok&&audio_set_stream(audio_volume)==0){g_title_audio_fd=open("mass:/FNFPS2/assets/freakyMenu_48000.pcm",O_RDONLY);if(g_title_audio_fd>=0)audio_ring_start_loop(g_title_audio_fd,1);}}
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




