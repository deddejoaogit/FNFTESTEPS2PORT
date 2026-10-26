# Orçamento de imagem: o que a medição mostrou

Este documento substitui os números anteriores sobre VRAM. Todos os valores
abaixo foram **medidos**, não estimados, e cada um pode ser reproduzido pelos
comandos citados.

## 1. O tamanho real da arte

Fonte: clone do repositório oficial `FunkinCrew/funkin.assets`
(`preload/`, `shared/`, `week1..7/`, `events/`, `sserafim/`).

    python3 tools/measure_assets.py --assets <clone>

Resultado, sem lacunas:

| item | valor |
|---|---|
| stems de imagem medidos | 176 (135 props de palco + 46 personagens) |
| lacunas | **0** |
| total a 32 bpp | **2 008 108 660 B** |
| total a 16 bpp | **1 004 054 330 B** |
| VRAM total do GS | 4 194 304 B |
| razão | **≈ 480× a VRAM** |

Duas descobertas mudaram o modelo:

1. **Personagens e muitos props são Animate Atlas em pasta**, não Sparrow:
   `shared/images/characters/bf-car/` contém `Animation.json`,
   `spritemap1.json` e `spritemap1.png`. Um personagem pode ter **vários**
   spritemaps (`renderType: multianimateatlas`), e a VRAM paga por todos.
2. **Os atlas são enormes**: `gfTankmen.png` é **8192×4096** (134 MB a 32 bpp),
   `limoDrive.png` e `smokeRight.png` são 4096×4096 (67 MB cada). Um único
   atlas já é dezenas de vezes a VRAM total.

Ou seja: *nenhuma* arrumação de "carregar o atlas" funciona. O modelo tem de ser
residência por quadro.

## 2. O modelo que já funciona no código

`src/main.c` já resolve isso para o tutorial, e é o modelo adotado para tudo:

```
tutorial_pack_load()   -> mantém o pacote CFZ4/LZ4 inteiro na RAM principal
tutorial_pack_frame()  -> descomprime UM quadro para o buffer de estágio
upload_raw()           -> sobe esse único quadro para a VRAM
```

Formato, lido do decodificador (não inventado):

```
"CFZ4", u32 versao=1, u32 count, u32 frame_bytes
count * (u32 offset, u32 size)
blocos: size bytes cada, um bloco LZ4 que descomprime exatamente frame_bytes
```

`frame_bytes` = `w*h*4` (PSMCT32) ou `w*h*2` (PSMCT16), **fixo para o pacote
inteiro** — logo o canvas é o maior quadro da animação.

## 3. O empacotador novo

    python3 tools/pacote_personagem.py --assets <clone> --characters bf,gf,dad \
        --bpp 16 --budget 393216

Cada pacote é decodificado de volta e conferido quadro a quadro (`--verify`).

### Personagens: resolução NATIVA a 16 bpp

| personagem | quadros | canvas | escala | bpp | VRAM/quadro | pacote |
|---|---|---|---|---|---|---|
| bf | 46 | 431×456 | 0.990 | 16 | 393 072 B | 594 897 B |
| gf | 45 | 513×369 | **1.000** | 16 | 378 594 B | 236 560 B |
| dad | 15 | 368×418 | **1.000** | 16 | 307 648 B | 189 170 B |

**Três personagens, um quadro cada: 1 087 312 B.** Não há perda de geometria —
só a profundidade de cor cai de 32 para 16 bpp (RGBA5551), que é a degradação
que o projeto autoriza.

Para comparação, o mesmo pedido a 32 bpp: 428 656 B no total, mas a escala cai
para ~0,42 — pior fidelidade **e** mais memória no fim das contas, porque o
orçamento é em bytes.

## 4. Orçamento por palco

    python3 tools/vram_budget.py

```
VRAM                                     4 194 304 B
2 framebuffers 512x384 PSMCT16            786 432 B
região de textura                       3 407 872 B
  fixas (fonte + HUD + notas)             622 592 B
  3 personagens em tamanho nativo       1 087 312 B
  sobra para o palco                    1 697 968 B
```

19 dos 21 palcos precisam de redução para caber nessa sobra. Os extremos:

| palco | escala | observação |
|---|---|---|
| schoolEvil | **1.000** | cabe nativo (56 KB) |
| schoolEvilErect | **1.000** | cabe nativo |
| phillyBlazin | 0.648 | 3 props |
| spookyMansion | 0.400 | 1 prop de 5,3 Mpx |
| school / schoolErect | 0.376 | 8 props, o maior com 5,5 Mpx |
| tankmanBattlefield | 0.334 | 17 props |
| mainStageErect | 0.329 | 9 props |
| phillyStreets | 0.260 | 12 props |
| sserafim | **0.145** | pior caso; 13 props, o maior com 7,6 Mpx |

A escala é **uniforme por palco**: posições, `zIndex`, `scale` e
`scrollFactor` de cada prop continuam os do JSON original. Só o pixel encolhe.
É a troca que o projeto autoriza explicitamente: fidelidade de imagem é
negociável, fidelidade de jogo não é.

## 5. O que isto NÃO prova

- Nenhum destes pacotes foi ainda **carregado pelo jogo no console**. O
  empacotador e o decodificador concordam byte a byte (verificado), e o formato
  é o mesmo que o tutorial já usa, mas isso é concordância local, não hardware.
- O caminho de desenho do jogo hoje **não** tem um carregador genérico de
  personagem/palco por música. `bf_frames.lz4f` e `gf_frames.lz4f` são
  carregados por nome fixo. Ligar os 46 personagens e 21 palcos exige esse
  carregador genérico, que ainda não existe.
- O palco a 15% (sserafim) é geometricamente correto e visualmente ruim. Se o
  resultado for inaceitável, a alternativa medida é compor props estáticos num
  único fundo — o que só é possível para props com o mesmo `scrollFactor`, e
  ainda não foi testado.

## 6. Escopo do achatamento (medido, não estimado)

Os personagens do jogo não usam um formato só. Medido sobre os 46:

| `renderType` | quantidade | formato no disco |
|---|---|---|
| `animateatlas` | 24 | pasta: `Animation.json` + `spritemapN.json/png` |
| `multianimateatlas` | 10 | idem, com vários spritemaps |
| `multisparrow` | 3 | PNG + XML |
| `sparrow` | 1 | PNG + XML |
| `packer` | 1 | PNG + `.txt` (`nome = x y w h`) |

    python3 tools/scope_animate.py --assets <clone>

| classificação | quantidade |
|---|---|
| folha plana, empacotável hoje | **12** |
| Animate com camadas, precisa de compositor | **34** |
| não resolvidos | **0** |
| folhas sem raster correspondente | **0** |

Os 12 planos já empacotam e verificam quadro a quadro:

    bf-christmas 295 fr  416x447 esca 1.000  2 arquivos
    bf-holding-gf 293 fr  445x424 esca 0.703  2 arquivos
    bf-pixel     126 fr   73x73  esca 1.000
    gf-pixel      30 fr  120x106 esca 1.000
    gf-tankmen    65 fr  515x367 esca 0.565
    nene-pixel    52 fr   75x84  esca 1.000
    pico-pixel   220 fr  136x85  esca 1.000
    senpai-angry 154 fr  125x164 esca 1.000
    senpai       154 fr  125x164 esca 1.000
    spirit       100 fr  256x256 esca 1.000
    spooky-dark  106 fr  437x433 esca 0.678
    spooky       106 fr  468x420 esca 0.727

`tutorial_pack_load` recusa pacote acima de 8 MB, então um personagem grande
sai em mais de um arquivo (bf-christmas: 2). Isso não é contorno: o runtime
nunca precisa do personagem inteiro residente, então a parte é a unidade
natural.

### O compositor, e como ele foi provado

`tools/achatar_animate.py` resolve o grafo Animate em quadros planos. O algoritmo
não foi inventado: foi lido do renderizador que o jogo usa
(`FunkinCrew/flxanimate`).

    flxanimate/animate/FlxElement.hx, fromJSONEx():
        m[0]=mx[0]; m[1]=mx[1]; m[4]=mx[2]; m[5]=mx[3]; m[12]=mx[4]; m[13]=mx[5]
        new FlxMatrix(m[0], m[1], m[4], m[5], m[12], m[13])

    flxanimate/animate/FlxTimeline.hx, fromJSONEx():
        for (layer in timeline.L) layers.push(FlxLayer.fromJSONEx(layer))

    flxanimate/data/SpriteMapData.hx, AnimateSpriteData:
        um sprite do spritemap é só um retângulo (x, y, w, h, rotated) na folha.

Duas coisas a fonte não fixa para export estático, e que por isso são
**medidas**, não assumidas:

1. **Dois tipos de elemento.** `FlxElement.fromJSON` começa com
   `var symbol = element.SI != null` e usa `element.ASI` no outro ramo. `SI` é
   instância de símbolo; `ASI` é bitmap, e `ASI.N` é o nome do sprite. O nível
   mais fundo do atlas é sempre `ASI` — foi isso que fez a primeira versão
   renderizar zero quadros.
2. **`inner=first` vs `inner=advance`.** `FlxElement.fromJSONEx` faz
   `params.firstFrame = element.SI.FF ?? 0`. Testadas as duas leituras contra a
   referência: `first` dá IoU 0,970, `advance` dá 0,495 e erra quadros
   alternados. `first` é a correta.

Validação contra o artefato que o jogo já usa (`bf_frames.lz4f`, 43 quadros de
258×254, decodificado do CFZ4 real):

| medida | resultado |
|---|---|
| IoU da silhueta (6 quadros) | **0,970** |
| escala nativa do render vs referência | **exatamente 0,500** |
| erro médio de RGB (25 287 px opacos) | razão 0,98–0,99 entre canais |
| correlação RGB | 0,887 / 0,899 / 0,883 |

A referência é o render nativo **a meia escala** — coerente com a análise de
exibição do `tools/ps2_display.py` (640×360 lógico dentro de 640×448). O
compositor reproduz a arte do jogo; a diferença de nitidez é a meia escala, que
é a do próprio original.

Personagens achatados e empacotados a 16 bpp:

| personagem | quadros | canvas @0.5 | VRAM/quadro | pacote |
|---|---|---|---|---|
| bf | 223 | 249×256 | 127 488 B | 3 138 355 B |
| pico | 73 | 376×271 | 203 792 B | 1 227 093 B |
| dad | 36 | 284×430 | 244 240 B | 766 486 B |

`bf` traz 223 quadros porque cobre **as 12 animações** (Idle, Left, Left Miss,
Right, Right Miss, Up, Up Miss, Down, Down Miss, Hey, Cheer, Scared); os 43 da
referência são um subconjunto exportado à parte. Os quadros não são inventados:
`inner=first` faz cada keyframe nomear seu próprio quadro interno, então a
expansão é só o intervalo que a timeline já declara.

### O que continua faltando aqui

- O compositor roda **offline**, no PC. Ele ainda não está no `build.sh` como
  etapa obrigatória, e nenhum destes pacotes foi carregado pelo jogo no console.
- O caminho de desenho do jogo continua sem carregador genérico por música: os
  pacotes existem, mas `main.c` ainda abre `bf_frames.lz4f` por nome fixo.

## 7. Os 46 personagens, empacotados e verificados

    tools/achatar_tudo.sh <clone-funkin.assets> build/packs/chars

Resolve os três layouts por personagem e empacota tudo a 16 bpp, escala 0.5.

| resultado | valor |
|---|---|
| personagens empacotados | **46 de 46** |
| falhas | **0** |
| quadros totais | **6 694** |
| blocos LZ4 verificados na volta | **todos** (0 ruins) |
| arquivos no pendrive | 46 pacotes + 2 partes (`bf-christmas`, `bf-holding-gf`) |
| bytes no pendrive | **110 MB** |
| VRAM por quadro | 106 KB a 378 KB |

Dois defeitos de caminho que essa execução revelou e que estavam escondidos:

1. Os planos declaram `assetPath` com a pasta dentro (`characters/spirit`), então
   o PNG fica em `<pacote>/images/characters/spirit.png`. Olhar só
   `<pacote>/images/<base>.png` errava 12 de 46.
2. `write_packs` calculava as partes mas não gravava — quem gravava era o
   chamador, e um chamador esqueceu. O script reportava sucesso com um arquivo
   que não existia no disco. Agora a escrita acontece num lugar só.

## 8. O que ainda separa isto de "jogável"

Medido, em ordem de dependência:

1. **Carregador genérico no jogo.** `main.c` abre `bf_frames.lz4f` e
   `gf_frames.lz4f` por nome fixo. Os 46 pacotes existem no disco, mas nada os
   procura por música/personagem. Sem isso, só o tutorial desenha.
2. **Palcos.** Os 21 palcos precisam do mesmo tratamento (props como CFZ4 e a
   escala da tabela da seção 4). Ainda não foram empacotados.
3. **Console.** Nenhum pacote foi carregado no PS2. O travamento no quadro 0
   continua sem causa provada, e o único dado que o resolve é `BOOT.txt` +
   `FNF_COLHEITA.txt` de uma execução em hardware.
