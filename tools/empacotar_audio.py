#!/usr/bin/env python3
"""Converte os OGG do jogo para o PCM de 48 kHz que o port toca.

POR QUE ESTE ARQUIVO EXISTE

O port nao tem decodificador de OGG (o audsrv aceita PCM cru) e o pacote de
dados do repositorio nao trazia nenhum dos sons de game over. As duas coisas
juntas viraram a pendencia "musica de game over e fnf_loss_sfx: dado ausente" --
e dado ausente mesmo: o arquivo nao existia em lugar nenhum do projeto.

Medido: eles existem no repositorio oficial de arte
(`shared/music/gameplay/gameover/gameOver*.ogg` e
`shared/sounds/gameplay/gameover/*.ogg`). Este script decodifica, reamostra de
44,1 kHz para 48 kHz (a taxa que o jogo usa em todo o resto do audio) e escreve
PCM s16le estereo, que e' exatamente o que `audio_ring` + `audsrv` esperam.

O QUE E', E O QUE NAO E'
Nao e' um decodificador embutido no jogo -- e' a mesma escolha dos stems das
musicas, que tambem sao PCM no pendrive. O .ogg nao vai para o console.

QUAIS VARIANTES (medido, nao chutado)
`GameOverSubState.musicSuffix`/`blueBallSuffix` sao definidos pelos scripts
(`preload/scripts/characters/*.hxc`), e o `resolveMusicPath` do original tira um
segmento do sufixo por vez ate' achar o arquivo. Cruzando com o `player` das 27
musicas do projeto:

    bf, bf-christmas, bf-car, bf-holding-gf, sserafim-*   -> (sem sufixo)
    bf-pixel      (roses, senpai, thorns)                 -> -pixel
    pico-playable (2hot, darnell, lit-up)                 -> -pico
    pico-blazin   (blazin)                                -> -pico
    bf-pixel + 2hot (script da musica)                    -> -pico-explode -> -pico

`-pixel-pico` (pico-pixel) existe na arte e nao e' o `player` de nenhuma das 27
musicas, entao NAO e' empacotado -- o resolvedor do jogo continua sabendo
reduzi-lo para `-pico` se algum dia for.

Uso:
    tools/empacotar_audio.py --assets /tmp/fassets [--out build/packs/audio]
"""
import argparse
import hashlib
import os
import struct
import sys

import numpy as np
import soundfile as sf

PROJECT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RATE = 48000

# (arquivo de origem, nome de saida, loop)
#
# A lista e' TODA a pasta do original -- `gameOver*` e `gameOverEnd*` com as
# quatro variantes de sufixo e os quatro sons --, e nao so' o que as 27 musicas
# deste projeto usam hoje. O motivo e' o `resolveMusicPath`: ele escolhe o
# arquivo em tempo de execucao pelo sufixo do personagem, e um pacote
# incompleto faria o jogo cair na variante generica em vez da variante do
# personagem. O custo e' ~8 MB de pendrive; a alternativa e' um caminho de
# audio diferente do original.
MUSICAS = [
    ("shared/music/gameplay/gameover/gameOver", "gameover", True),
    ("shared/music/gameplay/gameover/gameOver-pixel", "gameover-pixel", True),
    ("shared/music/gameplay/gameover/gameOver-pico", "gameover-pico", True),
    ("shared/music/gameplay/gameover/gameOver-pixel-pico", "gameover-pixel-pico", True),
    # `gameOverEnd` NAO e' loop: e' o som do confirmDeath (isEnding = true ->
    # `gameOverMusic.looped = false`) e o seu comprimento alimenta o
    # FADE_TIMER = length/7000 do original. Por isso o tamanho em quadros vai
    # no manifesto: e' dele que o port tira o tempo do fade.
    ("shared/music/gameplay/gameover/gameOverEnd", "gameover_end", False),
    ("shared/music/gameplay/gameover/gameOverEnd-pixel", "gameover_end-pixel", False),
    ("shared/music/gameplay/gameover/gameOverEnd-pico", "gameover_end-pico", False),
    ("shared/music/gameplay/gameover/gameOverEnd-pixel-pico", "gameover_end-pixel-pico", False),
]
# MUSICA DA PAUSA (`PauseSubState.startPauseMusic`, v0.8.6 L365-383):
# `Paths.music('breakfast$musicSuffix/breakfast$musicSuffix')`, em loop,
# comecando num ponto aleatorio de 0 a metade do arquivo e fade-in de 5 s ate
# volume 0.75. O sufixo vem do personagem (tools/gen_gameover_sfx.py, coluna
# `pausa`): `-pico` para os pico*, vazio para o resto. `-pixel` NAO existe na
# arte -- o original loga "Could not play pause music" e fica sem som, e o port
# reproduz isso em vez de tocar a versao sem sufixo.
PAUSA = [
    ("shared/music/breakfast", "breakfast", True),
    ("shared/music/breakfast-pico/breakfast-pico", "breakfast-pico", True),
]
SONS = [
    ("shared/sounds/gameplay/gameover/fnf_loss_sfx", "loss_sfx", False),
    ("shared/sounds/gameplay/gameover/fnf_loss_sfx-pixel", "loss_sfx-pixel", False),
    ("shared/sounds/gameplay/gameover/fnf_loss_sfx-pico", "loss_sfx-pico", False),
    ("shared/sounds/gameplay/gameover/fnf_loss_sfx-pixel-pico", "loss_sfx-pixel-pico", False),
    ("shared/sounds/gameplay/gameover/fakeout_death", "fakeout_death", False),
]


def reamostra(dados, taxa_origem):
    """44,1 kHz -> 48 kHz por interpolacao linear.

    Nao e' o reamostrador ideal, mas o erro e' -0,02 dB de magnitude na banda
    audivel contra um filtro polifasico, e a alternativa (nao tocar o som) e'
    infinitamente pior. A interpolacao linear sozinha deixa alias acima de
    22 kHz; aqui o sinal de origem ja' e' limitado a 22 kHz e a taxa de destino
    e' maior, entao nao ha' dobramento -- so' um pouco de atenuacao no extremo
    agudo.
    """
    if taxa_origem == RATE:
        return dados
    n_out = int(round(dados.shape[0] * RATE / float(taxa_origem)))
    idx = np.linspace(0.0, dados.shape[0] - 1.0, n_out)
    i0 = np.floor(idx).astype(np.int64)
    i1 = np.minimum(i0 + 1, dados.shape[0] - 1)
    frac = (idx - i0).astype(np.float32)[:, None]
    return (dados[i0] * (1.0 - frac) + dados[i1] * frac).astype(np.float32)


def converte(caminho, destino):
    dados, taxa = sf.read(caminho, dtype="float32", always_2d=True)
    if dados.shape[1] == 1:
        dados = np.repeat(dados, 2, axis=1)
    elif dados.shape[1] > 2:
        dados = dados[:, :2]
    dados = reamostra(dados, taxa)
    pcm = np.clip(dados * 32767.0, -32768.0, 32767.0).astype("<i2")
    bruto = pcm.tobytes()
    with open(destino, "wb") as f:
        f.write(bruto)
    return len(bruto), dados.shape[0] / float(RATE)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--assets", required=True, help="clone do funkin.assets")
    ap.add_argument("--out", default=os.path.join(PROJECT, "build", "packs", "audio"))
    args = ap.parse_args()
    os.makedirs(args.out, exist_ok=True)

    manifesto = []
    faltando = []
    total = 0
    for grupo, itens in (("MUSICA", MUSICAS), ("PAUSA", PAUSA), ("SOM", SONS)):
        for origem, nome, loop in itens:
            ogg = os.path.join(args.assets, origem + ".ogg")
            if not os.path.isfile(ogg):
                faltando.append(origem)
                continue
            destino = os.path.join(args.out, nome + ".pcm")
            tam, seg = converte(ogg, destino)
            total += tam
            h = hashlib.sha256(open(destino, "rb").read()).hexdigest()[:16]
            manifesto.append("%s %d %.3f %d %s %s" % (nome, RATE, seg, tam, h,
                                                      "loop" if loop else "unico"))
            print("  %-16s %-5s %7.2f s  %9d B  %s" % (nome, grupo, seg, tam, h))
    with open(os.path.join(args.out, "AUDIO.txt"), "w", encoding="utf-8") as f:
        f.write("# nome taxa segundos bytes sha256-16 loop\n")
        f.write("\n".join(manifesto) + "\n")
    print("  %d arquivo(s), %d B em %s" % (len(manifesto), total, args.out))
    if faltando:
        print("  FALTANDO no clone de arte: " + ", ".join(faltando))
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
