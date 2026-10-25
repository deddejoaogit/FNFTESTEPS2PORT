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
