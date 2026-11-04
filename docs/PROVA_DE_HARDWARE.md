# "Vai funcionar 100% no meu PS2?" — a resposta honesta e o que eu construí

## 1. A resposta curta: **não existe 100% sem rodar**

Nenhum programa em nenhum console tem garantia de 100 % antes de rodar. O que
existe é **prova por medição**: mede-se a máquina, mede-se o que o jogo precisa,
e compara-se. Se a máquina entrega tudo que o jogo consome, o que sobra é
**defeito de software** — e aí o problema é caçável, porque o jogo passou a
**dizer onde parou** (0.2.160).

O que eu não aceito é a alternativa que existia antes: "testa aí e me conta".
Isso transfere o trabalho para você sem te dar instrumento.

Então construí dois instrumentos:

| instrumento | onde roda | o que responde |
|---|---|---|
| **`tools/cartao_de_prova.py`** | aqui, agora | o jogo **cabe** nos limites do PS2? (RAM, VRAM, DMA, pacote, ELF, E/S por quadro) |
| **`PROVA_PS2.ELF`** | no **seu** PS2 | a **sua** máquina entrega esses limites? (RAM livre, VRAM, USB MB/s, áudio, controle, vsync) |

Um mede o software, o outro mede o hardware. Juntos, eles fecham o caso — e o
que sobrar de dúvida está listado no item 5, sem enfeite.

---

## 2. Cartão de prova do software (medido agora, nesta build)

Gerado por `python3 tools/cartao_de_prova.py` a partir do `FNF_PS2.map` e do
fonte — **números lidos de arquivo, não estimados**.

| item | medido | limite | resultado |
|---|---|---|---|
| RAM estática (.text+.data+.rodata+.bss) | 5 059 544 B (**4,83 MB**) | 32 MB | OK |
| `_end` (onde a heap começa) | `0x005D3558` | — | OK |
| heap livre depois do ELF | 27 445 928 B (**26,17 MB**) | — | OK |
| buffers dinâmicos do jogo | 17,83 MB | 26,17 MB | OK |
| **RAM total do jogo** | **22,65 MB** | 32 MB | OK — sobram **9,35 MB** |
| VRAM do mapa fixo | 1 810 432 B (**1,73 MB**) | 4 194 304 B | OK |
| maior textura | 640 px de largura | 1024 px | OK |
| pacote de desenho | 65 536 B (4096 quadwords) | — | OK |
| DMA por envio | 4096 quadwords | 65 535 (teto do DMAC) | OK |
| **escritas bloqueantes por quadro** | **0** | 0 | OK |
| ELF | 32 bits, machine 8 (MIPS), entry `0x001010C0` | ELF32/MIPS | OK |

**9 itens, 0 fora do limite.**

O item "escritas bloqueantes por quadro" é o que mais importa desta vez: ele
audita o corpo do laço de quadro procurando `open`+`write`+`fsync` no pendrive.
Na 0.2.159 esse número era **13**; na 0.2.160 é **0** (as que sobram são de uma
vez só — boot e transição — ou o marco `FRAME`, limitado a 5 por segundo dentro
do próprio `frame_mark`).

---

## 3. Prova de hardware (`PROVA_PS2.ELF`) — o que fazer

**https://drive.google.com/file/d/1vaozgMmWbnwbbPDs52bqDCxZlhX2CStY/view**

Não substitui nada: é um programa separado, de 1,7 MB, que **só mede**.

1. Copie `PROVA_PS2.ELF` para a **raiz** do pendrive, ao lado do `FNF_PS2.ELF`.
2. Rode pelo seu lançador (o mesmo que usa para o jogo).
3. Ele mede e **para na tela**: uma barra por teste, verde = passou, vermelho =
   falhou. Fundo sem vermelho = tudo passou.
4. Ele grava `FNFPS2\PROVA.txt` (e `PROVA.txt` na raiz e no memory card).

### O que ele mede, e de onde vem cada limite

| teste | mede | limite | por que esse limite |
|---|---|---|---|
| IOP reiniciou | `SifIopReset` + `SifIopSync` em < 2 s | tem que completar | se o reset não volta, o jogo nunca começa (era laço infinito até a 0.2.160) |
| módulos de USB | iomanX, fileXio, usbd, usbhdfsd | todos ≥ 0 | é o caminho do `mass:/` |
| vsync do GS | 60 quadros cronometrados | 55–65 Hz | o jogo conta tempo por quadro |
| VRAM total | aloca 256 KB até encher | ≥ 4 MiB − 64 KB | teto do GS é 4 194 304 B |
| RAM do EE | `memalign` de 512 KB até falhar | ≥ 26 MB | o jogo precisa de 22,65 MB + pilhas |
| banda de RAM | 80 MB copiados | > 100 MB/s | ordem de grandeza de um EE saudável |
| **leitura do pendrive** | abre 5 arquivos, cronometra | **≥ 2,0 MB/s** | abrir uma música lê charts + `stems.pcm` em streaming + os `.chp4` (até ~8 MB). Abaixo disso o início da música passa de 4 s e o anel de áudio não enche |
| áudio | LIBSD + audsrv + fila andando | init 0 e fila ≥ 0 | sem isso é "sem som" |
| controle | `padGetState` estável no porto 1 | — | sem controle não se joga |

Ele também imprime, **antes de qualquer teste**, o que o console diz de si
mesmo: `rom0:ROMVER`, `GetMemorySize()` e `MachineType()`. É a sua BIOS lida do
aparelho, não do adesivo.

---

## 4. Como ler o resultado — a tabela de decisão

| PROVA_PS2 | FNF_PS2 0.2.160 | conclusão |
|---|---|---|
| tudo verde | tela colorida com etapa/código | **o defeito é de software.** A tela vermelha dá etapa + código; com isso eu fecho o caso em uma rodada |
| tudo verde | abre e roda | acabou — a máquina e o jogo estão dentro dos limites |
| algum teste vermelho | qualquer coisa | **o limite é o teste que falhou**, não o jogo. Ex.: USB a 0,8 MB/s → o problema é o pendrive/porta, não o port |
| não abre nada | — | aí é o lançador (no firmware 2.30 não existe FMCB; veja abaixo) |

Detalhe do seu console: ROMVER `0230HC20080220` = **firmware v2.30, compilado
em 2008‑02‑20**, a última revisão da Sony, dos **SCPH‑900xx**. Nessa versão a
Sony desligou o update de OSD — **FMCB e exploits do tipo não funcionam**, só
modchip ou disco de boot. Se o `PROVA_PS2.ELF` não abrir, isso não é o jogo:
é o caminho de boot.

---

## 5. O que **nenhum** desses dois instrumentos prova (risco residual)

Escrito sem eufemismo, porque é isto que separa "medi" de "prometo":

1. **O driver USB no seu pendrive.** O `usbhdfsd` é um driver de 2004–2008
   escrito para pendrives daquela época. Um pendrive de 64 GB USB 3.0 pode
   responder de um jeito que ele não espera. A prova mede MB/s e erros de
   leitura, mas não prova estabilidade em horas de streaming.
2. **A sua TV / modo de vídeo.** O jogo desenha 640×448 e o GS escolhe o modo
   pela ligação. A prova mede o vsync; não mede overscan da sua TV.
3. **O seu lançador.** uLaunchELF, OPL, ESR e modchip inicializam o IOP de
   formas diferentes. A prova reinicia o IOP do mesmo jeito que o jogo — mas
   não no mesmo estado em que o seu lançador deixa a máquina.
4. **Bugs de software que só aparecem em trechos longos.** A prova roda minutos,
   não as 27 músicas. O que existe para isso é o harness de PC (que roda o jogo
   de verdade contra o código real do ps2sdk) e o relatório da 0.2.160.
5. **Defeito conhecido e ainda aberto:** a suíte offline reprova o **orçamento
   de VRAM de 5 dos 21 palcos** (`phillyBlazin` 0.492→0.451, `limoRide`
   0.345→0.321, `mainStage` 0.303→0.278, `phillyStreets` 0.247→0.229,
   `sserafim` 0.141→0.129). Isso **não** é o travamento do boot, mas é real:
   nesses cinco palcos o pool passa a reusar por cima. Conserto =
   reempacotar esses cinco na escala da coluna "usar".

Ou seja: o que eu posso afirmar é **"a máquina e o software cabem, com margem
medida, em todos os limites que dá para medir daqui"**. O que não posso é
afirmar "não existe nenhum bug" — e quem afirmasse isso estaria mentindo.

---

## 6. Se você só quiser rodar uma coisa

Rode o **`PROVA_PS2.ELF`** primeiro. Ele leva menos de 1 minuto, não depende de
nenhum asset do jogo, e o `PROVA.txt` que ele deixa responde de uma vez se o
problema é a máquina, a mídia ou o jogo. Depois rode o jogo: se a tela ficar
colorida com barras, me manda a foto/os arquivos e eu digo exatamente qual é o
passo que falhou.
