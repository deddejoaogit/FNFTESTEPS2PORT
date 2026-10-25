#!/usr/bin/env python3
"""check_charts.py - confere os charts FCH3 do projeto.

Os 81 charts (27 musicas x 3 dificuldades, 672 KB, 34.325 notas) ja estao
convertidos. Este programa NAO converte nada: ele valida, com as mesmas
regras do tutorial_chart.c do port, e imprime o total de notas -- que e a
unica forma barata de saber que a conversao inteira esta integra antes de
abrir o jogo no console.

  python3 tools/check_charts.py --dir /caminho/dos/charts

Regras verificadas (identicas as do port):
  - magic "FCH3", versao 3, tamanhos de registro 12/16/8/16;
  - checksum FNV-1a do payload;
  - notas em ordem, lane 0..3, must_press 0..1, ordem sequencial,
    duracao dentro do fim da musica;
  - bpms crescentes, o primeiro comecando em 0;
  - secoes crescentes, a primeira em 0, ordem sequencial;
  - eventos em ordem, tipo != 0, ordem sequencial.
"""
import argparse
import os
import struct
import sys

HEADER = 40


def fnv1a(dados):
    h = 2166136261
    for b in dados:
        h = ((h ^ b) * 16777619) & 0xFFFFFFFF
    return h


def checar(caminho):
    with open(caminho, "rb") as f:
        b = f.read()
    erros = []
    if len(b) < HEADER:
        return None, ["arquivo menor que o cabecalho"]
    if b[:4] != b"FCH3":
        return None, ["magic != FCH3"]
    ver, nota_sz = struct.unpack_from("<HH", b, 4)
    n_notas, = struct.unpack_from("<I", b, 8)
    n_bpms, n_sec, n_ev = struct.unpack_from("<HHH", b, 12)
    bpm_sz, sec_sz, ev_sz = struct.unpack_from("<HHH", b, 20)
    checksum, fim = struct.unpack_from("<II", b, 28)
    if ver != 3:
        erros.append("versao %d != 3" % ver)
    if (nota_sz, bpm_sz, sec_sz, ev_sz) != (12, 16, 8, 16):
        erros.append("tamanhos de registro %s" % str((nota_sz, bpm_sz, sec_sz, ev_sz)))
    payload = n_notas * 12 + n_bpms * 16 + n_sec * 8 + n_ev * 16
    if HEADER + payload != len(b):
        erros.append("tamanho do arquivo nao bate com o cabecalho")
        return None, erros
    if fnv1a(b[HEADER:]) != checksum:
        erros.append("checksum FNV-1a nao confere")
    if not n_notas or not fim:
        erros.append("sem notas ou sem fim de musica")
        return None, erros

    off = HEADER
    anterior = -1
    for i in range(n_notas):
        amostra, dur = struct.unpack_from("<II", b, off)
        lane, must = b[off + 8], b[off + 9]
        ordem, = struct.unpack_from("<H", b, off + 10)
        if lane > 3 or must > 1 or ordem != i:
            erros.append("nota %d invalida" % i)
            break
        if amostra > fim or dur > fim - amostra:
            erros.append("nota %d passa do fim" % i)
            break
        if amostra < anterior:
            erros.append("notas fora de ordem na %d" % i)
            break
        anterior = amostra
        off += 12

    off = HEADER + n_notas * 12
    anterior = -1
    for i in range(n_bpms):
        step, sample, sps, bpm = struct.unpack_from("<IIII", b, off)
        if not sps or sample > fim or (i and (step <= anterior or sample <= anterior)):
            erros.append("bpm %d invalido" % i)
            break
        anterior = sample
        off += 16

    off += 0
    for i in range(n_sec):
        start = struct.unpack_from("<I", b, off)[0]
        if i == 0 and start != 0:
            erros.append("primeira secao nao comeca em 0")
            break
        off += 8

    off = HEADER + n_notas * 12 + n_bpms * 16 + n_sec * 8
    anterior = -1
    for i in range(n_ev):
        amostra, tipo, valor, ordem = struct.unpack_from("<IIII", b, off)
        if not tipo or ordem != i or amostra > fim or amostra < anterior:
            erros.append("evento %d invalido" % i)
            break
        anterior = amostra
        off += 16

    return {"nome": os.path.basename(caminho), "notas": n_notas,
            "eventos": n_ev, "secoes": n_sec, "bpms": n_bpms,
            "fim_ms": fim // 48, "bytes": len(b)}, erros


def main(argv):
    ap = argparse.ArgumentParser()
    ap.add_argument("--dir", required=True)
    a = ap.parse_args(argv)
    arquivos = sorted(f for f in os.listdir(a.dir) if f.endswith(".chart"))
    if not arquivos:
        print("nenhum .chart em %s" % a.dir)
        return 1
    total_notas = total_eventos = 0
    ruins = 0
    print("%-28s %7s %8s %7s %8s %9s" % ("chart", "notas", "eventos", "secoes", "fim(s)", "bytes"))
    for f in arquivos:
        info, erros = checar(os.path.join(a.dir, f))
        if erros:
            ruins += 1
            print("%-28s ERRO: %s" % (f, "; ".join(erros[:2])))
            continue
        total_notas += info["notas"]
        total_eventos += info["eventos"]
        print("%-28s %7d %8d %7d %8d %9d"
              % (info["nome"], info["notas"], info["eventos"],
                 info["secoes"], info["fim_ms"] // 1000, info["bytes"]))
    print()
    print("%d charts, %d notas, %d eventos, %d com erro"
          % (len(arquivos), total_notas, total_eventos, ruins))
    if ruins == 0 and len(arquivos) == 81:
        print("bate com o esperado do projeto: 81 charts / 34.325 notas")
    return 0 if ruins == 0 else 2


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
