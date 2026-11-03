#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""MARATONA: joga cada musica ATE' O FIM, no jogo real, e cobra o Resultado.

POR QUE ISTO EXISTE
O sweep das 81 combinacoes da' 1 200 quadros por musica (~20 s). Isso prova que
o chart abre, que o palco desenha e que a musica comeca -- NAO prova que a
musica TERMINA. Muita coisa so' aparece no fim: o ultimo evento do script, a
secao de BPM que muda no meio, o fim do audio, a transicao para o Resultado, o
`endStuff` do `sserafim` (que so' existe nos ultimos 2 % da musica). Um port que
roda os primeiros 20 s de 27 musicas nao e' um port do jogo.

O que cada rodada mede, por musica:
  - se o jogo chega ao estado 9 (Resultado) DEPOIS do fim do chart;
  - o `play_result` (rank e percentual de acerto) e as tallies (SICK/GOOD/BAD/
    SHIT/MISS), com o autoplay apertando as notas do proprio chart;
  - a auditoria da faixa visivel (retangulos/linhas fora) e o estouro de pacote;
  - os eventos de erro que nunca podem aparecer: `palco_script_retry`,
    `char_sem_vram`, `palco_sem_vram`, `cut_fim_sem_vram`, `*_falta`.

Uso:
  python3 tools/maratona_harness.py                     # as 27, dificuldade normal
  python3 tools/maratona_harness.py --dificuldade 0     # so' easy
  python3 tools/maratona_harness.py --musicas spaghetti,monster
  python3 tools/maratona_harness.py --todas             # 27 x 3 = 81 charts

O `stems.pcm` de cada musica tem de ter a duracao do CHART (o padrao do
`--completo`): com um audio curto a musica "acaba" antes do ultimo evento e a
rodada mediria outra coisa.
"""
import argparse
import os
import re
import struct
import subprocess
import sys
import time

PROJECT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RAIZ = os.path.join(PROJECT, "build", "harness_assets")
BIN = os.path.join(PROJECT, "build", "fnf_harness")
LOG = os.path.join(RAIZ, "FNFPS2", "FNF_COLHEITA.txt")
RATE = 48000
FPS = 60.0
# A cutscene do `sserafim` (730/24 s) roda ANTES da contagem; a contagem e' de
# 4 batidas + 1 (o "3,2,1,GO!"). Os dois numeros ja' estao medidos no log:
# `cut_fim,1460000` e `tutorial_audio_start` no quadro 2 017.
CUTSCENE_SSERAFIM = 31.0
# O `endStuff` do `sserafim` roda DEPOIS do fim do chart e segura o Resultado:
# 9 s de sequencia (medido: `cut_endstuff_fim,432000`). Sem somar isto a rodada
# termina no meio da sequencia e cobra um Resultado que ainda nao devia existir.
FIM_SSERAFIM = 11.0
CONTAGEM = 3.5
# A transicao para o Resultado so' abre depois de `g_result_wait == 90` (1,5 s) e
# o `transition_frame` de 30 quadros (0,5 s): sem essa folga a rodada termina
# ANTES do estado 9 e mede a tela de jogo em vez do Resultado.
MARGEM = 6.5


def cancoes():
    import glob
    ids = []
    for caminho in sorted(glob.glob(os.path.join(PROJECT, "assets-data", "songs", "*-metadata.json"))):
        ids.append(os.path.basename(caminho)[: -len("-metadata.json")])
    return ids


def fim_do_chart(caminho):
    """`song_end_sample` do chart convertido (offset 32 do FCH3/4/5)."""
    try:
        with open(caminho, "rb") as f:
            cab = f.read(36)
    except OSError:
        return 0
    if len(cab) < 36 or cab[:4] not in (b"FCH3", b"FCH4", b"FCH5"):
        return 0
    return struct.unpack_from("<I", cab, 32)[0]


def e_sserafim(sid):
    """A UNICA musica com cutscene de abertura e sequencia de fim e' a do palco
    `sserafim` (a `spaghetti`): 730/24 s antes da contagem e o `endStuff` de 9 s
    depois do ultimo evento. O resto do que a rodada precisa somar vem do
    proprio chart (`song_end_sample`)."""
    return sid == "spaghetti"


def campo(nome, col=4, log=None):
    """Ultimo valor de um evento `E,<ticks>,<nome>,<a>,<b>`."""
    valor = None
    try:
        with open(log or LOG, "r", encoding="utf-8", errors="replace") as f:
            for linha in f:
                if not linha.startswith("E,"):
                    continue
                partes = linha.rstrip("\n").split(",")
                if len(partes) > col and partes[2] == nome:
                    valor = partes[col - 1]
    except OSError:
        return None
    return valor


def tem_evento(nome, log=None):
    try:
        with open(log or LOG, "r", encoding="utf-8", errors="replace") as f:
            for linha in f:
                if linha.startswith("E,") and ("," + nome + ",") in linha:
                    return True
    except OSError:
        return False
    return False


def prepara(musicas):
    """Garante o stems com a duracao do chart (esparso, barato) e o chart v5."""
    env = dict(os.environ)
    env["FNF_SWEEP_FMT"] = "v5"
    r = subprocess.run([sys.executable, os.path.join(PROJECT, "tools", "preparar_sweep.py"),
                        "--completo", "--forcar", "--musicas", ",".join(musicas)],
                       cwd=PROJECT, env=env, capture_output=True, text=True)
    print("  preparo: " + (r.stdout.strip().splitlines() or [""])[-1])
    return r.returncode == 0


def uma(sid, dif):
    chart = os.path.join(RAIZ, "FNFPS2", "assets", "songs", sid, ["easy", "normal", "hard"][dif] + ".chart")
    if not os.path.isfile(chart):
        return None, "sem chart"
    fim = fim_do_chart(chart)
    if not fim:
        return None, "chart ilegivel"
    segundos = fim / float(RATE)
    if e_sserafim(sid):
        segundos += CUTSCENE_SSERAFIM + FIM_SSERAFIM
    segundos += CONTAGEM + MARGEM
    quadros = int(segundos * FPS) + 1
    teclas = subprocess.run([sys.executable, os.path.join(PROJECT, "tools", "harness_presses.py"),
                             chart, "--pular", "0"],
                            cwd=PROJECT, capture_output=True, text=True).stdout.strip()
    if os.path.isfile(LOG):
        os.remove(LOG)
    env = dict(os.environ)
    env.update({
        "FNF_START_SONG": sid,
        "FNF_START_DIFF": str(dif),
        "PS2H_FRAMES": str(quadros),
        "PS2H_ROOT": os.path.relpath(RAIZ, PROJECT),
        "PS2H_WATCHDOG": "240",
        "PS2H_INPUT_AMOSTRAS": teclas,
    })
    t0 = time.time()
    r = subprocess.run([BIN], cwd=PROJECT, env=env, capture_output=True, text=True, timeout=300)
    dt = time.time() - t0
    saida = r.stdout + r.stderr
    res = {
        "sid": sid,
        "dif": dif,
        "quadros": quadros,
        "segundos": dt,
        "rc": r.returncode,
        "passou": "RESULTADO: PASSOU" in saida,
        "estado9": False,
        "fim_chart": fim,
        "notas": campo("play_result_tallies"),
        "resultado": campo("play_result"),
        "audio": campo("tutorial_audio_end"),
        "auditoria": "",
        "erros": [],
    }
    m = re.search(r"auditoria da faixa visivel: (\d+) retangulo\(s\).*?(\d+) linha\(s\) fora", saida)
    if m:
        res["auditoria"] = "%s ret / %s linha" % (m.group(1), m.group(2))
        if m.group(1) != "0" or m.group(2) != "0":
            res["erros"].append("auditoria")
    if "ESTOUROS DE PACOTE" in saida:
        mm = re.search(r"ESTOUROS DE PACOTE \.+ (\d+)", saida)
        if mm and mm.group(1) != "0":
            res["erros"].append("pacote")
    try:
        with open(LOG, "r", encoding="utf-8", errors="replace") as f:
            for linha in f:
                if not linha.startswith("E,"):
                    continue
                if ",state,9," in linha:
                    res["estado9"] = True
                for nome in ("palco_script_retry", "char_sem_vram", "palco_sem_vram",
                             "cut_fim_sem_vram", "cut_fim_falta", "cut_fim_tamanho",
                             "audio_falta", "play_audio_ausente"):
                    if ("," + nome + ",") in linha and nome not in res["erros"]:
                        res["erros"].append(nome)
    except OSError:
        pass
    return res, None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--musicas", default="", help="lista separada por virgula")
    ap.add_argument("--dificuldade", type=int, default=1, choices=[0, 1, 2])
    ap.add_argument("--todas", action="store_true", help="27 x 3 dificuldades")
    ap.add_argument("--sem-preparo", action="store_true")
    args = ap.parse_args()

    ids = [s for s in args.musicas.split(",") if s] or cancoes()
    difs = [0, 1, 2] if args.todas else [args.dificuldade]
    print("== maratona: %d musica(s) x %d dificuldade(s), cada uma ATE' O FIM ==" % (len(ids), len(difs)))
    if not args.sem_preparo and not prepara(ids):
        print("  FALHA: nao consegui preparar os assets do harness")
        return 1

    falhas = []
    total = 0
    t0 = time.time()
    for sid in ids:
        for dif in difs:
            total += 1
            res, erro = uma(sid, dif)
            nome = "%s/%d" % (sid, dif)
            if erro:
                falhas.append((nome, erro))
                print("  FALHA %-16s %s" % (nome, erro))
                continue
            ok = (res["passou"] and res["estado9"] and res["rc"] == 0 and not res["erros"])
            if not ok:
                falhas.append((nome, ",".join(res["erros"]) or ("sem Resultado" if not res["estado9"] else "harness")))
            print("  %-4s %-16s %5d quadros  %5.1f s de jogo  %6.1f s de roda  %-22s %s%s"
                  % ("ok" if ok else "FALHA", nome, res["quadros"], res["quadros"] / FPS,
                     res["segundos"], res["auditoria"] or "-",
                     ("resultado " + str(res["resultado"])) if res["resultado"] else "",
                     ("  ERROS: " + ",".join(res["erros"])) if res["erros"] else ""))

    print()
    print("  %d rodada(s) em %.1f s" % (total, time.time() - t0))
    if falhas:
        print("  FALHARAM: " + ", ".join("%s (%s)" % (n, m) for n, m in falhas))
        return 1
    print("  MARATONA: PASSOU -- as %d rodada(s) chegaram ao Resultado pelo fim do"
          " chart, com a faixa visivel intacta" % total)
    return 0


if __name__ == "__main__":
    sys.exit(main())
