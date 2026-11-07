# DIAGNOSTICO 0.2.168 — o que os logs da 0.2.167 disseram + instrumento total

## O que o utilizador reportou (0.2.167)
Tela de titulo abriu; travou em alguns momentos; depois "travou de vez" ao fim
de alguns segundos. Pedido explicito: logs que identifiquem ABSOLUTAMENTE tudo
(imagem, codigo, execucao, memoria).

## ANALISE DOS FICHEIROS ENVIADOS (0.2.167)
- Q-lines (FNF_LOG02-3): 227 linhas, frames 0..596+. t_total mediano 16 232 us.
  **Os picos de 53 ms a cada 9-10 quadros CONTINUAM** (27 quadros, in=52-83 ms)
  -- com o portao do BOOT.txt ja' a 10 s. Conclusao: os picos NAO eram do
  BOOT.txt (a atribuicao da 0.2.166 estava errada); a cadencia ~6/s e a
  duracao ~50 ms batem certo com as leituras USB do produtor de audio
  (READ_SIZE 32 KB a 0,7 MB/s = 46 ms por leitura, a cada ~10 quadros).
- O jogo morreu logo apos o quadro 600 -- exatamente onde a 0.2.167 fazia o
  DUPLO despejo (telemetria 115 KB + BOOT.txt) com o produtor de audio a ler
  do mesmo pendrive em paralelo.
- FNF_TRAVA: so' os das builds 0.2.163/0.2.164 -- o vigia NAO escreveu nesta
  sessao (caminho de escrita morto junto com o laco, ou desligamento antes
  dos 3 s).
- FNF_SCREEN_0000.rgba.txt 0 bytes: captura interrompida (resto do caminho
  antigo).
- DIAGNOSTICO-6: os ficheiros que faltavam ao projeto ESTAO no pendrive
  (tutorial_hud_official_320x256_gsalpha.rgba 327 680 B; menu_sfx.pcm 281 088 B).

## CAUSA UNICA DESTA BUILD
**Concorrencia de E/S no pendrive dentro do laco**: o usbhdfsd NAO e'
reentrante (documentado no proprio audio_ring.h) e a 0.2.167 ainda tinha o
produtor de audio a ler 32 KB enquanto o laco/escritores escreviam logs.
Duas manifestacoes medidas: (a) picos de 53 ms (leitura a bloquear os RPCs do
IOP: padGetState/audsrv), (b) travamento de vez (driver a morrer em
leitura+escrita simultaneas, o que tambem mata o vigia).

## CORRECAO (0.2.168)
1. PORTAO DE I/O UNICO e recursivo (io_gate em audio_ring.c): TODA a operacao
   de mass: (produtor, marcas, despejos, vigia, capturas) serializa-se.
2. READ_SIZE 32 KB -> 8 KB: pico de bloqueio do IOP ~46 ms -> ~12 ms.
3. audsrv_queued/available cacheados 1x por quadro: fim de ~14 RPCs/quadro.

## INSTRUMENTO TOTAL (o pedido explicito: "absolutamente tudo")
4. g_marca_actual: a chamada de risco corrente em texto; o FNF_TRAVA diz
   EXATAMENTE onde o jogo morreu (nome da chamada).
5. Registo universal de erros (telemetry_error): toda a falha de
   abertura/leitura/escrita com local+codigo; anel 64 + contagens; sai em
   todos os despejos (ERR,) e no FNF_TRAVA.
6. Miniatura do ecra em RAM (160x112) a cada 30 s: escrita no despejo do vigia
   e de anomalias -- a IMAGEM do momento.
7. Canario da pilha do produtor (128 B pintados, verificados por quadro; ck=).
8. Vigia reforçado: portao recursivo (nao encrava nele proprio), caminho
   alternativo mass:/FNF_TRAVA.txt, erros+marca+canario+miniatura no despejo,
   repeticao a cada 3 s enquanto parado (ja' existia).
9. Q-line alargada: er= (erros acumulados) e ck= (canario).

## ESTADO
- provado: picos de 53 ms a cada ~10 quadros presentes na 0.2.167 (medidos);
  morte da sessao logo apos o duplo despejo do quadro 600 (medido);
  nao-reentrancia do usbhdfsd (documentada no codigo, com casos historicos).
- hipotese: o travamento de vez e' leitura+escrita simultaneas no usbhdfsd;
  a 0.2.168 remove a simultaneidade (portao) e encurta as leituras.
- falta: medir no console (Q-lines novas: in/er/ck e o FNF_TRAVA se morrer);
  os 2 ficheiros do utilizador para as setas cortadas.

Harness ASan 0.2.168: PASSOU 1402 quadros, 2745 pacotes, 0 erros.
ELF md5 ca71e8f305a8b942456c5b369f5d14e8.
