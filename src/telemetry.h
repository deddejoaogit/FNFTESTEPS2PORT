#ifndef FNF_TELEMETRY_H
#define FNF_TELEMETRY_H
#include <tamtypes.h>
int telemetry_init(const char *build_id, const char *elf_path);
void telemetry_event(const char *name, int a, int b);
void telemetry_frame(u32 frame, u64 begin, u64 after_draw, u64 after_vsync,
                     unsigned short pressed, int animation, int animation_frame,
                     int asset_frame, u32 textured_quads, int qwords, int state,
                     int transitions, u32 audio_heard, u32 audio_queued,
                     u32 ring_level, u32 underruns);

/* O log eh continuo, nao um despejo no fim. telemetry_finish() so rodava no
   exit, entao um travamento levava o log inteiro com ele -- que eh exatamente
   o caso em que o log eh necessario. telemetry_flush() escreve o que estiver
   pendente e pode ser chamado a qualquer momento, inclusive a cada quadro. */
void telemetry_flush(void);
unsigned telemetry_qring_txt(char *buf, unsigned size); /* 0.2.178: amostras Q em texto (corpo do TRAVA) */
/* 0.2.179: LOG VIVO -- anel circular pre-alocado, escrito por cima de clusters
   ja' alocados. O laco do quadro apenas ENFILEIRA (logvivo); a thread
   escritora drena (logvivo_servir). Nada cria nem estende ficheiros. */
void logvivo_init(void);
void logvivo(const char *fmt, ...);
void logvivo_servir(void);
unsigned logvivo_pendentes(void);
unsigned logvivo_descartados(void);
unsigned telemetry_flush_skip(void);   /* 0.2.174: despejos saltados (portao ocupado) */
void telemetry_anomaly_exec(const char *motivo); /* 0.2.175: escrita real (thread escritora) */
void telemetry_despejo_pendente(void); /* 0.2.175: o quadro apenas PEDE o despejo */
int telemetry_tem_pendente(void);
void telemetry_servir_pendentes(void); /* chamado pela thread escritora */
/* 0.2.180: escrita em SLOTS PRE-ALOCADOS. O ficheiro e' criado e pre-alocado
   com zeros no boot; em jogo toda a escrita e' lseek+write sobre clusters
   ja' alocados + close (nunca criar/estender/truncar/fsync). fnwrite_slot
   escreve buf em `off` e poe um NUL a seguir; fnwrite_slot_append avanca o
   `off` do chamador. NAO tomam o portao -- quem chama gere-o. */
int fnwrite_slot(const char *path, u32 off, const void *buf, u32 len);
int fnwrite_slot_append(const char *path, u32 *off, const void *buf, u32 len);
unsigned telemetry_boot_id(void);       /* id deste boot (comum ao LOG VIVO) */

/* Linha crua, escrita e esvaziada na hora: para o que nao pode esperar o
   flush do quadro (os marcos de fase). Se o jogo pendurar, o que jah passou
   por aqui estah no cartao.

   ATENCAO, e esta atencao eh a razao de esta funcao existir separada da
   telemetry_line(): cada chamada aqui faz UMA escrita no pendrive, e uma
   escrita no mass: pelo fileXio eh uma chamada de RPC sincrona para o IOP,
   que por sua vez escreve um bloco no USB. Isso custa de poucos a muitos
   milissegundos. Usar isto por sprite, dentro do laco de render, foi o que
   deixou a build 0.2.108 sem abrir no console: eram ~20 escritas bloqueantes
   por quadro, no meio do quadro, e o jogo passou a rodar a alguns quadros por
   segundo -- tela parada, sem reacao. Para volume alto, use telemetry_line(),
   que so acumula no buffer e sai no flush do quadro. */
void telemetry_raw(const char *text);

/* Linha de volume: acumula no buffer e NAO escreve. Sai no proximo
   telemetry_flush(), que acontece uma vez por quadro. Eh o caminho certo para
   tudo que eh gerado muitos vezes por quadro -- a lista de sprites, por
   exemplo, que pode ter centenas de linhas. */
void telemetry_line(const char *text);

/* Marco de fase (boot, troca de estado, travamento). Mesmo descritor unico do
   resto do log -- nunca um segundo escritor abrindo e truncando o arquivo. */
void telemetry_phase(const char *phase, int row, int code);

/* O que a tela mostra no canto: resultado do init, quadros registrados,
   eventos pendentes, descartes e despejos. Se o log nao abriu, isso eh a unica
   pista que existe -- por isso precisa estar visivel o tempo todo, e nao
   enterrada num arquivo que talvez nao tenha sido criado. */
void telemetry_status(int *result, unsigned int *frames, unsigned int *events,
                      unsigned int *drops, unsigned int *flushes);

/* Uma linha por quadro com o estado completo da maquina de estados. Os campos
   sao inteiros de proposito: o log precisa ser comparavel por diff, e ponto
   flutuante no formato de texto introduce ruido de ultima casa. */
void telemetry_state(u32 frame, int ui_state, int target_state, int selection,
                     int story_level, int story_diff, int transition_frame,
                     int transitions, int transport_state, int audio_started,
                     int audio_playing, int menu_audio_suspended,
                     int chart_status, int tutorial_scene_ready,
                     int tutorial_audio_status, int story_assets_ready,
                     int menu_cache_ready, int title_anim_status,
                     int gf_uploaded, int bf_uploaded, int gf16_uploaded,
                     int story_gf_uploaded, int story_bf_uploaded,
                     int aspect_169, int viewport_360,
                     u32 music_clock, u32 audio_submitted, u32 ring_level,
                     u32 underruns, int audio_fd, int title_audio_fd,
                     int note_hits, int note_misses, int note_combo,
                     int note_score, int note_feedback, int event_cursor,
                     u32 band_clip_rects, u32 band_clip_lines,
                     u32 band_cull_rects);

/* Um evento de E/S de arquivo, com o resultado. Eh aqui que se ve um asset que
   nao abriu, um tamanho que nao bateu, ou uma leitura que devolveu menos. */
void telemetry_io(const char *op, const char *path, long result, long detail);
/* 0.2.168: registo universal de erros (local+codigo), so' RAM. */
void telemetry_error(const char *onde, long codigo);
unsigned int telemetry_error_total(void);
unsigned int telemetry_errors_txt(char *b, unsigned int cap);

int telemetry_finish(void);
#endif

/* ===================================================================== */
/*  LOG MASSIVO (0.2.165)                                                */
/* =====================================================================
 *
 * Pedido do utilizador, 2026-11-04: "um log absolutamente massivo que
 * forneca absolutamente todas as informacoes possiveis [...] desempenho,
 * valor nas memorias, estados, imagem da tela com print ao clicar um
 * comando, tudo". Este bloco e' esse log.
 *
 * O QUE ELE CUSTA, medido e nao suposto: a linha por quadro custa ~200 B de
 * formatacao em RAM. A escrita no pendrive acontece so' no telemetry_flush()
 * (um arquivo por despejo, 64 KB de buffer, <= 1 despejo por segundo). Com o
 * pendrive lido a 0,72 MB/s (PROVA 2), 64 KB = ~89 ms por despejo, 1,5% do
 * tempo de parede. Se o despejo cair dentro de um quadro, esse quadro custa
 * +89 ms -- e por isso o flush acontece a cada 300 quadros, e o proprio log
 * mostra, no campo `ts`, quanto custou o despejo anterior.
 *
 * Por que um anel em RAM: logar TODOS os quadros no cartao custaria 200 B x
 * 60/s = 12 KB/s, ou 40 KB por quadro de 2 s... nao: 12 KB/s e' 1,7% da
 * banda. O problema nao e' a banda, e' a latencia de cada escrita sincrona.
 * Entao: TODOS os quadros ficam no anel de RAM (512 quadros = ~30 KB), e o
 * anel inteiro vai para o cartao quando algo anormal acontece (quadro acima
 * de 100 ms, ou o cao de guarda vendo o jogo parado). No caminho normal saem
 * 10 linhas por segundo + todos os eventos.
 */

typedef struct {
    u32 frame;
    /* Tempos, em microssegundos. Somam ~t_total (fora a folga do laco). */
    u32 t_total, t_in, t_upd, t_dma, t_wait, t_cap, t_vsync, t_post, t_flush;
    u16 qwords, quads, binds, uploads, up_kb, ovf;
    u16 clip, cull, st, tr, sel, pad, pressed, anim, anim_frame;
    u32 clk, queued, ring, underruns;
    u32 vram_free, ram_bufs, io_kb;
    u32 sub_ok, lr_bytes;   /* 0.2.173: contadores vivos (audsrv ok / lido pelo produtor) */
    u8  eof, paused, audio_playing;
} fnf_frame_t;

/* Registra o quadro: entra no anel (sempre) e na linha de fluxo (10 Hz ou
   quando algo anormal). Nao escreve no pendrive. */
void telemetry_frame2(const fnf_frame_t *f);

/* Despeja o anel INTEIRO num arquivo proprio, agora. Para quando o jogo trava
   (cao de guarda) ou quando um quadro passa de 100 ms. Rate-limited. */
void telemetry_anomaly(const char *motivo);

/* Bloco de texto livre (snapshots: memoria, VRAM, texturas, GS, audio). */
void telemetry_snapshot(const char *tag, const char *texto);

/* Custeio de upload de textura (VRAM), somado por quadro pelo main.c. */
void telemetry_note_upload(unsigned int bytes);

/* Devolve o custo do ultimo flush em microssegundos (vai no campo t_flush). */
u32 telemetry_last_flush_us(void);

/* Estatisticas para a tela: quadros, quadros lentos, pior quadro (us). */
void telemetry_stats(u32 *frames, u32 *slow, u32 *max_us, u32 *hist50);

/* Linha do arquivo de sessao (FNF_SESSAO.txt): ambiente do boot. */
void telemetry_boot(const char *linha);
void telemetry_boot_flush(void);
