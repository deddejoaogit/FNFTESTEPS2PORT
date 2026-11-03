#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""O ESCRITOR do pacote de quadros da cutscene (formato `FQC1` v2).

POR QUE ESTE ARQUIVO EXISTE SEPARADO
O leitor em C (`src/engine/ss_cutscene.c`) e o escritor tem de concordar byte a
byte; se um dos dois mudar sozinho, o jogo desenha lixo sem erro nenhum. Aqui o
escritor e' UM so', usado por duas ferramentas:

  tools/assar_cutscene_sserafim.py  -> a arte REAL (o clone do funkin.assets)
  tools/gen_harness_assets.py       -> a arte SINTETICA do harness

Assim o harness exercita o mesmo formato que o console vai ler. Este modulo NAO
importa PIL, numpy nem nada de imagem de proposito: o gerador de assets do
harness roda em ambientes onde so' a biblioteca padrao esta' disponivel.

FORMATO (tudo little-endian)
  cabecalho  24 B  "FQC1" | u16 n_sprites | u16 n_frames | u32 fps100 |
                   u32 escala1000 | u16 tex_w | u16 tex_h | u16 n_labels |
                   u16 reservado (0)
  sprites    8 B   u16 x, y, w, h              (espaco do DOCUMENTO)
  rotulos   12 B   u16 primeiro | u16 quadros | char nome[8]
  quadros    4 B   u32 offset (relativo ao fim desta tabela)
  quads     14 B   u16 sprite | i16 a,b,c,d (1/4096) | i16 tx,ty (1/4 px)

O que cada campo quer dizer esta' documentado em src/engine/ss_cutscene.h; aqui
fica so' o empacotamento.
"""
import struct

MAGIC = b"FQC1"
CAB = 24
SPRITE_BYTES = 8
LABEL_BYTES = 12
QUAD_BYTES = 14
MAX_LABELS = 8          # o mesmo SSC_MAX_LABELS do leitor em C
NOME_BYTES = 8


def monta_pacote(nomes, retangulos, quadros, fps, escala,
                 tex_w, tex_h, labels=None):
    """Monta os bytes do `.quads`.

    nomes       lista de nomes de sprite (a ordem define o indice do quad)
    retangulos  [(x, y, w, h)] na MESMA ordem de `nomes`, no espaco do documento
    quadros     [[(indice, a, b, c, d, tx, ty)]] -- um por quadro da animacao
    fps, escala taxa da animacao e escala do atlas (a mesma da textura)
    tex_w/h     tamanho da TEXTURA que acompanha (o leitor exige > 0)
    labels      [(nome, primeiro, quadros)] -- as etiquetas de quadro do Animate
    """
    if len(nomes) != len(retangulos):
        raise ValueError("nomes e retangulos com tamanhos diferentes")
    if not nomes:
        raise ValueError("pacote sem sprite")
    if tex_w <= 0 or tex_h <= 0:
        raise ValueError("tamanho de textura invalido")
    labels = list(labels or [])
    if len(labels) > MAX_LABELS:
        raise ValueError("%d rotulos: o leitor aceita no maximo %d"
                         % (len(labels), MAX_LABELS))
    cab = MAGIC + struct.pack("<HHIIHHHH", len(nomes), len(quadros),
                              int(round(fps * 100)), int(round(escala * 1000)),
                              tex_w, tex_h, len(labels), 0)
    if len(cab) != CAB:
        raise AssertionError("cabecalho com %d bytes" % len(cab))
    corpo = bytearray(cab)
    for x, y, w, h in retangulos:
        corpo += struct.pack("<HHHH", x, y, w, h)
    for nome, primeiro, quantos in labels:
        nb = nome.encode("utf-8")[:NOME_BYTES]
        if len(nb) > NOME_BYTES:
            raise ValueError("nome de rotulo longo demais: %r" % nome)
        corpo += struct.pack("<HH", primeiro, quantos) + nb.ljust(NOME_BYTES, b"\0")
    tabela = bytearray()
    dados = bytearray()
    for q in quadros:
        tabela += struct.pack("<I", len(dados))
        for (i, a, b, c, d, tx, ty) in q:
            dados += struct.pack("<Hhhhhhh", i, a, b, c, d, tx, ty)
    return bytes(corpo) + bytes(tabela) + bytes(dados)
