#!/usr/bin/env python3
"""Sintetiza os quatro pacotes embutidos que o build precisa (assets/*.lz4f).

Por que existe: esses quatro arquivos sao arte convertida do jogo original
(logo do titulo e a GF dancando) e NAO ficam no repositorio -- sao baixados do
Drive na maquina do usuario. Sem eles o build inteiro para no assembler
(`.incbin` de arquivo ausente), e o harness de PC nao roda.

Este script produz arquivos VALIDOS do mesmo formato, com as dimensoes e a
contagem de quadros exatas que src/main.c espera, para que a compilacao e o
harness funcionem numa maquina sem a arte. Os pixels sao um padrao
deterministico, nao arte.

O QUE NAO FAZER: copiar a saida deste script para o pendrive. O jogo abriria,
mas o titulo mostraria o padrao. Ele existe para compilar e testar, e o nome de
cada arquivo gerado sai avisando disso.

    python3 tools/gen_packs_vazios.py            # escreve em assets/
    python3 tools/gen_packs_vazios.py --dir /tmp/x
"""
import argparse
import os
import struct

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(HERE)

# Os quatro .incbin de src/title_packs.S e src/story_packs.S, com as dimensoes
# e a contagem de quadros de src/main.c (TITLE_LOGO_*, TITLE_GF_*, story packs).
PACKS = [
    # nome, magica, quadros, bytes por quadro
    ("title_logoBumpin.lz4f",   b"TLZ4", 15, 320 * 240 * 4),
    ("title_gfDanceTitle.lz4f", b"TLZ4", 30, 256 * 235 * 4),
    ("story_gf_idle.lz4f",      b"CFZ4", 30, 170 * 157 * 4),
    ("story_bf_idle.lz4f",      b"CFZ4", 14, 164 * 165 * 4),
]


def lz4_block_literal(data):
    """Um bloco LZ4 valido que e' TODO literal (sem nenhuma copia).

    O decodificador do jogo aceita um bloco assim: e' o caminho mais simples do
    formato e exercita o mesmo codigo que um bloco comprimido. Nada de
    'comprimir': o objetivo e' ser valido, nao ser pequeno.
    """
    n = len(data)
    out = bytearray()
    token_len = 15 if n >= 15 else n
    out.append(token_len << 4)
    rest = n - token_len
    if token_len == 15:
        while rest >= 255:
            out.append(255)
            rest -= 255
        out.append(rest)
    out.extend(data)
    return bytes(out)


def frame_bytes(seed, size):
    """Quadros determinísticos: o mesmo byte em toda rodada, para que duas
    execucoes do harness sejam comparaveis."""
    # RGBA com alfa 0xFF: assim o quadro nao fica invisivel e o caminho de
    # textura com alfa e' o mesmo do asset real.
    base = bytes(((seed * 37 + i * 11) & 0xFF) for i in range(4))
    row = base * 1
    out = bytearray(size)
    for i in range(0, size, 4):
        out[i:i + 4] = row
    for i in range(3, size, 4):
        out[i] = 0xFF
    return bytes(out)


def write_pack(path, magic, count, frame_len, seed):
    table_len = 16 + count * 8
    blobs = [lz4_block_literal(frame_bytes(seed + i, frame_len)) for i in range(count)]
    header = magic + struct.pack("<III", 1, count, frame_len)
    entries = bytearray()
    offset = table_len
    for b in blobs:
        entries += struct.pack("<II", offset, len(b))
        offset += len(b)
    with open(path, "wb") as fh:
        fh.write(header)
        fh.write(entries)
        for b in blobs:
            fh.write(b)
    return offset


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--dir", default=os.path.join(REPO, "assets"))
    args = ap.parse_args()
    os.makedirs(args.dir, exist_ok=True)

    total = 0
    for name, magic, count, frame_len in PACKS:
        path = os.path.join(args.dir, name)
        size = write_pack(path, magic, count, frame_len, 7)
        total += size
        print("  %-26s %8d bytes  %2d quadros de %d B  (%s)"
              % (name, size, count, frame_len, magic.decode()))
    print("\n  %d bytes em %s" % (total, args.dir))
    print("  ATENCAO: sao pacotes SINTETICOS, para compilar e rodar o harness.")
    print("           Nao copie para o pendrive: o titulo mostraria o padrao.")


if __name__ == "__main__":
    main()
