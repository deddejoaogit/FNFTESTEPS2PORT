"""CHP4: o pacote de UM personagem (quadros + tabela de animacoes).

POR QUE EXISTE
O palco ja' e' generico (CFZ4 por prop), o personagem nao: o jogo conhece dois
pacotes escritos no codigo. Cada um dos 46 personagens vira um arquivo
`assets/chars/<id>.chp4` com os quadros comprimidos em LZ4 e, junto, a tabela
que diz quantos quadros cada animacao tem, a que taxa e se volta ao comeco --
porque esse dado nao existe no `character.json` (vem do atlas, que so' o clone
da arte tem).

FORMATO (o mesmo que `src/engine/char_pack.h` le')
    0   "CHP4"
    4   u32 versao = 1
    8   u32 n_quadros
    12  u32 bytes_por_quadro
    16  u32 w
    20  u32 h
    24  u32 n_animacoes
    28  u32 psm (32 ou 16)
    32  u32 reservado
    36  n_quadros * (u32 offset, u32 tamanho)
    ... n_animacoes * 40: nome[24], u32 primeiro, quantos, fps*100, looped
    ... os blocos LZ4

Este arquivo e' o lado que ESCREVE (o empacotador) e o lado que LE' em Python,
para que um pacote gerado possa ser conferido sem ligar o jogo -- e para que a
geracao de teste (tools/gen_harness_assets.py) e o empacotador real usem o mesmo
escritor, em vez de duas cópias do formato.
"""

import struct

MAGIC = b"CHP4"
VERSAO = 1
CABECALHO = 36
NOME = 24        # o mais longo do original: "singRIGHTmiss-beautiful" (23 + NUL)
ENTRADA = 40     # nome[24] + 4 campos de 4 bytes
MAX_ANIMS = 64
ERR_TAMANHO = "bytes_por_quadro nao bate com w*h*(psm/8)"


def escrever(quadros, w, h, psm, anims):
    """Monta o pacote.

    quadros  -- lista de bytes (um por quadro, todos do mesmo tamanho)
    anims    -- lista de dicts {nome, primeiro, quantos, fps100, looped}
    """
    if psm not in (16, 32):
        raise ValueError("psm tem de ser 16 ou 32")
    if not 0 < w <= 1024 or not 0 < h <= 1024:
        raise ValueError("canvas fora de 1..1024")
    if not quadros:
        raise ValueError("pacote sem quadro nenhum")
    if not 0 < len(anims) <= MAX_ANIMS:
        raise ValueError(f"numero de animacoes fora de 1..{MAX_ANIMS}")
    bpp = psm // 8
    frame_bytes = w * h * bpp
    for i, q in enumerate(quadros):
        if len(q) != frame_bytes:
            raise ValueError(f"quadro {i} tem {len(q)} bytes, esperado {frame_bytes}")

    tabela = CABECALHO + len(quadros) * 8
    anims_off = tabela
    corpo = anims_off + len(anims) * ENTRADA

    entradas = []
    blocos = bytearray()
    for q in quadros:
        comp = comprimir(q)
        entradas.append((corpo + len(blocos), len(comp)))
        blocos += comp

    out = bytearray()
    out += MAGIC
    out += struct.pack("<IIIIIII", VERSAO, len(quadros), frame_bytes, w, h, len(anims), psm)
    out += struct.pack("<I", 0)
    for off, tam in entradas:
        out += struct.pack("<II", off, tam)
    for a in anims:
        nome = a["nome"].encode("ascii")
        if len(nome) >= NOME:
            raise ValueError(f"nome de animacao longo demais: {a['nome']}")
        out += nome + b"\x00" * (NOME - len(nome))
        out += struct.pack("<IIII", a["primeiro"], a["quantos"], a["fps100"], a["looped"])
    out += blocos
    return bytes(out)


def ler(dados):
    """Devolve um dict com o cabecalho, a tabela de quadros e a de animacoes.

    Nao descomprime: para conferir o formato, o cabecalho basta; para conferir os
    pixels, use `quadro()`.
    """
    if len(dados) < CABECALHO:
        raise ValueError("cabecalho curto")
    if dados[:4] != MAGIC:
        raise ValueError("nao e' CHP4")
    versao, nq, fbytes, w, h, na, psm = struct.unpack_from("<IIIIIII", dados, 4)
    if versao != VERSAO:
        raise ValueError(f"versao {versao}")
    if psm not in (16, 32):
        raise ValueError(f"psm {psm}")
    if fbytes != w * h * (psm // 8):
        raise ValueError(ERR_TAMANHO)
    tabela = CABECALHO + nq * 8
    anims_off = tabela
    if anims_off + na * ENTRADA > len(dados):
        raise ValueError("tabela fora do arquivo")
    quadros = []
    for i in range(nq):
        off, tam = struct.unpack_from("<II", dados, CABECALHO + i * 8)
        if tam == 0 or off + tam > len(dados):
            raise ValueError(f"bloco {i} fora do arquivo")
        quadros.append({"offset": off, "tamanho": tam})
    anims = []
    for i in range(na):
        e = anims_off + i * ENTRADA
        nome = dados[e:e + NOME].split(b"\x00")[0].decode("ascii", "replace")
        primeiro, quantos, fps100, looped = struct.unpack_from("<IIII", dados, e + NOME)
        anims.append({"nome": nome, "primeiro": primeiro, "quantos": quantos,
                      "fps100": fps100, "looped": looped})
    return {"n_quadros": nq, "bytes_por_quadro": fbytes, "w": w, "h": h,
            "psm": psm, "quadros": quadros, "anims": anims}


def conferir(cab):
    """A tabela tem de caber nos quadros que o pacote realmente tem."""
    n = cab["n_quadros"]
    for a in cab["anims"]:
        if a["quantos"] <= 0:
            raise ValueError(f"animacao {a['nome']} sem quadro")
        if a["primeiro"] + a["quantos"] > n:
            raise ValueError(f"animacao {a['nome']} passa do fim ({a['primeiro']}+{a['quantos']} > {n})")
    return True


def quadro(dados, cab, i):
    from lz4py import decompress
    q = cab["quadros"][i]
    return decompress(dados[q["offset"]:q["offset"] + q["tamanho"]], cab["bytes_por_quadro"])


def comprimir(dados):
    from lz4py import compress
    return compress(dados)
