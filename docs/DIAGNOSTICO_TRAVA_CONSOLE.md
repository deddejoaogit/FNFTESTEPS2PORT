# Travamento no console — o que o BOOT.txt do usuário prova

Build no pendrive: `0.2.114`. Sessão de 2026-10-27, SCPH do usuário.

## Fatos medidos (não interpretação)

| Arquivo | Tamanho | O que isso prova |
|---|---|---|
| `BOOT.txt` | 23 B, uma linha: `passo 48: FRAME code=0` | o jogo entrou no laço de quadros e a última coisa que ele registrou foi o marco `FRAME`, que é escrito no topo de cada quadro enquanto `frame_no < 300` |
| `FNF_COLHEITA.txt` | 0 B | a telemetria não escreveu **nada** na sessão inteira |
| `FNF_SCREEN_0000.rgba.txt` | 0 B | o arquivo é criado com `O_TRUNC` na linha logo depois de `telemetry_init()`, então essa linha rodou; nenhuma captura foi feita |

### `passo 48` é o quadro ~15

`boot_mark()` numera cada chamada de `capture_status()`. Contando os marcos que
uma inicialização normal emite **antes** do laço e **dentro** do quadro 0
(`META_BEGIN` … `F0_FRAMEBUFFER_SET`, mais `DEFERRED_AUDIO_BEGIN` no quadro 3),
chega-se a ~33 marcos. Depois disso, cada quadro na tela de título emite
exatamente um: `FRAME`.

```
passo 48  ≈  quadro 15
```

Ou seja: **o travamento está dentro do corpo de um quadro, na tela de título
(`ui_state == 0`), por volta do quadro 15** — cerca de meio segundo de jogo, o
que bate com o relato ("apareceu a primeira tela, a música começou, ficou lento
e travou").

### O que está descartado por esses dados

- não é o boot (chegou ao laço);
- não é o tutorial, o story nem a troca de estado (todos emitem marcos próprios,
  e nenhum aparece);
- não é falta de asset no boot (os marcos de leitura de asset passaram);
- não é o `SleepThread()` de erro do boot.

### O que NÃO se sabe

Qual sub-passo do quadro travou. O `FRAME` é o **primeiro** marco do quadro:
entre ele e o próximo `FRAME` do quadro seguinte não existe nenhum marco em
`ui_state 0`. O `BOOT.txt` diz "quadro ~15", não diz "dentro de qual chamada".

E o `FNF_COLHEITA.txt` com 0 bytes tira a segunda fonte de dados. Duas causas
cabem nisso e o arquivo sozinho não decide:

- **(a)** o log nunca abriu (`log_fd < 0`): `telemetry_init()` devolve < 0 e o
  jogo segue mesmo assim. O arquivo de 0 byte seria de uma sessão anterior;
- **(b)** o log abriu e a primeira escrita falhou: `flush_output()` marca
  `output_error = 1` e **tudo** é descartado a partir dali, por projeto.

## Versões

- **0.2.116-diagnostico** é a build do diagnóstico deste documento. O código
  dela está no commit `9047630` do ramo `arena/9d71e796-fnftesteps2port`; se
  precisar regerar exatamente aquele ELF, é `git checkout 9047630 -- .` e
  `make`.
- Da 0.2.117 em diante o jogo tem o mapa de VRAM conferido e o framebuffer de
  16 bits (ver `PLANO_COMPLETO.md`, Fase 4). **Nada do diagnóstico mudou**:
  os mesmos `capture_status`, o mesmo `CSR`/`VIF1_STAT`, os mesmos marcos de
  quadro. Um log de 0.2.117 serve para o mesmo diagnóstico que um de 0.2.116.

## Plano da build 0.2.116-diagnostico (só coleta, nenhum conserto)

Regra 4 do projeto: sem dado não se conserta, coleta-se. Nada de evento do jogo
é removido, encurtado ou reordenado; nada de arte muda; nenhuma espera de
hardware fica maior.

### 1. Marco fino dentro do quadro, no escritor que já provou funcionar

O único escritor que comprovadamente escreveu no console foi o do `BOOT.txt`
(open → write → fsync → close, um arquivo por chamada). O `frame_mark()` novo
usa o **mesmo caminho** e grava uma linha com o sub-passo, o quadro e o estado de
áudio:

```
passo <n>: F.<subpasso> code=<c> f=<quadro> q=<audsrv_queued> ring=<anel>
           und=<underruns> sub=<submit_errors> zero=<zero_events>
```

Sub-passos marcados, na ordem em que o código os executa:

```
F.IN           entrada lida, botões
F.AUDIO        envio de áudio do menu (o laço da linha 1384)
F.CLOCK        relógio da música
F.TITLE        intro do título / recarga de GF / confirmação
F.DECODE       decodificação incremental do menu_cache
F.PREP         preparação de story/tutorial
F.STATE        máquina de estados e transições
F.ANIM         animações dos personagens
F.BG           fundo do menu (estado 6), pacote e DMA próprios
F.PACKET       montagem do pacote do quadro
F.DMA          envio do pacote e espera do desenho
F.VSYNC        espera do vsync
F.POST         telemetria e fim do quadro
```

**Custo limitado por construção, não por estimativa:** no máximo **1 escrita a
cada 200 ms** (5 por segundo), com `fsync` em uma a cada quatro. A 0.2.108
morreu com ~20 escritas por quadro (~1200 por segundo); isto é 5 por segundo.
O quadro inteiro pode ser rápido; o que fica no arquivo é o último sub-passo
que passou pelo portão de 200 ms.

### 2. Cão de guarda: a posição exata, mesmo em travamento duro

Os marcos ficam também num anel de RAM com as últimas 16 posições, sem
portão nenhum. Uma thread separada (prioridade baixa) conta quadros; se o
contador **não mudar em 8 acordadas de 200 ms (1,6 s)**, ela grava as 16
posições + contadores em `mass:/FNFPS2/FNF_TRAVA.txt` e faz `fsync`.

- **no máximo 3 gravações** na sessão inteira, espaçadas de 3 s;
- antes de gravar, chama `audio_ring_pause(1)` (espera limitada a 2 s) para o
  produtor de áudio parar de tocar o USB: o `usbhdfsd` não é reentrante;
- se a thread de áudio estiver morta, `audio_ring_pause` desiste em 2 s e a
  gravação acontece mesmo assim — é o último recurso de diagnóstico;
- custo no caminho normal: **zero escrita**; só uma thread dormindo.

É o único jeito de saber onde o processo principal parou quando ele para de
responder. Sem isso, o travamento continua mudo.

### 3. Resultado da telemetria registrado no BOOT.txt

Logo depois de `telemetry_init()`, um marco com o valor devolvido. Isso separa
(a) de (b) na próxima sessão: se vier negativo, o log nunca abriu e o 0 byte
está explicado; se vier 0, o log abriu e o problema é a escrita.

### 4. Laço de áudio do menu com limite (defeito provado por leitura)

`src/main.c:1384`:

```c
while(audsrv_available()>=AUDIO_READ_CHUNK){
    int n=audio_ring_take(...);
    if(n>0){ ... int j=audio_play_complete(...); if(j>0)...; }
    else break;
}
```

Não tem guarda nem saída por erro. `audio_play_complete()` desiste depois de
2000 × 1 ms = **2 s**. Se `audsrv_play_audio()` parar de aceitar bytes
devolvendo 0 ou erro, `audsrv_available()` não diminui, o laço **nunca sai** e
cada volta custa até 2 s: é exatamente "ficou lentíssimo e travou" na tela de
título (`ui_state 0`, o mesmo estado do `BOOT.txt`).

Correção: guarda de 64 voltas + saída em `j <= 0`, com marco `AUDIO_STALL`
trazendo os contadores. Se for essa a causa, o próximo `BOOT.txt` diz isso com
todas as letras. Se não for, o comportamento saudável não muda: quando
`audio_play_audio` aceita os bytes, o laço sai pela mesma condição de antes.

Isto **não é conserto de causa** — é tornar a falha visível e limitada. A causa
(por que o driver pararia de aceitar) só se decide com o dado.

## Resultados medidos desta build (0.2.116-diagnostico)

| Verificação | Resultado |
|---|---|
| compilação com o toolchain do ps2sdk | **0 avisos, 0 erros**; ELF 5 278 492 B |
| `./build.sh --tests` | `all suites passed` (16 s) |
| harness, 1400 quadros, roteiro título → menu → story → tutorial | **PASSOU**: 1401 quadros, 4906 pacotes, 0 estouros, 0 escritas fora do limite, todo asset abriu |
| marcas dentro do quadro realmente gravadas | provado com o processo morto no meio (`SIGKILL` aos 5 s): `BOOT.txt` ficou com `# despejo 6368 marcas f=31835 …` e as 16 fases em ordem (`F.VSYNC`, `F.POST`, `F.IN`, …) |
| fases distintas por despejo | 13 por quadro (era 1 única, sempre `F.IN`, na primeira versão — defeito encontrado e corrigido no teste) |
| custo por despejo | **um** `open` + **um** `write` de ~1 KB + **um** `close`; `fsync` em 1 de cada 4 despejos |
| custo máximo no console | **5 despejos/s** (~5 KB/s), atraso máximo entre o fato e o cartão: 800 ms |
| falha de escrita | desliga o diagnóstico na primeira falha (`g_fn_write_ok=0`), como manda a regra 7 |
| cão de guarda no caminho normal | **nenhuma escrita**; só uma thread dormindo 200 ms por vez; teto de 20 min, 3 despejos |

O `FNF_TRAVA.txt` aparece nas execuções do **harness** e isso é esperado: lá o
`DelayThread()` é uma função vazia, então as 15 acordadas de 200 ms acontecem em
microssegundos. No console cada acordada dorme de verdade, e o critério passa a
ser "3 s sem o quadro avançar". Nenhum teste depende desse arquivo.

## Verificação desta build

1. compila sem um único aviso, com o toolchain do ps2sdk;
2. suíte offline completa (mesmos 1401 quadros, ASan/UBSan limpos);
3. harness de 1400 quadros atravessando título → menu → story → tutorial, para
   provar que nada regrediu e que as marcas não quebram o caminho;
4. contagem de escritas do `frame_mark` conferida por código (limite por
   construção) e por execução no harness (`strace -c -e trace=write` sobre o
   arquivo de marco, se disponível);
5. os arquivos gerados no harness são inspecionados: a última linha antes do
   fim tem de ser `F.POST`.

## O que esta build não faz

Não corrige o travamento. Ela existe para que a próxima sessão no console
responda: **qual sub-passo, em qual quadro, com qual estado de áudio**. Com isso
o conserto deixa de ser hipótese.
