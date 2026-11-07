# CORREÇÕES — FNF PS2 (actualizado na 0.2.178)

Documento pedido pelo utilizador: **absolutamente tudo o que precisa de ser
corrigido**, incluindo TODAS as correções que ele cita. Cada entrada: número,
o que está errado, quem citou (UTILIZADOR / MEDIDO / CONHECIMENTO), onde está,
estado.

Convenções: `[ ]` aberta · `[x]` corrigida (diz a build) · `[~]` parcialmente.

---

## A. CORREÇÕES CITADAS PELO UTILIZADOR

### #1 [ ] SFX do clique chega TARDE (0.2.177, UTILIZADOR)
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

### #4 [ ] Textos SIMPLES (sem fonte especial) BUGADOS no story menu (0.2.177, UTILIZADOR)
"Os textos simples, sem fonte especial, estavam todos bugados" — os glifos do
caminho de texto simples saem corrompidos/errados no ecrã de story mode. Os
textos com fonte especial (títulos) parecem OK. Rever o desenho dos glifos
simples (font.rgba/font.widths → o caminho de texto simples) no contexto do
story menu (a scissor/band do story title pode interferir).

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
3. Operações FAT (criar/estender ficheiros) são frágeis depois de minutos de
   leitura em fluxo: PRÉ-CRIAR no boot; depois só APPEND.
4. `PollSema` é proibido em EE (decrementa sempre) — só WaitSema/SignalSema.
5. O `fsync` repetido mata o usbhdfsd (só no TRAVA).
6. Toda a afirmação = medida; toda a build = uma causa única.
7. A pintura da TV (GS puro) é o último canal quando o USB morre.

---
*Novas correções do utilizador = nova entrada em A. Actualizar os estados.*
