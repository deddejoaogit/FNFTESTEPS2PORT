#!/usr/bin/env python3
"""Empacotamento de quadros Sparrow num ATLAS (Sparrow XML -> retangulos).

Usado pelas pecas da tela de Resultado que precisam de TODOS os quadros
residentes ao mesmo tempo (o placar, os contadores de julgamento, o percentual
de conclusao): nelas nao existe "um quadro por vez" como nas folhas animadas.

O QUE SAI DAQUI
  - `ler_xml`: os quadros do Sparrow, com o corte (frameX/frameY) de cada um;
  - `quadros_do_prefixo`: a ordem das animacoes do `addByPrefix` do flixel;
  - `prateleiras` / `escolher_largura`: o empacotamento (a largura e' escolhida
    medindo a area de varias candidatas, nao por chute).

Nada aqui sabe de jogo: entra XML e sai retangulo.
"""

import xml.etree.ElementTree as ET

GAP = 1          # 1 px entre quadros: o filtro linear nao pega o vizinho
MAX_TEX = 1024   # limite de largura/altura de textura do GS
LARGURAS = (64, 96, 128, 160, 192, 224, 256, 320, 384, 448, 480, 512, 560,
            640, 768, 1024)


def ler_xml(caminho):
    """{nome: {x, y, w, h, fx, fy, fw, fh}} do Sparrow XML."""
    q = {}
    for st in ET.parse(caminho).getroot():
        a = st.attrib
        nome = a.get("name")
        if not nome:
            continue
        q[nome] = {
            "x": int(a["x"]), "y": int(a["y"]),
            "w": int(a["width"]), "h": int(a["height"]),
            "fx": int(a.get("frameX", 0)), "fy": int(a.get("frameY", 0)),
            "fw": int(a["frameWidth"]) if "frameWidth" in a else int(a["width"]),
            "fh": int(a["frameHeight"]) if "frameHeight" in a else int(a["height"]),
        }
    return q


def quadros_do_prefixo(quadros, prefixo):
    """Quadros de uma animacao, na ordem do `addByPrefix` do flixel."""
    achados = []
    for nome in quadros:
        if not nome.startswith(prefixo):
            continue
        resto = nome[len(prefixo):]
        if not resto.isdigit():
            continue
        achados.append((int(resto), nome))
    achados.sort()
    return [n for _i, n in achados]


def prateleiras(alturas_larguras, largura):
    """Empacota a maior altura primeiro, uma linha por vez.

    `alturas_larguras` e' [(w, h)]. Devolve (posicoes [(x, y)], altura total).
    """
    ordem = sorted(range(len(alturas_larguras)),
                   key=lambda i: -alturas_larguras[i][1])
    pos = [None] * len(alturas_larguras)
    x = y = 0
    altura_linha = 0
    for i in ordem:
        w, h = alturas_larguras[i]
        if x + w > largura:
            x = 0
            y += altura_linha + GAP
            altura_linha = 0
        pos[i] = (x, y)
        x += w + GAP
        altura_linha = max(altura_linha, h)
    return pos, y + altura_linha


def escolher_largura(alturas_larguras):
    """A largura de MENOR area total (largura x altura) entre as candidatas."""
    melhor = None
    for largura in LARGURAS:
        pos, altura = prateleiras(alturas_larguras, largura)
        if altura > MAX_TEX:
            continue
        area = largura * altura
        if melhor is None or area < melhor[0]:
            melhor = (area, largura, pos, altura)
    if melhor is None:
        return None
    _area, largura, pos, altura = melhor
    return largura, altura, pos
