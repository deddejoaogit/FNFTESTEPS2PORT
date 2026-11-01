#!/usr/bin/env python3
"""Empacota os icones de vida do HUD num formato que o jogo le.

POR QUE ESTE ARQUIVO EXISTE

Os dois icones ao lado da barra de vida sao o unico pedaco do HUD que o port
ainda desenhava com arte de reserva: `PlayState.hx` L2155/L2190 criam um
`HealthIcon` para o oponente e um para o jogador, e cada um mostra o rosto do
personagem (`CharacterData.healthIcon`, ou `Constants.DEFAULT_HEALTH_ICON` =
'face' quando o personagem nao traz um).

A arte EXISTE no repositorio de assets oficiais, em
`preload/images/icons/icon-<id>.png`, e estava dada como ausente porque o clone
esparso usado pelo empacotador de personagens nunca incluiu `preload/images`.
Este script le dali.

FORMATO (ICP1), 32 bytes de cabecalho, little-endian:

    0   "ICP1"
    4   u16 versao (1)
    6   u16 largura do quadro   (px)
    8   u16 altura do quadro    (px)
    10  u16 quadros (1..3)
    12  u16 psm (16 = PSMCT16)
    14  u16 passo da textura    (stride em px, multiplo de 64)
    16  u16 flags (bit 0 = isPixel)
    18  u16 reservado (0)
    20  u32 bytes por quadro (passo * altura * 2)
    24  u32 bytes de dados
    28  u32 soma dos bytes (para o leitor conferir)

Os quadros vem em seguida, um atras do outro, no MESMO layout que o
`upload_raw_psm(..., GS_PSM_16)` do jogo espera: RGBA5551 little-endian, com o
passo da textura (o GS exige largura multipla de 64 no TBW).

O TAMANHO NAO E' O DO ORIGINAL: no original o icone tem 150x150 num espaco de
1280x720, o que da' 75x75 no espaco de 640x360 que o port desenha. Aqui o
quadro sai com 96 px para caber o bop de 1,2x (90 px) e o zoom do camHUD de ate'
1,35x (101 px) sem borrar mais do que o original borra; os de pixel art saem em
32 px, que e' o tamanho deles no original (`HealthIcon.PIXEL_ICON_SIZE`).

Uso:
    tools/empacotar_icones.py --assets /tmp/fassets [--out build/packs/icons]
"""
import argparse
import json
import os
import struct
import sys

import numpy as np
from PIL import Image

PROJECT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# `Constants.DEFAULT_HEALTH_ICON` (util/Constants.hx L210): o icone que entra no
# lugar quando o personagem nao declara um.
DEFAULT_ICON = "face"

# `HealthIcon.HEALTH_ICON_SIZE` / `PIXEL_ICON_SIZE` (play/components/HealthIcon.hx
# L150 e L157), convertidos de 1280x720 para o espaco de 640x360 do port.
ICON_SIZE = 96
PIXEL_ICON_SIZE = 32

HEADER = 32
MAGIC = b"ICP1"


def icone_ids():
    """Todos os ids de icone que o jogo pode pedir.

    Tres fontes, e nenhuma delas e' um palpite: o id padrao do original, os
    `healthIcon.id` dos 46 personagens (o dado que o jogo le'), e os ids citados
    pelos eventos `SetHealthIcon` dos charts -- que apontam para o indice de um
    personagem, ou seja ja' estao cobertos pela segunda fonte. O conjunto sai
    ordenado para o pacote ser reproduzivel.
    """
    ids = {DEFAULT_ICON}
    base = os.path.join(PROJECT, "assets-data", "characters")
    for nome in sorted(os.listdir(base)):
        if not nome.endswith(".json"):
            continue
        with open(os.path.join(base, nome), encoding="utf-8-sig") as f:
            d = json.load(f)
        hi = d.get("healthIcon")
        if isinstance(hi, dict) and hi.get("id"):
            ids.add(hi["id"])
    return sorted(ids)


def is_pixel(icon_id):
    """1 quando o dado do personagem diz que o icone e' pixel art."""
    base = os.path.join(PROJECT, "assets-data", "characters")
    for nome in sorted(os.listdir(base)):
        if not nome.endswith(".json"):
            continue
        with open(os.path.join(base, nome), encoding="utf-8-sig") as f:
            d = json.load(f)
        hi = d.get("healthIcon")
        if isinstance(hi, dict) and hi.get("id") == icon_id:
            return bool(hi.get("isPixel"))
    return False


def to_ps2_16(img):
    """RGBA -> RGBA5551 little-endian, o mesmo layout de `to_ps2_bytes` do
    empacotador de personagem (e o que `upload_raw_psm` espera)."""
    a = np.array(img, dtype=np.uint8)
    r = (a[:, :, 0].astype(np.uint16) >> 3) & 0x1F
    g = (a[:, :, 1].astype(np.uint16) >> 3) & 0x1F
    b = (a[:, :, 2].astype(np.uint16) >> 3) & 0x1F
    al = (a[:, :, 3].astype(np.uint16) >> 7) & 0x01
    v = (r | (g << 5) | (b << 10) | (al << 15)).astype("<u2")
    return v


def frames_of(path, destino, pixel):
    """Frames do PNG, ja' no tamanho e no passo que a textura vai usar.

    O original guarda os icones no formato antigo: uma tira horizontal de
    150x150 por quadro (`HealthIcon.loadAnimationOld`), com 1, 2 ou 3 quadros
    (idle, losing e, quando existe, winning). O numero de quadros NAO e' chutado:
    e' largura/altura do PNG.
    """
    im = Image.open(path).convert("RGBA")
    w, h = im.size
    if h <= 0 or w % h != 0:
        raise ValueError("%s: %dx%d nao e' uma tira de quadros" % (path, w, h))
    n = w // h
    alvo = PIXEL_ICON_SIZE if pixel else ICON_SIZE
    passo = (alvo + 63) & ~63
    if alvo != passo:
        passo = (alvo + 63) & ~63
    quadros = []
    for i in range(n):
        q = im.crop((i * h, 0, (i + 1) * h, h)).resize((alvo, alvo), Image.LANCZOS)
        # O quadro vai para uma textura de passo `passo`: as colunas que sobram
        # sao preenchidas com transparente, senao o GS amostra lixo do vizinho.
        canvas = Image.new("RGBA", (passo, alvo), (0, 0, 0, 0))
        canvas.paste(q, (0, 0))
        quadros.append(to_ps2_16(canvas).tobytes())
    return quadros, alvo, passo


def escreve(destino, quadros, alvo, passo, pixel):
    bytes_quadro = passo * alvo * 2
    dados = b"".join(quadros)
    if any(len(q) != bytes_quadro for q in quadros):
        raise ValueError("quadro com tamanho diferente do passo")
    soma = sum(dados) & 0xFFFFFFFF
    cab = MAGIC + struct.pack("<8H3I", 1, alvo, alvo, len(quadros), 16, passo,
                              1 if pixel else 0, 0, bytes_quadro, len(dados), soma)
    if len(cab) != HEADER:
        raise ValueError("cabecalho com %d bytes" % len(cab))
    with open(destino, "wb") as f:
        f.write(cab + dados)
    return len(cab) + len(dados)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--assets", required=True,
                    help="clone do funkin.assets (com preload/images)")
    ap.add_argument("--out", default=os.path.join(PROJECT, "build", "packs", "icons"))
    args = ap.parse_args()

    origem = os.path.join(args.assets, "preload", "images", "icons")
    if not os.path.isdir(origem):
        print("FALTA %s -- o clone precisa incluir preload/images" % origem)
        return 1
    os.makedirs(args.out, exist_ok=True)

    total = 0
    linhas = []
    faltando = []
    for cid in icone_ids():
        png = os.path.join(origem, "icon-%s.png" % cid)
        if not os.path.isfile(png):
            faltando.append(cid)
            continue
        pixel = is_pixel(cid)
        quadros, alvo, passo = frames_of(png, args.out, pixel)
        destino = os.path.join(args.out, "%s.icp" % cid)
        n = escreve(destino, quadros, alvo, passo, pixel)
        total += n
        linhas.append((cid, alvo, len(quadros), passo, n))
        print("  %-20s %dx%-3d %d quadro(s) passo %-4d %7d B" %
              (cid, alvo, alvo, len(quadros), passo, n))

    with open(os.path.join(args.out, "ICONES.txt"), "w", encoding="utf-8") as f:
        f.write("# icone, tamanho do quadro, quadros, passo, bytes do arquivo\n")
        for l in linhas:
            f.write("%s %d %d %d %d\n" % l)

    print("  icones: %d arquivo(s), %d B" % (len(linhas), total))
    if faltando:
        # Dado faltando e' REPORTADO, nunca inventado: um icone sem PNG vira
        # lista aqui e o jogo cai no `face` (o mesmo que o original faz quando
        # `Assets.exists` diz que nao ha' icone -- HealthIcon.hx L587).
        print("  SEM ARTE (o jogo cai no `%s`): %s" % (DEFAULT_ICON, ", ".join(faltando)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
