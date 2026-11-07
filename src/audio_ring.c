#include "audio_ring.h"
#include "engine/audio_mix.h"
#include "engine/freeplay_audio.h"
#include <kernel.h>
#include <delaythread.h>
#include <unistd.h>
#include <string.h>
#include <tamtypes.h>

#define RING_SIZE (256*1024)
/* 0.2.168: 8 KB (era 32 KB). Medido nos Q-lines: cada leitura de 32 KB
   bloqueia o IOP ~46 ms (0,7 MB/s do pendrive) e os RPCs do quadro
   (padGetState/audsrv) esperam por tras -- os picos de 53 ms a cada ~10
   quadros. Com 8 KB o pico cai para ~12 ms, absorvivel pela espera de vsync. */
#define READ_SIZE (8*1024)
static unsigned char ring[RING_SIZE] __attribute__((aligned(128)));
static unsigned char io_block[READ_SIZE] __attribute__((aligned(128)));
static unsigned char thread_stack[16384] __attribute__((aligned(128)));
static volatile unsigned int rp,wp,level,running,reset_requested,switch_requested,source_eof,loop_source;
static volatile int pending_fd=-1;
static volatile int producer_exited=1;
/* FONTES SECUNDARIAS (overlay). Ver audio_ring.h: sao os sfx que tocam por
   cima da musica -- o `fnf_loss_sfx` do game over e os dois jingles do rank da
   Freeplay. `overlay_pedido[i]` e' o canal de pedido para a thread:
   -2 = nada pendente, -1 = parar, >= 0 = trocar para este fd (do comeco).
   Sao QUATRO slots porque o original pode ter dois `playOnce` ao mesmo tempo:
   o jingle do `rankDisplayNew` (600 ms, 0,585 a 1,004 s de duracao) ainda esta'
   tocando quando o do slam entra em 1100 ms -- medido nos proprios arquivos
   (`rankinperfect.ogg` 1,004 s; `rankinbad.ogg` 0,862 s; `rankinnormal.ogg`
   0,585 s). Com um slot so' o segundo cortaria o primeiro. */
#define RING_OVERLAYS 8
/* Os slots 0..2 sao FIXOS dos tres `playOnce` de menu da Freeplay
   (`audio_ring_overlay_slot`: scroll, confirm, cancel); o alocador automatico
   comeca no ULTIMO e desce ate' `RING_OVERLAY_AUTO_MIN`. Sem essa separacao,
   um jingle pegaria o slot de um deles: o `audio_ring_overlay_slot` seguinte
   fecharia o fd do jingle (o slot trocou de dono) e cortaria o som, ou receberia
   um fd que o outro ja' usa. Sao 8 e nao 4 porque o pior caso e' 3 sfx de menu
   + 5 fontes automaticas (no original os `FlxSound` nao tem limite). */
#define RING_OVERLAY_AUTO_MIN 3
#define RING_SEM_PEDIDO (-2)
/* O ganho do preview e' aplicado em TREchos deste tamanho dentro de cada bloco
   de 32 KB. O `fp_aud_ganho` e' a MENOR de duas rampas lineares: no ponto onde
   as duas se encontram (o fim do fadeIn = o comeco do fadeOut, 2 s) ele tem um
   BICO. Interpolar de uma ponta a outra de um bloco de 4096 amostras corta o
   bico: medido no teste do PC, o pico do fade saia 0,686 em vez de 0,700 --
   2 % de erro, e o mesmo erro aparecia na volta da janela. Com 256 amostras o
   pior erro e' 0,7 x 128/96000 = 0,09 %, e o custo e' 16 chamadas por bloco de
   85 ms. */
#define PREVIEW_TRECHO 256
static volatile int overlay_fd[RING_OVERLAYS];
static volatile int overlay_eof[RING_OVERLAYS];
static volatile int overlay_pedido[RING_OVERLAYS];
/* Volume do SLOT em por cento (`amix_volume`): o `FlxSound.volume` do original.
   Os jingles do rank e o `fnf_loss_sfx` sao 1,0; so' o `scrollMenu` da Freeplay
   e' 0,4 (`FreeplayState.hx` L2918 e L2388). Aplicado na amostra lida, antes de
   somar, porque cada fonte tem o seu. */
static volatile int overlay_vol[RING_OVERLAYS];
static volatile int overlay_pedido_vol[RING_OVERLAYS];
/* 1 = o slot TOCA EM LACO: no fim do arquivo ele volta ao comeco em vez de
   parar. E' o `FlxSound.play(true)` do original, e existe por UM caso medido:
   o `train_passes` do `phillyTrain` (L129), que toca 14,42 s em laco por baixo
   da musica inteira. Sem isto o trem some em 14 s e o palco fica mudo no resto
   da musica -- uma diferenca que so' o ouvido pega. */
static volatile int overlay_loop[RING_OVERLAYS];
static volatile int overlay_pedido_loop[RING_OVERLAYS];
/* Sobe a cada stop/inicio: os fds guardados pelo chamador para reusar deixam de
   valer quando o anel fecha tudo (ver audio_ring_geracao). */
static volatile unsigned int ring_geracao=0;
static unsigned char overlay_block[READ_SIZE] __attribute__((aligned(128)));
/* 1 = nao ha' fonte primaria: o produtor escreve SILENCIO e soma os overlays.
   E' o comeco do game over, antes de o original comecar a musica, e a
   cinematica de rank da Freeplay, que roda com `FlxG.sound.music.volume = 0`
   (`FreeplayState.hx` L1088). */
static volatile int sem_fonte=0;
static volatile int fonte_pedido=RING_SEM_PEDIDO,fonte_pedido_loop=0;
/* PREVIEW DA FREEPLAY: a primaria e' um TRECHO de um arquivo de 4 canais.
   `prev_ini`/`prev_fim` sao o intervalo em bytes do `loadPartial` do original;
   ao chegar em `prev_fim` o produtor volta para `prev_ini` em vez de voltar
   para o byte 0 do arquivo. Os 4 canais viram 2 (instrumental so') e o ganho
   do fade e' aplicado AQUI, antes de somar os overlays -- ver
   `amix_somar_ganho`. `prev_pos` conta amostras desde `prev_ini`, e e' o que
   alimenta `fp_aud_ganho`. */
static volatile int fonte_pedido_ini=0,fonte_pedido_fim=0;
static volatile int prev_ini=0,prev_fim=0,prev_ativo=0,prev_total=0;
static int prev_pos=0;
/* Serializacao do acesso ao USB. O usbhdfsd nao e reentrante, e a
   captura de tela grava 1,1 MB no pendrive pela thread principal
   enquanto esta thread le a musica do mesmo dispositivo. As duas ao
   mesmo tempo corrompem o estado do driver: depois de algumas
   capturas a musica e as animacoes paravam de vez, porque tanto o
   anel de audio quanto a thread de animacao leem do USB. */
static volatile int pause_req=0,in_read=0;
/* ---- 0.2.168: PORTAO DE I/O UNICO --------------------------------------
   O usbhdfsd nao e' reentrante (ver audio_ring.h): duas operacoes
   simultaneas -- o produtor a ler a musica e o laco/cao de guarda a escrever
   logs -- corrompem/travam o driver. E' a causa do travamento de vez
   (sessao 0.2.167: morte logo apos o duplo despejo do quadro 600).
   TODA a E/S de mass: passa por aqui: produtor, marcas, despejos, vigia,
   capturas. Criado cedo (io_gate_init) porque o boot ja' escreve. */
static int io_sema=-1;
static int io_owner=-1;      /* recursivo POR THREAD: o vigia segura o portao
                                enquanto chama caminhos que tambem o tomam */
static int io_depth=0;
void io_gate_init(void){if(io_sema<0){ee_sema_t sp;sp.init_count=1;sp.max_count=1;sp.option=0;io_sema=CreateSema(&sp);}}
void io_gate_lock(void){int me=GetThreadId();if(io_sema<0)io_gate_init();
  if(io_owner==me){io_depth++;return;}WaitSema(io_sema);io_owner=me;io_depth=1;}
void io_gate_unlock(void){int me=GetThreadId();
  if(io_owner!=me)return;          /* nunca desbloquear o portao dos outros */
  if(--io_depth>0)return;io_owner=-1;SignalSema(io_sema);}
int io_gate_trylock(void){int me=GetThreadId();if(io_sema<0){io_gate_init();return 1;}
  if(io_owner==me){io_depth++;return 1;}
  if(PollSema(io_sema)!=0)return 0;io_owner=me;io_depth=1;return 1;}

/* ---- 0.2.168: CANARIO DA PILHA DO PRODUTOR -----------------------------
   Estouro de pilha e' candidato a travamento silencioso: pinta-se o fundo da
   pilha da thread antes de a arrancar e verifica-se o padrao depois. Se o
   canario for trilhado, o Q-line mostra ck=0 e o vigia grava-o. */
static unsigned char g_canario_bytes[128];
static int g_canario_vivo=1;
static void canario_pintar(void){
  unsigned int i;
  for(i=0;i<sizeof(g_canario_bytes);i++)g_canario_bytes[i]=(unsigned char)(0xA5u^(i&255u));
  memcpy(thread_stack,g_canario_bytes,sizeof(g_canario_bytes));
}
int audio_ring_stack_ok(void){
  return g_canario_vivo&&memcmp(thread_stack,g_canario_bytes,sizeof(g_canario_bytes))==0;
}
void audio_ring_pause(int p){int n=0;pause_req=p?1:0;
 if(p){while(in_read&&n++<2000)DelayThread(1000);}}
static volatile unsigned int produced,reads_count,loops_count,errors_count,underruns_count,switches_count;
static int source_fd=-1,mutex=-1,thread_id=-1;
extern void *_gp;
static void lock_ring(void){WaitSema(mutex);} static void unlock_ring(void){SignalSema(mutex);}
/* Aplica os pedidos de fonte (primaria e overlays) no topo do bloco. Os
   pedidos sao consumidos aqui, e nao no chamador, porque e' esta thread que
   mexe no fd: o `close()` do lado do main.c so' acontece depois de
   `audio_ring_stop()`/`audio_ring_preview_parar()` esperarem o consumo. */
static void aplica_pedidos(void){
 int i;
 if(fonte_pedido!=RING_SEM_PEDIDO){
  int f=fonte_pedido;
  /* Uma troca de preview TOMA POSSE do fd novo e fecha o anterior, mas so'
     quando o anterior era um preview também: o `fd` do canal de musica normal
     pertence ao main.c e e' ele que fecha (ver audio_ring_stop). */
  if(prev_ativo&&f>=0&&source_fd>=0&&source_fd!=f)close(source_fd);
  loop_source=fonte_pedido_loop?1u:0u;
  prev_ini=fonte_pedido_ini;prev_fim=fonte_pedido_fim;
  prev_ativo=(f>=0&&prev_fim>prev_ini)?1:0;
  prev_total=prev_ativo?fp_aud_janela_amostras(prev_ini,prev_fim):0;
  prev_pos=0;
  source_fd=f;
  sem_fonte=(f<0)?1:0;
  source_eof=0;
  if(f>=0)lseek(f,prev_ativo?prev_ini:0,SEEK_SET);
  fonte_pedido=RING_SEM_PEDIDO;
 }
 for(i=0;i<RING_OVERLAYS;i++){
  if(overlay_pedido[i]!=RING_SEM_PEDIDO){
   int o=overlay_pedido[i];
   /* O slot trocou de DONO: o fd anterior era da thread e morre aqui. E' o
      mesmo contrato da primaria (o chamador nunca fecha o que entregou). */
   if(overlay_fd[i]>=0&&overlay_fd[i]!=o)close(overlay_fd[i]);
   overlay_fd[i]=o;
   overlay_vol[i]=overlay_pedido_vol[i];
   overlay_loop[i]=overlay_pedido_loop[i];
   overlay_pedido_loop[i]=0;
   if(o>=0){lseek(o,0,SEEK_SET);overlay_eof[i]=0;}else overlay_eof[i]=1;
   overlay_pedido[i]=RING_SEM_PEDIDO;
  }
 }
}
/* Soma os overlays (se houver) sobre o bloco recem-lido. A leitura e' feita
   aqui, nesta thread, e SEMPRE depois da primaria: e' o que mantem o pendrive
   com um acesso por vez. `og` pode ser menor que `got` no fim do overlay -- o
   resto do bloco fica so' com o que ja' estava, que e' o certo. */
static void soma_overlay(int got){
 int i,off,og;
 if(got<=0)return;
 for(i=0;i<RING_OVERLAYS;i++){
  if(overlay_fd[i]<0||overlay_eof[i])continue;
  /* Le `got` bytes, emendando o FIM DO ARQUIVO no comeco quando o slot esta'
     em laco. Antes o bloco parcial marcava fim de som: num arquivo cujo
     tamanho nao e' multiplo de READ_SIZE (o `train_passes` tem 2.768.640 B =
     84 blocos e meio) isso pararia o som no primeiro bloco incompleto. */
  off=0;
  while(off<got){
   io_gate_lock();in_read=1;og=read(overlay_fd[i],overlay_block+off,got-off);in_read=0;io_gate_unlock();
   if(og>0){off+=og;continue;}
   if(!overlay_loop[i]){overlay_eof[i]=1;break;}
   /* Arquivo vazio (ou ilegivel): nao ha' o que repetir. */
   if(off==0){overlay_eof[i]=1;break;}
   lseek(overlay_fd[i],0,SEEK_SET);
   loops_count++;
  }
  if(off<=0)continue;
  /* O volume do `FlxSound` do original, uma vez por bloco e antes de somar:
     cada fonte secundaria tem o seu (0,4 no `scrollMenu` da Freeplay). */
  if(overlay_vol[i]!=100)amix_volume((short*)overlay_block,off/4,overlay_vol[i]);
  /* 4 bytes por amostra estereo (L,R de 16 bits): o PCM do pendrive. */
  amix_somar_estereo((short*)io_block,(const short*)overlay_block,off/4);
 }
}
/* ver canario_pintar() acima */
static void producer(void *arg){(void)arg;while(running){int free_bytes,got,first,want;if(pause_req){DelayThread(2000);continue;}aplica_pedidos();if(switch_requested){source_fd=pending_fd;lseek(source_fd,0,SEEK_SET);sem_fonte=0;prev_ativo=0;prev_total=prev_pos=0;lock_ring();rp=wp=level=0;source_eof=0;unlock_ring();switches_count++;switch_requested=0;continue;}if(reset_requested){if(!sem_fonte)lseek(source_fd,prev_ativo?prev_ini:0,SEEK_SET);prev_pos=0;lock_ring();rp=wp=level=0;source_eof=0;unlock_ring();reset_requested=0;continue;}lock_ring();free_bytes=RING_SIZE-(int)level;unlock_ring();if(free_bytes<READ_SIZE){DelayThread(2000);continue;}
 if(sem_fonte){
  /* SEM FONTE: so' silencio + overlays. Se todos os overlays ja' acabaram (ou
     nao existem) nao ha' nada para produzir -- dormir aqui e' o que evita
     queimar CPU nos segundos em que a tela de game over espera a animacao de
     morte terminar. Os pedidos sao aplicados no topo do laco, entao um pedido
     feito por quem esta' esperando acorda a thread em ate' 5 ms. */
  int algum=0,i;
  for(i=0;i<RING_OVERLAYS;i++)if(overlay_fd[i]>=0&&!overlay_eof[i]){algum=1;break;}
  if(!algum){DelayThread(5000);continue;}
  memset(io_block,0,READ_SIZE);got=READ_SIZE;
 }else{
  if(!prev_ativo&&source_eof){if(loop_source){lseek(source_fd,0,SEEK_SET);source_eof=0;loops_count++;continue;}DelayThread(2000);continue;}
  want=READ_SIZE;
  if(prev_ativo){
   /* `prev_pos` conta AMOSTRAS e `prev_ini`/`prev_fim` sao BYTES: a conta da
      sobra e' em bytes, e o marcador de posicao volta a ZERO quando o arquivo
      volta para o comeco da janela. Misturar as duas unidades foi medido no
      teste do PC: a janela ficava 8x mais longa (8 bytes por amostra) e o laco
      so' acontecia no fim do ARQUIVO, nao no fim do TRECHO. */
   long resto=(long)prev_fim-(long)prev_pos*FP_AUD_STEMS_BYTES;
   if(resto<=0){
    /* Fim do trecho: volta para o comeco da JANELA. E' o `FlxTimer` de
       `length/1000` que chama `playCurSongPreview()` de novo (L3033-3038). */
    prev_pos=0;lseek(source_fd,prev_ini,SEEK_SET);loops_count++;
    resto=(long)prev_fim-(long)prev_ini;
   }
   if((long)want>resto)want=(int)resto;
  }
  io_gate_lock();in_read=1;got=read(source_fd,io_block,want);in_read=0;io_gate_unlock();
  if(got==0){
   if(prev_ativo){prev_pos=0;lseek(source_fd,prev_ini,SEEK_SET);loops_count++;continue;}
   source_eof=1;continue;
  }
  if(got<0){errors_count++;DelayThread(10000);continue;}
  if(prev_ativo){
   int quadros=got/FP_AUD_STEMS_BYTES;
   if(quadros<=0){prev_pos=0;lseek(source_fd,prev_ini,SEEK_SET);continue;}
   /* 4 canais -> 2, com o ganho do preview, ANTES de somar os overlays: os
      jingles sao `playOnce` a volume 1 e nao podem ser atenuados junto. */
   {int off=0;
    while(off<quadros){
     int n=quadros-off;
     if(n>PREVIEW_TRECHO)n=PREVIEW_TRECHO;
     amix_somar_ganho((const short*)io_block+off*4,(short*)io_block+off*2,n,
                      fp_aud_ganho(prev_pos+off,prev_total,FP_AUD_VOLUME),
                      fp_aud_ganho(prev_pos+off+n,prev_total,FP_AUD_VOLUME));
     off+=n;
    }}
   prev_pos+=quadros;
   got=quadros*4;
  }
 }
 soma_overlay(got);
 lock_ring();first=got;if(first>RING_SIZE-(int)wp)first=RING_SIZE-(int)wp;memcpy(ring+wp,io_block,first);if(first<got)memcpy(ring,io_block+first,got-first);wp=(wp+got)%RING_SIZE;level+=got;produced+=got;reads_count++;unlock_ring();}producer_exited=1;ExitThread();}
/* Inicializa o estado das fontes. `fd` < 0 = sem primaria (silencio +
   overlays), que e' o modo do comeco do game over e o da cinematica de rank.
   `prev` liga o modo PREVIEW (janela `[ini,fim)` em bytes, 4 canais -> 2). */
static void fontes_reset(int fd,int loop,int ini,int fim,int prev){
 int i;
 source_fd=fd;
 sem_fonte=(fd<0)?1:0;
 fonte_pedido=RING_SEM_PEDIDO;
 fonte_pedido_loop=0;
 fonte_pedido_ini=fonte_pedido_fim=0;
 prev_ini=ini;prev_fim=fim;
 prev_ativo=prev?1:0;
 prev_total=prev?fp_aud_janela_amostras(ini,fim):0;
 prev_pos=0;
 for(i=0;i<RING_OVERLAYS;i++){overlay_fd[i]=-1;overlay_eof[i]=1;overlay_pedido[i]=RING_SEM_PEDIDO;
  overlay_vol[i]=100;overlay_pedido_vol[i]=100;
  overlay_loop[i]=0;overlay_pedido_loop[i]=0;}
 loop_source=loop?1u:0u;
}
/* O mesmo arranque de thread das tres entradas do modulo. */
static int inicia_thread(void){
 ee_sema_t s;ee_thread_t t;int start_rc;
 memset(&s,0,sizeof(s));s.init_count=1;s.max_count=1;s.option=0;mutex=CreateSema(&s);canario_pintar();
 if(mutex<0){source_fd=-1;return -1;}
 memset(&t,0,sizeof(t));t.func=producer;t.stack=thread_stack;t.stack_size=sizeof(thread_stack);
 t.gp_reg=&_gp;t.initial_priority=40;thread_id=CreateThread(&t);
 if(thread_id<0){DeleteSema(mutex);mutex=-1;source_fd=-1;return -2;}
 running=1;producer_exited=0;start_rc=StartThread(thread_id,0);
 if(start_rc<0){running=0;producer_exited=1;DeleteThread(thread_id);thread_id=-1;
  DeleteSema(mutex);mutex=-1;source_fd=-1;return -3;}
 return 0;
}
static void prepara_inicio(void){
 ring_geracao++;
 pending_fd=-1;switch_requested=reset_requested=0;rp=wp=level=source_eof=0;
 produced=reads_count=loops_count=errors_count=underruns_count=switches_count=0;
}
int audio_ring_start_loop(int fd,int loop){
 if(thread_id>=0)audio_ring_stop();
 fontes_reset(fd,loop,0,0,0);
 prepara_inicio();
 return inicia_thread();
}
int audio_ring_start(int fd){return audio_ring_start_loop(fd,0);}
/* O overlay de partida entra no ESTADO INICIAL da thread (fontes_reset), e nao
   por pedido: nao ha' espera nenhuma, entao nao ha' o que falhar. */
int audio_ring_start_overlay(int overlay_fd_){
 if(thread_id>=0)audio_ring_stop();
 fontes_reset(-1,0,0,0,0);
 overlay_vol[0]=100;
 if(overlay_fd_>=0){overlay_fd[0]=overlay_fd_;overlay_eof[0]=0;lseek(overlay_fd_,0,SEEK_SET);}
 prepara_inicio();
 return inicia_thread();
}
/* O PREVIEW DA FREEPLAY: `fd` e' um `stems.pcm` de 4 canais e `ini`/`fim` sao o
 * trecho em bytes (ver `fp_aud_janela`). O anel toca SO' o instrumental, com o
 * ganho do fade, em laco dentro da janela. O fd passa a ser da thread: quem
 * chama fecha depois de `audio_ring_stop()`, ou nao fecha de jeito nenhum se
 * depois usar `audio_ring_preview_trocar`, que fecha o anterior sozinha. */
int audio_ring_iniciar_preview(int fd,long ini,long fim){
 if(thread_id>=0)audio_ring_stop();
 fontes_reset(fd,1,(int)ini,(int)fim,1);
 prepara_inicio();
 return inicia_thread();
}
int audio_ring_preview_trocar(int fd,long ini,long fim){
 if(thread_id<0)return audio_ring_iniciar_preview(fd,ini,fim);
 fonte_pedido_loop=1;
 fonte_pedido_ini=(int)ini;fonte_pedido_fim=(int)fim;
 fonte_pedido=fd;
 return 0;
}
/* PEDIDO, NAO ESPERA.
   As funcoes abaixo publicam o pedido e voltam. A thread do anel aplica no
   topo do bloco seguinte (milissegundos), e o fd antigo do OVERLAY e' fechado
   por ela -- ver audio_ring_stop. Esperar o consumo daqui ja' quebrou no
   harness: `DelayThread` nao bloqueia de verdade no PC, entao a espera virava
   500 voltas instantaneas e o pedido "falhava" enquanto no console funcionava.
   O retorno so' diz se existe uma thread para receber o pedido. */
int audio_ring_trocar_fonte(int fd,int loop){
 if(thread_id<0)return audio_ring_start_loop(fd,loop);
 fonte_pedido_loop=loop?1:0;fonte_pedido_ini=fonte_pedido_fim=0;
 fonte_pedido=fd;
 return 0;
}
/* Um slot livre para uma fonte secundaria. Devolve o indice (0..3) ou -1 --
 * -1 quando os quatro estao ocupados ou nao ha' thread. `fd` passa a ser da
 * thread: ela le e, no `audio_ring_stop()`, fecha. */
int audio_ring_overlay_vol(int fd,int vol_pct){
 int i;
 if(thread_id<0)return -1;
 if(fd<0)return -1;
 for(i=RING_OVERLAYS-1;i>=RING_OVERLAY_AUTO_MIN;i--){
  /* Slot com fd VIVO nao e' livre, mesmo em fim de som: o fd e' do chamador e
     um pedido novo para o mesmo slot o poria em dois lugares. */
  if(overlay_fd[i]<0){overlay_pedido_vol[i]=vol_pct;overlay_pedido_loop[i]=0;overlay_pedido[i]=fd;return i;}
 }
 return -1;
}
int audio_ring_overlay(int fd){return audio_ring_overlay_vol(fd,100);}
/* O mesmo alocador, com o slot em LACO (o `play(true)` do flixel). O pedido de
   laco e' escrito ANTES do pedido de fd -- a thread consome o fd no topo do
   bloco, e escrever a flag depois dela poderia deixa-la para o proximo som. */
int audio_ring_overlay_loop(int fd){
 int i;
 if(thread_id<0||fd<0)return -1;
 for(i=RING_OVERLAYS-1;i>=RING_OVERLAY_AUTO_MIN;i--){
  if(overlay_fd[i]<0){overlay_pedido_vol[i]=100;overlay_pedido_loop[i]=1;overlay_pedido[i]=fd;return i;}
 }
 return -1;
}
int audio_ring_overlay_slot(int slot,int fd,int vol_pct){
 if(thread_id<0||fd<0)return -1;
 if(slot<0||slot>=RING_OVERLAYS)return -1;
 overlay_pedido_vol[slot]=vol_pct;
 overlay_pedido_loop[slot]=0;
 overlay_pedido[slot]=fd;
 return 0;
}
unsigned int audio_ring_geracao(void){return ring_geracao;}
void audio_ring_overlay_parar(int slot){
 if(slot<0||slot>=RING_OVERLAYS)return;
 if(thread_id<0){if(overlay_fd[slot]>=0){close(overlay_fd[slot]);overlay_fd[slot]=-1;}overlay_eof[slot]=1;return;}
 overlay_pedido[slot]=-1;
}
/* O `trocar_overlay` historico e' o slot 0: o game over e a tela de resultado
 * usam UMA fonte secundaria por vez. */
int audio_ring_trocar_overlay(int fd){
 if(thread_id<0)return -12;
 overlay_pedido[0]=fd;
 return 0;
}
void audio_ring_parar_overlay(void){audio_ring_overlay_parar(0);}
int audio_ring_sem_fonte(void){return sem_fonte?1:0;}
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
   aquelas escritas na 0.2.64 a corrida apareceu. Agora a espera e'
   explicita: ate 600 ms pelo encerramento normal, e TerminateThread so
   como ultimo recurso. */
/* QUEM FECHA O FD DO OVERLAY E' A THREAD (ou este stop), NUNCA O CHAMADOR.
   O overlay e' um fd que so' a thread le; fechar do lado de fora enquanto ela
   le e' leitura de fd fechado. Por isso o `trocar_overlay`/`parar_overlay` sao
   PEDIDOS e este stop fecha o que sobrou -- depois de a thread ter saido (se
   ela teve de ser terminada a' forca, o fd NAO e' fechado: um fd vazado e'
   melhor que um close em cima de um read). */
void audio_ring_stop(void){int n=0,saiu=0,i;running=0;
 if(thread_id>=0){
  while(!producer_exited&&n++<600)DelayThread(1000);
  saiu=producer_exited;
  if(!producer_exited)TerminateThread(thread_id);
  DeleteThread(thread_id);thread_id=-1;producer_exited=1;
 }else saiu=1;
 if(mutex>=0){DeleteSema(mutex);mutex=-1;}
 for(i=0;i<RING_OVERLAYS;i++){
  if(overlay_fd[i]>=0){if(saiu)close(overlay_fd[i]);overlay_fd[i]=-1;}
  /* Pedido que a thread nao consumiu: o fd ja' e' do anel desde o pedido, e
     sem isto ele vazaria (a thread nao vai mais ler). */
  if(overlay_pedido[i]>=0)close(overlay_pedido[i]);
  overlay_eof[i]=1;overlay_pedido[i]=RING_SEM_PEDIDO;
  overlay_vol[i]=100;overlay_pedido_vol[i]=100;
 }
 /* Troca de primaria PENDENTE que a thread nao chegou a aplicar: o fd novo ja'
    era do anel desde o pedido, e ninguem mais o fecharia aqui. */
 if(fonte_pedido>=0&&fonte_pedido!=source_fd)close(fonte_pedido);
 source_fd=-1;pending_fd=-1;switch_requested=reset_requested=0;rp=wp=level=source_eof=0;sem_fonte=0;fonte_pedido=RING_SEM_PEDIDO;fonte_pedido_loop=0;fonte_pedido_ini=fonte_pedido_fim=0;prev_ini=prev_fim=prev_ativo=prev_total=0;prev_pos=0;
 /* A geracao sobe TAMBEM na parada: quem guardou um fd para reusar tem de
    saber que ele foi fechado aqui (audio_ring_geracao). */
 ring_geracao++;}
