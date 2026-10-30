#!/usr/bin/env python3
"""Prepara os assets minimos para tocar TODAS as musicas no harness.

POR QUE ISTO EXISTE
O harness provava uma musica (o Tutorial). As outras 26 nunca foram iniciadas
no PC: um caminho de chart errado numa musica do week7 so' apareceria no
console, com o jogo travado. Aqui cada musica ganha o que ela precisa para
COMECAR: o chart convertido (o mesmo FCH3 que o console le') e um stems.pcm
curto, so' para o audio abrir e o relogio da musica andar.

O stems NAO tem a duracao da musica: ele existe para o teste ouvir o comeco
(20 s por padrao). Fingir a duracao inteira custaria 100 MB por musica e nao
provaria nada a mais -- o que se quer provar e' "esta musica comeca".

Uso: python3 tools/preparar_sweep.py [--segundos 20] [--forcar]
"""
import argparse
import glob
import json
import os
import struct
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.dirname(HERE)
sys.path.insert(0, HERE)

DATA = os.path.join(PROJECT, "assets-data")
RAIZ = os.path.join(PROJECT, "build", "harness_assets", "FNFPS2", "assets")
RATE = 48000


def pcm(segundos, freq=196):
    """Sinal quadrado simples, estereo 16 bits 48 kHz (o que o audsrv toca)."""
    total = int(segundos * RATE) * 4
    out = bytearray(total)
    half = max(1, RATE // freq // 2)
    for i in range(total // 2):
        v = 0x1000 if (i // half) % 2 else -0x1000
        struct.pack_into("<h", out, i * 2, v)
    return bytes(out)


def mapa_musica_palco():
    """song id -> stage id, do playData de cada musica (a mesma fonte do registro).

    O sweep precisa da LISTA de palcos que as musicas usam, senao "N palcos
    desenharam" nao diz nada: pode ser que um palco nunca tenha aparecido. E a
    fonte e' o metadata de cada musica, nao o .inc gerado -- ler o arquivo
    gerado com expressao regular pegava blocos errados.
    """
    mapa = {}
    for f in sorted(glob.glob(os.path.join(DATA, "songs", "*-metadata.json"))):
        sid = os.path.basename(f)[:-len("-metadata.json")]
        d = json.load(open(f))
        st = (d.get("playData") or {}).get("stage")
        if not st:
            return {}
        mapa[sid] = st
    return mapa


def musicas():
    ids = []
    for f in sorted(glob.glob(os.path.join(DATA, "charts", "*-chart.json"))):
        ids.append(os.path.basename(f)[:-len("-chart.json")])
    return ids


def chart(assets, sid, diff, forcar):
    """Converte o chart dessa musica/dificuldade, ou reaproveita o ja' feito."""
    destino = os.path.join(RAIZ, "songs", sid, diff + ".chart")
    if os.path.isfile(destino) and not forcar:
        return 0
    pronto = os.path.join(PROJECT, "build", "charts-v3", "%s-%s.chart" % (sid, diff))
    if os.path.isfile(pronto):
        with open(pronto, "rb") as f:
            dados = f.read()
    else:
        meta = os.path.join(DATA, "songs", sid + "-metadata.json")
        fonte = os.path.join(DATA, "charts", sid + "-chart.json")
        if not os.path.isfile(meta) or not os.path.isfile(fonte):
            return -1
        tmp = os.path.join(PROJECT, "build", "charts-v3", "%s-%s.chart" % (sid, diff))
        os.makedirs(os.path.dirname(tmp), exist_ok=True)
        r = subprocess.run([sys.executable, os.path.join(HERE, "fnf_to_fch3.py"),
                            "--v3", "--metadata", meta, "--chart", fonte,
                            "--difficulty", diff, "-o", tmp],
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        if r.returncode != 0 or not os.path.isfile(tmp):
            return -2
        with open(tmp, "rb") as f:
            dados = f.read()
    os.makedirs(os.path.dirname(destino), exist_ok=True)
    with open(destino, "wb") as f:
        f.write(dados)
    return 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--segundos", type=float, default=20.0)
    ap.add_argument("--forcar", action="store_true")
    args = ap.parse_args()

    mapa = mapa_musica_palco()
    if len(mapa) != 27:
        print("  FALHA: o registro tem %d musicas com palco (esperado 27)" % len(mapa))
        return 1
    os.makedirs(os.path.join(PROJECT, "build", "sweep"), exist_ok=True)
    with open(os.path.join(PROJECT, "build", "sweep", "mapa-musica-palco.txt"), "w") as f:
        for sid in sorted(mapa):
            f.write("%s %s\n" % (sid, mapa[sid]))
    ids = musicas()
    if not ids:
        print("  FALHA: nenhum chart em assets-data/charts")
        return 1
    problemas = 0
    ncharts = 0
    for sid in ids:
        for diff in ("easy", "normal", "hard"):
            rc = chart(RAIZ, sid, diff, args.forcar)
            if rc == 0:
                ncharts += 1
            elif rc != -1:
                print("  %s/%s: falha ao converter (%d)" % (sid, diff, rc))
                problemas += 1
        stems = os.path.join(RAIZ, "songs", sid, "stems.pcm")
        if not os.path.isfile(stems) or args.forcar:
            os.makedirs(os.path.dirname(stems), exist_ok=True)
            with open(stems, "wb") as f:
                f.write(pcm(args.segundos))
    print("  sweep: %d musicas, %d charts, stems de %.0f s, %d problema(s)"
          % (len(ids), ncharts, args.segundos, problemas))
    return 1 if problemas else 0


if __name__ == "__main__":
    sys.exit(main())
