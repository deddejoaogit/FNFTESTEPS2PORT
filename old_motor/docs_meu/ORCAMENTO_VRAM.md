# Orçamento de VRAM — números, contas e o que cada um decide

Tudo aqui é conta fechada com as dimensões reais do GS e do projeto. Cada
número tem a fórmula ao lado para poder ser recontado.

## 1. Quanto existe

```
VRAM total do GS ......................... 4.194.304 B
dois framebuffers 640x448 PSMCT32 ........ 2.293.760 B  (640*448*4*2)
sobra para textura ....................... 1.900.544 B
```

O `vram_cache` aloca em blocos de 256 B (`VC_ALINHAMENTO`), porque o TBP0
do GS anda de 256 em 256.

## 2. Quanto cada cena paga de pedágio

Medido na fase 0 do projeto e confirmado no teste
`tests/test_asset_bank.c`:

```
HUD ......................................   294.912 B
fonte ....................................   327.680 B
cenário ..................................   524.288 B
                                          -----------
toda cena paga ........................... 1.146.880 B

sobra por cena ...........................   753.664 B   (1.900.544 - 1.146.880)
```

É esse o número que importa. **Não** são 1.900.544 B: 60 % da região já
está comprometida antes de qualquer personagem.

## 3. Contagem de texturas vivas (medida nos palcos)

| palco | texturas vivas |
|---|---:|
| tankmanBattlefield | 31 |
| phillyStreetsErect, sserafim | 28 |
| phillyStreets | 27 |
| mainStage | 16 |
| spookyMansion | 14 |
| **média** | **21** |

O port tem 5 texturas fixas (`tex`, `tex2`, `gftex`, `stagetex`,
`fonttex`). Não é problema de tamanho, é de **quantidade de vagas** — e é
por isso que `vram_cache` substitui os slots em vez de só encolher as
imagens.

## 4. Orçamento por textura viva (o que a preparação tem de entregar)

`753.664 / n`, e o lado equivalente em px numa folha 16 bpp:

| palco | texturas | B por textura | folha equivalente (16 bpp) |
|---|---:|---:|---|
| tankmanBattlefield | 31 | 24.311 | ~110 × 110 px |
| phillyStreetsErect | 28 | 26.916 | ~116 × 116 px |
| phillyStreets | 27 | 27.913 | ~118 × 118 px |
| mainStage | 16 | 47.104 | ~153 × 153 px |
| spookyMansion | 14 | 53.833 | ~164 × 164 px |

Gerado por `tools/prep_assets.py --orcamento`. Estes são os números que a
redução de imagem tem de respeitar — e são a razão pela qual a resolução
cai tanto: o pior palco tem 31 texturas em 753 KB.

## 5. A opção de tela menor

```
hoje   640x448 PSMCT32 ....... framebuffers 2.293.760 B, sobra 1.900.544 B
alternativa 512x384 PSMCT16 ... framebuffers   786.432 B, sobra 3.407.872 B
```

`3.407.872 / 1.900.544 = 1,79×` mais espaço para textura (a conta que o
enunciado chama de "quase o dobro"). Descontando o mesmo pedágio de
1.146.880 B por cena:

```
sobra por cena hoje ......................   753.664 B
sobra por cena com 512x384 PSMCT16 ....... 2.260.992 B   (3,0x)
```

É a alavanca mais barata do projeto: nenhum evento muda, nenhum
comportamento muda, só o tamanho e a profundidade do framebuffer.

## 6. Por que as folhas não podem ir como estão

Medidas reais de folhas do jogo:

| folha | dimensões | VRAM em 32 bpp |
|---|---|---|
| `limo/erect/limoDrive` | 4096 × 4096 | 67.108.864 B (16× a VRAM) |
| `christmas/erect/upperBop` | 4096 × 2048 | 33.554.432 B |
| `sserafim/floor` | 5362 × 754 | 16.174.592 B (6 pedaços de 1024 px) |
| `tank0`…`tank5` | 1024 × 1024 cada | 4.194.304 B cada |

Um palco inteiro (limoRideErect) pede **1.619.192.832 B** em resolução
nativa: 386× a VRAM total. Não existe compressão que resolva isso; o que
resolve é **fatiar em quadros e reduzir cada quadro**.

Além disso há o limite de largura do GS: **1024 px**. `sserafim/floor`
(5362 px) e as folhas de 4096 px precisam ser quebradas em pedaços; o
renderizador tem de saber em qual pedaço está cada quadro. Isso está
implementado em `empacotar_folha()` e `vram_bytes()`.

## 7. O que a preparação faz, medido

`tools/prep_assets.py --selftest` (roda no PC, sem console):

```
folha sintética 4096x4096 com 42 quadros de 512x512 (caso limoDrive)
  cada quadro nativo ................ 1.048.576 B
  folha nativa ...................... 44.040.192 B  (10x a VRAM toda)
  fatiada em 42 quadros ............. nenhum quadro descartado
  reduzida para 179x179 (escala 0.35)
  folha empacotada .................. 900 x 1619 = 3.315.712 B  (4,4x o orçamento)
  maior animação (14 quadros) ....... 900 x  539 = 1.103.872 B
  escala máxima que cabe ............ 0.281  (14 quadros de 512x512, 16 bpp)
```

Três conclusões medidas, nesta ordem:

1. **A folha inteira de um personagem não cabe** no orçamento por cena,
   nem reduzida a 179 px. Isso não é erro do conversor: é o motivo de o
   port atual ter 5 vagas e só jogar o tutorial.
2. **Residir por animação funciona.** O jogo usa uma faixa de cada vez
   (`idle`, `singLEFT`…), e o evento `PlayAnimation` do chart é justamente
   o que diz qual faixa tocar. Guardar por animação derruba 3.315.712 B
   para 1.103.872 B no mesmo dado.
3. **Com 16 bpp e residência por animação, a escala que cabe é ~0,28.**
   A imagem fica pior na proporção que o orçamento exigir. Nenhum quadro,
   nenhuma nota e nenhum evento é removido nesse processo.

## 8. O pacote que sai daqui

`tools/prep_assets.py` grava o mesmo container que o `main.c` já sabe ler
(`title_lz4_decode`, magic `CFZ4`, versão 1, contagem, bytes do quadro,
tabela `(offset,tamanho)`, blocos LZ4 crus). O teste embutido verifica que
o pacote volta **byte a byte idêntico** ao que entrou, e mede a
compressão: 11.398 B de arquivo para 2.691.444 B de pixels no caso
sintético (0,4 %).

LZ4 não é "magia de memória": ele reduz o **arquivo no pendrive**, não a
VRAM. Quem reduz a VRAM é o fatiamento, a escala e a profundidade de cor.
