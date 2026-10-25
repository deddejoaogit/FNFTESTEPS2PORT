# FNFTESTEPS2PORT — motor do port completo de FNF para PS2

Este repositório é o **motor** do port, com as partes que dá para
construir e medir sem o console. Tudo que está aqui compila e roda no PC
com `make test`; nada aqui foi declarado "pronto" sem uma verificação que
possa ser repetida.

## O que existe e está testado

| módulo | linhas | o que resolve | verificação |
|---|---:|---|---|
| `src/engine/conductor.{c,h}` | 330 | relógio musical a partir das **amostras ouvidas**, BPM por seção, bop de câmera (decay 0.95, intensidade 1.015), seguir de câmera, **as 13 curvas** | 77 asserções |
| `src/engine/judgement.{c,h}` | 230 | pontuação, vida, erro, combo, precisão, julgamento e rank, com os números oficiais do v0.8.6 | 54 asserções |
| `src/engine/chart.{c,h}` | 340 | carga de **FCH3** (os 81 charts já convertidos) e de FNC2; eventos entregues **uma vez cada, em ordem** | 60 asserções |
| `src/engine/vram_cache.{c,h}` | 290 | alocador de VRAM com despejo LRU, apelido de folha, fixação contada e **guarda de limite** | 56 asserções |
| `src/engine/asset_bank.{c,h}` | 200 | carregamento por cena: entra do pendrive, sai ao terminar, nada reservado para sempre | 90 asserções |
| `src/engine/bootlog.{c,h}` | 190 | marcos escritos na hora, **fase dentro do quadro**, cão de guarda, detecção de morte anterior | 23 asserções |
| `tools/prep_assets.py` | 470 | fatia folhas, reduz, quantiza para 16 bpp, empacota em `CFZ4`, mede e reduz até caber | selftest próprio |
| `tools/check_charts.py` | 120 | valida uma pasta de charts FCH3 (checksum, ordem, contagem de notas) | roda nos 81 do pendrive |

**Total: 360 asserções, todas passando.** Os números de cada módulo estão
nos comentários de cabeçalho, com a linha do fonte oficial de onde saíram.

## Como rodar

```bash
make test                                  # bateria do motor (gcc, sem PS2)
python3 tools/check_charts.py --dir charts/ # confere os 81 charts
python3 tools/prep_assets.py --selftest     # prova a preparação de imagem
python3 tools/prep_assets.py --orcamento    # tabela de VRAM por palco
```

Dependências do preparo de imagem: `pip install pillow numpy lz4`.

## Documentos

| arquivo | conteúdo |
|---|---|
| `docs/DIAGNOSTICO_PASSO66.md` | por que trava no passo 66, o instrumento para confirmar e a correção que torna o defeito impossível |
| `docs/ORCAMENTO_VRAM.md` | toda a conta de VRAM, com fórmula, e o que cada número decide |
| `docs/MODELO_CARREGAMENTO.md` | o que apagar do `main.c`, o patch que entra no lugar |

## Estado honesto do port completo

Feito e medido:

* motor de tempo, pontuação e chart — **independentes do console**;
* modelo de carregamento com despejo e liberação, provado no PC;
* preparação de imagem com orçamento automático, provada no PC;
* instrumento de boot com resolução de fase.

Falta, e depende do console/toolchain:

1. **rodar no PS2** — o travamento do passo 66 tem hipótese e instrumento,
   mas a confirmação é no hardware;
2. ligar `asset_bank` e `bootlog` no `main.c` (o patch está em
   `docs/MODELO_CARREGAMENTO.md`, são ~30 linhas);
3. estado de jogo genérico consumindo os eventos do chart (o mapa de
   eventos está em `src/engine/chart.h`, os tipos são os reais do v0.8.6);
4. as telas: seleção de música, pausa, resultado;
5. converter as imagens dos 46 personagens e 21 palcos com
   `tools/prep_assets.py`, agora que o orçamento por textura é conhecido.

O gargalo do projeto nunca foi a lógica do jogo — é o laço de verificação.
Cada item acima tem, nesta base, uma forma de ser verificado no PC antes
de gastar uma viagem ao console.
