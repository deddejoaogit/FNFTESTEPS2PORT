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


def formato_atual():
    """O formato que esta' instalado nos assets do harness, ou "" se nao se sabe.

    Sem isto, trocar FNF_SWEEP_FMT entre dois passes nao mudava nada: o
    `destino` ja' existia e o script reaproveitava o chart do passe anterior,
    entao o "passe v3" rodava v5 (ou o contrario) sem ninguem ver. O marcador
    fica no diretorio de assets e e' escrito pelo caminho que de fato grava.
    """
    try:
        with open(os.path.join(RAIZ, ".formato"), encoding="utf-8") as f:
            return f.read().strip()
    except OSError:
        return ""


MAGIC = {"v3": b"FCH3", "v4": b"FCH4", "v5": b"FCH5"}


def magia(caminho):
    """O fourcc do chart no disco, ou b"" se ele nao existe/nao abre."""
    try:
        with open(caminho, "rb") as f:
            return f.read(4)
    except OSError:
        return b""


def chart(assets, sid, diff, forcar):
    """Converte o chart dessa musica/dificuldade, ou reaproveita o ja' feito."""
    destino = os.path.join(RAIZ, "songs", sid, diff + ".chart")
    # FORMATO: por padrao v3, que e' o container que o console ja' provou. v4
    # e' necessario para os eventos que carregam TEXTO (PlayAnimation: o nome da
    # animacao): o v3 tem um u32 por evento e o nome se perde, entao no v3 o
    # port nao tem como saber que animacao tocar. FNF_SWEEP_FMT=v4 gera os
    # mesmos charts no container novo, e o sweep roda um passe nos dois.
    # FNF_SWEEP_FMT=v5 e' o v4 mais o NOTE KIND por nota (o `k` do JSON), que e'
    # o que faz `noanim` e `non_scoreable` chegarem ao jogo.
    fmt = os.environ.get("FNF_SWEEP_FMT", "v3")
    if formato_atual() != fmt:
        forcar = True
    # O marcador sozinho nao basta: um diretorio reaproveitado de outro passe
    # pode ter charts de outro formato com o marcador certo. O fourcc no proprio
    # arquivo e' a verdade, e um chart no formato errado e' reescrito.
    if not forcar and magia(destino) == MAGIC[fmt]:
        return 0
    pronto = os.path.join(PROJECT, "build", "charts-" + fmt, "%s-%s.chart" % (sid, diff))
    if os.path.isfile(pronto) and magia(pronto) != MAGIC[fmt]:
        os.unlink(pronto)
    if os.path.isfile(pronto):
        with open(pronto, "rb") as f:
            dados = f.read()
    else:
        meta = os.path.join(DATA, "songs", sid + "-metadata.json")
        fonte = os.path.join(DATA, "charts", sid + "-chart.json")
        if not os.path.isfile(meta) or not os.path.isfile(fonte):
            return -1
        tmp = os.path.join(PROJECT, "build", "charts-" + fmt, "%s-%s.chart" % (sid, diff))
        os.makedirs(os.path.dirname(tmp), exist_ok=True)
        r = subprocess.run([sys.executable, os.path.join(HERE, "fnf_to_fch3.py")]
                           + (["--v3"] if fmt == "v3" else ["--format", fmt])
                           + ["--metadata", meta, "--chart", fonte,
                            "--difficulty", diff, "-o", tmp],
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        if r.returncode != 0 or not os.path.isfile(tmp):
            return -2
        with open(tmp, "rb") as f:
            dados = f.read()
    os.makedirs(os.path.dirname(destino), exist_ok=True)
    with open(destino, "wb") as f:
        f.write(dados)
    with open(os.path.join(RAIZ, ".formato"), "w", encoding="utf-8") as f:
        f.write(fmt)
    return 0


def ap_args_musicas(args):
    return bool(getattr(args, "musicas", ""))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--segundos", type=float, default=20.0)
    ap.add_argument("--forcar", action="store_true")
    # --musicas: gera so' estas (virgula). O passe de eventos de animacao so'
    # precisa das musicas que TEM PlayAnimation, e as tres nao cabem com o mesmo
    # audio curto do sweep -- o evento da philly-nice cai em 13,7 s e o audio de
    # 20 s cortava a musica antes dele.
    ap.add_argument("--musicas", default="")
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
    if ap_args_musicas(args):
        quer = {m.strip() for m in args.musicas.split(",") if m.strip()}
        ids = [i for i in ids if i in quer]
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
