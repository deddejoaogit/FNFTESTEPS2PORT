#!/usr/bin/env python3
"""Empacota o AUDIO DAS 27 MUSICAS no formato que o jogo le'.

POR QUE ISTO EXISTE
O port abria `assets/audio_stems.pcm`, um arquivo plano com o audio do Tutorial,
e por isso so' uma musica das 27 tinha som. O caminho por musica existe desde a
0.2.x (`sr_stems_path`: `assets/songs/<id>/stems.pcm`, com o plano como reserva),
mas NENHUMA ferramenta gerava esses arquivos -- o pendrive saia sem eles e o
console so' tocava o Tutorial. Isto fecha esse buraco.

O FORMATO (medido no proprio jogo, nao escolhido aqui)
O anel de audio entrega **8 bytes por amostra**: instrumental 16 bits estereo
(4 B) seguido de vozes 16 bits estereo (4 B). `tutorial_audio_mix_submit` soma
as duas faixas em `amix_somar(samples, samples, n/8, ...)` e escreve **4 bytes**
por amostra de saida (estereo 16 bits, o formato que `audio_set_stream` poe no
audsrv). Ou seja: por quadro, na ordem
    [ InstL, InstR, VoxL, VoxR ]   (int16 little-endian cada)
e o numero de quadros do arquivo e' o numero de amostras do relogio da musica.
O orcamento das 27 musicas fecha exatamente nesse formato: 1 078 655 000 B =
46,8 min a 48 kHz x 8 B.

DE ONDE VEM O AUDIO
`songs/<id>/Inst.ogg` e `songs/<id>/Voices.ogg` do clone de arte
(`--assets <clone do funkin.assets>`). Musica sem `Voices.ogg` recebe vozes em
SILENCIO e isso vai para o manifesto e para a tela -- nunca e' inventado nem
escondido.

A DURACAO NAO E' INVENTADA
O fim da musica e' o `song_end_sample` do chart (`build/charts/<id>-<d>.chart`,
offset 32 do FCH3/4/5), que e' o mesmo numero que o jogo usa. Se o audio do
clone for MAIS CURTO que o chart, o jogo acabaria a musica antes do ultimo
evento e a diferenca e' reportada aqui.

Uso:
  python3 tools/empacotar_stems.py --assets <clone funkin.assets>
  python3 tools/empacotar_stems.py --autoteste        # prova a conta, sem clone
  python3 tools/empacotar_stems.py --conferir         # so' mede o que ja' existe
"""
import argparse
import glob
import hashlib
import os
import struct
import sys

import numpy as np

PROJECT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RATE = 48000
BYTES_POR_QUADRO = 8          # 2 fontes x 2 canais x int16


def cancoes():
    """Os ids das 27 musicas, do DADO (`assets-data/songs/*-metadata.json`).

    Nao de uma lista escrita aqui: uma musica nova no pacote entra sozinha e
    uma que saia nao deixa arquivo orfao no pendrive.
    """
    ids = []
    for caminho in sorted(glob.glob(os.path.join(PROJECT, "assets-data", "songs", "*-metadata.json"))):
        ids.append(os.path.basename(caminho)[:-len("-metadata.json")])
    return ids


def fim_do_chart(caminho):
    """A amostra em que a musica acaba, do proprio chart (offset 32).

    Mesma leitura do `tools/preparar_sweep.py`: e' `chart_info_t.song_end_sample`,
    o numero que o JOGO usa para decidir o fim da musica.
    """
    try:
        with open(caminho, "rb") as f:
            cab = f.read(36)
    except OSError:
        return 0
    if len(cab) < 36 or cab[:4] not in (b"FCH3", b"FCH4", b"FCH5"):
        return 0
    return struct.unpack_from("<I", cab, 32)[0]


def fim_da_musica(sid):
    """O maior `song_end_sample` entre as tres dificuldades (0 se nao houver)."""
    fim = 0
    for d in ("easy", "normal", "hard"):
        for padrao in ("songs/%s/%s.chart" % (sid, d), "charts/%s-%s.chart" % (sid, d)):
            caminho = os.path.join(PROJECT, "build", padrao)
            if os.path.isfile(caminho):
                fim = max(fim, fim_do_chart(caminho))
                break
    return fim


def reamostra(dados, taxa_origem):
    """44,1 kHz -> 48 kHz por interpolacao linear (a mesma conta do
    `tools/empacotar_audio.py`, com o mesmo motivo: o sinal de origem ja' esta'
    limitado a 22 kHz e a taxa de destino e' maior, entao nao ha' dobramento)."""
    if taxa_origem == RATE or dados.shape[0] == 0:
        return dados
    n_out = int(round(dados.shape[0] * RATE / float(taxa_origem)))
    idx = np.linspace(0.0, dados.shape[0] - 1.0, n_out)
    i0 = np.floor(idx).astype(np.int64)
    i1 = np.minimum(i0 + 1, dados.shape[0] - 1)
    frac = (idx - i0).astype(np.float32)[:, None]
    return (dados[i0] * (1.0 - frac) + dados[i1] * frac).astype(np.float32)


def le_estereo(caminho):
    """OGG -> float32 (n, 2) a 48 kHz. Mono vira estereo; >2 canais: os 2 primeiros."""
    import soundfile as sf     # importado aqui: `--autoteste` nao precisa dele
    dados, taxa = sf.read(caminho, dtype="float32", always_2d=True)
    if dados.shape[1] == 1:
        dados = np.repeat(dados, 2, axis=1)
    elif dados.shape[1] > 2:
        dados = dados[:, :2]
    return reamostra(dados, taxa)


def entrelaca(inst, vox):
    """As duas faixas -> o buffer de 8 B por quadro que o jogo le'.

    A mais curta e' completada com SILENCIO ate' a mais longa: o arquivo tem o
    tamanho da maior, e o mixer do jogo soma as duas (silêncio numa fonte e' o
    mesmo que a fonte nao existir, sem mudar o relogio da musica).
    """
    n = max(inst.shape[0], vox.shape[0])
    saida = np.zeros((n, 4), dtype=np.float32)
    if inst.shape[0]:
        saida[:inst.shape[0], 0:2] = inst
    if vox.shape[0]:
        saida[:vox.shape[0], 2:4] = vox
    return np.clip(saida * 32767.0, -32768.0, 32767.0).astype("<i2")


def empacota(sid, assets, out):
    """Converte as duas faixas de uma musica. Devolve (bytes, quadros, avisos)."""
    inst_ogg = os.path.join(assets, "songs", sid, "Inst.ogg")
    vox_ogg = os.path.join(assets, "songs", sid, "Voices.ogg")
    avisos = []
    if not os.path.isfile(inst_ogg):
        return None, "FALTA o instrumental (songs/%s/Inst.ogg)" % sid, avisos
    inst = le_estereo(inst_ogg)
    if os.path.isfile(vox_ogg):
        vox = le_estereo(vox_ogg)
    else:
        vox = np.zeros((0, 2), dtype=np.float32)
        avisos.append("sem Voices.ogg: vozes em SILENCIO")
    dados = entrelaca(inst, vox)
    destino_dir = os.path.join(out, sid)
    os.makedirs(destino_dir, exist_ok=True)
    destino = os.path.join(destino_dir, "stems.pcm")
    bruto = dados.tobytes()
    with open(destino, "wb") as f:
        f.write(bruto)
    quadros = dados.shape[0]
    fim = fim_da_musica(sid)
    if fim and quadros < fim:
        avisos.append("audio MAIS CURTO que o chart: %d < %d amostras (%.3f s a menos)"
                      % (quadros, fim, (fim - quadros) / float(RATE)))
    elif fim:
        avisos.append("sobra %.3f s depois do fim do chart" % ((quadros - fim) / float(RATE)))
    return bruto, quadros, avisos


def autoteste():
    """Prova a conta do formato SEM o clone de arte.

    Monta duas faixas conhecidas, empacota e LE' DE VOLTA com a mesma regra do
    `amix_somar` (soma 16 bits saturada), conferindo amostra por amostra. Se o
    entrelacamento ou a ordem dos canais estiver errada, isto falha aqui e nao
    no console.
    """
    falhas = 0

    def ok(cond, oque):
        nonlocal falhas
        if not cond:
            falhas += 1
            print("  FALHA: " + oque)

    inst = np.array([[0.25, -0.25], [0.5, -0.5], [0.0, 0.0]], dtype=np.float32)
    vox = np.array([[0.125, -0.125], [0.0, 0.0]], dtype=np.float32)   # mais curta
    dados = entrelaca(inst, vox)
    ok(dados.shape == (3, 4), "3 quadros x 4 canais (a mais longa manda)")
    ok(dados.dtype == np.dtype("<i2"), "int16 little-endian")
    ok(len(dados.tobytes()) == 3 * BYTES_POR_QUADRO, "8 bytes por quadro")
    # ordem: InstL, InstR, VoxL, VoxR
    ok(int(dados[0][0]) == 8191 and int(dados[0][1]) == -8191, "inst nos canais 0/1")
    ok(int(dados[0][2]) == 4095 and int(dados[0][3]) == -4095, "vozes nos canais 2/3")
    ok(int(dados[1][2]) == 0, "a voz que acabou virou silencio")
    # a soma que o jogo faz (amix_somar) devolve o estereo
    soma = np.clip(dados[:, 0].astype(np.int32) + dados[:, 2].astype(np.int32),
                   -32768, 32767)
    ok(int(soma[0]) == 8191 + 4095, "a soma Inst+Vox e' a do mixer")
    # 44,1 kHz -> 48 kHz
    x = reamostra(np.zeros((44100, 2), dtype=np.float32), 44100)
    ok(x.shape[0] == RATE, "44,1 kHz -> 48 kHz muda o numero de quadros")
    print("  autoteste: %s" % ("PASSOU" if falhas == 0 else "%d falha(s)" % falhas))
    return 0 if falhas == 0 else 1


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--assets", help="clone do funkin.assets (raiz com songs/<id>/Inst.ogg)")
    ap.add_argument("--out", default=os.path.join(PROJECT, "build", "packs", "stems"))
    ap.add_argument("--autoteste", action="store_true", help="prova o formato sem o clone")
    ap.add_argument("--conferir", action="store_true",
                    help="so' mede os stems que ja' existem em --out")
    args = ap.parse_args()

    if args.autoteste:
        return autoteste()

    ids = cancoes()
    print("== stems das %d musicas (48 kHz, 16 bits, Inst+Vox, 8 B/quadro) ==" % len(ids))

    if args.conferir:
        total = 0
        faltando = []
        for sid in ids:
            caminho = os.path.join(args.out, sid, "stems.pcm")
            if not os.path.isfile(caminho):
                faltando.append(sid)
                continue
            tam = os.path.getsize(caminho)
            quadros = tam // BYTES_POR_QUADRO
            fim = fim_da_musica(sid)
            total += tam
            print("  %-14s %9d B  %7.3f s  chart %7d  %s"
                  % (sid, tam, quadros / float(RATE), fim,
                     "ok" if (not fim or quadros >= fim) else "CURTO"))
        print("  %d de %d, %d B (%.2f GB) em %s"
              % (len(ids) - len(faltando), len(ids), total, total / 1073741824.0, args.out))
        if faltando:
            print("  FALTAM: " + ", ".join(faltando))
            return 1
        return 0

    if not args.assets:
        ap.error("--assets <clone> e' obrigatorio (ou --autoteste / --conferir)")

    os.makedirs(args.out, exist_ok=True)
    manifesto = []
    faltando = []
    total = 0
    for sid in ids:
        bruto, quadros, avisos = empacota(sid, args.assets, args.out)
        if bruto is None:
            faltando.append(sid)
            print("  %-14s FALTA o instrumental" % sid)
            continue
        total += len(bruto)
        h = hashlib.sha256(bruto).hexdigest()[:16]
        nota = "; ".join(avisos)
        manifesto.append("%s %d %.3f %d %s %s"
                         % (sid, RATE, quadros / float(RATE), len(bruto), h, nota))
        print("  %-14s %7.2f s  %9d B  %s%s"
              % (sid, quadros / float(RATE), len(bruto), h,
                 ("  (" + nota + ")") if nota else ""))
    with open(os.path.join(args.out, "STEMS.txt"), "w", encoding="utf-8") as f:
        f.write("# id taxa segundos bytes sha256-16 observacao\n")
        f.write("\n".join(manifesto) + "\n")
    print("  %d musica(s), %d B (%.2f GB) em %s"
          % (len(manifesto), total, total / 1073741824.0, args.out))
    if faltando:
        print("  FALTANDO no clone de arte: " + ", ".join(faltando))
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
