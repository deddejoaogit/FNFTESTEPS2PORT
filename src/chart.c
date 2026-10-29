#include "chart.h"
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#define CHART_HEADER_BYTES 40u
/* Limites elevados em 0.2.76, e corrigidos em 0.2.105.
   A nota da 0.2.76 dizia que o maior chart real era 2hot com 906
   notas. Isso era o NORMAL. Medindo os 81 charts oficiais
   (27 musicas x 3 dificuldades) convertidos dos JSON do jogo
   instalado, o maior e 2hot-HARD com 1.035 notas -- que estourava
   o limite de 1.024 e fazia o loader devolver -5, ou seja, a
   musica mais dificil do jogo nao carregava. Os demais maximos
   reais: 12 bpm (monster-easy), 104 secoes (guns-easy), 468
   eventos (spaghetti-easy).

   1.280 deixa ~24% de folga sobre o maior observado e custa 3.072 B
   de RAM estatica a mais (o payload passa de 24.832 B para
   27.904 B). Os quatro limites sao verificados por
   test/test_chart_limits.c contra os charts convertidos, para que
   um chart maior nao volte a quebrar em silencio. */
/* The four CHART_MAX_* limits live in the header now, so the test suite can
   check them. They are documented there. */
#define CHART_HEADER_BYTES_V3 40u
#define CHART_HEADER_BYTES_V4 48u
/* v3 events are 16 B, v4 events are 32 B. The buffer holds the worst case. */
#define CHART_EVENT_STRIDE_V3 16u
#define CHART_EVENT_STRIDE_V4 32u
#define CHART_MAX_PAYLOAD (CHART_MAX_NOTES*12u+CHART_MAX_BPMS*16u+CHART_MAX_SECTIONS*8u+CHART_MAX_EVENTS*CHART_EVENT_STRIDE_V4)

typedef struct { u32 start_step,start_sample,samples_per_step,bpm; } chart_bpm_t;
static unsigned char chart_payload[CHART_MAX_PAYLOAD] __attribute__((aligned(16)));
/* v4's string table: u32 offsets into chart_string_data, then the NUL-terminated
   strings. The block table follows it: a u32 count, then length-prefixed blobs. */
static const char*chart_strings[CHART_MAX_STRINGS];
/* v4's two tables, read as one tail: the string table (u32 offsets then the
   NUL-terminated strings) followed by the block table (a u32 count, then
   length-prefixed blobs). Reading them together means the file size check
   needs no separate accounting for each table. */
static unsigned char chart_tables[CHART_MAX_STRING_BYTES+CHART_MAX_BLOCK_BYTES];
static u32 chart_block_lengths[CHART_MAX_STRINGS];
static u32 chart_block_offsets[CHART_MAX_STRINGS];
static chart_bpm_t chart_bpms[CHART_MAX_BPMS];
static chart_event_t chart_events[CHART_MAX_EVENTS];
static chart_info_t chart_info;

static u16 chart_u16(const unsigned char *p){return (u16)((u16)p[0]|((u16)p[1]<<8));}
static u32 chart_u32(const unsigned char *p){return (u32)p[0]|((u32)p[1]<<8)|((u32)p[2]<<16)|((u32)p[3]<<24);}
static u32 chart_fnv1a(const unsigned char *p,u32 n){u32 h=2166136261u,i;for(i=0;i<n;i++){h^=p[i];h*=16777619u;}return h;}
/* FNV-1a continued from a previous block, so the payload and the tables hash
   as one stream the way the converter computed them. */
static u32 chart_fnv1a_more(const unsigned char *p,u32 n,u32 h){u32 i;for(i=0;i<n;i++){h^=p[i];h*=16777619u;}return h;}
/* The string table is string_count u32 offsets followed by the strings; where
   the strings end is not stored, so walk the NUL terminators. */
static u32 string_bytes_of(const unsigned char*base,u32 count,u32 limit){
 u32 i,cursor=count*4u;
 for(i=0;i<count;i++){
  u32 o=chart_u32(base+i*4u);
  u32 p=count*4u+o;
  if(p>=limit)return limit;
  while(p<limit&&base[p])p++;
  if(p+1u>cursor)cursor=p+1u;
 }
 return cursor;
}

static int read_exact(int fd,unsigned char *dst,u32 bytes){u32 done=0;while(done<bytes){int n=read(fd,dst+done,(int)(bytes-done));if(n<=0)return -1;done+=(u32)n;}return 0;}

int chart_load(const char *path,chart_info_t *out){
 unsigned char header[CHART_HEADER_BYTES_V4];u32 notes,bpms,sections,events,end,payload_bytes,tail_bytes,checksum,off,i,version,ev_stride,header_bytes,string_count,block_count,cursor,k;int fd,file_bytes;
 static const char magic3[4]={'F','C','H','3'},magic4[4]={'F','C','H','4'};
 memset(&chart_info,0,sizeof(chart_info));
 fd=open(path,O_RDONLY);if(fd<0)return -1;
 file_bytes=(int)lseek(fd,0,SEEK_END);if(file_bytes<(int)CHART_HEADER_BYTES_V3||lseek(fd,0,SEEK_SET)<0){close(fd);return -2;}
 /* The first 28 bytes mean the same thing in both versions, so the magic picks
    the version and the strides and header size follow from it. */
 if(read_exact(fd,header,CHART_HEADER_BYTES_V3)<0){close(fd);return -3;}
 if(!memcmp(header,magic3,4)){version=3u;ev_stride=CHART_EVENT_STRIDE_V3;header_bytes=CHART_HEADER_BYTES_V3;}
 else if(!memcmp(header,magic4,4)){version=4u;ev_stride=CHART_EVENT_STRIDE_V4;header_bytes=CHART_HEADER_BYTES_V4;}
 else{close(fd);return -4;}
 if(header_bytes>CHART_HEADER_BYTES_V3){if(read_exact(fd,header+CHART_HEADER_BYTES_V3,header_bytes-CHART_HEADER_BYTES_V3)<0){close(fd);return -3;}}
 /* v4's flags word says which tables are present (bit 0 strings, bit 1 blocks);
    any other bit set is a version this loader does not know. */
 if(chart_u16(header+4)!=version||chart_u16(header+6)!=12u||chart_u16(header+20)!=16u||chart_u16(header+22)!=8u||chart_u16(header+24)!=ev_stride||chart_u16(header+26)!=0u||(version==4u&&(chart_u16(header+18)&~(u16)3u))||(version==3u&&chart_u16(header+18)!=0u)){close(fd);return -4;}
 if(version==3u&&chart_u32(header+36)!=0u){close(fd);return -4;}
 notes=chart_u32(header+8);bpms=chart_u16(header+12);sections=chart_u16(header+14);events=chart_u16(header+16);checksum=chart_u32(header+28);end=chart_u32(header+32);
 if(!notes||notes>CHART_MAX_NOTES||!bpms||bpms>CHART_MAX_BPMS||!sections||sections>CHART_MAX_SECTIONS||events>CHART_MAX_EVENTS||!end){close(fd);return -5;}
 string_count=0u;block_count=0u;
 if(version==4u){
  string_count=chart_u32(header+36);block_count=chart_u32(header+44);
  if(string_count>CHART_MAX_STRINGS||block_count>CHART_MAX_STRINGS){close(fd);return -5;}
 }
 payload_bytes=notes*12u+bpms*16u+sections*8u+events*ev_stride;
 if(payload_bytes>CHART_MAX_PAYLOAD){close(fd);return -5;}
 if((u32)file_bytes<(u32)(header_bytes+payload_bytes)){close(fd);return -6;}
 tail_bytes=(u32)file_bytes-header_bytes-payload_bytes;
 if(tail_bytes>sizeof(chart_tables)){close(fd);return -5;}
 if(read_exact(fd,chart_payload,payload_bytes)<0){close(fd);return -7;}
 if(tail_bytes&&read_exact(fd,chart_tables,tail_bytes)<0){close(fd);return -7;}
 close(fd);
 /* the checksum covers the records and, for v4, the tables as well */
 {u32 h=chart_fnv1a(chart_payload,payload_bytes);
  if(tail_bytes)h=chart_fnv1a_more(chart_tables,tail_bytes,h);
  if(h!=checksum)return -8;}
 if(version==4u&&tail_bytes){
  /* string table: string_count u32 offsets into the data that follows them */
  const unsigned char*base=chart_tables;u32 data_start=string_count*4u;
  if(string_count*4u>tail_bytes)return -15;
  for(k=0;k<string_count;k++){
   u32 o=chart_u32(base+k*4u);
   if(data_start+o>=tail_bytes)return -15;
   chart_strings[k]=(const char*)(base+data_start+o);
  }
  cursor=string_count*4u;
  /* the strings themselves run from cursor to the start of the block table,
     which is where the converter put them */
  cursor=string_bytes_of(base,string_count,tail_bytes);
  /* block table: a u32 count, then length-prefixed blobs */
  if(cursor+4u>tail_bytes)return -15;
  {u32 bc=chart_u32(base+cursor);cursor+=4u;
   if(bc!=block_count)return -15;
   for(k=0;k<block_count;k++){
    u32 blen;
    if(cursor+4u>tail_bytes)return -15;
    blen=chart_u32(base+cursor);cursor+=4u;
    if(cursor+blen>tail_bytes)return -15;
    chart_block_lengths[k]=blen;chart_block_offsets[k]=cursor;cursor+=blen;
   }}
  if(cursor!=tail_bytes)return -15;
 }
 for(i=0;i<notes;i++){const unsigned char*r=chart_payload+i*12u;u32 sample=chart_u32(r),duration=chart_u32(r+4);if(r[8]>3||r[9]>1||chart_u16(r+10)!=i||sample>end||duration>end-sample||(i&&sample<chart_u32(chart_payload+(i-1u)*12u)))return -9;}
 off=notes*12u;
 for(i=0;i<bpms;i++){const unsigned char*r=chart_payload+off+i*16u;chart_bpm_t*b=&chart_bpms[i];b->start_step=chart_u32(r);b->start_sample=chart_u32(r+4);b->samples_per_step=chart_u32(r+8);b->bpm=chart_u32(r+12);if(!b->samples_per_step||!b->bpm||b->start_sample>end||(i&&(b->start_step<=chart_bpms[i-1].start_step||b->start_sample<=chart_bpms[i-1].start_sample)))return -10;}
 if(chart_bpms[0].start_step||chart_bpms[0].start_sample)return -11;
 off+=bpms*16u;
 for(i=0;i<sections;i++){const unsigned char*r=chart_payload+off+i*8u;u32 start=chart_u32(r);if(r[4]>1||!r[5]||chart_u16(r+6)!=i||(i&&start<=chart_u32(chart_payload+off+(i-1u)*8u)))return -12;}
 if(chart_u32(chart_payload+off)!=0)return -13;
 off+=sections*8u;
 for(i=0;i<events;i++){
  const unsigned char*r=chart_payload+off+i*ev_stride;chart_event_t*e=&chart_events[i];
  memset(e,0,sizeof(*e));
  e->sample=chart_u32(r);e->type=chart_u32(r+4);
  if(version==3u){e->value=chart_u32(r+8);e->order=chart_u32(r+12);}
  else{e->p0=chart_u32(r+8);e->p1=chart_u32(r+12);e->p2=chart_u32(r+16);e->a=chart_u32(r+20);e->b=chart_u32(r+24);e->ref=chart_u32(r+28);}
  if(!e->type||e->sample>end||(i&&e->sample<chart_events[i-1].sample))return -14;
  if(version==3u&&e->order!=i)return -14;
  if(version==4u&&(e->b&CHART_B_REF_IS_STRING)&&e->ref>=string_count)return -15;
  if(version==4u&&(e->b&CHART_B_REF_IS_BLOCK)&&e->ref>=block_count)return -15;
 }
 chart_info.note_count=notes;chart_info.bpm_count=bpms;chart_info.section_count=sections;chart_info.event_count=events;chart_info.song_end_sample=end;chart_info.checksum=checksum;chart_info.initial_bpm=chart_bpms[0].bpm;
 chart_info.version=version;chart_info.string_count=string_count;chart_info.block_count=block_count;
 if(out)*out=chart_info;
 return 0;
}

/* v4's string and block tables. Both accessors return 0 for a v3 file, which
   has no tables, and for an out-of-range index. */
const char *chart_event_string(u32 index){
 if(chart_info.version!=4u||index>=chart_info.string_count)return 0;
 return chart_strings[index];
}

const unsigned char *chart_event_block(u32 index,u32 *length){
 if(chart_info.version!=4u||index>=chart_info.block_count)return 0;
 if(length)*length=chart_block_lengths[index];
 return chart_tables+chart_block_offsets[index];
}

int chart_note(u32 index,chart_note_t*out){
 const unsigned char*r;
 if(!out||index>=chart_info.note_count)return -1;
 r=chart_payload+index*12u;
 out->sample=chart_u32(r);
 out->duration=chart_u32(r+4);
 out->lane=r[8];
 out->must_press=r[9];
 out->order=chart_u16(r+10);
 return 0;
}

const chart_event_t *chart_event(u32 index){return index<chart_info.event_count?&chart_events[index]:0;}
u32 chart_samples_per_step(u32 sample){u32 i=0;if(!chart_info.bpm_count)return 0;while(i+1u<chart_info.bpm_count&&sample>=chart_bpms[i+1u].start_sample)i++;return chart_bpms[i].samples_per_step;}
u32 chart_step_at(u32 sample){u32 i=0;while(i+1u<chart_info.bpm_count&&sample>=chart_bpms[i+1u].start_sample)i++;return chart_info.bpm_count?chart_bpms[i].start_step+(sample-chart_bpms[i].start_sample)/chart_bpms[i].samples_per_step:0;}

int chart_load_song(const char *root, const sr_song_t *song,
                    const char *difficulty, chart_info_t *info)
{
    char path[160];
    int n;
    if (!root || !song || !difficulty || !info) return -1;
    n = sr_chart_path(path, sizeof(path), root, song, difficulty);
    if (n < 0 || (unsigned int)n >= sizeof(path)) return -2;
    return chart_load(path, info);
}

int chart_load_song_layout(const char *root, const sr_song_t *song,
                           const char *difficulty, chart_info_t *info,
                           int *layout_out)
{
    char path[160];
    int layouts, li;
    if (!root || !song || !difficulty || !info) return -1;
    layouts = sr_chart_layout_count();
    /* O layout 0 e o do jogo original e e o que o pendrive usa. Se ele abrir,
       nada muda. Se nao abrir, os planos sao tentados na ordem: um pendrive
       cujo usbhdfsd nao consegue abrir a arvore aninhada as vezes abre o
       arquivo plano, e a diferenca entre o tutorial tocar e nao tocar eh
       exatamente essa. O layout que funcionou sai em *layout_out para o
       telemetria poder dizer qual foi. */
    int first_rc = 0;
    for (li = 0; li < layouts; li++) {
        int n = sr_chart_path_at(path, sizeof(path), root, song, difficulty,
                                 (unsigned int)li);
        int rc;
        if (n < 0 || (unsigned int)n >= sizeof(path)) return -2;
        rc = chart_load(path, info);
        if (rc == 0) {
            if (layout_out) *layout_out = li;
            return 0;
        }
        /* Guarda o codigo do layout 0, que e o do jogo original: se nenhum
           abrir, e esse o motivo que o diagnostico tem que mostrar. */
        if (li == 0) first_rc = rc;
    }
    if (layout_out) *layout_out = -1;
    return first_rc;
}
