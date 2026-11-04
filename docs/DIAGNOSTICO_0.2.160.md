# Por que a tela ficou preta — e o que mudou na 0.2.160

## O que o relato prova

"Barras azuis, depois tela preta e sem som."

Tela preta **e** sem som, juntas, não são um jogo travado por lentidão: são um
processo que **parou**. E no código da 0.2.159 havia **cinco** caminhos de boot
que faziam exatamente isso — chamavam `SleepThread()` e paravam para sempre,
sem desenhar nada:

| passo | caminho | quando dispara |
|---|---|---|
| 1 | `load_metadata()` < 0 | qualquer asset de runtime faltando/curto (font, menu_cache, menu_sfx) |
| 2 | `init_pad()` < 0 | SIO2MAN/PADMAN/padInit não responderam |
| 3 | `CACHE_ALLOC_FAIL` | `memalign` dos 1,6 MB do menu falhou |
| 4 | `CACHE_RLE_FAIL` | `menu_cache.rle` não abriu |
| 5 | `CACHE_SFX_FAIL` | `menu_sfx.pcm` não abriu |

Nenhum deles desenha, nenhum escreve o motivo, nenhum toca. Do lado de fora,
"tela preta e sem som" é isso.

## O segundo defeito medido: enxurrada de escritas no pendrive

`capture_status("FRAME", ...)` rodava em **todo** quadro enquanto
`frame_no < 300`. Cada chamada é `open`+`write`+`fsync`+`close` em
`mass:/FNFPS2/BOOT.txt` — uma RPC síncrona para o IOP, que escreve no USB:

```
~300 quadros × 1 escrita bloqueante  =  ~300 escritas nos primeiros 5 s
```

O próprio projeto já tinha medido esse padrão: é o que deixou a **0.2.108**
sem abrir no console (~20 escritas bloqueantes por quadro). A 0.2.159
reintroduziu uma versão menor dele, justamente na janela em que o áudio do
título está enchendo o anel.

## O terceiro: o reset do IOP era um laço infinito

```c
while(!SifIopReset("rom0:UDNL rom0:EELOADCNF",0));   /* para sempre */
```

Se o console recusar o reset, o jogo nunca sai da linha. Sem imagem, sem som,
sem log.

## O que a 0.2.160 faz

1. **Nenhuma falha fatal sai calada.** Os cinco `SleepThread()` viraram
   `boot_fail()`: pinta a tela (só retângulos do GS — não depende de fonte,
   textura ou de o pendrive ter aberto) e grava `FNFPS2/FNF_ERRO.txt`.
2. **A tela diz onde parou.** Fundo vermelho = fatal, amarelo = aviso; barras
   brancas no topo = número da etapa; 8 quadrados no meio = código em binário
   (bit 0 à esquerda); barra de baixo repete a etapa.
3. **O IOP tem limite**: 100 tentativas de 20 ms. Se falhar, a tela diz.
4. **Fim da enxurrada**: um marco de quadro a cada 200 ms, não um por quadro.
5. **Áudio que falha avisa**: se o `audsrv` não subir, aparece um aviso amarelo
   de 3 s e o jogo segue sem som (antes era indistinguível de "não abriu").
6. **Retrato do console** em todo boot: `FNFPS2/DIAGNOSTICO.txt` com o conteúdo
   de `rom0:ROMVER` (a BIOS de verdade, lida do console), `GetMemorySize()`,
   `MachineType()` e o tamanho medido de cada asset de runtime.

## O que pedir de volta

Do pendrive, três arquivos pequenos:

```
FNFPS2\FNF_ERRO.txt        (se existir — diz a etapa e o código)
FNFPS2\DIAGNOSTICO.txt     (a BIOS, a RAM e os tamanhos medidos)
FNFPS2\BOOT.txt            (última linha = último passo que completou)
```

Com o `BOOT.txt` da rodada que já aconteceu dá para dizer a etapa **sem**
rodar nada de novo: a última linha é o último marco que o jogo alcançou.

## Defeito medido à parte: 5 palcos acima do orçamento de VRAM

A suíte offline (`./build.sh --tests`) reprova o orçamento de 5 dos 21 palcos
com os assets reais:

```
limoRide       0.345 -> usar 0.321
mainStage      0.303 -> usar 0.278
phillyBlazin   0.492 -> usar 0.451
phillyStreets  0.247 -> usar 0.229
sserafim       0.141 -> usar 0.129
```

Isso não é o travamento do boot (o jogo morre antes de qualquer palco), mas é
um defeito real e medido: nesses cinco palcos a soma das texturas passa do
orçamento e o pool passa a despejar/reusar por cima. O conserto é reempacotar
esses cinco na escala da coluna "usar" — fica para a próxima build, com os
assets à mão.
