#!/usr/bin/env python3
"""Empacota os pedacos de UI do original que o port desenha como textura.

POR QUE ESTE ARQUIVO EXISTE

O HUD de gameplay do port vem inteiro de `tutorial_hud_official_320x256_gsalpha
.rgba`, um atlas que o port ja' usa e que veio do Drive. A BARRA de vida entra
nesse atlas como uma regiao (0,112) 301x10 -- mas a arte oficial dela e' um
arquivo separado, `shared/images/healthBar.png` (601x19, LA), e a regiao do
atlas foi feita a mao. O resultado e' uma barra branca sem a moldura com alpha
do original.

Como o atlas do HUD nao e' gerado por nenhum tool deste repositorio (ele veio
pronto), a solucao que nao mexe no layout de VRAM e' SOBREPOR a regiao da
barra: o jogo le `health_bar.rgba` e copia os pixels por cima do atlas antes do
upload. 301x10x4 = 12.040 bytes, nenhuma textura nova, nenhum endereco novo.

A arte e' reduzida 2x (601x19 -> 301x10) porque o HUD do port e' desenhado em
640x360 (o original e' 1280x720 e usa a arte 1:1). Reducao por MEDIA de 2x2 com
arredondamento -- nao NEAREST: a moldura tem alpha parcial (63..223) e jogar
fora 3 de cada 4 amostras de alpha deixaria a borda serrilhada.

O canal L vira R=G=B (o PNG e' LA; o FlxSprite do original desenha a
luminancia, e o canvas do port e' RGB com alpha de 8 bits).

Uso:
    PYTHONPATH=/tmp/pylibs tools/empacotar_ui.py --assets /tmp/fassets [--out build/packs/ui]
"""
import argparse
import os
import sys

import numpy as np
from PIL import Image

PROJECT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# Regiao da barra dentro do atlas do HUD (as mesmas coordenadas do sprite_region
# do main.c: 0,112 com 301x10).
BARRA_X, BARRA_Y, BARRA_W, BARRA_H = 0, 112, 301, 10


def reduz_2x(px, w, h):
    """601x19 -> 301x10 por media de 2x2 (a ultima coluna/linha fica sozinha)."""
    out = np.zeros((h // 2 + (h % 2), w // 2 + (w % 2), 4), dtype=np.uint8)
    for y in range(out.shape[0]):
        for x in range(out.shape[1]):
            bloco = px[y * 2:y * 2 + 2, x * 2:x * 2 + 2]
            out[y, x] = np.round(bloco.reshape(-1, 4).mean(axis=0)).astype(np.uint8)
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--assets", default=os.environ.get("FNF_ASSETS", ""))
    ap.add_argument("--out", default=os.path.join(PROJECT, "build", "packs", "ui"))
    args = ap.parse_args()
    if not args.assets:
        print("  FALHA: informe --assets <clone do funkin.assets>")
        return 2
    src = os.path.join(args.assets, "shared", "images", "healthBar.png")
    if not os.path.isfile(src):
        print("  FALTA: %s" % src)
        return 2

    im = Image.open(src).convert("LA")
    w, h = im.size
    la = np.array(im, dtype=np.uint8)
    # L,A -> R,G,B,A com R=G=B=L
    rgba = np.zeros((h, w, 4), dtype=np.uint8)
    rgba[:, :, 0] = la[:, :, 0]
    rgba[:, :, 1] = la[:, :, 0]
    rgba[:, :, 2] = la[:, :, 0]
    rgba[:, :, 3] = la[:, :, 1]
    tile = reduz_2x(rgba, w, h)
    # A regiao do atlas e' 301x10: completa o que faltar com transparente.
    final = np.zeros((BARRA_H, BARRA_W, 4), dtype=np.uint8)
    ch = min(BARRA_H, tile.shape[0])
    cw = min(BARRA_W, tile.shape[1])
    final[:ch, :cw] = tile[:ch, :cw]

    os.makedirs(args.out, exist_ok=True)
    destino = os.path.join(args.out, "health_bar.rgba")
    with open(destino, "wb") as f:
        f.write(final.tobytes())

    opacos = int((final[:, :, 3] == 255).sum())
    print("  health_bar.rgba: %dx%d de %dx%d (%dx%d, LA) -> %d B, %d px opacos, %d com alpha parcial"
          % (BARRA_W, BARRA_H, w, h, w, h, os.path.getsize(destino),
             opacos, int(((final[:, :, 3] > 0) & (final[:, :, 3] < 255)).sum())))
    return 0


if __name__ == "__main__":
    sys.exit(main())
