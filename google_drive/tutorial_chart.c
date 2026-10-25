#include "tutorial_chart.h"
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#define CHART_HEADER_BYTES 40u
/* Limites elevados em 0.2.76. O Butterscotch compilou os 28 charts
   oficiais e mostrou que so 3 cabiam em 256 notas: bopeebo 102,
   fresh 210 e tutorial 55. Os outros vao de 263 a 906 notas, com
   spaghetti chegando a 264 eventos. O limite era arbitrario e o
   custo de elevar e irrisorio: o payload maximo passa de 11.520 B
   para 24.832 B de RAM estatica, e o maior chart real, 2hot com 906
   notas, ocupa 12.632 B. */
#define CHART_MAX_NOTES 1024u
#define CHART_MAX_BPMS 16u
#define CHART_MAX_SECTIONS 512u
#define CHART_MAX_EVENTS 512u
#define CHART_MAX_PAYLOAD (CHART_MAX_NOTES*12u+CHART_MAX_BPMS*16u+CHART_MAX_SECTIONS*8u+CHART_MAX_EVENTS*16u)

typedef struct { u32 start_step,start_sample,samples_per_step,bpm; } chart_bpm_t;
static unsigned char chart_payload[CHART_MAX_PAYLOAD] __attribute__((aligned(16)));
static chart_bpm_t chart_bpms[CHART_MAX_BPMS];
static tutorial_chart_event_t chart_events[CHART_MAX_EVENTS];
static tutorial_chart_info_t chart_info;

static u16 chart_u16(const unsigned char *p){return (u16)((u16)p[0]|((u16)p[1]<<8));}
static u32 chart_u32(const unsigned char *p){return (u32)p[0]|((u32)p[1]<<8)|((u32)p[2]<<16)|((u32)p[3]<<24);}
static u32 chart_fnv1a(const unsigned char *p,u32 n){u32 h=2166136261u,i;for(i=0;i<n;i++){h^=p[i];h*=16777619u;}return h;}
static int read_exact(int fd,unsigned char *dst,u32 bytes){u32 done=0;while(done<bytes){int n=read(fd,dst+done,(int)(bytes-done));if(n<=0)return -1;done+=(u32)n;}return 0;}

int tutorial_chart_load(const char *path,tutorial_chart_info_t *out){
 unsigned char header[CHART_HEADER_BYTES];u32 notes,bpms,sections,events,end,payload_bytes,expected,checksum,off,i;int fd,file_bytes;
 memset(&chart_info,0,sizeof(chart_info));
 fd=open(path,O_RDONLY);if(fd<0)return -1;
 file_bytes=(int)lseek(fd,0,SEEK_END);if(file_bytes<(int)CHART_HEADER_BYTES||lseek(fd,0,SEEK_SET)<0){close(fd);return -2;}
 if(read_exact(fd,header,CHART_HEADER_BYTES)<0){close(fd);return -3;}
 if(memcmp(header,"FCH3",4)||chart_u16(header+4)!=3||chart_u16(header+6)!=12||chart_u16(header+20)!=16||chart_u16(header+22)!=8||chart_u16(header+24)!=16||chart_u16(header+18)!=0||chart_u16(header+26)!=0||chart_u32(header+36)!=0){close(fd);return -4;}
 notes=chart_u32(header+8);bpms=chart_u16(header+12);sections=chart_u16(header+14);events=chart_u16(header+16);checksum=chart_u32(header+28);end=chart_u32(header+32);
 if(!notes||notes>CHART_MAX_NOTES||!bpms||bpms>CHART_MAX_BPMS||!sections||sections>CHART_MAX_SECTIONS||events>CHART_MAX_EVENTS||!end){close(fd);return -5;}
 payload_bytes=notes*12u+bpms*16u+sections*8u+events*16u;expected=CHART_HEADER_BYTES+payload_bytes;
 if(expected>(u32)file_bytes||expected-CHART_HEADER_BYTES>CHART_MAX_PAYLOAD||expected!=(u32)file_bytes){close(fd);return -6;}
 if(read_exact(fd,chart_payload,payload_bytes)<0){close(fd);return -7;}close(fd);
 if(chart_fnv1a(chart_payload,payload_bytes)!=checksum)return -8;
 for(i=0;i<notes;i++){const unsigned char*r=chart_payload+i*12u;u32 sample=chart_u32(r),duration=chart_u32(r+4);if(r[8]>3||r[9]>1||chart_u16(r+10)!=i||sample>end||duration>end-sample||(i&&sample<chart_u32(chart_payload+(i-1u)*12u)))return -9;}
 off=notes*12u;
 for(i=0;i<bpms;i++){const unsigned char*r=chart_payload+off+i*16u;chart_bpm_t*b=&chart_bpms[i];b->start_step=chart_u32(r);b->start_sample=chart_u32(r+4);b->samples_per_step=chart_u32(r+8);b->bpm=chart_u32(r+12);if(!b->samples_per_step||!b->bpm||b->start_sample>end||(i&&(b->start_step<=chart_bpms[i-1].start_step||b->start_sample<=chart_bpms[i-1].start_sample)))return -10;}
 if(chart_bpms[0].start_step||chart_bpms[0].start_sample)return -11;
 off+=bpms*16u;
 for(i=0;i<sections;i++){const unsigned char*r=chart_payload+off+i*8u;u32 start=chart_u32(r);if(r[4]>1||!r[5]||chart_u16(r+6)!=i||(i&&start<=chart_u32(chart_payload+off+(i-1u)*8u)))return -12;}
 if(chart_u32(chart_payload+off)!=0)return -13;
 off+=sections*8u;
 for(i=0;i<events;i++){const unsigned char*r=chart_payload+off+i*16u;tutorial_chart_event_t*e=&chart_events[i];e->sample=chart_u32(r);e->type=chart_u32(r+4);e->value=chart_u32(r+8);e->order=chart_u32(r+12);if(!e->type||e->order!=i||e->sample>end||(i&&e->sample<chart_events[i-1].sample))return -14;}
 chart_info.note_count=notes;chart_info.bpm_count=bpms;chart_info.section_count=sections;chart_info.event_count=events;chart_info.song_end_sample=end;chart_info.checksum=checksum;chart_info.initial_bpm=chart_bpms[0].bpm;
 if(out)*out=chart_info;
 return 0;
}

int tutorial_chart_note(u32 index,tutorial_chart_note_t*out){
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

const tutorial_chart_event_t *tutorial_chart_event(u32 index){return index<chart_info.event_count?&chart_events[index]:0;}
u32 tutorial_chart_step_at(u32 sample){u32 i=0;while(i+1u<chart_info.bpm_count&&sample>=chart_bpms[i+1u].start_sample)i++;return chart_info.bpm_count?chart_bpms[i].start_step+(sample-chart_bpms[i].start_sample)/chart_bpms[i].samples_per_step:0;}

