# CORREÇÕES — FNF PS2 (actualizado na 0.2.181)

Documento pedido pelo utilizador: **absolutamente tudo o que precisa de ser
corrigido**, incluindo TODAS as correções que ele cita. Cada entrada: número,
o que está errado, quem citou (UTILIZADOR / MEDIDO / CONHECIMENTO), onde está,
estado.

Convenções: `[ ]` aberta · `[x]` corrigida (diz a build) · `[~]` parcialmente.

---

## A. CORREÇÕES CITADAS PELO UTILIZADOR

### #1 [x] SFX do clique chega TARDE (0.2.177, UTILIZADOR — RESOLVIDO 0.2.187: vozes ADPCM)
"o efeito sonoro de clicar só é emitido depois que já entrei na tela" — no
main menu, o som do clique devia tocar NO MOMENTO do clique, não depois da
transição. Provável causa: o SFX é pedido no estado seguinte (ou o menu_audio
é suspenso durante a transição e retomado depois). Local: main.c, caminho do
`pressed&PAD_CROSS` do título → transição → o `g_menu_sfx_pos` dos botões.

### #2 [ ] Transição título → main menu DIFERENTE do original (0.2.177, UTILIZADOR)
"a transição está diferente do original" — comparar com o Haxe/flixel (o flash /
fade do original, os tempos exactos). Fonte: `/tmp/funk86` (TitleState.hx /
MainMenuState.hx). Medir os tempos do original e copiar as constantes.

### #3 [ ] Transição FREEPLAY diferente do original (0.2.177, UTILIZADOR)
"Ao entrar no modo free play: transição também diferente do original". Idem #2,
comparar com FreeplayState.hx.

### #4 [x] Textos SIMPLES (sem fonte especial) BUGADOS (0.2.177 — RESOLVIDO 0.2.187: avanços proporcionais)
"Os textos simples, sem fonte especial, estavam todos bugados" — os glifos do
caminho de texto simples saem corrompidos/errados no ecrã de story mode. Os
textos com fonte especial (títulos) parecem OK. Rever o desenho dos glifos
simples (font.rgba/font.widths → o caminho de texto simples) no contexto do
story menu (a scissor/band do story title pode interferir).
**0.2.179 (UTILIZADOR, de novo): "os botões do story menu estavam de novo
bugados após entrar no free mode e depois no story menu"** — REPRO: entrar no
story menu pelo título (OK), ir ao freeplay, voltar ao story menu = BUGADO.
PROVA VISUAL (captura FNF_SCREEN_0001 da sessão 0.2.179, guardada em
`evidencia-console/0.2.179_story_flip.png`): os títulos das semanas e os
textos pequenos saem VERTICALMENTE INVERTIDOS (o verde da seta está certo —
o bug é do upload/desenho dos títulos+texto, não da captura). A hipótese
forte é `story_upload_title`/`story_font_alpha_decode` a reutilizar o slot
com dims/sentido errados quando o caminho vem do freeplay (ui_state 10).
LOG VIVO 0.2.180 regista `EV=STORY_RESTORE` (dims da tex + nível) para
provar a causa na próxima sessão. TRABALHO VISUAL SEGUINTES (#1-#4).

### #5 [x→~] SELECT+R3 CONGELAVA o jogo (0.2.177, UTILIZADOR)
"ao clicar Select+R3 o jogo travou e a tela de erro apareceu" — a captura
escrevia 286 KB NO LACO DO QUADRO; o write() pendurou no usbhdfsd (ficheiro
FNF_SHOT criado com 0 bytes) e o laco morreu ali (marca=F.DRAWN prova a
localização). CORRIGIDO na 0.2.178: a captura vai para RAM (readback LIMITADO
por timeout — não pode pendurar) e a escrita é da thread escritora; o laco do
quadro nunca mais escreve. Residual: a escrita da imagem pode ainda pendurar a
THREAD ESCRITORA (o jogo vive; o TRAVA traz o resto).

### #6 [x] Os VALORES da TV no txt — PERGUNTA DO UTILIZADOR: "já está anotando
automaticamente os valores exibidos no txt?" — **SIM, provado na própria
sessão 0.2.177**: o FNF_TRAVA-7.txt tem `# VALORES f=30865 st=7 beat=30865
q=0 ring=1048576 un=1 lr=119852128 so=26626` = exactamente os números pintados.

### #7 [x] Captura = "save state gigante" (pedido: imagem + máximo de info do
frame: visual, desempenho, sonora) — IMPLEMENTADO na 0.2.178: `# SAVESTATE`
com estado completo (ui, contadores, áudio, VRAM, marcas, amostras Q) + a
imagem 320×224; SELECT+R3 dispara; também sai no TRAVA.

---

## B. CORREÇÕES TÉCNICAS (MEDIDAS)

### #14 [x] SESSÃO CEGA (0.2.178, MEDIDO — o utilizador exigiu logs que
descrevam SEMPRE o que aconteceu) — sem freeze não havia registo nenhum (a
RAM perdia-se ao desligar). CORRIGIDO 0.2.179: **LOG VIVO** — anel circular
pré-alocado (`FNF_LOGVIVO.bin`, 512 KB) escrito por `lseek+write` sobre
clusters já alocados (sem criar/estender FAT durante o jogo). Regista SEMPRE:
transições de estado, acções (SELECT+R3), erros (local+código), anomalias, o
ciclo de vida das capturas, e o compacto do quadro a 2 Hz (tempos, estado,
pad, áudio, VRAM). O extractor v7 reconstrói a linha do tempo inteira.

### #15 [x] Capturas 0 bytes (0.2.178, MEDIDO) — a causa: pré-criar um ficheiro
VAZIO não aloca clusters; a primeira escrita precisa de ALOCAR no FAT (a
operação de metadados que morre depois de minutos de fluxo de áudio). Prova:
o MARCADOR chegava sempre ao FNF_TRAVA (que já tinha clusters). CORRIGIDO
0.2.179: os SHOT/SAVESTATE são PRÉ-ALOCADOS com zeros no boot e as escritas
são sobrescritas (`lseek+write`). Mais: a captura deixou de usar a pausa (que
ficava presa e matava a música) e passou a usar só o `io_gate`.

### #16 [x] FREEZE ao entrar no TUTORIAL (0.2.179, UTILIZADOR + MEDIDO)
"o jogo só travou ao entrar no tutorial". Cadeia de prova da sessão 0.2.179:
(1) BOOT.txt = "passo 45: TUT_FONT_BEGIN code=0" = a ÚLTIMA marca que
escreveu; (2) entre TUT_FONT_BEGIN e TUT_FONT_DONE só existem
`tutorial_vcr_restore()` (100% RAM — provado pela leitura do código) e o
`boot_mark` seguinte, que fazia `open(O_CREAT|O_TRUNC)` + `fsync` no BOOT.txt
— meta-op FAT + fsync DURANTE o jogo; (3) FNF_TRAVA = só "# VIGIA_VIVO" = o
despejo do vigia não chegou ao disco (caminho de escrita já morto); (4) as
capturas SHOT ficaram a zeros (pré-alocadas mas nunca escritas). CAUSA:
escritas de diagnóstico com meta-ops (criar/truncar/estender) + fsync morrem
no usbhdfsd depois de minutos de streaming — e esta acontecia dentro do laço,
na entrada do tutorial. CORRIGIDO 0.2.180: TODOS os ficheiros de diagnóstico
são pré-alocados com zeros no boot e escritos por `lseek+write`+`close` sobre
clusters já alocados; nunca criar/truncar/estender/fsync em jogo; o marco que
não cabe SALTA (e conta no LOG VIVO) — o diagnóstico nunca trava o jogo.

### #17 [x] Demasiados ficheiros de log (0.2.179, UTILIZADOR)
"Pode fazer os logs criam 5 ou 4 txts? Não consigo enviar muitos como no
último." A sessão 0.2.179 deixava 32 FNF_LOG%02d + 12 FNF_ANOM_%02d + 4 SHOT
+ 4 SAVESTATE + BOOT/SESSAO/DIAGNOSTICO/TRAVA/AV/SCREEN... (>50 ficheiros,
que ainda se multiplicavam nas cópias do Windows). CORRIGIDO 0.2.180: 5 TXT
de nomes fixos (FNF_BOOT.TXT, FNF_LOG.TXT, FNF_TRAVA.TXT, FNF_TELA.TXT,
FNF_ERRO.TXT) + FNF_LOGVIVO.bin + FNF_SHOT.rgba + FNF_SAVESTATE.bin. Os
antigos nomes rotativos desapareceram; o extractor v8 lê o layout novo E o
antigo.

### #18 [x] TELA PRETA NO BOOT (0.2.180, UTILIZADOR + MEDIDO) — REGRESSÃO
"O jogo não abriu, tela preta e sem som." FALHA TOTAL de arranque logo após a
0.2.179 (que entrava no tutorial). Prova dos ficheiros enviados: FNF_BOOT.TXT
= 8192 B com a última marca "passo 2: USB_PRONTO" (o slot foi preenchido até
ao fim) e FNF_LOG.TXT = 978 B (SÓ o cabeçalho da telemetria — o preenchimento
de zeros não escreveu UM byte sequer). Cadeia: entre a marca "USB_PRONTO" e a
seguinte só existia o novo laço de pré-alocação; ele preencheu o FNF_BOOT
(8 KB, OK) e MORREU no primeiro write do FNF_LOG.TXT — um ficheiro acabado de
escrever (978 B) que se estava a ESTENDER em 2 MB dentro de um ciclo fechado
de writes sem pausas. A 0.2.179 o maior preenchimento de boot era 512 KB
(LOG VIVO) e nunca houve uma extensão destas. CORRIGIDO 0.2.181: pré-alocação
usa o padrão PROVADO do logvivo_init (criar de novo O_TRUNC + preencher de uma
assentada + fechar), cada ficheiro ≤ 512 KB (anel do log = 512 KB, cabe ~2 h de
despejos), com marca "PREALLOC <nome>" ANTES de cada ficheiro (se morrer de
novo, o BOOT diz exactamente em qual) e o bloco descritivo do log passa a ser
escrito DEPOIS da pré-alocação (antes morria com o truncamento).

### #19 [x] ESTOURO DE HEAP no ecrã de diagnóstico do vigia (0.2.181, MEDIDO
com AddressSanitizer no harness) — bug LATENTE, independente do #18.
"corrupted size vs. prev_size" intermitente no harness. ASan: heap-buffer-
overflow em draw_clear ← fnwd_dig ← fnwd_num ← fnwd_pintar ← fnwd_dump — o
pacote DMA do vigia era packet_init(2048) = 2048 qwords e o ecrã pinta 39
dígitos × 7 segmentos × ~8 qwords ≈ 2222 qwords no pior caso. No consola isto
escrevia sobre o heap do EE e mandava GIF lixo quando o vigia disparava (num
freeze): o ecrã de diagnóstico podia ele próprio derrubar o jogo. CORRIGIDO
0.2.181: packet_init(4096) (o mesmo tamanho do pacote do quadro principal).
Prova: 6 execuções ASan seguidas limpas (antes falhava em ≤8).

### #20 [x] Diagnóstico 0.2.181 interpretado + diagnóstico que não destrói (0.2.182)
Os logs de 0.2.181 (boot 03700f00) resolveram 3 mistérios de uma vez:
(1) a "tela bugada com números" É a pintura do vigia a disparar em engasgos
reais de 3 s (parado_ha=3000ms em F.PACKET = o próprio telemetry_flush a
escrever para o USB); (2) ela fazia `graph_vram_clear`+`graph_initialize` =
**destruía o GS** — bf/gf desaparecidos, textos como texturas, barra de vida
parada = VRAM morta; (3) SFX tarde = produtor de áudio preso (SRV=HANG ×7 nos
Q; ui_som_tocar usava `audio_ring_overlay_slot` = a mesma mistura do produtor).
**Correções (0.2.182)**: fnwd_pintar NÃO toca no GS (pinta no buffer actual);
logvivo consertado (fd fechado após o pre-fill = o tamanho aparece; seq i=8..15;
porta NÃO-bloqueante); despejo do anel serializado dentro de telemetry_flush +
`# DESPEJO` por bloco (antes o anel nem saía!); anel de marcas de boot (slot 4096
= as marcas ENCASTELADAS do 0.2.176); SFX directo no canal (conta misturas) +
EV=SFX; EV=PRODUTOR preso/ok; hud hp/nt no logvivo; MAX_DUMPS 8 sem teto de
20 min; SHOT.rgba = captura real 320×224 no freeze; extrator v9 (resumo de
SFX/produtor/vigia + batimento do anel + VALORES sem duplicar).

### #21 [x] CONGELAMENTO NO TUTORIAL — cadeia causal COMPLETA (0.2.183)
Logs do teste 0.2.182 (boot 02e67e00) + o trace do vigia provaram a cadeia:
1. **O produtor de áudio morre DENTRO de um `read()` segurando o portão**
   (`iow.produtor_preso` ×2 em t=55s/57s; `io_gate` preso para sempre);
2. O `flush_output` do log SALTA quando o portão está ocupado → **o FNF_LOG.TXT
   nunca escreve durante a música** (0 blocos em 30 s; era intencional para o
   despejo periódico, mas o flush DO DESPEJO do vigia também saltava porque o
   roubo do portão não passava a posse → o dono morto continuava a "ocupar");
3. O quadro chamava `gio_read` (portão BLOQUEANTE) na bomba do stream da GF,
   no meio da transição para o tutorial → **bloqueou para SEMPRE** (trace:
   F.TITLE escrito em f=1798, F.STATE nunca; transition_frame=15 preso);
4. O vigia despejou (teve de ROUBAR o portão: `fnwd.roubo_portao`) e pintou a
   TV (a "tela de números" que o utilizador viu) — e o áudio drenou (q=0).
**Correções (0.2.183)**: `io_gate_lock_prazo(60ms)` em TODOS os gio_* (o quadro
nunca mais bloqueia: falha de E/S degrada, nunca congela); `io_gate_roubar()`
coerente (o vigia passa a ser dono e o flush do despejo escreve o LOG.TXT);
eventos P,/E, EM RAM entram no corpo do TRAVA (sobrevivem ao flush morto);
out_begin não descarta o buffer; marca F.GFRELOAD no trace.
**ABERTO (causa seguinte)**: o produtor de áudio morre no arranque da música do
tutorial (TUT_PCM_OPEN fd=3 OK; a morte é depois, no switch do ring) — sem som.

### #22 [x] Teste 0.2.183: o tutorial ABRE e JOGA-SE — causas novas identificadas (0.2.184)
PROVADO pelo teste (boot 03700f00 era o antigo; este: trace f=6074+):
(1) **O congelamento do tutorial ESTÁ RESOLVIDO** — a espera limitada de 60 ms
fez o jogo sobreviver ao engasgo ("a tela de erro apareceu mas depois o
tutorial abriu normalmente"); (2) o marcadór `marca=F.GFRELOAD` provou o ponto
exacto do engasgo (a bomba do stream da GF, como diagnosticado);
(3) **o produtor de áudio morre no SWITCH da música do tutorial** (55 s,
3 sessões seguidas) — o `lseek(source_fd,0,SEEK_SET)` do switch é uma meta-op
do usbhdfsd e é o ponto medido da morte do modelo; sem música e com o portão
preso, os loads dos personagens/ícones falham (tutorial sem personagens, barra
sem ícones, textos bugados) e o fim (ecrã de resultados) trava num RPC do IOP;
(4) pedido do utilizador: a TELA DE NÚMEROS do vigia foi REMOVIDA (aparecia em
engasgos recuperáveis e estragava a experiência) — o vigia só grava ficheiros.
**Correções (0.2.184)**: pintura do vigia removida; o `lseek` do switch
removido (o fd chega novo a offset 0); o monitor LIBERTA o portão quando o
produtor morre (roubo+unlock) → os voltam a carregar e o logvivo continua;
extrator v9.2 (sem duplicados + conclusão da cadeia).

### #23 [x] TESTE 0.2.184: O SOM TOCOU — e a cadeia do stop/switch ESTÁ PROVADA (0.2.185)
PROVADO pelo teste (story mode → tutorial, extrador v9.2):
(1) **O SOM TOCOU** (1.ª vez no tutorial/resultados!): o produtor leu 14,3 MB,
2 978 submissões audsrv, 4 underruns — o áudio está VIVO;
(2) `EV=PRODUTOR preso f=3051 → portao_livre f=3051 → ok f=7093`: o produtor
prendeu **67 s** dentro do usbhdfsd exactamente no switch da música do tutorial
(3.ª sessão seguida no mesmo ponto) — o jogo sobreviveu (o monitor libertou o
portão) e o som voltou em f=7093 (o som que o utilizador ouviu NO ECRÃ DE
RESULTADOS = o PCM da música a chegar atrasado);
(3) **a tela preta do tutorial + texturas bugadas + resultados sem personagens**
= as cargas falharam TODAS durante a janela do driver encaixado: a sequência
medida é `audio_ring_stop()` do switch → o produtor está DENTRO de um read →
a espera de 600 ms vence → **TerminateThread deixa uma leitura PENDENTE no
IOP** → os `close()` seguintes colidem com ela ("travamento duro, sem retorno"
— o próprio comentário da 0.2.63 previa este cenário) → o usbhdfsd encaixa →
o iow, no ciclo seguinte, rouba o portão e **prende-se no `open()` do
logvivo com o portão na mão** (prova: um ÚNICO `EV=PRODUTOR preso` em 67 s,
porque o iow deixou de correr) → `gio_*` (prazo 60 ms) falham em massa →
tutorial_scene_load/HUD/chars nunca carregam → **tela preta**; o ecrã de
resultados carrega contra o produtor já em streaming (o trylock perde) →
sem personagens + bugs visuais; o draw do resultado com dados lixo sufoca o
GS (tm_drawn−tm_wait = 221 ms no último quadro) → **freeze em F.PACKET**;
(4) **as capturas nunca saem** (SELECT+R3 ×4 = 4 "portao ocupado", shot a
zeros, despejos=0, FNF_LOG parado aos 12 s) — o trylock único perde sempre
para o streaming; o utilizador: "não vejo todas as imagens".
**Correções (0.2.185)** — uma causa (o switch do tutorial não pode matar o
produtor nem o driver):
(a) `audio_ring_stop()`: **PAUSA PRIMEIRO** (handshake provado — o ack garante
o produtor fora de reads) e, se ainda assim for preciso TerminateThread,
marca PARADA SUJA e **ninguém fecha os fds** (fd vazado > close sobre leitura
pendente);
(b) closes do TUT_PREP/back guardados com `audio_ring_parada_suja()`;
(c) **sem meta-ops nos fds novos**: lseeks removidos do switch de fonte, dos
overlays e do start_overlay (só os previews rebobinam — fd reutilizado);
(d) iow: flag USB-DOENTE — enquanto o produtor estiver preso, NENHUMA escrita
nossa toca o driver (o LOGVIVO fica em RAM e é servido depois);
(e) **capturas/SAVESTATE com a janela PAUSADA do produtor** (pause→trylock→
escrita→resume) + repetição automática — as imagens DEVEM sair;
(f) boot_mark com 3 tentativas (as marcas TUT_SCENE_DONE+ morriam à 1.ª);
(g) marca F.SINAL divide a janela F.PACKET→F.DMA em duas (o freeze final);
(h) extrador v9.3: q-lines ordenados, TODOS os eventos do LOGVIVO, regras de
conclusão para a cadeia do stop/switch e para as capturas.

### #8 [x] Números da TV sobrepostos (0.2.175 → corrigido 0.2.176)
Os números ciano/verde encostavam-se (o utilizador não conseguia ler o `so`).
Corrigido (tamanhos/posições novas).

### #9 [x→~] História do log não chegava ao ficheiro (0.2.177, MEDIDO)
O anel Q/S/E/P acumulava-se em RAM mas o TRAVA morria no `open()` do ficheiro
de despejo (FAT CREATE pendurado): o FNF_TRAVA ficava só com 3 linhas. 0.2.178:
(a) o corpo do TRAVA (marcas+erros+amostras Q) escreve ANTES de qualquer
escrita grande; (b) os ficheiros de diagnóstico são PRÉ-CRIADOS no boot (as
escritas seguintes são APPEND provado — o MARCADOR chegou sempre); (c) as
amostras Q saem também directamente no corpo do TRAVA.

### #10 [x] Despejos periódicos matavam o usbhdfsd (fechado na 0.2.177)
Prova cruzada: 0.2.148-158 (sem escritas) jogável; 0.2.159-162 (401 escritas,
sem leituras) 40 min; 0.2.173-176 (escritas durante o fluxo) morte ao 5º
despejo. Regra: NUNCA escrever enquanto a música corre.

### #11 [x] FNF_SCREEN_0000.rgba.txt = 0 bytes para sempre (MEDIDO)
Código morto: o ficheiro era aberto no boot e ninguém escrevia nele. Ignorar
(o extractor v6 já o diz). Eliminar numa limpeza futura.

### #12 [ ] pad=0000 em TODOS os registos (CAUSA SEPARADA)
Nunca foi visto um bit do pad nos Q-lines. O jogo responde ao X (o título
avança) — logo o pad CHEGA ao jogo; o que falha é o REGISTO (o campo `pad` da
telemetria). Ver de onde vem o campo `pad` no Q-line e ligá-lo ao `pressed`/`held` real.

### #13 [x] Extrador em pastas sem logs assustava (corrigido v4+)
O extractor avisa e procura a pasta FNFPS2 perto; mistura de sessões = o
protocolo manda apagar os FNF_* antigos (o utilizador cumpre — obrigado!).

---

## C. REGRAS DE OURO (nunca voltar atrás)

1. O laco do quadro NUNCA escreve em mass: (regra 7 das REGRAS.md).
2. Nenhuma escrita enquanto o produtor de áudio faz streaming (o usbhdfsd morre).
3. Operações FAT (criar/estender/truncar ficheiros) e `fsync` são fatais
   depois de minutos de leitura em fluxo: PRÉ-ALOCAR com ZEROS no boot;
   depois só `lseek+write` sobre clusters alocados + `close` (a regra da
   0.2.180 — ver #16).
4. `PollSema` é proibido em EE (decrementa sempre) — só WaitSema/SignalSema.
5. O `fsync` repetido mata o usbhdfsd (só no TRAVA).
6. Toda a afirmação = medida; toda a build = uma causa única.
7. A pintura da TV (GS puro) é o último canal quando o USB morre.

---

### #24 [x] Campanha 0.2.186: eventos v3 + bomba em thread + telemetria do harness (Fix A/B/C)
(1) **FACTO**: os charts FCH3 v3 (todos os reais) trazem FOCUS/ZOOM/SCROLL/BOP/
HEALTH_ICON no bloco de EVENTOS, mas o `chart_events_apply` só descodificava os
tipos 0-4 e os valores `(a<<8)|b` dos passos — 18/24 testes de unidade morriam
com "estouro de stack" (virgem=0) e os efeitos de câmara/ícones nunca chegavam
ao jogo = **"personagens/eventos não aparecem"** (causa 18). Corrigido:
`chart_events.c` decodifica os 5 tipos v3 (secção nova, layout verificado byte a
byte contra os charts reais em `assets-data/`) com `fx(u32)=(s32)/1000` e
primário = `(s16)p` para chars. 24/24 testes de unidade verdes.
(2) **FACTO**: o relógio da música avançava no frame loop ⇒ passos/letras a
depender do fps = "música engasga com fps baixo" (causa 17). Corrigido (Fix A):
thread dedicada prio 45 (`pump_thread_fn`, arranca no frame 1 com
`fnwd_start();iow_start()`) que chama `audio_bombear()` a cada 2 ms; os sites do
frame loop passaram a só marcar `g_pump_fn`. audsrv é thread-safe (audsrv_rpc.c
usa completion_sema em TODAS as chamadas — verificado no ps2sdk oficial).
Harness ×2 + ASan ×3 verdes; música reproduz no harness (124 928 amostras).
(3) **FACTO**: o teste de cobertura do run_tests.sh exigia `state,6/7/8` no
FNF_LOG.TXT do harness e falhava sempre. Investigação completa (logs rt10 e
direct1 preservados em /tmp): **o jogo corre os 1 402 quadros todos do roteiro
(h_frames_run++ é no graph_wait_vsync; 83 200 draws, 2 054 pacotes, 433 opens,
12/12 teclas, 0 estouros)** mas o CANAL DE TELEMETRIA PARA FICHEIRO (despejos
Q/E via iow/despejo) para de gravar de forma não-determinística (quadro 1 no
rt10, 584 no direct1) — mesma classe do "FNF_LOG parado" da consola (causa 19,
EM ABERTO). O `vigia` do jogo faz 8 dumps falsos no harness (DelayThread das
threads não-render = sem espera real ⇒ 15 acordadas instantâneas = falso
positivo de paragem) — artefacto do harness, não do jogo.
(4) **FACTO (ferramentas)**: os globs `FNF_LOG*.txt` não achavam `FNF_LOG.TXT`
em Linux (case-sensitive) — `check_stage_budget.py` e `test/run_tests.sh`
(2 sítios) corrigidos; o shim de host `test/ps2sdk-shim/kernel.h` não tinha
`GetTimerSystemTime` (só afectava os testes de unidade) — corrigido.
(5) **DESCARTADO**: loop infinito em `menu_atlas_resample_phys` — os laços são
limitados pela tabela `const g_menu_frame[5][2][3][3]` com valores sãos
(verificados); o ponto exacto da paragem do canal de log fica para a próxima
campanha com marcação fina no iow (REGRAS: sem dado ⇒ build seguinte coleta).

### #25 [x] CAMPANHA 0.2.187 — TODOS os bugs do teste 0.2.186 (UTILIZADOR: textura/grelha, fps, código morto, SFX tardio, freeze story menu, capturas ilimitadas)

Pedido: "revisasse o código e solucionasse todos os erros... Só corrija todos
estes bugs" + "Como dar print na tela?" → capturas ilimitadas a qualquer
momento (menos em cargas). FACTO CRÍTICO da sessão: o ELF testado reportava
`build: 0.2.185` mas CONTINHA o código 0.2.186 (o `printf > VERSION` correu
depois do make; o Makefile lê VERSION na compilação) — etiquetar sempre
VERSION **antes** do make (cumprido: 0.2.187 embutido 6× no ELF).

**Cadeia do FREEZE no story menu (FACTO, fechada)**: SELECT+R3 (f=4546) →
`estado_gigante_gravar` gravou 49 KB OK → `capture_framebuffer_fast` nunca
gravou (FNF_SHOT.rgba = 286 720 B a zeros) → usbhdfsd encaixa → IOP morre →
o LOOP bloqueava em `audsrv_queued` (MARCA `audsrv_queued`, main.c:9766 = a
marca exacta do savestate do FNF_TRAVA-4) → vigia e logvivo morrem juntos.
Corrigido: os RPCs do audsrv (queued/available) passaram para a **thread da
bomba** (`pump_thread_fn` actualiza `g_arpc_vara/varb`); o loop só lê cópias
— se o IOP morrer, quem bloqueia é a bomba (thread separada), o loop continua
a pintar (a garantia da 0.2.175). TESTE DE ACEITAÇÃO: o utilizador NÃO deve
conseguir travar o jogo com SELECT+R3 repetidos.

**SFX "só depois" (FACTO, fechado)**: o anel de áudio tem 1 MB (5,4 s de
música pré-produzida) e os SFX eram misturados à PRODUÇÃO = tocavam tarde
(`EV=SFX confirm_menu f=258 mist=0` provou a mistura em blocos futuros).
Corrigido: **vozes ADPCM do SPU2** (audsrv_load_adpcm/ch_play_adpcm) — os 7
samples (5 de `assets/result/*.pcm` + slices 0..39168 / 39168..241920 do
menu_sfx) sobem uma vez no boot e tocam no INSTANTE do botão. Formato do
buffer = ps2sdk adpcm.c verificado (header 16 B, pitch 0x1000=44.1k → 4551
para 48 kHz). Codificador próprio (shift do pico × 5 filtros, 70 ops/bloco).
Fallback automático para o mix no anel se o load falhar (nunca regressão de
som). RISCO CONHECIDO: o alloc SPU2 do adpcm começa em 0x5010 e o comentário
do ps2sdk admite sobreposição com o streaming — se a música "estragar" quando
os SFX tocam, desligar `SFX_ADPCM` (define no main.c) e reportar.

**FPS do main menu (FACTO por código)**: o uploader de animação (estado 6)
subia 4 texturas a 24 Hz com memcpy+dma_wait síncronos (~96 uploads/s = 15-30%
do orçamento do quadro). Corrigido: cache por personagem `(sel,af[,blend])` —
só sobe o quadro que MUDOU (`g_menu_up_chave[4]`); força só quando
`menu_anim_tick<0` (troca de textura).

**Textos (PROVA4)**: `font.widths` real = 10..23 em células de 24 unidades; o
`story_vcr_line` usava avanço FIXO 15 px (célula inteira) e o HUD 7.5 px —
colavam/espaçavam mal. Corrigido: avanços proporcionais às larguras reais
(VCR ×0.625, HUD ×0.3125×zoom, igual ao factor da célula de cada um);
`text_draw_alpha` já usava widths×0.5 (correcto).

**Capturas ilimitadas (PEDIDO)**: 8 slots pré-alocados rotativos
(`FNF_SHOT.rgba` + `FNF_SHOT_01..07.rgba`, os novos criados esparsos em boot
para não pesar); sem limite de 4 (`g_shot_n<4` removido); guarda de CARGA
(`g_trans_espelho>0` → `captura_bloqueada_carga`, o pedido fica para quando a
transição acaba — menos nas telas de carregamento, como pedido); escrita por
fatias de 16 linhas (20 KB) em vez de 224 escritas de 1,28 KB (matavam o IOP)
e `raw_write_chunks` a 16 KB + 2 ms. Extractor v9.4 lê os 8 slots + tempos de
quadro por estado.

**VALIDAÇÃO (0.2.187)**: harness 1402 quadros PASSOU ×N (0 estouros, 0 guard,
12/12 teclas, todo asset); logvivo prova o roteiro completo ESTADO 0→6 (f585)
→7 (f696, accept em 96 f = fidelidade 0.2.105) →8 (f775) com 4 ciclos
story/tutorial; `EV=SFX adpcm 7/7` + `EV=SFX confirm_menu f=420 adpcm`;
unidades ALL PASS. VERMELHO CONHECIDO (pré-existente, documentado): a secção
"gameplay de ponta a ponta" do run_tests.sh lê eventos de telemetria que no
harness param de ser drenados ~f=610 (a seguir ao savestate pedido pelo vigia;
o JOGO corre os 7602 quadros todos — só o dreno de log para) + esperava
`FNF_COLHEITA.txt` (extinto desde 0.2.164). Infra-estrutura de telemetria do
harness = trabalho futuro (IMPLEMENTAR).

---
*Novas correções do utilizador = nova entrada em A. Actualizar os estados.*

---

## 26. 0.2.188 (08/11/2026) — REGRESSAO DE BOOT: "tela preta e sem som, nao abriu, nao ha log" (0.2.187)

**Sintoma**: o PS2 nem arrancou -- nem o titulo, nem som, nem log.

**FACTO**: o unico codigo da 0.2.187 executado ANTES do primeiro pixel foi o novo
prealloc de slots de captura: `lseek` + `write` 1-byte = ficheiros ESPARSOS =
extensao FAT em ciclo fechado 12x. E' a mesmissima classe da correcao **#18**
(0.2.180, sintoma identico): escritas USB continuas no boot matam o usbhdfsd
neste console e o jogo morre antes de pintar.

**INFERENCIA**: a morte e' nessa janela; as causas B-E abaixo sao do mesmo bloco
novo e tambem correm antes do 1. pixel (arranque ADPCM) ou gastam RAM/BSS
(aumentando a chance de qualquer azar no boot).

**Correcoes (TODAS de uma vez, pedido "corrija o maximo de erros por versao")**:
- **A**: prealloc volta ao padrao PROVADO da #18: tabela curta (FNF_SHOT.rgba +
  FNF_SHOT_01..03.rgba + FNF_SAVESTATE.bin), preenchimento de uma assentada em
  `z[32768]`, `DelayThread(2000)` entre escritas e `DelayThread(10000)` entre
  ficheiros. Sem lseek, sem esparsa, sem nada novo.
- **B**: `sfx_adpcm_arrancar` deixa de correr no arranque -- a thread da bomba
  executa-o quando o loop pede (depois do primeiro pixel), com marcas
  SFX_ADPCM_BEGIN/INIT_OK/DONE no FNF_BOOT. Falhou = o caminho antigo do anel
  continua (fallback).
- **C**: o `static unsigned char tmp[1048576]` (1 MB de BSS) e' substituido por
  memalign (heap), leitura exacta em ciclo (short reads do usbhdfsd) e
  `DelayThread(5000)` entre amostras.
- **D**: o encoder ADPCM estava ERRADO: codificava 14 amostras por bloco e o
  formato real e' **16 bytes = 28 nibbles = 28 amostras** (adpcm.c do IOP). Os
  sons sairiam ruido. Corrigido para 28/bloco com bytes de dados = (hi<<4)|lo,
  nibble par em lo, flags 0x01 no ultimo bloco.
- **E**: os RPCs `audsrv_queued/available` (2 por iteracao da bomba = centenas/s
  de trafego SIF) passam a actualizacao limitada (1x em 16 iteracoes).
- **Rotacao de capturas coerente**: o `capture_framebuffer_fast` rotava %8 (8
  nomes) enquanto o prealloc so' criava 4 -- a 5a captura estenderia FAT em
  jogo (= classe #18). Agora G_SHOT_SLOTS=4 em todo o lado: FNF_SHOT.rgba +
  FNF_SHOT_01..03.rgba, a 5a captura reescreve o slot mais antigo.

**Prova do harness**: FNF_BOOT mostra `passo 44: SFX_ADPCM_BEGIN` ->
`passo 45: SFX_ADPCM_INIT_OK` -> `passo 46: SFX_ADPCM_DONE code=7` (7/7 sons
codificados) DEPOIS do boot; o botao premido antes de pronto caiu no fallback
do anel (mist=0) sem quebrar nada. 187 checks `ok` na suite de unidades;
o unico FAIL ("chart do tutorial") e' o falso negativo CONHECIDO do dreno do
log (~f=610) -- o chart carrega (provado pelo gameplay harness, PASSO51).
