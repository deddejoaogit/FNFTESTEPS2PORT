#!/usr/bin/env python3
"""Confere os pacotes de personagem (CHP4) de um diretorio.

POR QUE EXISTE
O leitor de verdade e' o C (`src/engine/char_pack.c`), e ele tem teste proprio.
Mas o ESCRITOR e' outro programa (o empacotador, em Python), e dois programas
que escrevem e leem o mesmo formato precisam de uma terceira opinião que olhe os
ARQUIVOS gerados, e nao os dois lados do mesmo raciocinio. Este e' esse olhar:

  1. todo pacote abre pelo leitor em Python (`tools/chp4.py`), que e' uma
     implementacao independente do C;
  2. o cabecalho bate com o canvas (bytes_por_quadro == w*h*(psm/8));
  3. toda animacao cabe nos quadros que o pacote tem (`conferir`);
  4. TODO quadro descomprime no tamanho declarado -- o que prova que os blocos
     LZ4 do arquivo nao estao truncados nem trocados;
  5. os nomes de animacao e a lista de personagens batem com
     `assets-data/characters` (o dado de origem), nao com uma copia.

O item 5 e' o que pega o erro mais provavel de todos: um personagem empacotado
com o atlas de outro. O pacote abriria, desenharia, e so' na tela alguem veria.

USO
    python3 tools/check_char_packs.py [diretorio]      (padrao: build/harness_assets/FNFPS2/assets/chars)
"""

import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.dirname(HERE)
sys.path.insert(0, HERE)

import chp4  # noqa: E402


def dados_de_origem():
    """id do personagem -> conjunto de nomes de animacao, de assets-data."""
    origem = {}
    d = os.path.join(PROJECT, "assets-data", "characters")
    if not os.path.isdir(d):
        return origem
    for arq in sorted(os.listdir(d)):
        if not arq.endswith(".json"):
            continue
        j = json.load(open(os.path.join(d, arq)))
        # id do personagem = nome do arquivo (o `name` da JSON e' o nome de
        # exibicao, com espacos e maiusculas).
        cid = arq[:-5]
        origem[cid] = {a["name"] for a in j.get("animations", [])}
    return origem


def main(argv):
    alvo = argv[1] if len(argv) > 1 else os.path.join(
        PROJECT, "build", "harness_assets", "FNFPS2", "assets", "chars")
    if not os.path.isdir(alvo):
        print(f"  {alvo} nao existe (nada a conferir)")
        return 0
    origem = dados_de_origem()
    falhas = []
    arquivos = sorted(f for f in os.listdir(alvo) if f.endswith(".chp4"))
    quadros = 0
    bytes_total = 0
    sombreadas = []
    anims_total = 0
    for nome in arquivos:
        caminho = os.path.join(alvo, nome)
        cid = nome[:-5]
        dados = open(caminho, "rb").read()
        bytes_total += len(dados)
        try:
            cab = chp4.ler(dados)
            chp4.conferir(cab)
        except ValueError as e:
            falhas.append(f"{nome}: {e}")
            continue
        quadros += cab["n_quadros"]
        anims_total += len(cab["anims"])
        # Nomes repetidos NAO sao erro: o original guarda as animacoes num Map
        # por nome e a ultima sobrescreve (FlxAnimationController._animations
        # .set), que e' o que o `cp_anim_i` do port faz. Existe um caso real (o
        # "Boyfriend (Holding GF)" tem dois `singUP`). Aqui so' se CONTA, para
        # saber que existe e que a escolhida e' a de indice maior.
        vistos = set()
        for a in cab["anims"]:
            if a["nome"] in vistos:
                sombreadas.append(f"{nome}: '{a['nome']}' sobrescreve a anterior")
            vistos.add(a["nome"])
        # Todo quadro tem de descomprimir no tamanho exato.
        for i in range(cab["n_quadros"]):
            try:
                q = chp4.quadro(dados, cab, i)
            except Exception as e:  # noqa: BLE001 -- qualquer erro do LZ4 conta
                falhas.append(f"{nome}: quadro {i} nao decodifica ({e})")
                break
            if len(q) != cab["bytes_por_quadro"]:
                falhas.append(f"{nome}: quadro {i} saiu com {len(q)} bytes, "
                              f"esperado {cab['bytes_por_quadro']}")
                break
        if origem:
            if cid not in origem:
                falhas.append(f"{nome}: personagem nao existe em assets-data/characters")
            else:
                faltando = origem[cid] - vistos
                extra = vistos - origem[cid]
                if faltando:
                    falhas.append(f"{nome}: faltam animacoes {sorted(faltando)[:4]}")
                if extra:
                    falhas.append(f"{nome}: animacoes que nao estao na origem {sorted(extra)[:4]}")

    print(f"  pacotes de personagem: {len(arquivos)} arquivos, {quadros} quadros, "
          f"{anims_total} animacoes, {bytes_total:,} bytes")
    if sombreadas:
        print(f"  {len(sombreadas)} animacao(oes) com nome repetido dentro do pacote "
              f"(vale a ultima, como no original): {sombreadas[0]}")
    if origem and len(arquivos) != len(origem):
        falhas.append(f"{len(arquivos)} pacotes para {len(origem)} personagens em assets-data")
    if falhas:
        for f in falhas[:20]:
            print(f"  FALHA: {f}")
        print(f"  {len(falhas)} falha(s)")
        return 1
    print("  ok: cada pacote abre, toda animacao cabe, todo quadro decodifica, "
          "e os nomes batem com assets-data")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
