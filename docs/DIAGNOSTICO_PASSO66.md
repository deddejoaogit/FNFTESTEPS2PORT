# Diagnóstico do travamento no passo 66 (`PASSO 66: FRAME`)

Situação medida, do enunciado e do código do port (0.2.112):

| fato | valor |
|---|---|
| último marco gravado no console | `passo 66: FRAME` |
| mesmo código no PC | mais de 1400 quadros, 0 estouros de pacote, 0 escritas em zona de guarda, 0 envios acima do limite do DMA |
| reservas de VRAM no boot | 6, todas na inicialização |
| chamadas de liberação de VRAM no projeto | **zero** |
| soma dos 8 buffers | 4.169.728 de 4.194.304 B = **99,4 %** |
| sobra declarada no próprio código | 163.840 B |
| pacotes gravados dentro do executável (`.incbin`) | 2.929.792 B |

## 1. O que "passa no PC" **não** prova

O banco de ensaio do PC substitui hardware por modelos. Ele imita bem a
aritmética (conductor, bop, easings, chart) e não imita nada que dependa de
estado do silício:

* **VRAM é um modelo, não um limite.** No PC um "upload de textura" é um
  `memcpy`. Nada impede o envio de passar do fim da VRAM. No console, um
  transferência GIF/DMA que cruza o topo da VRAM (ou que invade a área do
  framebuffer) trava o caminho de DMA, e aí **`graph_wait_vsync()` não
  retorna mais**. O sintoma no console é exatamente o observado: imagem
  aparece, texto aparece, e congela no primeiro quadro em que algo novo é
  enviado.
* **Pendrive.** O `usbhdfsd` não é reentrante e uma leitura pode não
  retornar se o driver for pego no meio de outra operação. O projeto já
  perdeu dois ciclos com isso (0.2.64 e 0.2.74). Uma `read()` de `mass:/`
  que trava deixa o jogo parado **sem contador de tempo**, e o último
  marco gravado continua sendo o anterior.
* **Áudio.** `audsrv_queued()`/`audsrv_available()` dependem do SPU2 e do
  IOP. Se o `audsrv` não subir, os laços de bombeamento esperam por espaço
  que nunca aparece.
* **Sincronismo de tela.** O `vsync` do GS vem do PLL; nada no PC o imita.

## 2. Hipótese principal, e por que ela é a mais provável

**O primeiro quadro desenha uma textura cujo bloco de VRAM termina fora da
VRAM, e o GS/DMA trava no `graph_wait_vsync` do quadro seguinte.**

Os números apontam para isso, não para um palpite:

1. Os 8 buffers somam 4.169.728 B. Sobram 163.840 B — e **o próprio
   `main.c` diz isso num comentário**.
2. Não existe uma única chamada de liberação no projeto. Então a soma dos
   buffers reservados no boot é o orçamento do jogo inteiro, para sempre.
3. Os pacotes `.incbin` (2.929.792 B) são carregados na inicialização e
   nunca saem, mesmo servindo só ao título e à história.
4. Uma textura de personagem em 640×448/32 bpp, por exemplo
   `tutorial_hud_official_320x256`, consome 320×256×4 = 327.680 B. Não
   cabe nos 163.840 B restantes. O desenho "funciona" no PC porque lá não
   há limite; no console o envio passa do endereço máximo.
5. O texto que aparece antes de congelar é o último desenho que coube na
   área válida. O congelamento é o primeiro envio que não coube.

Isto não é apresentado como certeza. É a hipótese que **explica os três
fatos ao mesmo tempo** (funciona no PC, trava no console, trava depois de
desenhar algo) e é a única que a medição de VRAM do próprio projeto
sustenta. As outras classes (pendrive, áudio) não explicam por que o
travamento acontece *depois do primeiro desenho*.

## 3. Como confirmar sem chutar (o instrumento)

`src/engine/bootlog.{c,h}` aumenta a resolução do instrumento: além do
número do passo, grava a **fase dentro do quadro** (PAD, AUDIO_CAUDAL,
AUDIO_ENVIO, AUDIO_LEITURA, CHART, ESTADO, DESENHO, CAPTURA, VSYNC,
TROCA_FB, TELEMETRIA). Com ele a pergunta "morreu onde?" passa a ter
resposta de uma linha por execução.

Ponto de integração no `main.c` (não é reescrita, são 6 linhas):

```c
#include "engine/bootlog.h"

static void log_para_arquivo(void *ctx, const char *linha) {
    int fd = (int)(long)ctx;
    write(fd, linha, strlen(linha));   /* iomanX do ps2sdk */
    write(fd, "\n", 1);
}

/* no começo da main() */
int logfd = open("mass:/FNFPS2/boot.log", O_WRONLY | O_CREAT | O_TRUNC);
bootlog_init(&g_boot, log_para_arquivo, (void *)(long)logfd, 1);
bootlog_marco(&g_boot, "FRAME", 0);          /* o marco 66 que já existe */

/* dentro do laço, em volta de cada etapa */
bootlog_quadro_inicio(&g_boot, frame_no);
bootlog_fase(&g_boot, BF_PAD);       padPortRead(...);
bootlog_fase(&g_boot, BF_AUDIO_ENVIO); ... audsrv_play_audio(...);
bootlog_fase(&g_boot, BF_DESENHO);   ... draw_rect_textured(...);
bootlog_fase(&g_boot, BF_VSYNC);     graph_wait_vsync();
bootlog_fase(&g_boot, BF_TROCA_FB);  graph_set_framebuffer_filtered(...);
bootlog_quadro_fim(&g_boot, ms_gasto);
```

Cada troca de fase é **escrita na hora**, em um arquivo pequeno. Um
travamento duro no meio do quadro deixa a última fase no pendrive. O
`bootlog_analisa_anterior()` detecta que o log anterior não tem
`MARCO=SHUTDOWN` e informa `CICLO_SEM_FIM`, o que separa "travou" de
"saiu limpando".

Como ler o resultado:

| última fase gravada | quem travou | próximo passo |
|---|---|---|
| `VSYNC` | GS/DMA. Bate com a hipótese da VRAM. | aplicar `asset_bank`, medir de novo |
| `DESENHO` | envio de textura (mesma família) | `vc_relatar()` no ponto do desenho |
| `AUDIO_ENVIO` | audsrv/SPU2 | `audsrv_queued()` no alerta |
| `AUDIO_LEITURA` | pendrive (`usbhdfsd`) | `io_fila` + fatias de 32 KB |
| `PAD` | libpad/`padPortOpen` | checar `padPortOpen` antes do laço |
| `INICIO` repetido no mesmo quadro | laço infinito fora das fases | `bootlog_alerta` por etapa |

## 4. A correção que torna o defeito impossível

Não é só remendar o valor do orçamento: a classe de defeito tem de sumir
do código.

1. **`vram_cache` ganhou guarda de limite.** Toda alocação é conferida
   contra `[base, base+tamanho]` **antes** do envio, e uma alocação
   inválida é recusada e contada em `falhas`. Teste:
   `tests/test_vram_cache.c` ("guarda de limite").
2. **`asset_bank` substitui as 8 variáveis globais.** Quem desenha pede
   por chave; o banco decide onde colocar, com despejo automático do menos
   usado. Nada é reservado "para sempre" na inicialização.
3. **Saída de cena libera de verdade.** `ab_cena_sair()` devolve a região
   inteira (o teste prova `vc_livre() == 1.900.544` depois de 200 trocas
   de cena).
4. **Fora o `.incbin`.** `title_packs.S` e `story_packs.S` somam
   2.929.792 B dentro do executável para servir a 2 das 4 telas. O pacote
   passa a morar no pendrive e a entrar/sair com a tela, como os pacotes
   do tutorial já fazem.

## 5. O que continua sem garantia

Nenhum teste no PC cobre travamento de pendrive, áudio ou vsync. A
garantia que existe é outra: **o log diz onde parou, e o alocador recusa
por construção o tipo de envio que trava o DMA**. Se após a troca o
console ainda travar, o `boot.log` vai dizer em qual fase, e aí a próxima
hipótese é medida, não adivinhada.
