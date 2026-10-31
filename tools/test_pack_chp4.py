#!/usr/bin/env python3
"""Prova o empacotador CHP4 (tools/pacote_personagem.pack_character_chp4).

POR QUE ESTE TESTE EXISTE
O jogo deixa de poder mostrar personagem nenhum se o pacote vier com a tabela
de animacoes errada, e os dois jeitos de errar sao silenciosos: pegar o quadro
de outra animacao (ordem do atlas em vez da ordem das animacoes) e gravar a
escala errada no campo 32 (personagem 2x maior ou menor sem erro nenhum). Nenhum
dos dois aparece num log de carga -- os dois aparecem na tela.

O que ele cobre, com um clone SINTETICO montado aqui (nao depende do clone da
arte existir nesta maquina):
  - um quadro por cor conhecida, e o pacote e' lido de volta quadro a quadro
    para provar que a animacao "idle" tem os quadros de "Idle" e a "singLEFT"
    os de "Left", na ordem do XML (nao na ordem do atlas);
  - `frameIndices` manda quando existe;
  - a escala vai no campo 32 e volta inteira (0,5 e 1,0);
  - duas animacoes que compartilham o mesmo quadro: ele aparece duas vezes e os
    intervalos na tabela continuam contiguos (o que o jogo assume);
  - animacao cujo prefixo nao existe no atlas e' REPORTADA em `faltando`, e
    nao vira animacao vazia nem quebra o pacote;
  - `budget` reduz a escala ate caber, e o que ele reporta e' o que o pacote
    tem de fato (bytes por quadro), nao uma estimativa.

O que ele NAO prova: que a arte real cabe em VRAM, nem que o console desenha
certo. Isso e' `tools/check_char_packs.py` sobre os 46 reais e o console.

Roda da raiz do projeto: python3 tools/test_pack_chp4.py
"""
import os
import struct
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import chp4  # noqa: E402

try:
    import numpy as np
    from PIL import Image
except ImportError as e:  # pragma: no cover - ambiente sem PIL
    print("PULAR: falta %s (Pillow/numpy) para montar o clone sintetico" % e)
    raise SystemExit(0)

import pacote_personagem as pp  # noqa: E402

falhas = 0


def checar(cond, msg):
    global falhas
    if not cond:
        print("  FALHA: %s" % msg)
        falhas += 1


def perto(a, b, eps=1e-6, msg=""):
    checar(abs(a - b) <= eps, "%s (%r != %r)" % (msg or "valor", a, b))


# --------------------------------------------------------------- o clone fake --

LADO = 32
# Cores com um canal diferente em cada: se a ordem dos quadros se trocar, o
# teste ve' na hora.
CORES = {
    "Idle 000": (255, 0, 0, 255),
    "Idle 001": (0, 255, 0, 255),
    "Left 000": (0, 0, 255, 255),
    "Left 001": (255, 255, 0, 255),
}
# Ordem do ATLAS embaralhada de proposito: XML manda Left 001 primeiro, entao
# "na ordem do atlas" e "na ordem da animacao" dao resultados diferentes.
ORDEM_XML = ["Left 001", "Idle 000", "Left 000", "Idle 001"]


def montar_clone(raiz):
    pacote = os.path.join(raiz, "shared", "images", "characters")
    os.makedirs(pacote, exist_ok=True)
    nr = len(ORDEM_XML)
    folha = Image.new("RGBA", (LADO * nr, LADO), (0, 0, 0, 0))
    for i, nome in enumerate(ORDEM_XML):
        bloco = Image.new("RGBA", (LADO, LADO), CORES[nome])
        folha.paste(bloco, (i * LADO, 0))
    png = os.path.join(pacote, "teste.png")
    folha.save(png)
    linhas = ['<?xml version="1.0" encoding="UTF-8"?>', "<TextureAtlas>"]
    for i, nome in enumerate(ORDEM_XML):
        linhas.append('  <SubTexture name="%s" x="%d" y="0" width="%d" height="%d"/>'
                      % (nome, i * LADO, LADO, LADO))
    linhas.append("</TextureAtlas>")
    with open(os.path.join(pacote, "teste.xml"), "w") as f:
        f.write("\n".join(linhas) + "\n")

    # DATA e' a raiz de assets-data: o leitor entra em <data>/characters/<id>.json.
    dados = os.path.join(raiz, "dados")
    os.makedirs(os.path.join(dados, "characters"), exist_ok=True)
    with open(os.path.join(dados, "characters", "teste.json"), "w") as f:
        f.write("""{
  "name": "Teste",
  "assetPath": "characters/teste",
  "animations": [
    {"name": "idle", "prefix": "Idle"},
    {"name": "singLEFT", "prefix": "Left"},
    {"name": "fantasma", "prefix": "NaoExiste"}
  ]
}""")
    # O `--assets` e a raiz do clone (que tem shared/, week1/, ...), nao a pasta
    # da semana: e assim que o empacotador real e chamado.
    return raiz, dados


def cor_do_quadro(cab, dados_pacote, i):
    """(r, g, b, a) do canto superior esquerdo do quadro `i`."""
    cru = chp4.quadro(dados_pacote, cab, i)
    w, h, psm = cab["w"], cab["h"], cab["psm"]
    if psm == 16:
        v = struct.unpack_from("<H", cru, 0)[0]
        r = (v & 0x1F) << 3
        g = ((v >> 5) & 0x1F) << 3
        b = ((v >> 10) & 0x1F) << 3
        a = 255 if (v >> 15) & 1 else 0
        return r, g, b, a
    return struct.unpack_from("<BBBB", cru, 0)


def main():
    print("== empacotador CHP4: quadro por animacao, escala e dados que faltam ==")
    raiz = tempfile.mkdtemp(prefix="fnf_pack_chp4_")
    assets, dados = montar_clone(raiz)
    saida = os.path.join(raiz, "packs")

    r = pp.pack_character_chp4(assets, "teste", saida, escala=1.0, bpp=32, data=dados)
    checar("error" not in r, "empacotou: %s" % r.get("error"))
    if "error" in r:
        return 1
    pacote = open(r["file"], "rb").read()
    cab = chp4.ler(pacote)
    chp4.conferir(cab)

    # Canvas: o maior quadro com os offsets, arredondado para o passo de 64 px
    # do GS (o port sobe com TBW de 64 em 64).
    checar(cab["w"] % 64 == 0, "canvas com passo de 64 px (%d)" % cab["w"])
    perto(cab["escala"], 1.0, msg="escala do pacote")

    # Tabela: as animacoes na ordem da JSON, cada uma contigua.
    nomes = [a["nome"] for a in cab["anims"]]
    checar(nomes == ["idle", "singLEFT"], "animacoes na ordem da JSON: %r" % (nomes,))
    primeiro = 0
    for a in cab["anims"]:
        checar(a["primeiro"] == primeiro,
               "animacao %s comeca em %d (esperado %d)" % (a["nome"], a["primeiro"], primeiro))
        primeiro += a["quantos"]
    checar(primeiro == cab["n_quadros"],
          "soma das animacoes (%d) == quadros do pacote (%d)" % (primeiro, cab["n_quadros"]))
    checar(r["frames"] == 4, "4 quadros no total (idle 2 + singLEFT 2): %d" % r["frames"])

    # E o conteudo: idle = Idle 000, Idle 001 (na ordem do XML); singLEFT =
    # Left 001, Left 000. Se a ordem do atlas vazasse para o pacote, os dois
    # primeiros seriam amarelo/azul.
    idle = cab["anims"][0]
    esperado_idle = [CORES["Idle 000"], CORES["Idle 001"]]
    for k in range(idle["quantos"]):
        cor = cor_do_quadro(cab, pacote, idle["primeiro"] + k)
        checar(cor == esperado_idle[k],
               "idle quadro %d e' %r, esperado %r" % (k, cor, esperado_idle[k]))
    esq = cab["anims"][1]
    esperado_esq = [CORES["Left 001"], CORES["Left 000"]]
    for k in range(esq["quantos"]):
        cor = cor_do_quadro(cab, pacote, esq["primeiro"] + k)
        checar(cor == esperado_esq[k],
               "singLEFT quadro %d e' %r, esperado %r" % (k, cor, esperado_esq[k]))

    # Dado que falta e' reportado, nao tapado.
    faltando = dict(r["faltando"])
    checar("fantasma" in faltando, "animacao sem quadro reportada: %r" % (r["faltando"],))
    checar("fantasma" not in nomes, "animacao sem quadro NAO entrou na tabela")

    # -------- escala de verdade no campo 32, e o desenho por 2x menor
    r2 = pp.pack_character_chp4(assets, "teste", saida, escala=0.5, bpp=32, data=dados)
    checar("error" not in r2, "empacotou a 0,5: %s" % r2.get("error"))
    cab2 = chp4.ler(open(r2["file"], "rb").read())
    perto(cab2["escala"], 0.5, msg="escala 0,5 no campo 32")
    # A largura e' arredondada para o passo de 64 px do GS, entao a 0,5 ela pode
    # dar o mesmo numero; a ALTURA nao tem passo e mostra a reducao de verdade.
    checar(cab2["h"] == cab["h"] // 2,
           "altura do canvas a 0,5 (%d -> %d)" % (cab["h"], cab2["h"]))
    # A cor continua a mesma: reduzir nao pode trocar o quadro.
    cor = cor_do_quadro(cab2, open(r2["file"], "rb").read(), cab2["anims"][0]["primeiro"])
    checar(cor == CORES["Idle 000"], "cor do idle a 0,5: %r" % (cor,))

    # -------- frameIndices manda quando existe
    with open(os.path.join(dados, "characters", "teste.json"), "w") as f:
        f.write("""{
  "name": "Teste",
  "assetPath": "characters/teste",
  "animations": [
    {"name": "escolhida", "prefix": "Idle", "frameIndices": [3, 3]}
  ]
}""")
    r3 = pp.pack_character_chp4(assets, "teste", saida, escala=1.0, bpp=32, data=dados)
    checar("error" not in r3, "frameIndices empacotou: %s" % r3.get("error"))
    pac3 = open(r3["file"], "rb").read()
    cab3 = chp4.ler(pac3)
    checar(cab3["n_quadros"] == 2, "frameIndices [3,3] -> 2 quadros: %d" % cab3["n_quadros"])
    # Indice 3 no atlas = "Idle 001" (verde), repetido.
    for k in range(2):
        cor = cor_do_quadro(cab3, pac3, k)
        checar(cor == CORES["Idle 001"], "frameIndices quadro %d e' %r" % (k, cor))

    # -------- o mesmo quadro em duas animacoes entra duas vezes, e a tabela
    # continua contigua (e' o caso real do "singUP" duplicado do bf-holding-gf).
    with open(os.path.join(dados, "characters", "teste.json"), "w") as f:
        f.write("""{
  "name": "Teste",
  "assetPath": "characters/teste",
  "animations": [
    {"name": "a", "frameIndices": [0, 1]},
    {"name": "b", "frameIndices": [1, 2]},
    {"name": "c", "frameIndices": [0]}
  ]
}""")
    r4 = pp.pack_character_chp4(assets, "teste", saida, escala=1.0, bpp=32, data=dados)
    checar("error" not in r4, "reuso empacotou: %s" % r4.get("error"))
    pac4 = open(r4["file"], "rb").read()
    cab4 = chp4.ler(pac4)
    chp4.conferir(cab4)
    checar([a["quantos"] for a in cab4["anims"]] == [2, 2, 1],
           "quantos por animacao: %r" % ([a["quantos"] for a in cab4["anims"]],))
    checar(cab4["n_quadros"] == 5, "quadro reusado ocupa 2 lugares: %d" % cab4["n_quadros"])
    # O atlas e' [Left 001, Idle 000, Left 000, Idle 001]: indice 1 = "Idle 000",
    # indice 0 = "Left 001".
    checar(cor_do_quadro(cab4, pac4, cab4["anims"][1]["primeiro"]) == CORES["Idle 000"],
           "animacao b comeca no quadro certo")
    checar(cor_do_quadro(cab4, pac4, cab4["anims"][2]["primeiro"]) == CORES["Left 001"],
           "animacao c comeca no quadro certo")

    # -------- budget: reduz ate' caber, e o reportado e' o medido
    with open(os.path.join(dados, "characters", "teste.json"), "w") as f:
        f.write("""{
  "name": "Teste",
  "assetPath": "characters/teste",
  "animations": [{"name": "idle", "prefix": "Idle"}]
}""")
    orcamento = 4 * 1024
    r5 = pp.pack_character_chp4(assets, "teste", saida, bpp=32, budget=orcamento,
                                data=dados)
    checar("error" not in r5, "budget empacotou: %s" % r5.get("error"))
    if "error" not in r5:
        checar(r5["vram_bytes"] <= orcamento,
               "vram_bytes %d cabe no budget %d" % (r5["vram_bytes"], orcamento))
        cab5 = chp4.ler(open(r5["file"], "rb").read())
        checar(cab5["bytes_por_quadro"] == r5["vram_bytes"],
               "o que o teste reporta e' o que o pacote tem")
        checar(cab5["w"] * cab5["h"] * 4 == cab5["bytes_por_quadro"],
               "canvas x 4 == bytes por quadro")
        perto(cab5["escala"], r5["escala"], msg="escala gravada == escala medida")

    print()
    if falhas:
        print("  %d falha(s)" % falhas)
        return 1
    print("  todas as conferencias passaram")
    return 0


if __name__ == "__main__":
    sys.exit(main())
