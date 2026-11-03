# FNF PS2 — a pasta do jogo (build 0.2.159)

São **24 arquivos**: 7 partes do jogo + 16 partes de áudio + este guia.
Baixe todos e **extraia na mesma pasta** (todos têm a mesma raiz `FNFPS2/`).

Estado verificado em 2026-11-03: os 24 arquivos estão com permissão
`anyone` (público) e a união deles cobre **746 de 746** arquivos da pasta
montada — nenhum arquivo a menos, nenhum tamanho diferente.

## Como fica o pendrive

```
(pendrive)/
  FNF_PS2.ELF          <- o executável
  LEIA-ME.txt
  FNFPS2/
    assets/            <- tudo que o ELF lê
```

O ELF fica na **raiz** e a pasta `FNFPS2/` **ao lado** dele. Se ficar
`mass:/FNFPS2/FNFPS2/...`, o jogo não abre.

## Partes do jogo (7)

| # | arquivo | tamanho | link direto |
|---|---------|---------|-------------|
| 01 | FNFPS2_jogo_parte01_de07.zip | 42,7 MB | https://drive.google.com/uc?export=download&id=1M5w4SBARvnXFkVDfJqvUDadaXEUiKXrJ |
| 02 | FNFPS2_jogo_parte02_de07.zip | 15,4 MB | https://drive.google.com/uc?export=download&id=10Gm14Obgpj7KMB9X8x9_y6CaBxlPYeYq |
| 03 | FNFPS2_jogo_parte03_de07.zip | 44,9 MB | https://drive.google.com/uc?export=download&id=1ZSNMi-e-zcGnrRJPE_HwDjspwHkUt5_Z |
| 04 | FNFPS2_jogo_parte04_de07.zip | 71,1 MB | https://drive.google.com/uc?export=download&id=1hkVkAyIQyryHeTfA4NRl50JCslJQRXNv |
| 05 | FNFPS2_jogo_parte05_de07.zip | 75,9 MB | https://drive.google.com/uc?export=download&id=1VQXxoAk3KVCn7OKkxDCAVTyPiFX-h1Lu |
| 06 | FNFPS2_jogo_parte06_de07.zip | 57,2 MB | https://drive.google.com/uc?export=download&id=1-1mKoXdkuqxp5SC8HwhBuryVnMn33yMn |
| 07 | FNFPS2_jogo_parte07_de07.zip | 11,6 MB | https://drive.google.com/uc?export=download&id=1uwApAASKzsHI6zpjRi1huDiKjlCBXAd8 |

Guia em texto: https://drive.google.com/file/d/1F2E2FVpLyedLD2UCcCYHlCFw9qWfS-R2/view

## Partes de áudio (16) — 1,13 GB, as 27 músicas

Extraia também na raiz do pendrive. Cada uma traz `FNFPS2/assets/songs/<id>/stems.pcm`.

| # | músicas | MB | id |
|---|---------|----|----|
| 01 | monster | 68,2 | 1arvbIzCVRiJHBUVeinip7ffUFAeSW4u_ |
| 02 | spaghetti | 66,7 | 1rtSkHVwloRdhmNY6Bf0wMplwnvs4zxIg |
| 03 | guns | 53,9 | 1QEduohmRfLVV9Ud-Ce2KnuJshZLvjA8G |
| 04 | winter-horrorland | 51,0 | 1kQDbZn8z6PN3CczyUeAyK-yoP3JBGCGN |
| 05 | darnell | 48,8 | 1KXQQ0GAcZES3nYtH0VeUgu3VkAaa7wBe |
| 06 | milf + stress | 95,5 | 1B0Dz7hX7yyj6BdRAfWjNaw0I1lPsoVOk |
| 07 | blazin + 2hot | 93,2 | 14rCUgcA4LaEdVxjrnVJ--udbpDP0D35F |
| 08 | lit-up + cocoa | 87,3 | 1159qIOKpMmISehIEtVZ0hGkcLmuG9dzt |
| 09 | blammed + high | 81,7 | 1vhaGOY-uFUP8PA6YY-4iOhpkD1E5hjAc |
| 10 | thorns + spookeez | 77,9 | 18K9IWITA1deGry3yyT3ou15NCaIz3Fl6 |
| 11 | senpai + philly-nice | 75,4 | 18EQ0Regedcrbcv20YmrFl9-19aLfcNXy |
| 12 | satin-panties + eggnog | 73,5 | 1tppcpemDMUjG59EgH8JO7PgBrCXAsye6 |
| 13 | roses + dadbattle | 69,6 | 1aSbvtzF4W1dDGa4KlDICj85S4UxSLHXx |
| 14 | south + ugh | 67,4 | 15WCBxXE7HbNluFQzX71-vsXz8ZcbfTJQ |
| 15 | pico + fresh + bopeebo | 96,2 | 1qYzCJqThmxPRbpmDhBjxNcD2mWtBXQHl |
| 16 | tutorial | 25,8 | 17Ydgaobp5Df73Md2PxwjB5ax5bFoFdo4 |

Link por id: `https://drive.google.com/uc?export=download&id=<id>`

## O que tem dentro (tudo arte/áudio de verdade)

- 46 personagens `.chp4` (167 MB)
- 21 palcos, 157 props (11 MB) + 8 arquivos da cutscene do sserafim
- 81 charts × 4 layouts = 324 arquivos
- 27 músicas com áudio real (1,13 GB)
- resultado, freeplay, ícones, barra de vida, splash
- áudio de game over, pausa, telas, menu e scripts de palco
- 16 assets de runtime: **todos os originais**, nenhum placeholder
  (`font.rgba`, `menu_cache.rle`, título com 15 e 30 quadros, `gf_frames`,
  `freakyMenu_48000.pcm` de 21,7 MB, parallax do tutorial…)

## O que ainda não está no ponto

- OPTIONS / MERCH / CREDITS abrem uma tela provisória "TELA NÃO PORTADA"
  com CIRCLE para voltar.
- Sem `stems.pcm` de uma música o jogo não congela: o relógio anda pelo
  tempo, o chart roda e chega ao Resultado com aviso no rodapé.
- **Nada disso foi medido em hardware real.** O que existe é o harness de
  PC; o console é o único juiz.

## Se algo der errado

O jogo grava `mass:/FNFPS2/FNF_COLHEITA.txt`. Mande esse arquivo de volta —
é por ele que se descobre o que aconteceu, sem adivinhar.
