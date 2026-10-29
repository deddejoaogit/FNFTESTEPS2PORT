#include "audio_ring.h"
#include <kernel.h>
#include <delaythread.h>
#include <unistd.h>
#include <string.h>
#include <tamtypes.h>

#define RING_SIZE (256*1024)
#define READ_SIZE (32*1024)
static unsigned char ring[RING_SIZE] __attribute__((aligned(128)));
static unsigned char io_block[READ_SIZE] __attribute__((aligned(128)));
static unsigned char thread_stack[16384] __attribute__((aligned(128)));
static volatile unsigned int rp,wp,level,running,reset_requested,switch_requested,source_eof,loop_source;
static volatile int pending_fd=-1;
static volatile int producer_exited=1;
/* Serializacao do acesso ao USB. O usbhdfsd nao e reentrante, e a
   captura de tela grava 1,1 MB no pendrive pela thread principal
   enquanto esta thread le a musica do mesmo dispositivo. As duas ao
   mesmo tempo corrompem o estado do driver: depois de algumas
   capturas a musica e as animacoes paravam de vez, porque tanto o
   anel de audio quanto a thread de animacao leem do USB. */
static volatile int pause_req=0,in_read=0;
void audio_ring_pause(int p){int n=0;pause_req=p?1:0;
 if(p){while(in_read&&n++<2000)DelayThread(1000);}}
static volatile unsigned int produced,reads_count,loops_count,errors_count,underruns_count,switches_count;
static int source_fd=-1,mutex=-1,thread_id=-1;
extern void *_gp;
static void lock_ring(void){WaitSema(mutex);} static void unlock_ring(void){SignalSema(mutex);}
static void producer(void *arg){(void)arg;while(running){int free_bytes,got,first;if(pause_req){DelayThread(2000);continue;}if(switch_requested){source_fd=pending_fd;lseek(source_fd,0,SEEK_SET);lock_ring();rp=wp=level=0;source_eof=0;unlock_ring();switches_count++;switch_requested=0;continue;}if(reset_requested){lseek(source_fd,0,SEEK_SET);lock_ring();rp=wp=level=0;source_eof=0;unlock_ring();reset_requested=0;continue;}lock_ring();free_bytes=RING_SIZE-(int)level;unlock_ring();if(free_bytes<READ_SIZE){DelayThread(2000);continue;}if(source_eof){if(loop_source){lseek(source_fd,0,SEEK_SET);source_eof=0;loops_count++;continue;}DelayThread(2000);continue;}in_read=1;got=read(source_fd,io_block,READ_SIZE);in_read=0;if(got==0){source_eof=1;continue;}if(got<0){errors_count++;DelayThread(10000);continue;}lock_ring();first=got;if(first>RING_SIZE-(int)wp)first=RING_SIZE-(int)wp;memcpy(ring+wp,io_block,first);if(first<got)memcpy(ring,io_block+first,got-first);wp=(wp+got)%RING_SIZE;level+=got;produced+=got;reads_count++;unlock_ring();}producer_exited=1;ExitThread();}
int audio_ring_start_loop(int fd,int loop){ee_sema_t s;ee_thread_t t;int start_rc;if(thread_id>=0)audio_ring_stop();source_fd=fd;pending_fd=-1;switch_requested=reset_requested=0;loop_source=loop?1u:0u;rp=wp=level=source_eof=0;produced=reads_count=loops_count=errors_count=underruns_count=0;memset(&s,0,sizeof(s));s.init_count=1;s.max_count=1;s.option=0;mutex=CreateSema(&s);if(mutex<0){source_fd=-1;return -1;}memset(&t,0,sizeof(t));t.func=producer;t.stack=thread_stack;t.stack_size=sizeof(thread_stack);t.gp_reg=&_gp;t.initial_priority=40;thread_id=CreateThread(&t);if(thread_id<0){DeleteSema(mutex);mutex=-1;source_fd=-1;return -2;}running=1;producer_exited=0;start_rc=StartThread(thread_id,0);if(start_rc<0){running=0;producer_exited=1;DeleteThread(thread_id);thread_id=-1;DeleteSema(mutex);mutex=-1;source_fd=-1;return -3;}return 0;}
int audio_ring_start(int fd){return audio_ring_start_loop(fd,0);}
int audio_ring_take(void *dst,int bytes){int first;if(bytes<=0)return 0;lock_ring();if((int)level<bytes){underruns_count++;unlock_ring();return 0;}first=bytes;if(first>RING_SIZE-(int)rp)first=RING_SIZE-(int)rp;memcpy(dst,ring+rp,first);if(first<bytes)memcpy((unsigned char*)dst+first,ring,bytes-first);rp=(rp+bytes)%RING_SIZE;level-=bytes;unlock_ring();return bytes;}
int audio_ring_take_available(void *dst,int bytes){int first,n;if(bytes<=0)return 0;lock_ring();n=(int)level;if(n>bytes)n=bytes;if(n<=0){unlock_ring();return 0;}first=n;if(first>RING_SIZE-(int)rp)first=RING_SIZE-(int)rp;memcpy(dst,ring+rp,first);if(first<n)memcpy((unsigned char*)dst+first,ring,n-first);rp=(rp+n)%RING_SIZE;level-=n;unlock_ring();return n;}
int audio_ring_eof(void){return source_eof?1:0;}
int audio_ring_level(void){int n;if(mutex<0)return (int)level;lock_ring();n=(int)level;unlock_ring();return n;}
/* Lock-free approximate read for background workers that can overlap a ring
 * stop/restart. The audio reserve check only needs a conservative snapshot. */
int audio_ring_level_snapshot(void){return (int)level;}
void audio_ring_stats(unsigned int*p,unsigned int*r,unsigned int*l,unsigned int*e,unsigned int*u){if(p)*p=produced;if(r)*r=reads_count;if(l)*l=loops_count;if(e)*e=errors_count;if(u)*u=underruns_count;}
int audio_ring_reset(void){int n=0;reset_requested=1;while(reset_requested&&n++<500)DelayThread(1000);return reset_requested?-1:0;}
int audio_ring_switch(int fd,int loop){int n=0;pending_fd=fd;loop_source=loop?1u:0u;switch_requested=1;while(switch_requested&&n++<1000)DelayThread(1000);return switch_requested?-1:0;}
void audio_ring_debug(int*fd,unsigned int*loop,unsigned int*sw,unsigned int*rpending,unsigned int*spending){if(fd)*fd=source_fd;if(loop)*loop=loop_source;if(sw)*sw=switches_count;if(rpending)*rpending=reset_requested;if(spending)*spending=switch_requested;}
/* PARADA COOPERATIVA.
   O produtor testa running no topo do laco, mas pode estar DENTRO de um
   read() de 32 KB vindo do USB, que leva dezenas de milissegundos. Matar
   a thread ali com TerminateThread deixa a leitura pendente no IOP, e o
   close(audio_fd) que o chamador faz logo em seguida colide com ela:
   travamento duro, sem retorno.
   Ate a 0.2.63 isso ficava escondido por acidente, porque havia ~38 ms
   de capture_status entre audio_ring_stop() e o close(). Ao remover
   aquelas escritas na 0.2.64 a corrida apareceu. Agora a espera e
   explicita: ate 600 ms pelo encerramento normal, e TerminateThread so
   como ultimo recurso. */
void audio_ring_stop(void){int n=0;running=0;if(thread_id>=0){
  while(!producer_exited&&n++<600)DelayThread(1000);
  if(!producer_exited)TerminateThread(thread_id);
  DeleteThread(thread_id);thread_id=-1;producer_exited=1;}if(mutex>=0){DeleteSema(mutex);mutex=-1;}source_fd=-1;pending_fd=-1;switch_requested=reset_requested=0;rp=wp=level=source_eof=0;}
