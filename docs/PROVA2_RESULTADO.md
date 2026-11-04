# PROVA 2 — o que o console do usuário respondeu (2026-11-04)

34 testes, 7 vermelhos. **Nenhum dos 7 é defeito de hardware.** Os sete são
limites ou contagens que eu escrevi errado no programa de teste. Este documento
guarda os números medidos, que passam a ser a referência do port.

## O que passou (e vale como fato medido)

| medição | resultado |
|---|---|
| IOP reinicia | `SifIopReset`/`SifIopSync` OK |
| módulos de USB | iomanX, fileXio, usbd, usbhdfsd carregados |
| **pendrive monta** | **1,43 s**, 7 tentativas, 5 entradas na raiz |
| RAM livre | 29,50 MB contíguos em blocos de 512 KB |
| integridade de 8 MB | 0 bytes errados |
| **VRAM do GS** | **16 blocos × 256 KB = 4 194 304 B** (o teto) |
| layout do jogo na VRAM | 483 328 B = **11,5 %**; sobram 3 710 976 B |
| desenho + DMA + FINISH | 0,01 ms |
| vsync | **59,97 Hz** em 60 quadros |
| abrir arquivo | 10 de 10, **2,04 ms por open** |
| 20 leituras com seek | 20 de 20, 61,33 ms por 32 KB |
| escrita 256 KB + releitura | bytes idênticos |
| **16 assets de runtime** | **16 exatos, 0 errados** |
| personagens `.chp4` | 46 de 46 |
| palcos | 21 de 21 |
| charts | 81 arquivos `.chart` |
| `bf.chp4` | magic `CHP4`, 5 766 127 B |
| áudio | LIBSD + audsrv, 48 kHz/16/2, 64 KB tocaram em ~2 ms |
| controle | `padGetState` estável no porto 1 |

**Conclusão dura:** pendrive, USB, GS, VRAM, RAM, áudio, controle e os 16
assets de runtime estão bons. O port não está a ser bloqueado por hardware
nem por falta de ficheiro.

## Os 7 vermelhos, um por um

1. **memory card ausente** — não há cartão no console. O jogo não usa `mc0:`
   para nada obrigatório. Falso alarme.
2. **leitura 32 KB menu_cache — 0,70 MB/s** `(limite que eu pus: 2,0)`
3. **leitura 64 KB menu_cache — 0,70 MB/s**
4. **leitura 32 KB audio_stems — 0,74 MB/s**
5. **leitura 64 KB audio_stems — 0,74 MB/s**

   Os quatro são o **mesmo número**: o USB do PS2 é 1.1 (12 Mbit/s = 1,5 MB/s
   teóricos) e o usbhdfsd entrega **0,70–0,75 MB/s** com blocos de 32 KB e
   também com 64 KB; uma leitura de 8 MB sai a 0,74 MB/s, ou seja, **o tamanho
   do bloco não muda nada** — é o barramento, não o pedido. O limite de 2,0 MB/s
   era fantasia minha e reprovou o próprio hardware. A PROVA 1 tinha o mesmo
   erro, e é por isso que a barra 7 dela era vermelha.

   O que isso **realmente** significa para o jogo: o boot lê ~17 MB antes do
   primeiro quadro → **~24 s**. Não é travamento; é lento. Foi lido como morto.

6. **músicas: 109 subpastas, 27 esperadas** — a pasta `songs/` do pacote tem
   27 pastas de música **e 81 pastas de dificuldade** (`bopeebo-easy`,
   `bopeebo-normal`, …) mais a do tutorial: 109. Minha contagem supunha a
   árvore achatada.
7. **0 `stems.pcm` em 109 pastas** — os stems ficam na pasta da música
   (27), não nas 81 de dificuldade. A prova seguinte, "stems.pcm da 1ª
   música", **abriu 31 334 400 B** com sucesso: os stems estão lá.

## Consequência para o port (0.2.162)

O `BOOT.txt` do jogo parou no passo 33 (`F0_FRAMEBUFFER_SET`), o fim do quadro
0 — e o `FNF_COLHEITA.txt` veio com **0 bytes** enquanto o `BOOT.txt`, que abre
e **fecha** o arquivo a cada marco, veio completo. A diferença é o `close()`: o
usbhdfsd só publica o tamanho do arquivo no FECHA. A telemetria por quadro
inteira, escrita para diagnosticar travamento, nunca chegou ao cartão — o port
estava cego por construção.

A 0.2.162 muda três coisas, todas de diagnóstico:

1. **todo despejo de log faz `open`→`write`→`fsync`→`close`** (telemetry.c);
2. **1 despejo por segundo em vez de 60**, e a lista de desenho só sai no quadro
   em que for pedida (antes: ~20 linhas por quadro, ~180 KB/s de E/S dentro do
   laço de render, num barramento de 0,7 MB/s);
3. **sinal de vida na tela**: 8 quadrados no canto que andam a cada 0,25 s e
   mostram o `ui_state` em binário — se andam, o laço está vivo; se param,
   pararam naquele quadro.  E barra de progresso durante os 5 carregamentos de
   título, para os ~24 s de boot deixarem de ser tela preta.
