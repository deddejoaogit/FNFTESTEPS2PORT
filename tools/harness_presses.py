#!/usr/bin/env python3
"""Gera o roteiro de botoes do harness a partir do PROPRIO chart.

Por que existe: o roteiro anterior apertava as setas em cadencia
pseudoaleatoria. Ele provava que o julgamento roda (alguns acertos), mas
tambem produzia dezenas de ghost misses -- e um ghost miss tira 0,08 de vida
(Constants.HIT_WINDOW_MS / Scoring.hx). Com 23 ghost misses o jogador morre de
verdade, e o teste de fim de musica nunca chegava ao resultado. Depois que o
game over existe, apertar no aleatorio deixa de ser um teste de gameplay: vira
um teste de derrota.

Aqui as pressoes sao calculadas das notas do chart:

    quadro = <quadro em que a musica chega em 0> + round(amostra / amostras_por_quadro)

`quadro_zero` NAO e' chutado: ele vem do log da primeira passada
(`tutorial_audio_start`). O roteiro e' deterministico: a mesma entrada produz
sempre o mesmo script, e duas passadas seguidas dao o mesmo resultado.

O roteiro sai em AMOSTRAS, nao em quadros -- formato "460800:LEFT,..." lido
pelo harness por PS2H_INPUT_AMOSTRAS. O motivo foi medido: o relogio da musica
anda por TEMPO, e no harness o tempo virtual tambem avanca dentro dos
DelayThread do audio, entao a razao amostras/quadro muda quando o envio de
audio muda. Um roteiro em quadros erra tudo quando isso acontece (aconteceu: 0
acertos); um roteiro em amostras aperta quando o jogo chega naquele ponto,
independente de quantos quadros levou.

Uso:
    python3 tools/harness_presses.py <chart> [--pular N] [--onda K]
Imprime "amostra:BOTAO,..." para cada nota do jogador.

--onda K poe o indice da musica da campanha em cada aperto ("K:amostra:BOTAO").
E' o que permite testar o Story Mode com 3 musicas seguidas: o relogio da
musica volta a zero em cada uma, entao as amostras de uma musica nao podem
disparar na outra -- sem o indice, o roteiro da segunda musica apertaria nas
horas da primeira, virando ghost miss e matando o jogador.
"""
import struct
import sys

NOTES_STRIDE = 12
# v5 grows the note record to 16 B (it carries the note-kind string index).
NOTES_STRIDE_V5 = 16
LANES = ["LEFT", "DOWN", "UP", "RIGHT"]  # g_receptor_x ordem do port


def read_chart(path):
    """Le o cabecalho e as notas de um .chart FCH3/FCH4/FCH5 do port."""
    with open(path, "rb") as f:
        data = f.read()
    magic = data[:4]
    if magic not in (b"FCH3", b"FCH4", b"FCH5"):
        raise SystemExit(f"{path}: nao e' um chart FCH3/FCH4/FCH5 ({magic!r})")
    stride = NOTES_STRIDE_V5 if magic == b"FCH5" else NOTES_STRIDE
    version = struct.unpack_from("<H", data, 4)[0]
    notes = struct.unpack_from("<I", data, 8)[0]
    bpms = struct.unpack_from("<H", data, 12)[0]
    sections = struct.unpack_from("<H", data, 14)[0]
    events = struct.unpack_from("<H", data, 16)[0]
    end = struct.unpack_from("<I", data, 32)[0]
    header = 40 if version == 3 else 48
    if header > len(data):
        raise SystemExit(f"{path}: cabecalho truncado")
    out = []
    off = header
    for i in range(notes):
        rec = data[off + i * stride: off + (i + 1) * stride]
        if len(rec) < stride:
            raise SystemExit(f"{path}: nota {i} truncada")
        sample, duration = struct.unpack_from("<II", rec, 0)
        lane, must_press = rec[8], rec[9]
        # v5 moves the order word to +12 because +10 is the kind index
        order = struct.unpack_from("<I" if stride == NOTES_STRIDE_V5 else "<H",
                                   rec, 12 if stride == NOTES_STRIDE_V5 else 10)[0]
        if order != i or lane > 3 or must_press > 1:
            raise SystemExit(f"{path}: nota {i} invalida (order {order}, lane {lane})")
        out.append((sample, duration, lane, must_press))
    spb = struct.unpack_from("<I", data, off + notes * stride + 8)[0]
    return out, end, spb, bpms, sections, events


def main(argv):
    if len(argv) < 2:
        raise SystemExit(__doc__)
    path = argv[1]
    pular = 0
    if "--pular" in argv:
        pular = int(argv[argv.index("--pular") + 1])
    onda = -1
    if "--onda" in argv:
        onda = int(argv[argv.index("--onda") + 1])
    notes, end, spb, bpms, sections, events = read_chart(path)

    player = [n for n in notes if n[3]]
    puladas = set()
    if pular > 0 and player:
        # Pula notas espalhadas (nao as primeiras): cada uma vira um MISS de
        # verdade, que o teste usa para exercitar o caminho de erro sem matar
        # o jogador.
        for k in range(pular):
            puladas.add((k * len(player)) // pular)

    itens = []
    for idx, (sample, _dur, lane, _mp) in enumerate(player):
        if idx in puladas:
            continue
        itens.append((sample, LANES[lane]))
    itens.sort()
    if onda >= 0:
        print(",".join(f"{onda}:{f}:{k}" for f, k in itens))
    else:
        print(",".join(f"{f}:{k}" for f, k in itens))
    sys.stderr.write(
        f"chart {path}: {len(notes)} notas ({len(player)} do jogador), "
        f"fim {end}, {spb} amostras/passo, {bpms} BPM(s), {sections} secao(oes), "
        f"{events} evento(s); {len(puladas)} nota(s) deixada(s) para tras\n")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
