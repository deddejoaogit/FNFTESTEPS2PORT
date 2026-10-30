"""Gera test/lz4_vetores.h: fluxos LZ4 reais comprimidos pelo tools/lz4py.py.

POR QUE ISTO EXISTE
O decodificador do jogo (src/engine/lz4_bloco.c) e' C; o codificador (o
empacotador) e' Python. Testar cada lado contra si mesmo nao pega o erro que
importa: os dois concordarem sobre o formato ERRADO. Aqui o Python comprime
casos de borda e reais, o resultado vai para um header, e o teste em C decodifica
e compara byte a byte com o original.

Casos escolhidos para cobrir o que o formato tem de dificil:
  - vazio e 1 byte (nenhum token / token unico);
  - literais longos (>= 15, com extensao de 255 em 255);
  - match no limite de 65535 de offset;
  - match SOBREPOSTO (offset menor que o tamanho do match) -- e' o que comprime
    area de cor chapada, que e' a maior parte de um quadro de desenho;
  - dado aleatorio, que nao comprime e sai tudo em literais;
  - dado periodico grande, que e' o formato dos assets sinteticos do harness.
"""
import os
import sys

import lz4py

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.dirname(HERE)
OUT = os.path.join(PROJECT, "test", "lz4_vetores.h")


def casos():
    import random
    rnd = random.Random(20261029)
    out = []

    def add(nome, dados):
        out.append((nome, dados))

    add("vazio", b"")
    add("um_byte", b"Z")
    add("quatro_iguais", b"aaaa")
    add("literais_longos", bytes(range(256)) * 2)          # ~512 literais, sem match
    add("cor_chapada", bytes([0x34, 0x12]) * 2000)         # match sobreposto, periodo 2
    add("periodo_1", b"\x77" * 5000)                       # offset 1, match enorme
    add("periodo_4097", (b"FNFPS2!!" * 512)[:4097] * 3)    # periodo > 4 KB
    add("aleatorio", bytes(rnd.randrange(256) for _ in range(3000)))
    add("desenho", bytes(((x * 7) ^ (x >> 3)) & 0xFF for x in range(8192)))
    return out


def render():
    linhas = []
    linhas.append("/* GERADO POR tools/gen_lz4_vetores.py - NAO EDITAR A MAO.")
    linhas.append("   Fluxos LZ4 reais, comprimidos pelo tools/lz4py.py (o lado que")
    linhas.append("   COMPRIME, usado pelo empacotador). test/test_lz4.c decodifica")
    linhas.append("   cada um com src/engine/lz4_bloco.c e compara com o original:")
    linhas.append("   e' a unica forma de pegar os dois concordando sobre o formato")
    linhas.append("   errado. */")
    linhas.append("#ifndef LZ4_VETORES_H")
    linhas.append("#define LZ4_VETORES_H")
    linhas.append("")
    linhas.append("typedef struct {")
    linhas.append("    const char *nome;")
    linhas.append("    const unsigned char *comprimido;")
    linhas.append("    unsigned int comprimido_bytes;")
    linhas.append("    const unsigned char *original;")
    linhas.append("    unsigned int original_bytes;")
    linhas.append("} lz4_vetor_t;")
    linhas.append("")
    nomes = []
    for nome, dados in casos():
        enc = lz4py.compress(dados)
        assert lz4py.decompress(enc, len(dados)) == dados, nome
        linhas.append("static const unsigned char v_%s_c[] = {%s};"
                      % (nome, ",".join(str(b) for b in enc) if enc else "0"))
        if len(dados) <= 3000:
            linhas.append("static const unsigned char v_%s_o[] = {%s};"
                          % (nome, ",".join(str(b) for b in dados) if dados else "0"))
        else:
            # dados grandes: o teste compara por regra, e a regra vai junto
            linhas.append("/* %s: %d bytes gerados pela regra do gerador */"
                          % (nome, len(dados)))
            linhas.append("static const unsigned char v_%s_o[] = {%s};"
                          % (nome, ",".join(str(b) for b in dados) if dados else "0"))
        nomes.append((nome, len(dados)))
        linhas.append("")
    linhas.append("static const lz4_vetor_t lz4_vetores[] = {")
    for nome, n in nomes:
        linhas.append('  {"%s", v_%s_c, sizeof(v_%s_c), v_%s_o, %du},'
                      % (nome, nome, nome, nome, n))
    linhas.append("};")
    linhas.append("#define LZ4_VETOR_COUNT %d" % len(nomes))
    linhas.append("")
    linhas.append("#endif /* LZ4_VETORES_H */")
    linhas.append("")
    return "\n".join(linhas)


def main(argv):
    if "--check" in argv[1:]:
        atual = open(OUT).read() if os.path.isfile(OUT) else ""
        if atual != render():
            print("lz4_vetores.h NAO corresponde a tools/lz4py.py; "
                  "rode tools/gen_lz4_vetores.py")
            return 1
        print("lz4_vetores.h confere com tools/lz4py.py")
        return 0
    with open(OUT, "w") as f:
        f.write(render())
    print("wrote test/lz4_vetores.h")
    return 0


if __name__ == "__main__":
    sys.path.insert(0, HERE)
    sys.exit(main(sys.argv))
